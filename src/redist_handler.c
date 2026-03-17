
//#include <assert.h>
#include <math.h>      // for sqrt
//#include <stdbool.h>   // for bool, true, false
#include <stdio.h>     // for fclose, fflush_unlocked, fopen, snprintf
#include <unistd.h>    // For fsync()
#include <stdlib.h>    // for free
#include <sys/stat.h>  // for mkdir
//#include <time.h>      // for clock, CLOCKS_PER_SEC, clock_t
//#include <unistd.h>    // for access
#include <string.h>

#include "redist_handler.h"
#include "utils/palrup_utils.h"
#include "utils/checker_utils.h"
//#include "checker_interface.h"
//#include "clause.h"
#include "comm_sig.h"
//#include "hash.h"
#include "import_merger.h"
//#include "plrat_checker.h"  // for palrup_utils_read_int, palrup_utils_log...
//#include "palrup_utils.h"
#include "secret.h"
#include "siphash_cls.h"
//#include "top_check.h"  // for top_check_commit_formula_sig, top_check_d...

#define TYPE u8
#define TYPED(THING) u8_ ## THING
#include "vec.h"
#undef TYPED
#undef TYPE

// Blocksize of underlying filesystem for more effizient writing.
#define BLOCKSIZE 16 * 1024

const char* out_path;  // named pipe
u64 n_solvers;         // number of solvers
double root_n;         // square root of number of solvers
size_t comm_size;
u64 redist_strat;  // redist_strategy
u64 local_rank;    // solver id
const u64 empty_ID = -1;

// Buffering.
struct u8_vec* write_buffer;
int* _re_current_literals_data;
u64 _re_current_literals_size;
u64 _re_current_ID = empty_ID;
u64* _re_count_clauses;
FILE** _bu_output_files;
char** file_names;
struct siphash** out_hash;
struct comm_sig** comm_sig_compute;

static inline size_t get_clause_size(int nb_literals) {
    return sizeof(u64) + sizeof(int) + nb_literals * sizeof(int);
}

static void write_buffer_to_file(FILE* file) {
    if (write_buffer->size == 0)
        return;

    u64 nb_written = UNLOCKED_IO(fwrite)(write_buffer->data, write_buffer->size, 1, file);
    if (nb_written < 1) palrup_utils_exit_eof();

    u8_vec_resize(write_buffer, 0);
}

void redist_handler_write_lrat_import_file(u64 clause_id, int* literals, int nb_literals, FILE* current_out) {
    if (redist_strat == 0) {
        fprintf(current_out, "%lu ", clause_id);
        fprintf(current_out, " ");
        fprintf(current_out, "n:%i", nb_literals);
        fprintf(current_out, " ");
        for (int i = 0; i < nb_literals; i++) {
            fprintf(current_out, "%i ", literals[i]);
        }
        fprintf(current_out, "%i", 0);
        fprintf(current_out, "\n");
    } else {
        if (write_buffer->size + get_clause_size(nb_literals) > write_buffer->capacity)
            write_buffer_to_file(current_out);

        memcpy(write_buffer->data + write_buffer->size, &clause_id, sizeof(u64));
        write_buffer->size += sizeof(u64);
        memcpy(write_buffer->data + write_buffer->size, &nb_literals, sizeof(int));
        write_buffer->size += sizeof(int);
        memcpy(write_buffer->data + write_buffer->size, literals, nb_literals * sizeof(int));
        write_buffer->size += nb_literals * sizeof(int);

        if (redist_strat != 3)
            write_buffer_to_file(current_out);
    }
}

void redist_handler_write_int(int value, FILE* current_out) {
    if (redist_strat == 0) {
        fprintf(current_out, "%i", value);
        fprintf(current_out, "\n");
    } else {
        palrup_utils_write_int(value, current_out);
    }
}

