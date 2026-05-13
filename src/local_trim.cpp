
#include <iostream>
#include <filesystem>
#include <assert.h>

#include "local_trim.hpp"

ProofTrimmer::ProofTrimmer(std::string proof_path, std::string working_path, u64 input_buffer_cap, u64 write_buffer_size) :
    queue(briefkasten::IndirectionAdapter {
            briefkasten::BufferedMessageQueueBuilder<u64>()
                .with_merger(briefkasten::aggregation::AppendMerger{})
                .with_splitter(briefkasten::aggregation::NoSplitter{})
                .build(),
            briefkasten::GridIndirectionScheme{MPI_COMM_WORLD}}),
    working_path(working_path),
    delete_line(std::vector<u8>(16, 0)),
    write_buffer_cap(write_buffer_size)
    {   
        // calculate paths with queue.rank() == MPI-rank == Pal-id
        int sqrt = std::ceil(std::sqrt((double) queue.size()));
        fragment_path = proof_path + "/" 
                        + std::to_string((int)(queue.rank() / sqrt)) + "/"
                        + std::to_string(queue.rank()) + "/out.palrup";
        std::string out_path = fragment_path + ".trim~";
        out_file = fopen(out_path.c_str(), "wb");
        
        // Handle imports
        int column = queue.rank() % sqrt;
        for (size_t i = 0; i < sqrt; i++) {
            int src_id = (i * sqrt) + column;
            parse_palrup_trim_import(working_path + "/"
                                     + std::to_string(src_id / sqrt) + "/"
                                     + std::to_string(src_id)
                                     + "/out.palrup_trim_import");
        }

        // open proof fragment
        proof_fragment = back_file_reader_init(fopen(fragment_path.c_str(), "rb"), input_buffer_cap);
        data_pointer = proof_fragment->buffer->data;

        mark_empty_clause(working_path + "/.unsat_found");
        write_buffer.reserve(write_buffer_size);

    }

ProofTrimmer::~ProofTrimmer() {
    palrup_utils_write_objs(write_buffer.data(), 1, write_buffer.size(), out_file);
    reverse_outfile();
    fclose(out_file);
    std::rename((fragment_path + ".trim~").c_str(), (fragment_path + ".trim").c_str());

    back_file_reader_free(this->proof_fragment);
    //std::ignore = this->queue.terminate(on_message);
}

