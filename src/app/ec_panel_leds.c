/* SPDX-License-Identifier: GPL-3.0-only */
/* 0.6.0: the LEDs and the live panel labels (ec_app_labels, read by plat through fm1core_label) of every mode
 * (docs/UX-V2.md, the mockups' tools/mockups/modes.js): the lit mode button, the white keys by role (steps, tracks,
 * loop keys, notes, the fader, patterns), the black keys by role (pages, sources, specials, rates, slices, banks),
 * SHIFT's layer while it is on. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ec_app.h"
#include "ec_browse.h"
#include "ec_razor_ui.h"
#include "ec_scene.h"

#define A (&ec_app)
#define P (&ec_app.pn)
#define UX (&ec_app.ux)

static int clampi(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }
static int iv(const float *v, int id) { return (int)floorf(v[id] + 0.5f); }
static int blink(uint32_t period) { return (A->ms / period) & 1u; }
static int is_slice_m(int m) { return m >= EC_MACH_GRID_SLICE && m <= EC_MACH_RAZOR_POLY; }

static int mb_button(int mb)
{
    static const int B[EC_NMB] = {-1, EC_B_FX, EC_B_SEL, EC_B_ENV, EC_B_LFO, EC_B_EDIT, EC_B_HOME, EC_B_SAVE,
                                  EC_B_ARP, EC_B_SEQ};
    return mb > 0 && mb < EC_NMB ? B[mb] : -1;
}

/* the mode the lit button stands for (under a settings list: the mode it was opened over) */
static int shown_mode(void) { return A->settings && P->set_scr ? P->set_scr - 1 : ec_app_mode(); }
static int sel_target(void) { return A->bus ? EC_NTRACK - 1 + A->bus : A->track; }

/* ------------------------------------------------------------------------------------------- white keys --- */
/* 0.6.14: B11's STEP layer in any step mode: the selected steps blink, trigs mid, empty steps off */
static int leds_step_layer(void)
{
    const ec_ptrack_t *tr = ec_project_track(&A->proj, A->track);
    const int len = ec_seq_pattern_steps(&A->seq, A->track);
    int i;
    if (!ec_lp_step_layer())
        return 0;
    for (i = 0; i < EC_NLOWER; i++) {
        const int idx = A->step_page * 16 + i;
        ec_plat_led_lower(i, idx >= len ? EC_LED_OFF : ec_lp_is_sel(idx) ? (blink(200) ? EC_LED_LIT : EC_LED_DIM)
                             : tr->steps[idx].trig || tr->steps[idx].slices ? EC_LED_MID
                                                                           : EC_LED_OFF);
    }
    return 1;
}

static void leds_steps(void)
{
    const ec_ptrack_t *tr = ec_project_track(&A->proj, A->track);
    const float *v = tr->v;
    const int len = ec_seq_pattern_steps(&A->seq, A->track);
    const int play = ec_seq_step(&A->seq, A->track);
    int i;
    if (leds_step_layer())
        return;
    for (i = 0; i < EC_NLOWER; i++) {
        const int idx = A->step_page * 16 + i;
        const ec_step_t *s = &tr->steps[idx];
        uint8_t l = EC_LED_OFF;
        if (idx < len && !A->bus) {
            if (s->trig || s->slices)
                l = ec_step_is_ghost(s, v) ? EC_LED_DIM : s->nlock ? (blink(160) ? EC_LED_MID : EC_LED_DIM) : EC_LED_MID;
            if (idx == play && A->seq.playing)
                l = EC_LED_LIT;
            if (P->clip_state != EC_CLIP_EMPTY && P->clip_kind == EC_CK_STEP && idx == P->clip_from)
                l = blink(120) ? EC_LED_LIT : EC_LED_FAINT;
        }
        if (A->step_held >> i & 1u)
            l = EC_LED_LIT;
        ec_plat_led_lower(i, l);
    }
}

static uint8_t target_led(int i)
{
    if (i < EC_NTRACK && (UX->solo >> i & 1u))
        return blink(200) ? EC_LED_LIT : EC_LED_DIM;     /* soloed: blinking */
    if (i == sel_target())
        return EC_LED_LIT;
    if (i < EC_NTRACK && !ec_app_track_audible(A, i))
        return EC_LED_OFF;                               /* muted, or silenced by a solo */
    return A->meter[i] > 0.5f ? EC_LED_MID : A->meter[i] > 0.05f ? EC_LED_DIM : EC_LED_FAINT;
}

static void leds_targets(void)
{
    int i;
    for (i = 0; i < EC_NLOWER; i++)
        ec_plat_led_lower(i, i < 9 ? target_led(i) : EC_LED_OFF);
}

