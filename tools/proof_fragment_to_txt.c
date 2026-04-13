
#include <stdlib.h>

#include "../src/utils/palrup_utils.h"
#include "../src/clause_flat.h"
#include "../src/siphash.h"
#include "../src/file_reader.h"

// Instantiate int_vec
#define TYPE int
#define TYPED(THING) int_##THING
#include "../src/vec.h"
#undef TYPED
#undef TYPE

// Instantiate u64_vec
#define TYPE u64
#define TYPED(THING) u64_##THING
#include "../src/vec.h"
#undef TYPED
#undef TYPE

char* PATH_IN;
char* PATH_OUT;

struct file_reader* reader;
struct int_vec* lits_buffer;
struct u64_vec* hints_buffer;

static inline void read_literals(int nb_lits) {
    int_vec_reserve(lits_buffer, nb_lits);
    file_reader_read_vbl_ints(lits_buffer->data, nb_lits, reader);
}

static inline void read_hints(int nb_hints) {
    u64_vec_reserve(hints_buffer, nb_hints);
    file_reader_read_vbl_uls(hints_buffer->data, nb_hints, reader);
}

int main(int argc, char *argv[]) {
    if (argc == 2) {
        // INIT
        PATH_IN = argv[1];
        PATH_OUT = palrup_utils_malloc(1024 * sizeof(char));
        snprintf(PATH_OUT, 1024, "%s.txt", PATH_IN);
        printf("* Set outpath to %s\n", PATH_OUT);
    } else if (argc == 3) {
        PATH_IN = argv[1];
        PATH_OUT = argv[2];
    } else {
        printf("* [ERROR] Need arguments <path_in> and <path_out>. ABORT.\n");
        abort();
    }

    // open files
    FILE* input = fopen(PATH_IN, "rb");
    FILE* output = fopen(PATH_OUT, "w");
    
    if (!input) {
        printf("* [ERROR] Could not open input file at %s. ABORT.\n", PATH_IN);
        abort();
    }

    if (!output) {
        printf("* [ERROR] Could not open output file at %s. ABORT.\n", PATH_OUT);
    }

    lits_buffer = int_vec_init(1);
    hints_buffer = u64_vec_init(1);
    siphash_init(SECRET_KEY);
    reader = file_reader_init(1048576, input, 0);
    size_t nb_produced = 0, nb_imported = 0, nb_deleted = 0;

    while (true) {
        char c = file_reader_read_vbl_char(reader);
        if (file_reader_eof_reached(reader)) {
            u8* sig = siphash_digest();
            
            printf("* Meta information on %s:\n", PATH_IN);
            printf("   - %lu clauses produced\n", nb_produced);
            printf("   - %lu clauses imported\n", nb_imported);
            printf("   - %lu clauses deleted\n", nb_deleted);
            printf("   - signature: %i, %i, %i, %i\n",
                   (unsigned int)sig[0], (unsigned int)sig[4], (unsigned int)sig[8], (unsigned int)sig[12]);

            break;

        } else if (c == TRUSTED_CHK_CLS_PRODUCE) {
            int_vec_resize(lits_buffer, 0);
            u64_vec_resize(hints_buffer, 0);

            u64 id = (u64)file_reader_read_vbl_sl(reader);
            siphash_update((u8*)&id, sizeof(u64));
            fprintf(output, "%c %lu", c, id);

            // parse lits
            int nb_lits = 0;
            while (true) {
                int lit = file_reader_read_vbl_int(reader);
                fprintf(output, " %i", lit);
                if (!lit) break;
                int_vec_push(lits_buffer, lit);
                nb_lits++;
            }
            siphash_update((u8*)lits_buffer->data, nb_lits * sizeof(int));

            // parse hints
            int nb_hints = 0;
            while (true) {
                u64 hint = (u64)file_reader_read_vbl_sl(reader);
                fprintf(output, " %lu", hint);
                if (!hint) break;
                u64_vec_push(hints_buffer, hint);
                nb_hints++;
            }

            fprintf(output, "\n");
            nb_produced++;

        } else if (c == TRUSTED_CHK_CLS_IMPORT) {
            int_vec_resize(lits_buffer, 0);

            u64 id = (u64)file_reader_read_vbl_sl(reader);
            fprintf(output, "%c %lu", c, id);
            
            // parse lits
            int nb_lits = 0;
            while (true) {
                int lit = file_reader_read_vbl_int(reader);
                fprintf(output, " %i", lit);
                if (!lit) break;
                int_vec_push(lits_buffer, lit);
                nb_lits++;
            }

            fprintf(output, "\n");
            nb_imported++;

        } else if (c == TRUSTED_CHK_CLS_DELETE) {
            u64_vec_resize(hints_buffer, 0);
            fprintf(output, "%c", c);

            // parse hints
            int nb_hints = 0;
            while (true) {
                u64 hint = (u64)file_reader_read_vbl_sl(reader);
                fprintf(output, " %lu", hint);
                if (!hint) break;
                u64_vec_push(hints_buffer, hint);
                nb_hints++;
            }

            fprintf(output, "\n");
            nb_deleted += nb_hints;

        } else {
            printf("* [ERROR] Invalid directive %d\n", c);
            break;
        }
    }

    file_reader_end(reader);
    siphash_free();
    int_vec_free(lits_buffer);
    u64_vec_free(hints_buffer);
    fclose(output);
    exit(0);
}
