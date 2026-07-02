#ifndef MINIMAP_H
#define MINIMAP_H

#include <stdint.h>

#include "engine.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MINIMAP_PATH_MAX 128

typedef struct {
    int tile_size;
    int pad;
    int show_fov_cone;
    int show_path;
} MinimapConfig;

typedef struct {
    int cols[MINIMAP_PATH_MAX];
    int rows[MINIMAP_PATH_MAX];
    int count;
    int found;
} MinimapPathResult;

void minimap_render(const MinimapConfig *config,
                    const GameState *state,
                    const MinimapPathResult *pf_result,
                    uint32_t *pixels,
                    int screen_w,
                    int screen_h);

int minimap_tile_at_screen(const MinimapConfig *config,
                           int screen_x,
                           int screen_y,
                           int *map_col,
                           int *map_row);

MinimapConfig minimap_default_config(void);

#ifdef __cplusplus
}
#endif

#endif
