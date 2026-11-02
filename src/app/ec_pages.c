/* SPDX-License-Identifier: GPL-3.0-only */
/* Categories, pages and cells (ec_pages.h). The cell order follows the original's pages, rearranged where the FM-1's
 * four knobs want the everyday params on KN1-KN4 (the original paired them for E2 / E3). */
#include <string.h>

#include "ec_dsp.h"
#include "ec_pages.h"
#include "ec_params.h"

static const char *const CAT[EC_NCAT] = {"TRIG", "SRC", "FILT", "AMP", "FX", "MOD", "MST", "FILE", "PAT"};
const char *ec_cat_name(int cat) { return cat >= 0 && cat < EC_NCAT ? CAT[cat] : "?"; }

static int iv(const float *v, int id) { return (int)(v[id] + 0.5f); }
static int is_slice(const float *v)
{
    const int m = iv(v, EC_P_MACHINE);
    return m >= EC_MACH_GRID_SLICE && m <= EC_MACH_RAZOR_POLY;
}

static void P(ec_page_t *o, int id, const char *label)
{
    if (o->n < EC_PAGE_ITEMS) {
        o->it[o->n].kind = id < 0 ? EC_IT_BLANK : EC_IT_PARAM;
        o->it[o->n].id = (uint8_t)(id < 0 ? 0 : id);
        o->it[o->n].flags = 0;
        o->it[o->n].label = label;
        o->n++;
    }
}
static void G(ec_page_t *o, int id, const char *label)
{
    if (o->n < EC_PAGE_ITEMS) {
        o->it[o->n].kind = EC_IT_GLOBAL;
        o->it[o->n].id = (uint8_t)id;
        o->it[o->n].flags = 0;
        o->it[o->n].label = label;
        o->n++;
    }
}
static void S(ec_page_t *o, int id, const char *label)
{
    if (o->n < EC_PAGE_ITEMS) {
        o->it[o->n].kind = EC_IT_SLOT;
        o->it[o->n].id = (uint8_t)id;
        o->it[o->n].flags = 0;
        o->it[o->n].label = label;
        o->n++;
    }
}
static void stopped_only(ec_page_t *o) { o->it[o->n - 1].flags |= EC_IF_STOPPED; }
static void Z(ec_page_t *o, int id)   /* E3: a razor editor cell */
{
    static const char *const LB[EC_NRZ] = {"STRT", "END", "PLAY", "ZOOM", "SNAP", "CHOK", "P/T", "SENS"};
    if (o->n < EC_PAGE_ITEMS) {
        o->it[o->n].kind = EC_IT_RAZOR;
        o->it[o->n].id = (uint8_t)id;
        o->it[o->n].flags = 0;
        o->it[o->n].label = LB[id];
        o->n++;
    }
}

int ec_cat_pages(int cat, const float *v)
{
    (void)v;
    switch (cat) {
    case EC_CAT_TRIG: return 3;
    case EC_CAT_SRC: return 4;
    case EC_CAT_FILT: return 3;
    case EC_CAT_FX: return 2;
    case EC_CAT_MOD: return 9;            /* E4: LFO 1, LFO 2, MOD ENV, VELO, MACROS, 4 x the macro's matrix */
    case EC_CAT_MASTER: return 2;
    case EC_CAT_FILE: return 2;   /* E3: SAMPLE, RAZOR */
    default: return 1;
    }
}

