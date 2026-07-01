/*
 * tests/test_texture.c — unit tests for the texture subsystem.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "../src/texture.h"

static int runs = 0, passed = 0;
#define CHECK(c, m) do { runs++; if(c){passed++; printf("  PASS  %s\n",m);} \
    else printf("  FAIL  %s (line %d)\n",m,__LINE__); } while(0)

static void test_alloc(void) {
    printf("\n[texture] Alloc / free\n");
    Texture t;
    texture_init(&t);
    CHECK(t.pixels == NULL, "init: pixels NULL");

    CHECK(texture_alloc(&t, 64, 64, "test") == 1, "alloc 64x64");
    CHECK(t.pixels != NULL, "pixels allocated");
    CHECK(t.width == 64 && t.height == 64, "dimensions");
    CHECK(strcmp(t.name, "test") == 0, "name set");

    texture_free(&t);
    CHECK(t.pixels == NULL, "free: pixels NULL");

    /* Invalid dimensions */
    CHECK(texture_alloc(&t, 0,  64, "x") == 0, "zero width rejected");
    CHECK(texture_alloc(&t, 64,  0, "x") == 0, "zero height rejected");
    CHECK(texture_alloc(&t, TEX_MAX_DIM + 1, 64, "x") == 0, "oversize rejected");
    CHECK(texture_alloc(NULL, 64, 64, "x") == 0, "NULL tex rejected");
}

static void test_fill_solid(void) {
    printf("\n[texture] Fill solid\n");
    Texture t;
    texture_init(&t);
    texture_alloc(&t, 4, 4, "solid");

    texture_fill_solid(&t, 0xFFFF0000u);
    int all_red = 1;
    for (int i = 0; i < 16; i++)
        if (t.pixels[i] != 0xFFFF0000u) { all_red = 0; break; }
    CHECK(all_red, "all pixels red after fill_solid");

    texture_free(&t);
}

static void test_fill_checker(void) {
    printf("\n[texture] Fill checker\n");
    Texture t;
    texture_init(&t);
    texture_alloc(&t, 8, 8, "checker");
    texture_fill_checker(&t, 0xFFFFFFFFu, 0xFF000000u, 4);

    CHECK(t.pixels[0] == 0xFFFFFFFFu, "top-left white");
    CHECK(t.pixels[4] == 0xFF000000u, "top-right (offset 4) black");
    CHECK(t.pixels[4 * 8 + 0] == 0xFF000000u, "row 4 col 0 black");

    texture_free(&t);
}

static void test_fill_brick(void) {
    printf("\n[texture] Fill brick\n");
    Texture t;
    texture_init(&t);
    texture_alloc(&t, 32, 32, "brick");
    texture_fill_brick(&t, 0xFF808080u, 0xFFCC6622u, 16, 8);

    /* Row 0 col 0 should be mortar */
    CHECK(t.pixels[0] == 0xFF808080u, "row 0 col 0 is mortar");
    /* Row 4 col 4 (inside first brick, past mortar) */
    CHECK(t.pixels[4 * 32 + 4] == 0xFFCC6622u, "inside brick is face color");

    texture_free(&t);
}

static void test_fill_noise(void) {
    printf("\n[texture] Fill noise\n");
    Texture t;
    texture_init(&t);
    texture_alloc(&t, 16, 16, "noise");
    texture_fill_noise(&t, 0xFF808080u, 0xFF101010u, 42);

    /* Not all pixels the same */
    int uniform = 1;
    uint32_t first = t.pixels[0];
    for (int i = 1; i < 256; i++)
        if (t.pixels[i] != first) { uniform = 0; break; }
    CHECK(!uniform, "noise produces varying pixels");

    /* Same seed = same output */
    Texture t2;
    texture_init(&t2);
    texture_alloc(&t2, 16, 16, "noise2");
    texture_fill_noise(&t2, 0xFF808080u, 0xFF101010u, 42);
    int same = 1;
    for (int i = 0; i < 256; i++)
        if (t.pixels[i] != t2.pixels[i]) { same = 0; break; }
    CHECK(same, "same seed produces same noise");

    texture_free(&t);
    texture_free(&t2);
}

static void test_sample(void) {
    printf("\n[texture] Sampling\n");
    Texture t;
    texture_init(&t);
    texture_alloc(&t, 4, 4, "sample");
    texture_fill_checker(&t, 0xFFFFFFFFu, 0xFF000000u, 2);

    /* (0,0) top-left = white */
    CHECK(texture_sample(&t, 0.0f, 0.0f) == 0xFFFFFFFFu, "sample (0,0) white");
    /* (0.5,0) = pixel (2,0) = black */
    CHECK(texture_sample(&t, 0.5f, 0.0f) == 0xFF000000u, "sample (0.5,0) black");

    /* int sample */
    CHECK(texture_sample_int(&t, 0, 0) == 0xFFFFFFFFu, "int sample (0,0)");
    CHECK(texture_sample_int(&t, 2, 0) == 0xFF000000u, "int sample (2,0)");

    /* Wrap around */
    CHECK(texture_sample_int(&t, 4, 0) == t.pixels[0], "int sample wraps");
    CHECK(texture_sample_int(&t,-1, 0) == t.pixels[3], "int sample negative wraps");

    /* NULL safety */
    CHECK(texture_sample(NULL, 0.5f, 0.5f) == 0xFF808080u, "NULL tex fallback");

    texture_free(&t);
}

