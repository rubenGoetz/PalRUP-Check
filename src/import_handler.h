
#pragma once

#include "options.h"

void import_handler_init(struct options* options);
void import_handler_log(unsigned long id, const int* literals, int nb_literals);
void import_handler_end();

// make methods available to unit tests
#ifdef UNIT_TEST
#include "clause_flat.h"
#include "heap.h"
void skip_heap_duplicates(clause_ptr c, struct clause_heap* heap);
void flush_heap_to_file(struct clause_heap* clause_heap, int file_id, float flush_ratio);
FILE* get_import_handler_out_file();
char* get_import_handler_out_file_name();
void set_import_handler_out_file(FILE* file);
void set_import_handler_max_id(unsigned long id);
#endif
