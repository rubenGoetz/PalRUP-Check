
#include "import_extractor.hpp"

extern "C" {
    #include <stdio.h>
    #include "../options.h"
}

int main(int argc, char *argv[]) {
    struct options* options = options_init();
    for (int i = 1; i < argc; i++) {
        options_try_match_arg(argv[i], "-palrup-path=", &(options->palrup_path));
        options_try_match_arg(argv[i], "-working-path=", &(options->working_path));
        options_try_match_ul(argv[i], "-num-solvers=", &(options->num_solvers));
        options_try_match_ul(argv[i], "-pal-id=", &(options->pal_id));
        options_try_match_ul(argv[i], "-redist-strat=", &(options->redist_strat));
        options_try_match_ul(argv[i], "-q-size-KB=", &(options->q_size));
        options_try_match_float(argv[i], "-q-alpha=", &(options->q_alpha));
        //options_try_match_ul(argv[i], "-write-buffer-KB=", &(options->write_buffer_size));
        //options_try_match_bool(argv[i], "-palrup_binary=", &(options->palrup_binary));
    }

    options_buffer_sizes_to_bytes(options);
    options_print(options);
    ImportExtractor(options).run();
    options_free(options);
    fflush(stdout);
    return 0;
}

