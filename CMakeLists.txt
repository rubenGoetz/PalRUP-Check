
cmake_minimum_required(VERSION 3.11.4)
project(palrup-checker)

add_definitions("-std=gnu99 -Wall -Wextra -Werror -pedantic-errors -flto -lm") 

add_executable(palrup_local_check
    src/utils/palrup_utils.c
    src/utils/checker_utils.c
    src/main_local_check.c
    src/local_checker.c
    src/file_reader.c
    src/import_handler.c
    src/secret.c
    src/siphash_cls.c
    src/siphash.c
    src/top_check.c
    src/heap.c
    src/merge_buffer.c
    src/vectors.c
    src/lrat_check.c
    src/comm_sig.c
    src/confirm.c
    src/clause_flat.c
    src/hash.c)

add_executable(palrup_redistribute
    src/utils/palrup_utils.c
    src/utils/checker_utils.c
    src/main_redistribute.c
    src/redist_handler.c
    src/comm_sig.c
    src/import_merger.c
    src/secret.c
    src/siphash_cls.c
    src/siphash.c
    src/vectors.c
    src/lrat_check.c
    src/file_reader.c
    src/hash.c)

add_executable(palrup_confirm
    src/utils/palrup_utils.c
    src/utils/checker_utils.c
    src/main_confirm.c
    src/clause_finder.c
    src/import_merger.c
    src/file_reader.c
    src/secret.c
    src/siphash_cls.c
    src/siphash.c
    src/lrat_check.c
    src/vectors.c
    src/comm_sig.c
    src/hash.c
)

target_link_libraries(palrup_local_check m)
target_link_libraries(palrup_redistribute m)
target_link_libraries(palrup_confirm m)

#add_custom_command(
#    TARGET palrup_local_check POST_BUILD
#    COMMAND ${CMAKE_COMMAND} -E copy_directory
#            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/pal/
#            ${CMAKE_CURRENT_BINARY_DIR}/)

add_custom_target(copy_scripts ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/scripts/pal
    ${CMAKE_CURRENT_BINARY_DIR}/
  COMMENT "Copying pal*.sh scripts"  
)  

#add_dependencies( copy_configs)
