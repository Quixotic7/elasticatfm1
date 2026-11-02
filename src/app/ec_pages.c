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
static void X(ec_page_t *o, int id, const char *label)      /* E5 items */
{
    if (o->n < EC_PAGE_ITEMS) {
        o->it[o->n].kind = EC_IT_E5;
        o->it[o->n].id = (uint8_t)id;
        o->it[o->n].flags = 0;
        o->it[o->n].label = label;
        o->n++;
    }
}
static void stopped_only(ec_page_t *o) { o->it[o->n - 1].flags |= EC_IF_STOPPED; }

int ec_cat_pages(int cat, const float *v)
{
    (void)v;
    switch (cat) {
    case EC_CAT_TRIG: return 3;
    case EC_CAT_SRC: return 4;
    case EC_CAT_FILT: return 3;
    case EC_CAT_FX: return 2;
    case EC_CAT_MOD: return 2;
    case EC_CAT_MASTER: return 3;          /* E5: + SCENE */
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
    default:                                   /* the modes not ported yet play the tempo_varispeed reader */
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
        o->e4 = 1;
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
        if (page == 0) {
            o->title = "SOURCE";
            if (is_slice(v)) {
                P(o, EC_P_PITCH, 0), P(o, EC_P_SLICE_PLAY_MODE, 0), P(o, EC_P_SLICE_INDEX, 0), P(o, EC_P_SLOT, 0);
                P(o, EC_P_SLICE_COUNT, 0), X(o, EC_X_CHOKE, "CHOK"), X(o, EC_X_MUTE, "SMUT");
                P(o, EC_P_SLICE_REVERSE, 0);
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
    case EC_CAT_FX:
        o->e4 = 1;
        if (page == 0) {
            o->title = "FX";
            P(o, EC_P_FX_MACHINE, 0), P(o, EC_P_FX_DRIVE, 0), P(o, EC_P_FX_MIX, 0);
        } else {
            o->title = "SENDS";
            P(o, EC_P_SEND1, 0), P(o, EC_P_SEND2, 0), P(o, EC_P_SEND_TAP, 0);
        }
        break;
    case EC_CAT_MOD:
        o->e4 = 1;
        if (page == 0) {
            o->title = "LFO 1";
            P(o, EC_P_LFO1_DEST, 0), P(o, EC_P_LFO1_WAVE, 0), P(o, EC_P_LFO1_SPEED, 0), P(o, EC_P_LFO1_DEPTH, 0);
            P(o, EC_P_LFO1_MODE, 0);
        } else {
            o->title = "LFO 2";
            P(o, EC_P_LFO2_DEST, 0), P(o, EC_P_LFO2_WAVE, 0), P(o, EC_P_LFO2_SPEED, 0), P(o, EC_P_LFO2_DEPTH, 0);
            P(o, EC_P_LFO2_MODE, 0);
        }
        break;
    case EC_CAT_MASTER:
        if (page == 0) {
            o->title = "MIX";
            P(o, EC_P_AMP, 0), P(o, EC_P_PAN, 0), P(o, EC_P_MUTE, 0), G(o, EC_G_BPM, "BPM");
        } else if (page == 1) {
            o->title = "MASTER";
            G(o, EC_G_BPM, "BPM"), G(o, EC_G_SWING, "SWNG"), G(o, EC_G_GLEN, "GLEN"), G(o, EC_G_RTN, "RTN");
        } else {
            o->title = "SCENE";                /* E5: the A/B crossfader */
            X(o, EC_X_XFD, "XFD");
        }
        break;
    case EC_CAT_FILE:
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
    case EC_CAT_TRIG: G(o, EC_G_SWING, "SWING"); break;
    case EC_CAT_PAT: G(o, EC_G_QUANT, "QUANT"), G(o, EC_G_GLEN, "GLEN"); break;
    case EC_CAT_MASTER:
        G(o, EC_G_RTN, "RTN"), G(o, EC_G_BPM, "BPM"), X(o, EC_X_GBPM, "GBPM"), X(o, EC_X_NAME, "NAME");   /* E5 */
        break;
    default: return 0;
    }
    return 1;
}
