
#pragma once

#include "utils/define.h"

typedef void* drup_clause;

drup_clause drup_clause_init(const u64 id, const int* lits, const int nb_lits);
void drup_clause_free(drup_clause c);
u64 drup_clause_get_id(drup_clause c);
int drup_clause_get_nb_lits(drup_clause c);
int* drup_clause_get_lits(drup_clause c);
