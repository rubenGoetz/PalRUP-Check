
#include <math.h>

#include "test_utils.h"
#include "../src/backward_file_reader.h"

static void test_back_file_reader_vbl_sl() {
    printf("   * fill file with vbl coded numbers\n");
    FILE* f = fopen("back_file_reader_test_file", "wb+");
    fputc(255, f);      // -127
    fputc(1, f);
    fputc(0, f);        // 0
    fputc(2, f);        // 1
    fputc(254, f);      // 255
    fputc(3, f);
    fputc(255, f);      // -(2^62 - 1)
    fputc(255, f);
    fputc(255, f);
    fputc(255, f);
    fputc(255, f);
    fputc(255, f);
    fputc(255, f);
    fputc(255, f);
    fputc(127, f);
    fputc(254, f);      // 2^62 - 1
    fputc(255, f);
    fputc(255, f);
    fputc(255, f);
    fputc(255, f);
    fputc(255, f);
    fputc(255, f);
    fputc(255, f);
    fputc(127, f);
    fputc(3, f);        // -1
    rewind(f);

    printf("   * create test back_file_reader\n");
    struct back_file_reader* bfr = back_file_reader_init(f, 12);
    do_assert(bfr->buffer->size == 0);
    do_assert(!bfr->eof);

    printf("   * check read data\n");
    long expected[] = {-1, (long)pow(2, 62) - 1, -((long)pow(2, 62) - 1), 255, 1, 0, -127};
    for (size_t i = 0; i < 7; i++)
        do_assert(expected[i] == back_file_reader_vbl_sl(bfr));

    do_assert(back_file_reader_eof(bfr));

    printf("   * clean up\n");
    back_file_reader_free(bfr);
}

static void test_back_file_reader_vbl_int() {
    printf("   * fill file with vbl coded numbers\n");
    FILE* f = fopen("back_file_reader_test_file", "wb+");
    fputc(255, f);      // -127
    fputc(1, f);
    fputc(0, f);        // 0
    fputc(2, f);        // 1
    fputc(254, f);      // 255
    fputc(3, f);
    fputc(255, f);      // -(2^31 - 1)
    fputc(255, f);
    fputc(255, f);
    fputc(255, f);
    fputc(15, f);
    fputc(254, f);      // 2^31 - 1
    fputc(255, f);
    fputc(255, f);
    fputc(255, f);
    fputc(15, f);
    fputc(3, f);        // -1
    rewind(f);

    printf("   * create test back_file_reader\n");
    struct back_file_reader* bfr = back_file_reader_init(f, 7);
    do_assert(bfr->buffer->size == 0);
    do_assert(!bfr->eof);

    printf("   * check read data\n");
    long expected[] = {-1, (long)pow(2, 31) - 1, -((long)pow(2, 31) - 1), 255, 1, 0, -127};
    for (size_t i = 0; i < 7; i++)
        do_assert(expected[i] == back_file_reader_vbl_sl(bfr));

    do_assert(back_file_reader_eof(bfr));

    printf("   * clean up\n");
    back_file_reader_free(bfr);
}

