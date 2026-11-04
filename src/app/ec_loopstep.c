/* SPDX-License-Identifier: GPL-3.0-only */
/* 0.6.6 (docs/UX-V2.md "LOOP ENGINE step entry"): sequencing regions and ranges from LOOP ENGINE. The norns grid had
 * a step row and a loop-key row at once; the FM-1 has one row of white keys, so LOOP ENGINE uses KEYBOARD's target
 * step (P->target: ALGO, shared with KEYBOARD; 0 = LIVE):
 *   - B11 = pick a step: tapped, pick mode stays on (the header says PICK STEP, the white keys STEP n) until a white
 *     key picks that step (of the step page shown) or B11 is tapped again (LIVE). Held + a white key does the same in
 *     one gesture. (A mouse on the web panel can't hold one key and press another: so the tap.) Held + K1 is still the
 *     zoom (ec_srckeys.c), not shown.
 *   - With a target, the white keys write it: the keys held together are one entry; letting go of the last writes
 *     their span as the step's region lock (LOOP_START / LOOP_END, in the loop keys' division) or, on the RANGE page,
 *     its range lock (RANGE_START / RANGE_END, the 16 keys over the whole sample), puts a trig on it and moves the
 *     target on (KEYBOARD's ADVANCE; SHIFT during the entry does the other). Slice tracks: the keys' slices.
 *     Stopped, the held keys also sound (the live loop keys / pads), so you hear what you write. */
#include <math.h>
#include <stdio.h>

#include "ec_app.h"

#define A (&ec_app)
#define P (&ec_app.pn)
#define E (&ec_app.e5)

static struct {
    int b11;              /* B11 held */
    int b11_used;         /* a white key picked a step (or K1 zoomed) while it was held */
    int pick;             /* pick mode: the next white key picks the target step */
    int was_on;           /* pick mode was on when B11 went down (a second tap: LIVE) */
    uint32_t keys;        /* the entry's keys (bit i: white key i), still held */
    uint32_t span;        /* every key of the entry */
    int lk[EC_NLOWER];    /* the loop key each held key sounds (stopped), 0 none */
    int fn;               /* SHIFT seen during the entry */
    int range;            /* the entry started on the RANGE page */
} g;

static int clampi(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }
static int iv(const float *v, int id) { return (int)floorf(v[id] + 0.5f); }
static int slice_track(void)
{
    const int m = iv(ec_app_params(A), EC_P_MACHINE);
    return m >= EC_MACH_GRID_SLICE && m <= EC_MACH_RAZOR_POLY;
}
static int on_range_page(void)
{
    ec_page_t pg;
    ec_app_page(A, &pg);
    return pg.pic == EC_PIC_WAVE_RANGE;
}

int ec_lp_active(void) { return ec_app_mode() == EC_CAT_SRC && !A->bus && !A->settings; }
int ec_lp_target(void) { return ec_lp_active() ? P->target : 0; }

/* B11 (LOOP ENGINE): a tap turns pick mode on (a second tap: LIVE); held + a white key picks at once */
void ec_lp_b11(int down)
{
    if (down) {
        g.b11 = 1;
        g.b11_used = 0;
        g.was_on = g.pick;
        g.pick = 1;
        A->redraw = 1;
        return;
    }
    if (!g.b11)
        return;
    g.b11 = 0;
    if (g.b11_used)                                /* it picked (or zoomed) while held: done */
        g.pick = 0;
    else if (g.was_on) {                           /* tapped again with pick mode on: LIVE */
        g.pick = 0;
        P->target = 0;
        ec_app_message("LIVE");
    }                                              /* else a tap: pick mode stays on */
    A->redraw = 1;
}
int ec_lp_b11_held(void) { return g.b11 && ec_lp_active(); }
int ec_lp_picking(void) { return g.pick && ec_lp_active(); }
void ec_lp_b11_used(void) { g.b11_used = 1; }

