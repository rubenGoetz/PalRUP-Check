
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "test_utils.h"
#include "../src/drup_check.h"
#include "../src/hash.h"
#include "../src/drup_clause.h"

#define MAX_NB_LITS 100
#define MAX_LIT 1000
#define NUM_CLAUSES 1000

drup_clause* clauses;
int vars;

static void test_drup_check_load() {
    printf("   * init drup checker\n");
    drup_check_init(vars);

    printf("   * load clauses\n");
    for (int i = 0; i < NUM_CLAUSES; i++) {
        drup_clause c = clauses[i];
        int nb_lits = drup_clause_get_nb_lits(c);
        int* lits = drup_clause_get_lits(c);
        for (int j = 0; j < nb_lits; j++)
            do_assert(!drup_check_load(lits[j]));
        do_assert(!drup_check_load(0));
    }
    do_assert(!drup_check_load(1)); // unfinished clause

    printf("   * check loaded clasues: ");
    do_assert(drup_check_get_nb_loaded_clauses() == NUM_CLAUSES);
    do_assert(drup_check_end_load());
    drup_check_load(0);
    do_assert(drup_check_get_nb_loaded_clauses() == NUM_CLAUSES + 1);
    do_assert(!drup_check_end_load());
    struct hash_table* clause_db = get_clause_db();
    for (u64 i = 0; i < NUM_CLAUSES; i++) {
        u64 id = i + 1;
        drup_clause c = hash_table_find(clause_db, id);
        do_assert(c);
        do_assert(drup_clause_get_id(c) == id);
        int nb_lits = drup_clause_get_nb_lits(c);
        int* lits = drup_clause_get_lits(c);
        do_assert(nb_lits == drup_clause_get_nb_lits(clauses[i]));
        for (int j = 0; j < nb_lits; j++)
            do_assert(lits[j] == drup_clause_get_lits(clauses[i])[j]);
    }
    drup_clause c = hash_table_find(clause_db, NUM_CLAUSES + 1);
    do_assert(c);
    do_assert(drup_clause_get_id(c) == NUM_CLAUSES + 1);
    do_assert(drup_clause_get_nb_lits(c) == 1);
    do_assert(*(drup_clause_get_lits(c)) == 1);

    printf("   * end drup checker\n");
    do_assert(!drup_check_unsat_found());
    drup_check_end();
}

static void test_drup_check_add_axiomatic_clause() {
    printf("   * init drup checker\n");
    drup_check_init(vars);

    printf("   * add axiomatic clauses\n");
    for (int i = 0; i < NUM_CLAUSES; i++) {
        drup_clause c = clauses[i];
        u64 id = drup_clause_get_id(c);
        int nb_lits = drup_clause_get_nb_lits(c);
        int* lits = drup_clause_get_lits(c);
        do_assert(!drup_check_add_axiomatic_clause(id, lits, nb_lits));
    }
    drup_check_end_load();
    
    printf("   * check added clauses\n");
    struct hash_table* clause_db = get_clause_db();
    for (int i = 0; i < NUM_CLAUSES; i++) {
        drup_clause c_original = clauses[i];
        u64 id = drup_clause_get_id(c_original);
        int nb_lits = drup_clause_get_nb_lits(c_original);
        int* lits = drup_clause_get_lits(c_original);
        drup_clause c_db = hash_table_find(clause_db, id);
        do_assert(id == drup_clause_get_id(c_db));
        do_assert(nb_lits == drup_clause_get_nb_lits(c_db));
        for (int j = 0; j < nb_lits; j++)
            do_assert(lits[j] == drup_clause_get_lits(c_db)[j]);
    }

    printf("   * delete clauses\n");
    do_assert(!drup_check_delete_clause(drup_clause_get_lits(clauses[0]), drup_clause_get_nb_lits(clauses[0])));
    do_assert(!drup_check_delete_clauses((u64*)clauses[1], 1));
    u64 ids[NUM_CLAUSES - 2];
    for (int i = 2; i < NUM_CLAUSES; i++)
        ids[i - 2] = drup_clause_get_id(clauses[i]);
    do_assert(!drup_check_delete_clauses(ids, NUM_CLAUSES - 2));
    do_assert(NUM_CLAUSES - 1 == drup_check_delete_clauses(ids, NUM_CLAUSES - 1));

    printf("   * check clause deletion\n");
    do_assert(clause_db->size == 0);

    printf("   * end drup checker\n");
    do_assert(!drup_check_unsat_found());
    drup_check_end();
}

