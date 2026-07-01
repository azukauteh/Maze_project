#ifndef TEXTURE_H
#define TEXTURE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Texture subsystem — software-rendered wall and floor textures.
 *
 * Textures are stored as raw RGBA pixel arrays (R=bits31-24, G=23-16,
 * B=15-8, A=7-0). All textures are power-of-two square dimensions.
 *
 * The raycaster samples textures using integer UV coordinates:
 *   u = hit_offset_within_tile * tex->width / MAP_SCALE
 *   v = (screen_y - wall_top)  * tex->height / wall_height
 *
 * VULN NOTE (for fuzzer):
 *   texture_load_ppm() reads PPM pixel data into tex->pixels using a
 *   pre-allocated buffer sized from the declared width/height in the header.
 *   If the pixel body is longer than width*height pixels, the write loop
 *   continues past the end of the allocation — OOB heap write.
 */

#define TEX_MAX_DIM   256   /* maximum texture side length */
#define TEX_MAX_COUNT  32   /* maximum textures in a registry */

typedef struct {
    uint32_t *pixels;   /* ARGB8888, row-major, width*height entries */
    int       width;
    int       height;
    char      name[64]; /* logical name, e.g. "wall_grey" */
} Texture;

typedef struct {
    Texture textures[TEX_MAX_COUNT];
    int     count;
} TextureRegistry;

/* ---- Lifecycle ---- */
void texture_init    (Texture *tex);
void texture_free    (Texture *tex);
int  texture_alloc   (Texture *tex, int width, int height, const char *name);

/* ---- Procedural generators ---- */
void texture_fill_solid  (Texture *tex, uint32_t color);
void texture_fill_checker(Texture *tex, uint32_t c0, uint32_t c1,
                          int check_size);
void texture_fill_brick  (Texture *tex, uint32_t mortar, uint32_t face,
                          int brick_w, int brick_h);
void texture_fill_noise  (Texture *tex, uint32_t base, uint32_t variance,
                          unsigned int seed);
void texture_fill_stripes(Texture *tex, uint32_t c0, uint32_t c1,
                          int stripe_w, int horizontal);

/* ---- PPM loader (VULN: no body-length cap) ---- */
int texture_load_ppm(Texture *tex, const uint8_t *data, size_t size,
                     const char *name);

/* ---- Sampling ---- */
uint32_t texture_sample    (const Texture *tex, float u, float v);
uint32_t texture_sample_int(const Texture *tex, int u, int v);

/* ---- Registry ---- */
void     texture_registry_init(TextureRegistry *reg);
void     texture_registry_free(TextureRegistry *reg);
Texture *texture_registry_get (TextureRegistry *reg, const char *name);
int      texture_registry_add (TextureRegistry *reg, Texture tex);
void     texture_registry_generate_defaults(TextureRegistry *reg);

#ifdef __cplusplus
}
#endif

#endif /* TEXTURE_H */
