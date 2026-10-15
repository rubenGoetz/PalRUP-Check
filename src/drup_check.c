
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "drup_check.h"
#include "utils/palrup_utils.h"
#include "utils/checker_utils.h"

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

// ----- Definitions for DRUP to LRUP conversion -----
#define EMPTY_HINTS
#define PUSH_HINT(H)
#define UPDATE_HINT_PREFIX
#define PROPAGATE_UNIT(LIT) (void)LIT;

#ifdef DRUP_TO_LRUP_CONVERSION

#define TYPE u64
#define TYPED(THING) u64_##THING
#include "vec.h"
#undef TYPED
#undef TYPE

#include "hashset.h"
#include "hash.h"

struct hash_set* marked_hints;
struct hash_set* clause_lits;
struct hash_table* derived_hints;
u64 hint_prefix = 0;
struct watcher_vec* detailed_hints;
struct u64_vec* hints;
struct u64_vec* hints_tail;
struct u64_vec* deletions;
struct unsigned_vec* dfs_stack;
u64* unit_ids;
watcher* derived_unit_hints;

#undef EMPTY_HINTS
#define EMPTY_HINTS watcher_vec_resize(detailed_hints, hint_prefix)

#undef PUSH_HINT
#define PUSH_HINT(H) (watcher_vec_push(detailed_hints, H))

#endif
// ---------------------------------------------------

#define ON_STACK 2
#define ON_UNITS 3
#define POS_VAL 1
#define NEG_VAL -1
#define NEUTRAL_VAL 0

// Clasue DB
struct clause_db {
    unsigned* lits;
    unsigned lits_size;
    unsigned lits_capacity;
    unsigned earliest_delete;
    unsigned delete_count;
} db, hint_db;

