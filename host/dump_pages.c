/* SPDX-License-Identifier: GPL-3.0-only */
/* ec_dump_pages: every page of every category, its setup, the bus pages and F3, for every context that can change
 * them (track machine x warp mode x filter machine x filter env mode x amp env mode x insert FX machine; each bus with
 * each FX machine), through the public ec_pages API. Prints a hash over all of them, and with --list the distinct
 * pages with the first context that shows each. Used to prove that a layout reproduces another (the 0.4.0 pages:
 * host/fixtures/pages-0.4.0.txt), whatever code builds them.
 * --ncat N hashes the first N categories only: 0.4.0 had nine (0.6.0 added XFD, CROSSFADER, which on the 0.4.0 layout
 * shows MST's SCENE page again), so `--layout test --ncat 9` still gives 0.4.0's hash.
 *   ec_dump_pages [--list] [--layout test|shipped] [--ncat N] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef OLD_PAGES
#include "ec_layout_test.h"
#endif
#include "ec_pages.h"
#include "ec_params.h"

/* the categories by their ids (not ec_cat_name: 0.6.0 named them after the modes, and the hash must not depend on
 * the names the layout gives them) */
static const char *const CID[] = {"TRIG", "SRC", "FILT", "AMP", "FX", "MOD", "MST", "FILE", "PAT", "XFD"};
#define cid(c) ((c) < (int)(sizeof CID / sizeof CID[0]) ? CID[c] : "?")

static unsigned long long H = 1469598103934665603ULL;
static void hs(const char *s)
{
    for (; *s; s++)
        H = (H ^ (unsigned char)*s) * 1099511628211ULL;
    H = (H ^ 0xFFu) * 1099511628211ULL;
}

/* the picture the screen draws for the page (what the screens decide it by: 0.4.0 by category / page / kind, since
 * the layout by the page's own picture) */
static const char *const PICN[] = {"steps", "wave", "wave-machine", "wave-warp", "wave-range", "filter", "fenv",
                                   "aenv", "fx", "sends", "lfo1", "lfo2", "menv", "vel", "macros", "macro", "mix",
                                   "scene", "file", "razor", "pattern"};
#ifdef OLD_PAGES   /* (built against 8c762e2, 0.4.0's hand-written ec_pages.c, to record host/fixtures/pages-0.4.0.hash:
                      cc -DOLD_PAGES host/dump_pages.c build/libelasticat.a ...) */
static int CUR_CAT, CUR_PAGE, CUR_BUS;
static const char *picname(const ec_page_t *o, int *arg)
{
    static const char *const PV[] = {"", "fx", "sends", "lfo1", "lfo2", "menv", "vel", "macros", "macro"};
    *arg = o->kind == EC_PV_MACRO ? CUR_PAGE - 5 : 0;
    if (!strcmp(o->title, "SETUP"))
        return "setup";
    if (!strcmp(o->title, "F3"))
        return "steps";
    if (!strcmp(o->title, "MIX"))
        return "mix";
    if (o->kind)
        return PV[o->kind];
    if (CUR_CAT == EC_CAT_SRC && !CUR_BUS)
        return PICN[1 + CUR_PAGE];
    if (CUR_CAT == EC_CAT_FILE)
        return CUR_PAGE == 1 ? "razor" : "file";
    if (CUR_CAT == EC_CAT_PAT)
        return "pattern";
    if (CUR_CAT == EC_CAT_MASTER && CUR_PAGE == 2)
        return "scene";
    if (CUR_CAT == EC_CAT_FILT && CUR_PAGE < 2)
        return CUR_PAGE ? "fenv" : "filter";
    if (CUR_CAT == EC_CAT_AMP)
        return "aenv";
    return "steps";
}
#define SETCUR(c, p, b) (CUR_CAT = (c), CUR_PAGE = (p), CUR_BUS = (b))
#else
static const char *picname(const ec_page_t *o, int *arg)
{
    *arg = o->arg;
    if (!strcmp(o->title, "SETUP"))
        return "setup";
    return o->pic >= 0 && o->pic < (int)(sizeof PICN / sizeof PICN[0]) ? PICN[o->pic] : "?";
}
#define SETCUR(c, p, b) ((void)0)
#endif

