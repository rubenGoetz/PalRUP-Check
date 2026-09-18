
#include <stdio.h>
#include <stdlib.h>
#include "utils/define.h"
#include "utils/palrup_utils.h"
#include "file_reader.h"

file_reader* file_reader_init(u64 buffer_size_bytes, FILE* file, int pal_id) {
    (void)pal_id;
    struct file_reader* reader = (struct file_reader*)palrup_utils_malloc(sizeof(struct file_reader));
    reader->file = file;
    reader->buffer = (char*)palrup_utils_malloc(buffer_size_bytes);
    setvbuf(reader->file, reader->buffer, _IOFBF, buffer_size_bytes);
    return reader;
}
void file_reader_end(file_reader* reader) {
    fclose(reader->file);
    free(reader->buffer);
    free(reader);
}

bool file_reader_eof_reached(file_reader* reader) {
    return feof(reader->file);
}

char file_reader_read_vbl_char(file_reader* reader) {
    return UNLOCKED_IO(fgetc)(reader->file);
}
u64 file_reader_read_vbl_sl(file_reader* reader) {
    u64 coefficient = 1;
    unsigned long tmp = 0;
    char c = UNLOCKED_IO(fgetc)(reader->file);

    while (c & 128) {
        tmp += coefficient * (c & 127);
        coefficient *= 128;
        c = UNLOCKED_IO(fgetc)(reader->file);
    }
    tmp += coefficient * c;

    // calculate sign. odds map to negatives, even to positive
    if (tmp % 2)
        return -(long)((tmp - 1) / 2);
    return (long)(tmp / 2);
}
int file_reader_read_vbl_int(file_reader* reader) {
    u64 coefficient = 1;
    unsigned int tmp = 0;
    char c = UNLOCKED_IO(fgetc)(reader->file);

    while (c & 128) {
        tmp += coefficient * (c & 127);
        coefficient *= 128;

        c = UNLOCKED_IO(fgetc)(reader->file);
    }
    tmp += coefficient * c;

    // calculate sign. odds map to negatives, even to positive
    if (tmp % 2)
        return -(int)((tmp - 1) / 2);
    return (int)(tmp / 2);
}
