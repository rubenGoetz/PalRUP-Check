
#include <stdlib.h>
#include <stdio.h>

#include "redist_handler.h"
#include "utils/palrup_utils.h"

size_t rth_num_solvers;
size_t rth_msg_group_size_in;
size_t rth_msg_group_size_out;
size_t column;
char** in_file_names;
char** out_file_names;
FILE** out_files;

static inline int get_out_file_id(u64 clause_id) {
    // might need to be altered for differing strategies
    return (clause_id % rth_num_solvers) % rth_msg_group_size_out;
}

static inline void log_id(u64 id) {
    palrup_utils_write_ul(id, out_files[get_out_file_id(id)]);
}

static void init_msg_group() {
    in_file_names = palrup_utils_malloc(sizeof(char*) * rth_msg_group_size_in);
    out_file_names = palrup_utils_malloc(sizeof(char*) * rth_msg_group_size_out);
    out_files = palrup_utils_malloc(sizeof(FILE*) * rth_msg_group_size_out);
}

static void init_strat_3(struct options* options) {
    rth_msg_group_size_in = palrup_utils_calc_root_ceil(options->num_solvers);
    rth_msg_group_size_out = 1;
    init_msg_group();
    column = options->pal_id % rth_msg_group_size_in;
    size_t dir_hierarchy = options->pal_id / rth_msg_group_size_in;

    // open out files
    out_file_names[0] = palrup_utils_malloc(512 * sizeof(char));
    snprintf(out_file_names[0], 512, "%s/%lu/%lu/out.palrup_trim_import",
             options->working_path, dir_hierarchy, options->pal_id);
    char tmp_path[513];
    snprintf(tmp_path, 513, "%s~", out_file_names[0]);
    out_files[0] = fopen(tmp_path, "wb");
    if (!out_files[0]) {
        snprintf(palrup_utils_msgstr, MSG_LEN, "Could not open import file at %s", tmp_path);
        palrup_utils_log_err(palrup_utils_msgstr);
        abort();
    }

    size_t offset = dir_hierarchy * rth_msg_group_size_in;  // row number * pals in row
    for (size_t i = 0; i < rth_msg_group_size_in; i++) {
        // calc input file paths
        in_file_names[i] = palrup_utils_malloc(512 * sizeof(char));
        snprintf(in_file_names[i], 512, "%s/%lu/%lu/out.palrup_trim_proxy",
                 options->working_path, dir_hierarchy, offset + i);
    }
} 

void redist_trim_handler_init(struct options* options) {
    rth_num_solvers = options->num_solvers;

    switch (options->redist_strat) {
    // TODO: implement strat 1 and 2
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

void redist_trim_handler_run() {
    for (size_t i = 0; i < rth_msg_group_size_in; i++) {
        FILE* input = fopen(in_file_names[i], "rb");
        if (!input) {
            snprintf(palrup_utils_msgstr, MSG_LEN, "Could not open proxy file at %s", in_file_names[i]);
            palrup_utils_log_warn(palrup_utils_msgstr);
            continue;
        }
        snprintf(palrup_utils_msgstr, MSG_LEN, "parse file at %s", in_file_names[i]);
        palrup_utils_log(palrup_utils_msgstr);

        u64 id = 0;
        while (true) {
            // read id
            id = palrup_utils_read_ul(input);
            if (!id) break;
                
            // log id if it belongs in pals's column
            if ((id % rth_num_solvers) % rth_msg_group_size_in == column)
                log_id(id);
        }

        fclose(input);
    }
}

void redist_trim_handler_end() {
    for (size_t i = 0; i < rth_msg_group_size_in; i++)
        free(in_file_names[i]);
    
    for (size_t i = 0; i < rth_msg_group_size_out; i++) {
        palrup_utils_write_ul(0, out_files[0]);
        fclose(out_files[i]);
        char tmp_path[513];
        snprintf(tmp_path, 513, "%s~", out_file_names[i]);
        rename(tmp_path, out_file_names[i]);
        free(out_file_names[i]);
    }
    
    free(in_file_names);
    free(out_file_names);
    free(out_files);
}
