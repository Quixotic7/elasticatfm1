/* SPDX-License-Identifier: GPL-3.0-only */
/* The layout's packed text form (tools/layout/README.md): what tools/layout/gen_layout.py --pack and the designer
 * (layout_compile.js pack()) write, read here into a layout the firmware's own ec_pages.c then uses (the designer's
 * preview), and written back from any layout (the compiled one too: the tests compare the three). Preview and tests
 * only: the firmware never loads a layout.
 *
 *   ECL2 <tab> name
 *   C  page0 npages setup button order tag name   x EC_NCAT (EC_CAT_* order; 0.6.0: the mode)
 *   S  button order name                       x EC_NSCREEN (the fixed screens' modes)
 *   B  bus_page[3] bus_setup[3] mst_src f3 bus_key[3]
 *   P  pic arg key var0 nvar title             per page
 *   V  mach warp filt fenv env fx item0 n title   per variant (title empty: the page's)
 *   I  kind id flags label                     per item (label empty: NULL) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ec_layout_pack.h"

#define MAXP 256
#define MAXV 1024
#define MAXI 4096
#define POOL 32768

static ec_layout_t g_lay;
static ec_lay_page_t g_page[MAXP];
static ec_lay_var_t g_var[MAXV];
static ec_item_t g_item[MAXI];
static char g_pool[POOL];
static int g_pool_n;
static char g_err[160];

const char *ec_layout_pack_error(void) { return g_err; }

static const char *keep(const char *s, size_t n, int null_if_empty)
{
    char *p;
    if (!n && null_if_empty)
        return NULL;
    if (g_pool_n + (int)n + 1 > POOL)
        return NULL;
    p = g_pool + g_pool_n;
    memcpy(p, s, n);
    p[n] = 0;
    g_pool_n += (int)n + 1;
    return p;
}

/* the line's fields (tab separated, the last one the rest of the line) */
static int fields(const char *l, size_t len, const char **f, size_t *fl, int max)
{
    int n = 0;
    size_t i = 0, s = 0;
    while (n < max) {
        if (n == max - 1) {
            f[n] = l + s, fl[n] = len - s, n++;
            break;
        }
        for (i = s; i < len && l[i] != '\t'; i++)
            ;
        f[n] = l + s, fl[n] = i - s, n++;
        if (i >= len)
            break;
        s = i + 1;
    }
    return n;
}

static long num(const char *s, size_t n) { char b[24]; snprintf(b, sizeof b, "%.*s", (int)(n < 23 ? n : 23), s); return strtol(b, 0, 10); }

#define FAIL(...) do { snprintf(g_err, sizeof g_err, __VA_ARGS__); return NULL; } while (0)

