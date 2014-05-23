/* 
 * Kevin Loh (kevinloh)
 * Chun-Wei Chen (mijc0517)
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sthread.h>

/*********************************************************************/
/* Stack Implementation                                              */
/*********************************************************************/
struct _stack_t {
  int* array;
  int size;
  int max_size;
};

typedef struct _stack_t* stack;

/* Initializes a stack that has max size = numElem. */
stack stack_init(int numElem) {
  stack ret = (stack) malloc(sizeof(struct _stack_t));
  assert(ret != NULL);
  
  ret->array = (int *) malloc(numElem * sizeof(int));
  assert(ret->array != NULL);

  ret->size = 0;
  ret->max_size = numElem;
  return ret;
}

void stack_free(stack s) {
  free(s->array);
  free(s);
}

int stack_size(stack s) {
  return s->size;
}

bool stack_is_empty(stack s) {
  return s->size == 0;
}

bool stack_push(stack s, int elem) {
  if (s->size >= s->max_size) {
    return false;
  }

  (s->array)[s->size] = elem;
  s->size++;
  return true;
}

int stack_pop(stack s) {
  assert(!stack_is_empty(s));

  int ret = (s->array)[s->size - 1];
  s->size--;
  return ret;
}

/*********************************************************************/
/* Part 3: Food services problem                                     */
/*********************************************************************/
static stack burger_stack;
static int bid_counter = 0;
static int max_burgers;
static sthread_mutex_t stack_lock;
static sthread_cond_t has_burger;

void usage();
void *make_burger(void *arg);
void *eat_burger(void *arg);

int main(int argc, char** argv) {
  if (argc != 4) {
    usage();
  }

  int num_cooks = atoi(argv[1]);
  if (num_cooks < 0) {
    printf("num_cooks: %d -- num_cooks must be >= 0.\n", num_cooks);
    exit(EXIT_FAILURE);
  }

  int num_students = atoi(argv[2]);
  if (num_students < 0) {
    printf("num_students: %d -- num_students must be >= 0.\n", num_students);
    exit(EXIT_FAILURE);
  }

  max_burgers = atoi(argv[3]);
  if (max_burgers < 0) {
    printf("num_burgers: %d -- num_burgers must be >= 0.\n", max_burgers);
    exit(EXIT_FAILURE);
  }

  printf("Running test-burgers, impl: %s\n",
         (sthread_get_impl() == STHREAD_PTHREAD_IMPL) ? "pthread" : "user");

  burger_stack = stack_init(max_burgers);

  sthread_init();
  stack_lock = sthread_mutex_init();
  assert(stack_lock != NULL);
  has_burger = sthread_cond_init();
  assert(has_burger != NULL);

  int i;
  int arg[num_cooks + num_students];
  sthread_t threads[num_cooks + num_students];

  for (i = 0; i < num_cooks; i++) {
    arg[i] = i;
    threads[i] = sthread_create(make_burger, (void *) &(arg[i]), 1);
    assert(threads[i] != NULL);
  }

  for (i = num_cooks; i < num_cooks + num_students; i++) {
    arg[i] = i;
    threads[i] = sthread_create(eat_burger, (void *) &(arg[i]), 1);
    assert(threads[i] != NULL);
  }

  for (i = 0; i < num_cooks + num_students; i++) {
    sthread_join(threads[i]);
  }

  stack_free(burger_stack);
  sthread_mutex_free(stack_lock);
  sthread_cond_free(has_burger);
  exit(EXIT_SUCCESS);
}

void usage() {
  printf("Usage: ./test-burgers num_cooks num_students num_burgers\n");
  exit(EXIT_FAILURE);
}

void *make_burger(void *arg) {
  while (1) {
    sthread_mutex_lock(stack_lock);
    if (bid_counter >= max_burgers) {
      sthread_cond_broadcast(has_burger);
      sthread_mutex_unlock(stack_lock);
      return arg;
    }

    bid_counter++;
    stack_push(burger_stack, bid_counter);
    printf("Cook %d has cooked burger %d.\n", *((int *) arg), bid_counter);

    sthread_cond_signal(has_burger);
    sthread_mutex_unlock(stack_lock);
    sthread_yield();
  }

  // shouldn't reach here
  return NULL;
}

void *eat_burger(void *arg) {
  int bid;
  while (1) {
    sthread_mutex_lock(stack_lock);
    if (bid_counter >= max_burgers && stack_is_empty(burger_stack)) {
      sthread_mutex_unlock(stack_lock);
      return arg;
    }

    while (stack_is_empty(burger_stack)) {
      sthread_cond_wait(has_burger, stack_lock);
    }

    bid = stack_pop(burger_stack);
    printf("Student %d has eaten burger %d.\n", *((int *) arg), bid);
    sthread_mutex_unlock(stack_lock);
    sthread_yield();
  }

  // shouldn't reach here
  return NULL;
}
