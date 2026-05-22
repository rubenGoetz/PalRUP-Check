
#include "backward_import_merger.hpp"

extern "C" {
    #include <stdio.h>
}

#include <iostream>

BackImpMerger::BackImpMerger(vector<string> file_paths, u64 capacity) :
    bfrs(vector<struct back_file_reader*>(0)),
    Q(priority_queue<pair<u64,int>>()) {
    int i = 0;
    for (string file_path : file_paths) {
        FILE* file = fopen(file_path.c_str(), "rb");
        if (file) {
            struct back_file_reader* bfr = back_file_reader_init(file, capacity);
            if (!back_file_reader_empty(bfr)) {
                bfrs.push_back(bfr);
                Q.push({back_file_reader_ul(bfr), i++});
            }
        }
    }
}

BackImpMerger::~BackImpMerger() {
    // TODO: fix mem leak
    // for (auto bfr : bfrs)
    //     back_file_reader_free(bfr);
}

pair<u64,int> BackImpMerger::next() {
    if (Q.size() <= 0)
        return {0,0};

    u64 id = Q.top().first;
    int i = Q.top().second;
    int count = 1;
    Q.pop();

    if (!back_file_reader_empty(bfrs[i]))
        Q.push({back_file_reader_ul(bfrs[i]), i});

    while (Q.size() > 0 && Q.top().first == id) {
        i = Q.top().second;
        count++;
        Q.pop();
    
        if (!back_file_reader_empty(bfrs[i]))
            Q.push({back_file_reader_ul(bfrs[i]), i});
    }

    return {id, count};
}
