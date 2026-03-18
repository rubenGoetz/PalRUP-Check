
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "test_utils.h"
#include "../src/utils/palrup_utils.h"
#include "../src/utils/checker_utils.h"

#define DEFAULT_ELEM_COUNT 1000

// ----- UTIL -----

static int* generate_sorted_array(int factor) {
    printf("   * create array of sorted integers\n");
    int* a = palrup_utils_calloc(DEFAULT_ELEM_COUNT, sizeof(int));
    for (size_t i = 0; i < DEFAULT_ELEM_COUNT; i++)
        a[i] = i - (DEFAULT_ELEM_COUNT / 2) * factor;
    return a;
}

static int* generate_copy_array(int* a) {
    printf("   * create copy of array\n");
    int* b = palrup_utils_calloc(DEFAULT_ELEM_COUNT, sizeof(int));
    memcpy(b, a , DEFAULT_ELEM_COUNT * sizeof(int));
    return b;
}

static int* generate_shuffled_copy_array(int* a) {
    printf("   * create shuffled copy of array\n");
    int* b = palrup_utils_calloc(DEFAULT_ELEM_COUNT, sizeof(int));
    memcpy(b, a, DEFAULT_ELEM_COUNT * sizeof(int));
    for (size_t i = DEFAULT_ELEM_COUNT - 1; i > 0; i--) {
        size_t j = (size_t) (drand48()*(i+1));
        int t = b[j];
        b[j] = b[i];
        b[i] = t;
    }
    return b;
}

static u8* generate_sig() {
    printf("   * generate signature\n");
    u8* sig = (u8*)palrup_utils_malloc(SIG_SIZE_BYTES);
    for (size_t i = 0; i < SIG_SIZE_BYTES; i++)
        sig[i] = drand48() * (u8)-1;
    return sig;
}

static u8* generate_copy_sig(u8* sig) {
    printf("   * create copy of signature\n");
    u8* new_sig = (u8*)palrup_utils_malloc(SIG_SIZE_BYTES);
    memcpy(new_sig, sig, SIG_SIZE_BYTES);
    return new_sig;
}

// ----- TEST -----

static void test_checker_utils_check_hints() {
    printf("   * create arrays of hints\n");
    size_t elem_count = DEFAULT_ELEM_COUNT;
    u64* a = palrup_utils_calloc(elem_count, sizeof(u64));
    for (size_t i = 0; i < elem_count; i++)
        a[i] = drand48() * (u64)-3 + 3;     // assert some smaller ids are possible
    
    printf("   * check hint invariant\n");
    do_assert(!checker_utils_check_hints(a[0], a, elem_count));
    do_assert(!checker_utils_check_hints(1, a, elem_count));
    do_assert(checker_utils_check_hints((u64)-1, a, elem_count));
    
    printf("    * free hints\n");
    free(a);
}

static void test_checker_utils_equal_signatures() {
    u8* a = generate_sig();
    u8* b = generate_copy_sig(a);

    printf("   * check comparison between signatures\n");
    do_assert(checker_utils_equal_signatures(a, b));
    b[0] = ~b[0];
    do_assert(!checker_utils_equal_signatures(a, b));

    printf("   * free signatures\n");
    free(a);
    free(b);
}

static void test_bin_search() {
    size_t elem_count = DEFAULT_ELEM_COUNT;
    int* a = generate_sorted_array(3);

    printf("   * try to find every element in array\n");
    for (size_t i = 0; i < elem_count; i++)
        do_assert(bin_search(a, a[i], 0, elem_count));
    
    printf("   * try to find missing elements\n");
    do_assert(!bin_search(a, 2, 0, elem_count));
    do_assert(!bin_search(a, 7, 0, elem_count));
    do_assert(!bin_search(a, -2, 0, elem_count));
    do_assert(!bin_search(a, -7, 0, elem_count));
    do_assert(!bin_search(a, a[0] - 1, 0, elem_count));
    do_assert(!bin_search(a, a[elem_count - 1] + 1, 0, elem_count));

    printf("   * check variable start / end\n");
    int start = elem_count / 4, end = 3* (elem_count / 4);
    do_assert(bin_search(a, a[elem_count / 2], start, end));
    do_assert(!bin_search(a, a[0], start, end));
    do_assert(!bin_search(a, a[elem_count - 1], start, end));

    printf("   * free array\n");
    free(a);
}

static void test_checker_utils_compare_lits() {
    size_t elem_count = DEFAULT_ELEM_COUNT;
    int* a = generate_sorted_array(1);
    int* b = generate_copy_array(a);
    int* c = generate_shuffled_copy_array(a);

    printf("   * check comparison between arrays\n");
    do_assert(checker_utils_compare_lits(a, b, elem_count, elem_count));
    do_assert(!checker_utils_compare_lits(a, b, elem_count, elem_count - 1));
    b[(size_t)drand48()*elem_count] = elem_count;   // alter b
    do_assert(!checker_utils_compare_lits(a, b, elem_count, elem_count));
    do_assert(!checker_utils_compare_lits(a, c, elem_count, elem_count));

    printf("   * free arrays\n");
    free(a);
    free(b);
    free(c);
}

static void test_checker_utils_compare_semi_sorted_lits() {
    size_t elem_count = DEFAULT_ELEM_COUNT;
    int* a = generate_sorted_array(1);
    int* b = generate_shuffled_copy_array(a);

    printf("   * check comparison between arrays\n");
    do_assert(checker_utils_compare_semi_sorted_lits(a, b, elem_count, elem_count));
    do_assert(!checker_utils_compare_semi_sorted_lits(a, b, elem_count, elem_count - 1));
    b[(size_t)drand48()*elem_count] = elem_count;   // alter b
    do_assert(!checker_utils_compare_semi_sorted_lits(a, b, elem_count, elem_count));

    printf("   * free arrays\n");
    free(a);
    free(b);
}

// ----- INIT -----

static void init_tests() {
    srand48(time(NULL));
}

static void wrap_up_tests() {

}

int main(int argc, char const *argv[])
{
    UNUSED(argc); UNUSED(argv);

    printf("** initialize tests\n");
    init_tests();
    
    printf("** test checker_utils_check_hints\n");
    test_checker_utils_check_hints();

    printf("** test checker_utils_equal_signatures\n");
    test_checker_utils_equal_signatures();

    printf("** test checker_utils_compare_lits\n");
    test_checker_utils_compare_lits();

    printf("** test bin_search\n");
    test_bin_search();
    
    printf("** test checker_utils_compare_semi_sorted_lits\n");
    test_checker_utils_compare_semi_sorted_lits();

    printf("** wrap tests up\n");
    wrap_up_tests();

    printf("** DONE\n");
    return 0;
}