int nb_known_vars;
int unsat_found = 0;    // 1 for regular unsat found, 2 for early conflict
bool formula_loaded = false;
bool propagate_units = true;
u64 drup_check_empty_id = 0;

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
u64 prop_stack_propagated;

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
static void compress_db(struct clause_db* db) {
    if (db == &hint_db)
        return;
    LOG("Compress drup check data base");
    unsigned i = db->earliest_delete - 1;
    unsigned new_size = db->earliest_delete;
    do {
        // skip unused spaces
        // Code analysis might complain about jump depending on uninitialized value.
        // compress_db however will never be called before any deletions occoured
        // and thus i < db.lits_size, i.e. db.lits[i] is initialized.
        while (db->lits[++i] == -1U);
        if (i >= db->lits_size) break;
        
        // correct watches for next clause
        unsigned first_watch = db->lits[i];
        unsigned second_watch = db->lits[i+1];
        assert(ABS(get_elit(first_watch)) <= nb_known_vars);
        assert(ABS(get_elit(second_watch)) <= nb_known_vars);
        struct watcher_vec* v1 = &(occurences[first_watch]);
        struct watcher_vec* v2 = &(occurences[second_watch]);
        for (u64 j = 0; j < v1->size; j++)
            if((v1->data[j].nb_lits > 2) & (v1->data[j].c.ptr == i)) v1->data[j].c.ptr = new_size;
        for (u64 j = 0; j < v2->size; j++)
            if((v2->data[j].nb_lits > 2) & (v2->data[j].c.ptr == i)) v2->data[j].c.ptr = new_size;
        
        // copy clause into new space
        while (db->lits[i] != -1U)
            db->lits[new_size++] = db->lits[i++];
        db->lits[new_size++] = -1U;
    } while (i < db->lits_size);
    db->lits_size = new_size;
    db->earliest_delete = db->lits_capacity;
    db->delete_count = 0;
}
static inline unsigned add_clause_to_db(struct clause_db* db, const unsigned* lits, int nb_lits) {
    assert(db->lits_size + nb_lits > db->lits_size);    // check for uint overflow
    if (db->lits_size + nb_lits >= db->lits_capacity)   // compress DB if necessary
        compress_db(db);
    if (db->lits_size + nb_lits >= db->lits_capacity) {  // resize DB if necessary
        int new_cap = MIN((unsigned)-1, db->lits_size * 1.3);
        LOG("[GROW] data base from %u to %lu bytes.", db->lits_capacity, new_cap * sizeof(unsigned));
        db->lits = palrup_utils_realloc(db->lits, new_cap * sizeof(unsigned));
        db->lits_capacity = new_cap;
    }

    unsigned offset = db->lits_size;
    memcpy(db->lits + db->lits_size, lits, nb_lits * sizeof(unsigned));
    db->lits_size += nb_lits;
    db->lits[db->lits_size++] = -1U;    // clasue separator
    assert(db->lits_size <= db->lits_capacity);
    return offset;
}
static inline void delete_clause_from_db(struct clause_db* db, const unsigned offset, unsigned nb_lits) {
    assert(db->lits_size >= offset + nb_lits);
    assert(offset < offset + nb_lits);
    memset(db->lits + offset, -1U, nb_lits * sizeof(unsigned));    // mark unused space with -1U
    db->earliest_delete = MIN(db->earliest_delete, offset);
    // TODO: try some "empty factor" instead of fixed number of deletions
    if (++db->delete_count >= 10000) // TODO: tune parameter and make option?
        compress_db(db);
}
static void drup_check_reset_assignment() {
    // reset assignments
    for (unsigned *lit = trail->data, *end = trail->data + trail->size; lit < end; lit++) {
        assignment[*lit] = NEUTRAL_VAL;
        assignment[NEG(*lit)] = NEUTRAL_VAL;
    }
    // clear trail
    unsigned_vec_resize(trail, 0);

    #ifndef NDEBUG
        #ifndef DRUP_TO_LRUP_CONVERSION
            for (char *a = assignment, *end = assignment + (2 * nb_known_vars); a < end; a++)
                assert(*a == 0);
        #endif
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

#ifdef DRUP_TO_LRUP_CONVERSION
static void update_hint_prefix() {
    for (watcher *hint = detailed_hints->data, *end = hint + detailed_hints->size; hint < end; hint++) {
        // add clause into immutable hint_db
        if (hint->nb_lits > 2) {
            unsigned offset = add_clause_to_db(&hint_db, db.lits + hint->c.ptr, hint->nb_lits);
            hint->c.ptr = offset;
        }

        if (!derived_unit_hints[hint->hint_lit].id) {
            derived_unit_hints[hint->hint_lit] = *hint;
            derived_unit_hints[hint->hint_lit].hint_lit = 0;        // used to mark hint
            hash_table_insert(derived_hints, hint->id, (void*)1);
        }
    }
}

#undef PROPAGATE_UNIT
#define PROPAGATE_UNIT(LIT) do { \
        prop_stack_propagated = 0;  \
        unsigned_vec_resize(prop_stack, 1); \
        prop_stack->data[0] = LIT;  \
        if (drup_check_propagate()) {   \
            LOG("Early conflict found at clause %lu for lit %u", id, LIT);    \
            unsat_found = 2; \
            drup_check_empty_id = id;   \
            return 0;   \
        }   \
        unsigned_vec_resize(trail, 0);  \
        update_hint_prefix(); \
    } while (0)

