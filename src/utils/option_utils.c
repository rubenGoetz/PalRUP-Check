
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "option_utils.h"

bool begins_with(const char* str, const char* prefix) {
    u64 i = 0;
    while (true) {
        if (prefix[i] == '\0') return true;
        if (str[i] == '\0') return prefix[i] == '\0';
        if (str[i] != prefix[i]) return false;
        i++;
    }
}

void option_utils_try_match_arg(const char* arg, const char* opt, const char** out) {
    if (begins_with(arg, opt)) *out = arg + strlen(opt);
}
void option_utils_try_match_num(const char* arg, const char* opt, u64* out) {
    const char* start_of_number = arg + strlen(opt);
    if (begins_with(arg, opt)) *out = strtol(start_of_number, NULL, 10);
}
void option_utils_try_match_bool(const char* arg, const char* opt, bool* out) {
    const char* start_of_number = arg + strlen(opt);
    if (begins_with(arg, opt)) *out = (bool)atoi(start_of_number);
}
void option_utils_try_match_flag(const char* arg, const char* opt, bool* out) {
    if (begins_with(arg, opt)) *out = true;
}
