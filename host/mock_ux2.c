/* SPDX-License-Identifier: GPL-3.0-only */
/* The 0.6.0 mockups (docs/UX-V2.md): every mode's screen and its mode-entry pop-up, the APP SETTINGS list, the palette
 * sheet and the theme sheet. Mockup tooling only: nothing here is firmware behaviour.
 *
 * The owner's rules (2026-11-03): all UI is designed at 120x120 and scaled by exactly 2, no 1-pixel details, and a
 * 16-colour palette. So everything is drawn as 4-bit palette indices into a 120x120 buffer, in the mockup grid's units
 * (the same units ec_screens.c writes inside its R() / T() macros), with the firmware's own 3x5 font (ec_gfx_glyph) and
 * the real screens' geometry (the header, the four cells); then expanded through one theme table (host/mock_palette.h)
 * to the LCD's RGB565 at 2x and written by the harness's PNG writer at 2x again (480x480, as results/ already are).
 *
 *   ec_mock_ux2 <out dir>     -> <Mode>_screen.png, <Mode>_popup.png, AppSettings_screen.png, _palette.png,
 *                                _themes.png, palette.json (the tables, for tools/mockups/check_png.py)
 * Exit 1 if a buffer breaks the 2x2 rule (it cannot by construction; checked anyway on the expanded LCD). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ec_gfx.h"
#include "ec_png.h"
#include "mock_palette.h"

#define W 120
#define H 120

/* --------------------------------------------------------------------------------- the indexed kit --- */
static uint8_t *g_px;
static int g_w = W, g_h = H;
static uint8_t g_screen[W * H];

static void canvas(uint8_t *px, int w, int h) { g_px = px, g_w = w, g_h = h; }

static void R(int c, int x, int y, int w, int h)
{
    int i, j;
    if (x < 0) w += x, x = 0;
    if (y < 0) h += y, y = 0;
    if (x + w > g_w) w = g_w - x;
    if (y + h > g_h) h = g_h - y;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            g_px[(y + j) * g_w + x + i] = (uint8_t)c;
}
static void FR(int c, int x, int y, int w, int h)   /* a one-unit outline (2 physical pixels) */
{
    R(c, x, y, w, 1), R(c, x, y + h - 1, w, 1), R(c, x, y, 1, h), R(c, x + w - 1, y, 1, h);
}
static int TW(const char *s, int sc) { const int n = (int)strlen(s); return n ? n * 4 * sc - sc : 0; }
/* the firmware's 3x5 font (ec_gfx_glyph), sc grid units per font pixel: sc 1 = the screens' small text (EC_U), 2 = T2 */
static int T(int c, const char *s, int x, int y, int sc, int al)
{
    const int w = TW(s, sc);
    x = al == EC_AL_RIGHT ? x - w : al == EC_AL_CENTER ? x - w / 2 : x;
    for (; *s; s++, x += 4 * sc) {
        const int g = ec_gfx_glyph(*s);
        int r, k;
        for (r = 0; r < 5; r++)
            for (k = 0; k < 3; k++)
                if (g >> (14 - (r * 3 + k)) & 1)
                    R(c, x + k * sc, y + r * sc, sc, sc);
    }
    return x;
}
#define TL(c, s, x, y) T(c, s, x, y, 1, EC_AL_LEFT)
#define TR(c, s, x, y) T(c, s, x, y, 1, EC_AL_RIGHT)

static uint32_t g_rng = 1;
static float rnd(void) { g_rng = g_rng * 1664525u + 1013904223u; return (float)(g_rng >> 8) / 16777216.0f; }

/* --------------------------------------------------------------------------------- the real screens' parts --- */
/* the header (ec_screens.c header()): the track box, the title on the cream bar, the right info, the page box (or a
 * pop-up's back / OK arrows, ec_scr_arrows; or the count of held steps in the lock colour) */
static void tri(int x, int y, int left, int c)
{
    int k;
    for (k = 0; k < 4; k++)
        R(c, left ? x + k : x + 3 - k, y + 3 - k, 1, 1 + 2 * k);
}
static void header(const char *track, const char *title, const char *page, const char *right, int held, int arrows)
{
    const int box = arrows ? 21 : 11;
    R(PX_L12, 0, 0, W, 10);
    R(PX_BG, 0, 0, 14, 10);
    T(PX_L12, track, 7, 3, 1, EC_AL_CENTER);
    TL(PX_BG, title, 17, 3);
    R(held ? PX_LOCK : PX_BG, W - box, 0, box, 10);
    if (arrows) {
        tri(W - 18, 2, 1, PX_L15), R(PX_L15, W - 15, 4, 3, 1);
        tri(W - 7, 2, 0, PX_L15), R(PX_L15, W - 10, 4, 3, 1);
    } else if (held) {
        char b[4];
        snprintf(b, sizeof b, "%d", held);
        T(PX_BG, b, W - 6, 3, 1, EC_AL_CENTER);
    } else if (page)
        T(PX_L12, page, W - 6, 3, 1, EC_AL_CENTER);
    if (right && right[0])
        TR(PX_BG, right, W - box - 3, 3);
}

/* a cell (ec_screens.c cells()): label, K-tag (or a matrix mark: the source's tag and an orange dot), the value big
 * when it fits, the bar; flags: L = locked on the held step (the lock chip), A = the value in the accent,
 * D = a bipolar DEPTH bar from the centre */
typedef struct {
    const char *l, *v;
    float f;
    const char *fl, *mark;
} cell_t;

static void cells(const cell_t c[4], int lanes, int lane)
{
    int i;
    R(PX_L3, 0, 76, W, 1);
    R(PX_L3, 60, 76, 1, 44);
    R(PX_L3, 0, 98, W, 1);
    for (i = 0; i < lanes && lanes > 1; i++)
        R(i == lane ? PX_L15 : PX_L5, W - 1 - (lanes - i) * 10, 72, 8, 3);
    for (i = 0; i < 4; i++) {
        const int x = (i % 2) * 61 + 3, y = i < 2 ? 79 : 101;
        const char *fl = c[i].fl ? c[i].fl : "";
        const int lock = strchr(fl, 'L') != 0, acc = strchr(fl, 'A') != 0, dep = strchr(fl, 'D') != 0;
        char tag[4];
        int big;
        snprintf(tag, sizeof tag, "K%d", i + 1);
        if (!c[i].l || !c[i].l[0]) {
            TR(PX_L3, tag, x + 54, y);
            continue;
        }
        if (lock) {                                  /* a step lock on the held step: the whole cell in LOCK */
            const int fx = i % 2 ? 61 : 0, fy = i < 2 ? 77 : 99;
            R(PX_LOCK, fx, fy, i % 2 ? W - 61 : 60, 21);
        }
        TL(lock ? PX_BG : PX_L8, c[i].l, x, y);
        if (c[i].mark) {
            TR(lock ? PX_BG : PX_L12, c[i].mark, x + 54, y);
            R(lock ? PX_BG : PX_ACC, x + 54 - TW(c[i].mark, 1) - 3, y + 1, 2, 2);
        } else
            TR(lock ? PX_BG : PX_L5, tag, x + 54, y);
        big = TW(c[i].v, 2) <= 54;
        T(lock ? PX_BG : acc ? PX_ACC : PX_L15, c[i].v, x, big ? y + 7 : y + 9, big ? 2 : 1, EC_AL_LEFT);
        if (lock) {                                  /* the bar: a dotted track, the value solid, both dark */
            int k;
            for (k = 0; k < 54; k += 2)
                R(PX_BG, x + k, y + 18, 1, 1);
        } else
            R(PX_L3, x, y + 18, 54, 1);
        if (dep) {
            const int w = (int)(fabsf(c[i].f) * 27.0f + 0.5f);
            R(lock ? PX_BG : PX_L8, c[i].f >= 0 ? x + 27 : x + 27 - w, y + 18, w, 1);
            R(lock ? PX_BG : PX_L12, x + 27, y + 17, 1, 3);
        } else
            R(lock ? PX_BG : PX_L8, x, y + 18, (int)(c[i].f * 54.0f + 0.5f), 1);
    }
}

