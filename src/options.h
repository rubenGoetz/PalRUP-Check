
#pragma once

#include <stdbool.h>

#include "utils/define.h"

struct options {
    // universally needed
    char* working_path;
    u64 num_solvers;
    u64 pal_id;
    u64 read_buffer_size;   // defaults to 4096 KiB
    u64 redist_strat;       // defaults to 3
    bool drup;              // defaults to false
    #ifdef DRUP_TO_LRUP_CONVERSION
    int convert_to_lrup;    // defaults to 0. 1 to convert and log imports, 2 to not log imports and only convert
    #endif

    // partially needed
    char* formula_path;
    char* palrup_path;
    bool palrup_binary;     // defaults to true
    u64 write_buffer_size;  // defaults to 4096 KiB

    // local_checker
    u64 merge_buffer_size;  // defaults to 4096 KiB
    u64 q_size;             // defaults to 2 * 4096 KiB
    float q_alpha;          // defaults to .5
};

struct options* options_init();
void options_free(struct options* options);

void options_try_match_arg(char* arg, char* opt, char** out);
void options_try_match_ul(const char* arg, const char* opt, u64* out);
void options_try_match_int(const char* arg, const char* opt, int* out);
void options_try_match_float(const char* arg, const char* opt, float* out);
void options_try_match_bool(const char* arg, const char* opt, bool* out);
void options_try_match_flag(const char* arg, const char* opt, bool* out);

void options_buffer_sizes_to_bytes(struct options* options);
void options_print(struct options* options);
