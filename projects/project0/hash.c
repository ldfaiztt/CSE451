/* Implements the abstract hash table. */

#include <assert.h>
#include <stdlib.h>

#include "hash.h"

/* Using an entry of this type is just a suggestion. You're
 * free to change this. */
typedef struct _hash_entry {
  void* key;
  void* value;
} hash_entry;

struct _hash_table {
  /* TODO: add members to this struct! */
  hash_hasher hf;
  hash_compare hc;
  size_t size;
  size_t capacity;
  hash_entry* entries;
};

static void hash_resize(hash_table* ht);

/* TODO: implement the functions in hash.h here! */

hash_table* hash_create(hash_hasher hh, hash_compare hc) {
  hash_table* ht = (hash_table*) malloc(sizeof(hash_table));

  if (ht == NULL)
    return NULL;

  // fill in the member of the hash_table
  ht->hf = hh;
  ht->hc = hc;
  ht->size = 0;
  ht->capacity = 11;
  ht->entries = (hash_entry *) malloc(11 * sizeof(hash_entry));

  // free up hash_table if malloc array of hash_entry failed
  if (ht->entries == NULL) {
    free(ht);
    return NULL;
  }

  // indicate every position of array is free for insertion
  for (int i = 0; i < ht->capacity; i++)
    (ht->entries[i]).value = NULL;

  return ht;
}

void hash_insert(hash_table* ht, void* key, void* value,
                 void** removed_key_ptr, void** removed_value_ptr) {
  assert(ht != NULL);

  // don't support inserting (key, NULL)
  if (value == NULL)
    return;

  // resize the hash_table if needed
  hash_resize(ht);

  // get the hash value of the key
  uint64_t val = ht->hf(key) % ht->capacity;
  uint64_t i = val;

  // search through the array to see if the key exists,
  // starting from position "val"
  do {
    // if key exists, update the key//value and return old key/value
    if ((ht->entries[i]).value != NULL &&
        (ht->hc((ht->entries[i]).key, key) == 0)) {
      *removed_key_ptr = (ht->entries[i]).key;
      *removed_value_ptr = (ht->entries[i]).value;
      (ht->entries[i]).key = key;
      (ht->entries[i]).value = value;
      return;
    }

    // go to the next unexplored position
    i = (i + 1) % ht->capacity;
  } while (i != val);

  // key doesn't exists yet, find the empty spot for inserting
  // this key/value starting at "val"
  i = val;
  while ((ht->entries[i]).value != NULL)
    i = (i + 1) % ht->capacity;

  // insert key/value and increase the size by one
  (ht->entries[i]).key = key;
  (ht->entries[i]).value = value;
  ht->size += 1;
}

bool hash_lookup(hash_table* ht, const void* key, void** value_ptr) {
  assert(ht != NULL);

  // get the hash value of the key
  uint64_t val = ht->hf(key) % ht->capacity;
  uint64_t i = val;

  // search through the array to see if the key exists,
  // starting from position "val"
  do {
    // key found
    if ((ht->entries[i]).value != NULL &&
        (ht->hc((ht->entries[i]).key, key) == 0)) {
      *value_ptr = (ht->entries[i]).value;
      return true;
    }

    // go to the next unexplored position
    i = (i + 1) % ht->capacity;
  } while (i != val);

  // key not found
  return false;
}

bool hash_is_present(hash_table* ht, const void* key) {
  assert(ht != NULL);

  void* dum_val_ptr;
  return hash_lookup(ht, key, &dum_val_ptr);
}

bool hash_remove(hash_table* ht, const void* key,
                 void** removed_key_ptr, void** removed_value_ptr) {
  assert(ht != NULL);

  // get the hash value of the key
  uint64_t val = ht->hf(key) % ht->capacity;
  uint64_t i = val;

  // search through the array to see if the key exists,
  // starting from position "val"
  do {
    // if found, remove key/value and set the position
    // to be free for insertion
    if ((ht->entries[i]).value != NULL &&
        (ht->hc((ht->entries[i]).key, key) == 0)) {
      *removed_key_ptr = (ht->entries[i]).key;
      *removed_value_ptr = (ht->entries[i]).value;
      (ht->entries[i]).value = NULL;
      ht->size -= 1;
      return true;
    }

    // go to the next unexplored position
    i = (i + 1) % ht->capacity;
  } while (i != val);

  // key not found
  return false;
}

void hash_destroy(hash_table* ht, bool free_keys, bool free_values) {
  assert(ht != NULL);

  for (int i = 0; i < ht->capacity; i++) {
    // free up dynamically allocated keys and values in array of hash_entry
    if ((ht->entries[i]).value != NULL) {
      if (free_keys)
        free((ht->entries[i]).key);

      if (free_values)
        free((ht->entries[i]).value);
    }
  }

  // free the array of hash_entry in hash_table and then free the hash_table
  free(ht->entries);
  free(ht);
  ht = NULL;
}

static void hash_resize(hash_table* ht) {
  // resize if load factor > 1/2
  if (ht->size <= ht->capacity / 2)
    return;

  hash_table* new_ht = (hash_table*) malloc(sizeof(hash_table));

  // fail to malloc new hash table
  if (new_ht == NULL)
    return;

  // fill in the member for new hash_table
  new_ht->hf = ht->hf;
  new_ht->hc = ht->hc;
  new_ht->size = 0;
  new_ht->capacity = ht->capacity * 2 + 1;
  new_ht->entries = (hash_entry *) malloc((ht->capacity * 2 + 1) *
                                          sizeof(hash_entry));

  // free up new hash_table if malloc array of hash_entry failed
  if (new_ht->entries == NULL) {
    free(new_ht);
    return;
  }

  // indicate every position of array is free for insertion
  for (int i = 0; i < new_ht->capacity; i++)
    (new_ht->entries[i]).value = NULL;

  // insert key/value in original hash_table to new hash_table
  void* dum_key_ptr = NULL;
  void* dum_value_ptr = NULL;
  for (int j = 0; j < ht->capacity; j++) {
    if ((ht->entries[j]).value != NULL) {
      hash_insert(new_ht, (ht->entries[j]).key, (ht->entries[j]).value,
                  &dum_key_ptr, &dum_value_ptr);

      // assert inserting key/value as expected
      assert(dum_key_ptr == NULL);
      assert(dum_value_ptr == NULL);
    }
  }

  // swap the strcuture and then free the new hash_table
  hash_table temp;
  temp = *ht;
  *ht = *new_ht;
  *new_ht = temp;
  hash_destroy(new_ht, false, false);
}
