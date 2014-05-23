/* 
 * sthread.h - The public interface to simplethreads, used by
 *             applications.
 *
 */

#ifndef STHREAD_H
#define STHREAD_H 1

/* Define the sthread_t type (a pointer to an _sthread structure)
 * without knowing how it is actually implemented (that detail is
 * hidden from the public API).
 */
typedef struct _sthread *sthread_t;

/* New threads always begin life in a routine of this type */
typedef void *(*sthread_start_func_t)(void *);

/* Sthreads supports multiple implementations, so that one can test
 * programs with different kinds of threads (e.g. compare kernel to user
 * threads). This enum represents an implementation choice. (Which
 * is actually a compile time choice.)
 */
typedef enum { STHREAD_PTHREAD_IMPL, STHREAD_USER_IMPL } sthread_impl_t;

/* Return the implementation that was selected at compile time. */
sthread_impl_t sthread_get_impl(void);

/* 
 * Perform any initialization needed. Should be called exactly
 * once, before any other sthread functions (sthread_get_impl 
 * excepted).
 */
void sthread_init();

/* Create a new thread starting at the routine given, which will
 * be passed arg. The new thread does not necessarily execute immediatly
 * (as in, sthread_create shouldn't force a switch to the new thread).
 * If the thread will be joined, the joinable flag should be set. 
 * Otherwise, it should be 0.
 */
sthread_t sthread_create(sthread_start_func_t start_routine, void *arg,
		int joinable);

/* Exit the calling thread with return value ret.
 * Note: In this version of simplethreads, there is no way
 * to retrieve the return value.
 */
void sthread_exit(void *ret);

/* Voluntarily yield the CPU to another waiting
 * thread (or, possibly, another process).
 */
void sthread_yield(void);

/* Wait until the specified thread has exited.
 * Returns the value returned by that thread's
 * start function.  Results are undefined if
 * if the thread was not created with the joinable
 * flag set or if it has already been joined.
 */
void* sthread_join( sthread_t t);

/**********************************************************************/
/* Synchronization Primitives: Mutexs and Condition Variables         */
/**********************************************************************/

typedef struct _sthread_mutex *sthread_mutex_t;

/* Return a new, unlocked mutex */
sthread_mutex_t sthread_mutex_init();

/* Free a no-longer needed mutex.
 * Assume mutex has no waiters. */
void sthread_mutex_free(sthread_mutex_t lock);

/* Acquire the lock, blocking if neccessary. */
void sthread_mutex_lock(sthread_mutex_t lock);

/* Release the lock. Assumed that the calling thread owns the lock */
void sthread_mutex_unlock(sthread_mutex_t lock);


typedef struct _sthread_cond *sthread_cond_t;

/* Return a new condition variable. */
sthread_cond_t sthread_cond_init();

/* Free a no-longer needed condition variable.
 * Assume condition variable has no waiters. */
void sthread_cond_free(sthread_cond_t cond);

/* Signal that the condition has been met, awakening a single waiting
 * thread. (Though not switching to the newly awoken thread
 * immediatly.) */
void sthread_cond_signal(sthread_cond_t cond);

/* Signal that the condition has been met, awakening all waiting
 * threads. */
void sthread_cond_broadcast(sthread_cond_t cond);

/* Block the calling thread until the condition has been signaled.
 * Atomically does the following:
 * 1. Releases lock.
 * 2. Add thread to the waiters for cond.
 * 3. Sleeps thread until awoken. */
void sthread_cond_wait(sthread_cond_t cond, sthread_mutex_t lock);

#endif /* STHREAD_H */
