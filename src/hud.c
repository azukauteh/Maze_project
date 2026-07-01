/*
 * src/hud.c — software-rendered HUD overlay.
 *
 * Uses a built-in 5x7 pixel font for all text. No external font files.
 * All drawing clips to screen bounds. Thread-safe (no globals).
 */

#include "hud.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ------------------------------------------------------------------ */
/* 5x7 bitmap font (ASCII 32..126)                                    */
/* Each glyph: 5 bytes, one per column, bits 0-6 = rows top-to-bottom */
/* ------------------------------------------------------------------ */

static const uint8_t font5x7[][5] = {
    /* 32 ' '  */ {0x00,0x00,0x00,0x00,0x00},
    /* 33 '!'  */ {0x00,0x00,0x5F,0x00,0x00},
    /* 34 '"'  */ {0x00,0x07,0x00,0x07,0x00},
    /* 35 '#'  */ {0x14,0x7F,0x14,0x7F,0x14},
    /* 36 '$'  */ {0x24,0x2A,0x7F,0x2A,0x12},
    /* 37 '%'  */ {0x23,0x13,0x08,0x64,0x62},
    /* 38 '&'  */ {0x36,0x49,0x56,0x20,0x50},
    /* 39 '\'' */ {0x00,0x08,0x07,0x03,0x00},
    /* 40 '('  */ {0x00,0x1C,0x22,0x41,0x00},
    /* 41 ')'  */ {0x00,0x41,0x22,0x1C,0x00},
    /* 42 '*'  */ {0x2A,0x1C,0x7F,0x1C,0x2A},
    /* 43 '+'  */ {0x08,0x08,0x3E,0x08,0x08},
    /* 44 ','  */ {0x00,0x80,0x70,0x30,0x00},
    /* 45 '-'  */ {0x08,0x08,0x08,0x08,0x08},
    /* 46 '.'  */ {0x00,0x60,0x60,0x00,0x00},
    /* 47 '/'  */ {0x20,0x10,0x08,0x04,0x02},
    /* 48 '0'  */ {0x3E,0x51,0x49,0x45,0x3E},
    /* 49 '1'  */ {0x00,0x42,0x7F,0x40,0x00},
    /* 50 '2'  */ {0x72,0x49,0x49,0x49,0x46},
    /* 51 '3'  */ {0x21,0x41,0x49,0x4D,0x33},
    /* 52 '4'  */ {0x18,0x14,0x12,0x7F,0x10},
    /* 53 '5'  */ {0x27,0x45,0x45,0x45,0x39},
    /* 54 '6'  */ {0x3C,0x4A,0x49,0x49,0x31},
    /* 55 '7'  */ {0x41,0x21,0x11,0x09,0x07},
    /* 56 '8'  */ {0x36,0x49,0x49,0x49,0x36},
    /* 57 '9'  */ {0x46,0x49,0x49,0x29,0x1E},
    /* 58 ':'  */ {0x00,0x36,0x36,0x00,0x00},
    /* 59 ';'  */ {0x00,0x56,0x36,0x00,0x00},
    /* 60 '<'  */ {0x08,0x14,0x22,0x41,0x00},
    /* 61 '='  */ {0x14,0x14,0x14,0x14,0x14},
    /* 62 '>'  */ {0x00,0x41,0x22,0x14,0x08},
    /* 63 '?'  */ {0x02,0x01,0x59,0x09,0x06},
    /* 64 '@'  */ {0x3E,0x41,0x5D,0x59,0x4E},
    /* 65 'A'  */ {0x7C,0x12,0x11,0x12,0x7C},
    /* 66 'B'  */ {0x7F,0x49,0x49,0x49,0x36},
    /* 67 'C'  */ {0x3E,0x41,0x41,0x41,0x22},
    /* 68 'D'  */ {0x7F,0x41,0x41,0x22,0x1C},
    /* 69 'E'  */ {0x7F,0x49,0x49,0x49,0x41},
    /* 70 'F'  */ {0x7F,0x09,0x09,0x09,0x01},
    /* 71 'G'  */ {0x3E,0x41,0x49,0x49,0x7A},
    /* 72 'H'  */ {0x7F,0x08,0x08,0x08,0x7F},
    /* 73 'I'  */ {0x00,0x41,0x7F,0x41,0x00},
    /* 74 'J'  */ {0x20,0x40,0x41,0x3F,0x01},
    /* 75 'K'  */ {0x7F,0x08,0x14,0x22,0x41},
    /* 76 'L'  */ {0x7F,0x40,0x40,0x40,0x40},
    /* 77 'M'  */ {0x7F,0x02,0x1C,0x02,0x7F},
    /* 78 'N'  */ {0x7F,0x04,0x08,0x10,0x7F},
    /* 79 'O'  */ {0x3E,0x41,0x41,0x41,0x3E},
    /* 80 'P'  */ {0x7F,0x09,0x09,0x09,0x06},
    /* 81 'Q'  */ {0x3E,0x41,0x51,0x21,0x5E},
    /* 82 'R'  */ {0x7F,0x09,0x19,0x29,0x46},
    /* 83 'S'  */ {0x46,0x49,0x49,0x49,0x31},
    /* 84 'T'  */ {0x01,0x01,0x7F,0x01,0x01},
    /* 85 'U'  */ {0x3F,0x40,0x40,0x40,0x3F},
    /* 86 'V'  */ {0x1F,0x20,0x40,0x20,0x1F},
    /* 87 'W'  */ {0x3F,0x40,0x38,0x40,0x3F},
    /* 88 'X'  */ {0x63,0x14,0x08,0x14,0x63},
    /* 89 'Y'  */ {0x07,0x08,0x70,0x08,0x07},
    /* 90 'Z'  */ {0x61,0x51,0x49,0x45,0x43},
    /* 91 '['  */ {0x00,0x7F,0x41,0x41,0x00},
    /* 92 '\\'  */ {0x02,0x04,0x08,0x10,0x20},
    /* 93 ']'  */ {0x00,0x41,0x41,0x7F,0x00},
    /* 94 '^'  */ {0x04,0x02,0x01,0x02,0x04},
    /* 95 '_'  */ {0x40,0x40,0x40,0x40,0x40},
    /* 96 '`'  */ {0x00,0x03,0x07,0x08,0x00},
    /* lowercase follows — use uppercase bitmaps for simplicity */
    /* 97..122: map to 65..90 */
    {0x7C,0x12,0x11,0x12,0x7C}, /* a */
    {0x7F,0x49,0x49,0x49,0x36}, /* b */
    {0x3E,0x41,0x41,0x41,0x22}, /* c */
    {0x7F,0x41,0x41,0x22,0x1C}, /* d */
    {0x7F,0x49,0x49,0x49,0x41}, /* e */
    {0x7F,0x09,0x09,0x09,0x01}, /* f */
    {0x3E,0x41,0x49,0x49,0x7A}, /* g */
    {0x7F,0x08,0x08,0x08,0x7F}, /* h */
    {0x00,0x41,0x7F,0x41,0x00}, /* i */
    {0x20,0x40,0x41,0x3F,0x01}, /* j */
    {0x7F,0x08,0x14,0x22,0x41}, /* k */
    {0x7F,0x40,0x40,0x40,0x40}, /* l */
    {0x7F,0x02,0x1C,0x02,0x7F}, /* m */
    {0x7F,0x04,0x08,0x10,0x7F}, /* n */
    {0x3E,0x41,0x41,0x41,0x3E}, /* o */
    {0x7F,0x09,0x09,0x09,0x06}, /* p */
    {0x3E,0x41,0x51,0x21,0x5E}, /* q */
    {0x7F,0x09,0x19,0x29,0x46}, /* r */
    {0x46,0x49,0x49,0x49,0x31}, /* s */
    {0x01,0x01,0x7F,0x01,0x01}, /* t */
    {0x3F,0x40,0x40,0x40,0x3F}, /* u */
    {0x1F,0x20,0x40,0x20,0x1F}, /* v */
    {0x3F,0x40,0x38,0x40,0x3F}, /* w */
    {0x63,0x14,0x08,0x14,0x63}, /* x */
    {0x07,0x08,0x70,0x08,0x07}, /* y */
    {0x61,0x51,0x49,0x45,0x43}, /* z */
};

