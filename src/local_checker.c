
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

#include "local_checker.h"
//#include "file_reader.h"
#include "dummy_file_reader.h"
#include "utils/palrup_utils.h"
#include "utils/checker_utils.h"
#include "lrat_check.h"
#include "drup_check.h"
#include "import_handler.h"
#include "siphash_cls.h"
#include "lrat_top_check.h"
#include "drup_top_check.h"
#include "hash.h"
#include "clause_flat.h"

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

#ifdef DRUP_TO_LRUP_CONVERSION
#include "utils/file_utils.h"
extern FILE* lrup_out;
#endif

struct local_checker_stats {
    u64 nb_produced;
    u64 nb_imported;
    u64 nb_imported_used;
    u64 nb_deleted;
} local_checker_stats_init = {0, 0, 0, 0};

// formula
long nb_clauses;

// global values
u64 lc_num_solvers;
u64 lc_pal_id;
u64 lc_max_derived_id = 0;
bool lc_drup;
bool palrup_binary;
char fragment_path[512];
char working_path[512];
struct file_reader* proof;
struct siphash* clause_hash;
struct local_checker_stats lc_stats;

// global buffers.
struct int_vec* buf_lits;
struct u64_vec* buf_hints;
struct hash_table* import_table;

static inline int parse_header(FILE* formula) {
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
        LOG_ERR("Error: 'p cnf' line not found in the formula file");
        return false;
    }

    LOG("Start reading the formula file: cnf %i %li:", nb_vars, nb_clauses);
    return nb_vars;
}

static void load_formula_lrat(FILE* formula) {
    int nb_vars = parse_header(formula);
    int tmp = 0;

    lrat_top_check_init(nb_vars, false, false);
    while (true) {
        int lit;
        tmp = fscanf(formula, " %i ", &lit);
        if (tmp == EOF) break;

        lrat_top_check_load(lit);
    }

    lrat_top_check_end_load();
    u64 lc_nb_loaded_clauses = lrat_top_check_get_nb_loaded_clauses();

    LOG("Formula loaded nb_clauses:%lu", lc_nb_loaded_clauses);
}

static void load_formula_drup(FILE* formula) {
    int nb_vars = parse_header(formula);
    int tmp = 0;

    drup_top_check_init(nb_vars);
    while (true) {
        int lit;
        tmp = fscanf(formula, " %i ", &lit);
        if (tmp == EOF) break;

        drup_top_check_load(lit);
    }

    drup_top_check_end_load();
    u64 lc_nb_loaded_clauses = drup_top_check_get_nb_loaded_clauses();

    LOG("Formula loaded nb_clauses:%lu", lc_nb_loaded_clauses);
}

static inline void finish_parse() {
    u8* sig = siphash_cls_digest(clause_hash);

    // write .palrup.hash file
    char finger_print_path[517];
    snprintf(finger_print_path, 517, "%s.hash", fragment_path);
    FILE* finger_print = fopen(finger_print_path, "wb");
    COND_ERR(!finger_print, "Can't open file %s", finger_print_path);
                
    palrup_utils_write_sig(sig, finger_print);
    fclose(finger_print);
}
static inline void check_id(u64 id, bool all) {
    // Starting point of assigned ids
    if (id <= (u64)nb_clauses) {
        LOG_ERR("Learned clause has ID lower than original formula. ID:%lu, pal_id:%lu, num_solvers:%lu", id, lc_pal_id, lc_num_solvers);
        exit(1);
    }
    if (!all) return;

    // locality of assigned IDs
    if (id % lc_num_solvers != lc_pal_id) {
        LOG_ERR("Learned clause has non local ID. ID:%lu, pal_id:%lu, num_solvers:%lu", id, lc_pal_id, lc_num_solvers);
        exit(1);
    }

    // Monotonicity of assigned IDs
    if (id < lc_max_derived_id) {
        LOG_ERR("Learned clause has lower ID that previously learned clause. newID:%lu, prevID:%lu", id, lc_max_derived_id);
        exit(1);
    }

    lc_max_derived_id = id;
}
static inline void parse_lits() {
    int_vec_resize(buf_lits, 0);
    while (true) {
        int lit = file_reader_read_vbl_int(proof);
        if (!lit) break;
        int_vec_push(buf_lits, lit);
    }
}