static void warp_items(const float *v, ec_page_t *o)
{
    switch (iv(v, EC_P_MODE)) {
    case EC_WARP_TAPE:
    case EC_WARP_TEMPO_VARISPEED:
        P(o, EC_P_WARP_RATE, 0);
        P(o, EC_P_MODE_SWITCH_FADE, 0);
        break;
    case EC_WARP_GRANULAR:
        P(o, EC_P_GRAIN_DENSITY, 0);
        P(o, EC_P_GRAIN_SIZE, 0);
        P(o, EC_P_GRAIN_JITTER, 0);
        P(o, EC_P_WARP_RATE, 0);
        P(o, EC_P_GRAIN_SPEED, 0);
        P(o, EC_P_GRAIN_SPEED_RAND, 0);
        P(o, EC_P_GRAIN_DIRECTION, 0);
        break;
    case EC_WARP_CHOPPED:
        P(o, EC_P_CHOP_SLICE_LEN, 0);
        P(o, EC_P_CHOP_HOLD, 0);
        P(o, EC_P_CHOP_LOOP_MODE, 0);
        P(o, EC_P_WARP_RATE, 0);
        P(o, EC_P_CHOP_ATTACK, 0);
        P(o, EC_P_CHOP_RELEASE, 0);
        break;
    /* E3: lib/warp_modes (.lua) through mode_param_layout (top row first; RATE after the mode's own top-row params,
     * envelope ATK / REL leading the second row) */
    case EC_WARP_DOMINO:
        P(o, EC_P_CHOP_STEPS, "SLCS"), P(o, EC_P_CHOP_SLICE_LEN, 0), P(o, EC_P_CHOP_LOOP_MODE, 0);
        P(o, EC_P_WARP_RATE, 0);
        P(o, EC_P_CHOP_ATTACK, 0), P(o, EC_P_CHOP_RELEASE, 0), P(o, EC_P_CHOP_HOLD, 0);
        break;
    case EC_WARP_RANDOM_OLA:
        P(o, EC_P_MODE_MACRO, "CHAOS"), P(o, EC_P_WSOLA_WINDOW, 0), P(o, EC_P_WSOLA_SEARCH, 0);
        P(o, EC_P_WARP_RATE, 0);
        break;
    case EC_WARP_PITCH_CORRECTED:
        P(o, EC_P_MODE_MACRO, "SMEAR"), P(o, EC_P_PV_WINDOW, 0), P(o, EC_P_PV_DISPERSION, 0);
        P(o, EC_P_WARP_RATE, 0);
        break;
    case EC_WARP_HARMONIZER:
        P(o, EC_P_MODE_MACRO, "MIX"), P(o, EC_P_HARM_INTERVAL, 0), P(o, EC_P_HARM_INTERVAL2, 0);
        P(o, EC_P_HARM_INTERVAL3, 0), P(o, EC_P_WARP_RATE, 0);
        break;
    case EC_WARP_WAVETABLE:
        P(o, EC_P_MODE_MACRO, "MORF"), P(o, EC_P_WT_WINDOW, 0), P(o, EC_P_WT_CYCLE, 0), P(o, EC_P_WT_LFO_RATE, 0);
        P(o, EC_P_WT_LFO_DEPTH, 0), P(o, EC_P_WT_LFO_SHAPE, 0), P(o, EC_P_XFADE, 0), P(o, EC_P_WARP_RATE, 0);
        break;
    case EC_WARP_SPECTRAL_FREEZE:
        P(o, EC_P_FREEZE_AMOUNT, 0), P(o, EC_P_SPECTRAL_BLUR, 0), P(o, EC_P_WARP_RATE, 0);
        break;
    case EC_WARP_FORMANT:
        P(o, EC_P_FORMANT_SHIFT, 0), P(o, EC_P_FREEZE_AMOUNT, 0), P(o, EC_P_WARP_RATE, 0);
        break;
    case EC_WARP_GSTRETCH:
    case EC_WARP_GSTRETCH2:
        P(o, EC_P_GRAIN_SPEED, "STRCH"), P(o, EC_P_GRAIN_SIZE, "PWIN"), P(o, EC_P_GRAIN_DENSITY, "SMTH");
        P(o, EC_P_WARP_RATE, 0);
        break;
    default:
        P(o, EC_P_MODE_MACRO, 0);
        P(o, EC_P_WARP_RATE, 0);
        break;
    }
}

static void filter_items(const float *v, ec_page_t *o)
{
    switch (iv(v, EC_P_FILTER_MACHINE)) {
    case EC_FILT_MORPHING:
        P(o, EC_P_FILTER_CUTOFF, 0), P(o, EC_P_FILTER_RES, 0), P(o, EC_P_FILTER_MORPH, 0);
        P(o, EC_P_FENV_DEPTH, 0), P(o, EC_P_FILTER_DRIVE, 0);
        break;
    case EC_FILT_LADDER:
        P(o, EC_P_FILTER_CUTOFF, 0), P(o, EC_P_FILTER_RES, 0), P(o, EC_P_FILTER_DRIVE, 0);
        P(o, EC_P_FENV_DEPTH, 0);
        break;
    case EC_FILT_COMB:
        P(o, EC_P_FILTER_CUTOFF, "TUNE"), P(o, EC_P_FILTER_RES, "FB"), P(o, EC_P_FILTER_DRIVE, "DAMP");
        P(o, EC_P_FENV_DEPTH, 0), P(o, EC_P_FILTER_MIX, 0);
        break;
    case EC_FILT_FORMANT:
        P(o, EC_P_FILTER_CUTOFF, 0), P(o, EC_P_FILTER_RES, 0), P(o, EC_P_FILTER_MORPH, "VOWL");
        P(o, EC_P_FENV_DEPTH, 0), P(o, EC_P_FILTER_MIX, 0);
        break;
    default:                                   /* classic, svf */
        P(o, EC_P_FILTER_CUTOFF, 0), P(o, EC_P_FILTER_RES, 0), P(o, EC_P_FILTER_TYPE, 0);
        P(o, EC_P_FENV_DEPTH, 0), P(o, EC_P_FILTER_DRIVE, 0);
        break;
    }
}

