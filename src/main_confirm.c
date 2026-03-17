
#include "utils/palrup_utils.h"
#include "clause_finder.h"

int main(int argc, char *argv[]) {
    const char *palrup_path = "", *working_path = "";
    u64 num_solvers = 0, pal_id = 0, redist_strat = 3, read_buffer_KB = 1024;
    bool use_palrup_binary = true;
    for (int i = 1; i < argc; i++) {
        palrup_utils_try_match_arg(argv[i], "-palrup-path=", &palrup_path);
        palrup_utils_try_match_arg(argv[i], "-working-path=", &working_path);
        palrup_utils_try_match_num(argv[i], "-num-solvers=", &num_solvers);
        palrup_utils_try_match_num(argv[i], "-pal-id=", &pal_id);
        palrup_utils_try_match_num(argv[i], "-read-buffer-KB=", &read_buffer_KB);
        palrup_utils_try_match_num(argv[i], "-redist-strat=", &redist_strat);
        palrup_utils_try_match_bool(argv[i], "-palrup_binary=", &use_palrup_binary);
    }

    snprintf(palrup_utils_msgstr, 512, "Option list: -palrup-path=%s -working-path=%s -num-solvers=%lu -pal-id=%lu -redist-strat=%lu -read-buffer-KB=%lu",
             palrup_path, working_path, num_solvers, pal_id, redist_strat, read_buffer_KB);
    palrup_utils_log(palrup_utils_msgstr);
    u64 read_buffer_size = read_buffer_KB * 1024; // convert to bytes
    clause_finder_init(palrup_path, working_path, pal_id, num_solvers, redist_strat, read_buffer_size, use_palrup_binary);
    clause_finder_run();
    clause_finder_end();
    fflush(stdout);
    return 0;
}
