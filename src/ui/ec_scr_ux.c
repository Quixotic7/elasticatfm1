/* SPDX-License-Identifier: GPL-3.0-only */
/* 0.3.0's screens (ec_scr_ux.h), in the mockup's 120x120 grid at scale EC_U, palette as ec_screens.c: cream for
 * what is active, grey for the rest, orange for locks and the playhead. Pictures, not hints: no text but values. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ec_gfx.h"
#include "ec_loader.h"
#include "ec_scr_e5.h"
#include "ec_scr_ux.h"

#define W EC_GW
#define H EC_GH
#define R(c, x, y, w, h) ec_gfx_rect(EC_U * (x), EC_U * (y), EC_U * (w), EC_U * (h), (c))
#define F(c, x, y, w, h) ec_gfx_frame(EC_U * (x), EC_U * (y), EC_U * (w), EC_U * (h), (c))
#define T(c, s, x, y, al) ec_gfx_text_al(EC_U * (x), EC_U * (y), (s), EC_U, (c), (al))
#define T2(c, s, x, y, al) ec_gfx_text_al(EC_U * (x), EC_U * (y), (s), 2 * EC_U, (c), (al))
#define TW(s) (ec_gfx_text_w((s), 1))
#define LN(c, x0, y0, x1, y1) ec_gfx_line(EC_U * (x0), EC_U * (y0), EC_U * (x1), EC_U * (y1), (c))

/* ------------------------------------------------------------------------------------------- icons --- */
/* a left-pointing triangle (back) and a right-pointing one (OK), 7 units high */
static void tri(int x, int y, int left, uint16_t c)
{
    int k;
    for (k = 0; k < 4; k++)
        R(c, left ? x + k : x + 3 - k, y + 3 - k, 1, 1 + 2 * k);
}

void ec_scr_arrows(int x, int y, uint16_t c)
{
    tri(x + 1, y + 1, 1, c);
    R(c, x + 4, y + 3, 3, 1);                      /* back: an arrow left */
    tri(x + 12, y + 1, 0, c);
    R(c, x + 9, y + 3, 3, 1);                      /* OK: an arrow right */
}

static void ic_box_arrow(int x, int y, int down, uint16_t c)   /* memorize (into the box) / recall (out of it) */
{
    F(c, x, y + 3, 7, 4);
    R(c, x + 3, y, 1, 4);
    if (down)
        R(c, x + 2, y + 2, 3, 1);
    else
        R(c, x + 2, y + 1, 3, 1);
}
static void ic_copy(int x, int y, uint16_t c)
{
    F(c, x, y, 5, 5);
    F(c, x + 2, y + 2, 5, 5);
}
static void ic_paste(int x, int y, uint16_t c)
{
    F(c, x, y + 1, 7, 6);
    R(c, x + 2, y, 3, 2);
    R(c, x + 2, y + 3, 3, 1);
}
static void ic_tap(int x, int y, uint16_t c)
{
    F(c, x, y, 7, 7);
    R(c, x + 2, y + 2, 3, 3);
}
static void ic_lock(int x, int y, uint16_t c)
{
    R(c, x, y + 3, 7, 4);
    F(c, x + 1, y, 5, 4);
}
static void ic_pencil(int x, int y, uint16_t c)
{
    LN(c, x, y + 6, x + 6, y);
    LN(c, x + 1, y + 6, x + 6, y + 1);
    R(c, x, y + 5, 2, 2);
}
static void ic_undo(int x, int y, int redo, uint16_t c)
{
    tri(redo ? x + 4 : x, y, !redo, c);
    R(c, redo ? x : x + 3, y + 3, 4, 1);
    R(c, redo ? x : x + 6, y + 3, 1, 4);
}
static void ic_stop(int x, int y, uint16_t c)
{
    R(c, x, y, 1, 7);
    R(c, x + 2, y + 1, 5, 5);
}
static void ic_save(int x, int y, uint16_t c)
{
    R(c, x, y, 7, 7);
    R(EC_C_BG, x + 2, y + 1, 3, 2);
    R(EC_C_BG, x + 1, y + 4, 5, 2);
}
static void ic_grid(int x, int y, uint16_t c)
{
    R(c, x, y, 3, 3), R(c, x + 4, y, 3, 3), R(c, x, y + 4, 3, 3), R(c, x + 4, y + 4, 3, 3);
}
static void ic_snap(int x, int y, uint16_t c)
{
    int k;
    R(c, x, y + 6, 7, 1);
    for (k = 0; k < 4; k++)
        R(c, x + k * 2, y + (k & 1 ? 3 : 0), 1, k & 1 ? 3 : 6);
}
static void ic_setup(int x, int y, uint16_t c)     /* a list */
{
    R(c, x, y, 7, 1), R(c, x, y + 3, 7, 1), R(c, x, y + 6, 7, 1);
    R(c, x, y, 1, 7);
}

/* ------------------------------------------------------------------------------------------- SHIFT's map --- */
/* the panel drawn small: the eleven upper keys, then the buttons and knobs SHIFT gives a job, each with its icon;
 * the keys without a job are dark (FIZZ: only SHIFT's keys light) */
