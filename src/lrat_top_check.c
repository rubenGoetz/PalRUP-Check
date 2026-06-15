
#include <string.h>

#include "utils/palrup_utils.h"
#include "utils/checker_utils.h"
#include "lrat_check.h"
#include "siphash.h"
#include "confirm.h"

bool parsed_formula = false;
signature formula_signature;

bool lrat_valid = true;


void lrat_top_check_compute_clause_signature(u64 id, const int* lits, int nb_lits, u8* out) {
    siphash_reset();
    siphash_update((u8*) &id, sizeof(u64));
    siphash_update((u8*) lits, nb_lits*sizeof(int));
    siphash_update(formula_signature, SIG_SIZE_BYTES);
    const u8* hash_out = siphash_digest();
    //palrup_utils_copy_bytes(out, hash_out, SIG_SIZE_BYTES);
    memcpy(out, hash_out, SIG_SIZE_BYTES);
}


void lrat_top_check_init(int nb_vars, bool check_model, bool lenient) {
    siphash_init(SECRET_KEY);
    lrat_check_init(nb_vars, check_model, lenient);
}

void lrat_top_check_end() {
    siphash_free();
    lrat_check_end();
}

void lrat_top_check_commit_formula_sig(const u8* f_sig) {
    // Store formula signature to validate later after loading
    //palrup_utils_copy_bytes(formula_signature, f_sig, SIG_SIZE_BYTES);
    memcpy(formula_signature, f_sig, SIG_SIZE_BYTES);
}

void lrat_top_check_load(int lit) {
    lrat_valid &= lrat_check_load(lit);
}

bool lrat_top_check_end_load() {
    u8* sig_from_chk;
    lrat_valid = lrat_valid && lrat_check_end_load(&sig_from_chk);
    if (!lrat_valid) return false;
    return lrat_valid;
}

u64 lrat_top_check_get_nb_loaded_clauses() {
    return lrat_check_get_nb_loaded_clauses();
}

bool lrat_top_check_produce(unsigned long id, const int* literals, int nb_literals,
    const unsigned long* hints, int nb_hints) {
    
    // forward clause to checker
    lrat_valid &= lrat_check_add_clause(id, literals, nb_literals, hints, nb_hints);
    if (!lrat_valid) return false;
    return true;
}

bool lrat_top_check_import(unsigned long id, const int* literals, int nb_literals,
    const u8* signature_data) {
    
    // verify signature
    signature computed_sig;
    lrat_top_check_compute_clause_signature(id, literals, nb_literals, computed_sig);
    if (!checker_utils_equal_signatures(signature_data, computed_sig)) {
        lrat_valid = false;
        snprintf(palrup_utils_msgstr, 512, "Signature check of clause %lu failed", id);
        return false;
    }

    // signature verified - forward clause to checker as an axiom
    lrat_valid &= lrat_check_add_axiomatic_clause(id, literals, nb_literals);
    return lrat_valid;
}

bool lrat_top_check_delete(const unsigned long* ids, int nb_ids) {
    return lrat_check_delete_clause(ids, nb_ids);
}

bool lrat_top_check_validate_unsat(u8* out_signature_or_null) {
    lrat_valid &= lrat_check_validate_unsat();
    if (!lrat_valid) return false;
    if (out_signature_or_null)
        confirm_result(formula_signature, 20, out_signature_or_null);
    return true;
}

bool lrat_top_check_validate_sat(int* model, u64 size, u8* out_signature_or_null) {
    lrat_valid &= lrat_check_validate_sat(model, size);
    if (!lrat_valid) return false;
    if (out_signature_or_null)
        confirm_result(formula_signature, 10, out_signature_or_null);
    return true;
}

bool lrat_top_check_valid() {return lrat_valid;}
