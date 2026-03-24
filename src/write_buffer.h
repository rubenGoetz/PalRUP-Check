
#pragma once

#include <stdio.h>

#include "utils/define.h"

void write_buffer_init(u64 capacity);
void write_buffer_end();

void write_buffer_write_to_file();
void write_buffer_swich_context(FILE* new_file);
void write_buffer_add_clause(u64 clause_id, int nb_lits, int* lits);