static void parse_lrup() {
    u64 id;
    while (true) {
        char c = file_reader_read_vbl_char(proof);
        //if (file_reader_eof_reached(proof)) {
        if (c == EOF) {
            finish_parse();
            break;

        } else if (c == TRUSTED_CHK_CLS_PRODUCE) {
            u64_vec_resize(buf_hints, 0);
            
            /*u64*/ id = (u64)file_reader_read_vbl_sl(proof);
            siphash_cls_update(clause_hash, (u8*)&id, sizeof(u64));

            check_id(id, true);
            parse_lits();
            siphash_cls_update(clause_hash, (u8*)buf_lits->data, buf_lits->size * sizeof(int));

            // parse hints
            while (true) {
                u64 hint = (u64)file_reader_read_vbl_sl(proof);
                if (!hint) break;
                u64_vec_push(buf_hints, hint);

                // if hint is imported clause => log clause
                clause_ptr c = hash_table_find(import_table, hint);
                if (c) {
                    import_handler_log(c);
                    hash_table_delete_last_found(import_table);
                    lc_stats.nb_imported_used++;
                }
            }

            //check IDs in hints
            if (!checker_utils_check_hints(id, buf_hints->data, buf_hints->size)) {
                LOG_ERR("Discoverd hint >= id in produced clause. ID:%lu", id);
                exit(1);
            }

            // forward to checker
            lrat_top_check_produce(id, buf_lits->data, buf_lits->size,
                                   buf_hints->data, buf_hints->size);
            lc_stats.nb_produced++;

        } else if (c == TRUSTED_CHK_CLS_IMPORT) {
            /*u64*/ id = (u64)file_reader_read_vbl_sl(proof);
            check_id(id, false);
            parse_lits();

            // forward to checker
            lrat_check_add_axiomatic_clause(id, buf_lits->data, buf_lits->size);
            lc_stats.nb_imported++;

            // hold to see if clause will be used
            clause_ptr c = create_flat_clause(id, buf_lits->size, buf_lits->data);
            if (!hash_table_insert(import_table, id, c)) {
                LOG_ERR("Could not insert clause of id %lu into hash table", id);
                abort();
            }

        } else if (c == TRUSTED_CHK_CLS_DELETE) {
            u64_vec_resize(buf_hints, 0);

            // parse hints
            while (true) {
                u64 hint = (u64)file_reader_read_vbl_sl(proof);
                if (!hint) break;
                u64_vec_push(buf_hints, hint);

                // imported clause was not used
                hash_table_delete(import_table, hint);
            }

            lrat_top_check_delete(buf_hints->data, buf_hints->size);
            lc_stats.nb_deleted += buf_hints->size;

        } else {
            //LOG_ERR("Invalid directive! c: %d filesize:%lu", c, proof->total_bytes);
            LOG_ERR("Invalid directive! c: %d", c);
            exit(1);
        }

        if (UNLIKELY(!lrat_top_check_valid())) {    
            LOG_ERR("Checker not valid anymore");
            exit(1);
        }
    }
}

