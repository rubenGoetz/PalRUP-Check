
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
        u64 nb_produced         = 0;
        u64 nb_imported         = 0;
        u64 nb_imported_used    = 0;
        u64 nb_deleted          = 0;
        u64 nb_flushes          = 0;
        u64 nb_flush_merges     = 0;

        operator std::string() const {
            return "import_extractor_stats: "
                      "nb_produced:" + to_string(nb_produced) + " "
                    + "nb_imported:" + to_string(nb_imported) + " "
                    + "nb_imported_used:" + to_string(nb_imported_used) + " "
                    + "nb_deleted:" + to_string(nb_deleted) + " "
                    + "nb_flushes:" + to_string(nb_flushes) + " "
                    + "nb_flush_merges:" + to_string(nb_flush_merges);
        }
    } stats;

    public:
        ImportExtractor(struct options* options);
        ~ImportExtractor();

        void run();

    private:
        void init_strat_3(struct options* options);
        void empty_clause_found();
        void log_id(u64 id);
        void flush_Q(float alpha, int file_id);
        int get_file_id(u64 id);

    // make internals accessible to unit tests
    friend class ImportExtractorTest;
};
