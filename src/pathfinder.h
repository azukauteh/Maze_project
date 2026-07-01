#ifndef PATHFINDER_H
#define PATHFINDER_H

#include "engine.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Pathfinder — BFS maze solver operating on the engine's tile grid.
 *
 * Used for:
 *   - Automated test: verify that every level has a solvable path from
 *     spawn tile to exit tile. A level that fails this check is broken.
 *   - Runtime hint system: compute the next tile toward the exit and
 *     expose it to the HUD as a directional arrow.
 *   - Regression: if a level edit breaks solvability, the test catches it
 *     before it ships.
 *
 * The BFS treats CELL_FLOOR (0) and CELL_EXIT (2) as passable.
 * CELL_WALL (1) and out-of-bounds tiles are impassable.
 *
 * All coordinates are tile coordinates (not world pixels).
 * Tile (col, row) maps to world pixel centre (col*64+32, row*64+32).
 */

#define PF_MAX_NODES   (MAP_WIDTH * MAP_HEIGHT)
#define PF_NO_PATH     -1

/* A single tile coordinate */
typedef struct { int col; int row; } PFTile;

/* The full BFS result */
typedef struct {
    int     found;                  /* 1 if a path exists, 0 otherwise     */
    int     length;                 /* number of tiles in path (incl. both ends) */
    PFTile  path[PF_MAX_NODES];    /* path[0] = start, path[length-1] = goal   */
    int     visited[MAP_HEIGHT][MAP_WIDTH]; /* 1 if tile was reached by BFS */
    int     dist[MAP_HEIGHT][MAP_WIDTH];    /* BFS distance from start       */
} PFResult;

/*
 * BFS node used internally — exposed so tests can inspect queue state.
 */
typedef struct {
    int col, row;
    int parent; /* index into queue array, -1 for root */
} PFNode;

/* ---- Core BFS ---- */

/*
 * pf_solve — run BFS from (start_col, start_row) to the exit tile (CELL_EXIT).
 * Returns a PFResult with found=1 and the reconstructed path on success.
 * Returns found=0 if no path exists (level is unsolvable).
 */
PFResult pf_solve(const GameState *state, int start_col, int start_row);

/*
 * pf_solve_from_player — convenience wrapper that converts player world
 * position to tile coordinates before calling pf_solve.
 */
PFResult pf_solve_from_player(const GameState *state);

/* ---- Utilities ---- */

/*
 * pf_tile_is_passable — returns 1 if the tile at (col, row) is floor or exit.
 */
int pf_tile_is_passable(const GameState *state, int col, int row);

/*
 * pf_world_to_tile — convert world-space pixel coords to tile coords.
 */
PFTile pf_world_to_tile(float wx, float wy);

/*
 * pf_tile_to_world_centre — convert tile coords to world-space pixel centre.
 */
void pf_tile_to_world_centre(PFTile tile, float *wx, float *wy);

/*
 * pf_next_step — given a solved PFResult, return the tile one step from
 * start toward the goal. Returns PFTile{-1,-1} if result is not solved.
 */
PFTile pf_next_step(const PFResult *result);

/*
 * pf_hint_angle — given a solved PFResult and player world position,
 * return the angle in degrees toward the next step tile. Used by HUD
 * to draw a directional hint arrow.
 */
float pf_hint_angle(const PFResult *result, float px, float py);

/*
 * pf_print — debug-print the map with the BFS path overlaid.
 * Uses '#' for walls, '.' for floor, '*' for path, 'S' for start, 'E' for exit.
 */
void pf_print(const GameState *state, const PFResult *result);

/*
 * pf_count_reachable — count tiles reachable from (start_col, start_row).
 * Used to detect isolated pockets that aren't connected to the exit.
 */
int pf_count_reachable(const GameState *state, int start_col, int start_row);

#ifdef __cplusplus
}
#endif

#endif /* PATHFINDER_H */
