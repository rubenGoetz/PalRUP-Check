
#include <stdio.h>

#include "options.h"
#include "import_extractor.h"

int main(int argc, char *argv[]) {
    struct options* options = options_init();
    for (int i = 1; i < argc; i++) {
        options_try_match_arg(argv[i], "-palrup-path=", &(options->palrup_path));
        options_try_match_arg(argv[i], "-working-path=", &(options->working_path));
        options_try_match_ul(argv[i], "-num-solvers=", &(options->num_solvers));
        options_try_match_ul(argv[i], "-pal-id=", &(options->pal_id));
        options_try_match_ul(argv[i], "-redist-strat=", &(options->redist_strat));
        //options_try_match_ul(argv[i], "-wriete-buffer-KB=", &(options->write_buffer_size));
        //options_try_match_bool(argv[i], "-palrup_binary=", &(options->palrup_binary));
    }

    //options_buffer_sizes_to_bytes(options);
    options_print(options);
    import_extractor_init(options);
    import_extractor_run();
    import_extractor_end();
    options_free(options);
    fflush(stdout);
    return 0;
}

