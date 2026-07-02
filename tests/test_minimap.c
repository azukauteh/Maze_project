#include "minimap.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_W 320
#define TEST_H 240

static void init_state(GameState *state) {
    int i;

    memset(state, 0, sizeof(*state));
    state->width = TEST_W;
    state->height = TEST_H;
    state->player.px = 2.5f * (float)MAP_SCALE;
    state->player.py = 2.5f * (float)MAP_SCALE;
    state->player.pa = 0.0f;
    state->player.pdx = 1.0f;
    state->player.pdy = 0.0f;
    for (i = 0; i < MAP_WIDTH * MAP_HEIGHT; ++i) {
        state->map[i] = 0;
    }
    for (i = 0; i < MAP_WIDTH; ++i) {
        state->map[i] = 1;
        state->map[(MAP_HEIGHT - 1) * MAP_WIDTH + i] = 1;
    }
    for (i = 0; i < MAP_HEIGHT; ++i) {
        state->map[i * MAP_WIDTH] = 1;
        state->map[i * MAP_WIDTH + MAP_WIDTH - 1] = 1;
    }
    state->map[6 * MAP_WIDTH + 6] = 2;
}

static int count_nonzero(const uint32_t *pixels, int count) {
    int i;
    int nonzero = 0;

    for (i = 0; i < count; ++i) {
        if (pixels[i] != 0u) {
            nonzero++;
        }
    }
    return nonzero;
}

static int count_region_nonzero(const uint32_t *pixels, int w, int x0, int y0, int x1, int y1) {
    int x;
    int y;
    int count = 0;

    for (y = y0; y <= y1; ++y) {
        for (x = x0; x <= x1; ++x) {
            if (pixels[y * w + x] != 0u) {
                count++;
            }
        }
    }
    return count;
}

static void test_render_nonzero_and_null_path(void) {
    GameState state;
    MinimapConfig config = { 12, 4, 1, 1 };
    uint32_t *pixels = calloc(TEST_W * TEST_H, sizeof(uint32_t));

    assert(pixels != NULL);
    init_state(&state);
    minimap_render(&config, &state, NULL, pixels, TEST_W, TEST_H);
    assert(count_nonzero(pixels, TEST_W * TEST_H) > 0);
    free(pixels);
}

static void test_tile_hit_corners(void) {
    MinimapConfig config = { 10, 7, 1, 1 };
    int col;
    int row;

    assert(minimap_tile_at_screen(&config, 7, 7, &col, &row));
    assert(col == 0 && row == 0);
    assert(minimap_tile_at_screen(&config, 7 + 7 * 10 + 5, 7, &col, &row));
    assert(col == 7 && row == 0);
    assert(minimap_tile_at_screen(&config, 7, 7 + 7 * 10 + 5, &col, &row));
    assert(col == 0 && row == 7);
    assert(minimap_tile_at_screen(&config, 7 + 7 * 10 + 5, 7 + 7 * 10 + 5, &col, &row));
    assert(col == 7 && row == 7);
    assert(!minimap_tile_at_screen(&config, 0, 0, &col, &row));
}

static void test_all_wall_map(void) {
    GameState state;
    MinimapConfig config = minimap_default_config();
    uint32_t pixels[TEST_W * TEST_H];
    int i;

    init_state(&state);
    for (i = 0; i < MAP_WIDTH * MAP_HEIGHT; ++i) {
        state.map[i] = 1;
    }
    memset(pixels, 0, sizeof(pixels));
    minimap_render(&config, &state, NULL, pixels, TEST_W, TEST_H);
    assert(count_nonzero(pixels, TEST_W * TEST_H) > 0);
}

static void test_fov_draws_outside_player_dot(void) {
    GameState state;
    MinimapConfig config = { 16, 8, 1, 0 };
    uint32_t pixels[TEST_W * TEST_H];
    int player_x;
    int player_y;
    int outside;

    init_state(&state);
    memset(pixels, 0, sizeof(pixels));
    minimap_render(&config, &state, NULL, pixels, TEST_W, TEST_H);
    player_x = config.pad + (int)((state.player.px / (float)MAP_SCALE) * (float)config.tile_size);
    player_y = config.pad + (int)((state.player.py / (float)MAP_SCALE) * (float)config.tile_size);
    outside = count_region_nonzero(pixels, TEST_W, player_x + 10, player_y - 24, player_x + 64, player_y + 24);
    assert(outside > 0);
}

static void test_path_overlay(void) {
    GameState state;
    MinimapConfig config = { 12, 4, 1, 1 };
    MinimapPathResult path;
    uint32_t pixels[TEST_W * TEST_H];

    init_state(&state);
    memset(&path, 0, sizeof(path));
    path.found = 1;
    path.count = 3;
    path.cols[0] = 1; path.rows[0] = 1;
    path.cols[1] = 2; path.rows[1] = 1;
    path.cols[2] = 3; path.rows[2] = 1;
    memset(pixels, 0, sizeof(pixels));
    minimap_render(&config, &state, &path, pixels, TEST_W, TEST_H);
    assert(count_nonzero(pixels, TEST_W * TEST_H) > 0);
}

static void test_null_safety(void) {
    int col = 123;
    int row = 456;
    MinimapConfig config = minimap_default_config();

    minimap_render(NULL, NULL, NULL, NULL, 0, 0);
    assert(!minimap_tile_at_screen(&config, 1, 1, NULL, &row));
    assert(!minimap_tile_at_screen(&config, 1, 1, &col, NULL));
}

int main(void) {
    test_render_nonzero_and_null_path();
    test_tile_hit_corners();
    test_all_wall_map();
    test_fov_draws_outside_player_dot();
    test_path_overlay();
    test_null_safety();
    printf("test_minimap passed\n");
    return 0;
}

/* Minimap test notes 001. Pixel assertions avoid image snapshots. */
