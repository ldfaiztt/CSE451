#include <stdbool.h>
#include <sthread.h>
#include <assert.h>

#include "queue.h"
#include "sioux_run.h"
#include "thread_pool.h"

struct _thread_pool {
  queue* request_queue;
  sthread_mutex_t mutex;
  sthread_cond_t request_ready;
  int num_threads_avail;
};

const char* tp_docroot;

static void* handle_request(void *arg);

thread_pool* thread_pool_init(int num_threads, const char* docroot) {
  // only create the thread pool if num_threads is greater than 0
  if (num_threads <= 0) {
    return NULL;
  }

  // Initialize thread pool and all its fields
  thread_pool *tp = (thread_pool *) malloc(sizeof(thread_pool));
  assert(tp != NULL);

  tp_docroot = docroot;

  tp->request_queue = queue_create();
  assert(tp->request_queue != NULL);

  tp->mutex = sthread_mutex_init();
  assert(tp->mutex != NULL);

  tp->request_ready = sthread_cond_init();
  assert(tp->request_ready != NULL);

  tp->num_threads_avail = 0;

  sthread_mutex_lock(tp->mutex);
  int i;
  for (i = 0; i < num_threads; i++) {
    // create the worker thread with thread pool as argument
    // so that handle request allows the thread to handle
    // the requests in the thread pool's request queue
    sthread_create(handle_request, (void *) tp, 1);
  }

  // wait for all of the threads to be born and initialized
  while (tp->num_threads_avail != num_threads) {
    sthread_mutex_unlock(tp->mutex);
    sthread_mutex_lock(tp->mutex);
  }
  sthread_mutex_unlock(tp->mutex);

  return tp;
}

void thread_pool_dispatch(thread_pool* tp, int* conn_ptr) {
  assert(tp != NULL);

  sthread_mutex_lock(tp->mutex);

  // enqueue the new coming request to the request queue
  // and then signal a worker thread to handle it
  queue_append(tp->request_queue, (queue_element *) conn_ptr);
  sthread_cond_signal(tp->request_ready);

  sthread_mutex_unlock(tp->mutex);
}


static void* handle_request(void *arg) {
  assert(arg != NULL);

  // since start_routine_func_t takes a void* as argument and
  // we pass in a thread_pool*, so cast it back to thrad_pool*
  // to access its fields
  thread_pool *tp = (thread_pool *) arg;
  sthread_mutex_lock(tp->mutex);
  while (1) {
    tp->num_threads_avail++;

    // wait for request_ready to be signal
    sthread_cond_wait(tp->request_ready, tp->mutex);

    tp->num_threads_avail--;

    // keep trying to get the request until no request
    // in request queue
    while (!queue_is_empty(tp->request_queue)) {
      // get the next request in request queue
      int* conn_ptr;
      queue_remove(tp->request_queue, (queue_element **) &conn_ptr);

      sthread_mutex_unlock(tp->mutex);
      web_handle_connection(*conn_ptr, tp_docroot);
      free(conn_ptr);
      sthread_mutex_lock(tp->mutex);
    }
  }

  // decrement the number of threads available in thread pool
  // by 1 when the thread is about to exit
  tp->num_threads_avail--;
  sthread_mutex_unlock(tp->mutex);

  return NULL;
}
