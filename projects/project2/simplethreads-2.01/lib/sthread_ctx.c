/* sthread_ctx.c - Support for creating and switching thread contexts.
 *
 */

#include <config.h>

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>

#include <sthread_ctx.h>

#ifdef STHREAD_CPU_I386
#include "sthread_switch_i386.h"
#endif

#ifdef STHREAD_CPU_X86_64
#include "sthread_switch_x86_64.h"
#endif

#ifdef STHREAD_CPU_POWERPC
#include "sthread_switch_powerpc.h"
#endif


/* Stack size is used to allocate a region of memory for a thread's stack
 * in sthread_new_ctx. pthread_create(3) says that it uses a default stack
 * size of 2 MB, unless this is limited by the RLIMIT_STACK soft resource
 * limit. ulimit -s on the UW CSE lab VMs says that this limit is 8192 *
 * 1024 bytes (8 MB). We use 2 MB here to match pthreads.
 */
const size_t sthread_stack_size = 2 * 1024 * 1024;

static void sthread_init_stack(sthread_ctx_t *ctx,
                               sthread_ctx_start_func_t func);

sthread_ctx_t *sthread_new_ctx(sthread_ctx_start_func_t func) {
  sthread_ctx_t *ctx;

  ctx = (sthread_ctx_t*)malloc(sizeof(sthread_ctx_t));
  if (ctx == NULL) {
    fprintf(stderr, "Out of memory (sthread_new_ctx)\n");
    return NULL;
  }

  ctx->stackbase = (char*)malloc(sthread_stack_size);
  if (ctx->stackbase == NULL) {
    free(ctx);
    fprintf(stderr, "Out of memory (sthread_new_ctx)\n");
    return NULL;
  }

  /* The stack grows down (towards lower memory addresses), so the first
   * SP is at the top (highest memory address). The stack pointer is
   * decremented before an item is pushed onto the stack, and is
   * incremented after an item is popped from the stack.
   * Why do we subtract 16 here? Not sure (this is left over from
   * i386 code), but I don't think it makes any big difference, except
   * for reducing the size of the stack by 16 bytes.
   */
  ctx->sp = ctx->stackbase + sthread_stack_size - 16;

  sthread_init_stack(ctx, func);

  return ctx;
}

/* Initialize a stack as if it had been saved by sthread_switch. */
static void sthread_init_stack(
    sthread_ctx_t *ctx, sthread_ctx_start_func_t func) {
  memset(ctx->stackbase, 0, sthread_stack_size);

  /* Push the address of the thread's starting function onto the stack
   * (decrement the stack pointer, then store the item). This will
   * become the initial stack frame, with the return instruction pointer
   * at the top of the frame.
   * For more information about stack layout, see section 6.2 (Stacks)
   * of the Intel Software Developer's Manual:
   * http://www.intel.com/content/www/us/en/processors/architectures-software-developer-manuals.html
   */
  ctx->sp -= sizeof(sthread_ctx_start_func_t);
  *((sthread_ctx_start_func_t*)ctx->sp) = func;

  /* Leave room for the values pushed on the stack by the "save" half
   * of _sthread_switch. The amount of room varies between CPUs, so we
   * get this value from the architecture-specific header file. */
  ctx->sp -= STHREAD_CONTEXT_SIZE;
}

/* Create a new sthread_ctx_t, but don't initialize it.
 * This new sthread_ctx_t is suitable for use as 'old' in
 * a call to sthread_switch, since sthread_switch is defined to overwrite
 * 'old'. It should not be used as 'new' until it has been initialized.
 */
sthread_ctx_t *sthread_new_blank_ctx() {
  sthread_ctx_t *ctx;
  ctx = (sthread_ctx_t*)malloc(sizeof(sthread_ctx_t));
  if (ctx == NULL) {
    fprintf(stderr, "Out of memory (sthread_new_ctx)\n");
    return NULL;
  }

  /* Put some bogus values in */
  ctx->sp = (char*)0xbeefcafe;
  ctx->stackbase = NULL;
  return ctx;
}

/* Free resources used by given (not currently running) context. */
void sthread_free_ctx(sthread_ctx_t *ctx) {
  if (ctx->stackbase) {
    free(ctx->stackbase);
  }
  ctx->stackbase = (char*)0xdeaddead;
  ctx->sp = (char*)0xdeaddead;
  free(ctx);
}

/* Avoid allowing the compiler to optimize the call to
 * Xsthread_switch as a tail-call on architectures that support
 * that (powerpc). */
void sthread_anti_optimize(void) __attribute__((noinline));
void sthread_anti_optimize() {
}

/* Save the currently running thread context into old, and
 * start running the context new. Old may be uninitialized,
 * but new must contain a valid saved context. */
void sthread_switch(sthread_ctx_t *old, sthread_ctx_t *new) {
  /* Call the assembly: */
  if (old != new) {
    Xsthread_switch(&(old->sp), new->sp);
  }

  /* Do not put anything useful here. In some cases (namely, the
   * first time a new thread is run), _sthread_switch does
   * not return to this line. (It instead returns to the
   * start function set when the stack was initialized.)
   *
   * sthread_anti_optimize just forces gcc not to optimize
   * the call to Xsthread_switch.
   */
  sthread_anti_optimize();
}
