/* SPDX-License-Identifier: GPL-3.0-only */
/* The screen kit (ec_ui_kit.h), ported from the approved mockups (host/mock_ux2.c): the same parts, the same units. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ec_spr_intro.h"
#include "ec_ui_kit.h"

#define W EC_GW
#define H EC_GH
#define R(c, x, y, w, h) ec_gfx_rect((x), (y), (w), (h), (c))
#define T(c, s, x, y, sc, al) ec_gfx_text_al((x), (y), (s), (sc), (c), (al))
#define TW(s, sc) ec_gfx_text_w((s), (sc))

/* ------------------------------------------------------------------------------------------- header --- */
/* a 6x6 head of 2-unit columns (2, 4, 6 high) pointing left or right (0.6.1: no 1-unit tip) */
static void tri(int x, int y, int left, int c)
{
    int k;
    for (k = 0; k < 3; k++)
        R(c, left ? x + 2 * k : x + 4 - 2 * k, y + 2 - k, 2, 2 + 2 * k);
}

void ec_kit_arrows(int x, int y, int c)
{
    tri(x, y, 1, c), R(c, x + 6, y + 2, 2, 2);           /* back: an arrow left */
    tri(x + 12, y, 0, c), R(c, x + 10, y + 2, 2, 2);     /* OK: an arrow right */
}

void ec_kit_header(const char *target, const char *title, const char *page, const char *right, int held, int arrows)
{
    ec_kit_header2(target, 0, title, page, right, held, arrows);
}

void ec_kit_header2(const char *target, const char *steps, const char *title, const char *page, const char *right,
                    int held, int arrows)
{
    const int box = arrows ? 21 : 11;
    const int sb = steps && steps[0] ? EC_KIT_STEPBOX : 0;   /* (2-unit lattice: a gap of 2, an even width) */
    R(EC_C_L12, 0, 0, W, 10);
    R(EC_C_BG, 0, 0, 14, 10);
    T(EC_C_L12, target ? target : "", 7, 3, 1, EC_AL_CENTER);
    if (sb) {                                      /* 0.6.40 (owner): the step page beside the target, its own box */
        R(EC_C_BG, EC_KIT_STEPBOX_X, 0, sb, 10);
        T(EC_C_L12, steps, EC_KIT_STEPBOX_X + sb / 2, 3, 1, EC_AL_CENTER);
    }
    T(EC_C_BG, title ? title : "", 17 + (sb ? EC_KIT_STEPOFF : 0), 3, 1, EC_AL_LEFT);
    R(held && !arrows ? EC_C_LOCK : EC_C_BG, W - box, 0, box, 10);
    if (arrows)
        ec_kit_arrows(W - 19, 2, EC_C_L15);
    else if (held) {
        char b[8];
        snprintf(b, sizeof b, "%d", held);
        T(EC_C_BG, b, W - 6, 3, 1, EC_AL_CENTER);
    } else if (page && page[0])
        T(EC_C_L12, page, W - 6, 3, 1, EC_AL_CENTER);
    if (right && right[0])
        T(EC_C_BG, right, W - box - 3, 3, 1, EC_AL_RIGHT);
}

/* ------------------------------------------------------------------------------------------- cells --- */
/* 0.6.1 (owner, docs/UX-V2.md §10): no 1-unit details in a cell. The bar is 2 units high and lives on a lattice of
 * 2-unit cells (27 across its 54 units), so every fill, tick, mark and gap in it is a multiple of 2 units wide; a
 * tiny non-zero depth still draws one lattice cell (2 units). The cell's box (ec_kit_cell_box): a step lock fills
 * it, "locked on some step" outlines it 2 units thick. Top cells y 76..98, bottom cells y 97..119 (they share rows
 * 97..98); left cells x 0..60, right cells x 59..119 (they share columns 59..60): from the box's top, 2 rows
 * (outline), the label (5), a gap, the value (10), a gap, the bar (2), 2 rows (outline). The rules (2 units, L3):
 * row 76..77 across (the top cells' outline rows) and columns 59..60 down (the shared outline columns); no rule
 * between the top and bottom cells (a 2-unit gap there instead: it would touch the top bars). */
void ec_kit_cell_box(int i, int *x, int *y, int *w, int *h)
{
    *x = i % 2 ? 59 : 0;
    *w = 61;
    *y = i < 2 ? 76 : 97;
    *h = 23;
}

unsigned ec_kit_cells_drawn;