static void process_hints(int nb_lits) {
    if (detailed_hints->size < 1) return;

    u64_vec_resize(hints, 0);
    u64_vec_resize(hints_tail, 0);
    unsigned_vec_resize(dfs_stack, 0);
    //struct unsigned_vec* dfs_stack = unsigned_vec_init(256);        // TODO: make static

    // mark negated clause lits as satisfied
    for (int i = 0; i < nb_lits; i++)
        hash_set_insert(marked_hints, prop_stack->data[i] + 1);
    
    // put lits from detailed hints on stack
    watcher* w = detailed_hints->data + detailed_hints->size - 1;
    if (!w->id) {
        // derived_unit_hints already contain conflict => skip detailed_hints iteration
        //unsigned_vec_push(dfs_stack, w->hint_lit);
        watcher *hint = &(derived_unit_hints[w->hint_lit]);
        u64_vec_push(hints_tail, hint->id);

        switch (hint->nb_lits) {
            case 1: break;
            case 2:
                ;
                unsigned neg_c_lit = NEG(hint->c.lit);
                unsigned neg_block = NEG(hint->blocking_lit);
                if (!hash_set_find(marked_hints, neg_c_lit + 1))
                    unsigned_vec_push(dfs_stack, neg_c_lit);
                if (!hash_set_find(marked_hints, neg_block + 1))
                    unsigned_vec_push(dfs_stack, neg_block);
                break;
            default:
                for (unsigned *lit = hint_db.lits + hint->c.ptr; *lit != -1U; lit++) {
                    unsigned neg_lit = NEG(*lit);
                    if (!hash_set_find(marked_hints, neg_lit + 1))    
                        unsigned_vec_push(dfs_stack, neg_lit);
                }
                break;
        }
        
    } else {
        assert(w);
        assert(w >= detailed_hints->data);
        assert(w < detailed_hints->data + detailed_hints->size);

        switch (w->nb_lits) {
            case 1: break;
            case 2:
                ;
                unsigned neg_c_lit = NEG(w->c.lit);
                unsigned neg_block = NEG(w->blocking_lit);
                if (!hash_set_find(marked_hints, neg_c_lit + 1)) {
                    unsigned_vec_push(dfs_stack, neg_c_lit);
                    derived_unit_hints[neg_c_lit].hint_lit = 1;
                }
                if (!hash_set_find(marked_hints, neg_block + 1)) {
                    unsigned_vec_push(dfs_stack, neg_block);
                    derived_unit_hints[neg_block].hint_lit = 1;
                }
                break;
            default:
                for (unsigned *lit = db.lits + w->c.ptr; *lit != -1U; lit++) {
                    unsigned neg_lit = NEG(*lit);
                    if (!hash_set_find(marked_hints, neg_lit + 1)) {
                        unsigned_vec_push(dfs_stack, neg_lit);
                        derived_unit_hints[neg_lit].hint_lit = 1;
                    }
                }
                break;
        }
        u64_vec_push(hints_tail, w->id);

        while (w-- > detailed_hints->data) {
            if (!derived_unit_hints[w->hint_lit].hint_lit)
                continue;
            //if (derived_unit_hints[w->hint_lit].id &&
            //    w > detailed_hints->data + derived_unit_hints[w->hint_lit])
            //    continue;

            switch (w->nb_lits) {
                case 1:     // unit clause
                    ;
                    unsigned neg_lit = NEG(w->hint_lit);
                    if (hash_set_find(marked_hints, neg_lit + 1)) break;
                    derived_unit_hints[neg_lit].hint_lit = 1;  // mark hint
                    unsigned_vec_push(dfs_stack, neg_lit);
                    break;
                case 2:     // binary clause
                    ;
                    unsigned neg_c_lit = NEG(w->c.lit);
                    unsigned neg_block = NEG(w->blocking_lit);
                    if (w->blocking_lit == w->hint_lit) {
                        if (!hash_set_find(marked_hints, neg_c_lit + 1)) {
                            derived_unit_hints[neg_c_lit].hint_lit = 1;  // mark hint
                            unsigned_vec_push(dfs_stack, neg_c_lit);
                        }
                    } else
                        if (!hash_set_find(marked_hints, neg_block + 1)) {
                            derived_unit_hints[neg_block].hint_lit = 1;  // mark hint
                            unsigned_vec_push(dfs_stack, neg_block);
                        }
                    break;
                default:
                    ;
                    for (unsigned *lit = db.lits + w->c.ptr; *lit != -1U; lit++) {
                        if (*lit == w->hint_lit) continue;
                        unsigned neg_lit = NEG(*lit);
                        if (hash_set_find(marked_hints, neg_lit + 1)) continue;
                        derived_unit_hints[neg_lit].hint_lit = 1;  // mark hint
                        unsigned_vec_push(dfs_stack, neg_lit);
                    }
                    break;
            }

            u64_vec_push(hints_tail, w->id);
            derived_unit_hints[w->hint_lit].hint_lit = 0;  // unmark hint
        }
    }

    // Begin DFS
    while (dfs_stack->size) {
        unsigned lit = dfs_stack->data[dfs_stack->size - 1];
        watcher *hint = &(derived_unit_hints[lit]);
        hint->hint_lit = 0;     // revert potential marks

        if (!hint->id) {    // skip hints from detailed_hints
            dfs_stack->size--;
            continue;
        }

        if (hash_set_find(marked_hints, lit + 1)) {
            // lit is already processed
            
            dfs_stack->size--;
            continue;
        }

        if (hint->nb_lits == 1) {
            u64_vec_push(hints, hint->id);
            hash_set_insert(marked_hints, lit + 1);

            dfs_stack->size--;
            continue;
        }

        // put children on stack
        if (hint->nb_lits == 2) {
            bool done = true;
            unsigned neg_c_lit = NEG(hint->c.lit);
            unsigned neg_block = NEG(hint->blocking_lit);
            if (hint->blocking_lit == lit) {
                if (!hash_set_find(marked_hints, neg_c_lit + 1)) {
                    unsigned_vec_push(dfs_stack, neg_c_lit);
                    done = false;
                }
            } else
                if (!hash_set_find(marked_hints, neg_block + 1)) {
                    unsigned_vec_push(dfs_stack, neg_block);
                    done = false;
                }

            if (done) {
                // all children are already in hints
                u64_vec_push(hints, hint->id);
                hash_set_insert(marked_hints, lit + 1);
                hint->hint_lit = 0;

                dfs_stack->size--;
            }
            continue;
        }

        if (hint->nb_lits > 2) {
            unsigned* c_lits = hint_db.lits + hint->c.ptr;
            bool done = true;
            for (unsigned* c_lit = c_lits; *c_lit != -1U; c_lit++) {
                if (*c_lit == lit) continue;
                unsigned neg_lit = NEG(*c_lit);
                if (!hash_set_find(marked_hints, neg_lit + 1)) {    // neg_lit is already in hints
                    unsigned_vec_push(dfs_stack, neg_lit);
                    done = false;
                }
            }

            if (done) {
                // all children are already in hints
                u64_vec_push(hints, hint->id);
                hash_set_insert(marked_hints, lit + 1);
                hint->hint_lit = 0;

                dfs_stack->size--;
            }

            continue;
        }
        dfs_stack->size--;
    }
    
    // append tail to hints
    for (u64 *hint_id = hints_tail->data + hints_tail->size - 1; hint_id >= hints_tail->data; hint_id--)
        u64_vec_push(hints, *hint_id);

    EMPTY_HINTS;
    hash_set_clear(marked_hints);
}

