
#include <glob.h>
#include <assert.h>
#include <stdlib.h>
#include <math.h>

#include "local_trim.h"
#include "utils/palrup_utils.h"
#include "backward_file_reader.h"
#include "hash.h"

#define TYPE int
#define TYPED(THING) int_##THING
#include "../src/vec.h"
#undef TYPED
#undef TYPE

#define TYPE u64
#define TYPED(THING) u64_##THING
#include "vec.h"
#undef TYPED
#undef TYPE

#define SENTINEL (void*)1

struct local_trim_stats {
    u64 read_lines;
    u64 kept_lines;
    u64 added_lines;
} local_trim_stats_init = {0, 0, 0};
struct local_trim_stats lt_stats;

struct back_file_reader* bfr;
FILE* out_file;
char* out_file_path;
struct hash_table* marked_clauses;
bool expect_empty_clause;

struct int_vec* lits_buffer;
struct u64_vec* hints_buffer;
struct u8_vec* write_buffer;

bool hold = false;

static void write_line_backwards(struct back_file_reader* bfr, u64 idx) {
    assert(idx <= bfr->read_idx);

    // write as much data as possible into buffer
    u64 write_idx = MAX((long)idx, (long)(bfr->read_idx - (write_buffer->capacity - write_buffer->size)));
    assert(bfr->read_idx >= write_idx);
    while (bfr->read_idx > write_idx)
        write_buffer->data[write_buffer->size++] = bfr->buffer->data[bfr->read_idx--];
    
    // flush buffer if necessary
    if (write_buffer->size == write_buffer->capacity) {
        palrup_utils_write_objs(write_buffer->data, 1, write_buffer->size, out_file);
        write_buffer->size = 0;
    }

    // write rest of line into buffer
    while (bfr->read_idx >= idx)
        write_buffer->data[write_buffer->size++] = bfr->buffer->data[bfr->read_idx--];
}

// TODO: make inplace
static void reverse_outfile() {
    FILE* out_file_rev = fopen(out_file_path, "wb");
    if (!out_file_rev) {
        snprintf(palrup_utils_msgstr, MSG_LEN, "could not open file at %s", out_file_path);
        palrup_utils_log_err(palrup_utils_msgstr);
        abort();
    }

    struct u8_vec* read_buffer = u8_vec_init(write_buffer->capacity);
    write_buffer->size = 0;

    while (ftell(out_file) > 0) {
        size_t to_read = MIN(read_buffer->capacity, (u64)ftell(out_file));
        fseek(out_file, -to_read, SEEK_CUR);
        palrup_utils_read_objs(read_buffer->data, 1, to_read, out_file);
        fseek(out_file, -to_read, SEEK_CUR);

        for (size_t i = 0; i < to_read; i++)
            write_buffer->data[i] = read_buffer->data[to_read - 1 - i];
        
        palrup_utils_write_objs(write_buffer->data, 1, to_read, out_file_rev);
    }

    fclose(out_file_rev);
    u8_vec_free(read_buffer);
}

static void print_stats() {
    char msg[512];
    snprintf(msg, 512, "local_trim_stats read_lines:%lu, kept_lines:%lu, added_lines:%lu", lt_stats.read_lines, lt_stats.kept_lines, lt_stats.added_lines);
    palrup_utils_log(msg);
}

static void mark_empty_clause(struct options* options) {
    // get all pals that found the empty clause
    glob_t globbuf;
    char pattern[1024];
    snprintf(pattern, 1024, "%s/.unsat_found/*", options->working_path);
    glob(pattern, 0, NULL, &globbuf);

    // warn if no empty clause was found
    if (globbuf.gl_pathc == 0)
        palrup_utils_log_warn("No empty clause waas found in local pass");

    // get decisive pal_id with empty clasue
    u64 empty_pal_id = -1;
    for (size_t i = 0; i < globbuf.gl_pathc; i++)
        empty_pal_id = MIN(empty_pal_id, (u64)atol(globbuf.gl_pathv[i]));

    // find and mark empty clause if necessary
    if (empty_pal_id == options->pal_id)
        expect_empty_clause = true;
    else
        expect_empty_clause = false;

    globfree(&globbuf);
}

static void init_strat_3(struct options* options) {
    // read imports and mark all clauses
    int msg_group_size = palrup_utils_calc_root_ceil(options->num_solvers);
    char path[512];
    int column = options->pal_id % msg_group_size;
    for (int i = 0; i < msg_group_size; i++) {
        int src_id = (i * msg_group_size) + column;
        snprintf(path, 512, "%s/%i/%i/out.palrup_import",
                 options->working_path, src_id / msg_group_size, src_id);
        FILE* file = fopen(path, "rb");
        if (!file) {
            snprintf(palrup_utils_msgstr, MSG_LEN, "Could not open import file at %s", path);
            palrup_utils_log_err(palrup_utils_msgstr);
        }
        
        // TODO: make dedicated trim communication files
        // parse file
        while (true) {
            // read clause
            u64 id = palrup_utils_read_ul(file);
            if (!id) break;

            // skip lits
            int nb_lits = palrup_utils_read_int(file);
            int_vec_resize(lits_buffer, nb_lits);
            palrup_utils_read_ints(lits_buffer->data, nb_lits, file);

            // mark id
            if (id % options->num_solvers == options->pal_id)
                hash_table_insert(marked_clauses, id, SENTINEL);
        }

        fclose(file);
    }
} 

