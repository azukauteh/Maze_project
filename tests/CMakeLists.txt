cmake_minimum_required(VERSION 3.15)

set(ENGINE_SRCS
    ../src/engine.c
    ../src/map_parser.c
    ../src/config_parser.c
    ../src/level_loader.c
    ../src/texture.c
    ../src/sprite.c
    ../src/hud.c
    ../src/savegame.c
    ../src/renderer.c
    ../src/pathfinder.c
    ../src/audio.c
)

add_executable(test_level       test_level.c       ${ENGINE_SRCS})
add_executable(test_map_parser  test_map_parser.c  ../src/map_parser.c)
add_executable(test_config      test_config.c      ../src/config_parser.c)
add_executable(test_texture     test_texture.c     ../src/texture.c)
add_executable(test_savegame    test_savegame.c    ../src/engine.c
                                                    ../src/map_parser.c
                                                    ../src/savegame.c)
add_executable(test_hud         test_hud.c         ../src/hud.c)
add_executable(test_sprite      test_sprite.c      ../src/texture.c
                                                    ../src/sprite.c)
add_executable(test_pathfinder  test_pathfinder.c  ../src/engine.c
                                                    ../src/map_parser.c
                                                    ../src/pathfinder.c)

foreach(t test_level test_map_parser test_config test_texture
          test_savegame test_hud test_sprite test_pathfinder)
    target_include_directories(${t} PRIVATE ../src)
    if(UNIX)
        target_link_libraries(${t} PRIVATE m)
    endif()
endforeach()

enable_testing()
add_test(NAME level_transitions COMMAND test_level)
add_test(NAME map_parser        COMMAND test_map_parser)
add_test(NAME config_parser     COMMAND test_config)
add_test(NAME texture           COMMAND test_texture)
add_test(NAME savegame          COMMAND test_savegame)
add_test(NAME hud               COMMAND test_hud)
add_test(NAME sprite            COMMAND test_sprite)
add_test(NAME pathfinder        COMMAND test_pathfinder)
