
#include <stdlib.h>
#include <glob.h>
#include <dirent.h>
#include <math.h>

#include "test_utils.c"

#define FORMULA_DIR "../formulas"
#define PROOFS_DIR "../proofs"
#define WORKING_DIR "Testing/working"
#define NUM_SOLVERS 12

size_t comm_size;

static void clean_proof() {
    printf("   * clean up potential .hash files in proof dirs\n");
    glob_t globbuf;
    char pattern[1024];
    snprintf(pattern, 1024, "%s/*/*/*/*.hash", PROOFS_DIR);
    glob(pattern, 0, NULL, &globbuf);
    for (size_t i = 0; i < globbuf.gl_pathc; i++)
        remove(globbuf.gl_pathv[i]);
}

static void clean_working() {
    printf("   * clean up working dir\n");
    char cmd[512];
    snprintf(cmd, 512, "rm -r %s 2>/dev/null", WORKING_DIR);
    system(cmd);

    for (size_t i = 0; i < comm_size; i++) {
        int dir_hierarchy = i / palrup_utils_calc_root_ceil(NUM_SOLVERS);
        snprintf(cmd, 512, "mkdir -p %s/%i/%lu", WORKING_DIR, dir_hierarchy, i);
        system(cmd);
    }
}

static void validate() {
    printf("   * validate proof check\n");
    char path[512];
    snprintf(path, 512, "%s/.unsat_found", WORKING_DIR);
    DIR* dir = opendir(path);
    do_assert(dir);

    glob_t globbuf;
    char pattern[512];
    snprintf(pattern, 512, "%s/*/*/.check_ok", WORKING_DIR);
    glob(pattern, 0, NULL, &globbuf);
    do_assert(globbuf.gl_pathc == NUM_SOLVERS);
}

static void run_strat3() {
    clean_proof();
    clean_working();
    char cmd[1024];

    printf("   * run local check\n");
    for (size_t i = 0; i < NUM_SOLVERS; i++) {
        snprintf(cmd, 1024, "./palrup_local_check -formula-path=%s/r3unsat_300.cnf -palrup-path=%s/r3unsat_300 -working-path=%s -num-solvers=%i -pal-id=%lu >/dev/null",
                 FORMULA_DIR, PROOFS_DIR, WORKING_DIR, NUM_SOLVERS, i);
        int res = system(cmd);
        do_assert(!res);
    }

    printf("   * run redistribute\n");
    for (size_t i = 0; i < comm_size; i++) {
        snprintf(cmd, 1024, "./palrup_redistribute -working-path=%s -num-solvers=%i -pal-id=%lu >/dev/null",
                 WORKING_DIR, NUM_SOLVERS, i);
        int res = system(cmd);
        do_assert(!res);
    }
    
    printf("   * run confirm\n");
    for (size_t i = 0; i < NUM_SOLVERS; i++) {
        snprintf(cmd, 1024, "./palrup_confirm -palrup-path=%s/r3unsat_300 -working-path=%s -num-solvers=%i -pal-id=%lu >/dev/null",
                 PROOFS_DIR, WORKING_DIR, NUM_SOLVERS, i);
        int res = system(cmd);
        do_assert(!res);
    }

    validate();
}

int main(int argc, char const *argv[]) {
    UNUSED(argc);
    UNUSED(argv);

    comm_size = pow(palrup_utils_calc_root_ceil(NUM_SOLVERS), 2);

    printf("** run strat 3 with default params\n");
    run_strat3();

    return 0;
}
