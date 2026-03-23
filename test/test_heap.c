
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "test_utils.h"
#include "../src/heap.h"

//#include "../src/clause_flat.h"

#define DEFAULT_HEAP_CAPACITY 1024 * 1024
#define DEFAULT_MAX_NB_LITS 100

// Maximum heap capacity in Byte
unsigned int HEAP_CAPACITY;
unsigned int MAX_NB_LITS;
unsigned int NUM_CLAUSES;

clause_ptr* clauses;
clause_ptr bonus_clause;
struct clause_heap* heap;

// insert clauses with duplicates until heap is full
static size_t fill_heap_with_duplicates(unsigned int offset) {
    printf("   * insert duplicate clauses into heap\n");
    size_t unique_clause_cnt = 0;
    bool exit_loop = false;
    while (true) {
        for (size_t i = 0; i < unique_clause_cnt; i++) {
            size_t idx = (offset + unique_clause_cnt) % NUM_CLAUSES;
            clause_ptr c = clauses[idx];
            clause_ptr c_copy = create_flat_clause(get_clause_id(c),
                                                   get_clause_nb_lits(c),
                                                   get_clause_lits(c));
            if (heap_insert(heap, c_copy)) {
                exit_loop = true;
                delete_flat_clause(c_copy);
                break;
            }
        }
        if (exit_loop) {
            break;
        }
        unique_clause_cnt++;
    }
    printf("   * inserted %lu unique clauses %lu times\n", unique_clause_cnt, heap->element_count);
    return unique_clause_cnt;
}

// ----- TEST -----

static void build_heap() {
    printf("   * insert clauses\n");
    u64 old_size = 0;
    u64 old_count = 0;
    u64 initial_data_size = heap->data_size;
    for (u64 i = 0; i < NUM_CLAUSES; i++) {
        int res = heap_insert(heap, clauses[i]);
        do_assert(!res);
        do_assert(old_size + get_clause_size(clauses[i]) == heap->size);
        do_assert(old_count + 1 == heap->element_count);
        old_size = heap->size;
        old_count = heap->element_count;
    }
    do_assert(heap_insert(heap, bonus_clause));
    do_assert(heap->size == old_size);
    do_assert(heap->element_count == old_count);

    // assert that heap->data has grown succesfully
    do_assert(initial_data_size < heap->data_size);
}

static void check_size() {
    printf("   * check heap size\n");
    do_assert(heap->element_count == NUM_CLAUSES);
    do_assert(heap->size <= HEAP_CAPACITY);
}

static void check_element_count() {
    printf("   * check element count\n");
    do_assert(heap->element_count == NUM_CLAUSES);
    for (u64 i = 0; i < NUM_CLAUSES; i++) {
        do_assert(heap->data[i] != NULL);
    }
}

static void check_heap_invariant() {
    printf("   * check heap invariant\n");
    for (u64 i = 0; i < heap->element_count; i++)
    {
        u64 left_child_idx = (i * 2) + 1;
        u64 right_child_idx = (i * 2) + 2;

        clause_ptr clause = heap->data[i];

        if (left_child_idx < heap->element_count) {
            clause_ptr left_child = heap->data[left_child_idx];
            do_assert(get_clause_id(clause) <= get_clause_id(left_child));
        }

        if (right_child_idx < heap->element_count) {
            clause_ptr right_child = heap->data[right_child_idx];
            do_assert(get_clause_id(clause) <= get_clause_id(right_child));
        }
    }
}

static void check_heap_min() {
    printf("   * check heap min\n");
    do_assert(compare_flat_clause(heap->data[0], heap_get_min(heap)));
}

static void check_flush() {
    printf("   * check flush heap\n");
    clause_ptr* sorted_clauses = palrup_utils_calloc(NUM_CLAUSES, sizeof(clause_ptr));

    for (u64 i = 0; i < NUM_CLAUSES; i++) {
        u64 old_count = heap->element_count;
        clause_ptr clause = heap_pop_min(heap);
        
        do_assert(heap->element_count == old_count - 1);
        if (heap->size > 0)
            do_assert(!compare_flat_clause(clause, heap_get_min(heap)));

        sorted_clauses[i] = clause;
    }
    do_assert(heap->size == 0);
    do_assert(heap->element_count == 0);

    for (u64 i = 1; i < NUM_CLAUSES; i++)
        do_assert(get_clause_id(sorted_clauses[i]) >= get_clause_id(sorted_clauses[i - 1]));

    free(sorted_clauses);
}

