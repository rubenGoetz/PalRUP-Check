
#include <assert.h>
#include <stdlib.h>

#include "file_writer.h"
#include "utils/palrup_utils.h"

#define MAX_OBJ_LEN 10  // vbl coded u64 has length 10

file_writer* file_writer_init(FILE* file, u64 buffer_capacity) {
    assert(buffer_capacity > MAX_OBJ_LEN);
    file_writer* fw = palrup_utils_malloc(sizeof(file_writer));
    u8* buffer = file ? palrup_utils_malloc(buffer_capacity) : NULL;
    fw->file = file;
    fw->buffer = buffer;
    fw->buffer_size = 0;
    fw->buffer_capacity = buffer_capacity;

    return fw;
}
void file_writer_free(struct file_writer* fw) {
    if (fw->file) {
        file_writer_flush(fw);
        fclose(fw->file);
        free(fw->buffer);
    }
    free(fw);
}

void file_writer_vbl_sl(file_writer* fw, long l){
    if(!fw->file) return;

    // assure enough space
    if (fw->buffer_size + 10 > fw->buffer_capacity)
        file_writer_flush(fw);
    assert(fw->buffer_size + 10 <= fw->buffer_capacity);

    unsigned long tmp = l < 0 ? -l : l;
    tmp = (2 * tmp) + (l < 0);
    while (tmp & (~127UL)) {    // while more than 7 bits remain
        fw->buffer[fw->buffer_size++] = (char)(tmp & 127UL) | 128;
        tmp >>= 7;
    }
    fw->buffer[fw->buffer_size++] = (char)(tmp);
}
void file_writer_vbl_int(file_writer* fw, int i) {
    if(!fw->file) return;
    
    // assure enough space
    if (fw->buffer_size + 5 > fw->buffer_capacity)
        file_writer_flush(fw);
    assert(fw->buffer_size + 10 <= fw->buffer_capacity);

    unsigned int tmp = i < 0 ? -i : i;
    tmp = (2 * tmp) + (i < 0);

    while (tmp & (~127UL)) {    // while more than 7 bits remain
        fw->buffer[fw->buffer_size++] = (char)(tmp & 127UL) | 128;
        tmp >>= 7;
    }
    fw->buffer[fw->buffer_size++] = (char)(tmp);
}
void file_writer_vbl_char(file_writer* fw, char c) {
    if(!fw->file) return;
    
    if (fw->buffer_size >= fw->buffer_capacity)
        file_writer_flush(fw);
    assert(fw->buffer_size < fw->buffer_capacity);

    fw->buffer[fw->buffer_size++] = c;
}

void file_writer_flush(file_writer* fw) {
    if(!fw->file) return;

    assert(fw->buffer_size <= fw->buffer_capacity);
    u64 nb_written = UNLOCKED_IO(fwrite)(fw->buffer, sizeof(u8), fw->buffer_size, fw->file);
    if (nb_written < fw->buffer_size) palrup_utils_exit_eof();
    fw->buffer_size = 0;
}
