
#include <stdlib.h>
#include <string.h>

#include "write_buffer.h"
#include "utils/palrup_utils.h"

#define TYPE u8
#define TYPED(THING) u8_ ## THING
#include "vec.h"
#undef TYPED
#undef TYPE

struct u8_vec* buffer;
FILE* file;

void write_buffer_init(u64 capacity) {
    buffer = u8_vec_init(capacity);
}
void write_buffer_end() {
    write_buffer_write_to_file();
    u8_vec_free(buffer);
}

static inline size_t get_clause_size(int nb_literals) {
    return sizeof(u64) + sizeof(int) + nb_literals * sizeof(int);
}

void write_buffer_write_to_file() {
    if (buffer->size == 0)
        return;

    u64 nb_written = UNLOCKED_IO(fwrite)(buffer->data, buffer->size, 1, file);
    if (nb_written < 1) palrup_utils_exit_eof();

    u8_vec_resize(buffer, 0);
}
void write_buffer_swich_context(FILE* new_file) {
    if (file == new_file)
        return;
    
    if (file)
        write_buffer_write_to_file();
    file = new_file;
}
void write_buffer_add_clause(u64 clause_id, int nb_lits, int* lits) {
    if (buffer->size + get_clause_size(nb_lits) > buffer->capacity)
        write_buffer_write_to_file();

    memcpy(buffer->data + buffer->size, &clause_id, sizeof(u64));
    buffer->size += sizeof(u64);
    memcpy(buffer->data + buffer->size, &nb_lits, sizeof(int));
    buffer->size += sizeof(int);
    memcpy(buffer->data + buffer->size, lits, nb_lits * sizeof(int));
    buffer->size += nb_lits * sizeof(int);
}
