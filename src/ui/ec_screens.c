/* SPDX-License-Identifier: GPL-3.0-only */
/* The screens (ec_screens.h), on the 120x120 grid in palette slots (ec_gfx.h), built from the kit (ec_ui_kit.h) in
 * the approved mockups' geometry (host/mock_ux2.c): the header, the picture, the four cells. The slots' roles: L3
 * rules, L5 dim, L8 labels, L12 header / marks, L15 values, ACC the accent (playhead, alerts), LOCK the locks. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ec_browse.h"
#include "ec_gfx.h"
#include "ec_loader.h"
#include "ec_mtx.h"
#include "ec_scr_e5.h"
#include "ec_screens.h"
#include "ec_holdmenu.h"
#include "ec_scr_e4.h"
#include "ec_scr_modes.h"
#include "ec_scr_ux.h"
#include "ec_ui_kit.h"

#define W EC_GW
#define H EC_GH
#define R(c, x, y, w, h) ec_gfx_rect((x), (y), (w), (h), (c))
#define T(c, s, x, y, al) ec_gfx_text_al((x), (y), (s), 1, (c), (al))
#define T2(c, s, x, y, al) ec_gfx_text_al((x), (y), (s), 2, (c), (al))
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
/* the header: the target, the title, the right-hand info and, in the box at the right: a pop-up's arrows (back / OK),
 * the count of held steps (on LOCK: the knobs lock), else the page. 0.6.1: the title line shows a message (alerts on
 * ALERT) or what a held key does (a source key on SRC, held steps on LOCK) instead (ec_hdr_title); then the line is
 * that text alone, on its bar, without the right-hand info */
void ec_scr_header_s(const ec_app_t *a, const char *title, const char *page, const char *right)
{
    char line[40], buf[40];
    const int popup = ec_app_popup(a);
    const int held = a->step_held && !a->bus && !popup ? __builtin_popcount(a->step_held) : 0;
    const int bar = ec_hdr_title(title, line, sizeof line);
    fit(buf, sizeof buf, line, 17);
    ec_kit_header(target_name(a), buf, page, ec_hdr_custom() ? "" : right, held, popup);
    if (ec_hdr_custom()) {                         /* the message / held key's line on its bar */
        R(bar, 14, 0, W - 14 - (popup ? 21 : 11), 10);
        T(EC_C_BG, buf, 17, 3, EC_AL_LEFT);
    }
    if (a == &ec_app && a->pn.f_sticky && !popup && !held) {   /* 0.6.12 (owner): sticky F1 / F2 stays in view in
                                                    * the box at the right, on the accent, whatever the line says */
        R(EC_C_ACC, W - 11, 0, 11, 10);
        T(EC_C_BG, a->pn.f_sticky == 1 ? "F1" : "F2", W - 6, 3, EC_AL_CENTER);
    }
}

static void header(const ec_app_t *a, const char *title, int page, const char *right)
{
    char pg[8];
    if (page > 0)
        snprintf(pg, sizeof pg, "%d", page);
    else
        pg[0] = 0;
    ec_scr_header_s(a, title, pg, right);
}

void ec_scr_header_page(const ec_app_t *a, const char *title, int page, const char *right)
{
    int k, n;
    if (a == &ec_app && !a->settings && ec_nav_place(&k, &n) && n > 1) {
        char pp[8];
        snprintf(pp, sizeof pp, "P%d", k + 1);
        ec_scr_header_s(a, title, pp, right);
    } else
        header(a, title, page, right);
}

/* 0.6.1: the hold menu (ec_holdmenu.h), the only thing over the screen: its values as a list under the header, the
 * cursor's row bright on a band, the value set now marked, the list scrolled to keep the cursor in view */
static void hold_menu(const ec_app_t *a)
{
    const int n = ec_hm_count(), cur = ec_hm_cursor(), now = ec_hm_current(), rows = 8, rh = 13;
    int top = cur - rows / 2, i;
    char r[16];
    if (top > n - rows)
        top = n - rows;
    if (top < 0)
        top = 0;
    snprintf(r, sizeof r, "%d/%d", cur + 1, n);
    ec_scr_header_s(a, "", "", r);
    for (i = 0; i < rows && top + i < n; i++) {
        const int k = top + i, y = 12 + i * rh, on = k == cur;
        if (on)                                    /* 0.6.1: 2 units round the name (over the gaps) */
            R(EC_C_L3, 0, y - 1, W, rh + 1);
        if (k == now)
            R(on ? EC_C_ACC : EC_C_L8, 2, y + 3, 3, 6);
        T2(on ? EC_C_L15 : EC_C_L8, ec_hm_label(k), 9, y + 1, EC_AL_LEFT);
    }
    if (top > 0)                                   /* more above / below: a block at the right edge */
        R(EC_C_L8, W - 4, 12, 3, 4);
    if (top + rows < n)
        R(EC_C_L8, W - 4, 12 + rows * rh - 5, 3, 4);
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
    const char *tag = ec_cat_tag(a->cat), *title = a->settings ? "SETUP" : pg->title;
    if (a->cat == EC_CAT_SRC && !a->bus) {         /* 0.6.19 (owner): the pages are named after the track's machine:
                                                    * LOOP/LOOP, SLICE/SLICE, RAZOR/MACHINE, S.POLY/WARP, NEIGH/.. */
        tag = ec_machine_name(iv(tv(a), EC_P_MACHINE));
        if (title && !strcmp(title, "LOOP"))
            title = tag;
    }
    snprintf(out, cap, "%s/%s", tag, title);     /* 0.6.0: the tag */
}

/* ------------------------------------------------------------------------------------------- cells --- */
/* 0.5.0: a matrix destination's mark: the source's tag ("LFO1"; several sources: their count, "2 MOD") */
static void mark_text(uint32_t m, char *b, size_t cap)
{
    int n = 0, s = 0, k;
    for (k = 1; k < EC_MS_N; k++)
        if (m >> k & 1u) {
            n++;
            s = k;
        }
    if (n == 1)
        snprintf(b, cap, "%s", ec_mtx_src_name(s));
    else
        snprintf(b, cap, "%d MOD", n);
}

/* 0.6.1: the held source key's EC_MS_* (0 none) */
static int held_src(const ec_app_t *a) { return a->mk.held >= 0 ? ec_mk_src_of_key(a->mk.held) : 0; }

/* 0.6.1 (docs/UX-V2.md §10): a cell's step-lock state for param id: on a held step that locks it, the whole cell in
 * LOCK (the value is the lock's: the caller's); no step held and some step of the pattern locks it, the outline */
static unsigned lock_state(const ec_app_t *a, int id, int locked)
{
    if (locked)
        return EC_KC_LOCK;
    if (!a->step_held && !a->bus && !a->settings && (EC_PARAMS[id].flags & EC_PF_LOCKABLE) &&
        ec_ux_locked_somewhere(id))
        return EC_KC_SOME;
    return 0;
}

