
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "import_handler.h"
#include "utils/palrup_utils.h"
#include "file_reader.h"
#include "comm_sig.h"
#include "heap.h"
#include "merge_buffer.h"
#include "write_buffer.h"

#ifdef UNIT_TEST
#define unit_static
#else
#define unit_static static
#endif

struct import_handler_stats {
    u64 nb_clauses;
    u64 nb_duplicates;
    u64 nb_flushes;
    u64 nb_file_merges;
} import_handler_stats_init = {0, 0, 0, 0};

// global values
u64 ih_num_solvers;
size_t ih_msg_group_size;
u64 ih_redist_strat;
u64 ih_pal_id;
float alpha;
struct import_handler_stats ih_stats;

// global buffers
struct merge_buffer* merge_buffer;

// per message
struct comm_sig** signatures;
struct clause_heap** clause_heaps;
char** file_names;
FILE** out_files;
u64* max_ids;
u64* clause_counts;

#ifdef UNIT_TEST
FILE* get_import_handler_out_file() {
    return out_files[0];
}

char* get_import_handler_out_file_name() {
    return file_names[0];
}

void set_import_handler_out_file(FILE* file) {
    out_files[0] = file;
}

void set_import_handler_max_id(unsigned long id) {
    max_ids[0] = id;
}
#endif

// TODO: Add dedicated unit tests
static u64 get_merge_file_pos(u64 clause_id, FILE* file) {
    rewind(file);

    while (true) {
        u64 id = palrup_utils_read_ul(file);
        if (clause_id <= id)
            break;
        int nb_lits = palrup_utils_read_int(file);
        palrup_utils_skip_bytes(nb_lits * sizeof(int), file);
    }

    fseek(file, -sizeof(u64), SEEK_CUR);
    return ftell(file);
}

// TODO: Add dedicated unit tests
static void write_clause_to_merge_buffered_file(clause_ptr clause, FILE* write_ptr, struct merge_buffer* buffer) {
    FILE* read_ptr = buffer->file;

    if (!buffer->eof) {
        if ((long)get_clause_size(clause) + ftell(write_ptr) < ftell(read_ptr))
            merge_buffer_fill(buffer);

        // couldn't make enough space for new clause
        if ((long)get_clause_size(clause) + ftell(write_ptr) < ftell(read_ptr)) {
            palrup_utils_log_err("Could not create enough space while merging clauses into .palrup_proxy file");
            exit(1);
        }
    }

    palrup_utils_write_flat_clause(clause, get_clause_size(clause), write_ptr);
}

static inline int get_file_id(u64 clause_id) {
    // might need to be altered for differing strategies
    return (clause_id % ih_num_solvers) % ih_msg_group_size;
}

static void init_msg_group() {
    signatures = palrup_utils_malloc(sizeof(struct comm_sig*) * ih_msg_group_size);
    clause_heaps = palrup_utils_malloc(sizeof(struct clause_heap*) * ih_msg_group_size);
    file_names = palrup_utils_malloc(sizeof(char*) * ih_msg_group_size);
    out_files = palrup_utils_malloc(sizeof(FILE*) * ih_msg_group_size);
    
    max_ids = palrup_utils_calloc(ih_msg_group_size, sizeof(unsigned long));
    clause_counts = palrup_utils_calloc(ih_msg_group_size, sizeof(u64));
}

// write out file for each pal
// TODO: fix strat 1
static void init_strat_1(struct options* options) {
    ih_msg_group_size = ih_num_solvers;
    init_msg_group();

    for (size_t i = 0; i < ih_msg_group_size; i++) {
        signatures[i] = comm_sig_init(SECRET_KEY_2);
        clause_heaps[i] = heap_init(options->q_size);
        file_names[i] = palrup_utils_malloc(1024 * sizeof(char));
        u64 dir_hierarchy = i / palrup_utils_calc_root_ceil(ih_num_solvers);
        snprintf(file_names[i], 1024, "%s/%lu/%lu/%lu.palrup_proxy~",
                 options->working_path, dir_hierarchy, i, ih_pal_id);
        out_files[i] = fopen(file_names[i], "wb+");

        if (!(out_files[i])) {
            snprintf(palrup_utils_msgstr, MSG_LEN, "Could not create file at %s\n", file_names[i]);
            palrup_utils_log_err(palrup_utils_msgstr);
        }
    }
}

