
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "palrup_utils.h"

char palrup_utils_msgstr[MSG_LEN] = "";

void palrup_utils_log(const char* msg) {
    printf("c [CORE %i] %s\n", getpid(), msg);
}
void palrup_utils_log_err(const char* msg) {
    printf("c [CORE %i] [ERROR] %s\n", getpid(), msg);
}

void palrup_utils_exit_eof() {
    palrup_utils_log("end-of-file - terminating");
    exit(0);
}
void exit_oom() {
    palrup_utils_log("allocation failed - terminating");
    exit(0);
}

bool begins_with(const char* str, const char* prefix) {
    u64 i = 0;
    while (true) {
        if (prefix[i] == '\0') return true;
        if (str[i] == '\0') return prefix[i] == '\0';
        if (str[i] != prefix[i]) return false;
        i++;
    }
}

void palrup_utils_try_match_arg(const char* arg, const char* opt, const char** out) {
    if (begins_with(arg, opt)) *out = arg + strlen(opt);
}
void palrup_utils_try_match_num(const char* arg, const char* opt, u64* out) {
    const char* start_of_number = arg + strlen(opt);
    if (begins_with(arg, opt)) *out = strtol(start_of_number, NULL, 10);
}
void palrup_utils_try_match_bool(const char* arg, const char* opt, bool* out) {
    const char* start_of_number = arg + strlen(opt);
    if (begins_with(arg, opt)) *out = (bool)atoi(start_of_number);
}
void palrup_utils_try_match_flag(const char* arg, const char* opt, bool* out) {
    if (begins_with(arg, opt)) *out = true;
}

void* palrup_utils_malloc(u64 size) {
    void* res = malloc(size);
    if (!res) exit_oom();
    return res;
}
void* palrup_utils_realloc(void* from, u64 new_size) {
    void* res = realloc(from, new_size);
    if (!res) exit_oom();
    return res;
}
void* palrup_utils_calloc(u64 nb_objs, u64 size_per_obj) {
    void* res = calloc(nb_objs, size_per_obj);
    if (!res) exit_oom();
    return res;
}

void palrup_utils_read_objs(void* data, size_t size, size_t nb_objs, FILE* file) {
    u64 nb_read = UNLOCKED_IO(fread)(data, size, nb_objs, file);
    if (nb_read < nb_objs) palrup_utils_exit_eof();
}
bool palrup_utils_read_bool(FILE* file) {
    int res = UNLOCKED_IO(fgetc)(file);
    if (res == EOF) palrup_utils_exit_eof();
    return res ? 1 : 0;
}
int palrup_utils_read_char(FILE* file) {
    int res = UNLOCKED_IO(fgetc)(file);
    if (res == EOF) palrup_utils_exit_eof();
    return res;
}
int palrup_utils_read_int(FILE* file) {
    int i;
    palrup_utils_read_objs(&i, sizeof(int), 1, file);
    return i;
}
void palrup_utils_read_ints(int* data, u64 nb_ints, FILE* file) {
    palrup_utils_read_objs(data, sizeof(int), nb_ints, file);
}
u64 palrup_utils_read_ul(FILE* file) {
    u64 u;
    palrup_utils_read_objs(&u, sizeof(u64), 1, file);
    return u;
}
void palrup_utils_read_uls(u64* data, u64 nb_uls, FILE* file) {
    palrup_utils_read_objs(data, sizeof(u64), nb_uls, file);
}
void palrup_utils_read_sig(u8* out_sig, FILE* file) {
    signature dummy;
    if (!out_sig) out_sig = dummy;
    palrup_utils_read_objs(out_sig, sizeof(int), 4, file);
}
void palrup_utils_skip_bytes(u64 nb_bytes, FILE* file) {
    u8 dummy[nb_bytes];
    palrup_utils_read_objs(dummy, 1, nb_bytes, file);
}

void write_objs(const void* data, size_t size, size_t nb_objs, FILE* file) {
    u64 nb_read = UNLOCKED_IO(fwrite)(data, size, nb_objs, file);
    if (nb_read < nb_objs) palrup_utils_exit_eof();
}
void palrup_utils_write_char(char c, FILE* file) {
    int res = UNLOCKED_IO(fputc)(c, file);
    if (res == EOF) palrup_utils_exit_eof();
}
void palrup_utils_write_bool(bool b, FILE* file) {
    int res = UNLOCKED_IO(fputc)(b ? 1 : 0, file);
    if (res == EOF) palrup_utils_exit_eof();
}
void palrup_utils_write_int(int i, FILE* file) {
    write_objs(&i, sizeof(int), 1, file);
}
void palrup_utils_write_ints(const int* data, u64 nb_ints, FILE* file) {
    write_objs(data, sizeof(int), nb_ints, file);
}
void palrup_utils_write_ul(u64 u, FILE* file) {
    write_objs(&u, sizeof(u64), 1, file);
}
void palrup_utils_write_uls(const u64* data, u64 nb_uls, FILE* file) {
    write_objs(data, sizeof(u64), nb_uls, file);
}
void palrup_utils_write_flat_clause(const void* data, size_t clause_size, FILE* file) {
    write_objs(data, clause_size, 1, file);
}
void palrup_utils_write_sig(const u8* sig, FILE* file) {
    write_objs(sig, sizeof(int), 4, file);
}

u64 palrup_utils_2d_to_rank(u64 x, u64 y, u64 n) {
    return y * n + x;
}
u64 palrup_utils_rank_to_x(u64 rank, u64 n) {
    return rank % n;
}
u64 palrup_utils_rank_to_y(u64 rank, u64 n) {
    return rank / n;
}
