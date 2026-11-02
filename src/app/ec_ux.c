/* SPDX-License-Identifier: GPL-3.0-only */
/* 0.3.0 (ec_ux.h): the lanes, the knob speed, the takeover, the lock chips' lookup and the live panel labels
 * (ec_app_labels: what every knob, button and key does right now; plat reads them through fm1core_label). */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ec_app.h"
#include "ec_browse.h"
#include "ec_razor_ui.h"

#define A (&ec_app)
#define UX (&ec_app.ux)

static int clampi(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }

int ec_ux_lanes(int n) { return n <= 4 ? 1 : (n + 3) / 4; }

/* Slowly a click is the finest step; spun, the steps grow up to ~1/16 of the range a click (FIZZ; ChoralRoot's 5 %
 * is near). The speed is the gap since the knob's last turn (the detents of one tick share its multiplier). */
int ec_ux_accel(int role, int steps, int det)
{
    uint32_t gap = A->ms - UX->knob_ms[clampi(role, 0, 7)];
    int maxm;
    float t;
    UX->knob_ms[clampi(role, 0, 7)] = A->ms;
    if (steps <= 24)                                 /* lists and small ranges: one step a click */
        return 1;
    (void)det;
    maxm = steps / 16 > 1 ? steps / 16 : 1;
    if (gap >= 90)
        return 1;
    t = (float)(90 - (int)gap) / 80.0f;              /* 90 ms: 0 .. 10 ms: 1 */
    t = t > 1 ? 1 : t;
    return 1 + (int)(t * t * (float)(maxm - 1) + 0.5f);
}

void ec_ux_takeover(const char *label, const char *val, float frac)
{
    snprintf(UX->to_label, sizeof UX->to_label, "%s", label);
    snprintf(UX->to_val, sizeof UX->to_val, "%s", val);
    UX->to_sub[0] = 0;
    UX->to_frac = frac < 0 ? 0 : frac > 1 ? 1 : frac;
    UX->to_kind = EC_TO_VALUE;
    UX->to_ms = A->ms ? A->ms : 1;
    A->redraw = 1;
}

int ec_ux_locked_somewhere(int id)
{
    const ec_ptrack_t *tr = &A->proj.pat[A->proj.current].tr[A->track];
    const int len = ec_seq_pattern_steps(&A->seq, A->track);
    int i, k;
    for (i = 0; i < len && i < EC_MAXSTEPS; i++)
        for (k = 0; k < tr->steps[i].nlock; k++)
            if (tr->steps[i].lock_id[k] == id)
                return 1;
    return 0;
}

/* ------------------------------------------------------------------------------------------- labels --- */
static void L(int which, const char *s) { ec_plat_label(which, s); }

static int iv(const float *v, int id) { return (int)floorf(v[id] + 0.5f); }

static int has_setup(int cat)
{
    ec_page_t pg;
    if (A->bus && ec_settings_items_bus(cat, A->bus, &pg))
        return 1;
    return ec_settings_items(cat, ec_project_track(&A->proj, A->track)->v, &pg);
}

static const int BTN_CAT[EC_NB] = {EC_CAT_FX, EC_CAT_SRC, EC_CAT_AMP, EC_CAT_MOD, EC_CAT_FILT, -1, -1,
                                   EC_CAT_FILE, EC_CAT_PAT, EC_CAT_TRIG, -1, -1, -1, -1};
static const char *const CATN[EC_NB] = {"FX", "SRC", "AMP", "MOD", "FILT", "SHIFT", "HOME", "FILE", "PAT", "TRIG",
                                        "PLAY", "REC", "LOOP KEYS", "KEYS"};

