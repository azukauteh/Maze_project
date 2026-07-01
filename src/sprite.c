/*
 * src/sprite.c — billboard sprite system.
 *
 * Sprites are sorted back-to-front by squared distance (avoids sqrt),
 * projected onto the screen plane, and drawn column-by-column.
 * Each column is clipped against the ZBuffer filled by the wall pass.
 *
 * No intentional vulnerabilities in this module — it is the reference
 * for "how to do it right" contrast against texture.c and map_parser.c.
 */

#include "sprite.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

#define PI            3.14159265359f
#define DEG_TO_RAD(a) ((a) * PI / 180.0f)

/* ------------------------------------------------------------------ */
/* Lifecycle                                                           */
/* ------------------------------------------------------------------ */

void sprite_list_init(SpriteList *list) {
    if (!list) return;
    memset(list, 0, sizeof(*list));
}

void sprite_list_clear(SpriteList *list) {
    if (!list) return;
    list->count = 0;
}

int sprite_add(SpriteList *list, float wx, float wy,
               SpriteType type, int tex_index, float scale) {
    if (!list || list->count >= SPRITE_MAX_COUNT) return -1;
    Sprite *s   = &list->sprites[list->count];
    s->wx        = wx;
    s->wy        = wy;
    s->type      = type;
    s->active    = 1;
    s->tex_index = tex_index;
    s->scale     = scale > 0.0f ? scale : 1.0f;
    s->tint      = 0xFFFFFFFFu;
    return list->count++;
}

void sprite_collect(SpriteList *list, int index) {
    if (!list || index < 0 || index >= list->count) return;
    list->sprites[index].active = 0;
}

/* ------------------------------------------------------------------ */
/* Z-buffer                                                            */
/* ------------------------------------------------------------------ */

int zbuffer_alloc(ZBuffer *zb, int width) {
    if (!zb || width <= 0) return 0;
    zb->zbuf  = (float *)malloc((size_t)width * sizeof(float));
    if (!zb->zbuf) return 0;
    zb->width = width;
    zbuffer_clear(zb);
    return 1;
}

void zbuffer_free(ZBuffer *zb) {
    if (!zb) return;
    if (zb->zbuf) { free(zb->zbuf); zb->zbuf = NULL; }
    zb->width = 0;
}

void zbuffer_clear(ZBuffer *zb) {
    if (!zb || !zb->zbuf) return;
    for (int i = 0; i < zb->width; i++)
        zb->zbuf[i] = 1e6f;
}

/* ------------------------------------------------------------------ */
/* Sort (insertion sort — count is small, ≤64)                        */
/* ------------------------------------------------------------------ */

void sprite_sort_by_distance(SpriteList *list, float px, float py) {
    if (!list || list->count < 2) return;

    /* Compute squared distances */
    float dist2[SPRITE_MAX_COUNT];
    for (int i = 0; i < list->count; i++) {
        float dx = list->sprites[i].wx - px;
        float dy = list->sprites[i].wy - py;
        dist2[i] = dx * dx + dy * dy;
    }

    /* Insertion sort descending (far first) */
    for (int i = 1; i < list->count; i++) {
        Sprite tmp  = list->sprites[i];
        float  td   = dist2[i];
        int    j    = i - 1;
        while (j >= 0 && dist2[j] < td) {
            list->sprites[j + 1] = list->sprites[j];
            dist2[j + 1]         = dist2[j];
            j--;
        }
        list->sprites[j + 1] = tmp;
        dist2[j + 1]         = td;
    }
}

/* ------------------------------------------------------------------ */
/* Project                                                             */
/* ------------------------------------------------------------------ */

