
#pragma once

#include <stdbool.h>

void clause_finder_init(const char* main_path, const char* imports_path, unsigned long solver_id, unsigned long num_solvers, unsigned long redistribution_strategy, unsigned long read_buffer_size, bool use_palrup_binary);
void clause_finder_run();
void clause_finder_end();
