#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "map_parser.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    MazeMap map;
    memset(&map, 0, sizeof(map));
    (void)parse_maze_map(data, size, &map);
    return 0;
}
