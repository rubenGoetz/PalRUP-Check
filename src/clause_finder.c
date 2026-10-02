
#include <math.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "clause_finder.h"
#include "utils/checker_utils.h"
#include "import_merger.h"
#include "file_reader.h"

#define TYPE int
#define TYPED(THING) int_##THING
#include "vec.h"
#undef TYPED
#undef TYPE

// global values
u64 cf_num_solvers;
u64 cf_pal_id;
u64 cf_msg_group_size;
u64 cf_last_line = (u64)-1;
u64 cf_empty_clause_id = (u64)-1;
u64 cf_parsed_lines = 0;
bool cf_drup;
u8 sig_res_reported[16];
bool palrup_binary;
char confirm_path[512];
struct siphash* proof_check_hash;
struct file_reader* proof_reader;

// global buffers
u64 current_ID = EMPTY_ID;
u64 current_literals_size;
int* current_literals_data;
struct int_vec* proof_lits;
bool eof_proof_fragment = false;

// per message
struct siphash** import_check_hash;

static inline void finish_parse(bool stop_prematurely) {
    if (stop_prematurely) {
        LOG("Finish parse at line %lu of %lu necessary lines", cf_parsed_lines, cf_last_line);
        // allows termination of checking procedure if added clauses have higher that the empty clause,
        // since any clause of higher ID can not be used to derive it regardless.
        // Stop at line number given by local check, but make sure that the next important 
        // clause addition would have a larger id than the empty clause.
        COND_ERR(current_ID != EMPTY_ID && current_ID <= cf_empty_clause_id, "Error: clause left to check rank:%lu ID:%lu", cf_pal_id, current_ID);
        // finish parsing of importfiles to assure corect communication via fingerprints.
        while (current_ID != EMPTY_ID)
            import_merger_next();
    } else
        COND_ERR(current_ID != EMPTY_ID, "Error: clause left to check rank:%lu ID:%lu", cf_pal_id, current_ID);

    const u8* sig_res_computed = siphash_cls_digest(proof_check_hash);
    if (!checker_utils_equal_signatures(sig_res_computed, sig_res_reported)) {
        LOG_ERR("Signature does not match in Proof!");
        LOG_ERR("Computed sig: %lu", *((u64*)sig_res_computed));
        LOG_ERR("Read sig: %lu", *((u64*)sig_res_reported));
        fflush(stdout);
        abort();
    }
    siphash_cls_free(proof_check_hash);

    for (size_t i = 0; i < cf_msg_group_size; i++) {
        sig_res_computed = siphash_cls_digest(import_check_hash[i]);
        import_merger_read_sig((int*)sig_res_reported, i);
        if (!checker_utils_equal_signatures(sig_res_computed, sig_res_reported)) {
            LOG_ERR("Signature does not match in import %lu!", i);
            LOG_ERR("Computed sig: %lu", *((u64*)sig_res_computed));
            LOG_ERR("Read sig: %lu", *((u64*)sig_res_reported));
            fflush(stdout);
            abort();
        }
    }

    eof_proof_fragment = true;
}
static inline void parse_lits() {
    int_vec_resize(proof_lits, 0);
    while (true) {
        int lit = file_reader_read_vbl_int(proof_reader);
        if (!lit) break;
        int_vec_push(proof_lits, lit);
    }
}

