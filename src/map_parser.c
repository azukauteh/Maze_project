#include "map_parser.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int is_valid_cell(char c) {
    return c == '#' || c == '.' || c == 'S' || c == 'E';
}

static char *skip_ws(char *p) {
    while (*p != '\0' && isspace((unsigned char)*p)) {
        p++;
    }
    return p;
}

int parse_maze_map(const uint8_t *data, size_t size, MazeMap *out_map) {
    char buffer[256];
    char *cursor;
    char *end;
    long width;
    long height;
    int row;
    int col;

    if (data == NULL || out_map == NULL || size == 0) {
        return 0;
    }

    size = size < sizeof(buffer) - 1 ? size : sizeof(buffer) - 1;
    memcpy(buffer, data, size);
    buffer[size] = '\0';

    cursor = buffer;
    cursor = skip_ws(cursor);
    if (*cursor == '\0') {
        return 0;
    }

    width = strtol(cursor, &end, 10);
    if (end == cursor || width <= 0 || width > MAP_MAX_WIDTH) {
        return 0;
    }
    cursor = skip_ws(end);

    height = strtol(cursor, &end, 10);
    if (end == cursor || height <= 0 || height > MAP_MAX_HEIGHT) {
        return 0;
    }
    cursor = end;

    memset(out_map, 0, sizeof(*out_map));
    out_map->width = (int)width;
    out_map->height = (int)height;

    for (row = 0; row < height; ++row) {
        char *line_start = cursor;
        char *line_end;

        while (*cursor != '\0' && *cursor != '\n' && *cursor != '\r') {
            cursor++;
        }

        line_end = cursor;
        if (line_start == line_end) {
            return 0;
        }

        while (line_end > line_start && isspace((unsigned char)line_end[-1])) {
            line_end--;
        }

        if ((int)(line_end - line_start) != width) {
            return 0;
        }

        for (col = 0; col < width; ++col) {
            char cell = line_start[col];
            if (!is_valid_cell(cell)) {
                return 0;
            }
            out_map->cells[row][col] = cell;
        }

        while (*cursor == '\r' || *cursor == '\n') {
            cursor++;
        }
    }

    return 1;
}
