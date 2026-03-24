
#include <math.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>

#include "redist_handler.h"
#include "utils/palrup_utils.h"
#include "utils/checker_utils.h"
#include "comm_sig.h"
#include "import_merger.h"
#include "siphash_cls.h"
#include "write_buffer.h"

#define TYPE u8
#define TYPED(THING) u8_ ## THING
#include "vec.h"
#undef TYPED
#undef TYPE

// global values
u64 rh_num_solvers;
u64 rh_redist_strat;
u64 rh_pal_id;
size_t rh_msg_group_size_in;
size_t rh_msg_group_size_out;
const char* working_path;

// global buffer
struct u8_vec* write_buffer;
int* rh_current_literals_data;
u64 rh_current_literals_size;
u64 rh_current_ID = EMPTY_ID;
u64* rh_count_clauses;
FILE** out_files;
char** out_file_names;
char** in_file_paths;
struct siphash** out_hash;
struct comm_sig** comm_sig_compute;

static inline int get_out_file_id(u64 clause_id) {
    // might need to be altered for differing strategies
    return (clause_id % rh_num_solvers) % rh_msg_group_size_out;
}

static void init_msg_group_in() {
    comm_sig_compute = palrup_utils_malloc(sizeof(struct comm_sig*) * rh_msg_group_size_in);
    out_file_names = palrup_utils_calloc(rh_msg_group_size_in, sizeof(char*));
    in_file_paths = palrup_utils_calloc(rh_msg_group_size_in, sizeof(char*));
}

static void init_msg_group_out() {
    out_files = palrup_utils_malloc(sizeof(FILE*) * rh_msg_group_size_out);
    out_hash = palrup_utils_malloc(sizeof(struct siphash*) * rh_msg_group_size_out);
    rh_count_clauses = palrup_utils_calloc(rh_msg_group_size_out, sizeof(u64));
}

static void init_strat3() {
    rh_msg_group_size_in = palrup_utils_calc_root_ceil(rh_num_solvers);
    rh_msg_group_size_out = 1;
    init_msg_group_in();
    init_msg_group_out();

    out_file_names[0] = palrup_utils_calloc(1024, sizeof(char));
    snprintf(out_file_names[0], 512, "%s/%lu/%lu/out.palrup_import~",
             working_path, rh_pal_id / palrup_utils_calc_root_ceil(rh_num_solvers), rh_pal_id);
    out_files[0] = fopen(out_file_names[0], "wb");
    if (!out_files[0]) {
        snprintf(palrup_utils_msgstr, MSG_LEN, "Can not open out file at %s", out_file_names[0]);
        palrup_utils_log_err(palrup_utils_msgstr);
    }
    out_hash[0] = siphash_cls_init(SECRET_KEY);
    rh_count_clauses[0] = 0;
    
    unsigned int dir_hierarchy = rh_pal_id / rh_msg_group_size_in;
    int offset = dir_hierarchy * rh_msg_group_size_in;  // row number * pals in row
    for (size_t i = 0; i < rh_msg_group_size_in; i++) {
        comm_sig_compute[i] = comm_sig_init(SECRET_KEY_2);

        in_file_paths[i] = palrup_utils_calloc(512, sizeof(char));
        snprintf(in_file_paths[i], 512, "%s/%u/%lu/out.palrup_proxy", working_path, dir_hierarchy, offset + i);
    }
}

void redist_handler_init(struct options* options) {
    rh_num_solvers = options->num_solvers;
    rh_redist_strat = options->redist_strat;
    rh_pal_id = options->pal_id;
    working_path = options->working_path;

    write_buffer = u8_vec_init(options->write_buffer_size);

    switch (rh_redist_strat) {
    // TODO: implement strat 1 and 2
    case 1:
    case 2:
    case 3:
        init_strat3();
        break;
    default:
        snprintf(palrup_utils_msgstr, MSG_LEN, "Redistribution strategy %lu not available.", rh_redist_strat);
        palrup_utils_log_err(palrup_utils_msgstr);
        break;
    }

    import_merger_init(rh_msg_group_size_in, in_file_paths, &rh_current_ID, &rh_current_literals_data, &rh_current_literals_size, options->read_buffer_size, NULL, comm_sig_compute);
    write_buffer_init(options->write_buffer_size);
    write_buffer_swich_context(out_files[0]);
}

void redist_handler_run() {
    u64 column = rh_pal_id % rh_msg_group_size_in;
    while (true) {
        import_merger_next();

        int destination_index = get_out_file_id(rh_current_ID);
        if (UNLIKELY(rh_current_ID == EMPTY_ID)) break;

        // skip clauses for different columns
        if ((rh_current_ID % rh_num_solvers) % rh_msg_group_size_in != column)
            continue;

        siphash_cls_update(out_hash[destination_index], (u8*)&rh_current_ID, sizeof(u64));
        siphash_cls_update(out_hash[destination_index], (u8*)rh_current_literals_data, sizeof(int) * rh_current_literals_size);

        write_buffer_swich_context(out_files[destination_index]);
        write_buffer_add_clause(rh_current_ID, rh_current_literals_size, rh_current_literals_data);

        // TODO: print out?
        rh_count_clauses[destination_index] += 1;
    }
    snprintf(palrup_utils_msgstr, 512, "Done pal_id=%lu", rh_pal_id);
    palrup_utils_log(palrup_utils_msgstr);
}

void redist_handler_end() {
    // check signatures
    for (size_t i = 0; i < rh_msg_group_size_in; i++) {
        u8* computed_incoming_sig = comm_sig_digest(comm_sig_compute[i]);
        const u8 reported_incoming_sig[16];
        import_merger_read_sig((int*)reported_incoming_sig, i);
        if (!checker_utils_equal_signatures(reported_incoming_sig, computed_incoming_sig)) {
            snprintf(palrup_utils_msgstr, MSG_LEN, "Signature does not match in import! local rank: %lu\n", rh_pal_id);
            palrup_utils_log(palrup_utils_msgstr);
            snprintf(palrup_utils_msgstr, MSG_LEN, "Signature A is: %lu\n", *((u64*)computed_incoming_sig));
            palrup_utils_log(palrup_utils_msgstr);
            snprintf(palrup_utils_msgstr, MSG_LEN, "Signature B is: %lu\n", *((u64*)reported_incoming_sig));
            palrup_utils_log(palrup_utils_msgstr);
            abort();
        }
        free(computed_incoming_sig);
    }

    // wrap up out files
    write_buffer_end();
    for (size_t i = 0; i < rh_msg_group_size_out; i++) {
        u8* sig = siphash_cls_digest(out_hash[i]);
        palrup_utils_write_ul(0,out_files[i]);  // mark end of clauses
        palrup_utils_write_sig(sig, out_files[i]);
        fclose(out_files[i]);
        int new_str_len = strlen(out_file_names[i])-1;
        char new_filename[new_str_len];
        memcpy(new_filename, out_file_names[i], new_str_len);
        new_filename[new_str_len] = '\0';
        rename(out_file_names[i], new_filename);
    }
    
    // free msg_group in
    for (size_t i = 0; i < rh_msg_group_size_in; i++) {
        comm_sig_free(comm_sig_compute[i]);
        free(in_file_paths[i]);
    }

    // free msg_group out
    for (size_t i = 0; i < rh_msg_group_size_out; i++) {
        siphash_cls_free(out_hash[i]);
        free(out_file_names[i]);
    }

    free(out_hash);
    free(comm_sig_compute);
    free(rh_count_clauses);
    free(out_files);
    free(out_file_names);
    free(in_file_paths);
    u8_vec_free(write_buffer);
    import_merger_end();
}