/* E4: the FX machine's cells (lib/fx_modes/<machine>.lua) */
static void fx_items(int machine, ec_page_t *o)
{
    int ids[8], n, i;
    const char *lb[8];
    n = ec_fx_page_items(machine, ids, lb);
    for (i = 0; i < n; i++)
        P(o, ids[i], lb[i]);
}

void ec_page_items_bus(int cat, int page, const float *v, int bus, const float *bv, ec_page_t *o)
{
    ec_page_items(cat, page, v, bus, o);
    if (o->busv && bv)
        fx_items((int)(bv[EC_P_FX_MACHINE] + 0.5f), o);
}

int ec_settings_items_bus(int cat, int bus, ec_page_t *o)
{
    memset(o, 0, sizeof *o);
    o->title = "SETUP";
    if (!bus || bus > 3 || cat == EC_CAT_MASTER || cat == EC_CAT_FILE || cat == EC_CAT_PAT ||
        (bus == 3 && cat == EC_CAT_SRC))
        return 0;
    o->busv = bus;
    P(o, EC_P_FX_MACHINE, bus == 1 ? "SEND 1 FX" : bus == 2 ? "SEND 2 FX" : "MASTER FX");
    return 1;
}

void ec_page_items(int cat, int page, const float *v, int bus, ec_page_t *o)
{
    memset(o, 0, sizeof *o);
    o->title = "";
    if (bus && cat != EC_CAT_MASTER && cat != EC_CAT_FILE && cat != EC_CAT_PAT) {
        if (bus == 3 && cat == EC_CAT_SRC) {   /* the master bus's SOURCE is the mix overview */
            o->title = "MIX";
            P(o, EC_P_AMP, 0), P(o, EC_P_PAN, 0), P(o, EC_P_MUTE, 0), G(o, EC_G_BPM, "BPM");
            return;
        }
        o->title = bus == 1 ? "SEND 1" : bus == 2 ? "SEND 2" : "MASTER";
        o->busv = bus;                       /* E4: filled by ec_page_items_bus */
        o->kind = EC_PV_FX;
        return;
    }
    switch (cat) {
    case EC_CAT_TRIG:
        if (page == 0) {
            o->title = "TRIG";
            P(o, EC_P_PITCH, "NOTE"), P(o, EC_P_LENGTH, 0), P(o, EC_P_VELOCITY, 0), P(o, -1, 0);
            P(o, EC_P_ENV_RESET, 0), P(o, EC_P_LFO_RESET, 0), P(o, EC_P_FILTER_RESET, 0);
        } else if (page == 1) {
            o->title = "COND";
            P(o, EC_P_TRIG_CHANCE, 0), P(o, EC_P_TRIG_CONDITION, 0), P(o, EC_P_TRIG_RATCHET, 0);
        } else if (is_slice(v)) {
            o->title = "SLICE";
            P(o, EC_P_SLICE_INDEX, 0), P(o, EC_P_SLICE_PLAY_MODE, 0), P(o, EC_P_SLICE_RATCHET, 0);
            P(o, EC_P_SLICE_REVERSE, 0);
        } else {
            o->title = "MTRIG";
            P(o, EC_P_TRIG_JUMP, 0), P(o, EC_P_TRIG_RELEASE, 0);
        }
        break;
    case EC_CAT_SRC:
        if (iv(v, EC_P_MACHINE) == EC_MACH_NEIGHBOR) {   /* E3: no source params (lib/machines/neighbor.lua) */
            static const char *const NT[4] = {"SOURCE", "MACHINE", "WARP", "RANGE"};
            o->title = NT[page & 3];
            break;
        }
        if (page == 0) {
            o->title = "SOURCE";
            if (is_slice(v)) {
                P(o, EC_P_PITCH, 0), P(o, EC_P_SLICE_PLAY_MODE, 0), P(o, EC_P_SLICE_INDEX, 0), P(o, EC_P_SLOT, 0);
                P(o, EC_P_SLICE_COUNT, 0), P(o, -1, 0), P(o, -1, 0), P(o, EC_P_SLICE_REVERSE, 0);
            } else {
                P(o, EC_P_PITCH, 0), P(o, EC_P_LOOP_START, 0), P(o, EC_P_LOOP_END, 0), P(o, EC_P_SLOT, 0);
                P(o, EC_P_XFADE, 0), P(o, EC_P_LOOP_REVERSE, 0);
            }
        } else if (page == 1) {
            o->title = "MACHINE";
            if (is_slice(v)) {
                P(o, EC_P_SLICE_COUNT, "SLIC"), P(o, EC_P_SLICE_PLAY_MODE, 0), P(o, EC_P_SLICE_REVERSE, 0);
                P(o, EC_P_SLICE_RATCHET, 0), P(o, EC_P_SLICE_SYNC, 0), P(o, EC_P_SLICE_RATE, 0);
            } else {
                P(o, EC_P_LOOP_REVERSE, "LREV"), P(o, EC_P_XFADE, 0), P(o, EC_P_TRIG_RELEASE, 0);
            }
        } else if (page == 2) {
            o->title = "WARP";
            warp_items(v, o);
        } else {
            o->title = "RANGE";
            P(o, EC_P_RANGE_START, 0), P(o, EC_P_RANGE_END, 0);
        }
        break;
    case EC_CAT_FILT:
        if (page == 0) {
            o->title = "FILTER";
            filter_items(v, o);
        } else if (page == 1) {
            o->title = "F.ENV";
            if (iv(v, EC_P_FENV_MODE) == EC_ENV_ADSR) {
                P(o, EC_P_FENV_ATTACK, 0), P(o, EC_P_FENV_DECAY, 0), P(o, EC_P_FENV_SUSTAIN, 0);
                P(o, EC_P_FENV_RELEASE, 0), P(o, EC_P_FENV_DEPTH, 0);
            } else {
                P(o, EC_P_FENV_ATTACK, 0), P(o, EC_P_FENV_HOLD, 0), P(o, EC_P_FENV_RELEASE, 0);
                P(o, EC_P_FENV_DEPTH, 0);
            }
        } else {
            o->title = "F.MIX";
            P(o, EC_P_FILTER_BALANCE, 0), P(o, EC_P_FILTER_MIX, 0);
        }
        break;
    case EC_CAT_AMP:
        o->title = "AMP";
        if (iv(v, EC_P_ENV_MODE) == EC_ENV_ADSR) {
            P(o, EC_P_ENV_ATTACK, 0), P(o, EC_P_ENV_DECAY, 0), P(o, EC_P_ENV_SUSTAIN, 0), P(o, EC_P_ENV_RELEASE, 0);
            P(o, EC_P_AMP, 0), P(o, EC_P_PAN, 0), P(o, EC_P_SEND1, 0), P(o, EC_P_SEND2, 0);
        } else {
            P(o, EC_P_ENV_ATTACK, 0), P(o, EC_P_ENV_HOLD, 0), P(o, EC_P_ENV_RELEASE, 0), P(o, EC_P_AMP, 0);
            P(o, EC_P_PAN, 0), P(o, EC_P_SEND1, 0), P(o, EC_P_SEND2, 0);
        }
        break;
    case EC_CAT_FX:                          /* E4 */
        if (page == 0) {
            o->title = "FX";
            o->kind = EC_PV_FX;
            fx_items(iv(v, EC_P_FX_MACHINE), o);
        } else {
            o->title = "SENDS";
            o->kind = EC_PV_SENDS;
            P(o, EC_P_SEND1, 0), P(o, EC_P_SEND2, 0), P(o, EC_P_SEND_TAP, 0);
        }
        break;
    case EC_CAT_MOD:                         /* E4: lib/pages/model.lua's MOD pages + the macro matrix */
        if (page == 0) {
            o->title = "LFO 1";
            o->kind = EC_PV_LFO1;
            P(o, EC_P_LFO1_DEST, 0), P(o, EC_P_LFO1_WAVE, 0), P(o, EC_P_LFO1_SPEED, 0), P(o, EC_P_LFO1_DEPTH, 0);
            P(o, EC_P_LFO1_MODE, 0);
        } else if (page == 1) {
            o->title = "LFO 2";
            o->kind = EC_PV_LFO2;
            P(o, EC_P_LFO2_DEST, 0), P(o, EC_P_LFO2_WAVE, 0), P(o, EC_P_LFO2_SPEED, 0), P(o, EC_P_LFO2_DEPTH, 0);
            P(o, EC_P_LFO2_MODE, 0);
        } else if (page == 2) {
            o->title = "MOD ENV";
            o->kind = EC_PV_MENV;
            P(o, EC_P_MENV_ATTACK, 0), P(o, EC_P_MENV_DECAY, 0), P(o, EC_P_MENV_SUSTAIN, 0);
            P(o, EC_P_MENV_RELEASE, 0), P(o, EC_P_MENV_DEST, 0), P(o, EC_P_MENV_DEPTH, 0);
        } else if (page == 3) {
            o->title = "VELOCITY";
            o->kind = EC_PV_VEL;
            P(o, EC_P_MVEL_DEST, 0), P(o, EC_P_MVEL_DEPTH, 0);
        } else if (page == 4) {
            o->title = "MACROS";
            o->kind = EC_PV_MACROS;
            P(o, EC_P_MACRO1, 0), P(o, EC_P_MACRO2, 0), P(o, EC_P_MACRO3, 0), P(o, EC_P_MACRO4, 0);
        } else {
            static const char *const T[4] = {"MACRO 1", "MACRO 2", "MACRO 3", "MACRO 4"};
            const int m = page - 5 < 0 ? 0 : page - 5 > 3 ? 3 : page - 5;
            int d;
            o->title = T[m];
            o->kind = EC_PV_MACRO;
            P(o, EC_P_MACRO1 + m, "VAL");
            for (d = 0; d < 5; d++)
                P(o, EC_P_MACRO_DEPTH + m * 5 + d, 0);
        }
        break;
    case EC_CAT_MASTER:
        if (page == 0) {
            o->title = "MIX";
            P(o, EC_P_AMP, 0), P(o, EC_P_PAN, 0), P(o, EC_P_MUTE, 0), G(o, EC_G_BPM, "BPM");
        } else {
            o->title = "MASTER";
            G(o, EC_G_BPM, "BPM"), G(o, EC_G_SWING, "SWNG"), G(o, EC_G_GLEN, "GLEN"), G(o, EC_G_RTN, "RTN");
        }
        break;
    case EC_CAT_FILE:
        if (page == 1) {   /* E3: the razor editor (Display.dc.html mode "razor", Razor.dc.html) */
            o->title = "RAZOR";
            Z(o, EC_RZ_START), Z(o, EC_RZ_END), Z(o, EC_RZ_PLAY), Z(o, EC_RZ_ZOOM);
            Z(o, EC_RZ_SNAP), Z(o, EC_RZ_CHOKE), Z(o, EC_RZ_PITCH), Z(o, EC_RZ_SENS);
            break;
        }
        o->title = "SAMPLE";
        S(o, EC_S_BPM, "BPM"), S(o, EC_S_STEPS, "STEP"), S(o, EC_S_GAIN, "GAIN"), P(o, EC_P_SLOT, 0);
        S(o, EC_S_TRIM_START, "T-ST"), S(o, EC_S_TRIM_END, "T-EN");
        break;
    case EC_CAT_PAT:
        o->title = "PATTERN";
        P(o, EC_P_PATTERN_STEPS, 0), P(o, EC_P_RATE, 0), G(o, EC_G_GLEN, "GLEN"), G(o, EC_G_QUANT, "QUANT");
        break;
    default: break;
    }
}

