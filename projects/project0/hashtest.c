#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hash.h"

static const size_t kBufferLength = 32;
static const uint32_t kMaxInsertions = 100000;
static const char kNotFoundKey[] = "not-found key";

/* Matches the hash_hasher definition in hash.h: */
static uint64_t hash_fn(const void* k) {
  uint64_t hash_val = 0;
  uint64_t coefficient = 1;

  for (const char* p = (const char*) k; *p != '\0'; p++) {
    hash_val += coefficient * (*p);
    coefficient *= 37;
  }

  return hash_val;
}

/* Matches the hash_compare definition in hash.h. This function compares
 * two keys that are strings. */
static int hash_strcmp(const void* k1, const void* k2) {
  return strcmp((const char*) k1, (const char*) k2);
}

int main(int argc, char* argv[]) {
  /* Check for correct invocation: */
  if (argc != 2) {
    printf("Usage: %s <N>\n"
        "Run test inserting a total of N items\n", argv[0]);
    return 1;
  }
  int N = atoi(argv[1]);
  if (N <= 0 || N > kMaxInsertions) {
    N = kMaxInsertions;
  }

  /* Create the hash table. */
  hash_table* ht = hash_create(hash_fn, hash_strcmp);

  /* First phase: insert some data. */
  printf("\nInsert phase:\n");
  char* k;
  int64_t* v;
  char* removed_key = NULL;
  int64_t* removed_value = NULL;
  for (int i = 0; i < N * 2; i++) {
    k = (char*) malloc(kBufferLength);
    snprintf(k, kBufferLength, "String %d", i % N);
    v = (int64_t*) malloc(sizeof(int64_t));
    *v = i;
    // The hash map takes ownership of the key and value:
    hash_insert(ht, k, v, (void**) &removed_key, (void**) &removed_value);
    if (removed_value != NULL) {
      printf("Replaced (%s, %" PRIi64 ") while inserting (%s, %" PRIi64 ")\n",
             removed_key, *removed_value, k, *v);
      free(removed_key);
      free(removed_value);
    } else {
      printf("Inserted (%s, %" PRIi64 ")\n", k, *v);
    }
  }

  /* Second phase: look up some data. */
  printf("\nLookup phase:\n");
  char strbuf[kBufferLength];
  for (int i = N - 1; i >= 0; i--) {
    snprintf(strbuf, kBufferLength, "String %d", i);
    if (!hash_lookup(ht, strbuf, (void**) &v)) {
      printf("Entry for %s not found\n", strbuf);
    } else {
      printf("%s -> %" PRIi64 "\n", strbuf, *v);
    }
  }

  /* Look up a key that hasn't been inserted: */
  if (!hash_lookup(ht, kNotFoundKey, (void**) &v)) {
    printf("Lookup of \"%s\" failed (as expected)\n", kNotFoundKey);
  } else {
    printf("%s -> %" PRIi64 " (unexpected!)\n", kNotFoundKey, *v);
  }

  /* Test hash_is_present. */
  printf("\nKey_Is_Present phase:\n");
  for (int i = N - 1; i >= 0; i--) {
    snprintf(strbuf, kBufferLength, "String %d", i);
    if (!hash_is_present(ht, strbuf)) {
      printf("Key %s is not present\n", strbuf);
    } else {
      printf("Key %s is present\n", strbuf);
    }
  }

  /* Try hash_is_present with the key that hasn't been inserted. */
  if (!hash_is_present(ht, kNotFoundKey)) {
    printf("Key %s is not present (as expected)\n", kNotFoundKey);
  } else {
    printf("Key %s is present (unexpected!)\n", kNotFoundKey);
  }

  /* Remove some entry and check if remove is done correctly. */
  printf("\nRemove phase:\n");
  removed_key = NULL;
  removed_value = NULL;
  for (int i = N - 1; i >= 0; i -= 2) {
    snprintf(strbuf, kBufferLength, "String %d", i);
    if (!hash_remove(ht, strbuf, (void**) &removed_key,
                     (void**) &removed_value)) {
      printf("Key %s not found, hash table is not modified\n", strbuf);
    } else {
      printf("Removed (%s, %" PRIi64 ")\n", removed_key, *removed_value);
      free(removed_key);
      free(removed_value);
    }
  }

  for (int i = N - 1; i >= 0; i--) {
    snprintf(strbuf, kBufferLength, "String %d", i);
    if (!hash_lookup(ht, strbuf, (void**) &v)) {
      printf("Entry for %s not found\n", strbuf);
    } else {
      printf("%s -> %" PRIi64 "\n", strbuf, *v);
    }
  }

  /* Try to remove an entry for a non-existing key. */
  removed_key = NULL;
  removed_value = NULL;
  if (!hash_remove(ht, kNotFoundKey, (void**) &removed_key,
                   (void**) &removed_value)) {
    printf("Key %s not found, hash table is not modified (as expected)\n",
           kNotFoundKey);
  } else {
    printf("Removed (%s, %" PRIi64 ")\n", removed_key, *removed_value);
    free(removed_key);
    free(removed_value);
  }

  /* Destroy the hash table and free things that we've allocated. Because
   * we allocated both the keys and the values, we instruct the hash map
   * to free both.
   */
  hash_destroy(ht, true, true);

  return 0;
}
