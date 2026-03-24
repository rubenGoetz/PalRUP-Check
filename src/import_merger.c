
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "import_merger.h"
#include "utils/palrup_utils.h"
#include "utils/checker_utils.h"
#include "file_reader.h"

#define TYPE int
#define TYPED(THING) int_##THING
#include "vec.h"
#undef TYPED
#undef TYPE

struct merger_stats {
    u64 nb_read;
    u64 nb_duplicates;
} merger_stats_init = {0, 0};
struct merger_stats stats;

size_t _im_n_files;
bool* im_clauses_left;
u64* im_clause_ids;
struct int_vec** im_all_lits;
struct file_reader** im_import_files;
struct siphash** im_check_hash;
struct comm_sig** im_check_comm_sig;
int last_index_to_load = 0;

// Buffering.
u64* im_current_id;  // is -1 if no clause is available (end of files)
int** im_current_literals_data;
u64* im_current_literals_size;

void read_literals_index(int index, int nb_lits) {
    struct int_vec* buf_lits = im_all_lits[index];
    int_vec_resize(buf_lits, nb_lits);
    file_reader_read_ints(buf_lits->data, nb_lits, im_import_files[index]);
}

void load_clause_if_available(int index) {
    if (LIKELY(im_clauses_left[index])) {
        struct file_reader* file = im_import_files[index];
        im_clause_ids[index] = file_reader_read_ul(file);
        // no clauses left
        if (im_clause_ids[index] == 0) {
            im_clause_ids[index] = -1;
            return;
        }
        int nb_lits = file_reader_read_int(file);
        read_literals_index(index, nb_lits);
        if (im_check_hash != NULL) {
            siphash_cls_update(im_check_hash[index], (const u8*)&im_clause_ids[index], sizeof(u64));
            siphash_cls_update(im_check_hash[index], (const u8*)im_all_lits[index]->data, im_all_lits[index]->size * sizeof(int));
        } else if (im_check_comm_sig != NULL) {
            comm_sig_update_clause(im_check_comm_sig[index], im_clause_ids[index], im_all_lits[index]->data, nb_lits);
        }
        stats.nb_read++;
    } else {
        im_clause_ids[index] = -1;
    }
}

void copy_lits(int* dest, int* src, int nb_lits) {
    for (int i = 0; i < nb_lits; i++) {
        dest[i] = src[i];
    }
}

void import_merger_init(int count_input_files, char** file_paths, u64* current_id, int** current_literals_data, u64* current_literals_size, u64 read_buffer_size, struct siphash** import_check_hash, struct comm_sig** comm_sig_compute) {
    _im_n_files = count_input_files;
    im_check_hash = import_check_hash;
    im_check_comm_sig = comm_sig_compute;
    im_current_id = current_id;              // output location
    im_current_literals_data = current_literals_data;  // output location
    im_current_literals_size = current_literals_size;  // output location
    im_clause_ids = palrup_utils_malloc(sizeof(u64) * _im_n_files);
    im_all_lits = palrup_utils_malloc(sizeof(struct int_vec*) * _im_n_files);
    im_clauses_left = palrup_utils_malloc(sizeof(bool) * _im_n_files);
    im_import_files = palrup_utils_calloc(_im_n_files, sizeof(struct file_reader*));
    stats = merger_stats_init;
    for (size_t i = 0; i < _im_n_files; i++) {
        if (access(file_paths[i], F_OK) == 0) {   
            im_import_files[i] = file_reader_init(read_buffer_size, fopen(file_paths[i], "rb"), -1);
            if (!(im_import_files[i]))
                palrup_utils_exit_eof();
            im_all_lits[i] = int_vec_init(1);
            im_clauses_left[i] = true;
        } else {
            // create sentinels for missing files
            im_clauses_left[i] = false;
            im_clause_ids[i] = -1;
        }
        
    }
    // load the first clause of each file exept for 0
    // that way import_merger_next does not need an if statement at start (if "last_index_to_load is invalid")
    for (size_t i = 1; i < _im_n_files; i++) {
        load_clause_if_available(i);
    }
}

static void print_stats() {
    char msg[512];
    snprintf(msg, 512, "merger_stats nb_read:%lu, nb_duplicates:%lu", stats.nb_read, stats.nb_duplicates);
    palrup_utils_log(msg);
}

void import_merger_end() {
    for (size_t i = 0; i < _im_n_files; i++) {
        if (im_import_files[i]) {
            file_reader_end(im_import_files[i]);
            int_vec_free(im_all_lits[i]);
        }
    }

    free(im_import_files);
    free(im_all_lits);
    free(im_clause_ids);
    free(im_clauses_left);
    print_stats();
}

void import_merger_next() {
    load_clause_if_available(last_index_to_load);  
    bool imports_left = false;
    u64 current_id = EMPTY_ID;
    *im_current_id = EMPTY_ID;
    size_t index_to_load = -1;
    struct int_vec candidate_lits;
    // find the smallest clause id
    for (size_t i = 0; i < _im_n_files; i++) {
        u64 temp_id = im_clause_ids[i];
        

        if (temp_id < current_id && temp_id != EMPTY_ID) {
            current_id = temp_id;
            index_to_load = i;
            candidate_lits = *im_all_lits[index_to_load];
            imports_left = true;
        } else if (temp_id == current_id && current_id != EMPTY_ID) { // check and skip duplicates
            candidate_lits = *im_all_lits[index_to_load];
            const struct int_vec* temp_lits = im_all_lits[i];
            if (UNLIKELY(!checker_utils_compare_lits(candidate_lits.data, temp_lits->data, candidate_lits.size, temp_lits->size))) {
                char err_str[512];
                snprintf(err_str, 512, "literals do not match \nID:%lu index_to_load:%lu i:%lu", current_id, index_to_load, i);
                palrup_utils_log_err(err_str);
                exit(1);
            }
            stats.nb_duplicates++;
            load_clause_if_available(i);
        }
    }

    if (!imports_left) {
        return;
    }
    // load the lits
    *im_current_literals_size = candidate_lits.size;
    *im_current_literals_data = candidate_lits.data;
    *im_current_id = current_id;
    last_index_to_load = index_to_load;
}

void import_merger_read_sig(int* sig_res_reported, int index) {
    if (im_import_files[index])
        file_reader_read_ints(sig_res_reported, 4, im_import_files[index]);
    else {
        struct comm_sig* dummy_sig = comm_sig_init(SECRET_KEY_2);
        u8* sig = comm_sig_digest(dummy_sig);
        memcpy(sig_res_reported, sig, SIG_SIZE_BYTES);
        free(dummy_sig);
        free(sig);
    }
}