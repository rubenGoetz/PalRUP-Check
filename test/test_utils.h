
#pragma once

#include <stdbool.h>

#include "../src/utils/palrup_utils.h"

#define UNUSED(X) (void)X;

void do_assert(bool cond);
int* generate_sorted_array(int elem_count, int factor);
int* generate_copy_array(int elem_count, int* a);
int* generate_shuffled_copy_array(int elem_count, int* a);
