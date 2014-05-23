#include <config.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ucontext.h>
#include "sthread_preempt.h"
#include "sthread_ctx.h"
#include "sthread_user.h"

#ifdef STHREAD_CPU_I386
#include "sthread_switch_i386.h"
#endif

#ifdef STHREAD_CPU_X86_64
#include "sthread_switch_x86_64.h"
#endif

#include <sys/time.h>
#include <sys/timeb.h>
#include <signal.h>

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

// #define ITIMER_REAL ITIMER_VIRTUAL
// #define SIGALRM SIGVTALRM
// #define ITIMER_REAL ITIMER_PROF
// #define SIGALRM SIGPROF

#define LOCK_UNLOCKED 0
#define LOCK_LOCKED 1

int good_interrupts = 0;
int handled_interrupts = 0;
int dropped_interrupts = 0;

int inited = false;

void timer_tick64(int signo, siginfo_t *siginfo, void *context);
void vtimer_tick(int signo, siginfo_t *siginfo, void *context);
void vtimer_reset(void);

/* defined in the start.c and end.c files respectively */
extern void proc_start();
extern void proc_end();

static sthread_ctx_start_func_t interruptHandler;
static int sthread_interrupts_enabled;
static struct itimerval sthread_period; // stores timer period
static const int WD_PERIOD = 500000; // watchdog period in usec.
static int sthread_watchdog_sleep;           // if 0, wd resets itimer_real

void sthread_print_stats() {
  printf("\ngood interrupts: %d\n", good_interrupts);
  printf("dropped interrupts: %d\n", dropped_interrupts);

  /* handled_interrupts is tracked, but not printed here. In general, the
   * handled_interrupts count is expected to be a few less than the
   * good_interrupts count: because handled_interrupts is incremented
   * AFTER interruptHandler() returns in timer_tick[64](), and because
   * interruptHandler() will cause the current thread (whose good_interrupts
   * was just incremented) to be suspended, handled_interrupts should
   * generally be less than good_interrupts by the number of threads that
   * have been created.
   */
#ifdef DEBUG_PREEMPT
  printf("handled interrupts: %d\n", handled_interrupts);
#endif
}

void sthread_init_stats() {
  int ret;
  struct sigaction sa;
  sigset_t mask;

  sa.sa_handler = (void(*)(int)) sthread_print_stats;  // NOLINT
  sa.sa_flags = 0;
  ret = sigemptyset(&mask);
  if (ret != 0) {
    perror("sigemptyset() failed");
    abort();
  }
  sa.sa_mask = mask;
  // allow getting interrupts statistics via ctrl-backslash
  ret = sigaction(SIGQUIT, &sa, NULL);
  if (ret != 0) {
    perror("sigaction(SIGQUIT) failed");
    abort();
  }
}

void debug_print_timer_val(const char *name) {
  int ret;
  struct itimerval it;

  ret = getitimer(ITIMER_REAL, &it);
  if (ret != 0) {
    perror("getitimer(ITIMER_REAL) failed");
    abort();
  }
  fprintf(stderr, "TIMER %s:\n\tinterval sec=%ld, usec=%ld\n\tvalue sec=%ld, "
          "usec=%ld\n", name, it.it_interval.tv_sec, it.it_interval.tv_usec,
          it.it_value.tv_sec, it.it_value.tv_usec);
}

/* Sets the timer value to the value saved in sthread_period, or, if it is 0,
 * then to the full interval. This function only makes sense after
 * sthread_timer_init() has first been called.
 *
 * According to setitimer(2), it seems like this function should be
 * unnecessary - if the itimer interval is set to non-zero, then the itimer
 * is supposed to reset infinitely as far as I can tell. However, I found
 * this to not be the case when the preemption period is set to a very small
 * value, which may cause SIGALRMs to be received repeatedly, e.g. while
 * the previous SIGALRM is still being handled or before interrupts can be
 * disabled or whatnot. Calling this function periodically seems to avoid
 * this problem. */
