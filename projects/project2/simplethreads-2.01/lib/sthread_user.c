/* Simplethreads Instructional Thread Package
 * 
 * sthread_user.c - Implements the sthread API using user-level threads.
 *
 *    You need to implement the routines in this file.
 *
 * Change Log:
 * 2002-04-15        rick
 *   - Initial version.
 */

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include <sthread.h>
#include <sthread_queue.h>
#include <sthread_user.h>
#include <sthread_ctx.h>
#include <sthread_user.h>
#include <sthread_preempt.h>

static const int TIMEOUT = 20;

struct _sthread {
  sthread_ctx_t *saved_ctx;
  int tid;
  sthread_start_func_t start_routine;
  void *start_routine_args;
  void *ret_val;
  bool joinable;
  struct _sthread *join_caller;
  bool has_terminated;
};

static unsigned int tid_counter = 0;
static bool init_called = false;
static sthread_t running_thread;
static sthread_queue_t ready_queue;
static sthread_queue_t dead_queue;
static sthread_t dead_thread;

/*********************************************************************/
/* Part 1: Creating and Scheduling Threads                           */
/*********************************************************************/
void sthread_user_init(void) {
  sthread_t main_thread = (sthread_t) malloc(sizeof(struct _sthread));
  if (main_thread == NULL) {
    printf("Error: cannot create main thread.\n");
    exit(EXIT_FAILURE);
  }

  main_thread->saved_ctx = sthread_new_blank_ctx();
  main_thread->tid = tid_counter;
  main_thread->start_routine = NULL;
  main_thread->start_routine_args = NULL;
  main_thread->ret_val = NULL;
  main_thread->joinable = false;
  main_thread->join_caller = NULL;
  main_thread->has_terminated = false;

  tid_counter++;
  running_thread = main_thread;
  ready_queue = sthread_new_queue();
  dead_queue = sthread_new_queue();
  dead_thread = NULL;
  init_called = true;

  sthread_preemption_init(sthread_user_yield, TIMEOUT);
}

void sthread_run(void) {
  int old = splx(LOW);
  running_thread->ret_val = running_thread->start_routine(running_thread->start_routine_args);
  splx(old);
  sthread_user_exit(running_thread->ret_val);
}

void sthread_cleanup(void) {
  int old = splx(HIGH);
  if (sthread_queue_is_empty(ready_queue)) {
    sthread_free_queue(ready_queue);

    if (dead_thread != NULL) {
      if (dead_thread->saved_ctx != NULL) { 
        sthread_free_ctx(dead_thread->saved_ctx);
        dead_thread->saved_ctx = NULL;
      }
      sthread_enqueue(dead_queue, dead_thread);
      dead_thread = NULL;
    }
  
    while (!sthread_queue_is_empty(dead_queue)) {
      free(sthread_dequeue(dead_queue));
    }
    sthread_free_queue(dead_queue);

    sthread_queue_clear_free_list();
  }
  splx(old);
}

sthread_t sthread_user_create(sthread_start_func_t start_routine, void *arg,
    int joinable) {
  if (!init_called) {
    printf("sthread_init hasn't been called yet.\n");
    return NULL;
  }

  if (start_routine == NULL) {
    printf("start_routine cannot be NULL.\n");
    return NULL;
  }

  sthread_t new_thread = (sthread_t) malloc(sizeof(struct _sthread));
  if (new_thread == NULL) {
    printf("Unable to create new thread.\n");
    return NULL;
  }

  new_thread->saved_ctx = sthread_new_ctx(sthread_run);
  new_thread->start_routine = start_routine;
  new_thread->start_routine_args = arg;
  new_thread->ret_val = NULL;
  new_thread->joinable = joinable;
  new_thread->join_caller = NULL;
  new_thread->has_terminated = false;

  int old = splx(HIGH);
  new_thread->tid = tid_counter++;
  sthread_enqueue(ready_queue, new_thread);
  splx(old);

  return new_thread;
}

