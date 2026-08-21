
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <string.h>

#include "../src/drup_top_check.h"
#include "../src/file_reader.h"
//#include "../src/dummy_file_reader.h"

// Instantiate int_vec
#define TYPE int
#define TYPED(THING) int_##THING
#include "../src/vec.h"
#undef TYPED
#undef TYPE

char* FORMULA_PATH;
char* PROOF_PATH;

struct file_reader* reader;
struct int_vec* lits_buffer;

unsigned vars = 0;
unsigned max_var = 0;
unsigned clauses = 0;
unsigned parsed = 0;
size_t nb_produced = 0, nb_imported = 0, nb_deleted = 0;

clock_t start_formula, end_formula, start_proof, end_proof;

// ----- Definitions for DRUP to LRUP conversion -----
#define WRITE_ADDITION
#define WRITE_HINTS
#define WRITE_IMPORT
#define WRITE_DELETIONS

#ifdef DRUP_TO_LRUP_CONVERSION

#include "../src/file_writer.h"
file_writer* lrup_out;
extern struct u64_vec* hints;
extern struct u64_vec* deletions;

#define TYPE u64
#define TYPED(THING) u64_##THING
#include "../src/vec.h"
#undef TYPED
#undef TYPE

#define WRITE_SL(X) file_writer_vbl_sl(lrup_out, X)
#define WRITE_INT(X) file_writer_vbl_int(lrup_out, X)
#define WRITE_CHAR(X) file_writer_vbl_char(lrup_out, X)

#define WRITE_LIT_BUFFER do {   \
        WRITE_SL(id);   \
        for (u64 i = 0; i < lits_buffer->size; i++)    \
            WRITE_INT(lits_buffer->data[i]);   \
        WRITE_CHAR(0);  \
    } while (0)

#undef WRITE_ADDITION
#define WRITE_ADDITION do { \
        WRITE_CHAR('a');    \
        WRITE_LIT_BUFFER;   \
    } while (0)

#undef WRITE_HINTS
#define WRITE_HINTS do {    \
        for (u64 i = 0; i < hints->size; i++)   \
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

#endif

#define ABS(X) (X < 0 ? -X : X)

bool is_digit(char c) {
    switch (c) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            return true;
    
        default:
            return false;
    }
}

bool is_whitespace(char c) {
    switch (c) {
        case ' ':
        case '\n':
        case '\t':
        return true;
    
        default:
            return false;
    }
}

int read_int(FILE* f) {
    int res = 0;
    int sign = 1;
    char c = fgetc(f);
    while (is_whitespace(c)) c = fgetc(f);
    if (c == EOF) return __INT_MAX__;
    if (c == '-') { sign = -1; c = fgetc(f); }
    for (; is_digit(c); c = fgetc(f))
        res = (res * 10) + (c - '0');

    return res * sign;
}

bool parse_formula() {
    FILE* formula = fopen(FORMULA_PATH, "r");
    if (!formula) {
        printf("* [ERROR] Could not open formula at %s\n", FORMULA_PATH);
        return false;
    }

    // TODO: parse comments
    char c;
    c = fgetc(formula); if (c != 'p') return false;
    c = fgetc(formula); if (c != ' ') return false;
    c = fgetc(formula); if (c != 'c') return false;
    c = fgetc(formula); if (c != 'n') return false;
    c = fgetc(formula); if (c != 'f') return false;
    c = fgetc(formula); if (c != ' ') return false;

    // get vars
    vars = read_int(formula);
    
    // get clauses
    clauses = read_int(formula);

    drup_top_check_init(vars);

    while (!feof(formula)) {
        int lit = read_int(formula);
        if (feof(formula)) break;
        if (lit == 0) parsed++;
        if (ABS(lit) > max_var) max_var = ABS(lit);
        if (!drup_top_check_load(lit)) {
            fclose(formula);
            return false;
        }
    }
    if (!drup_top_check_end_load()) {
        fclose(formula);
        return false;
    }

    fclose(formula);
    assert((u64)clauses == drup_top_check_get_nb_loaded_clauses());
    return true;
}