static void test_registry(void) {
    printf("\n[texture] Registry\n");
    TextureRegistry reg;
    texture_registry_init(&reg);
    CHECK(reg.count == 0, "init: count 0");

    Texture t;
    texture_init(&t);
    texture_alloc(&t, 8, 8, "wall_test");
    texture_fill_solid(&t, 0xFFAABBCCu);

    CHECK(texture_registry_add(&reg, t) == 1, "add succeeds");
    CHECK(reg.count == 1, "count == 1");

    Texture *found = texture_registry_get(&reg, "wall_test");
    CHECK(found != NULL, "get by name returns non-NULL");
    CHECK(found->pixels[0] == 0xFFAABBCCu, "retrieved pixel correct");

    CHECK(texture_registry_get(&reg, "missing") == NULL, "missing name = NULL");
    CHECK(texture_registry_get(NULL, "x")       == NULL, "NULL reg = NULL");

    texture_registry_free(&reg);
    CHECK(reg.count == 0, "free: count 0");
}

static void test_defaults(void) {
    printf("\n[texture] Default generation\n");
    TextureRegistry reg;
    texture_registry_generate_defaults(&reg);

    CHECK(reg.count >= 7, "at least 7 default textures");
    CHECK(texture_registry_get(&reg, "wall_grey")  != NULL, "wall_grey exists");
    CHECK(texture_registry_get(&reg, "wall_green") != NULL, "wall_green exists");
    CHECK(texture_registry_get(&reg, "wall_red")   != NULL, "wall_red exists");
    CHECK(texture_registry_get(&reg, "exit")       != NULL, "exit exists");
    CHECK(texture_registry_get(&reg, "floor")      != NULL, "floor exists");

    /* All generated textures have valid pixels */
    for (int i = 0; i < reg.count; i++) {
        CHECK(reg.textures[i].pixels != NULL, "generated texture has pixels");
        CHECK(reg.textures[i].width  > 0,     "generated texture has width");
        CHECK(reg.textures[i].height > 0,     "generated texture has height");
    }

    texture_registry_free(&reg);
}

static void test_ppm_valid(void) {
    printf("\n[texture] PPM loader (valid)\n");

    /* Minimal valid 2x2 P6 PPM: 4 RGB pixels */
    const uint8_t ppm[] = {
        'P','6','\n',
        '2',' ','2','\n',
        '2','5','5','\n',
        /* row 0 */
        0xFF,0x00,0x00,  /* red */
        0x00,0xFF,0x00,  /* green */
        /* row 1 */
        0x00,0x00,0xFF,  /* blue */
        0xFF,0xFF,0xFF,  /* white */
    };

    Texture t;
    texture_init(&t);
    int ok = texture_load_ppm(&t, ppm, sizeof(ppm), "ppm_test");
    CHECK(ok == 1, "valid 2x2 PPM loads");
    CHECK(t.width == 2 && t.height == 2, "PPM dimensions");

    texture_free(&t);
}

static void test_ppm_invalid(void) {
    printf("\n[texture] PPM loader (invalid)\n");
    Texture t;
    texture_init(&t);

    CHECK(texture_load_ppm(NULL,  10, &t, "x") == 0, "NULL data");
    CHECK(texture_load_ppm((const uint8_t *)"", 0, &t, "x") == 0, "empty");

    /* Wrong magic */
    const uint8_t p3[] = "P3\n2 2\n255\n255 0 0\n";
    CHECK(texture_load_ppm(p3, sizeof(p3), &t, "x") == 0, "P3 rejected");

    /* Oversized declared dimensions */
    const uint8_t big[] = "P6\n9999 9999\n255\n";
    CHECK(texture_load_ppm(big, sizeof(big), &t, "x") == 0,
          "oversized dimensions rejected");
}

int main(void) {
    printf("=== texture tests ===\n");
    test_alloc();
    test_fill_solid();
    test_fill_checker();
    test_fill_brick();
    test_fill_noise();
    test_sample();
    test_registry();
    test_defaults();
    test_ppm_valid();
    test_ppm_invalid();
    printf("\n%d / %d passed\n", passed, runs);
    return passed == runs ? 0 : 1;
}
