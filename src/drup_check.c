
//#define NDEBUG  //TODO: remove

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "drup_check.h"
#include "utils/palrup_utils.h"

#define TYPE int
#define TYPED(THING) int_##THING
#include "vec.h"
#undef TYPED
#undef TYPE

#define TYPE drup_clause
#define TYPED(THING) drup_clause_##THING
#include "vec.h"
#undef TYPED
#undef TYPE

#define TYPE watcher
#define TYPED(THING) watcher_##THING
#include "vec.h"
#undef TYPED
#undef TYPE

// TODO: check for duplicate lits somewhere?
u64 blocking_lit_used = 0;
u64 nb_known_vars;
bool unsat_found = false;
bool formula_loaded = false;

// occurence list for 2 watched literals
// units do not need to be in occournce lists, since we assign them permanently
// TODO: Keep a literal of each clause directly in occurrence list to check if satisfied
// TODO: make occurences direct array of vec structs
struct watcher_vec* occurences;

// buffer for clause addition
struct int_vec* add_buffer;

// numer of original clauses added via load function
u64 original_ids = 0;

// current variable assignment
char* assignment;

// stack of currently assigned variables
struct int_vec* trail;

// stack of variables to be propagated
struct int_vec* prop_stack;

// stack of permanent unit clauses and their literals
struct int_vec* units;

#define ABS(X) (X < 0 ? -X : X)

static inline int get_idx(int lit) {
    assert(lit != 0);
    assert(ABS(lit) <= nb_known_vars);
    int sign = lit > 0;
    int ulit = sign ? lit : -lit;
    return (ulit * 2) - sign - 1;
}
//static inline int get_var(int lit) {
//    assert(lit != 0);
//    assert(ABS(lit) <= nb_known_vars);
//    return ABS(lit);
//}
//static inline char get_sign(int lit) {
//    assert(lit != 0);
//    assert(ABS(lit) <= nb_known_vars);
//    return lit < 0 ? -1 : 1;
//}
static int drup_check_propagate() {
    while (prop_stack->size > 0) {
        int lit = prop_stack->data[--(prop_stack->size)];
        assert(lit != 0);
        assert(ABS(lit) <= nb_known_vars);
        int idx = get_idx(lit);
        char a_sign = assignment[idx];

        // if var was already assigned a different value
        if (a_sign != 0 && (a_sign == -1))
            return 1;   // conflict found

        // assign value
        assert((a_sign == 1) || (a_sign == 0));
        if (a_sign == 0) {
            assignment[idx] = 1;
            assignment[idx ^ 1] = -1;
            int_vec_push(trail, lit);
        }
        assert(assignment[idx] == 1 && assignment[idx ^ 1] == -1);

        // fix occurences and propagate
        struct watcher_vec * const v = &(occurences[idx ^ 1]);
        for (u64 i = 0; i < v->size; i++) {
            watcher w = v->data[i];
            //__builtin_prefetch(w.c.ptr);
            //__builtin_prefetch(w.c.ptr + 1);
            int nb_lits = w.nb_lits;
            if (nb_lits == 1)
                return 1;   // conflict found
            assert(nb_lits > 1);

            if (assignment[get_idx(w.blocking_lit)] == 1) {
                blocking_lit_used++;
                continue;   // clause is satisfied
            }

            int first_watch = -lit;
            int second_watch;
            if (nb_lits == 2)   // binary clauses are stored in watch directly
                second_watch = w.blocking_lit == first_watch ? w.c.lit : w.blocking_lit;
            else
                second_watch = w.c.ptr[0] == first_watch ? w.c.ptr[1] : w.c.ptr[0];
            assert(first_watch == -lit);
            assert(first_watch != second_watch);
            assert(assignment[get_idx(first_watch)] == -1);
            int second_idx = get_idx(second_watch);

            // check second_watch
            if (assignment[second_idx] == 1)
                continue;   // clause is satisfied
            if (assignment[second_idx] == -1) {
                // second watch should be last unassigned lit in any clause
                #ifndef NDEBUG
                    // assert that all lits in clause are assigned
                    for (int j = 2; j < nb_lits; j++)
                        assert(assignment[get_idx(w.c.ptr[j])] != 0);
                #endif
                return 1;   // conflict found
            }
            assert(assignment[get_idx(first_watch)] == -1);
            assert(assignment[second_idx] == 0);

            // find first unassigned lit in c
            bool recurse = true;
            int* lits = w.c.ptr;
            for (int j = 2; j < nb_lits; j++) {
                int c_lit = lits[j];
                char c_sign = assignment[get_idx(c_lit)];
                if (c_sign == -1)
                    continue;

                // found unassigned or satisfied lit besides second_watch
                //  => swap with first_watch and fix occourence lists
                int x = lits[0] == first_watch ? 0 : 1;    // first watch might correspond to first or second lit
                lits[x] = c_lit;
                lits[j] = first_watch;

                watcher_vec_push(&(occurences[get_idx(c_lit)]), w);
                v->data[i--] = v->data[--(v->size)];

                recurse = false;
                break;
            }
        
            if (recurse)
                int_vec_push(prop_stack, second_watch);
        
        }   // for all occurences
    }   // while stack not empy

    return 0;   // no conflict found
}
static void drup_check_reset_assignment() {
    // reset assignments
    for (u64 i = 0; i < trail->size; i++) {
        int idx = get_idx(trail->data[i]);
        assignment[idx] = 0;
        assignment[idx ^ 1] = 0;
    }
    // clear trail
    int_vec_clear(trail);
    #ifndef NDEBUG
        // assert no assignments persist
        for (u64 i = 0; i < nb_known_vars * 2; i++)
            assert(assignment[i] == 0);
    #endif
}
static bool compare_lits(const int* lits1, const int* lits2, int nb_lits) {
    // lits are unsorted => quadratic
    for (int i = 0; i < nb_lits; i++) {
        int lit = lits1[i];
        bool found = false;
        for (int j = 0; j < nb_lits; j++) {
            if (lits2[j] == lit) {
                found = true;
                break;
            }
        }
        if (!found)
            return false;
    }
    
    return true;    // all lits found
}

