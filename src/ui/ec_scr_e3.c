/* SPDX-License-Identifier: GPL-3.0-only */
/* E3 screens: FILE / RAZOR (design/mockup/Display.dc.html mode "razor" on the 120x120 grid, Razor.dc.html) and the
 * neighbor machine's routing view on SRC. Same grid, palette and font as ec_screens.c. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ec_gfx.h"
#include "ec_razor_ui.h"
#include "ec_screens.h"
#include "ec_ui_kit.h"

#define W EC_GW
#define R(c, x, y, w, h) ec_gfx_rect((x), (y), (w), (h), (c))
#define T(c, s, x, y, al) ec_gfx_text_al((x), (y), (s), 1, (c), (al))
#define T2(c, s, x, y, al) ec_gfx_text_al((x), (y), (s), 2, (c), (al))
#define TW(s) (ec_gfx_text_w((s), 1))

static int iv(const float *v, int id) { return (int)floorf(v[id] + 0.5f); }
static const float *tv(const ec_app_t *a) { return a->proj.pat[a->proj.current].tr[a->track].v; }

/* the visible part of track space at the zoom: centred on the last edited point, else the selected slice */
static void view(const ec_app_t *a, float *v0, float *v1)
{
    const ec_razor_t *rz = &a->proj.razor[a->track];
    const int k = ec_rzui.sel - 1;
    const float w = 128.0f / (float)ec_rzui.zoom;
    float c = ec_rzui.focus >= 0 && ec_rzui.focus_slice == ec_rzui.sel ? ec_rzui.focus
                                                                         : 0.5f * (rz->start[k] + rz->end[k]);
    float a0 = c - 0.5f * w;
    if (a0 < 0)
        a0 = 0;
    if (a0 + w > 128)
        a0 = 128 - w;
    *v0 = a0;
    *v1 = a0 + w;
}

/* which slice a track-space point is in (the last one that holds it), 0 none */
static int slice_at(const ec_razor_t *rz, int count, float x)
{
    int k, hit = 0;
    for (k = 0; k < count; k++)
        if (x >= rz->start[k] && x < rz->end[k])
            hit = k + 1;
    return hit;
}

