cmake_minimum_required(VERSION 3.15)

find_package(SDL2 REQUIRED)

add_executable(maze
    main.c
    engine.c
    map_parser.c
    config_parser.c
    level_loader.c
    texture.c
    sprite.c
    hud.c
    savegame.c
)

target_include_directories(maze PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(maze PRIVATE ${SDL2_LIBRARIES})
if(UNIX)
    target_link_libraries(maze PRIVATE m)
endif()

add_executable(walls Walls.cpp)
set_target_properties(walls PROPERTIES CXX_STANDARD 11)