static void cells(const ec_app_t *a, const ec_page_t *pg)
{
    const int lane = a->ux.f3 ? 0 : ec_app_lane(a, pg);
    const int nl = a->ux.f3 ? 1 : ec_ux_lanes(pg->n);
    static char lab[4][24], val[4][24], mk[4][12];
    ec_kit_cell_t c[4];
    int i;
    memset(c, 0, sizeof c);
    for (i = 0; i < 4; i++) {
        const int ci = lane * 4 + i;
        const ec_item_t *it;
        ec_item_t mx;
        uint32_t marks;
        int locked = 0;
        float v;
        if (ci >= pg->n || pg->it[ci].kind == EC_IT_BLANK)
            continue;
        ec_mk_item(&pg->it[ci], &mx);                 /* 0.5.0: a MATRIX cell is its row's param */
        it = &mx;
        if (it->kind == EC_IT_PARAM && !a->settings && !a->ux.f3 && ec_mk_hot_slot(it->id) >= 0) {
            /* 0.5.0: a source key held: the cell shows the source > param and the slot's DEPTH from the centre
             * (0.6.1: in SRC, and the depth param's three lock states) */
            const int k = ec_mk_hot_slot(it->id), did = ec_mtx_depth_id(k);
            const ec_ptrack_t *tr = &a->proj.pat[a->proj.current].tr[a->track];
            float d = tr->v[did];
            const int lk = !a->bus && ec_app_held_lock(a, did, &d);
            c[i].flags |= lock_state(a, did, lk);
            snprintf(lab[i], sizeof lab[i], "%s>%s", ec_mtx_src_name(ec_mtx_src(tr->v, k)), ec_app_item_label(it));
            ec_param_format(did, d, 3, val[i], sizeof val[i]);
            c[i].label = lab[i];
            c[i].value = val[i];
            c[i].frac = d / 100.0f;
            c[i].flags |= EC_KC_DEPTH | EC_KC_HOT;
            continue;
        }
        v = ec_app_item_value(a, it, &locked);
        ec_app_item_format(a, it, v, val[i], sizeof val[i]);
        snprintf(lab[i], sizeof lab[i], "%s", ec_app_item_label(it));
        c[i].label = lab[i];
        c[i].value = val[i];
        if (locked)
            c[i].flags |= EC_KC_LOCK;
        else if (it->kind == EC_IT_PARAM)
            c[i].flags |= lock_state(a, it->id, 0);
        if ((it->flags & EC_IF_STOPPED) && a->seq.playing)
            c[i].flags |= EC_KC_DIM;
        /* 0.6.0 (the mocks): a live value in the accent: PATTERN LAUNCH's NEXT while one is queued, KEYBOARD's target
         * step, the crossfader while it glides */
        if ((it->kind == EC_IT_UX && it->id == EC_UX_NEXT && a->seq.pending >= 0) ||
            (it->kind == EC_IT_UX && it->id == EC_UX_TARGET && a->pn.target) ||
            (it->kind == EC_IT_E5 && it->id == EC_X_XFD && a->e5.gliding))
            c[i].flags |= EC_KC_ACC;
        if (it->kind == EC_IT_PARAM && !a->settings && (marks = ec_mk_marks(it->id)) != 0) {
            mark_text(marks, mk[i], sizeof mk[i]);
            c[i].mark = mk[i];
        }
        if (it->kind == EC_IT_PARAM && EC_PARAMS[it->id].fmt == EC_PF_MDEP) {   /* 0.5.0: DEPTH from the centre */
            int k;
            c[i].frac = v / 100.0f;
            c[i].flags |= EC_KC_DEPTH;
            for (k = 0; k < EC_MTX_SLOTS; k++)        /* 0.6.1: a slot of the held source: its bar in SRC */
                if (ec_mtx_depth_id(k) == it->id && held_src(a) && ec_mtx_src(tv(a), k) == held_src(a))
                    c[i].flags |= EC_KC_HOT;
        } else
            c[i].frac = ec_app_item_frac(a, it, v);
        /* the actual (resolved) value as a mark when it differs (BASE_VALUE_RESOLVER) */
        if (it->kind == EC_IT_PARAM && !a->bus && a->seq.playing && it->id != EC_P_LOOP_START &&
            it->id != EC_P_LOOP_END) {
            const float act = ec_seq_value(&a->seq, a->track, it->id);
            if (act != v) {
                c[i].actual = ec_app_item_frac(a, it, act);
                c[i].flags |= EC_KC_ACTUAL;
            }
        }
    }
    ec_kit_cells(c, nl, lane);
}

/* a param's envelope stage (EC_KIT_ST_*), 0 none */
static unsigned stage_of(int id)
{
    switch (id) {
    case EC_P_FENV_ATTACK:
    case EC_P_ENV_ATTACK:
    case EC_P_MENV_ATTACK: return EC_KIT_ST_A;
    case EC_P_FENV_DECAY:
    case EC_P_ENV_DECAY:
    case EC_P_MENV_DECAY: return EC_KIT_ST_D;
    case EC_P_FENV_SUSTAIN:
    case EC_P_ENV_SUSTAIN:
    case EC_P_MENV_SUSTAIN: return EC_KIT_ST_S;
    case EC_P_FENV_HOLD:
    case EC_P_ENV_HOLD: return EC_KIT_ST_H;
    case EC_P_FENV_RELEASE:
    case EC_P_ENV_RELEASE:
    case EC_P_MENV_RELEASE: return EC_KIT_ST_R;
    default: return 0;
    }
}

/* the envelope's selected stage (norns' draw_envelope_bars: the stages the knobs edit, bright): the stage of the value
 * knob turned last among this lane's stage cells; none turned yet: every stage the lane edits */
static unsigned hot_stages(const ec_app_t *a, const ec_page_t *pg)
{
    const int lane = ec_app_lane(a, pg);
    unsigned hot = 0, last = 0;
    uint32_t last_ms = 0;
    int i;
    for (i = lane * 4; i < lane * 4 + 4 && i < pg->n; i++) {
        const uint32_t t = a->ux.knob_ms[EC_E_K1 + i - lane * 4];
        unsigned st;
        if (pg->it[i].kind != EC_IT_PARAM || !(st = stage_of(pg->it[i].id)))
            continue;
        hot |= st;
        if (t && t >= last_ms)
            last_ms = t, last = st;
    }
    return last ? last : hot;
}

/* ------------------------------------------------------------------------------------------- waveform --- */
/* E3: slice k's columns (razor machines: their own points; grid: the even division) */
static void slice_cols(const ec_app_t *a, int k, int cnt, float lo, float hi, int x0, int cols, int *sx, int *ex)
{
    float s0 = 128.0f * (float)k / (float)cnt, e0 = 128.0f * (float)(k + 1) / (float)cnt;
    if (ec_razor_is_razor(iv(tv(a), EC_P_MACHINE)))
        ec_razor_range(&a->proj, a->track, k + 1, &s0, &e0);
    *sx = x0 + (int)((lo + (hi - lo) * s0 / 128.0f) * cols);
    *ex = x0 + (int)((lo + (hi - lo) * e0 / 128.0f) * cols);
}

/* 0.6.7 (owner): the LOOP ENGINE waveform is a view window, in whole-file fractions [wa, wb]: the range (the target
 * step's range lock, else the track's) inside the slot's trim on the LOOP / MACHINE / WARP pages, so they show only
 * what is between range start and end; the whole file on the RANGE page (where the range is set) */
typedef struct {
    float wa, wb;
    int x0, cols;
} view_t;
static int vx(const view_t *w, float f) { return w->x0 + (int)((f - w->wa) / (w->wb - w->wa) * (float)w->cols + 0.5f); }
static void view_of(const ec_app_t *a, int whole, view_t *w)
{
    const float *v = tv(a);
    const ec_slot_t *sl = &a->proj.slot[ec_app_track_slot(a, a->track)];
    const int tgt = ec_app_target_step(a);
    float r0 = v[EC_P_RANGE_START], r1 = v[EC_P_RANGE_END], x;
    w->x0 = 2;
    w->cols = W - 4;
    if (whole) {
        w->wa = 0;
        w->wb = 1;
        return;
    }
    if (tgt >= 0) {
        const ec_step_t *st = &a->proj.pat[a->proj.current].tr[a->track].steps[tgt];
        if (ec_step_lock_get(st, EC_P_RANGE_START, &x))
            r0 = x;
        if (ec_step_lock_get(st, EC_P_RANGE_END, &x))
            r1 = x;
    } else if (a->seq.playing) {                   /* 0.6.12: playing, the resolved range (the step's lock) */
        r0 = ec_seq_value(&a->seq, a->track, EC_P_RANGE_START);
        r1 = ec_seq_value(&a->seq, a->track, EC_P_RANGE_END);
    }
    w->wa = sl->trim_start + (sl->trim_end - sl->trim_start) * r0 / 128.0f;
    w->wb = sl->trim_start + (sl->trim_end - sl->trim_start) * r1 / 128.0f;
    if (w->wb < w->wa) {
        const float t = w->wa;
        w->wa = w->wb, w->wb = t;
    }
    if (w->wb - w->wa < 1e-4f)
        w->wb = w->wa + 1e-4f;
}