/* the settings list (ec_screens.c settings_screen()): 13-unit rows, label left, value big right, K-tag, dotted rule */
typedef struct {
    const char *l, *v;
    int dim;
} row_t;
static void settings_list(const row_t *r, int n, int active)
{
    int i;
    for (i = 0; i < n; i++) {
        const int y = 14 + i * 13, act = i / 4 == active / 4;
        char tag[4];
        int x;
        snprintf(tag, sizeof tag, "K%d", i % 4 + 1);
        if (i == active) {
            R(PX_L3, 0, y - 1, W, 12);
            R(PX_ACC, 0, y - 1, 1, 12);
        }
        TL(i == active ? PX_L15 : act ? PX_L8 : PX_L5, r[i].l, 3, y + 3);
        T(r[i].dim ? PX_L5 : act ? PX_L15 : PX_L8, r[i].v, W - 22, y, 2, EC_AL_RIGHT);
        TR(act ? PX_L5 : PX_L3, tag, W - 2, y + 3);
        for (x = 2; x < W - 2; x += 2)
            R(PX_L3, x, y + 12, 1, 1);
    }
}

/* ------------------------------------------------------------------------------------------- pictures --- */
/* a drum loop's waveform: hits every `per` columns, decaying; colour inside [a, b), dim outside */
static void wave(int x0, int x1, int top, int bot, int per, uint32_t seed, int c_in, int c_out, int a, int b)
{
    const int mid = (top + bot) / 2, half = (bot - top) / 2;
    int x;
    g_rng = seed;
    for (x = x0; x < x1; x++) {
        const int ph = (x - x0) % per;
        const float env = expf(-(float)ph / ((float)per * 0.35f)) * (0.55f + 0.45f * rnd());
        int h = (int)(env * half + 0.5f);
        if (h < 1)
            h = 1;
        R(x >= a && x < b ? c_in : c_out, x, mid - h, 1, 2 * h);
    }
}
static void playhead(int x, int top, int bot) { R(PX_ACC, x, top, 1, bot - top); }

/* a strip of 16 steps (TRIG's picture): trig bars, dashes for empty steps, the held one framed, lock dots */
static void step_bars(const char *pat, int top, int bot, int held, const char *locks)
{
    int i;
    for (i = 0; i < 16; i++) {
        const int x = 3 + i * 7;
        if (pat[i] == 'x')
            R(i == held ? PX_L15 : PX_L12, x, top, 5, bot - top);
        else
            R(PX_L3, x, bot - 2, 5, 2);
        if (i == held)
            FR(PX_L15, x - 2, top - 2, 9, bot - top + 4);
        if (locks && locks[i] == 'o')
            R(PX_LOCK, x + 1, bot + 4, 3, 2);
    }
}

/* ------------------------------------------------------------------------------------------- the modes --- */
static void scr_main(void)
{
    static const cell_t c[4] = {{"RATE", "1.00", 0.5f, 0, 0}, {"PITCH", "+7", 0.6f, "L", 0},
                                {"START", "0%", 0, 0, 0}, {"LEN", "100%", 1, 0, 0}};
    int i;
    header("T1", "LOOP/LOOP", "1", "S05", 1, 0);
    TL(PX_L15, "AMEN_170.WAV", 2, 13);
    TR(PX_L8, "120", W - 2, 13);
    wave(2, 118, 20, 58, 14, 7, PX_L15, PX_L15, 0, W);
    playhead(37, 20, 58);
    for (i = 0; i < 16; i++) {                       /* the steps: trigs, the held one, its lock */
        const int x = 3 + i * 7, trig = (0x9535 >> i) & 1;
        R(i == 4 ? PX_L15 : trig ? PX_L12 : PX_L3, x, 63, 5, trig || i == 4 ? 6 : 2);
        if (i == 4)
            FR(PX_L15, x - 2, 61, 9, 10);
    }
    R(PX_LOCK, 3 + 4 * 7 + 1, 72, 3, 2);
    cells(c, 2, 0);
}

static void scr_shift(void)
{
    static const char *const N[9] = {"T1", "T2", "T3", "T4", "T5", "T6", "S1", "S2", "MST"};
    static const cell_t c[4] = {{"BPM", "120.0", 0.5f, 0, 0}, {"SWING", "54", 0.54f, 0, 0},
                                {"TRACK", "T3", 0.25f, "A", 0}, {"SNAP", "K1-K4", 0, 0, 0}};
    int i;
    header("T3", "SHIFT", "", "", 0, 0);
    TL(PX_L8, "BPM", 3, 14);
    T(PX_L15, "120.0", 3, 21, 3, EC_AL_LEFT);
    TR(PX_L8, "SWING", W - 3, 14);
    T(PX_L15, "54", W - 3, 21, 3, EC_AL_RIGHT);
    for (i = 0; i < 9; i++) {                        /* the track keys W1..W9 */
        const int x = 3 + i * 13;
        if (i == 2) {
            R(PX_L15, x, 46, 12, 11);
            T(PX_BG, N[i], x + 6, 49, 1, EC_AL_CENTER);
        } else {
            FR(i < 6 ? PX_L8 : PX_L5, x, 46, 12, 11);
            T(i < 6 ? PX_L12 : PX_L8, N[i], x + 6, 49, 1, EC_AL_CENTER);
        }
    }
    for (i = 0; i < 9; i++)                           /* the meters under them */
        R(i == 2 ? PX_L15 : PX_L5, 4 + i * 13, 62, (int)(10 * (0.3f + 0.7f * ((i * 37) % 10) / 10.0f)), 3);
    cells(c, 0, 0);
}

static void scr_fx(void)
{
    static const char *const S[5] = {"INS 1", "INS 2", "SEND 1", "SEND 2", "MASTER"};
    static const char *const E[5] = {"BITCRUSH", "DELAY", "REVERB", "TAPE", "COMP"};
    static const float M[5] = {0.4f, 0.25f, 0.6f, 0.3f, 1.0f};
    static const cell_t c[4] = {{"BITS", "8", 0.5f, 0, 0}, {"RATE", "22K", 0.45f, 0, 0},
                                {"MIX", "40%", 0.4f, 0, "LFO1"}, {"TONE", "+2", 0.55f, 0, 0}};
    int i;
    header("T1", "FX/INSERT 1", "1", "P01", 0, 0);
    for (i = 0; i < 5; i++) {
        const int y = 12 + i * 12, on = i == 0;
        if (on)
            R(PX_L3, 0, y, W, 12);
        TL(on ? PX_L15 : PX_L8, S[i], 3, y + 4);
        T(on ? PX_L15 : PX_L12, E[i], 30, y + 1, 2, EC_AL_LEFT);
        R(PX_L5, 100, y + 5, 17, 1);                 /* the slot's mix */
        R(on ? PX_ACC : PX_L12, 100, y + 4, (int)(17 * M[i] + 0.5f), 3);
    }
    cells(c, 0, 0);
}

/* ---- the norns bar renders (Elasticat/lib/ui/page_render.lua: draw_filter_bars, env_bars, draw_envelope_bars) ----
 * norns draws 42 bars of 2 px + a 1 px gap across 128 px; on the 120 grid that rhythm fits 38 bars (x 3..116). The
 * rest is the norns logic: bar heights from the same response / stage maths, dim bars at level 6 (L5), the bright
 * ones at level 12 (L15). */
#define NBARS 38
#define BAR_X(i) (3 + 3 * (i))

static float resp_lp(float u, float fc, float q)
{
    const float d = u - fc;
    float base = d <= 0 ? 1.0f : 1.0f - d * 3.5f;
    if (base < 0)
        base = 0;
    return base + q * 1.05f * expf(-(d * d) / 0.005f);
}

/* the filter's magnitude as bars from the bottom; stopped: the envelope-depth band from the cutoff bright, else the
 * cutoff's bar alone */
