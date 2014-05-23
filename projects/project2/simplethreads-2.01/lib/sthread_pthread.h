/*
 * sthread_pthread.h - This file defines the pthreads (presumed kernel
 *                     thread) implementation of sthreads. We provide
 *                     this implementation in sthread_pthread.c.
 *                     The routines are described in the sthread.h file.
 *
 */

#ifndef STHREAD_PTHREAD_H
#define STHREAD_PTHREAD_H 1

void sthread_pthread_init(void);
sthread_t sthread_pthread_create(
    sthread_start_func_t start_routine, void *arg, int joinable);
void sthread_pthread_exit(void *ret);
void sthread_pthread_yield(void);
void* sthread_pthread_join(sthread_t t);

sthread_mutex_t sthread_pthread_mutex_init(void);
void sthread_pthread_mutex_free(sthread_mutex_t lock);
void sthread_pthread_mutex_lock(sthread_mutex_t lock);
void sthread_pthread_mutex_unlock(sthread_mutex_t lock);
sthread_cond_t sthread_pthread_cond_init(void);
void sthread_pthread_cond_free(sthread_cond_t cond);
void sthread_pthread_cond_signal(sthread_cond_t cond);
void sthread_pthread_cond_broadcast(sthread_cond_t cond);
void sthread_pthread_cond_wait(
    sthread_cond_t cond, sthread_mutex_t lock);

#endif /* STHREAD_PTHREAD_H */
