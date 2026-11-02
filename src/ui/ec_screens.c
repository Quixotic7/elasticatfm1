/* SPDX-License-Identifier: GPL-3.0-only */
/* The screens (ec_screens.h). Coordinates are the mockup's 120x120 grid (Display.dc.html, mode "fm1"), drawn at
 * scale EC_U; the layer colours are the mockup's (l3 rules, l5 dim, l8 labels, l12 header / marks, l15 values, acc
 * the one accent: playhead, locks, alerts). */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ec_browse.h"
#include "ec_gfx.h"
#include "ec_loader.h"
#include "ec_scr_e5.h"
#include "ec_screens.h"
#include "ec_scr_e4.h"
#include "ec_scr_ux.h"

#define W EC_GW
#define H EC_GH
#define R(c, x, y, w, h) ec_gfx_rect(EC_U * (x), EC_U * (y), EC_U * (w), EC_U * (h), (c))
#define T(c, s, x, y, al) ec_gfx_text_al(EC_U * (x), EC_U * (y), (s), EC_U, (c), (al))
#define T2(c, s, x, y, al) ec_gfx_text_al(EC_U * (x), EC_U * (y), (s), 2 * EC_U, (c), (al))
#define TW(s) (ec_gfx_text_w((s), 1))

static int iv(const float *v, int id) { return (int)floorf(v[id] + 0.5f); }
static int clampi(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }
static const float *tv(const ec_app_t *a) { return a->proj.pat[a->proj.current].tr[a->track].v; }
static int is_slice(const float *v)
{
    const int m = iv(v, EC_P_MACHINE);
    return m >= EC_MACH_GRID_SLICE && m <= EC_MACH_RAZOR_POLY;
}

static void fit(char *out, size_t cap, const char *s, int n)
{
    const int l = (int)strlen(s);
    if (l <= n)
        snprintf(out, cap, "%s", s);
    else
        snprintf(out, cap, "%.*s..", n - 2, s);
}

static const char *target_name(const ec_app_t *a)
{
    static const char *const N[] = {"T1", "T2", "T3", "T4", "T5", "T6", "S1", "S2", "MST"};
    return a->bus ? N[EC_NTRACK - 1 + a->bus] : N[a->track];
}

/* ------------------------------------------------------------------------------------------- header --- */
/* the header: the target, the title (or a message), the right-hand info and, in the box at the right: a pop-up's
 * arrows (OCT- back, OCT+ OK), the count of held steps (the lock colour: the knobs lock), else the page */
static void header(const ec_app_t *a, const char *title, int page, const char *right)
{
    char pg[8], buf[40];
    const int popup = ec_app_popup(a);
    const int held = a->step_held && !a->bus && !popup ? __builtin_popcount(a->step_held) : 0;
    const int box = popup ? 21 : 11;
    R(EC_C_L12, 0, 0, W, 10);
    R(EC_C_BG, 0, 0, 14, 10);
    T(EC_C_L12, target_name(a), 7, 3, EC_AL_CENTER);
    if (a->msg_ms)
        fit(buf, sizeof buf, a->msg, 17);
    else
        snprintf(buf, sizeof buf, "%s", title);
    T(EC_C_BG, buf, 17, 3, EC_AL_LEFT);
    R(held ? EC_C_ACC : EC_C_BG, W - box, 0, box, 10);
    if (popup)
        ec_scr_arrows(W - 19, 1, EC_C_L15);
    else {
        snprintf(pg, sizeof pg, "%d", held ? held : page);
        T(held ? EC_C_BG : EC_C_L12, pg, W - 6, 3, EC_AL_CENTER);
    }
    if (right && right[0])
        T(EC_C_BG, right, W - box - 3, 3, EC_AL_RIGHT);
}

/* the default right-hand info: the held step, else the pattern (and the queued one) */
static void right_info(const ec_app_t *a, char *out, size_t cap)
{
    const int hs = ec_app_held_step(a);
    if (hs >= 0)
        snprintf(out, cap, "S%02d", hs + 1);
    else if (a->seq.pending >= 0)
        snprintf(out, cap, "P%02d>%02d", a->proj.current + 1, a->seq.pending + 1);
    else
        snprintf(out, cap, "P%02d", a->proj.current + 1);
}

static void title_of(const ec_app_t *a, const ec_page_t *pg, char *out, size_t cap)
{
    snprintf(out, cap, "%s/%s", ec_cat_name(a->cat), a->settings ? "SETUP" : pg->title);
}

