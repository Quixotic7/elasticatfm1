/* SPDX-License-Identifier: GPL-3.0-only */
/* Drawing on the 240x240 RGB565 LCD (ec_plat_lcd(): the panel's byte order, high byte first): the palette, rects,
 * lines, dotted rules and the mockup's 3x5 pixel font (design/mockup/Display.dc.html) at any integer scale.
 * Coordinates are physical pixels; the mockup's look is its 120x120 grid at scale 2 (EC_U = 2), so a screen writes
 * U(x) for a mockup coordinate and draws text at scale EC_U (the mockup's 1x). Everything is clipped. */
#ifndef EC_GFX_H
#define EC_GFX_H
#include <stdint.h>

/* RGB888 -> RGB565 in the LCD's byte order */
#define EC_RGB565(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
#define EC_PX(v) ((uint16_t)((((v) >> 8) & 0xFFu) | (((v) & 0xFFu) << 8)))
#define EC_RGB(r, g, b) EC_PX(EC_RGB565(r, g, b))

/* the palette (PLAN.md "Look"; the mockup's layer names in brackets) */
#define EC_C_BG EC_RGB(0x0B, 0x0B, 0x0A)     /* the ground [k] */
#define EC_C_L3 EC_RGB(0x34, 0x33, 0x2E)     /* rules, inactive marks [l3] */
#define EC_C_L5 EC_RGB(0x5E, 0x5C, 0x54)     /* dim content [l5] */
#define EC_C_L8 EC_RGB(0x9A, 0x97, 0x8B)     /* labels [l8] */
#define EC_C_L12 EC_RGB(0xCF, 0xCB, 0xBE)    /* the header bar, markers [l12] */
#define EC_C_L15 EC_RGB(0xF6, 0xF3, 0xE8)    /* values, active content [l15] */
#define EC_C_ACC EC_RGB(0xFF, 0x6A, 0x1F)    /* the one accent: playhead, alerts [acc] */
#define EC_C_BLACK EC_RGB(0x00, 0x00, 0x00)

#define EC_U 2                                /* the mockup grid's scale */
#define EC_GW (240 / EC_U)                    /* the mockup grid: 120 x 120 */
#define EC_GH (240 / EC_U)

enum { EC_AL_LEFT, EC_AL_CENTER, EC_AL_RIGHT };

void ec_gfx_begin(uint16_t *lcd);             /* the frame buffer to draw into (ec_plat_lcd()) */
void ec_gfx_fill(uint16_t px);
void ec_gfx_rect(int x, int y, int w, int h, uint16_t px);
void ec_gfx_frame(int x, int y, int w, int h, uint16_t px);      /* a 1-pixel outline */
void ec_gfx_line(int x0, int y0, int x1, int y1, uint16_t px);   /* Bresenham, 1 pixel */
void ec_gfx_dots_h(int x0, int x1, int y, int step, int size, uint16_t px);   /* a dotted rule, size x size dots */
void ec_gfx_dots_v(int x, int y0, int y1, int step, int size, uint16_t px);
void ec_gfx_pixel(int x, int y, uint16_t px);

/* the 3x5 font: 4 * scale pixels per character (one column of space), 5 * scale high. Lower case is drawn as upper
 * case; characters without a glyph are blank. Returns the x after the text. */
int ec_gfx_text(int x, int y, const char *s, int scale, uint16_t px);
int ec_gfx_text_al(int x, int y, const char *s, int scale, uint16_t px, int align);
int ec_gfx_text_w(const char *s, int scale);  /* width in pixels (no trailing space column) */
int ec_gfx_glyph(char c);                     /* the 15-bit glyph (rows top-down, 3 bits each, MSB left), 0 none */

#endif
