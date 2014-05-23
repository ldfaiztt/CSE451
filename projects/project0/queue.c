/* Implements queue abstract data type. */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

/* Each link in the queue stores a queue_element and
 * a pointer to the next link in the queue. */
typedef struct _queue_link {
  queue_element* elem;
  struct _queue_link* next;
} queue_link;

/* This is the actual implementation of the queue struct that
 * is declared in queue.h. */
struct _queue {
  queue_link* head;
};

queue* queue_create() {
  queue* q = (queue*) malloc(sizeof(queue));

  // check if malloc succeeded
  if (q != NULL)
    q->head = NULL;

  return q;
}

/* Private */
static queue_link* queue_new_element(queue_element* elem) {
  queue_link* ql = (queue_link*) malloc(sizeof(queue_link));

  // check if malloc succeeded
  if (ql != NULL) {
    ql->elem = elem;
    ql->next = NULL;
  }

  return ql;
}

void queue_append(queue* q, queue_element* elem) {
  assert(q != NULL);

  queue_link* new_elem = queue_new_element(elem);
  assert(new_elem != NULL);

  // Set element to be head of queue if queue is empty.
  if (q->head == NULL) {
    q->head = new_elem;
    return;
  }

  // Find the last link in the queue.
  queue_link* cur;
  for (cur = q->head; cur->next; cur = cur->next) {}

  // Append the new link.
  cur->next = new_elem;
}

bool queue_remove(queue* q, queue_element** elem_ptr) {
  queue_link* old_head;

  assert(q != NULL);
  assert(elem_ptr != NULL);
  if (queue_is_empty(q)) {
    return false;
  }

  *elem_ptr = q->head->elem;
  old_head = q->head;
  q->head = q->head->next;

  // free the _queue_link struct after removing the element
  free(old_head);

  return true;
}

void queue_destroy(queue* q) {
  queue_link* cur;
  queue_link* next;
  if (q != NULL) {
    cur = q->head;
    while (cur) {
      next = cur->next;
      free(cur);
      cur = next;
    }
    free(q);
  }
}

bool queue_is_empty(queue* q) {
  assert(q != NULL);
  return q->head == NULL;
}

/* private */
static bool queue_count_one(queue_element* elem, queue_function_args* args) {
  size_t* count = (size_t*) args;
  *count = *count + 1;
  return true;
}

size_t queue_size(queue* q) {
  size_t count = 0;
  queue_apply(q, queue_count_one, &count);
  return count;
}

bool queue_apply(queue* q, queue_function qf, queue_function_args* args) {
  assert(q != NULL && qf != NULL);

  if (queue_is_empty(q))
    return false;

  for (queue_link* cur = q->head; cur; cur = cur->next) {
    if (!qf(cur->elem, args))
      break;
  }

  return true;
}

/*
 * Helper method for queue_reverse.
 */
static void queue_find_idx(queue_link* qls, size_t cur_idx,
                           size_t target_idx, queue_link** qlt) {
  for (*qlt = qls; cur_idx < target_idx; *qlt = (*qlt)->next)
    cur_idx++;
}

void queue_reverse(queue* q) {
  assert(q != NULL);

  size_t qs = queue_size(q);

  // no need to modify the queue is its size is less than 2
  if (qs < 2)
    return;

  // see the queue as two halves and swap the element
  // of the queue link (i) in the first half with the
  // element of the corresponding queue link (qs-i-1)
  queue_link* cur = q->head;
  queue_link* target;
  for (size_t i = 0; i < qs / 2; i++) {
    queue_find_idx(cur, i, qs - i - 1, &target);
    queue_element* temp;
    temp = cur->elem;
    cur->elem = target->elem;
    target->elem = temp;
    cur = cur->next;
  }
}

/* Sorting part I refer to CSE 333 Project 1. */
void queue_sort(queue* q, queue_compare qc) {
  assert(q != NULL);

  size_t qs = queue_size(q);
  // no need to sort if size of queue if less than 2
  if (qs < 2)
    return;

  // sort the queue using bubble sort
  // boolean flag to indicate whether sorting is done
  bool swapped;
  do {
    queue_link* cur = q->head;
    swapped = false;

    while (cur->next != NULL) {
      int res = qc(cur->elem, cur->next->elem);

      // swap elements of two queue links if res > 0
      if (res > 0) {
        queue_element* temp;
        temp = cur->elem;
        cur->elem = cur->next->elem;
        cur->next->elem = temp;
        swapped = true;
      }
      cur = cur->next;
    }
  } while (swapped);
}