/* the window's peaks, one per 2-unit column, from the sample's frames (a cache: recomputed only when the window,
 * the sample or its decoded length changes; at most 64 frames read per column) */
#define WAVE_N 64
static const uint8_t *window_peaks(const ec_ss_sample_t *smp, const view_t *w, int n)
{
    static uint8_t pk[WAVE_N];
    static const ec_ss_sample_t *c_smp;
    static float c_wa = -1, c_wb = -1;
    static uint32_t c_ready;
    static int c_n;
    int c;
    if (smp == c_smp && w->wa == c_wa && w->wb == c_wb && smp->frames_ready == c_ready && n == c_n)
        return pk;
    c_smp = smp, c_wa = w->wa, c_wb = w->wb, c_ready = smp->frames_ready, c_n = n;
    for (c = 0; c < n && c < WAVE_N; c++) {
        const double f0 = (w->wa + (w->wb - w->wa) * (double)c / n) * smp->frames;
        const double f1 = (w->wa + (w->wb - w->wa) * (double)(c + 1) / n) * smp->frames;
        uint32_t i = (uint32_t)(f0 < 0 ? 0 : f0), e = (uint32_t)(f1 < 0 ? 0 : f1), step;
        int m = 0;
        if (e > smp->frames_ready)
            e = smp->frames_ready;
        if (e <= i)
            e = i + 1;
        step = (e - i) / 64 ? (e - i) / 64 : 1;
        for (; i < e && i < smp->frames_ready; i += step) {
            const int l = smp->data[2 * i] < 0 ? -smp->data[2 * i] : smp->data[2 * i];
            const int r = smp->data[2 * i + 1] < 0 ? -smp->data[2 * i + 1] : smp->data[2 * i + 1];
            if (l > m)
                m = l;
            if (r > m)
                m = r;
        }
        pk[c] = (uint8_t)(m >> 7 > 255 ? 255 : m >> 7);
    }
    return pk;
}
static void view_wave(const ec_ss_sample_t *smp, const view_t *w, int top, int bot, int c_in, int c_out, int rs, int re)
{
    const int n = w->cols / 2;
    if (!smp->data || !smp->frames_ready) {        /* (still decoding the start: the overview) */
        ec_kit_wave(smp->overview, EC_SS_OVERVIEW, w->x0, w->x0 + w->cols, top, bot, c_in, c_out, rs, re);
        return;
    }
    ec_kit_wave(window_peaks(smp, w, n), n, w->x0, w->x0 + w->cols, top, bot, c_in, c_out, rs, re);
}

/* the live grains (GRAIN warps), when the engine publishes them: the frame, the waveform dim, the grains, in the
 * view window. 0 = none */
/* 0.6.14 (owner): the grain view keeps the region in view: the bright span ls..le (the caller's columns, in w) and
 * the brackets / playhead the caller draws after it */
static int grain_view(const ec_app_t *a, const ec_ss_sample_t *smp, const view_t *w, int top, int bot, int ls, int le)
{
#ifndef EC_KIT_LOCAL_GRAIN_VIEW
    ec_grain_view_t g[EC_GRAIN_VIEW_MAX];
    const int n = ec_dsp_grains(a->dsp, a->track, g);
    view_t in = *w;
    int k, m = 0;
    if (n <= 0)
        return 0;

    for (k = 0; k < n; k++) {                      /* into the window; the ones outside are not drawn */
        const float p = (g[k].pos - w->wa) / (w->wb - w->wa);
        if (p < -0.05f || p > 1.05f)
            continue;
        g[m] = g[k];
        g[m].pos = p;
        g[m].len = g[k].len / (w->wb - w->wa);
        m++;
    }
    view_wave(smp, &in, top, bot, EC_C_L8, EC_C_L3, ls, le);   /* (owner: no frame box; the live region brighter) */
    ec_kit_grains(g, m, w->x0, w->x0 + w->cols, top);
    return 1;
#else
    (void)a, (void)smp, (void)w, (void)top, (void)bot, (void)ls, (void)le;
    return 0;
#endif
}

/* the brackets: a region's start and end, 2 units, the lock colour when they are a step's lock */
static void brackets(int rs, int re, int top, int bot, int c)
{
    R(c, rs - 2, top - 3, 2, bot - top + 6);       /* 0.6.1: 2 units */
    R(c, rs - 2, top - 3, 5, 2);
    R(c, rs - 2, bot + 1, 5, 2);                   /* 0.6.17 (owner): a whole bracket: the bottom corner too */
    R(c, re, top - 3, 2, bot - top + 6);
    R(c, re - 3, top - 3, 5, 2);
    R(c, re - 3, bot + 1, 5, 2);
}

/* the track's sample area: waveform with region / slices / playhead, or why there is none. whole: the RANGE page
 * (the whole file, the range bracketed), else the range's window (the loop region bracketed). 0.6.7: with a target
 * step the brackets are that step's region (its lock, else the track's; RANGE: its range) */
