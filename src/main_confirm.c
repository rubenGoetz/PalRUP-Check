
#include "utils/palrup_utils.h"
#include "options.h"
#include "clause_finder.h"

int main(int argc, char *argv[]) {
    options_init();
    for (int i = 1; i < argc; i++) {
        options_try_match_arg(argv[i], "-palrup-path=", &(options.palrup_path));
        options_try_match_arg(argv[i], "-working-path=", &(options.working_path));
        options_try_match_ul(argv[i], "-num-solvers=", &(options.num_solvers));
        options_try_match_ul(argv[i], "-pal-id=", &(options.pal_id));
        options_try_match_ul(argv[i], "-read-buffer-KB=", &(options.read_buffer_size));
        options_try_match_ul(argv[i], "-redist-strat=", &(options.redist_strat));
        options_try_match_bool(argv[i], "-palrup_binary=", &(options.palrup_binary));
    }

    options_buffer_sizes_to_bytes();
    options_print();
    clause_finder_init();
    clause_finder_run();
    clause_finder_end();
    fflush(stdout);
    return 0;
}