static void leds_loop(void)
{
    const float *v = ec_app_params(A);
    const int m = iv(v, EC_P_MACHINE);
    const int hi = P->zoom_half ? 16 : 0;
    ec_track_status_t st;
    int i;
    if (A->bus) {
        for (i = 0; i < EC_NLOWER; i++)
            ec_plat_led_lower(i, EC_LED_OFF);
        return;
    }
    if (ec_lp_step_layer()) {                      /* 0.6.9: the STEP layer: the selected steps lit, trigs mid */
        const ec_ptrack_t *tr = ec_project_track(&A->proj, A->track);
        const int len = ec_seq_pattern_steps(&A->seq, A->track);
        for (i = 0; i < EC_NLOWER; i++) {
            const int idx = A->step_page * 16 + i;
            /* 0.6.12 (owner): no trig = off, a trig mid, the selected steps blink */
            ec_plat_led_lower(i, idx >= len ? EC_LED_OFF : ec_lp_is_sel(idx) ? (blink(200) ? EC_LED_LIT : EC_LED_DIM)
                                 : tr->steps[idx].trig || tr->steps[idx].slices ? EC_LED_MID
                                                                               : EC_LED_OFF);
        }
        return;
    }
    ec_dsp_track_status(A->dsp, A->track, &st);
    if (ec_lp_on_range()) {                        /* 0.6.21 (owner): the RANGE page: the range over the 16 keys */
        const float r0 = v[EC_P_RANGE_START], r1 = v[EC_P_RANGE_END];
        for (i = 0; i < EC_NLOWER; i++) {
            const float ks = (float)i * 8.0f, ke = ks + 8.0f;
            uint8_t l = ke > r0 + 0.001f && ks < r1 - 0.001f ? EC_LED_LIT : EC_LED_OFF;
            if (ec_lp_keys() >> i & 1u)
                l = EC_LED_LIT;
            if (ec_lp_is_sel(A->step_page * 16 + i))
                l = blink(200) ? EC_LED_LIT : EC_LED_OFF;
            ec_plat_led_lower(i, l);
        }
        return;
    }
    if (is_slice_m(m)) {
        const int cnt = clampi(iv(v, EC_P_SLICE_COUNT), 1, 32);
        const uint32_t mute = ec_e5_slices(&A->proj, A->track)->mute;
        uint32_t sounding = A->seq.tr[A->track].pads;
        int j;
        for (j = 0; j < st.voices && j < EC_MAX_VOICES_TRACK; j++)
            if (st.voice_slice[j] >= 1 && st.voice_slice[j] <= 32)
                sounding |= 1u << (st.voice_slice[j] - 1);
        for (i = 0; i < EC_NLOWER; i++) {
            const int s = i + hi;
            ec_plat_led_lower(i, s >= cnt ? EC_LED_OFF : (sounding >> s & 1u) ? EC_LED_LIT
                                 : (mute >> s & 1u)                         ? EC_LED_FAINT
                                                                            : EC_LED_DIM);
        }
        return;
    }
    {
        const int div = clampi((iv(v, EC_P_LOOP_DIVISION) + 1) / 2 * 2, 2, 32);
        const float units = 128.0f / (float)div;
        float ls = ec_seq_value(&A->seq, A->track, EC_P_LOOP_START);
        float le = ec_seq_value(&A->seq, A->track, EC_P_LOOP_END);
        const int tgt = ec_app_target_step(A);
        int ph = -1;
        if (tgt >= 0 && !st.keys_held) {           /* 0.6.14 (owner): a selected step: its region (its lock) */
            const ec_step_t *sp = &ec_project_track(&A->proj, A->track)->steps[tgt];
            float x;
            ls = ec_step_lock_get(sp, EC_P_LOOP_START, &x) ? x : ec_project_track(&A->proj, A->track)->v[EC_P_LOOP_START];
            le = ec_step_lock_get(sp, EC_P_LOOP_END, &x) ? x : ec_project_track(&A->proj, A->track)->v[EC_P_LOOP_END];
        }
        {
            const ec_slot_t *sl = &A->proj.slot[ec_app_track_slot(A, A->track)];
            const float span = sl->trim_end - sl->trim_start;
            const float lo = sl->trim_start + span * v[EC_P_RANGE_START] / 128.0f;
            const float hi2 = sl->trim_start + span * v[EC_P_RANGE_END] / 128.0f;
            if (st.reader_on && hi2 > lo)          /* the playhead in track space -> its key */
                ph = (int)((st.pos_sample - lo) / (hi2 - lo) * 128.0f / units);
            if (st.keys_held && hi2 > lo) {        /* 0.6.21 (owner): keys held: the live span lights, as it plays */
                ls = (st.region_start - lo) / (hi2 - lo) * 128.0f;
                le = (st.region_end - lo) / (hi2 - lo) * 128.0f;
            }
        }
        for (i = 0; i < EC_NLOWER; i++) {
            const int key = i + hi, idx = A->step_page * 16 + i;
            const float ks = (float)key * units, ke = ks + units;
            uint8_t l = EC_LED_OFF;                /* 0.6.14 (owner): only the active range lights, fully */
            if (key < div && ke > ls + 0.001f && ks < le - 0.001f)
                l = EC_LED_LIT;
            if (key == ph && l == EC_LED_OFF)
                l = EC_LED_DIM;                    /* (the playhead outside the range: a trace) */
            if (st.keys_held >> key & 1u)
                l = EC_LED_LIT;
            if (ec_lp_is_sel(idx))                 /* the selected steps blink on their keys */
                l = blink(200) ? EC_LED_LIT : EC_LED_OFF;
            ec_plat_led_lower(i, l);
        }
    }
}

static void leds_keyboard(void)
{
    int i;
    if (leds_step_layer())
        return;
    for (i = 0; i < EC_NLOWER; i++) {
        const int k = ec_key_of_lower(i);
        const int c = ((ec_kb_semis(k) % 12) + 12) % 12 == 0;
        ec_plat_led_lower(i, c ? EC_LED_DIM : EC_LED_FAINT);
    }
}

static void leds_xfd(void)
{
    const float pos = A->proj.e5.sc.pos;
    int i;
    for (i = 0; i < 12; i++) {
        const float d = fabsf(pos - ec_xfd_key_value(i));
        uint8_t l = d < 0.05f ? EC_LED_MID : d < 0.1f ? EC_LED_DIM : EC_LED_FAINT;
        if (P->xf_key == i)
            l = EC_LED_LIT;
        ec_plat_led_lower(i, l);
    }
    for (i = 0; i < 4; i++)
        ec_plat_led_lower(12 + i, ec_as->glide == i ? EC_LED_LIT : EC_LED_DIM);
}

static void leds_patterns(void)
{
    int i;
    for (i = 0; i < EC_NLOWER; i++) {
        const int p = ec_pat_index(P->bank, i);
        ec_plat_led_lower(i, p == A->proj.current ? EC_LED_LIT
                             : p == A->seq.pending ? (blink(120) ? EC_LED_LIT : EC_LED_DIM)
                             : A->proj.pat[p].used ? EC_LED_DIM
                                                   : EC_LED_FAINT);
    }
}

static void leds_engines(void)
{
    const float *bv = ec_app_bus_params(A, EC_P_FX_MACHINE);
    const int cur = iv(bv ? bv : ec_app_params(A), EC_P_FX_MACHINE);
    const int n = (int)EC_PARAMS[EC_P_FX_MACHINE].max;
    int i;
    for (i = 0; i < EC_NLOWER; i++)
        ec_plat_led_lower(i, i + 1 > n ? EC_LED_OFF : i + 1 == cur ? EC_LED_LIT : EC_LED_DIM);
}