static void filter_bars(int top, int bot, float cut, float res_raw, float depth, int playing)
{
    const float res = res_raw / 127.0f, fc = 0.06f + cut / 127.0f * 0.88f;
    const float reach = playing ? 0 : depth * 0.42f, lo = reach >= 0 ? fc : fc + reach, hi = reach >= 0 ? fc + reach : fc;
    const int nearest = (int)floorf(fc * (NBARS - 1) + 0.5f);
    const float scale = (float)(bot - top - 1) / 1.5f;
    int i;
    for (i = 0; i < NBARS; i++) {
        const float u = (float)i / (NBARS - 1);
        float g = resp_lp(u, fc, res);
        int bh, bright;
        g = g < 0 ? 0 : g > 1.5f ? 1.5f : g;
        bh = (int)floorf(g * scale + 0.5f);
        if (bh < 1)
            continue;
        bright = fabsf(reach) > 0 ? (u >= lo && u <= hi) : i == nearest;
        R(bright ? PX_L15 : PX_L5, BAR_X(i), bot - bh, 2, bh);
    }
}

/* env_bars(): the envelope as one amplitude per bar, by bar counts (attack >= 1 bar, attack + decay <= half,
 * release <= half, sustain >= 1 bar, a 0 stage = 0 bars = a cliff); mode 1 ADSR, 2 AHR; >= 128 = INF */
enum { ST_A = 1, ST_D = 2, ST_S = 4, ST_R = 8, ST_H = 16 };
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
    if (mode == 1) {
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
        PUSH(ab, 0.0f, 1.0f, ST_A);
        PUSH(db, 1.0f, sus, ST_D);
        PUSH(sb, sus, sus, ST_S);
        PUSH(rb, sus, 0.0f, ST_R);
    } else {
        int ab = SPAN(a, 127);
        ab = ab < 1 ? 1 : ab > half ? half : ab;
        if (hd >= 128) {
            PUSH(ab, 0.0f, 1.0f, ST_A);
            PUSH(n - ab, 1.0f, 1.0f, ST_H);
        } else if (r >= 128) {
            const int hb = SPAN(hd, 128) < n - ab ? SPAN(hd, 128) : n - ab;
            PUSH(ab, 0.0f, 1.0f, ST_A);
            PUSH(hb, 1.0f, 1.0f, ST_H);
            PUSH(n - ab - hb, 1.0f, 1.0f, ST_R);
        } else {
            const int hb = SPAN(hd, 128) < half ? SPAN(hd, 128) : half, rb = SPAN(r, 128) < half ? SPAN(r, 128) : half;
            PUSH(ab, 0.0f, 1.0f, ST_A);
            PUSH(hb, 1.0f, 1.0f, ST_H);
            PUSH(rb, 1.0f, 0.0f, ST_R);
        }
    }
#undef SPAN
#undef PUSH
    return m;
}

/* draw_envelope_bars(): the stages the selected knob pair edits are bright */
static void envelope_bars(int top, int bot, int mode, float a, float d, float s, float hd, float r, int hot)
{
    float amp[NBARS];
    int st[NBARS], n = env_bars(mode, a, d, s, hd, r, NBARS, amp, st), i;
    for (i = 0; i < n; i++) {
        const int bh = (int)floorf((amp[i] < 0 ? 0 : amp[i] > 1 ? 1 : amp[i]) * (bot - top - 1) + 0.5f);
        if (bh >= 1)
            R(st[i] & hot ? PX_L15 : PX_L5, BAR_X(i), bot - bh, 2, bh);
    }
}

static void scr_filter(void)
{
    static const cell_t c[4] = {{"CUTOFF", "2.4K", 0.62f, 0, "LFO1"}, {"RESO", "35", 0.35f, 0, 0},
                                {"TYPE", "LP", 0, 0, 0}, {"DRIVE", "0", 0, 0, 0}};
    header("T1", "FILTER/FILTER", "1", "P01", 0, 0);
    TL(PX_L15, "CLASSC", 2, 13);
    TR(PX_L8, "LP", W - 2, 13);
    filter_bars(20, 75, 80, 35, 0.25f, 0);           /* stopped: the env depth (+25 %) band bright */
    cells(c, 0, 0);
}

static void scr_filter_env(void)
{
    static const cell_t c[4] = {{"ATTACK", "12MS", 0.08f, 0, 0}, {"HOLD", "80MS", 0.23f, 0, 0},
                                {"RELEASE", "300MS", 0.55f, 0, 0}, {"DEPTH", "+25%", 0.25f, "D", 0}};
    header("T1", "FILTER/FILTER ENV", "2", "", 0, 0);
    TL(PX_L15, "CLASSC", 2, 13);
    TR(PX_L8, "AHR", W - 2, 13);
    envelope_bars(20, 75, 2, 10, 0, 127, 30, 70, ST_R);   /* K3 / K4 selected: RELEASE (and DEPTH) */
    cells(c, 2, 1);
}

static void scr_amp(void)
{
    static const cell_t c[4] = {{"ATTACK", "4MS", 0.05f, 0, 0}, {"DECAY", "220MS", 0.35f, 0, 0},
                                {"SUST", "70%", 0.7f, 0, 0}, {"RELEASE", "380MS", 0.45f, 0, 0}};
    header("T1", "AMP/AMP ENV", "2", "P01", 0, 0);
    TL(PX_L15, "ADSR", 2, 13);
    TR(PX_L8, "VEL 100", W - 2, 13);
    envelope_bars(20, 75, 1, 2, 40, 89, 0, 60, ST_A | ST_D);   /* K1 / K2 selected: ATTACK, DECAY */
    cells(c, 2, 0);
}

static void scr_mod_env(void)
{
    static const cell_t c[4] = {{"ATTACK", "30MS", 0.19f, 0, 0}, {"DECAY", "0MS", 0, 0, 0},
                                {"SUST", "55%", 0.55f, 0, 0}, {"RELEASE", "160MS", 0.39f, 0, 0}};
    header("T1", "AMP/MOD ENV", "3", "P01", 0, 0);
    TL(PX_L15, "ADSR", 2, 13);
    TR(PX_L8, "DECAY 0", W - 2, 13);
    envelope_bars(20, 75, 1, 24, 0, 70, 0, 50, ST_S | ST_R);   /* DECAY 0 = the cliff; K3 / K4: SUST, RELEASE */
    cells(c, 2, 1);
}

static void scr_mod(void)
{
    static const char *const S[8] = {"LFO1", "M1", "ENV", "--", "--", "--", "--", "--"};
    static const char *const D[8] = {"CUTOFF", "PITCH", "START", "--", "--", "--", "--", "--"};
    static const char *const V[8] = {"+25%", "+12%", "-40%", "0", "0", "0", "0", "0"};
    static const float F[8] = {0.25f, 0.12f, -0.4f, 0, 0, 0, 0, 0};
    static const cell_t c[4] = {{"SRC", "LFO1", 0.2f, 0, 0}, {"DST", "CUTOFF", 0.1f, 0, 0},
                                {"DEPTH", "+25%", 0.25f, "D", 0}, {0, 0, 0, 0, 0}};
    int i;
    header("T1", "MOD/MATRIX", "1", "P01", 0, 0);
    for (i = 0; i < 8; i++) {                        /* the matrix page's rows (results/mod-matrix/matrix_page.png) */
        const int y = 12 + i * 8, on = i == 0, used = i < 3;
        char n[4];
        const int w = (int)(fabsf(F[i]) * 16 + 0.5f);
        if (on)
            R(PX_L3, 0, y, W, 8);
        snprintf(n, sizeof n, "%d", i + 1);
        TL(on ? PX_L15 : PX_L5, n, 2, y + 2);
        TL(on ? PX_L15 : used ? PX_L12 : PX_L5, S[i], 9, y + 2);
        TL(on ? PX_L15 : used ? PX_L12 : PX_L5, D[i], 29, y + 2);
        TR(on ? PX_L15 : used ? PX_L12 : PX_L5, V[i], 78, y + 2);
        R(PX_L5, 84, y + 4, 34, 1);
        R(on ? PX_ACC : used ? PX_L12 : PX_L5, F[i] >= 0 ? 101 : 101 - w, y + 3, w, 3);
        R(PX_L12, 101, y + 2, 1, 5);
    }
    cells(c, 0, 0);
}

