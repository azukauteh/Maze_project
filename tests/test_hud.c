/*
 * tests/test_hud.c — unit tests for the HUD subsystem.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../src/hud.h"

static int runs = 0, passed = 0;
#define CHECK(c, m) do { runs++; if(c){passed++; printf("  PASS  %s\n",m);} \
    else printf("  FAIL  %s (line %d)\n",m,__LINE__); } while(0)

#define W 320
#define H 240

static uint32_t *make_fb(void) {
    uint32_t *fb = (uint32_t *)calloc(W * H, sizeof(uint32_t));
    return fb;
}

/* ------------------------------------------------------------------ */
/* State tests                                                         */
/* ------------------------------------------------------------------ */

static void test_init(void) {
    printf("\n[hud] Initialization\n");
    HUDState hud;
    hud_init(&hud, 6);

    CHECK(hud.max_levels     == 6, "max_levels set");
    CHECK(hud.current_level  == 1, "current_level starts at 1");
    CHECK(hud.flash_timer    == 0, "flash_timer starts at 0");
    CHECK(hud.show_crosshair == 1, "crosshair enabled by default");
    CHECK(hud.frame_count    == 0, "frame_count starts at 0");
}

static void test_update(void) {
    printf("\n[hud] Update\n");
    HUDState hud;
    hud_init(&hud, 6);

    hud_update(&hud);
    CHECK(hud.frame_count == 1, "frame_count increments");

    hud_flash(&hud, "LEVEL 2");
    CHECK(hud.flash_timer == HUD_MSG_DURATION, "flash_timer set on flash");

    /* Timer counts down */
    for (int i = 0; i < 10; i++) hud_update(&hud);
    CHECK(hud.flash_timer == HUD_MSG_DURATION - 10, "flash_timer decrements");

    /* Timer does not go below 0 */
    for (int i = 0; i < HUD_MSG_DURATION + 100; i++) hud_update(&hud);
    CHECK(hud.flash_timer == 0, "flash_timer clamps at 0");
}

static void test_setters(void) {
    printf("\n[hud] Setters\n");
    HUDState hud;
    hud_init(&hud, 6);

    hud_set_level(&hud, 4);
    CHECK(hud.current_level == 4, "set_level works");

    hud_set_player(&hud, 100.0f, 200.0f, 90.0f);
    CHECK(hud.player_x == 100.0f, "player_x set");
    CHECK(hud.player_y == 200.0f, "player_y set");
    CHECK(hud.player_angle == 90.0f, "player_angle set");

    hud_flash(&hud, "hello");
    CHECK(strcmp(hud.flash_msg, "hello") == 0, "flash_msg set");
    CHECK(hud.flash_timer == HUD_MSG_DURATION, "flash_timer reset");
}

static void test_flash_truncate(void) {
    printf("\n[hud] Flash message truncation\n");
    HUDState hud;
    hud_init(&hud, 6);

    char long_msg[HUD_MSG_MAX_LEN + 32];
    memset(long_msg, 'A', sizeof(long_msg) - 1);
    long_msg[sizeof(long_msg) - 1] = '\0';

    hud_flash(&hud, long_msg);
    CHECK(strlen(hud.flash_msg) == HUD_MSG_MAX_LEN - 1,
          "flash_msg truncated to max len");
    CHECK(hud.flash_msg[HUD_MSG_MAX_LEN - 1] == '\0',
          "flash_msg null-terminated after truncation");
}

/* ------------------------------------------------------------------ */
/* Drawing primitives                                                  */
/* ------------------------------------------------------------------ */

static void test_draw_rect(void) {
    printf("\n[hud] draw_rect\n");
    uint32_t *fb = make_fb();

    hud_draw_rect(fb, W, H, 10, 10, 20, 10, 0xFFFF0000u);

    /* Interior should be red */
    CHECK(fb[10 * W + 10] == 0xFFFF0000u, "top-left corner filled");
    CHECK(fb[10 * W + 29] == 0xFFFF0000u, "top-right corner filled");
    CHECK(fb[19 * W + 10] == 0xFFFF0000u, "bottom-left corner filled");

    /* Outside should be black */
    CHECK(fb[9  * W + 10] == 0x00000000u, "above rect untouched");
    CHECK(fb[20 * W + 10] == 0x00000000u, "below rect untouched");

    /* NULL safety */
    hud_draw_rect(NULL, W, H, 0, 0, 10, 10, 0xFFFFFFFFu); /* must not crash */

    /* Clip to screen bounds */
    hud_draw_rect(fb, W, H, W - 5, H - 5, 20, 20, 0xFF0000FFu);
    CHECK(fb[(H-1)*W + (W-1)] == 0xFF0000FFu, "clipped rect draws at screen edge");

    free(fb);
}

