
#pragma once

#include <stdbool.h>
#include <stdio.h>

#include "define.h"

#define MSG_LEN 1024

extern char palrup_utils_msgstr[MSG_LEN];

void palrup_utils_log(const char* msg);
void palrup_utils_log_err(const char* msg);

void palrup_utils_exit_eof();

void* palrup_utils_malloc(u64 size);
void* palrup_utils_realloc(void* from, u64 new_size);
void* palrup_utils_calloc(u64 nb_objs, u64 size_per_obj);

bool palrup_utils_read_bool(FILE* file);
int palrup_utils_read_char(FILE* file);
int palrup_utils_read_int(FILE* file);
void palrup_utils_read_ints(int* data, u64 nb_ints, FILE* file);
u64 palrup_utils_read_ul(FILE* file);
void palrup_utils_read_uls(u64* data, u64 nb_uls, FILE* file);
void palrup_utils_read_sig(u8* out_sig, FILE* file);
void palrup_utils_skip_bytes(u64 nb_bytes, FILE* file);

void palrup_utils_write_bool(bool b, FILE* file);
void palrup_utils_write_char(char c, FILE* file);
void palrup_utils_write_int(int i, FILE* file);
void palrup_utils_write_ints(const int* data, u64 nb_ints, FILE* file);
void palrup_utils_write_ul(u64 u, FILE* file);
void palrup_utils_write_uls(const u64* data, u64 nb_uls, FILE* file);

void palrup_utils_write_flat_clause(const void* data, size_t clause_size, FILE* file);
void palrup_utils_write_sig(const u8* sig, FILE* file);

u64 palrup_utils_2d_to_rank(u64 x, u64 y, u64 n);
u64 palrup_utils_rank_to_x(u64 rank, u64 n);
u64 palrup_utils_rank_to_y(u64 rank, u64 n);
