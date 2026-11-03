/* SPDX-License-Identifier: GPL-3.0-only */
/* The 120x120 indexed canvas, its primitives, the themes and the 3x5 pixel font (ec_gfx.h) */
#include <ctype.h>
#include <string.h>

#include "ec_gfx.h"

#define W EC_GW
#define H EC_GH

static uint8_t g_cv[W * H];
#ifdef EC_GFX_TEXT_MASK
/* test builds: which canvas units the last drawing made text or a sprite (1: the 3x5 glyphs' strokes are 1 unit at
 * scale 1 by design, the sprites are the owner's norns art as it is) and which anything else (0), for test_grid's
 * hairline check */
static uint8_t g_txt[W * H];
static int g_in_text;
const uint8_t *ec_gfx_text_mask(void) { return g_txt; }
#define MARK(i, n) memset(g_txt + (i), g_in_text, (size_t)(n))
#else
#define MARK(i, n) ((void)0)
#endif
#ifdef EC_GFX_WHO
/* debugging (-DEC_GFX_WHO): the outermost drawing call's return address per canvas unit (host/test_grid.c dumps it) */
static void *g_who[W * H];
static void *g_cur;
static int g_depth;
void *const *ec_gfx_who(void) { return g_who; }
#define ENTER() (g_depth++ ? (void)0 : (void)(g_cur = __builtin_return_address(0)))
#define LEAVE() ((void)g_depth--)
#define WHO(i, n) do { int _k; for (_k = 0; _k < (n); _k++) g_who[(i) + _k] = g_cur; } while (0)
#else
#define ENTER() ((void)0)
#define LEAVE() ((void)0)
#define WHO(i, n) ((void)0)
#endif
static uint16_t *g_fb;
static int g_theme = EC_TH_CREAM;

/* The glyphs of design/mockup/Display.dc.html (its table G), ASCII 32..95: 5 rows of 3 bits, top row first, the left
 * pixel the high bit of its row. "(added)": glyphs the mockup has no entry for, drawn in its style. */
static const uint16_t FONT[64] = {
    0x0000, /* ' ' */
    0x2482, /* '!' (added) */
    0x0000, /* '"' (none: blank) */
    0x5F7D, /* '#' (added) */
    0x0000, /* '$' (none: blank) */
    0x52A5, /* '%' */
    0x2AAB, /* '&' (added) */
    0x2400, /* ''' (added) */
    0x1491, /* '(' (added) */
    0x4494, /* ')' (added) */
    0x0AA8, /* '*' (added) */
    0x05D0, /* '+' */
    0x0014, /* ',' (added) */
    0x01C0, /* '-' */
    0x0002, /* '.' */
    0x12A4, /* '/' */
    0x7B6F, /* '0' */
    0x2C97, /* '1' */
    0x73E7, /* '2' */
    0x72CF, /* '3' */
    0x5BC9, /* '4' */
    0x79CF, /* '5' */
    0x79EF, /* '6' */
    0x7292, /* '7' */
    0x7BEF, /* '8' */
    0x7BCF, /* '9' */
    0x0410, /* ':' */
    0x0000, /* ';' (none: blank) */
    0x1511, /* '<' */
    0x0E38, /* '=' */
    0x4454, /* '>' */
    0x6282, /* '?' (added) */
    0x0000, /* '@' (none: blank) */
    0x2BED, /* 'A' */
    0x6BAE, /* 'B' */
    0x3923, /* 'C' */
    0x6B6E, /* 'D' */
    0x79A7, /* 'E' */
    0x79A4, /* 'F' */
    0x396B, /* 'G' */
    0x5BED, /* 'H' */
    0x7497, /* 'I' */
    0x126A, /* 'J' */
    0x5BAD, /* 'K' */
    0x4927, /* 'L' */
    0x5FED, /* 'M' */
    0x6B6D, /* 'N' */
    0x2B6A, /* 'O' */
    0x6BA4, /* 'P' */
    0x2B73, /* 'Q' */
    0x6BAD, /* 'R' */
    0x388E, /* 'S' */
    0x7492, /* 'T' */
    0x5B6F, /* 'U' */
    0x5B6A, /* 'V' */
    0x5BFD, /* 'W' */
    0x5AAD, /* 'X' */
    0x5A92, /* 'Y' */
    0x72A7, /* 'Z' */
    0x3493, /* '[' (added) */
    0x0000, /* '\' (none: blank) */
    0x6496, /* ']' (added) */
    0x2A00, /* '^' (added) */
    0x0007, /* '_' */
};

int ec_gfx_glyph(char c)
{
    int u = toupper((unsigned char)c);
    return u >= 32 && u < 96 ? FONT[u - 32] : 0;
}

void ec_gfx_begin(uint16_t *lcd) { g_fb = lcd; }
uint8_t *ec_gfx_canvas(void) { return g_cv; }

void ec_gfx_set_theme(int t) { g_theme = t < 0 ? 0 : t >= EC_NTHEME ? EC_NTHEME - 1 : t; }
int ec_gfx_theme(void) { return g_theme; }
int ec_gfx_themes(void) { return EC_NTHEME; }
const char *ec_gfx_theme_name(int t) { return t >= 0 && t < EC_NTHEME ? ec_theme_names[t] : ""; }

