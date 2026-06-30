
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

#define TYPE unsigned
#define TYPED(THING) unsigned_##THING
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

// Clasue DB
struct clause_db {
    unsigned* lits;
    unsigned lits_size;
    unsigned lits_capacity;
    unsigned earliest_delete;
    unsigned delete_count;
} db;

// TODO: check for duplicate lits somewhere?
int nb_known_vars;
bool unsat_found = false;
bool formula_loaded = false;
bool propagate_units = true;

// occurence list for 2 watched literals
// units do not need to be in occournce lists, since we assign them permanently
struct watcher_vec* occurences;

// buffer for clause addition
struct int_vec* add_buffer;

// numer of original clauses added via load function
u64 original_ids = 0;

// current variable assignment
char* assignment;

// stack of currently assigned variables
struct unsigned_vec* trail;

// stack of variables to be propagated
struct unsigned_vec* prop_stack;

// stack of permanent unit clauses and their literals
struct unsigned_vec* units;

#define ABS(X) (X < 0 ? -X : X)
#define NEG(LIT) (LIT ^ 1)
#define INTERNALIZE_LITS(ILITS, LITS, SIZE) \
    unsigned ILITS[SIZE];   \
    for (int i = 0; i < SIZE; i++)   \
        ILITS[i] = get_ilit(LITS[i]);
#define OVERWRITE_WITH_ILITS(ILITS, LITS, SIZE) \
    for (int i = 0; i < SIZE; i++)   \
        ILITS[i] = get_ilit(LITS[i]);
#define DEFINE_ILITS INTERNALIZE_LITS(ilits, lits, nb_lits)

