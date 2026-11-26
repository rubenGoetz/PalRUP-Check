
#include "utils/palrup_utils.h"
#include "options.h"
#include "redist_handler.h"

int main(int argc, char *argv[]) {
    options_init();
    for (int i = 1; i < argc; i++) {
        options_try_match_arg(argv[i], "-working-path=", &(options.working_path));
        options_try_match_ul(argv[i], "-num-solvers=", &(options.num_solvers));
        options_try_match_ul(argv[i], "-pal-id=", &(options.pal_id));
        options_try_match_ul(argv[i], "-read-buffer-KB=", &(options.read_buffer_size));
        options_try_match_ul(argv[i], "-write-buffer-KB=", &(options.write_buffer_size));
        options_try_match_ul(argv[i], "-redist-strat=", &(options.redist_strat));
    }

    options_buffer_sizes_to_bytes();
    options_print();
    redist_handler_init();
    redist_handler_run();
    redist_handler_end();
    fflush(stdout);
    return 0;
}
