
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

#include "local_checker.h"
#include "file_reader.h"
#include "utils/palrup_utils.h"
#include "utils/checker_utils.h"
#include "lrat_check.h"
#include "import_handler.h"
#include "siphash_cls.h"
#include "top_check.h"

// Instantiate int_vec
#define TYPE int
#define TYPED(THING) int_##THING
#include "vec.h"
#undef TYPED
#undef TYPE

// Instantiate u64_vec
#define TYPE u64
#define TYPED(THING) u64_##THING
#include "vec.h"
#undef TYPED
#undef TYPE

struct local_checker_stats {
    u64 nb_produced;
    u64 nb_imported;
    u64 nb_deleted;
} local_checker_stats_init = {0, 0, 0};

// formula
long nb_clauses;

// global values
u64 lc_num_solvers;
u64 lc_pal_id;
bool palrup_binary;
char fragment_path[512];
char working_path[512];
struct file_reader* proof;
struct siphash* clause_hash;
struct local_checker_stats lc_stats;

// global buffers.
//signature buf_sig;
struct int_vec* buf_lits;
struct u64_vec* buf_hints;

static bool load_formula(FILE* formula) {
    int nb_vars;

    char buffer[1024];
    bool foundPcnf = false;
    int tmp = 0;

    while (fgets(buffer, sizeof(buffer), formula)) {
        if (buffer[0] == 'c') continue;  // Skip comments

        // Check for the line starting with "p cnf"
        tmp = sscanf(buffer, "p cnf %i %li \n", &nb_vars, &nb_clauses);
        if (tmp == 2) {
            foundPcnf = true;
            break;
        }
    }

    if (!foundPcnf) {
        palrup_utils_log_err("Error: 'p cnf' line not found in the formula file");
        return false;
    }

    if (lc_pal_id == 0) {
        char msg[512];
        snprintf(msg, 512, "Start reading the formula file: cnf %i %li:", nb_vars, nb_clauses);
        palrup_utils_log(msg);
    }

    top_check_init(nb_vars, false, false);
    bool no_error = true;
    while (true) {
        int lit;
        tmp = fscanf(formula, " %i ", &lit);

        if (tmp == EOF) break;

        top_check_load(lit);
    }

    top_check_end_load();
    u64 lc_nb_loaded_clauses = top_check_get_nb_loaded_clauses();

    if (lc_pal_id == 0) {
        char log_str[512];
        snprintf(log_str, 512, "Formula loaded nb_clauses:%lu", lc_nb_loaded_clauses);
        palrup_utils_log(log_str);
    }

    return no_error;
}

static void parse() {
    u64 max_derived_id = 0;
    while (true) {
        char c = file_reader_read_vbl_char(proof);
        if (file_reader_eof_reached(proof)) {
            u8* sig = siphash_cls_digest(clause_hash);

            // write .palrup.hash file
            char finger_print_path[517];
            snprintf(finger_print_path, 517, "%s.hash", fragment_path);
            FILE* finger_print = fopen(finger_print_path, "wb");
            if (!finger_print) {
                char msg[1024];
                snprintf(msg, 1024, "Can't open file %s", finger_print_path);
                palrup_utils_log_err(msg);
            }
                
            palrup_utils_write_sig(sig, finger_print);
            fclose(finger_print);
            break;

        } else if (c == TRUSTED_CHK_CLS_PRODUCE) {
            int_vec_resize(buf_lits, 0);
            u64_vec_resize(buf_hints, 0);
            
            u64 id = (u64)file_reader_read_vbl_sl(proof);
            siphash_cls_update(clause_hash, (u8*)&id, sizeof(u64));

            // Starting point of assigned ids
            if (id <= (u64)nb_clauses) {
                char msg[523];
                snprintf(msg, 512, "Learned clause has ID lower that original formula. ID:%lu, pal_id:%lu, num_solvers:%lu", id, lc_pal_id, lc_num_solvers);
                palrup_utils_log_err(msg);
                exit(1);
            }

            // locality of assigned IDs
            if (id % lc_num_solvers != lc_pal_id) {
                char msg[523];
                snprintf(msg, 512, "Learned clause has non local ID. ID:%lu, pal_id:%lu, num_solvers:%lu", id, lc_pal_id, lc_num_solvers);
                palrup_utils_log_err(msg);
                exit(1);
            }

            // Monotonicity of assigned IDs
            if (id < max_derived_id) {
                char msg[523];
                snprintf(msg, 512, "Learned clause has lower ID that previously learned clause. newID:%lu, prevID:%lu", id, max_derived_id);
                palrup_utils_log_err(msg);
                exit(1);
            }
            max_derived_id = id;

            // parse lits
            int nb_lits = 0;
            while (true) {
                int lit = file_reader_read_vbl_int(proof);
                if (!lit) break;
                int_vec_push(buf_lits, lit);
                nb_lits++;
            }
            siphash_cls_update(clause_hash, (u8*)buf_lits->data, nb_lits * sizeof(int));

            // parse hints
            int nb_hints = 0;
            while (true) {
                u64 hint = (u64)file_reader_read_vbl_sl(proof);
                if (!hint) break;
                u64_vec_push(buf_hints, hint);
                nb_hints++;
            }

            //check IDs in hints
            if (!checker_utils_check_hints(id, buf_hints->data, nb_hints)) {
                char msg[523];
                snprintf(msg, 512, "Discoverd hint >= id in produced clause. ID:%lu", id);
                palrup_utils_log_err(msg);
                exit(1);
            }

            // forward to checker
            top_check_produce(id, buf_lits->data, nb_lits,
                              buf_hints->data, nb_hints);
            lc_stats.nb_produced++;

        } else if (c == TRUSTED_CHK_CLS_IMPORT) {
            int_vec_resize(buf_lits, 0);

            u64 id = (u64)file_reader_read_vbl_sl(proof);

            // Check ID against original formula
            if (id <= (u64)nb_clauses) {
                char msg[523];
                snprintf(msg, 512, "Learned clause has ID lower that original formula. ID:%lu, pal_id:%lu, num_solvers:%lu", id, lc_pal_id, lc_num_solvers);
                palrup_utils_log_err(msg);
                exit(1);
            }

            // parse lits
            int nb_lits = 0;
            while (true) {
                int lit = file_reader_read_vbl_int(proof);
                if (!lit) break;
                int_vec_push(buf_lits, lit);
                nb_lits++;
            }

            // forward to checker
            lrat_check_add_axiomatic_clause(id, buf_lits->data, nb_lits);
            lc_stats.nb_imported++;

            // write in file for stage 2
            import_handler_log(id, buf_lits->data, nb_lits);

        } else if (c == TRUSTED_CHK_CLS_DELETE) {
            u64_vec_resize(buf_hints, 0);

            // parse hints
            int nb_hints = 0;
            while (true) {
                u64 hint = (u64)file_reader_read_vbl_sl(proof);
                if (!hint) break;
                u64_vec_push(buf_hints, hint);
                nb_hints++;
            }

            top_check_delete(buf_hints->data, nb_hints);
            lc_stats.nb_deleted += nb_hints;

        } else {
            char errlog[512];
            snprintf(errlog, 512, "Invalid directive! c: %d filesize:%lu", c, proof->total_bytes);
            palrup_utils_log_err(errlog);
            exit(1);
        }

        if (UNLIKELY(!top_check_valid())) {    
            palrup_utils_log_err(palrup_utils_msgstr);
            exit(1);
        }
    }
}