static void scr_mix(void)
{
    static const char *const N[9] = {"T1", "T2", "T3", "T4", "T5", "T6", "S1", "S2", "M"};
    static const float LV[9] = {0.8f, 0.6f, 0.9f, 0.3f, 0.0f, 0.55f, 0.6f, 0.2f, 0.85f};
    static const float VOL[9] = {0.75f, 0.6f, 0.8f, 0.7f, 0.7f, 0.65f, 0.5f, 0.5f, 0.8f};
    static const char MS[9] = {'M', 0, 'S', 0, 'M', 0, 0, 0, 0};
    static const cell_t c[4] = {{"LEVEL", "-4.0", 0.7f, 0, 0}, {"PAN", "C", 0.5f, 0, 0},
                                {"SEND 1", "20%", 0.2f, 0, 0}, {"SEND 2", "0%", 0, 0, 0}};
    const int top = 19, mh = 42;
    int i;
    header("T2", "MIX", "", "1X", 0, 0);
    for (i = 0; i < 9; i++) {                        /* ec_screens.c mix_screen(), shortened for the cells */
        const int x = 2 + i * 13, cx = x + 6, x0 = cx - 5, h = (int)(mh * LV[i] + 0.5f);
        const int muted = MS[i] == 'M';
        char b[2] = {MS[i], 0};
        if (MS[i])
            T(muted ? PX_ACC : PX_L15, b, cx, 12, 1, EC_AL_CENTER);
        R(PX_L3, x0, top, 4, mh);
        R(muted ? PX_L5 : PX_L12, x0, top + mh - h, 4, h);
        if (LV[i] > 0.78f)
            R(PX_ACC, x0, top + mh - h, 4, 2);
        R(PX_L5, cx + 1, top, 1, mh);
        R(PX_L15, cx, top + (int)(mh * (1 - VOL[i]) + 0.5f) - 1, 4, 3);
        if (i == 1) {
            R(PX_L15, x + 1, 63, 11, 9);
            T(PX_BG, N[i], cx, 65, 1, EC_AL_CENTER);
        } else
            T(muted ? PX_L5 : PX_L8, N[i], cx, 65, 1, EC_AL_CENTER);
    }
    cells(c, 0, 0);
}

static void scr_pages(void)
{
    static const uint16_t PG[4] = {0x5151, 0x4949, 0x5555, 0x0000};
    static const cell_t c[4] = {{"LENGTH", "32", 0.5f, 0, 0}, {"RATE", "1X", 0.5f, 0, 0},
                                {"LOOP", "1-2", 0.5f, 0, 0}, {"CHAIN", "--", 0, 0, 0}};
    int p, j;
    header("T1", "PAGES", "2", "P01", 0, 0);
    for (p = 0; p < 4; p++) {
        const int y = 13 + p * 15, sel = p == 1, loop = p < 2;
        char n[2] = {(char)('1' + p), 0};
        if (sel) {
            R(PX_L15, 2, y, 11, 12);
            T(PX_BG, n, 8, y + 1, 2, EC_AL_CENTER);
        } else
            T(loop ? PX_L12 : PX_L5, n, 8, y + 1, 2, EC_AL_CENTER);
        if (loop)
            R(PX_ACC, 15, y, 1, 12);                  /* in the loop */
        for (j = 0; j < 16; j++) {
            const int x = 18 + j * 6 + (j / 4) * 1, on = PG[p] >> (15 - j) & 1;
            R(on ? (loop ? PX_L12 : PX_L5) : PX_L3, x, y + (on ? 1 : 8), 5, on ? 10 : 3);
        }
    }
    cells(c, 0, 0);
}

/* LOOP ENGINE / WARP with the GRAIN warp: the live grains over the waveform (docs/UX-V2.md, "Live grains"). Modelled on
 * src/dsp/ec_grains.c: GDEN grains overlap (spawned every GSIZ / GDEN), each spawned at the emitter +- 2 * SPRD of the
 * file (gspread_lag * frames) and reading on at RATE for GSIZ; its level is the Hann window at its phase. Here: a
 * 1.41 s loop over 112 units (79 units / s), GSIZ 80 ms = 6 units, SPRD 60 ms = +-13 units, 14 grains. Each grain is
 * a block centred on its read position, as wide as its length, in a lane of its own (the scatter), shaded by level
 * through the grey ramp, the newest in the accent; the emitter is the accent notches on the frame. */
static void scr_loop(void)
{
    static const cell_t c[4] = {{"GDEN", "14", 14 / 64.0f, 0, 0}, {"GSIZ", "80MS", 0.16f, 0, 0},
                                {"SPRD", "60MS", 0.24f, 0, 0}, {"RATE", "1X", 0.12f, 0, 0}};
    const float ups = 112.0f / 1.41f, len = 0.08f * ups, spread = 2.0f * 0.06f * 112.0f;
    const int emit = 44, n = 14;
    int i, k;
    header("T1", "LOOP/WARP", "3", "P01", 0, 0);
    TL(PX_L15, "AMEN_170.WAV", 2, 13);
    TR(PX_L8, "GRAIN", W - 2, 13);
    FR(PX_L8, 2, 20, 116, 42);
    wave(4, 116, 23, 59, 14, 7, PX_L3, PX_L3, 0, W);
    g_rng = 23;
    for (k = n - 1; k >= 0; k--) {                   /* oldest first, so the newest sit on top */
        const float ph = ((float)k + 0.6f * rnd()) / (float)n;
        const float age = ph * len;                  /* how far it has read (rate 1) = how far the emitter moved */
        const float spawn = (float)emit - age + (2.0f * rnd() - 1.0f) * spread;
        const float rd = spawn + age * (1.0f + 0.1f * (2.0f * rnd() - 1.0f));
        const float lv = sinf(3.14159265f * ph) * sinf(3.14159265f * ph);
        const int lane = (k * 5) % 9, w = (int)(len + 0.5f), x = (int)(rd - len / 2.0f + 0.5f);
        const int col = k == 0 ? PX_ACC : lv > 0.75f ? PX_L15 : lv > 0.45f ? PX_L12 : lv > 0.2f ? PX_L8 : PX_L5;
        R(col, x, 23 + lane * 4, w, 3);
    }
    R(PX_ACC, emit, 20, 1, 3), R(PX_ACC, emit, 59, 1, 3);
    for (i = 0; i < 16; i++) {                       /* the loop keys W1..W16 */
        const int x = 3 + i * 7;
        R(i == 5 ? PX_ACC : i % 4 ? PX_L5 : PX_L12, x, 66, 5, i % 4 ? 2 : 4);
    }
    cells(c, 2, 0);
}

static void scr_trig(void)
{
    static const cell_t c[4] = {{"CHANCE", "50%", 0.5f, "L", 0}, {"COND", "1:2", 0.2f, 0, 0},
                                {"RATCHET", "1", 0, 0, 0}, {0, 0, 0, 0, 0}};
    header("T1", "TRIG/COND", "2", "S05", 1, 0);
    TL(PX_L15, "STEP 5  1 LOCK", 2, 13);
    TR(PX_L8, "120", W - 2, 13);
    step_bars("x.x.x..xx.x.xx..", 26, 62, 4, "....o...........");
    cells(c, 0, 0);
}

