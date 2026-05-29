
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

// TODO: make runtime option
#define USE_RUP true

char* PATH_IN;
char* PATH_OUT;

struct file_reader* reader;
struct int_vec* lits_buffer;

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("* [ERROR] Need arguments <path_in> and <path_out>. ABORT.\n");
        abort();
    }
    
    // INIT
    PATH_IN = argv[1];
    PATH_OUT = argv[2];

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

            if (!USE_RUP) {
                // parse hints
                int nb_hints = 0;
                while (true) {
                    u64 hint = (u64)file_reader_read_vbl_sl(reader);
                    fprintf(output, " %lu", hint);
                    if (!hint) break;
                    nb_hints++;
                }
            }

            fprintf(output, "\n");
            nb_produced++;

        } else if (c == TRUSTED_CHK_CLS_IMPORT) {

            u64 id = (u64)file_reader_read_vbl_sl(reader);
            fprintf(output, "%c %lu", c, id);
            
            // parse lits
            int nb_lits = 0;
            while (true) {
                int lit = file_reader_read_vbl_int(reader);
                fprintf(output, " %i", lit);
                if (!lit) break;
                nb_lits++;
            }

            fprintf(output, "\n");
            nb_imported++;

        } else if (c == TRUSTED_CHK_CLS_DELETE) {
            fprintf(output, "%c", c);

            if (!USE_RUP) {
                // parse hints
                int nb_hints = 0;
                while (true) {
                    u64 hint = (u64)file_reader_read_vbl_sl(reader);
                    fprintf(output, " %lu", hint);
                    if (!hint) break;
                    nb_hints++;
                }
                nb_deleted += nb_hints;
            } else {
                // parse lits
                int nb_lits = 0;
                while (true) {
                    int lit = file_reader_read_vbl_int(reader);
                    fprintf(output, " %i", lit);
                    if (!lit) break;
                    nb_lits++;
                }
                nb_deleted++;
            }

            fprintf(output, "\n");

        } else {
            printf("* [ERROR] Invalid directive %d", c);
            break;
        }
    }

    file_reader_end(reader);
    siphash_free();
    int_vec_free(lits_buffer);
    fclose(output);
    exit(0);
}
