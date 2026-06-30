
#include <assert.h>
#include <string.h>

#include "drup_top_check.h"
#include "drup_check.h"

// count mishaps during runtime
// state is considered invalid if mishaps > 0
u64 mishaps;

// signature of checked proof
// TODO: might not be needed
signature proof_sig;

void drup_top_check_init(int nb_vars) {
    drup_check_init(nb_vars);
}
void drup_top_check_end() {
    drup_check_end();
}

bool drup_top_check_load(int lit) {
    mishaps += drup_check_load(lit);
    return drup_top_check_valid();
}
bool drup_top_check_end_load() {
    mishaps += drup_check_end_load();
    mishaps += !drup_check_formula_loaded();
    return drup_top_check_valid();
}
u64 drup_top_check_get_nb_loaded_clauses() {
    return drup_check_get_nb_loaded_clauses();
}

bool drup_top_check_add(u64 id, const int* lits, int nb_lits) {
    mishaps += drup_check_add_clause(id, lits, nb_lits);
    return drup_top_check_valid();
}
bool drup_top_check_import(u64 id, const int* lits, int nb_lits) {
    mishaps += drup_check_add_axiomatic_clause(id, lits, nb_lits, false);
    return drup_top_check_valid();
}
bool drup_top_check_delete(const int* lits, int nb_lits) {
    mishaps += drup_check_delete_clause(lits, nb_lits); 
    return drup_top_check_valid();
}

bool drup_top_check_unsat_found() {
    return drup_check_unsat_found();
}
inline bool drup_top_check_valid() {
    return mishaps == 0;
}
u64 drup_top_check_mishaps() {
    return mishaps;
}
