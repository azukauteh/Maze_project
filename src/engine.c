#include "engine.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define PI            3.14159265359f
#define DEG_TO_RAD(a) ((a) * PI / 180.0f)

#define CELL_FLOOR  0
#define CELL_WALL   1
#define CELL_EXIT   2

#define FOG_MAX     400.0f
#define MINI_SCALE  8
#define MINI_PAD    6

/* ------------------------------------------------------------------
   Maps — all 6 levels
   Rules: outer ring = all 1s, exactly one 2 (exit), no other values.
   Exit is always at tile (col=6, row=6) so the minimap gives a
   consistent landmark across levels. Spawn angles vary so the player
   cannot trivially repeat the same strategy each time.
   ------------------------------------------------------------------ */

/* Level 1 — open corridors, blue palette. Intro. */
static const int level1_map[MAP_WIDTH * MAP_HEIGHT] = {
    1,1,1,1,1,1,1,1,
    1,0,1,0,0,0,0,1,
    1,0,1,0,0,0,0,1,
    1,0,1,0,0,0,0,1,
    1,0,0,0,0,0,0,1,
    1,0,0,0,0,1,0,1,
    1,0,0,0,0,0,2,1,
    1,1,1,1,1,1,1,1,
};

/* Level 2 — denser corridors, green palette. */
static const int level2_map[MAP_WIDTH * MAP_HEIGHT] = {
    1,1,1,1,1,1,1,1,
    1,0,0,0,1,0,0,1,
    1,0,1,0,1,0,1,1,
    1,0,1,0,0,0,0,1,
    1,1,1,0,1,0,0,1,
    1,0,0,0,1,0,1,1,
    1,0,1,0,0,0,2,1,
    1,1,1,1,1,1,1,1,
};

/* Level 3 — tight dead-ends, red palette. Spawn faces south. */
static const int level3_map[MAP_WIDTH * MAP_HEIGHT] = {
    1,1,1,1,1,1,1,1,
    1,0,0,0,0,0,0,1,
    1,0,1,1,1,1,0,1,
    1,0,1,0,0,0,0,1,
    1,0,1,0,1,1,1,1,
    1,0,0,0,1,0,0,1,
    1,1,1,0,0,0,2,1,
    1,1,1,1,1,1,1,1,
};

/*
 * Level 4 — spiral approach, purple palette.
 * The only path to the exit winds counter-clockwise around a central
 * pillar. Spawn faces west so the first instinct (turn right) is wrong.
 *
 *   1 1 1 1 1 1 1 1
 *   1 0 0 0 0 0 0 1
 *   1 0 1 1 1 1 0 1
 *   1 0 1 0 0 1 0 1
 *   1 0 1 0 1 1 0 1
 *   1 0 0 0 0 0 0 1
 *   1 1 1 1 1 0 2 1
 *   1 1 1 1 1 1 1 1
 */
static const int level4_map[MAP_WIDTH * MAP_HEIGHT] = {
    1,1,1,1,1,1,1,1,
    1,0,0,0,0,0,0,1,
    1,0,1,1,1,1,0,1,
    1,0,1,0,0,1,0,1,
    1,0,1,0,1,1,0,1,
    1,0,0,0,0,0,0,1,
    1,1,1,1,1,0,2,1,
    1,1,1,1,1,1,1,1,
};

/*
 * Level 5 — broken grid, orange palette.
 * Looks like a grid but every other intersection is blocked, forcing
 * the player to backtrack repeatedly. Spawn faces north (up = row 0
 * direction) which is also wrong. Exit requires going south first.
 *
 *   1 1 1 1 1 1 1 1
 *   1 0 1 0 1 0 0 1
 *   1 0 0 0 0 0 1 1
 *   1 1 0 1 0 1 0 1
 *   1 0 0 0 0 0 0 1
 *   1 0 1 0 1 1 0 1
 *   1 0 0 0 1 0 2 1
 *   1 1 1 1 1 1 1 1
 */
static const int level5_map[MAP_WIDTH * MAP_HEIGHT] = {
    1,1,1,1,1,1,1,1,
    1,0,1,0,1,0,0,1,
    1,0,0,0,0,0,1,1,
    1,1,0,1,0,1,0,1,
    1,0,0,0,0,0,0,1,
    1,0,1,0,1,1,0,1,
    1,0,0,0,1,0,2,1,
    1,1,1,1,1,1,1,1,
};

/*
 * Level 6 — maximum density, cyan palette. Final level.
 * Almost every cell is walled. The single open path is S-shaped.
 * Spawn faces east (right) but the first segment goes south.
 * FOG is the biggest hazard here — the narrow corridors make walls
 * appear and disappear abruptly.
 *
 *   1 1 1 1 1 1 1 1
 *   1 0 1 0 0 0 0 1
 *   1 0 1 0 1 1 0 1
 *   1 0 0 0 1 0 0 1
 *   1 1 1 0 1 0 1 1
 *   1 0 0 0 0 0 1 1
 *   1 0 1 1 1 0 2 1
 *   1 1 1 1 1 1 1 1
 */