void drup_check_init(int nb_vars) {
    nb_known_vars = nb_vars;
    unsat_found = false;
    formula_loaded = false;
    original_ids = 0;
    int nb_lits = 2 * nb_vars;
    occurences = (struct watcher_vec*)palrup_utils_malloc(nb_lits * sizeof(struct watcher_vec));
    for (int i = 0; i < nb_lits; i++) {
        // init vectors manualy to save indirection introduced by vec_init
        struct watcher_vec * const vec = &(occurences[i]);
        vec->size = 0;
        vec->capacity = 4;
        vec->data = palrup_utils_calloc(4, sizeof(watcher));
    }

    add_buffer = int_vec_init(16);
    assignment = palrup_utils_calloc(nb_lits, sizeof(u8));
    trail = int_vec_init(16);
    prop_stack = int_vec_init(16);
    units = int_vec_init(16);
}
void drup_check_end() {
    formula_loaded = true;
    for (size_t i = 0; i < (nb_known_vars * 2); i++) {
        struct watcher_vec * const v = &(occurences[i]);
        while (v->size > 0) {
            // Could be way more eficient, but we are only cleaning up
            watcher w = v->data[--(v->size)];
            //drup_check_delete_clause(drup_clause_get_lits(c), drup_clause_get_nb_lits(c));
            if (w.nb_lits > 2) {
                if (w.c.ptr[0] == 0) free(w.c.ptr);
                else w.c.ptr[0] = 0;    // mark clause as visited. will be freed with second watched lit
            }
        }
        free(v->data);
    }
    free(occurences);
    int_vec_free(add_buffer);
    free(assignment);
    int_vec_free(trail);
    int_vec_free(prop_stack);
    int_vec_free(units);

    // reset gloabl variables
    nb_known_vars = 0;
    unsat_found = false;
    formula_loaded = false;
    original_ids = 0;
    printf(">> blocking_lit_used:%lu\n", blocking_lit_used);
}

int drup_check_load(int lit) {
    if (formula_loaded)
        return 1;

    if (lit == 0) {
        u64 id = ++original_ids;
        int* lits = add_buffer->data;
        int nb_lits = add_buffer->size;
        drup_check_add_axiomatic_clause(id, lits, nb_lits);
                
        int_vec_resize(add_buffer, 0);
    } else int_vec_push(add_buffer, lit);
    return 0;
}
int drup_check_end_load() {
    if (add_buffer->size > 0) {
        snprintf(palrup_utils_msgstr, MSG_LEN, "%lu literals left in unfinished clause", add_buffer->size);
        palrup_utils_log_warn(palrup_utils_msgstr);
        return 1;
    }

    if (formula_loaded) {
        snprintf(palrup_utils_msgstr, MSG_LEN, "Loading of formula was already marked as done.");
        palrup_utils_log_warn(palrup_utils_msgstr);
    }

    formula_loaded = true;
    return 0;
}
u64 drup_check_get_nb_loaded_clauses() {
    return original_ids;
}

