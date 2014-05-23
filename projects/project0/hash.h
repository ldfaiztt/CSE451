#ifndef _HASH_H_
#define _HASH_H_

/* Definitions for abstract hash table. NOTE: You do not need to support
 * insertion of (key, NULL). This gives you the flexibility to treat a
 * NULL value as a marker for a deleted entry, should you have the need.
 *
 * The specific implementation of the hash map is up to you, whether you
 * decide on linear probing, quadratic probing, separate chaining, etc. */

#include <stdbool.h>
#include <stdint.h>

/* A hash table is type "hash_table"; the actual "_hash_table" struct is
 * defined in hash.c, but we declare this typedef here to provide clients
 * with an opaque handle for a hash table. */
typedef struct _hash_table hash_table;

/* The client supplies a function to hash the key to a uint64_t
 * and another function that can compare two keys for equality. */
typedef uint64_t (*hash_hasher)(const void*);  // Hash function type
/* Element comparison function: For two arguments e1 and e2, returns 0 if
 * e1 == e2, -1 if e1 < e2, or 1 if e1 > e2. */
typedef int (*hash_compare)(const void*, const void*);

/* Creates and returns a new hash table. The client must define a hash
 * function and a comparison function and pass pointers to these functions
 * as arguments to this function.
 *
 * Returns: pointer to the created hash table. */
hash_table* hash_create(hash_hasher, hash_compare);

/* Inserts a (key, value) pair into the hash table. The implementation
 * should resize the hash table once the size / capacity ratio reaches a
 * certain threshold in order to minimize space usage and maximize speed.
 * This function takes ownership of *key and *value. After insertion, the
 * caller is permitted to modify *value, but is not permitted to change
 * *key. If a value had already been inserted into the hash table for the
 * given key, then *removed_key_ptr is set to point to the replaced key,
 * and *removed_value_ptr is set to point to the replaced value. The caller
 * is responsible for freeing the returned key and value. */
void hash_insert(hash_table* ht, void* key, void* value,
                 void** removed_key_ptr, void** removed_value_ptr);

/* Looks up the specified key in the hash table. If the key is found, then
 * the pointer to its value is stored in *value_ptr; the caller can then
 * directly manipulate the value that is pointed to.
 *
 * Returns: true if the key was found, false if not. */
bool hash_lookup(hash_table* ht, const void* key, void** value_ptr);

/* Checks if a key has been inserted into the hash table.
 *
 * Returns: true if the key is present in the hash table, false if not. */
bool hash_is_present(hash_table* ht, const void* key);

/* Removes the entry for the given key from the hash table. If the key is
 * found in the hash table, then *removed_key_ptr is set to point to the
 * key previously inserted, and *removed_value_ptr is set to point to the
 * value that was previously inserted, but the caller is responsible for
 * freeing them.
 *
 * Returns: true if the entry for the key was removed, false if not. */
bool hash_remove(hash_table* ht, const void* key,
                 void** removed_key_ptr, void** removed_value_ptr);

/* Destroys a hash table and frees the memory used by the entries and the hash
 * table itself. If the free_values argument is true, then this function
 * will call free() on each entry's value, and similarly for free_keys. Hence
 * if the values in the entries were allocated dynamically (using malloc()),
 * then free_values should be set to true; if the values were allocated on the
 * client's stack (i.e. in local variables in a test function), then free_values
 * should be set to false, and similarly for the keys.
 */
void hash_destroy(hash_table* ht, bool free_keys, bool free_values);

#endif  // _HASH_H_