static void test_draw_char(void) {
    printf("\n[hud] draw_char\n");
    uint32_t *fb = make_fb();

    /* Draw 'A' at (10,10) scale 1 — should set at least one pixel */
    hud_draw_char(fb, W, H, 10, 10, 'A', 0xFFFFFFFFu, 1);

    int any_set = 0;
    for (int y = 10; y < 10 + 7; y++)
        for (int x = 10; x < 10 + 5; x++)
            if (fb[y * W + x] == 0xFFFFFFFFu) { any_set = 1; break; }
    CHECK(any_set, "draw_char sets at least one pixel");

    /* Scale 2 — glyph should be larger */
    memset(fb, 0, W * H * sizeof(uint32_t));
    hud_draw_char(fb, W, H, 10, 10, 'A', 0xFF0000FFu, 2);
    int count_s2 = 0;
    for (int y = 10; y < 10 + 14; y++)
        for (int x = 10; x < 10 + 10; x++)
            if (fb[y * W + x] == 0xFF0000FFu) count_s2++;
    CHECK(count_s2 > 0, "scale 2 draws pixels");

    /* NULL safety */
    hud_draw_char(NULL, W, H, 0, 0, 'X', 0xFFFFFFFFu, 1);

    free(fb);
}

static void test_draw_string(void) {
    printf("\n[hud] draw_string\n");
    uint32_t *fb = make_fb();

    hud_draw_string(fb, W, H, 0, 0, "HI", 0xFFFFFFFFu, 1);
    int any = 0;
    for (int i = 0; i < W * 8; i++)
        if (fb[i]) { any = 1; break; }
    CHECK(any, "draw_string draws something");

    /* NULL string — must not crash */
    hud_draw_string(fb, W, H, 0, 0, NULL, 0xFFFFFFFFu, 1);
    /* NULL pixels */
    hud_draw_string(NULL, W, H, 0, 0, "X", 0xFFFFFFFFu, 1);

    free(fb);
}

static void test_draw_number(void) {
    printf("\n[hud] draw_number\n");
    uint32_t *fb = make_fb();

    hud_draw_number(fb, W, H, 0, 0, 42, 0xFFFFFFFFu, 1);
    int any = 0;
    for (int i = 0; i < W * 8; i++)
        if (fb[i]) { any = 1; break; }
    CHECK(any, "draw_number draws digits");

    /* Zero */
    memset(fb, 0, W * H * sizeof(uint32_t));
    hud_draw_number(fb, W, H, 0, 0, 0, 0xFFFFFFFFu, 1);
    any = 0;
    for (int i = 0; i < W * 8; i++)
        if (fb[i]) { any = 1; break; }
    CHECK(any, "draw_number draws 0");

    free(fb);
}

/* ------------------------------------------------------------------ */
/* Render integration                                                  */
/* ------------------------------------------------------------------ */

static void test_render(void) {
    printf("\n[hud] Full render\n");
    uint32_t *fb = make_fb();
    HUDState hud;
    hud_init(&hud, 6);
    hud_set_level(&hud, 3);
    hud_flash(&hud, "LEVEL 3");

    /* Must not crash on valid inputs */
    hud_render(&hud, fb, W, H);

    /* Level badge area (top-right) should have some non-zero pixels */
    int badge_pixels = 0;
    for (int y = 0; y < 30; y++)
        for (int x = W/2; x < W; x++)
            if (fb[y * W + x]) badge_pixels++;
    CHECK(badge_pixels > 0, "level badge draws pixels");

    /* Crosshair area (centre) should have pixels */
    int cross_pixels = 0;
    int cx = W/2, cy = H/2;
    for (int y = cy - 15; y < cy + 15; y++)
        for (int x = cx - 15; x < cx + 15; x++)
            if (x >= 0 && x < W && y >= 0 && y < H && fb[y*W+x])
                cross_pixels++;
    CHECK(cross_pixels > 0, "crosshair draws pixels");

    /* NULL safety */
    hud_render(NULL, fb, W, H);
    hud_render(&hud, NULL, W, H);

    free(fb);
}

static void test_debug_overlay(void) {
    printf("\n[hud] Debug overlay\n");
    uint32_t *fb = make_fb();
    HUDState hud;
    hud_init(&hud, 6);
    hud.show_debug = 1;
    hud_set_player(&hud, 128.0f, 256.0f, 45.0f);
    hud_render(&hud, fb, W, H);

    /* Minimap area ends at ~row 70 (8 tiles * 8px scale + pad). 
       Debug overlay starts just below that. Check something is drawn. */
    int debug_pixels = 0;
    for (int y = 70; y < 90; y++)
        for (int x = 0; x < 120; x++)
            if (fb[y * W + x]) debug_pixels++;
    CHECK(debug_pixels > 0, "debug overlay draws something");

    free(fb);
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */

int main(void) {
    printf("=== hud tests ===\n");
    test_init();
    test_update();
    test_setters();
    test_flash_truncate();
    test_draw_rect();
    test_draw_char();
    test_draw_string();
    test_draw_number();
    test_render();
    test_debug_overlay();
    printf("\n%d / %d passed\n", passed, runs);
    return passed == runs ? 0 : 1;
}
