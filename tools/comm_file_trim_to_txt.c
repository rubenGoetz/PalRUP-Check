
#include <stdlib.h>

#include "../src/utils/palrup_utils.h"

#define TYPE int
#define TYPED(THING) int_##THING
#include "../src/vec.h"
#undef TYPED
#undef TYPE

char* PATH_IN;
char* PATH_OUT;

struct int_vec* lits_buffer;

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
        printf("* Need arguments <path_in> and <path_out>. ABORT.\n");
        abort();
    }

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

    u64 id;
    u64 id_cnt = 0;
    while (true) {
        // read id
        u64 nb_read = UNLOCKED_IO(fread)(&id, sizeof(u64), 1, input);
        if (nb_read < 1) break;     // eof reached
        
        // write id
        fprintf(output, "%lu\n", id);
        id_cnt++;
    }

    printf("* read %lu IDs\n", id_cnt);

    fclose(input);
    fclose(output);
    exit(0);
}