static void sample_area(const ec_app_t *a, int top, int bot, int whole)
{
    const int s = ec_app_track_slot(a, a->track);
    const ec_slotrt_t *rt = &a->slot[s];
    const ec_ss_sample_t *smp = ec_app_track_sample(a, a->track);
    const float *v = tv(a);
    char buf[64];
    view_t w;
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
        ec_gfx_dots_h(2, W - 2, (top + bot) / 2, 2, EC_C_L3);
        if (rt->state == EC_SL_MISSING || rt->state == EC_SL_ERROR) {
            T2(EC_C_ALERT, rt->state == EC_SL_MISSING ? "MISSING" : "ERROR", W / 2, top + 4, EC_AL_CENTER);
            fit(buf, sizeof buf, p[0] ? p : rt->err, 28);
            T(EC_C_L8, buf, W / 2, bot - 9, EC_AL_CENTER);
        } else
            T2(EC_C_L5, ec_app_track_off(a, a->track) ? "SLOT OFF" : "NO SAMPLE", W / 2, top + 8, EC_AL_CENTER);
        return;
    }
    if (rt->state == EC_SL_LOADING)
        return;
    view_of(a, whole, &w);
    {
        ec_track_status_t st;
        const int tgt = ec_app_target_step(a);
        const ec_step_t *tst = tgt >= 0 ? &a->proj.pat[a->proj.current].tr[a->track].steps[tgt] : 0;
        const ec_slot_t *sl = &a->proj.slot[s];
        int rs, re, k, locked = 0;
        ec_dsp_track_status(a->dsp, a->track, &st);
        if (is_slice(v) && !whole) {               /* the slices divide the range (the view on LOOP ENGINE's pages;
                                                    * 0.6.28: the RANGE page is the loop track's code, exactly);
                                                    * 0.6.18 (owner): drawn as the SLICE editor draws them: the edges
                                                    * dark behind the wave, a sounding slice a cream block with the
                                                    * wave cut out, muted slices dim */
            const float lo = w.wa, hi = w.wb;        /* 0.6.29 (owner): the slices chop the range, and the view IS
                                                    * the range (the target's lock, else the resolved one) */
            const int cnt = clampi(iv(v, EC_P_SLICE_COUNT), 1, 32);
            const int razor = ec_razor_is_razor(iv(v, EC_P_MACHINE));
            const ec_razor_t *rz = &a->proj.razor[a->track];
            const int sx0 = w.x0, sx1 = w.x0 + w.cols, scols = sx1 - sx0;
            const int mid = (top + bot) / 2, half = (bot - top) / 2;
            uint32_t on = a->seq.tr[a->track].pads;
            int c;
            for (k = 0; k < st.voices && k < EC_MAX_VOICES_TRACK; k++)
                if (st.voice_slice[k] >= 1 && st.voice_slice[k] <= 32)
                    on |= 1u << (st.voice_slice[k] - 1);
            for (k = 0; k < cnt; k++)                  /* the sounding slices: cream blocks */
                if (on >> k & 1u) {
                    int sx, ex;
                    slice_cols(a, k, cnt, 0, 1, sx0, scols, &sx, &ex);
                    if (ex > sx)
                        R(EC_C_L15, sx, top - 2, ex - sx, bot - top + 4);
                }
            for (k = 0; k < cnt; k++) {                /* the other slices' edges behind the wave, dark */
                int sx, ex;
                if (on >> k & 1u)
                    continue;
                slice_cols(a, k, cnt, 0, 1, sx0, scols, &sx, &ex);
                if (k > 0 || razor)
                    R(EC_C_L3, sx & ~1, top - 2, 2, bot - top + 6);
                if (razor && k + 1 < cnt) {            /* a gap before the next slice: its end too */
                    int nx, nex;
                    slice_cols(a, k + 1, cnt, 0, 1, sx0, scols, &nx, &nex);
                    if (nx - ex > 2)
                        R(EC_C_L3, ex & ~1, top - 2, 2, bot - top + 6);
                }
            }
            if (smp->data && smp->frames_ready && scols > 2) {   /* the wave per 2-unit column, coloured by its slice */
                const double f0 = lo * smp->frames, f1 = hi * smp->frames;
                for (c = 0; c + 1 < scols; c += 2) {
                    const double a0 = f0 + (f1 - f0) * c / scols, a1 = f0 + (f1 - f0) * (c + 2) / scols;
                    const float x = 128.0f * ((float)c + 1.0f) / (float)scols;
                    uint32_t i0 = a0 < 0 ? 0 : (uint32_t)a0, i1 = a1 < 0 ? 0 : (uint32_t)a1, i, step;
                    int pk = 0, hh, sli = 0, col;
                    if (razor) {
                        for (k = 0; k < cnt; k++)
                            if (x >= rz->start[k] && x < rz->end[k])
                                sli = k + 1;
                    } else
                        sli = (int)(x / 128.0f * (float)cnt) + 1;
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
                    hh = (int)((long)pk * (half - 1) / 32768);
                    col = sli && (on >> (sli - 1) & 1u) ? EC_C_BG
                        : !sli || (razor && rz->mute[sli - 1]) ? EC_C_L5 : EC_C_L12;
                    R(col, sx0 + c, mid - hh, 2, 2 * hh + 2);
                }
            } else
                view_wave(smp, &w, top, bot, EC_C_L12, EC_C_L5, sx0, sx1);
            for (k = 0; k < cnt; k++)                  /* the sounding slices' edges, in front */
                if (on >> k & 1u) {
                    int sx, ex;
                    slice_cols(a, k, cnt, 0, 1, sx0, scols, &sx, &ex);
                    R(EC_C_L15, sx & ~1, top - 2, 2, bot - top + 6);
                    R(EC_C_L15, (ex - 2) & ~1, top - 2, 2, bot - top + 6);
                }
            for (k = 0; k < st.voices && k < EC_MAX_VOICES_TRACK; k++) {
                const int px = vx(&w, st.voice_pos[k]);
                if (px >= w.x0 && px < w.x0 + w.cols)
                    ec_kit_playhead(px, top - 1, bot + 1);
            }
            return;
        }
        if (whole) {                               /* RANGE: the range (the target's lock, else the track's; 0.6.28:
                                                    * LIVE, the resolved one: the playing step's lock shows) */
            float r0 = tst ? v[EC_P_RANGE_START] : ec_seq_value(&a->seq, a->track, EC_P_RANGE_START);
            float r1 = tst ? v[EC_P_RANGE_END] : ec_seq_value(&a->seq, a->track, EC_P_RANGE_END), x;
            if (tst && ec_step_lock_get(tst, EC_P_RANGE_START, &x))
                r0 = x, locked = 1;
            if (tst && ec_step_lock_get(tst, EC_P_RANGE_END, &x))
                r1 = x, locked = 1;
            rs = vx(&w, sl->trim_start + (sl->trim_end - sl->trim_start) * r0 / 128.0f);
            re = vx(&w, sl->trim_start + (sl->trim_end - sl->trim_start) * r1 / 128.0f);
        } else if (tst) {                          /* a selected step: its region (a lock, else the track's); 0.6.26:
                                                    * keys held too: the entry is written into it at every press, and
                                                    * the live span plays in another step's range */
            float l0 = v[EC_P_LOOP_START], l1 = v[EC_P_LOOP_END], x;
            if (ec_step_lock_get(tst, EC_P_LOOP_START, &x))
                l0 = x, locked = 1;
            if (ec_step_lock_get(tst, EC_P_LOOP_END, &x))
                l1 = x, locked = 1;
            rs = w.x0 + (int)(l0 / 128.0f * (float)w.cols + 0.5f);
            re = w.x0 + (int)(l1 / 128.0f * (float)w.cols + 0.5f);
        } else {                                   /* the track's region: static markers (0.6.12, as norns) */
            const float l0 = ec_seq_value(&a->seq, a->track, EC_P_LOOP_START);
            const float l1 = ec_seq_value(&a->seq, a->track, EC_P_LOOP_END);
            rs = w.x0 + (int)(l0 / 128.0f * (float)w.cols + 0.5f);
            re = w.x0 + (int)(l1 / 128.0f * (float)w.cols + 0.5f);
        }
        rs = clampi(rs, w.x0, w.x0 + w.cols);
        re = clampi(re, w.x0, w.x0 + w.cols);
        if (re <= rs)
            re = rs + 2;
        {                                          /* 0.6.12: the bright span is what the engine plays now (the
                                                    * loop keys' span, a scrub); the brackets stay put */
            int ls = clampi(vx(&w, st.region_start), w.x0, w.x0 + w.cols);
            int le = clampi(vx(&w, st.region_end), w.x0, w.x0 + w.cols);
            if ((!st.reader_on && !a->seq.tr[a->track].live_keys) || tst)   /* (a selected step: its lock) */
                ls = rs, le = re;
            if (le <= ls)
                le = ls + 2;
            if (!tst && a->seq.tr[a->track].live_keys && st.reader_on)   /* 0.6.17 (owner): LIVE, the brackets
                                                                   * follow the loop keys while they are held */
                rs = ls, re = le;
            if (!grain_view(a, smp, &w, top, bot, ls, le))   /* 0.6.14: the grains too get the brackets below */
                view_wave(smp, &w, top, bot, EC_C_L15, EC_C_L5, ls, le);
        }
        if (whole || tst || !st.reader_on || a->seq.tr[a->track].live_keys)   /* 0.6.27 (owner): LIVE, playing,
                                                    * no key held: no brackets (the bright span says what plays) */
            brackets(rs, re, top, bot, locked ? EC_C_LOCK : EC_C_L12);
        if (st.reader_on) {
            const int px = vx(&w, st.pos_sample);
            if (px >= w.x0 && px < w.x0 + w.cols)
                ec_kit_playhead(px, top - 1, bot + 1);
        }
    }
}

