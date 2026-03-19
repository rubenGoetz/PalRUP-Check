
#include <stdlib.h>
#include <string.h>

#include "clause_flat.h"
#include "utils/palrup_utils.h"

// ----- SET -----

static inline void set_id(clause_ptr ptr, id_type id) {
    *((id_type*)ptr) = id;
}

static inline void set_nb_lits(clause_ptr ptr, nb_lits_type nb_lits) {
    *((nb_lits_type*)((char*)ptr + sizeof(id_type))) = nb_lits;
}

static inline void set_lits(clause_ptr ptr, nb_lits_type nb_lits, const lit_type* lits) {
    lit_type* write_ptr = (lit_type*)((char*)ptr + sizeof(id_type) + sizeof(nb_lits_type));
    for (size_t i = 0; i < nb_lits; i++) {
        *write_ptr++ = lits[i];
    }   
}

// ----- GET -----

inline id_type get_clause_id(clause_ptr ptr) {
    return *((id_type*)ptr);
}

inline nb_lits_type get_clause_nb_lits(clause_ptr ptr) {
    return *((nb_lits_type*)((char*)ptr + sizeof(id_type)));
}

inline lit_type* get_clause_lits(clause_ptr ptr) {
    return (lit_type*)((char*)ptr + sizeof(id_type) + sizeof(nb_lits_type));
}

inline size_t get_clause_size(clause_ptr ptr) {
    return sizeof(id_type) + sizeof(nb_lits_type) + (get_clause_nb_lits(ptr) * sizeof(lit_type));
}

// ----- OBJECT CREATION / DELETION -----

clause_ptr create_flat_clause(id_type id, nb_lits_type nb_lits, const lit_type* lits) {
    clause_ptr ptr = palrup_utils_malloc(sizeof(id_type)
                                            + sizeof(nb_lits_type)
                                            + (nb_lits * sizeof(lit_type)));
    if (ptr == NULL) {
        palrup_utils_log_err("could not allocate enough memory for flat clause");
        return NULL;
    }

    set_id(ptr, id);
    set_nb_lits(ptr, nb_lits);

    if (lits)
        set_lits(ptr, nb_lits, lits);
    
    return ptr;
}

void delete_flat_clause(clause_ptr ptr) {
    free(ptr);
}

// ----- UTILS -----

bool compare_flat_clause(clause_ptr c1, clause_ptr c2) {
    if (c1 == c2)
        return true;
    if (get_clause_id(c1) != get_clause_id(c2)
        || get_clause_nb_lits(c1) != get_clause_nb_lits(c2))
        return false;
    
    return !memcmp(c1, c2, sizeof(id_type) + sizeof(nb_lits_type) + (get_clause_nb_lits(c1) * sizeof(lit_type)));
}