void ec_kit_cells(const ec_kit_cell_t c[4], int lanes, int lane)
{
    int i;
    ec_kit_cells_drawn++;
    R(EC_C_L3, 0, 76, W, 2);
    R(EC_C_L3, 59, 76, 2, 44);
    for (i = 0; i < lanes && lanes > 1; i++)
        R(i == lane ? EC_C_L15 : EC_C_L5, W - 1 - (lanes - i) * 10, 73, 8, 2);
    for (i = 0; i < 4; i++) {
        const unsigned fl = c[i].flags;
        const int lock = (fl & EC_KC_LOCK) != 0, hot = (fl & EC_KC_HOT) != 0;
        const char *val = c[i].value ? c[i].value : "";
        int bx, by, bw, bh, x, y, by0, big, k;
        char tag[4];
        ec_kit_cell_box(i, &bx, &by, &bw, &bh);
        x = bx + 3;
        y = by + 2;                                  /* the label row; the value y+6..y+15, the bar y+17..y+18 */
        by0 = y + EC_KIT_BAR_DY;
        snprintf(tag, sizeof tag, "K%d", i + 1);
        if (!c[i].label || !c[i].label[0]) {
            T(EC_C_L3, tag, x + 54, y, 1, EC_AL_RIGHT);
            continue;
        }
        if (lock) {                                  /* a step lock on the held step: the whole cell in LOCK */
            R(EC_C_LOCK, bx, by, bw, bh);
            y--;                                     /* no outline: label and value a unit higher, so the LOCK */
        }                                            /* between the value and the bar is 2 units, not a 1-unit line */
        else if (fl & EC_KC_SOME) {                  /* locked on some step: a 2-unit outline round the whole cell */
            R(EC_C_LOCK, bx, by, bw, 2);
            R(EC_C_LOCK, bx, by + bh - 2, bw, 2);
            R(EC_C_LOCK, bx, by, 2, bh);
            R(EC_C_LOCK, bx + bw - 2, by, 2, bh);
        }
        T(lock ? EC_C_BG : hot ? EC_C_L15 : EC_C_L8, c[i].label, x, y, 1, EC_AL_LEFT);
        if (c[i].mark && c[i].mark[0]) {             /* a matrix destination: its source's tag and a dot */
            const int room = 54 - TW(c[i].label, 1) - 1;
            if (TW(c[i].mark, 1) + 5 > room)
                R(lock ? EC_C_BG : EC_C_ACC, x + 52, y + 1, 2, 2);
            else {
                T(lock ? EC_C_BG : EC_C_L12, c[i].mark, x + 54, y, 1, EC_AL_RIGHT);
                R(lock ? EC_C_BG : EC_C_ACC, x + 54 - TW(c[i].mark, 1) - 3, y + 1, 2, 2);
            }
        } else if (!hot)
            T(lock ? EC_C_BG : EC_C_L5, tag, x + 54, y, 1, EC_AL_RIGHT);
        big = TW(val, 2) <= 54;
        T(lock ? EC_C_BG : (fl & EC_KC_DIM) ? EC_C_L5 : (fl & EC_KC_ACC) ? EC_C_ACC : EC_C_L15, val, x,
          big ? y + 6 : y + 8, big ? 2 : 1, EC_AL_LEFT);
        /* the bar: the track (L3; on LOCK: dark 2-unit dots), the value (L8; a held source's depth: SRC; on LOCK:
         * dark), on the 2-unit lattice */
        if (lock)
            for (k = 0; k < EC_KIT_BAR_CELLS; k += 2)
                R(EC_C_BG, x + 2 * k, by0, 2, 2);
        else
            R(EC_C_L3, x, by0, 2 * EC_KIT_BAR_CELLS, 2);
        if (fl & EC_KC_DEPTH) {                      /* bipolar: lattice cell 13 is the centre tick */
            const float f = c[i].frac < -1 ? -1 : c[i].frac > 1 ? 1 : c[i].frac;
            int n = (int)(fabsf(f) * 13.0f + 0.5f);
            if (n == 0 && f != 0)
                n = 1;                               /* a tiny depth: one lattice cell, never less */
            if (n)
                R(lock ? EC_C_BG : hot ? EC_C_SRC : EC_C_L8, f >= 0 ? x + 28 : x + 26 - 2 * n, by0, 2 * n, 2);
            R(lock ? EC_C_BG : EC_C_L12, x + 26, by0, 2, 2);
        } else {
            const float f = c[i].frac < 0 ? 0 : c[i].frac > 1 ? 1 : c[i].frac;
            const int n = (int)(f * (float)EC_KIT_BAR_CELLS + 0.5f);
            if (n)
                R(lock ? EC_C_BG : hot ? EC_C_SRC : EC_C_L8, x, by0, 2 * n, 2);
        }
        if (fl & EC_KC_ACTUAL) {                     /* the resolved value, where it differs: one lattice cell */
            const float f = c[i].actual < 0 ? 0 : c[i].actual > 1 ? 1 : c[i].actual;
            R(lock ? EC_C_BG : EC_C_ACC, x + 2 * (int)(f * (float)(EC_KIT_BAR_CELLS - 1) + 0.5f), by0, 2, 2);
        }
    }
}

