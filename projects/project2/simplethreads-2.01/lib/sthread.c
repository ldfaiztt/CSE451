/*
 * sthread.c - Implements the public API (the functions defined in
 *             include/sthread.h). Since sthreads supports two implementations
 *             (pthreads and student supplied user-level threads), this
 *             just consists of dispatching the calls to the implementation
 *             that the application choose.
 *
 */

#include <config.h>

#include <assert.h>

#include <sthread.h>
#include <sthread_pthread.h>
#include <sthread_user.h>

#ifdef USE_PTHREADS
#define IMPL_CHOOSE(pthread, user) pthread
#else
#define IMPL_CHOOSE(pthread, user) user
#endif

void sthread_init(void) {
  IMPL_CHOOSE(sthread_pthread_init(), sthread_user_init());
}

sthread_t sthread_create(sthread_start_func_t start_routine, void *arg,
                         int joinable) {
  sthread_t newth;
  IMPL_CHOOSE(newth = sthread_pthread_create(start_routine, arg, joinable),
              newth = sthread_user_create(start_routine, arg, joinable));
  return newth;
}

void sthread_exit(void *ret) {
  IMPL_CHOOSE(sthread_pthread_exit(ret), sthread_user_exit(ret));
}

void sthread_yield(void) {
  IMPL_CHOOSE(sthread_pthread_yield(), sthread_user_yield());
}

void* sthread_join(sthread_t t) {
  void *retptr;
  IMPL_CHOOSE(retptr = sthread_pthread_join(t),
              retptr = sthread_user_join(t));
  return retptr;
}

/**********************************************************************/
/* Synchronization Primitives: Mutexs and Condition Variables         */
/**********************************************************************/


sthread_mutex_t sthread_mutex_init() {
  sthread_mutex_t lock;
  IMPL_CHOOSE(lock = sthread_pthread_mutex_init(),
              lock = sthread_user_mutex_init());
  return lock;
}

void sthread_mutex_free(sthread_mutex_t lock) {
  IMPL_CHOOSE(sthread_pthread_mutex_free(lock),
              sthread_user_mutex_free(lock));
}

void sthread_mutex_lock(sthread_mutex_t lock) {
  IMPL_CHOOSE(sthread_pthread_mutex_lock(lock),
              sthread_user_mutex_lock(lock));
}

void sthread_mutex_unlock(sthread_mutex_t lock) {
  IMPL_CHOOSE(sthread_pthread_mutex_unlock(lock),
              sthread_user_mutex_unlock(lock));
}


sthread_cond_t sthread_cond_init(void) {
  sthread_cond_t cond;
  IMPL_CHOOSE(cond = sthread_pthread_cond_init(),
              cond = sthread_user_cond_init());
  return cond;
}

void sthread_cond_free(sthread_cond_t cond) {
  IMPL_CHOOSE(sthread_pthread_cond_free(cond),
              sthread_user_cond_free(cond));
}

void sthread_cond_signal(sthread_cond_t cond) {
  IMPL_CHOOSE(sthread_pthread_cond_signal(cond),
              sthread_user_cond_signal(cond));
}

void sthread_cond_broadcast(sthread_cond_t cond) {
  IMPL_CHOOSE(sthread_pthread_cond_broadcast(cond),
              sthread_user_cond_broadcast(cond));
}

void sthread_cond_wait(sthread_cond_t cond, sthread_mutex_t lock) {
  IMPL_CHOOSE(sthread_pthread_cond_wait(cond, lock),
              sthread_user_cond_wait(cond, lock));
}
