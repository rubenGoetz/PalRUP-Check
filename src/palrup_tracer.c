
#include <stdlib.h>
#include <assert.h>

#include "palrup_tracer.h"
#include "utils/define.h"
#include "hash.h"

#define TYPE u64
#define TYPED(THING) u64_##THING
#include "vec.h"
#undef TYPED
#undef TYPE

struct palrup_tracer {
    unsigned long nb_orig_clauses;
    unsigned long solver_id;
    unsigned long num_solvers;
    unsigned long last_id;
    bool use_binary;
    FILE* proof_fragment;
    struct hash_table* H;           // id mapping
    struct u64_vec* deletes;        // accumulates clause deletions to be written before next clause addition or import
};

struct palrup_tracer* palrup_tracer_init(const unsigned long nb_orig_clauses, const bool use_binary, const char* fragment_path, const unsigned long solver_id, const unsigned long nb_solvers) {
    struct palrup_tracer* tracer = (struct palrup_tracer*)malloc(sizeof(struct palrup_tracer));
    
    tracer->nb_orig_clauses = nb_orig_clauses;
    tracer->solver_id = solver_id;
    tracer->num_solvers = nb_solvers;
    tracer->last_id = nb_orig_clauses - (nb_orig_clauses % nb_solvers) + solver_id;
    tracer->use_binary = use_binary;
    if (tracer->use_binary) tracer->proof_fragment = fopen(fragment_path, "wb");
    else tracer->proof_fragment = fopen(fragment_path, "w");
    assert(tracer->proof_fragment);
    tracer->H = hash_table_init(16);
    tracer->deletes = u64_vec_init(16);
    
    return tracer;
}
void palrup_tracer_free(struct palrup_tracer* tracer) {
    fclose(tracer->proof_fragment);
    hash_table_light_free(tracer->H);
    u64_vec_free(tracer->deletes);
    free(tracer);
}

static inline void write_char(struct palrup_tracer* tracer, char c) {
    if (tracer->use_binary) fputc(c, tracer->proof_fragment);
    else if (c == (char)0) fprintf(tracer->proof_fragment, " 0");
    else fprintf(tracer->proof_fragment, "%c", c);
}
static inline void write_int(struct palrup_tracer* tracer, int i) {
    if (tracer->use_binary) {
        // encode sign
        unsigned int tmp = i < 0 ? -i : i;
        tmp = 2 * tmp + (i < 0);

        while (tmp & (~127)) {      // while more than 7 bits remain
            fputc((char)(tmp & 127) | 128, tracer->proof_fragment);
            tmp >>= 7;
        }
        fputc((char)tmp, tracer->proof_fragment);
    }
    else fprintf(tracer->proof_fragment, " %i", i);
}
static inline void write_ul(struct palrup_tracer* tracer, unsigned long ul) {
    if (tracer->use_binary) {
        unsigned long tmp = ul;
        while (tmp & (~127UL)) {    // while more than 7 bits remain
            fputc((char)(tmp & 127UL) | 128, tracer->proof_fragment);
            tmp >>= 7;
        }
        fputc((char)tmp, tracer->proof_fragment);
    }
    else fprintf(tracer->proof_fragment, " %lu", ul);
}
static inline void write_endline(struct palrup_tracer* tracer) {
    if (!tracer->use_binary) fprintf(tracer->proof_fragment, "\n");
}

// TODO: make efficient
// returns next clause ID for solver and transforms hints 
unsigned long palrup_tracer_next_id(struct palrup_tracer* tracer, const int nb_hints, const unsigned long* ext_hints) {
    // ID calculation variables:
    //    v         v      vialble IDs
    // ---|----p----|--->  Integer space
    //    |-o-|--t--|
    //        s

    // calculate s
    unsigned long s = tracer->last_id;
    for (int i = 0; i < nb_hints; i++)
        if (s < ext_hints[i]) s = ext_hints[i];
    
    // calculate o
    unsigned long o = 1 + s - tracer->last_id;
    
    // calculate t
    unsigned long t = tracer->num_solvers - (o % tracer->num_solvers);

    // calculate next id
    unsigned long next_id = tracer->last_id + o + (t % tracer->num_solvers);

    assert(next_id > tracer->last_id);
    assert(next_id % tracer->num_solvers == tracer->solver_id);
    for (int i = 0; i < nb_hints; i++) assert(next_id > ext_hints[i]);
    tracer->last_id = next_id;
    return next_id;
}

static inline void write_deletes(struct palrup_tracer* tracer) {
    struct u64_vec* deletes = tracer->deletes;
    if (deletes->size) {
        write_char(tracer, TRUSTED_CHK_CLS_DELETE);
        while (deletes->size)
            write_ul(tracer, deletes->data[--(deletes->size)]);
        write_char(tracer, 0);
        write_endline(tracer);
    }
}
void palrup_tracer_log_clause_addition(struct palrup_tracer* tracer, const unsigned long id, const int nb_lits, const int* lits, const int nb_hints, const unsigned long* hints) {
    // Write delete line if necessary
    write_deletes(tracer);

    // map hints
    unsigned long ext_hints[nb_hints];
    for (int i = 0; i < nb_hints; i++) {
        unsigned long ext_hint = (unsigned long)hash_table_find(tracer->H, hints[i]);
        ext_hints[i] = ext_hint ? ext_hint : hints[i];    // keep original & unknown ids
        assert(ext_hints[i] > 0);
    }

    // map id
    unsigned long ext_id = palrup_tracer_next_id(tracer, nb_hints, ext_hints);
    hash_table_insert(tracer->H, id, (void*)ext_id);    

    //write add line
    write_char(tracer, TRUSTED_CHK_CLS_PRODUCE);
    write_ul(tracer, ext_id);
    for (int i = 0; i < nb_lits; i++)
        write_int(tracer, lits[i]);
    write_char(tracer, 0);
    for (int i = 0; i < nb_hints; i++)
        write_ul(tracer, ext_hints[i]);
    write_char(tracer, 0);
    write_endline(tracer);
}
void palrup_tracer_log_clause_import(struct palrup_tracer* tracer, unsigned long id, int nb_lits, int* lits) {
    // Write delete line if necessary
    write_deletes(tracer);
    
    // write import line
    write_char(tracer, TRUSTED_CHK_CLS_IMPORT);
    write_ul(tracer, id);
    for (int i = 0; i < nb_lits; i++)
        write_int(tracer, lits[i]);
    write_char(tracer, 0);
    write_endline(tracer);
}
void palrup_tracer_log_clause_deletion(struct palrup_tracer* tracer, unsigned long id) {
    // store id for next delete line
    unsigned long ext_id = (unsigned long)hash_table_find(tracer->H, id);
    ext_id = ext_id ? ext_id : id;  // keep original & unknown ids
    assert(ext_id > 0);
    u64_vec_push(tracer->deletes, ext_id);
}
