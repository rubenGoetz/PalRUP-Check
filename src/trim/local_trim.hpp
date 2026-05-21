
#include <fstream>

// Include the whole post office
#include <briefkasten/aggregators.hpp>
#include <briefkasten/buffered_queue.hpp>
#include <briefkasten/grid_indirection.hpp>
#include <briefkasten/indirection.hpp>
#include <briefkasten/noop_indirection.hpp>
#include <briefkasten/queue_builder.hpp>

#include "../../lib/tsl/robin_map.h"
#include "backward_import_merger.hpp"

extern "C" {
    #include "../backward_file_reader.h"
}

typedef unsigned long u64;
typedef unsigned char u8;

namespace bka = briefkasten;

class ProofTrimmer {
    private:
        std::string fragment_path;
        std::string working_path;
        struct back_file_reader* proof_fragment;
        bool expect_empty_clause = false;
        u64 write_buffer_cap;       // std::vector does not guaratee an exact capacity after reserve()
        std::vector<u8> write_buffer;
        std::vector<u8> delete_line;
        FILE* out_file;   // C style file IO to reuse existing code
        /* Contains key, value pairs with clause ids as keys.
         * Values encode the following information:
         *  v > 0  : there are still v expected messages concerning this id
         *  v == 0 : all expected messages were received and c(id) is redundant in global proof
         *  v < 0  : c(id) is needed by at least one pal and thus not redundat
         * Values get lazily decremented when messages arrive but are assured to encode
         * expected number of messages by the time the respective clause gets processed.
         */
        tsl::robin_map<u64, int> marked_clauses;
        u8* data_pointer;
        BackImpMerger importQ;
        u64 last_importQ_id;

        // Encode false as negative. (Works, since proof generation is using singed ids)
        bka::BufferedMessageQueue<
            u64,
            u64,
            std::vector<u64>,
            std::vector<u64>,
            bka::aggregation::AppendMerger,
            bka::aggregation::NoSplitter,
            bka::aggregation::NoOpCleaner> queue;


    public:
        ProofTrimmer(std::string palrup_path,
                     std::string working_path,
                     u64 input_buffer_cap,
                     u64 write_buffer_size);
        ~ProofTrimmer();
        void trim();

    private:
        void parse_palrup_trim_import(std::string path);
        void mark_empty_clause(std::string unsat_found_path);
        void mark_next_import();
        void write_line_backwards(u64 idx);
        void write_delete_line();
        void reverse_outfile();
        vector<string> calc_input_paths();
};