void sthread_timer_reset(void) {
  int ret;

  // Check that value isn't 0.  If it is, then do a full reset.  This
  // situation occurs when the interval timer was turned off in
  // timer_tick64() or if the interval timer wasn't properly reset.
  if (sthread_period.it_value.tv_sec == 0 &&
      sthread_period.it_value.tv_usec == 0) {
    sthread_period.it_value.tv_sec = sthread_period.it_interval.tv_sec;
    sthread_period.it_value.tv_usec = sthread_period.it_interval.tv_usec;
  }

  ret = setitimer(ITIMER_REAL, &sthread_period, NULL);
  if (ret != 0) {
    perror("setitimer(ITIMER_REAL) failed");
    abort();
  }
}

// See comments for sthread_timer_reset
void vtimer_reset(void) {
  int ret;
  struct itimerval it;

  it.it_interval.tv_sec = 0;
  it.it_interval.tv_usec = WD_PERIOD;
  it.it_value.tv_sec = 0;
  it.it_value.tv_usec = WD_PERIOD;
  ret = setitimer(ITIMER_VIRTUAL, &it, NULL);
  if (ret != 0) {
    perror("setitimer(ITIMER_VIRTUAL) failed");
    abort();
  }
  return;
}

void sthread_timer_init(sthread_ctx_start_func_t func, int period) {
  int ret;
  struct sigaction sa;
  struct sigaction virt_sa;
  sigset_t mask;
  sigset_t virt_mask;

  sthread_init_stats();
  interruptHandler = func;

  // interrupts are initially off
  sthread_interrupts_enabled = 0;
  sthread_watchdog_sleep = 0;

  // Save these values
  sthread_period.it_value.tv_sec = period/1000000;
  sthread_period.it_value.tv_usec = period%1000000;
  sthread_period.it_interval.tv_sec = period/1000000;
  sthread_period.it_interval.tv_usec = period%1000000;

  // Set up initial signal handler to just ignore SIGALRM.  This is needed in
  // case the signal fires before splx(LOW) has been called at the end of
  // sthread_preemption_init().
  /* See detailed comments in splx(). */
  // 1) register a system handler
  sa.sa_flags = SA_SIGINFO|SA_RESTART;
  sa.sa_sigaction = timer_tick64;
  sigemptyset(&mask);
  sigaddset(&mask, SIGVTALRM); // ignore sigvtalarm while handling alarm
  sa.sa_mask = mask;
  ret = sigaction(SIGALRM, &sa, NULL);
  if (ret != 0) {
    perror("sigaction(SIGALRM) failed");
    abort();
  }
  // 2) Start the interval timer
  sthread_timer_reset();

  // We'll use the virtual interval timer as a watchdog aganst anything funny
  // happening with the real (wall time) timer.
  // 1) Register a signal handler
  virt_sa.sa_flags = SA_SIGINFO|SA_RESTART;
  virt_sa.sa_sigaction = vtimer_tick;
  sigemptyset(&virt_mask);
  sigaddset(&virt_mask, SIGALRM); // ignore sigalarm while handling vtalarm
  virt_sa.sa_mask = virt_mask;
  ret = sigaction(SIGVTALRM, &virt_sa, NULL);
  if (ret != 0) {
    perror("sigaction(SIGVTALRM) failed");
    abort();
  }
  // 2) Activate the virtual timer to fire every 100ms
  vtimer_reset();
}

void vtimer_tick(int signo, siginfo_t *siginfo, void *context) {
  if (sthread_watchdog_sleep) {
    sthread_watchdog_sleep = 0; // wake up next time if not reset
  } else {
    fprintf(stderr,
            ".-------------------------------------------------------------------.\n"
            "| Warning: The watchdog timer has gone off.  You may have disabled  |\n"
            "| interrupts for longer than you should.  If you are 100%% sure your |\n"
            "| code works, then this may indicate a preemption error, so please  |\n"
            "| contact the TAs.                                                  |\n"
            "'-------------------------------------------------------------------'\n"
            );
    // Force a full reset
    sthread_period.it_value.tv_sec = sthread_period.it_interval.tv_sec;
    sthread_period.it_value.tv_sec = sthread_period.it_interval.tv_usec;
    sthread_timer_reset();
  }
}