bool check_proof() {
    while (true) {
        char c = file_reader_read_vbl_char(reader);
        if (file_reader_eof_reached(reader)) {
        //if (c == EOF) {
            return true;
            break;

        } else if (c == TRUSTED_CHK_CLS_PRODUCE) {
            int_vec_resize(lits_buffer, 0);

            u64 id = (u64)file_reader_read_vbl_sl(reader);
            
            // parse lits
            while (true) {
                int lit = file_reader_read_vbl_int(reader);
                if (!lit) break;
                int_vec_push(lits_buffer, lit);
            }

            WRITE_DELETIONS;
            WRITE_ADDITION;

            if (!drup_top_check_add(id, lits_buffer->data, lits_buffer->size)) {
                printf("* [ERROR] while adding clause %lu\n", id);
                break;
            }
            
            WRITE_HINTS;
            nb_produced++;

        } else if (c == TRUSTED_CHK_CLS_IMPORT) {
            int_vec_resize(lits_buffer, 0);

            u64 id = (u64)file_reader_read_vbl_sl(reader);
            
            // parse lits
            while (true) {
                int lit = file_reader_read_vbl_int(reader);
                if (!lit) break;
                int_vec_push(lits_buffer, lit);
            }

            WRITE_IMPORT;

            if (!drup_top_check_import(id, lits_buffer->data, lits_buffer->size)) {
                printf("* [ERROR] while importing clause %lu\n", id);
                break;
            }

            nb_imported++;

        } else if (c == TRUSTED_CHK_CLS_DELETE) {
            int_vec_resize(lits_buffer, 0);
            
            // parse lits
            while (true) {
                int lit = file_reader_read_vbl_int(reader);
                if (!lit) break;
                int_vec_push(lits_buffer, lit);
            }

            if (!drup_top_check_delete(lits_buffer->data, lits_buffer->size)) {
                printf("* [ERROR] while deleting cluase\n");
                break;
            }

            nb_deleted++;

        } else {
            printf("* [ERROR] Invalid directive %d", c);
            break;
        }
    }
    return false;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("* [ERROR] Need argument <formula_path> <proof_path> [OPTIONS]. ABORT.\n");
        abort();
    }
    
    FORMULA_PATH = argv[1];
    PROOF_PATH = argv[2];

    #ifdef DRUP_TO_LRUP_CONVERSION
    FILE* lrup_file;
    char lrup_file_path[750];
    snprintf(lrup_file_path, 750, "%s.extended", PROOF_PATH);
    LOG("print extended proof fragment to %s", lrup_file_path);
    lrup_file = fopen(lrup_file_path, "wb");
    lrup_out = file_writer_init(lrup_file, 1024);
    #endif

    start_formula = clock();
    if (!parse_formula()) {
        printf("* [ERROR] while parsing formula.\n");
        return 1;
    };
    end_formula = clock();
    
    printf("* Meta information on %s:\n", FORMULA_PATH);
    printf("   - %i variables expected\n", vars);
    printf("   - %i variables parsed\n", max_var);
    printf("   - %i clauses expected\n", clauses);
    printf("   - %i clauses parsed\n", parsed);
    printf("   - %f cpu seconds to parse formula\n", (double)(end_formula - start_formula) / CLOCKS_PER_SEC);
    
    FILE* f = fopen(PROOF_PATH, "rb");
    reader = file_reader_init(1048576, f, 0);
    lits_buffer = int_vec_init(16);
    
    start_proof = clock();
    if (!check_proof()) {
        printf("* [ERROR] while checking proof.\n");
        return 1;
    }
    end_proof = clock();

    if(!drup_top_check_valid()) {
        printf("s NOT VALIDATED.\n");
    } else {
        printf("s VALIDATED\n");
    }

    drup_top_check_end();
    file_reader_end(reader);
    int_vec_free(lits_buffer);
    #ifdef DRUP_TO_LRUP_CONVERSION
    file_writer_free(lrup_out);
    #endif

    printf("* Meta information on %s:\n", PROOF_PATH);
    printf("   - %lu clauses produced\n", nb_produced);
    printf("   - %lu clauses imported\n", nb_imported);
    printf("   - %lu clauses deleted\n", nb_deleted);
    printf("   - %lf cpu seconds to check proof\n", (double)(end_proof - start_proof) / CLOCKS_PER_SEC);

    return 0;
}

