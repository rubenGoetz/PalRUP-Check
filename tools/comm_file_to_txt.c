
#include <stdlib.h>

#include "../src/utils/palrup_utils.h"
#include "../src/clause_flat.h"

#define TYPE int
#define TYPED(THING) int_##THING
#include "../src/vec.h"
#undef TYPED
#undef TYPE

char* PATH_IN;
char* PATH_OUT;

struct int_vec* lits_buffer;

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("* Need arguments <path_in> and <path_out>. ABORT.\n");
        abort();
    }
    
    // INIT
    PATH_IN = argv[1];
    PATH_OUT = argv[2];

    // open files
    FILE* input = fopen(PATH_IN, "rb");
    FILE* output = fopen(PATH_OUT, "w");
    
    if (!input) {
        printf("* Could not open input file at %s. ABORT.\n", PATH_IN);
        abort();
    }

    if (!output) {
        printf("* Could not open output file at %s. ABORT.\n", PATH_OUT);
        abort();
    }

    lits_buffer = int_vec_init(1);
    u64 clause_count = 0;
    
    while (true) {
        // read clause
        u64 id = palrup_utils_read_ul(input);
        if (!id) break;
        int nb_lits = palrup_utils_read_int(input);
        int_vec_resize(lits_buffer, nb_lits);
        palrup_utils_read_ints(lits_buffer->data, nb_lits, input);

        // write cluase
        fprintf(output, "[CLAUSE %lu]\n", clause_count);
        fprintf(output, "  ID: %lu\n", id);
        fprintf(output, "  NB_LITS: %i\n", nb_lits);
        fprintf(output, "  LITS:");
        for (int j = 0; j < nb_lits; j++) {
            fprintf(output, " %i", lits_buffer->data[j]);
        }
        fprintf(output, "\n");

        clause_count++;
    }

    // write sig into spaceholder
    u8 sig[16];
    palrup_utils_read_sig(sig, input);

    if (fgetc(input) != EOF)
        printf("* [ERROR] EOF was not reached.");

    printf("* Meta information on %s:\n", PATH_IN);
    printf("   - %lu clauses contained\n", clause_count);
    printf("   - signature: %i, %i, %i, %i\n",
            (unsigned int)sig[0], (unsigned int)sig[4], (unsigned int)sig[8], (unsigned int)sig[12]);

    fclose(input);
    fclose(output);
    int_vec_free(lits_buffer);
    exit(0);
}