void sthread_user_exit(void *ret) {
  if (!init_called) {
    printf("sthread_init hasn't been called yet.\n");
    exit(EXIT_FAILURE);
  }

  int old = splx(HIGH);

  // Free the previously terminated thread's context
  if (dead_thread != NULL) {
    if (dead_thread->saved_ctx != NULL) {
      sthread_free_ctx(dead_thread->saved_ctx);
      dead_thread->saved_ctx = NULL;
    }
    sthread_enqueue(dead_queue, dead_thread);
  }

  // Thread is exiting
  running_thread->has_terminated = true;
  dead_thread = running_thread;

  // Check if there is another blocked thread waiting to join
  // on this thread
  if (dead_thread->join_caller != NULL) {
    running_thread = dead_thread->join_caller;
    dead_thread->join_caller = NULL;
    splx(old);
    sthread_switch(dead_thread->saved_ctx, running_thread->saved_ctx);
    //    splx(old);

  // No thread is waiting to join on this thread, and there are
  // threads remaining on the ready queue, dispatch it
  } else if (!sthread_queue_is_empty(ready_queue)) {
    running_thread = sthread_dequeue(ready_queue);
    splx(old);
    sthread_switch(dead_thread->saved_ctx, running_thread->saved_ctx);
    //    splx(old);

  // No thread is waiting to join, and the ready queue is empty,
  // which means this is the main thread. Just free everything
  // and exit.
  } else {
    sthread_cleanup();
    sthread_free_ctx(running_thread->saved_ctx);
    free(running_thread);

    tid_counter = 0;
    init_called = false;
    running_thread = NULL;
    ready_queue = NULL;
    dead_queue = NULL;
    dead_thread = NULL;

    exit(*((int *)ret));
  }
}

void* sthread_user_join(sthread_t t) {
  if (!init_called) {
    printf("sthread_init hasn't been called yet.\n");
    return NULL;
  }

  if (t == NULL || !(t->joinable)) {
    return NULL;
  }

  int old = splx(HIGH);
  if (!t->has_terminated) {
    t->join_caller = running_thread;
    running_thread = sthread_dequeue(ready_queue);
    sthread_switch(t->join_caller->saved_ctx, running_thread->saved_ctx);
  }

  if (t->saved_ctx != NULL) {
    sthread_free_ctx(t->saved_ctx);
    t->saved_ctx = NULL;
  }

  splx(old); // enable interrupt down here to avoid double free

  return t->ret_val;
}

void sthread_user_yield(void) {
  if (!init_called) {
    printf("sthread_init hasn't been called yet.\n");
    exit(EXIT_FAILURE);
  }

  int old = splx(HIGH);

  if (!sthread_queue_is_empty(ready_queue)) {
    sthread_t old_thread = running_thread;
    sthread_enqueue(ready_queue, running_thread);
    running_thread = sthread_dequeue(ready_queue);
    sthread_switch(old_thread->saved_ctx, running_thread->saved_ctx);
  } else {
    if (dead_thread != NULL) {
      if (dead_thread->saved_ctx != NULL) {
	sthread_free_ctx(dead_thread->saved_ctx);
	dead_thread->saved_ctx = NULL;
      }
      sthread_enqueue(dead_queue, dead_thread);
      dead_thread = NULL;
    }
  }

  splx(old);
}

/*********************************************************************/
/* Part 2: Synchronization Primitives                                */
/*********************************************************************/

struct _sthread_mutex {
  lock_t mutex_lock;
  int tid;                        // tid of the thread holding the lock
  sthread_queue_t blocked_queue;  // a queue of blocked threads waiting for the lock
};

sthread_mutex_t sthread_user_mutex_init() {
  sthread_mutex_t ret = (sthread_mutex_t) malloc(sizeof(struct _sthread_mutex));

  if (ret != NULL) {
    ret->mutex_lock = 0;
    ret->tid = -1;
    ret->blocked_queue = sthread_new_queue();
  }

  return ret;
}

void sthread_user_mutex_free(sthread_mutex_t lock) {
  if (lock != NULL) {
    sthread_free_queue(lock->blocked_queue);
    free(lock);
  }
}