/* the loop-key ruler (slices: the pads) */
static void key_ruler(const ec_app_t *a, int ky)
{
    const float *v = tv(a);
    ec_track_status_t st;
    uint16_t lit = 0, in = 0;
    int i, n;
    ec_dsp_track_status(a->dsp, a->track, &st);
    if (is_slice(v)) {
        n = clampi(iv(v, EC_P_SLICE_COUNT), 1, 16);
        lit = (uint16_t)(a->seq.tr[a->track].pads | a->seq.tr[a->track].fired_slices);
        in = 0xFFFFu;
    } else {
        const int div = clampi((iv(v, EC_P_LOOP_DIVISION) + 1) / 2 * 2, 2, 32);
        const float units = 128.0f / (float)div;
        const float ls = v[EC_P_LOOP_START], le = v[EC_P_LOOP_END];
        n = div;
        lit = (uint16_t)st.keys_held;
        for (i = 0; i < 16 && i < div; i++) {
            const float ks = (float)i * units, ke = ks + units;
            if (ke > ls + 0.001f && ks < le - 0.001f)
                in |= (uint16_t)(1u << i);
        }
    }
    ec_kit_key_ruler(ky, lit, in, n);
}

/* 0.6.6: LOOP ENGINE with a target step (docs/UX-V2.md "LOOP ENGINE step entry"), in place of the loop-key ruler:
 * the step page's 16 steps (y 64..69: a trig a bright block, else a floor), the target (a bright cap) and the
 * playhead (an accent cap) at y 61..62, and under each step its region lock (RANGE page: its range lock) as a LOCK bar
 * placed in its cell like the region in the loop (y 71..72). The waveform's brackets show the target's region
 * (sample_area) */
static int lock_pair(const ec_step_t *st, int range, float *lo, float *hi)
{
    const int a = range ? EC_P_RANGE_START : EC_P_LOOP_START, b = range ? EC_P_RANGE_END : EC_P_LOOP_END;
    const int ha = ec_step_lock_get(st, a, lo), hb = ec_step_lock_get(st, b, hi);
    if (!ha && !hb)
        return 0;
    if (!ha)
        *lo = 0;
    if (!hb)
        *hi = 128;
    return 1;
}
/* 0.6.12 (owner): the step strip shows on both layers. tint < 0: the STEP layer (white); else the LOOP layer, where
 * the strip is in the layer's colour, red (ALERT) with steps selected (the loop keys write into them), blue (SRC)
 * LIVE, with a rule under it in that colour */
static void loop_steps(const ec_app_t *a, int range, int tint)
{
    const ec_ptrack_t *tr = &a->proj.pat[a->proj.current].tr[a->track];
    const int len = ec_seq_pattern_steps(&a->seq, a->track);
    const int play = ec_seq_step(&a->seq, a->track);
    /* 0.6.14 (owner): on the LOOP layer every element is in the tint (red / blue): the trigs, the empty steps'
     * floors, the selection and the playhead; the STEP layer is white with the accent playhead */
    const int bright = EC_C_L15, mid = tint >= 0 ? tint : EC_C_L12;   /* 0.6.14 (owner): the selected steps white */
    const int floor_a = tint >= 0 ? tint : EC_C_L5, floor_b = tint >= 0 ? tint : EC_C_L3;
    const int head = tint >= 0 ? tint : EC_C_ACC;
    int i;
    for (i = 0; i < 16; i++) {
        const int idx = a->step_page * 16 + i, x = 3 + i * 7;
        const ec_step_t *st = &tr->steps[idx];
        float lo, hi;
        if (idx >= len) {
            R(EC_C_L3, x + 1, 68, 2, 2);
            continue;
        }
        const int sel = a == &ec_app && ec_lp_is_sel(idx);   /* 0.6.9: every selected step */
        if (st->trig || st->slices)
            R(sel ? bright : mid, x, 64, 5, 6);
        else
            R(sel ? bright : i % 4 == 0 ? floor_a : floor_b, x, 68, 5, 2);
        if (sel)                                     /* selected: its cap joins its cell (a full column) */
            R(bright, x, 61, 5, sel && !(st->trig || st->slices) ? 9 : 3);
        if (idx == play && (a->seq.playing || a->seq.paused))
            R(head, x, 61, 5, 2);
        if (lock_pair(st, range, &lo, &hi)) {        /* its lock, placed in the cell as in the loop */
            int l = (int)floorf(lo / 128.0f * 5.0f), r = (int)ceilf(hi / 128.0f * 5.0f);
            l = clampi(l, 0, 3);
            r = clampi(r, l + 2, 5);                 /* at least 2 units */
            R(EC_C_LOCK, x + l, 71, r - l, 2);
        }
    }
}
#define A_IS_BUS(a) ((a)->bus != 0)
/* 0.6.14 (owner): while B11 is held the cells area shows the pattern's pages instead of the knobs' params: up to 8
 * tiles (the shown page filled, a page in the loop outlined bright, a muted one dim, beyond the length a dot; the
 * playing page's number in the accent), and under them LEN (K1) and RATE (K2) */
