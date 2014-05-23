/*
 * sthread_user.h - This file defines the user-level thread
 *                  implementation of sthreads. The routines
 *                  are all described in the sthread.h file.
 *
 */

#ifndef STHREAD_USER_H
#define STHREAD_USER_H 1

/* Part 1: Basic Threads */
void sthread_user_init(void);
sthread_t sthread_user_create(sthread_start_func_t start_routine, void *arg,
                              int joinable);
void sthread_user_exit(void *ret);
void sthread_user_yield(void);
void* sthread_user_join(sthread_t t);

/* Part 2: Synchronization Primitives */
sthread_mutex_t sthread_user_mutex_init(void);
void sthread_user_mutex_free(sthread_mutex_t lock);
void sthread_user_mutex_lock(sthread_mutex_t lock);
void sthread_user_mutex_unlock(sthread_mutex_t lock);

sthread_cond_t sthread_user_cond_init(void);
void sthread_user_cond_free(sthread_cond_t cond);
void sthread_user_cond_signal(sthread_cond_t cond);
void sthread_user_cond_broadcast(sthread_cond_t cond);
void sthread_user_cond_wait(sthread_cond_t cond,
                            sthread_mutex_t lock);

#endif /* STHREAD_USER_H */
