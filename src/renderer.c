/*
 * src/renderer.c — extended render pipeline.
 *
 * This module wraps the DDA raycasting core from engine.c with additional
 * passes: textured walls, floor casting, sprites, HUD.
 *
 * Wall texture mapping:
 *   When a ray hits a wall at world coordinate (rx, ry), the texture U
 *   coordinate is the fractional offset within the tile:
 *     hit on vertical grid line:   u = frac(ry / 64)
 *     hit on horizontal grid line: u = frac(rx / 64)
 *   This is multiplied by tex->width to get the texel column.
 *
 * Floor/ceiling casting:
 *   For each pixel row below the wall slice (floor) or above it (ceiling),
 *   the real-world tile coordinate is computed by inverse-projecting the
 *   screen pixel through the camera plane. This is O(W*H) per frame —
 *   slower than the wall pass but produces correct perspective-correct UV.
 *   Can be toggled off (renderer_set_textured_floor(0)) for performance.
 *
 * No intentional vulnerabilities in this module.
 */

#include "renderer.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>

#define PI            3.14159265359f
#define DEG_TO_RAD(a) ((a) * PI / 180.0f)
#define MAP_SCALE     64

/* ------------------------------------------------------------------ */
/* Helpers (duplicated locally to avoid engine.c coupling)            */
/* ------------------------------------------------------------------ */

static int fix_angle(int a) {
    while (a > 359) a -= 360;
    while (a < 0)   a += 360;
    return a;
}

static int map_cell_ren(const GameState *state, int mx, int my) {
    if (mx < 0 || mx >= MAP_WIDTH || my < 0 || my >= MAP_HEIGHT) return 1;
    return state->map[my * MAP_WIDTH + mx];
}

