
#include <stdlib.h>

#include "../src/utils/palrup_utils.h"
#include "../src/clause_flat.h"
#include "../src/comm_sig.h"
#include "../src/secret.h"

#define TYPE int
#define TYPED(THING) int_##THING
#include "../src/vec.h"
#undef TYPED
#undef TYPE

char* PATH_IN;
struct int_vec* lits_buffer;

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("* Need argument <path_in>. ABORT.\n");
        exit(1);
    }
    
    printf("* init\n");
    // INIT
    PATH_IN = argv[1];

    FILE* file = fopen(PATH_IN, "rb");
    if (!file) {
        printf("* Could not open file. ABORT.\n");
        exit(1);
    }

    struct comm_sig* expected_sig = comm_sig_init(SECRET_KEY_2);
    u8 read_sig[16];
    lits_buffer = int_vec_init(1);

    printf("* compute signature for clauses..\n");
    size_t clause_count = 0;
    while (true) {
        u64 id = palrup_utils_read_ul(file);
        if (!id) break;
        int nb_lits = palrup_utils_read_int(file);
        int_vec_resize(lits_buffer, nb_lits);
        palrup_utils_read_ints(lits_buffer->data, nb_lits, file);
        comm_sig_update_clause(expected_sig, id, lits_buffer->data, nb_lits);
        clause_count++;
    }

    printf("* read signature..\n");
    palrup_utils_read_sig(read_sig, file);
    u8* expected_sig_dig = comm_sig_digest(expected_sig);

    printf("* assert EOF\n");
    if (!(fgetc(file) == EOF))
        printf("* [ERROR] EOF was not reached.\n");;

    printf("* assert signatures\n");
    bool match = true;
    for (size_t i = 0; i < 16; i++) {
        if (read_sig[i] != expected_sig_dig[i])
            match = false;
    }
    
    if (match)
        printf("* [SUCCESS] written signature matches recalculated one :)\n");
    else
        printf("* [FAILURE] written signature does not match recalculated one :(\n");
    printf("* calculated signatute: %i, %i, %i, %i\n",
            (unsigned int)expected_sig_dig[0], (unsigned int)expected_sig_dig[4], (unsigned int)expected_sig_dig[8], (unsigned int)expected_sig_dig[12]);
    printf("* read signature: %i, %i, %i, %i\n",
            (unsigned int)read_sig[0], (unsigned int)read_sig[4], (unsigned int)read_sig[8], (unsigned int)read_sig[12]);

    free(expected_sig_dig);
    comm_sig_free(expected_sig);
    fclose(file);
    int_vec_free(lits_buffer);
    
    return 0;
}