const ec_layout_t *ec_layout_unpack(const char *text)
{
    const char *p = text;
    int np = 0, nv = 0, ni = 0, nc = 0, ns = 0, line = 0, hb = 0, i;
    memset(&g_lay, 0, sizeof g_lay);
    g_pool_n = 0;
    g_err[0] = 0;
    while (*p) {
        const char *e = strchr(p, '\n');
        const size_t len = e ? (size_t)(e - p) : strlen(p);
        const char *f[12];
        size_t fl[12];
        int n;
        line++;
        if (!len) {
            p += 1;
            continue;
        }
        switch (p[0]) {
        case 'E':
            n = fields(p, len, f, fl, 2);
            if (n < 1 || fl[0] != 4 || memcmp(f[0], "ECL2", 4))
                FAIL("line %d: not ECL2", line);
            g_lay.name = keep(n > 1 ? f[1] : "", n > 1 ? fl[1] : 0, 0);
            break;
        case 'C':
            n = fields(p, len, f, fl, 8);
            if (n != 8 || nc >= EC_NCAT)
                FAIL("line %d: C", line);
            g_lay.cat[nc].page0 = (uint16_t)num(f[1], fl[1]);
            g_lay.cat[nc].npages = (uint8_t)num(f[2], fl[2]);
            g_lay.cat[nc].setup = (int16_t)num(f[3], fl[3]);
            g_lay.cat[nc].button = (uint8_t)num(f[4], fl[4]);
            g_lay.cat[nc].order = (uint8_t)num(f[5], fl[5]);
            g_lay.cat[nc].tag = keep(f[6], fl[6], 0);
            g_lay.cat[nc].name = keep(f[7], fl[7], 0);
            nc++;
            break;
        case 'S':
            n = fields(p, len, f, fl, 4);
            if (n != 4 || ns >= EC_NSCREEN)
                FAIL("line %d: S", line);
            g_lay.screen[ns].button = (uint8_t)num(f[1], fl[1]);
            g_lay.screen[ns].order = (uint8_t)num(f[2], fl[2]);
            g_lay.screen[ns].name = keep(f[3], fl[3], 0);
            ns++;
            break;
        case 'B':
            n = fields(p, len, f, fl, 12);
            if (n != 12)
                FAIL("line %d: B", line);
            for (i = 0; i < 3; i++) {
                g_lay.bus_page[i] = (int16_t)num(f[1 + i], fl[1 + i]), g_lay.bus_setup[i] = (int16_t)num(f[4 + i], fl[4 + i]);
                g_lay.bus_key[i] = (uint8_t)num(f[9 + i], fl[9 + i]);
            }
            g_lay.mst_src = (int16_t)num(f[7], fl[7]);
            g_lay.f3 = (int16_t)num(f[8], fl[8]);
            hb = 1;
            break;
        case 'P':
            n = fields(p, len, f, fl, 7);
            if (n != 7 || np >= MAXP)
                FAIL("line %d: P", line);
            g_page[np].pic = (uint8_t)num(f[1], fl[1]);
            g_page[np].arg = (uint8_t)num(f[2], fl[2]);
            g_page[np].key = (uint8_t)num(f[3], fl[3]);
            g_page[np].var0 = (uint16_t)num(f[4], fl[4]);
            g_page[np].nvar = (uint8_t)num(f[5], fl[5]);
            g_page[np].title = keep(f[6], fl[6], 0);
            np++;
            break;
        case 'V':
            n = fields(p, len, f, fl, 10);
            if (n != 10 || nv >= MAXV)
                FAIL("line %d: V", line);
            g_var[nv].mach = (uint8_t)num(f[1], fl[1]);
            g_var[nv].warp = (uint32_t)num(f[2], fl[2]);
            g_var[nv].filt = (uint8_t)num(f[3], fl[3]);
            g_var[nv].fenv = (uint8_t)num(f[4], fl[4]);
            g_var[nv].env = (uint8_t)num(f[5], fl[5]);
            g_var[nv].fx = (uint32_t)num(f[6], fl[6]);
            g_var[nv].item0 = (uint16_t)num(f[7], fl[7]);
            g_var[nv].n = (uint8_t)num(f[8], fl[8]);
            g_var[nv].title = keep(f[9], fl[9], 1);
            nv++;
            break;
        case 'I':
            n = fields(p, len, f, fl, 5);
            if (n != 5 || ni >= MAXI)
                FAIL("line %d: I", line);
            g_item[ni].kind = (uint8_t)num(f[1], fl[1]);
            g_item[ni].id = (uint8_t)num(f[2], fl[2]);
            g_item[ni].flags = (uint8_t)num(f[3], fl[3]);
            g_item[ni].label = keep(f[4], fl[4], 1);
            ni++;
            break;
        default: FAIL("line %d: unknown record %c", line, p[0]);
        }
        p += len + (e ? 1 : 0);
    }
    if (nc != EC_NCAT || ns != EC_NSCREEN || !hb || !np)
        FAIL("incomplete (%d categories, %d screens, %d pages)", nc, ns, np);
    /* every index in range: a bad pack must not crash the preview */
    for (i = 0; i < EC_NCAT; i++)
        if (!g_lay.cat[i].npages || g_lay.cat[i].page0 + g_lay.cat[i].npages > np || g_lay.cat[i].setup >= np ||
            g_lay.cat[i].button >= EC_NMB || !g_lay.cat[i].name || !g_lay.cat[i].tag)
            FAIL("category %d out of range", i);
    for (i = 0; i < EC_NSCREEN; i++)
        if (g_lay.screen[i].button >= EC_NMB || !g_lay.screen[i].name)
            FAIL("screen %d out of range", i);
    for (i = 0; i < 3; i++)
        if (g_lay.bus_page[i] < 0 || g_lay.bus_page[i] >= np || g_lay.bus_setup[i] >= np)
            FAIL("bus %d out of range", i);
    if (g_lay.mst_src >= np || g_lay.f3 < 0 || g_lay.f3 >= np)
        FAIL("mst_src / f3 out of range");
    for (i = 0; i < np; i++)
        if (!g_page[i].nvar || g_page[i].var0 + g_page[i].nvar > nv || g_page[i].pic >= EC_NPIC ||
            g_page[i].key > EC_NBKEY)
            FAIL("page %d out of range", i);
    for (i = 0; i < nv; i++)
        if (g_var[i].item0 + g_var[i].n > ni || g_var[i].n > EC_PAGE_ITEMS)
            FAIL("variant %d out of range", i);
    for (i = 0; i < ni; i++)
        if (g_item[i].kind > EC_IT_MTX)
            FAIL("item %d: kind", i);
    g_lay.page = g_page, g_lay.var = g_var, g_lay.item = g_item;
    g_lay.npage = (uint16_t)np, g_lay.nvar = (uint16_t)nv, g_lay.nitem = (uint16_t)ni;
    return &g_lay;
}

