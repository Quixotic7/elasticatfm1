/* SPDX-License-Identifier: GPL-3.0-only */
/* The packed form three ways round: the compiled-in layout packed (C) = gen_layout.py --pack of the same JSON
 * (Python), and that text loaded (the designer's preview path) and packed again = the same text; the loaded layout
 * gives the same pages as the compiled one (ec_dump_pages' contexts, here a sample of them).
 *   ec_test_pack <shipped.pack>   (gen_layout.py design/layout/elasticat.json --pack shipped.pack) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ec_layout_pack.h"
#include "ec_params.h"

static char a[65536], b[65536], t[65536];

static int same_pages(const ec_layout_t *x, const ec_layout_t *y)
{
    float v[EC_NP];
    ec_page_t p, q;
    int m, w, c, pg, bad = 0;
    ec_params_default(v);
    for (m = 1; m <= 8; m++)
        for (w = 1; w <= 13; w++) {
            v[EC_P_MACHINE] = (float)m, v[EC_P_MODE] = (float)w, v[EC_P_FX_MACHINE] = (float)(1 + (m * w) % 18);
            for (c = 0; c < EC_NCAT; c++) {
                ec_layout_use(x);
                const int np = ec_cat_pages(c, v);
                for (pg = 0; pg < np; pg++) {
                    ec_layout_use(x);
                    ec_page_items(c, pg, v, 0, &p);
                    ec_layout_use(y);
                    ec_page_items(c, pg, v, 0, &q);
                    if (strcmp(p.title, q.title) || p.n != q.n || p.pic != q.pic ||
                        memcmp(p.it, q.it, sizeof p.it[0] * (size_t)p.n) != 0) {
                        int i, lab = 0;
                        for (i = 0; i < p.n; i++)   /* (labels are pointers: compare the text) */
                            lab |= p.it[i].kind != q.it[i].kind || p.it[i].id != q.it[i].id ||
                                   p.it[i].flags != q.it[i].flags ||
                                   strcmp(p.it[i].label ? p.it[i].label : "", q.it[i].label ? q.it[i].label : "");
                        if (lab || strcmp(p.title, q.title) || p.n != q.n || p.pic != q.pic) {
                            if (bad++ < 5)
                                printf("differs: %s page %d (mach %d warp %d)\n", ec_cat_name(c), pg, m, w);
                        }
                    }
                }
            }
        }
    for (c = 0; c < EC_NMODE; c++) {                     /* 0.6.0: the modes and their black keys, by the API */
        char a1[96], b1[96];
        int k;
        ec_layout_use(x);
        snprintf(a1, sizeof a1, "%s|%s|%d|%d", ec_mode_name(c), c < EC_NCAT ? ec_cat_tag(c) : "", ec_mode_button(c),
                 ec_mode_order(c));
        for (k = 1; k <= EC_NBKEY; k++)
            snprintf(a1 + strlen(a1), sizeof a1 - strlen(a1), "%d,%d;", ec_cat_key_page(c, k), ec_cat_key_bus(c, k));
        ec_layout_use(y);
        snprintf(b1, sizeof b1, "%s|%s|%d|%d", ec_mode_name(c), c < EC_NCAT ? ec_cat_tag(c) : "", ec_mode_button(c),
                 ec_mode_order(c));
        for (k = 1; k <= EC_NBKEY; k++)
            snprintf(b1 + strlen(b1), sizeof b1 - strlen(b1), "%d,%d;", ec_cat_key_page(c, k), ec_cat_key_bus(c, k));
        if (strcmp(a1, b1) && bad++ < 5)
            printf("differs: mode %d (%s / %s)\n", c, a1, b1);
    }
    ec_layout_use(NULL);
    return bad;
}

int main(int argc, char **argv)
{
    FILE *f;
    size_t n;
    const ec_layout_t *l;
    int fails = 0;
    if (argc < 2 || !(f = fopen(argv[1], "rb"))) {
        printf("usage: ec_test_pack <shipped.pack>\n");
        return 2;
    }
    n = fread(t, 1, sizeof t - 1, f);
    fclose(f);
    t[n] = 0;
    if (ec_layout_pack(&EC_LAYOUT_SHIPPED, a, sizeof a) < 0 || strcmp(a, t)) {
        printf("FAIL the compiled layout packed differs from gen_layout.py --pack\n");
        fails++;
    } else
        printf("ok   the compiled layout packed = gen_layout.py --pack (%zu bytes)\n", n);
    l = ec_layout_unpack(t);
    if (!l) {
        printf("FAIL the pack does not load: %s\n", ec_layout_pack_error());
        return 1;
    }
    if (ec_layout_pack(l, b, sizeof b) < 0 || strcmp(b, t)) {
        printf("FAIL loaded and packed again differs\n");
        fails++;
    } else
        printf("ok   loaded and packed again = the pack\n");
    if (same_pages(&EC_LAYOUT_SHIPPED, l)) {
        printf("FAIL the loaded layout's pages differ from the compiled one's\n");
        fails++;
    } else
        printf("ok   the loaded layout's pages, modes and black keys = the compiled one's\n");
    if (ec_layout_unpack("ECL2\tx\nC\t0\t9\t-1\t6\t1\tLOOP\tLOOP ENGINE\n")) {
        printf("FAIL a truncated pack loaded\n");
        fails++;
    } else
        printf("ok   a truncated pack is refused (%s)\n", ec_layout_pack_error());
    return fails ? 1 : 0;
}
