/* SPDX-License-Identifier: GPL-3.0-only */
/* A tiny PNG writer (8-bit RGB, stored deflate blocks: no zlib needed) for the harness's screenshots */
#ifndef EC_PNG_H
#define EC_PNG_H
#include <stdint.h>

/* the LCD (RGB565, high byte first in memory, as the panel holds it) as a PNG, each pixel scale x scale; 0 ok */
int ec_png_write_lcd(const char *path, const uint16_t *lcd_be, int w, int h, int scale);

#endif