void ec_scr_razor(const ec_app_t *a, const ec_page_t *pg)
{
    const ec_razor_t *rz = &a->proj.razor[a->track];
    const ec_ss_sample_t *smp = ec_app_track_sample(a, a->track);
    const int count = ec_rzui_count(a);
    const int sel = ec_rzui.sel < 1 ? 1 : ec_rzui.sel > count ? count : ec_rzui.sel;
    const int top = 22, bot = 57, x0 = 2, x1 = W - 2;
    const int razor = ec_razor_is_razor(iv(tv(a), EC_P_MACHINE));
    char t[32], r[16];
    float v0, v1;
    double lo, hi;
    int k;
    snprintf(r, sizeof r, "ZOOM %dX", ec_rzui.zoom);
    ec_scr_header(a, "FILE/RAZOR", 2, r);
    snprintf(t, sizeof t, "SLICE %02d/%02d", sel, count);
    T(razor ? EC_C_L15 : EC_C_L8, t, 2, 13, EC_AL_LEFT);
    snprintf(r, sizeof r, "SNAP %s", ec_rzui_snap_name(rz->snap));
    T(EC_C_L8, r, W - 2, 13, EC_AL_RIGHT);
    if (!razor)   /* the table is edited for any track; only the razor machines play it */
        T(EC_C_ALERT, "NOT RAZOR", W / 2, 19, EC_AL_CENTER);
    view(a, &v0, &v1);
    ec_rzui_window(a, a->track, &lo, &hi);
    if (!smp) {
        ec_gfx_dots_h(x0, x1, (top + bot) / 2, 2, EC_C_L3);
        T2(EC_C_L5, "NO SAMPLE", W / 2, top + 10, EC_AL_CENTER);
    } else {
        /* the waveform, read from the sample: peak |L,R| per grid column */
        const int px0 = x0, pcols = x1 - x0, mid = (top + bot) / 2, half = (bot - top) / 2;
        const double f0 = (lo + (hi - lo) * v0 / 128.0) * smp->frames, f1 = (lo + (hi - lo) * v1 / 128.0) * smp->frames;
        int sx = x0 + (int)((rz->start[sel - 1] - v0) / (v1 - v0) * (float)(x1 - x0));
        int ex = x0 + (int)((rz->end[sel - 1] - v0) / (v1 - v0) * (float)(x1 - x0));
        int c;
        sx = sx < x0 ? x0 : sx;
        ex = ex > x1 ? x1 : ex;
        if (ex > sx)
            R(EC_C_L15, sx, top - 2, ex - sx, bot - top + 4);   /* the selected slice: a cream block (mockup) */
        else
            ec_gfx_dots_h(x0, x1, mid, 2, EC_C_L3);
        for (c = 0; c < pcols; c++) {
            const double a0 = f0 + (f1 - f0) * c / pcols, a1 = f0 + (f1 - f0) * (c + 1) / pcols;
            uint32_t i0 = a0 < 0 ? 0 : (uint32_t)a0, i1 = a1 < 0 ? 0 : (uint32_t)a1, i, step;
            const float x = v0 + (v1 - v0) * ((float)c + 0.5f) / (float)pcols;
            const int sl = slice_at(rz, count, x);
            int pk = 0, h;
            int col;
            if (i1 >= smp->frames)
                i1 = smp->frames - 1;
            if (i1 < i0)
                i1 = i0;
            step = (i1 - i0) / 24 + 1;
            for (i = i0; i <= i1; i += step) {
                int l = smp->data[2 * (size_t)i], rr = smp->data[2 * (size_t)i + 1];
                l = l < 0 ? -l : l;
                rr = rr < 0 ? -rr : rr;
                if (l > pk)
                    pk = l;
                if (rr > pk)
                    pk = rr;
            }
            h = (int)((long)pk * half / 32768);
            col = sl == sel ? EC_C_BG : !sl || rz->mute[sl - 1] ? EC_C_L5 : EC_C_L12;
            ec_gfx_rect(px0 + c, mid - h, 1, 2 * h + 1, col);
        }
        /* the slice starts (and ends where a gap follows), dotted */
        for (k = 0; k < count; k++) {
            const float e[2] = {rz->start[k], rz->end[k]};
            int j;
            for (j = 0; j < 2; j++) {
                const int px = x0 + (int)((e[j] - v0) / (v1 - v0) * (float)(x1 - x0));
                if (j == 1 && k + 1 < count && fabsf(rz->start[k + 1] - e[1]) < 0.02f)
                    continue;
                if (px >= x0 && px <= x1)
                    ec_gfx_dots_v(px, top - 2, bot + 3, 2, k == sel - 1 ? EC_C_L15 : EC_C_L8);
            }
        }
        /* the transients at the chop sensitivity: ticks over the waveform (Auto-Chop's cut points) */
        {
            float tr[EC_RZ_ONSET_WINDOWS];
            const int n = ec_rzui_transients(a, a->track, tr, EC_RZ_ONSET_WINDOWS);
            for (k = 0; k < n; k++) {
                const int px = x0 + (int)((tr[k] - v0) / (v1 - v0) * (float)(x1 - x0));
                if (px >= x0 && px < x1)
                    R(EC_C_L8, px, top - 3, 1, 1);
            }
        }
        /* the sounding voices */
        {
            ec_track_status_t st;
            ec_dsp_track_status(a->dsp, a->track, &st);
            for (k = 0; k < st.voices && k < EC_MAX_VOICES_TRACK; k++) {
                const double x = (st.voice_pos[k] - lo) / (hi - lo) * 128.0;
                const int px = x0 + (int)((x - v0) / (v1 - v0) * (float)(x1 - x0));
                if (px >= x0 && px < x1)
                    ec_kit_playhead(px, top - 1, bot + 1);
            }
        }
        /* the slice numbers, where they fit (the selected one always) */
        for (k = 0; k < count; k++) {
            char n[4];
            const float c0 = rz->start[k] > v0 ? rz->start[k] : v0, c1 = rz->end[k] < v1 ? rz->end[k] : v1;
            const int px0s = x0 + (int)((c0 - v0) / (v1 - v0) * (float)(x1 - x0));
            const int px1s = x0 + (int)((c1 - v0) / (v1 - v0) * (float)(x1 - x0));
            snprintf(n, sizeof n, "%d", k + 1);
            if (c1 <= c0)
                continue;
            if (k == sel - 1 || px1s - px0s >= TW(n) + 2)
                T(k == sel - 1 ? EC_C_L15 : rz->mute[k] ? EC_C_L3 : EC_C_L8, n, (px0s + px1s) / 2, bot + 5,
                  EC_AL_CENTER);
        }
    }
    /* the zoom window over the whole track (zoomed in) */
    if (ec_rzui.zoom > 1) {
        const int zx = x0 + (int)(v0 / 128.0f * (float)(x1 - x0)), zw = (int)((v1 - v0) / 128.0f * (float)(x1 - x0));
        R(EC_C_L3, x0, 67, x1 - x0, 1);
        R(EC_C_L12, zx, 66, zw > 1 ? zw : 1, 3);
    }
    /* the pads: this bank of 16 slices on the lower keys */
    {
        const float cw = (float)(W - 4) / 16.0f;
        const int bank = (sel - 1) / 16;
        uint32_t on = a->seq.tr[a->track].pads;
        for (k = 0; k < 16; k++) {
            const int s = bank * 16 + k + 1, x = (int)(2 + k * cw + 0.5f), w = (int)(cw + 0.5f) - 2;
            if (s > count)
                continue;
            if (s == sel)
                R(EC_C_L15, x, 70, w, 3);
            else if (on >> (s - 1) & 1u)
                R(EC_C_ACC, x, 71, w, 1);
            else
                R(rz->mute[s - 1] ? EC_C_L3 : EC_C_L8, x, 71, w, 1);
        }
    }
    ec_scr_cells(a, pg);
}

