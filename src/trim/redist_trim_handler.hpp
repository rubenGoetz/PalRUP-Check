
#pragma once

#include <string>
#include <vector>
#include <fstream>

extern "C" {
    #include <cstddef>
    #include "../options.h"
}

using namespace std;

class TrimRedistributor {
    size_t num_solvers;
    size_t msg_group_size_in;
    size_t msg_group_size_out;
    size_t column;
    vector<string> in_file_names;
    vector<ifstream> in_files;
    vector<string> out_file_names;
    vector<ofstream> out_files;

    public:
        TrimRedistributor(struct options* options);
        ~TrimRedistributor();

        void run();

    private:
        void init_strat_3(struct options* options);
        void init_msg_group();
        void log_id(u64 id);
        int get_out_file_id(u64 clause_id);
};