static const int level6_map[MAP_WIDTH * MAP_HEIGHT] = {
    1,1,1,1,1,1,1,1,
    1,0,1,0,0,0,0,1,
    1,0,1,0,1,1,0,1,
    1,0,0,0,1,0,0,1,
    1,1,1,0,1,0,1,1,
    1,0,0,0,0,0,1,1,
    1,0,1,1,1,0,2,1,
    1,1,1,1,1,1,1,1,
};

/* ------------------------------------------------------------------
   Level descriptor table — add new levels here only.
   ------------------------------------------------------------------ */
typedef struct {
    const int *map;
    float      spawn_x, spawn_y, spawn_a;
} LevelDesc;

static const LevelDesc levels[] = {
    /* index 0 unused; levels are 1-indexed */
    { NULL,        0,     0,     0   },
    { level1_map,  150.f, 400.f, 90  },   /* 1 */
    { level2_map,  100.f, 100.f, 0   },   /* 2 */
    { level3_map,  100.f, 100.f, 270 },   /* 3 */
    { level4_map,  100.f, 400.f, 180 },   /* 4 — spawn faces west */
    { level5_map,  100.f, 100.f,  0  },   /* 5 — spawn top-left open tile, faces east */
    { level6_map,  100.f, 100.f, 0   },   /* 6 — spawn faces east */
};

#define NUM_LEVELS ((int)(sizeof(levels)/sizeof(levels[0])) - 1)

/* ------------------------------------------------------------------
   Helpers
   ------------------------------------------------------------------ */

static int fix_angle(int a) {
    while (a > 359) a -= 360;
    while (a < 0)   a += 360;
    return a;
}

static float proj_dist(float ax, float ay, float bx, float by, int ang) {
    return cosf(DEG_TO_RAD(ang)) * (bx - ax) - sinf(DEG_TO_RAD(ang)) * (by - ay);
}

static inline uint32_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

static uint32_t apply_fog(uint32_t color, float dist) {
    float t = dist / FOG_MAX;
    if (t > 1.0f) t = 1.0f;
    float k = 1.0f - t;
    uint8_t r = (uint8_t)(((color >> 16) & 0xFF) * k);
    uint8_t g = (uint8_t)(((color >>  8) & 0xFF) * k);
    uint8_t b = (uint8_t)(((color      ) & 0xFF) * k);
    return rgb(r, g, b);
}

static void draw_vline(uint32_t *pixels, int width, int height,
                       int x, int y1, int y2, uint32_t color) {
    if (x < 0 || x >= width) return;
    if (y1 > y2) { int t = y1; y1 = y2; y2 = t; }
    y1 = y1 < 0 ? 0 : y1;
    y2 = y2 >= height ? height - 1 : y2;
    for (int y = y1; y <= y2; y++)
        pixels[y * width + x] = color;
}

static int map_cell(const GameState *state, int mx, int my) {
    if (mx < 0 || mx >= MAP_WIDTH || my < 0 || my >= MAP_HEIGHT)
        return CELL_WALL;
    return state->map[my * MAP_WIDTH + mx];
}

static int cell_at_pos(const GameState *state, float x, float y) {
    return map_cell(state, (int)x >> 6, (int)y >> 6);
}

/* ------------------------------------------------------------------
   Public API
   ------------------------------------------------------------------ */

int engine_num_levels(void) { return NUM_LEVELS; }

void engine_init(GameState *state) {
    state->current_level  = 1;
    state->level_complete = 0;

    const LevelDesc *d = &levels[1];
    memcpy(state->map, d->map, MAP_WIDTH * MAP_HEIGHT * sizeof(int));

    state->player.px  = d->spawn_x;
    state->player.py  = d->spawn_y;
    state->player.pa  = (int)d->spawn_a;
    state->player.pdx = cosf(DEG_TO_RAD(d->spawn_a));
    state->player.pdy = -sinf(DEG_TO_RAD(d->spawn_a));

    if (state->pixels) free(state->pixels);
    state->pixels = (uint32_t *)malloc(state->width * state->height * sizeof(uint32_t));
    memset(state->pixels, 0, state->width * state->height * sizeof(uint32_t));
}

void engine_next_level(GameState *state) {
    state->level_complete = 0;
    state->current_level++;

    if (state->current_level > NUM_LEVELS) return; /* caller handles win */

    const LevelDesc *d = &levels[state->current_level];
    memcpy(state->map, d->map, MAP_WIDTH * MAP_HEIGHT * sizeof(int));
    state->player.px  = d->spawn_x;
    state->player.py  = d->spawn_y;
    state->player.pa  = (int)d->spawn_a;
    state->player.pdx = cosf(DEG_TO_RAD(d->spawn_a));
    state->player.pdy = -sinf(DEG_TO_RAD(d->spawn_a));
}

