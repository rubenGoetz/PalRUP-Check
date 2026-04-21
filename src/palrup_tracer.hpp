
#ifndef _PalRUPTracer_h
#define _PalRUPTracer_h

// C++ wrapper for the PalRUP-Tracer library to log PalRUP proof fragments in a clause sharing solver
// 
// How to use:
//  - Create PalRUPTracer object
//  - log clauses with log_clause_addition(), log_clause_import(), log_clause_deletion()
//  - If necessary calculate new PalRUP clasue ID's with next_id(). Keep in mind that any specific ID will be returned at most once.
//  - proof fragment is only guaranteed to be finished after ~PalRUPTracer() was called

#include <string>
#include <vector>

extern "C" {
    #include "palrup_tracer.h"
}

class PalRUPTracer {
    struct palrup_tracer* tracer;

    public:
        PalRUPTracer(const unsigned long nb_orig_clauses,
            const bool use_binary,
            const std::string fragment_path,
            const unsigned long solver_id,
            const unsigned long num_solvers) : tracer(palrup_tracer_init(nb_orig_clauses, use_binary, fragment_path.c_str(), solver_id, num_solvers)) {};
        ~PalRUPTracer() { palrup_tracer_free(this->tracer); };
        
        unsigned long next_id(const int nb_hints, const std::vector<unsigned long> &ext_hints) {
            return palrup_tracer_next_id(this->tracer, nb_hints, ext_hints.data());
        };

        void log_clause_addition(const unsigned long id, const std::vector<int> &lits, const std::vector<unsigned long> &hints) {
            palrup_tracer_log_clause_addition(this->tracer, id, lits.size(), lits.data(), hints.size(), hints.data());
        };

        void log_clause_import(const unsigned long id, const std::vector<int> &lits) {
            palrup_tracer_log_clause_import(this->tracer, id, lits.size(), lits.data());
        };

        void log_clause_deletion(const unsigned long id) {
            palrup_tracer_log_clause_deletion(this->tracer, id);
        };
};

#endif
