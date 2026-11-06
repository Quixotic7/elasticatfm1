/* SPDX-License-Identifier: GPL-3.0-only */
/* E5's screens (ec_scr_e5.h; 0.3.0: the PAGE overlay became F3, src/ui/ec_scr_ux.c), on the 120x120 grid in palette slots (ec_gfx.h). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ec_gfx.h"
#include "ec_scr_e5.h"
#include "ec_ui_kit.h"

#define W EC_GW
#define H EC_GH
#define R(c, x, y, w, h) ec_gfx_rect((x), (y), (w), (h), (c))
#define F(c, x, y, w, h) ec_gfx_frame((x), (y), (w), (h), (c))
#define T(c, s, x, y, al) ec_gfx_text_al((x), (y), (s), 1, (c), (al))
#define T2(c, s, x, y, al) ec_gfx_text_al((x), (y), (s), 2, (c), (al))
#define TW(s) (ec_gfx_text_w((s), 1))

static int iv(const float *v, int id) { return (int)floorf(v[id] + 0.5f); }
static const char *const TN[EC_NTRACK] = {"T1", "T2", "T3", "T4", "T5", "T6"};

static int scene_page(const ec_app_t *a)
{
    return !a->settings && ec_page_pic(a->cat, a->page[a->cat], a->bus) == EC_PIC_SCENE;
}

/* ------------------------------------------------------------------------------------------- the fader --- */
/* the A box, the six glide positions, the B box; 22 units high from y0 */
static void fader(const ec_app_t *a, int y0)
{
    const ec_scenes_t *sc = &a->proj.e5.sc;
    const int x0 = 22, x1 = W - 22, span = x1 - x0;
    const int px = x0 + (int)(sc->pos * (float)span + 0.5f);
    int s, g;
    char n[8];
    for (s = 0; s < 2; s++) {
        const int bx = s ? W - 18 : 2;
        const int cnt = ec_scene_count(sc, s);
        const int held = a->e5.anchor == s + 1;
        if (held || cnt) {
            R(held ? EC_C_ACC : EC_C_L15, bx, y0, 16, 16);
            T2(EC_C_BG, s ? "B" : "A", bx + 8, y0 + 3, EC_AL_CENTER);
        } else {
            F(EC_C_L5, bx, y0, 16, 16);
            T2(EC_C_L5, s ? "B" : "A", bx + 8, y0 + 3, EC_AL_CENTER);
        }
        snprintf(n, sizeof n, "%d", cnt);
        T(cnt ? EC_C_L8 : EC_C_L3, n, bx + 8, y0 + 18, EC_AL_CENTER);
    }
    R(EC_C_L3, x0, y0 + 7, span, 2);                              /* the track */
    R(EC_C_L12, x0, y0 + 7, px - x0, 2);                          /* A .. the position */
    for (g = 0; g < 6; g++) {                                     /* the glide keys */
        const int gx = x0 + g * span / 5;
        const int held = a->e5.glide_held >> g & 1u;
        R(held ? EC_C_L15 : EC_C_L5, gx, y0 + (held ? 2 : 4), 2, held ? 12 : 8);   /* 0.6.1: 2 units */
    }
    if (a->e5.gliding) {                                          /* where the glide is heading */
        const int tx = x0 + (int)(a->e5.glide_target * (float)span + 0.5f);
        R(EC_C_L15, tx - 1, y0 + 16, 3, 2);
    }
    R(EC_C_ACC, px - 1, y0, 3, 16);                               /* the position */
    snprintf(n, sizeof n, "%d", (int)floorf(sc->pos * 128.0f + 0.5f));
    T(EC_C_L15, n, W / 2, y0 + 18, EC_AL_CENTER);
}

