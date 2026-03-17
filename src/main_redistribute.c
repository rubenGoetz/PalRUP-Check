
#include "utils/palrup_utils.h" 
#include "redist_handler.h"

int main(int argc, char *argv[]) {
    const char *working_path = "";
    u64 num_solvers = 0, pal_id = 0, redist_strat = 3, read_buffer_KB = 1024;
    for (int i = 1; i < argc; i++) {
        palrup_utils_try_match_arg(argv[i], "-working-path=", &working_path);
        palrup_utils_try_match_num(argv[i], "-num-solvers=", &num_solvers);
        palrup_utils_try_match_num(argv[i], "-pal-id=", &pal_id);
        palrup_utils_try_match_num(argv[i], "-read-buffer-KB=", &read_buffer_KB);
        palrup_utils_try_match_num(argv[i], "-redist-strat=", &redist_strat);
    }

    snprintf(palrup_utils_msgstr, 512, "Option list: -working-path=%s -num-solvers=%lu -pal-id=%lu -redist-strat=%lu -read-buffer-KB=%lu",
             working_path, num_solvers, pal_id, redist_strat, read_buffer_KB);
    palrup_utils_log(palrup_utils_msgstr);
    u64 read_buffer_size = read_buffer_KB * 1024;  // convert to bytes
    redist_handler_init(working_path, pal_id, num_solvers, redist_strat, read_buffer_size);
    redist_handler_run();
    redist_handler_end();
    fflush(stdout);
    return 0;
}