u64 redist_handler_get_destination_rank(size_t id) {
    u64 x = palrup_utils_rank_to_x(local_rank, comm_size);
    u64 y = palrup_utils_rank_to_y(id * comm_size, comm_size);
    return palrup_utils_2d_to_rank(x, y, comm_size);
}

void redist_handler_init(const char* working_path, unsigned long pal_id, unsigned long num_solvers, unsigned long redist_strategy, unsigned long read_buffer_size) {
    redist_strat = redist_strategy;
    n_solvers = num_solvers;
    root_n = sqrt((double)num_solvers);
    comm_size = (size_t)ceil(root_n);  // round to nearest integer
    if (redist_strat == 1) {
        comm_size = n_solvers;
    }
    out_path = working_path;
    local_rank = pal_id;
    unsigned int dir_hierarchy = local_rank / comm_size;
    _bu_output_files = palrup_utils_malloc(sizeof(FILE*) * comm_size);
    file_names = palrup_utils_calloc(comm_size, sizeof(char*));
    _re_count_clauses = palrup_utils_calloc(comm_size, sizeof(u64));
    out_hash = palrup_utils_malloc(sizeof(struct siphash*) * comm_size);
    comm_sig_compute = palrup_utils_malloc(sizeof(struct comm_sig*) * comm_size);
    write_buffer = u8_vec_init(BLOCKSIZE);

    snprintf(palrup_utils_msgstr, 512, "root_n:%f", root_n);
    if (local_rank == 0) palrup_utils_log(palrup_utils_msgstr);
    if (redist_strat == 3) {
        char tmp_path[1024];
        snprintf(tmp_path, 512, "%s/%lu/%lu/out.palrup_import~", out_path, pal_id / comm_size, pal_id);
        FILE* out_file = fopen(tmp_path, "wb");
        struct siphash* single_out_hash = siphash_cls_init(SECRET_KEY);
        for (size_t i = 0; i < comm_size; i++) {
            file_names[i] = palrup_utils_calloc(1024, sizeof(char));
            memcpy(file_names[i], tmp_path, 1024);
            _bu_output_files[i] = out_file;
            out_hash[i] = single_out_hash;
            comm_sig_compute[i] = comm_sig_init(SECRET_KEY_2);
        }
    } else {
        for (size_t i = 0; i < comm_size; i++) {
            char folder_path[512];
            char tmp_path[1024];

            u64 dest_rank = redist_handler_get_destination_rank(i);
            snprintf(folder_path, 512, "%s/%lu/%lu", out_path, dest_rank / comm_size, dest_rank);
            mkdir(folder_path, 0755);
            snprintf(tmp_path, 1024, "%s/%lu.palrup_import~", folder_path, palrup_utils_rank_to_y(local_rank, comm_size));
            file_names[i] = palrup_utils_calloc(1024, sizeof(char));
            memcpy(file_names[i], tmp_path, 1024);
            _bu_output_files[i] = fopen(tmp_path, "wb");

            if (!(_bu_output_files[i])) palrup_utils_exit_eof();

            out_hash[i] = siphash_cls_init(SECRET_KEY);
            comm_sig_compute[i] = comm_sig_init(SECRET_KEY_2);
        }
    }

    // signature for empty files. Avoids unnecessary error logs.
    struct comm_sig* dummy_sig = comm_sig_init(SECRET_KEY_2);
    u8* sig = comm_sig_digest(dummy_sig);
    char** file_paths = palrup_utils_malloc(sizeof(char*) * comm_size);
    int offset = (local_rank / comm_size) * comm_size;  // row number * pals in row
    for (size_t i = 0; i < comm_size; i++) {
        file_paths[i] = palrup_utils_malloc(512);
        if (redist_strat == 3)
            snprintf(file_paths[i], 512, "%s/%u/%lu/out.palrup_proxy", out_path, dir_hierarchy, offset + i);
        else
            snprintf(file_paths[i], 512, "%s/%u/%lu/%lu.palrup_proxy", out_path, dir_hierarchy, local_rank, i);
        if (access(file_paths[i], F_OK) != 0) {
            // file doesn't exist
            // create placeholder file containing only 0
            FILE* f = fopen(file_paths[i], "wb");
            palrup_utils_write_ul(0, f);      // write EOF flag
            palrup_utils_write_sig(sig, f);    // write placeholder signature
            fclose(f);
        }
    }
    free(sig);
    comm_sig_free(dummy_sig);
    import_merger_init(comm_size, file_paths, &_re_current_ID, &_re_current_literals_data, &_re_current_literals_size, read_buffer_size, NULL, comm_sig_compute);

    // free
    for (size_t i = 0; i < comm_size; i++) {
        free(file_paths[i]);
    }
    free(file_paths);
}