/*static void process_hints(int nb_lits) {
    if (detailed_hints->size < 1) return;
    // TODO: BFS over hint tree implizitly stored in derived_unit_hints:
    // Store watches directly in derived_unit_hints
    // Find a way to keep order
    // Make seperate hint_db for hint lits, to enable db compression
    // replace complex hints with unit ones if possible
    
    // Alternative: DFS -> build hints in correct order and keep hashset of already hinted lits

    // Hotfix: check if w->hint_lit \in derived_unit_hints and only add to hashset if so

    u64_vec_resize(hints, 0);
    //marked_hints = hash_set_init(16);
    //clause_lits = hash_set_init(16);
    watcher* w = detailed_hints->data + detailed_hints->size - 1;
    watcher* prefix = detailed_hints->data + hint_prefix;

    for (int i = 0; i < nb_lits; i++)
        hash_set_insert(clause_lits, prop_stack->data[i] + 1);

    if (!w->id) {   // Prefix already contains conflict.
        w = &(detailed_hints->data[derived_unit_hints[w->hint_lit]]);
        assert(w < detailed_hints->data + hint_prefix);
    }
    assert(w);
    assert(w >= detailed_hints->data);
    assert(w < detailed_hints->data + detailed_hints->size);

    // special last hint
    u64_vec_push(hints, w->id);
    switch (w->nb_lits) {
        case 1:     // unit clause
            if (!hash_set_find(clause_lits, NEG(w->hint_lit) + 1))
                hash_set_insert(marked_hints, NEG(w->hint_lit) + 1);
            break;
        case 2:     // binary clause
            if (!hash_set_find(clause_lits, NEG(w->c.lit) + 1))
                hash_set_insert(marked_hints, NEG(w->c.lit) + 1);
            if (!hash_set_find(clause_lits, NEG(w->blocking_lit) + 1))
                hash_set_insert(marked_hints, NEG(w->blocking_lit) + 1);
            break;
        default:
            ;
            unsigned* lits = w < prefix ? hint_db.lits + w->c.ptr : db.lits + w->c.ptr;
            for (int i = 0; i < w->nb_lits; i++)
                if (!hash_set_find(clause_lits, NEG(lits[i]) + 1))
                    hash_set_insert(marked_hints, NEG(lits[i]) + 1);
            break;
    }

    // filter hints
    while (w-- > detailed_hints->data) {
        if (!hash_set_find(marked_hints, w->hint_lit + 1))
            continue;
        if (derived_unit_hints[w->hint_lit] &&
            w > detailed_hints->data + derived_unit_hints[w->hint_lit])
            continue;

        u64_vec_push(hints, w->id);
        switch (w->nb_lits) {
            case 1:     // unit clause
                break;
            case 2:     // binary clause
                if (hash_set_find(clause_lits, w->c.lit + 1)) {
                    assert(derived_unit_hints[NEG(w->c.lit)]);
                    LOG("Found satisfied lit during hint processing, try restart");
                    watcher_vec_push(detailed_hints, detailed_hints->data[derived_unit_hints[NEG(w->c.lit)]]);
                }
                if (hash_set_find(clause_lits, w->blocking_lit + 1)) {
                    assert(derived_unit_hints[NEG(w->c.lit)]);
                    LOG("Found satisfied lit during hint processing, try restart");
                    watcher_vec_push(detailed_hints, detailed_hints->data[derived_unit_hints[NEG(w->blocking_lit)]]);
                }

                //if (w->c.lit != w->hint_lit) {
                    if (!hash_set_find(clause_lits, NEG(w->c.lit) + 1))
                        hash_set_insert(marked_hints, NEG(w->c.lit) + 1);
                //} else {
                    if (!hash_set_find(clause_lits, NEG(w->blocking_lit) + 1))
                        hash_set_insert(marked_hints, NEG(w->blocking_lit) + 1);
                //}
                break;
            default:    // long clause
                ;
                //unsigned* lits = hint_db.lits + w->c.ptr;
                unsigned* lits = w < prefix ? hint_db.lits + w->c.ptr : db.lits + w->c.ptr;
                for (unsigned* lit = lits; *lit != -1U; lit++)
                    if (hash_set_find(clause_lits, *lit + 1)) {
                        assert(derived_unit_hints[NEG(*lit)]);
                        LOG("Found satisfied lit during hint processing, try restart");
                        watcher_vec_push(detailed_hints, detailed_hints->data[derived_unit_hints[NEG(*lit)]]);
                    }

                for (int i = 0; i < w->nb_lits; i++)
                    //if (lits[i] != w->hint_lit)
                        if (!hash_set_find(clause_lits, NEG(lits[i]) + 1))
                            hash_set_insert(marked_hints, NEG(lits[i]) + 1);
                break;
        }
        
        hash_set_delete_last_found(marked_hints);    // remove mark
    }

    EMPTY_HINTS;
    hash_set_clear(marked_hints);
    hash_set_clear(clause_lits);
    //hash_set_free(marked_hints);
    //hash_set_free(clause_lits);
}*/
#endif

