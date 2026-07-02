/*
 * minimap.c
 *
 * Standalone minimap renderer for the maze game.
 *
 * This module does:
 * - Draw the 8x8 engine map to a caller-owned framebuffer.
 * - Draw wall, empty, exit, player, heading, FOV, and path overlay pixels.
 * - Convert screen clicks in the minimap rectangle back to map cells.
 * - Accept NULL optional path data without crashing.
 *
 * This module does NOT:
 * - Modify GameState or own map data.
 * - Allocate memory.
 * - Call SDL or any platform graphics API.
 * - Perform BFS itself.
 */

#include "minimap.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>

#define MINIMAP_PI 3.14159265358979323846f
#define MINIMAP_COLOR_BG 0x00101010u
#define MINIMAP_COLOR_WALL 0x00a0a0a0u
#define MINIMAP_COLOR_EMPTY 0x00202020u
#define MINIMAP_COLOR_EXIT 0x0000cc44u
#define MINIMAP_COLOR_GRID 0x00404040u
#define MINIMAP_COLOR_PLAYER 0x00ffdd00u
#define MINIMAP_COLOR_DIR 0x00ffffffu
#define MINIMAP_COLOR_FOV 0x00ffaa00u
#define MINIMAP_COLOR_PATH 0x0000ffffu

static int minimap_clamp_tile_size(int tile_size) {
    if (tile_size <= 0) {
        return 8;
    }
    if (tile_size > 64) {
        return 64;
    }
    return tile_size;
}

static int minimap_clamp_pad(int pad) {
    if (pad < 0) {
        return 0;
    }
    if (pad > 256) {
        return 256;
    }
    return pad;
}

static float minimap_deg_to_rad(float angle) {
    return angle * MINIMAP_PI / 180.0f;
}

static void minimap_put_pixel(uint32_t *pixels, int screen_w, int screen_h, int x, int y, uint32_t color) {
    if (pixels == NULL || screen_w <= 0 || screen_h <= 0) {
        return;
    }
    if (x < 0 || y < 0 || x >= screen_w || y >= screen_h) {
        return;
    }
    pixels[y * screen_w + x] = color;
}

static void minimap_fill_rect(uint32_t *pixels, int screen_w, int screen_h,
                              int x, int y, int w, int h, uint32_t color) {
    int yy;
    int xx;

    if (pixels == NULL || w <= 0 || h <= 0) {
        return;
    }

    for (yy = y; yy < y + h; ++yy) {
        for (xx = x; xx < x + w; ++xx) {
            minimap_put_pixel(pixels, screen_w, screen_h, xx, yy, color);
        }
    }
}