static void parse_lrat() {
    while (true) {
        char c = file_reader_read_vbl_char(proof_reader);
        if (file_reader_eof_reached(proof_reader)) {
            finish_parse(false);
            break;

        } else if (c == TRUSTED_CHK_CLS_PRODUCE) {
            u64 id = (u64)file_reader_read_vbl_sl(proof_reader);
            siphash_cls_update(proof_check_hash, (u8*)&id, sizeof(u64));

            parse_lits();
            siphash_cls_update(proof_check_hash, (u8*)proof_lits->data, proof_lits->size * sizeof(int));
            
            while (true) {
                u64 hint = file_reader_read_vbl_sl(proof_reader);
                if (!hint) break;
            }

            // next line
            if (id < current_ID)
                continue;

            // check if the clause is the same
            if (id == current_ID) {
                if (checker_utils_compare_lits(current_literals_data, proof_lits->data, current_literals_size, proof_lits->size))
                    break;
                else {
                    LOG_ERR("Literals do not match in proof. pal_id:%lu clause_ID:%lu", cf_pal_id, current_ID);
                    fflush(stdout);
                    abort();
                }
            }

            COND_ERR(id > current_ID, "Clause of not found. pal_id:%lu clause_ID:%lu", cf_pal_id, current_ID);

        } else if (c == TRUSTED_CHK_CLS_IMPORT) {
            // skip id
            file_reader_read_vbl_sl(proof_reader);

            // skip lits
            while (true) {
                int lit = file_reader_read_vbl_int(proof_reader);
                if (!lit) break;
            }

        } else if (c == TRUSTED_CHK_CLS_DELETE) {
            while (true)
                if (!file_reader_read_vbl_sl(proof_reader))
                    break;
            
        } else {
            LOG_ERR("Invalid directive %c", c);
            fflush(stdout);
            abort();
        }

        cf_parsed_lines++;
    }
}

static void parse_drup() {
    u64 id = 0;
    while (true) {
        char c = file_reader_read_vbl_char(proof_reader);
        if (file_reader_eof_reached(proof_reader) | (cf_parsed_lines == cf_last_line)) {
            finish_parse(true);
            break;

        } else if (c == TRUSTED_CHK_CLS_PRODUCE) {
            id = (u64)file_reader_read_vbl_sl(proof_reader);
            siphash_cls_update(proof_check_hash, (u8*)&id, sizeof(u64));

            parse_lits();
            siphash_cls_update(proof_check_hash, (u8*)proof_lits->data, proof_lits->size * sizeof(int));
            
            // next line
            if (id < current_ID)
                continue;

            // check if the clause is the same
            if (id == current_ID) {
                if (checker_utils_compare_lits(current_literals_data, proof_lits->data, current_literals_size, proof_lits->size))
                    break;
                else {
                    LOG_ERR("Literals do not match in proof. pal_id:%lu clause_ID:%lu", cf_pal_id, current_ID);
                    fflush(stdout);
                    abort();
                }
            }

            COND_ERR(id > current_ID, "Clause of not found. pal_id:%lu clause_ID:%lu", cf_pal_id, current_ID);

        } else if (c == TRUSTED_CHK_CLS_IMPORT) {
            // skip id
            id = file_reader_read_vbl_sl(proof_reader);

            // skip lits
            while (true) {
                int lit = file_reader_read_vbl_int(proof_reader);
                if (!lit) break;
            }

        } else if (c == TRUSTED_CHK_CLS_DELETE) {
            // skip hints
            while (true)
                if (!file_reader_read_vbl_int(proof_reader))
                    break;
            
        } else {
            LOG_ERR("Invalid directive %c", c);
            fflush(stdout);
            abort();
        }

        cf_parsed_lines++;
    }
}

static void init_strat3(struct options* options) {
    cf_msg_group_size = palrup_utils_calc_root_ceil(cf_num_solvers);
    import_check_hash = palrup_utils_malloc(sizeof(struct siphash*) * cf_msg_group_size);
    
    // calculate incoming file paths
    char* file_paths[cf_msg_group_size];
    int column = cf_pal_id % cf_msg_group_size;
    for (size_t i = 0; i < cf_msg_group_size; i++) {
        file_paths[i] = palrup_utils_malloc(512);
        int src_rank = (i * cf_msg_group_size) + column;
        snprintf(file_paths[i], 512, "%s/%lu/%i/%s",
                 options->working_path, src_rank / cf_msg_group_size, src_rank, IMPORT_FILE_NAME);
        import_check_hash[i] = siphash_cls_init(SECRET_KEY);
    }

    import_merger_init(cf_msg_group_size, file_paths, &current_ID, &current_literals_data, &current_literals_size, options->read_buffer_size, import_check_hash, NULL);
    for (size_t i = 0; i < cf_msg_group_size; i++)
        free(file_paths[i]);
}

