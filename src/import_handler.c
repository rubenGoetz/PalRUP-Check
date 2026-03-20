
#include <assert.h>
#include <math.h>
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

#ifdef UNIT_TEST
#define unit_static
#else
#define unit_static static
#endif

struct importer_stats {
    u64 nb_clauses;
    u64 nb_duplicates;
    u64 nb_flushes;
    u64 nb_file_merges;
} importer_stats_init = {0, 0, 0, 0};

struct importer_stats stats; 

const char* out_path;  // named pipe
u64 n_solvers;         // number of solvers
double root_n;         // square root of number of solvers
struct comm_sig** signatures;
size_t comm_size;
u64 redist_strat;  // redistribution_strategy
u64 local_rank;    // solver id
float alpha;

// Buffering.
struct u8_vec* write_buffer;
struct clause_heap** clause_heaps;
unsigned long* max_ids;
FILE** out_files;
char** file_names;  // to be removed. uses for file shenanigans
u64* clause_counts; // count clauses contained in each file
struct merge_buffer* merge_buffer;

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
    //palrup_utils_read_int(file);

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
static void write_clause_to_buffered_file(clause_ptr clause, FILE* write_ptr, struct merge_buffer* buffer) {
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

static void import_handler_write_hash(u8* hash, FILE* current_out) {
    palrup_utils_write_sig(hash, current_out);
}

static u64 import_handler_get_proxy_rank(size_t id) {
    u64 x = palrup_utils_rank_to_x(id % n_solvers, comm_size);
    u64 y = palrup_utils_rank_to_y(local_rank, comm_size);
    return palrup_utils_2d_to_rank(x, y, comm_size);
}

void import_handler_init(struct options* options) {
    redist_strat = options->redist_strat;
    n_solvers = options->num_solvers;
    root_n = sqrt((double)options->num_solvers);
    comm_size = (size_t)ceil(root_n);  // round to nearest integer
    if (redist_strat == 1) {
        comm_size = n_solvers;
    }
    out_path = options->working_path;
    local_rank = options->pal_id;
    unsigned int dir_hierarchy = local_rank / comm_size;
    clause_heaps = palrup_utils_malloc(sizeof(struct clause_heap*) * comm_size);
    max_ids = palrup_utils_calloc(comm_size, sizeof(unsigned long));
    out_files = palrup_utils_malloc(sizeof(FILE*) * comm_size);
    file_names = palrup_utils_malloc(sizeof(char*) * comm_size);
    clause_counts = palrup_utils_calloc(comm_size, sizeof(u64));
    signatures = palrup_utils_malloc(sizeof(struct comm_sig*) * comm_size);
    merge_buffer = merge_buffer_init(options->merge_buffer_size, NULL);
    stats = importer_stats_init;
    write_buffer = u8_vec_init(options->write_buffer_size);
    alpha = options->q_alpha;

    char proof_folder[512];
    char ids_path[1024];
    struct clause_heap* single_heap;
    struct comm_sig* single_sig;
    if (redist_strat == 3) {
        single_heap = heap_init(options->q_size);
        single_sig = comm_sig_init(SECRET_KEY_2);
        snprintf(proof_folder, 512, "%s/%u/%lu", out_path, dir_hierarchy, local_rank);
        snprintf(ids_path, 1024, "%s/out.palrup_proxy~", proof_folder);
        FILE* out_file = fopen(ids_path, "wb+");
        for (size_t i = 0; i < comm_size; i++)
            out_files[i] = out_file;
    }

    if (local_rank == 0) {
        char msg[512];
        snprintf(msg, 512, "comm_size: %ld", comm_size);
        palrup_utils_log(msg);
    }

    for (size_t i = 0; i < comm_size; i++) {
        u64 proxy_rank = import_handler_get_proxy_rank(i);
        if (redist_strat == 2)
            snprintf(proof_folder, 512, "%s/%u/%lu", out_path, dir_hierarchy, proxy_rank);
        else if (redist_strat == 3);    
        else
            snprintf(proof_folder, 512, "%s/%lu", out_path, i);

        if (proxy_rank > options->num_solvers - 1) {
            mkdir(proof_folder, 0755);
        }

        // mark filenames as 'not finished writing'
        if (redist_strat == 2)
            snprintf(ids_path, 1024, "%s/%lu.palrup_proxy~", proof_folder, palrup_utils_rank_to_x(local_rank, comm_size));
        else if (redist_strat == 3);
        else
            snprintf(ids_path, 1024, "%s/%lu.palrup_import~", proof_folder, local_rank);

        if (redist_strat != 3) {
            out_files[i] = fopen(ids_path, "wb+");
            if (!(out_files[i])) {
                char msg[1048];
                snprintf(msg, 1048, "out_files not created: %s\n", ids_path);
                palrup_utils_log_err(msg);
            }
        }
        // palrup_utils_write_int(0, out_files[i]);   // placeholder to insert number of clauses in file
        file_names[i] = palrup_utils_calloc(1024, sizeof(char));
        memcpy(file_names[i], ids_path, 1024);

        if (redist_strat == 3) {
            clause_heaps[i] = single_heap;
            signatures[i] = single_sig;
        } else {
            clause_heaps[i] = heap_init(options->q_size);        
            signatures[i] = comm_sig_init(SECRET_KEY_2);
        }
    }
}

unit_static void skip_heap_duplicates(clause_ptr c, struct clause_heap* heap) {
    if (heap->size <= 0)
        return;
    clause_ptr next_c = heap_get_min(heap);
    while (next_c != NULL && get_clause_id(c) == get_clause_id(next_c)) {
        if (!compare_flat_clause(c, next_c)) {
            palrup_utils_log_err("differing clauses with same id detected");
            abort();
        }
        
        delete_flat_clause(heap_pop_min(heap));
        stats.nb_duplicates++;
        if (heap->size <= 0)
            break;
        next_c = heap_get_min(heap);
    }
}

static void write_buffer_to_file(FILE* file) {
    if (write_buffer->size == 0)
        return;

    u64 nb_written = UNLOCKED_IO(fwrite)(write_buffer->data, write_buffer->size, 1, file);
    if (nb_written < 1) palrup_utils_exit_eof();

    u8_vec_resize(write_buffer, 0);
}

// flush_ratio \in [0,1] denotes the maximum fill level of the heap after the flush operation
unit_static void flush_heap_to_file(struct clause_heap* clause_heap, int file_id, float flush_ratio) {
    if (clause_heap->size <= 0)
        return;
    stats.nb_flushes++;
    unsigned long* max_id = &(max_ids[file_id]);
    FILE* write_ptr = out_files[file_id];
    clause_ptr c = heap_get_min(clause_heap);

    if (*max_id < get_clause_id(c)) {   // simply appent heap to file
        while (clause_heap->size > clause_heap->capacity * flush_ratio) {
            if (clause_heap->size <= 0)
                break;
            // TODO: integrate write buffer
            c = heap_pop_min(clause_heap);
            skip_heap_duplicates(c, clause_heap);

            // write clause to file and remove from heap
            //write_flat_clause_to_file(c, write_ptr);
            if (write_buffer->size + get_clause_size(c) > write_buffer->capacity)
                write_buffer_to_file(write_ptr);
            memcpy(write_buffer->data + write_buffer->size, c, get_clause_size(c));
            write_buffer->size += get_clause_size(c);
            
            clause_counts[file_id]++;
            comm_sig_update_clause(signatures[file_id],
                                   get_clause_id(c),
                                   get_clause_lits(c),
                                   get_clause_nb_lits(c));
            *max_id = get_clause_id(c);
            delete_flat_clause(c);
        }
        if (redist_strat != 3)
            write_buffer_to_file(write_ptr);
    } else {    // merge file with heap to assure sorted clauses in file 
        stats.nb_file_merges++;
        
        // with strat 3 the buffer might not be empty
        if (redist_strat == 3)
            write_buffer_to_file(write_ptr);
        merge_buffer_open_file(merge_buffer, file_names[file_id]);
        merge_buffer_set_file_pointer(merge_buffer, get_merge_file_pos(get_clause_id(c), write_ptr));

        // merge file and heap
        clause_ptr min_clause, file_clause, heap_clause;
        file_clause = merge_buffer_next_clause(merge_buffer);
        if (clause_heap->size > (clause_heap->capacity * flush_ratio)) {
            heap_clause = heap_pop_min(clause_heap);
            skip_heap_duplicates(heap_clause, clause_heap);
        }
        bool add_sig;
        while (file_clause || clause_heap->size > (clause_heap->capacity * flush_ratio)) {
            add_sig = false;

            if (!file_clause || (heap_clause && get_clause_id(file_clause) > get_clause_id(heap_clause))) {
                min_clause = heap_clause;
                heap_clause = heap_pop_min(clause_heap);
                skip_heap_duplicates(heap_clause, clause_heap);
                add_sig = true;
            } else if (!heap_clause || get_clause_id(file_clause) < get_clause_id(heap_clause)) {
                min_clause = file_clause;
                file_clause = merge_buffer_next_clause(merge_buffer);
            } else if (compare_flat_clause(file_clause, heap_clause)) {
                min_clause = file_clause;
                file_clause = merge_buffer_next_clause(merge_buffer);
                delete_flat_clause(heap_clause);
                stats.nb_duplicates++;
                heap_clause = heap_pop_min(clause_heap);
                skip_heap_duplicates(heap_clause, clause_heap);
            } else {
                palrup_utils_log_err("differing clauses with same id detected");
                exit(1);
            }

            write_clause_to_buffered_file(min_clause, write_ptr, merge_buffer);
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
            write_clause_to_buffered_file(heap_clause, write_ptr, merge_buffer);
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

static void print_stats() {
    char msg[512];
    snprintf(msg, 512, "importer_stats nb_clauses:%lu, nb_duplicates:%lu, nb_flushes:%lu, nb_file_merges:%lu", stats.nb_clauses, stats.nb_duplicates, stats.nb_flushes, stats.nb_file_merges);
    palrup_utils_log(msg);
}

void import_handler_end() {

    size_t effective_comm_size = comm_size;
    if (redist_strat == 3) // heaps/files/sigs all point to the same objects
        effective_comm_size = 1;

    for (size_t i = 0; i < effective_comm_size; i++) {
        flush_heap_to_file(clause_heaps[i], i, 0);
        assert(clause_heaps[i]->size == 0);
        write_buffer_to_file(out_files[i]);
        u8* sig = comm_sig_digest(signatures[i]);
        palrup_utils_write_ul(0,out_files[i]);
        import_handler_write_hash(sig, out_files[i]);
        comm_sig_free(signatures[i]);
        free(sig);
    }

    for (size_t i = 0; i < effective_comm_size; i++) {
        heap_free(clause_heaps[i]);
        fclose(out_files[i]);
        int new_str_len = strlen(file_names[i])-1;
        char new_filename[new_str_len];
        memcpy(new_filename, file_names[i], new_str_len);
        new_filename[new_str_len] = '\0';
        rename(file_names[i], new_filename);
    }

    for (size_t i = 0; i < comm_size; i++)
        free(file_names[i]);

    free(out_files);
    free(file_names);
    free(clause_counts);
    free(clause_heaps);
    free(max_ids);
    free(signatures);
    u8_vec_free(write_buffer);
    merge_buffer_free(merge_buffer);
    print_stats();
}

void import_handler_log(unsigned long id, const int* literals, int nb_literals) {
    int file_id = palrup_utils_rank_to_x(id % n_solvers, comm_size);
    clause_ptr _clause = create_flat_clause(id, nb_literals, literals);
    struct clause_heap* clause_heap = clause_heaps[file_id];
    stats.nb_clauses++;

    // write to file if capacity is reached
    if (heap_insert(clause_heap, _clause)) {
        flush_heap_to_file(clause_heap, file_id, alpha);
        if (heap_insert(clause_heap, _clause)) {
            palrup_utils_log_err("Clause could not be inserted into heap.");
            exit(1);
        }
    }
}