void sthread_user_mutex_lock(sthread_mutex_t lock) {
  if (lock == NULL) {
    return;
  }

  while (atomic_test_and_set(&(lock->mutex_lock))) {}

  while (lock->tid != -1) {
    int old = splx(HIGH);
    sthread_t old_thread = running_thread;
    sthread_enqueue(lock->blocked_queue, running_thread);
    atomic_clear(&(lock->mutex_lock));
    running_thread = sthread_dequeue(ready_queue);
    sthread_switch(old_thread->saved_ctx, running_thread->saved_ctx);
    splx(old);
    while (atomic_test_and_set(&(lock->mutex_lock))) {}
  }

  int old = splx(HIGH);
  lock->tid = running_thread->tid;
  splx(old);
  atomic_clear(&(lock->mutex_lock));
}

void sthread_user_mutex_unlock(sthread_mutex_t lock) {
  if (lock == NULL) {
    return;
  }

  while (atomic_test_and_set(&(lock->mutex_lock))) {}

  if (lock->tid != running_thread->tid) {
    atomic_clear(&(lock->mutex_lock));
    return;
  }

  // If the blocked queue is not empty, there is at least 1 thread
  // waiting to execute. Put it into the ready thread so it can
  // be dispatched some time
  if (!sthread_queue_is_empty(lock->blocked_queue)) {
    sthread_t thread = sthread_dequeue(lock->blocked_queue);
    int old = splx(HIGH);
    sthread_enqueue(ready_queue, thread);
    splx(old);
  }

  lock->tid = -1;
  atomic_clear(&(lock->mutex_lock));
}


struct _sthread_cond {
  lock_t cond_lock;
  sthread_queue_t cond_queue;
};

sthread_cond_t sthread_user_cond_init(void) {
  sthread_cond_t ret = (sthread_cond_t) malloc(sizeof(struct _sthread_cond));

  if (ret != NULL) {
    ret->cond_lock = 0;
    ret->cond_queue = sthread_new_queue();
  }

  return ret;
}

void sthread_user_cond_free(sthread_cond_t cond) {
  if (cond == NULL) {
    return;
  }

  sthread_free_queue(cond->cond_queue);
  free(cond);
}

void sthread_user_cond_signal(sthread_cond_t cond) {
  if (cond == NULL) {
    return;
  }

  while (atomic_test_and_set(&(cond->cond_lock))) {}
  if (sthread_queue_is_empty(cond->cond_queue)) {
    atomic_clear(&(cond->cond_lock));
    return;
  }
  sthread_t released_thread = sthread_dequeue(cond->cond_queue);
  atomic_clear(&(cond->cond_lock));

  int old = splx(HIGH);
  sthread_enqueue(ready_queue, released_thread);
  splx(old);
}

void sthread_user_cond_broadcast(sthread_cond_t cond) {
  if (cond == NULL) {
    return;
  }

  while (atomic_test_and_set(&(cond->cond_lock))) {}
  sthread_t released_thread;
  while (!sthread_queue_is_empty(cond->cond_queue)) {
    released_thread = sthread_dequeue(cond->cond_queue);
    int old = splx(HIGH);
    sthread_enqueue(ready_queue, released_thread);
    splx(old);
  }
  atomic_clear(&(cond->cond_lock));
}

void sthread_user_cond_wait(sthread_cond_t cond, sthread_mutex_t lock) {
  if (cond == NULL || lock == NULL) {
    return;
  }

  sthread_user_mutex_unlock(lock);

  while (atomic_test_and_set(&(cond->cond_lock))) {}
  int old = splx(HIGH);
  sthread_enqueue(cond->cond_queue, running_thread);
  atomic_clear(&(cond->cond_lock));

  sthread_t wait_thread = running_thread;
  sthread_t thread = sthread_dequeue(ready_queue);
  running_thread = thread;
  sthread_switch(wait_thread->saved_ctx, running_thread->saved_ctx);
  splx(old);

  sthread_user_mutex_lock(lock);
}