static void knob_labels(void)
{
    ec_page_t pg;
    int k, lane;
    char b[12];
    L(EC_LBL_KNOB0 + EC_E_ALGO, A->e5.naming ? "" : UX->f3 ? "SWING" : "TRACK");
    L(EC_LBL_KNOB0 + EC_E_PRESETS, A->e5.naming ? "" : A->fn ? "PATTERN" : "SAMPLE");
    L(EC_LBL_KNOB0 + 7, "VOL");
    if (A->e5.naming) {
        static const char *const NK[4] = {"POS", "CHAR", "RANDOM", "DEL / SPC"};
        L(EC_LBL_KNOB0 + EC_E_SELECT, "POS");
        for (k = 0; k < 4; k++)
            L(EC_LBL_KNOB0 + EC_E_K1 + k, NK[k]);
        return;
    }
    if (UX->f3) {
        ec_page_f3(&pg);
        lane = 0;
        L(EC_LBL_KNOB0 + EC_E_SELECT, "TEMPO");
    } else {
        ec_app_page(A, &pg);
        lane = ec_app_lane(A, &pg);
        if (ec_rzui_active(A))
            L(EC_LBL_KNOB0 + EC_E_SELECT, "SLICE");
        else if (ec_page_pic(A->cat, A->page[A->cat], A->bus) == EC_PIC_FILE && !A->settings)
            L(EC_LBL_KNOB0 + EC_E_SELECT, "FILE");
        else if (ec_ux_lanes(pg.n) > 1) {
            snprintf(b, sizeof b, "LANE %d", lane + 1);
            L(EC_LBL_KNOB0 + EC_E_SELECT, b);
        } else
            L(EC_LBL_KNOB0 + EC_E_SELECT, A->settings ? "" : "PAGE");
    }
    for (k = 0; k < 4; k++) {
        const int c = lane * 4 + k;
        const ec_item_t *it = c < pg.n ? &pg.it[c] : 0;
        const char *s = it && it->kind != EC_IT_BLANK ? ec_app_item_label(it) : "";
        if (s[0] && (UX->oct_held & 2) && it->kind == EC_IT_PARAM && !UX->f3) {
            snprintf(b, sizeof b, "CLR %s", s);   /* OCT+ held: the knob clears that lock */
            s = b;
            L(EC_LBL_KNOB0 + EC_E_K1 + k, s);
            continue;
        }
        if (s[0] && A->fn) {
            snprintf(b, sizeof b, "SNAP %s", s);
            s = b;
        }
        L(EC_LBL_KNOB0 + EC_E_K1 + k, s);
    }
}

static void button_labels(void)
{
    const int scope = ec_e5_scope_held();
    const int popup = ec_app_popup(A);
    int b;
    for (b = 0; b < EC_NB; b++) {
        const char *s = CATN[b];
        const int c = BTN_CAT[b];
        if (A->fn) {                                   /* SHIFT held: its shortcuts, the rest has no job */
            s = c >= 0 ? (has_setup(c) ? (A->settings && A->cat == c ? "PAGES" : "SETUP") : "") : "";
            if (b == EC_B_GLO)
                s = "SHIFT";
            else if (b == EC_B_PLAY)
                s = scope ? "PASTE" : "STOP";
            else if (b == EC_B_REC)
                s = scope ? "COPY" : "SAVE";
            else if (b == EC_B_OCTDN)
                s = ec_rzui_active(A) ? "EVEN" : "UNDO";
            else if (b == EC_B_OCTUP)
                s = ec_rzui_active(A) ? "AUTO-CHOP" : "REDO";
        } else {
            if (c >= 0 && A->settings && A->cat == c)
                s = "PAGES";
            else if (b == EC_B_SAVE && A->cat == EC_CAT_FILE && ec_page_pic(A->cat, A->page[A->cat], A->bus) == EC_PIC_FILE &&
                     ec_browse_count() > 0 &&
                     strcmp(A->proj.slot[ec_app_track_slot(A, A->track)].path, ec_browse_path(A->cursor)) != 0)
                s = "LOAD";
            else if (b == EC_B_PLAY)
                s = scope ? "CLEAR" : A->seq.playing ? "PAUSE" : "PLAY";
            else if (b == EC_B_REC)
                s = scope ? "COPY" : "SAMPLING";                   /* E6: the REC pop-up */
            else if (b == EC_B_OCTDN || b == EC_B_OCTUP) {
                if (A->e5.confirm)
                    s = b == EC_B_OCTUP ? "YES" : "NO";
                else if (popup)
                    s = b == EC_B_OCTUP ? "OK" : "BACK";
                else if (ec_rzui_active(A))
                    s = b == EC_B_OCTUP ? "SPLIT" : "DELETE";
                else if (UX->oct_held && !(UX->oct_held & (b == EC_B_OCTDN ? 1 : 2)))
                    s = "F3";                                  /* the other OCT held: both = F3 */
                else
                    s = b == EC_B_OCTDN ? (A->keymode & 1 ? "STEPS" : "LOOP KEYS") : (A->keymode & 2 ? "STEPS" : "KEYS");
            }
        }
        L(EC_LBL_BTN0 + b, s);
    }
}

