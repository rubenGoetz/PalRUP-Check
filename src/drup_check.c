
#include <assert.h>
#include <stdlib.h>

#include "drup_check.h"
#include "drup_clause.h"
#include "utils/palrup_utils.h"
#include "hash.h"

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

u64 nb_known_vars;
bool unsat_found = false;
bool formula_loaded = false;

// full occurence list for literals
// TODO: make 2 watch
struct drup_clause_vec** occurences;

// buffer for clause addition
struct int_vec* add_buffer;

// numer of original clasues added via load function
u64 original_ids = 0;

// stores clauses by id
struct hash_table* clause_table;

// current variable assignment
u8* assignment;

// stack of currently assigned variables
struct int_vec* trail;

// stack of permanent unit clauses
struct int_vec* units;

static inline int get_idx(int lit) {
    assert(lit != 0);
    int sign = lit > 0;
    int ulit = sign ? lit : -lit;
    assert((ulit * 2) - sign <= (int)(2*nb_known_vars) + 1);
    return (ulit * 2) - sign;
}
static inline int get_var(int lit) {
    return lit > 0 ? lit : -lit;
}
static int drup_check_unit_propagation(int lit) {
    assert(lit > 0 ? (u64)lit <= 2*nb_known_vars : (u64)lit >= -2*nb_known_vars);

    int var = get_var(lit);
    u8 sign = assignment[var];
    
    // if var was already assigned a different value
    if (sign != 0 && (sign != (u8)(lit > 0 ? 1 : -1)))
        return 1;   // conflict found

    // assign value
    assignment[var] = lit > 0 ? 1 : -1;
    int_vec_push(trail, lit);

    // propagate
    struct drup_clause_vec* v = occurences[get_idx(-lit)];
    for (int i = 0; (u64)i < v->size; i++) {
        drup_clause c = v->data[i];
        assert(hash_table_find(clause_table, drup_clause_get_id(c)));
        int unassigned_vars = 0;
        int unsat_lits = 0;

        // is unit?
        int nb_lits = drup_clause_get_nb_lits(c);
        int* lits = drup_clause_get_lits(c);
        int lit;
        for (int j = 0; j < nb_lits; j++) {
            lit = lits[j];
            int var = get_var(lit);

            // lit is unassigned
            if (assignment[var] == 0)
                unassigned_vars++;
            // lit satisfies clasue
            else if (assignment[var] == (u8)(lit > 0 ? 1 : -1)) {
                unassigned_vars = 0;
                break;
            // lit does not satisfy clause
            } else
                unsat_lits++;

            if (unassigned_vars > 1)
                break;
        }

        if (unsat_lits == nb_lits)
            // clause is not satisfied with current assignment
            //  => conflict found
            return 1;

        if (unassigned_vars != 1)
            continue;   // clause does not become unit

        // clause becomes unit => propagate
        if (drup_check_unit_propagation(lit)) {
            return 1;   // conflict found
        }
    }

    return 0;   // no conflict found
}
static void drup_check_reset_assignment() {
    // reset assignments
    for (u64 i = 0; i < trail->size; i++)
        assignment[get_var(trail->data[i])] = 0;
    // clear trail
    int_vec_clear(trail);
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
    occurences = (struct drup_clause_vec**)palrup_utils_malloc((1 + nb_lits) * sizeof(struct drup_clause_vec*));
    for (int i = 0; i < nb_lits + 1; i++)
        occurences[i] = drup_clause_vec_init(4);

    add_buffer = int_vec_init(16);
    clause_table = hash_table_init(16);
    assignment = palrup_utils_calloc(nb_vars + 1, sizeof(u8));
    trail = int_vec_init(16);
    units = int_vec_init(16);
}
void drup_check_end() {
    for (size_t i = 0; i < (nb_known_vars * 2) + 1; i++) {
        drup_clause_vec_free(occurences[i]);
    }
    free(occurences);
    int_vec_free(add_buffer);
    original_ids = 0;
    hash_table_free(clause_table);
    free(assignment);
    int_vec_free(trail);
    int_vec_free(units);
}

