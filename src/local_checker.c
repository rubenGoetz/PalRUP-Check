
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <assert.h>
#include <signal.h>
#include <time.h>

#include "local_checker.h"
#include "file_reader.h"
#include "utils/palrup_utils.h"
#include "utils/checker_utils.h"
#include "lrat_check.h"
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

// ----- Definitions for DRUP to LRUP conversion -----
#define WRITE_ADDITION
#define WRITE_HINTS
#define WRITE_IMPORT
#define WRITE_DELETIONS
#define LOG_IMPORT(C) import_handler_log(C)

#ifdef DRUP_TO_LRUP_CONVERSION

#include "file_writer.h"
file_writer* lrup_out;
char* lrup_file_path;
int convert_to_lrup;

// Feels a bit hacky but effectively cleans up interfaces
extern struct u64_vec* hints;
extern struct u64_vec* deletions;

#define WRITE_SL(X) file_writer_vbl_sl(lrup_out, X)
#define WRITE_INT(X) file_writer_vbl_int(lrup_out, X)
#define WRITE_CHAR(X) file_writer_vbl_char(lrup_out, X)

#define WRITE_LIT_BUFFER do {   \
        WRITE_SL(id);   \
        for (u64 i = 0; i < buf_lits->size; i++)    \
            WRITE_INT(buf_lits->data[i]);   \
        WRITE_CHAR(0);  \
    } while (0)

#undef WRITE_ADDITION
#define WRITE_ADDITION do { \
        WRITE_CHAR('a');    \
        WRITE_LIT_BUFFER;   \
    } while (0)

#undef WRITE_HINTS
#define WRITE_HINTS do {    \
        for (long i = hints->size - 1; i >= 0; i--)   \
            WRITE_SL(hints->data[i]);   \
        WRITE_CHAR(0);  \
    } while (0)

#undef WRITE_IMPORT
#define WRITE_IMPORT do { \
        WRITE_CHAR('i');    \
        WRITE_LIT_BUFFER;   \
    } while (0)

#undef WRITE_DELETIONS
#define WRITE_DELETIONS do {    \
        if (!deletions->size) break;    \
        WRITE_CHAR('d');    \
        for (u64 i = 0; i < deletions->size; i++)   \
            WRITE_SL(deletions->data[i]);   \
        WRITE_CHAR(0);  \
        deletions->size = 0;    \
    } while (0)

#undef LOG_IMPORT
#define LOG_IMPORT(C) if (convert_to_lrup < 2) import_handler_log(C)

#endif
// ---------------------------------------------------

bool _initialized = false;

struct local_checker_stats {
    u64 nb_produced;
    u64 nb_imported;
    u64 nb_imported_used;
    u64 nb_deleted;
    u64 nb_lines_parsed;
} local_checker_stats_init = {0, 0, 0, 0, 0};

// formula
long nb_clauses;

// global values
u64 lc_num_solvers;
u64 lc_pal_id;
u64 lc_max_derived_id = 0;
u64 lc_unsat_id = (u64)-1;
bool lc_drup;
bool palrup_binary;
char fragment_path[512];
char working_path[512];
char unsat_folder[525];
char unsat_details[533];
struct file_reader* proof;
struct siphash* clause_hash;
struct local_checker_stats lc_stats;

// global buffers.
struct int_vec* buf_lits;
struct u64_vec* buf_hints;
struct hash_table* import_table;

volatile bool poll = false;
struct sigaction lc_sa = {0};
struct sigevent lc_sev = {0};
struct itimerspec lc_its = {0};
timer_t lc_timer;
static void timer_handler(int sig, siginfo_t *si, void *uc) {
    (void)sig;
    (void)si;
    (void)uc;
    poll = true;
}

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

    LOG("Start reading the formula file: cnf %i %li", nb_vars, nb_clauses);
    return nb_vars;
}

