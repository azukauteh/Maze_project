cmake_minimum_required(VERSION 3.15)

add_executable(test_level
    test_level.c
    ../src/engine.c
    ../src/map_parser.c
)

target_include_directories(test_level PRIVATE ../src)

if(UNIX)
    target_link_libraries(test_level PRIVATE m)
endif()

enable_testing()
add_test(NAME level_transitions COMMAND test_level)
