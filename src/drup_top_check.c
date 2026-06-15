
#include <assert.h>
#include <string.h>

#include "drup_top_check.h"
#include "drup_check.h"
#include "siphash.h"

// Internal validity. Can not recover from a mishap
bool drup_valid = true;

// count mishaps during runtime
u64 mishaps;

// signature of checked proof
// TODO: might not be needed
signature proof_sig;

void drup_top_check_init(int nb_vars) {
    siphash_init(SECRET_KEY);
    drup_check_init(nb_vars);
}
void drup_top_check_end(u8* sig) {
    if (sig) {
        const u8* hash = siphash_digest();
        memcpy(sig, hash, SIG_SIZE_BYTES);
    }
    
    siphash_free();
    drup_check_end();
}

bool drup_top_check_load(int lit) {
    int res = drup_check_load(lit);

    if (res) {
        mishaps += res;
        drup_valid = false;
    }
    return drup_valid;
}
bool drup_top_check_end_load() {
    int res = drup_check_end_load();
    res += !drup_check_formula_loaded();

    if (res) {
        mishaps++;
        drup_valid = false;
    }
    return drup_valid;
}
u64 drup_top_check_get_nb_loaded_clauses() {
    return drup_check_get_nb_loaded_clauses();
}

bool drup_top_check_add(u64 id, const int* lits, int nb_lits) {
    int res = drup_check_add_clause(id, lits, nb_lits);
    
    siphash_update((u8*) &id, sizeof(u64));
    siphash_update((u8*) lits, nb_lits*sizeof(int));

    if (res) {
        mishaps += res;
        drup_valid = false;
    }
    return drup_valid;
}
bool drup_top_check_import(u64 id, const int* lits, int nb_lits) {
    int res = drup_check_add_axiomatic_clause(id, lits, nb_lits);

    siphash_update((u8*) &id, sizeof(u64));
    siphash_update((u8*) lits, nb_lits*sizeof(int));

    if (res) {
        mishaps += res;
        drup_valid = false;
    }
    return drup_valid;
}
bool drup_top_check_delete(const int* lits, int nb_lits) {
    int res = drup_check_delete_clause(lits, nb_lits);

    if (res) {
        mishaps += res;
        drup_valid = false;
    }
    return drup_valid;
}

bool drup_top_check_unsat_found() {
    return drup_check_unsat_found();
}
bool drup_top_check_valid() {
    return drup_valid;
}
u64 drup_top_check_mishaps() {
    return mishaps;
}
