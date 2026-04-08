
#include "utils/palrup_utils.h"
#include "options.h"
#include "local_trim.h"

int main(int argc, char *argv[]) {
    struct options* options = options_init();
    for (int i = 0; i < argc; i++) {
        options_try_match_arg(argv[i], "-palrup-path=", &(options->palrup_path));
        options_try_match_arg(argv[i], "-working-path=", &(options->working_path));
        options_try_match_ul(argv[i], "-pal-id=", &(options->pal_id));
        options_try_match_ul(argv[i], "-num-solvers=", &(options->num_solvers));
        options_try_match_ul(argv[i], "-write-buffer-KB=", &(options->write_buffer_size));
        options_try_match_ul(argv[i], "-redist-strat=", &(options->redist_strat));
        options_try_match_bool(argv[i], "-use-checker-comm-files=", &(options->use_checker_comm_files));
    }
    
    options_buffer_sizes_to_bytes(options);
    options_print(options);
    local_trim_init(options);
    local_trim_run();
    local_trim_end();
    fflush(stdout);
    options_free(options);
    return 0;
}