static const uint8_t *glyph_for(char c) {
    int idx = (unsigned char)c - 32;
    int max = (int)(sizeof(font5x7) / sizeof(font5x7[0]));
    if (idx < 0 || idx >= max) idx = 0; /* space for unknown */
    return font5x7[idx];
}

/* ------------------------------------------------------------------ */
/* Primitives                                                          */
/* ------------------------------------------------------------------ */

void hud_draw_rect(uint32_t *pixels, int sw, int sh,
                   int x, int y, int w, int h, uint32_t color) {
    if (!pixels) return;
    int x1 = x < 0 ? 0 : x;
    int y1 = y < 0 ? 0 : y;
    int x2 = (x + w) > sw ? sw : (x + w);
    int y2 = (y + h) > sh ? sh : (y + h);
    for (int row = y1; row < y2; row++)
        for (int col = x1; col < x2; col++)
            pixels[row * sw + col] = color;
}

void hud_draw_char(uint32_t *pixels, int sw, int sh,
                   int x, int y, char c, uint32_t color, int scale) {
    if (!pixels || scale <= 0) return;
    const uint8_t *glyph = glyph_for(c);
    for (int col = 0; col < 5; col++) {
        uint8_t bits = glyph[col];
        for (int row = 0; row < 7; row++) {
            if (bits & (1 << row)) {
                int px = x + col * scale;
                int py = y + row * scale;
                hud_draw_rect(pixels, sw, sh, px, py, scale, scale, color);
            }
        }
    }
}