void ProofTrimmer::trim() {
    auto on_message = [&](auto envelope){
        for (auto it = envelope.message.begin(); it != envelope.message.end(); it++) {
            if (*it >> 63) {    // clasue was not used
                u64 id = -(*it);
                auto val_before = marked_clauses.find(id).value();
                marked_clauses.find(id).value() -= 1;
                auto val_after = marked_clauses.find(id).value();
                // std::cout << "[" << queue.rank() << "] id " << id << " was not used" << std::endl;
            } else {    // clause was used
                marked_clauses.find(*it).value() = -1;
                // std::cout << "[" << queue.rank() << "] id " << *it << " was used" << std::endl;
            }
        }
    };

    u64 id;
    u64 line_count = 0;
    do {
        // trim local proof as while possible
        while (!back_file_reader_eof(proof_fragment) || !(proof_fragment->read_idx == 0)) {
            u64 line_idx = back_file_reader_get_start_line_idx(proof_fragment);
            //std::cout << "[" << queue.rank() << "] line " << ++line_count << std::endl;
            switch (back_file_reader_decode_char(proof_fragment, line_idx)) {
            case TRUSTED_CHK_CLS_DELETE:
                // skip delete lines. deletes will be placed for new hints.
                break;

            case TRUSTED_CHK_CLS_IMPORT: {
                id = back_file_reader_decode_sl(proof_fragment, line_idx+1);
                auto mark = marked_clauses.find(id);
                if (mark != marked_clauses.end()) {
                    write_line_backwards(line_idx);
                    // send "used" message
                    queue.post_message_blocking(id,
                                                id % queue.size(),
                                                on_message);
                    marked_clauses.erase(mark);
                }
                else queue.post_message_blocking(-id, id % queue.size(), on_message); // send "unused" message
                break;

            } case TRUSTED_CHK_CLS_PRODUCE: {
                id = back_file_reader_decode_sl(proof_fragment, line_idx+1);
                auto mark = marked_clauses.find(id);
                if (mark == marked_clauses.end()) {     // Clause was not used by any pal but ..
                    if (expect_empty_clause) {          // .. might be the empty clause
                        u64 tmp_idx = line_idx + 1;
                        while (proof_fragment->buffer->data[tmp_idx++] & 128);     // skip clause id
                        int tmp = back_file_reader_decode_int(proof_fragment, tmp_idx);
                        if (tmp)    // not the empty clause
                            break;

                        snprintf(palrup_utils_msgstr, MSG_LEN, "Found empty clause with id %lu.", id);
                        palrup_utils_log(palrup_utils_msgstr);
                        u64 hint_idx = proof_fragment->read_idx;
                        write_line_backwards(line_idx);
                        proof_fragment->read_idx = hint_idx;

                        // mark hints
                        back_file_reader_vbl_sl(proof_fragment);   // skip 0
                        while (true) {
                            u64 hint = back_file_reader_vbl_sl(proof_fragment);
                            if (hint == 0)
                                break;
                            marked_clauses.insert({hint, -1});
                        }

                        expect_empty_clause = false;
                    }
                    break;
                } 

                while (mark.value() > 0) {     // Wait to see if clause was used
                    std::ignore = queue.terminate(on_message);
                    mark = marked_clauses.find(id);
                }
                
                if (mark.value() < 0) {     // Clause was marked by self or other pal
                    marked_clauses.erase(mark);
                    u64 hint_idx = proof_fragment->read_idx;

                    // mark hints
                    back_file_reader_vbl_sl(proof_fragment);   // skip 0
                    delete_line.resize(1);  // first byte should always be 0
                    while (true) {
                        u64 tmp_idx = proof_fragment->read_idx;
                        u64 hint = back_file_reader_vbl_sl(proof_fragment);
                        if (hint == 0)
                            break;
                        auto insert_res = marked_clauses.insert({hint, -1});
                        auto value = insert_res.first.value();
                        if (insert_res.second) {
                            // ID was last used here and can thus be deleted
                            while (tmp_idx > proof_fragment->read_idx)
                                delete_line.push_back(proof_fragment->buffer->data[tmp_idx--]);
                        } else {
                            insert_res.first.value() = -1;
                        }
                    }

                    // write delete line
                    if (delete_line.size() > 1) {
                        delete_line.push_back('d');
                        write_delete_line();
                    }

                    // write add line
                    proof_fragment->read_idx = hint_idx;
                    write_line_backwards(line_idx);
                } else {
                    // Clause was imported by other pals but not used by any of them
                }

                break;

            } default:
                snprintf(palrup_utils_msgstr, MSG_LEN, "Invalid directive '%c' while parsing proof.", back_file_reader_decode_char(proof_fragment, line_idx));
                palrup_utils_log_err(palrup_utils_msgstr);
                abort();
            }

            // finish line
            back_file_reader_skip_to_idx(proof_fragment, line_idx ? line_idx - 1 : 0);
        }
    } while (!queue.terminate(on_message));
}

// TODO: make external
// parse expected imports
void ProofTrimmer::parse_palrup_trim_import(std::string path) {
    std::ifstream input;
    input.open(path, std::ios::in | std::ios::binary);

    u64 id;
    while (true ) {
        // read next id for this pal
        input.read((char *)&id, sizeof(id));
        //std::cout << queue.rank() << " id:" << id << std::endl;

        if (!id) break;
        if (id % queue.size() != queue.rank())
            continue;

        // increment id count
        auto it = marked_clauses.find(id);
        if (it != marked_clauses.end())
            it.value()++;
        else marked_clauses.insert({id, 1});
        //if (it != marked_clauses.end());
        //else marked_clauses.insert({id, -1});
    }
    
}

void ProofTrimmer::mark_empty_clause(std::string unsat_found_path) {
    // get solver id with empty clause
    int empty_clause_rank = INT32_MAX;
    for (const auto & entry : std::filesystem::directory_iterator(unsat_found_path))
        if (std::stoi(entry.path().filename()) < empty_clause_rank)
            empty_clause_rank = std::stoi(entry.path().filename());

    if (empty_clause_rank == INT32_MAX)
        palrup_utils_log_warn("No empty clause was found in local pass");

    if (empty_clause_rank == queue.rank())
        expect_empty_clause = true;
}

void ProofTrimmer::write_line_backwards(u64 idx) {
    assert(idx <= proof_fragment->read_idx);

    // write as much data as possible into buffer
    u64 write_idx = std::max((long)idx, (long)(proof_fragment->read_idx - (write_buffer_cap - write_buffer.size())));
    //std::cout << ">> [" << queue.rank() << "]"
    //          << " idx:" << idx
    //          << " read_idx:" << proof_fragment->read_idx
    //          << " write_idx:" << write_idx
    //          << " write_buffer_cap:" << write_buffer_cap
    //          << " write_buffer.size():" << write_buffer.size()
    //          << " write_buffer.capacity():" << write_buffer.capacity()
    //          << std::endl;
    assert(proof_fragment->read_idx >= write_idx);
    while (proof_fragment->read_idx > write_idx)
        write_buffer.push_back(proof_fragment->buffer->data[proof_fragment->read_idx--]);
    assert(write_buffer.size() <= write_buffer_cap);
    assert(write_buffer.capacity() <= write_buffer_cap);

    // flush buffer if necessary
    if (write_buffer.size() == write_buffer_cap) {
        palrup_utils_write_objs(write_buffer.data(), 1, write_buffer.size(), out_file);
        write_buffer.resize(0); // TODO: overwrites whole vector with 0 :(
    }

    // write rest of line into buffer
    while (proof_fragment->read_idx >= idx && proof_fragment->read_idx != (u64)-1 )
        write_buffer.push_back(proof_fragment->buffer->data[proof_fragment->read_idx--]);
    assert(write_buffer.size() <= write_buffer_cap);
    assert(write_buffer.capacity() <= write_buffer_cap);
}