uint16_t ec_gfx_lcd_colour(int theme, int c)
{
    const uint32_t v = ec_themes[theme < 0 || theme >= EC_NTHEME ? 0 : theme][c & 15];
    return EC_RGB((v >> 16) & 255u, (v >> 8) & 255u, v & 255u);
}

void ec_gfx_present(void)
{
    uint16_t lut[EC_NPAL];
    int x, y, i;
    if (!g_fb)
        return;
    for (i = 0; i < EC_NPAL; i++)
        lut[i] = ec_gfx_lcd_colour(g_theme, i);
    for (y = 0; y < H; y++) {
        const uint8_t *src = g_cv + y * W;
        uint16_t *r0 = g_fb + (2 * y) * EC_GFX_LCD_W, *r1 = r0 + EC_GFX_LCD_W;
        for (x = 0; x < W; x++) {
            const uint16_t px = lut[src[x] & 15];
            r0[2 * x] = r0[2 * x + 1] = r1[2 * x] = r1[2 * x + 1] = px;
        }
    }
}

void ec_gfx_rect(int x, int y, int w, int h, int c)
{
    int j;
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > W)
        w = W - x;
    if (y + h > H)
        h = H - y;
    if (w <= 0 || h <= 0)
        return;
    ENTER();
    for (j = 0; j < h; j++) {
        memset(g_cv + (y + j) * W + x, c & 15, (size_t)w);
        MARK((y + j) * W + x, w);
        WHO((y + j) * W + x, w);
    }
    LEAVE();
}

void ec_gfx_fill(int c)
{
    memset(g_cv, c & 15, sizeof g_cv);
    MARK(0, W * H);
}

void ec_gfx_pixel(int x, int y, int c)
{
    ENTER();
    if (x >= 0 && y >= 0 && x < W && y < H) {
        g_cv[y * W + x] = (uint8_t)(c & 15);
        MARK(y * W + x, 1);
        WHO(y * W + x, 1);
    }
    LEAVE();
}

/* a 2x2 block at (x, y): the brush of lines and dots (no detail finer than 2 units, docs/UX-V2.md §7 / §10) */
static void dot2(int x, int y, int c) { ec_gfx_rect(x, y, 2, 2, c); }

void ec_gfx_frame(int x, int y, int w, int h, int c)
{
    ENTER();
    ec_gfx_rect(x, y, w, 2, c);
    ec_gfx_rect(x, y + h - 2, w, 2, c);
    ec_gfx_rect(x, y, 2, h, c);
    ec_gfx_rect(x + w - 2, y, 2, h, c);
    LEAVE();
}

void ec_gfx_line(int x0, int y0, int x1, int y1, int c)
{
    const int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    const int dy = -(y1 > y0 ? y1 - y0 : y0 - y1), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    ENTER();
    for (;;) {
        int e2;
        dot2(x0, y0, c);
        if (x0 == x1 && y0 == y1)
            break;
        e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
    LEAVE();
}

void ec_gfx_dots_h(int x0, int x1, int y, int step, int c)
{
    int x;
    ENTER();
    for (x = x0; x + 1 < x1; x += step > 4 ? step : 4)
        dot2(x, y, c);
    LEAVE();
}
void ec_gfx_dots_v(int x, int y0, int y1, int step, int c)
{
    int y;
    ENTER();
    for (y = y0; y + 1 < y1; y += step > 4 ? step : 4)
        dot2(x, y, c);
    LEAVE();
}

void ec_gfx_dim(void)
{
    int i;
    for (i = 0; i < W * H; i++)
        g_cv[i] = ec_pal_dim[g_cv[i] & 15];
}

void ec_gfx_blit4(const uint8_t *packed, int w, int h, int x, int y, int key)
{
    int i, j;
    ENTER();
#ifdef EC_GFX_TEXT_MASK
    g_in_text = 1;
#endif
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++) {
            const int k = j * w + i, b = packed[k >> 1], v = k & 1 ? b & 15 : b >> 4;
            if (v != key)
                ec_gfx_pixel(x + i, y + j, v);
        }
#ifdef EC_GFX_TEXT_MASK
    g_in_text = 0;
#endif
    LEAVE();
}

int ec_gfx_text(int x, int y, const char *s, int scale, int c)
{
#ifdef EC_GFX_TEXT_MASK
    g_in_text = 1;
#endif
    ENTER();
    for (; *s; s++, x += 4 * scale) {
        const int g = ec_gfx_glyph(*s);
        int r, k;
        if (!g)
            continue;
        for (r = 0; r < 5; r++)
            for (k = 0; k < 3; k++)
                if (g >> (14 - (r * 3 + k)) & 1)
                    ec_gfx_rect(x + k * scale, y + r * scale, scale, scale, c);
    }
    LEAVE();
#ifdef EC_GFX_TEXT_MASK
    g_in_text = 0;
#endif
    return x;
}

int ec_gfx_text_w(const char *s, int scale)
{
    const int n = (int)strlen(s);
    return n ? n * 4 * scale - scale : 0;
}

int ec_gfx_text_al(int x, int y, const char *s, int scale, int c, int align)
{
    const int w = ec_gfx_text_w(s, scale);
    const int x0 = align == EC_AL_RIGHT ? x - w : align == EC_AL_CENTER ? x - w / 2 : x;
    return ec_gfx_text(x0, y, s, scale, c);
}
