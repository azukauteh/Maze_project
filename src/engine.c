#include "engine.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define PI 3.14159265359
#define DEG_TO_RAD(a) ((a) * PI / 180.0)
#define RAD_TO_DEG(a) ((a) * 180.0 / PI)

static int default_map[MAP_WIDTH * MAP_HEIGHT] = {
    1,1,1,1,1,1,1,1,
    1,0,1,0,0,0,0,1,
    1,0,1,0,0,0,0,1,
    1,0,1,0,0,0,0,1,
    1,0,0,0,0,0,0,1,
    1,0,0,0,0,1,0,1,
    1,0,0,0,0,0,0,1,
    1,1,1,1,1,1,1,1
};

static int fix_angle(int a) {
    if (a > 359) a -= 360;
    if (a < 0) a += 360;
    return a;
}

static float distance(float ax, float ay, float bx, float by, int ang) {
    return cos(DEG_TO_RAD(ang)) * (bx - ax) - sin(DEG_TO_RAD(ang)) * (by - ay);
}

void engine_init(GameState *state) {
    state->player.px = 150.0f;
    state->player.py = 400.0f;
    state->player.pa = 90;
    state->player.pdx = cos(DEG_TO_RAD(state->player.pa));
    state->player.pdy = -sin(DEG_TO_RAD(state->player.pa));
    
    memcpy(state->map, default_map, sizeof(default_map));
    
    /* Allocate framebuffer */
    if (state->pixels) {
        free(state->pixels);
    }
    state->pixels = (uint32_t *)malloc(state->width * state->height * sizeof(uint32_t));
    memset(state->pixels, 0, state->width * state->height * sizeof(uint32_t));
}

void engine_input(GameState *state, int key, int pressed) {
    if (!pressed) return;
    
    float speed = 5.0f;
    float rot_speed = 5.0f;
    
    switch (key) {
        case 'w':
        case 'W':
            state->player.px += state->player.pdx * speed;
            state->player.py += state->player.pdy * speed;
            break;
        case 's':
        case 'S':
            state->player.px -= state->player.pdx * speed;
            state->player.py -= state->player.pdy * speed;
            break;
        case 'a':
        case 'A':
            state->player.pa += rot_speed;
            state->player.pa = fix_angle(state->player.pa);
            state->player.pdx = cos(DEG_TO_RAD(state->player.pa));
            state->player.pdy = -sin(DEG_TO_RAD(state->player.pa));
            break;
        case 'd':
        case 'D':
            state->player.pa -= rot_speed;
            state->player.pa = fix_angle(state->player.pa);
            state->player.pdx = cos(DEG_TO_RAD(state->player.pa));
            state->player.pdy = -sin(DEG_TO_RAD(state->player.pa));
            break;
    }
}

void engine_update(GameState *state) {
    /* Placeholder for physics/logic updates */
}

static inline uint32_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return (r << 16) | (g << 8) | b;
}

static void draw_vertical_line(uint32_t *pixels, int width, int height, int x, int y1, int y2, uint32_t color) {
    if (x < 0 || x >= width) return;
    if (y1 > y2) { int tmp = y1; y1 = y2; y2 = tmp; }
    if (y2 < 0 || y1 >= height) return;
    
    y1 = (y1 < 0) ? 0 : y1;
    y2 = (y2 >= height) ? height - 1 : y2;
    
    for (int y = y1; y <= y2; y++) {
        pixels[y * width + x] = color;
    }
}