static void test_back_file_reader_get_start_line_idx() {
    printf("   * open proof file\n");
    FILE* f = fopen("../test/vbl_fragment", "rb");
    
    printf("   * create test back_file_reader\n");
    struct back_file_reader* bfr = back_file_reader_init(f, 1024);

    // TODO: test saved idx with fill

    //expected lines: a a i d

    printf("   * check lines\n");
    u64 idx = back_file_reader_get_start_line_idx(bfr);
    do_assert(idx == 53);
    do_assert(back_file_reader_vbl_sl(bfr) == 0);
    do_assert(back_file_reader_vbl_sl(bfr) == 371172);
    do_assert(back_file_reader_vbl_char(bfr) == 'd');
    
    idx = back_file_reader_get_start_line_idx(bfr);
    do_assert(idx == 42);
    do_assert(back_file_reader_vbl_int(bfr) == 0);
    do_assert(back_file_reader_vbl_int(bfr) == 146);
    do_assert(back_file_reader_vbl_int(bfr) == -31);
    do_assert(back_file_reader_vbl_int(bfr) == -53);
    do_assert(back_file_reader_vbl_int(bfr) == -177);
    do_assert(back_file_reader_vbl_sl(bfr) == 293349);
    do_assert(back_file_reader_vbl_char(bfr) == 'i');

    idx = back_file_reader_get_start_line_idx(bfr);
    do_assert(idx == 21);
    do_assert(back_file_reader_vbl_int(bfr) == 0);
    do_assert(back_file_reader_vbl_sl(bfr) == 92076);
    do_assert(back_file_reader_vbl_sl(bfr) == 365832);
    do_assert(back_file_reader_vbl_sl(bfr) == 371316);
    do_assert(back_file_reader_vbl_sl(bfr) == 371940);
    do_assert(back_file_reader_vbl_sl(bfr) == 371736);
    do_assert(back_file_reader_vbl_int(bfr) == 0);
    do_assert(back_file_reader_vbl_int(bfr) == 371988);
    do_assert(back_file_reader_vbl_char(bfr) == 'a');

    idx = back_file_reader_get_start_line_idx(bfr);
    do_assert(idx == 0);
    do_assert(back_file_reader_vbl_int(bfr) == 0);
    do_assert(back_file_reader_vbl_sl(bfr) == 367800);
    do_assert(back_file_reader_vbl_sl(bfr) == 371448);
    do_assert(back_file_reader_vbl_sl(bfr) == 370908);
    do_assert(back_file_reader_vbl_sl(bfr) == 371040);
    // test if '0d' will still be correctly interpreted as hints instead of its own line
    do_assert(back_file_reader_vbl_sl(bfr) == 50);
    do_assert(back_file_reader_vbl_int(bfr) == 0);
    do_assert(back_file_reader_vbl_int(bfr) == -120);
    do_assert(back_file_reader_vbl_int(bfr) == 371976);
    do_assert(back_file_reader_vbl_char(bfr) == 'a');

    do_assert(back_file_reader_eof(bfr));
    do_assert(bfr->read_idx <= 0);
    do_assert(bfr->buffer->size == 0);

    printf("   * clean up\n");
    back_file_reader_free(bfr);
}

static void test_back_file_reader_skip_bytes() {
    printf("   * open proof file\n");
    // contains 57 byte
    FILE* f = fopen("../test/vbl_fragment", "rb");
    
    printf("   * create test back_file_reader\n");
    struct back_file_reader* bfr = back_file_reader_init(f, 20);

    printf("   * skip bytes\n");
    back_file_reader_skip_bytes(bfr, 5);
    do_assert(back_file_reader_vbl_sl(bfr) == 0);
    back_file_reader_skip_bytes(bfr, 10);
    do_assert(back_file_reader_vbl_sl(bfr) == 0);
    back_file_reader_skip_bytes(bfr, 15);
    do_assert(back_file_reader_vbl_sl(bfr) == 0);
    back_file_reader_skip_bytes(bfr, 4);
    do_assert(back_file_reader_vbl_sl(bfr) == 0);
    back_file_reader_skip_bytes(bfr, 13);
    do_assert(back_file_reader_vbl_sl(bfr) == 0);
    back_file_reader_skip_bytes(bfr, 10);
    do_assert(back_file_reader_eof(bfr));

    printf("   * clean up\n");
    back_file_reader_free(bfr);
}

int main(int argc, char const *argv[]) {
    UNUSED(argc);
    UNUSED(argv);
    
    printf("** test back_file_reader_vbl_sl\n");
    test_back_file_reader_vbl_sl();

    printf("** test back_file_reader_vbl_int\n");
    test_back_file_reader_vbl_int();

    printf("** test back_file_reader_get_start_line_idx\n");
    test_back_file_reader_get_start_line_idx();

    printf("** test back_file_reader_skip_bytes\n");
    test_back_file_reader_skip_bytes();

    return 0;
}