int drup_check_add_axiomatic_clause(u64 id, const int* lits, int nb_lits) {
    assert(nb_lits >= 0);
    
    // init clause
    //drup_clause c = drup_clause_init(id, lits, nb_lits);
    (void)id;
    
    // mark unsat as found
    if (nb_lits == 0) {
        unsat_found = true;
        return 0;
    }
    
    //int nb_lits;
    //int blocking_lit;
    //clause c;

    // handle unit as special case
    if (nb_lits == 1) {
        int lit = lits[0];
        int_vec_push(units, lit);
        //drup_clause_vec_push(occurences[get_idx(lit)], c);
        watcher w = { .nb_lits = 1, .blocking_lit = lit, .c.ptr = NULL };
        watcher_vec_push(&(occurences[get_idx(lit)]), w);
        return 0;
    }

    // handle binary as special case
    if (nb_lits == 2) {
        watcher w = { .nb_lits = 2, .blocking_lit = lits[0], .c.lit = lits[1] };
        watcher_vec_push(&(occurences[get_idx(lits[0])]), w);
        watcher_vec_push(&(occurences[get_idx(lits[1])]), w);
        return 0;
    }

    // add clause to occurence list of first two lits
    assert(nb_lits > 2);
    int* c = malloc(nb_lits * sizeof(int));
    memcpy(c, lits, nb_lits * sizeof(int));
    watcher w = { .nb_lits = nb_lits, .blocking_lit = lits[nb_lits - 1], .c.ptr = c };
    int idx = get_idx(lits[0]);
    watcher_vec_push(&(occurences[idx]), w);
    idx = get_idx(lits[1]);
    watcher_vec_push(&(occurences[idx]), w);

    return 0;
}
int drup_check_add_clause(u64 id, const int* lits, int nb_lits) {
    if (!formula_loaded) {
        snprintf(palrup_utils_msgstr, MSG_LEN, "Can not check clause %lu before formula is fully loaded.", id);
        palrup_utils_log_err(palrup_utils_msgstr);
        return 1;
    }
    
    int res = 1;
    int_vec_resize(prop_stack, units->size);
    memcpy(prop_stack->data, units->data, units->size * sizeof(int));
    for (int i = 0; i < nb_lits; i++)
        int_vec_push(prop_stack, -lits[i]);
    if (drup_check_propagate()) {   // conflict found
        drup_check_reset_assignment();
        drup_check_add_axiomatic_clause(id, lits, nb_lits);
        res = 0;
    } else drup_check_reset_assignment();
    
    //if (nb_lits == 0) {
    //    // propagate known units to check empty clause
    //    int_vec_resize(prop_stack, units->size);
    //    memcpy(prop_stack->data, units->data, units->size * sizeof(int));
    //    if (drup_check_propagate()) {   // conflict found
    //        drup_check_reset_assignment();
    //        drup_check_add_axiomatic_clause(id, lits, nb_lits);
    //        res = 0;
    //    }
    //}

    return res;
}
int drup_check_delete_clause(const int* lits, int nb_lits) {
    if (!formula_loaded) {
        snprintf(palrup_utils_msgstr, MSG_LEN, "Can not delete clause before formula is fully loaded.");
        palrup_utils_log_err(palrup_utils_msgstr);
        return 1;
    }

    // find first clause occurences and delete it 
    for (int i = 0; i < nb_lits; i++) {
        int lit = lits[i];
        int idx = get_idx(lit);
        struct watcher_vec * const v = &(occurences[idx]);

        for (u64 j = 0; j < v->size; j++) {
            watcher w = v->data[j];
            if (w.nb_lits != nb_lits)
                continue;
            // lits are stored inside watch if nb_lits < 3
            // TODO: check if comparison for binary clause works
            if (nb_lits > 2 ? compare_lits(lits, w.c.ptr, nb_lits) : compare_lits(lits, &(w.blocking_lit), nb_lits)) {
                // remove clause from occurence lists and delete
                v->data[j] = v->data[--(v->size)];
                if (nb_lits == 1)
                    return 0;   // only occurence was deleted

                if (nb_lits == 2)
                    break;  // second watch will be found and deleted by second iteration

                // Skip some iterations of outer loop since we know the second occurence list.
                int second_watch = w.c.ptr[0] == lit ? w.c.ptr[1] : w.c.ptr[0];
                struct watcher_vec * const v2 = &(occurences[get_idx(second_watch)]);
                for (size_t k = 0; k < v2->size; k++) {
                    watcher w2 = v2->data[k];
                    if (w2.c.ptr != w.c.ptr) continue;
                    // watch points to same clause => free clause and delete watch
                    v2->data[k] = v2->data[--(v2->size)];
                    free(w2.c.ptr);
                    return 0;
                }
                
                return 1;   // second watch not found
            }
        }
    }

    return 0;
}

bool drup_check_unsat_found() {
    return unsat_found;
}
bool drup_check_formula_loaded() {
    return formula_loaded;
}
u64 drup_check_get_clause_id(const int* lits, int nb_lits) {
    // TODO: implement?
    (void)lits;
    (void)nb_lits;
    //drup_clause c = find_clause(lits, nb_lits);
    //if (c) return drup_clause_get_id(c);
    return 0; // clause not found
}
drup_clause find_clause(const int* lits, int nb_lits) {
    // TODO: implement?
    (void)lits;
    (void)nb_lits;
    return 0;

    //for (int i = 0; i < nb_lits; i++) {
    //    int lit = lits[i];
    //    int idx = get_idx(lit);
    //    struct drup_clause_vec* v = occurences[idx];

    //    for (u64 j = 0; j < v->size; j++) {
    //        drup_clause c = v->data[j];
    //        if (drup_clause_get_nb_lits(c) != nb_lits)
    //            continue;
    //        if (compare_lits(lits, drup_clause_get_lits(c), nb_lits))
    //            return c;   // clause found
    //    }
    //}
    //return NULL; // clause not found
}

#ifdef UNIT_TEST
struct watcher_vec** get_occurences() { return occurences; }
#endif