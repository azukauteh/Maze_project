/*
 * tests/test_level.c — regression tests for engine level logic.
 *
 * Compile and run without SDL2:
 *   cd build && ctest --output-on-failure
 *   ./tests/test_level
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

#include "../src/engine.h"
#include "../src/map_parser.h"

/* ------------------------------------------------------------------ */
/* Harness                                                             */
/* ------------------------------------------------------------------ */

static int tests_run    = 0;
static int tests_passed = 0;

#define CHECK(cond, msg) do { \
    tests_run++; \
    if (cond) { \
        tests_passed++; \
        printf("  PASS  %s\n", msg); \
    } else { \
        printf("  FAIL  %s  (line %d)\n", msg, __LINE__); \
    } \
} while(0)

static GameState make_state(void) {
    GameState s;
    memset(&s, 0, sizeof(s));
    s.width  = 320;
    s.height = 240;
    engine_init(&s);
    return s;
}

static void free_state(GameState *s) {
    if (s->pixels) { free(s->pixels); s->pixels = NULL; }
}

static int count_cell(const GameState *s, int value) {
    int n = 0;
    for (int i = 0; i < MAP_WIDTH * MAP_HEIGHT; i++)
        if (s->map[i] == value) n++;
    return n;
}

static int outer_walls_intact(const GameState *s) {
    for (int c = 0; c < MAP_WIDTH; c++) {
        if (s->map[0 * MAP_WIDTH + c] != 1) return 0;
        if (s->map[(MAP_HEIGHT-1) * MAP_WIDTH + c] != 1) return 0;
    }
    for (int r = 0; r < MAP_HEIGHT; r++) {
        if (s->map[r * MAP_WIDTH + 0] != 1) return 0;
        if (s->map[r * MAP_WIDTH + (MAP_WIDTH-1)] != 1) return 0;
    }
    return 1;
}