static void scr_keyboard(void)
{
    static const cell_t c[4] = {{"SLICE", "4", 0.15f, 0, 0}, {"PITCH", "+3", 0.55f, 0, 0},
                                {"VEL", "96", 0.75f, 0, 0}, {"STEP", "5", 0.3f, "A", 0}};
    static const int CUT[11] = {0, 9, 21, 30, 44, 52, 63, 74, 85, 97, 112};
    int i;
    header("T2", "KEYBOARD", "4", "RAZOR", 0, 0);
    TL(PX_L15, "SLICE 4/24", 2, 13);
    TR(PX_L8, "OCT 0", W - 2, 13);
    R(PX_L3, 2 + CUT[3], 20, CUT[4] - CUT[3], 30);
    wave(2, 114, 20, 50, 11, 3, PX_L15, PX_L8, 2 + CUT[3], 2 + CUT[4]);
    for (i = 0; i < 11; i++)
        R(i == 3 || i == 4 ? PX_ACC : PX_L5, 2 + CUT[i], 18, 1, 34);
    for (i = 0; i < 16; i++) {                       /* step entry: the target step */
        const int x = 3 + i * 7, done = i < 4 && (i % 2 == 0);
        R(i == 4 ? PX_ACC : done ? PX_L12 : PX_L3, x, 58, 5, i == 4 || done ? 8 : 2);
    }
    T(PX_ACC, ">", 3 + 5 * 7, 59, 1, EC_AL_LEFT);
    cells(c, 0, 0);
}

static void scr_file_browser(void)
{
    static const char *const F[5] = {"JUNGLE/", "AMEN_170.WAV", "THINK_96.WAV", "FUNKY_DRUMMER.WAV", "APACHE.WAV"};
    static const char *const L[5] = {"", "2.1S", "4.0S", "3.2S", "2.7S"};
    static const cell_t c[4] = {{"SLOT", "38", 0.3f, 0, 0}, {"LENGTH", "4.0S", 0.2f, 0, 0},
                                {"BPM", "96", 0.4f, 0, 0}, {"FREE", "13M", 0.59f, 0, 0}};
    int i;
    header("T1", "FILE/BROWSE", "1", "3/12", 0, 0);
    TL(PX_L5, "/AUDIO/BREAKS/", 2, 13);
    TR(PX_L8, "SLOT 38", W - 2, 13);
    for (i = 0; i < 5; i++) {                        /* ec_screens.c file_screen()'s list */
        const int y = 21 + i * 8, cur = i == 2;
        if (cur)
            R(PX_L15, 0, y - 1, W, 7);
        TL(cur ? PX_BG : i == 0 ? PX_L12 : PX_L8, F[i], 6, y);
        TR(cur ? PX_BG : PX_L5, L[i], W - 2, y);
        if (i == 1)
            R(PX_ACC, 2, y + 1, 2, 3);               /* in the pool */
    }
    TL(PX_L8, "POOL 37/128", 2, 61);                 /* the pool meter */
    TR(PX_L8, "41%", W - 2, 61);
    R(PX_L3, 2, 68, 116, 4);
    R(PX_L12, 2, 68, 46, 4);
    R(PX_ACC, 46, 68, 2, 4);
    cells(c, 0, 0);
}

/* ------------------------------------------------------------------------- 0.6.42 design: PROJECTS --- */
/* (owner, 2026-11-05) FILE's two halves: SAMPLES (the browser, rooted at AUDIO/, an UP row on top) and PROJECTS (every
 * Elasticat project in data/elasticat/projects/: load, save, save +1, rename, a random name; the name keybed is FIZZ's) */
static void projects(int confirm)
{
    static const char *const N[5] = {"ACID LAKE", "SPICY WALRUS", "SPICY WALRUS 2", "COSMIC TRAM", "PROJ-007"};
    static const char *const AGE[5] = {"NOW", "2H", "1D", "3D", "6D"};
    static const char used[64 + 1] = "xxxxxx..xx......xxx.............x...............................";
    int i;
    if (confirm)                                     /* a question waits: the arrows are OCT- (no) and OCT+ (yes) */
        header("T1", "LOAD COSMIC TRAM?", 0, "", 0, 1);
    else
        header("T1", "FILE/PROJECTS", "", "2/14", 0, 0);
    TL(PX_L5, "NOW", 2, 13);                         /* the project you work on, and whether it is saved */
    TL(PX_L15, "ACID LAKE", 18, 13);
    TR(PX_ACC, "EDITED", W - 2, 13);
    for (i = 0; i < 5; i++) {                        /* newest first */
        const int y = 22 + i * 8, cur = i == (confirm ? 3 : 1);
        if (cur)
            R(PX_L15, 0, y - 1, W, 7);
        TL(cur ? PX_BG : PX_L8, N[i], 6, y);
        TR(cur ? PX_BG : i == 0 ? PX_ACC : PX_L5, AGE[i], W - 2, y);
        if (i == 0)
            R(PX_ACC, 2, y + 1, 2, 3);               /* the one loaded */
    }
    R(PX_L3, 0, 63, W, 1);
    /* the selected project's card: its name big, its 64 patterns (a bubble each one written), its tempo and pool */
    T(confirm ? PX_ACTION : PX_L15, confirm ? "COSMIC TRAM" : "SPICY WALRUS", 2, 67, 2, EC_AL_LEFT);
    for (i = 0; i < 64; i++) {
        const int x = 3 + (i % 16) * 7, y = 82 + (i / 16) * 7;
        if (used[i] == 'x')
            R(i == 0 ? PX_ACC : PX_L12, x, y, 5, 5);
        else
            FR(PX_L3, x, y, 5, 5);
    }
    if (confirm) {                                   /* what yes does to the project you leave */
        TL(PX_ACC, "ACID LAKE SAVED FIRST", 2, 112);
        return;
    }
    TL(PX_L8, "128 BPM", 2, 112);
    TR(PX_L8, "9 SAMPLES", W - 2, 112);
}
static void scr_projects(void) { projects(0); }
static void scr_projects_confirm(void) { projects(1); }

/* the name keybed (FIZZ's typewriter): B1..B11 = A..K, W1..W16 = L..Z and SPACE; the rolled name starts selected */
static void scr_project_name(void)
{
    static const int BK[11] = {0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14};   /* the white key each black key follows */
    int i;
    header("T1", "PROJECT NAME", 0, "", 0, 1);
    R(PX_L15, 2, 17, 98, 14);                        /* the suggested name, selected: the first key replaces it */
    T(PX_BG, "SPICY WALRUS", 4, 19, 2, EC_AL_LEFT);
    R(PX_SPECIAL, 104, 17, 14, 14);                  /* the die it rolled with */
    R(PX_BG, 106, 19, 2, 2), R(PX_BG, 114, 19, 2, 2), R(PX_BG, 110, 23, 2, 2);
    R(PX_BG, 106, 27, 2, 2), R(PX_BG, 114, 27, 2, 2);
    for (i = 0; i < 10; i++)                         /* the last ten names: where you are among them */
        R(i == 9 ? PX_L15 : PX_L5, 2 + i * 6, 36, 4, 2);
    TL(PX_L5, "12/16", 2, 42);
    for (i = 0; i < 16; i++) {                       /* the keybed: whites L..Z and SPACE */
        const int x = 4 + i * 7;
        char b[2] = {(char)(i < 15 ? 'L' + i : 0), 0};
        R(i == 6 ? PX_ACC : PX_L12, x, 60, 6, 56);
        if (i < 15)
            T(PX_BG, b, x + 3, 108, 1, EC_AL_CENTER);
        else
            R(PX_BG, x + 1, 110, 4, 1);              /* SPACE */
    }
    for (i = 0; i < 11; i++) {                       /* blacks A..K over them */
        const int x = 4 + BK[i] * 7 + 4;
        char b[2] = {(char)('A' + i), 0};
        R(PX_BG, x - 1, 59, 8, 32);
        R(PX_L3, x, 60, 6, 30);
        T(PX_L15, b, x + 3, 82, 1, EC_AL_CENTER);
    }
}

