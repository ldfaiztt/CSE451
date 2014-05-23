/*
 * test-cond.c - Simple test of condition variables.
 *
 * Implements a producer/consumer with 2 consumer threads to
 * transfer 100 items.
 *
 * NOTE: this test is known to fail occasionally (maybe 5% or less?)
 * when configured with pthreads on x86_64 (not sure why this wasn't
 * noticed with i386); I guess it's hard to create a program that
 * tests if condition variables are working while still finishing
 * and failing if they are not.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <sthread.h>

#define MAXTHREADS 10
static int num_threads = 2;  // haven't tested with anything else...
static const int max_transfer = 100;
static int transfered = 0;
static int waiting = 0;
static sthread_mutex_t mutex;
static sthread_cond_t avail_cond;

void *thread_start(void *arg);

int main(int argc, char **argv) {
  int sent, checks, i;
  sthread_t child[MAXTHREADS];


  printf("Testing sthread_cond_*, impl: %s\n",
         (sthread_get_impl() == STHREAD_PTHREAD_IMPL) ? "pthread" : "user");
  assert(num_threads <= MAXTHREADS);

  sthread_init();

  mutex = sthread_mutex_init();
  avail_cond = sthread_cond_init();

  sthread_mutex_lock(mutex);

  for (i = 0; i < num_threads; i++) {
    child[i] = sthread_create(thread_start, NULL, 1);
    if (child[i] == NULL) {
      printf("sthread_create %d failed\n", i);
      exit(1);
    }
  }

  assert(transfered == 0);

  /* This should let the other thread run at some point. */
  sthread_mutex_unlock(mutex);

  /* Send a bunch of things for the other threads to take */
  sent = 0;
  while (sent < max_transfer) {
    sthread_mutex_lock(mutex);
    waiting++;
    sent++;
    sthread_cond_signal(avail_cond);
    sthread_mutex_unlock(mutex);
    sthread_yield();
  }

  printf("Sent %d\n", sent);

  /* Now give the other threads 100 tries to get
   * them all across. We assume that's enough
   * for the sake of not running this test forever. */
  checks = 10000;  // arbitrary??
  while (checks > 0) {
    sthread_mutex_lock(mutex);
    if (transfered != max_transfer) {
      checks--;
    } else {
      /* broadcast to let the consumers know we've
       * finished, so they can exit
       * (othrewise, they may still be holding the lock
       * when we try to free it below) */
      sthread_cond_broadcast(avail_cond);
      checks = -1;
    }
    sthread_mutex_unlock(mutex);
    sthread_yield();
  }

  if (checks == -1) {
    /* Wait for child threads to finish, otherwise we could try to
     * free the mutex before they've unlocked it! */
    printf("joining on children\n");
    for (i = 0; i < num_threads; i++) {
      sthread_join(child[i]);
      printf("joined with child %d\n", i);
    }
    printf("sthread_cond passed\n");
  } else {
    printf("*** sthread_cond failed\n");
    /* If we failed, don't bother joining on threads. */
  }

  sthread_mutex_free(mutex);
  sthread_cond_free(avail_cond);
  return 0;
}

/* Consumer thread - remove items from the "buffer"
 * until they've all been transfered. */
void *thread_start(void *arg) {
  sthread_mutex_lock(mutex);

  while (transfered < max_transfer) {
    /* Wait for main() to put something up for us to take */
    while (transfered < max_transfer && waiting == 0) {
      sthread_cond_wait(avail_cond, mutex);

      /* This isn't part of using cond vars, but
       * helps the test fail quickly if they aren't
       * working properly: */
      sthread_mutex_unlock(mutex);
      sthread_yield();
      sthread_mutex_lock(mutex);
    }
    /* Either there is something waiting, or we've finished */

    if (waiting != 0) {
      /* Take it */
      waiting--;
      transfered++;
    }
  }

  sthread_mutex_unlock(mutex);

  return 0;
}
