
#include <math.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "clause_finder.h"
#include "utils/checker_utils.h"
#include "import_merger.h"
#include "file_reader.h"

#define TYPE int
#define TYPED(THING) int_##THING
#include "vec.h"
#undef TYPED
#undef TYPE

char confirm_folder[512];
u64 n_solvers;         // number of solvers
double root_n;         // square root of number of solvers
size_t comm_size;
u64 redist_strat;  // redistribution_strategy
u64 local_rank;    // solver id
FILE* my_proof;
const u64 empty_ID = -1;
struct siphash* proof_check_hash;
u8 sig_res_reported[16];
struct siphash** import_check_hash;

// Buffering.
struct int_vec** all_lits;
int* left_clauses;
FILE** out_files;
u64 current_ID = empty_ID;
int* current_literals_data;
u64 current_literals_size;
struct file_reader* proof_reader;

struct int_vec* proof_lits;

bool palrup_binary;

void read_literals(int nb_lits) {
    int_vec_reserve(proof_lits, nb_lits);
    file_reader_read_ints(proof_lits->data, nb_lits, proof_reader);
}

void parse(bool* found_T) {
    while (true) {
        int c = file_reader_read_vbl_char(proof_reader);
        if (file_reader_eof_reached(proof_reader)) {
            if (current_ID != empty_ID) {
                char err_str[512];
                snprintf(err_str, 512, "Error: clause left to check rank:%lu ID:%lu", local_rank, current_ID);
                palrup_utils_log_err(err_str);
                exit(1);
            }
            const u8* sig_res_computed = siphash_cls_digest(proof_check_hash);

            if (!checker_utils_equal_signatures(sig_res_computed, sig_res_reported)) {
                palrup_utils_log_err("Signature does not match in Proof!");
                printf("Signature A is: %lu\n", *((u64*)sig_res_computed));
                printf("Signature B is: %lu\n", *((u64*)sig_res_reported));
                exit(1);
            } else {
                char msg[512];
                snprintf(msg, 512, "Signature matches in local rank: %lu", local_rank);
                //palrup_utils_log(msg);
            }
            siphash_cls_free(proof_check_hash);
            for (size_t i = 0; i < comm_size; i++) {
                sig_res_computed = siphash_cls_digest(import_check_hash[i]);
                import_merger_read_sig((int*)sig_res_reported, i);
                if (!checker_utils_equal_signatures(sig_res_computed, sig_res_reported)) {
                    palrup_utils_log_err("Signature does not match in import!");
                    printf("Signature A is: %lu\n", *((u64*)sig_res_computed));
                    printf("Signature B is: %lu\n", *((u64*)sig_res_reported));
                    exit(1);
                } else {
                    char msg[512];
                    snprintf(msg, 512, "Signature matches in import local rank: %lu", local_rank);
                    //palrup_utils_log(msg);
                }
            }
            *found_T = true;
            break;
        } else if (c == TRUSTED_CHK_CLS_PRODUCE) {
            int_vec_resize(proof_lits, 0);

            u64 id = (u64)file_reader_read_vbl_sl(proof_reader);

            siphash_cls_update(proof_check_hash, (u8*)&id, sizeof(u64));
            
            // parse lits
            int nb_lits = 0;
            while (true) {
                int lit = file_reader_read_vbl_int(proof_reader);
                if (!lit) break;
                int_vec_push(proof_lits, lit);
                nb_lits++;
            }

            siphash_cls_update(proof_check_hash, (u8*)proof_lits->data, nb_lits * sizeof(int));
            
            // skip hints
            while (true) {
                u64 hint = file_reader_read_vbl_sl(proof_reader);
                if (!hint) break;
            }

            // skip line
            if (id < current_ID) {
                continue;
            }

            // check if the clause is the same
            if (id == current_ID) {
                if (checker_utils_compare_semi_sorted_lits(current_literals_data, proof_lits->data, current_literals_size, nb_lits)) {
                    break;
                } else {
                    char err_str[512];
                    snprintf(err_str, 512, "literals do not match in proof my rank:%lu ID:%lu", local_rank, current_ID);
                    if (true/*local_rank == 0*/) {
                        printf("current_literals_data %lu: ", current_literals_size);
                        for (u64 i = 0; i < current_literals_size; i++) {
                            printf("%d ", current_literals_data[i]);
                        }
                        printf("\n");
                        printf("proof_lits %i: ", nb_lits);
                        for (int i = 0; i < nb_lits; i++) {
                            printf("%d ", proof_lits->data[i]);
                        }
                        printf("\n");
                    }

                    palrup_utils_log_err(err_str);
                    exit(1);
                }
            }

            if (id > current_ID) {
                char err_str[512];
                snprintf(err_str, 512, "clause not found in proof my rank:%lu ID:%lu", local_rank, current_ID);
                palrup_utils_log_err(err_str);
                exit(1);
            }

        } else if (c == TRUSTED_CHK_CLS_IMPORT) {
            // skip id
            file_reader_read_vbl_sl(proof_reader);

            // skip lits
            while (true) {
                int lit = file_reader_read_vbl_int(proof_reader);
                if (!lit) break;
            }

        } else if (c == TRUSTED_CHK_CLS_DELETE) {
            // skip hints
            while (true) {
                u64 hint = (u64)file_reader_read_vbl_sl(proof_reader);
                if (!hint) break;
            }
            
        } else {
            palrup_utils_log_err("Invalid directive!");
            exit(1);
        }
    }
}