/* the entry written: the span of keys lo..hi (0-based white keys) */
static void write_entry(int lo, int hi)
{
    ec_ptrack_t *tr = ec_project_track(&A->proj, A->track);
    const float *v = ec_app_params(A);
    const int len = ec_seq_pattern_steps(&A->seq, A->track);
    const int idx = clampi(P->target, 1, len) - 1;
    ec_step_t *s = &tr->steps[idx];
    int adv = ec_as->advance != 0, i;
    char b[24];
    ec_undo_steps(&E->undo, &A->proj, A->track, idx / 16, (uint16_t)(1u << (idx % 16)), g.range ? "RANGE" : "REGION",
                  0xE00u + (uint32_t)idx, A->ms);
    E->undo.last_key = 0;
    s->trig = 1;
    if (slice_track() && !g.range) {               /* slice tracks: the keys' slices */
        const int half = P->zoom_half ? 16 : 0;
        s->slices = 0;
        for (i = 0; i < EC_NLOWER; i++)
            if (g.span >> i & 1u)
                s->slices |= 1u << clampi(i + half, 0, 31);
        snprintf(b, sizeof b, "S%02d SLICE %d", idx + 1, lo + half + 1);
    } else if (g.range) {                          /* RANGE: the 16 keys over the whole sample */
        const float u = 128.0f / 16.0f;
        if (ec_step_lock_set(s, EC_P_RANGE_START, (float)lo * u) < 0 ||
            ec_step_lock_set(s, EC_P_RANGE_END, (float)(hi + 1) * u) < 0)
            ec_app_message("LOCKS FULL");
        snprintf(b, sizeof b, "S%02d RANGE %d-%d", idx + 1, lo + 1, hi + 1);
    } else {                                       /* the region, in the loop keys' division (as live REC's) */
        const int div = clampi((iv(v, EC_P_LOOP_DIVISION) + 1) / 2 * 2, 2, 32);
        const float u = 128.0f / (float)div;
        const int half = P->zoom_half && div > 16 ? 16 : 0;
        const int k0 = clampi(lo + 1 + half, 1, div), k1 = clampi(hi + 1 + half, 1, div);
        if (ec_step_lock_set(s, EC_P_LOOP_START, (float)(k0 - 1) * u) < 0 ||
            ec_step_lock_set(s, EC_P_LOOP_END, (float)k1 * u) < 0)
            ec_app_message("LOCKS FULL");
        if (k0 == k1)
            snprintf(b, sizeof b, "S%02d LOOP %d", idx + 1, k0);
        else
            snprintf(b, sizeof b, "S%02d LOOP %d-%d", idx + 1, k0, k1);
    }
    ec_app_message(b);
    ec_seq_touch(&A->seq, A->track);
    ec_app_mark_dirty();
    if (g.fn)                                      /* SHIFT during the entry: the other */
        adv = !adv;
    if (adv) {
        P->target = P->target % len + 1;
        A->step_page = (P->target - 1) / 16;
    }
}

/* a white key in LOOP ENGINE: 1 = taken (B11's step pick, or an entry on the target step) */
int ec_lp_white(int k, int i, int down, int vel)
{
    int lo, hi;
    if (!ec_lp_active() || i < 0 || i >= EC_NLOWER)
        return 0;
    if (down && g.pick) {                          /* pick mode (B11 tapped or held) + a key: that step */
        const int len = ec_seq_pattern_steps(&A->seq, A->track);
        const int st = A->step_page * 16 + i + 1;
        char b[16];
        g.b11_used = 1;
        g.pick = 0;
        if (st <= len) {
            P->target = st;
            snprintf(b, sizeof b, "STEP %d", st);
            ec_app_message(b);
        }
        A->redraw = 1;
        return 1;
    }
    if (down) {
        if (!P->target)
            return 0;
        if (!g.keys) {                             /* a new entry */
            g.span = 0;
            g.fn = 0;
            g.range = on_range_page();
        }
        g.keys |= 1u << i;
        g.span |= 1u << i;
        g.fn |= A->fn;
        g.lk[i] = 0;
        if (!A->seq.playing) {                     /* stopped: hear it */
            const int lk = ec_e5_loop_key(k, i, 1);
            if (slice_track())
                ec_seq_slice_pad(&A->seq, A->track, lk, 1, (float)vel / 127.0f);
            else
                ec_seq_loop_key(&A->seq, A->track, lk, 1);
            g.lk[i] = lk;
        }
        A->redraw = 1;
        return 1;
    }
    if (!(g.keys >> i & 1u))
        return 0;
    if (g.lk[i]) {
        if (slice_track())
            ec_seq_slice_pad(&A->seq, A->track, g.lk[i], 0, 0);
        else
            ec_seq_loop_key(&A->seq, A->track, g.lk[i], 0);
        g.lk[i] = 0;
    }
    g.keys &= ~(1u << i);
    g.fn |= A->fn;
    if (g.keys || !P->target)                      /* still held: the entry goes on */
        return 1;
    for (lo = 0; lo < EC_NLOWER && !(g.span >> lo & 1u); lo++)
        ;
    for (hi = EC_NLOWER - 1; hi > 0 && !(g.span >> hi & 1u); hi--)
        ;
    write_entry(lo, hi);
    g.span = 0;
    A->redraw = 1;
    return 1;
}

/* the entry's keys held now (the LEDs and the screen) */
uint32_t ec_lp_keys(void) { return g.keys; }