void ec_scr_pages_panel(const ec_app_t *a)
{
    const float *v = tv(a);
    const int len = ec_seq_pattern_steps(&a->seq, a->track), pages = (len + 15) / 16;
    const uint16_t m = ec_e5_page_loop_mask(&a->proj, a->track, pages);
    const int play = ec_seq_step(&a->seq, a->track) / 16;
    char b[16];
    int i;
    R(EC_C_L3, 0, 76, W, 2);
    for (i = 0; i < 8; i++) {
        const int x = 2 + i * 15, y = 80, w = 13, h = 15;
        const int cur = i == a->step_page, in = i < pages && (m >> i & 1u);
        snprintf(b, sizeof b, "%d", i + 1);
        if (i >= pages) {
            R(EC_C_L3, x + 5, y + 7, 3, 2);
            continue;
        }
        if (cur)
            R(EC_C_L15, x, y, w, h);
        else if (in) {
            R(EC_C_L12, x, y, w, h);
            R(EC_C_BG, x + 2, y + 2, w - 4, h - 4);
        } else {
            R(EC_C_L5, x, y, w, h);
            R(EC_C_BG, x + 2, y + 2, w - 4, h - 4);
        }
        T(cur ? EC_C_BG : i == play && (a->seq.playing || a->seq.paused) ? EC_C_ACC : in ? EC_C_L15 : EC_C_L5, b,
          x + w / 2, y + 4, EC_AL_CENTER);
    }
    T(EC_C_L8, "LEN", 2, 100, EC_AL_LEFT);
    T(EC_C_L8, "K1", 57, 100, EC_AL_RIGHT);
    snprintf(b, sizeof b, "%d", len);
    T2(EC_C_L15, b, 2, 107, EC_AL_LEFT);
    T(EC_C_L8, "RATE", 62, 100, EC_AL_LEFT);
    T(EC_C_L8, "K2", W - 3, 100, EC_AL_RIGHT);
    T2(EC_C_L15, ec_rate_label(iv(v, EC_P_RATE)), 62, 107, EC_AL_LEFT);
    R(EC_C_L3, 59, 97, 2, 23);
}
/* the step strip: this step page's trigs, ghosts, locks, the held step, the playhead; the pages under it */
static void step_strip(const ec_app_t *a, int top, int bot, int pages_y)
{
    const ec_ptrack_t *tr = &a->proj.pat[a->proj.current].tr[a->track];
    const int len = ec_seq_pattern_steps(&a->seq, a->track);
    const int play = ec_seq_step(&a->seq, a->track);
    const int pages = (len + 15) / 16;
    const int hs = ec_app_held_step(a);
    uint16_t trigs = 0, ghosts = 0, locks = 0, beyond = 0;
    int i, held = -1, pl = -1;
    for (i = 0; i < 16; i++) {
        const int idx = a->step_page * 16 + i;
        const ec_step_t *s = &tr->steps[idx];
        const int on = s->trig || s->slices;
        if (idx >= len) {
            beyond |= (uint16_t)(1u << i);
            continue;
        }
        if (on && !ec_step_is_ghost(s, tr->v))
            trigs |= (uint16_t)(1u << i);
        else if (on)
            ghosts |= (uint16_t)(1u << i);
        if (s->nlock && on)
            locks |= (uint16_t)(1u << i);
        if (idx == play && (a->seq.playing || a->seq.paused))
            pl = i;
        if (idx == hs)
            held = i;
    }
    ec_kit_steps(top, bot, trigs, ghosts, locks, beyond, held, pl);
    if (a == &ec_app && ec_lp_active())            /* 0.6.14 (owner): the selected steps (white caps), each trig's
                                                    * VELOCITY lock as its block's height, a PITCH lock as a blue
                                                    * mark (above the middle: up, below: down) */
        for (i = 0; i < 16; i++) {
            const int idx = a->step_page * 16 + i, x = 3 + i * 7;
            const ec_step_t *s = &tr->steps[idx];
            float x1;
            if (idx >= len)
                continue;
            if ((trigs | ghosts) >> i & 1u) {
                if (ec_step_lock_get(s, EC_P_VELOCITY, &x1)) {
                    const float vv = x1 < 0 ? 0 : x1 > 1 ? 1 : x1;
                    const int h = (int)((1.0f - vv) * (float)(bot - top - 2) + 0.5f) / 2 * 2;
                    if (h > 0)
                        R(EC_C_BG, x, top, 5, h);
                }
                if (ec_step_lock_get(s, EC_P_PITCH, &x1))
                    R(EC_C_SRC, x + 1, x1 >= tr->v[EC_P_PITCH] ? top + 2 : bot - 4, 3, 2);
            }
            if (ec_lp_is_sel(idx)) {
                R(EC_C_L15, x, top - 4, 5, 2);
                if (!((trigs | ghosts) >> i & 1u))
                    R(EC_C_L15, x, bot - 2, 5, 2);
            }
        }
    for (i = 0; i < pages && pages > 1 && pages_y > 0; i++)
        R(i == a->step_page ? EC_C_L15 : EC_C_L5, W / 2 - pages * 2 + i * 4, pages_y, 2, 2);
}

static const char *file_name(const ec_app_t *a)
{
    const ec_ss_sample_t *smp = ec_app_track_sample(a, a->track);
    const ec_slot_t *sl = &a->proj.slot[ec_app_track_slot(a, a->track)];
    if (smp)
        return smp->name;
    if (sl->path[0])
        return strrchr(sl->path, '/') ? strrchr(sl->path, '/') + 1 : sl->path;
    return "-";
}

/* SRC (LOOP ENGINE): the caption, the waveform y 20..58, then what the white keys are: the steps (y 63) or the loop
 * keys (y 66) */
static void src_screen(const ec_app_t *a, const ec_page_t *pg)
{
    const float *v = tv(a);
    const ec_slot_t *sl = &a->proj.slot[ec_app_track_slot(a, a->track)];
    const int mach = iv(v, EC_P_MACHINE);
    char t[32], r[16], left[40];
    title_of(a, pg, t, sizeof t);
    right_info(a, r, sizeof r);
    ec_scr_header_page(a, t, a->page[a->cat] + 1, r);
    switch (pg->pic) {
    case EC_PIC_WAVE: fit(left, sizeof left, file_name(a), 20); break;
    case EC_PIC_WAVE_MACHINE: snprintf(left, sizeof left, "%s", ec_machine_name(mach)); break;
    case EC_PIC_WAVE_WARP: fit(left, sizeof left, file_name(a), 20); break;   /* the warp: on the right */
    default: snprintf(left, sizeof left, "RANGE"); break;
    }
    if (mach == EC_MACH_NEIGHBOR)   /* E3 */
        snprintf(left, sizeof left, "FROM T%d", a->track);
    if (mach == EC_MACH_INPUT)      /* E6 */
        snprintf(left, sizeof left, "INPUT");
    T(EC_C_L15, left, 2, 13, EC_AL_LEFT);
    if (pg->pic == EC_PIC_WAVE_WARP && mach != EC_MACH_NEIGHBOR && mach != EC_MACH_INPUT) {
        /* WARP: the warp's short name on the right (the mock's "GRAIN"); E3: only the spectral budget falls back (to
         * tape): then TAPE in the accent */
        ec_track_status_t st;
        ec_dsp_track_status(a->dsp, a->track, &st);
        if (!st.mode_implemented)
            T(EC_C_ACC, "TAPE", W - 2, 13, EC_AL_RIGHT);
        else {
            ec_param_format(EC_P_MODE, v[EC_P_MODE], 3, r, sizeof r);
            T(EC_C_L8, r, W - 2, 13, EC_AL_RIGHT);
        }
    } else {
        snprintf(r, sizeof r, "%d", (int)floorf(sl->bpm + 0.5f));
        T(EC_C_L8, r, W - 2, 13, EC_AL_RIGHT);
    }
    sample_area(a, 22, 58, pg->pic == EC_PIC_WAVE_RANGE);
    if (mach != EC_MACH_NEIGHBOR && mach != EC_MACH_INPUT) {   /* E3, E6 */
        if (a == &ec_app && ec_lp_step_layer())    /* 0.6.14 (owner): STEP layer yellow, loop keys red, LIVE blue;
                                                    * the selected steps white on every layer */
            loop_steps(a, pg->pic == EC_PIC_WAVE_RANGE, EC_C_SPECIAL);
        else if (a == &ec_app)
            loop_steps(a, pg->pic == EC_PIC_WAVE_RANGE, ec_lp_first() >= 0 ? EC_C_ALERT : EC_C_SRC);
        else
            key_ruler(a, 66);                      /* (the mocks: the loop-key ruler) */
    }
    if (a == &ec_app && ec_lp_b11_held() && !A_IS_BUS(a))   /* 0.6.14 (owner): B11 held: the pages, LEN and RATE */
        ec_scr_pages_panel(a);
    else
        cells(a, pg);
}

/* ------------------------------------------------------------------------------------------- generic pages --- */
/* the actual (resolved) values, with a held step's locks: what the filter / envelope pictures draw */
static void shown_values(const ec_app_t *a, float *v)
{
    const int hs = ec_app_held_step(a);
    int i;
    for (i = 0; i < EC_NP; i++)
        v[i] = ec_seq_value(&a->seq, a->track, i);
    if (hs >= 0) {
        const ec_step_t *s = &a->proj.pat[a->proj.current].tr[a->track].steps[hs];
        for (i = 0; i < s->nlock; i++)
            if (s->lock_id[i] < EC_NP)
                v[s->lock_id[i]] = s->lock_val[i];
    }
}