static void load_formula_lrat(FILE* formula) {
    int nb_vars = parse_header(formula);
    int tmp = 0;

    lrat_top_check_init(nb_vars, false, true);
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

    // TODO: document
    // encodes the line number to which the fragment was parsed and thus the hash calculated
    palrup_utils_write_ul(lc_stats.nb_lines_parsed, finger_print);
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
        LOG_ERR("Learned clause has ID lower than previously learned clause. newID:%lu, prevID:%lu", id, lc_max_derived_id);
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
static inline void poll_unsat() {
    if (!poll) return;                                          // only poll in time intervals
    if (lc_unsat_id != (u64)-1) { poll = false; return; }       // ID already parsed
    if (access(unsat_details, R_OK)) { poll = false; return; }  // File can not be read
    FILE* details = fopen(unsat_details, "rb");
    if(!details) {                                              // Error opening the file
        LOG_ERR("Could not read file at %s", unsat_details);
        poll = false;
        return;
    }
    lc_unsat_id = palrup_utils_read_ul(details);
    LOG("Received ID of empty clause: %lu", lc_unsat_id);
    fclose(details);
    
    // disarm timer
    lc_its.it_value.tv_sec = 0;
    lc_its.it_value.tv_nsec = 0;
    if (timer_settime(lc_timer, 0, &lc_its, NULL) == -1)
        LOG_ERR("Could not disarm timer");

    poll = false;
}

static void parse_lrup() {
    u64 id;
    while (true) {
        char c = file_reader_read_vbl_char(proof);
        if (file_reader_eof_reached(proof)) {
            finish_parse();
            break;

        } else if (c == TRUSTED_CHK_CLS_PRODUCE) {
            u64_vec_resize(buf_hints, 0);
            
            id = (u64)file_reader_read_vbl_sl(proof);
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
            buf_lits->size = checker_utils_remove_duplicates(buf_lits->data, buf_lits->size);
            lrat_top_check_produce(id, buf_lits->data, buf_lits->size,
                                   buf_hints->data, buf_hints->size);
            lc_stats.nb_produced++;

        } else if (c == TRUSTED_CHK_CLS_IMPORT) {
            id = (u64)file_reader_read_vbl_sl(proof);
            check_id(id, false);
            parse_lits();

            // forward to checker
            buf_lits->size = checker_utils_remove_duplicates(buf_lits->data, buf_lits->size);
            lrat_check_add_axiomatic_clause(id, buf_lits->data, buf_lits->size);
            lc_stats.nb_imported++;

            // hold to see if clause will be used
            clause_ptr c = create_flat_clause(id, buf_lits->size, buf_lits->data);
            if (!hash_table_insert(import_table, id, c)) {
                LOG_ERR("Could not insert clause of id %lu into hash table", id);
                fflush(stdout);
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
            LOG_ERR("Invalid directive! c: %d", c);
            exit(1);
        }

        lc_stats.nb_lines_parsed++;
        if (UNLIKELY(!lrat_top_check_valid())) {    
            LOG_ERR("Checker not valid anymore");
            exit(1);
        }
    }
}

static void parse_drup() {
    u64 id = 0;
    while (true) {
        poll_unsat();
        char c = file_reader_read_vbl_char(proof);
        if (file_reader_eof_reached(proof) | (id > lc_unsat_id)) {
            if (id > lc_unsat_id)
                LOG("Halt check for IDs > empty clause: %lu", lc_unsat_id);
            finish_parse();
            break;

        } else if (c == TRUSTED_CHK_CLS_PRODUCE) {
            id = (u64)file_reader_read_vbl_sl(proof);
            siphash_cls_update(clause_hash, (u8*)&id, sizeof(u64));
            check_id(id, true);
            parse_lits();
            siphash_cls_update(clause_hash, (u8*)buf_lits->data, buf_lits->size * sizeof(int));

            WRITE_DELETIONS;
            WRITE_ADDITION;

            // forward to checker
            drup_top_check_add(id, buf_lits->data, buf_lits->size);
            lc_stats.nb_produced++;

            WRITE_HINTS;

        } else if (c == TRUSTED_CHK_CLS_IMPORT) {
            id = (u64)file_reader_read_vbl_sl(proof);
            check_id(id, false);
            parse_lits();

            WRITE_IMPORT;

            // forward to checker
            drup_top_check_import(id, buf_lits->data, buf_lits->size);
            lc_stats.nb_imported++;

            clause_ptr c = create_flat_clause(id, buf_lits->size, buf_lits->data);
            LOG_IMPORT(c);

        } else if (c == TRUSTED_CHK_CLS_DELETE) {
            parse_lits();
            if (!drup_top_check_delete(buf_lits->data, buf_lits->size))
                LOG_ERR("To be deteted clause was not found");
            lc_stats.nb_deleted++;

        } else {
            LOG_ERR("Invalid directive! c: %d", c);
            exit(1);
        }

        lc_stats.nb_lines_parsed++;
        if (UNLIKELY(!drup_top_check_valid())) {    
            LOG_ERR("Checker not valid anymore");
            exit(1);
        }
    }
}

void local_checker_init(struct options* options) {
    if (_initialized) return;
    
    lc_num_solvers = options->num_solvers;
    lc_pal_id = options->pal_id;
    lc_drup = options->drup;
    palrup_binary = options->palrup_binary;
    lc_stats = local_checker_stats_init;
    lc_unsat_id = (u64)-1;
    clause_hash = siphash_cls_init(SECRET_KEY);
    unsigned int dir_hierarchy = options->pal_id / palrup_utils_calc_root_ceil(lc_num_solvers);
    snprintf(fragment_path, 512, "%s/%u/%lu/%s",
             options->palrup_path, dir_hierarchy, options->pal_id,
             lc_drup ? DRUP_FRAGMENT_NAME : LRUP_FRAGMENT_NAME);
    snprintf(working_path, 512, "%s", options->working_path);
    lc_stats = local_checker_stats_init;
    snprintf(unsat_folder, 525, "%s/.unsat_found", working_path);
    snprintf(unsat_details, 533, "%s/details", unsat_folder);

    buf_lits = int_vec_init(1);
    buf_hints = u64_vec_init(1);
    import_table = hash_table_init(16);

    #ifdef DRUP_TO_LRUP_CONVERSION
    convert_to_lrup = 0;
    FILE* lrup_file;
    if (options->convert_to_lrup && options->drup) {
        convert_to_lrup = options->convert_to_lrup;     // only set convert_to_lrup if drup is also set
        lrup_file_path = palrup_utils_malloc(750);
        snprintf(lrup_file_path, 750, "%s/%u/%lu/%s~",
                 options->palrup_path, dir_hierarchy, options->pal_id, LRUP_FRAGMENT_NAME);
        LOG("print extended proof fragment to %s", lrup_file_path);
        lrup_file = fopen(lrup_file_path, "wb");
    } else {
        lrup_file_path = NULL;
        lrup_file = NULL;
    }
    lrup_out = file_writer_init(lrup_file, options->write_buffer_size);
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

    #ifdef DRUP_TO_LRUP_CONVERSION
        if (convert_to_lrup < 2)    // only init import_handler if we actually log imports
            import_handler_init(options);
    #else
        import_handler_init(options);
    #endif

    // setup poll timer if necessary
    if (lc_drup) {
        poll = false;

        // create signal action
        lc_sa.sa_sigaction = timer_handler;
        sigemptyset(&lc_sa.sa_mask);
        if (sigaction(SIGRTMIN, &lc_sa, NULL) == -1)
            LOG_ERR("Could not create polling signal handler");

        // create signal event
        lc_sev.sigev_notify = SIGEV_SIGNAL;
        lc_sev.sigev_signo = SIGRTMIN;
        lc_sev.sigev_value.sival_ptr = &lc_timer;

        // create timer
        if (timer_create(CLOCK_THREAD_CPUTIME_ID, &lc_sev, &lc_timer) == -1)
            LOG_ERR("Could not create timer");
        
        // arm timer
        lc_its.it_value.tv_sec = 0.5;
        lc_its.it_value.tv_nsec = 500000000;
        lc_its.it_interval.tv_sec = lc_its.it_value.tv_sec;
        lc_its.it_interval.tv_nsec = lc_its.it_value.tv_nsec;
        if (timer_settime(lc_timer, 0, &lc_its, NULL) == -1)
            LOG_ERR("Could not arm timer");
    }

    _initialized = true;
}

int local_checker_run() {
    if (!_initialized) return 1;

    lc_drup ? parse_drup() : parse_lrup();
    
    if (lc_drup ? drup_top_check_unsat_found() : lrat_top_check_validate_unsat(NULL)) {
        u64 empty_id = lc_drup ? drup_top_check_empty_clause_id() : lrat_top_check_empty_clause_id();
        assert(empty_id);
        LOG("Found empty clause of ID %lu", empty_id);
        if (mkdir(unsat_folder, 0777))
            LOG_WARN("Could not create dir %s", unsat_folder);  // dir might already have been created by another pal

        FILE* details = fopen(unsat_details, "ab");
        if (details) {
            // TODO: document this
            palrup_utils_write_ul(empty_id, details);
            fclose(details);
        } else LOG_ERR("Could not open file %s", unsat_details);
    }
    LOG("rank:%lu prod:%lu imp:%lu imp_used:%lu del:%lu lines_parsed:%lu n_s:%lu",
        lc_pal_id, lc_stats.nb_produced, lc_stats.nb_imported, lc_stats.nb_imported_used, lc_stats.nb_deleted, lc_stats.nb_lines_parsed, lc_num_solvers);

    return 0;
}

void local_checker_end() {
    if (!_initialized) return;
    #ifdef DRUP_TO_LRUP_CONVERSION
    if (convert_to_lrup < 2)
        import_handler_end();
    file_writer_free(lrup_out);
    if (lrup_file_path) {
        // mark lrup file as finished
        int new_str_len = strlen(lrup_file_path)-1;
        char new_filename[new_str_len];
        memcpy(new_filename, lrup_file_path, new_str_len);
        new_filename[new_str_len] = '\0';
        rename(lrup_file_path, new_filename);
        free(lrup_file_path);
    }
    #else
    import_handler_end();
    #endif
    int_vec_free(buf_lits);
    u64_vec_free(buf_hints);
    file_reader_end(proof);
    u8 sig[SIG_SIZE_BYTES];
    lc_drup ? drup_top_check_end(sig) : lrat_top_check_end();
    siphash_cls_free(clause_hash);
    hash_table_free(import_table);
    lc_unsat_id = (u64)-1;
    poll = false;
    if (lc_drup) timer_delete(lc_timer);
    _initialized = false;
}
