#ifndef MAP_PARSER_H
#define MAP_PARSER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAP_MAX_WIDTH  32
#define MAP_MAX_HEIGHT 32

/*
 * Text format (original):
 *   "W H\n"
 *   followed by H rows of W characters each.
 *   Valid cell chars: # (wall)  . (floor)  S (start)  E (exit)
 */
typedef struct {
    int  width;
    int  height;
    char cells[MAP_MAX_HEIGHT][MAP_MAX_WIDTH];
} MazeMap;

/*
 * Binary format (little-endian):
 *   [0..1]  magic  = 0x4D5A  ('M','Z')
 *   [2..3]  width  uint16_t
 *   [4..5]  height uint16_t
 *   [6..]   cells  row-major, 1 byte each
 *             0 = floor  1 = wall  2 = exit  3 = start
 *
 * FUZZ NOTE: width*height multiplication is done in a signed int before
 * malloc — intentional integer-overflow vulnerability for fuzzer discovery.
 */
#define BINARY_MAP_MAGIC 0x4D5A

typedef struct {
    uint16_t magic;
    uint16_t width;
    uint16_t height;
} __attribute__((packed)) BinaryMapHeader;

/*
 * RLE format:
 *   header line: "RLE W H\n"
 *   body: pairs of <decimal-count><cell-char> with no separator
 *   Example: "RLE 4 2\n4#2.2#2#\n" → row0="####"  row1="..##" (but reversed)
 *
 * FUZZ NOTE: decoded length is not pre-validated against W*H before writing
 * into the output buffer — intentional OOB write for fuzzer discovery.
 */
typedef struct {
    int  width;
    int  height;
    char cells[MAP_MAX_HEIGHT * MAP_MAX_WIDTH];
} RLEMap;

/* Validation report returned by validate_maze_map() */
typedef struct {
    int  valid;
    int  exit_count;
    int  start_count;
    int  wall_count;
    int  floor_count;
    char error[128];
} MapValidation;

/* ---- Parsers ---- */
int parse_maze_map   (const uint8_t *data, size_t size, MazeMap *out);
int parse_binary_map (const uint8_t *data, size_t size, MazeMap *out);
int parse_rle_map    (const uint8_t *data, size_t size, RLEMap  *out);

/* ---- Validation ---- */
MapValidation validate_maze_map (const MazeMap *map);

/* ---- Conversion ---- */
int mazemap_to_rle (const MazeMap *in, RLEMap  *out);
int rle_to_mazemap (const RLEMap  *in, MazeMap *out);

/* ---- Serialisation ---- */
int mazemap_to_binary (const MazeMap *in, uint8_t *out, size_t out_size,
                       size_t *written);

#ifdef __cplusplus
}
#endif

#endif /* MAP_PARSER_H */