void engine_input(GameState *state, int key, int pressed) {
    if (!pressed || state->level_complete) return;

    const float speed     = 5.0f;
    const float rot_speed = 5.0f;

    switch (key) {
        case 'w': case 'W': {
            float nx = state->player.px + state->player.pdx * speed;
            float ny = state->player.py + state->player.pdy * speed;
            int c = cell_at_pos(state, nx, ny);
            if      (c == CELL_EXIT) state->level_complete = 1;
            else if (c != CELL_WALL) { state->player.px = nx; state->player.py = ny; }
            break;
        }
        case 's': case 'S': {
            float nx = state->player.px - state->player.pdx * speed;
            float ny = state->player.py - state->player.pdy * speed;
            if (cell_at_pos(state, nx, ny) == CELL_FLOOR) {
                state->player.px = nx; state->player.py = ny;
            }
            break;
        }
        case 'a': case 'A':
            state->player.pa  = fix_angle(state->player.pa + (int)rot_speed);
            state->player.pdx = cosf(DEG_TO_RAD(state->player.pa));
            state->player.pdy = -sinf(DEG_TO_RAD(state->player.pa));
            break;
        case 'd': case 'D':
            state->player.pa  = fix_angle(state->player.pa - (int)rot_speed);
            state->player.pdx = cosf(DEG_TO_RAD(state->player.pa));
            state->player.pdy = -sinf(DEG_TO_RAD(state->player.pa));
            break;
    }
}

void engine_update(GameState *state) { (void)state; }

/* ------------------------------------------------------------------
   Render
   ------------------------------------------------------------------ */

/*
 * Per-level sky/floor base colors.
 * sr/sg/sb = sky base RGB. fr/fg/fb = floor base RGB.
 * Gradient adds up to +30/+40 on top of these toward the horizon.
 *
 * Level  Sky description     Floor description
 * -----  -----------------   -----------------
 *   1    Dark navy           Charcoal
 *   2    Dark forest green   Warm dark brown
 *   3    Dark crimson        Dark maroon
 *   4    Deep purple         Dark indigo
 *   5    Dark amber/orange   Dark sienna
 *   6    Dark teal/cyan      Very dark slate
 */
typedef struct { uint8_t sr,sg,sb, fr,fg,fb; } LevelPalette;

static LevelPalette level_palette(int level) {
    switch (level) {
        case 2:  return (LevelPalette){ 20, 40, 20,  30, 25, 20 };
        case 3:  return (LevelPalette){ 40, 10, 10,  25, 15, 15 };
        case 4:  return (LevelPalette){ 30, 10, 50,  20, 10, 35 };
        case 5:  return (LevelPalette){ 50, 30, 10,  35, 20, 10 };
        case 6:  return (LevelPalette){ 10, 40, 45,  10, 25, 30 };
        default: return (LevelPalette){ 20, 20, 60,  25, 25, 25 };
    }
}

/*
 * Per-level wall base colors (bright face / dark face).
 *
 * Level  Theme
 * -----  -----
 *   1    Grey
 *   2    Green-grey
 *   3    Red-grey
 *   4    Purple
 *   5    Orange-brown
 *   6    Teal/cyan
 */
static uint32_t wall_color(int level, int bright) {
    switch (level) {
        case 2:  return bright ? rgb( 80,130, 80) : rgb( 55, 90, 55);
        case 3:  return bright ? rgb(140, 70, 70) : rgb(100, 50, 50);
        case 4:  return bright ? rgb(110, 60,160) : rgb( 75, 40,115);
        case 5:  return bright ? rgb(160,100, 40) : rgb(115, 70, 28);
        case 6:  return bright ? rgb( 40,160,160) : rgb( 28,110,110);
        default: return bright ? rgb(120,120,120) : rgb( 80, 80, 80);
    }
}

static void render_sky_floor(GameState *state) {
    int width  = state->width;
    int height = state->height;
    int half   = height / 2;
    LevelPalette p = level_palette(state->current_level);

    for (int y = 0; y < half; y++) {
        float t  = (float)y / (float)half;
        uint32_t c = rgb(
            (uint8_t)(p.sr + 30 * t),
            (uint8_t)(p.sg + 30 * t),
            (uint8_t)(p.sb + 40 * t));
        for (int x = 0; x < width; x++)
            state->pixels[y * width + x] = c;
    }

    for (int y = half; y < height; y++) {
        float t  = (float)(y - half) / (float)(height - half);
        uint32_t c = rgb(
            (uint8_t)(p.fr + 25 * t),
            (uint8_t)(p.fg + 20 * t),
            (uint8_t)(p.fb + 15 * t));
        for (int x = 0; x < width; x++)
            state->pixels[y * width + x] = c;
    }
}