/* ------------------------------------------------------------------------------------------- cells --- */
static void cells(const ec_app_t *a, const ec_page_t *pg)
{
    const int lane = a->ux.f3 ? 0 : ec_app_lane(a, pg);
    const int nl = a->ux.f3 ? 1 : ec_ux_lanes(pg->n);
    int i;
    R(EC_C_L3, 0, 76, W, 1);
    R(EC_C_L3, 60, 76, 1, 44);
    R(EC_C_L3, 0, 98, W, 1);
    for (i = 0; i < nl && nl > 1; i++)                /* the lanes: SELECT steps them, the active one bright */
        R(i == lane ? EC_C_L15 : EC_C_L5, W - 1 - (nl - i) * 10, 72, 8, 3);
    for (i = 0; i < 4; i++) {
        const int x = (i % 2) * 61 + 3, y = i < 2 ? 79 : 101, c = lane * 4 + i;
        char tag[8], val[24];
        const ec_item_t *it;
        int locked = 0, some = 0, big;
        float v, f;
        uint16_t vc;
        snprintf(tag, sizeof tag, "K%d", i + 1);
        if (c >= pg->n || pg->it[c].kind == EC_IT_BLANK) {
            T(EC_C_L3, tag, x + 54, y, EC_AL_RIGHT);
            continue;
        }
        it = &pg->it[c];
        v = ec_app_item_value(a, it, &locked);
        ec_app_item_format(a, it, v, val, sizeof val);
        /* the lock chips: filled = locked on the held step (or held scene / pad), outlined = locked on some step */
        if (!locked && it->kind == EC_IT_PARAM && !a->bus && !a->settings && (EC_PARAMS[it->id].flags & EC_PF_LOCKABLE))
            some = ec_ux_locked_somewhere(it->id);
        vc = locked ? EC_C_BG : EC_C_L15;
        if ((it->flags & EC_IF_STOPPED) && a->seq.playing)
            vc = EC_C_L5;
        T(EC_C_L8, ec_app_item_label(it), x, y, EC_AL_LEFT);
        T(EC_C_L5, tag, x + 54, y, EC_AL_RIGHT);
        big = TW(val) * 2 <= 54;
        if (locked)
            R(EC_C_ACC, x - 1, y + (big ? 6 : 8), (big ? TW(val) * 2 : TW(val)) + 3, big ? 12 : 7);
        else if (some)
            ec_gfx_frame(EC_U * (x - 1), EC_U * (y + (big ? 6 : 8)), EC_U * ((big ? TW(val) * 2 : TW(val)) + 3),
                         EC_U * (big ? 12 : 7), EC_C_ACC);
        if (big)
            T2(vc, val, x, y + 7, EC_AL_LEFT);
        else
            T(vc, val, x, y + 9, EC_AL_LEFT);
        /* the stored value's bar, the actual (resolved) value as a mark when it differs (BASE_VALUE_RESOLVER) */
        f = ec_app_item_frac(a, it, v);
        R(EC_C_L3, x, y + 18, 54, 1);
        R(locked ? EC_C_ACC : EC_C_L8, x, y + 18, (int)(f * 54.0f + 0.5f), 1);
        if (it->kind == EC_IT_PARAM && !a->bus && a->seq.playing) {
            const float act = ec_seq_value(&a->seq, a->track, it->id);
            if (act != v && it->id != EC_P_LOOP_START && it->id != EC_P_LOOP_END) {
                const int ax = x + (int)(ec_app_item_frac(a, it, act) * 53.0f + 0.5f);
                R(EC_C_ACC, ax, y + 17, 1, 2);
            }
        }
    }
}

/* ------------------------------------------------------------------------------------------- waveform --- */
static void wave(const ec_ss_sample_t *s, int top, int bot, int x0, int x1, int rs, int re)
{
    const int mid = (top + bot) / 2, half = (bot - top) / 2, cols = x1 - x0;
    int c;
    ec_gfx_dots_h(EC_U * x0, EC_U * x1, EC_U * mid, 2 * EC_U, EC_U, EC_C_L3);
    for (c = 0; c < cols; c++) {
        int lo = c * EC_SS_OVERVIEW / cols, hi = (c + 1) * EC_SS_OVERVIEW / cols, i, pk = 0, h;
        if (hi <= lo)
            hi = lo + 1;
        for (i = lo; i < hi && i < EC_SS_OVERVIEW; i++)
            if (s->overview[i] > pk)
                pk = s->overview[i];
        h = (pk * half + 127) / 255;
        R(x0 + c >= rs && x0 + c < re ? EC_C_L15 : EC_C_L5, x0 + c, mid - h, 1, 2 * h + 1);
    }
}

/* E3: slice k's columns (razor machines: their own points; grid: the even division) */
static void slice_cols(const ec_app_t *a, int k, int cnt, float lo, float hi, int x0, int cols, int *sx, int *ex)
{
    float s0 = 128.0f * (float)k / (float)cnt, e0 = 128.0f * (float)(k + 1) / (float)cnt;
    if (ec_razor_is_razor(iv(tv(a), EC_P_MACHINE)))
        ec_razor_range(&a->proj, a->track, k + 1, &s0, &e0);
    *sx = x0 + (int)((lo + (hi - lo) * s0 / 128.0f) * cols);
    *ex = x0 + (int)((lo + (hi - lo) * e0 / 128.0f) * cols);
}

