
#include <iostream>
#include <vector>
#include <ctime>
#include <algorithm>
#include <fstream>

#include "../../src/trim/backward_import_merger.hpp"

extern "C" {
    #include "../test_utils.h"
}

#define NUM_IDS_PER_FILE 1000
#define NUM_FILES 5
#define FILE_PREFIX "Testing/backward_import_reader_test"

static void test_next() {
    std::cout << "   * generate ids" << std::endl;
    std::srand(unsigned(std::time(nullptr)));
    std::vector<std::vector<u64>> ids(NUM_FILES, std::vector<u64>(NUM_IDS_PER_FILE));
    for (auto &v : ids) {
        v.front() = 1;  // assure same ID exists in multiple files
        std::generate(v.begin() + 1, v.end() - 5, std::rand);
        // assure some ID exists multiple times
        for (size_t i = NUM_IDS_PER_FILE - 5; i < v.size(); i++)
            v[i] = v[NUM_IDS_PER_FILE - 6];
        std::sort(v.begin(), v.end());
    }

    std::cout << "   * write import files" << std::endl;
    std::vector<std::string> file_paths(0);
    for (int i = 0; i < NUM_FILES; i++) {
        std::ofstream f;
        file_paths.push_back(FILE_PREFIX "_" + to_string(i));
        f.open(file_paths.back(), ios::binary);
        f.write(reinterpret_cast<char*>(ids[i].data()), sizeof(u64) * ids[i].size());
        f.close();
    }

    std::cout << "   * init BackImpMerger" << std::endl; 
    BackImpMerger merger = BackImpMerger(file_paths, 80);
    
    std::cout << "   * check merged sequence" << std::endl;
    u64 imports = 0;
    u64 last_id = -1;
    while (true) {
        auto elem = merger.next();
        if (!elem.first)
            break;
        imports += elem.second;
        do_assert(elem.first < last_id);
        last_id = elem.first;
    }
    do_assert(imports == NUM_FILES * NUM_IDS_PER_FILE);

}

int main(int argc, char const *argv[]) {
    UNUSED(argc);
    UNUSED(argv);

    std::cout << "** test BackImpMerger.next" << std::endl;
    test_next();

    return 0;
}

