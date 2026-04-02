
#include <stdlib.h>
#include <string.h>

#include "options.h"
#include "utils/palrup_utils.h"

struct options* options_init() {
    struct options* options = palrup_utils_malloc(sizeof(struct options));
    
    // universally needed
    options->working_path = 0;
    options->num_solvers = 0;
    options->pal_id = 0;
    options->read_buffer_size = 4096 * 1024;
    options->redist_strat = 3;

    // partially needed
    options->formula_path = 0;
    options->palrup_path = 0;
    options->palrup_binary = true;
    options->write_buffer_size = 4096 * 1024;
    
    // local_checker
    options->merge_buffer_size = 4096 * 1024;
    options->q_size = 2 * 4096 * 1024;
    options->q_alpha = .5;

    return options;
}

static bool begins_with(const char* str, const char* prefix) {
    u64 i = 0;
    while (true) {
        if (prefix[i] == '\0') return true;
        if (str[i] == '\0') return prefix[i] == '\0';
        if (str[i] != prefix[i]) return false;
        i++;
    }
}
void options_free(struct options* options) {
    free(options);
}

void options_try_match_arg(char* arg, char* opt, char** out) {
    if (begins_with(arg, opt)) *out = arg + strlen(opt);
}
void options_try_match_ul(const char* arg, const char* opt, u64* out) {
    const char* start_of_number = arg + strlen(opt);
    if (begins_with(arg, opt)) *out = strtol(start_of_number, NULL, 10);
}
void options_try_match_float(const char* arg, const char* opt, float* out) {
    const char* start_of_number = arg + strlen(opt);
    if (begins_with(arg, opt)) *out = strtof(start_of_number, NULL);
}
void options_try_match_bool(const char* arg, const char* opt, bool* out) {
    const char* start_of_number = arg + strlen(opt);
    if (begins_with(arg, opt)) *out = (bool)atoi(start_of_number);
}
void options_try_match_flag(const char* arg, const char* opt, bool* out) {
    if (begins_with(arg, opt)) *out = true;
}

void options_buffer_sizes_to_bytes(struct options* options) {
    options->read_buffer_size *= 1024;
    options->write_buffer_size *= 1024;
    options->merge_buffer_size *= 1024;
    options->q_size *= 1024;
}

static void add_str(char* dest, size_t* dest_cap, const char* src) {
    // double length of dest if necessary
    if (strlen(dest) + strlen(src) > *dest_cap) {
        *dest_cap = *dest_cap * 2;
        char new_dest[*dest_cap];
        memcpy(new_dest, src, strlen(dest));
        dest = new_dest;
    }

    strcat(dest, src);
}
static void add_u64(char* dest, size_t* dest_cap, u64 src) {
    char buff[20];
    sprintf(buff, "%lu", src);
    add_str(dest, dest_cap, buff);
}
static void add_float(char* dest, size_t* dest_cap, float src) {
    char buff[50];
    sprintf(buff, "%.2f", src);
    add_str(dest, dest_cap, buff);
}

void options_print(struct options* options) {
    size_t str_cap = 512;
    char op_list[str_cap];
    snprintf(op_list, str_cap, "Option list:");
    
    if (options->working_path) {
        add_str(op_list, &str_cap, " -working-path=");
        add_str(op_list, &str_cap, options->working_path);
    }
    if (options->num_solvers) {
        add_str(op_list, &str_cap, " -num-solvers=");
        add_u64(op_list, &str_cap, options->num_solvers);
    }
    // pal_id=0 is valid value
    //if (options->pal_id) {
        add_str(op_list, &str_cap, " -pal-id=");
        add_u64(op_list, &str_cap, options->pal_id);
    //}
    if (options->read_buffer_size) {
        add_str(op_list, &str_cap, " -read-buffer-size=");
        add_u64(op_list, &str_cap, options->read_buffer_size);
    }
    if (options->redist_strat) {
        add_str(op_list, &str_cap, " -redist-strat=");
        add_u64(op_list, &str_cap, options->redist_strat);
    }

    if (options->formula_path) {
        add_str(op_list, &str_cap, " -formula-path=");
        add_str(op_list, &str_cap, options->formula_path);
    }
    if (options->palrup_path) {
        add_str(op_list, &str_cap, " -palrup-path=");
        add_str(op_list, &str_cap, options->palrup_path);
    }
    if (options->palrup_binary) {
        add_str(op_list, &str_cap, " -palrup-binary=");
        add_u64(op_list, &str_cap, options->palrup_binary);
    }
    if (options->write_buffer_size) {
        add_str(op_list, &str_cap, " -write-buffer-size=");
        add_u64(op_list, &str_cap, options->write_buffer_size);
    }

    if (options->merge_buffer_size) {
        add_str(op_list, &str_cap, " -merge-buffer-size=");
        add_u64(op_list, &str_cap, options->merge_buffer_size);
    }
    if (options->q_size) {
        add_str(op_list, &str_cap, " -q-size=");
        add_u64(op_list, &str_cap, options->q_size);
    }
    if (options->q_alpha) {
        add_str(op_list, &str_cap, " -q-alpha=");
        add_float(op_list, &str_cap, options->q_alpha);
    }

    palrup_utils_log(op_list);
}
