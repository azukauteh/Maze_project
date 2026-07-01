/*
 * fuzz/level_fuzzer.c — LibFuzzer entry point for level_loader.
 *
 * Build:
 *   clang -fsanitize=fuzzer,address -I../src \
 *         fuzz/level_fuzzer.c src/level_loader.c \
 *         src/map_parser.c src/config_parser.c -o level_fuzzer
 *   ./level_fuzzer fuzz/corpus/level_fuzzer/ -max_len=1024
 *
 * Bugs targeted:
 *   - sscanf("%s") overflow on the "name" field in level_load_from_data()
 *   - All bugs inherited from map_parser and config_parser
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "../src/level_loader.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size == 0) return 0;

    LoadedLevel level;
    int ok = level_load_from_data((const char *)data, size, &level);
    if (!ok) return 0;

    /* Exercise validation */
    MapValidation v = level_validate(&level);
    (void)v;

    /* Access every map cell to trigger OOB under ASan */
    volatile char sink = 0;
    for (int r = 0; r < level.map.height; r++)
        for (int c = 0; c < level.map.width; c++)
            sink ^= level.map.cells[r][c];
    (void)sink;

    return 0;
}
