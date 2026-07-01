/*
 * src/pathfinder.c — BFS maze solver.
 *
 * Pure C, no dynamic allocation (uses fixed-size arrays on the stack).
 * Safe to call from tests without SDL2 or a framebuffer.
 *
 * Algorithm: standard BFS with a parent-pointer array for path reconstruction.
 *
 *   queue[]    — circular queue of PFNode, capacity PF_MAX_NODES
 *   parent[]   — for each tile index, the index of the tile we came from
 *   visited[][] — marks tiles already enqueued
 *
 * After BFS terminates, if the exit tile was reached, the path is
 * reconstructed by following parent[] pointers backward from the exit,
 * then reversing the result into path[].
 *
 * Neighbours: 4-connected (N, S, E, W). No diagonal movement.
 */

#include "pathfinder.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define PI 3.14159265359f

/* 4-connected neighbour offsets: (dcol, drow) */
static const int DIRS[4][2] = { {0,-1}, {0,1}, {-1,0}, {1,0} };

/* Flat tile index */
static inline int tile_idx(int col, int row) {
    return row * MAP_WIDTH + col;
}

/* ------------------------------------------------------------------ */
/* Core utilities                                                      */
/* ------------------------------------------------------------------ */

int pf_tile_is_passable(const GameState *state, int col, int row) {
    if (!state) return 0;
    if (col < 0 || col >= MAP_WIDTH || row < 0 || row >= MAP_HEIGHT) return 0;
    int cell = state->map[tile_idx(col, row)];
    return cell == 0 || cell == 2; /* CELL_FLOOR or CELL_EXIT */
}

PFTile pf_world_to_tile(float wx, float wy) {
    PFTile t;
    t.col = (int)wx >> 6; /* divide by 64 */
    t.row = (int)wy >> 6;
    return t;
}

void pf_tile_to_world_centre(PFTile tile, float *wx, float *wy) {
    if (wx) *wx = (float)(tile.col * 64) + 32.0f;
    if (wy) *wy = (float)(tile.row * 64) + 32.0f;
}

/* ------------------------------------------------------------------ */
/* BFS                                                                 */
/* ------------------------------------------------------------------ */

PFResult pf_solve(const GameState *state, int start_col, int start_row) {
    PFResult result;
    memset(&result, 0, sizeof(result));
    result.found  = 0;
    result.length = 0;

    /* Initialise distance grid to -1 (unvisited) */
    for (int r = 0; r < MAP_HEIGHT; r++)
        for (int c = 0; c < MAP_WIDTH; c++)
            result.dist[r][c] = -1;

    if (!state) return result;
    if (!pf_tile_is_passable(state, start_col, start_row)) return result;

    /* BFS queue — static array, capacity PF_MAX_NODES */
    PFNode queue[PF_MAX_NODES];
    int    parent[PF_MAX_NODES]; /* parent flat index, -1 = root */
    int    qhead = 0, qtail = 0;

    memset(parent, -1, sizeof(parent));

    /* Enqueue start */
    int start_flat = tile_idx(start_col, start_row);
    queue[qtail].col    = start_col;
    queue[qtail].row    = start_row;
    queue[qtail].parent = -1;
    qtail++;
    result.visited[start_row][start_col] = 1;
    result.dist[start_row][start_col]    = 0;
    parent[start_flat] = start_flat; /* root points to itself */

    int goal_flat = -1;
    int goal_col  = -1, goal_row = -1;

    while (qhead < qtail) {
        PFNode cur = queue[qhead++];
        int cur_flat = tile_idx(cur.col, cur.row);
        int cur_dist = result.dist[cur.row][cur.col];

        /* Check if this is the exit */
        if (state->map[cur_flat] == 2) {
            goal_flat = cur_flat;
            goal_col  = cur.col;
            goal_row  = cur.row;
            result.found = 1;
            break;
        }

        /* Expand 4 neighbours */
        for (int d = 0; d < 4; d++) {
            int nc = cur.col + DIRS[d][0];
            int nr = cur.row + DIRS[d][1];
            if (!pf_tile_is_passable(state, nc, nr)) continue;
            if (result.visited[nr][nc]) continue;

            result.visited[nr][nc] = 1;
            result.dist[nr][nc]    = cur_dist + 1;
            parent[tile_idx(nc, nr)] = cur_flat;

            if (qtail < PF_MAX_NODES) {
                queue[qtail].col    = nc;
                queue[qtail].row    = nr;
                queue[qtail].parent = cur_flat;
                qtail++;
            }
        }
    }

    if (!result.found) return result;

    /* Reconstruct path by following parent[] back from goal to start */
    int path_flat[PF_MAX_NODES];
    int path_len = 0;
    int cur = goal_flat;
    while (cur != start_flat && path_len < PF_MAX_NODES) {
        path_flat[path_len++] = cur;
        int p = parent[cur];
        if (p == cur) break; /* root */
        cur = p;
    }
    path_flat[path_len++] = start_flat;

    /* Reverse into result.path[] */
    result.length = path_len;
    for (int i = 0; i < path_len; i++) {
        int flat = path_flat[path_len - 1 - i];
        result.path[i].col = flat % MAP_WIDTH;
        result.path[i].row = flat / MAP_WIDTH;
    }

    return result;
}