/* SAMPLES: the browser rooted at AUDIO/ (the RemoteFM1 folder above it is out of reach), an UP row on top */
static void scr_samples_browser(void)
{
    static const char *const F[5] = {"UP", "JUNGLE/", "AMEN_170.WAV", "THINK_96.WAV", "FUNKY_DRUMMER.WAV"};
    static const char *const L[5] = {"", "", "2.1S", "4.0S", "3.2S"};
    static const cell_t c[4] = {{"SLOT", "38", 0.3f, 0, 0}, {"LENGTH", "4.0S", 0.2f, 0, 0},
                                {"BPM", "96", 0.4f, 0, 0}, {"FREE", "13M", 0.59f, 0, 0}};
    int i;
    header("T1", "FILE/SAMPLES", "", "4/13", 0, 0);
    TL(PX_L5, "AUDIO/BREAKS/", 2, 13);
    TR(PX_L8, "SLOT 38", W - 2, 13);
    for (i = 0; i < 5; i++) {
        const int y = 21 + i * 8, cur = i == 3;
        if (cur)
            R(PX_L15, 0, y - 1, W, 7);
        if (i == 0) {                                /* UP: an arrow, the folder above */
            R(PX_L12, 3, y, 1, 1), R(PX_L12, 2, y + 1, 3, 1), R(PX_L12, 3, y + 2, 1, 3);
            TL(PX_L12, "UP", 8, y);
            TR(PX_L5, "AUDIO/", W - 2, y);
            continue;
        }
        TL(cur ? PX_BG : i == 1 ? PX_L12 : PX_L8, F[i], 6, y);
        TR(cur ? PX_BG : PX_L5, L[i], W - 2, y);
        if (i == 2)
            R(PX_ACC, 2, y + 1, 2, 3);
    }
    TL(PX_L8, "POOL 37/128", 2, 61);
    TR(PX_L8, "41%", W - 2, 61);
    R(PX_L3, 2, 68, 116, 4);
    R(PX_L12, 2, 68, 46, 4);
    R(PX_ACC, 46, 68, 2, 4);
    cells(c, 0, 0);
}

static void scr_file_editor(void)
{
    static const cell_t c[4] = {{"START", "0.12S", 0.03f, 0, 0}, {"END", "3.80S", 0.95f, 0, 0},
                                {"BPM", "96", 0.4f, 0, 0}, {"GAIN", "+0DB", 0.5f, 0, 0}};
    int i;
    header("T1", "FILE/EDIT", "4", "4X", 0, 0);
    TL(PX_L15, "THINK_96.WAV", 2, 13);
    TR(PX_L8, "SLOT 38", W - 2, 13);
    wave(2, 118, 22, 58, 18, 11, PX_L15, PX_L5, 12, 106);
    R(PX_ACC, 12, 20, 1, 40), R(PX_ACC, 12, 20, 4, 3);
    R(PX_ACC, 105, 20, 1, 40), R(PX_ACC, 102, 20, 4, 3);
    for (i = 0; i < 16; i++) {                       /* the trim keys W1..W16 */
        const int x = 3 + i * 7, set = i == 1 || i == 14;
        R(set ? PX_ACC : PX_L5, x, 64, 5, set ? 4 : 2);
    }
    R(PX_L3, 2, 71, 116, 2);                         /* where the zoomed view sits in the file */
    R(PX_L12, 30, 71, 29, 2);
    cells(c, 0, 0);
}

static void scr_sampler(void)
{
    static const cell_t c[4] = {{"LENGTH", "2 BARS", 0.4f, 0, 0}, {"GAIN", "+0DB", 0.5f, 0, 0},
                                {"MONITOR", "OFF", 0, 0, 0}, {"FREE", "13M", 0.59f, 0, 0}};
    int i, ch;
    header("T1", "SAMPLER", "", "SLOT 39", 0, 0);
    TL(PX_L8, "IN AIN1", 2, 13);
    TR(PX_L8, "/DATA/REC", W - 2, 13);
    for (ch = 0; ch < 2; ch++) {                     /* the VU: 24 segments */
        const int y = 22 + ch * 13, lit = ch ? 16 : 17;
        T(PX_L8, ch ? "R" : "L", 2, y, 2, EC_AL_LEFT);
        for (i = 0; i < 24; i++)
            R(i < lit ? (i >= 18 ? PX_ACC : PX_L15) : i == 20 ? PX_L12 : PX_L3, 12 + i * 4 + (i / 6), y, 3, 10);
    }
    R(PX_ACC, 4, 52, 6, 6), R(PX_ACC, 3, 53, 8, 4), R(PX_ACC, 5, 51, 4, 8);   /* armed: a dot */
    T(PX_ACC, "ARMED", 14, 50, 2, EC_AL_LEFT);
    T(PX_L15, "BAR 5", W - 2, 50, 2, EC_AL_RIGHT);
    for (i = 0; i < 8; i++)                          /* the take's bars */
        R(i < 2 ? PX_L12 : PX_L3, 3 + i * 15, 66, 13, 4);
    cells(c, 0, 0);
}

static void scr_crossfader(void)
{
    static const cell_t c[4] = {{"POSITION", "74", 74 / 127.0f, "A", 0}, {"GLIDE", "MED", 0.66f, 0, 0},
                                {"A LOCKS", "12", 0.5f, 0, 0}, {"B LOCKS", "9", 0.4f, 0, 0}};
    const int x0 = 16, len = 88, kx = x0 + len * 74 / 127, tx = x0 + 8 * 8;
    int i;
    header("T1", "CROSSFADER", "", "MED", 0, 0);
    T(PX_L15, "A", 3, 22, 2, EC_AL_LEFT);
    T(PX_L15, "B", W - 3, 22, 2, EC_AL_RIGHT);
    R(PX_L3, x0, 26, len, 3);
    R(PX_L8, x0, 26, kx - x0, 3);
    for (i = kx + 4; i < tx; i += 2)                 /* gliding to the held key */
        R(PX_ACC, i, 27, 1, 1);
    R(PX_L15, kx - 2, 20, 5, 15);
    for (i = 0; i < 12; i++)                         /* W1..W12 */
        R(i == 8 ? PX_ACC : PX_L5, x0 + i * 8 - (i == 11 ? 1 : 0), 39, 2, i == 8 ? 5 : 3);
    T(PX_L15, "74", kx, 47, 2, EC_AL_CENTER);
    T(PX_L8, "12", 3, 62, 2, EC_AL_LEFT);
    T(PX_L8, "9", W - 3, 62, 2, EC_AL_RIGHT);
    for (i = 0; i < 4; i++)                          /* W13..W16: the glide times */
        R(i == 2 ? PX_L15 : PX_L3, 40 + i * 11, 64, 9, 6);
    cells(c, 0, 0);
}

static void scr_patterns(void)
{
    static const cell_t c[4] = {{"PLAYING", "A3", 0.5f, 0, 0}, {"NEXT", "A4", 0.5f, "A", 0},
                                {"QUANT", "1 BAR", 0.5f, 0, 0}, {"LENGTH", "32", 0.5f, 0, 0}};
    int i;
    header("T1", "PATTERNS", "A", "Q 1 BAR", 0, 0);
    for (i = 0; i < 16; i++) {                       /* ec_screens.c pat_screen()'s grid, one bank */
        const int col = i % 4, row = i / 4, x = 2 + col * 29, y = 12 + row * 14, w = 27, h = 12;
        char n[4];
        snprintf(n, sizeof n, "A%d", i + 1);
        if (i == 2) {
            R(PX_L15, x, y, w, h);
            T(PX_BG, n, x + w / 2, y + 1, 2, EC_AL_CENTER);
        } else if (i == 3) {
            R(PX_ACC, x, y, w, h);
            T(PX_BG, n, x + w / 2, y + 1, 2, EC_AL_CENTER);
        } else {
            FR(i < 6 ? PX_L8 : PX_L3, x, y, w, h);
            T(i < 6 ? PX_L12 : PX_L5, n, x + w / 2, y + 1, 2, EC_AL_CENTER);
        }
    }
    R(PX_L3, 2, 70, 116, 2);                         /* to the launch */
    R(PX_ACC, 2, 70, 80, 2);
    cells(c, 0, 0);
}