static void shift_map(const ec_app_t *a)
{
    int i;
    R(EC_C_BG, 0, 10, W, H - 10);
    for (i = 0; i < EC_NUPPER; i++) {
        const int x = 1 + i * 108 / 10, y = 14;
        const uint16_t c = EC_C_L15;
        switch (i) {
        case 0: F(c, x, y, 9, 14), ic_box_arrow(x + 1, y + 5, 1, c); break;
        case 1: F(c, x, y, 9, 14), ic_box_arrow(x + 1, y + 5, 0, a->e5.memo_ok ? c : EC_C_L5); break;
        case 6: F(c, x, y, 9, 14), ic_copy(x + 1, y + 5, c); break;
        case 7: F(c, x, y, 9, 14), ic_paste(x + 1, y + 5, a->e5.clip_pat.ok ? c : EC_C_L5); break;
        case 8: F(c, x, y, 9, 14), ic_tap(x + 1, y + 5, c); break;
        case 9: F(c, x, y, 9, 14), ic_lock(x + 1, y + 5, a->fill_latched ? EC_C_ACC : c); break;
        case 10: F(c, x, y, 9, 14), ic_pencil(x + 1, y + 5, c); break;
        default: R(EC_C_L3, x + 2, y + 6, 5, 2); break;
        }
    }
    /* the buttons: OCT- undo, OCT+ redo, PLAY stop, REC save; the categories' setup */
    {
        const int y = 36;
        F(EC_C_L15, 2, y, 18, 14), ic_undo(7, y + 4, 0, a->e5.undo.n ? EC_C_L15 : EC_C_L5);
        F(EC_C_L15, 22, y, 18, 14), ic_undo(27, y + 4, 1, a->e5.undo.nr ? EC_C_L15 : EC_C_L5);
        F(EC_C_L15, 58, y, 18, 14), ic_stop(64, y + 4, EC_C_L15);
        F(EC_C_L15, 78, y, 18, 14), ic_save(84, y + 4, a->dirty ? EC_C_ACC : EC_C_L15);
        F(EC_C_L8, 100, y, 18, 14), ic_setup(106, y + 4, EC_C_L8);
    }
    /* the knobs: PRESETS the pattern, the value knobs snap */
    {
        const int y = 56;
        F(EC_C_L15, 2, y, 18, 14), ic_grid(7, y + 4, EC_C_L15);
        for (i = 0; i < 4; i++)
            F(EC_C_L15, 40 + i * 20, y, 18, 14), ic_snap(46 + i * 20, y + 4, EC_C_L15);
    }
    R(EC_C_L3, 0, 74, W, 1);
}

/* ------------------------------------------------------------------------------------------- the takeover --- */
/* s centred around y (mockup units), as big as fits in h units; too long for scale 2 on one line: two lines */
static void big_text(const char *s, int y, int h)
{
    int sc;
    for (sc = 5; sc >= 2; sc--)
        if (TW(s) * sc <= W - 6 && 5 * sc <= h)
            break;
    if (sc >= 2) {
        ec_gfx_text_al(EC_U * (W / 2), EC_U * (y - 5 * sc / 2), s, EC_U * sc, EC_C_L15, EC_AL_CENTER);
        return;
    }
    {
        char a[40], b[40];
        const int n = (int)strlen(s);
        int cut = n / 2, k;
        for (k = 0; k < n / 2; k++) {              /* the break nearest the middle */
            if (s[n / 2 + k] == '_' || s[n / 2 + k] == ' ' || s[n / 2 + k] == '-') {
                cut = n / 2 + k;
                break;
            }
            if (n / 2 - k > 0 && (s[n / 2 - k] == '_' || s[n / 2 - k] == ' ' || s[n / 2 - k] == '-')) {
                cut = n / 2 - k;
                break;
            }
        }
        snprintf(a, sizeof a, "%.*s", cut, s);
        snprintf(b, sizeof b, "%s", s + cut + (s[cut] == '_' || s[cut] == ' ' ? 1 : 0));
        sc = TW(a) > TW(b) ? TW(a) : TW(b);
        sc = sc * 2 <= W - 6 ? 2 : 1;
        ec_gfx_text_al(EC_U * (W / 2), EC_U * (y - 6 * sc), a, EC_U * sc, EC_C_L15, EC_AL_CENTER);
        ec_gfx_text_al(EC_U * (W / 2), EC_U * (y + sc), b, EC_U * sc, EC_C_L15, EC_AL_CENTER);
    }
}

