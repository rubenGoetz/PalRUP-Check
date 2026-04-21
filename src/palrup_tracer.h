
#pragma once

// Implements a PalRUP-Tracer library to log PalRUP proof fragments in a clause sharing solver
// 
// How to use:
//  - create a tracer with palrup_tracer_init() before logging any clasues
//  - log clauses during runtime with palrup_tracer_log_clause_addition(), palrup_tracer_log_clause_import(), palrup_tracer_log_clause_deletion()
//  - If necessary calculate new PalRUP clasue ID's with palrup_tracer_next_id(). Keep in mind that any specific ID will be returned at most once.
//  - finish proof fragment and free ressources with palrup_tracer_free()

#include <stdbool.h>
#include <stdio.h>

struct palrup_tracer;

struct palrup_tracer* palrup_tracer_init(const unsigned long nb_orig_clauses, const bool use_binary, const char* fragment_path, const unsigned long solver_id, const unsigned long num_solvers);
void palrup_tracer_free(struct palrup_tracer* tracer);

unsigned long palrup_tracer_next_id(struct palrup_tracer* tracer, const int nb_hints, const unsigned long* ext_hints);
void palrup_tracer_log_clause_addition(struct palrup_tracer* tracer, const unsigned long id, const int nb_lits, const int* lits, const int nb_hints, const unsigned long* hints);
void palrup_tracer_log_clause_import(struct palrup_tracer* tracer, const unsigned long id, const int nb_lits, const int* lits);
void palrup_tracer_log_clause_deletion(struct palrup_tracer* tracer, const unsigned long id);
