#ifndef ENGINE_H
#define ENGINE_H

#include <stdint.h>

/* Map dimensions */
#define MAP_WIDTH 8
#define MAP_HEIGHT 8
#define MAP_SCALE 64

/* Game state */
typedef struct {
    float px, py;       /* Player position */
    float pa;           /* Player angle (0-360) */
    float pdx, pdy;     /* Player direction */
} Player;

typedef struct {
    int width, height;
    uint32_t *pixels;   /* Framebuffer */
    Player player;
    int map[MAP_WIDTH * MAP_HEIGHT];
} GameState;

/* Initialize game state */
void engine_init(GameState *state);

/* Handle player input (WASD + arrow keys) */
void engine_input(GameState *state, int key, int pressed);

/* Update game logic */
void engine_update(GameState *state);

/* Render frame to framebuffer */
void engine_render(GameState *state);

#endif // ENGINE_H