static void leds_white(void)
{
    const int mode = ec_app_mode();
    int i;
    if (ec_e5_leds_lower())                        /* naming, F3's pages */
        return;
    if (P->as_open || P->rs_open || ec_pn_picking()) {   /* (0.6.15: the picker: only the black keys light) */
        for (i = 0; i < EC_NLOWER; i++)
            ec_plat_led_lower(i, EC_LED_OFF);
        return;
    }
    if (mode == EC_MODE_SAMPLER && !A->fn) {       /* the input's VU: -48 .. 0 dB, the top four in full */
        const int lit = (int)floorf(ec_sampler_vu() * 16.0f + 0.5f);
        for (i = 0; i < EC_NLOWER; i++)
            ec_plat_led_lower(i, i < lit ? (i >= 12 ? EC_LED_LIT : EC_LED_MID) : EC_LED_OFF);
        return;
    }
    if (mode == EC_CAT_FILE && !A->fn && !A->settings && ec_file_page() != EC_PIC_RAZOR) {
        const ec_slot_t *sl = &A->proj.slot[ec_app_track_slot(A, A->track)];
        const float vw = 1.0f / ec_file_zoom();
        if (ec_file_page() != EC_PIC_EDITOR) {     /* 0.6.15: BROWSE / POOL: the preview's loop keys: the span lit, the
                                                    * playhead's key flashing in it; else dark */
            const ec_ss_sample_t *x = P->pv_state == EC_PV_READY ? ec_ss_ready(P->pv_handle) : 0;
            const int ph = x && x->frames && P->pv_play ? (int)(P->pv_pos / (double)x->frames * 16.0) : -1;
            for (i = 0; i < EC_NLOWER; i++) {
                const int in = (P->pv_keys >> i) & 1u || (P->pv_keys && i > __builtin_ctz(P->pv_keys) &&
                                                          i < 31 - __builtin_clz(P->pv_keys));
                uint8_t l = in ? EC_LED_MID : EC_LED_OFF;
                if (i == ph)
                    l = blink(80) ? EC_LED_LIT : (in ? EC_LED_MID : EC_LED_DIM);
                ec_plat_led_lower(i, l);
            }
            return;
        }
        for (i = 0; i < EC_NLOWER; i++) {          /* the editor: the keys at the trim lit */
            const float pos = P->ed_view + (float)i / 15.0f * vw;
            const int set = fabsf(pos - sl->trim_start) < vw / 30.0f || fabsf(pos - sl->trim_end) < vw / 30.0f;
            ec_plat_led_lower(i, set ? EC_LED_LIT : EC_LED_FAINT);
        }
        return;
    }
    if (A->fn && mode != EC_CAT_XFD) {
        leds_targets();
        return;
    }
    if (P->slot_key && mode == EC_CAT_FX) {
        leds_engines();
        return;
    }
    if (A->settings) {
        leds_steps();
        return;
    }
    switch (mode) {
    case EC_CAT_MASTER: leds_targets(); break;
    case EC_CAT_SRC: leds_loop(); break;
    case EC_MODE_KEYBOARD: leds_keyboard(); break;
    case EC_CAT_XFD: leds_xfd(); break;
    case EC_CAT_PAT: leds_patterns(); break;
    default:
        if (ec_rzui_active(A)) {
            leds_steps();
            ec_rzui_leds(A);
        } else
            leds_steps();
        break;
    }
}

/* ------------------------------------------------------------------------------------------- black keys --- */
static const char *pick_name(int mode, int page)
{
    if (mode == EC_CAT_FILE)
        return page >= 0 && ec_page_pic(EC_CAT_FILE, page, 0) == EC_PIC_EDITOR ? "EDITOR"
             : page >= 0 && ec_page_pic(EC_CAT_FILE, page, 0) == EC_PIC_RAZOR ? "SLICE" : "BROWSE";
    switch (mode) {
    case EC_CAT_SRC: return "LOOP";
    case EC_CAT_TRIG: return "TRIG";
    case EC_MODE_KEYBOARD: return "KEYS";
    case EC_CAT_MASTER: return "MIX";
    case EC_MODE_PAGES: return "PAGES";
    case EC_MODE_SAMPLER: return "SAMPLER";
    default: return ec_mode_name(mode);
    }
}

/* 0.6.15: a mode button held: only the black keys of its modes light (the shown one bright) */
static int leds_pick(void)
{
    int modes[8], pages[8], n, i;
    const int cur = ec_app_mode();
    if (!ec_pn_picking())
        return 0;
    n = ec_pn_pick_list(P->btn, modes, pages, 8);
    for (i = 0; i < EC_NBKEY; i++) {
        uint8_t l = EC_LED_OFF;
        if (i < n)
            l = modes[i] == cur && (pages[i] < 0 || pages[i] == A->page[EC_CAT_FILE]) ? EC_LED_LIT : EC_LED_DIM;
        ec_plat_led_upper(i, l);
    }
    return 1;
}

