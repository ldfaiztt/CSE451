#include <stdio.h>
#include <assert.h>
#include "queue.h"

// Print out the index and the value of each element.
bool show_one(queue_element* elem, queue_function_args* args) {
  printf("Item %d == %d\n", *(int*) args, *(int*) elem);
  *(int*) args = *(int*) args + 1;
  return true;
}

int compare_elem(queue_element* e1, queue_element* e2) {
  int i1 = *(int*) e1;
  int i2 = *(int*) e2;

  if (i1 < i2)
    return -1;
  if (i1 > i2)
    return 1;
  return 0;
}

int main(int argc, char* argv[]) {
  queue* q = queue_create();
  assert(q != NULL);
  assert(queue_is_empty(q));
  assert(queue_size(q) == 0);

  queue_reverse(q);
  assert(queue_is_empty(q));
  assert(queue_size(q) == 0);

  queue_sort(q, &compare_elem);
  assert(queue_is_empty(q));
  assert(queue_size(q) == 0);

  int x = 0;
  int y = 1;
  int z = 2;
  queue_append(q, &x);
  assert(!queue_is_empty(q));
  assert(queue_size(q) == 1);
  int index = 0;
  queue_apply(q, show_one, &index);  // q: 0
  printf("\n");

  queue_reverse(q);
  assert(!queue_is_empty(q));
  assert(queue_size(q) == 1);
  index = 0;
  queue_apply(q, show_one, &index);  // q: 0
  printf("\n");

  queue_sort(q, &compare_elem);
  assert(!queue_is_empty(q));
  assert(queue_size(q) == 1);
  index = 0;
  queue_apply(q, show_one, &index);  // q: 0
  printf("\n");

  queue_element* elem;
  queue_remove(q, &elem);
  assert(queue_is_empty(q));
  assert(queue_size(q) == 0);

  queue_append(q, &x);
  queue_append(q, &y);
  queue_append(q, &z);
  queue_append(q, &x);
  assert(!queue_is_empty(q));
  assert(queue_size(q) == 4);
  index = 0;
  queue_apply(q, show_one, &index);  // q: 0 1 2 0
  printf("\n");

  queue_remove(q, &elem);
  assert(queue_size(q) == 3);
  index = 0;
  queue_apply(q, show_one, &index);  // q: 1 2 0
  printf("\n");

  queue_remove(q, &elem);
  assert(queue_size(q) == 2);
  index = 0;
  queue_apply(q, show_one, &index);  // q: 2 0
  printf("\n");

  queue_remove(q, &elem);
  assert(queue_size(q) == 1);
  index = 0;
  queue_apply(q, show_one, &index);  // q: 0
  printf("\n");

  queue_remove(q, &elem);
  assert(queue_is_empty(q));
  assert(queue_size(q) == 0);

  queue_append(q, &z);
  queue_append(q, &y);
  queue_append(q, &x);
  assert(queue_size(q) == 3);
  index = 0;
  queue_apply(q, show_one, &index);  // q: 2 1 0
  printf("\n");

  queue_sort(q, &compare_elem);
  index = 0;
  queue_apply(q, show_one, &index);  // q: 0 1 2
  printf("\n");

  queue_reverse(q);
  index = 0;
  queue_apply(q, show_one, &index);  // q: 2 1 0
  printf("\n");

  queue_append(q, &z);
  assert(queue_size(q) == 4);
  index = 0;
  queue_apply(q, show_one, &index);  // q: 2 1 0 2
  printf("\n");

  queue_sort(q, &compare_elem);
  index = 0;
  queue_apply(q, show_one, &index);  // q: 0 1 2 2
  printf("\n");

  queue_reverse(q);
  index = 0;
  queue_apply(q, show_one, &index);  // q: 2 2 1 0

  queue_destroy(q);
  q = NULL;

  return 0;
}
