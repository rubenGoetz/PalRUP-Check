
#pragma once

#include <stdbool.h>

#include "utils/define.h"
#include "drup_clause.h"

void drup_check_init(int nb_vars);
void drup_check_end();

int drup_check_load(int lit);
int drup_check_end_load();
u64 drup_check_get_nb_loaded_clauses();

int drup_check_add_axiomatic_clause(u64 id, const int* lits, int nb_lits);
int drup_check_add_clause(u64 id, const int* lits, int nb_lits);
int drup_check_delete_clause(const int* lits, int nb_lits);

bool drup_check_unsat_found();
bool drup_check_formula_loaded();
u64 drup_check_get_clause_id(const int* lits, int nb_lits);
drup_clause find_clause(const int* lits, int nb_lits);

#ifdef UNIT_TEST
struct drup_clause_vec** get_occurences();
#endif