static void leds_black(void)
{
    const int mode = ec_app_mode();
    if (leds_pick())
        return;
    int i;
    uint8_t l[EC_NBKEY];
    memset(l, EC_LED_OFF, sizeof l);
    if (A->e5.naming || P->as_open || P->rs_open || UX->f3) {
    } else if (A->fn && (mode == EC_CAT_FILE || mode == EC_MODE_SAMPLER)) {
        l[10] = EC_LED_LIT;                                            /* purge */
    } else if (A->fn) {
        if (mode == EC_CAT_XFD) {
            l[0] = l[1] = EC_LED_LIT;
        } else {
            l[0] = blink(250) ? EC_LED_LIT : EC_LED_MID;              /* tap tempo */
            l[1] = A->fill_latched ? EC_LED_LIT : EC_LED_MID;          /* latch */
            l[3] = A->e5.memo_ok ? EC_LED_LIT : EC_LED_DIM;            /* recall (B4) */
            l[4] = EC_LED_LIT;                                         /* memorize (B5) */
            l[10] = ec_seq_fill_active(&A->seq) ? EC_LED_LIT : EC_LED_MID;   /* fill */
        }
    } else if (!A->settings) {
        switch (mode) {
        case EC_CAT_MASTER: {
            const int r = iv(ec_app_params(A), EC_P_RATE);
            for (i = 0; i < 8; i++)
                l[i] = !ec_mix_rate_index(i + 1) || A->bus ? EC_LED_OFF : ec_mix_rate_index(i + 1) == r ? EC_LED_LIT
                                                                                                       : EC_LED_DIM;
            l[8] = l[9] = A->bus ? EC_LED_OFF : EC_LED_DIM;
            l[10] = P->cat ? EC_LED_LIT : EC_LED_MID;
        } break;
        case EC_MODE_PAGES: {
            const int pages = ec_pages_count();
            const int play = ec_seq_step(&A->seq, A->track);
            for (i = 0; i < EC_NBKEY && i < pages; i++) {
                l[i] = i == A->step_page ? EC_LED_LIT
                       : ec_e5_page_in_loop(&A->proj, A->track, i, pages) ? EC_LED_MID : EC_LED_FAINT;
                if (A->seq.playing && play >= 0 && play / 16 == i && i != A->step_page && blink(125))
                    l[i] = EC_LED_LIT;
                if (P->clip_state != EC_CLIP_EMPTY && P->clip_kind == EC_CK_PAGE && i == P->clip_from)
                    l[i] = blink(120) ? EC_LED_LIT : EC_LED_FAINT;
            }
        } break;
        case EC_MODE_KEYBOARD:
            if (ec_kb_slice_track()) {
                const float *v = ec_app_params(A);
                const int cnt = clampi(iv(v, EC_P_SLICE_COUNT), 1, 32), sel = iv(v, EC_P_SLICE_INDEX);
                for (i = 0; i < 10; i++) {
                    const int s = P->kb_bank * 10 + i + 1;
                    l[i] = s > cnt ? EC_LED_OFF : s == sel ? EC_LED_LIT : EC_LED_DIM;
                }
                l[10] = EC_LED_MID;
            } else
                for (i = 0; i < EC_NBKEY; i++)
                    l[i] = EC_LED_FAINT;
            break;
        case EC_CAT_XFD: {
            const ec_scenes_t *sc = &A->proj.e5.sc;
            for (i = 0; i < 2; i++)
                l[i] = A->e5.anchor == i + 1 ? EC_LED_LIT : ec_scene_count(sc, i) ? EC_LED_MID : EC_LED_DIM;
            l[10] = P->xf_b11 ? EC_LED_LIT : EC_LED_DIM;
        } break;
        case EC_CAT_PAT:
            for (i = 0; i < EC_NBANK; i++)
                l[i] = i == P->bank ? EC_LED_LIT : EC_LED_DIM;
            break;
        case EC_CAT_FILE: {
            const int pic = ec_file_page();
            if (pic == EC_PIC_EDITOR) {            /* 0.6.15: B1 / B2 the pages (the shown one lit), B9 AUTO BPM */
                l[0] = A->ux.lane[EC_CAT_FILE] == 0 ? EC_LED_LIT : EC_LED_DIM;
                l[1] = A->ux.lane[EC_CAT_FILE] == 1 ? EC_LED_LIT : EC_LED_DIM;
                l[8] = l[9] = EC_LED_MID;
                l[10] = P->pv_play ? EC_LED_LIT : EC_LED_MID;
                break;
            }
            if (pic == EC_PIC_RAZOR) {             /* 0.6.15: B1..B3 the pages, B9 EVEN, B10 CHOP, B11 the preview */
                for (i = 0; i < 3; i++)
                    l[i] = A->ux.lane[EC_CAT_FILE] == i ? EC_LED_LIT : EC_LED_DIM;
                l[8] = l[9] = EC_LED_MID;
                l[10] = P->pv_play ? EC_LED_LIT : EC_LED_MID;
                break;
            }
            for (i = 1; i <= EC_NBKEY; i++)
                if (ec_file_key_page(i) >= 0)
                    l[i - 1] = ec_file_key_page(i) == A->page[EC_CAT_FILE] ? EC_LED_LIT : EC_LED_DIM;
            if (pic == EC_PIC_FILE) {              /* 0.6.15: B9 REMOVE (lit when the file is pooled), B10 ADD */
                l[0] = l[1] = EC_LED_DIM;
                l[8] = EC_LED_MID;
                l[9] = ec_file_next_free() >= 0 ? EC_LED_MID : EC_LED_FAINT;
                l[10] = P->pv_play ? EC_LED_LIT : EC_LED_MID;
            } else if (pic == EC_PIC_POOL) {       /* 0.6.1: ASSIGN; 0.6.15: B9 FREE, B11 the move (lit in hand) */
                l[8] = EC_LED_MID;
                l[9] = EC_LED_MID;
                l[10] = ec_file_pool_carrying() ? (blink(200) ? EC_LED_LIT : EC_LED_MID) : EC_LED_MID;
            }
        } break;
        case EC_MODE_SAMPLER: {
            static const int K[6] = {0, 1, 2, 3, 4, 4};
            for (i = 0; i < 5; i++)
                l[i] = i == K[clampi(A->rec.len, 0, 5)] ? EC_LED_LIT : EC_LED_DIM;
            l[10] = A->rec.state == EC_RS_ARMED ? (blink(250) ? EC_LED_LIT : EC_LED_DIM)
                    : A->rec.state != EC_RS_IDLE ? EC_LED_LIT : EC_LED_MID;
        } break;
        default:
            if (mode == EC_CAT_TRIG && A->step_held && !A->bus) {   /* 0.6.14: a step held: the note keys */
                float x;
                const int has = ec_app_held_lock(A, EC_P_PITCH, &x);
                const int at = has ? clampi((int)floorf(x + 0.5f), -5, 5) + 5 : -1;
                for (i = 0; i < EC_NBKEY; i++)
                    l[i] = i == at ? EC_LED_LIT : i == 5 ? EC_LED_MID : EC_LED_DIM;
            } else if (ec_lp_b11_held() && !A->bus) {     /* 0.6.13 / 0.6.14: B11 held: the pages on B1..B10 */
                const int pages = ec_pages_count();
                const uint16_t m = ec_e5_page_loop_mask(&A->proj, A->track, pages);
                for (i = 0; i < 10; i++) {         /* 0.6.14 (owner): a muted page dark; the shown one muted blinks */
                    const int in = (m >> i) & 1u;
                    l[i] = i >= pages ? EC_LED_OFF : i == A->step_page ? (in || blink(200) ? EC_LED_LIT : EC_LED_OFF)
                                                                       : in ? EC_LED_DIM : EC_LED_OFF;
                }
            } else if (ec_src_mode(mode)) {
                int cur = -1;
                ec_pn_roles(mode, &cur);
                for (i = 0; i < EC_NBKEY; i++) {
                    const char r = ec_src_role(mode, i + 1);
                    l[i] = r == 'P' ? (i == cur ? EC_LED_LIT : EC_LED_DIM) : r == 'S' ? EC_LED_DIM
                           : r == 'X' ? EC_LED_MID : EC_LED_OFF;
                    if (P->bk_held >> i & 1u && r != 'O')
                        l[i] = EC_LED_LIT;
                }
            }
            break;
        }
    }
    if (ec_lp_active() && !A->fn)                  /* 0.6.9: LOOP ENGINE's B11: lit on the STEP layer */
        l[10] = ec_lp_step_layer() ? EC_LED_LIT : ec_lp_first() >= 0 ? EC_LED_MID : EC_LED_DIM;
    for (i = 0; i < EC_NBKEY; i++)
        ec_plat_led_upper(i, l[i]);
}

