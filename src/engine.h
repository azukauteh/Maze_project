#ifndef ENGINE_H
#define ENGINE_H

#include <stdint.h>

#define MAP_WIDTH  8
#define MAP_HEIGHT 8
#define MAP_SCALE  64

typedef struct {
    float px, py;
    float pa;
    float pdx, pdy;
} Player;

typedef struct {
    int       width, height;
    uint32_t *pixels;
    Player    player;
    int       map[MAP_WIDTH * MAP_HEIGHT];
    int       current_level;
    int       level_complete;
} GameState;

void engine_init(GameState *state);
void engine_input(GameState *state, int key, int pressed);
void engine_update(GameState *state);
void engine_render(GameState *state);
void engine_next_level(GameState *state);
int  engine_num_levels(void);   /* returns total number of levels */

#endif /* ENGINE_H */