static void filter_bars(const ec_app_t *a, int top, int bot)
{
    float v[EC_NP];
    const int m = iv(tv(a), EC_P_FILTER_MACHINE);
    shown_values(a, v);
    ec_kit_filter_bars(top, bot, v[EC_P_FILTER_CUTOFF], v[EC_P_FILTER_RES], iv(v, EC_P_FILTER_TYPE),
                       m == EC_FILT_MORPHING || m == EC_FILT_FORMANT ? v[EC_P_FILTER_MORPH] / 128.0f : -1.0f,
                       (v[EC_P_FENV_DEPTH] - 64.0f) / 64.0f, a->seq.playing);
}

static void env_bars(const ec_app_t *a, const ec_page_t *pg, int pic, int top, int bot)
{
    float v[EC_NP];
    const unsigned hot = hot_stages(a, pg);
    shown_values(a, v);
    if (pic == EC_PIC_FENV)
        ec_kit_env_bars(top, bot, iv(v, EC_P_FENV_MODE), v[EC_P_FENV_ATTACK], v[EC_P_FENV_DECAY], v[EC_P_FENV_SUSTAIN],
                        v[EC_P_FENV_HOLD], v[EC_P_FENV_RELEASE], hot);
    else if (pic == EC_PIC_AENV)
        ec_kit_env_bars(top, bot, iv(v, EC_P_ENV_MODE), v[EC_P_ENV_ATTACK], v[EC_P_ENV_DECAY], v[EC_P_ENV_SUSTAIN],
                        v[EC_P_ENV_HOLD], v[EC_P_ENV_RELEASE], hot);
    else
        ec_kit_env_bars(top, bot, EC_ENV_ADSR, v[EC_P_MENV_ATTACK], v[EC_P_MENV_DECAY], v[EC_P_MENV_SUSTAIN], 0,
                        v[EC_P_MENV_RELEASE], hot);
}