/* the track's sample area: waveform with region / slices / playhead, or why there is none */
static void sample_area(const ec_app_t *a, int top, int bot)
{
    const int s = ec_app_track_slot(a, a->track);
    const ec_slotrt_t *rt = &a->slot[s];
    const ec_ss_sample_t *smp = ec_app_track_sample(a, a->track);
    const float *v = tv(a);
    const int x0 = 2, x1 = W - 2, cols = x1 - x0;
    char buf[64];
    if (iv(v, EC_P_MACHINE) == EC_MACH_NEIGHBOR) {   /* E3: the routing, not a sample */
        ec_scr_neighbor(a, top, bot);
        return;
    }
    if (iv(v, EC_P_MACHINE) == EC_MACH_INPUT) {      /* E6: the live input */
        ec_scr_input(a, top, bot);
        return;
    }
    if (rt->state == EC_SL_LOADING || rt->state == EC_SL_QUEUED) {
        const int pm = rt->state == EC_SL_LOADING ? (int)ec_load_permille() : 0;
        snprintf(buf, sizeof buf, "LOADING %d%%", pm / 10);
        T(EC_C_L8, buf, W / 2, top + 10, EC_AL_CENTER);
        R(EC_C_L3, 2, top + 20, W - 4, 3);
        R(EC_C_L15, 2, top + 20, (W - 4) * pm / 1000, 3);
        if (!smp)
            return;
    }
    if (!smp) {
        const char *p = a->proj.slot[s].path;
        ec_gfx_dots_h(EC_U * 2, EC_U * (W - 2), EC_U * ((top + bot) / 2), 2 * EC_U, EC_U, EC_C_L3);
        if (rt->state == EC_SL_MISSING || rt->state == EC_SL_ERROR) {
            T2(EC_C_ACC, rt->state == EC_SL_MISSING ? "MISSING" : "ERROR", W / 2, top + 4, EC_AL_CENTER);
            fit(buf, sizeof buf, p[0] ? p : rt->err, 28);
            T(EC_C_L8, buf, W / 2, bot - 9, EC_AL_CENTER);
        } else
            T2(EC_C_L5, "NO SAMPLE", W / 2, top + 8, EC_AL_CENTER);
        return;
    }
    if (rt->state == EC_SL_LOADING)
        return;
    {
        ec_track_status_t st;
        int rs, re, ph;
        ec_dsp_track_status(a->dsp, a->track, &st);
        if (is_slice(v)) {
            /* the slices divide the trim x range window */
            const ec_slot_t *sl = &a->proj.slot[s];
            const float span = sl->trim_end - sl->trim_start;
            const float lo = sl->trim_start + span * v[EC_P_RANGE_START] / 128.0f;
            const float hi = sl->trim_start + span * v[EC_P_RANGE_END] / 128.0f;
            const int cnt = clampi(iv(v, EC_P_SLICE_COUNT), 1, 32);
            int k;
            uint32_t on = a->seq.tr[a->track].pads;
            rs = x0 + (int)(lo * cols);
            re = x0 + (int)(hi * cols);
            for (k = 0; k < st.voices && k < EC_MAX_VOICES_TRACK; k++)
                if (st.voice_slice[k] >= 1 && st.voice_slice[k] <= 32)
                    on |= 1u << (st.voice_slice[k] - 1);
            for (k = 0; k < cnt; k++)
                if (on >> k & 1u) {
                    int sx, ex;
                    slice_cols(a, k, cnt, lo, hi, x0, cols, &sx, &ex);
                    R(EC_C_L3, sx, top - 2, ex - sx, bot - top + 4);
                }
            wave(smp, top, bot, x0, x1, rs, re);
            for (k = 0; k < cnt; k++) {
                int sx, ex;
                slice_cols(a, k, cnt, lo, hi, x0, cols, &sx, &ex);
                if (k > 0 || ec_razor_is_razor(iv(v, EC_P_MACHINE)))
                    ec_gfx_dots_v(EC_U * sx, EC_U * (top - 2), EC_U * (bot + 3), 2 * EC_U, EC_U, EC_C_L8);
            }
            for (k = 0; k < st.voices && k < EC_MAX_VOICES_TRACK; k++) {
                const int px = EC_U * x0 + (int)(st.voice_pos[k] * (float)(EC_U * cols));
                ec_gfx_rect(px, EC_U * (top - 1), EC_U, EC_U * (bot - top + 2), EC_C_ACC);
            }
            return;
        }
        rs = x0 + (int)(st.region_start * cols + 0.5f);
        re = x0 + (int)(st.region_end * cols + 0.5f);
        if (re <= rs)
            re = rs + 1;
        wave(smp, top, bot, x0, x1, rs, re);
        /* the region brackets */
        R(EC_C_L12, rs - 1, top - 3, 1, bot - top + 6);
        R(EC_C_L12, rs - 1, top - 3, 4, 2);
        R(EC_C_L12, re, top - 3, 1, bot - top + 6);
        R(EC_C_L12, re - 3, top - 3, 4, 2);
        if (st.reader_on) {                            /* the playhead, at full resolution */
            ph = EC_U * x0 + (int)(st.pos_sample * (float)(EC_U * cols));
            ec_gfx_rect(ph, EC_U * (top - 1), EC_U, EC_U * (bot - top + 2), EC_C_ACC);
        }
    }
}

/* the loop-key ruler (slices: the pads) */
static void key_ruler(const ec_app_t *a, int ky)
{
    const float *v = tv(a);
    const float cw = (float)(W - 4) / 16.0f;
    ec_track_status_t st;
    int i;
    ec_dsp_track_status(a->dsp, a->track, &st);
    if (is_slice(v)) {
        const int cnt = clampi(iv(v, EC_P_SLICE_COUNT), 1, 32);
        uint32_t on = a->seq.tr[a->track].pads | a->seq.tr[a->track].fired_slices;
        for (i = 0; i < 16; i++) {
            const int x = (int)(2 + i * cw + 0.5f), w = (int)(cw + 0.5f) - 2;
            if (i >= cnt)
                continue;
            if (on >> i & 1u)
                R(EC_C_L15, x, ky - 1, w, 3);
            else
                R(i % 4 == 0 ? EC_C_L8 : EC_C_L5, x, ky, w, 1);
        }
        return;
    }
    {
        const int div = clampi((iv(v, EC_P_LOOP_DIVISION) + 1) / 2 * 2, 2, 32);
        const float units = 128.0f / (float)div;
        const float ls = v[EC_P_LOOP_START], le = v[EC_P_LOOP_END];
        for (i = 0; i < 16 && i < div; i++) {
            const int x = (int)(2 + i * cw + 0.5f), w = (int)(cw + 0.5f) - 2;
            const float ks = (float)i * units, ke = ks + units;
            if (st.keys_held >> i & 1u)
                R(EC_C_L15, x, ky - 1, w, 3);
            else
                R(ke > ls + 0.001f && ks < le - 0.001f ? EC_C_L8 : EC_C_L3, x, ky, w, 1);
        }
    }
}

