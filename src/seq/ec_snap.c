/* SPDX-License-Identifier: GPL-3.0-only */
/* 0.3.0: SHIFT + a value knob snaps (ec_snap.h). The tables are the norns script's own K1 + E2 / E3 snap lists
 * (lib/pages/model.lua, lib/machines, lib/warp_modes, lib/filter_modes, lib/fx_modes: `snaps = {..}`,
 * `fn_snap_multiple`); a param without one follows ChoralRoot's OPT snap (MANUAL.md "Knob speed and OPT snap"):
 * 0..127 to 0 16 32 .. 112 127, 0..128 and the bipolar ranges to steps of 8, semitones to octaves, values with
 * decimals to whole numbers (0..1 to quarters), a small range (up to 17 values) one step a click; lists never snap. */
#include <math.h>

#include "ec_snap.h"

#define N(a) (int)(sizeof(a) / sizeof(a)[0])

static const float S_PITCH[] = {-24, -12, -7, 0, 7, 12, 24};
static const float S_HARM[] = {-24, -12, -7, -5, -3, 0, 3, 4, 5, 7, 12, 24};
static const float S_Q127[] = {0, 32, 64, 96, 127};
static const float S_Q128[] = {0, 32, 64, 96, 128};
static const float S_QUART[] = {0, 0.25f, 0.5f, 0.75f, 1};
static const float S_REGION[] = {0, 8, 16, 32, 64, 96, 120, 128};
static const float S_XFADE[] = {0, 0.001f, 0.002f, 0.005f, 0.01f, 0.02f, 0.05f, 0.1f, 0.25f};
static const float S_SLOT[] = {1, 2, 4, 8, 16};
static const float S_P2_32[] = {1, 2, 4, 8, 16, 32};
static const float S_CHOPSTEPS[] = {1, 2, 4, 8, 16, 32, 64};
static const float S_CHOPENV[] = {0.0005f, 0.001f, 0.002f, 0.005f, 0.01f, 0.02f, 0.05f, 0.1f, 0.2f};
static const float S_CHOPHOLD[] = {0, 0.25f, 0.5f, 0.75f, 0.9f, 1};
static const float S_GSIZE[] = {0.005f, 0.01f, 0.02f, 0.04f, 0.08f, 0.16f, 0.32f};
static const float S_GDENS[] = {1, 2, 4, 8, 16, 32, 64};
static const float S_GJIT[] = {0, 0.005f, 0.01f, 0.02f, 0.05f, 0.1f, 0.25f};
static const float S_GSPEED[] = {0, 0.25f, 0.5f, 1, 2, 4};
static const float S_GRAND[] = {0, 0.1f, 0.25f, 0.5f, 1};
static const float S_HALF[] = {0, 0.5f, 1};
static const float S_WRATE[] = {0.125f, 0.25f, 0.5f, 0.75f, 1, 1.5f, 2, 3, 4, 8};
static const float S_SRATE[] = {0.125f, 0.25f, 0.5f, 1, 2, 4, 8};
static const float S_RATCH4[] = {1, 2, 3, 4};
static const float S_RATCH8[] = {1, 2, 3, 4, 6, 8};
static const float S_LEN[] = {4, 8, 16, 32, 48, 64, 96, 128};   /* 0.6.14: 8 pages */
static const float S_PRATE[] = {7, 13, 21, 22};
static const float S_TLEN[] = {0.25f, 0.5f, 1, 2, 4, 8, 16};
static const float S_CHANCE[] = {0, 25, 50, 75, 100};
static const float S_AMP[] = {0, 32, 64, 100, 127};
static const float S_LDIV[] = {2, 4, 8, 16, 32};
static const float S_OWIN[] = {0.005f, 0.01f, 0.02f, 0.04f, 0.08f, 0.16f, 0.32f};
static const float S_OWAN[] = {0, 0.005f, 0.01f, 0.02f, 0.05f, 0.1f};
static const float S_PVWIN[] = {0.005f, 0.01f, 0.02f, 0.05f, 0.1f, 0.2f, 0.5f, 1, 2};
static const float S_WTWIN[] = {2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64};
static const float S_WTCYC[] = {32, 64, 128, 256, 512, 1024, 2048, 4096};
static const float S_WTLRATE[] = {0, 0.25f, 0.5f, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8000};
static const float S_COMP_TH[] = {2, 6, 14, 30, 62, 127};
static const float S_COMP_RA[] = {0, 29, 59, 88, 127};
static const float S_COMP_AT[] = {17, 55, 72, 98, 127};
static const float S_COMP_RE[] = {32, 55, 79, 103, 127};
static const float S_COMP_MU[] = {0, 17, 42, 77, 126};
static const float S_DUCK_AM[] = {37, 63, 95, 114};
static const float S_DUCK_AT[] = {0, 52, 75, 104, 127};
static const float S_DUCK_RE[] = {0, 28, 64, 92, 120};
static const float S_LIM_G[] = {0, 21, 42, 64, 85, 127};
static const float S_LIM_C[] = {0, 64, 95, 116, 124, 127};
static const float S_BPM[] = {60, 80, 90, 100, 110, 120, 128, 136, 140, 160, 180};
static const float S_GAIN[] = {0, 0.5f, 1, 1.5f, 2, 3, 4};
static const float S_SWING[] = {50, 54, 58, 62, 67, 71, 75};

