
#include <stdlib.h>
#include <glob.h>
#include <dirent.h>
#include <math.h>

#include "test_utils.c"

#define FORMULA_DIR "../formulas"
#define PROOFS_DIR "../proofs"
#define PROOFS_TRIM_DIR "Testing/proofs_trim"
#define WORKING_DIR "Testing/working"
#define NUM_SOLVERS 12

size_t comm_size;

static void clean_proof(char* palrup_path) {
    printf("   * clean up potential .hash files in proof dirs\n");
    glob_t globbuf;
    char pattern[1024];
    snprintf(pattern, 1024, "%s/*/*/*/*.hash", palrup_path);
    glob(pattern, 0, NULL, &globbuf);
    for (size_t i = 0; i < globbuf.gl_pathc; i++)
        remove(globbuf.gl_pathv[i]);

    globfree(&globbuf);
}

static void mv_proof_trim() {
    printf("   * create proof trim\n");
    char cmd[512];
    snprintf(cmd, 512, "rm -r \"%s\" >/dev/null", PROOFS_TRIM_DIR);
    system(cmd);

    snprintf(cmd, 512, "for x in %s/r3unsat_300/*/*/out.palrup.trim; do dir=\"%s/r3unsat_300/\"; y=${x%%.trim}; y=${y#$dir}; mkdir -p \"%s/${y%%/out.palrup}\"; mv $x %s/$y; done >/dev/null",
             PROOFS_DIR, PROOFS_DIR, PROOFS_TRIM_DIR, PROOFS_TRIM_DIR);
    system(cmd);
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

    globfree(&globbuf);
    free(dir);
}

static void run_trim() {
    clean_working();
    char cmd[1024];

    printf("   * run extract imports\n");
    for (size_t i = 0; i < NUM_SOLVERS; i++) {
        snprintf(cmd, 1024, "./palrup_extract_imports -palrup-path=%s/r3unsat_300 -working-path=%s -num-solvers=%i -pal-id=%lu >/dev/null",
                 PROOFS_DIR, WORKING_DIR, NUM_SOLVERS, i);
        int res = system(cmd);
        do_assert(!res);
    }

    printf("   * run redistribute trim\n");
    for (size_t i = 0; i < comm_size; i++) {
        snprintf(cmd, 1024, "./palrup_redistribute_trim -working-path=%s -num-solvers=%i -pal-id=%lu >/dev/null",
                 WORKING_DIR, NUM_SOLVERS, i);
        int res = system(cmd);
        do_assert(!res);
    }

    for (size_t i = 0; i < NUM_SOLVERS; i++) {
        snprintf(cmd, 1024, "./palrup_local_trim -palrup-path=%s/r3unsat_300 -working-path=%s -num-solvers=%i -pal-id=%lu >/dev/null",
                 PROOFS_DIR, WORKING_DIR, NUM_SOLVERS, i);
        int res = system(cmd);
        do_assert(!res);
    }

    mv_proof_trim();
}

static void run_strat3(char* palrup_path) {
    clean_proof(palrup_path);
    clean_working();
    char cmd[1024];

    printf("   * run local check\n");
    for (size_t i = 0; i < NUM_SOLVERS; i++) {
        snprintf(cmd, 1024, "./palrup_local_check -formula-path=%s/r3unsat_300.cnf -palrup-path=%s -working-path=%s -num-solvers=%i -pal-id=%lu >/dev/null",
                 FORMULA_DIR, palrup_path, WORKING_DIR, NUM_SOLVERS, i);
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
        snprintf(cmd, 1024, "./palrup_confirm -palrup-path=%s -working-path=%s -num-solvers=%i -pal-id=%lu >/dev/null",
                 palrup_path, WORKING_DIR, NUM_SOLVERS, i);
        int res = system(cmd);
        do_assert(!res);
    }

    validate();
    clean_proof(palrup_path);
}

int main(int argc, char const *argv[]) {
    UNUSED(argc);
    UNUSED(argv);

    comm_size = pow(palrup_utils_calc_root_ceil(NUM_SOLVERS), 2);

    printf("** run strat 3 with default params\n");
    char path[512];
    snprintf(path, 512, "%s/r3unsat_300", PROOFS_DIR);
    run_strat3(path);

    printf("** run trim\n");
    run_trim();

    printf("** run strat 3 with default params on trimmed proof\n");
    run_strat3(PROOFS_TRIM_DIR);

    return 0;
}
