
#pragma once

//#include <string.h>
#include <stdio.h>
//#include <sys/stat.h>
//#include <stdlib.h>
#include "utils/palrup_utils.h"

#define TYPE u8
#define TYPED(THING) u8_ ## THING
#include "vec.h"
#undef TYPED
#undef TYPE

struct file_reader{
    /* data */
    int local_rank;
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

void fill_buffer(struct file_reader* reader);

struct file_reader* file_reader_init(u64 buffer_size_bytes, FILE* file, int local_rank);
bool file_reader_check_bounds(u64 nb_bytes, struct file_reader* reader);
bool file_reader_eof_reached(struct file_reader* reader);

int file_reader_read_int(struct file_reader* reader);
void file_reader_read_ints(int* data, u64 nb_ints, struct file_reader* reader);
long file_reader_read_vbl_sl(struct file_reader* reader);
u64  file_reader_read_ul(struct file_reader* reader);
void file_reader_read_uls(u64* data, u64 nb_uls, struct file_reader* reader);
char file_reader_read_char(struct file_reader* reader);
void file_reader_skip_bytes(u64 nb_bytes, struct file_reader* reader);
void file_reader_seek(u64 byte_pos, struct file_reader* reader);

int file_reader_read_vbl_int(struct file_reader* reader);
u64 file_reader_read_vbl_ul(struct file_reader* reader);
void file_reader_read_vbl_ints(int* data, u64 nb_ints, struct file_reader* reader);
void file_reader_read_vbl_uls(u64* data, u64 nb_uls, struct file_reader* reader);
char file_reader_read_vbl_char(struct file_reader* reader);

void file_reader_end(struct file_reader* reader);