/* ------------------------------------------------------------------------------------------- buttons --- */
static void leds_buttons(void)
{
    const int lit = ec_mode_button(shown_mode());
    const int popup = ec_app_popup(A);
    int mb;
    for (mb = 1; mb < EC_NMB; mb++) {
        uint8_t l;
        if (A->fn)
            l = mb == EC_MB_HOME || mb == EC_MB_FILE ? EC_LED_LIT : EC_LED_MID;   /* settings / SAVE */
        else if (mb == lit)
            l = A->settings && blink(250) ? EC_LED_MID : EC_LED_LIT;
        else
            l = EC_LED_FAINT;
        if (mb == EC_MB_SEQ && UX->f3)
            l = EC_LED_LIT;
        if (mb == EC_MB_FILE && P->file_refused_ms && A->ms - P->file_refused_ms < 800)
            l = blink(100) ? EC_LED_LIT : EC_LED_OFF;   /* FILE refused while playing: it flashes */
        ec_plat_led_button(mb_button(mb), l);
    }
    ec_plat_led_button(EC_B_FN, P->shift_sticky ? (blink(400) ? EC_LED_LIT : EC_LED_MID) : A->fn ? EC_LED_LIT
                                                                                                  : EC_LED_FAINT);
    ec_plat_led_button(EC_B_PLAY, A->seq.playing ? EC_LED_LIT : EC_LED_DIM);
    ec_plat_led_play_green(A->seq.playing ? EC_LED_LIT : EC_LED_OFF);
    /* live REC: lit while recording, blinking while CAPTURE waits / the count-in runs; FILE: dark */
    ec_plat_led_button(EC_B_REC, ec_app_mode() == EC_CAT_FILE ? EC_LED_OFF : ec_liverec_armed() ?
                                 (blink(250) ? EC_LED_LIT : EC_LED_DIM) : ec_liverec_on() ? EC_LED_LIT : EC_LED_DIM);
    if (popup) {                                   /* F1 back lit, F2 OK blinking (ChoralRoot) */
        ec_plat_led_button(EC_B_OCTDN, EC_LED_LIT);
        ec_plat_led_button(EC_B_OCTUP, blink(300) ? EC_LED_LIT : EC_LED_DIM);
    } else if (A->fn) {
        ec_plat_led_button(EC_B_OCTDN, A->e5.undo.n ? EC_LED_LIT : EC_LED_DIM);
        ec_plat_led_button(EC_B_OCTUP, A->e5.undo.nr ? EC_LED_LIT : EC_LED_DIM);
    } else if (ec_app_mode() == EC_MODE_KEYBOARD) {
        ec_plat_led_button(EC_B_OCTDN, P->oct < 0 ? EC_LED_LIT : EC_LED_DIM);
        ec_plat_led_button(EC_B_OCTUP, P->oct > 0 ? EC_LED_LIT : EC_LED_DIM);
    } else if (ec_rzui_active(A)) {
        ec_plat_led_button(EC_B_OCTDN, EC_LED_LIT);
        ec_plat_led_button(EC_B_OCTUP, EC_LED_LIT);
    } else {
        const int on = P->f_held | P->f_sticky;
        ec_plat_led_button(EC_B_OCTDN, on & 1 ? (P->f_sticky & 1 && blink(400) ? EC_LED_MID : EC_LED_LIT)
                                              : EC_LED_FAINT);
        ec_plat_led_button(EC_B_OCTUP, on & 2 ? (P->f_sticky & 2 && blink(400) ? EC_LED_MID : EC_LED_LIT)
                                              : EC_LED_FAINT);
    }
}

void ec_pn_leds(void)
{
    leds_white();
    leds_black();
    leds_buttons();
    if (!A->fn)
        ec_e5_leds();                              /* E5: muted pads faint */
}

/* ------------------------------------------------------------------------------------------- labels --- */
static void L(int which, const char *s) { ec_plat_label(which, s); }

