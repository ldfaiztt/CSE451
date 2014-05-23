typedef struct _thread_pool thread_pool;

/* Initializes the thread_pool with the given number of threads.
 * Returns the thread_pool if size > 0, otherwise returns NULL. */
thread_pool* thread_pool_init(int num_threads, const char* docroot);


/* Enqueue a request for dispatch. */
void thread_pool_dispatch(thread_pool* tp, int* conn_ptr);
