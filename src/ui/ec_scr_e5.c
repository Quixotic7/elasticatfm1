/* SPDX-License-Identifier: GPL-3.0-only */
/* E5's screens (ec_scr_e5.h), in the mockup's 120x120 grid at scale EC_U, palette as ec_screens.c. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ec_gfx.h"
#include "ec_scr_e5.h"

#define W EC_GW
#define H EC_GH
#define R(c, x, y, w, h) ec_gfx_rect(EC_U * (x), EC_U * (y), EC_U * (w), EC_U * (h), (c))
#define F(c, x, y, w, h) ec_gfx_frame(EC_U * (x), EC_U * (y), EC_U * (w), EC_U * (h), (c))
#define T(c, s, x, y, al) ec_gfx_text_al(EC_U * (x), EC_U * (y), (s), EC_U, (c), (al))
#define T2(c, s, x, y, al) ec_gfx_text_al(EC_U * (x), EC_U * (y), (s), 2 * EC_U, (c), (al))
#define TW(s) (ec_gfx_text_w((s), 1))

static int iv(const float *v, int id) { return (int)floorf(v[id] + 0.5f); }
static const float *tv(const ec_app_t *a) { return a->proj.pat[a->proj.current].tr[a->track].v; }
static const char *const TN[EC_NTRACK] = {"T1", "T2", "T3", "T4", "T5", "T6"};

static int scene_page(const ec_app_t *a)
{
    return a->cat == EC_CAT_MASTER && !a->settings && a->page[EC_CAT_MASTER] == 2 && !a->patn_open;
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
        R(held ? EC_C_L15 : EC_C_L5, gx, y0 + (held ? 2 : 4), 1, held ? 12 : 8);
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
    for (k = 0; k < EC_NTRACK; k++) {
        const int t = (a->track + k) % EC_NTRACK;
        const float *v = p->pat[p->current].tr[t].v;
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
            snprintf(b, sizeof b, "%s %s", TN[t], EC_PARAMS[i].label);
            T(t == a->track ? EC_C_L15 : EC_C_L8, b, 2, y, EC_AL_LEFT);
            xa = 36 + (int)(ec_param_frac(i, va) * 56.0f + 0.5f);
            xb = 36 + (int)(ec_param_frac(i, vb) * 56.0f + 0.5f);
            xc = 36 + (int)(ec_param_frac(i, cur) * 56.0f + 0.5f);
            R(EC_C_L3, 36, y + 2, 57, 1);
            R(EC_C_L5, xa < xb ? xa : xb, y + 2, abs(xb - xa) + 1, 1);
            R(EC_C_L8, xa, y, 1, 5);
            R(EC_C_L12, xb, y, 1, 5);
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
        ec_gfx_dots_h(EC_U * 2, EC_U * (W - 2), EC_U * (y0 + 12), 2 * EC_U, EC_U, EC_C_L3);
}

static void scene_screen(const ec_app_t *a)
{
    ec_page_t pg;
    char r[16];
    ec_app_page(a, &pg);
    snprintf(r, sizeof r, "XFD %d", (int)floorf(a->proj.e5.sc.pos * 128.0f + 0.5f));
    ec_screens_header(a, "MST/SCENE", 3, r);
    fader(a, 13);
    R(EC_C_L3, 0, 38, W, 1);
    morph_list(a, 41, 4);
    ec_screens_cells(a, &pg);
}

/* ------------------------------------------------------------------------------------------- PAGE --- */
static void page_screen(const ec_app_t *a)
{
    static const char *const MODE[EC_NPM] = {"SEL", "LOOP", "RATE"};
    static const char *const TITLE[EC_NPM] = {"PAGE/SELECT", "PAGE/LOOP", "PAGE/RATE"};
    const int steps = ec_seq_pattern_steps(&a->seq, a->track);
    const int pages = (steps + 15) / 16;
    const int mode = a->e5.page_mode;
    const int play = ec_seq_step(&a->seq, a->track);
    const int ppage = play >= 0 && (a->seq.playing || a->seq.paused) ? play / 16 : -1;
    const int rate = iv(tv(a), EC_P_RATE);
    const ec_ptrack_t *tr = &a->proj.pat[a->proj.current].tr[a->track];
    char r[16], b[16];
    int i;
    snprintf(r, sizeof r, "%d/%d", a->step_page + 1, pages);
    ec_screens_header(a, TITLE[mode], 1, r);
    for (i = 0; i < EC_NPM; i++) {                               /* the mode tabs (SELECT turns them) */
        const int x = 2 + i * 39;
        if (i == mode) {
            R(EC_C_L15, x, 13, 37, 9);
            T(EC_C_BG, MODE[i], x + 18, 15, EC_AL_CENTER);
        } else {
            F(EC_C_L3, x, 13, 37, 9);
            T(EC_C_L8, MODE[i], x + 18, 15, EC_AL_CENTER);
        }
    }
    for (i = 0; i < 16; i++) {
        const int c = i % 4, row = i / 4, x = 2 + c * 29, y = 25 + row * 12, w = 27, h = 10;
        uint16_t fill = 0, frame = EC_C_L3, txt = EC_C_L3;
        if (mode == EC_PM_RATE) {
            const int ri = (int)ec_e5_rate_of_key(i);
            snprintf(b, sizeof b, "%s", ec_rate_label(ri));
            if (ri == rate)
                fill = EC_C_L15, txt = EC_C_BG;
            else
                frame = i == 8 ? EC_C_L8 : EC_C_L5, txt = EC_C_L8;
        } else {
            snprintf(b, sizeof b, "%d", i + 1);
            if (i < pages) {
                if (mode == EC_PM_SELECT) {
                    if (i == a->step_page)
                        fill = EC_C_L15, txt = EC_C_BG;
                    else
                        frame = EC_C_L8, txt = EC_C_L12;
                } else if (ec_e5_page_in_loop(&a->proj, a->track, i, pages))
                    fill = EC_C_L12, txt = EC_C_BG;
                else
                    frame = EC_C_L5, txt = EC_C_L5;
            }
        }
        if (fill)
            R(fill, x, y, w, h);
        else
            F(frame, x, y, w, h);
        T(txt, b, x + w / 2, y + 3, EC_AL_CENTER);
        if (mode != EC_PM_RATE && i == ppage)
            R(EC_C_ACC, x, y + h - 1, w, 1);
    }
    /* below: the selected page's steps, the loop as played, or the rate */
    R(EC_C_L3, 0, 76, W, 1);
    if (mode == EC_PM_SELECT) {
        const int pg = a->step_page;
        snprintf(b, sizeof b, "PAGE %d", pg + 1);
        T2(EC_C_L15, b, 2, 80, EC_AL_LEFT);
        for (i = 0; i < 16; i++) {
            const int idx = pg * 16 + i, x = 2 + i * 7, y = 100;
            const ec_step_t *s = &tr->steps[idx];
            if (idx >= steps)
                R(EC_C_L3, x, y + 7, 6, 1);
            else if (s->trig || s->slices)
                R(idx == play && a->seq.playing ? EC_C_ACC : EC_C_L15, x, y, 6, 8);
            else
                F(idx == play && a->seq.playing ? EC_C_ACC : EC_C_L5, x, y, 6, 8);
        }
    } else if (mode == EC_PM_LOOP) {
        int total = 0, x = 2;
        for (i = 0; i < pages; i++)
            if (ec_e5_page_in_loop(&a->proj, a->track, i, pages))
                total += steps - i * 16 < 16 ? steps - i * 16 : 16;
        snprintf(b, sizeof b, "%d STEPS", total);
        T2(EC_C_L15, b, 2, 80, EC_AL_LEFT);
        for (i = 0; i < pages; i++) {                             /* the pages in play order, the loop bright */
            const int len = steps - i * 16 < 16 ? steps - i * 16 : 16;
            const int w = len * (W - 4) / (pages * 16) - 1;
            const int in = ec_e5_page_in_loop(&a->proj, a->track, i, pages);
            R(in ? EC_C_L12 : EC_C_L3, x, 102, w > 1 ? w : 1, 6);
            if (i == ppage && play >= 0)
                R(EC_C_ACC, x + (play - i * 16) * w / (len > 0 ? len : 1), 100, 1, 10);
            x += w + 1;
        }
    } else {
        snprintf(b, sizeof b, "%sX", ec_rate_label(rate));
        T2(EC_C_L15, b, 2, 80, EC_AL_LEFT);
    }
}