int ec_settings_items(int cat, const float *v, ec_page_t *o)
{
    (void)v;
    memset(o, 0, sizeof *o);
    o->title = "SETUP";
    switch (cat) {
    case EC_CAT_SRC:
        P(o, EC_P_MACHINE, 0);
        P(o, EC_P_MODE, 0), stopped_only(o);
        P(o, EC_P_LOOP_DIVISION, 0), P(o, EC_P_TRIG_POLY, 0);
        break;
    case EC_CAT_FILT:
        P(o, EC_P_FILTER_MACHINE, 0), stopped_only(o);
        P(o, EC_P_FENV_MODE, 0);
        P(o, EC_P_FILTER_MIX_MODE, 0), stopped_only(o);
        break;
    case EC_CAT_AMP:
        P(o, EC_P_ENV_MODE, 0), P(o, EC_P_ENV_RANGE, 0), P(o, EC_P_PORTAMENTO, 0), P(o, EC_P_SLICE_HOLD_TO_STEP, 0);
        P(o, EC_P_SLICE_HOLD, 0);
        break;
    case EC_CAT_FX:                          /* E4: the insert machine lives behind FN (a swap respawns the FX) */
        P(o, EC_P_FX_MACHINE, "INSERT FX"), P(o, EC_P_SEND_TAP, "SEND TAP");
        break;
    case EC_CAT_TRIG: G(o, EC_G_SWING, "SWING"); break;
    case EC_CAT_PAT: G(o, EC_G_QUANT, "QUANT"), G(o, EC_G_GLEN, "GLEN"); break;
    case EC_CAT_MASTER: G(o, EC_G_RTN, "RTN"), G(o, EC_G_BPM, "BPM"); break;
    default: return 0;
    }
    return 1;
}
