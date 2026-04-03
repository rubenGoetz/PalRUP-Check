
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

    u64 id_cnt = 0;
    
    while (true) {
        // read id
        u64 id = palrup_utils_read_ul(input);
        if (!id) break;
        
        // write id
        fprintf(output, "%lu\n", id);
        id_cnt++;
    }

    fclose(input);
    fclose(output);
    exit(0);
}