static void minimap_draw_line(uint32_t *pixels, int screen_w, int screen_h,
                              int x0, int y0, int x1, int y1, uint32_t color) {
    int dx;
    int sx;
    int dy;
    int sy;
    int err;

    if (pixels == NULL) {
        return;
    }

    dx = abs(x1 - x0);
    sx = x0 < x1 ? 1 : -1;
    dy = -abs(y1 - y0);
    sy = y0 < y1 ? 1 : -1;
    err = dx + dy;

    for (;;) {
        int e2;
        minimap_put_pixel(pixels, screen_w, screen_h, x0, y0, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static void minimap_draw_disc(uint32_t *pixels, int screen_w, int screen_h,
                              int cx, int cy, int radius, uint32_t color) {
    int y;
    int x;
    int rr;

    if (radius < 1) {
        radius = 1;
    }
    rr = radius * radius;
    for (y = -radius; y <= radius; ++y) {
        for (x = -radius; x <= radius; ++x) {
            if (x * x + y * y <= rr) {
                minimap_put_pixel(pixels, screen_w, screen_h, cx + x, cy + y, color);
            }
        }
    }
}

static uint32_t minimap_cell_color(int cell) {
    if (cell == 1) {
        return MINIMAP_COLOR_WALL;
    }
    if (cell == 2) {
        return MINIMAP_COLOR_EXIT;
    }
    return MINIMAP_COLOR_EMPTY;
}

MinimapConfig minimap_default_config(void) {
    MinimapConfig config;
    config.tile_size = 8;
    config.pad = 8;
    config.show_fov_cone = 1;
    config.show_path = 1;
    return config;
}

void minimap_render(const MinimapConfig *config,
                    const GameState *state,
                    const MinimapPathResult *pf_result,
                    uint32_t *pixels,
                    int screen_w,
                    int screen_h) {
    MinimapConfig local;
    int tile;
    int pad;
    int row;
    int col;
    int map_px_w;
    int map_px_h;
    int player_x;
    int player_y;
    int player_radius;

    if (state == NULL || pixels == NULL || screen_w <= 0 || screen_h <= 0) {
        return;
    }

    local = config != NULL ? *config : minimap_default_config();
    tile = minimap_clamp_tile_size(local.tile_size);
    pad = minimap_clamp_pad(local.pad);
    map_px_w = MAP_WIDTH * tile;
    map_px_h = MAP_HEIGHT * tile;

    minimap_fill_rect(pixels, screen_w, screen_h, pad - 1, pad - 1,
                      map_px_w + 2, map_px_h + 2, MINIMAP_COLOR_BG);

    for (row = 0; row < MAP_HEIGHT; ++row) {
        for (col = 0; col < MAP_WIDTH; ++col) {
            int cell = state->map[row * MAP_WIDTH + col];
            int x = pad + col * tile;
            int y = pad + row * tile;
            minimap_fill_rect(pixels, screen_w, screen_h, x, y, tile, tile, minimap_cell_color(cell));
            minimap_draw_line(pixels, screen_w, screen_h, x, y, x + tile - 1, y, MINIMAP_COLOR_GRID);
            minimap_draw_line(pixels, screen_w, screen_h, x, y, x, y + tile - 1, MINIMAP_COLOR_GRID);
        }
    }

    if (local.show_path && pf_result != NULL && pf_result->found) {
        int i;
        int count = pf_result->count;
        if (count > MINIMAP_PATH_MAX) {
            count = MINIMAP_PATH_MAX;
        }
        for (i = 0; i < count; ++i) {
            int path_col = pf_result->cols[i];
            int path_row = pf_result->rows[i];
            if (path_col >= 0 && path_col < MAP_WIDTH && path_row >= 0 && path_row < MAP_HEIGHT) {
                int cx = pad + path_col * tile + tile / 2;
                int cy = pad + path_row * tile + tile / 2;
                minimap_draw_disc(pixels, screen_w, screen_h, cx, cy, tile / 5 + 1, MINIMAP_COLOR_PATH);
            }
        }
    }

    player_x = pad + (int)((state->player.px / (float)MAP_SCALE) * (float)tile);
    player_y = pad + (int)((state->player.py / (float)MAP_SCALE) * (float)tile);
    player_radius = tile / 3;
    if (player_radius < 2) {
        player_radius = 2;
    }

    minimap_draw_disc(pixels, screen_w, screen_h, player_x, player_y, player_radius, MINIMAP_COLOR_PLAYER);
    {
        int line_len = tile * 2;
        int dir_x = player_x + (int)(cosf(minimap_deg_to_rad(state->player.pa)) * (float)line_len);
        int dir_y = player_y - (int)(sinf(minimap_deg_to_rad(state->player.pa)) * (float)line_len);
        minimap_draw_line(pixels, screen_w, screen_h, player_x, player_y, dir_x, dir_y, MINIMAP_COLOR_DIR);
    }

    if (local.show_fov_cone) {
        int cone_len = tile * 4;
        float left = state->player.pa + 30.0f;
        float right = state->player.pa - 30.0f;
        int lx = player_x + (int)(cosf(minimap_deg_to_rad(left)) * (float)cone_len);
        int ly = player_y - (int)(sinf(minimap_deg_to_rad(left)) * (float)cone_len);
        int rx = player_x + (int)(cosf(minimap_deg_to_rad(right)) * (float)cone_len);
        int ry = player_y - (int)(sinf(minimap_deg_to_rad(right)) * (float)cone_len);
        minimap_draw_line(pixels, screen_w, screen_h, player_x, player_y, lx, ly, MINIMAP_COLOR_FOV);
        minimap_draw_line(pixels, screen_w, screen_h, player_x, player_y, rx, ry, MINIMAP_COLOR_FOV);
    }
}

int minimap_tile_at_screen(const MinimapConfig *config,
                           int screen_x,
                           int screen_y,
                           int *map_col,
                           int *map_row) {
    MinimapConfig local;
    int tile;
    int pad;
    int col;
    int row;

    if (map_col != NULL) {
        *map_col = -1;
    }
    if (map_row != NULL) {
        *map_row = -1;
    }
    if (map_col == NULL || map_row == NULL) {
        return 0;
    }

    local = config != NULL ? *config : minimap_default_config();
    tile = minimap_clamp_tile_size(local.tile_size);
    pad = minimap_clamp_pad(local.pad);

    if (screen_x < pad || screen_y < pad) {
        return 0;
    }

    col = (screen_x - pad) / tile;
    row = (screen_y - pad) / tile;
    if (col < 0 || row < 0 || col >= MAP_WIDTH || row >= MAP_HEIGHT) {
        return 0;
    }

    *map_col = col;
    *map_row = row;
    return 1;
}

/*
 * Minimap maintenance notes:
 * 001. The minimap intentionally renders into the caller framebuffer.
 * 002. The renderer does not clear the whole screen.
 * 003. Tile size is clamped so bad configs remain safe.
 * 004. Pad is clamped so negative origins do not underflow.
 * 005. Player projection uses MAP_SCALE from engine.h.
 * 006. A map cell value of 1 is treated as wall.
 * 007. A map cell value of 2 is treated as exit.
 * 008. Any other map cell value is treated as passable floor.
 * 009. Path overlay is optional and ignored when pf_result is NULL.
 * 010. Path overlay ignores out-of-bounds cells.
 */