typedef struct {
    const float *v;
    int n;
    float mult;          /* > 0: multiples of this instead of a list (fn_snap_multiple) */
} snap_t;

static snap_t table(int id)
{
    snap_t s = {0, 0, 0};
#define L(a) (s.v = (a), s.n = N(a))
    switch (id) {
    case EC_P_PITCH:
    case EC_P_FORMANT_SHIFT: L(S_PITCH); break;
    case EC_P_HARM_INTERVAL:
    case EC_P_HARM_INTERVAL2:
    case EC_P_HARM_INTERVAL3: L(S_HARM); break;
    case EC_P_SLOT: L(S_SLOT); break;
    case EC_P_LOOP_START:
    case EC_P_LOOP_END: L(S_REGION); break;
    case EC_P_RANGE_START:
    case EC_P_RANGE_END: s.mult = 8; break;
    case EC_P_XFADE: L(S_XFADE); break;
    case EC_P_MODE_MACRO:
    case EC_P_PV_DISPERSION:
    case EC_P_SPECTRAL_BLUR:
    case EC_P_VELOCITY: L(S_QUART); break;
    case EC_P_CHOP_STEPS: L(S_CHOPSTEPS); break;
    case EC_P_CHOP_SLICE_LEN: s.mult = 1; break;
    case EC_P_CHOP_ATTACK:
    case EC_P_CHOP_RELEASE: L(S_CHOPENV); break;
    case EC_P_CHOP_HOLD: L(S_CHOPHOLD); break;
    case EC_P_GRAIN_SIZE: L(S_GSIZE); break;
    case EC_P_GRAIN_DENSITY: L(S_GDENS); break;
    case EC_P_GRAIN_JITTER: L(S_GJIT); break;
    case EC_P_GRAIN_SPEED: L(S_GSPEED); break;
    case EC_P_GRAIN_SPEED_RAND: L(S_GRAND); break;
    case EC_P_GRAIN_DIRECTION:
    case EC_P_FREEZE_AMOUNT: L(S_HALF); break;
    case EC_P_WARP_RATE: L(S_WRATE); break;
    case EC_P_SLICE_RATE: L(S_SRATE); break;
    case EC_P_SLICE_COUNT:
    case EC_P_SLICE_INDEX: L(S_P2_32); break;
    case EC_P_SLICE_RATCHET: L(S_RATCH4); break;
    case EC_P_TRIG_RATCHET: L(S_RATCH8); break;
    case EC_P_PATTERN_STEPS: L(S_LEN); break;
    case EC_P_RATE: L(S_PRATE); break;
    case EC_P_LENGTH: L(S_TLEN); break;
    case EC_P_TRIG_CHANCE: L(S_CHANCE); break;
    case EC_P_AMP: L(S_AMP); break;
    case EC_P_LOOP_DIVISION: L(S_LDIV); break;
    case EC_P_WSOLA_WINDOW: L(S_OWIN); break;
    case EC_P_WSOLA_SEARCH: L(S_OWAN); break;
    case EC_P_PV_WINDOW: L(S_PVWIN); break;
    case EC_P_WT_WINDOW: L(S_WTWIN); break;
    case EC_P_WT_CYCLE: L(S_WTCYC); break;
    case EC_P_WT_LFO_RATE: L(S_WTLRATE); break;
    case EC_P_WT_LFO_DEPTH:
    case EC_P_FILTER_MORPH:
    case EC_P_FILTER_BALANCE:
    case EC_P_LFO1_DEPTH:
    case EC_P_LFO2_DEPTH:
    case EC_P_ECHO_OFFSET:
    case EC_P_FLANGER_FEEDBACK:
    case EC_P_MOTION_WIDTH:
    case EC_P_EQ_LOW:
    case EC_P_EQ_MID:
    case EC_P_EQ_HIGH:
    case EC_P_RINGS_FREQ:
    case EC_P_RINGS_FINE: L(S_Q128); break;
    case EC_P_FILTER_CUTOFF:
    case EC_P_FILTER_RES:
    case EC_P_FILTER_DRIVE:
    case EC_P_FILTER_MIX:
    case EC_P_FX_DRIVE:
    case EC_P_FX_MIX:
    case EC_P_SEND1:
    case EC_P_SEND2:
    case EC_P_DELAY_FEEDBACK:
    case EC_P_DELAY_TONE:
    case EC_P_REVERB_SIZE:
    case EC_P_REVERB_DAMP:
    case EC_P_LOFI_BITS:
    case EC_P_LOFI_RATE:
    case EC_P_DUCK_SENS:
    case EC_P_LIMIT_RELEASE:
    case EC_P_DESTROY_WARBLE:
    case EC_P_DESTROY_FOLD:
    case EC_P_DESTROY_SAT:
    case EC_P_DESTROY_CRUSH:
    case EC_P_DESTROY_SRR:
    case EC_P_DESTROY_TONE:
    case EC_P_DESTROY_LEVEL:
    case EC_P_ECHO_TONE:
    case EC_P_ECHO_WOBBLE:
    case EC_P_BH_GRAVITY:
    case EC_P_BH_MOD:
    case EC_P_BH_LOW:
    case EC_P_BH_PREDELAY:
    case EC_P_MODFX_RATE:
    case EC_P_MODFX_DEPTH:
    case EC_P_MODFX_TONE:
    case EC_P_CHORUS_WIDTH:
    case EC_P_PHASER_CENTER:
    case EC_P_PHASER_FEEDBACK:
    case EC_P_EQ_XLOW:
    case EC_P_EQ_XHI:
    case EC_P_TAPE_WOW:
    case EC_P_TAPE_FLUTTER:
    case EC_P_TAPE_SAT:
    case EC_P_CASS_NOISE:
    case EC_P_CASS_CRACKLE:
    case EC_P_CASS_DROP:
    case EC_P_CASS_TONE:
    case EC_P_MOTION_TREM:
    case EC_P_MOTION_PAN:
    case EC_P_RINGS_FEEDBACK: L(S_Q127); break;
    case EC_P_COMP_THRESH: L(S_COMP_TH); break;
    case EC_P_COMP_RATIO: L(S_COMP_RA); break;
    case EC_P_COMP_ATTACK: L(S_COMP_AT); break;
    case EC_P_COMP_RELEASE: L(S_COMP_RE); break;
    case EC_P_COMP_MAKEUP: L(S_COMP_MU); break;
    case EC_P_DUCK_AMOUNT: L(S_DUCK_AM); break;
    case EC_P_DUCK_ATTACK: L(S_DUCK_AT); break;
    case EC_P_DUCK_RELEASE: L(S_DUCK_RE); break;
    case EC_P_LIMIT_GAIN: L(S_LIM_G); break;
    case EC_P_LIMIT_CEIL: L(S_LIM_C); break;
    default: break;
    }
#undef L
    return s;
}

