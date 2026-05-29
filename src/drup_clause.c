
#include <string.h>
#include <stdlib.h>

#include "drup_clause.h"
#include "utils/palrup_utils.h"

drup_clause drup_clause_init(const u64 id, const int* lits, const int nb_lits) {
    drup_clause c = palrup_utils_malloc(sizeof(u64) + sizeof(int) + (sizeof(int) * nb_lits));
    *((u64*)c) = id;
    *((int*)((char*)c + sizeof(u64))) = nb_lits;
    memcpy((char*)c + sizeof(u64) + sizeof(int), lits, nb_lits * sizeof(int));
    return c;
}
inline void drup_clause_free(drup_clause c) {
    free(c);
}

inline u64 drup_clause_get_id(drup_clause c) {
    return *((u64*)c);
}
inline int drup_clause_get_nb_lits(drup_clause c) {
    return *((int*)((char*)c + sizeof(u64)));
}
inline int* drup_clause_get_lits(drup_clause c) {
    return (int*)((char*)c + sizeof(u64) + sizeof(int));
}