#ifdef STHREAD_CPU_X86_64
void timer_tick64(int signo, siginfo_t *siginfo, void *context) {
  int ret;
  sigset_t mask;

  // Put the watchdog timer back to sleep
  sthread_watchdog_sleep = 1;

  if (!sthread_interrupts_enabled) {
    // Turn off the timer
    struct itimerval it;
    it.it_interval.tv_sec = 0;
    it.it_interval.tv_usec = 0;
    it.it_value.tv_sec = 0;
    it.it_value.tv_usec = 0;
    setitimer(ITIMER_REAL, &it, NULL);
    return;
  }

  /* See sigaction(2). ucontext_t is defined in /usr/include/sys/ucontext.h.
   * This code was inspired by
   * http://stackoverflow.com/questions/5397041/getting-the-saved-instruction-pointer-address-from-a-signal-handler.  NOLINT
   * PJH: I printed the ip value obtained here while running test-preempt.c
   * and confirmed that the ip is always somewhere within a range of around
   * 300 instructions, which matches the eip behavior on i386. I also
   * ran test-preempt in the debugger and confirmed that Xsthread_switch
   * and Xsthread_switch_end correctly represent the beginning and end of
   * the Xsthread_switch assembly code. */
  ucontext_t *uctx = (ucontext_t *)context;
  uint64_t ip = uctx->uc_mcontext.gregs[REG_RIP];

  /* Ensures that the pc is within our system code, not system code (libc): */
  if (ip >= (uint64_t) proc_start &&
      ip < (uint64_t) proc_end &&
      !(ip >= (uint64_t) Xsthread_switch &&
        ip < (uint64_t) Xsthread_switch_end)) {
    good_interrupts++;

#ifdef DEBUG_PREEMPT
    sthread_print_stats();
#endif

    /* Allow SIGALRM to be delivered during interruptHandler() if timer goes
     * off again - it was automatically set to blocked when this signal
     * handler was called.
     *
     * (PJH: I believe that this is done because it requires students to
     *  explicitly disable interrupts inside of their own interrupt handler
     *  function. This creates a race condition between the SIGALRM re-
     *  enabling here and the interrupt disabling in the interrupt handler
     *  function, but the effect of this should just be that timer_tick64()
     *  is called again. If the preemption period is set too low, then this
     *  could seemingly happen frequently; with a larger preemption period
     *  it should happen rarely, but the possibility is still present.)
     *
     * (jay: I think it's because their interrupt handler switches threads
     * and so this handler wouldn't return to re-enable SIGALRM and it
     * would just be disabled for the entire process.) */
    ret = sigemptyset(&mask);
    if (ret != 0) {
      perror("sigemptyset() failed");
      abort();
    }
    ret = sigaddset(&mask, SIGALRM);
    ret += sigaddset(&mask, SIGVTALRM);
    if (ret != 0) {
      perror("sigaddset() failed");
      abort();
    }
    ret = sigprocmask(SIG_UNBLOCK, &mask, NULL);
    if (ret != 0) {
      perror("sigprocmask() failed");
      abort();
    }
    interruptHandler();
    handled_interrupts++;
  } else {
    /* PJH: I ran test-preempt with a tiny preemption interval and printed
     * out the ip here, then used gdb to check what functions tend to be
     * running when interrupts are dropped (using the command "disas <ip>").
     * I saw the following functions:
     *   __memset_sse2 (many many times at the beginning)
     *   _IO_puts
     *   __write_nocancel (several times)
     *   __sigprocmask
     *   _IO_vfprintf_internal
     * All of these seem to make sense. */
    dropped_interrupts++;
#ifdef DEBUG_PREEMPT
    sthread_print_stats();
#endif

    /* NOTE: adding debugging printf statements right here seemed to
     * greatly exacerbate problems with the itimer not being reset when
     * the preemption period is very small. This would seem to suggest that
     * these problems are caused by SIGALRM being delivered AGAIN while this
     * signal handling function is still running, which perhaps causes the
     * itimer to not be reset as expected.
     *
     * Adding calls to sthread_timer_reset() seems to have eliminated these
     * problems, but keep this in mind for future debugging. */
  }
}