int drup_check_propagate() {
    #ifndef NDEBUG
    u64 initial_prop_stack_size = prop_stack->size;
    #endif

    while (prop_stack_propagated < prop_stack->size) {
        unsigned lit = prop_stack->data[prop_stack_propagated++];
        assert(ABS(get_elit(lit)) <= nb_known_vars);
        char a_sign = assignment[lit];

        // assign value
        switch (a_sign) {
            case NEG_VAL:
                assert(prop_stack_propagated <= initial_prop_stack_size);
                #ifdef DRUP_TO_LRUP_CONVERSION
                ;
                unsigned neg_lit = NEG(lit);
                watcher h = { .nb_lits = 1,
                              .blocking_lit = neg_lit,
                              .c.ptr = -1,
                              .hint_lit = neg_lit,
                              .id = 0
                            };
                PUSH_HINT(h);
                #endif
                return 1;
            case POS_VAL: continue;
            default: break;
        }
        assignment[lit] = POS_VAL;
        assignment[NEG(lit)] = NEG_VAL;
        unsigned_vec_push(trail, lit);
        assert(assignment[lit] == POS_VAL && assignment[NEG(lit)] == NEG_VAL);

        // fix occurences and propagate
        unsigned first_watch = NEG(lit);
        struct watcher_vec * const v = &(occurences[first_watch]);
        watcher* w = v->data;
        watcher* end = v->data + v->size;
        while (w != end) {
            int nb_lits = w->nb_lits;
            assert(nb_lits > 1);    // there are no unit watches

            if (assignment[w->blocking_lit] == POS_VAL) {
                w++;
                continue;   // clause is satisfied
            }

            unsigned second_watch;
            unsigned* lits = db.lits + w->c.ptr;
            if (nb_lits == 2)   // binary clauses are stored in watch directly
                second_watch = w->blocking_lit == first_watch ? w->c.lit : w->blocking_lit;
            else
                second_watch = lits[0] == first_watch ? lits[1] : lits[0];
            assert(first_watch == NEG(lit));
            assert(first_watch != second_watch);
            assert(assignment[first_watch] == NEG_VAL);
            assert(ABS(get_elit(second_watch)) <= nb_known_vars);

            // check second_watch
            switch (assignment[second_watch]) {
                case POS_VAL: w++; continue;
                case NEG_VAL:
                    #ifndef NDEBUG
                        // Second watch should be last unassigned lit in any clause.
                        // Assert that all lits in clause are negatively assigned.
                        for (int j = 2; j < nb_lits; j++)
                            assert(assignment[lits[j]] == NEG_VAL);
                    #endif
                    #ifdef DRUP_TO_LRUP_CONVERSION
                    w->hint_lit = second_watch;
                    PUSH_HINT(*w);
                    #endif
                    return 1;   // conflict found
                default: break;
            }
            assert(assignment[second_watch] != POS_VAL && assignment[second_watch] != NEG_VAL);

            // find first unassigned lit in c
            for (unsigned* c_lit = lits + 2; c_lit != lits + nb_lits; c_lit++) {
                assert(*c_lit != first_watch);
                assert(*c_lit != second_watch);
                if (assignment[*c_lit] == NEG_VAL)
                    continue;

                // found unassigned or satisfied lit besides second_watch
                //  => swap with first_watch and fix occourence lists
                *lits = *c_lit;
                *(lits + 1) = second_watch;
                *c_lit = first_watch;

                watcher_vec_push(&(occurences[*lits]), *w);
                *w = *(--end);
                v->size--;

                goto no_recurse;
            }
        
            // Second_watch is the only unassigned lit in the clause.
            // If we have to generate hints for the DRUP->LRUP conversion
            // we need to do some lookahead here, to assure the last hint 
            // simplifies to the one and only empty clause. Otherwise we can 
            // do some more propagations to omit the necassary datastructures
            // and their encompasing overhead.
            #ifdef DRUP_TO_LRUP_CONVERSION
            if (assignment[second_watch] == NEUTRAL_VAL) {
                // Save hint and mark lit as "on the stack".
                // Only add lit to the stack if it is not already on it in some way
                w->hint_lit = second_watch;
                PUSH_HINT(*w);
                assignment[second_watch] = ON_STACK;
                assignment[NEG(second_watch)] = -ON_STACK;
                unsigned_vec_push(trail, second_watch);
            #endif
            // mark second_watch to be propagated
            unsigned_vec_push(prop_stack, second_watch);
            #ifdef DRUP_TO_LRUP_CONVERSION
            } else if (assignment[second_watch] == -ON_STACK) {
                // conflicting unit is already on stack, we need to stop the hints here
                w->hint_lit = second_watch;
                PUSH_HINT(*w);
                return 1;
            }
            #endif
            w++;

            no_recurse:;
        
        }   // for all occurences
    }   // while stack not empy

    return 0;   // no conflict found
}

