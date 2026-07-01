/*
 * fuzz/map_fuzzer.c — LibFuzzer entry points for all three map parsers.
 *
 * Build:
 *   clang -fsanitize=fuzzer,address -I../src \
 *         fuzz/map_fuzzer.c src/map_parser.c -o map_fuzzer
 *   ./map_fuzzer fuzz/corpus/map_fuzzer/ -max_len=512
 *
 * Three fuzz targets share one LLVMFuzzerTestOneInput by dispatching on the
 * first byte of input (routing byte):
 *   0x00 — text parser
 *   0x01 — binary parser
 *   0x02 — RLE parser
 * Remaining bytes are fed to the selected parser.
 *
 * Bugs targeted:
 *   - text:   buffer overflow on oversized input row
 *   - binary: integer overflow in width*height (VULN a in map_parser.c)
 *   - RLE:    out-of-bounds write when decoded length > W*H (VULN b)
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include "../src/map_parser.h"

/* ---- Text parser target ---- */

static void fuzz_text(const uint8_t *data, size_t size) {
    MazeMap map;
    int ok = parse_maze_map(data, size, &map);
    if (ok) {
        /* Exercise validation on every successfully parsed map */
        MapValidation v = validate_maze_map(&map);
        (void)v;

        /* Exercise serialisation round-trip */
        uint8_t  bin[8 + MAP_MAX_WIDTH * MAP_MAX_HEIGHT];
        size_t   written = 0;
        if (mazemap_to_binary(&map, bin, sizeof(bin), &written)) {
            MazeMap map2;
            parse_binary_map(bin, written, &map2);
        }
    }
}

/* ---- Binary parser target ---- */

static void fuzz_binary(const uint8_t *data, size_t size) {
    MazeMap map;
    int ok = parse_binary_map(data, size, &map);
    if (ok) {
        /* Access every declared cell to trigger any OOB under ASan */
        volatile char sink = 0;
        for (int r = 0; r < map.height; r++)
            for (int c = 0; c < map.width; c++)
                sink ^= map.cells[r][c];
        (void)sink;

        /* Round-trip back to binary */
        uint8_t out[8 + MAP_MAX_WIDTH * MAP_MAX_HEIGHT];
        size_t  wr = 0;
        mazemap_to_binary(&map, out, sizeof(out), &wr);
    }
}

/* ---- RLE parser target ---- */

static void fuzz_rle(const uint8_t *data, size_t size) {
    RLEMap rle;
    int ok = parse_rle_map(data, size, &rle);
    if (ok) {
        /* Convert to MazeMap and validate */
        MazeMap map;
        if (rle_to_mazemap(&rle, &map)) {
            MapValidation v = validate_maze_map(&map);
            (void)v;

            /* Round-trip RLE */
            RLEMap rle2;
            mazemap_to_rle(&map, &rle2);
        }

        /* Access flat cells buffer */
        volatile char sink = 0;
        int total = rle.width * rle.height;
        for (int i = 0; i < total; i++)
            sink ^= rle.cells[i];
        (void)sink;
    }
}

/* ---- Router ---- */

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 2) return 0;

    uint8_t route = data[0];
    const uint8_t *payload = data + 1;
    size_t         psize   = size - 1;

    switch (route % 3) {
        case 0: fuzz_text  (payload, psize); break;
        case 1: fuzz_binary(payload, psize); break;
        case 2: fuzz_rle   (payload, psize); break;
    }
    return 0;
}
