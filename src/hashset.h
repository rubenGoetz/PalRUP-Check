
#pragma once

#include <stdbool.h>        // for bool
#include "utils/palrup_utils.h"

// A hash table mapping from u32 keys to void values.
// The key zero is a magic number representing an empty entry.
// The capacity is provided logarithmically such that the
// table's capacity is always a power of two.
// Growing is done in powers of two, if the load factor exceeds 0.5.
// No shrinking is done. Assuming roughly monotonic growth,
// the table's load factor is always between 0.25 and 0.5.
// Collisions are handled via linear probing, which in this mode
// appears to perform reasonably well.

struct hash_set {
    u32 size;
    u32 max_size;
    float growth_factor;
    u32 capacity;
    u32* data;
    u32 last_found_idx;
};

struct hash_set* hash_set_init(int log_init_capacity);
bool hash_set_find(struct hash_set* ht, u32 key);
bool hash_set_insert(struct hash_set* ht, u32 key);
bool hash_set_delete(struct hash_set* ht, u32 key);
bool hash_set_delete_last_found(struct hash_set* ht);
void hash_set_clear(struct hash_set* ht);
void hash_set_free(struct hash_set* ht);