static int no_invalid_cells(const GameState *s) {
    for (int i = 0; i < MAP_WIDTH * MAP_HEIGHT; i++) {
        int v = s->map[i];
        if (v != 0 && v != 1 && v != 2) return 0;
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* 1. Initialization                                                   */
/* ------------------------------------------------------------------ */

static void test_init(void) {
    printf("\n[1] Initialization\n");
    GameState s = make_state();

    CHECK(s.current_level  == 1,    "current_level starts at 1");
    CHECK(s.level_complete == 0,    "level_complete starts at 0");
    CHECK(s.pixels         != NULL, "framebuffer allocated");
    CHECK(s.player.px > 0,          "player x > 0");
    CHECK(s.player.py > 0,          "player y > 0");

    float len = sqrtf(s.player.pdx * s.player.pdx + s.player.pdy * s.player.pdy);
    CHECK(fabsf(len - 1.0f) < 0.001f, "direction vector is unit length");
    CHECK(engine_num_levels() == 6,    "engine reports 6 total levels");

    free_state(&s);
}

/* ------------------------------------------------------------------ */
/* 2. Wall collision                                                   */
/* ------------------------------------------------------------------ */

static void test_collision(void) {
    printf("\n[2] Wall collision\n");
    GameState s = make_state();

    /* Face west into the border wall from col 1 */
    s.player.px  = 96.0f;
    s.player.py  = 96.0f;
    s.player.pa  = 180;
    s.player.pdx = -1.0f;
    s.player.pdy =  0.0f;
    float ox = s.player.px, oy = s.player.py;
    engine_input(&s, 'W', 1);
    CHECK(s.player.px == ox, "x blocked by west border wall");
    CHECK(s.player.py == oy, "y unchanged on west collision");
    CHECK(s.level_complete == 0, "no level_complete on wall hit");

    /* Face east into col-2 internal wall (level 1, row 1) */
    s.player.px  = 96.0f;
    s.player.py  = 96.0f;
    s.player.pa  = 0;
    s.player.pdx = 1.0f;
    s.player.pdy = 0.0f;
    ox = s.player.px;
    engine_input(&s, 'W', 1);
    CHECK(s.player.px == ox, "x blocked by internal east wall");

    /* Backward move into wall should also be blocked */
    s.player.px  = 96.0f;
    s.player.py  = 96.0f;
    s.player.pa  = 0;
    s.player.pdx = 1.0f;
    s.player.pdy = 0.0f;
    /* moving backward = west = into border */
    engine_input(&s, 'S', 1);
    CHECK(s.player.px == 96.0f, "backward move into border blocked");

    free_state(&s);
}

/* ------------------------------------------------------------------ */
/* 3. Exit detection                                                   */
/* ------------------------------------------------------------------ */

static void test_exit(void) {
    printf("\n[3] Exit detection\n");
    GameState s = make_state();

    /* Exit tile (col=6, row=6) world x: 384..447, world y: 384..447.
       Stand at x=379, face east. One step (speed=5) -> x=384 = col 6. */
    s.player.px  = 379.0f;
    s.player.py  = 416.0f;
    s.player.pa  = 0;
    s.player.pdx = 1.0f;
    s.player.pdy = 0.0f;

    CHECK(s.level_complete == 0, "level_complete is 0 before exit");
    engine_input(&s, 'W', 1);
    CHECK(s.level_complete == 1, "level_complete set after stepping into exit");

    /* Input ignored once level_complete is set */
    int old_level = s.current_level;
    engine_input(&s, 'W', 1);
    CHECK(s.current_level == old_level, "current_level not changed by input after exit");

    free_state(&s);
}

/* ------------------------------------------------------------------ */
/* 4. Level loading — all 6 levels                                    */
/* ------------------------------------------------------------------ */

static void test_level_load(void) {
    printf("\n[4] Level loading (all 6)\n");
    GameState s = make_state();
    int max = engine_num_levels();

    int prev_map[MAP_WIDTH * MAP_HEIGHT];
    memcpy(prev_map, s.map, sizeof(prev_map));

    for (int target = 2; target <= max; target++) {
        engine_next_level(&s);

        char msg[64];
        snprintf(msg, sizeof(msg), "current_level == %d", target);
        CHECK(s.current_level == target, msg);
        CHECK(s.level_complete == 0, "level_complete reset");
        CHECK(count_cell(&s, 2) == 1, "exactly one exit tile");
        CHECK(outer_walls_intact(&s), "outer walls solid");
        CHECK(no_invalid_cells(&s),   "no invalid cell values");

        int diff = 0;
        for (int i = 0; i < MAP_WIDTH * MAP_HEIGHT; i++)
            if (s.map[i] != prev_map[i]) { diff = 1; break; }
        CHECK(diff, "map differs from previous level");

        /* Direction vector must still be unit length after spawn reset */
        float len = sqrtf(s.player.pdx * s.player.pdx +
                          s.player.pdy * s.player.pdy);
        CHECK(fabsf(len - 1.0f) < 0.001f, "spawn direction vector unit length");

        memcpy(prev_map, s.map, sizeof(prev_map));
    }

    /* Beyond last level: must not crash */
    engine_next_level(&s);
    CHECK(1, "engine_next_level beyond max does not crash");

    free_state(&s);
}

/* ------------------------------------------------------------------ */
/* 5. Map invariants — all 6 levels via sequential loading            */
/* ------------------------------------------------------------------ */

static void test_map_invariants(void) {
    printf("\n[5] Map invariants (all 6 levels)\n");
    GameState s = make_state();
    int max = engine_num_levels();

    for (int lv = 1; lv <= max; lv++) {
        char msg[80];

        snprintf(msg, sizeof(msg), "level %d: exactly one exit", lv);
        CHECK(count_cell(&s, 2) == 1, msg);

        snprintf(msg, sizeof(msg), "level %d: at least 10 wall tiles", lv);
        CHECK(count_cell(&s, 1) >= 10, msg);

        snprintf(msg, sizeof(msg), "level %d: outer border is solid", lv);
        CHECK(outer_walls_intact(&s), msg);

        snprintf(msg, sizeof(msg), "level %d: no invalid cell values", lv);
        CHECK(no_invalid_cells(&s), msg);

        snprintf(msg, sizeof(msg), "level %d: player spawns in open tile", lv);
        int cell = s.map[(int)(s.player.py / 64) * MAP_WIDTH +
                         (int)(s.player.px / 64)];
        CHECK(cell == 0, msg);

        if (lv < max) engine_next_level(&s);
    }

    free_state(&s);
}

/* ------------------------------------------------------------------ */
/* 6. Map parser                                                       */
/* ------------------------------------------------------------------ */

static void test_map_parser(void) {
    printf("\n[6] Map parser\n");
    MazeMap m;

    const char *valid =
        "3 3\n"
        "###\n"
        "#.#\n"
        "###\n";
    CHECK(parse_maze_map((const uint8_t *)valid, strlen(valid), &m) == 1,
          "parse valid 3x3 map succeeds");
    CHECK(m.width  == 3, "parsed width == 3");
    CHECK(m.height == 3, "parsed height == 3");
    CHECK(m.cells[0][0] == '#', "top-left cell is wall");
    CHECK(m.cells[1][1] == '.', "centre cell is floor");

    CHECK(parse_maze_map((const uint8_t *)"", 0, &m) == 0,
          "empty input returns 0");
    CHECK(parse_maze_map(NULL, 10, &m) == 0,
          "NULL data returns 0");

    const char *bad_cell = "2 2\n#X\n##\n";
    CHECK(parse_maze_map((const uint8_t *)bad_cell, strlen(bad_cell), &m) == 0,
          "invalid cell char returns 0");

    const char *short_row = "3 2\n##\n###\n";
    CHECK(parse_maze_map((const uint8_t *)short_row, strlen(short_row), &m) == 0,
          "row width mismatch returns 0");

    const char *with_se = "3 3\n#S#\n#.#\n#E#\n";
    CHECK(parse_maze_map((const uint8_t *)with_se, strlen(with_se), &m) == 1,
          "map with S and E parses successfully");
    CHECK(m.cells[0][1] == 'S', "S cell parsed correctly");
    CHECK(m.cells[2][1] == 'E', "E cell parsed correctly");

    /* Oversized dimensions */
    const char *too_big = "33 33\n";
    CHECK(parse_maze_map((const uint8_t *)too_big, strlen(too_big), &m) == 0,
          "dimensions exceeding MAP_MAX rejected");

    /* Zero dimension */
    const char *zero_w = "0 3\n###\n###\n###\n";
    CHECK(parse_maze_map((const uint8_t *)zero_w, strlen(zero_w), &m) == 0,
          "zero width rejected");
}

/* ------------------------------------------------------------------ */
/* 7. Rotation correctness                                             */
/* ------------------------------------------------------------------ */

static void test_rotation(void) {
    printf("\n[7] Rotation\n");
    GameState s = make_state();

    /* Rotating 72 times by 5 degrees = 360 degrees = full circle */
    int start_pa = s.player.pa;
    for (int i = 0; i < 72; i++)
        engine_input(&s, 'A', 1);  /* turn left */
    CHECK(s.player.pa == start_pa, "72x5deg left rotation returns to start angle");

    /* Direction vector must remain unit length through all rotations */
    float len = sqrtf(s.player.pdx * s.player.pdx + s.player.pdy * s.player.pdy);
    CHECK(fabsf(len - 1.0f) < 0.01f, "direction vector unit after full rotation");

    free_state(&s);
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */

int main(void) {
    printf("=== Maze engine regression tests ===\n");

    test_init();
    test_collision();
    test_exit();
    test_level_load();
    test_map_invariants();
    test_map_parser();
    test_rotation();

    printf("\n%d / %d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