// write out files for each pal in row
// TODO: fix strat 2
static void init_strat_2(struct options* options) {
    ih_msg_group_size = palrup_utils_calc_root_ceil(ih_num_solvers);
    init_msg_group();

    int dir_hierarchy = ih_pal_id / ih_msg_group_size;
    for (size_t i = 0; i < ih_msg_group_size; i++) {
        signatures[i] = comm_sig_init(SECRET_KEY_2);
        clause_heaps[i] = heap_init(options->q_size);
        file_names[i] = palrup_utils_malloc(1024 * sizeof(char));
        u64 target_pal_id = (dir_hierarchy * ih_msg_group_size) + i;
        snprintf(file_names[i], 1024, "%s/%i/%lu/%lu.palrup_proxy~",
                 options->working_path, dir_hierarchy, target_pal_id, ih_pal_id % ih_msg_group_size);
        out_files[i] = fopen(file_names[i], "wb+");

        if (!(out_files[i])) {
            snprintf(palrup_utils_msgstr, MSG_LEN, "Could not create file at %s\n", file_names[i]);
            palrup_utils_log_err(palrup_utils_msgstr);
        }
    }
}

// write single out file in pals' own directory
static void init_strat_3(struct options* options) {
    ih_msg_group_size = 1;
    init_msg_group();
    int dir_hierarchy = ih_pal_id / palrup_utils_calc_root_ceil(ih_num_solvers);
    file_names[0] = palrup_utils_malloc(1024 * sizeof(char));
    snprintf(file_names[0], 1024, "%s/%u/%lu/out.palrup_proxy~", options->working_path, dir_hierarchy, ih_pal_id);

    signatures[0] = comm_sig_init(SECRET_KEY_2);
    clause_heaps[0] = heap_init(options->q_size);
    out_files[0] = fopen(file_names[0], "wb+");

    if (!(out_files[0])) {
        snprintf(palrup_utils_msgstr, MSG_LEN, "Could not create file at %s\n", file_names[0]);
        palrup_utils_log_err(palrup_utils_msgstr);
    }
}

static void print_stats() {
    char msg[512];
    snprintf(msg, 512, "import_handler_stats nb_clauses:%lu, nb_duplicates:%lu, nb_flushes:%lu, nb_file_merges:%lu", ih_stats.nb_clauses, ih_stats.nb_duplicates, ih_stats.nb_flushes, ih_stats.nb_file_merges);
    palrup_utils_log(msg);
}

// flush_ratio \in [0,1] denotes the maximum fill level of the heap after the flush operation
unit_static void flush_heap_to_file(struct clause_heap* clause_heap, int file_id, float flush_ratio) {
    if (clause_heap->size <= 0)
        return;
    ih_stats.nb_flushes++;
    unsigned long* max_id = &(max_ids[file_id]);
    FILE* write_ptr = out_files[file_id];
    clause_ptr c = heap_get_min(clause_heap);

    if (*max_id < get_clause_id(c)) {   // simply appent heap to file
        write_buffer_swich_context(write_ptr);
        while (clause_heap->size > clause_heap->capacity * flush_ratio) {
            if (clause_heap->size <= 0)
                break;
            ih_stats.nb_duplicates += heap_delete_duplicates(clause_heap);
            c = heap_pop_min(clause_heap);

            // write clause to file and remove from heap
            write_buffer_add_clause(get_clause_id(c),
                                    get_clause_nb_lits(c),
                                    get_clause_lits(c));
            clause_counts[file_id]++;
            comm_sig_update_clause(signatures[file_id],
                                   get_clause_id(c),
                                   get_clause_lits(c),
                                   get_clause_nb_lits(c));
            *max_id = get_clause_id(c);
            delete_flat_clause(c);
        }

    } else {    // merge file with heap to assure sorted clauses in file 
        ih_stats.nb_file_merges++;
        
        // TODO: integrate write_buffer

        // with strat 3 the buffer might not be empty
        if (ih_redist_strat == 3)
            write_buffer_write_to_file(write_ptr);
        merge_buffer_open_file(merge_buffer, file_names[file_id]);
        merge_buffer_set_file_pointer(merge_buffer, get_merge_file_pos(get_clause_id(c), write_ptr));

        // merge file and heap
        clause_ptr min_clause, file_clause, heap_clause;
        file_clause = merge_buffer_next_clause(merge_buffer);
        if (clause_heap->size > (clause_heap->capacity * flush_ratio)) {
            ih_stats.nb_duplicates += heap_delete_duplicates(clause_heap);
            heap_clause = heap_pop_min(clause_heap);
        }
        bool add_sig;
        while (file_clause || clause_heap->size > (clause_heap->capacity * flush_ratio)) {
            add_sig = false;

            if (!file_clause || (heap_clause && get_clause_id(file_clause) > get_clause_id(heap_clause))) {
                min_clause = heap_clause;
                ih_stats.nb_duplicates += heap_delete_duplicates(clause_heap);
                heap_clause = heap_pop_min(clause_heap);
                add_sig = true;
            } else if (!heap_clause || get_clause_id(file_clause) < get_clause_id(heap_clause)) {
                min_clause = file_clause;
                file_clause = merge_buffer_next_clause(merge_buffer);
            } else if (compare_flat_clause(file_clause, heap_clause)) {
                min_clause = file_clause;
                file_clause = merge_buffer_next_clause(merge_buffer);
                delete_flat_clause(heap_clause);
                ih_stats.nb_duplicates++;
                ih_stats.nb_duplicates += heap_delete_duplicates(clause_heap);
                heap_clause = heap_pop_min(clause_heap);
            } else {
                palrup_utils_log_err("differing clauses with same id detected");
                exit(1);
            }

            write_clause_to_merge_buffered_file(min_clause, write_ptr, merge_buffer);
            if (add_sig) {
                clause_counts[file_id]++;
                comm_sig_update_clause(signatures[file_id],
                                       get_clause_id(min_clause),
                                       get_clause_lits(min_clause),
                                       get_clause_nb_lits(min_clause));
            }
            *max_id = get_clause_id(min_clause);
            delete_flat_clause(min_clause);
        }

        // write potentially remaining heap clause
        if (heap_clause) {
            write_clause_to_merge_buffered_file(heap_clause, write_ptr, merge_buffer);
            clause_counts[file_id]++;
            comm_sig_update_clause(signatures[file_id],
                                   get_clause_id(heap_clause),
                                   get_clause_lits(heap_clause),
                                   get_clause_nb_lits(heap_clause));
            delete_flat_clause(heap_clause);
        }

        assert(merge_buffer->size == 0);
    }
}