static void fmt(const ec_page_t *o, char *b, size_t cap)
{
    int i, l, arg;
    const char *pn = picname(o, &arg);
    l = snprintf(b, cap, "\"%s\" n%d %s:%d bus%d |", o->title, o->n, pn, arg, o->busv);
    for (i = 0; i < o->n && l < (int)cap; i++)
        l += snprintf(b + l, cap - (size_t)l, " %d:%d:%d:%s", o->it[i].kind, o->it[i].id, o->it[i].flags,
                      o->it[i].label ? o->it[i].label : "-");
}

#define MAXD 4096
static char seen[MAXD][320];
static int nseen;
static int list;
static int ncat = EC_NCAT;

static void out(const char *where, const char *ctx, const ec_page_t *o)
{
    char b[320], k[400];
    int i;
    fmt(o, b, sizeof b);
    snprintf(k, sizeof k, "%s %s", where, b);
    hs(where);
    hs(b);
    if (!list)
        return;
    for (i = 0; i < nseen; i++)
        if (!strcmp(seen[i], k))
            return;
    if (nseen < MAXD) {
        snprintf(seen[nseen++], sizeof seen[0], "%s", k);
        printf("%s [%s] %s\n", where, ctx, b);
    }
}

int main(int argc, char **argv)
{
    float v[EC_NP], bv[EC_NP];
    ec_page_t o;
    int a, m, w, f, fe, e, x, c, p, b;
    char where[64], ctx[96];
    for (a = 1; a < argc; a++) {
        if (!strcmp(argv[a], "--list"))
            list = 1;
        else if (!strcmp(argv[a], "--ncat") && a + 1 < argc) {
            ncat = atoi(argv[++a]);
            if (ncat < 1 || ncat > EC_NCAT)
                ncat = EC_NCAT;
        }
#ifndef OLD_PAGES
        else if (!strcmp(argv[a], "--layout") && a + 1 < argc) {
            a++;
            if (strcmp(argv[a], "test") && strcmp(argv[a], "shipped")) {
                fprintf(stderr, "ec_dump_pages: no layout %s (test, shipped)\n", argv[a]);
                return 2;
            }
            ec_layout_use(!strcmp(argv[a], "test") ? &EC_LAYOUT_TEST : NULL);
        }
#endif
    }
    ec_params_default(v);
    for (m = 1; m <= 8; m++)
        for (w = 1; w <= 13; w++)
            for (f = 1; f <= 6; f++)
                for (fe = 1; fe <= 2; fe++)
                    for (e = 1; e <= 2; e++)
                        for (x = 1; x <= 18; x++) {
                            v[EC_P_MACHINE] = (float)m, v[EC_P_MODE] = (float)w, v[EC_P_FILTER_MACHINE] = (float)f;
                            v[EC_P_FENV_MODE] = (float)fe, v[EC_P_ENV_MODE] = (float)e, v[EC_P_FX_MACHINE] = (float)x;
                            snprintf(ctx, sizeof ctx, "mach %d warp %d filt %d fenv %d env %d fx %d", m, w, f, fe, e, x);
                            for (c = 0; c < ncat; c++) {
                                const int np = ec_cat_pages(c, v);
                                snprintf(where, sizeof where, "%s pages %d", cid(c), np);
                                hs(where);
                                for (p = 0; p < np; p++) {
                                    SETCUR(c, p, 0);
                                    ec_page_items_bus(c, p, v, 0, 0, &o);
                                    snprintf(where, sizeof where, "%s/%d", cid(c), p);
                                    out(where, ctx, &o);
                                }
                                if (ec_settings_items(c, v, &o)) {
                                    snprintf(where, sizeof where, "%s/SETUP", cid(c));
                                    out(where, ctx, &o);
                                }
                            }
                        }
    ec_params_default(v);
    ec_params_default(bv);
    for (b = 1; b <= 3; b++)
        for (x = 1; x <= 18; x++) {
            bv[EC_P_FX_MACHINE] = (float)x;
            snprintf(ctx, sizeof ctx, "bus %d fx %d", b, x);
            for (c = 0; c < ncat; c++) {
                const int np = ec_cat_pages(c, v);
                for (p = 0; p < np; p++) {
                    SETCUR(c, p, b);
                    ec_page_items_bus(c, p, v, b, bv, &o);
                    snprintf(where, sizeof where, "B%d %s/%d", b, cid(c), p);
                    out(where, ctx, &o);
                }
                if (ec_settings_items_bus(c, b, &o)) {
                    snprintf(where, sizeof where, "B%d %s/SETUP", b, cid(c));
                    out(where, ctx, &o);
                }
            }
        }
    ec_page_f3(&o);
    out("F3", "-", &o);
    printf("pages hash %016llx\n", H);
    return 0;
}
