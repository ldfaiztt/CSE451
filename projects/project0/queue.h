#ifndef _QUEUE_H_
#define _QUEUE_H_

#include <stdbool.h>
#include <stdlib.h>

// Forward declaration of the queue struct. The actual definition
// is implementation-specific. In this case, queue.c provides the
// implementation.
typedef struct _queue queue;

// The queue stores raw void pointers.
typedef void queue_element;

/*
 * Creates and returns a new queue.
 */
queue* queue_create();

/*
 * Appends an element to the end of the queue.
 */
void queue_append(queue* q, queue_element* elem);

/* Remove the first element from the queue and leaves result in elem_ptr.
 *   Returns false if isEmpty() prior to remove.
 */
bool queue_remove(queue* q, queue_element** elem_ptr);

/*
 * Destroys the queue.
 */
void queue_destroy(queue* q);

/*
 * Returns true if queue is empty, false otherwise.
 */
bool queue_is_empty(queue* q);

/*
 * Returns number of elements in the queue.
 */
size_t queue_size(queue* q);

/*
 * Function Application.
 *
 * Applies the given function to each element of the queue. Given closure
 * is also provided to the function. Given function should
 * return true if iteration should continue.
 *
 * Returns isEmpty().
 *
 * Behaviour is undefined if the state of the queue changes during the application.
 *
 * Sample client use:
 *
   bool count_elements(queue_element* elem, queue_args* args) {
     size_t* count = (size_t*) args;
     *count++;
     // Return true to indicate that iteration should continue.
     return true;
   }

   void bar() {
     queue* q = queue_new();
     if (!q) error();
  
     for (int i = 0; i < 10; i++) {
       queue_append(q, q);
     }
  
     size_t count = 0;
     queue_apply(q, count_elements, &count);
     printf("Queue size is %d\n", x);
   }
*/

// Arguments can be passed to queue functions as raw void* pointers.
typedef void queue_function_args;

// Signature for a function to be applied to an element of a queue.
typedef bool (*queue_function)(queue_element*, queue_function_args*);

// Apply the queue_function to the elements of the given queue,
// passing args as an argument to each application of qf.
bool queue_apply(queue* q, queue_function qf,
                 queue_function_args* args);

/* THESE FUNCTIONS ARE NOT IMPLEMENTED */

/*
 * Reverses the elements on the queue in place.
 */
void queue_reverse(queue* q);

// Compare the given two elements of the queue. queue_compare functions
// should return -1 if e1 < e2, 0 if e1 == e2, and 1 if e1 > e2.
typedef int (*queue_compare)(queue_element* /* e1* */, queue_element* /* e2* */);  // NOLINT

// Sorts the elements of the given queue in place.
void queue_sort(queue* q, queue_compare qc);

#endif  // _QUEUE_H_

