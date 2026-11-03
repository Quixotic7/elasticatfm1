/* SPDX-License-Identifier: GPL-3.0-only */
/* 0.6.0 (docs/UX-V2.md §3): MIX (EDIT, first) and PAGES (EDIT, second).
 * MIX: W1..W9 the targets T1..T6 S1 S2 MST (tap select, hold 400 ms mute; SHIFT + a key solo, SHIFT + B11 (FILL) held +
 * a key every mute and solo off: ec_panel.c), B1..B8 the selected track's rate (1/8 1/4 1/2 3/4 1X 3/2 2X 4X), B9
 * slower, B10 faster (the rate table's neighbours), B11 the CAT (norns' dancingcat at the tempo; B11 or any mode
 * button leaves it, the track keys keep working).
 * PAGES: the black keys are the pattern's step pages, FIZZ style: a tap edits the page, a hold (0.4 s) puts it in or
 * out of the loop; with F1 / F2 on they copy / cut and paste whole pages (§2). */
#include <math.h>
#include <stdio.h>

#include "ec_app.h"
#include "ec_ui_kit.h"

#define A (&ec_app)
#define P (&ec_app.pn)

static int clampi(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }
static int iv(const float *v, int id) { return (int)floorf(v[id] + 0.5f); }

/* ------------------------------------------------------------------------------------------- MIX --- */
void ec_mix_track_down(int i)
{
    const uint16_t bit = (uint16_t)(1u << i);
    P->trk_down |= bit;
    P->trk_used &= (uint16_t)~bit;
    P->trk_ms[i] = A->ms;
}

void ec_mix_track_up(int i)
{
    const uint16_t bit = (uint16_t)(1u << i);
    if (!(P->trk_down & bit))
        return;
    P->trk_down &= (uint16_t)~bit;
    if (!(P->trk_used & bit))                      /* a tap: select */
        ec_pn_select_target(i);
    P->trk_used &= (uint16_t)~bit;
}

void ec_mix_hold_tick(void)
{
    int i;
    for (i = 0; i < EC_NTRACK; i++) {              /* a track key held: mute / unmute */
        const uint16_t bit = (uint16_t)(1u << i);
        if ((P->trk_down & bit) && !(P->trk_used & bit) && A->ms - P->trk_ms[i] >= EC_HOLD_MS) {
            P->trk_used |= bit;
            ec_app_toggle_mute(i);
        }
    }
    for (i = EC_NTRACK; i < 9; i++) {              /* (S1 S2 MST have no mute: a hold selects too) */
        const uint16_t bit = (uint16_t)(1u << i);
        if ((P->trk_down & bit) && !(P->trk_used & bit) && A->ms - P->trk_ms[i] >= EC_HOLD_MS) {
            P->trk_used |= bit;
            ec_pn_select_target(i);
        }
    }
}

/* the rate keys over the rate table (ec_params.h EC_RATE_FRAC: speeds 1/16 .. 2); 4X needs a 22nd entry */
int ec_mix_rate_index(int key)
{
    static const int R[8] = {2, 4, 7, 10, 13, 17, 21, 22};
    if (key < 1 || key > 8)
        return 0;
    return R[key - 1] <= EC_RATES ? R[key - 1] : 0;
}

static void set_rate(int idx)
{
    float *v = ec_app_params(A);
    char b[16];
    idx = clampi(idx, 1, EC_RATES);
    ec_e5_edit_param(A->track, EC_P_RATE, 0);
    v[EC_P_RATE] = (float)idx;
    ec_seq_timing_changed(&A->seq);
    ec_seq_touch(&A->seq, A->track);
    snprintf(b, sizeof b, "T%d RATE %s", A->track + 1, ec_rate_label(idx));
    ec_app_message(b);
    ec_app_mark_dirty();
}

void ec_mix_black(int key)
{
    if (key == 11) {                               /* the CAT */
        P->cat = !P->cat;
        P->cat_ms = A->ms;
        if (P->cat) {                              /* 0.6.3: a new fractal each time, never the last one */
            P->cat_rng ^= A->ms * 0x9E3779B1u ^ P->cat_n * 0x85EBCA77u;
            P->cat_algo1 = ec_scr_cat_next(&P->cat_rng, P->cat_algo1 - 1) + 1;
            P->cat_seed = P->cat_rng * 0x2C1B3C6Du + (uint32_t)P->cat_algo1;
            P->cat_t = 0.0;
            P->cat_beats = ec_seq_beats(&A->seq);
        }
        A->redraw = 1;
        return;
    }
    if (A->bus) {
        ec_app_message("TRACKS ONLY");
        return;
    }
    if (key <= 8) {
        const int r = ec_mix_rate_index(key);
        if (r)
            set_rate(r);
    } else
        set_rate(iv(ec_app_params(A), EC_P_RATE) + (key == 9 ? -1 : 1));
}

/* ------------------------------------------------------------------------------------------- PAGES --- */
int ec_pages_count(void) { return (ec_seq_pattern_steps(&A->seq, A->track) + 15) / 16; }

void ec_pages_black_down(int key)
{
    const int pg = key - 1;
    if (ec_pn_f(1) || ec_pn_f(2)) {                /* §2: whole pages */
        P->f_used = 3;
        P->bk_used |= (uint16_t)(1u << pg);
        if (pg < ec_pages_count())
            ec_clip_tap(EC_CK_PAGE, pg);
    }
}

void ec_pages_black_up(int key)
{
    const int pg = key - 1;
    if (P->bk_used >> pg & 1u)
        return;
    if (pg < ec_pages_count()) {                   /* a tap: edit that page */
        char b[16];
        A->step_page = pg;
        A->step_held = 0;
        snprintf(b, sizeof b, "PAGE %d", pg + 1);
        ec_app_message(b);
    }
}

void ec_pages_hold_tick(void)
{
    int i;
    if (ec_app_mode() != EC_MODE_PAGES || A->settings)
        return;
    for (i = 0; i < EC_NBKEY; i++)                 /* a hold: the page in / out of the loop */
        if ((P->bk_held >> i & 1u) && !(P->bk_used >> i & 1u) && A->ms - P->bk_ms[i] >= EC_HOLD_MS) {
            const int pages = ec_pages_count();
            P->bk_used |= (uint16_t)(1u << i);
            if (i < pages) {
                ec_e5_toggle_page_loop(&A->proj, A->track, i, pages);
                ec_seq_timing_changed(&A->seq);
                ec_app_mark_dirty();
            }
        }
}