//int compare_clause(const void* a, const void* b) {
//    u64 id_a = ((struct clause*)a)->id;
//    u64 id_b = ((struct clause*)b)->id;
//    return (id_a - id_b);
//}

void redist_handler_end() {
    for (size_t i = 0; i < comm_size; i++) {
        u8* computed_incoming_sig = comm_sig_digest(comm_sig_compute[i]);
        const u8 reported_incoming_sig[16];
        import_merger_read_sig((int*)reported_incoming_sig, i);
        if (!checker_utils_equal_signatures(reported_incoming_sig, computed_incoming_sig)) {

            printf("Signature does not match in import! local rank: %lu\n", local_rank);
            printf("Signature A is: %lu\n", *((u64*)computed_incoming_sig));
            printf("Signature B is: %lu\n", *((u64*)reported_incoming_sig));
        } else {
            char msg[512];
            snprintf(msg, 512, "Signature matches in import local rank: %lu", local_rank);
            //palrup_utils_log(msg);
        }
        free(computed_incoming_sig);
        comm_sig_free(comm_sig_compute[i]);
    }

    size_t effective_comm_size = comm_size;
    if (redist_strat == 3) // heaps/files/sigs all point to the same objects
        effective_comm_size = 1;
    
    for (size_t i = 0; i < effective_comm_size; i++) {
        write_buffer_to_file(_bu_output_files[i]);
        u8* sig = siphash_cls_digest(out_hash[i]);
        palrup_utils_write_ul(0,_bu_output_files[i]);  // mark end of clauses
        palrup_utils_write_sig(sig, _bu_output_files[i]);
        fclose(_bu_output_files[i]);
        int new_str_len = strlen(file_names[i])-1;
        char new_filename[new_str_len];
        memcpy(new_filename, file_names[i], new_str_len);
        new_filename[new_str_len] = '\0';
        rename(file_names[i], new_filename);
        siphash_cls_free(out_hash[i]);
    }

    for (size_t i = 0; i < comm_size; i++)
        free(file_names[i]);

    free(out_hash);
    free(comm_sig_compute);
    free(_re_count_clauses);
    free(_bu_output_files);
    free(file_names);
    u8_vec_free(write_buffer);
    import_merger_end();
}

void redist_handler_run() {
    u64 column = local_rank % comm_size;
    while (true) {
        import_merger_next();

        int destination_index = palrup_utils_rank_to_y(_re_current_ID % n_solvers, comm_size);
        if (UNLIKELY(_re_current_ID == empty_ID)) break;

        // skip clauses for different columns
        if ((_re_current_ID % n_solvers) % comm_size != column)
            continue;

        siphash_cls_update(out_hash[destination_index], (u8*)&_re_current_ID, sizeof(u64));
        siphash_cls_update(out_hash[destination_index], (u8*)_re_current_literals_data, sizeof(int) * _re_current_literals_size);

        redist_handler_write_lrat_import_file(
            _re_current_ID,
            _re_current_literals_data,
            _re_current_literals_size,
            _bu_output_files[destination_index]);

        _re_count_clauses[destination_index] += 1;
    }
    snprintf(palrup_utils_msgstr, 512, "Done local_rank=%lu", local_rank);
    palrup_utils_log(palrup_utils_msgstr);
}