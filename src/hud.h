#ifndef HUD_H
#define HUD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * HUD (heads-up display) — overlays drawn on top of the 3D view.
 *
 * Everything is software-rendered into the same uint32_t framebuffer
 * used by the raycaster. No SDL2 font or texture loading is required.
 *
 * Included elements:
 *   - Level number badge (top-right)
 *   - Simple progress bar (bottom centre)
 *   - Flash message (centre screen, fades after N frames)
 *   - Crosshair (centre)
 *   - Frame counter (top-right corner, debug mode)
 */

#define HUD_MSG_MAX_LEN  64
#define HUD_MSG_DURATION 120  /* frames the flash message is visible */

typedef struct {
    char  flash_msg[HUD_MSG_MAX_LEN];
    int   flash_timer;       /* counts down from HUD_MSG_DURATION to 0 */
    int   current_level;
    int   max_levels;
    int   show_crosshair;
    int   show_debug;
    int   frame_count;
    float player_x;
    float player_y;
    float player_angle;
} HUDState;

/* ---- Lifecycle ---- */
void hud_init(HUDState *hud, int max_levels);
void hud_update(HUDState *hud);

/* ---- Setters ---- */
void hud_set_level  (HUDState *hud, int level);
void hud_set_player (HUDState *hud, float x, float y, float angle);
void hud_flash      (HUDState *hud, const char *msg);

/* ---- Render ---- */
void hud_render(const HUDState *hud, uint32_t *pixels,
                int screen_w, int screen_h);

/* ---- Primitive drawing (used by hud_render, exposed for testing) ---- */
void hud_draw_rect(uint32_t *pixels, int screen_w, int screen_h,
                   int x, int y, int w, int h, uint32_t color);

void hud_draw_char(uint32_t *pixels, int screen_w, int screen_h,
                   int x, int y, char c, uint32_t color, int scale);

void hud_draw_string(uint32_t *pixels, int screen_w, int screen_h,
                     int x, int y, const char *str, uint32_t color,
                     int scale);

void hud_draw_number(uint32_t *pixels, int screen_w, int screen_h,
                     int x, int y, int number, uint32_t color, int scale);

#ifdef __cplusplus
}
#endif

#endif /* HUD_H */
