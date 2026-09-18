
#pragma once

#include <stdio.h>
#include <stdlib.h>
#include "utils/define.h"
#include "utils/palrup_utils.h"

struct file_reader {
    FILE* file;
    char* buffer;
};
typedef struct file_reader file_reader;

file_reader* file_reader_init(u64 buffer_size_bytes, FILE* file, int pal_id);
void file_reader_end(file_reader* reader);

bool file_reader_eof_reached(file_reader* reader);

char file_reader_read_vbl_char(file_reader* reader);
u64 file_reader_read_vbl_sl(file_reader* reader);
int file_reader_read_vbl_int(file_reader* reader);
