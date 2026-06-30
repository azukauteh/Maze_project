#ifndef MAP_PARSER_H
#define MAP_PARSER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAP_MAX_WIDTH 32
#define MAP_MAX_HEIGHT 32

typedef struct {
    int width;
    int height;
    char cells[MAP_MAX_HEIGHT][MAP_MAX_WIDTH];
} MazeMap;

int parse_maze_map(const uint8_t *data, size_t size, MazeMap *out_map);

#ifdef __cplusplus
}
#endif

#endif
