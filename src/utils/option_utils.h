
#pragma once

#include <stdbool.h>

#include "define.h"

void option_utils_try_match_arg(const char* arg, const char* opt, const char** out);
void option_utils_try_match_num(const char* arg, const char* opt, u64* out);
void option_utils_try_match_bool(const char* arg, const char* opt, bool* out);
void option_utils_try_match_flag(const char* arg, const char* opt, bool* out);