void import_handler_init(struct options* options) {
    ih_num_solvers = options->num_solvers;
    ih_redist_strat = options->redist_strat;
    ih_pal_id = options->pal_id;
    alpha = options->q_alpha;
    ih_stats = import_handler_stats_init;
    
    merge_buffer = merge_buffer_init(options->merge_buffer_size, NULL);
    write_buffer_init(options->write_buffer_size);

    switch (ih_redist_strat) {
    case 1:
        init_strat_1(options);
        break;
    case 2:
        init_strat_2(options);
        break;
    case 3:
        init_strat_3(options);
        break;
    default:
        snprintf(palrup_utils_msgstr, MSG_LEN, "Redistribution strategy %lu not available.", ih_redist_strat);
        palrup_utils_log_err(palrup_utils_msgstr);
        break;
    }
}

void import_handler_log(clause_ptr clause) {
    int file_id = get_file_id(get_clause_id(clause));
    //clause_ptr clause = create_flat_clause(clause_id, nb_literals, literals);
    struct clause_heap* clause_heap = clause_heaps[file_id];
    ih_stats.nb_clauses++;

    // write to file if capacity is reached
    if (heap_insert(clause_heap, clause)) {
        flush_heap_to_file(clause_heap, file_id, alpha);
        if (heap_insert(clause_heap, clause)) {
            palrup_utils_log_err("Clause could not be inserted into heap.");
            exit(1);
        }
    }
}

void import_handler_end() {
    for (size_t i = 0; i < ih_msg_group_size; i++) {
        // wrap up out file
        flush_heap_to_file(clause_heaps[i], i, 0);
        write_buffer_write_to_file();
        assert(clause_heaps[i]->size == 0);
        palrup_utils_write_ul(0,out_files[i]);
        u8* sig = comm_sig_digest(signatures[i]);
        palrup_utils_write_sig(sig, out_files[i]);
        free(sig);
        fclose(out_files[i]);
        
        // mark out file as finished
        int new_str_len = strlen(file_names[i])-1;
        char new_filename[new_str_len];
        memcpy(new_filename, file_names[i], new_str_len);
        new_filename[new_str_len] = '\0';
        rename(file_names[i], new_filename);
        
        // free msg_group
        comm_sig_free(signatures[i]);
        heap_free(clause_heaps[i]);
        free(file_names[i]);
    }

    free(out_files);
    free(file_names);
    free(clause_counts);
    free(clause_heaps);
    free(max_ids);
    free(signatures);
    merge_buffer_free(merge_buffer);
    write_buffer_end();
    print_stats();
}