void clause_finder_init(const char* main_path, const char* imports_path, unsigned long solver_id, unsigned long num_solvers, unsigned long redistribution_strategy, unsigned long read_buffer_size, bool use_palrup_binary) {
    redist_strat = redistribution_strategy;
    palrup_binary = use_palrup_binary;
    n_solvers = num_solvers;
    double d_num = (double)n_solvers;
    root_n = sqrt(d_num);
    comm_size = (size_t)ceil(root_n);  // round to nearest integer
    if (redist_strat == 1) {
        comm_size = n_solvers;
    }
    local_rank = solver_id;
    unsigned int dir_hierarchy = local_rank / comm_size;
    proof_lits = int_vec_init(1);

    snprintf(confirm_folder, 512, "%s/%u/%lu/.check_ok", imports_path, dir_hierarchy, local_rank);

    char proof_path[768];
    char finger_print_path[1024];
    snprintf(proof_path, 768, "%s/%u/%lu/out.palrup", main_path, dir_hierarchy, local_rank);
    snprintf(finger_print_path, 1024, "%s.hash", proof_path);
    my_proof = fopen(proof_path, "rb");
    FILE* finger_print = fopen(finger_print_path, "rb");
    if (!finger_print) {
        char msg[1040];
        snprintf(msg, 1040, "Can't open file %s", finger_print_path);
        palrup_utils_log_err(msg);
    }
    palrup_utils_read_sig(sig_res_reported, finger_print);

    proof_check_hash = siphash_cls_init(SECRET_KEY);
    proof_reader = file_reader_init(read_buffer_size, my_proof, local_rank);

    char** file_paths = palrup_utils_malloc(sizeof(char*) * comm_size);
    import_check_hash = palrup_utils_malloc(sizeof(struct siphash*) * comm_size);

    int column = local_rank % comm_size;
    for (size_t i = 0; i < comm_size; i++) {
        file_paths[i] = palrup_utils_malloc(768);
        if (redist_strat == 3){
            int src_rank = (i * comm_size) + column;
            snprintf(file_paths[i], 768, "%s/%lu/%i/out.palrup_import", imports_path, src_rank / comm_size, src_rank);
        } else
            snprintf(file_paths[i], 768, "%s/%u/%lu/%lu.palrup_import", imports_path, dir_hierarchy, local_rank, i);

        import_check_hash[i] = siphash_cls_init(SECRET_KEY);
    }

    import_merger_init(comm_size, file_paths, &current_ID, &current_literals_data, &current_literals_size, read_buffer_size, import_check_hash, NULL);

    // free
    for (size_t i = 0; i < comm_size; i++) {
        free(file_paths[i]);
    }
    free(file_paths);
    fclose(finger_print);
}

void clause_finder_end() {
    for (size_t i = 0; i < comm_size; i++)  {
        siphash_cls_free(import_check_hash[i]);
    }
    free(import_check_hash);
    file_reader_end(proof_reader);
    import_merger_end();
    int_vec_free(proof_lits);
}

void clause_finder_run() {
    bool found_T = false;
    while (!found_T) {
        import_merger_next();

        // skip clauses for different lines
        if (current_ID % n_solvers != local_rank && current_ID != (u64)-1)
            continue;

        parse(&found_T);
    }
    mkdir(confirm_folder, 0777);

    char msg[512];
    snprintf(msg, 512, "Done local_rank=%lu", local_rank);
    palrup_utils_log(msg);
}