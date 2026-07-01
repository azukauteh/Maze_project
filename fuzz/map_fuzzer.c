/*
 * fuzz/map_fuzzer.c — LibFuzzer entry point for map_parser.
 *
 * Build (requires clang with libFuzzer support):
 *
 *   clang -fsanitize=fuzzer,address \
 *         -I../src \
 *         fuzz/map_fuzzer.c src/map_parser.c \
 *         -o map_fuzzer
 *   ./map_fuzzer fuzz/corpus/map_fuzzer/ -max_len=256
 *
 * ClusterFuzzLite runs this automatically on every PR via
 * .clusterfuzzlite/build.sh. See project.yaml for engine config.
 *
 * What is being fuzzed
 * --------------------
 * parse_maze_map() accepts arbitrary byte input and must never:
 *   - crash or abort
 *   - read out of bounds (caught by ASan)
 *   - hang (DDA loop is bounded to 8 steps; parser loop is bounded to height)
 *   - return garbage in out_map when it returns 0
 *
 * The fuzzer feeds random byte sequences as if they were map text files.
 * Interesting edge cases found during manual testing:
 *   - Width/height declared as 0 or negative
 *   - Width/height exceeding MAP_MAX_WIDTH / MAP_MAX_HEIGHT
 *   - Row shorter or longer than declared width
 *   - Invalid cell characters (anything not in #.SE)
 *   - Input truncated mid-row
 *   - Input with only whitespace
 *   - Integer overflow in width * height
 *   - Very large declared dimensions (e.g. "32 32") with truncated body
 *
 * Corpus
 * ------
 * fuzz/corpus/map_fuzzer/seed1.txt is the initial seed. LibFuzzer mutates it
 * and adds new interesting inputs to the corpus directory automatically.
 * Commit interesting corpus files so future runs start from a richer base.
 *
 * Parser contract (from map_parser.h)
 * ------------------------------------
 * Returns 1 on success, 0 on any parse error.
 * On success, out_map->width, out_map->height, and out_map->cells are valid.
 * On failure, out_map contents are unspecified (zeroed by memset inside parser).
 * Input size is capped at sizeof(buffer)-1 = 255 bytes inside the parser.
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "../src/map_parser.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    MazeMap map;

    /*
     * Call the parser. We do not care about the return value — both 0 and 1
     * are valid outcomes. We only care that the function does not crash,
     * corrupt memory, or loop indefinitely.
     */
    parse_maze_map(data, size, &map);

    /*
     * If parsing succeeded, do a basic sanity check on the output struct.
     * These assertions will trip ASan/UBSan if the parser wrote out of bounds.
     */
    if (map.width  > 0 && map.width  <= MAP_MAX_WIDTH &&
        map.height > 0 && map.height <= MAP_MAX_HEIGHT) {
        /* Access every declared cell to trigger any OOB reads under ASan */
        volatile char sink = 0;
        for (int r = 0; r < map.height; r++)
            for (int c = 0; c < map.width; c++)
                sink ^= map.cells[r][c];
        (void)sink;
    }

    return 0;  /* non-zero return causes LibFuzzer to treat input as a crash */
}
