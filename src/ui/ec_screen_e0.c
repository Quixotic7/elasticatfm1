/* SPDX-License-Identifier: GPL-3.0-only */
/* The E0 screen (ec_screen_e0.h), on the mockup's 120x120 grid (FM1Screens "fm1" mode) at scale EC_U */
#include <stdio.h>
#include <string.h>

#include "ec_gfx.h"
#include "ec_screen_e0.h"

#define W EC_GW
#define R(c, x, y, w, h) ec_gfx_rect(EC_U * (x), EC_U * (y), EC_U * (w), EC_U * (h), (c))
#define T(c, s, x, y, al) ec_gfx_text_al(EC_U * (x), EC_U * (y), (s), EC_U, (c), (al))
#define T2(c, s, x, y, al) ec_gfx_text_al(EC_U * (x), EC_U * (y), (s), 2 * EC_U, (c), (al))

/* the header bar: a dark box with the track, the title on the cream bar, the page box, a right-hand note */
static void header(const char *trk, const char *title, const char *page, const char *right)
{
    R(EC_C_L12, 0, 0, W, 10);
    R(EC_C_BG, 0, 0, 14, 10);
    T(EC_C_L12, trk, 7, 3, EC_AL_CENTER);
    T(EC_C_BG, title, 17, 3, EC_AL_LEFT);
    R(EC_C_BG, W - 11, 0, 11, 10);
    T(EC_C_L12, page, W - 6, 3, EC_AL_CENTER);
    if (right && right[0])
        T(EC_C_BG, right, W - 14, 3, EC_AL_RIGHT);
}

/* s cut to n characters ("LONG_NAME_OF_A..") */
static void fit(char *out, size_t cap, const char *s, int n)
{
    const int l = (int)strlen(s);
    if (l <= n)
        snprintf(out, cap, "%s", s);
    else
        snprintf(out, cap, "%.*s..", n - 2, s);
}

static void wave(const ec_e0_view_t *v, int top, int bot)
{
    const int mid = (top + bot) / 2, half = (bot - top) / 2, x0 = 2, cols = W - 4;
    const int ph = v->pos >= 0 ? (int)(v->pos * (float)cols) : -1;
    int c;
    ec_gfx_dots_h(EC_U * 2, EC_U * (W - 2), EC_U * mid, 2 * EC_U, EC_U, EC_C_L3);
    for (c = 0; c < cols; c++) {
        int a = (c * v->overview_n) / cols, b = ((c + 1) * v->overview_n) / cols, i, pk = 0, h;
        if (b <= a)
            b = a + 1;
        for (i = a; i < b && i < v->overview_n; i++)
            if (v->overview[i] > pk)
                pk = v->overview[i];
        h = (pk * half + 127) / 255;
        R(ph >= 0 ? (c < ph ? EC_C_L15 : EC_C_L5) : EC_C_L12, x0 + c, mid - h, 1, 2 * h + 1);
    }
    if (v->pos >= 0) {                         /* the playhead, at full resolution */
        const int px = EC_U * x0 + (int)(v->pos * (float)(EC_U * cols));
        ec_gfx_rect(px, EC_U * (top - 1), EC_U, EC_U * (bot - top + 2), EC_C_ACC);
    }
}

void ec_screen_e0_draw(const ec_e0_view_t *v)
{
    char buf[64], right[24];
    const int top = 22, bot = 66, ky = bot + 5;
    int i;

    ec_gfx_fill(EC_C_BG);
    right[0] = 0;
    if (v->bpm > 0)
        snprintf(right, sizeof right, "%d BPM", (int)(v->bpm + 0.5f));
    header("E0", "ELASTICAT", "0", right);

    /* the file line */
    if (v->rate)
        snprintf(right, sizeof right, "%dK %s", (int)((v->rate + 500) / 1000), v->channels > 1 ? "ST" : "MO");
    else
        right[0] = 0;
    fit(buf, sizeof buf, v->file && v->file[0] ? v->file : "NO SAMPLE", 22);
    T(v->file && v->file[0] ? EC_C_L15 : EC_C_L5, buf, 2, 13, EC_AL_LEFT);
    T(EC_C_L8, right, W - 2, 13, EC_AL_RIGHT);

    /* the waveform, or the load's progress, or a message */
    if (v->load_permille >= 0) {
        const int w = (W - 4) * v->load_permille / 1000;
        snprintf(buf, sizeof buf, "LOADING %d%%", v->load_permille / 10);
        T(EC_C_L8, buf, W / 2, 34, EC_AL_CENTER);
        R(EC_C_L3, 2, 43, W - 4, 3);
        R(EC_C_L15, 2, 43, w, 3);
    } else if (v->message && v->message[0]) {
        fit(buf, sizeof buf, v->message, 28);
        ec_gfx_dots_h(EC_U * 2, EC_U * (W - 2), EC_U * ((top + bot) / 2), 2 * EC_U, EC_U, EC_C_L3);
        T(EC_C_ACC, buf, W / 2, 34, EC_AL_CENTER);
    } else if (v->overview && v->overview_n > 0)
        wave(v, top, bot);
    else
        ec_gfx_dots_h(EC_U * 2, EC_U * (W - 2), EC_U * ((top + bot) / 2), 2 * EC_U, EC_U, EC_C_L3);

    /* the 16-step ruler: the playing step bright, quarter marks lighter */
    for (i = 0; i < 16; i++) {
        const float cw = (float)(W - 4) / 16.0f;
        const int x = (int)(2 + i * cw + 0.5f), w = (int)(cw + 0.5f) - 2;
        if (i == v->step)
            R(EC_C_L15, x, ky - 1, w, 3);
        else
            R(i % 4 == 0 ? EC_C_L8 : EC_C_L3, x, ky, w, 1);
    }

    /* two cells: VOL (the MASTER pot), STATE (PLAY) */
    R(EC_C_L3, 0, 76, W, 1);
    R(EC_C_L3, 60, 76, 1, 22);
    R(EC_C_L3, 0, 98, W, 1);
    T(EC_C_L8, "VOL", 3, 79, EC_AL_LEFT);
    T(EC_C_L5, "MST", 57, 79, EC_AL_RIGHT);
    snprintf(buf, sizeof buf, "%d%%", v->vol_pct);
    T2(EC_C_L15, buf, 3, 86, EC_AL_LEFT);
    T(EC_C_L8, "STATE", 64, 79, EC_AL_LEFT);
    T(EC_C_L5, "PLAY", 118, 79, EC_AL_RIGHT);
    T2(v->playing ? EC_C_ACC : EC_C_L15, v->playing ? "PLAY" : "STOP", 64, 86, EC_AL_LEFT);

    /* the last event and the file cursor */
    T(EC_C_L8, "LAST", 2, 102, EC_AL_LEFT);
    fit(buf, sizeof buf, v->last_event && v->last_event[0] ? v->last_event : "-", 23);
    T(EC_C_L15, buf, 24, 102, EC_AL_LEFT);
    T(EC_C_L8, "FILE", 2, 111, EC_AL_LEFT);
    if (v->nfiles > 0) {
        char n[48];
        fit(n, sizeof n, v->cursor_name ? v->cursor_name : "", 16);
        snprintf(buf, sizeof buf, "%d/%d %s", v->cursor + 1, v->nfiles, n);
        T(v->cursor_loaded ? EC_C_L15 : EC_C_ACC, buf, 24, 111, EC_AL_LEFT);
    } else
        T(EC_C_L5, v->files_note ? v->files_note : "NONE", 24, 111, EC_AL_LEFT);
}