static void render_walls(GameState *state) {
    int width     = state->width;
    int height    = state->height;
    int ray_count = 60;
    int fov_half  = 30;
    int ra        = fix_angle(state->player.pa + fov_half);

    for (int r = 0; r < ray_count; r++) {
        float disV = 1e6f, disH = 1e6f;
        int   hitV = 0,   hitH = 0;
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
                int c = map_cell(state, (int)rx >> 6, (int)ry >> 6);
                if (c == CELL_WALL || c == CELL_EXIT) {
                    disV = proj_dist(state->player.px, state->player.py, rx, ry, ra);
                    hitV = c; break;
                }
                rx += xo; ry += yo;
            }
        }

        /* Horizontal intersections */
        if (fabsf(sinf(DEG_TO_RAD(ra))) > 0.001f) {
            float rx, ry, xo, yo;
            float Tan2 = 1.0f / Tan;
            if (sinf(DEG_TO_RAD(ra)) > 0.0f) {
                ry = (float)((((int)state->player.py >> 6) << 6)) - 0.0001f;
                yo = -64.0f;
            } else {
                ry = (float)((((int)state->player.py >> 6) << 6) + 64);
                yo = 64.0f;
            }
            rx = (state->player.py - ry) * Tan2 + state->player.px;
            xo = -yo * Tan2;
            for (int dof = 0; dof < 8; dof++) {
                int c = map_cell(state, (int)rx >> 6, (int)ry >> 6);
                if (c == CELL_WALL || c == CELL_EXIT) {
                    disH = proj_dist(state->player.px, state->player.py, rx, ry, ra);
                    hitH = c; break;
                }
                rx += xo; ry += yo;
            }
        }

        float    dist    = disV;
        int      hitCell = hitV;
        int      bright  = 1;
        if (disH < disV) { dist = disH; hitCell = hitH; bright = 0; }

        uint32_t color;
        if (hitCell == CELL_EXIT)
            color = bright ? rgb(0, 210, 90) : rgb(0, 150, 65);
        else
            color = wall_color(state->current_level, bright);

        int ca = fix_angle(state->player.pa - ra);
        dist   = dist * cosf(DEG_TO_RAD(ca));
        if (dist < 1.0f) dist = 1.0f;

        color = apply_fog(color, dist);

        int lineH   = (MAP_SCALE * height) / (int)dist;
        if (lineH > height) lineH = height;
        int lineOff = (height - lineH) / 2;

        int sx = (r * width) / ray_count;
        int ex = ((r + 1) * width) / ray_count;
        for (int x = sx; x < ex && x < width; x++)
            draw_vline(state->pixels, width, height, x, lineOff, lineOff + lineH, color);

        ra = fix_angle(ra - 1);
    }
}

static void render_minimap(GameState *state) {
    int width  = state->width;
    int height = state->height;
    int ox = MINI_PAD, oy = MINI_PAD;

    for (int row = 0; row < MAP_HEIGHT; row++) {
        for (int col = 0; col < MAP_WIDTH; col++) {
            int cell = state->map[row * MAP_WIDTH + col];
            uint32_t color;
            if      (cell == CELL_WALL) color = rgb(180, 180, 180);
            else if (cell == CELL_EXIT) color = rgb(0, 210, 90);
            else                        color = rgb(40, 40, 40);

            int px = ox + col * MINI_SCALE;
            int py = oy + row * MINI_SCALE;
            for (int dy = 0; dy < MINI_SCALE - 1; dy++)
                for (int dx = 0; dx < MINI_SCALE - 1; dx++) {
                    int sx = px + dx, sy = py + dy;
                    if (sx >= 0 && sx < width && sy >= 0 && sy < height)
                        state->pixels[sy * width + sx] = color;
                }
        }
    }

    /* Player dot */
    int pdx_px = ox + (int)(state->player.px / 64.0f * MINI_SCALE);
    int pdy_px = oy + (int)(state->player.py / 64.0f * MINI_SCALE);
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            int sx = pdx_px + dx, sy = pdy_px + dy;
            if (sx >= 0 && sx < width && sy >= 0 && sy < height)
                state->pixels[sy * width + sx] = rgb(255, 80, 80);
        }

    /* Direction line */
    for (int i = 0; i < 5; i++) {
        int sx = pdx_px + (int)(state->player.pdx * i);
        int sy = pdy_px + (int)(state->player.pdy * i);
        if (sx >= 0 && sx < width && sy >= 0 && sy < height)
            state->pixels[sy * width + sx] = rgb(255, 200, 0);
    }
}

void engine_render(GameState *state) {
    render_sky_floor(state);
    render_walls(state);
    render_minimap(state);
}
