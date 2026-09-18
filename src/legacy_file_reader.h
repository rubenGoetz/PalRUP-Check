
#pragma once

#include <stdio.h>
#include "utils/define.h"

#define TYPE u8
#define TYPED(THING) u8_ ## THING
#include "vec.h"
#undef TYPED
#undef TYPE

struct legacy_file_reader{
    /* data */
    int pal_id;
    char* read_buffer;
    struct u8_vec fragment_buffer;
    long buffer_size;
    char* pos;
    char* end;
    long remaining_bytes; // starts as filesize
    long total_bytes; // filesize
    long actual_buffer_size; // may be smaller than buffer_size when near EOF
    FILE* buffered_file;
};

void legacy_fill_buffer(struct legacy_file_reader* reader);

struct legacy_file_reader* legacy_file_reader_init(u64 buffer_size_bytes, FILE* file, int pal_id);
bool legacy_file_reader_check_bounds(u64 nb_bytes, struct legacy_file_reader* reader);
bool legacy_file_reader_eof_reached(struct legacy_file_reader* reader);

int legacy_file_reader_read_int(struct legacy_file_reader* reader);
void legacy_file_reader_read_ints(int* data, u64 nb_ints, struct legacy_file_reader* reader);
u64  legacy_file_reader_read_ul(struct legacy_file_reader* reader);
void legacy_file_reader_read_uls(u64* data, u64 nb_uls, struct legacy_file_reader* reader);
char legacy_file_reader_read_char(struct legacy_file_reader* reader);
void legacy_file_reader_skip_bytes(u64 nb_bytes, struct legacy_file_reader* reader);
void legacy_file_reader_seek(u64 byte_pos, struct legacy_file_reader* reader);

int legacy_file_reader_read_vbl_int(struct legacy_file_reader* reader);
long legacy_file_reader_read_vbl_sl(struct legacy_file_reader* reader);
u64 legacy_file_reader_read_vbl_ul(struct legacy_file_reader* reader);
void legacy_file_reader_read_vbl_ints(int* data, u64 nb_ints, struct legacy_file_reader* reader);
void legacy_file_reader_read_vbl_uls(u64* data, u64 nb_uls, struct legacy_file_reader* reader);
char legacy_file_reader_read_vbl_char(struct legacy_file_reader* reader);

void legacy_file_reader_end(struct legacy_file_reader* reader);