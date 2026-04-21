
#include <unistd.h>
#include <time.h>
#include <stdlib.h>

#include "test_utils.h"
#include "../src/hash.h"
#include "../tracer/palrup_tracer.h"
#include "../src/file_reader.h"

#define TYPE u64
#define TYPED(THING) u64_##THING
#include "../src/vec.h"
#undef TYPED
#undef TYPE

#define TYPE int
#define TYPED(THING) int_##THING
#include "../src/vec.h"
#undef TYPED
#undef TYPE

#define NUM_SOLVERS 7
#define NUM_ORIG_CLAUSES 1280
#define MAX_NUM_HINTS 50
#define ID_COUNT 1000
#define FRAGMENT_PATH "Testing/palrup_tracer_test"
#define ADD_LINES 100
#define IMP_LINES 100
#define MAX_LIT 999
#define NUM_DELETIONS 15

// TODO: add missing tests

static void check_proof_fragment(size_t nb_expected_lines) {
    printf("   * check proof fragment syntax\n");
    struct file_reader* reader = file_reader_init(1024, fopen(FRAGMENT_PATH, "rb"), 0);
    u64 nb_read_lines = 0, last_produced_id = 0;
    struct u64_vec* hints_buffer = u64_vec_init(16);
    struct int_vec* lits_buffer = int_vec_init(16);

    while (true) {
        char c = file_reader_read_vbl_char(reader);
        if (file_reader_eof_reached(reader)) {
            do_assert(nb_read_lines == nb_expected_lines);
            break;
        } else if (c == TRUSTED_CHK_CLS_PRODUCE) {
            int_vec_resize(lits_buffer, 0);
            u64_vec_resize(hints_buffer, 0);

            u64 id = (u64)file_reader_read_vbl_ul(reader);
            do_assert(id > last_produced_id);
            //do_assert(id % NUM_SOLVERS == reader->pal_id);

            // parse lits
            while (true) {
                int lit = file_reader_read_vbl_int(reader);
                if (!lit) break;
            }

            // parse hints
            while (true) {
                u64 hint = (u64)file_reader_read_vbl_ul(reader);
                if (!hint) break;
                do_assert(hint < id);
            }

            nb_read_lines++;

        } else if (c == TRUSTED_CHK_CLS_IMPORT) {
            int_vec_resize(lits_buffer, 0);

            (u64)file_reader_read_vbl_ul(reader);

            // parse lits
            while (true) {
                int lit = file_reader_read_vbl_int(reader);
                if (!lit) break;
            }

            nb_read_lines++;

        } else if (c == TRUSTED_CHK_CLS_DELETE) {
            u64_vec_resize(hints_buffer, 0);

            // parse hints
            while (true) {
                u64 hint = (u64)file_reader_read_vbl_ul(reader);
                if (!hint) break;
            }

            nb_read_lines++;

        } else {
            printf("   * Invalid directive %X\n", c);
            do_assert(false);
        }
    }

    file_reader_end(reader);
    int_vec_free(lits_buffer);
    u64_vec_free(hints_buffer);
}

static void test_palrup_tracer_next_id() {
    printf("   * test plain id generation for solver ids [0, %u)\n", NUM_SOLVERS);
    for (size_t solver_id = 0; solver_id < NUM_SOLVERS; solver_id++) {
        struct palrup_tracer* tracer = palrup_tracer_init(NUM_ORIG_CLAUSES, true, FRAGMENT_PATH, solver_id, NUM_SOLVERS);
        u64 new_id = 0, old_id = NUM_ORIG_CLAUSES;
        int nb_hints = 0;
        for (size_t i = 0; i < ID_COUNT; i++) {
            new_id = palrup_tracer_next_id(tracer, nb_hints, NULL);
            do_assert(new_id > old_id);
            do_assert(new_id % NUM_SOLVERS == solver_id);
        }
        palrup_tracer_free(tracer);
    }
}

static void test_log_clause_addition() {
    printf("   * init palrup tracer\n");
    struct palrup_tracer* tracer = palrup_tracer_init(NUM_ORIG_CLAUSES, true, FRAGMENT_PATH, 0, NUM_SOLVERS);

    printf("   * generate and shuffle ids\n");
    int ids[NUM_ORIG_CLAUSES + ADD_LINES];
    for (int i = 0; i < NUM_ORIG_CLAUSES + ADD_LINES; i++)
        ids[i] = i + 1;
    int* shuffled_ids = generate_shuffled_copy_array(ADD_LINES, &(ids[NUM_ORIG_CLAUSES]));
    for (size_t i = 0; i < ADD_LINES; i++)
        ids[NUM_ORIG_CLAUSES + i] = shuffled_ids[i];
    free(shuffled_ids);

    printf("   * write %i clause additions\n", ADD_LINES);
    for (size_t i = 0; i < ADD_LINES; i++) {
        int nb_lits = drand48() * MAX_NUM_HINTS;
        int lits[nb_lits];
        for (int i = 0; i < nb_lits; i++)
            lits[i] = drand48() < 0.5 ? (random() % MAX_LIT) + 1 : (-random() % MAX_LIT) - 1;
        
        int nb_hints = drand48() * MAX_NUM_HINTS;
        unsigned long hints[nb_hints];
        for (int i = 0; i < nb_hints; i++) {
            hints[i] = (unsigned long)ids[(int)(drand48() * (NUM_ORIG_CLAUSES + i - 1))];
            do_assert(hints[i] > 0);
        }
        
        palrup_tracer_log_clause_addition(tracer, (unsigned long)ids[i], nb_lits, lits, nb_hints, hints);
    }

    printf("   * end palrup tracer\n");
    palrup_tracer_free(tracer);

    check_proof_fragment(ADD_LINES);
}