/* ------------------------------------------------------------------------------------------- the list --- */
#define LIST_X 4                                    /* the label's x (the accent edge is x 0..1) */
#define LIST_R (W - 6)                              /* the value's right edge (the scroll mark is x W-2..W-1) */

static int list_two(const ec_kit_row_t *r)          /* label and value don't fit on one line (or forced) */
{
    const char *l = r->label ? r->label : "", *v = r->value ? r->value : "";
    int lw = TW(l, 1);
    if (r->note)
        lw += 3 + TW(r->note, 1);
    return r->two || LIST_X + lw + 6 + TW(v, 2) > LIST_R;
}

int ec_kit_list_row_h(const ec_kit_row_t *r) { return list_two(r) ? EC_KIT_LIST_H2 : EC_KIT_LIST_H1; }

void ec_kit_list(const ec_kit_row_t *r, int n, int sel, int y0, int y1, int *ys)
{
    const int room = y1 - y0;
    int first = 0, i, y, total = 0, sum;
    if (sel >= n)
        sel = n - 1;
    if (sel < 0)
        sel = 0;
    for (i = 0; i < n; i++)
        total += ec_kit_list_row_h(&r[i]) + EC_KIT_LIST_GAP;
    /* the window: from the top, scrolled just far enough that the selected row (and the next, when there is one) fits */
    for (;;) {
        sum = 0;
        for (i = first; i <= sel + 1 && i < n; i++)
            sum += ec_kit_list_row_h(&r[i]) + EC_KIT_LIST_GAP;
        if (sum - EC_KIT_LIST_GAP <= room || first >= sel)
            break;
        first++;
    }
    if (ys)
        for (i = 0; i < n; i++)
            ys[i] = -1;
    for (i = first, y = y0; i < n; i++) {
        const ec_kit_row_t *q = &r[i];
        const int two = list_two(q), h = two ? EC_KIT_LIST_H2 : EC_KIT_LIST_H1, on = i == sel;
        const char *l = q->label ? q->label : "", *v = q->value ? q->value : "";
        const int vsc = TW(v, 2) <= LIST_R - LIST_X ? 2 : 1;
        int lx;
        if (y + h > y1)
            break;
        if (ys)
            ys[i] = y;
        if (on) {                                    /* the selected row: an L3 band with a 2-unit accent edge */
            R(EC_C_L3, 0, y, W - 4, h);
            R(EC_C_ACC, 0, y, 2, h);
        }
        /* (2 units of band at least around every glyph: no 1-unit strip of it) */
        lx = T(on ? EC_C_L15 : EC_C_L8, l, LIST_X, two ? y + 3 : y + 5, 1, EC_AL_LEFT);
        if (q->note)
            T(EC_C_ALERT, q->note, lx + 3, two ? y + 3 : y + 5, 1, EC_AL_LEFT);
        if (two)                                     /* the value on its own line under the label */
            T(q->dim ? EC_C_L5 : on ? EC_C_L15 : EC_C_L12, v, LIST_R, vsc == 2 ? y + 12 : y + 15, vsc, EC_AL_RIGHT);
        else
            T(q->dim ? EC_C_L5 : on ? EC_C_L15 : EC_C_L12, v, LIST_R, y + 2, 2, EC_AL_RIGHT);
        y += h + EC_KIT_LIST_GAP;
    }
    if (total - EC_KIT_LIST_GAP > room && n > 1) {   /* more rows than fit: where the selection is, at the right */
        const int th = room * room / total < 4 ? 4 : room * room / total;
        const int ty = y0 + (room - th) * sel / (n - 1);
        R(EC_C_L5, W - 2, ty & ~1, 2, th & ~1);
    }
}

