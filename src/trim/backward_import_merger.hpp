
#pragma once

#include <vector>
#include <string>
#include <queue>

extern "C" {
    #include "backward_file_reader.h"
}

using namespace std;

class BackImpMerger {
    vector<struct back_file_reader*> bfrs;
    priority_queue<pair<u64,int>> Q;

    public:
    BackImpMerger() = default;
    BackImpMerger(vector<string> file_paths, u64 capacity, int rank);
    ~BackImpMerger();

    pair<u64,int> next();

};
