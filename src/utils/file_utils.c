
#include "define.h"
#include "file_utils.h"

void file_utils_write_vbl_sl(long l, FILE* file) {
    unsigned long tmp = l < 0 ? -l : l;
    tmp = (2 * tmp) + (l < 0);

    while (tmp & (~127UL)) {    // while more than 7 bits remain
        fputc((char)(tmp & 127UL) | 128, file);
        tmp >>= 7;
    }
    fputc((char)tmp, file);
}
void file_utils_write_vbl_int(int i, FILE* file) {
    // encode sign
    unsigned int tmp = i < 0 ? -i : i;
    tmp = 2 * tmp + (i < 0);

    while (tmp & (~127)) {      // while more than 7 bits remain
        fputc((char)(tmp & 127) | 128, file);
        tmp >>= 7;
    }
    fputc((char)tmp, file);
}
void file_utils_write_vbl_char(char c, FILE* file) {
    fputc(c, file);
}
