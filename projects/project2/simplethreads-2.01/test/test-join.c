/* Simple test of thread create
 */

#include <stdio.h>
#include <stdlib.h>
#include <sthread.h>

void *thread_start(void *arg);

#define MAXTHREADS 10
int nThreads = 3;

int main(int argc, char **argv) {
  int n;
  int *arg[MAXTHREADS];
  int *retptr;
  sthread_t child[MAXTHREADS];

  printf("Testing sthread_create, impl: %s\n",
         (sthread_get_impl() == STHREAD_PTHREAD_IMPL) ? "pthread" : "user");

  if ( argv[1] ) nThreads = atoi(argv[1]);
  if ( nThreads > MAXTHREADS ) nThreads = MAXTHREADS;
  printf("Creating %d threads\n", nThreads);

  sthread_init();

  for (n = 0; n < nThreads; ++n) {
    arg[n] = malloc(sizeof(arg[n]));
    if (!arg[n]) {
      printf("malloc() failed\n");
      exit(1);
    }
    *(arg[n]) = n;

    child[n] = sthread_create(thread_start, (void*) arg[n], 1);
    if (child[n] == NULL) {
      printf("sthread_create failed\n");
      exit(1);
    }
  }

  for (n = nThreads - 1; n >= 0; --n) {
    retptr = (int *)sthread_join(child[n]);
    printf("main: joined with %d: %d\n", n, *retptr);
    free(arg[n]);
  }

  return 0;
}

void *thread_start(void *arg) {
  int argint = *(int *)arg;
  printf("\tChild in thread_start, arg = %d\n", argint);
  *(int *)arg = 0 - argint;
  return arg;
}
