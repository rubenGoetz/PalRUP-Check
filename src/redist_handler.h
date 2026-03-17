
#pragma once

void redist_handler_init(const char* working_path, unsigned long pal_id, unsigned long num_solvers, unsigned long redist_strategy, unsigned long read_buffer_size);
void redist_handler_run();
void redist_handler_end();
