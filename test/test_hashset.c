#include "test_utils.h"
#include "../src/hashset.h"

void test_small() {
    printf("[TEST] --- begin test_small() ---\n");

    struct hash_set* hs = hash_set_init(7);
    do_assert(hs->size == 0);
    do_assert(hs->capacity == 1<<7);

    bool ok;
    u64 cap = hs->capacity;
    for (u32 i = 1; i <= 63; i++) {
        ok = hash_set_insert(hs, i);
        do_assert(ok);
        do_assert(hs->size == i);
        do_assert(hs->capacity == cap);
        do_assert(hash_set_find(hs, i));
    }
    do_assert(hs->size == 63);
    do_assert(hs->capacity == 128);

    for (u32 i = 1; i <= 63; i++) {
        do_assert(hash_set_find(hs, i));
    }

    ok = hash_set_insert(hs, 64);
    do_assert(ok);
    do_assert(hs->size == 64);
    do_assert(hs->capacity == 128);

    ok = hash_set_insert(hs, 65);
    do_assert(ok);
    do_assert(hs->size == 65);
    do_assert(hs->capacity == 256);

    for (u32 i = 1; i <= 65; i++) {
        if (i == 40) continue;
        do_assert(hash_set_find(hs, i));
        do_assert(hash_set_delete(hs, i));
        do_assert(!hash_set_find(hs, i));
    }
    do_assert(hs->size == 1);
    do_assert(hash_set_find(hs, 40));
    do_assert(hash_set_delete_last_found(hs));
    do_assert(hs->size == 0);
    do_assert(!hash_set_find(hs, 40));

    hash_set_free(hs);

    printf("[TEST] ---  end  test_small() ---\n\n");
}

void test_big() {
    printf("[TEST] --- begin test_big() ---\n");

    struct hash_set* hs = hash_set_init(7);
    do_assert(hs->size == 0);

    bool ok;

    u32 nb_elems = (1u<<20)+3;
    for (u32 i = 1; i <= nb_elems; i++) {
        do_assert(!hash_set_find(hs, i));
        ok = hash_set_insert(hs, i);
        do_assert(ok);
        do_assert(hash_set_find(hs, i));
    }
    do_assert(hs->size == nb_elems);

    for (u32 i = 1; i <= nb_elems; i++) {
        do_assert(hs->size == nb_elems - i + 1);
        do_assert(hash_set_find(hs, i));
        ok = hash_set_delete_last_found(hs);
        do_assert(ok);
        do_assert(!hash_set_find(hs, i));
    }
    do_assert(hs->size == 0);

    hash_set_free(hs);

    printf("[TEST] ---  end  test_big() ---\n\n");
}

void test_alternate() {
    printf("[TEST] --- begin test_alternate() ---\n");

    struct hash_set* hs = hash_set_init(7);
    do_assert(hs->size == 0);

    bool ok;

    // In block sizes of b=256, insert b elements and then delete b/2 of them
    // (those with an odd key, since keys start at 1).
    const u32 block_size = 256;
    const u32 nb_iterations = 9999;
    u32 counter = 1;
    for (u32 outer = 0; outer < nb_iterations; outer++) {
        for (u32 i = 0; i < block_size; i++) {
            do_assert(!hash_set_find(hs, counter));
            ok = hash_set_insert(hs, counter);
            do_assert(ok);
            do_assert(hash_set_find(hs, counter));
            counter++;
        }
        for (u32 delcounter = counter - block_size; delcounter < counter; delcounter += 2) {
            do_assert(hash_set_find(hs, delcounter));
            ok = hash_set_delete_last_found(hs);
            do_assert(ok);
            do_assert(!hash_set_find(hs, delcounter));
        }
    }

    // Check that all odd keys are deleted and all even keys are present.
    printf("size=%lu counter=%u\n", (unsigned long)hs->size, counter);
    for (u32 i = 1; i <= counter; i++) {
        if (i % 2 == 1) {
            // deleted
            do_assert(!hash_set_find(hs, i));
        } else if (i < counter) {
            // present
            do_assert(hash_set_find(hs, i));
        }
    }
    do_assert(hs->size == counter/2);

    hash_set_free(hs);

    printf("[TEST] ---  end  test_alternate() ---\n\n");
}

int main() {
    test_small();
    test_big();
    test_alternate();
}