static void takeover(const ec_app_t *a)
{
    const ec_ux_t *u = &a->ux;
    const char *v = u->to_val;
    int i;
    R(EC_C_BG, 0, 10, W, H - 10);
    if (u->to_label[0])
        T2(EC_C_L8, u->to_label, W / 2, 14, EC_AL_CENTER);   /* above the value, never under it */
    if (u->to_kind == EC_TO_SAMPLE) {              /* a file name: no extension, on two lines when it must */
        char b[40], *dot;
        snprintf(b, sizeof b, "%s", v);
        dot = strrchr(b, '.');
        if (dot && dot != b)
            *dot = 0;
        big_text(b, 42, 22);
    } else
        big_text(v, 42, 22);
    for (i = 0; i < 24; i++) {                     /* the stripes */
        const int on = (float)i < u->to_frac * 24.0f + 0.01f || (i == 0 && u->to_frac > 0);
        R(on ? EC_C_L15 : EC_C_L3, 4 + i * 4 + 4, 74, 3, 12);
    }
    if (u->to_kind == EC_TO_SAMPLE) {              /* the folder position, the load */
        const ec_slotrt_t *rt = &a->slot[ec_app_track_slot(a, a->track)];
        T2(EC_C_L8, u->to_sub, W / 2, 92, EC_AL_CENTER);
        if (u->pre_idx >= 0 || rt->state == EC_SL_QUEUED || rt->state == EC_SL_LOADING) {
            const int pm = rt->state == EC_SL_LOADING ? (int)ec_load_permille() : 0;
            R(EC_C_L3, 8, 108, W - 16, 3);
            R(EC_C_ACC, 8, 108, (W - 16) * pm / 1000, 3);
        }
    }
}

/* ------------------------------------------------------------------------------------------- F3 --- */
static void f3_screen(const ec_app_t *a)
{
    const int steps = ec_seq_pattern_steps(&a->seq, a->track);
    const int pages = (steps + 15) / 16;
    const int play = ec_seq_step(&a->seq, a->track);
    const int ppage = play >= 0 && (a->seq.playing || a->seq.paused) ? play / 16 : -1;
    ec_page_t pg;
    char b[16];
    int i;
    ec_screens_header(a, "F3", 1, "");
    /* tempo (SELECT) and swing (ALGORITHM) */
    if (fabsf(a->proj.bpm - floorf(a->proj.bpm + 0.5f)) < 0.05f)
        snprintf(b, sizeof b, "%d", (int)floorf(a->proj.bpm + 0.5f));
    else
        snprintf(b, sizeof b, "%.1f", (double)a->proj.bpm);
    T(EC_C_L8, "BPM", 3, 13, EC_AL_LEFT);
    ec_gfx_text_al(EC_U * 3, EC_U * 20, b, 3 * EC_U, EC_C_L15, EC_AL_LEFT);
    T(EC_C_L8, "SWNG", 64, 13, EC_AL_LEFT);
    snprintf(b, sizeof b, "%d%%", (int)floorf(a->proj.swing + 0.5f));
    ec_gfx_text_al(EC_U * 64, EC_U * 20, b, 3 * EC_U, EC_C_L15, EC_AL_LEFT);
    if (a->seq.playing)                            /* the beat */
        R((a->seq.global_pos / 4) % 2 ? EC_C_L5 : EC_C_ACC, 54, 22, 4, 4);
    R(EC_C_L3, 0, 37, W, 1);
    /* the pages: tap a key = edit it (bright), hold = in / out of the loop (filled), the playing one underlined */
    for (i = 0; i < 16; i++) {
        const int c = i % 8, row = i / 8, x = 2 + c * 14 + (c >= 4 ? 4 : 0), y = 41 + row * 16, w = 12, h = 13;
        snprintf(b, sizeof b, "%d", i + 1);
        if (i >= pages) {
            R(EC_C_L3, x + w / 2 - 1, y + h / 2, 2, 1);
            continue;
        }
        if (ec_e5_page_in_loop(&a->proj, a->track, i, pages)) {
            R(i == a->step_page ? EC_C_L15 : EC_C_L8, x, y, w, h);
            T(EC_C_BG, b, x + w / 2, y + 4, EC_AL_CENTER);
        } else {
            F(i == a->step_page ? EC_C_L15 : EC_C_L5, x, y, w, h);
            T(i == a->step_page ? EC_C_L15 : EC_C_L5, b, x + w / 2, y + 4, EC_AL_CENTER);
        }
        if (i == a->step_page)
            R(EC_C_L15, x, y + h + 1, w, 1);
        if (i == ppage)
            R(EC_C_ACC, x, y + h - 2, w, 2);
    }
    ec_page_f3(&pg);
    ec_screens_cells(a, &pg);
}

int ec_scr_ux_full(const ec_app_t *a)
{
    if (a->ux.f3 && !a->e5.naming) {
        f3_screen(a);
        return 1;
    }
    return 0;
}

void ec_scr_ux_overlay(const ec_app_t *a)
{
    if (ec_app_shift_map(a))
        shift_map(a);
    else if (a->ux.to_ms)
        takeover(a);
    if (a->step_held && !a->bus && !a->e5.naming && !a->ux.f3 && !a->patn_open) {   /* the knobs lock: the frame */
        ec_gfx_frame(0, 0, 240, 240, EC_C_ACC);
        ec_gfx_frame(1, 1, 238, 238, EC_C_ACC);
        ec_gfx_frame(2, 2, 236, 236, EC_C_ACC);
    }
}