static void scr_record(void)
{
    static const row_t r[7] = {{"MODE", "CAPTURE", 0}, {"LENGTH", "4 BARS", 1}, {"COUNT-IN", "1 BAR", 0},
                               {"METRONOME", "REC", 0}, {"CLICK LVL", "-12DB", 0}, {"AUTOMATION", "ON", 0},
                               {"QUANTIZE", "KEEP", 0}};
    header("T1", "REC/SETUP", "", "", 0, 1);
    settings_list(r, 7, 0);
}

static void scr_app_settings(void)
{
    static const row_t r[7] = {{"THEME", "CREAM", 0}, {"MODE POP-UP", "ON", 0}, {"KNOB SPEED", "MED", 0},
                               {"AUTO-PREVIEW", "ON", 0}, {"AFTERTOUCH", "ON", 0}, {"METRO SOUND", "WOOD", 0},
                               {"MIDI CC", "1+11", 0}};
    int i;
    header("T1", "APP/SETTINGS", "", "", 0, 1);
    settings_list(r, 7, 0);
    for (i = 0; i < 6; i++)                          /* THEME: the six, the current one marked */
        R(i == 0 ? PX_L15 : PX_L5, 26 + i * 4, 21, 3, i == 0 ? 3 : 2);
}

/* ------------------------------------------------------------------------------------------- sprites --- */
/* norns sprite sheets converted by tools/mockups/sprite_to_c.py --bin: 120x64 frames of palette indices */
#define SPR_W 120
#define SPR_H 64
typedef struct {
    uint8_t *px;
    int n;
} sprite_t;
static sprite_t g_intro, g_cat;

static void load_sprite(sprite_t *sp, const char *path)
{
    FILE *f = fopen(path, "rb");
    long n;
    if (!f) {
        fprintf(stderr, "%s: cannot open\n", path);
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    sp->px = malloc((size_t)n);
    if (!sp->px || fread(sp->px, 1, (size_t)n, f) != (size_t)n)
        exit(1);
    fclose(f);
    sp->n = (int)(n / (SPR_W * SPR_H));
}
static void blit(const sprite_t *sp, int frame, int x0, int y0)
{
    int x, y;
    for (y = 0; y < SPR_H; y++)
        for (x = 0; x < SPR_W; x++)
            R(sp->px[(size_t)frame * SPR_W * SPR_H + y * SPR_W + x], x0 + x, y0 + y, 1, 1);
}

/* the launch intro (norns draw_intro_screen): the logo frame centred on the whole screen, nothing else */
static void scr_intro(void)
{
    if (g_intro.n)
        blit(&g_intro, 42, 0, 28);
}

/* MIX's B11: the CAT (norns' Visualizer page): the frame on SHADE (the art's own dark grey), the header and the track
 * keys kept (they still work while it shows) */
static void scr_cat(void)
{
    static const char *const N[9] = {"T1", "T2", "T3", "T4", "T5", "T6", "S1", "S2", "MST"};
    int i;
    R(PX_SHADE, 0, 10, W, H - 10);
    header("T2", "MIX/CAT", "", "120 BPM 1X", 0, 0);
    if (g_cat.n)
        blit(&g_cat, 3, 0, 22);
    for (i = 0; i < 9; i++) {
        const int x = 3 + i * 13;
        if (i == 1) {
            R(PX_L15, x, 99, 12, 11);
            T(PX_BG, N[i], x + 6, 102, 1, EC_AL_CENTER);
        } else
            T(PX_L8, N[i], x + 6, 102, 1, EC_AL_CENTER);
    }
}

/* a contact sheet of every frame, 2-unit L3 gaps */
static uint8_t g_sheetpx[1000 * 500];
static void sheet(const char *dir, const char *name, const sprite_t *sp, int cols);

/* ------------------------------------------------------------------------------------------- the pop-up --- */
/* docs/UX-V2.md §3 "Entering a mode": the mode's name big, its current page, the 11 black keys in their roles, over
 * the dimmed screen. Roles: P page, S source, X special, A action, O unused; cur = the current page's key or -1 */
static int role_px(char r)
{
    switch (r) {
    case 'P': return PX_L15;
    case 'S': return PX_SRC;
    case 'X': return PX_SPECIAL;
    case 'A': return PX_ACTION;
    default: return PX_L3;
    }
}
static void popup(const char *name, const char *page, const char *roles, int cur)
{
    const int bx = 4, by = 22, bw = 112, bh = 76;
    int i, sc;
    for (i = 0; i < W * H; i++)
        g_px[i] = PX_DIM[g_px[i]];
    R(PX_BG, bx, by, bw, bh);
    R(PX_L12, bx, by, bw, 26);
    for (sc = 4; sc > 1 && TW(name, sc) > bw - 6; sc--)
        ;
    T(PX_BG, name, W / 2, by + 13 - 5 * sc / 2, sc, EC_AL_CENTER);
    T(PX_ACC, page, W / 2, by + 32, 2, EC_AL_CENTER);
    for (i = 0; i < 11; i++) {                       /* B1..B11 */
        const int x = 6 + i * 10, y = by + 50, c = role_px(roles[i]);
        if (roles[i] == 'O')
            FR(PX_L3, x, y, 8, 14);
        else
            R(c, x, y, 8, 14);
        if (i == cur)
            R(PX_L15, x, y + 16, 8, 3);
    }
    FR(PX_L12, bx, by, bw, bh);
}

/* ------------------------------------------------------------------------------------------- output --- */
static uint16_t g_lcd[2000 * 1000];

/* the index buffer through a theme -> RGB565 (LCD order) at 2x; then the 2x2 rule checked on the result */
static int expand(const uint8_t *px, int w, int h, const uint32_t *pal, uint16_t *out)
{
    int x, y, bad = 0;
    for (y = 0; y < 2 * h; y++)
        for (x = 0; x < 2 * w; x++) {
            const uint32_t c = pal[px[(y / 2) * w + x / 2] & 15];
            out[y * 2 * w + x] = EC_RGB((c >> 16) & 255, (c >> 8) & 255, c & 255);
        }
    for (y = 0; y < 2 * h; y += 2)
        for (x = 0; x < 2 * w; x += 2) {
            const uint16_t a = out[y * 2 * w + x];
            bad += out[y * 2 * w + x + 1] != a || out[(y + 1) * 2 * w + x] != a || out[(y + 1) * 2 * w + x + 1] != a;
        }
    return bad;
}
static int g_fail;
static void save(const char *dir, const char *name, const uint8_t *px, int w, int h, const uint32_t *pal)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    if (expand(px, w, h, pal, g_lcd)) {
        fprintf(stderr, "%s: breaks the 2x2 rule\n", name);
        g_fail = 1;
    }
    if (ec_png_write_lcd(path, g_lcd, 2 * w, 2 * h, 2)) {
        fprintf(stderr, "%s: cannot write\n", path);
        g_fail = 1;
    } else
        printf("%s %dx%d\n", path, 4 * w, 4 * h);
}

