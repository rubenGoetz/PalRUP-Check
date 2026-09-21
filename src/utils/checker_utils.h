
#pragma once

#include <stdbool.h>

#include "define.h"

//bool checker_utils_import_unchecked(unsigned long id, const int* literals, int nb_literals);
bool checker_utils_check_hints(unsigned long id, const unsigned long* hints, int nb_hints);
bool checker_utils_equal_signatures(const u8* left, const u8* right);

bool checker_utils_compare_sorted_lits(const int* lits1, const int* lits2, int nb_lits1, int nb_lits2);
bool checker_utils_compare_semi_sorted_lits(const int* sorted_lits, const int* unsorted_lits, int nb_sorted, int nb_unsorted);
bool checker_utils_compare_lits(const int* lits1, const int* lits2, int nb_lits1, int nb_lits2);

int checker_utils_remove_duplicates(int* lits, int nb_lits);

#ifdef UNIT_TEST
bool bin_search(const int* a, int elem, int start, int end);
#endif