void clause_finder_init(struct options* options) {
    cf_num_solvers = options->num_solvers;
    cf_pal_id = options->pal_id;
    cf_drup = options->drup;
    cf_last_line = (u64)-1;
    cf_empty_clause_id = (u64)-1;
    cf_parsed_lines = 0;
    palrup_binary = options->palrup_binary;
    proof_check_hash = siphash_cls_init(SECRET_KEY);
    proof_lits = int_vec_init(1);

    int dir_hierarchy = cf_pal_id / palrup_utils_calc_root_ceil(cf_num_solvers);
    
    // init proof fragment
    char frag_path[512];
    snprintf(frag_path, 512, "%s/%u/%lu/%s",
             options->palrup_path, dir_hierarchy, cf_pal_id,
             cf_drup ? DRUP_FRAGMENT_NAME : LRUP_FRAGMENT_NAME);
    FILE* proof_frag = fopen(frag_path, "rb");
    if (!proof_frag) {
        snprintf(palrup_utils_msgstr, MSG_LEN, "Could not open proof fragment at %s\n", frag_path);
        palrup_utils_log_err(palrup_utils_msgstr);
    }
    proof_reader = file_reader_init(options->read_buffer_size, proof_frag, cf_pal_id);
    
    // read proof signature
    char sig_path[512];
    snprintf(sig_path, 512, "%s/%u/%lu/%s.hash",
             options->palrup_path, dir_hierarchy, cf_pal_id,
             cf_drup ? DRUP_FRAGMENT_NAME : LRUP_FRAGMENT_NAME);
    FILE* frag_sig = fopen(sig_path, "rb");
    if (!frag_sig) {
        snprintf(palrup_utils_msgstr, MSG_LEN, "Could not open proof fragment's signature at %s\n", sig_path);
        palrup_utils_log_err(palrup_utils_msgstr);
    }
    cf_last_line = palrup_utils_read_ul(frag_sig);
    LOG("Received last line number: %lu", cf_last_line);
    palrup_utils_read_sig(sig_res_reported, frag_sig);
    fclose(frag_sig);

    char unsat_details[533];
    snprintf(unsat_details, 533, "%s/.unsat_found/details", options->working_path);
    FILE* details = fopen(unsat_details, "rb");
    if(details) {
        cf_empty_clause_id = palrup_utils_read_ul(details);
        LOG("Received empty clause ID %lu", cf_empty_clause_id);
    } else
        LOG_WARN("Could not read unsat details at %s", unsat_details);

    // create .check_ok flag path
    snprintf(confirm_path, 512, "%s/%u/%lu/.check_ok",
             options->working_path, dir_hierarchy, cf_pal_id);

    switch (options->redist_strat) {
    case 1:
    case 2:
    case 3:
        init_strat3(options);
        break;
    default:
        snprintf(palrup_utils_msgstr, MSG_LEN, "Redistribution strategy %lu not available.", options->redist_strat);
        palrup_utils_log_err(palrup_utils_msgstr);
        break;
    }
}

void clause_finder_run() {
    while (!eof_proof_fragment) {
        import_merger_next();

        // skip clauses for different lines
        if (current_ID % cf_num_solvers != cf_pal_id && current_ID != (u64)-1)
            continue;

        cf_drup ? parse_drup() : parse_lrat();
    }
    mkdir(confirm_path, 0777);

    char msg[512];
    snprintf(msg, 512, "Done pal_id=%lu", cf_pal_id);
    palrup_utils_log(msg);
}

void clause_finder_end() {
    for (size_t i = 0; i < cf_msg_group_size; i++)  {
        siphash_cls_free(import_check_hash[i]);
    }
    free(import_check_hash);
    file_reader_end(proof_reader);
    import_merger_end();
    int_vec_free(proof_lits);
}
