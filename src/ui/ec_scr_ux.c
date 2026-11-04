/* SPDX-License-Identifier: GPL-3.0-only */
/* 0.3.0's screens (ec_scr_ux.h), on the 120x120 grid in palette slots (ec_gfx.h): cream for
 * what is active, grey for the rest, orange for locks and the playhead. Pictures, not hints: no text but values. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ec_gfx.h"
#include "ec_loader.h"
#include "ec_scr_e5.h"
#include "ec_scr_ux.h"
#include "ec_ui_kit.h"

#define W EC_GW
#define H EC_GH
#define R(c, x, y, w, h) ec_gfx_rect((x), (y), (w), (h), (c))
#define F(c, x, y, w, h) ec_gfx_frame((x), (y), (w), (h), (c))
#define T(c, s, x, y, al) ec_gfx_text_al((x), (y), (s), 1, (c), (al))
#define T2(c, s, x, y, al) ec_gfx_text_al((x), (y), (s), 2, (c), (al))
#define TW(s) (ec_gfx_text_w((s), 1))

/* ------------------------------------------------------------------------------------------- icons --- */
void ec_scr_arrows(int x, int y, int c) { ec_kit_arrows(x + 1, y + 1, c); }

/* ------------------------------------------------------------------------------------------- SHIFT's map --- */
/* SHIFT held alone (the mock's SHIFT): BPM (SELECT) and swing (ALGORITHM) big, the track keys W1..W9 with their
 * levels. 0.6.6: over the top of the page only (y 0..75): the page's own cells stay below, since SHIFT + a knob snaps
 * those params (the norns snap values, ec_snap.c) */
static void shift_map(const ec_app_t *a)
{
    static const char *const N[9] = {"T1", "T2", "T3", "T4", "T5", "T6", "S1", "S2", "MST"};
    const int sel = a->bus ? EC_NTRACK - 1 + a->bus : a->track;
    static char bpm[12], sw[8];
    int i;
    R(EC_C_BG, 0, 0, W, 76);                         /* down to the cells' top rule (y 76) */
    ec_screens_header(a, "SHIFT", 0, "");
    snprintf(bpm, sizeof bpm, "%.1f", (double)a->proj.bpm);   /* the mock's 120.0: the tenths SELECT sets */
    snprintf(sw, sizeof sw, "%d", (int)floorf(a->proj.swing + 0.5f));
    T(EC_C_L8, "BPM", 3, 14, EC_AL_LEFT);
    ec_gfx_text_al(3, 21, bpm, 3, EC_C_L15, EC_AL_LEFT);
    T(EC_C_L8, "SWING", W - 3, 14, EC_AL_RIGHT);
    ec_gfx_text_al(W - 3, 21, sw, 3, EC_C_L15, EC_AL_RIGHT);
    for (i = 0; i < 9; i++) {                        /* the track keys W1..W9 */
        const int x = 3 + i * 13;
        const float lv = a->meter[i] > 1 ? 1 : a->meter[i];
        if (i == sel) {
            R(EC_C_L15, x, 46, 12, 11);
            T(EC_C_BG, N[i], x + 6, 49, EC_AL_CENTER);
        } else {                                     /* 0.6.1: a dim block, not a 1-unit frame */
            const int audible = i >= EC_NTRACK || ec_app_track_audible(a, i);
            if (i < 6 && audible)
                R(EC_C_L3, x, 46, 12, 11);
            T(i < 6 && audible ? EC_C_L12 : EC_C_L8, N[i], x + 6, 49, EC_AL_CENTER);
        }
        if (lv > 0)
            R(i == sel ? EC_C_L15 : EC_C_L5, x + 1, 62, (int)(10.0f * lv + 0.5f) < 2 ? 2 : (int)(10.0f * lv + 0.5f), 3);
    }
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
        ec_gfx_text_al(W / 2, y - 5 * sc / 2, s, sc, EC_C_L15, EC_AL_CENTER);
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
        ec_gfx_text_al(W / 2, y - 6 * sc, a, sc, EC_C_L15, EC_AL_CENTER);
        ec_gfx_text_al(W / 2, y + sc, b, sc, EC_C_L15, EC_AL_CENTER);
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
        R(on ? EC_C_L15 : EC_C_L3, 4 + i * 4 + 4, 74, 2, 12);   /* 0.6.1: 2-unit gaps */
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
    ec_gfx_text_al(3, 20, b, 3, EC_C_L15, EC_AL_LEFT);
    T(EC_C_L8, "SWNG", 64, 13, EC_AL_LEFT);
    snprintf(b, sizeof b, "%d%%", (int)floorf(a->proj.swing + 0.5f));
    ec_gfx_text_al(64, 20, b, 3, EC_C_L15, EC_AL_LEFT);
    if (a->seq.playing)                            /* the beat */
        R((a->seq.global_pos / 4) % 2 ? EC_C_L5 : EC_C_ACC, 54, 22, 4, 4);
    R(EC_C_L3, 0, 36, W, 2);
    /* the pages: tap a key = edit it (bright), hold = in / out of the loop (filled), the playing one underlined */
    for (i = 0; i < 16; i++) {
        const int c = i % 8, row = i / 8, x = 2 + c * 14 + (c >= 4 ? 4 : 0), y = 41 + row * 16, w = 12, h = 13;
        snprintf(b, sizeof b, "%d", i + 1);
        if (i >= pages) {
            R(EC_C_L3, x + w / 2 - 1, y + h / 2, 2, 2);
            continue;
        }
        if (ec_e5_page_in_loop(&a->proj, a->track, i, pages)) {
            R(i == a->step_page ? EC_C_L15 : EC_C_L8, x, y, w, h);
            T(EC_C_BG, b, x + w / 2, y + 4, EC_AL_CENTER);
        } else {
            F(i == a->step_page ? EC_C_L15 : EC_C_L5, x, y, w, h);
            T(i == a->step_page ? EC_C_L15 : EC_C_L5, b, x + w / 2, y + 4, EC_AL_CENTER);
        }
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
}
