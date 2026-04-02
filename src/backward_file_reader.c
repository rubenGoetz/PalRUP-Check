
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "backward_file_reader.h"
#include "utils/palrup_utils.h"

struct back_file_reader* back_file_reader_init(FILE* file, u64 capacity) {
    struct back_file_reader* bfr = (struct back_file_reader*)palrup_utils_malloc(sizeof(struct back_file_reader));
    bfr->file = file;
    fseek(bfr->file, 0, SEEK_END);
    bfr->eof = false;
    bfr->buffer = u8_vec_init(capacity);
    bfr->read_idx = 0;

    return bfr;
}
void back_file_reader_free(struct back_file_reader* bfr) {
    if (bfr->file)
        fclose(bfr->file);
    u8_vec_free(bfr->buffer);
    free(bfr);
}

long back_file_reader_fill(struct back_file_reader* bfr) {
    if (back_file_reader_eof(bfr))
        return 0;

    FILE* file = bfr->file;
    struct u8_vec* buffer = bfr->buffer;
    u8* data = buffer->data;
    
    long to_read = buffer->capacity - buffer->size;
    to_read = ftell(file) < to_read ? ftell(file) : to_read;
    if (buffer->size)
        for (int i = buffer->size - 1; i >= 0; i--)
            data[to_read + i] = data[i];
    buffer->size += to_read;
    bfr->read_idx = buffer->size - 1;

    fseek(file, -to_read, SEEK_CUR);
    u64 nb_read = UNLOCKED_IO(fread)(buffer->data, sizeof(u8), to_read, file);
    if (nb_read != (u64)to_read) {
        palrup_utils_log_err("Could not read file correctly");
        abort();
    }
    fseek(file, -to_read, SEEK_CUR);
    
    return to_read;
}
bool back_file_reader_eof(struct back_file_reader* bfr) {
    return !ftell(bfr->file);
}

// TODO: make uncoded binary readable

char back_file_reader_decode_char(struct back_file_reader* bfr, u64 idx) {
    return bfr->buffer->data[idx];
}
long back_file_reader_decode_sl(struct back_file_reader* bfr, u64 idx) {
    u8* data = bfr->buffer->data;
    u64 local_idx = idx;
    
    long tmp = 0;
    long coefficient = 1;
    while (data[local_idx] & 128) {
        tmp += coefficient * (data[local_idx++] & 127);
        coefficient *= 128;
    }
    tmp += coefficient * data[local_idx];

    if (tmp % 2)
        return -(long)((tmp - 1) / 2);
    return (long)(tmp / 2);
}
int back_file_reader_decode_int(struct back_file_reader* bfr, u64 idx) {
    u8* data = bfr->buffer->data;
    u64 local_idx = idx;
    
    int tmp = 0;
    int coefficient = 1;
    while (data[local_idx] & 128) {
        tmp += coefficient * (data[local_idx++] & 127);
        coefficient *= 128;
    }
    tmp += coefficient * data[local_idx];

    if (tmp % 2)
        return -(int)((tmp - 1) / 2);
    return (int)(tmp / 2);
}

char back_file_reader_vbl_char(struct back_file_reader* bfr) {
    if (bfr->buffer->size < 1)
        back_file_reader_fill(bfr);
    bfr->buffer->size = bfr->read_idx;

    return back_file_reader_decode_char(bfr, bfr->read_idx ? bfr->read_idx-- : 0);
}
long back_file_reader_vbl_sl(struct back_file_reader* bfr) {
    struct u8_vec* buffer = bfr->buffer;
    
    // u64 can consume at most 10 bytes in vbl encoding
    if (buffer->size < 10)
        back_file_reader_fill(bfr);

    // find start of u64
    while (bfr->read_idx > 0 && buffer->data[bfr->read_idx - 1] & 128) {
        bfr->read_idx--;
        if (bfr->read_idx == 0) {
            assert(back_file_reader_eof(bfr));
            break;
        }
    }
    buffer->size = bfr->read_idx;

    return back_file_reader_decode_sl(bfr, bfr->read_idx ? bfr->read_idx-- : 0);
}
int back_file_reader_vbl_int(struct back_file_reader* bfr) {
    struct u8_vec* buffer = bfr->buffer;
    
    // int can consume at most 5 bytes in vbl encoding
    if (buffer->size < 5)
        back_file_reader_fill(bfr);

    // find start of int
    while (buffer->data[bfr->read_idx - 1] & 128) {
        bfr->read_idx--;
        if (bfr->read_idx == 0) {
            assert(back_file_reader_eof(bfr));
            break;
        }
    }
    buffer->size = bfr->read_idx;
    
    return back_file_reader_decode_int(bfr, bfr->read_idx ? bfr->read_idx-- : 0);
}


// return index of start of next line in buffer.
// aborts if no such start can be found, i.e. line can not fit in buffer 
// or file contains errors
u64 back_file_reader_get_start_line_idx(struct back_file_reader* bfr) {
    u8* data = bfr->buffer->data;
    if (bfr->read_idx == 0)
        back_file_reader_fill(bfr);

    long idx = bfr->read_idx;
    long save_point = idx;
    bool found_short_line = false;
    while (true) {
        switch (data[idx--]) {
        case 0:
            if (found_short_line)   // 0 marks hints in an 'a' line
                return save_point;
            continue;
        case TRUSTED_CHK_CLS_DELETE:
        case TRUSTED_CHK_CLS_IMPORT:
            if (idx < 0)
                return 0;
            if (data[idx--] != 0)
                continue;

            // might be only the hints of an 'a' line
            if (!found_short_line) {
                save_point = idx + 2;
                found_short_line = true;
            } else
                return save_point;
            continue;
        case TRUSTED_CHK_CLS_PRODUCE:
            if (idx < 0)
                return 0;
            if (data[idx--] == 0) // found beginning of line
                return idx + 2;
            continue;
        default:
            if (idx < 0) {
                long offset = back_file_reader_fill(bfr);
                idx += offset;
                save_point += offset;
                if (idx < 0) {
                    snprintf(palrup_utils_msgstr, MSG_LEN, "Could not find beginning of line");
                    palrup_utils_log_err(palrup_utils_msgstr);
                    abort();
                }
            }
            continue;
        }
        break;
    }
    assert(false);
    return 0; // should never be reached
}

// skip n bytes, skip to file end if not enough bytes remain
// cannot skip more bytes than fit into buffer
void back_file_reader_skip_bytes(struct back_file_reader* bfr, size_t n) {
    if (bfr->buffer->size < n)
        back_file_reader_fill(bfr);
    
    bfr->read_idx = bfr->read_idx < n ? 0 : bfr->read_idx - n;
    bfr->buffer->size = bfr->read_idx + 1;
}

// skips to given idx < read_ptr
void back_file_reader_skip_to_idx(struct back_file_reader* bfr, u64 idx) {
    assert(idx <= bfr->read_idx);
    bfr->read_idx = idx;
    bfr->buffer->size = idx + 1;
}
