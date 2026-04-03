
#include <stdio.h>

#include "options.h"
#include "redist_trim_handler.h"

int main(int argc, char *argv[]) {
    struct options* options = options_init();
    for (int i = 1; i < argc; i++) {
        options_try_match_arg(argv[i], "-working-path=", &(options->working_path));
        options_try_match_ul(argv[i], "-num-solvers=", &(options->num_solvers));
        options_try_match_ul(argv[i], "-pal-id=", &(options->pal_id));
        //options_try_match_ul(argv[i], "-read-buffer-KB=", &(options->read_buffer_size));
        //options_try_match_ul(argv[i], "-write-buffer-KB=", &(options->write_buffer_size));
        options_try_match_ul(argv[i], "-redist-strat=", &(options->redist_strat));
    }

    //options_buffer_sizes_to_bytes(options);
    options_print(options);
    redist_trim_handler_init(options);
    redist_trim_handler_run();
    redist_trim_handler_end();
    options_free(options);
    fflush(stdout);
    return 0;
}


