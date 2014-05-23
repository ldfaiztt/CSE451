#ifndef STHREAD_PREEMPT
#define STHREAD_PREEMPT

#include <sthread_ctx.h>
#include <stdint.h>

#define HIGH 0
#define LOW  1

/* Explicitly use a 32-bit unsigned value for the lock, rather than just
 * an int, because we're going to be using this with assembly instructions
 * and we want to know exactly how wide it is:
 */
typedef uint32_t lock_t;


/* start preemption - func will be called every period microseconds */
void sthread_preemption_init(sthread_ctx_start_func_t func, int period);

/* Turns inturrupts ON and off 
 * Returns the last state of the inturrupts
 * LOW = inturrupts ON
 * HIGH = inturrupts OFF
 */
int splx(int splval);

/*
 * atomic_test_and_set - using the native compare and exchange on the 
 * Intel x86.
 *
 * Example usage:
 *
 *   lock_t lock;
 *   while(atomic_test_and_set(&lock)) { } // spin
 *   _critical section_
 *   atomic_clear(&lock); 
 */

int atomic_test_and_set(lock_t *l);
void atomic_clear(lock_t *l);


/*
 * sthread_print_stats - prints out the number of drupped interrupts
 *   and "successful" interrupts
 */
void sthread_print_stats();

#endif  // STHREAD_PREEMPT