static void upper_labels(void)
{
    static const char *const U[EC_NUPPER] = {"T1", "T2", "T3", "T4", "T5", "T6", "S1", "S2", "MST", "FILL", "PATN"};
    static const char *const SU[EC_NUPPER] = {"MEMORIZE", "RECALL", "", "", "", "", "COPY PAT", "PASTE PAT", "TAP",
                                              "LATCH", "NAME"};
    int i;
    for (i = 0; i < EC_NUPPER; i++) {
        const char *s = A->e5.naming ? "" : A->fn ? SU[i] : U[i];
        if (!A->fn && !A->e5.naming && i < EC_NTRACK && A->fill_held)
            s = "ALL ON";
        L(EC_LBL_BLACK0 + i, s);
    }
}

static void lower_labels(void)
{
    static const char *const NN[7] = {"C", "D", "E", "F", "G", "A", "B"};
    const float *v = ec_project_track(&A->proj, A->track)->v;
    const int m = iv(v, EC_P_MACHINE);
    const int slice = m >= EC_MACH_GRID_SLICE && m <= EC_MACH_RAZOR_POLY;
    int i;
    char b[12];
    for (i = 0; i < EC_NLOWER; i++) {
        b[0] = 0;
        if (A->e5.naming)
            ;
        else if (UX->f3)
            snprintf(b, sizeof b, "PAGE %d", i + 1);
        else if (ec_rzui_active(A) && !A->sel_held)
            snprintf(b, sizeof b, "%s %d", A->fn ? "MUTE" : "SLICE", (ec_rzui.sel - 1) / 16 * 16 + i + 1);
        else if (A->patn_open)
            snprintf(b, sizeof b, "P%02d", i + 1);
        else if (ec_e5_scene_layer()) {
            const int j = i % 8;
            if (j == 0 || j == 7)
                snprintf(b, sizeof b, "%s%s", A->fn ? "STORE " : "", j ? "B" : "A");
            else
                snprintf(b, sizeof b, "GLIDE %d", j);
        } else if (A->sel_held || A->keymode == EC_KM_LOOP) {
            const int hi = A->fn ? 16 : 0;
            snprintf(b, sizeof b, "%s %d", slice ? "SLICE" : "LOOP", i + 1 + hi);
        } else if (A->keymode == EC_KM_KEYS)
            snprintf(b, sizeof b, "%s%d", NN[i % 7], 3 + i / 7);
        else
            snprintf(b, sizeof b, "%s%d", A->fn ? "GHOST " : "", A->step_page * 16 + i + 1);
        L(EC_LBL_WHITE0 + i, b);
    }
}

void ec_app_labels(void)
{
    knob_labels();
    button_labels();
    upper_labels();
    lower_labels();
    if (A->rec.open && !A->fn && !A->e5.naming && !A->e5.confirm)   /* E6: the REC pop-up's knobs and buttons */
        ec_rec_labels();
}
