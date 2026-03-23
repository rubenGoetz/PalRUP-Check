
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>

#include "file_reader.h"
#include "utils/palrup_utils.h"

// TODO: refactor and cleanup logs

void fill_buffer(struct file_reader* reader) {
    u64 read_size;
    
    if (UNLIKELY(reader->remaining_bytes < reader->buffer_size)) {
        read_size = reader->remaining_bytes;
    } else {
        read_size = reader->buffer_size;
    }
    if (UNLIKELY(read_size == 0)) {
        return;
    }
    u64 objs_read = UNLOCKED_IO(fread)(reader->read_buffer, read_size, 1, reader->buffered_file);
    if (UNLIKELY(objs_read == 0)) {
        printf("ERROR\n");
        printf("buffer_size: %lu\n", reader->buffer_size);
        printf("remaining bytes: %lu\n", reader->remaining_bytes);
        printf("read size: %lu\n", read_size);
    }
    reader->pos = reader->read_buffer;
    reader->end = reader->pos + read_size;
    reader->remaining_bytes -= read_size;
    reader->actual_buffer_size = read_size;
}

struct file_reader* file_reader_init(u64 buffer_size_bytes, FILE* file, int pal_id) {
    struct file_reader* reader = (struct file_reader*)palrup_utils_malloc(sizeof(struct file_reader));

    struct stat st;
    int fd = fileno(file);
    fstat(fd, &st);
    reader->total_bytes = st.st_size;
    reader->remaining_bytes = st.st_size;

    reader->pal_id = pal_id;
    reader->buffered_file = file;
    reader->buffer_size = buffer_size_bytes;
    reader->read_buffer = (char*)palrup_utils_malloc(buffer_size_bytes);
    reader->actual_buffer_size = 0;
    reader->pos = reader->read_buffer;
    reader->end = reader->pos;
    reader->fragment_buffer.size = 0;
    reader->fragment_buffer.capacity = 1;
    reader->fragment_buffer.data = palrup_utils_calloc(1, sizeof(u8));

    fill_buffer(reader);

    return reader;
}

void file_reader_seek(u64 byte_pos, struct file_reader* reader) {
    long searched_byte = (long)byte_pos;
    if (searched_byte > reader->total_bytes){
        printf("Error: Seeking beyond file size.\n");
        return;
    }
    long loaded_end = reader->total_bytes - reader->remaining_bytes;
    long loaded_start = loaded_end - reader->actual_buffer_size;

    if (loaded_start <= searched_byte && searched_byte < loaded_end){ // seeked position is loaded in buffer
        reader->pos = reader->read_buffer + (searched_byte - loaded_start);
    }
    else if (loaded_end <= searched_byte){ // seeked position comes later
        long current_pos = (reader->pos - reader->read_buffer) + loaded_start;
        file_reader_skip_bytes(searched_byte - current_pos, reader);
    }
    else{ // seeked position comes earlier
        long offset_front = reader->buffer_size/10L;
        long set_position = MAX(searched_byte - offset_front, 0L);
        reader->remaining_bytes = reader->total_bytes - set_position;
        fseek(reader->buffered_file, set_position, SEEK_SET);
        fill_buffer(reader);
        reader->pos += searched_byte - set_position;
    }
}

void file_reader_end(struct file_reader* reader){
    fclose(reader->buffered_file);
    free(reader->fragment_buffer.data);
    free(reader->read_buffer);
    free(reader);
}

bool file_reader_check_bounds(u64 nb_bytes, struct file_reader* reader){
    
    long bytes_till_end = reader->end - reader->pos;
    if (UNLIKELY(bytes_till_end == 0)){
        fill_buffer(reader);
    }
    bytes_till_end = reader->end - reader->pos;
    if(((long)nb_bytes) > bytes_till_end){
        u8_vec_resize(&reader->fragment_buffer, nb_bytes);
        memcpy(reader->fragment_buffer.data, reader->pos, bytes_till_end); // put first part of the data in the fragment buffer
        nb_bytes -= bytes_till_end;
        u8* fragment_pos = reader->fragment_buffer.data + bytes_till_end;
        while ((long)nb_bytes > reader->buffer_size){
            fill_buffer(reader);
            memcpy(fragment_pos, reader->pos, reader->buffer_size);
            fragment_pos += reader->buffer_size;
            nb_bytes -= reader->buffer_size;
        }
        fill_buffer(reader);
        memcpy(fragment_pos, reader->pos, nb_bytes); // put the rest of the data in the fragment buffer
        
        reader->pos += nb_bytes;
        

        return false;
    }
    return true;
}

bool file_reader_eof_reached(struct file_reader* reader) {
    return (reader->end <= reader->pos) && (!reader->remaining_bytes);
}

