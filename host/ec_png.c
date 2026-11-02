/* SPDX-License-Identifier: GPL-3.0-only */
/* The PNG writer (ec_png.h) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ec_png.h"

static uint32_t g_crc[256];

static void crc_init(void)
{
    uint32_t n, c, k;
    for (n = 0; n < 256; n++) {
        c = n;
        for (k = 0; k < 8; k++)
            c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        g_crc[n] = c;
    }
}
static uint32_t crc(uint32_t c, const uint8_t *p, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++)
        c = g_crc[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    return c;
}
static void be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24), p[1] = (uint8_t)(v >> 16), p[2] = (uint8_t)(v >> 8), p[3] = (uint8_t)v;
}
static void chunk(FILE *f, const char *type, const uint8_t *data, uint32_t n)
{
    uint8_t h[8], t[4];
    uint32_t c;
    be32(h, n);
    memcpy(h + 4, type, 4);
    fwrite(h, 1, 8, f);
    if (n)
        fwrite(data, 1, n, f);
    c = crc(0xFFFFFFFFu, (const uint8_t *)type, 4);
    c = crc(c, data, n) ^ 0xFFFFFFFFu;
    be32(t, c);
    fwrite(t, 1, 4, f);
}

int ec_png_write_lcd(const char *path, const uint16_t *lcd_be, int w, int h, int scale)
{
    const int W = w * scale, H = h * scale;
    const size_t raw_n = (size_t)H * (1 + (size_t)W * 3);
    const size_t nblk = (raw_n + 65534) / 65535;
    uint8_t *raw = malloc(raw_n), *z = malloc(raw_n + nblk * 5 + 6);
    uint8_t ihdr[13];
    size_t at = 0, zn = 0, i;
    uint32_t a = 1, b = 0;
    FILE *f;
    int x, y;
    if (!raw || !z)
        return -1;
    crc_init();
    for (y = 0; y < H; y++) {
        raw[at++] = 0;                          /* filter: none */
        for (x = 0; x < W; x++) {
            const uint8_t *px = (const uint8_t *)(lcd_be + (y / scale) * w + (x / scale));
            const unsigned v = (unsigned)px[0] << 8 | px[1];
            raw[at++] = (uint8_t)((v >> 11) * 255 / 31);
            raw[at++] = (uint8_t)(((v >> 5) & 63) * 255 / 63);
            raw[at++] = (uint8_t)((v & 31) * 255 / 31);
        }
    }
    z[zn++] = 0x78, z[zn++] = 0x01;             /* zlib: deflate, no dictionary */
    for (i = 0; i < raw_n; i += 65535) {
        const size_t n = raw_n - i < 65535 ? raw_n - i : 65535;
        z[zn++] = i + n >= raw_n ? 1 : 0;       /* a stored block, final at the end */
        z[zn++] = (uint8_t)n, z[zn++] = (uint8_t)(n >> 8);
        z[zn++] = (uint8_t)~n, z[zn++] = (uint8_t)(~n >> 8);
        memcpy(z + zn, raw + i, n);
        zn += n;
    }
    for (i = 0; i < raw_n; i++) {
        a = (a + raw[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    be32(z + zn, b << 16 | a);
    zn += 4;
    f = fopen(path, "wb");
    if (!f) {
        free(raw), free(z);
        return -1;
    }
    fwrite("\x89PNG\r\n\x1a\n", 1, 8, f);
    be32(ihdr, (uint32_t)W);
    be32(ihdr + 4, (uint32_t)H);
    ihdr[8] = 8, ihdr[9] = 2, ihdr[10] = 0, ihdr[11] = 0, ihdr[12] = 0;
    chunk(f, "IHDR", ihdr, 13);
    chunk(f, "IDAT", z, (uint32_t)zn);
    chunk(f, "IEND", NULL, 0);
    fclose(f);
    free(raw), free(z);
    return 0;
}