/* ------------------------------------------------------------------------------------------- pictures --- */
void ec_kit_wave(const uint8_t *ov, int n, int x0, int x1, int top, int bot, int c_in, int c_out, int rs, int re)
{
    const int mid = (top + bot) / 2, half = (bot - top) / 2, cols = x1 - x0;
    int c;
    if (cols <= 0 || n <= 0)
        return;
    for (c = 0; c + 1 < cols; c += 2) {              /* 0.6.1: 2-unit columns, at least 2 high */
        int lo = c * n / cols, hi = (c + 2) * n / cols, i, pk = 0, h;
        if (hi <= lo)
            hi = lo + 1;
        for (i = lo; i < hi && i < n; i++)
            if (ov[i] > pk)
                pk = ov[i];
        h = (pk * (half - 1) + 127) / 255;
        R(x0 + c >= rs && x0 + c < re ? c_in : c_out, x0 + c, mid - h, 2, 2 * h + 2);
    }
}

void ec_kit_playhead(int x, int top, int bot) { R(EC_C_ACC, x, top, 2, bot - top); }

#define BAR_X(i) (3 + 4 * (i))

/* page_render.lua's response sketches: unity band, roll-off past the cutoff, resonance as a bump at the cutoff */
static float resp_lp(float u, float fc, float q)
{
    const float d = u - fc;
    float base = d <= 0 ? 1.0f : 1.0f - d * 3.5f;
    if (base < 0)
        base = 0;
    return base + q * 1.05f * expf(-(d * d) / 0.005f);
}
static float resp_notch(float u, float fc, float q)
{
    const float d = u - fc, nw = 0.11f - 0.06f * q;
    return 1.0f - 0.95f * expf(-(d * d) / (nw * nw));
}
static float resp_bp(float u, float fc, float q)
{
    const float d = u - fc, bw = 0.20f - 0.13f * q;
    return (0.75f + 0.45f * q) * expf(-(d * d) / (bw * bw));
}
static float response(float u, float fc, float q, int type, float morph)
{
    if (morph >= 0) {
        if (morph < 0.5f) {
            const float t = morph * 2;
            return resp_lp(u, fc, q) * (1 - t) + resp_notch(u, fc, q) * t;
        } else {
            const float t = (morph - 0.5f) * 2;
            return resp_notch(u, fc, q) * (1 - t) + resp_lp(1 - u, 1 - fc, q) * t;
        }
    }
    switch (type) {
    case 2: return resp_lp(1 - u, 1 - fc, q);        /* HP = the mirrored LP */
    case 3: return resp_bp(u, fc, q);
    case 4: return resp_notch(u, fc, q);
    default: return resp_lp(u, fc, q);
    }
}

void ec_kit_filter_bars(int top, int bot, float cut, float res_raw, int type, float morph, float depth, int playing)
{
    const float res = res_raw / 127.0f < 0 ? 0 : res_raw / 127.0f > 1 ? 1 : res_raw / 127.0f;
    const float fc = 0.06f + (cut / 127.0f < 0 ? 0 : cut / 127.0f > 1 ? 1 : cut / 127.0f) * 0.88f;
    const float reach = playing ? 0 : depth * 0.42f, lo = reach >= 0 ? fc : fc + reach, hi = reach >= 0 ? fc + reach : fc;
    const int nearest = (int)floorf(fc * (EC_KIT_NBARS - 1) + 0.5f);
    const float scale = (float)(bot - top - 1) / 1.5f;
    int i;
    for (i = 0; i < EC_KIT_NBARS; i++) {
        const float u = (float)i / (EC_KIT_NBARS - 1);
        float g = response(u, fc, res, type, morph);
        int bh, bright;
        g = g < 0 ? 0 : g > 1.5f ? 1.5f : g;
        bh = (int)floorf(g * scale + 0.5f);
        if (bh < 1)
            continue;
        bh = bh < 2 ? 2 : bh;
        bright = fabsf(reach) > 0 ? (u >= lo && u <= hi) : i == nearest;
        R(bright ? EC_C_L15 : EC_C_L5, BAR_X(i), bot - bh, 2, bh);
    }
}

