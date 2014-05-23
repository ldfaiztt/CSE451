/* Simplethreads Instructional Thread Package
 * Available under the terms of the GNU General Public License.
 * Copyright (c) University of Washington 2002
 * 
 * sthread_ctx.h - Private (for use by your implementation of the
 *                 sthread library itself, but not for applications
 *                 directly) interface to the sthread_ctx_t Abstract
 *                 Data Type.
 */

#ifndef STHREAD_CTX_H
#define STHREAD_CTX_H 1

#include <sthread.h>

typedef struct _sthread_ctx {
  // Bottom of the stack
  char *stackbase;
  // Current stackpointer (if thread is not running).
  // Initialized to stackbase + sthread_stack_size.
  char *sp;
} sthread_ctx_t;

typedef void (*sthread_ctx_start_func_t)(void);

/* Make a new context. Note the sthread_ctx_start_func_t is not
 * the same as the sthread_start_func_t; the former takes no arguments
 * and returns nothing, while the later is takes/returns a void*.
 */
sthread_ctx_t *sthread_new_ctx(sthread_ctx_start_func_t func);

/* Create a new sthread_ctx_t, but don't initialize it.
 * This new sthread_ctx_t is suitable for use as 'old' in
 * a call to sthread_switch, since sthread_switch is defined to overwrite
 * 'old'. It should not be used as 'new' until it has been initialized.
 */
sthread_ctx_t *sthread_new_blank_ctx();

/* Free the resources used by the given context. The passed
 * context should not be the currently active context. */
void sthread_free_ctx(sthread_ctx_t *ctx);

void sthread_switch(sthread_ctx_t *old, sthread_ctx_t *new);

#endif /* STHREAD_CTX_H */
