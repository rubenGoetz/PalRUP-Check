
#pragma once

#include <stdbool.h>

void local_checker_init(const char* formula_path, const char* palrup_path, const char* working_path, unsigned long pal_id, unsigned long num_solvers, unsigned long redist_strat, unsigned long read_buffer_size, bool use_palrup_binary);
void local_checker_end();
int local_checker_run();
