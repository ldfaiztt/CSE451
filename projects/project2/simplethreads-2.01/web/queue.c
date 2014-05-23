// Kevin Loh (1030431)
// kevinloh@uw.edu
// 9/30/2013

// Bug 1: queue_append does not check for null pointer (empty queue)
// and will cause an error

// Bug 2: queue_remove does not free the node whose element is being
// removed.

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
  assert(q != NULL);

  q->head = NULL;
  return q;
}

/* Private */
static queue_link* queue_new_element(queue_element* elem) {
  queue_link* ql = (queue_link*) malloc(sizeof(queue_link));
  assert(ql != NULL);

  ql->elem = elem;
  ql->next = NULL;

  return ql;
}

void queue_append(queue* q, queue_element* elem) {
  assert(q != NULL);

  // Bug 1
  if (queue_is_empty(q)) {
    q->head = queue_new_element(elem);

  } else {
    // Find the last link in the queue.
    queue_link* cur;
    for (cur = q->head; cur->next; cur = cur->next) {}

    // Append the new link.
    cur->next = queue_new_element(elem);
  }
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

  // Bug 2
  free(old_head);
  old_head = NULL;

  return true;
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

  queue_link* cur;
  for (cur = q->head; cur; cur = cur->next) {
    if (!qf(cur->elem, args))
      break;
  }

  return true;
}

void queue_reverse(queue* q) {
  assert(q != NULL);

  size_t q_size = queue_size(q);
  if (q_size > 1) {
    queue_link* old_head =  q->head;
    queue_link* first =     q->head;
    queue_link* second =    first->next;

    while (second != NULL) {
      old_head->next = second->next;
      second->next = first;

      first = second;
      second = (old_head->next == NULL ? NULL : old_head->next);
    }


      q->head = first;
  }
}

static queue_link* find_smallest(queue_link* ql, queue_compare qc) {
  assert(ql != NULL && qc != NULL);

  queue_link* ret = ql;
  queue_link* cur = ql;

  while (cur != NULL) {
    int compare = qc(cur->elem, ret->elem);
    if (compare < 0) {
      ret = cur;
    }
    cur = cur->next;
  }

  return ret;
}

void queue_sort(queue* q, queue_compare qc) {
  assert(q != NULL && qc != NULL);

  queue_link* cur = q->head;

  while (cur != NULL && cur->next != NULL) {
    queue_link* smallest = find_smallest(cur, qc);
    queue_element* temp = cur->elem;
    cur->elem = smallest->elem;
    smallest->elem = temp;

    cur = cur->next;
  }
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
