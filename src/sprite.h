#ifndef SPRITE_H
#define SPRITE_H

#include <stdint.h>
#include "texture.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Sprite subsystem — billboard sprites in the 3D view.
 *
 * Sprites are world-space objects (pickups, decorations, markers)
 * rendered as camera-facing quads. Sort by distance, draw back-to-front,
 * clip against the per-column Z-buffer from the wall pass.
 *
 * This implementation is a scaffold: the renderer is not yet wired to
 * engine_render() but the data model and sort/clip logic are complete
 * and tested.
 */

#define SPRITE_MAX_COUNT 64

typedef enum {
    SPRITE_TYPE_DECORATION = 0,  /* static, no interaction */
    SPRITE_TYPE_PICKUP     = 1,  /* collected on contact   */
    SPRITE_TYPE_MARKER     = 2,  /* directional hint arrow */
} SpriteType;

typedef struct {
    float      wx, wy;         /* world-space position (pixels, like player) */
    SpriteType type;
    int        active;         /* 0 = dead/collected, skip rendering          */
    int        tex_index;      /* index into TextureRegistry                  */
    float      scale;          /* 1.0 = one tile wide                         */
    uint32_t   tint;           /* ARGB multiplicative tint (0xFFFFFFFF = none) */
} Sprite;

typedef struct {
    Sprite sprites[SPRITE_MAX_COUNT];
    int    count;
} SpriteList;

/* Z-buffer: one float per screen column, filled during wall pass */
typedef struct {
    float  *zbuf;   /* allocated to screen_width floats */
    int     width;
} ZBuffer;

/* ---- Lifecycle ---- */
void sprite_list_init (SpriteList *list);
void sprite_list_clear(SpriteList *list);
int  sprite_add       (SpriteList *list, float wx, float wy,
                       SpriteType type, int tex_index, float scale);
void sprite_collect   (SpriteList *list, int index);

/* ---- Z-buffer ---- */
int  zbuffer_alloc(ZBuffer *zb, int width);
void zbuffer_free (ZBuffer *zb);
void zbuffer_clear(ZBuffer *zb);

/* ---- Sort ---- */
void sprite_sort_by_distance(SpriteList *list,
                             float player_x, float player_y);

/* ---- Project ---- */
typedef struct {
    int   screen_x;    /* centre column of sprite on screen */
    int   screen_y0;   /* top row */
    int   screen_y1;   /* bottom row */
    int   col_start;   /* first screen column */
    int   col_end;     /* last screen column (exclusive) */
    float dist;        /* corrected distance */
    int   visible;     /* 0 if behind camera or fully clipped */
} SpriteProjection;

SpriteProjection sprite_project(const Sprite *sp,
                                float px, float py, float pa_deg,
                                int screen_w, int screen_h,
                                int fov_deg);

/* ---- Render ---- */
void sprite_render_all(const SpriteList   *list,
                       const ZBuffer      *zb,
                       const TextureRegistry *reg,
                       uint32_t           *pixels,
                       int                 screen_w,
                       int                 screen_h,
                       float               px, float py, float pa_deg,
                       int                 fov_deg);

#ifdef __cplusplus
}
#endif

#endif /* SPRITE_H */
