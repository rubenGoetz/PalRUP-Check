
#include <stdbool.h>
#include "utils/define.h"

// Top level checking procedure. Checks clauses, validates signatures,
// and returns certificates for (un)satisfiability.

void lrat_top_check_init(int nb_vars, bool check_model, bool lenient);
void lrat_top_check_end();
void lrat_top_check_commit_formula_sig(const u8* f_sig);
void lrat_top_check_load(int lit);
bool lrat_top_check_end_load();
u64 lrat_top_check_get_nb_loaded_clauses();
void lrat_top_check_compute_clause_signature(u64 id, const int* lits, int nb_lits, u8* out);
bool lrat_top_check_produce(unsigned long id, const int* literals, int nb_literals,
    const unsigned long* hints, int nb_hints);
bool lrat_top_check_import(unsigned long id, const int* literals, int nb_literals,
    const u8* signature_data);
bool lrat_top_check_delete(const unsigned long* ids, int nb_ids);
bool lrat_top_check_validate_unsat(u8* out_signature_or_null);
bool lrat_top_check_validate_sat(int* model, u64 size, u8* out_signature_or_null);
bool lrat_top_check_valid();