void local_trim_init(struct options* options) {
    // read proof and open proof.trim
    char frag_path[512];
    snprintf(frag_path, 1024, "%s/%lu/%lu/out.palrup",
             options->palrup_path,
             options->pal_id / palrup_utils_calc_root_ceil(options->num_solvers),
             options->pal_id);
    FILE* input = fopen(frag_path, "rb");
    bfr = back_file_reader_init(input, options->read_buffer_size);

    out_file_path = palrup_utils_malloc(1024);
    snprintf(out_file_path, 1024, "%s.trim", frag_path);
    snprintf(frag_path, 1024, "%s~", out_file_path);
    out_file = fopen(frag_path, "wb+");
    if (!out_file) {
        snprintf(palrup_utils_msgstr, MSG_LEN, "could not open file at %s", out_file_path);
        palrup_utils_log(palrup_utils_msgstr);
    }

    // init global datastructures
    marked_clauses = hash_table_init(16);
    lits_buffer = int_vec_init(16);
    hints_buffer = u64_vec_init(16);
    write_buffer = u8_vec_init(options->write_buffer_size);
    lt_stats = local_trim_stats_init;

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

    mark_empty_clause(options);
}

void local_trim_run() {
    // scan proof backwards
    u64 id;
    while (!back_file_reader_eof(bfr) || !(bfr->read_idx == 0)) {
        u64 line_idx = back_file_reader_get_start_line_idx(bfr);
        lt_stats.read_lines++;
        switch (back_file_reader_decode_char(bfr, line_idx)) {
        case TRUSTED_CHK_CLS_DELETE:
            // skip delete lines. deletes will be placed for new hints.
            break;

        case TRUSTED_CHK_CLS_IMPORT:
            id = back_file_reader_decode_sl(bfr, line_idx+1);
            if (hash_table_find(marked_clauses, id)) {
                write_line_backwards(bfr, line_idx);
                hash_table_delete_last_found(marked_clauses);
                lt_stats.kept_lines++;
            }
            break;

        case TRUSTED_CHK_CLS_PRODUCE:
            id = back_file_reader_decode_sl(bfr, line_idx+1);
            if (hash_table_find(marked_clauses, id)) {
                hash_table_delete_last_found(marked_clauses);
                u64 hint_idx = bfr->read_idx;

                write_line_backwards(bfr, line_idx);
                bfr->read_idx = hint_idx;

                // mark hints
                back_file_reader_vbl_sl(bfr);   // skip 0
                while (true) {
                    u64 hint = back_file_reader_vbl_sl(bfr);
                    if (hint == 0)
                        break;
                    if (hash_table_insert(marked_clauses, hint, SENTINEL)) {
                        // TODO: write delete line for new hints
                    }
                }

                lt_stats.kept_lines++;
            } else if (expect_empty_clause) {
                // check for empty clause
                u64 tmp_idx = line_idx + 1;
                while (bfr->buffer->data[tmp_idx++] & 128);     // skip clause id
                int tmp = back_file_reader_decode_int(bfr, tmp_idx);
                if (tmp)    // not the empty clause
                    break;

                snprintf(palrup_utils_msgstr, MSG_LEN, "Found empty clause with id %lu.", id);
                palrup_utils_log(palrup_utils_msgstr);
                u64 hint_idx = bfr->read_idx;
                write_line_backwards(bfr, line_idx);
                bfr->read_idx = hint_idx;

                // mark hints
                back_file_reader_vbl_sl(bfr);   // skip 0
                while (true) {
                    u64 hint = back_file_reader_vbl_sl(bfr);
                    if (hint == 0)
                        break;
                    hash_table_insert(marked_clauses, hint, SENTINEL);
                }

                expect_empty_clause = false;
            }
            break;

        default:
            snprintf(palrup_utils_msgstr, MSG_LEN, "Invalid directive '%c' while parsing proof.", back_file_reader_decode_char(bfr, line_idx));
            palrup_utils_log_err(palrup_utils_msgstr);
            abort();
        }

        // finish line
        back_file_reader_skip_to_idx(bfr, line_idx ? line_idx - 1 : 0);
    }
}

void local_trim_end() {
    // finish out_file
    palrup_utils_write_objs(write_buffer->data, 1, write_buffer->size, out_file);
    reverse_outfile();
    char tmp_file_path[1024];
    snprintf(tmp_file_path, 1024, "%s~", out_file_path);
    remove(tmp_file_path);

    // free
    free(out_file_path);
    back_file_reader_free(bfr);
    hash_table_light_free(marked_clauses);
    fclose(out_file);
    int_vec_free(lits_buffer);
    u64_vec_free(hints_buffer);
    u8_vec_free(write_buffer);

    print_stats();
}
