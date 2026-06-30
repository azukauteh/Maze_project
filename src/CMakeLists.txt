cmake_minimum_required(VERSION 3.15)

find_package(SDL2 REQUIRED)

# Main raycaster game
add_executable(maze 
  main.c
  engine.c
)

target_include_directories(maze PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(maze PRIVATE ${SDL2_LIBRARIES})

if(UNIX)
  target_link_libraries(maze PRIVATE m)
endif()

# Optional: legacy walls placeholder
add_executable(walls Walls.cpp)
set_target_properties(walls PROPERTIES CXX_STANDARD 11)