/* ------------------------------------------------------------------------------------------- naming --- */
static void name_screen(const ec_app_t *a)
{
    const char *set = EC_NAME_CHARS;
    const int nset = (int)strlen(set);
    const ec_e5app_t *e = &a->e5;
    char t[24], b[4];
    int k, ci = 0;
    const char *p;
    if (e->naming == EC_NM_PROJECT)
        snprintf(t, sizeof t, "NAME/PROJECT");
    else
        snprintf(t, sizeof t, "NAME/P%02d", e->name_pat + 1);
    ec_screens_header(a, t, 1, "");
    for (k = 0; k < EC_NAME - 1; k++) {                           /* the 16 places */
        const int x = 4 + k * 7, cur = k == e->name_cur;
        b[0] = e->name[k];
        b[1] = 0;
        if (cur)
            R(EC_C_L3, x - 1, 21, 7, 14);
        ec_gfx_text_al(EC_U * (x + 3), EC_U * 23, b, 2 * EC_U, cur ? EC_C_ACC : EC_C_L15, EC_AL_CENTER);
        R(cur ? EC_C_ACC : EC_C_L3, x, 35, 5, cur ? 2 : 1);
    }
    p = strchr(set, e->name[e->name_cur]);
    ci = p && e->name[e->name_cur] ? (int)(p - set) : 0;
    for (k = -3; k <= 3; k++) {                                   /* the character wheel (KN2) */
        const int x = W / 2 + k * 15;
        const char c = set[((ci + k) % nset + nset) % nset];
        b[0] = c == ' ' ? '_' : c;
        b[1] = 0;
        if (!k) {
            R(EC_C_L15, x - 7, 44, 14, 16);
            T2(EC_C_BG, b, x, 47, EC_AL_CENTER);
        } else
            T2(abs(k) == 1 ? EC_C_L8 : abs(k) == 2 ? EC_C_L5 : EC_C_L3, b, x, 47, EC_AL_CENTER);
    }
    /* the four cells: POS, CHAR, NAME (namesizer), DEL */
    R(EC_C_L3, 0, 76, W, 1);
    R(EC_C_L3, 60, 76, 1, 44);
    R(EC_C_L3, 0, 98, W, 1);
    {
        static const char *const LB[4] = {"POS", "CHAR", "RAND", "DEL"};
        for (k = 0; k < 4; k++) {
            const int x = (k % 2) * 61 + 3, y = k < 2 ? 79 : 101;
            char tag[4], val[8];
            snprintf(tag, sizeof tag, "K%d", k + 1);
            T(EC_C_L8, LB[k], x, y, EC_AL_LEFT);
            T(EC_C_L5, tag, x + 54, y, EC_AL_RIGHT);
            if (k == 0) {
                snprintf(val, sizeof val, "%02d", e->name_cur + 1);
                T2(EC_C_L15, val, x, y + 7, EC_AL_LEFT);
            } else if (k == 1) {
                snprintf(val, sizeof val, "%s", e->name[e->name_cur] == ' ' ? "SPC" : (char[2]){e->name[e->name_cur], 0});
                T2(EC_C_L15, val, x, y + 7, EC_AL_LEFT);
            } else if (k == 2) {                                  /* a die */
                F(EC_C_L15, x, y + 7, 11, 11);
                R(EC_C_L15, x + 2, y + 9, 2, 2), R(EC_C_L15, x + 7, y + 9, 2, 2), R(EC_C_L15, x + 4, y + 11, 3, 3);
                R(EC_C_L15, x + 2, y + 14, 2, 2), R(EC_C_L15, x + 7, y + 14, 2, 2);
            } else
                T2(EC_C_L15, "< >", x, y + 7, EC_AL_LEFT);
        }
    }
}