static inline uint32_t rgb_ren(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

static uint32_t apply_fog_ren(uint32_t color, float dist, float fog_max) {
    float t = dist / fog_max;
    if (t > 1.0f) t = 1.0f;
    float k = 1.0f - t;
    uint8_t r = (uint8_t)(((color >> 16) & 0xFF) * k);
    uint8_t g = (uint8_t)(((color >>  8) & 0xFF) * k);
    uint8_t b = (uint8_t)(((color      ) & 0xFF) * k);
    return rgb_ren(r, g, b);
}

static void draw_vline_ren(uint32_t *pixels, int width, int height,
                           int x, int y1, int y2, uint32_t color) {
    if (x < 0 || x >= width) return;
    if (y1 > y2) { int t = y1; y1 = y2; y2 = t; }
    y1 = y1 < 0 ? 0 : y1;
    y2 = y2 >= height ? height - 1 : y2;
    for (int y = y1; y <= y2; y++)
        pixels[y * width + x] = color;
}

/* ------------------------------------------------------------------ */
/* Lifecycle                                                           */
/* ------------------------------------------------------------------ */

int renderer_init(Renderer *ren, int screen_w, TextureRegistry *tex,
                  SpriteList *sprites, HUDState *hud) {
    if (!ren || screen_w <= 0) return 0;
    memset(ren, 0, sizeof(*ren));
    ren->textures        = tex;
    ren->sprites         = sprites;
    ren->hud             = hud;
    ren->textured_walls  = 1;
    ren->textured_floor  = 0;   /* off by default — expensive */
    ren->show_sprites    = 1;
    ren->show_hud        = 1;
    return zbuffer_alloc(&ren->zbuf, screen_w);
}

void renderer_shutdown(Renderer *ren) {
    if (!ren) return;
    zbuffer_free(&ren->zbuf);
    memset(ren, 0, sizeof(*ren));
}

/* ------------------------------------------------------------------ */
/* Config                                                              */
/* ------------------------------------------------------------------ */

void renderer_set_textured_walls(Renderer *ren, int on) {
    if (ren) ren->textured_walls = on;
}
void renderer_set_textured_floor(Renderer *ren, int on) {
    if (ren) ren->textured_floor = on;
}
void renderer_set_show_sprites(Renderer *ren, int on) {
    if (ren) ren->show_sprites = on;
}
void renderer_set_show_hud(Renderer *ren, int on) {
    if (ren) ren->show_hud = on;
}

/* ------------------------------------------------------------------ */
/* Texture selection                                                   */
/* ------------------------------------------------------------------ */

/*
 * Per-level wall texture name table.
 * Must stay in sync with texture_registry_generate_defaults().
 */
static const char *wall_tex_name(int level) {
    switch (level) {
        case 2:  return "wall_green";
        case 3:  return "wall_red";
        case 4:  return "wall_purple";
        case 5:  return "wall_orange";
        case 6:  return "wall_cyan";
        default: return "wall_grey";
    }
}

const Texture *renderer_wall_texture(const Renderer *ren, int level,
                                     int cell_type) {
    if (!ren || !ren->textures) return NULL;
    if (cell_type == 2) /* CELL_EXIT */
        return texture_registry_get(ren->textures, "exit");
    return texture_registry_get(ren->textures, wall_tex_name(level));
}

/* ------------------------------------------------------------------ */
/* Sky / floor gradient pass (same as engine.c but callable separately)*/
/* ------------------------------------------------------------------ */

typedef struct { uint8_t sr,sg,sb,fr,fg,fb; } RenPalette;

static RenPalette ren_palette(int level) {
    switch (level) {
        case 2: return (RenPalette){ 20,40,20, 30,25,20 };
        case 3: return (RenPalette){ 40,10,10, 25,15,15 };
        case 4: return (RenPalette){ 30,10,50, 20,10,35 };
        case 5: return (RenPalette){ 50,30,10, 35,20,10 };
        case 6: return (RenPalette){ 10,40,45, 10,25,30 };
        default:return (RenPalette){ 20,20,60, 25,25,25 };
    }
}

void renderer_pass_sky_floor(Renderer *ren, GameState *state) {
    (void)ren;
    int width  = state->width;
    int height = state->height;
    int half   = height / 2;
    RenPalette p = ren_palette(state->current_level);

    for (int y = 0; y < half; y++) {
        float t = (float)y / (float)half;
        uint32_t c = rgb_ren(
            (uint8_t)(p.sr + 30 * t),
            (uint8_t)(p.sg + 30 * t),
            (uint8_t)(p.sb + 40 * t));
        for (int x = 0; x < width; x++)
            state->pixels[y * width + x] = c;
    }
    for (int y = half; y < height; y++) {
        float t = (float)(y - half) / (float)(height - half);
        uint32_t c = rgb_ren(
            (uint8_t)(p.fr + 25 * t),
            (uint8_t)(p.fg + 20 * t),
            (uint8_t)(p.fb + 15 * t));
        for (int x = 0; x < width; x++)
            state->pixels[y * width + x] = c;
    }
}

/* ------------------------------------------------------------------ */
/* Wall pass — DDA + texture sampling                                  */
/* ------------------------------------------------------------------ */

/*
 * Per-ray DDA result — used by both the wall pass and floor cast.
 */
typedef struct {
    float    dist;      /* corrected perpendicular distance */
    float    hit_x;     /* world x of intersection          */
    float    hit_y;     /* world y of intersection          */
    int      cell;      /* CELL_WALL or CELL_EXIT            */
    int      vertical;  /* 1 if hit a vertical grid line    */
    int      line_h;    /* screen height of wall slice      */
    int      line_off;  /* screen y of top of slice         */
} RayHit;

static RayHit cast_ray(const GameState *state, int ra) {
    RayHit hit;
    memset(&hit, 0, sizeof(hit));
    hit.dist = 1e6f;

    float disV = 1e6f, disH = 1e6f;
    float vx = 0, vy = 0, hx = 0, hy = 0;
    int   hitV = 0, hitH = 0;
    float Tan  = tanf(DEG_TO_RAD(ra));

    /* Vertical intersections */
    if (fabsf(cosf(DEG_TO_RAD(ra))) > 0.001f) {
        float rx, ry, xo, yo;
        if (cosf(DEG_TO_RAD(ra)) > 0.0f) {
            rx = (float)((((int)state->player.px >> 6) << 6) + 64);
            xo = 64.0f;
        } else {
            rx = (float)((((int)state->player.px >> 6) << 6)) - 0.0001f;
            xo = -64.0f;
        }
        ry = (state->player.px - rx) * Tan + state->player.py;
        yo = -xo * Tan;

        for (int dof = 0; dof < 8; dof++) {
            int c = map_cell_ren(state, (int)rx >> 6, (int)ry >> 6);
            if (c == 1 || c == 2) {
                float d = cosf(DEG_TO_RAD(ra)) * (rx - state->player.px)
                        - sinf(DEG_TO_RAD(ra)) * (ry - state->player.py);
                disV = d; vx = rx; vy = ry; hitV = c; break;
            }
            rx += xo; ry += yo;
        }
    }

    /* Horizontal intersections */
    if (fabsf(sinf(DEG_TO_RAD(ra))) > 0.001f) {
        float rx, ry, xo, yo;
        float T2 = 1.0f / Tan;
        if (sinf(DEG_TO_RAD(ra)) > 0.0f) {
            ry = (float)((((int)state->player.py >> 6) << 6)) - 0.0001f;
            yo = -64.0f;
        } else {
            ry = (float)((((int)state->player.py >> 6) << 6) + 64);
            yo = 64.0f;
        }
        rx = (state->player.py - ry) * T2 + state->player.px;
        xo = -yo * T2;

        for (int dof = 0; dof < 8; dof++) {
            int c = map_cell_ren(state, (int)rx >> 6, (int)ry >> 6);
            if (c == 1 || c == 2) {
                float d = cosf(DEG_TO_RAD(ra)) * (rx - state->player.px)
                        - sinf(DEG_TO_RAD(ra)) * (ry - state->player.py);
                disH = d; hx = rx; hy = ry; hitH = c; break;
            }
            rx += xo; ry += yo;
        }
    }

    /* Fisheye fix */
    int ca = fix_angle(state->player.pa - ra);
    float cos_ca = cosf(DEG_TO_RAD(ca));

    if (disV < disH) {
        hit.dist     = disV * cos_ca;
        hit.hit_x    = vx;
        hit.hit_y    = vy;
        hit.cell     = hitV;
        hit.vertical = 1;
    } else {
        hit.dist     = disH * cos_ca;
        hit.hit_x    = hx;
        hit.hit_y    = hy;
        hit.cell     = hitH;
        hit.vertical = 0;
    }

    if (hit.dist < 1.0f) hit.dist = 1.0f;

    hit.line_h   = (MAP_SCALE * state->height) / (int)hit.dist;
    if (hit.line_h > state->height) hit.line_h = state->height;
    hit.line_off = (state->height - hit.line_h) / 2;

    return hit;
}

void renderer_pass_walls(Renderer *ren, GameState *state) {
    int width     = state->width;
    int height    = state->height;
    int ray_count = 60;
    int fov_half  = 30;
    int ra        = fix_angle(state->player.pa + fov_half);

    zbuffer_clear(&ren->zbuf);

    for (int r = 0; r < ray_count; r++) {
        RayHit hit = cast_ray(state, ra);

        /* Write Z-buffer */
        int sx = (r * width) / ray_count;
        int ex = ((r + 1) * width) / ray_count;
        for (int x = sx; x < ex && x < width; x++)
            if (x >= 0 && x < ren->zbuf.width)
                ren->zbuf.zbuf[x] = hit.dist;

        /* Texture lookup */
        uint32_t flat_color;
        if (hit.cell == 2)
            flat_color = hit.vertical ? rgb_ren(0,210,90) : rgb_ren(0,150,65);
        else
            flat_color = hit.vertical ? rgb_ren(120,120,120) : rgb_ren(80,80,80);

        flat_color = apply_fog_ren(flat_color, hit.dist, 400.0f);

        const Texture *tex = NULL;
        if (ren->textured_walls && ren->textures)
            tex = renderer_wall_texture(ren, state->current_level, hit.cell);

        /* Compute texture U from hit offset within tile */
        float hit_frac = hit.vertical
            ? fmodf(hit.hit_y, 64.0f) / 64.0f
            : fmodf(hit.hit_x, 64.0f) / 64.0f;

        for (int x = sx; x < ex && x < width; x++) {
            for (int y = hit.line_off; y < hit.line_off + hit.line_h; y++) {
                if (y < 0 || y >= height) continue;
                uint32_t color;
                if (tex) {
                    float v = (float)(y - hit.line_off) / (float)hit.line_h;
                    color = texture_sample(tex, hit_frac, v);
                    color = apply_fog_ren(color, hit.dist, 400.0f);
                } else {
                    color = flat_color;
                }
                state->pixels[y * width + x] = color;
            }
        }

        ra = fix_angle(ra - 1);
    }
}

/* ------------------------------------------------------------------ */
/* Floor casting (perspective-correct per-pixel UV)                   */
/* ------------------------------------------------------------------ */

void renderer_pass_floor_cast(Renderer *ren, GameState *state) {
    if (!ren || !ren->textured_floor) return;

    int width  = state->width;
    int height = state->height;
    int half   = height / 2;

    const Texture *floor_tex = NULL;
    if (ren->textures)
        floor_tex = texture_registry_get(ren->textures, "floor");

    float pa_rad = DEG_TO_RAD(state->player.pa);
    float left_ray_x  = cosf(pa_rad + DEG_TO_RAD(30.0f));
    float left_ray_y  = -sinf(pa_rad + DEG_TO_RAD(30.0f));
    float right_ray_x = cosf(pa_rad - DEG_TO_RAD(30.0f));
    float right_ray_y = -sinf(pa_rad - DEG_TO_RAD(30.0f));

    for (int y = half + 1; y < height; y++) {
        /* Row distance from camera plane */
        float row_dist = (float)(half) / (float)(y - half);

        /* Real-world step between pixels on this row */
        float step_x = row_dist * (right_ray_x - left_ray_x) / (float)width;
        float step_y = row_dist * (right_ray_y - left_ray_y) / (float)width;

        /* Real-world position of leftmost pixel on this row */
        float floor_x = state->player.px / 64.0f + row_dist * left_ray_x;
        float floor_y = state->player.py / 64.0f + row_dist * left_ray_y;

        for (int x = 0; x < width; x++) {
            float u = floor_x - floorf(floor_x);
            float v = floor_y - floorf(floor_y);

            uint32_t color;
            if (floor_tex)
                color = texture_sample(floor_tex, u, v);
            else
                color = rgb_ren(40, 40, 40);

            /* Also draw ceiling (mirrored) */
            state->pixels[y * width + x]              = color;
            state->pixels[(height - 1 - y) * width + x] = rgb_ren(20, 20, 50);

            floor_x += step_x;
            floor_y += step_y;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Sprite pass                                                         */
/* ------------------------------------------------------------------ */

void renderer_pass_sprites(Renderer *ren, GameState *state) {
    if (!ren || !ren->show_sprites || !ren->sprites || !ren->textures) return;

    sprite_sort_by_distance(ren->sprites,
                            state->player.px, state->player.py);
    sprite_render_all(ren->sprites, &ren->zbuf, ren->textures,
                      state->pixels, state->width, state->height,
                      state->player.px, state->player.py,
                      (float)state->player.pa, 60);
}

/* ------------------------------------------------------------------ */
/* HUD pass                                                            */
/* ------------------------------------------------------------------ */

void renderer_pass_hud(Renderer *ren, GameState *state) {
    if (!ren || !ren->show_hud || !ren->hud) return;
    hud_render(ren->hud, state->pixels, state->width, state->height);
}

/* ------------------------------------------------------------------ */
/* Full frame                                                          */
/* ------------------------------------------------------------------ */

void renderer_draw_frame(Renderer *ren, GameState *state) {
    if (!ren || !state || !state->pixels) return;

    renderer_pass_sky_floor(ren, state);

    if (ren->textured_floor)
        renderer_pass_floor_cast(ren, state);

    renderer_pass_walls(ren, state);
    renderer_pass_sprites(ren, state);
    renderer_pass_hud(ren, state);
}