static void src_screen(const ec_app_t *a, const ec_page_t *pg)
{
    const float *v = tv(a);
    const ec_ss_sample_t *smp = ec_app_track_sample(a, a->track);
    const ec_slot_t *sl = &a->proj.slot[ec_app_track_slot(a, a->track)];
    char t[32], r[16], left[40];
    title_of(a, pg, t, sizeof t);
    right_info(a, r, sizeof r);
    header(a, t, a->page[a->cat] + 1, r);
    switch (a->page[EC_CAT_SRC]) {
    case 0: fit(left, sizeof left, smp ? smp->name : sl->path[0] ? strrchr(sl->path, '/') ? strrchr(sl->path, '/') + 1
                                                                                              : sl->path
                                                                       : "-",
                20);
        break;
    case 1: snprintf(left, sizeof left, "%s", is_slice(v) ? "SLICE MACHINE" : "LOOP MACHINE"); break;
    case 2: snprintf(left, sizeof left, "%s", ec_warp_name(iv(v, EC_P_MODE))); break;
    default: snprintf(left, sizeof left, "RANGE"); break;
    }
    if (a->page[EC_CAT_SRC] == 1)
        snprintf(left, sizeof left, "%s", ec_machine_name(iv(v, EC_P_MACHINE)));
    if (iv(v, EC_P_MACHINE) == EC_MACH_NEIGHBOR)   /* E3 */
        snprintf(left, sizeof left, "FROM T%d", a->track);
    if (iv(v, EC_P_MACHINE) == EC_MACH_INPUT)      /* E6 */
        snprintf(left, sizeof left, "INPUT");
    T(EC_C_L15, left, 2, 13, EC_AL_LEFT);
    if (a->page[EC_CAT_SRC] == 2) {   /* E3: only the spectral budget falls back (to tape) */
        ec_track_status_t st;
        ec_dsp_track_status(a->dsp, a->track, &st);
        if (!st.mode_implemented)
            T(EC_C_ACC, "TAPE", 2 + TW(left) + 4, 13, EC_AL_LEFT);
    }
    snprintf(r, sizeof r, "%d", (int)floorf(sl->bpm + 0.5f));
    T(EC_C_L8, r, W - 2, 13, EC_AL_RIGHT);
    sample_area(a, 22, 66);
    if (iv(v, EC_P_MACHINE) != EC_MACH_NEIGHBOR && iv(v, EC_P_MACHINE) != EC_MACH_INPUT)   /* E3, E6 */
        key_ruler(a, 71);
    cells(a, pg);
}

/* ------------------------------------------------------------------------------------------- generic pages --- */
static void step_strip(const ec_app_t *a, int top, int bot)
{
    const ec_ptrack_t *tr = &a->proj.pat[a->proj.current].tr[a->track];
    const int len = ec_seq_pattern_steps(&a->seq, a->track);
    const int play = ec_seq_step(&a->seq, a->track);
    const int pages = (len + 15) / 16;
    const float cw = (float)(W - 4) / 16.0f;
    const int hs = ec_app_held_step(a);
    int i;
    for (i = 0; i < 16; i++) {
        const int idx = a->step_page * 16 + i;
        const int x = (int)(2 + i * cw + 0.5f), w = (int)(cw + 0.5f) - 1;
        const ec_step_t *s = &tr->steps[idx];
        const int on = s->trig || s->slices;
        if (idx >= len) {
            R(EC_C_L3, x + w / 2, (top + bot) / 2, 1, 1);
            continue;
        }
        if (on && !ec_step_is_ghost(s, tr->v))
            R(EC_C_L12, x, top, w, bot - top);
        else if (on) {
            ec_gfx_frame(EC_U * x, EC_U * top, EC_U * w, EC_U * (bot - top), EC_C_L8);
        } else
            R(i % 4 == 0 ? EC_C_L5 : EC_C_L3, x, bot - 2, w, 2);
        if (s->nlock && on)
            R(EC_C_ACC, x + w / 2 - 1, bot + 2, 2, 2);
        if (idx == play && (a->seq.playing || a->seq.paused))
            R(EC_C_ACC, x, top - 3, w, 2);
        if (idx == hs)
            ec_gfx_frame(EC_U * x - 2, EC_U * top - 2, EC_U * w + 4, EC_U * (bot - top) + 4, EC_C_L15);
    }
    for (i = 0; i < pages && pages > 1; i++)
        R(i == a->step_page ? EC_C_L15 : EC_C_L5, W / 2 - pages * 2 + i * 4, bot + 6, 3, 1);
}

