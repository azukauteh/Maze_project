#ifndef LEVEL_LOADER_H
#define LEVEL_LOADER_H

#include "map_parser.h"
#include "config_parser.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * .maz file format (plain text):
 *
 *   [meta]
 *   name    = Level One
 *   author  = Azuka
 *   version = 1
 *
 *   [spawn]
 *   x     = 150
 *   y     = 400
 *   angle = 90
 *
 *   [map]
 *   format = text      ; "text", "rle", or "binary"
 *   data   =
 *   8 8
 *   ########
 *   #.#....#
 *   ...
 *
 * VULN NOTE: the "name" value is copied into a 32-byte fixed buffer using
 * sscanf("%s") — intentional unbounded-write vulnerability for fuzzer.
 */

#define LOADER_NAME_MAX   32
#define LOADER_AUTHOR_MAX 64
#define LOADER_VERSION_MAX 8

typedef struct {
    char   name   [LOADER_NAME_MAX];
    char   author [LOADER_AUTHOR_MAX];
    char   version[LOADER_VERSION_MAX];
    float  spawn_x;
    float  spawn_y;
    float  spawn_angle;
    MazeMap map;
    int    loaded;
} LoadedLevel;

/*
 * level_load_from_data — parse a .maz file from a memory buffer.
 * Returns 1 on success, 0 on any parse error.
 */
int level_load_from_data(const char *data, size_t size, LoadedLevel *out);

/*
 * level_load_from_file — read a .maz file by path and parse it.
 * Returns 1 on success, 0 on error.
 */
int level_load_from_file(const char *path, LoadedLevel *out);

/*
 * level_validate — run map validation on a loaded level.
 * Returns the MapValidation result.
 */
MapValidation level_validate(const LoadedLevel *level);

/*
 * level_print_meta — print metadata to stdout.
 */
void level_print_meta(const LoadedLevel *level);

#ifdef __cplusplus
}
#endif

#endif /* LEVEL_LOADER_H */
