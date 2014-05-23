/* Simple test of thread create
 */

#include <stdio.h>
#include <stdlib.h>
#include <sthread.h>

void *thread_start(void *arg);

int success = 0;

int main(int argc, char **argv) {
  int i;
  int *arg = malloc(sizeof(*arg));
  if (!arg) {
    printf("error: malloc failed\n");
    exit(1);
  }
  *arg = 1;

  printf("Testing sthread_create, impl: %s\n",
         (sthread_get_impl() == STHREAD_PTHREAD_IMPL) ? "pthread" : "user");

  sthread_init();

  if (sthread_create(thread_start, (void *)arg, 0) == NULL) {
    printf("sthread_create failed\n");
    exit(1);
  }

  /* Without using other thread primitives (which we don't want to
   * rely on for this first test), we can't know for sure that the
   * child thread runs and completes if we yield just once (in fact,
   * with x86_64 pthreads the child almost never completes after the
   * main thread yields just once). So, we yield an arbitrary number
   * of times here before exiting (100 isn't always enough, but 1000
   * seems to be). Even if the child thread doesn't finish running by
   * the time we're done looping, the success/fail of this test shouldn't
   * change, but the output may appear in an unexpected order. */
  for (i = 0; i < 1000; i++) {
    sthread_yield();
  }
  printf("back in main\n");

  free(arg);
  return 0;
}

void *thread_start(void *arg) {
  if (arg) {
    printf("In thread_start, arg = %d\n", *(int *)arg);
  }
  return 0;
}