/* the neighbor machine: no sample -- the left track feeding this one */
void ec_scr_neighbor(const ec_app_t *a, int top, int bot)
{
    const int left = a->track - 1;
    char l[8], r[8];
    int y0 = top + 4, h = bot - top - 14;
    if (left < 0) {
        T2(EC_C_ALERT, "NO LEFT TRACK", W / 2, top + 10, EC_AL_CENTER);
        return;
    }
    snprintf(l, sizeof l, "T%d", left + 1);
    snprintf(r, sizeof r, "T%d", a->track + 1);
    ec_gfx_frame(6, y0, 40, h, EC_C_L8);
    R(EC_C_L15, 74, y0, 40, h);
    T2(EC_C_L15, l, 26, y0 + h / 2 - 5, EC_AL_CENTER);
    T2(EC_C_BG, r, 94, y0 + h / 2 - 5, EC_AL_CENTER);
    /* the arrow */
    R(EC_C_L12, 48, y0 + h / 2, 22, 1);
    R(EC_C_L12, 67, y0 + h / 2 - 2, 1, 5);
    R(EC_C_L12, 68, y0 + h / 2 - 1, 1, 3);
    R(EC_C_L12, 69, y0 + h / 2, 1, 1);
    /* the levels: the left track's (heard only through this one) and this one's */
    {
        float pl = a->meter[left] > 1 ? 1 : a->meter[left], pr = a->meter[a->track] > 1 ? 1 : a->meter[a->track];
        R(EC_C_L3, 6, bot - 6, 40, 2);
        R(EC_C_L12, 6, bot - 6, (int)(40 * pl + 0.5f), 2);
        R(EC_C_L3, 74, bot - 6, 40, 2);
        R(EC_C_L12, 74, bot - 6, (int)(40 * pr + 0.5f), 2);
    }
    T(EC_C_L8, "NEIGHBOR", W / 2, top - 3 + 0, EC_AL_CENTER);
}