void engine_render(GameState *state) {
    /* Clear framebuffer */
    memset(state->pixels, 0x20, state->width * state->height * sizeof(uint32_t));
    
    int width = state->width;
    int height = state->height;
    int fov_half_width = 30;  /* FOV / 2 */
    int ray_count = 60;
    
    /* Render vertical wall slices */
    int ra = fix_angle(state->player.pa + fov_half_width);
    
    for (int r = 0; r < ray_count; r++) {
        int dof = 0;
        float disV = 100000.0f, disH = 100000.0f;
        float vx = state->player.px, vy = state->player.py;
        float hx = state->player.px, hy = state->player.py;
        
        /* Check vertical grid lines */
        float Tan = tan(DEG_TO_RAD(ra));
        if (cos(DEG_TO_RAD(ra)) > 0.001f) {
            float rx = (((int)state->player.px >> 6) << 6) + 64;
            float ry = (state->player.px - rx) * Tan + state->player.py;
            float xo = 64.0f, yo = -xo * Tan;
            
            dof = 0;
            while (dof < 8) {
                int mx = (int)(rx) >> 6;
                int my = (int)(ry) >> 6;
                int mp = my * MAP_WIDTH + mx;
                if (mp >= 0 && mp < MAP_WIDTH * MAP_HEIGHT && state->map[mp] == 1) {
                    disV = distance(state->player.px, state->player.py, rx, ry, ra);
                    vx = rx;
                    vy = ry;
                    dof = 8;
                } else {
                    rx += xo;
                    ry += yo;
                    dof++;
                }
            }
        } else if (cos(DEG_TO_RAD(ra)) < -0.001f) {
            float rx = (((int)state->player.px >> 6) << 6) - 0.0001f;
            float ry = (state->player.px - rx) * Tan + state->player.py;
            float xo = -64.0f, yo = -xo * Tan;
            
            dof = 0;
            while (dof < 8) {
                int mx = (int)(rx) >> 6;
                int my = (int)(ry) >> 6;
                int mp = my * MAP_WIDTH + mx;
                if (mp >= 0 && mp < MAP_WIDTH * MAP_HEIGHT && state->map[mp] == 1) {
                    disV = distance(state->player.px, state->player.py, rx, ry, ra);
                    vx = rx;
                    vy = ry;
                    dof = 8;
                } else {
                    rx += xo;
                    ry += yo;
                    dof++;
                }
            }
        }
        
        /* Check horizontal grid lines */
        Tan = 1.0f / Tan;
        if (sin(DEG_TO_RAD(ra)) > 0.001f) {
            float ry = (((int)state->player.py >> 6) << 6) - 0.0001f;
            float rx = (state->player.py - ry) * Tan + state->player.px;
            float yo = -64.0f, xo = -yo * Tan;
            
            dof = 0;
            while (dof < 8) {
                int mx = (int)(rx) >> 6;
                int my = (int)(ry) >> 6;
                int mp = my * MAP_WIDTH + mx;
                if (mp >= 0 && mp < MAP_WIDTH * MAP_HEIGHT && state->map[mp] == 1) {
                    disH = distance(state->player.px, state->player.py, rx, ry, ra);
                    hx = rx;
                    hy = ry;
                    dof = 8;
                } else {
                    rx += xo;
                    ry += yo;
                    dof++;
                }
            }
        } else if (sin(DEG_TO_RAD(ra)) < -0.001f) {
            float ry = (((int)state->player.py >> 6) << 6) + 64;
            float rx = (state->player.py - ry) * Tan + state->player.px;
            float yo = 64.0f, xo = -yo * Tan;
            
            dof = 0;
            while (dof < 8) {
                int mx = (int)(rx) >> 6;
                int my = (int)(ry) >> 6;
                int mp = my * MAP_WIDTH + mx;
                if (mp >= 0 && mp < MAP_WIDTH * MAP_HEIGHT && state->map[mp] == 1) {
                    disH = distance(state->player.px, state->player.py, rx, ry, ra);
                    hx = rx;
                    hy = ry;
                    dof = 8;
                } else {
                    rx += xo;
                    ry += yo;
                    dof++;
                }
            }
        }
        
        /* Use closest hit */
        float dist = disV;
        uint32_t color = rgb(100, 100, 100);
        if (disH < disV) {
            dist = disH;
            color = rgb(80, 80, 80);
        }
        
        /* Fix fisheye and compute wall height */
        int ca = fix_angle(state->player.pa - ra);
        dist = dist * cos(DEG_TO_RAD(ca));
        
        int lineH = (MAP_SCALE * height) / (int)(dist + 1);
        if (lineH > height) lineH = height;
        
        int lineOff = (height - lineH) / 2;
        
        /* Draw vertical wall slice */
        int sx = (r * width) / ray_count;
        int ex = ((r + 1) * width) / ray_count;
        for (int x = sx; x < ex && x < width; x++) {
            draw_vertical_line(state->pixels, width, height, x, lineOff, lineOff + lineH, color);
        }
        
        ra = fix_angle(ra - 1);
    }
}