static void knob_labels(void)
{
    const int mode = ec_app_mode();
    ec_page_t pg;
    int k, lane;
    char b[12];
    L(EC_LBL_KNOB0 + 7, "VOL");
    if (A->e5.naming) {
        static const char *const NK[4] = {"POS", "CHAR", "RANDOM", "DEL / SPC"};
        L(EC_LBL_KNOB0 + EC_E_SELECT, "POS");
        L(EC_LBL_KNOB0 + EC_E_ALGO, "");
        L(EC_LBL_KNOB0 + EC_E_PRESETS, "");
        for (k = 0; k < 4; k++)
            L(EC_LBL_KNOB0 + EC_E_K1 + k, NK[k]);
        return;
    }
    if ((ec_lp_b11_held() || mode == EC_MODE_PAGES) && !A->bus && !A->settings) {   /* 0.6.14: B11 held (PAGES:
                                                    * always): LEN, RATE, the zoom, the page */
        static const char *const BK[4] = {"LEN", "RATE", "ZOOM", ""};
        L(EC_LBL_KNOB0 + EC_E_SELECT, "");
        L(EC_LBL_KNOB0 + EC_E_ALGO, "STEP");
        L(EC_LBL_KNOB0 + EC_E_PRESETS, "PAGE");
        for (k = 0; k < 4; k++)
            L(EC_LBL_KNOB0 + EC_E_K1 + k, BK[k]);
        return;
    }
    if (P->as_open) {
        L(EC_LBL_KNOB0 + EC_E_SELECT, "ROW");
        L(EC_LBL_KNOB0 + EC_E_ALGO, "");
        L(EC_LBL_KNOB0 + EC_E_PRESETS, "");
        for (k = 0; k < 4; k++)                    /* 0.6.1: a list menu: K1 the selected row */
            L(EC_LBL_KNOB0 + EC_E_K1 + k, k ? "" : ec_as_row_label(P->as_row));
        return;
    }
    if (P->rs_open) {                              /* the record settings */
        L(EC_LBL_KNOB0 + EC_E_SELECT, "ROW");
        L(EC_LBL_KNOB0 + EC_E_ALGO, "");
        L(EC_LBL_KNOB0 + EC_E_PRESETS, "");
        for (k = 0; k < 4; k++)
            L(EC_LBL_KNOB0 + EC_E_K1 + k, k ? "" : ec_rs_row_label(P->rs_row));
        return;
    }
    if (UX->f3) {
        ec_page_f3(&pg);
        lane = 0;
        L(EC_LBL_KNOB0 + EC_E_SELECT, "TEMPO");
        L(EC_LBL_KNOB0 + EC_E_ALGO, "SWING");
        L(EC_LBL_KNOB0 + EC_E_PRESETS, "NAME");
    } else {
        ec_app_page(A, &pg);
        lane = ec_app_lane(A, &pg);
        L(EC_LBL_KNOB0 + EC_E_ALGO, A->fn ? "SWING" : ec_rzui_active(A) ? "SLICE" : mode == EC_CAT_FILE && ec_file_page() == EC_PIC_FILE ? "+/- POOL"
                                                    : mode == EC_CAT_FILE && ec_file_page() == EC_PIC_EDITOR ? "MARKER"
                                                    : mode == EC_CAT_FILE ? "" : mode == EC_MODE_KEYBOARD || ec_lp_active() ? "TARGET"
                                                    : ec_fx_on_sends() ? "SEND MODE" : "STEP PG");
        L(EC_LBL_KNOB0 + EC_E_PRESETS, mode == EC_CAT_MASTER ? "TRACK" : mode == EC_CAT_FILE && (ec_file_page() == EC_PIC_FILE || ec_file_page() == EC_PIC_POOL) ? "PREVIEW" : (ec_lp_active() || mode == EC_MODE_PAGES) && !A->step_held ? "PAGE" : A->fn ? "TRACK" : A->bus ? "" : A->step_held ? "LOCK SLOT" : "SLOT");
        if (A->fn)
            L(EC_LBL_KNOB0 + EC_E_SELECT, "BPM");
        else if (A->settings)
            L(EC_LBL_KNOB0 + EC_E_SELECT, "ROW");    /* 0.6.1: a list menu */
        else if (mode == EC_MODE_KEYBOARD || mode == EC_MODE_PAGES || mode == EC_CAT_XFD || mode == EC_MODE_SAMPLER)
            L(EC_LBL_KNOB0 + EC_E_SELECT, "");
        else if (mode == EC_CAT_PAT)
            L(EC_LBL_KNOB0 + EC_E_SELECT, "BANK");
        else if (ec_page_pic(A->cat, A->page[A->cat], A->bus) == EC_PIC_FILE)
            L(EC_LBL_KNOB0 + EC_E_SELECT, "FILE");
        else if (ec_page_pic(A->cat, A->page[A->cat], A->bus) == EC_PIC_POOL)
            L(EC_LBL_KNOB0 + EC_E_SELECT, "SLOT");
        else if (!A->bus && ec_page_pic(A->cat, A->page[A->cat], 0) == EC_PIC_MATRIX)
            L(EC_LBL_KNOB0 + EC_E_SELECT, "ROW");
        else if (ec_ux_lanes(pg.n) > 1) {
            snprintf(b, sizeof b, "LANE %d", lane + 1);
            L(EC_LBL_KNOB0 + EC_E_SELECT, b);
        } else
            L(EC_LBL_KNOB0 + EC_E_SELECT, "PAGE");
    }
    for (k = 0; k < 4; k++) {
        const int c = A->settings && !UX->f3 ? (k ? -1 : ec_app_set_item(A, &pg)) : lane * 4 + k;   /* a list: K1 */
        const ec_item_t *it = c >= 0 && c < pg.n ? &pg.it[c] : 0;
        const char *s = it && it->kind != EC_IT_BLANK ? ec_app_item_label(it) : "";
        if (P->cat)
            s = "";
        if (P->slot_key && !UX->f3 && (k == 0)) {  /* a quick setting held: K1 changes it */
            const int id = ec_app_mode() == EC_CAT_FX ? EC_P_FX_MACHINE
                           : ec_app_mode() == EC_CAT_FILT ? EC_P_FILTER_TYPE
                           : ec_app_mode() == EC_CAT_AMP ? EC_P_ENV_MODE : EC_P_MACHINE;
            L(EC_LBL_KNOB0 + EC_E_K1, EC_PARAMS[id].label);
            continue;
        }
        if (s[0] && it->kind == EC_IT_PARAM && !UX->f3 && !A->settings && (ec_pn_f(1) || ec_pn_f(2))) {
            snprintf(b, sizeof b, "%s %s", ec_pn_f(1) ? "CLR" : "UNMOD", s);
            s = b;
        } else if (s[0] && A->fn) {
            snprintf(b, sizeof b, "SNAP %s", s);
            s = b;
        }
        L(EC_LBL_KNOB0 + EC_E_K1 + k, s);
    }
}

static const char *button_name(int mb)
{
    int modes[8], i;
    const int n = ec_button_modes(mb, modes, 8);
    const int cur = shown_mode();
    static const char *const D[EC_NMB] = {"", "FX", "FILTER", "AMP", "MOD", "MIX", "LOOP", "FILE", "XFADE", "PATTERN"};
    /* 0.6.11: HOME and FILE from another mode open their first sub-mode (ec_panel.c mode_tap), EDIT its last */
    const int opens = mb == EC_MB_HOME || mb == EC_MB_FILE ? (n > 0 ? modes[0] : -1) : P->last[mb];
    for (i = 0; i < n; i++)                        /* a button with sub-modes: the one it is on (or will open) */
        if (modes[i] == cur || (ec_mode_button(cur) != mb && modes[i] == opens)) {
            switch (modes[i]) {
            case EC_CAT_SRC: return "LOOP";
            case EC_CAT_TRIG: return "TRIG";
            case EC_MODE_KEYBOARD: return "KEYS";
            case EC_CAT_MASTER: return "MIX";
            case EC_MODE_PAGES: return "PAGES";
            case EC_CAT_FILE:                  /* 0.6.15: its sub-modes by page */
                if (cur == EC_CAT_FILE && ec_file_page() == EC_PIC_EDITOR)
                    return "EDITOR";
                if (cur == EC_CAT_FILE && ec_file_page() == EC_PIC_RAZOR)
                    return "SLICE";
                return "FILE";
            case EC_MODE_SAMPLER: return "SAMPLER";
            default: break;
            }
        }
    return D[mb];
}

