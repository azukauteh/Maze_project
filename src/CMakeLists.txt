cmake_minimum_required(VERSION 3.15)

<<<<<<< Updated upstream
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
=======
find_package(SDL2 REQUIRED)

set(MAZE_CORE_SOURCES
  engine.c
  map_parser.c
  minimap.c
  timer.c
  event.c
  particle.c
  input_history.c
)

add_library(maze_core STATIC ${MAZE_CORE_SOURCES})
target_include_directories(maze_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${SDL2_INCLUDE_DIRS})
target_link_libraries(maze_core PUBLIC ${SDL2_LIBRARIES})

if(UNIX)
  target_link_libraries(maze_core PUBLIC m)
endif()

# Main raycaster game
add_executable(maze 
  main.c
  engine.c
  map_parser.c
  minimap.c
  timer.c
  event.c
  particle.c
  input_history.c
)

target_include_directories(maze PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(maze PRIVATE ${SDL2_LIBRARIES})

>>>>>>> Stashed changes
if(UNIX)
    target_link_libraries(maze PRIVATE m)
endif()

<<<<<<< Updated upstream
add_executable(walls Walls.cpp)
set_target_properties(walls PROPERTIES CXX_STANDARD 11)
=======
# Optional: legacy walls placeholder
add_executable(walls Walls.cpp)
set_target_properties(walls PROPERTIES CXX_STANDARD 11)
>>>>>>> Stashed changes
