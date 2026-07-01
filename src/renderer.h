#ifndef RENDERER_H
#define RENDERER_H

#include <stdint.h>
#include "engine.h"
#include "texture.h"
#include "sprite.h"
#include "hud.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Renderer — extended render pipeline that adds textured walls, a
 * textured floor/ceiling, sprite pass, HUD overlay, and a ZBuffer.
 *
 * Relationship to engine_render():
 *   engine_render() is the original flat-color raycaster kept for
 *   compatibility and headless testing. renderer_draw_frame() is the
 *   extended pipeline that calls the same DDA core but adds:
 *     1. Textured wall columns (u coordinate from hit offset)
 *     2. Floor/ceiling texture cast per pixel below/above wall slice
 *     3. ZBuffer filled during wall pass for sprite clipping
 *     4. Sprite pass (sprite_render_all)
 *     5. HUD overlay (hud_render)
 *
 * The renderer owns no GPU state. It writes only to state->pixels.
 * SDL2 texture upload stays in main.c.
 *
 * Texture mapping coordinate system:
 *   Wall u:  (hit_world_coord % 64) / 64.0  → maps to tex x
 *   Wall v:  (screen_y - wall_top) / wall_h  → maps to tex y
 *   Floor:   computed per pixel using inverse projection (expensive but correct)
 */

typedef struct {
    TextureRegistry *textures;  /* borrowed, not owned */
    SpriteList      *sprites;   /* borrowed, not owned */
    HUDState        *hud;       /* borrowed, not owned */
    ZBuffer          zbuf;      /* owned — allocated in renderer_init */
    int              textured_walls;   /* 0 = flat color, 1 = textured */
    int              textured_floor;   /* 0 = gradient, 1 = textured   */
    int              show_sprites;     /* 0 = skip sprite pass          */
    int              show_hud;         /* 0 = skip HUD overlay          */
} Renderer;

/* ---- Lifecycle ---- */
int  renderer_init   (Renderer *ren, int screen_w, TextureRegistry *tex,
                      SpriteList *sprites, HUDState *hud);
void renderer_shutdown(Renderer *ren);

/* ---- Per-frame ---- */
void renderer_draw_frame(Renderer *ren, GameState *state);

/* ---- Sub-passes (exposed for testing) ---- */
void renderer_pass_sky_floor (Renderer *ren, GameState *state);
void renderer_pass_walls     (Renderer *ren, GameState *state);
void renderer_pass_floor_cast(Renderer *ren, GameState *state);
void renderer_pass_sprites   (Renderer *ren, GameState *state);
void renderer_pass_hud       (Renderer *ren, GameState *state);

/* ---- Texture selection ---- */
/*
 * renderer_wall_texture — return the Texture* to use for a given level
 * and wall cell type. Returns NULL if registry is empty (falls back to
 * flat color in the wall pass).
 */
const Texture *renderer_wall_texture(const Renderer *ren, int level,
                                     int cell_type);

/* ---- Config ---- */
void renderer_set_textured_walls (Renderer *ren, int on);
void renderer_set_textured_floor (Renderer *ren, int on);
void renderer_set_show_sprites   (Renderer *ren, int on);
void renderer_set_show_hud       (Renderer *ren, int on);

#ifdef __cplusplus
}
#endif

#endif /* RENDERER_H */