/* env_bars(): the bar counts per stage */
static int env_bars(int mode, float a, float d, float s, float hd, float r, int n, float *amp, int *st)
{
    const int half = n / 2;
    const float sus = s / 127.0f < 0 ? 0 : s / 127.0f > 1 ? 1 : s / 127.0f;
    int m = 0;
#define SPAN(v, mx) ((int)floorf((v) / (mx) * half + 0.5f))
#define PUSH(cnt, from, to, stg)                                                                                      \
    do {                                                                                                               \
        int _i;                                                                                                        \
        for (_i = 1; _i <= (cnt) && m < n; _i++)                                                                       \
            amp[m] = (from) + ((to) - (from)) * (float)_i / (float)(cnt), st[m++] = (stg);                             \
    } while (0)
    if (mode != 2) {
        int ab = SPAN(a, 127), db = SPAN(d, 127), rb, sb;
        if (ab + db > half) {
            const float sc = (float)half / (float)(ab + db);
            ab = (int)floorf(ab * sc), db = (int)floorf(db * sc);
        }
        ab = ab < 1 ? 1 : ab;
        rb = r >= 128 ? 0 : SPAN(r, 127) < half ? SPAN(r, 127) : half;
        sb = n - ab - db - rb;
        if (sb < 1) {
            int need = 1 - sb, take = need < rb ? need : rb;
            rb -= take, need -= take;
            if (need > 0)
                take = need < db ? need : db, db -= take, need -= take;
            if (need > 0)
                ab = ab - need < 1 ? 1 : ab - need;
            sb = n - ab - db - rb;
        }
        PUSH(ab, 0.0f, 1.0f, EC_KIT_ST_A);
        PUSH(db, 1.0f, sus, EC_KIT_ST_D);
        PUSH(sb, sus, sus, EC_KIT_ST_S);
        PUSH(rb, sus, 0.0f, EC_KIT_ST_R);
    } else {
        int ab = SPAN(a, 127);
        ab = ab < 1 ? 1 : ab > half ? half : ab;
        if (hd >= 128) {
            PUSH(ab, 0.0f, 1.0f, EC_KIT_ST_A);
            PUSH(n - ab, 1.0f, 1.0f, EC_KIT_ST_H);
        } else if (r >= 128) {
            const int hb = SPAN(hd, 128) < n - ab ? SPAN(hd, 128) : n - ab;
            PUSH(ab, 0.0f, 1.0f, EC_KIT_ST_A);
            PUSH(hb, 1.0f, 1.0f, EC_KIT_ST_H);
            PUSH(n - ab - hb, 1.0f, 1.0f, EC_KIT_ST_R);
        } else {
            const int hb = SPAN(hd, 128) < half ? SPAN(hd, 128) : half, rb = SPAN(r, 128) < half ? SPAN(r, 128) : half;
            PUSH(ab, 0.0f, 1.0f, EC_KIT_ST_A);
            PUSH(hb, 1.0f, 1.0f, EC_KIT_ST_H);
            PUSH(rb, 1.0f, 0.0f, EC_KIT_ST_R);
        }
    }
#undef SPAN
#undef PUSH
    return m;
}

void ec_kit_env_bars(int top, int bot, int mode, float a, float d, float s, float hd, float r, unsigned hot)
{
    float amp[EC_KIT_NBARS];
    int st[EC_KIT_NBARS], n = env_bars(mode, a, d, s, hd, r, EC_KIT_NBARS, amp, st), i;
    for (i = 0; i < n; i++) {
        int bh = (int)floorf((amp[i] < 0 ? 0 : amp[i] > 1 ? 1 : amp[i]) * (bot - top - 1) + 0.5f);
        bh = bh == 1 ? 2 : bh;
        if (bh >= 1)
            R((unsigned)st[i] & hot ? EC_C_L15 : EC_C_L5, BAR_X(i), bot - bh, 2, bh);
    }
}

