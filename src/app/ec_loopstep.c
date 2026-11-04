/* SPDX-License-Identifier: GPL-3.0-only */
/* 0.6.9 (docs/UX-V2.md "LOOP ENGINE step entry", the owner's second pass): sequencing regions and ranges from LOOP
 * ENGINE. The norns grid had a step row and a loop-key row at once; the FM-1 has one row of white keys, so B11 switches
 * them between two layers, LOOP keys and STEP keys:
 *   - LOOP layer: the loop keys work exactly as live (they scrub and sound, playing or not). With steps selected, each
 *     entry (the keys held together; letting go of the last) also writes its span into every selected step: the
 *     region lock (LOOP_START / LOOP_END in the loop keys' division) or, on the RANGE page, the range lock (the 16
 *     keys over the whole sample), and a trig. Slice tracks: the keys' slices. Nothing advances.
 *   - STEP layer: the white keys select steps (of the step page shown): the first key since the layer came up
 *     replaces the selection, the next ones add / remove steps.
 *   - B11 held = the other layer while held; a quick click (nothing else done while it was down) latches the other
 *     layer (STEP latched: hold B11 for the loop keys, the reverse). B11 never goes LIVE: ALGO below step 1 does.
 *   - B11 held + OCT- = clear the selected steps (trig and locks; one undo). B11 held + K1 = the zoom (ec_srckeys.c).
 *   - ALGO selects one step (P->target, shared with KEYBOARD); below step 1 = LIVE (nothing selected).
 * The selection acts as held steps: the knobs lock onto every selected step (ec_app.c), the cells and the brackets show
 * the first one (P->target is kept as the first selected). */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ec_app.h"

#define A (&ec_app)
#define P (&ec_app.pn)
#define E (&ec_app.e5)
#define QUICK_MS 300u

