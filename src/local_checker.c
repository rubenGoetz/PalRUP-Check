
#include "local_checker.h"  // for file_reader_read_int, palrup_utils_log...

#include <math.h>
#include <stdbool.h>  // for bool, true, false
#include <stdio.h>    // for fclose, fflush_unlocked, fopen, snprintf
#include <stdlib.h>   // for free
#include <unistd.h>    // for access
#include <sys/stat.h>

#include "utils/palrup_utils.h"
#include "utils/checker_utils.h"
//#include "checker_interface.h"
//#include "hash.h"
#include "file_reader.h"
#include "import_handler.h"
#include "secret.h"
#include "siphash_cls.h"
#include "top_check.h"  // for top_check_commit_formula_sig, top_check_d...

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

struct file_reader* proof;  // named pipe
struct siphash* clause_hash;

int nb_vars;               // # variables in formula
long nb_clauses;           // # clauses in formula
signature formula_sig;     // formula signature
u64 nb_solvers;            // number of solvers
u64 solver_rank;           // solver id
u64 redist;                // redistribution_strategy
u64 pc_nb_loaded_clauses;  // number of loaded clauses
char proof_path_in[512];
char redestribute_path_out[512];

bool do_logging = true;
bool palrup_binary;

// Buffering.
signature buf_sig;
struct int_vec* buf_lits;
struct u64_vec* buf_hints;

//static void read_literals(int nb_lits) {
//    int_vec_reserve(buf_lits, nb_lits);
//    file_reader_read_ints(buf_lits->data, nb_lits, proof);
//}

//static void read_hints(int nb_hints) {
//    u64_vec_reserve(buf_hints, nb_hints);
//    file_reader_read_uls(buf_hints->data, nb_hints, proof);
//}

static bool load_from_file(FILE* formula) {
    int nb_vars;
    long nClauses;

    char buffer[1024];
    bool foundPcnf = false;
    int tmp = 0;

    while (fgets(buffer, sizeof(buffer), formula)) {
        if (buffer[0] == 'c') continue;  // Skip comments

        // Check for the line starting with "p cnf"
        tmp = sscanf(buffer, "p cnf %i %li \n", &nb_vars, &nClauses);
        if (tmp == 2) {
            foundPcnf = true;
            break;
        }
    }
    nb_clauses = nClauses;

    if (!foundPcnf) {
        palrup_utils_log_err("Error: 'p cnf' line not found in the formula file");
        return false;
    }

    if (solver_rank == 0) {
        char msg[512];
        snprintf(msg, 512, "Start reading the formula file: cnf %i %li:", nb_vars, nClauses);
        palrup_utils_log(msg);
    }

    top_check_init(nb_vars, false, false);
    bool no_error = true;
    while (true) {
        int lit;
        tmp = fscanf(formula, " %i ", &lit);

        if (tmp == EOF) break;

        top_check_load(lit);
        // printf("lit: %i\n", lit);
    }

    top_check_end_load();
    pc_nb_loaded_clauses = top_check_get_nb_loaded_clauses();

    if (solver_rank == 0) {
        char log_str[512];
        snprintf(log_str, 512, "Formular Loaded nb_clauses:%lu", pc_nb_loaded_clauses);
        palrup_utils_log(log_str);
    }

    return no_error;
}

