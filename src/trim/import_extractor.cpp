
#include <iostream>
#include <assert.h>

#include "import_extractor.hpp"

extern "C" {
    #include <stdio.h>
    #include <stdlib.h>
    #include <sys/stat.h>

    #include "../utils/palrup_utils.h"
    #include "../file_reader.h"
    #include "../hash.h"
}

inline int ImportExtractor::get_file_id(u64 id) {
    // might need to be altered for differing strategies
    return (id % num_solvers) % msg_group_size;
}

inline void ImportExtractor::log_id(u64 id) {
    int file_id = get_file_id(id);
    auto Q = Qs[file_id];

    Qs[file_id].push(id);
    if (Q.size() >= Q_max - 1)        // flush to file
        flush_Q(Q_alpha, file_id);
}

void ImportExtractor::flush_Q(float alpha, int file_id) {
    auto &Q = Qs[file_id];
    if (Q.size() <= 0)
        return;
    stats.nb_flushes++;    

    if (Q.top() >= file_max_ids[file_id]) {     // no merge necessary
        while (true) {
            out_files[file_id].write((char *)&Q.top(), sizeof(u64));
            if (Q.size() <= (Q_max * alpha) + 1) {
                file_max_ids[file_id] = Q.top();
                Q.pop();
                break;
            }
            Q.pop();
        }
    } else {
        stats.nb_flush_merges++;
        fstream &out_file = out_files[file_id];

        out_file.seekg(ios::beg);
        u64 buf;
        
        // skip to position where merging needs to start
        out_file.read(reinterpret_cast<char*>(&buf), sizeof(u64));
        while (buf <= Q.top() && !out_file.eof())
            out_file.read(reinterpret_cast<char*>(&buf), sizeof(u64));

        // merge rest of file with Q
        while (true) {
            if (!out_file.eof()) {
                Q.push(buf);
                out_file.seekg(-sizeof(u64), ios::cur);
            } else break;

            out_file.write((char *)&Q.top(), sizeof(u64));
            if (Q.size() <= (Q_max * alpha) + 1) {
                file_max_ids[file_id] = Q.top();
                Q.pop();
                break;
            }
            Q.pop();
            out_file.read(reinterpret_cast<char*>(&buf), sizeof(u64));
        }

        if (Q.size() <= (Q_max * alpha) + 1)
            assert(false);  // error if nothing is appended to file

        // write rest of Q
        out_file.clear();
        while (true) {
            out_files[file_id].write((char *)&Q.top(), sizeof(u64));
            if (Q.size() <= (Q_max * alpha) + 1) {
                file_max_ids[file_id] = Q.top();
                Q.pop();
                break;
            }
            Q.pop();
        }
    }
}

void ImportExtractor::empty_clause_found() {
    mkdir(unsat_folder.c_str(), 0777);
    if (mkdir((unsat_folder + "/" + to_string(pal_id)).c_str(), 0777)) {
        snprintf(palrup_utils_msgstr, MSG_LEN, "Could not create unsat flag.");
        palrup_utils_log_err(palrup_utils_msgstr);
    }
}

void ImportExtractor::init_strat_3(struct options* options) {
    msg_group_size = 1;
    file_max_ids = { 0 };
    Qs = { priority_queue<u64, vector<u64>, std::greater<u64>>() };
    file_names = { string(options->working_path) + "/"
                    + to_string(pal_id / palrup_utils_calc_root_ceil(num_solvers)) + "/"
                    + to_string(pal_id) + "/"
                    + "out.palrup_trim_proxy" };
    out_files = vector<fstream>(1);
    out_files[0].open(file_names[0] + "~", ios::out | ios::in | ios::trunc | ios::binary);
    if (!(out_files[0])) {
        snprintf(palrup_utils_msgstr, MSG_LEN, "Could not create file at %s\n", (file_names[0] + "~").c_str());
        palrup_utils_log_err(palrup_utils_msgstr);
    }
}

ImportExtractor::ImportExtractor(struct options* options) {
    pal_id = options->pal_id;
    num_solvers = options->num_solvers;
    Q_max = options->q_size / 8;
    Q_alpha = options->q_alpha;

    // open proof fragment
    string frag_path = string(options->palrup_path) + "/"
                        + to_string(pal_id / palrup_utils_calc_root_ceil(num_solvers)) + "/"
                        + to_string(pal_id) + "/"
                        + "out.palrup";
    proof_fragment = file_reader_init(options->read_buffer_size,
                                      fopen(frag_path.c_str(), "rb"),
                                      pal_id);

    // init flag for if empty clause was found 
    unsat_folder = string(options->working_path) + "/.unsat_found";

    // init strat specific data structures
    switch (options->redist_strat) {
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

void ImportExtractor::run() {
    // parse proof fragment
    u64 id;
    while (true) {
        char c = file_reader_read_vbl_char(proof_fragment);
        if (file_reader_eof_reached(proof_fragment))
            break;

        switch (c) {
        case TRUSTED_CHK_CLS_DELETE:
            stats.nb_deleted++;
            file_reader_read_vbl_sl(proof_fragment);    // skip id
            while (true) {    // skip hints
                u64 hint = file_reader_read_vbl_sl(proof_fragment);
                if (!hint)
                    break;
            }
            continue;
        
        case TRUSTED_CHK_CLS_IMPORT:
            stats.nb_imported++;
            log_id(file_reader_read_vbl_sl(proof_fragment));
            while (true)    // skip lits
                if (!file_reader_read_vbl_int(proof_fragment))
                    break;
            continue;

        case TRUSTED_CHK_CLS_PRODUCE:
            stats.nb_produced++;
            id = file_reader_read_vbl_sl(proof_fragment);    // skip id

            // skip lits
            if (!file_reader_read_vbl_int(proof_fragment))
                empty_clause_found();
            else while (file_reader_read_vbl_int(proof_fragment));

            // check hints
            while (true) {
                // skip hints
                if (!file_reader_read_vbl_sl(proof_fragment))
                    break;
            }
            continue;

        default:
            snprintf(palrup_utils_msgstr, MSG_LEN, "Unknown directive %c", c);
            palrup_utils_log_err(palrup_utils_msgstr);
            abort();
        }
    }
}

ImportExtractor::~ImportExtractor() {
    file_reader_end(proof_fragment);
    
    for (size_t i = 0; i < msg_group_size; i++) {
        // rename out files
        flush_Q(i, 0);
        u64 zero = 0;
        out_files[i].close();
        rename((file_names[i] + "~").c_str(), file_names[i].c_str());
    }

    palrup_utils_log(string(stats).c_str());
}