static void test_log_clause_import() {
    printf("   * init palrup tracer\n");
    struct palrup_tracer* tracer = palrup_tracer_init(NUM_ORIG_CLAUSES, true, FRAGMENT_PATH, 0, NUM_SOLVERS);

    printf("   * generate and shuffle ids\n");
    int ids[NUM_ORIG_CLAUSES + IMP_LINES];
    for (int i = 0; i < NUM_ORIG_CLAUSES + IMP_LINES; i++)
        ids[i] = i + 1;
    int* shuffled_ids = generate_shuffled_copy_array(IMP_LINES, &(ids[NUM_ORIG_CLAUSES]));
    for (size_t i = 0; i < IMP_LINES; i++)
        ids[NUM_ORIG_CLAUSES + i] = shuffled_ids[i];
    free(shuffled_ids);

    printf("   * write %i clause imports\n", IMP_LINES);
    for (size_t i = 0; i < IMP_LINES; i++) {
        int nb_lits = drand48() * MAX_NUM_HINTS;
        int lits[nb_lits];
        for (int i = 0; i < nb_lits; i++)
            lits[i] = drand48() < 0.5 ? (random() % MAX_LIT) + 1 : (-random() % MAX_LIT) - 1;
        
        palrup_tracer_log_clause_import(tracer, (unsigned long)ids[i], nb_lits, lits);
    }

    printf("   * end palrup tracer\n");
    palrup_tracer_free(tracer);

    check_proof_fragment(IMP_LINES);
}

static void test_log_clause_deletion() {
    printf("   * init palrup tracer\n");
    struct palrup_tracer* tracer = palrup_tracer_init(NUM_ORIG_CLAUSES, true, FRAGMENT_PATH, 0, NUM_SOLVERS);

    printf("   * log some deletions\n");
    for (size_t i = 0; i < NUM_DELETIONS; i++)
        palrup_tracer_log_clause_deletion(tracer, (unsigned long)i+1);

    printf("   * print out deletions and add ids to data base\n");
    for (size_t i = 0; i < NUM_DELETIONS; i++)
        palrup_tracer_log_clause_addition(tracer, i+1, 0, NULL, 0, NULL);

    printf("   * log some mapped deletions\n");
    for (size_t i = 0; i < NUM_DELETIONS; i++)
        palrup_tracer_log_clause_deletion(tracer, (unsigned long)i+1);

    printf("   * print out deletions\n");
    palrup_tracer_log_clause_import(tracer, NUM_DELETIONS + 2, 0, NULL);

    printf("   * end palrup tracer\n");
    palrup_tracer_free(tracer);

    printf("   * check written deletions\n");
    struct file_reader* reader = file_reader_init(1024, fopen(FRAGMENT_PATH, "rb"), 0);

    // first delete line
    do_assert(file_reader_read_char(reader) == TRUSTED_CHK_CLS_DELETE);
    for (size_t i = NUM_DELETIONS; i > 0; i--)
        do_assert(file_reader_read_vbl_ul(reader) == i);
    do_assert(file_reader_read_vbl_ul(reader) == 0);
    
    // addition lines
    for (size_t i = 0; i < NUM_DELETIONS; i++) {
        do_assert(file_reader_read_vbl_char(reader) == TRUSTED_CHK_CLS_PRODUCE);
        do_assert(file_reader_read_vbl_ul(reader) != i);    // mapped id
        do_assert(file_reader_read_vbl_int(reader) == 0);
        do_assert(file_reader_read_vbl_int(reader) == 0);
    }

    // second delete line
    do_assert(file_reader_read_vbl_char(reader) == TRUSTED_CHK_CLS_DELETE);
    for (size_t i = 0; i < NUM_DELETIONS; i++)
        do_assert(file_reader_read_vbl_ul(reader) != i+1);  // mapped ids
    do_assert(file_reader_read_vbl_ul(reader) == 0);
    
    // import line
    do_assert(file_reader_read_vbl_char(reader) == TRUSTED_CHK_CLS_IMPORT);
    file_reader_read_vbl_ul(reader);
    do_assert(file_reader_read_vbl_int(reader) == 0);

    // EOF
    do_assert(file_reader_eof_reached(reader));

    file_reader_end(reader);
}

static void init() {
    srand48(time(NULL));
    srandom(time(NULL));
}

int main(int argc, char const *argv[]) {
    UNUSED(argc);
    UNUSED(argv);

    printf("** init tests\n");
    init();

    printf("** test palrup_tracer_next_id\n");
    test_palrup_tracer_next_id();

    printf("** test palrup_tracer_log_clause_addition\n");
    test_log_clause_addition();

    printf("** test palrup_tracer_log_clause_import\n");
    test_log_clause_import();

    printf("** test palrup_tracer_log_clause_deletion\n");
    test_log_clause_deletion();

    printf("** DONE\n");
    return 0;
}