/* one snap step from x towards det's sign through the list (the original's snap_value: the next value past x) */
float ec_snap_list(const float *v, int n, float x, int det)
{
    const float tol = 1e-4f * (fabsf(v[n - 1] - v[0]) + 1.0f);
    int i;
    if (n <= 0)
        return x;
    if (det > 0) {
        for (i = 0; i < n; i++)
            if (v[i] > x + tol)
                return v[i];
        return v[n - 1];
    }
    for (i = n - 1; i >= 0; i--)
        if (v[i] < x - tol)
            return v[i];
    return v[0];
}

float ec_snap_multiple(float x, int det, float m, float lo, float hi)
{
    float y = det > 0 ? (floorf(x / m + 1e-4f) + 1) * m : (ceilf(x / m - 1e-4f) - 1) * m;
    return y < lo ? lo : y > hi ? hi : y;
}

float ec_snap_range(float lo, float hi, float step, int fmt, float x, int det)
{
    const float range = hi - lo;
    if (range / (step > 0 ? step : 1) <= 17.0f)        /* a small range: one step a click */
        return x + (det > 0 ? step : -step) < lo ? lo : x + (det > 0 ? step : -step) > hi ? hi : x + (det > 0 ? step : -step);
    if (fmt == EC_PF_PITCH)                             /* semitones: octaves */
        return ec_snap_multiple(x, det, 12, lo, hi);
    if (lo == 0 && hi == 127) {
        static const float Q[] = {0, 16, 32, 48, 64, 80, 96, 112, 127};
        return ec_snap_list(Q, N(Q), x, det);
    }
    if (range <= 1.0001f)                               /* 0..1: quarters */
        return ec_snap_multiple(x, det, 0.25f * range, lo, hi);
    if (step < 1.0f && range <= 16.0f)                  /* decimals: whole numbers */
        return ec_snap_multiple(x, det, 1, lo, hi);
    if (range >= 120 && range <= 136)                   /* 0..128, bipolar: steps of 8 */
        return ec_snap_multiple(x, det, 8, lo, hi);
    if (range > 99 && range < 101)
        return ec_snap_multiple(x, det, 10, lo, hi);
    return ec_snap_multiple(x, det, floorf(range / 8.0f + 0.5f), lo, hi);
}