void ProofTrimmer::write_delete_line() {
    // write as much data as possible
    u64 to_write = std::min((long)delete_line.size(), (long)(write_buffer_cap - write_buffer.size()));
    //std::cout << ">> [" << queue.rank() << ",1]"
    //          << " delete_line.size():" << delete_line.size()
    //          << " to_write:" << to_write
    //          << " space:" << write_buffer_cap - write_buffer.size()
    //          << std::endl;
    write_buffer.insert(write_buffer.end(), delete_line.begin(), delete_line.begin() + to_write);
    assert(write_buffer.size() <= write_buffer_cap);
    assert(write_buffer.capacity() <= write_buffer_cap);
    
    // flush buffer if necessary
    if (write_buffer.size() == write_buffer_cap) {
        palrup_utils_write_objs(write_buffer.data(), 1, write_buffer.size(), out_file);
        write_buffer.resize(0);
    }

    // write rest of deleline into buffer
    //std::cout << ">> [" << queue.rank() << ",2]"
    //          << " delete_line.size():" << delete_line.size()
    //          << " write_buffer.size():" << write_buffer.size()
    //          << " space:" << write_buffer_cap - write_buffer.size()
    //          << std::endl;
    write_buffer.insert(write_buffer.end(), delete_line.begin() + to_write, delete_line.end());
    assert(write_buffer.size() <= write_buffer_cap);
    assert(write_buffer.capacity() <= write_buffer_cap);
}

void ProofTrimmer::reverse_outfile() {
    // (re)open file streams unbuffered
    fclose(out_file);
    out_file = fopen((fragment_path + ".trim~").c_str(), "rb+");
    setvbuf(out_file, NULL, _IONBF, 0);
    fseek(out_file, 0, SEEK_END);
    FILE* out_file_rev = fopen((fragment_path + ".trim~").c_str(), "rb+");
    setvbuf(out_file_rev, NULL, _IONBF, 0);
    std::vector<u8> write_buffer_rev(write_buffer_cap);

    u64 file_size = ftell(out_file);
    u64 reads = (file_size / 2) / write_buffer_cap;
    u8 tmp;
    for (size_t i = 0; i < reads; i++) {
        // read blocks
        palrup_utils_read_objs(write_buffer_rev.data(), 1, write_buffer_cap, out_file_rev);
        fseek(out_file_rev, -write_buffer_cap, SEEK_CUR);
        fseek(out_file, -write_buffer_cap, SEEK_CUR);
        palrup_utils_read_objs(write_buffer.data(), 1, write_buffer_cap, out_file);
        fseek(out_file, -write_buffer_cap, SEEK_CUR);

        // reverse blocks
        for (size_t i = 0; i < write_buffer_cap; i++) {
            tmp = write_buffer_rev[i];
            write_buffer_rev[i] = write_buffer[write_buffer_cap - 1 - i];
            write_buffer[write_buffer_cap - 1 - i] = tmp;
        }

        // write blocks
        palrup_utils_write_objs(write_buffer.data(), 1, write_buffer_cap, out_file);
        palrup_utils_write_objs(write_buffer_rev.data(), 1, write_buffer_cap, out_file_rev);
        fseek(out_file, -write_buffer_cap, SEEK_CUR);
    }
    
    // handle overlap
    long overlap_size = (long)(ftell(out_file) - ftell(out_file_rev));
    write_buffer.resize(overlap_size);
    write_buffer_rev.resize(overlap_size);
    palrup_utils_read_objs(write_buffer.data(), sizeof(u8), overlap_size, out_file_rev);
    fseek(out_file_rev, -overlap_size, SEEK_CUR);

    for (long i = 0; i < overlap_size; i++)
        write_buffer_rev[overlap_size - 1 - i] = write_buffer[i];
    palrup_utils_write_objs(write_buffer_rev.data(), sizeof(u8), overlap_size, out_file_rev);

    fclose(out_file_rev);
}