void hud_draw_string(uint32_t *pixels, int sw, int sh,
                     int x, int y, const char *str, uint32_t color,
                     int scale) {
    if (!pixels || !str) return;
    int cx = x;
    while (*str) {
        hud_draw_char(pixels, sw, sh, cx, y, *str, color, scale);
        cx += (5 + 1) * scale; /* 5 wide + 1 gap */
        str++;
    }
}

void hud_draw_number(uint32_t *pixels, int sw, int sh,
                     int x, int y, int number, uint32_t color, int scale) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", number);
    hud_draw_string(pixels, sw, sh, x, y, buf, color, scale);
}

/* ------------------------------------------------------------------ */
/* HUD state                                                           */
/* ------------------------------------------------------------------ */

void hud_init(HUDState *hud, int max_levels) {
    if (!hud) return;
    memset(hud, 0, sizeof(*hud));
    hud->max_levels    = max_levels;
    hud->current_level = 1;
    hud->show_crosshair = 1;
}

void hud_update(HUDState *hud) {
    if (!hud) return;
    hud->frame_count++;
    if (hud->flash_timer > 0) hud->flash_timer--;
}

void hud_set_level(HUDState *hud, int level) {
    if (!hud) return;
    hud->current_level = level;
}

void hud_set_player(HUDState *hud, float x, float y, float angle) {
    if (!hud) return;
    hud->player_x     = x;
    hud->player_y     = y;
    hud->player_angle = angle;
}

void hud_flash(HUDState *hud, const char *msg) {
    if (!hud || !msg) return;
    strncpy(hud->flash_msg, msg, HUD_MSG_MAX_LEN - 1);
    hud->flash_msg[HUD_MSG_MAX_LEN - 1] = '\0';
    hud->flash_timer = HUD_MSG_DURATION;
}

/* ------------------------------------------------------------------ */
/* Render                                                              */
/* ------------------------------------------------------------------ */