PFResult pf_solve_from_player(const GameState *state) {
    if (!state) {
        PFResult r; memset(&r, 0, sizeof(r)); return r;
    }
    PFTile t = pf_world_to_tile(state->player.px, state->player.py);
    return pf_solve(state, t.col, t.row);
}

/* ------------------------------------------------------------------ */
/* Path utilities                                                      */
/* ------------------------------------------------------------------ */

PFTile pf_next_step(const PFResult *result) {
    PFTile bad = { -1, -1 };
    if (!result || !result->found || result->length < 2) return bad;
    return result->path[1]; /* path[0] is current tile */
}

float pf_hint_angle(const PFResult *result, float px, float py) {
    PFTile next = pf_next_step(result);
    if (next.col < 0) return 0.0f;
    float tx, ty;
    pf_tile_to_world_centre(next, &tx, &ty);
    float dx = tx - px;
    float dy = ty - py;
    float angle = atan2f(-dy, dx) * 180.0f / PI;
    if (angle < 0) angle += 360.0f;
    return angle;
}

/* ------------------------------------------------------------------ */
/* Debug print                                                         */
/* ------------------------------------------------------------------ */

void pf_print(const GameState *state, const PFResult *result) {
    if (!state) return;

    /* Build a set of path tiles for O(1) lookup */
    int in_path[MAP_HEIGHT][MAP_WIDTH];
    memset(in_path, 0, sizeof(in_path));
    if (result && result->found) {
        for (int i = 0; i < result->length; i++)
            in_path[result->path[i].row][result->path[i].col] = 1;
    }

    printf("Pathfinder map (%dx%d):\n", MAP_WIDTH, MAP_HEIGHT);
    for (int r = 0; r < MAP_HEIGHT; r++) {
        for (int c = 0; c < MAP_WIDTH; c++) {
            int cell = state->map[r * MAP_WIDTH + c];
            char ch;
            if (cell == 1) {
                ch = '#';
            } else if (cell == 2) {
                ch = 'E';
            } else if (result && in_path[r][c]) {
                if (r == result->path[0].row && c == result->path[0].col)
                    ch = 'S';
                else
                    ch = '*';
            } else {
                ch = '.';
            }
            putchar(ch);
        }
        putchar('\n');
    }

    if (result) {
        if (result->found)
            printf("Path length: %d tiles\n", result->length);
        else
            printf("No path found.\n");
    }
}

/* ------------------------------------------------------------------ */
/* Reachability                                                        */
/* ------------------------------------------------------------------ */

int pf_count_reachable(const GameState *state, int start_col, int start_row) {
    if (!state) return 0;
    if (!pf_tile_is_passable(state, start_col, start_row)) return 0;

    int visited[MAP_HEIGHT][MAP_WIDTH];
    memset(visited, 0, sizeof(visited));

    PFTile queue[PF_MAX_NODES];
    int qhead = 0, qtail = 0;

    visited[start_row][start_col] = 1;
    queue[qtail++] = (PFTile){ start_col, start_row };
    int count = 1;

    while (qhead < qtail) {
        PFTile cur = queue[qhead++];
        for (int d = 0; d < 4; d++) {
            int nc = cur.col + DIRS[d][0];
            int nr = cur.row + DIRS[d][1];
            if (!pf_tile_is_passable(state, nc, nr)) continue;
            if (visited[nr][nc]) continue;
            visited[nr][nc] = 1;
            count++;
            if (qtail < PF_MAX_NODES)
                queue[qtail++] = (PFTile){ nc, nr };
        }
    }

    return count;
}
