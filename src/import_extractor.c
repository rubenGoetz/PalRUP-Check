
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "import_extractor.h"
#include "utils/palrup_utils.h"
#include "file_reader.h"
#include "hash.h"

struct import_extractor_stats {
    u64 nb_produced;
    u64 nb_imported;
    u64 nb_imported_used;
    u64 nb_deleted;
} ie_stats = {0, 0, 0, 0};

#define SENTINEL (void*)1

size_t ie_pal_id;
size_t ie_msg_group_size;
size_t ie_num_solvers;
struct file_reader* proof_fragment;
char** file_names;
FILE** out_files;
struct hash_table* import_table;    // TODO: make AMQ?
char* unsat_folder;

static inline int get_file_id(u64 clause_id) {
    // might need to be altered for differing strategies
    return (clause_id % ie_num_solvers) % ie_msg_group_size;
}

static inline void log_id(u64 id) {
    palrup_utils_write_ul(id, out_files[get_file_id(id)]);
}

static void empty_clause_found() {
    mkdir(unsat_folder, 0777);
    char unsat_folder_sub[1024];
    snprintf(unsat_folder_sub, 1024, "%s/%lu", unsat_folder, ie_pal_id);
    if (mkdir(unsat_folder_sub, 0777)) {
        snprintf(palrup_utils_msgstr, MSG_LEN, "Could not create unsat flag.");
        palrup_utils_log_err(palrup_utils_msgstr);
    }
}

static void print_stats() {
    char msg[512];
    snprintf(msg, 512, "import_extractor_stats nb_produced:%lu, nb_imported:%lu, nb_imported_used:%lu, nb_deleted:%lu", ie_stats.nb_produced, ie_stats.nb_imported, ie_stats.nb_imported_used, ie_stats.nb_deleted);
    palrup_utils_log(msg);
}

static void init_strat_3(struct options* options) {
    ie_msg_group_size = 1;
    file_names = palrup_utils_malloc(sizeof(char*) * ie_msg_group_size);
    out_files = palrup_utils_malloc(sizeof(FILE*) * ie_msg_group_size);
    int dir_hierarchy = options->pal_id / palrup_utils_calc_root_ceil(ie_num_solvers);
    file_names[0] = palrup_utils_malloc(511 * sizeof(char));
    snprintf(file_names[0], 511, "%s/%u/%lu/out.palrup_trim_proxy",
             options->working_path, dir_hierarchy, options->pal_id);
    char file_name[512];
    snprintf(file_name, 512, "%s~", file_names[0]);
    out_files[0] = fopen(file_name, "wb");
    if (!(out_files[0])) {
        snprintf(palrup_utils_msgstr, MSG_LEN, "Could not create file at %s\n", file_name);
        palrup_utils_log_err(palrup_utils_msgstr);
    }
}

void import_extractor_init(struct options* options) {
    ie_num_solvers = options->num_solvers;
    ie_pal_id = options->pal_id;

    // open proof fragment
    char frag_path[1024];
    snprintf(frag_path, 1024, "%s/%lu/%lu/out.palrup",
             options->palrup_path,
             options->pal_id / palrup_utils_calc_root_ceil(ie_num_solvers),
             options->pal_id);
    proof_fragment = file_reader_init(options->read_buffer_size, fopen(frag_path, "rb"), options->pal_id);
    
    // init global data structures
    import_table = hash_table_init(16);

    // init flag for if empty clause was found 
    unsat_folder = palrup_utils_malloc(512 * sizeof(char));
    snprintf(unsat_folder, 512, "%s/.unsat_found", options->working_path);

    // init strat specific data structures
    switch (options->redist_strat) {
    case 1:
    case 2:
    case 3:
        init_strat_3(options);
        break;
    default:
        snprintf(palrup_utils_msgstr, MSG_LEN, "Redistribution strategy %lu not available.", options->redist_strat);
        palrup_utils_log_err(palrup_utils_msgstr);
        break;
    }
}

void import_extractor_run() {
    // parse proof fragment
    while (true) {
        char c = file_reader_read_vbl_char(proof_fragment);
        if (file_reader_eof_reached(proof_fragment))
            break;

        switch (c) {
        case TRUSTED_CHK_CLS_DELETE:
            ie_stats.nb_deleted++;
            file_reader_read_vbl_sl(proof_fragment);    // skip id
            while (true) {    // skip hints
                u64 hint = file_reader_read_vbl_sl(proof_fragment);
                if (!hint)
                    break;
                hash_table_delete(import_table, hint);
            }
            continue;
        
        case TRUSTED_CHK_CLS_IMPORT:
            ie_stats.nb_imported++;
            hash_table_insert(import_table, file_reader_read_vbl_sl(proof_fragment), SENTINEL);
            while (true)    // skip lits
                if (!file_reader_read_vbl_int(proof_fragment))
                    break;
            continue;

        case TRUSTED_CHK_CLS_PRODUCE:
            ie_stats.nb_produced++;
            file_reader_read_vbl_sl(proof_fragment);    // skip id

            // skip lits
            if (!file_reader_read_vbl_int(proof_fragment))
                empty_clause_found();
            else while (file_reader_read_vbl_int(proof_fragment));

            // check hints
            while (true) {
                u64 hint = file_reader_read_vbl_sl(proof_fragment);
                if (!hint)
                    break;
                if (hash_table_find(import_table, hint)) {
                    hash_table_delete_last_found(import_table);
                    log_id(hint);
                }
            }
            continue;

        default:
            snprintf(palrup_utils_msgstr, MSG_LEN, "Unknown directive %c", c);
            palrup_utils_log_err(palrup_utils_msgstr);
            abort();
        }
    }
}

void import_extractor_end() {
    file_reader_end(proof_fragment);
    
    for (size_t i = 0; i < ie_msg_group_size; i++) {
        // rename out files
        palrup_utils_write_ul(0, out_files[0]);
        fclose(out_files[i]);
        char old_name[1025];
        snprintf(old_name, 1025, "%s~", file_names[i]);
        rename(old_name, file_names[i]);
        free(file_names[i]);
    }
        
    free(file_names);
    hash_table_light_free(import_table);
    print_stats();
}