int ec_layout_pack(const ec_layout_t *l, char *out, int cap)
{
    int n = 0, i;
#define W(...) do { if (n < cap) n += snprintf(out + n, (size_t)(cap - n), __VA_ARGS__); } while (0)
    W("ECL2\t%s\n", l->name ? l->name : "");
    for (i = 0; i < EC_NCAT; i++)
        W("C\t%d\t%d\t%d\t%d\t%d\t%s\t%s\n", l->cat[i].page0, l->cat[i].npages, l->cat[i].setup, l->cat[i].button,
          l->cat[i].order, l->cat[i].tag ? l->cat[i].tag : "", l->cat[i].name ? l->cat[i].name : "");
    for (i = 0; i < EC_NSCREEN; i++)
        W("S\t%d\t%d\t%s\n", l->screen[i].button, l->screen[i].order, l->screen[i].name ? l->screen[i].name : "");
    W("B\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n", l->bus_page[0], l->bus_page[1], l->bus_page[2],
      l->bus_setup[0], l->bus_setup[1], l->bus_setup[2], l->mst_src, l->f3, l->bus_key[0], l->bus_key[1],
      l->bus_key[2]);
    for (i = 0; i < l->npage; i++)
        W("P\t%d\t%d\t%d\t%d\t%d\t%s\n", l->page[i].pic, l->page[i].arg, l->page[i].key, l->page[i].var0,
          l->page[i].nvar, l->page[i].title);
    for (i = 0; i < l->nvar; i++) {
        const ec_lay_var_t *v = &l->var[i];
        W("V\t%d\t%lu\t%d\t%d\t%d\t%lu\t%d\t%d\t%s\n", v->mach, (unsigned long)v->warp, v->filt, v->fenv, v->env,
          (unsigned long)v->fx, v->item0, v->n, v->title ? v->title : "");
    }
    for (i = 0; i < l->nitem; i++)
        W("I\t%d\t%d\t%d\t%s\n", l->item[i].kind, l->item[i].id, l->item[i].flags,
          l->item[i].label ? l->item[i].label : "");
#undef W
    return n < cap ? n : -1;
}