void ec_kit_grains(const ec_grain_view_t *g, int n, int x0, int x1, int top)
{
    const float cols = (float)(x1 - x0);
    int k, pass;
    if (n > EC_GRAIN_VIEW_MAX)
        n = EC_GRAIN_VIEW_MAX;
    for (pass = 0; pass < 2; pass++)                 /* the newest last, so it sits on top */
        for (k = 0; k < n; k++) {
            const float lv = g[k].level;
            const int newest = g[k].newest != 0;
            int w, x, col;
            if (newest != pass)
                continue;
            w = (int)(g[k].len * cols + 0.5f);
            w = w < 2 ? 2 : w;
            x = x0 + (int)(g[k].pos * cols - (float)w / 2.0f + 0.5f);
            if (x < x0)
                w -= x0 - x, x = x0;
            if (x + w > x1)
                w = x1 - x;
            if (w < 2)                               /* clipped to a sliver at an edge: 2 units inside it */
                w = 2, x = x < x0 + 1 ? x0 : x1 - 2;
            col = newest ? EC_C_ACC : lv > 0.75f ? EC_C_L15 : lv > 0.45f ? EC_C_L12 : lv > 0.2f ? EC_C_L8 : EC_C_L5;
            R(col, x, top + ((k * 5) % 9) * 4, w, 2);
            if (w >= 6)                              /* 0.6.15 (owner): its direction: a blue head at the leading end */
                R(EC_C_SRC, g[k].dir < 0 ? x : x + w - 2, top + ((k * 5) % 9) * 4, 2, 2);
        }
}

void ec_kit_grain_frame(int x, int y, int w, int h, int ex)
{
    ec_gfx_frame(x, y, w, h, EC_C_L8);
    if (ex >= 0)
        R(EC_C_ACC, ex, y, 2, 4), R(EC_C_ACC, ex, y + h - 4, 2, 4);
}

void ec_kit_steps(int top, int bot, uint16_t trigs, uint16_t ghosts, uint16_t locks, uint16_t beyond, int held,
                  int play)
{
    int i;
    for (i = 0; i < 16; i++) {
        const int x = 3 + i * 7;
        if (beyond >> i & 1u) {
            R(EC_C_L3, x + 1, bot - 2, 2, 2);
            continue;
        }
        if (trigs >> i & 1u)
            R(i == held ? EC_C_L15 : EC_C_L12, x, top, 5, bot - top);
        else if (ghosts >> i & 1u)                   /* 0.6.1: a ghost is a dim block (a 2-unit frame would leave a
                                                      * 1-unit hole in a 5-unit step) */
            R(i == held ? EC_C_L15 : EC_C_L5, x, top, 5, bot - top);
        else
            R(i == held ? EC_C_L15 : i % 4 == 0 ? EC_C_L5 : EC_C_L3, x, bot - 2, 5, 2);
        if (i == held)                               /* the held step: a bright cap over it (0.6.1: no frame) */
            R(EC_C_L15, x, top - 4, 5, 2);
        if (locks >> i & 1u)
            R(EC_C_LOCK, x + 1, bot + 2, 3, 2);
        if (i == play)
            R(EC_C_ACC, x, top - 4, 5, 2);
    }
}

void ec_kit_key_ruler(int y, uint16_t lit, uint16_t in_range, int n)
{
    int i;
    for (i = 0; i < 16 && i < n; i++) {
        const int x = 3 + i * 7;
        if (lit >> i & 1u)
            R(EC_C_ACC, x, y, 5, 4);
        else if (in_range >> i & 1u)
            R(i % 4 ? EC_C_L8 : EC_C_L12, x, y, 5, i % 4 ? 2 : 4);
        else
            R(i % 4 ? EC_C_L3 : EC_C_L5, x, y, 5, 2);
    }
}

void ec_kit_track_keys(int y, int sel, uint16_t muted)
{
    static const char *const N[9] = {"T1", "T2", "T3", "T4", "T5", "T6", "S1", "S2", "MST"};
    int i;
    for (i = 0; i < 9; i++) {
        const int x = 3 + i * 13;
        if (i == sel) {
            R(EC_C_L15, x, y, 12, 11);
            T(EC_C_BG, N[i], x + 6, y + 3, 1, EC_AL_CENTER);
        } else
            T(muted >> i & 1u ? EC_C_L5 : EC_C_L8, N[i], x + 6, y + 3, 1, EC_AL_CENTER);
    }
}

/* ------------------------------------------------------------------------------------------- sprites --- */
int ec_scr_intro_frames(void) { return EC_SPR_INTRO_FRAMES; }

void ec_kit_sprite(const uint8_t *packed, int w, int h, int x, int y) { ec_gfx_blit4(packed, w, h, x, y, -1); }

void ec_scr_intro(int frame)
{
    frame = frame < 0 ? 0 : frame >= EC_SPR_INTRO_FRAMES ? EC_SPR_INTRO_FRAMES - 1 : frame;
    ec_gfx_fill(EC_C_BG);
    ec_kit_sprite(ec_spr_intro[frame], EC_SPR_INTRO_W, EC_SPR_INTRO_H, (W - EC_SPR_INTRO_W) / 2, 28);
}