static float env_w(float sec)
{
    float w;
    if (sec >= EC_ENV_INF_SECONDS / 10)
        return 26;
    w = 3.0f + 7.0f * log2f(1.0f + sec * 20.0f);
    return w > 34 ? 34 : w;
}

/* the envelope's shape: ADSR or AHR, segment widths on a log time scale */
static void env_shape(const ec_app_t *a, int mode, int ia, int idc, int is, int ir, int ih, int top, int bot)
{
    const float *v = tv(a);
    const int er = iv(v, EC_P_ENV_RANGE);
    const float at = ec_xf_env_seconds(v[ia], er), rl = ec_xf_env_seconds(v[ir], er);
    float x = 4, y0 = (float)bot, y1 = (float)top, xs[6], ys[6];
    int n = 0, i;
    xs[n] = x, ys[n++] = y0;
    x += env_w(at), xs[n] = x, ys[n++] = y1;
    if (mode == EC_ENV_ADSR) {
        const float dc = ec_xf_env_seconds(v[idc], er), su = ec_xf_amount(v[is]);
        const float ysu = y0 - (y0 - y1) * su;
        x += env_w(dc), xs[n] = x, ys[n++] = ysu;
        x += 20, xs[n] = x, ys[n++] = ysu;
        x += env_w(rl), xs[n] = x, ys[n++] = y0;
    } else {
        const float ho = ec_xf_env_seconds(v[ih], er);
        x += env_w(ho), xs[n] = x, ys[n++] = y1;
        x += env_w(rl), xs[n] = x, ys[n++] = y0;
    }
    ec_gfx_dots_h(EC_U * 2, EC_U * (W - 2), EC_U * bot, 2 * EC_U, EC_U, EC_C_L3);
    for (i = 0; i + 1 < n; i++) {
        const int ax = (int)(xs[i] * EC_U), ay = (int)(ys[i] * EC_U), bx = (int)(xs[i + 1] * EC_U),
                  by = (int)(ys[i + 1] * EC_U);
        ec_gfx_line(ax, ay, bx, by, EC_C_L12);
        ec_gfx_line(ax, ay + 1, bx, by + 1, EC_C_L12);
        R(EC_C_L15, (int)xs[i + 1] - 1, (int)ys[i + 1] - 1, 2, 2);
    }
    if (rl >= EC_ENV_INF_SECONDS / 10 || (mode != EC_ENV_ADSR && ec_xf_env_seconds(v[ih], er) >= EC_ENV_INF_SECONDS / 10))
        T(EC_C_L5, "INF", (int)xs[n - 1] + 4, bot - 6, EC_AL_LEFT);
}

/* an approximate magnitude response of the filter machine (cutoff / res / type / morph), dB */
static float filter_db(const float *v, float f)
{
    const int m = iv(v, EC_P_FILTER_MACHINE);
    const float fc = ec_xf_cutoff_hz(v[EC_P_FILTER_CUTOFF]);
    const float res = ec_xf_amount(v[EC_P_FILTER_RES]);
    const float q = 0.707f + res * 6.0f;
    const float w = f / fc, w2 = w * w;
    const float den = sqrtf((1 - w2) * (1 - w2) + (w / q) * (w / q)) + 1e-6f;
    float lp = 1.0f / den, hp = w2 / den, bp = (w / q) / den, notch = fabsf(1 - w2) / den, mag;
    int type = iv(v, EC_P_FILTER_TYPE);
    switch (m) {
    case EC_FILT_MORPHING: {
        const float mo = ec_xf_morph(v[EC_P_FILTER_MORPH]);
        mag = mo < 0.5f ? lp * (1 - 2 * mo) + bp * 2 * mo : bp * (2 - 2 * mo) + hp * (2 * mo - 1);
    } break;
    case EC_FILT_LADDER: mag = lp * lp * (0.5f + res); break;
    case EC_FILT_COMB: {
        const float g = 0.3f + res * 0.65f;
        mag = 1.0f / sqrtf(1 + g * g - 2 * g * cosf(6.2831853f * w)) * (1 - g) * 1.5f;
    } break;
    case EC_FILT_FORMANT: {
        const float mo = ec_xf_morph(v[EC_P_FILTER_MORPH]);
        const float f1 = 300 + 500 * mo, f2 = 900 + 1500 * (1 - mo), f3 = 2600;
        float s = 0;
        const float fs[3] = {f1, f2, f3};
        int k;
        for (k = 0; k < 3; k++) {
            const float ww = f / fs[k], dd = sqrtf((1 - ww * ww) * (1 - ww * ww) + (ww / q) * (ww / q)) + 1e-6f;
            s += (ww / q) / dd;
        }
        mag = s;
    } break;
    default:
        mag = type == EC_FTYPE_HP ? hp : type == EC_FTYPE_BP ? bp : type == EC_FTYPE_NOTCH ? notch : lp;
        break;
    }
    return 20.0f * log10f(mag + 1e-5f);
}