int file_reader_read_int(struct file_reader* reader){
    int res;
    if (file_reader_check_bounds(sizeof(int), reader)){
        res = *((int*)(reader->pos));
        reader->pos += sizeof(int);
    } else {
        res = *((int*)(reader->fragment_buffer.data));
    }
    return res;
}

void file_reader_read_ints(int* data, u64 nb_ints, struct file_reader* reader){
    if (file_reader_check_bounds(nb_ints * sizeof(int), reader)){
        memcpy(data, reader->pos, nb_ints * sizeof(int));  
        reader->pos += nb_ints * sizeof(int);
    } else {
        memcpy(data, reader->fragment_buffer.data, nb_ints * sizeof(int));
    }
}


u64  file_reader_read_ul(struct file_reader* reader){
    u64 res;
    if (file_reader_check_bounds(sizeof(u64), reader)){
        res = *((u64*)(reader->pos));
        reader->pos += sizeof(u64);
    } else {
        res = *((u64*)(reader->fragment_buffer.data));
    }
    return res;
}
void file_reader_read_uls(u64* data, u64 nb_uls, struct file_reader* reader){
    if (file_reader_check_bounds(nb_uls * sizeof(u64), reader)){
        memcpy(data, reader->pos, nb_uls * sizeof(u64));
        reader->pos += nb_uls * sizeof(u64);
    } else {
        memcpy(data, reader->fragment_buffer.data, nb_uls * sizeof(u64));
    }
}

char file_reader_read_char(struct file_reader* reader){
    char res;
    file_reader_check_bounds(sizeof(char), reader); // makes shure that the char is in the buffer
    res = *(reader->pos);
    reader->pos += 1;
    
    return res;
}


void file_reader_skip_bytes(u64 nb_bytes, struct file_reader* reader){
    long bytes_till_end = reader->end - reader->pos;
    while (UNLIKELY(bytes_till_end <= (long)nb_bytes)){ // reader has reached the end of the buffer
        fill_buffer(reader);
        nb_bytes -= bytes_till_end;
        bytes_till_end = reader->buffer_size;
    }
    reader->pos += nb_bytes; // move the reader to the right position
    
}


int file_reader_read_vbl_int(struct file_reader* reader) {
    long bytes_till_end = reader->end - reader->pos;
    u64 coefficient = 1;
    unsigned int tmp = 0;

    if (bytes_till_end <= 0)
        fill_buffer(reader);

    while (*(reader->pos) & 128) {
        tmp += coefficient * (*(reader->pos)++ & 127);
        coefficient *= 128;

        if (reader->end - reader->pos <= 0)
            fill_buffer(reader);
    }
    tmp += coefficient * *(reader->pos)++;

    // calculate sign. odds map to negatives, even to positive
    if (tmp % 2)
        return -(int)((tmp - 1) / 2);
    return (int)(tmp / 2);
}

long file_reader_read_vbl_sl(struct file_reader* reader) {
    long bytes_till_end = reader->end - reader->pos;
    u64 coefficient = 1;
    unsigned long tmp = 0;

    if (bytes_till_end <= 0)
        fill_buffer(reader);

    while (*(reader->pos) & 128) {
        tmp += coefficient * (*(reader->pos)++ & 127);
        coefficient *= 128;

        if (reader->end - reader->pos <= 0)
            fill_buffer(reader);
    }
    tmp += coefficient * *(reader->pos)++;

    // calculate sign. odds map to negatives, even to positive
    if (tmp % 2)
        return -(long)((tmp - 1) / 2);
    return (long)(tmp / 2);
}

u64 file_reader_read_vbl_ul(struct file_reader* reader) {
    long bytes_till_end = reader->end - reader->pos;
    u64 coefficient = 1;
    u64 tmp = 0;

    if (bytes_till_end <= 0)
        fill_buffer(reader);

    while (*(reader->pos) & 128) {
        tmp += coefficient * (*(reader->pos)++ & 127);
        coefficient *= 128;

        if (reader->end - reader->pos <= 0)
            fill_buffer(reader);
    }
    tmp += coefficient * *(reader->pos)++;
    
    return tmp;
}

void file_reader_read_vbl_ints(int* data, u64 nb_ints, struct file_reader* reader) {
    for (size_t i = 0; i < nb_ints; i++)
        data[i] = file_reader_read_vbl_int(reader);
}

void file_reader_read_vbl_uls(u64* data, u64 nb_uls, struct file_reader* reader) {
    for (size_t i = 0; i < nb_uls; i++)
        data[i] = file_reader_read_vbl_int(reader);
}

inline char file_reader_read_vbl_char(struct file_reader* reader) {
    return file_reader_read_char(reader);
}