static void button_labels(void)
{
    const int popup = ec_app_popup(A);
    const int mode = ec_app_mode();
    int mb;
    for (mb = 1; mb < EC_NMB; mb++) {
        const char *s = button_name(mb);
        if (A->fn && !popup)
            s = mb == EC_MB_HOME ? "LOOP SETUP" : mb == EC_MB_FILE ? "SAVE" : mb == EC_MB_SEQ ? "F3"
                : mb == EC_MB_ARP ? "APP SETTINGS" : s;
        L(EC_LBL_BTN0 + mb_button(mb), s);
    }
    L(EC_LBL_BTN0 + EC_B_GLO, "SHIFT");
    L(EC_LBL_BTN0 + EC_B_PLAY, mode == EC_CAT_FILE ? "" : A->seq.playing ? "STOP" : "PLAY");
    L(EC_LBL_BTN0 + EC_B_REC, mode == EC_CAT_FILE ? "" : P->rs_open ? "OK" : A->fn ? "REC SETUP"
                               : ec_liverec_on() ? "REC OFF" : "REC");
    if (A->e5.confirm) {
        L(EC_LBL_BTN0 + EC_B_OCTDN, "NO");
        L(EC_LBL_BTN0 + EC_B_OCTUP, "YES");
    } else if (popup) {
        L(EC_LBL_BTN0 + EC_B_OCTDN, "BACK");
        L(EC_LBL_BTN0 + EC_B_OCTUP, "OK");
    } else if (A->fn) {
        L(EC_LBL_BTN0 + EC_B_OCTDN, ec_rzui_active(A) ? "EVEN" : "UNDO");
        L(EC_LBL_BTN0 + EC_B_OCTUP, ec_rzui_active(A) ? "AUTO-CHOP" : "REDO");
    } else if (ec_rzui_active(A)) {
        L(EC_LBL_BTN0 + EC_B_OCTDN, "DELETE");
        L(EC_LBL_BTN0 + EC_B_OCTUP, "SPLIT");
    } else if (mode == EC_MODE_KEYBOARD) {
        L(EC_LBL_BTN0 + EC_B_OCTDN, "OCT-");
        L(EC_LBL_BTN0 + EC_B_OCTUP, "OCT+");
    } else if (mode == EC_CAT_FILE && ec_file_page() == EC_PIC_FILE) {
        L(EC_LBL_BTN0 + EC_B_OCTDN, "COPY");
        L(EC_LBL_BTN0 + EC_B_OCTUP, "FREE SLOT");
    } else if (mode == EC_CAT_FILE && ec_file_page() == EC_PIC_EDITOR) {   /* 0.6.1: the point the keys place */
        L(EC_LBL_BTN0 + EC_B_OCTDN, "START");
        L(EC_LBL_BTN0 + EC_B_OCTUP, "END");
    } else if (mode == EC_CAT_FILE && ec_file_page() == EC_PIC_POOL) {
        L(EC_LBL_BTN0 + EC_B_OCTDN, "");           /* 0.6.15: the move is B11 held */
        L(EC_LBL_BTN0 + EC_B_OCTUP, "FREE");
    } else if (P->f_held && P->f_held != 3) {
        L(EC_LBL_BTN0 + EC_B_OCTDN, P->f_held & 1 ? "COPY" : "F3");
        L(EC_LBL_BTN0 + EC_B_OCTUP, P->f_held & 2 ? "CUT" : "F3");
    } else {
        const int pt = mode == EC_MODE_PAGES ? 1 : mode == EC_CAT_PAT ? 2 : 0;
        static const char *const C[3] = {"COPY", "COPY PG", "COPY PT"}, *const X[3] = {"CUT", "CUT PG", "CUT PT"};
        L(EC_LBL_BTN0 + EC_B_OCTDN, C[pt]);
        L(EC_LBL_BTN0 + EC_B_OCTUP, X[pt]);
    }
}