static void filter_curve(const ec_app_t *a, int top, int bot)
{
    float v[EC_NP];
    const int cols = W - 4;
    int c, prev = -1, i;
    for (i = 0; i < EC_NP; i++)                 /* the actual (resolved) values: a firing lock shows */
        v[i] = ec_seq_value(&a->seq, a->track, i);
    {
        const int hs = ec_app_held_step(a);
        if (hs >= 0) {                          /* a held step shows its locks */
            const ec_step_t *s = &a->proj.pat[a->proj.current].tr[a->track].steps[hs];
            int k;
            for (k = 0; k < s->nlock; k++)
                if (s->lock_id[k] < EC_NP)
                    v[s->lock_id[k]] = s->lock_val[k];
        }
    }
    ec_gfx_dots_h(EC_U * 2, EC_U * (W - 2), EC_U * (top + (bot - top) * 18 / 54), 2 * EC_U, EC_U, EC_C_L3);
    for (c = 0; c < cols; c++) {
        const float f = 20.0f * powf(1000.0f, (float)c / (float)cols);
        float db = filter_db(v, f);
        int y;
        db = db > 18 ? 18 : db < -36 ? -36 : db;
        y = top + (int)((18.0f - db) / 54.0f * (float)(bot - top));
        R(EC_C_L3, 2 + c, y, 1, bot - y);
        if (prev >= 0)
            ec_gfx_line(EC_U * (2 + c - 1), EC_U * prev, EC_U * (2 + c), EC_U * y, EC_C_L12);
        R(EC_C_L12, 2 + c, y, 1, 1);
        prev = y;
    }
    {
        const float fc = ec_xf_cutoff_hz(v[EC_P_FILTER_CUTOFF]);
        const int cx = 2 + (int)(log10f(fc / 20.0f) / 3.0f * (float)cols);
        R(EC_C_ACC, cx, top, 1, bot - top);
    }
}

static void generic_screen(const ec_app_t *a, const ec_page_t *pg)
{
    const float *v = tv(a);
    char t[32], r[16], left[40];
    title_of(a, pg, t, sizeof t);
    right_info(a, r, sizeof r);
    header(a, t, a->settings ? 1 : a->page[a->cat] + 1, r);
    if (pg->kind != EC_PV_NONE) {               /* E4: FX / SENDS / MOD visuals */
        ec_scr_e4(a, pg);
        cells(a, pg);
        return;
    }
    left[0] = 0;
    switch (a->cat) {
    case EC_CAT_TRIG: {
        const int hs = ec_app_held_step(a);
        if (hs >= 0)
            snprintf(left, sizeof left, "STEP %d  %d LOCKS", hs + 1,
                     a->proj.pat[a->proj.current].tr[a->track].steps[hs].nlock);
        else
            snprintf(left, sizeof left, "%s  LEN %d  %s", ec_machine_name(iv(v, EC_P_MACHINE)),
                     ec_seq_pattern_steps(&a->seq, a->track), ec_rate_label(iv(v, EC_P_RATE)));
    } break;
    case EC_CAT_FILT:
        snprintf(left, sizeof left, "%s", ec_filter_name(iv(v, EC_P_FILTER_MACHINE)));
        break;
    case EC_CAT_AMP: snprintf(left, sizeof left, "%s", iv(v, EC_P_ENV_MODE) == EC_ENV_ADSR ? "ADSR" : "AHR"); break;
    default: snprintf(left, sizeof left, "%s", pg->title); break;
    }
    T(EC_C_L15, left, 2, 13, EC_AL_LEFT);
    snprintf(r, sizeof r, "%d", (int)floorf(a->proj.bpm + 0.5f));
    T(EC_C_L8, r, W - 2, 13, EC_AL_RIGHT);
    if (a->cat == EC_CAT_FILT && a->page[EC_CAT_FILT] == 0)
        filter_curve(a, 22, 68);
    else if (a->cat == EC_CAT_FILT && a->page[EC_CAT_FILT] == 1)
        env_shape(a, iv(v, EC_P_FENV_MODE), EC_P_FENV_ATTACK, EC_P_FENV_DECAY, EC_P_FENV_SUSTAIN, EC_P_FENV_RELEASE,
                  EC_P_FENV_HOLD, 24, 66);
    else if (a->cat == EC_CAT_AMP)
        env_shape(a, iv(v, EC_P_ENV_MODE), EC_P_ENV_ATTACK, EC_P_ENV_DECAY, EC_P_ENV_SUSTAIN, EC_P_ENV_RELEASE,
                  EC_P_ENV_HOLD, 24, 66);
    else
        step_strip(a, 30, 58);
    cells(a, pg);
}

/* ------------------------------------------------------------------------------------------- settings --- */
static void settings_screen(const ec_app_t *a, const ec_page_t *pg)
{
    char t[32], r[16];
    int i;
    title_of(a, pg, t, sizeof t);
    right_info(a, r, sizeof r);
    header(a, t, 1, r);
    for (i = 0; i < pg->n; i++) {
        const ec_item_t *it = &pg->it[i];
        const int y = 14 + i * 13;
        const int blocked = (it->flags & EC_IF_STOPPED) && a->seq.playing;
        const int act = i / 4 == ec_app_lane(a, pg);     /* the active lane bright, the others grey */
        char val[24], tag[8];
        const float x = ec_app_item_value(a, it, 0);
        ec_app_item_format(a, it, x, val, sizeof val);
        snprintf(tag, sizeof tag, "K%d", i % 4 + 1);
        T(act ? EC_C_L8 : EC_C_L5, ec_app_item_label(it), 3, y + 3, EC_AL_LEFT);
        T2(blocked ? EC_C_L5 : act ? EC_C_L15 : EC_C_L8, val, W - 22, y, EC_AL_RIGHT);
        T(blocked ? EC_C_ACC : act ? EC_C_L5 : EC_C_L3, blocked ? "STOP" : tag, W - 2, y + 3, EC_AL_RIGHT);
        ec_gfx_dots_h(EC_U * 2, EC_U * (W - 2), EC_U * (y + 12), 2 * EC_U, EC_U, EC_C_L3);
    }
}