#else
/* signal handler */
void timer_tick(int sig, struct sigcontext scp) {
  /* Ensures that the pc is within our system code, not system code (libc).
   * The definition of struct sigcontext is in /usr/include/bits/sigcontext.h
   * According to sigaction(2), we're not supposed to be able to access the
   * sigcontext in this way anymore:
   *   Before the introduction of SA_SIGINFO it was also possible to get
   *   some additional information, namely by using a sa_handler with second
   *   argument of type struct sigcontext.  See the relevant kernel sources
   *   for details. This use is obsolete now.
   */
  if (scp.eip >= (uint64_t) proc_start &&
      scp.eip < (uint64_t) proc_end &&
      !(scp.eip >= (uint64_t) Xsthread_switch &&
        scp.eip < (uint64_t) Xsthread_switch_end)) {
    sigset_t mask, oldmask;
    good_interrupts++;
    sigemptyset(&mask);
    sigaddset(&mask, SIGALRM);
    sigprocmask(SIG_UNBLOCK, &mask, &oldmask);
    interruptHandler();
    handled_interrupts++;
  } else {
    dropped_interrupts++;
  }
}
#endif

void debug_print_sigaction(struct sigaction *sa, const char *name) {
  fprintf(stderr, "SIGACTION %s: sa_flags = 0x%" PRIx32 ", sa_sigaction = 0x"
          PRIx64 ", sa_handler = 0x%" PRIx64 " (SIG_IGN = 0x%" PRIx64 ")\n",
          name, (uint32_t) (sa->sa_flags), (uint64_t) (sa->sa_sigaction),
          (uint64_t) (sa->sa_handler), (uint64_t) SIG_IGN);
}

/* Turns interrupts ON and off
 * Returns the last state of the interrupts
 * LOW = interrupts ON
 * HIGH = interrupts OFF
 */
int splx(int splval) {
  struct itimerval it;
  int ret = sthread_interrupts_enabled;

  if (!inited) {
    fprintf(stderr, "splx() called before inited set to true!\n");
    abort();
  }

  if (splval == HIGH) {
    // Turn off interrupts.
    // First save the currentf value on the timer so that when interrupts
    // are turned back on, this time is used.  Then, flag that they should
    // be disabled next time SIGALRM is delivered.
    getitimer(ITIMER_REAL, &it);
    sthread_period.it_value.tv_sec = it.it_value.tv_sec;
    sthread_period.it_value.tv_usec = it.it_value.tv_usec;
    sthread_interrupts_enabled = 0;
  } else {
    // Turn on interrupts.
    // Set the enabled flag high and reset the timer.  Reset here will use
    // the values saved above so that no thread can hog all the time by
    // abusing functions that use splx internally.
    sthread_interrupts_enabled = 1;
    sthread_timer_reset();
  }
  return ret;
}

/* start preemption - func will be called every period microseconds */
void sthread_preemption_init(sthread_ctx_start_func_t func, int period) {
#ifndef DISABLE_PREEMPTION
  sthread_timer_init(func, period);
  inited = true;
  splx(LOW);
#endif
}


/*
 * atomic_test_and_set - using the native compare and exchange on the
 * Intel x86.
 *
 * Example usage:
 *
 *   lock_t mylock;
 *   while(atomic_test_and_set(&lock)) { } // spin
 *   _critical section_
 *   atomic_clear(&lock);
 */