/* the params the fader moves: track, label, a bar with both ends and where the morph is */
static void morph_list(const ec_app_t *a, int y0, int rows)
{
    const ec_project_t *p = &a->proj;
    const ec_scenes_t *sc = &p->e5.sc;
    int shown = 0, total = 0, k, i;
    char b[16];
    for (k = 0; k < EC_SCN_ROWS; k++) {   /* the tracks from the selected one, then S1 S2 MST */
        static const char *const BN[3] = {"S1", "S2", "MS"};
        const int t = k < EC_NTRACK ? (a->track + k) % EC_NTRACK : k;
        const float *v = t < EC_NTRACK ? p->pat[p->current].tr[t].v : p->bus[t - EC_NTRACK];
        for (i = 0; i < EC_NP; i++) {
            const int ha = sc->has[0][t][i], hb = sc->has[1][t][i];
            float va, vb, cur;
            int y, xa, xb, xc;
            if (!ha && !hb)
                continue;
            va = ha ? sc->val[0][t][i] : v[i];
            vb = hb ? sc->val[1][t][i] : v[i];
            if (va == vb)
                continue;
            total++;
            if (shown >= rows)
                continue;
            y = y0 + shown * 8;
            cur = sc->on[t][i] ? sc->xf[t][i] : v[i];
            snprintf(b, sizeof b, "%s %s", t < EC_NTRACK ? TN[t] : BN[t - EC_NTRACK], EC_PARAMS[i].label);
            T(t == a->track || t == EC_NTRACK - 1 + a->bus ? EC_C_L15 : EC_C_L8, b, 2, y, EC_AL_LEFT);
            xa = 36 + (int)(ec_param_frac(i, va) * 56.0f + 0.5f);
            xb = 36 + (int)(ec_param_frac(i, vb) * 56.0f + 0.5f);
            xc = 36 + (int)(ec_param_frac(i, cur) * 56.0f + 0.5f);
            R(EC_C_L3, 36, y + 2, 58, 2);                         /* 0.6.1: 2 units */
            R(EC_C_L5, xa < xb ? xa : xb, y + 2, abs(xb - xa) + 2, 2);
            R(EC_C_L8, xa, y, 2, 5);
            R(EC_C_L12, xb, y, 2, 5);
            R(EC_C_ACC, xc - 1 < 36 ? 36 : xc - 1, y, 2, 5);
            ec_param_format(i, cur, iv(v, EC_P_ENV_RANGE), b, sizeof b);
            T(EC_C_L15, b, W - 2, y, EC_AL_RIGHT);
            shown++;
        }
    }
    if (total > shown) {
        snprintf(b, sizeof b, "+%d", total - shown);
        T(EC_C_L8, b, W - 2, y0 + rows * 8, EC_AL_RIGHT);
    }
    if (!total)
        ec_gfx_dots_h(2, W - 2, y0 + 12, 2, EC_C_L3);
}

static void scene_screen(const ec_app_t *a)
{
    ec_page_t pg;
    char r[16];
    ec_app_page(a, &pg);
    snprintf(r, sizeof r, "XFD %d", (int)floorf(a->proj.e5.sc.pos * 128.0f + 0.5f));
    ec_screens_header(a, "MST/SCENE", 3, r);
    fader(a, 13);
    R(EC_C_L3, 0, 37, W, 2);
    morph_list(a, 41, 4);
    ec_screens_cells(a, &pg);
}

