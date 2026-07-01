/*
 * fuzz/texture_fuzzer.c — LibFuzzer entry point for texture_load_ppm().
 *
 * Build:
 *   clang -fsanitize=fuzzer,address -I../src \
 *         fuzz/texture_fuzzer.c src/texture.c -lm -o texture_fuzzer
 *   ./texture_fuzzer fuzz/corpus/texture_fuzzer/ -max_len=4096
 *
 * Bug targeted: OOB heap write in texture_load_ppm() when binary body
 * is larger than declared width*height pixels.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

#include "../src/texture.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 4) return 0;

    Texture tex;
    texture_init(&tex);

    int ok = texture_load_ppm(&tex, data, size, "fuzz_tex");
    if (ok) {
        /* Access every declared pixel under ASan */
        volatile uint32_t sink = 0;
        int n = tex.width * tex.height;
        for (int i = 0; i < n; i++)
            sink ^= tex.pixels[i];
        (void)sink;

        /* Sample at boundary UVs */
        texture_sample    (&tex, 0.0f, 0.0f);
        texture_sample    (&tex, 1.0f, 1.0f);
        texture_sample    (&tex, 0.5f, 0.5f);
        texture_sample_int(&tex, 0, 0);
        texture_sample_int(&tex, tex.width - 1, tex.height - 1);
    }

    texture_free(&tex);
    return 0;
}
