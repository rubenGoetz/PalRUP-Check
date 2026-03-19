
#include "utils/option_utils.h"
#include "utils/palrup_utils.h"
#include "local_checker.h"

int main(int argc, char *argv[]) {
    const char *formula_path = "", *palrup_path = "", *working_path = "";
    u64 num_solvers = 0, pal_id = 0, redist_strat = 3, read_buffer_KB = 1024;
    bool use_palrup_binary = true;
    for (int i = 1; i < argc; i++) {
        option_utils_try_match_arg(argv[i], "-formula-path=", &formula_path);
        option_utils_try_match_arg(argv[i], "-palrup-path=", &palrup_path);
        option_utils_try_match_arg(argv[i], "-working-path=", &working_path);
        option_utils_try_match_num(argv[i], "-num-solvers=", &num_solvers);
        option_utils_try_match_num(argv[i], "-pal-id=", &pal_id);
        option_utils_try_match_num(argv[i], "-read-buffer-KB=", &read_buffer_KB);
        option_utils_try_match_num(argv[i], "-redist-strat=", &redist_strat);
        option_utils_try_match_bool(argv[i], "-palrup-binary=", &use_palrup_binary);
    }

    snprintf(palrup_utils_msgstr, 512, "Option list: -formula-path=%s -palrup-path=%s -working-path=%s -num-solvers=%lu -pal-id=%lu -redist-strat=%lu -palrup-binary=%i -read-buffer-KB=%lu",
             formula_path, palrup_path, working_path, num_solvers, pal_id, redist_strat, use_palrup_binary, read_buffer_KB);
    palrup_utils_log(palrup_utils_msgstr);
    u64 read_buffer_size = read_buffer_KB * 1024; // convert to bytes
    local_checker_init(formula_path, palrup_path, working_path, pal_id, num_solvers, redist_strat, read_buffer_size, use_palrup_binary);
    int res = local_checker_run();
    local_checker_end();
    fflush(stdout);
    return res;
}