#if defined(STHREAD_CPU_I386) || defined(STHREAD_CPU_X86_64)
int atomic_test_and_set(lock_t *l) {
  /* PJH: trying to understand this code... see
   *   http://ibiblio.org/gferg/ldp/GCC-Inline-Assembly-HOWTO.html
   * - AT&T syntax: op-code src dest. (Intel syntax: opcode dst src)
   * - Register names are prefixed by %
   * - Last character of op-code name determines size of operands: suffixes
   *   'b', 'w' and 'l' are byte (8-bit?), word (16-bit?), and long (32-bit?).
   *   In Intel syntax, operands are prefixed with 'byte ptr', 'word ptr',
   *   and 'dword ptr' instead.
   * - Indirect memory references: base register is inside of ().
   *   e.g. %eax is the value inside of register eax, while (%eax) is
   *   the value at the location POINTED TO by register eax! (To specify
   *   the register eax itself, i.e. as a destination, %%eax is used.)
   * - First argument in __asm__ is assembler template. %2 is the 3rd
   *   C expression (in parentheses), the literal value 1. (%3) is the
   *   value pointed to by the 4th C expression, l (the pointer to the
   *   lock_t!).
   * - Second argument is output operands. Each operand is an operand-
   *   constraint string followed by the C expression in parentheses.
   *   Operands are comma-separated. The 'a' (%0) is a "constraint"
   *   that tells GCC to store the operand in register eax. The '='
   *   is a "constraint modifier" that says that eax is the output operand
   *   and is write-only.
   * - Third argument is input operands. "a" (%1, eax) is "initialized"
   *   to 0 (this matters because the cmpxchg instruction has %eax as an
   *   implicit input operand).
   *   The "r" constraints specify that the input values (from the C
   *   expressions in parentheses) can be stored in any register.
   * - CMPXCHG has three operands: a source operand in a register, another
   *   source operand in EAX, and a destination operand. If the values
   *   contained in the DESTINATION operand and the EAX register are equal,
   *   then the destination operand is replaced with the value of the other
   *   (non-EAX) source operand. Otherwise, if the values in the dest and
   *   EAX are not equal, then the value of the dest operand is stored
   *   into EAX.
   *   (search for "Intel software developer manual" to find the manual
   *   that provides this information.)
   * - The "lock" prefix ensures that the instruction is performed
   *   atomically.
   *
   * So, finally, the code below atomically compares the value pointed at by
   * the lock_t *l argument with 0; if *l is 0 (unlocked), then *l is replaced
   * with 1 (locked), otherwise *l remains its original value (probably 1,
   * locked).
   *
   * FOR x86_64: lock_t is just an int, which is still 32-bits (4 bytes)
   * according to my test on a UW CSE Fedora15 Linux VM (1/29/12).
   * lock_t is only accessed/used in the simplethreads code by
   * atomic_test_and_set() and atomic_clear(), in sthread_preempt.h and
   * sthread_preempt.c. The locked (1) and unlocked (0) values for the
   * lock_t are specified as absolutes, so they will match any type.
   * The only question is if the 'l' suffix on cmpxchgl still matches
   * the int type of "(%3)" (which is *l) on x86_64. The answer to this
   * question probably lies with the GNU Assembler documentation, and
   * not the Intel software developer's manual...
   * This page (http://www.x86-64.org/documentation/assembly.html) says
   * that a 'q' suffix is used for a "quad-word" (64-bit) operand, so
   * 'l' should still refer to 32-bit integers...
   * To ensure that the operand and instruction widths match, we've
   * re-defined lock_t to explicitly be a 32-bit (unsigned) value,
   * rather than just an int.
   *
   * "info as" (as is the GNU assembler) says in section 9.13.3.1 that
   * "Mnemonic suffixes of `b', `w', `l' and `q' specify byte (8-bit),
   * word (16-bit), long (32-bit) and quadruple word (64-bit) memory
   * references.
   * Could also check out: http://en.wikibooks.org/wiki/X86_Assembly/GAS_Syntax
   */
  int val;
  __asm__ __volatile__("lock cmpxchgl %2, (%3)"
                       : "=a" (val)
                       : "a" (LOCK_UNLOCKED), "r" (LOCK_LOCKED), "r" (l));
  return val;
}


/*
 * atomic_clear - on the intel x86
 *
 */
void atomic_clear(lock_t *l) {
  /* See the description of atomic_test_and_set(): *l is 0 when
   * unlocked, and *l is 1 when locked. To unlock, we simply set
   * *l to 0; this does not have to be atomic.
   */
  *l = LOCK_UNLOCKED;
}
#endif  // (STHREAD_CPU_I386 || STHREAD_CPU_X86_64)