static struct {
    uint32_t sel[EC_MAXSTEPS / 32];   /* the selected steps (bit idx) */
    int latched;          /* STEP latched as the base layer */
    int b11;              /* B11 held */
    uint32_t b11_ms;
    int b11_used;         /* something was done while B11 was down: not a quick click */
    int fresh;            /* the STEP layer just came up: its first key replaces the selection */
    uint32_t keys;        /* the LOOP entry's keys (bit i: white key i), still held */
    uint32_t span;        /* every key of the entry */
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
static int len_now(void) { return ec_seq_pattern_steps(&A->seq, A->track); }

int ec_lp_active(void) { return ec_app_mode() == EC_CAT_SRC && !A->bus && !A->settings; }
int ec_lp_is_sel(int idx) { return idx >= 0 && idx < EC_MAXSTEPS && (g.sel[idx / 32] >> (idx % 32) & 1u); }
static void sel_set(int idx, int on)
{
    if (idx < 0 || idx >= EC_MAXSTEPS)
        return;
    if (on)
        g.sel[idx / 32] |= 1u << (idx % 32);
    else
        g.sel[idx / 32] &= ~(1u << (idx % 32));
}
int ec_lp_first(void)
{
    int i;
    const int len = len_now();
    for (i = 0; i < len; i++)
        if (ec_lp_is_sel(i))
            return i;
    return -1;
}
int ec_lp_count(void)
{
    int i, n = 0;
    const int len = len_now();
    for (i = 0; i < len; i++)
        n += ec_lp_is_sel(i);
    return n;
}
uint16_t ec_lp_page_mask(int page)
{
    uint16_t m = 0;
    int i;
    for (i = 0; i < 16; i++)
        if (page * 16 + i < len_now() && ec_lp_is_sel(page * 16 + i))
            m |= (uint16_t)(1u << i);
    return m;
}
static void sync_target(void)
{
    const int f = ec_lp_first();
    P->target = f >= 0 ? f + 1 : 0;
    A->redraw = 1;
}
int ec_lp_target(void) { return ec_lp_active() && ec_lp_first() >= 0 ? ec_lp_first() + 1 : 0; }
int ec_lp_step_layer(void) { return ec_lp_active() && (g.latched ^ g.b11); }
int ec_lp_b11_held(void) { return g.b11 && ec_lp_active(); }
void ec_lp_b11_used(void) { g.b11_used = 1; }
uint32_t ec_lp_keys(void) { return g.keys; }

/* ALGO in LOOP ENGINE: one step selected (KEYBOARD's target), below step 1 = LIVE */
void ec_lp_algo(int det)
{
    int f = ec_lp_first();
    P->target = f >= 0 ? f + 1 : 0;
    ec_kb_target(det);
    memset(g.sel, 0, sizeof g.sel);
    if (P->target)
        sel_set(P->target - 1, 1);
    A->redraw = 1;
}

void ec_lp_b11(int down)
{
    if (down) {
        g.b11 = 1;
        g.b11_ms = A->ms;
        g.b11_used = 0;
        if (!g.latched)                            /* the STEP layer comes up while held */
            g.fresh = 1;
        A->redraw = 1;
        return;
    }
    if (!g.b11)
        return;
    g.b11 = 0;
    if (!g.b11_used && A->ms - g.b11_ms < QUICK_MS) {   /* a quick click: latch the other layer */
        g.latched = !g.latched;
        if (g.latched)
            g.fresh = 1;
        ec_app_message(g.latched ? "STEP KEYS" : "LOOP KEYS");
    }
    A->redraw = 1;
}

/* B11 held + OCT-: the selected steps cleared (trig and every lock), one undo a step page */
void ec_lp_clear(void)
{
    ec_ptrack_t *tr = ec_project_track(&A->proj, A->track);
    const int pages = (len_now() + 15) / 16;
    int pg, i, n = 0;
    char b[20];
    g.b11_used = 1;
    for (pg = 0; pg < pages; pg++) {
        const uint16_t m = ec_lp_page_mask(pg);
        if (!m)
            continue;
        ec_undo_steps(&E->undo, &A->proj, A->track, pg, m, "CLEAR", 0xE80u + (uint32_t)pg, A->ms);
        E->undo.last_key = 0;
        for (i = 0; i < 16; i++)
            if (m >> i & 1u) {
                ec_step_clear(&tr->steps[pg * 16 + i]);
                n++;
            }
    }
    snprintf(b, sizeof b, n ? "CLEARED %d" : "NO STEP", n);
    ec_app_message(b);
    ec_seq_touch(&A->seq, A->track);
    ec_app_mark_dirty();
}

/* the entry written into every selected step: the span of keys lo..hi (0-based white keys) */
static void write_entry(int lo, int hi)
{
    ec_ptrack_t *tr = ec_project_track(&A->proj, A->track);
    const float *v = ec_app_params(A);
    const int pages = (len_now() + 15) / 16, half = P->zoom_half ? 16 : 0;
    const int div = clampi((iv(v, EC_P_LOOP_DIVISION) + 1) / 2 * 2, 2, 32);
    const int k0 = clampi(lo + 1 + (div > 16 ? half : 0), 1, div), k1 = clampi(hi + 1 + (div > 16 ? half : 0), 1, div);
    int pg, i, k, n = 0;
    char b[24];
    for (pg = 0; pg < pages; pg++) {
        const uint16_t m = ec_lp_page_mask(pg);
        if (!m)
            continue;
        ec_undo_steps(&E->undo, &A->proj, A->track, pg, m, g.range ? "RANGE" : "REGION", 0xE00u + (uint32_t)pg,
                      A->ms);
        E->undo.last_key = 0;
        for (i = 0; i < 16; i++) {
            ec_step_t *s;
            if (!(m >> i & 1u))
                continue;
            s = &tr->steps[pg * 16 + i];
            s->trig = 1;
            n++;
            if (slice_track() && !g.range) {       /* slice tracks: the keys' slices */
                s->slices = 0;
                for (k = 0; k < EC_NLOWER; k++)
                    if (g.span >> k & 1u)
                        s->slices |= 1u << clampi(k + half, 0, 31);
            } else if (g.range) {                  /* RANGE: the 16 keys over the whole sample */
                const float u = 128.0f / 16.0f;
                if (ec_step_lock_set(s, EC_P_RANGE_START, (float)lo * u) < 0 ||
                    ec_step_lock_set(s, EC_P_RANGE_END, (float)(hi + 1) * u) < 0)
                    ec_app_message("LOCKS FULL");
            } else {                               /* the region, in the loop keys' division (as live REC's) */
                const float u = 128.0f / (float)div;
                if (ec_step_lock_set(s, EC_P_LOOP_START, (float)(k0 - 1) * u) < 0 ||
                    ec_step_lock_set(s, EC_P_LOOP_END, (float)k1 * u) < 0)
                    ec_app_message("LOCKS FULL");
            }
        }
    }
    if (g.range)
        snprintf(b, sizeof b, "RANGE %d-%d > %d", lo + 1, hi + 1, n);
    else if (k0 == k1)
        snprintf(b, sizeof b, "LOOP %d > %d STEP%s", k0, n, n == 1 ? "" : "S");
    else
        snprintf(b, sizeof b, "LOOP %d-%d > %d", k0, k1, n);
    ec_app_message(b);
    ec_seq_touch(&A->seq, A->track);
    ec_app_mark_dirty();
}

/* a white key in LOOP ENGINE's STEP layer: 1 = taken (it selects); the LOOP layer is the caller's (live) */
int ec_lp_white(int k, int i, int down, int vel)
{
    (void)k, (void)vel;
    if (!ec_lp_step_layer() || i < 0 || i >= EC_NLOWER)
        return 0;
    if (down) {
        const int idx = A->step_page * 16 + i;
        if (g.b11)
            g.b11_used = 1;
        if (idx >= len_now())
            return 1;
        if (g.fresh) {                             /* the layer's first key: a new selection */
            memset(g.sel, 0, sizeof g.sel);
            g.fresh = 0;
            sel_set(idx, 1);
        } else
            sel_set(idx, !ec_lp_is_sel(idx));
        sync_target();
    }
    return 1;
}

/* a loop key (LOOP layer) went down / up: with steps selected, the entry is written when its last key is let go */
void ec_lp_loop_note(int i, int down)
{
    int lo, hi;
    if (!ec_lp_active() || i < 0 || i >= EC_NLOWER)
        return;
    if (g.b11)
        g.b11_used = 1;                            /* (the reverse: B11 held over STEP latched) */
    if (down) {
        if (ec_lp_first() < 0)
            return;
        if (!g.keys) {                             /* a new entry */
            g.span = 0;
            g.range = on_range_page();
        }
        g.keys |= 1u << i;
        g.span |= 1u << i;
        return;
    }
    if (!(g.keys >> i & 1u))
        return;
    g.keys &= ~(1u << i);
    if (g.keys || ec_lp_first() < 0)
        return;
    for (lo = 0; lo < EC_NLOWER && !(g.span >> lo & 1u); lo++)
        ;
    for (hi = EC_NLOWER - 1; hi > 0 && !(g.span >> hi & 1u); hi--)
        ;
    write_entry(lo, hi);
    g.span = 0;
    A->redraw = 1;
}