void parse(u64* nb_produced, u64* nb_imported, u64* nb_deleted) {
    u64 max_derived_id = 0;
    while (true) {
        char c = file_reader_read_vbl_char(proof);
        if (file_reader_eof_reached(proof)) {
            u8* sig = siphash_cls_digest(clause_hash);

            // write in new file for stage 2
            char finger_print_path[517];
            snprintf(finger_print_path, 517, "%s.hash", proof_path_in);
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
                snprintf(msg, 512, "Learned clause has ID lower that original formula. ID:%lu, solver_rank:%lu, nb_solvers:%lu", id, solver_rank, nb_solvers);
                palrup_utils_log_err(msg);
                exit(1);
            }

            // locality of assigned IDs
            if (id % nb_solvers != solver_rank) {
                char msg[523];
                snprintf(msg, 512, "Learned clause has non local ID. ID:%lu, solver_rank:%lu, nb_solvers:%lu", id, solver_rank, nb_solvers);
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
            *nb_produced += 1;

        } else if (c == TRUSTED_CHK_CLS_IMPORT) {
            int_vec_resize(buf_lits, 0);

            u64 id = (u64)file_reader_read_vbl_sl(proof);

            // Check ID against original formula
            if (id <= (u64)nb_clauses) {
                char msg[523];
                snprintf(msg, 512, "Learned clause has ID lower that original formula. ID:%lu, solver_rank:%lu, nb_solvers:%lu", id, solver_rank, nb_solvers);
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
            checker_utils_import_unchecked(id, buf_lits->data, nb_lits);
            *nb_imported += 1;

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
            *nb_deleted += nb_hints;

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

void local_checker_init(const char* formula_path, const char* palrup_path, const char* working_path, unsigned long pal_id, unsigned long num_solvers, unsigned long redist_strat, unsigned long read_buffer_size, bool use_palrup_binary) {
    FILE* formula;
    palrup_binary = use_palrup_binary;
    clause_hash = siphash_cls_init(SECRET_KEY);
    //double root_n = sqrt((double)num_solvers);
    size_t comm_size = (size_t)ceil(sqrt((double)num_solvers));  // round to nearest integer
    if (redist_strat == 1) {
        comm_size = num_solvers;
    }
    unsigned int dir_hierarchy = pal_id / comm_size;
    snprintf(proof_path_in, 512, "%s/%u/%lu/out.palrup", palrup_path, dir_hierarchy, pal_id);
    snprintf(redestribute_path_out, 512, "%s", working_path);

    if (access(proof_path_in, F_OK) != 0) {
        snprintf(palrup_utils_msgstr, MSG_LEN, "proof_path_in does not exist. Creating it: %s", proof_path_in);
        palrup_utils_log(palrup_utils_msgstr);
        // file doesn't exist
        // create placeholder file containing only 0
        FILE* f = fopen(proof_path_in, "wb");
        fclose(f);
    } 

    FILE* proof_stream = fopen(proof_path_in, "rb+");
    if (!proof_stream) palrup_utils_log_err("proof_path_in could not be opened");
    proof = file_reader_init(read_buffer_size, proof_stream, pal_id);


    formula = fopen(formula_path, "rb");
    if (!formula) palrup_utils_log_err("formula_path could not be opened");
    //UNUSED(formula_path);
    buf_lits = int_vec_init(1 << 14);
    buf_hints = u64_vec_init(1 << 14);
    nb_solvers = num_solvers;
    solver_rank = pal_id;
    import_handler_init(working_path, pal_id, num_solvers, redist_strat, read_buffer_size);
    if (!load_from_file(formula)) {
        exit(0);
    }
    fclose(formula);
}

int local_checker_run() {
    u64 nb_produced = 0, nb_imported = 0, nb_deleted = 0;

    parse(&nb_produced, &nb_imported, &nb_deleted);
    
    if (top_check_validate_unsat(NULL)) {
        char unsat_folder[525];
        snprintf(unsat_folder, 525, "%s/.unsat_found", redestribute_path_out);
        if (mkdir(unsat_folder, 0777) == 0) {
            char unsat_folder_sub[1024];
            snprintf(unsat_folder_sub, 1024, "%s/%lu", unsat_folder, solver_rank);
            mkdir(unsat_folder_sub, 0777);
        }
    }
    snprintf(palrup_utils_msgstr, 512, "rank:%lu prod:%lu imp:%lu del:%lu n_s:%lu", solver_rank, nb_produced, nb_imported, nb_deleted, nb_solvers);
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