void drup_check_init(int nb_vars) {
    nb_known_vars = nb_vars;
    unsat_found = 0;
    drup_check_empty_id = 0;
    formula_loaded = false;
    original_ids = 0;
    int nb_lits = 2 * nb_vars;
    occurences = (struct watcher_vec*)palrup_utils_malloc(nb_lits * sizeof(struct watcher_vec));
    for (int i = 0; i < nb_lits; i++) {
        // init vectors manualy to save indirection introduced by vec_init
        struct watcher_vec * const vec = &(occurences[i]);
        vec->size = 0;
        vec->capacity = 4;
        vec->data = palrup_utils_calloc(vec->capacity, sizeof(watcher));
    }

    // clause db
    unsigned capacity = 1 << 31;
    db.lits = palrup_utils_malloc(capacity * sizeof(int));
    db.lits_size = 0;
    db.lits_capacity = capacity;
    db.earliest_delete = capacity;
    db.delete_count = 0;

    // hint db
    capacity = 1 << 16;
    hint_db.lits = palrup_utils_malloc(capacity * sizeof(int));
    hint_db.lits_size = 0;
    hint_db.lits_capacity = capacity;
    hint_db.earliest_delete = capacity;
    hint_db.delete_count = 0;

    add_buffer = int_vec_init(1024);
    assignment = palrup_utils_calloc(nb_lits, sizeof(u8));
    trail = unsigned_vec_init(1024);
    prop_stack = unsigned_vec_init(1024);
    units = unsigned_vec_init(1024);
    prop_stack_propagated = 0;
    #ifdef DRUP_TO_LRUP_CONVERSION
    hint_prefix = 0;
    // TODO: try max size for hash set/table
    marked_hints = hash_set_init(12);
    clause_lits = hash_set_init(10);
    derived_hints = hash_table_init(16);
    detailed_hints = watcher_vec_init(256);
    dfs_stack = unsigned_vec_init(256);
    hints = u64_vec_init(16);
    hints_tail = u64_vec_init(16);
    deletions = u64_vec_init(16);
    unit_ids = palrup_utils_calloc(nb_lits, sizeof(u64));
    derived_unit_hints = palrup_utils_calloc(nb_lits, sizeof(watcher));
    #endif
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
    unsat_found = 0;
    drup_check_empty_id = 0;
    formula_loaded = false;
    original_ids = 0;
    memset(&db, 0, sizeof(struct clause_db));
    prop_stack_propagated = 0;
    #ifdef DRUP_TO_LRUP_CONVERSION
    LOG("hint_prefix:%lu", hint_prefix);
    hint_prefix = 0;
    hash_set_free(marked_hints);
    hash_set_free(clause_lits);
    hash_table_light_free(derived_hints);
    watcher_vec_free(detailed_hints);
    u64_vec_free(hints);
    u64_vec_free(hints_tail);
    u64_vec_free(deletions);
    unsigned_vec_free(dfs_stack);
    free(unit_ids);
    free(derived_unit_hints);
    #endif
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

    // mark unsat as found
    if (nb_lits == 0) {
        if (!unsat_found) {
            unsat_found = 1;
            drup_check_empty_id = id;
        }
        return 0;
    }

    // transform elits into ilits if necessary
    unsigned ilits[nb_lits];
    if (internal_lits)
        memcpy(ilits, lits, nb_lits * sizeof(unsigned));    // TODO: not copy?
    else {
        OVERWRITE_WITH_ILITS(ilits, lits, nb_lits);
        nb_lits = checker_utils_remove_duplicates((int*)ilits, nb_lits);
    }

    // handle unit as special case
    if (nb_lits == 1) {
        unsigned lit = ilits[0];
        unsigned_vec_push(units, lit);
        #ifdef DRUP_TO_LRUP_CONVERSION
            unit_ids[lit] = id;
            watcher w = { .nb_lits = 1,
                          .blocking_lit = lit,
                          .c.ptr = -1,
                          .hint_lit = lit,
                          .id = id
                        };
            PUSH_HINT(w);
            PROPAGATE_UNIT(lit);
        #endif
        return 0;
    }

    watcher w;
    if (nb_lits == 2) {     // handle binary as special case
        // init watcher
        w.nb_lits = 2;
        w.blocking_lit = ilits[0];
        w.c.lit = ilits[1];
        #ifdef DRUP_TO_LRUP_CONVERSION
        w.hint_lit = 0;
        w.id = id;
        #endif

        // add watcher to watchlists
        watcher_vec_push(&(occurences[ilits[0]]), w);
        watcher_vec_push(&(occurences[ilits[1]]), w);
    } else {
        assert(nb_lits > 2);

        // switch already negated lits out of watches
        unsigned tmp = ilits[0];
        int i = 0;
        for (; i < nb_lits; i++)
            if (assignment[ilits[i]] != NEG_VAL) {
                ilits[0] = ilits[i];
                ilits[i] = tmp;
                break;
            }
        tmp = ilits[1];
        for (i++; i < nb_lits; i++)
            if (assignment[ilits[i]] != NEG_VAL) {
                ilits[1] = ilits[i];
                ilits[i] = tmp;
                break;
            }

        // init watcher
        unsigned db_offset = add_clause_to_db(&db, ilits, nb_lits);
        w.nb_lits = nb_lits;
        w.blocking_lit = ilits[nb_lits - 1];
        w.c.ptr = db_offset;
        #ifdef DRUP_TO_LRUP_CONVERSION
        w.hint_lit = 0;
        w.id = id;
        #endif

        // add clause to occurence list of first two lits
        watcher_vec_push(&(occurences[ilits[0]]), w);
        watcher_vec_push(&(occurences[ilits[1]]), w);
    }


    int effective_size = 0;
    unsigned unassigned_lit;
    for(unsigned* ilit = ilits; ilit < ilits + nb_lits; ilit++)
        if (assignment[*ilit] != NEG_VAL) {
            effective_size++;
            unassigned_lit = *ilit;
        }
    if (effective_size == 1 && assignment[unassigned_lit] == NEUTRAL_VAL) {
        #ifdef DRUP_TO_LRUP_CONVERSION
        w.hint_lit = unassigned_lit;
        #endif
        PUSH_HINT(w);
        PROPAGATE_UNIT(unassigned_lit);
    }

    return 0;
}
int drup_check_add_clause(u64 id, const int* lits, int nb_lits) {
    if (!formula_loaded) {
        snprintf(palrup_utils_msgstr, MSG_LEN, "Can not check clause %lu before formula is fully loaded.", id);
        palrup_utils_log_err(palrup_utils_msgstr);
        return 1;
    }

    // If a conflict was already derived, checking is done
    if (unsat_found) return 0;
    
    DEFINE_ILITS;
    nb_lits = checker_utils_remove_duplicates((int*)ilits, nb_lits);

    int res = 1;
    prop_stack_propagated = 0;
    unsigned_vec_resize(prop_stack, 0);
    for (int i = 0; i < nb_lits; i++) {
        #ifdef DRUP_TO_LRUP_CONVERSION
            unsigned neg_ilit = NEG(ilits[i]);
            unsigned_vec_push(prop_stack, neg_ilit);
            if (assignment[neg_ilit] == NEUTRAL_VAL) {
                assignment[neg_ilit] = ON_STACK;
                assignment[ilits[i]] = -ON_STACK;
                unsigned_vec_push(trail, neg_ilit);
            }
        #else
            unsigned_vec_push(prop_stack, NEG(ilits[i]));
        #endif
    }
    EMPTY_HINTS;
    if (drup_check_propagate()) {   // conflict found
        drup_check_reset_assignment();
        #ifdef DRUP_TO_LRUP_CONVERSION
        process_hints(nb_lits);
        #endif
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
    nb_lits = checker_utils_remove_duplicates((int*)ilits, nb_lits);
    if (nb_lits <= 1) return 0;     // don't delete unit or empty clauses

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
                #ifdef DRUP_TO_LRUP_CONVERSION
                // TODO: find more elegant solution
                if (!hash_table_find(derived_hints, w.id))   // do not delete in lrup if the clause is a hint
                    u64_vec_push(deletions, w.id);
                #endif
                // remove clause from occurence lists and delete
                v->data[j] = v->data[--(v->size)];
                if (nb_lits == 1)
                    return 0;   // only occurence was deleted

                // Skip some iterations of outer loop since we know the second occurence list.
                unsigned second_watch;
                if (nb_lits == 2)
                    second_watch = ilits[1];
                else {
                    unsigned* c = db.lits + w.c.ptr;
                    second_watch = c[0] == lit ? c[1] : c[0];
                }
                struct watcher_vec * const v2 = &(occurences[second_watch]);
                for (size_t k = 0; k < v2->size; k++) {
                    watcher w2 = v2->data[k];
                    #ifdef DRUP_TO_LRUP_CONVERSION
                    // skip hint_lit in comparison
                    if (memcmp(&w, &w2, sizeof(unsigned) * 3) | (w.id != w2.id)) continue;
                    #else
                    // memcmp is possible, since the hint_lit will not be compiled into the watches here
                    if (memcmp(&w, &w2, sizeof(watcher))) continue;
                    #endif
                    
                    // watch points to same clause => delete watch
                    v2->data[k] = v2->data[--(v2->size)];
                    if (nb_lits > 2) delete_clause_from_db(&db, w.c.ptr, w.nb_lits);
                    return 0;
                }
                
                return 1;   // second watch not found
            }
        }
    }

    return 1;
}

int drup_check_unsat_found() {
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

u64 drup_check_empty_clause_id() {
    return drup_check_empty_id;
}
void drup_check_generate_empty_clause_hints() {
    #ifdef DRUP_TO_LRUP_CONVERSION
    process_hints(0);
    #endif
}

#ifdef UNIT_TEST
struct watcher_vec* get_occurences() { return occurences; }
#endif