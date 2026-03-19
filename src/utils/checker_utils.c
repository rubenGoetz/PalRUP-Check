
#include <assert.h>

#include "checker_utils.h"
#include "../lrat_check.h"

#ifdef UNIT_TEST
#define unit_static
#else
#define unit_static static
#endif

bool checker_utils_import_unchecked(unsigned long id, const int* literals, int nb_literals) {
    // signature is veryfied in later stages - forward clause to checker as an axiom
    return lrat_check_add_axiomatic_clause(id, literals, nb_literals);
}
bool checker_utils_check_hints(unsigned long id, const unsigned long* hints, int nb_hints) {
    for (int i = 0; i < nb_hints; i++)
        if (hints[i] >= id)
            return false;
    return true;
}
bool checker_utils_equal_signatures(const u8* left, const u8* right) {
    for (u64 i = 0; i < SIG_SIZE_BYTES; i++)
        if (left[i] != right[i]) return false;
    return true;
}

unit_static bool bin_search(int* a, int elem, int start, int end) {
    // integer division rounds towards zero
    // => a[end] can never be reached
    // => use nb_elements as end
    assert(start >= 0);
    assert(end >= start);
    int pos = (start + end) / 2;
    if (a[pos] == elem)
        return true;
    
    if (pos == start || pos == end)
        return false;

    if (a[pos] < elem)
        return bin_search(a, elem, pos, end);

    if (a[pos] > elem)
        return bin_search(a, elem, start, pos);

    return false;   // silence compiler warnings
}
bool checker_utils_compare_lits(int* lits1, int* lits2, int nb_lits1, int nb_lits2) {
    if (UNLIKELY(nb_lits1 != nb_lits2)) return false;
    for (int i = 0; i < nb_lits1; i++) {
        if (UNLIKELY(lits1[i] != lits2[i])) return false;
    }
    return true;
}
bool checker_utils_compare_semi_sorted_lits(int* sorted_lits, int* unsorted_lits, int nb_sorted, int nb_unsorted) {
    if (UNLIKELY(nb_sorted != nb_unsorted)) return false;
    for (int i = 0; i < nb_unsorted; i++)
        if (!bin_search(sorted_lits, unsorted_lits[i], 0, nb_sorted))
            return false;

    return true;
}
