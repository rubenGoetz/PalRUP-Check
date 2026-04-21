
cmake_minimum_required(VERSION 3.11.4)
project(palrup-checker)

add_definitions("-std=gnu99 -Wall -Wextra -Werror -pedantic-errors -flto -lm") 

# Module Libraries

add_library(import_handler
            src/utils/palrup_utils.c
            src/import_handler.c
            src/secret.c
            src/comm_sig.c
            src/heap.c
            src/merge_buffer.c
            src/vectors.c
            src/clause_flat.c
            src/options.c
            src/write_buffer.c)
target_link_libraries(import_handler m)

add_library(local_checker
            src/utils/palrup_utils.c
            src/utils/checker_utils.c
            src/local_checker.c
            src/file_reader.c
            src/secret.c
            src/siphash_cls.c
            src/top_check.c
            src/lrat_check.c
            src/siphash.c
            src/confirm.c
            src/vectors.c
            src/hash.c)
target_link_libraries(local_checker m import_handler)

add_library(import_merger
            src/utils/palrup_utils.c
            src/utils/checker_utils.c
            src/import_merger.c
            src/file_reader.c
            src/vectors.c
            src/comm_sig.c)

add_library(redist_handler
            src/utils/palrup_utils.c
            src/utils/checker_utils.c
            src/redist_handler.c
            src/comm_sig.c
            src/secret.c
            src/siphash_cls.c
            src/vectors.c
            src/write_buffer.c)
target_link_libraries(redist_handler m import_merger)

add_library(clause_finder
            src/utils/palrup_utils.c
            src/utils/checker_utils.c
            src/clause_finder.c
            src/file_reader.c
            src/secret.c
            src/siphash_cls.c
            src/vectors.c)
target_link_libraries(clause_finder m import_merger)

add_library(palrup_tracer STATIC
            src/utils/palrup_utils.c
            src/palrup_tracer.c
            src/hash.c
            src/vectors.c
            src/file_reader.c)

add_library(palrup_tracer_hpp STATIC
            src/utils/palrup_utils.c
            src/palrup_tracer.hpp
            src/palrup_tracer.c
            src/hash.c
            src/vectors.c
            src/file_reader.c)
set_target_properties(palrup_tracer_hpp PROPERTIES LINKER_LANGUAGE CXX)

# Executables

add_executable(palrup_local_check src/main_local_check.c src/options.c)
target_link_libraries(palrup_local_check local_checker)

add_executable(palrup_redistribute src/main_redistribute.c src/options.c)
target_link_libraries(palrup_redistribute redist_handler)

add_executable(palrup_confirm src/main_confirm.c src/options.c)
target_link_libraries(palrup_confirm clause_finder)

add_custom_target(copy_scripts ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/scripts/pal
    ${CMAKE_CURRENT_BINARY_DIR}/
  COMMENT "Copying pal*.sh scripts"  
)

# Tools
add_executable(check_comm_file
               src/utils/palrup_utils.c
               src/clause_flat.c
               src/comm_sig.c
               src/secret.c
               src/vectors.c
               tools/check_comm_file.c)

add_executable(comm_file_to_txt
               src/utils/palrup_utils.c
               src/clause_flat.c
               src/vectors.c
               tools/comm_file_to_txt.c)

add_executable(proof_fragment_to_txt
               src/utils/palrup_utils.c
               src/clause_flat.c
               src/siphash.c
               src/secret.c
               src/file_reader.c
               src/vectors.c
               tools/proof_fragment_to_txt.c)

# Tests
include(CTest)
add_library(test_utils
            src/utils/palrup_utils.c
            src/utils/checker_utils.c
            src/vectors.c
            test/test_utils.c)

add_executable(test_checker_utils
               src/utils/checker_utils.c
               test/test_checker_utils.c)
target_link_libraries(test_checker_utils test_utils)
target_compile_definitions(test_checker_utils PRIVATE UNIT_TEST)
add_test(NAME Test_Checker_Utils
         COMMAND test_checker_utils)

add_executable(test_clause_flat
               src/clause_flat.c
               test/test_clause_flat.c)
target_link_libraries(test_clause_flat test_utils)
add_test(NAME Test_Clause_Flat
         COMMAND test_clause_flat)

add_executable(test_file_reader
               src/file_reader.c
               test/test_file_reader.c)
target_link_libraries(test_file_reader test_utils)
add_test(NAME Test_File_Reader
         COMMAND test_file_reader)

add_executable(test_hash
               src/hash.c
               test/test_hash.c)
target_link_libraries(test_hash test_utils)
add_test(NAME Test_Hash
         COMMAND test_hash)

add_executable(test_heap 
  src/clause_flat.c 
  src/heap.c 
  test/test_heap.c)
target_link_libraries(test_heap test_utils)
add_test(NAME Test_Heap
         COMMAND test_heap)

add_library(test_import_handler_lib src/import_handler.c)
target_compile_definitions(test_import_handler_lib PRIVATE UNIT_TEST)
target_link_libraries(test_import_handler_lib import_handler)
add_executable(test_import_handler
               test/test_import_handler.c)
target_link_libraries(test_import_handler test_import_handler_lib test_utils)
add_test(NAME Test_Import_Handler
         COMMAND test_import_handler)

add_executable(test_merge_buffer 
  src/merge_buffer.c
  src/clause_flat.c
  test/test_merge_buffer.c)
target_link_libraries(test_merge_buffer test_utils)
add_test(NAME Test_Merge_Buffer
         COMMAND test_merge_buffer)

add_executable(test_palrup_tracer
               test/test_palrup_tracer.c)
target_link_libraries(test_palrup_tracer palrup_tracer test_utils)
add_test(NAME Test_PalRUP_Tracer
         COMMAND test_palrup_tracer)

add_executable(test_full_run
               test/test_full_run.c)
target_link_libraries(test_full_run m test_utils)
add_test(NAME Test_Full_Run
         COMMAND test_full_run)
