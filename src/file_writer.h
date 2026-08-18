
#pragma once

#include <stdio.h>
#include <stdbool.h>

#include "utils/define.h"

struct file_writer {
    FILE* file;
    u8* buffer;
    u64 buffer_size;
    u64 buffer_capacity;
};

typedef struct file_writer file_writer;

file_writer* file_writer_init(FILE* file, u64 buffer_capacity);
void file_writer_free(file_writer* fw);

void file_writer_vbl_sl(file_writer* fw, long l);
void file_writer_vbl_int(file_writer* fw, int i);
void file_writer_vbl_char(file_writer* fw, char c);

void file_writer_flush(file_writer* fw);