typedef struct {
    const char *file, *name, *page, *roles;
    int cur;
    void (*draw)(void);
} mmode_t;
static const mmode_t MODES[] = {
    {"Main", "LOOP ENGINE", "LOOP", "PPPPPSSSSSX", 0, scr_main},
    {"Shift", "SHIFT", "STICKY", "AAAAOOOOOOA", -1, scr_shift},
    {"Fx", "FX", "INSERT 1", "PPPPPSSSSSX", 0, scr_fx},
    {"Filter", "FILTER", "FILTER", "PPPOOSSSSSX", 0, scr_filter},
    {"Filter_env", 0, 0, 0, 0, scr_filter_env},
    {"Amp", "AMP", "AMP ENV", "PPPOOSSSSSO", 1, scr_amp},
    {"Mod_env", 0, 0, 0, 0, scr_mod_env},
    {"Mod", "MODULATION", "MATRIX", "PSSSSSSSSSX", 0, scr_mod},
    {"Mix", "MIX", "T2  1X", "AAAAAAAAAAX", 4, scr_mix},
    {"Pages", "PAGES", "PAGE 2", "PPPPOOOOOOO", 1, scr_pages},
    {"Loop", "LOOP ENGINE", "WARP", "PPPPPSSSSSX", 2, scr_loop},
    {"Trig", "TRIG", "COND", "PPPOOSSSSSO", 1, scr_trig},
    {"Keyboard", "KEYBOARD", "SLICE 4", "AAAAAAAAAAX", 3, scr_keyboard},
    {"FileBrowser", "FILE", "BROWSE", "AAPPPOOOOAX", -1, scr_file_browser},
    {"FileEditor", "FILE", "EDITOR 4X", "APPPPPPPPPA", 3, scr_file_editor},
    {"Projects", "PROJECTS", "LOAD SAVE", "AAAOOOOAAAO", -1, scr_projects},   /* 0.6.42 design */
    {"ProjectsConfirm", 0, 0, 0, 0, scr_projects_confirm},
    {"ProjectName", 0, 0, 0, 0, scr_project_name},
    {"SamplesBrowser", "SAMPLES", "BROWSE", "AAPPPOOOOAX", -1, scr_samples_browser},
    {"Sampler", "SAMPLER", "2 BARS", "AAAAAOOOOOX", 2, scr_sampler},
    {"Crossfader", "CROSSFADER", "GLIDE MED", "XXOOOOOOOOX", -1, scr_crossfader},
    {"Patterns", "PATTERNS", "BANK A", "PPPPOOOOOOO", 0, scr_patterns},
    {"Record", "RECORD", "CAPTURE", "OOOOOOOOOOO", -1, scr_record},
    {"AppSettings", 0, 0, 0, 0, scr_app_settings},   /* a pop-up itself */
    {"Intro", 0, 0, 0, 0, scr_intro},
    {"Cat", 0, 0, 0, 0, scr_cat},
};
#define NMODE ((int)(sizeof MODES / sizeof MODES[0]))

static uint8_t g_sheet[744 * 168];

static void sheet(const char *dir, const char *name, const sprite_t *sp, int cols)
{
    const int rows = (sp->n + cols - 1) / cols, w = 2 + cols * (SPR_W + 2), h = 2 + rows * (SPR_H + 2);
    int f;
    canvas(g_sheetpx, w, h);
    memset(g_sheetpx, PX_L3, (size_t)w * h);
    for (f = 0; f < sp->n; f++)
        blit(sp, f, 2 + (f % cols) * (SPR_W + 2), 2 + (f / cols) * (SPR_H + 2));
    save(dir, name, g_sheetpx, w, h, PX_THEME_DEFAULT);
    canvas(g_screen, W, H);
}

int main(int argc, char **argv)
{
    char name[128];
    int m, t, i;
    FILE *f;
    if (argc < 2) {
        fprintf(stderr, "usage: ec_mock_ux2 <out dir> [--intro intro.bin] [--cat cat.bin]\n");
        return 2;
    }
    for (i = 2; i + 1 < argc; i += 2)
        load_sprite(!strcmp(argv[i], "--cat") ? &g_cat : &g_intro, argv[i + 1]);
    for (m = 0; m < NMODE; m++) {
        canvas(g_screen, W, H);
        memset(g_screen, PX_BG, sizeof g_screen);
        MODES[m].draw();
        snprintf(name, sizeof name, "%s_screen.png", MODES[m].file);
        save(argv[1], name, g_screen, W, H, PX_THEME_DEFAULT);
        if (!MODES[m].name)
            continue;                                /* a page, a pop-up or a sprite: no mode-entry pop-up */
        popup(MODES[m].name, MODES[m].page, MODES[m].roles, MODES[m].cur);
        snprintf(name, sizeof name, "%s_popup.png", MODES[m].file);
        save(argv[1], name, g_screen, W, H, PX_THEME_DEFAULT);
    }

    /* _palette.png: the 16 slots of the default theme with their names */
    canvas(g_screen, W, H);
    memset(g_screen, PX_BG, sizeof g_screen);
    for (i = 0; i < PX_N; i++) {
        const int x = 2 + (i % 4) * 30, y = 2 + (i / 4) * 30;
        char n[4];
        R(i, x, y, 27, 16);
        if (i == PX_BG || i == PX_BLACK || i == PX_SHADE)
            FR(PX_L5, x, y, 27, 16);
        snprintf(n, sizeof n, "%d", i);
        TL(i == PX_L15 || i == PX_L12 || i == PX_SPECIAL || i == PX_OK || i == PX_ACTION ? PX_BG : PX_L15, n, x + 2, y + 2);
        TL(PX_L12, PX_NAME[i], x, y + 19);
    }
    save(argv[1], "_palette.png", g_screen, W, H, PX_THEME_DEFAULT);
    if (g_intro.n)
        sheet(argv[1], "Intro_frames.png", &g_intro, 8);
    if (g_cat.n)
        sheet(argv[1], "Cat_frames.png", &g_cat, 4);

    /* _themes.png: the MATRIX screen in each theme, its name and its 16 swatches; each column drawn in its own theme
     * (the sheet alone has more than 16 colours: 16 per column) */
    {
        const int cw = 124, sh = 168;
        static uint16_t col[6][248 * 336];
        static uint8_t colpx[124 * 168];
        static uint16_t all[744 * 2 * 168 * 2];
        int y;
        (void)g_sheet;
        for (t = 0; t < TH_N; t++) {
            canvas(colpx, cw, sh);
            memset(colpx, PX_BG, sizeof colpx);
            T(PX_L15, TH_NAME[t], cw / 2, 3, 2, EC_AL_CENTER);
            canvas(g_screen, W, H);
            memset(g_screen, PX_BG, sizeof g_screen);
            scr_mod();
            for (y = 0; y < H; y++)
                memcpy(colpx + (16 + y) * cw + 2, g_screen + y * W, W);
            canvas(colpx, cw, sh);
            FR(PX_L3, 1, 15, 122, 122);
            for (i = 0; i < PX_N; i++)
                R(i, 2 + (i % 8) * 15, 140 + (i / 8) * 13, 14, 12);
            if (expand(colpx, cw, sh, PX_THEMES[t], col[t])) {
                fprintf(stderr, "_themes.png: breaks the 2x2 rule\n");
                g_fail = 1;
            }
        }
        for (y = 0; y < 2 * sh; y++)
            for (t = 0; t < TH_N; t++)
                memcpy(all + y * 2 * cw * TH_N + t * 2 * cw, col[t] + y * 2 * cw, 2 * cw * sizeof(uint16_t));
        snprintf(name, sizeof name, "%s/_themes.png", argv[1]);
        if (ec_png_write_lcd(name, all, 2 * cw * TH_N, 2 * sh, 2))
            g_fail = 1;
        else
            printf("%s %dx%d\n", name, 4 * cw * TH_N, 4 * sh);
    }

    /* palette.json: the tables, for check_png.py */
    snprintf(name, sizeof name, "%s/palette.json", argv[1]);
    if ((f = fopen(name, "w")) != 0) {
        fprintf(f, "{\"slots\": [");
        for (i = 0; i < PX_N; i++)
            fprintf(f, "%s\"%s\"", i ? ", " : "", PX_NAME[i]);
        fprintf(f, "],\n \"themes\": {");
        for (t = 0; t < TH_N; t++) {
            fprintf(f, "%s\n  \"%s\": [", t ? "," : "", TH_NAME[t]);
            for (i = 0; i < PX_N; i++)
                fprintf(f, "%s\"#%06X\"", i ? ", " : "", PX_THEMES[t][i]);
            fprintf(f, "]");
        }
        fprintf(f, "\n }}\n");
        fclose(f);
    }
    return g_fail;
}
