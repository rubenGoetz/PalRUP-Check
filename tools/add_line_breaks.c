
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

int main(int argc, char *argv[]) {
    if (argc == 2) {
        // INIT
        PATH_IN = argv[1];
        PATH_OUT = palrup_utils_malloc(1024 * sizeof(char));
        snprintf(PATH_OUT, 1024, "%s.lineendings", PATH_IN);
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

    siphash_init(SECRET_KEY);
    size_t nb_produced = 0, nb_imported = 0, nb_deleted = 0;

    while (true) {
        char c = fgetc(input);
        if (feof(input)) {
            u8* sig = siphash_digest();
            
            printf("* Meta information on %s:\n", PATH_IN);
            printf("   - %lu clauses produced\n", nb_produced);
            printf("   - %lu clauses imported\n", nb_imported);
            printf("   - %lu deletion lines\n", nb_deleted);
            printf("   - signature: %i, %i, %i, %i\n",
                   (unsigned int)sig[0], (unsigned int)sig[4], (unsigned int)sig[8], (unsigned int)sig[12]);

            break;

        } else if (c == TRUSTED_CHK_CLS_PRODUCE) {
            
            while (c != 0) {        // a id <lits>
                fputc(c, output);
                c = fgetc(input);
            }
            fputc(c, output);
            c = fgetc(input);
            while (c != 0) {        // <hints>
                fputc(c, output);
                c = fgetc(input);
            }
            fputc(c, output);

            fputc(0, output);       // additional linebreak 0

            nb_produced++;

        } else if (c == TRUSTED_CHK_CLS_IMPORT) {
            
            while (c != 0) {        // i id <lits>
                fputc(c, output);
                c = fgetc(input);
            }
            fputc(c, output);

            fputc(0, output);       // additional linebreak 0

            nb_imported++;

        } else if (c == TRUSTED_CHK_CLS_DELETE) {
            
            while (c != 0) {        // d <ids>
                fputc(c, output);
                c = fgetc(input);
            }
            fputc(c, output);

            fputc(0, output);       // additional linebreak 0

            nb_deleted++;

        } else {
            printf("* [ERROR] Invalid directive %d\n", c);
            break;
        }
    }

    siphash_free();
    fclose(output);
    exit(0);
}
