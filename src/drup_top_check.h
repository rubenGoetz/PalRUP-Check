
#pragma once

#include <stdbool.h>
#include "utils/define.h"

void drup_top_check_init(int nb_vars);
void drup_top_check_end();

bool drup_top_check_load(int lit);
bool drup_top_check_end_load();
u64 drup_top_check_get_nb_loaded_clauses();

bool drup_top_check_add(u64 id, const int* lits, int nb_lits);
bool drup_top_check_import(u64 id, const int* lits, int nb_lits);
bool drup_top_check_delete(const int* lits, int nb_lits);

bool drup_top_check_unsat_found();
bool drup_top_check_valid();
u64 drup_top_check_mishaps();
