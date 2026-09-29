
#include "hashset.h"
#include "utils/palrup_utils.h"
#include <assert.h>  // for assert
#include <stdlib.h>  // for free
#include <string.h>  // for memset

u32 compute_hash_u32(u32 key) {
    return (0x811C9DC5U ^ key) * 0x01000193U;
}
u32 compute_idx_u32(struct hash_set* ht, u32 key) {
    return compute_hash_u32(key) & (ht->capacity-1);
}

bool hash_set_find_entry(struct hash_set* ht, u32 key, u32* idx) {
    u32 i = compute_idx_u32(ht, key);
    const u32 orig_idx = i;
    while (i < ht->capacity) {
        u32 entry = ht->data[i];
        if (entry == 0) {
            *idx = i; return false; // key is not present.
        }
        if (entry == key) {
            *idx = i; return true; // key found
        }
        i++;
    }
    i = 0;
    while (i < orig_idx) {
        u32 entry = ht->data[i];
        if (entry == 0) {
            *idx = i; return false; // key is not present.
        }
        if (entry == key) {
            *idx = i; return true; // key found
        }
        i++;
    }
    return false; // searched the entire table
}

bool hash_set_realloc_table(struct hash_set* ht) {
    u32 new_capacity = (u32) (ht->growth_factor * ht->capacity);
    //printf("GROW %lu -> %lu\n", ht->capacity, new_capacity);
    LOG("[GROW] hashset from %u to %u bytes.", ht->capacity, new_capacity);
    u32* old_data = ht->data;
    u32 old_capacity = ht->capacity;
    ht->data = (u32*) palrup_utils_calloc(new_capacity, sizeof(u32));
    if (!ht->data) return false;
    ht->size = 0;
    ht->max_size = (u32) (ht->growth_factor * ht->max_size);
    ht->capacity = new_capacity;
    for (u32 i = 0; i < old_capacity; i++) {
        u32 cell = old_data[i];
        if (cell != 0) {
            if (!hash_set_insert(ht, cell))
                return false;
        }
    }
    free(old_data);
    return true;
}

bool hash_set_handle_gap(struct hash_set* ht, u32 idx_of_gap) {

    u32 i = idx_of_gap;
    u32 j = i;
    while (true) {
        // search forward through the following cells of the table
        // until finding either another empty cell or a key that
        // can be moved to cell i (that is, a key whose hash value
        // is equal to or earlier than i)

        j = (j+1) & (ht->capacity-1);
        if (ht->data[j] == 0) {
            // empty cell found!
            // When an empty cell is found, then emptying cell i
            // is safe and the deletion process terminates.
            ht->data[i] = 0;
            return true;
        }

        u32 k = compute_idx_u32(ht, ht->data[j]);
        if ((j > i && (k <= i || k > j)) 
            || (j < i && k <= i && k > j)) {

            // movable cell found!
            // when the search finds a key that can be moved to cell i, it performs this move.
            u32* entry_at_deletion = &ht->data[i];
            u32* entry2move = &ht->data[j];
            *entry_at_deletion = *entry2move;
            *entry2move = 0;

            // This empties out another cell, later in the same block of occupied cells.
            // The search for a movable key continues for the new emptied cell,
            // in the same way, until it terminates by reaching a cell that was already empty.
            i = j;
        }
    }
}



struct hash_set* hash_set_init(int log_init_capacity) {
    struct hash_set* ht = palrup_utils_malloc(sizeof(struct hash_set));
    ht->capacity = 1<<log_init_capacity;
    ht->size = 0;
    ht->max_size = (u32) (ht->capacity >> 1);
    ht->growth_factor = 2;
    ht->data = (u32*) palrup_utils_calloc(ht->capacity, sizeof(u32));
    return ht;
}

bool hash_set_find(struct hash_set* ht, u32 key) {
    u32 idx;
    if (!hash_set_find_entry(ht, key, &idx)) return 0;
    ht->last_found_idx = idx;
    assert(ht->data[idx] == key);
    return true;
}

bool hash_set_insert(struct hash_set* ht, u32 key) {
    if (key == 0) return false; // key 0 is reserved!

    if (ht->size == ht->max_size) {
        if (!hash_set_realloc_table(ht)) return false; // no memory left
        if (ht->size >= ht->max_size)
            return false; // sth went very wrong during realloc
    }

    u32 idx;
    if (hash_set_find_entry(ht, key, &idx))
        return false; // found an element with this key!
    if (ht->data[idx] != 0)
        return false; // table completely full - shouldn't happen!
    // idx now points to the empty position

    ht->data[idx] = key;
    ht->size++;
    return true;
}

bool hash_set_delete(struct hash_set* ht, u32 key) {
    u32 idx;
    if (!hash_set_find_entry(ht, key, &idx)) return false;
    if (!hash_set_handle_gap(ht, idx)) return false;
    ht->size--;
    return true;
}

bool hash_set_delete_last_found(struct hash_set* ht) {
    if (!hash_set_handle_gap(ht, ht->last_found_idx)) return false;
    ht->size--;
    return true;
}

void hash_set_clear(struct hash_set* ht) {
    memset((void*) ht->data, 0, ht->size * sizeof(u32));
}

void hash_set_free(struct hash_set* ht) {
    free(ht->data);
    free(ht);
}
