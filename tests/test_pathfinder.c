/*
 * tests/test_pathfinder.c — unit tests for the BFS pathfinder.
 *
 * Key invariant: every level in the engine must be solvable.
 * This test calls pf_solve_from_player() on all 6 levels and fails
 * if any level has no path from spawn to exit.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "../src/engine.h"
#include "../src/pathfinder.h"

static int runs = 0, passed = 0;
#define CHECK(c, m) do { runs++; if(c){passed++; printf("  PASS  %s\n",m);} \
    else printf("  FAIL  %s (line %d)\n",m,__LINE__); } while(0)

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

/* ------------------------------------------------------------------ */
/* Tile utilities                                                      */
/* ------------------------------------------------------------------ */

static void test_world_tile_conversion(void) {
    printf("\n[pathfinder] World<->tile conversion\n");

    PFTile t = pf_world_to_tile(64.0f, 128.0f);
    CHECK(t.col == 1, "world (64,*) -> tile col 1");
    CHECK(t.row == 2, "world (*,128) -> tile row 2");

    PFTile t2 = pf_world_to_tile(0.0f, 0.0f);
    CHECK(t2.col == 0 && t2.row == 0, "world (0,0) -> tile (0,0)");

    float wx, wy;
    PFTile t3 = { 3, 5 };
    pf_tile_to_world_centre(t3, &wx, &wy);
    CHECK(fabsf(wx - (3*64+32)) < 0.01f, "tile->world x centre");
    CHECK(fabsf(wy - (5*64+32)) < 0.01f, "tile->world y centre");
}

static void test_passable(void) {
    printf("\n[pathfinder] Tile passability\n");
    GameState s = make_state();

    /* Border tiles (row 0, col 0) are always walls */
    CHECK(pf_tile_is_passable(&s, 0, 0) == 0, "border wall not passable");
    CHECK(pf_tile_is_passable(&s, 7, 7) == 0, "border wall not passable");

    /* Open interior tile */
    CHECK(pf_tile_is_passable(&s, 1, 4) == 1, "open tile passable");

    /* Exit tile (CELL_EXIT = 2) is passable */
    CHECK(pf_tile_is_passable(&s, 6, 6) == 1, "exit tile passable");

    /* Out of bounds */
    CHECK(pf_tile_is_passable(&s, -1, 0)       == 0, "col -1 not passable");
    CHECK(pf_tile_is_passable(&s,  0, MAP_HEIGHT) == 0, "row overflow not passable");

    /* NULL state */
    CHECK(pf_tile_is_passable(NULL, 1, 1) == 0, "NULL state not passable");

    free_state(&s);
}

/* ------------------------------------------------------------------ */
/* BFS correctness                                                     */
/* ------------------------------------------------------------------ */

static void test_simple_solve(void) {
    printf("\n[pathfinder] Simple BFS solve\n");
    GameState s = make_state(); /* level 1 */

    PFResult r = pf_solve_from_player(&s);
    CHECK(r.found == 1, "level 1 is solvable from spawn");
    CHECK(r.length > 0, "path length > 0");

    /* First tile is spawn tile */
    PFTile spawn = pf_world_to_tile(s.player.px, s.player.py);
    CHECK(r.path[0].col == spawn.col && r.path[0].row == spawn.row,
          "path starts at spawn tile");

    /* Last tile is exit (CELL_EXIT) */
    PFTile goal = r.path[r.length - 1];
    int cell = s.map[goal.row * MAP_WIDTH + goal.col];
    CHECK(cell == 2, "path ends at exit tile");

    /* All intermediate tiles are passable */
    int all_passable = 1;
    for (int i = 0; i < r.length; i++) {
        if (!pf_tile_is_passable(&s, r.path[i].col, r.path[i].row)) {
            all_passable = 0; break;
        }
    }
    CHECK(all_passable, "all path tiles are passable");

    /* Consecutive tiles are adjacent (4-connected) */
    int all_adjacent = 1;
    for (int i = 1; i < r.length; i++) {
        int dc = abs(r.path[i].col - r.path[i-1].col);
        int dr = abs(r.path[i].row - r.path[i-1].row);
        if (!((dc == 1 && dr == 0) || (dc == 0 && dr == 1))) {
            all_adjacent = 0; break;
        }
    }
    CHECK(all_adjacent, "consecutive path tiles are 4-adjacent");

    free_state(&s);
}