static void white_labels(void)
{
    static const char *const T[9] = {"T1", "T2", "T3", "T4", "T5", "T6", "S1", "S2", "MST"};
    static const char *const NN[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    static const char *const G[4] = {"INST", "FAST", "MED", "SLOW"};
    const int mode = ec_app_mode();
    int i;
    char b[12];
    for (i = 0; i < EC_NLOWER; i++) {
        b[0] = 0;
        if (A->e5.naming || P->as_open || P->rs_open)
            ;
        else if (mode == EC_MODE_SAMPLER && !A->fn)
            snprintf(b, sizeof b, "%ddB", -48 + 3 * (i + 1));   /* (the VU's scale) */
        else if (mode == EC_CAT_FILE && !A->fn && !A->settings && ec_file_page() == EC_PIC_EDITOR)
            snprintf(b, sizeof b, "%s", A->pn.ed_point ? "END" : "START");
        else if (mode == EC_CAT_FILE && !A->fn && !A->settings &&
                 (ec_file_page() == EC_PIC_FILE || ec_file_page() == EC_PIC_POOL))
            ;
        else if (UX->f3)
            snprintf(b, sizeof b, "PAGE %d", i + 1);
        else if (A->fn && mode == EC_CAT_XFD) {
            if (i < 12)
                snprintf(b, sizeof b, "JUMP %d", (int)(ec_xfd_key_value(i) * 127.0f + 0.5f));
        }
        else if (A->fn && mode == EC_CAT_MASTER && i < EC_NTRACK)
            snprintf(b, sizeof b, A->fill_held ? "ALL ON" : "SOLO %s", T[i]);
        else if (A->fn)
            snprintf(b, sizeof b, "%s", i < 9 ? T[i] : "");
        else if (P->slot_key && mode == EC_CAT_FX) {
            if (i + 1 <= (int)EC_PARAMS[EC_P_FX_MACHINE].max)
                snprintf(b, sizeof b, "FX %d", i + 1);
        }
        else if (A->settings)
            snprintf(b, sizeof b, "%d", A->step_page * 16 + i + 1);
        else
            switch (mode) {
            case EC_CAT_MASTER: snprintf(b, sizeof b, "%s", i < 9 ? T[i] : ""); break;
            case EC_CAT_SRC: {
                const float *v = ec_app_params(A);
                if (ec_lp_step_layer())            /* 0.6.9: B11's STEP layer: the keys select steps */
                    snprintf(b, sizeof b, "STEP %d", A->step_page * 16 + i + 1);
                else
                    snprintf(b, sizeof b, "%s%d", is_slice_m(iv(v, EC_P_MACHINE)) ? "SL" : "L",
                             i + 1 + (P->zoom_half ? 16 : 0));
            } break;
            case EC_MODE_KEYBOARD: {
                const int s = ec_kb_semis(ec_key_of_lower(i));
                snprintf(b, sizeof b, "%s%d", NN[(s % 12 + 12) % 12], 4 + (int)floorf((float)s / 12.0f));
            } break;
            case EC_CAT_XFD:
                if (i < 12)
                    snprintf(b, sizeof b, "%d", (int)(ec_xfd_key_value(i) * 127.0f + 0.5f));
                else
                    snprintf(b, sizeof b, "%s", G[i - 12]);
                break;
            case EC_CAT_PAT: snprintf(b, sizeof b, "%c%d", 'A' + P->bank, i + 1); break;
            default:
                if (ec_rzui_active(A))
                    snprintf(b, sizeof b, "%s %d", A->fn ? "MUTE" : "SLICE", (ec_rzui.sel - 1) / 16 * 16 + i + 1);
                else
                    snprintf(b, sizeof b, "%d", A->step_page * 16 + i + 1);
                break;
            }
        L(EC_LBL_WHITE0 + i, b);
    }
}

static void black_labels(void)
{
    static const char *const SH[EC_NBKEY] = {"TAP", "LATCH", "", "RECALL", "MEMO", "", "", "", "", "", "FILL"};
    static const char *const MIX[EC_NBKEY] = {"1/8", "1/4", "1/2", "3/4", "1X", "3/2", "2X", "4X", "SLOWER", "FASTER",
                                              "CAT"};
    static const char *const NN[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    const int mode = ec_app_mode();
    int i;
    char b[12];
    if (ec_pn_picking()) {                         /* 0.6.15: the held button's modes */
        int modes[8], pages[8];
        const int n = ec_pn_pick_list(P->btn, modes, pages, 8);
        for (i = 0; i < EC_NBKEY; i++)
            L(EC_LBL_BLACK0 + i, i < n ? pick_name(modes[i], pages[i]) : "");
        return;
    }
    for (i = 0; i < EC_NBKEY; i++) {
        const char *s = "";
        b[0] = 0;
        if (A->e5.naming || P->as_open || P->rs_open || UX->f3)
            ;
        else if (A->fn && (mode == EC_CAT_FILE || mode == EC_MODE_SAMPLER))
            s = i == 10 ? "PURGE" : "";
        else if (A->fn && mode == EC_CAT_XFD)
            s = i == 0 ? "CLEAR A" : i == 1 ? "CLEAR B" : "";
        else if (A->fn)
            s = SH[i];
        else if (A->settings)
            ;
        else if (mode == EC_CAT_TRIG && A->step_held && !A->bus)   /* 0.6.14: a step held: the note, F# = 0 */
            snprintf(b, sizeof b, "%+d", i - 5), s = b;
        else if (ec_lp_b11_held() && !A->bus && i < 10) {   /* 0.6.13 / 0.6.14: the pages (the step modes) */
            if (i < ec_pages_count())
                snprintf(b, sizeof b, "PAGE%d", i + 1), s = b;
        } else
            switch (mode) {
            case EC_CAT_MASTER: s = i < 8 && !ec_mix_rate_index(i + 1) ? "" : MIX[i]; break;
            case EC_MODE_PAGES:
                if (i < ec_pages_count())
                    snprintf(b, sizeof b, "PAGE%d", i + 1), s = b;
                break;
            case EC_MODE_KEYBOARD:
                if (ec_kb_slice_track()) {
                    if (i == 10)
                        s = "BANK";
                    else
                        snprintf(b, sizeof b, "SL%d", P->kb_bank * 10 + i + 1), s = b;
                } else {
                    const int sm = ec_kb_semis(ec_key_of_upper(i));
                    snprintf(b, sizeof b, "%s%d", NN[(sm % 12 + 12) % 12], 4 + (int)floorf((float)sm / 12.0f));
                    s = b;
                }
                break;
            case EC_CAT_XFD: s = i == 0 ? "SCENE A" : i == 1 ? "SCENE B" : i == 10 ? "CAPTURE" : ""; break;
            case EC_CAT_PAT:
                if (i < EC_NBANK)
                    snprintf(b, sizeof b, "BANK %c", 'A' + i), s = b;
                break;
            case EC_CAT_FILE: {
                static const char *const BR[EC_NBKEY] = {"UP", "INTO", "", "", "", "", "", "", "REMOVE", "ADD", "PREVIEW"};
                static const char *const PO[EC_NBKEY] = {"", "", "", "", "", "", "", "", "FREE", "ASSIGN", "MOVE"};
                static const char *const ED[EC_NBKEY] = {"P1", "P2", "", "", "", "", "", "", "AUTO BPM", "RECALC", "PREVIEW"};
                static const char *const RZ[EC_NBKEY] = {"P1", "P2", "P3", "", "", "", "", "", "EVEN", "CHOP", "PREVIEW"};
                const int pic = ec_file_page();
                if (pic == EC_PIC_POOL && (i == 8 || i == 9 || i == 10))
                    s = PO[i];
                else if (pic == EC_PIC_EDITOR)
                    s = ED[i];
                else if (pic == EC_PIC_RAZOR)
                    s = RZ[i];
                else if (ec_file_key_page(i + 1) >= 0) {
                    ec_page_t pg;
                    ec_page_items(EC_CAT_FILE, ec_file_key_page(i + 1), ec_app_params(A), 0, &pg);
                    s = pg.title ? pg.title : "";
                } else if (pic == EC_PIC_FILE)
                    s = BR[i];
                else if (pic == EC_PIC_POOL)
                    s = i == 9 ? "ASSIGN" : i == 10 ? "PREVIEW" : "";
            } break;
            case EC_MODE_SAMPLER: {
                static const char *const SL[5] = {"FREE", "1 BAR", "2 BARS", "4 BARS", "8 BARS"};
                static const char *const ST[4] = {"ARM", "DISARM", "STOP", ""};
                if (i < 5)
                    s = SL[i];
                else if (i == 10)
                    s = ST[clampi(A->rec.state, 0, 3)];
            } break;
            default:
                if (ec_src_mode(mode))
                    s = ec_src_label(mode, i + 1);
                break;
            }
        L(EC_LBL_BLACK0 + i, s);
    }
}

void ec_app_labels(void)
{
    knob_labels();
    button_labels();
    white_labels();
    black_labels();
    if (A->rec.open && !A->fn && !A->e5.naming && !A->e5.confirm)   /* E6: the SAMPLER's stand-in */
        ec_rec_labels();
    ec_mk_labels();                                                    /* a source held: K1..K4 route it */
}
