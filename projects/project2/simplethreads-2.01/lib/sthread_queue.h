/* Note: sthread_queue_t is not synchronized. If used from multiple
 * threads, it is the users responsibility to provide suitable mutual
 * exclusion. The sthread queue library maintains a global free list
 * from which it allocates links, so even if all queues are freed via
 * sthread_free_queue(), Valgrind will still report memory as "in use
 * at exit". Applications or libraries building on the sthread library
 * may use sthread_queue_clear_free_list() to free the memory associated
 * with this free list prior to exit in order to avoid such reports.
 */

#ifndef STHREAD_QUEUE_H
#define STHREAD_QUEUE_H

#include <sthread.h>

struct _sthread_queue;
typedef struct _sthread_queue* sthread_queue_t;

/* Create a new, empty queue */
sthread_queue_t sthread_new_queue();

/* Destroy the given queue. Asserts that the queue is empty. */
void sthread_free_queue(sthread_queue_t queue);

/* Add the given thread to the end of the queue */
void sthread_enqueue(sthread_queue_t queue, sthread_t sth);

/* Return, and remove, the next thread from the queue, or NULL
 * if queue is empty */
sthread_t sthread_dequeue(sthread_queue_t queue);

/* Return the number of threads currently in the queue */
int sthread_queue_size(sthread_queue_t queue);

/* Return true if queue has no threads, false otherwise */
int sthread_queue_is_empty(sthread_queue_t queue);

/* Clear the global free list associated with the sthread
 * queue library. In order to maintain efficiency of queue
 * insertions, this should be called a single time when
 * the sthread library has no further use for queues. */
void sthread_queue_clear_free_list(void);

#endif /* STHREAD_QUEUE_H */