static void test_all_levels_solvable(void) {
    printf("\n[pathfinder] All 6 levels solvable\n");
    GameState s = make_state();
    int max = engine_num_levels();

    for (int lv = 1; lv <= max; lv++) {
        PFResult r = pf_solve_from_player(&s);
        char msg[64];
        snprintf(msg, sizeof(msg), "level %d solvable", lv);
        CHECK(r.found == 1, msg);

        if (r.found) {
            snprintf(msg, sizeof(msg), "level %d path length >= 2", lv);
            CHECK(r.length >= 2, msg);
        }

        if (lv < max) engine_next_level(&s);
    }

    free_state(&s);
}

static void test_no_path(void) {
    printf("\n[pathfinder] Unsolvable map\n");
    GameState s = make_state();

    /* Block the exit by surrounding it with walls */
    s.map[6 * MAP_WIDTH + 5] = 1; /* tile left of exit */
    s.map[5 * MAP_WIDTH + 6] = 1; /* tile above exit */
    /* exit is at (6,6), below is border, right is border — now isolated */

    PFResult r = pf_solve_from_player(&s);
    CHECK(r.found == 0, "isolated exit returns found=0");
    CHECK(r.length == 0, "path length 0 when no path");

    free_state(&s);
}

static void test_next_step(void) {
    printf("\n[pathfinder] Next step hint\n");
    GameState s = make_state();

    PFResult r = pf_solve_from_player(&s);
    CHECK(r.found == 1, "prerequisite: level 1 solvable");

    PFTile next = pf_next_step(&r);
    CHECK(next.col >= 0 && next.col < MAP_WIDTH,  "next step col in bounds");
    CHECK(next.row >= 0 && next.row < MAP_HEIGHT, "next step row in bounds");

    /* Next step is adjacent to spawn */
    PFTile spawn = r.path[0];
    int dc = abs(next.col - spawn.col);
    int dr = abs(next.row - spawn.row);
    CHECK((dc == 1 && dr == 0) || (dc == 0 && dr == 1),
          "next step is adjacent to spawn");

    /* No path — should return (-1,-1) */
    PFResult bad; bad.found = 0; bad.length = 0;
    PFTile bad_next = pf_next_step(&bad);
    CHECK(bad_next.col == -1, "no path: next_step col == -1");
    CHECK(bad_next.row == -1, "no path: next_step row == -1");

    /* NULL */
    PFTile null_next = pf_next_step(NULL);
    CHECK(null_next.col == -1, "NULL result: next_step col == -1");

    free_state(&s);
}

static void test_hint_angle(void) {
    printf("\n[pathfinder] Hint angle\n");
    GameState s = make_state();

    PFResult r = pf_solve_from_player(&s);
    CHECK(r.found == 1, "prerequisite: solvable");

    float angle = pf_hint_angle(&r, s.player.px, s.player.py);
    CHECK(angle >= 0.0f && angle < 360.0f, "hint angle in [0,360)");

    /* No path → angle 0 */
    PFResult bad; bad.found = 0; bad.length = 0;
    float bad_angle = pf_hint_angle(&bad, 0, 0);
    CHECK(bad_angle == 0.0f, "no path hint angle == 0");

    free_state(&s);
}

static void test_count_reachable(void) {
    printf("\n[pathfinder] Count reachable\n");
    GameState s = make_state();

    /* From an open tile — should reach multiple tiles */
    int count = pf_count_reachable(&s, 1, 1);
    CHECK(count > 1, "reachable count > 1 from open tile");

    /* From a wall tile — should return 0 */
    int wall_count = pf_count_reachable(&s, 0, 0);
    CHECK(wall_count == 0, "reachable count 0 from wall tile");

    /* Count should include exit tile */
    int exits = 0;
    PFResult r = pf_solve_from_player(&s);
    if (r.found)
        exits = pf_count_reachable(&s,
            r.path[r.length-1].col, r.path[r.length-1].row);
    CHECK(exits > 0, "exit tile has reachable count > 0");

    /* NULL state */
    CHECK(pf_count_reachable(NULL, 1, 1) == 0, "NULL state returns 0");

    free_state(&s);
}

static void test_print_no_crash(void) {
    printf("\n[pathfinder] pf_print does not crash\n");
    GameState s = make_state();
    PFResult r = pf_solve_from_player(&s);

    pf_print(&s, &r);          /* normal path */
    pf_print(&s, NULL);        /* no result */
    pf_print(NULL, &r);        /* NULL state */
    pf_print(NULL, NULL);      /* both NULL */
    CHECK(1, "pf_print does not crash in any case");

    free_state(&s);
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */

int main(void) {
    printf("=== pathfinder tests ===\n");
    test_world_tile_conversion();
    test_passable();
    test_simple_solve();
    test_all_levels_solvable();
    test_no_path();
    test_next_step();
    test_hint_angle();
    test_count_reachable();
    test_print_no_crash();
    printf("\n%d / %d passed\n", passed, runs);
    return passed == runs ? 0 : 1;
}
