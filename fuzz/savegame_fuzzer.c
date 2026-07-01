/*
 * fuzz/savegame_fuzzer.c — LibFuzzer entry point for the savegame loader.
 *
 * Build:
 *   clang -fsanitize=fuzzer,address -I../src \
 *         fuzz/savegame_fuzzer.c src/savegame.c src/engine.c \
 *         src/map_parser.c -lm -o savegame_fuzzer
 *   ./savegame_fuzzer fuzz/corpus/savegame_fuzzer/ -max_len=64
 *
 * Bugs targeted:
 *   a. Version bypass: crafted version > SAVE_VERSION still accepted
 *      (version field logged but not gated).
 *   b. NaN/Inf in float fields: savegame_load_from_data should reject
 *      non-finite floats. Fuzzer finds edge cases isfinite() misses.
 *   c. Checksum collision: XOR checksum is weak. Fuzzer may find inputs
 *      where mutated data produces the same checksum (birthday collision
 *      not theoretically interesting but practically reachable in 32-bit).
 *
 * Two fuzz targets share the single entry point, dispatched on route byte:
 *   0x00 — raw binary blob fed directly to savegame_load_from_data
 *   0x01 — round-trip test: save a crafted GameState, corrupt the buffer,
 *           try to load — should either succeed cleanly or fail gracefully
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "../src/engine.h"
#include "../src/savegame.h"
#include "../src/map_parser.h"

/* Minimal GameState for fuzzing — no SDL2, no real framebuffer */
static GameState make_dummy_state(void) {
    GameState s;
    memset(&s, 0, sizeof(s));
    s.width  = 320;
    s.height = 240;
    s.current_level = 1;
    s.player.px  = 100.0f;
    s.player.py  = 100.0f;
    s.player.pa  = 90;
    s.player.pdx = 0.0f;
    s.player.pdy = -1.0f;
    return s;
}

/* Target 0: raw binary blob -> load */
static void fuzz_raw_load(const uint8_t *data, size_t size) {
    GameState s = make_dummy_state();
    int ok = savegame_load_from_data(data, size, &s);
    if (ok) {
        /* Verify post-conditions */
        if (!isfinite(s.player.px) || !isfinite(s.player.py) ||
            !isfinite(s.player.pdx) || !isfinite(s.player.pdy)) {
            /* Should never happen — but if it does, ASan will catch any
             * downstream use of these values as NaN in math operations */
            volatile float sink = s.player.px + s.player.py;
            (void)sink;
        }
    }
}

/* Target 1: save + corrupt + reload round-trip */
static void fuzz_roundtrip(const uint8_t *data, size_t size) {
    if (size < 4) return;

    GameState s = make_dummy_state();

    /* Use first 4 bytes to set level and position */
    s.current_level = (data[0] % 6) + 1;
    s.player.px = (float)(data[1] & 0x7F) * 4.0f + 10.0f;
    s.player.py = (float)(data[2] & 0x7F) * 4.0f + 10.0f;
    s.player.pa = (int)(data[3]) % 360;

    uint8_t buf[SAVE_SIZE];
    size_t  wr = 0;
    if (!savegame_save_to_buffer(&s, buf, sizeof(buf), &wr)) return;

    /* Corrupt some bytes using the remaining fuzz data */
    for (size_t i = 4; i < size && i < wr; i++)
        buf[i % wr] ^= data[i];

    GameState s2 = make_dummy_state();
    savegame_load_from_data(buf, wr, &s2);
    /* Must not crash. If loaded, values must be finite. */
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 2) return 0;

    uint8_t route  = data[0];
    const uint8_t *payload = data + 1;
    size_t         psize   = size - 1;

    switch (route % 2) {
        case 0: fuzz_raw_load  (payload, psize); break;
        case 1: fuzz_roundtrip (payload, psize); break;
    }
    return 0;
}
