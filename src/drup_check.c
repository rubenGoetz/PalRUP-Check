
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

#define TYPE u8
#define TYPED(THING) u8_##THING
#include "vec.h"
#undef TYPED
#undef TYPE

#define TYPE drup_clause
#define TYPED(THING) drup_clause_##THING
#include "vec.h"
#undef TYPED
#undef TYPE

// TODO: check for duplicate lits somewhere?

u64 nb_known_vars;
bool unsat_found = false;
bool formula_loaded = false;

// occurence list for 2 watched literals
// units do not need to be in occournce lists, since we assign them permanently
// TODO: Keep a literal of each clause directly in occurrence list to check if satisfied
struct drup_clause_vec** occurences;

// buffer for clause addition
struct int_vec* add_buffer;

// numer of original clasues added via load function
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
static inline int get_var(int lit) {
    assert(lit != 0);
    assert(ABS(lit) <= nb_known_vars);
    return ABS(lit);
}
static inline char get_sign(int lit) {
    assert(lit != 0);
    assert(ABS(lit) <= nb_known_vars);
    return lit < 0 ? -1 : 1;
}
static int drup_check_propagate() {
    while (prop_stack->size > 0) {
        int lit = prop_stack->data[--(prop_stack->size)];
        assert(lit != 0);
        assert(ABS(lit) <= nb_known_vars);

        int var = get_var(lit);
        char a_sign = assignment[var];
        char lit_sign = get_sign(lit);

        // if var was already assigned a different value
        if (a_sign != 0 && (a_sign != lit_sign))
            return 1;   // conflict found

        // assign value
        assert((assignment[var] == lit_sign) || (assignment[var] == 0));
        if (a_sign == 0) {
            assignment[var] = lit_sign;
            int_vec_push(trail, lit);        
        }
        assert(assignment[var] == lit_sign);

        // fix occurences and propagate
        struct drup_clause_vec* v = occurences[get_idx(-lit)];
        for (u64 i = 0; i < v->size; i++) {
            drup_clause c = v->data[i];
            int nb_lits = drup_clause_get_nb_lits(c);
            int* lits = drup_clause_get_lits(c);
            if (nb_lits == 1)
                return 1;   // conflict found
            assert(nb_lits > 1);

            // make first_watch -lit
            int first_watch = lits[0];
            int second_watch = lits[1];
            if (second_watch == -lit) {
                int tmp = first_watch;
                first_watch = second_watch;
                second_watch = tmp;

                lits[0] = first_watch;
                lits[1] = second_watch;
            }
            assert(first_watch == -lit);
            assert(assignment[get_var(first_watch)] == get_sign(lit));

            // check second_watch
            if (assignment[get_var(second_watch)] == get_sign(second_watch))
                continue;   // clause is satisfied
            if (assignment[get_var(second_watch)] == -get_sign(second_watch)) {
                // second watch should be last unassigned lit in any clause
                #ifndef NDEBUG
                    // assert that all lits in clause are assigned
                    for (int j = 2; j < nb_lits; j++)
                        assert(assignment[get_var(lits[j])] != 0);
                #endif
                return 1;   // conflict found
            }
            assert(assignment[get_var(first_watch)] == -get_sign(first_watch));
            assert(assignment[get_var(second_watch)] == 0);

            // find first unassigned lit in c
            bool recurse = true;
            for (int j = 2; j < nb_lits; j++) {
                int c_lit = lits[j];
                char c_sign = assignment[get_var(c_lit)];
                if (c_sign == -get_sign(c_lit))
                    continue;

                // found unassigned or satisfied lit besides second_watch
                //  => swap with first_watch and fix occourence lists
                lits[0] = c_lit;
                lits[j] = first_watch;

                drup_clause_vec_push(occurences[get_idx(c_lit)], c);
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
    for (u64 i = 0; i < trail->size; i++)
        assignment[get_var(trail->data[i])] = 0;
    // clear trail
    int_vec_clear(trail);
    #ifndef NDEBUG
        // assert units are still assigned
        for (u64 i = 0; i < nb_known_vars + 1; i++)
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
    occurences = (struct drup_clause_vec**)palrup_utils_malloc(nb_lits * sizeof(struct drup_clause_vec*));
    for (int i = 0; i < nb_lits; i++)
        occurences[i] = drup_clause_vec_init(4);

    add_buffer = int_vec_init(16);
    assignment = palrup_utils_calloc(nb_vars + 1, sizeof(u8));
    trail = int_vec_init(16);
    prop_stack = int_vec_init(16);
    units = int_vec_init(16);
}
void drup_check_end() {
    formula_loaded = true;
    for (size_t i = 0; i < (nb_known_vars * 2); i++) {
        struct drup_clause_vec* v = occurences[i];
        for (size_t j = 0; j < v->size; j++) {
            // Could be way more eficient, but we are only cleaning up
            drup_clause c = v->data[j--];
            drup_check_delete_clause(drup_clause_get_lits(c), drup_clause_get_nb_lits(c));
        }
        drup_clause_vec_free(v);
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
    drup_clause c = drup_clause_init(id, lits, nb_lits);
    
    // mark unsat as found
    if (nb_lits == 0) {
        unsat_found = true;
        return 0;
    }
    
    // handle unit as special case
    if (nb_lits == 1) {
        int lit = lits[0];
        int_vec_push(units, lit);
        drup_clause_vec_push(occurences[get_idx(lit)], c);
        return 0;
    }

    // add clause to occurence list of first two lits
    int idx = get_idx(lits[0]);
    drup_clause_vec_push(occurences[idx], c);
    idx = get_idx(lits[1]);
    drup_clause_vec_push(occurences[idx], c);

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
        struct drup_clause_vec* v = occurences[idx];

        for (u64 j = 0; j < v->size; j++) {
            drup_clause c = v->data[j];
            if (drup_clause_get_nb_lits(c) != nb_lits)
                continue;
            if (compare_lits(lits, drup_clause_get_lits(c), nb_lits)) {
                // remove clause from occurence lists and delete
                v->data[j] = v->data[--(v->size)];
                if (drup_clause_get_nb_lits(c) == 1) {
                    drup_clause_free(c);
                    return 0;
                }

                int second_watch = drup_clause_get_lits(c)[0];
                if (second_watch == lit)
                    second_watch = drup_clause_get_lits(c)[1];
                struct drup_clause_vec* v2 = occurences[get_idx(second_watch)];
                for (size_t k = 0; k < v2->size; k++) {
                    drup_clause c2 = v2->data[k];
                    if (c2 != c) continue;
                    v2->data[k] = v2->data[--(v2->size)];
                    drup_clause_free(c2);
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
    // TODO: find empty clause
    drup_clause c = find_clause(lits, nb_lits);
    if (c) return drup_clause_get_id(c);
    return 0; // clause not found
}
drup_clause find_clause(const int* lits, int nb_lits) {
    for (int i = 0; i < nb_lits; i++) {
        int lit = lits[i];
        int idx = get_idx(lit);
        struct drup_clause_vec* v = occurences[idx];

        for (u64 j = 0; j < v->size; j++) {
            drup_clause c = v->data[j];
            if (drup_clause_get_nb_lits(c) != nb_lits)
                continue;
            if (compare_lits(lits, drup_clause_get_lits(c), nb_lits))
                return c;   // clause found
        }
    }
    return NULL; // clause not found
}

#ifdef UNIT_TEST
struct drup_clause_vec** get_occurences() { return occurences; }
#endif