static void test_heap_delete_duplicates() {
    do_assert(heap->size == 0);
    size_t unique_clause_cnt = fill_heap_with_duplicates(0);

    printf("   * check flush with duplicate skipping\n");
    for (size_t i = 0; i < unique_clause_cnt; i++) {
        do_assert(heap->element_count > 0);
        heap_delete_duplicates(heap);
        clause_ptr c = heap_pop_min(heap);
        if (heap_get_min(heap)) {
            if (get_clause_id(c) == get_clause_id(heap_get_min(heap))) {
                printf("HEAP:\n");
                printf("  heap->cap=%lu\n", heap->capacity);
                printf("  heap->size=%lu\n", heap->size);
                printf("  heap->elem_count=%lu\n", heap->element_count);
                printf("  heap->data_size=%lu\n", heap->data_size);
            }
            do_assert(get_clause_id(c) != get_clause_id(heap_get_min(heap)));
        }
        delete_flat_clause(c);
    }
    do_assert(heap->size == 0);
    do_assert(heap->element_count == 0);
    

    printf("   * empty heap\n");
    while (heap->size > 0)
        delete_flat_clause(heap_pop_min(heap));

    printf("   * check detection of differing clauses with same id\n");
    int pid = fork();
    int status;
    do_assert(pid >= 0);
    if (pid == 0) {
        // Mute child process
        if (!freopen("/dev/null", "w", stdout))
            printf("   * could not mute child process\n");

        int lits1[3] = {1,2,3};
        int lits2[3] = {4,5,6};
        clause_ptr c1 = create_flat_clause(1,3,lits1);
        clause_ptr c2 = create_flat_clause(1,3,lits2);

        heap_insert(heap, c1);
        heap_insert(heap, c2);
        heap_delete_duplicates(heap);     // expected to abort

        delete_flat_clause(c1);
        delete_flat_clause(c2);
        exit(0);
    } else
        wait(&status);
        
    do_assert(status != 0);
}

// ----- INIT -----

static void generate_random_clauses() {
    printf("   * generate clauses\n");

    // Unit clause is 8 Byte
    // => at most HEAP_CAPACITY / 8 clauses will be generated
    clauses = (clause_ptr*)palrup_utils_calloc(HEAP_CAPACITY / 8, sizeof(clause_ptr));
    NUM_CLAUSES = 0;
    u64 clause_mem = 0;
    while (true) {
        u64 id = (u64)random();
        unsigned int nb_lits = (unsigned int)((random() % MAX_NB_LITS) + 1);
        int* lits = palrup_utils_calloc(nb_lits, sizeof(int));
        lits[0] = (int)NUM_CLAUSES;    // esures differing clauses
        for (size_t j = 1; j < nb_lits; j++)
            lits[j] = random();
        clause_ptr c = create_flat_clause(id, nb_lits, lits);
        free(lits);
        // enough clauses generated
        if (clause_mem + get_clause_size(c) > HEAP_CAPACITY) {
            bonus_clause = c;
            break;
        }
        clauses[NUM_CLAUSES++] = c;
        clause_mem += get_clause_size(c);
    }

    printf("   * generated %u clauses, allocating %luB\n", NUM_CLAUSES, clause_mem);
}

static void init_tests() {
    srandom(time(NULL));
    printf("   * init heap\n");
    heap = heap_init(HEAP_CAPACITY);
    NUM_CLAUSES = 0;
    generate_random_clauses();
}

static void wrap_up_tests() {
    for (size_t i = 0; i < NUM_CLAUSES; i++)
        delete_flat_clause(clauses[i]);
    delete_flat_clause(bonus_clause);
    free(clauses);
    heap_free(heap);
}

int main(int argc, char *argv[]) {
    if (argc == 1) {
        printf("** no arguments given, default to HEAP_CAPACITY = %uB, MAX_NB_LITS=%u\n", DEFAULT_HEAP_CAPACITY, DEFAULT_MAX_NB_LITS);
        HEAP_CAPACITY = DEFAULT_HEAP_CAPACITY;
        MAX_NB_LITS = DEFAULT_MAX_NB_LITS;
    } else if (argc == 2) {
        printf("** set HEAP_CAPACITY = %sB, MAX_NB_LITS=%u\n", argv[1], DEFAULT_MAX_NB_LITS);
        HEAP_CAPACITY = atoi(argv[1]);
        MAX_NB_LITS = DEFAULT_MAX_NB_LITS;
    } else if (argc == 3) {
        printf("** set HEAP_CAPACITY = %sB, MAX_NB_LITS=%s\n", argv[1], argv[2]);
        HEAP_CAPACITY = atoi(argv[1]);
        MAX_NB_LITS = atoi(argv[2]);
    } else {
        printf("** can take at most two arguments\n");
        printf("** ABORT\n");
        abort();
    }

    printf("** initialize tests\n");
    init_tests();

    printf("** build heap\n");
    build_heap();
    
    printf("** check initial heap\n");
    check_size();
    check_element_count();
    check_heap_invariant();

    printf("** check heap behaviour\n");
    check_heap_min();
    check_flush();

    printf("** check heap duplicate deletion\n");
    test_heap_delete_duplicates();

    printf("** wrap tests up\n");
    wrap_up_tests();

    printf("** DONE\n");
    return 0;
}
