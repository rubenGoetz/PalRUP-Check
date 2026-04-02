
#pragma once

#include <stdio.h>
#include <stdbool.h>

#include "utils/define.h"

#define TYPE u8
#define TYPED(THING) u8_ ## THING
#include "vec.h"
#undef TYPED
#undef TYPE

struct back_file_reader {
    FILE* file;
    u64 read_idx;
    bool eof;
    struct u8_vec* buffer;
};

struct back_file_reader* back_file_reader_init(FILE* file, u64 capacity);
void back_file_reader_free(struct back_file_reader* bfr);

long back_file_reader_fill(struct back_file_reader* bfr);
bool back_file_reader_eof(struct back_file_reader* bfr);

// TODO: make uncoded binary readable

char back_file_reader_decode_char(struct back_file_reader* bfr, u64 idx);
long back_file_reader_decode_sl(struct back_file_reader* bfr, u64 idx);
int back_file_reader_decode_int(struct back_file_reader* bfr, u64 idx);

char back_file_reader_vbl_char(struct back_file_reader* bfr);
long back_file_reader_vbl_sl(struct back_file_reader* bfr);
int back_file_reader_vbl_int(struct back_file_reader* bfr);

u64 back_file_reader_get_start_line_idx(struct back_file_reader* bfr);
void back_file_reader_skip_bytes(struct back_file_reader* bfr, size_t n);
void back_file_reader_skip_to_idx(struct back_file_reader* bfr, u64 idx);
