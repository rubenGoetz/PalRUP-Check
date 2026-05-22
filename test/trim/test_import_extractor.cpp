
#include <iostream>
#include <fstream>
#include <filesystem>
#include <ctime>

#include "../../src/trim/import_extractor.hpp"

extern "C" {
    #include "../test_utils.h"
    #include "../../src/options.h"
}

#define QUEUE_CAP 1000
#define ALPHA 0.5
#define PALRUP_PATH "../proofs/r3unsat_300"
#define WORKING_PATH "Testing/import_extractor_test"

// make ImportExtractor accessable
class ImportExtractorTest {
    ImportExtractor extractor;

    public:
        ImportExtractorTest(struct options* options) :
            extractor(options) {};
        void log_id(u64 id) { extractor.log_id(id); };
        void flush_Q(float alpha, int file_id) { extractor.flush_Q(alpha, file_id); };
};

struct options* options;

static void check_written_file() {
    std::cout << "   * check written file" << std::endl;
    ifstream f;
    f.open(WORKING_PATH "/0/0/out.palrup_trim_proxy", ios::binary);
    u64 id;
    u64 old_id = 0, id_count = 0;
    while (true) {
        f.read(reinterpret_cast<char*>(&id), sizeof(u64));
        if (f.eof())
            break;
        do_assert(old_id <= id);
        id_count++;
        old_id = id;
    }
    do_assert(id_count == QUEUE_CAP);
}

static void check_simple_flush() {
    std::cout << "   * init new ImportExtractor" << std::endl;
    ImportExtractorTest *extractor = new ImportExtractorTest(options);

    std::cout << "   * fill Q" << std::endl;
    for (size_t i = 0; i < QUEUE_CAP; i++)
        extractor->log_id((u64)std::rand());

    std::cout << "   * flush first half of Q:" << std::endl;
    extractor->flush_Q(0.5, 0);

    std::cout << "   * flush second half of Q: ";
    extractor->flush_Q(0, 0);
    delete extractor;

    check_written_file();
}

static void check_merge_flush() {
    std::cout << "   * init new ImportExtractor" << std::endl;
    ImportExtractorTest *extractor = new ImportExtractorTest(options);

    std::cout << "   * fill Q" << std::endl;
    for (size_t i = 0; i < QUEUE_CAP - 2; i++)
        extractor->log_id((u64)std::rand());
    extractor->log_id(1000);

    std::cout << "   * flush first half of Q:" << std::endl;
    extractor->flush_Q(0.5, 0);

    std::cout << "   * ensure smaller id than in file" << std::endl;
    extractor->log_id(1);

    std::cout << "   * flush second half of Q: ";
    extractor->flush_Q(0, 0);
    delete extractor;

    check_written_file();
}

static void test_flush_Q() {
    check_simple_flush();
    check_merge_flush();
}

static void init_tests() {
    options = options_init();
    options->q_size = QUEUE_CAP;
    options->q_alpha = ALPHA;
    options->pal_id = 0;
    options->num_solvers = 1;
    options->palrup_path = PALRUP_PATH;
    options->working_path = WORKING_PATH;

    std::filesystem::create_directories(WORKING_PATH "/0/0");

    std::srand(std::time({}));
}

static void wrap_up_tests() {
    options_free(options);
}

int main(int argc, char const *argv[]) {
    UNUSED(argc);
    UNUSED(argv);

    std::cout << "** init tests" << std::endl;
    init_tests();

    std::cout << "** test ImportExtractor.flush_Q" << std::endl;
    test_flush_Q();

    std::cout << "** wrap up tests" << std::endl;
    wrap_up_tests();

    return 0;
}