void local_checker_init(struct options* options) {
    lc_num_solvers = options->num_solvers;
    lc_pal_id = options->pal_id;
    palrup_binary = options->palrup_binary;
    lc_stats = local_checker_stats_init;
    clause_hash = siphash_cls_init(SECRET_KEY);
    unsigned int dir_hierarchy = options->pal_id / palrup_utils_calc_root_ceil(lc_num_solvers);
    snprintf(fragment_path, 512, "%s/%u/%lu/out.palrup", options->palrup_path, dir_hierarchy, options->pal_id);
    snprintf(working_path, 512, "%s", options->working_path);
    lc_stats = local_checker_stats_init;

    buf_lits = int_vec_init(1);
    buf_hints = u64_vec_init(1);

    FILE* proof_fragment = fopen(fragment_path, "rb");
    if (!proof_fragment) {
        snprintf(palrup_utils_msgstr, MSG_LEN, "Proof fragment could not be opened at %s", fragment_path);
        palrup_utils_log_err(palrup_utils_msgstr);
    }
    proof = file_reader_init(options->read_buffer_size, proof_fragment, options->pal_id);

    FILE* formula;
    formula = fopen(options->formula_path, "rb");
    if (!formula) {
        snprintf(palrup_utils_msgstr, MSG_LEN, "Formula could not be opened at %s", options->formula_path);
        palrup_utils_log_err(palrup_utils_msgstr);
    }        
    if (!load_formula(formula))
        exit(0);
    fclose(formula);

    import_handler_init(options);
}

int local_checker_run() {
    parse();
    
    if (top_check_validate_unsat(NULL)) {
        char unsat_folder[525];
        snprintf(unsat_folder, 525, "%s/.unsat_found", working_path);
        if (mkdir(unsat_folder, 0777) == 0) {
            char unsat_folder_sub[1024];
            snprintf(unsat_folder_sub, 1024, "%s/%lu", unsat_folder, lc_pal_id);
            mkdir(unsat_folder_sub, 0777);
        }
    }
    snprintf(palrup_utils_msgstr, 512, "rank:%lu prod:%lu imp:%lu del:%lu n_s:%lu",
             lc_pal_id, lc_stats.nb_produced, lc_stats.nb_imported, lc_stats.nb_deleted, lc_num_solvers);
    palrup_utils_log(palrup_utils_msgstr);

    return 0;
}

void local_checker_end() {
    import_handler_end();
    int_vec_free(buf_lits);
    u64_vec_free(buf_hints);
    file_reader_end(proof);
    top_check_end();
    siphash_cls_free(clause_hash);
}