/* ------------------------------------------------------------------------------------------- toasts --- */
static void toast(const ec_app_t *a)
{
    const char *s = a->e5.toast;
    const int big = (int)strlen(s) <= 13;
    const int w = (big ? 2 * TW(s) : TW(s)) + 8, h = big ? 16 : 11;
    const int x = (W - w) / 2, y = 54;
    const uint16_t bg = a->e5.toast_keep ? EC_C_ACC : EC_C_L15;
    R(EC_C_BG, x - 1, y - 1, w + 2, h + 2);
    R(bg, x, y, w, h);
    if (big)
        T2(EC_C_BG, s, W / 2, y + 3, EC_AL_CENTER);
    else
        T(EC_C_BG, s, W / 2, y + 3, EC_AL_CENTER);
}

/* ------------------------------------------------------------------------------------------- dispatch --- */
int ec_scr_e5_full(const ec_app_t *a)
{
    if (a->e5.naming) {
        name_screen(a);
        return 1;
    }
    if (ec_e5_page_visible()) {
        page_screen(a);
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
    const int full = a->e5.naming || ec_e5_page_visible() || scene_page(a);
    if (!full && !a->settings && ec_e5_scene_layer()) {           /* the scene strip over any page */
        R(EC_C_BG, 0, 10, W, 31);
        fader(a, 12);
        R(EC_C_L3, 0, 40, W, 1);
    }
    if (!full && (a->patn_open || a->cat == EC_CAT_PAT) && !a->settings) {   /* the pattern's name */
        const char *n = ec_e5_pattern_name(a->proj.current);
        if (n[0]) {
            char b[24];
            snprintf(b, sizeof b, "P%02d %s", a->proj.current + 1, n);
            R(EC_C_L12, 15, 1, 74, 8);
            T(EC_C_BG, b, 17, 3, EC_AL_LEFT);
        }
    }
    if (a->e5.toast_ms)
        toast(a);
}