static inline unsigned get_ilit(int elit) {
    assert(elit != 0);
    assert(ABS(elit) <= nb_known_vars);
    int sign = elit > 0;
    int ulit = sign ? elit : -elit;
    unsigned ilit = (ulit * 2) - sign - 1;
    assert(ilit != -1U);
    return ilit;
}
static inline int get_elit(unsigned ilit) {
    assert(ilit != -1U);
    int sign = 1 - (ilit & 1U);
    int ulit = (ilit + 1 + sign) / 2;
    return sign ? ulit : -ulit;
}
static void compress_db() {
    unsigned i = db.earliest_delete - 1;
    unsigned new_size = db.earliest_delete;
    do {
        // skip unused spaces
        while (db.lits[++i] == -1U);
        if (i >= db.lits_size) break;
        
        // correct watches for next clause
        unsigned first_watch = db.lits[i];
        unsigned second_watch = db.lits[i+1];
        assert(ABS(get_elit(first_watch)) <= nb_known_vars);
        assert(ABS(get_elit(second_watch)) <= nb_known_vars);
        struct watcher_vec* v1 = &(occurences[first_watch]);
        struct watcher_vec* v2 = &(occurences[second_watch]);
        for (u64 j = 0; j < v1->size; j++)
            if((v1->data[j].nb_lits > 2) & (v1->data[j].c.ptr == i)) v1->data[j].c.ptr = new_size;
        for (u64 j = 0; j < v2->size; j++)
            if((v2->data[j].nb_lits > 2) & (v2->data[j].c.ptr == i)) v2->data[j].c.ptr = new_size;
        
        // copy clause into new space
        while (db.lits[i] != -1U)
            db.lits[new_size++] = db.lits[i++];
        db.lits[new_size++] = -1U;
    } while (i < db.lits_size);
    db.lits_size = new_size;
    db.earliest_delete = db.lits_capacity;
    db.delete_count = 0;
}
static inline void add_clause_to_db(const unsigned* lits, int nb_lits) {
    assert(db.lits_size + nb_lits > db.lits_size);    // check for uint overflow
    if (db.lits_size + nb_lits >= db.lits_capacity)   // compress DB if necessary
        compress_db();
    if (db.lits_size + nb_lits >= db.lits_capacity) {  // resize DB if necessary
        LOG("resize clause db");
        int new_cap = MIN((unsigned)-1, db.lits_size * 1.3);
        db.lits = palrup_utils_realloc(db.lits, new_cap * sizeof(unsigned));
        db.lits_capacity = new_cap;
    }

    memcpy(db.lits + db.lits_size, lits, nb_lits * sizeof(unsigned));
    db.lits_size += nb_lits;
    db.lits[db.lits_size++] = -1U;    // clasue separator
    assert(db.lits_size <= db.lits_capacity);
}
static inline void delete_clasue_from_db(const unsigned offset, unsigned nb_lits) {
    assert(db.lits_size >= offset + nb_lits);
    assert(offset < offset + nb_lits);
    memset(db.lits + offset, -1U, nb_lits * sizeof(unsigned));    // mark unused space with -1U
    db.earliest_delete = MIN(db.earliest_delete, offset);
    if (++db.delete_count >= 10000) // TODO: tune parameter and make option?
        compress_db();
}
static void drup_check_reset_assignment() {
    // reset assignments
    for (u64 i = 0; i < trail->size; i++) {
        assignment[trail->data[i]] = (char)0;
        assignment[NEG(trail->data[i])] = (char)0;
    }
    // clear trail
    unsigned_vec_resize(trail, 0);
    #ifndef NDEBUG
        // assert no assignments persist
        for (int i = 0; i < nb_known_vars * 2; i++)
            //assert(assignment[i] == 0);
            if (assignment[i] != 0){};
                //printf(">> assignment[%i] != 0\n", i);
    #endif
}
static bool compare_lits(const unsigned* lits1, const unsigned* lits2, int nb_lits) {
    // lits are unsorted => quadratic
    for (int i = 0; i < nb_lits; i++) {
        unsigned lit = lits1[i];
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

int drup_check_propagate() {
    while (prop_stack->size > 0) {
        unsigned lit = prop_stack->data[--(prop_stack->size)];
        assert(ABS(get_elit(lit)) <= nb_known_vars);
        char a_sign = assignment[lit];

        switch (a_sign) {
            case -1: return 1;  // conflict found
            case 1 : continue;  // already assigned to same value and propagated
            default: break;
        }

        // assign value
        assert(a_sign == 0);
        assignment[lit] = 1;
        assignment[NEG(lit)] = -1;
        unsigned_vec_push(trail, lit);
        assert(assignment[lit] == 1 && assignment[NEG(lit)] == -1);

        // fix occurences and propagate
        unsigned first_watch = NEG(lit);
        struct watcher_vec * const v = &(occurences[first_watch]);
        for (u64 i = 0; i < v->size; i++) {
            __builtin_prefetch(db.lits + (v->data[i + 16]).c.ptr);
            watcher w = v->data[i];
            int nb_lits = w.nb_lits;
            if (nb_lits == 1) return 1;   // conflict found
            assert(nb_lits > 1);

            if (assignment[w.blocking_lit] == 1)
                continue;   // clause is satisfied

            unsigned second_watch;
            unsigned* lits = db.lits + w.c.ptr;
            if (nb_lits == 2)   // binary clauses are stored in watch directly
                second_watch = w.blocking_lit == first_watch ? w.c.lit : w.blocking_lit;
            else {
                second_watch = lits[0] == first_watch ? lits[1] : lits[0];
            }
            assert(first_watch == NEG(lit));
            assert(first_watch != second_watch);
            assert(assignment[first_watch] == -1);

            // check second_watch
            switch (assignment[second_watch]) {
                case 1: continue;
                case -1:
                    // second watch should be last unassigned lit in any clause
                    #ifndef NDEBUG
                        // assert that all lits in clause are negatively assigned
                        for (int j = 2; j < nb_lits; j++)
                            assert(assignment[lits[j]] == -1);
                    #endif
                    return 1;   // conflict found
                default: break;
            }
            assert(assignment[second_watch] == 0);

            // find first unassigned lit in c
            for (int j = 2; j < nb_lits; j++) {
                unsigned c_lit = lits[j];
                if (assignment[c_lit] == -1)
                    continue;

                // found unassigned or satisfied lit besides second_watch
                //  => swap with first_watch and fix occourence lists
                lits[0] = c_lit;
                lits[1] = second_watch;
                lits[j] = first_watch;

                watcher_vec_push(&(occurences[c_lit]), w);
                v->data[i--] = v->data[--(v->size)];

                goto no_recurse;
            }
        
            unsigned_vec_push(prop_stack, second_watch);
            
            no_recurse:;
        
        }   // for all occurences
    }   // while stack not empy

    return 0;   // no conflict found
}
int drup_check_propagate_unit(unsigned unit) {
    unsigned_vec_resize(prop_stack, 1);
    prop_stack->data[0] = unit;
    int conflicts_found = 0;
    
    while (prop_stack->size > 0) {
        unsigned lit = prop_stack->data[--(prop_stack->size)];
        assert(ABS(get_elit(lit)) <= nb_known_vars);
        char a_sign = assignment[lit];

        switch (a_sign) {
            case -1: return ++conflicts_found;  // conflict found
            case 1 : continue;  // already assigned to same value and propagated
            default: break;
        }

        // assign value
        assert(a_sign == 0);
        assignment[lit] = 1;
        assignment[NEG(lit)] = -1;
        assert(assignment[lit] == 1 && assignment[NEG(lit)] == -1);

        // fix occurences and propagate
        unsigned first_watch = NEG(lit);
        struct watcher_vec * const v = &(occurences[first_watch]);
        for (u64 i = 0; i < v->size; i++) {
            __builtin_prefetch(db.lits + (v->data[i + 16]).c.ptr);
            watcher w = v->data[i];
            int nb_lits = w.nb_lits;
            if (nb_lits == 1) {
                return ++conflicts_found;   // conflict found
            }
            assert(nb_lits > 1);

            if (assignment[w.blocking_lit] == 1)
                continue;   // clause is satisfied

            unsigned second_watch;
            unsigned* lits = db.lits + w.c.ptr;
            if (nb_lits == 2)   // binary clauses are stored in watch directly
                second_watch = w.blocking_lit == first_watch ? w.c.lit : w.blocking_lit;
            else {
                second_watch = lits[0] == first_watch ? lits[1] : lits[0];
            }
            assert(first_watch == NEG(lit));
            assert(first_watch != second_watch);
            assert(assignment[first_watch] == -1);

            // check second_watch
            switch (assignment[second_watch]) {
                case 1: continue;   // clasue is satisfied
                case -1:
                    // second watch should be last unassigned lit in any clause
                    #ifndef NDEBUG
                        // assert that all lits in clause are negatively assigned
                        for (int j = 2; j < nb_lits; j++)
                            assert(assignment[lits[j]] == -1);
                    #endif
                    return ++conflicts_found;   // conflict found
                default: break;
            }
            assert(assignment[second_watch] == 0);

            // find first unassigned lit in c
            for (int j = 2; j < nb_lits; j++) {
                unsigned c_lit = lits[j];
                if (assignment[c_lit] == -1)
                    continue;

                // found unassigned or satisfied lit besides second_watch
                //  => swap with first_watch and fix occourence lists
                lits[0] = c_lit;
                lits[1] = second_watch;
                lits[j] = first_watch;

                watcher_vec_push(&(occurences[c_lit]), w);
                v->data[i--] = v->data[--(v->size)];

                goto no_recurse;
            }
        
            unsigned_vec_push(prop_stack, second_watch);
            unsigned_vec_push(units, second_watch);
            
            no_recurse:;
        
        }   // for all occurences
    }   // while stack not empy

    return conflicts_found;   // no conflict found
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

    unsigned capazity = 1 << 31;
    db.lits = palrup_utils_malloc(capazity * sizeof(int));
    db.lits_size = 0;
    db.lits_capacity = capazity;
    db.earliest_delete = capazity;
    db.delete_count = 0;

    add_buffer = int_vec_init(16);
    assignment = palrup_utils_calloc(nb_lits, sizeof(u8));
    trail = unsigned_vec_init(16);
    prop_stack = unsigned_vec_init(16);
    units = unsigned_vec_init(16);
}
void drup_check_end() {
    formula_loaded = true;
    for (int i = 0; i < (nb_known_vars * 2); i++) {
        struct watcher_vec * const v = &(occurences[i]);
        free(v->data);
    }
    free(occurences);
    int_vec_free(add_buffer);
    free(assignment);
    unsigned_vec_free(trail);
    unsigned_vec_free(prop_stack);
    unsigned_vec_free(units);
    free(db.lits);

    // reset gloabl variables
    nb_known_vars = 0;
    unsat_found = false;
    formula_loaded = false;
    original_ids = 0;
    memset(&db, 0, sizeof(struct clause_db));
}

int drup_check_load(int lit) {
    if (formula_loaded)
        return 1;

    if (lit == 0) {
        drup_check_add_axiomatic_clause(++original_ids, add_buffer->data, add_buffer->size, false);
                
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

int drup_check_add_axiomatic_clause(u64 id, const int* lits, int nb_lits, bool internal_lits) {
    assert(nb_lits >= 0);
    (void)id;
    
    // mark unsat as found
    if (nb_lits == 0) {
        unsat_found = true;
        return 0;
    }

    // transform elits into ilits if necessary
    unsigned ilits[nb_lits];
    if (internal_lits)
        memcpy(ilits, lits, nb_lits * sizeof(unsigned));    // TODO: not copy?
    else 
        OVERWRITE_WITH_ILITS(ilits, lits, nb_lits);

    // TODO: meassure if this is actually worth it
    unsigned actual_size = nb_lits;
    unsigned tmp;
    for (unsigned i = 0; i < actual_size; i++)
        switch (assignment[ilits[i]]) {
            case -1:
                // sort to the back
                tmp = ilits[i];
                ilits[i--] = ilits[--actual_size];
                ilits[actual_size] = tmp;
                break;
            case 1: return 0;
            default: break;
        }

    // handle unit as special case
    if ((nb_lits == 1) | (actual_size == 1)) {
        unsigned lit = ilits[0];
        unsigned_vec_push(units, lit);
        watcher w = { .nb_lits = 1, .blocking_lit = lit, .c.ptr = -1 };
        if (propagate_units) {
            if (drup_check_propagate_unit(lit)) {
                // units are conflicting already
                //  => can not keep them assigned any longer
                LOG("Conflict found for clause %lu", id);
                for (u64 i = 0; i < units->size; i++) {
                    assignment[units->data[i]] = 0;
                    assignment[NEG(units->data[i])] = 0;
                }
                #ifndef NDEBUG
                    for (int i = 0; i < nb_known_vars * 2; i++)
                        assert(assignment[i] == 0);
                #endif
                propagate_units = false;
            }
        }
        watcher_vec_push(&(occurences[lit]), w);
        return 0;
    }

    // handle binary as special case
    if (nb_lits == 2) {
        watcher w = { .nb_lits = 2, .blocking_lit = ilits[0], .c.lit = ilits[1] };
        watcher_vec_push(&(occurences[ilits[0]]), w);
        watcher_vec_push(&(occurences[ilits[1]]), w);
        return 0;
    }

    // add clause to occurence list of first two lits
    assert(nb_lits > 2);
    watcher w = { .nb_lits = nb_lits, .blocking_lit = ilits[nb_lits - 1], .c.ptr = db.lits_size };
    watcher_vec_push(&(occurences[ilits[0]]), w);
    watcher_vec_push(&(occurences[ilits[1]]), w);
    add_clause_to_db(ilits, nb_lits);

    return 0;
}
int drup_check_add_clause(u64 id, const int* lits, int nb_lits) {
    if (!formula_loaded) {
        snprintf(palrup_utils_msgstr, MSG_LEN, "Can not check clause %lu before formula is fully loaded.", id);
        palrup_utils_log_err(palrup_utils_msgstr);
        return 1;
    }
    
    DEFINE_ILITS;

    int res = 1;
    unsigned_vec_resize(prop_stack, units->size);
    memcpy(prop_stack->data, units->data, units->size * sizeof(int));
    for (int i = 0; i < nb_lits; i++)
        unsigned_vec_push(prop_stack, NEG(ilits[i]));
    if (drup_check_propagate()) {   // conflict found
        drup_check_reset_assignment();
        res = drup_check_add_axiomatic_clause(id, (int*)ilits, nb_lits, true);
    } else drup_check_reset_assignment();

    return res;
}
int drup_check_delete_clause(const int* lits, int nb_lits) {
    if (!formula_loaded) {
        snprintf(palrup_utils_msgstr, MSG_LEN, "Can not delete clause before formula is fully loaded.");
        palrup_utils_log_err(palrup_utils_msgstr);
        return 1;
    }

    DEFINE_ILITS;

    // find first clause occurences and delete it 
     for (int i = 0; i < nb_lits; i++) {
        unsigned lit = ilits[i];
        struct watcher_vec * const v = &(occurences[lit]);

        for (u64 j = 0; j < v->size; j++) {
            watcher w = v->data[j];
            if (w.nb_lits != nb_lits)
                continue;
            // lits are stored inside watch if nb_lits < 3
            if (nb_lits > 2 ? compare_lits(ilits, db.lits + w.c.ptr, nb_lits) : compare_lits(ilits, &(w.blocking_lit), nb_lits)) {
                // remove clause from occurence lists and delete
                v->data[j] = v->data[--(v->size)];
                if (nb_lits == 1)
                    return 0;   // only occurence was deleted

                if (nb_lits == 2)
                    break;  // second watch will be found and deleted by second iteration

                // Skip some iterations of outer loop since we know the second occurence list.
                unsigned* c = db.lits + w.c.ptr;
                unsigned second_watch = c[0] == lit ? c[1] : c[0];
                struct watcher_vec * const v2 = &(occurences[second_watch]);
                for (size_t k = 0; k < v2->size; k++) {
                    watcher w2 = v2->data[k];
                    if (w2.nb_lits != w.nb_lits || w2.c.ptr != w.c.ptr) continue;
                    // watch points to same clause => delete watch
                    v2->data[k] = v2->data[--(v2->size)];
                    delete_clasue_from_db(w.c.ptr, w.nb_lits);
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
    //    int idx = get_ilit(lit);
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
struct watcher_vec* get_occurences() { return occurences; }
#endif