SpriteProjection sprite_project(const Sprite *sp,
                                float px, float py, float pa_deg,
                                int screen_w, int screen_h,
                                int fov_deg) {
    SpriteProjection proj;
    memset(&proj, 0, sizeof(proj));

    if (!sp || !sp->active) return proj;

    /* Translate sprite into camera space */
    float dx = sp->wx - px;
    float dy = sp->wy - py;

    float pa_rad = DEG_TO_RAD(pa_deg);
    float cos_a  = cosf(pa_rad);
    float sin_a  = -sinf(pa_rad); /* Y axis is inverted in world space */

    /* Camera-space coordinates */
    float cam_x = cos_a * dx - sin_a * dy;
    float cam_z = sin_a * dx + cos_a * dy;

    /* Behind camera */
    if (cam_z <= 0.01f) return proj;

    proj.dist = cam_z;

    float half_fov_tan = tanf(DEG_TO_RAD((float)fov_deg * 0.5f));

    /* Project to screen */
    float proj_x  = (cam_x / (cam_z * half_fov_tan)) * ((float)screen_w * 0.5f)
                    + (float)screen_w * 0.5f;

    /* Sprite apparent height: one tile at distance cam_z */
    int sprite_h = (int)((float)screen_h / cam_z * 64.0f * sp->scale);
    if (sprite_h < 1) sprite_h = 1;
    if (sprite_h > screen_h * 4) sprite_h = screen_h * 4;

    int sprite_w = sprite_h; /* square sprites */

    proj.screen_x = (int)proj_x;
    proj.screen_y0 = (screen_h - sprite_h) / 2;
    proj.screen_y1 = proj.screen_y0 + sprite_h;
    proj.col_start = proj.screen_x - sprite_w / 2;
    proj.col_end   = proj.screen_x + sprite_w / 2;
    proj.visible   = 1;

    return proj;
}

/* ------------------------------------------------------------------ */
/* Render all sprites                                                  */
/* ------------------------------------------------------------------ */

void sprite_render_all(const SpriteList      *list,
                       const ZBuffer         *zb,
                       const TextureRegistry *reg,
                       uint32_t              *pixels,
                       int                    screen_w,
                       int                    screen_h,
                       float                  px, float py, float pa_deg,
                       int                    fov_deg) {
    if (!list || !zb || !reg || !pixels) return;

    for (int i = 0; i < list->count; i++) {
        const Sprite *sp = &list->sprites[i];
        if (!sp->active) continue;

        SpriteProjection proj = sprite_project(sp, px, py, pa_deg,
                                               screen_w, screen_h, fov_deg);
        if (!proj.visible) continue;
        if (proj.dist >= 1e5f) continue;

        /* Get texture */
        const Texture *tex = NULL;
        if (sp->tex_index >= 0 && sp->tex_index < reg->count)
            tex = &reg->textures[sp->tex_index];

        int sprite_w = proj.col_end - proj.col_start;
        if (sprite_w <= 0) continue;

        for (int col = proj.col_start; col < proj.col_end; col++) {
            if (col < 0 || col >= screen_w) continue;
            /* Z-clip: skip column if wall is closer */
            if (zb->zbuf[col] <= proj.dist) continue;

            /* Texture u coordinate */
            float u = (float)(col - proj.col_start) / (float)sprite_w;

            for (int row = proj.screen_y0; row < proj.screen_y1; row++) {
                if (row < 0 || row >= screen_h) continue;
                float v = (float)(row - proj.screen_y0) /
                          (float)(proj.screen_y1 - proj.screen_y0);

                uint32_t color;
                if (tex)
                    color = texture_sample(tex, u, v);
                else
                    color = 0xFF00D25Au; /* fallback: green */

                /* Skip fully transparent */
                if ((color >> 24) == 0) continue;

                /* Apply tint */
                if (sp->tint != 0xFFFFFFFFu) {
                    uint8_t tr = (sp->tint >> 16) & 0xFF;
                    uint8_t tg = (sp->tint >>  8) & 0xFF;
                    uint8_t tb = (sp->tint      ) & 0xFF;
                    uint8_t cr = (uint8_t)(((color >> 16) & 0xFF) * tr / 255);
                    uint8_t cg = (uint8_t)(((color >>  8) & 0xFF) * tg / 255);
                    uint8_t cb = (uint8_t)(((color      ) & 0xFF) * tb / 255);
                    color = 0xFF000000u | ((uint32_t)cr << 16) |
                            ((uint32_t)cg << 8) | cb;
                }

                pixels[row * screen_w + col] = color;
            }
        }
    }
}
