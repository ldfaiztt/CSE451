#include <config.h>

#include <stdlib.h>
#include <assert.h>
#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif

#include <sthread.h>
#include <sthread_queue.h>

struct _sthread_queue_elem {
  sthread_t sth;
  struct _sthread_queue_elem *next;
};
typedef struct _sthread_queue_elem* sthread_queue_elem_t;

static sthread_queue_elem_t free_list = NULL;

#ifdef USE_PTHREADS
/* We need to lock the free_list, but can't depend on the user
 * having implemented locks. So we use pthread locks when available,
 * and depend on the non-preemptive nature of user threads otherwise.
 */
static pthread_mutex_t free_list_lock;

#define LOCK_FREE_LIST pthread_mutex_lock(&free_list_lock)
#define UNLOCK_FREE_LIST pthread_mutex_unlock(&free_list_lock)

#else /* USE_PTHREADS */

#define LOCK_FREE_LIST ((void)0)
#define UNLOCK_FREE_LIST ((void)0)

#endif /* USE_PTHREADS */

struct _sthread_queue {
  sthread_queue_elem_t head;
  sthread_queue_elem_t tail;
  int size;
};

/* Create a new, empty queue. Asserts against error. */
sthread_queue_t sthread_new_queue() {
  sthread_queue_t queue;

  queue = (sthread_queue_t)malloc(sizeof(struct _sthread_queue));
  assert(queue != NULL);

  queue->head = queue->tail = NULL;
  queue->size = 0;

#ifdef USE_PTHREADS
  pthread_mutex_init(&free_list_lock, NULL);
#endif

  return queue;
}

/* Destroy the given queue. Asserts that the queue is empty. */
void sthread_free_queue(sthread_queue_t queue) {
  assert(queue->size == 0);
  free(queue);
}

/* Add the given thread to the end of the queue */
void sthread_enqueue(sthread_queue_t queue, sthread_t sth) {
  sthread_queue_elem_t elem;
  LOCK_FREE_LIST;
  elem = free_list;
  if (elem == NULL)
    elem = (sthread_queue_elem_t)malloc(sizeof(struct _sthread_queue_elem));
  else
    free_list = free_list->next;
  UNLOCK_FREE_LIST;

  assert(elem != NULL);

  elem->next = NULL;
  elem->sth = sth;

  if (queue->tail != NULL) {
    queue->tail->next = elem;
  } else {
    assert(queue->head == NULL);
    queue->head = elem;
  }
  queue->tail = elem;

  queue->size++;
}

/* Return, and remove, the next thread from the queue, or NULL
 * if queue is empty */
sthread_t sthread_dequeue(sthread_queue_t queue) {
  sthread_queue_elem_t head;
  sthread_t sth;

  if (queue->head == NULL)
    return NULL;

  head = queue->head;

  sth = head->sth;
  if (head->next == NULL) {
    assert(queue->size == 1);
    assert(head == queue->tail);
    queue->tail = NULL;
  }
  queue->head = head->next;

  /* Return to free list */
  LOCK_FREE_LIST;
  head->next = free_list;
  free_list = head;
  UNLOCK_FREE_LIST;

  head = NULL;

  queue->size--;

  return sth;
}

/* Return the number of threads currently in the queue */
int sthread_queue_size(sthread_queue_t queue) {
  return queue->size;
}

/* Return true if queue has no threads, false otherwise */
int sthread_queue_is_empty(sthread_queue_t queue) {
  return (queue->size == 0);
}

/* Clear the global free list associated with the sthread
 * queue library. In order to maintain efficiency of queue
 * insertions, this should be called a single time when
 * the sthread library has no further use for queues. */
void sthread_queue_clear_free_list(void) {
  sthread_queue_elem_t current = free_list;
  while (current != NULL) {
    sthread_queue_elem_t previous = current;
    current = current->next;
    free(previous);
  }
  free_list = NULL;
}