static void test_drup_check_add_clause() {
    printf("   * init drup checker\n");
    drup_check_init(10);

    printf("   * add formula\n");
    int lits1[] = {1, 2, 3};
    drup_check_add_axiomatic_clause(1, lits1, 3);
    int lits2[] = {4, 5, 6};
    drup_check_add_axiomatic_clause(2, lits2, 3);
    int lits3[] = {1, -2};
    drup_check_add_axiomatic_clause(3, lits3, 2);
    int lits4[] = {-3, -4};
    drup_check_add_axiomatic_clause(4, lits4, 2);
    int lits5[] = {2, -7};
    drup_check_add_axiomatic_clause(5, lits5, 2);
    drup_check_end_load();

    printf("   * add redundant clauses\n");
    int lits11[] = {1, 5, 6};
    do_assert(!drup_check_add_clause(11, lits11, 3));
    int lits12[] = {1, -7};
    do_assert(!drup_check_add_clause(12, lits12, 2));
    int lits13[] = {-3, 6, 5};
    do_assert(!drup_check_add_clause(13, lits13, 3));

    printf("   * add non redundant clauses\n");
    int lits14[] = {8};
    do_assert(drup_check_add_clause(14, lits14, 1));
    int lits15[] = {1, 5};
    do_assert(drup_check_add_clause(15, lits15, 2));

    printf("   * check if added clauses exist in database\n");
    u64 ids[] = {1,2,3,4,5,11,12,13,14,15};
    struct hash_table* clause_db = get_clause_db();
    for (int i = 0; i < 8; i++) {
        u64 id = ids[i];
        drup_clause c = hash_table_find(clause_db, id);
        do_assert(c);
        do_assert(id == drup_clause_get_id(c));
    }
    for (int i = 8; i < 10; i++) {
        u64 id = ids[i];
        drup_clause c = hash_table_find(clause_db, id);
        do_assert(!c);
    }
    
    printf("   * end drup checker\n");
    do_assert(!drup_check_unsat_found());
    drup_check_end();
}

static void test_drup_check_unsat_found() {
    printf("   * init drup checker\n");
    drup_check_init(10);

    printf("   * load unsatisfiable formula\n");
    int lits1[] = {1, 2};
    drup_check_add_axiomatic_clause(1, lits1, 2);
    int lits2[] = {1, -2};
    drup_check_add_axiomatic_clause(2, lits2, 2);
    int lits3[] = {-1, 2};
    drup_check_add_axiomatic_clause(3, lits3, 2);
    int lits4[] = {-1, -2};
    drup_check_add_axiomatic_clause(4, lits4, 2);
    drup_check_end_load();

    printf("   * add proof clauses\n");
    int lits5[] = {1};
    do_assert(!drup_check_add_clause(5, lits5, 1));
    int lits6[] = {0};
    do_assert(!drup_check_add_clause(6, lits6, 0));

    printf("   * check unsat\n");
    do_assert(drup_check_unsat_found());

    printf("   * end drup checker\n");
    drup_check_end();
}

static void test_drup_check_get_clause_id() {
    printf("   * init drup checker\n");
    drup_check_init(vars);

    printf("   * add axiomatic clauses\n");
    for (int i = 0; i < NUM_CLAUSES; i++) {
        drup_clause c = clauses[i];
        u64 id = drup_clause_get_id(c);
        int nb_lits = drup_clause_get_nb_lits(c);
        int* lits = drup_clause_get_lits(c);
        do_assert(!drup_check_add_axiomatic_clause(id, lits, nb_lits));
    }

    printf("   * find added clauses\n");
    for (int i = 0; i < NUM_CLAUSES; i++) {
        drup_clause c = clauses[i];
        u64 id = drup_clause_get_id(c);
        int nb_lits = drup_clause_get_nb_lits(c);
        int* lits = drup_clause_get_lits(c);
        do_assert(id == drup_check_get_clause_id(lits, nb_lits));
    }

    printf("   * end drup checker\n");
    drup_check_end();
}

static void init_tests() {
    srand48(time(NULL));
    srandom(time(NULL));

    printf("   * generate clasues\n");
    clauses = palrup_utils_malloc(NUM_CLAUSES * sizeof(drup_clause));
    drup_clause* cs = clauses;
    UNUSED(cs);
    vars = 0;
    for (size_t i = 0; i < NUM_CLAUSES; i++) {
        u64 id = i + 1;
        int nb_lits = (random() % MAX_NB_LITS) + 1;
        int lits[nb_lits];
        for (int j = 0; j < nb_lits; j++) {
            int lit = (random() % MAX_LIT) + 1;
            if (lit == 0) lit = 1;
            if (lit > vars) vars = lit;
            lit *= (drand48() > 0.5 ? 1 : -1);
            lits[j] = lit;
        }
        clauses[i] = drup_clause_init(id, lits, nb_lits);
    }
}

static void wrap_up_tests() {
    for (size_t i = 0; i < NUM_CLAUSES; i++)
        drup_clause_free(clauses[i]);
    free(clauses);
}

int main(int argc, char const *argv[]) {
    UNUSED(argc);
    UNUSED(argv);

    printf("** init tests\n");
    init_tests();

    printf("** test drup_check_load\n");
    test_drup_check_load();

    printf("** test drup_check_add_axiomatic_clause\n");
    test_drup_check_add_axiomatic_clause();

    printf("** test drup_check_add_clause\n");
    test_drup_check_add_clause();

    printf("** test drup_check_unsat_found\n");
    test_drup_check_unsat_found();

    printf("** test drup_check_get_clause_id\n");
    test_drup_check_get_clause_id();

    printf("** wrap up tests\n");
    wrap_up_tests();

    return 0;
}