/* ------------------------------------------------------------------------------------------- naming --- */
static void name_screen(const ec_app_t *a)
{
    /* 0.6.47 (owner, FIZZ's keybed): the name (selected while suggested / rolled: the first key replaces it), the die
     * when rolled, the last ten names as dots, the length; the keybed: B1..B11 over W1..W16 (two rows, as the FM-1's),
     * a letter on every key (SHIFT: the second set), the key typed last glowing. On the 2-unit lattice */
    static const int BK[11] = {0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14};   /* the white key each black key follows */
    static const char *const TN[9] = {"T1", "T2", "T3", "T4", "T5", "T6", "S1", "S2", "MST"};
    const ec_e5app_t *e = &a->e5;
    const int n = (int)strlen(e->name), big = n <= 12, cw = big ? 8 : 4, sh = a->fn;
    const int glow = e->name_key >= 0 && a->ms - e->name_key_ms < 250u;
    char b[24];
    int k;
    ec_kit_header(TN[a->bus ? EC_NTRACK - 1 + a->bus : a->track],
                  e->naming == EC_NM_PATTERN ? "PATTERN NAME" : e->naming == EC_NM_FILE ? "RENAME" : "PROJECT NAME",
                  "", "", 0, 1);
    if (e->name_sel && n)                          /* selected: a cream band, the name dark */
        R(EC_C_L15, 2, 16, (n * cw) + 2, big ? 14 : 10);
    ec_gfx_text_al(3, big ? 18 : 18, e->name, big ? 2 : 1, e->name_sel ? EC_C_BG : EC_C_L15, EC_AL_LEFT);
    if (!e->name_sel) {                            /* the cursor: a bar under its place */
        const int x = 2 + e->name_cur * cw;
        R(EC_C_ACC, x, big ? 32 : 26, cw - (big ? 2 : 0) > 2 ? cw - 2 : 2, 2);
    }
    if (e->name_rolled) {                          /* the die it rolled with */
        R(EC_C_SPECIAL, 104, 16, 14, 14);
        R(EC_C_BG, 106, 18, 2, 2), R(EC_C_BG, 114, 18, 2, 2), R(EC_C_BG, 110, 22, 2, 2);
        R(EC_C_BG, 106, 26, 2, 2), R(EC_C_BG, 114, 26, 2, 2);
    }
    for (k = 0; k < e->name_nh; k++)               /* the last ten names: where you are among them */
        R(k == e->name_hp && e->name_sel ? EC_C_L15 : EC_C_L5, 2 + k * 6, 38, 4, 2);
    snprintf(b, sizeof b, "%d/16", n);
    T(n >= EC_NAME - 1 ? EC_C_ALERT : EC_C_L5, b, W - 2, 44, EC_AL_RIGHT);
    if (sh)
        T(EC_C_ACC, "SHIFT", 2, 44, EC_AL_LEFT);
    for (k = 0; k < 11; k++) {                     /* the black keys */
        const int x = 12 + BK[k] * 6 + 2, lit = glow && e->name_key == k;
        R(lit ? EC_C_ACC : EC_C_L3, x, 56, 4, 24);
        T(lit ? EC_C_BG : EC_C_L15, ec_e5_name_key_char(1, k, sh), x + 2, 72, EC_AL_CENTER);
    }
    for (k = 0; k < 16; k++) {                     /* the white keys */
        const int x = 12 + k * 6, lit = glow && e->name_key == 11 + k;
        const char *c = ec_e5_name_key_char(0, k, sh);
        R(lit ? EC_C_ACC : EC_C_L12, x, 84, 4, 32);
        if (c[0] == ' ')
            R(EC_C_BG, x, 110, 4, 2);              /* SPACE */
        else
            T(EC_C_BG, c, x + 2, 106, EC_AL_CENTER);
    }
}

/* ------------------------------------------------------------------------------------------- dispatch --- */
int ec_scr_e5_full(const ec_app_t *a)
{
    if (a->e5.naming) {
        name_screen(a);
        return 1;
    }
    if (scene_page(a)) {
        scene_screen(a);
        return 1;
    }
    return 0;
}

void ec_scr_e5_overlay(const ec_app_t *a)
{
    const int full = a->e5.naming || a->ux.f3 || scene_page(a);
    if (!full && !a->settings && ec_e5_scene_layer()) {           /* the scene strip over any page */
        R(EC_C_BG, 0, 10, W, 31);
        fader(a, 12);
        R(EC_C_L3, 0, 39, W, 2);
    }
    if (!full && (a->cat == EC_CAT_PAT && !a->scr) && !a->settings && !ec_hdr_custom()) {   /* the pattern's name */
        const char *n = ec_e5_pattern_name(a->proj.current);
        if (n[0]) {
            char b[24];
            snprintf(b, sizeof b, "P%02d %s", a->proj.current + 1, n);
            R(EC_C_L12, 15, 1, 74, 8);
            T(EC_C_BG, b, 17, 3, EC_AL_LEFT);
        }
    }
}
