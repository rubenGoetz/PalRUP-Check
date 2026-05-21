
#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <queue>

extern "C" {
    #include "../options.h"
}

using namespace std;

class ImportExtractor {
    size_t pal_id;
    size_t msg_group_size;
    size_t num_solvers;
    struct file_reader* proof_fragment;
    vector<string> file_names;
    vector<fstream> out_files;
    vector<u64> file_max_ids;
    string unsat_folder;
    vector<priority_queue<u64, vector<u64>, std::greater<u64>>> Qs;
    u64 Q_max;
    float Q_alpha;

    struct import_extractor_stats {
        u64 nb_produced;
        u64 nb_imported;
        u64 nb_imported_used;
        u64 nb_deleted;
    } ie_stats = {0, 0, 0, 0};

    public:
        ImportExtractor(struct options* options);
        ~ImportExtractor();

        void run();

    private:
        void init_strat_3(struct options* options);
        void empty_clause_found();
        void print_stats();
        void log_id(u64 id);
        void flush_Q(float alpha, int file_id);
        int get_file_id(u64 id);
};