static void generic_screen(const ec_app_t *a, const ec_page_t *pg)
{
    const float *v = tv(a);
    char t[32], r[16], left[40];
    title_of(a, pg, t, sizeof t);
    right_info(a, r, sizeof r);
    if (a->settings)
        header(a, t, 1, r);
    else                                           /* 0.6.2: P1, P2 .. (lanes, INSERT's knob pages), else the page */
        ec_scr_header_page(a, t, a->page[a->cat] + 1, r);
    if (pg->pic == EC_PIC_MENV) {                 /* MOD ENV: the norns bars, the source's routes over them */
        ec_scr_e4(a, pg);
        env_bars(a, pg, pg->pic, 20, 75);
        cells(a, pg);
        return;
    }
    if ((pg->pic >= EC_PIC_FX && pg->pic <= EC_PIC_MACRO) || pg->pic == EC_PIC_MATRIX) {   /* E4: FX / SENDS / MOD visuals */
        ec_scr_e4(a, pg);
        cells(a, pg);
        return;
    }
    left[0] = 0;
    r[0] = 0;
    switch (a->cat) {
    case EC_CAT_TRIG: {
        const int hs = ec_app_held_step(a);
        if (hs >= 0) {
            const int nl = a->proj.pat[a->proj.current].tr[a->track].steps[hs].nlock;
            snprintf(left, sizeof left, "STEP %d  %d LOCK%s", hs + 1, nl, nl == 1 ? "" : "S");
        }
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
    if (pg->pic == EC_PIC_FILTER) {
        static const char *const FT[5] = {"", "LP", "HP", "BP", "NOTCH"};
        const int m = iv(v, EC_P_FILTER_MACHINE);
        snprintf(r, sizeof r, "%s", m == EC_FILT_MORPHING || m == EC_FILT_FORMANT ? "MORPH"
                                                                                    : FT[clampi(iv(v, EC_P_FILTER_TYPE), 1, 4)]);
    } else if (pg->pic == EC_PIC_FENV)
        snprintf(r, sizeof r, "%s", iv(v, EC_P_FENV_MODE) == EC_ENV_ADSR ? "ADSR" : "AHR");
    else
        snprintf(r, sizeof r, "%d", (int)floorf(a->proj.bpm + 0.5f));
    T(EC_C_L15, left, 2, 13, EC_AL_LEFT);
    T(EC_C_L8, r, W - 2, 13, EC_AL_RIGHT);
    if (pg->pic == EC_PIC_FILTER)
        filter_bars(a, 20, 75);
    else if (pg->pic == EC_PIC_FENV || pg->pic == EC_PIC_AENV)
        env_bars(a, pg, pg->pic, 20, 75);
    else
        step_strip(a, 26, 62, 70);
    if (a == &ec_app && ec_lp_b11_held() && !A_IS_BUS(a))   /* 0.6.14: B11 held: the pages panel (TRIG) */
        ec_scr_pages_panel(a);
    else
        cells(a, pg);
}

/* ------------------------------------------------------------------------------------------- settings --- */
static void settings_screen(const ec_app_t *a, const ec_page_t *pg)
{
    static char lab[EC_PAGE_ITEMS][24], val[EC_PAGE_ITEMS][24];
    ec_kit_row_t rows[EC_PAGE_ITEMS];
    int idx[EC_PAGE_ITEMS];
    char t[32], r[16];
    int i, n;
    title_of(a, pg, t, sizeof t);
    right_info(a, r, sizeof r);
    header(a, t, 1, r);
    n = ec_app_set_rows(pg, idx);                   /* 0.6.1: a list menu, the blanks left out */
    memset(rows, 0, sizeof rows);
    for (i = 0; i < n; i++) {
        const ec_item_t *it = &pg->it[idx[i]];
        const int blocked = (it->flags & EC_IF_STOPPED) && a->seq.playing;
        ec_app_item_format(a, it, ec_app_item_value(a, it, 0), val[i], sizeof val[i]);
        snprintf(lab[i], sizeof lab[i], "%s", ec_app_item_label(it));
        rows[i].label = lab[i];
        rows[i].value = val[i];
        rows[i].dim = (uint8_t)blocked;
        rows[i].note = blocked ? "STOP" : 0;
    }
    ec_kit_list(rows, n, a->ux.set_row, 12, H, 0);
}

/* ------------------------------------------------------------------------------------------- MIX --- */
/* the mixer (the mock's MIX): mute / solo letters, the meters y 19..52 with the volume marks, each strip's pan under
 * its meter (0.6.1, docs/UX-V2.md §10 "MIX shows each track's pan"), the track names, and the cells under them.
 * 0.6.1: nothing 1 grid unit (the meter moves in 2-unit steps, the volume rail is 2 wide, 2-unit gaps) */

/* a track's pan now, -1..1: the resolved value (a playing step's lock, the crossfader) plus the modulation's pan */
float ec_scr_mix_pan_now(const ec_app_t *a, int t)
{
    ec_track_status_t st;
    float p = ec_xf_bipolar(ec_seq_value(&a->seq, t, EC_P_PAN));
    ec_dsp_track_status(a->dsp, t, &st);
    p += st.mod[EC_MOD_PAN] < -1 ? -1 : st.mod[EC_MOD_PAN] > 1 ? 1 : st.mod[EC_MOD_PAN];
    return p < -1 ? -1 : p > 1 ? 1 : p;
}

/* the pan track under strip x (x+1..x+10, y 55..60): the track (L3, 2 high) with its centre in L8 (2 units, in the
 * track: nothing sticks out 1 unit), the marker (2 wide, 6 high) 4 steps of 1 unit either side of the centre */
int ec_scr_mix_pan_x(int x, float pan) { return x + 5 + (int)floorf(pan * 4.0f + 0.5f); }

static void mix_pan(int x, float pan, int col)
{
    R(EC_C_L3, x + 1, 57, 10, 2);
    R(EC_C_L8, x + 5, 57, 2, 2);
    if (col >= 0)
        R(col, ec_scr_mix_pan_x(x, pan), 55, 2, 6);
}

static void mix_screen(const ec_app_t *a, const ec_page_t *pg)
{
    static const char *const N[9] = {"T1", "T2", "T3", "T4", "T5", "T6", "S1", "S2", "M"};
    const int top = 19, mh = 34;
    const int sel = a->bus ? EC_NTRACK - 1 + a->bus : a->track;
    char t[32], r[16];
    int i;
    snprintf(t, sizeof t, "%s", ec_cat_tag(EC_CAT_MASTER));   /* 0.6.0: MIX, the track's rate (the mock) */
    if (a->bus)
        right_info(a, r, sizeof r);
    else
        snprintf(r, sizeof r, "%s", ec_rate_label(iv(tv(a), EC_P_RATE)));
    header(a, t, 0, r);
    for (i = 0; i < 9; i++) {
        const int x = 2 + i * 13, cx = x + 6, x0 = x + 1;
        const int muted = i < EC_NTRACK && a->proj.pat[a->proj.current].tr[i].v[EC_P_MUTE] > 0.5f;
        const int solo = i < EC_NTRACK && (a->ux.solo >> i & 1u);
        float lev = a->meter[i], vol;
        int h;
        if (i < EC_NTRACK)
            vol = a->proj.pat[a->proj.current].tr[i].v[EC_P_AMP] / 127.0f;
        else if (i == 8)
            vol = (float)a->master / 1023.0f;
        else
            vol = 0;
        lev = lev > 1 ? 1 : lev;
        vol = vol < 0 ? 0 : vol > 1 ? 1 : vol;
        if (muted || solo)
            T(muted ? EC_C_ACC : EC_C_L15, muted ? "M" : "S", cx, 12, EC_AL_CENTER);
        R(EC_C_L3, x0, top, 4, mh);
        h = 2 * (int)(mh * lev * 0.5f + 0.5f);               /* 2-unit steps */
        R(muted ? EC_C_L5 : EC_C_L12, x0, top + mh - h, 4, h);
        if (lev > 0.78f)
            R(EC_C_ACC, x0, top + mh - h, 4, 2);
        if (i != 6 && i != 7) {
            R(EC_C_L5, x + 8, top, 2, mh);
            R(EC_C_L15, x + 7, top + (int)((mh - 2) * (1 - vol) + 0.5f), 4, 2);
        }
        /* the pan: the tracks' live pan; S1 / S2 (stereo returns, no pan param) rest at the centre; MST has none */
        if (i < EC_NTRACK)
            mix_pan(x, ec_scr_mix_pan_now(a, i), i == sel ? EC_C_ACC : muted ? EC_C_L5 : EC_C_L12);
        else if (i < 8)
            mix_pan(x, 0, i == sel ? EC_C_ACC : EC_C_L5);
        if (i == sel) {
            R(EC_C_L15, x + 1, 63, 11, 9);
            T(EC_C_BG, N[i], cx, 65, EC_AL_CENTER);
        } else
            T(muted ? EC_C_L5 : EC_C_L8, N[i], cx, 65, EC_AL_CENTER);
    }
    cells(a, pg);
}

/* ------------------------------------------------------------------------------------------- PAT --- */
/* the pattern grid (the mock's PATTERNS): the bank's 16, the current one filled, the queued one blinking in the
 * accent, used ones a dim block; the global cycle under them */
static void pat_screen(const ec_app_t *a, const ec_page_t *pg)
{
    const int bank = a->proj.current / 16;
    char t[32], r[16];
    int i;
    snprintf(t, sizeof t, "%s/%s", ec_cat_tag(EC_CAT_PAT), pg->title);
    right_info(a, r, sizeof r);
    header(a, t, 1, r);
    for (i = 0; i < 16 && bank * 16 + i < EC_NPAT; i++) {
        const int p = bank * 16 + i, col = i % 4, row = i / 4, x = 2 + col * 29, y = 11 + row * 15, w = 27, h = 14;
        char n[8];
        snprintf(n, sizeof n, "%c%d", 'A' + bank, i + 1);
        if (p == a->proj.current) {
            R(EC_C_L15, x, y, w, h);
            T2(EC_C_BG, n, x + w / 2, y + 2, EC_AL_CENTER);
        } else if (p == a->seq.pending) {
            R((a->ms / 120) & 1u ? EC_C_ACC : EC_C_L5, x, y, w, h);
            T2(EC_C_BG, n, x + w / 2, y + 2, EC_AL_CENTER);
        } else {
            if (a->proj.pat[p].used)                /* 0.6.1: a dim block (a 2-unit frame would cut the name) */
                R(EC_C_L3, x, y, w, h);
            T2(a->proj.pat[p].used ? EC_C_L12 : EC_C_L5, n, x + w / 2, y + 2, EC_AL_CENTER);
        }
    }
    /* the global cycle: a pending switch engages at its end */
    {
        const int gl = clampi(a->proj.glen, 1, 256);
        const int pos = a->seq.playing || a->seq.paused ? a->seq.global_pos : 0;
        R(EC_C_L3, 2, 71, W - 4, 2);
        R(a->seq.pending >= 0 ? EC_C_ACC : EC_C_L8, 2, 71, (W - 4) * (pos + 1) / gl, 2);
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
    if (a->settings) {
        settings_screen(a, &pg);
        return;
    }
    switch (pg.pic) {                              /* the page's picture (the layout) */
    case EC_PIC_MIX: mix_screen(a, &pg); break;
    case EC_PIC_WAVE:
    case EC_PIC_WAVE_MACHINE:
    case EC_PIC_WAVE_WARP:
    case EC_PIC_WAVE_RANGE:
        if (a->bus)
            generic_screen(a, &pg);
        else
            src_screen(a, &pg);
        break;
    case EC_PIC_RAZOR: ec_scr_razor(a, &pg); break;   /* E3 */
    case EC_PIC_FILE: ec_scr_file_browser(a, &pg); break;      /* 0.6.0 */
    case EC_PIC_EDITOR: ec_scr_file_editor(a, &pg); break;
    case EC_PIC_POOL: ec_scr_file_pool(a, &pg); break;   /* 0.6.1 */
    case EC_PIC_PATTERN: pat_screen(a, &pg); break;
    default: generic_screen(a, &pg); break;
    }
}

/* E3: the shared pieces for src/ui/ec_scr_e3.c */
void ec_scr_header(const ec_app_t *a, const char *title, int page, const char *right) { header(a, title, page, right); }
void ec_scr_cells(const ec_app_t *a, const ec_page_t *pg) { cells(a, pg); }
void ec_screens_draw(const ec_app_t *a)
{
    ec_gfx_fill(EC_C_BG);
    if (a->pn.intro) {                            /* 0.6.0: the intro (any key or knob skips it) */
        ec_scr_intro(a->pn.intro_frame);
        ec_gfx_present();
        return;
    }
    if (a == &ec_app && ec_hm_shown()) {           /* 0.6.1: a hold menu covers everything while its key is held */
        hold_menu(a);
        ec_gfx_present();
        return;
    }
    if (a->e5.naming)                             /* E5: naming */
        ec_scr_e5_full(a);
    else if (!ec_scr_ux_full(a) && !ec_scr_modes_full(a) && !ec_scr_e5_full(a))   /* F3; 0.6.0: the mode screens */
        draw_main(a);
    ec_scr_e5_overlay(a);                         /* E5: the pattern's name (0.6.1: no toasts) */
    ec_scr_ux_overlay(a);                         /* 0.3.0: the takeover, SHIFT's screen */
    ec_gfx_present();                             /* the canvas through the theme to the LCD at 2x */
}