int drup_check_load(int lit) {
    if (formula_loaded)
        return 1;

    if (lit == 0) {
        u64 id = ++original_ids;
        int* lits = add_buffer->data;
        int nb_lits = add_buffer->size;
        drup_clause c = drup_clause_init(id, lits, nb_lits);
        // add clause to clause data base
        if(!hash_table_insert(clause_table, id, c))
            return 1;
        // add clause to occurence lists
        for (int i = 0; i < nb_lits; i++) {
            int idx = get_idx(lits[i]);
            drup_clause_vec_push(occurences[idx], c);
        }
        
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
    int res = 0;
    // count clause as part of formula if formula is not fully loaded yet
    if (!formula_loaded) {
        if (id != ++original_ids) {
            snprintf(palrup_utils_msgstr, MSG_LEN, "Original clauses should be loaded with incremental ids.");
            palrup_utils_log_warn(palrup_utils_msgstr);
            res = 1;
        }
    }
    
    // store clause
    drup_clause c = drup_clause_init(id, lits, nb_lits);
    if (!hash_table_insert(clause_table, id, c)) {
        snprintf(palrup_utils_msgstr, MSG_LEN, "Could not load clause %lu into checker.", id);
        palrup_utils_log_err(palrup_utils_msgstr);
        return 1;
    }
    
    // add clause to occurence lists
    for (int i = 0; i < nb_lits; i++) {
        int idx = get_idx(lits[i]);        
        drup_clause_vec_push(occurences[idx], c);
    }

    // remember units
    if (nb_lits == 1) {
        int lit = lits[0];
        int_vec_push(units, lit);
        assignment[get_var(lit)] = lit > 0 ? 1 : -1;
    }

    // mark unsat as found
    if (nb_lits == 0)
        unsat_found = true;

    return res;
}
int drup_check_add_clause(u64 id, const int* lits, int nb_lits) {
    if (!formula_loaded) {
        snprintf(palrup_utils_msgstr, MSG_LEN, "Can not check clause %lu before formula is fully loaded.", id);
        palrup_utils_log_err(palrup_utils_msgstr);
        return 1;
    }
    
    int res = 1;
    for (int i = 0; i < nb_lits; i++) {
        if (drup_check_unit_propagation(-lits[i])) {   // conflict found
            drup_check_reset_assignment();
            drup_check_add_axiomatic_clause(id, lits, nb_lits);
            res = 0;
            break;
        }
    }
    
    if (nb_lits == 0) {
        // propagate known units to check empty clause
        for (size_t i = 0; i < units->size; i++) {
            if (drup_check_unit_propagation(units->data[i])) {   // conflict found
                drup_check_add_axiomatic_clause(id, lits, nb_lits);
                res = 0;
                break;
            }
        }
    }

    return res;
}
int drup_check_delete_clause(const int* lits, int nb_lits) {
    if (!formula_loaded) {
        snprintf(palrup_utils_msgstr, MSG_LEN, "Can not delete clause before formula is fully loaded.");
        palrup_utils_log_err(palrup_utils_msgstr);
        return 1;
    }

    // find id
    u64 id = drup_check_get_clause_id(lits, nb_lits);
    if (!id) {
        snprintf(palrup_utils_msgstr, MSG_LEN, "Could not find clause to delete.");
        palrup_utils_log_err(palrup_utils_msgstr);
        return 1;
    }

    // delete clause from db
    drup_clause c = hash_table_find(clause_table, id);
    if (!c) {
        snprintf(palrup_utils_msgstr, MSG_LEN, "Clause %lu not in clause table.", drup_clause_get_id(c));
        palrup_utils_log_err(palrup_utils_msgstr);
        return 1;
    }
    if (!hash_table_delete_last_found(clause_table)) {
        snprintf(palrup_utils_msgstr, MSG_LEN, "Could not delete clause %lu from clause table.", drup_clause_get_id(c));
        palrup_utils_log_err(palrup_utils_msgstr);
        return 1;
    };

    // delete c from all its lits' occourence lists
    for (int j = 0; j < nb_lits; j++) {
        int idx = get_idx(lits[j]);
        struct drup_clause_vec* v = occurences[idx];
        for (u64 k = 0; k < v->size; k++) {
            if (v->data[k] == c) {
                v->data[k] = v->data[--(v->size)];
                break;
            }
        }
    }

    // delete c from memory
    drup_clause_free(c);

    return 0;
}
int drup_check_delete_clauses(const u64* ids, int nb_ids) {
    if (!formula_loaded) {
        snprintf(palrup_utils_msgstr, MSG_LEN, "Can not delete clauses before formula is fully loaded.");
        palrup_utils_log_err(palrup_utils_msgstr);
        return 1;
    }
    
    int res = 0;
    for (int i = 0; i < nb_ids; i++) {
        u64 id = ids[i];
        drup_clause c = hash_table_find(clause_table, id);
        if (!c) {
            res++;
            continue;
        }
        int* lits = drup_clause_get_lits(c);
        // delete c from all its lits' occourence lists
        for (int j = 0; j < drup_clause_get_nb_lits(c); j++) {
            int idx = get_idx(lits[j]);
            struct drup_clause_vec* v = occurences[idx];
            for (u64 k = 0; k < v->size; k++) {
                if (v->data[k] == c) {
                    v->data[k] = v->data[--(v->size)];
                    break;
                }
            }
        }

        // delete c from known clauses
        if (!hash_table_delete_last_found(clause_table)) {
            snprintf(palrup_utils_msgstr, MSG_LEN, "Was not able to delete clause %lu from checker.", id);
            palrup_utils_log_warn(palrup_utils_msgstr);
            res++;
        }

        // delete clause from memory
        drup_clause_free(c);
    }
    return res;     // return number of mishaps
}

bool drup_check_unsat_found() {
    return unsat_found;
}
bool drup_check_formula_loaded() {
    return formula_loaded;
}
u64 drup_check_get_clause_id(const int* lits, int nb_lits) {
    for (int i = 0; i < nb_lits; i++) {
        int lit = lits[i];
        int idx = get_idx(lit);
        struct drup_clause_vec* v = occurences[idx];

        for (u64 j = 0; j < v->size; j++) {
            drup_clause c = v->data[j];
            if (drup_clause_get_nb_lits(c) != nb_lits)
                continue;
            if (compare_lits(lits, drup_clause_get_lits(c), nb_lits))
                return drup_clause_get_id(c);   // clause found
        }
    }
    
    return 0; // clause not found
}

#ifdef UNIT_TEST
struct hash_table* get_clause_db() { return clause_table; }
#endif