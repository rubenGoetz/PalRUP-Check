
#include <stdlib.h>
#include <stdio.h>
#include <queue>

#include "redist_trim_handler.hpp"

extern "C" {
    #include "../utils/palrup_utils.h"
}

inline int TrimRedistributor::get_out_file_id(u64 clause_id) {
    // might need to be altered for differing strategies
    return (clause_id % num_solvers) % msg_group_size_out;
}

inline void TrimRedistributor::log_id(u64 id) {
    stats.written_ids++;
    out_files[get_out_file_id(id)].write(reinterpret_cast<char*>(&id), sizeof(u64));
}

void TrimRedistributor::init_msg_group() {
    in_file_names = vector<string>(msg_group_size_in);
    in_files = vector<ifstream>(msg_group_size_in);
    out_file_names = vector<string>(msg_group_size_out);
    out_files = vector<ofstream>(msg_group_size_out);
}

void TrimRedistributor::init_strat_3(struct options* options) {
    msg_group_size_in = palrup_utils_calc_root_ceil(options->num_solvers);
    msg_group_size_out = 1;
    init_msg_group();
    column = options->pal_id % msg_group_size_in;
    size_t dir_hierarchy = options->pal_id / msg_group_size_in;

    // open out files
    out_file_names[0] = string(options->working_path) + "/"
                         + to_string(dir_hierarchy) + "/"
                         + to_string(options->pal_id) + "/"
                         + "out.palrup_trim_import";
    out_files[0].open(out_file_names[0] + "~", ios::binary);
    if (!out_files[0]) {
        snprintf(palrup_utils_msgstr, MSG_LEN, "Could not open import file at %s", (out_file_names[0] + "~").c_str());
        palrup_utils_log_err(palrup_utils_msgstr);
        abort();
    }

    size_t offset = dir_hierarchy * msg_group_size_in;  // row number * pals in row
    for (size_t i = 0; i < msg_group_size_in; i++) {
        // calc input file paths
        in_file_names[i] = string(options->working_path) + "/"
                            + to_string(dir_hierarchy) + "/"
                            + to_string(offset + i) + "/"
                            + "out.palrup_trim_proxy";
        in_files[i].open(in_file_names[i], ios::binary);
    }
} 

TrimRedistributor::TrimRedistributor(struct options* options) {
    num_solvers = options->num_solvers;

    switch (options->redist_strat) {
    // TODO: implement strat 1 and 2
    case 1:
    case 2:
    case 3:
        init_strat_3(options);
        break;
    default:
        snprintf(palrup_utils_msgstr, MSG_LEN, "Redistribution strategy %lu not available.", options->redist_strat);
        palrup_utils_log_err(palrup_utils_msgstr);
        break;
    }
}

void TrimRedistributor::run() {
    // init priority queue
    auto Q = priority_queue<pair<u64,int>, vector<pair<u64,int>>, std::greater<pair<u64,int>>>();
    u64 id = 0;
    for (int i = 0; i < msg_group_size_in; i++) {
        if (in_files[i].fail())
            continue;
        in_files[i].read(reinterpret_cast<char*>(&id), sizeof(u64));
        bool eof = in_files[i].eof();
        if (id && !in_files[i].eof())
            Q.push({id, i});
    }

    int i;
    while (Q.size() > 0) {
        id = Q.top().first;
        i = Q.top().second;
        Q.pop();
        stats.read_ids++;

        // log id if it belongs in column
        if ((id % num_solvers) % msg_group_size_in == column)
            log_id(id);

        if (!in_files[i].eof()) {
            in_files[i].read(reinterpret_cast<char*>(&id), sizeof(u64));
            if (!in_files[i].eof())
                Q.push({id, i});
        }
    }
}

TrimRedistributor::~TrimRedistributor() {
    for (size_t i = 0; i < msg_group_size_out; i++) {
        out_files[i].close();
        rename((out_file_names[i] + "~").c_str(), out_file_names[i].c_str());
    }

    palrup_utils_log(string(stats).c_str());
}
