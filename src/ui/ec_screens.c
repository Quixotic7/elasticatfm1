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
/* the header: the target, the title (or a message), the right-hand info and, in the box at the right: a pop-up's
 * arrows (back / OK), the count of held steps (on LOCK: the knobs lock), else the page */
static void header(const ec_app_t *a, const char *title, int page, const char *right)
{
    char pg[8], buf[40];
    const int popup = ec_app_popup(a);
    const int held = a->step_held && !a->bus && !popup ? __builtin_popcount(a->step_held) : 0;
    if (a->msg_ms)
        fit(buf, sizeof buf, a->msg, 17);
    else
        snprintf(buf, sizeof buf, "%s", title);
    if (page > 0)
        snprintf(pg, sizeof pg, "%d", page);
    else
        pg[0] = 0;
    ec_kit_header(target_name(a), buf, pg, right, held, popup);
    if (a->msg_ms && a->msg_alert) {               /* 0.6.0: an alert (STOP FIRST, FULL): the title bar in ALERT */
        R(EC_C_ALERT, 14, 0, W - 14 - (popup ? 21 : 11), 10);
        T(EC_C_BG, buf, 17, 3, EC_AL_LEFT);
    }
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
    snprintf(out, cap, "%s/%s", ec_cat_tag(a->cat), a->settings ? "SETUP" : pg->title);   /* 0.6.0: the tag */
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
            /* 0.5.0: a source key held: the cell shows the source > param and the slot's DEPTH from the centre */
            const int k = ec_mk_hot_slot(it->id), hs = ec_app_held_step(a);
            const ec_ptrack_t *tr = &a->proj.pat[a->proj.current].tr[a->track];
            float d = tr->v[ec_mtx_depth_id(k)];
            if (hs >= 0 && ec_step_lock_get(&tr->steps[hs], ec_mtx_depth_id(k), &d))
                c[i].flags |= EC_KC_LOCK;
            snprintf(lab[i], sizeof lab[i], "%s>%s", ec_mtx_src_name(ec_mtx_src(tr->v, k)), ec_app_item_label(it));
            ec_param_format(ec_mtx_depth_id(k), d, 3, val[i], sizeof val[i]);
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
        else if (it->kind == EC_IT_PARAM && !a->bus && !a->settings && (EC_PARAMS[it->id].flags & EC_PF_LOCKABLE) &&
                 ec_ux_locked_somewhere(it->id))
            c[i].flags |= EC_KC_SOME;
        if ((it->flags & EC_IF_STOPPED) && a->seq.playing)
            c[i].flags |= EC_KC_DIM;
        if (it->kind == EC_IT_PARAM && !a->settings && (marks = ec_mk_marks(it->id)) != 0) {
            mark_text(marks, mk[i], sizeof mk[i]);
            c[i].mark = mk[i];
        }
        if (it->kind == EC_IT_PARAM && EC_PARAMS[it->id].fmt == EC_PF_MDEP) {   /* 0.5.0: DEPTH from the centre */
            c[i].frac = v / 100.0f;
            c[i].flags |= EC_KC_DEPTH;
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

/* the stages (EC_KIT_ST_*) the active lane's knobs edit, for the envelope bars */
static unsigned hot_stages(const ec_app_t *a, const ec_page_t *pg)
{
    const int lane = ec_app_lane(a, pg);
    unsigned hot = 0;
    int i;
    for (i = lane * 4; i < lane * 4 + 4 && i < pg->n; i++) {
        if (pg->it[i].kind != EC_IT_PARAM)
            continue;
        switch (pg->it[i].id) {
        case EC_P_FENV_ATTACK:
        case EC_P_ENV_ATTACK:
        case EC_P_MENV_ATTACK: hot |= EC_KIT_ST_A; break;
        case EC_P_FENV_DECAY:
        case EC_P_ENV_DECAY:
        case EC_P_MENV_DECAY: hot |= EC_KIT_ST_D; break;
        case EC_P_FENV_SUSTAIN:
        case EC_P_ENV_SUSTAIN:
        case EC_P_MENV_SUSTAIN: hot |= EC_KIT_ST_S; break;
        case EC_P_FENV_HOLD:
        case EC_P_ENV_HOLD: hot |= EC_KIT_ST_H; break;
        case EC_P_FENV_RELEASE:
        case EC_P_ENV_RELEASE:
        case EC_P_MENV_RELEASE: hot |= EC_KIT_ST_R; break;
        default: break;
        }
    }
    return hot;
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

/* the live grains (GRAIN warps), when the engine publishes them: the frame, the waveform dim, the grains. 0 = none */
static int grain_view(const ec_app_t *a, const ec_ss_sample_t *smp, int top, int bot)
{
#ifndef EC_KIT_LOCAL_GRAIN_VIEW
    ec_grain_view_t g[EC_GRAIN_VIEW_MAX];
    const int n = ec_dsp_grains(a->dsp, a->track, g);
    if (n <= 0)
        return 0;
    ec_kit_grain_frame(2, top, W - 4, bot - top, -1);
    ec_kit_wave(smp->overview, EC_SS_OVERVIEW, 4, W - 4, top + 3, bot - 3, EC_C_L3, EC_C_L3, 0, W);
    ec_kit_grains(g, n, 4, W - 4, top + 3);
    return 1;
#else
    (void)a, (void)smp, (void)top, (void)bot;
    return 0;
#endif
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
        ec_gfx_dots_h(2, W - 2, (top + bot) / 2, 2, EC_C_L3);
        if (rt->state == EC_SL_MISSING || rt->state == EC_SL_ERROR) {
            T2(EC_C_ALERT, rt->state == EC_SL_MISSING ? "MISSING" : "ERROR", W / 2, top + 4, EC_AL_CENTER);
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
        int rs, re, k;
        ec_dsp_track_status(a->dsp, a->track, &st);
        if (is_slice(v)) {
            /* the slices divide the trim x range window */
            const ec_slot_t *sl = &a->proj.slot[s];
            const float span = sl->trim_end - sl->trim_start;
            const float lo = sl->trim_start + span * v[EC_P_RANGE_START] / 128.0f;
            const float hi = sl->trim_start + span * v[EC_P_RANGE_END] / 128.0f;
            const int cnt = clampi(iv(v, EC_P_SLICE_COUNT), 1, 32);
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
            ec_kit_wave(smp->overview, EC_SS_OVERVIEW, x0, x1, top, bot, EC_C_L15, EC_C_L5, rs, re);
            for (k = 0; k < cnt; k++) {
                int sx, ex;
                slice_cols(a, k, cnt, lo, hi, x0, cols, &sx, &ex);
                if (k > 0 || ec_razor_is_razor(iv(v, EC_P_MACHINE)))
                    ec_gfx_dots_v(sx, top - 2, bot + 3, 2, EC_C_L8);
            }
            for (k = 0; k < st.voices && k < EC_MAX_VOICES_TRACK; k++)
                ec_kit_playhead(x0 + (int)(st.voice_pos[k] * (float)cols), top - 1, bot + 1);
            return;
        }
        if (grain_view(a, smp, top, bot))
            return;
        rs = x0 + (int)(st.region_start * cols + 0.5f);
        re = x0 + (int)(st.region_end * cols + 0.5f);
        if (re <= rs)
            re = rs + 1;
        ec_kit_wave(smp->overview, EC_SS_OVERVIEW, x0, x1, top, bot, EC_C_L15, EC_C_L5, rs, re);
        /* the region brackets */
        R(EC_C_L12, rs - 1, top - 3, 1, bot - top + 6);
        R(EC_C_L12, rs - 1, top - 3, 4, 2);
        R(EC_C_L12, re, top - 3, 1, bot - top + 6);
        R(EC_C_L12, re - 3, top - 3, 4, 2);
        if (st.reader_on)
            ec_kit_playhead(x0 + (int)(st.pos_sample * (float)cols), top - 1, bot + 1);
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
    for (i = 0; i < pages && pages > 1 && pages_y > 0; i++)
        R(i == a->step_page ? EC_C_L15 : EC_C_L5, W / 2 - pages * 2 + i * 4, pages_y, 3, 1);
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
    header(a, t, a->page[a->cat] + 1, r);
    switch (pg->pic) {
    case EC_PIC_WAVE: fit(left, sizeof left, file_name(a), 20); break;
    case EC_PIC_WAVE_MACHINE: snprintf(left, sizeof left, "%s", ec_machine_name(mach)); break;
    case EC_PIC_WAVE_WARP: snprintf(left, sizeof left, "%s", ec_warp_name(iv(v, EC_P_MODE))); break;
    default: snprintf(left, sizeof left, "RANGE"); break;
    }
    if (mach == EC_MACH_NEIGHBOR)   /* E3 */
        snprintf(left, sizeof left, "FROM T%d", a->track);
    if (mach == EC_MACH_INPUT)      /* E6 */
        snprintf(left, sizeof left, "INPUT");
    T(EC_C_L15, left, 2, 13, EC_AL_LEFT);
    if (pg->pic == EC_PIC_WAVE_WARP) {   /* E3: only the spectral budget falls back (to tape) */
        ec_track_status_t st;
        ec_dsp_track_status(a->dsp, a->track, &st);
        if (!st.mode_implemented)
            T(EC_C_ACC, "TAPE", 2 + TW(left) + 4, 13, EC_AL_LEFT);
    }
    snprintf(r, sizeof r, "%d", (int)floorf(sl->bpm + 0.5f));
    T(EC_C_L8, r, W - 2, 13, EC_AL_RIGHT);
    sample_area(a, 22, 58);
    if (mach != EC_MACH_NEIGHBOR && mach != EC_MACH_INPUT) {   /* E3, E6 */
        key_ruler(a, 66);                          /* 0.6.0: LOOP ENGINE's white keys play the loop */
    }
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
    header(a, t, a->settings ? 1 : a->page[a->cat] + 1, r);
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
    cells(a, pg);
}

/* ------------------------------------------------------------------------------------------- settings --- */
static void settings_screen(const ec_app_t *a, const ec_page_t *pg)
{
    static char lab[EC_PAGE_ITEMS][24], val[EC_PAGE_ITEMS][24];
    ec_kit_row_t rows[EC_PAGE_ITEMS];
    char t[32], r[16];
    int i;
    title_of(a, pg, t, sizeof t);
    right_info(a, r, sizeof r);
    header(a, t, 1, r);
    for (i = 0; i < pg->n && i < EC_PAGE_ITEMS; i++) {
        const ec_item_t *it = &pg->it[i];
        const int blocked = (it->flags & EC_IF_STOPPED) && a->seq.playing;
        ec_app_item_format(a, it, ec_app_item_value(a, it, 0), val[i], sizeof val[i]);
        snprintf(lab[i], sizeof lab[i], "%s", ec_app_item_label(it));
        rows[i].label = lab[i];
        rows[i].value = val[i];
        rows[i].dim = (uint8_t)blocked;
        rows[i].note = blocked ? "STOP" : 0;
    }
    ec_kit_settings(rows, i, ec_app_lane(a, pg), -1);
}

/* ------------------------------------------------------------------------------------------- MIX --- */
/* the mixer (the mock's MIX): mute / solo letters, the meters y 19..61 with the volume marks, the track names, and
 * the cells under them */
static void mix_screen(const ec_app_t *a, const ec_page_t *pg)
{
    static const char *const N[9] = {"T1", "T2", "T3", "T4", "T5", "T6", "S1", "S2", "M"};
    const int top = 19, mh = 42;
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
        const int x = 2 + i * 13, cx = x + 6, x0 = cx - 5;
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
        if (muted || solo)
            T(muted ? EC_C_ACC : EC_C_L15, muted ? "M" : "S", cx, 12, EC_AL_CENTER);
        R(EC_C_L3, x0, top, 4, mh);
        h = (int)(mh * lev + 0.5f);
        R(muted ? EC_C_L5 : EC_C_L12, x0, top + mh - h, 4, h);
        if (lev > 0.78f)
            R(EC_C_ACC, x0, top + mh - h, 4, 2);
        R(EC_C_L5, cx + 1, top, 1, mh);
        if (i != 6 && i != 7)
            R(EC_C_L15, cx, top + (int)(mh * (1 - vol) + 0.5f) - 1, 4, 3);
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
 * accent, used ones framed; the global cycle under them */
static void pat_screen(const ec_app_t *a, const ec_page_t *pg)
{
    const int bank = a->proj.current / 16;
    char t[32], r[16];
    int i;
    snprintf(t, sizeof t, "%s/%s", ec_cat_tag(EC_CAT_PAT), pg->title);
    right_info(a, r, sizeof r);
    header(a, t, 1, r);
    for (i = 0; i < 16 && bank * 16 + i < EC_NPAT; i++) {
        const int p = bank * 16 + i, col = i % 4, row = i / 4, x = 2 + col * 29, y = 12 + row * 14, w = 27, h = 12;
        char n[8];
        snprintf(n, sizeof n, "%c%d", 'A' + bank, i + 1);
        if (p == a->proj.current) {
            R(EC_C_L15, x, y, w, h);
            T2(EC_C_BG, n, x + w / 2, y + 1, EC_AL_CENTER);
        } else if (p == a->seq.pending) {
            R((a->ms / 120) & 1u ? EC_C_ACC : EC_C_L5, x, y, w, h);
            T2(EC_C_BG, n, x + w / 2, y + 1, EC_AL_CENTER);
        } else {
            ec_gfx_frame(x, y, w, h, a->proj.pat[p].used ? EC_C_L8 : EC_C_L3);
            T2(a->proj.pat[p].used ? EC_C_L12 : EC_C_L5, n, x + w / 2, y + 1, EC_AL_CENTER);
        }
    }
    /* the global cycle: a pending switch engages at its end */
    {
        const int gl = clampi(a->proj.glen, 1, 256);
        const int pos = a->seq.playing || a->seq.paused ? a->seq.global_pos : 0;
        R(EC_C_L3, 2, 70, W - 4, 2);
        R(a->seq.pending >= 0 ? EC_C_ACC : EC_C_L8, 2, 70, (W - 4) * (pos + 1) / gl, 2);
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
    if (a->e5.naming)                             /* E5: naming */
        ec_scr_e5_full(a);
    else if (!ec_scr_ux_full(a) && !ec_scr_modes_full(a) && !ec_scr_e5_full(a))   /* F3; 0.6.0: the mode screens */
        draw_main(a);
    ec_scr_e5_overlay(a);                         /* E5: toasts */
    ec_scr_ux_overlay(a);                         /* 0.3.0: the takeover, SHIFT's screen */
    ec_scr_modes_overlay(a);                      /* 0.6.0: the mode pop-up */
    ec_gfx_present();                             /* the canvas through the theme to the LCD at 2x */
}