/* ------------------------------------------------------------------------------------------- MIX --- */
static void mix_screen(const ec_app_t *a, const ec_page_t *pg)
{
    static const char *const N[9] = {"T1", "T2", "T3", "T4", "T5", "T6", "S1", "S2", "M"};
    const int top = 14, mh = H - 18 - top;
    const float cw = (float)(W - 4) / 9.0f;
    const int sel = a->bus ? EC_NTRACK - 1 + a->bus : a->track;
    char t[32], r[16];
    int i;
    (void)pg;
    snprintf(t, sizeof t, "MST/MIX");
    if (a->bus) {
        right_info(a, r, sizeof r);
    } else {
        char vol[8], pan[8];
        const float *v = tv(a);
        ec_param_format(EC_P_AMP, v[EC_P_AMP], 3, vol, sizeof vol);
        ec_param_format(EC_P_PAN, v[EC_P_PAN], 3, pan, sizeof pan);
        snprintf(r, sizeof r, "%s %s", vol, pan);
    }
    header(a, t, a->page[EC_CAT_MASTER] + 1, r);
    for (i = 0; i < 9; i++) {
        const int x = (int)(2 + i * cw + 0.5f), cx = x + (int)(cw / 2), x0 = cx - 5;
        const int muted = i < EC_NTRACK && a->proj.pat[a->proj.current].tr[i].v[EC_P_MUTE] > 0.5f;
        float lev = a->meter[i], vol;
        int h;
        if (i < EC_NTRACK)
            vol = a->proj.pat[a->proj.current].tr[i].v[EC_P_AMP] / 127.0f;
        else if (i == 8)
            vol = (float)a->master / 1023.0f;
        else
            vol = 0;
        lev = lev > 1 ? 1 : lev;
        R(EC_C_L3, x0, top, 4, mh);
        h = (int)(mh * lev + 0.5f);
        R(muted ? EC_C_L5 : EC_C_L12, x0, top + mh - h, 4, h);
        if (lev > 0.78f)
            R(EC_C_ACC, x0, top + mh - h, 4, 3);
        R(EC_C_L5, cx + 2, top, 1, mh);
        if (i != 6 && i != 7)
            R(EC_C_L15, cx + 1, top + (int)(mh * (1 - vol) + 0.5f) - 1, 4, 3);
        if (i < EC_NTRACK) {                       /* the pan, a tick under the strip */
            const float p = (a->proj.pat[a->proj.current].tr[i].v[EC_P_PAN] - 64.0f) / 64.0f;
            R(EC_C_L8, cx - 1 + (int)(p * 4.0f), H - 3, 2, 1);
        }
        if (i == sel) {
            R(EC_C_L15, x + 1, H - 14, (int)(cw + 0.5f) - 2, 9);
            T(EC_C_BG, N[i], cx, H - 12, EC_AL_CENTER);
        } else
            T(muted ? EC_C_L5 : EC_C_L8, N[i], cx, H - 12, EC_AL_CENTER);
    }
}

/* ------------------------------------------------------------------------------------------- FILE --- */
static void file_screen(const ec_app_t *a, const ec_page_t *pg)
{
    const int s = ec_app_track_slot(a, a->track);
    const ec_slot_t *sl = &a->proj.slot[s];
    const ec_slotrt_t *rt = &a->slot[s];
    const ec_ss_sample_t *smp = ec_app_track_sample(a, a->track);
    const int n = ec_browse_count();
    char t[32], r[16], buf[64];
    int i;
    title_of(a, pg, t, sizeof t);
    if (n > 0)
        snprintf(r, sizeof r, "%d/%d", a->cursor + 1, n);
    else
        r[0] = 0;
    header(a, t, 1, r);
    fit(buf, sizeof buf, smp ? smp->name : sl->path[0] ? (strrchr(sl->path, '/') ? strrchr(sl->path, '/') + 1 : sl->path)
                                                        : "EMPTY",
        20);
    T(smp ? EC_C_L15 : EC_C_L5, buf, 2, 13, EC_AL_LEFT);
    snprintf(r, sizeof r, "SLOT %d", s + 1);
    T(EC_C_L8, r, W - 2, 13, EC_AL_RIGHT);
    /* the list: 6 rows around the cursor */
    if (n > 0) {
        int first = a->cursor - 2;
        if (first > n - 6)
            first = n - 6;
        if (first < 0)
            first = 0;
        for (i = 0; i < 6 && first + i < n; i++) {
            const int idx = first + i, y = 21 + i * 8;
            const int loaded = sl->path[0] && !strcmp(ec_browse_path(idx), sl->path);
            fit(buf, sizeof buf, ec_browse_name(idx), 26);
            if (idx == a->cursor) {
                R(EC_C_L15, 0, y - 1, W, 7);
                T(EC_C_BG, buf, 6, y, EC_AL_LEFT);
            } else
                T(EC_C_L8, buf, 6, y, EC_AL_LEFT);
            if (loaded)
                R(EC_C_ACC, 2, y + 1, 2, 3);
        }
    } else
        T2(EC_C_L5, a->scanning ? "SCANNING" : a->no_folder ? "NO FOLDER" : "NO FILES", W / 2, 36, EC_AL_CENTER);
    /* the slot's state */
    if (rt->state == EC_SL_LOADING || rt->state == EC_SL_QUEUED) {
        const int pm = rt->state == EC_SL_LOADING ? (int)ec_load_permille() : 0;
        R(EC_C_L3, 2, 70, W - 4, 3);
        R(EC_C_L15, 2, 70, (W - 4) * pm / 1000, 3);
    } else if (rt->state == EC_SL_MISSING || rt->state == EC_SL_ERROR) {
        snprintf(buf, sizeof buf, "%s %s", rt->state == EC_SL_MISSING ? "MISSING:" : "ERROR:",
                 rt->state == EC_SL_MISSING ? sl->path : rt->err);
        fit(t, sizeof t, buf, 29);
        T(EC_C_ACC, t, 2, 69, EC_AL_LEFT);
    } else if (smp) {
        snprintf(buf, sizeof buf, "%.1fS %dK %s", (double)smp->frames / smp->rate, (int)((smp->rate + 500) / 1000),
                 smp->channels > 1 ? "ST" : "MO");
        T(EC_C_L5, buf, 2, 69, EC_AL_LEFT);
    }
    cells(a, pg);
}