void hud_render(const HUDState *hud, uint32_t *pixels,
                int sw, int sh) {
    if (!hud || !pixels) return;

    /* ---- Level badge: top-right ---- */
    {
        char badge[32];
        snprintf(badge, sizeof(badge), "LVL %d/%d",
                 hud->current_level, hud->max_levels);
        int bw = (int)strlen(badge) * 6 * 2 + 8;
        int bh = 7 * 2 + 6;
        int bx = sw - bw - 8;
        int by = 8;
        /* Dark semi-transparent background */
        hud_draw_rect(pixels, sw, sh, bx - 2, by - 2, bw + 4, bh + 4,
                      0xAA000000u);
        hud_draw_string(pixels, sw, sh, bx, by, badge, 0xFFFFFFFFu, 2);
    }

    /* ---- Progress bar: bottom centre ---- */
    {
        int bar_w    = 200;
        int bar_h    = 10;
        int bar_x    = (sw - bar_w) / 2;
        int bar_y    = sh - bar_h - 12;
        int fill_w   = bar_w * hud->current_level / (hud->max_levels > 0
                       ? hud->max_levels : 1);

        hud_draw_rect(pixels, sw, sh, bar_x,   bar_y, bar_w, bar_h,
                      0x88222222u);
        hud_draw_rect(pixels, sw, sh, bar_x,   bar_y, fill_w, bar_h,
                      0xFF00CC55u);
        /* Border */
        hud_draw_rect(pixels, sw, sh, bar_x-1, bar_y-1, bar_w+2, 1,
                      0x88888888u);
        hud_draw_rect(pixels, sw, sh, bar_x-1, bar_y+bar_h, bar_w+2, 1,
                      0x88888888u);
    }

    /* ---- Flash message: centre ---- */
    if (hud->flash_timer > 0) {
        int slen   = 0;
        const char *p = hud->flash_msg;
        while (*p++) slen++;
        int fw = slen * 6 * 2;
        int fh = 7 * 2;
        int fx = (sw - fw) / 2;
        int fy = sh / 2 - 40;

        /* Fade alpha based on timer */
        uint8_t alpha = (uint8_t)(hud->flash_timer * 255 / HUD_MSG_DURATION);
        uint32_t bg   = (uint32_t)(alpha / 2) << 24;
        uint32_t fg   = ((uint32_t)alpha << 24) | 0x00FFEE00u;

        hud_draw_rect(pixels, sw, sh, fx - 6, fy - 4, fw + 12, fh + 8, bg);
        hud_draw_string(pixels, sw, sh, fx, fy, hud->flash_msg, fg, 2);
    }

    /* ---- Crosshair: centre ---- */
    if (hud->show_crosshair) {
        int cx = sw / 2;
        int cy = sh / 2;
        hud_draw_rect(pixels, sw, sh, cx - 8, cy - 1, 6, 2, 0xCCFFFFFFu);
        hud_draw_rect(pixels, sw, sh, cx + 3, cy - 1, 6, 2, 0xCCFFFFFFu);
        hud_draw_rect(pixels, sw, sh, cx - 1, cy - 8, 2, 6, 0xCCFFFFFFu);
        hud_draw_rect(pixels, sw, sh, cx - 1, cy + 3, 2, 6, 0xCCFFFFFFu);
    }

    /* ---- Debug overlay: top-left (below minimap) ---- */
    if (hud->show_debug) {
        char dbg[64];
        snprintf(dbg, sizeof(dbg), "X%.0f Y%.0f A%.0f F%d",
                 hud->player_x, hud->player_y,
                 hud->player_angle, hud->frame_count);
        int dy = 8 * 8 + 16; /* below minimap (8 tiles * 8px scale + pad) */
        hud_draw_rect(pixels, sw, sh, 4, dy, (int)strlen(dbg) * 6 + 4,
                      10, 0x88000000u);
        hud_draw_string(pixels, sw, sh, 6, dy + 1, dbg, 0xFFCCCCCCu, 1);
    }
}