static void parse_drup() {
    while (true) {
        char c = file_reader_read_vbl_char(proof);
        //if (file_reader_eof_reached(proof)) {
        if (c == EOF) {
            finish_parse();
            break;

        } else if (c == TRUSTED_CHK_CLS_PRODUCE) {
            u64 id = (u64)file_reader_read_vbl_sl(proof);
            siphash_cls_update(clause_hash, (u8*)&id, sizeof(u64));
            check_id(id, true);
            parse_lits();
            siphash_cls_update(clause_hash, (u8*)buf_lits->data, buf_lits->size * sizeof(int));

            #ifdef DRUP_TO_LRUP_CONVERSION
            // print out clause if it wasn't checked
            file_utils_write_vbl_char('a', lrup_out);
            file_utils_write_vbl_sl(id, lrup_out);
            for (u64 i = 0; i < buf_lits->size; i++)
                file_utils_write_vbl_int(buf_lits->data[i], lrup_out);
            file_utils_write_vbl_char(0, lrup_out);
            #endif

            // forward to checker
            drup_top_check_add(id, buf_lits->data, buf_lits->size);
            lc_stats.nb_produced++;

            #ifdef DRUP_TO_LRUP_CONVERSION
            //file_utils_write_vbl_char(0, lrup_out);
            #endif

        } else if (c == TRUSTED_CHK_CLS_IMPORT) {
            u64 id = (u64)file_reader_read_vbl_sl(proof);
            check_id(id, false);
            parse_lits();

            #ifdef DRUP_TO_LRUP_CONVERSION
            // print out clause if it wasn't checked
            file_utils_write_vbl_char('i', lrup_out);
            file_utils_write_vbl_sl(id, lrup_out);
            for (u64 i = 0; i < buf_lits->size; i++)
                file_utils_write_vbl_int(buf_lits->data[i], lrup_out);
            file_utils_write_vbl_char(0, lrup_out);
            #endif

            // forward to checker
            drup_check_add_axiomatic_clause(id, buf_lits->data, buf_lits->size, false);
            lc_stats.nb_imported++;

            clause_ptr c = create_flat_clause(id, buf_lits->size, buf_lits->data);
            import_handler_log(c);

        } else if (c == TRUSTED_CHK_CLS_DELETE) {
            parse_lits();
            //u64 id = drup_check_get_clause_id(buf_lits->data, buf_lits->size);

            // imported clause was not used
            // TODO: store ids somewhere?
            //hash_table_delete(import_table, id);
            drup_top_check_delete(buf_lits->data, buf_lits->size);
            lc_stats.nb_deleted++;

        } else {
            //LOG_ERR("Invalid directive! c: %d filesize:%lu", c, proof->total_bytes);
            LOG_ERR("Invalid directive! c: %d", c);
            exit(1);
        }

        if (UNLIKELY(!drup_top_check_valid())) {    
            LOG_ERR("Checker not valid anymore");
            exit(1);
        }
    }
}

void local_checker_init(struct options* options) {
    lc_num_solvers = options->num_solvers;
    lc_pal_id = options->pal_id;
    lc_drup = options->drup;
    palrup_binary = options->palrup_binary;
    lc_stats = local_checker_stats_init;
    clause_hash = siphash_cls_init(SECRET_KEY);
    unsigned int dir_hierarchy = options->pal_id / palrup_utils_calc_root_ceil(lc_num_solvers);
    snprintf(fragment_path, 512, "%s/%u/%lu/out.palrup", options->palrup_path, dir_hierarchy, options->pal_id);
    snprintf(working_path, 512, "%s", options->working_path);
    lc_stats = local_checker_stats_init;

    buf_lits = int_vec_init(1);
    buf_hints = u64_vec_init(1);
    import_table = hash_table_init(16);

    #ifdef DRUP_TO_LRUP_CONVERSION
        char lrup_out_path[750];
        snprintf(lrup_out_path, 750, "%s.extended", fragment_path);
        LOG("print extended proof fragment to %s", lrup_out_path);
        lrup_out = fopen(lrup_out_path, "wb");
    #endif

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
    lc_drup ? load_formula_drup(formula) : load_formula_lrat(formula);
    fclose(formula);

    import_handler_init(options);
}

int local_checker_run() {
    lc_drup ? parse_drup() : parse_lrup();
    
    if (lc_drup ? drup_top_check_unsat_found() : lrat_top_check_validate_unsat(NULL)) {
        char unsat_folder[525];
        snprintf(unsat_folder, 525, "%s/.unsat_found", working_path);
        if (mkdir(unsat_folder, 0777) == 0) {
            char unsat_folder_sub[1024];
            snprintf(unsat_folder_sub, 1024, "%s/%lu", unsat_folder, lc_pal_id);
            mkdir(unsat_folder_sub, 0777);
        }
    }
    LOG("rank:%lu prod:%lu imp:%lu imp_used:%lu del:%lu n_s:%lu",
        lc_pal_id, lc_stats.nb_produced, lc_stats.nb_imported, lc_stats.nb_imported_used, lc_stats.nb_deleted, lc_num_solvers);

    return 0;
}

void local_checker_end() {
    import_handler_end();
    int_vec_free(buf_lits);
    u64_vec_free(buf_hints);
    file_reader_end(proof);
    u8 sig[SIG_SIZE_BYTES];
    lc_drup ? drup_top_check_end(sig) : lrat_top_check_end();
    siphash_cls_free(clause_hash);
    hash_table_free(import_table);
}