/* ------------------------------------------------------------------------------------------- PAT --- */
static void pat_screen(const ec_app_t *a, const ec_page_t *pg)
{
    char t[32], r[16];
    int i;
    snprintf(t, sizeof t, "PAT/%s", a->patn_open ? "LOAD" : pg->title);
    right_info(a, r, sizeof r);
    header(a, t, 1, r);
    for (i = 0; i < EC_NPAT; i++) {
        const int c = i % 4, row = i / 4, x = 2 + c * 29, y = 13 + row * 15, w = 27, h = 13;
        char n[4];
        snprintf(n, sizeof n, "%02d", i + 1);
        if (i == a->proj.current) {
            R(EC_C_L15, x, y, w, h);
            T2(EC_C_BG, n, x + w / 2, y + 2, EC_AL_CENTER);
        } else if (i == a->seq.pending) {
            R((a->ms / 120) & 1u ? EC_C_ACC : EC_C_L5, x, y, w, h);
            T2(EC_C_BG, n, x + w / 2, y + 2, EC_AL_CENTER);
        } else {
            ec_gfx_frame(EC_U * x, EC_U * y, EC_U * w, EC_U * h, a->proj.pat[i].used ? EC_C_L8 : EC_C_L3);
            T2(a->proj.pat[i].used ? EC_C_L12 : EC_C_L5, n, x + w / 2, y + 2, EC_AL_CENTER);
        }
    }
    /* the global cycle: a pending switch engages at its end */
    {
        const int gl = clampi(a->proj.glen, 1, 256);
        const int pos = a->seq.playing || a->seq.paused ? a->seq.global_pos : 0;
        R(EC_C_L3, 2, 73, W - 4, 1);
        R(a->seq.pending >= 0 ? EC_C_ACC : EC_C_L8, 2, 73, (W - 4) * (pos + 1) / gl, 1);
    }
    cells(a, pg);
}

/* ------------------------------------------------------------------------------------------- dispatch --- */
/* E5's screens share the header and the cells */
void ec_screens_header(const ec_app_t *a, const char *title, int page, const char *right)
{
    header(a, title, page, right);
}
void ec_screens_cells(const ec_app_t *a, const ec_page_t *pg) { cells(a, pg); }

static void draw_main(const ec_app_t *a)
{
    ec_page_t pg;
    if (a->rec.open) {                             /* E6: the REC pop-up */
        ec_scr_rec(a);
        return;
    }
    ec_app_page(a, &pg);
    if (a->patn_open) {
        ec_page_t pp;
        ec_page_items(EC_CAT_PAT, 0, tv(a), 0, &pp);
        pat_screen(a, &pp);
        return;
    }
    if (a->settings) {
        settings_screen(a, &pg);
        return;
    }
    if (!strcmp(pg.title, "MIX")) {
        mix_screen(a, &pg);
        return;
    }
    switch (a->cat) {
    case EC_CAT_SRC:
        if (a->bus)
            generic_screen(a, &pg);
        else
            src_screen(a, &pg);
        break;
    case EC_CAT_FILE:
        if (a->page[EC_CAT_FILE] == 1)   /* E3 */
            ec_scr_razor(a, &pg);
        else
            file_screen(a, &pg);
        break;
    case EC_CAT_PAT: pat_screen(a, &pg); break;
    default: generic_screen(a, &pg); break;
    }
}

/* E3: the shared pieces for src/ui/ec_scr_e3.c */
void ec_scr_header(const ec_app_t *a, const char *title, int page, const char *right) { header(a, title, page, right); }
void ec_scr_cells(const ec_app_t *a, const ec_page_t *pg) { cells(a, pg); }
void ec_screens_draw(const ec_app_t *a)
{
    ec_gfx_fill(EC_C_BG);
    if (!ec_scr_ux_full(a) && !ec_scr_e5_full(a))   /* 0.3.0: F3; E5: naming, the SCENE page */
        draw_main(a);
    ec_scr_e5_overlay(a);                         /* E5: the scene strip, toasts */
    ec_scr_ux_overlay(a);                         /* 0.3.0: the held-step frame, the takeover, SHIFT's map */
}
