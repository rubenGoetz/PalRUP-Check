
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_utils.h"

void do_assert(bool cond) {
    #ifndef NDEBUG
        assert(cond);
    #else
        if (!cond) {
            printf("Assertion failed! Compile with DEBUG mode for details.\n");
            abort();
        }
    #endif
}

int* generate_sorted_array(int elem_count, int factor) {
    printf("   * create array of sorted integers\n");
    int* a = palrup_utils_calloc(elem_count, sizeof(int));
    for (int i = 0; i < elem_count; i++)
        a[i] = i - (elem_count / 2) * factor;
    return a;
}

int* generate_copy_array(int elem_count, int* a) {
    printf("   * create copy of array\n");
    int* b = palrup_utils_calloc(elem_count, sizeof(int));
    memcpy(b, a , elem_count * sizeof(int));
    return b;
}

int* generate_shuffled_copy_array(int elem_count, int* a) {
    printf("   * create shuffled copy of array\n");
    int* b = palrup_utils_calloc(elem_count, sizeof(int));
    memcpy(b, a, elem_count * sizeof(int));
    for (size_t i = elem_count - 1; i > 0; i--) {
        size_t j = (size_t) (drand48()*(i+1));
        int t = b[j];
        b[j] = b[i];
        b[i] = t;
    }
    return b;
}