float ec_param_snap(int id, float x, int det)
{
    const ec_param_def_t *d = &EC_PARAMS[id];
    snap_t s;
    int k, n = det < 0 ? -det : det;
    if (d->kind != EC_PK_NUM)                           /* lists never snap: one item a click */
        return ec_param_nudge(id, x, det);
    if (d->fmt == EC_PF_MDEP) {                         /* 0.5.0: a matrix DEPTH: 0, +-25, +-50, +-100 */
        static const float Q[] = {-100, -50, -25, 0, 25, 50, 100};
        for (k = 0; k < n; k++)
            x = ec_snap_list(Q, N(Q), x, det);
        return x;
    }
    s = table(id);
    for (k = 0; k < n; k++) {
        if (s.n)
            x = ec_snap_list(s.v, s.n, x, det);
        else if (s.mult > 0)
            x = ec_snap_multiple(x, det, s.mult, d->min, d->max);
        else
            x = ec_snap_range(d->min, d->max, d->step, d->fmt, x, det);
    }
    return ec_param_clamp(id, x);
}

float ec_global_snap(int which, float x, int det)
{
    int k, n = det < 0 ? -det : det;
    for (k = 0; k < n; k++)
        switch (which) {
        case EC_SNAP_BPM: x = ec_snap_list(S_BPM, N(S_BPM), x, det); break;
        case EC_SNAP_GLEN: x = ec_snap_list(S_LEN, N(S_LEN), x, det); break;
        case EC_SNAP_SWING: x = ec_snap_list(S_SWING, N(S_SWING), x, det); break;
        case EC_SNAP_GAIN: x = ec_snap_list(S_GAIN, N(S_GAIN), x, det); break;
        case EC_SNAP_STEPS: x = ec_snap_multiple(x, det, 8, 1, 512); break;
        case EC_SNAP_XFD: x = ec_snap_list(S_Q128, N(S_Q128), x, det); break;
        default: break;
        }
    return x;
}
