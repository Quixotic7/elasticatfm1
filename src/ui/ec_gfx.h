/* SPDX-License-Identifier: GPL-3.0-only */
/* Drawing (docs/UX-V2.md §7): every screen draws 4-bit palette indices (ec_palette.h, the EC_C_* slots) into one
 * static 120x120 canvas, in grid units; ec_gfx_present() expands it through the current theme to the 240x240 RGB565
 * LCD (ec_plat_lcd(): the panel's byte order, high byte first) at exactly 2x. So no detail finer than one grid unit
 * (2 physical pixels) can exist, and nothing outside the theme's 16 colours.
 * Primitives: rects, one-unit frames, lines, dotted rules, the mockup's 3x5 font at any integer scale, packed 4-bit
 * sprites, and the pop-up dim. Everything is clipped to the canvas. */
#ifndef EC_GFX_H
#define EC_GFX_H
#include <stdint.h>

#include "ec_palette.h"

/* RGB888 -> RGB565 in the LCD's byte order */
#define EC_RGB565(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
#define EC_PX(v) ((uint16_t)((((v) >> 8) & 0xFFu) | (((v) & 0xFFu) << 8)))
#define EC_RGB(r, g, b) EC_PX(EC_RGB565(r, g, b))

#define EC_U 2                                /* physical pixels per grid unit (ec_gfx.c only) */
#define EC_GW 120                             /* the grid: 120 x 120 */
#define EC_GH 120
#define EC_GFX_LCD_W (EC_GW * EC_U)
#define EC_GFX_LCD_H (EC_GH * EC_U)

enum { EC_AL_LEFT, EC_AL_CENTER, EC_AL_RIGHT };

void ec_gfx_begin(uint16_t *lcd);             /* the frame buffer ec_gfx_present() writes (ec_plat_lcd()) */
void ec_gfx_present(void);                    /* the canvas through the theme to the LCD at 2x */
uint8_t *ec_gfx_canvas(void);                 /* the 120x120 indices, row-major */

/* the themes (ec_palette.h EC_TH_*): the APP SETTINGS THEME row */
void ec_gfx_set_theme(int t);                 /* clamped to 0..EC_NTHEME-1 */
int ec_gfx_theme(void);
int ec_gfx_themes(void);                      /* EC_NTHEME */
const char *ec_gfx_theme_name(int t);         /* "CREAM" .. ; "" out of range */
uint16_t ec_gfx_lcd_colour(int theme, int c); /* slot c of a theme as the LCD stores it (EC_PX order) */

/* grid units, c = a palette slot EC_C_* */
void ec_gfx_fill(int c);
void ec_gfx_rect(int x, int y, int w, int h, int c);
void ec_gfx_frame(int x, int y, int w, int h, int c);            /* a one-unit outline */
void ec_gfx_line(int x0, int y0, int x1, int y1, int c);         /* Bresenham, one unit */
void ec_gfx_dots_h(int x0, int x1, int y, int step, int c);      /* a dotted rule of one-unit dots */
void ec_gfx_dots_v(int x, int y0, int y1, int step, int c);
void ec_gfx_pixel(int x, int y, int c);
void ec_gfx_dim(void);                        /* every slot to its darker one (ec_pal_dim): behind a pop-up */
/* a sprite of w x h palette indices packed two per byte (high nibble first, rows top-down: sprite_to_c.py --c) at
 * (x, y); key >= 0: that index is transparent */
void ec_gfx_blit4(const uint8_t *packed, int w, int h, int x, int y, int key);

/* the 3x5 font: 4 * scale units per character (one column of space), 5 * scale high. Lower case is drawn as upper
 * case; characters without a glyph are blank. Returns the x after the text. */
int ec_gfx_text(int x, int y, const char *s, int scale, int c);
int ec_gfx_text_al(int x, int y, const char *s, int scale, int c, int align);
int ec_gfx_text_w(const char *s, int scale);  /* width in units (no trailing space column) */
int ec_gfx_glyph(char c);                     /* the 15-bit glyph (rows top-down, 3 bits each, MSB left), 0 none */

#endif
