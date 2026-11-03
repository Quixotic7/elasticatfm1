/* SPDX-License-Identifier: GPL-3.0-only */
/* E4: the FX and MOD params -> engine units (lib/elasticat.lua param_xforms), their real-unit display
 * (ID_FORMATTERS), and the FX pages' cells per machine (the lib/fx_modes display order). */
#include <math.h>
#include <stdio.h>

#include "ec_mtx.h"
#include "ec_params.h"

static int iv(const float *v, int id) { return (int)floorf(v[id] + 0.5f); }
static float amt(const float *v, int id) { return ec_xf_amount(v[id]); }
static float bip(const float *v, int id) { return ec_xf_bipolar(v[id]); }
static int clampi(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }

static const float DELAY_BEATS[16] = {0.125f, 1.f / 6, 0.25f, 1.f / 3, 0.375f, 0.5f, 2.f / 3, 0.75f,
                                      1.f,    4.f / 3, 1.5f, 2.f,     3.f,    4.f,  8.f,     16.f};
static const float MOD_BEATS[10] = {32, 16, 8, 4, 2, 1, 0.5f, 0.25f, 0.125f, 0.0625f};

static float lofi_bits(float v) { return 1.f + (v / 127.f) * 23.f; }
static float lofi_hz(float v) { return 1000.f * powf(48.f, v / 127.f); }

const char *ec_fx_short_name(int m)
{
    return m >= 1 && m <= EC_PARAMS[EC_P_FX_MACHINE].nopt ? EC_PARAMS[EC_P_FX_MACHINE].opts[m - 1] : "?";
}

void ec_params_fx(const float *v, ec_fx_params_t *f)
{
    f->machine = clampi(iv(v, EC_P_FX_MACHINE), 1, EC_FX_COUNT);
    f->mix = amt(v, EC_P_FX_MIX);
    f->drive = amt(v, EC_P_FX_DRIVE);
    f->wet = amt(v, EC_P_FX_WET);            /* 0.6.1 */
    f->delay_beats = DELAY_BEATS[clampi(iv(v, EC_P_DELAY_TIME), 1, 15) - 1];
    f->delay_feedback = amt(v, EC_P_DELAY_FEEDBACK);
    f->delay_tone = amt(v, EC_P_DELAY_TONE);
    f->reverb_size = amt(v, EC_P_REVERB_SIZE);
    f->reverb_damp = amt(v, EC_P_REVERB_DAMP);
    f->lofi_bits = lofi_bits(v[EC_P_LOFI_BITS]);
    f->lofi_rate = lofi_hz(v[EC_P_LOFI_RATE]);
    f->comp_thresh = amt(v, EC_P_COMP_THRESH);
    f->comp_ratio = amt(v, EC_P_COMP_RATIO);
    f->comp_attack = amt(v, EC_P_COMP_ATTACK);
    f->comp_release = amt(v, EC_P_COMP_RELEASE);
    f->comp_makeup = amt(v, EC_P_COMP_MAKEUP);
    f->destroy_warble = amt(v, EC_P_DESTROY_WARBLE);
    f->destroy_fold = amt(v, EC_P_DESTROY_FOLD);
    f->destroy_sat = amt(v, EC_P_DESTROY_SAT);
    f->destroy_crush = amt(v, EC_P_DESTROY_CRUSH);
    f->destroy_srr = amt(v, EC_P_DESTROY_SRR);
    f->destroy_tone = amt(v, EC_P_DESTROY_TONE);
    f->destroy_level = amt(v, EC_P_DESTROY_LEVEL);
    f->echo_beats = DELAY_BEATS[clampi(iv(v, EC_P_ECHO_TIME), 1, 15) - 1];
    f->echo_offset = bip(v, EC_P_ECHO_OFFSET);
    f->echo_mode = clampi(iv(v, EC_P_ECHO_MODE) - 1, 0, 1);
    f->echo_tone = amt(v, EC_P_ECHO_TONE);
    f->echo_wobble = amt(v, EC_P_ECHO_WOBBLE);
    f->bh_gravity = amt(v, EC_P_BH_GRAVITY);
    f->bh_mod = amt(v, EC_P_BH_MOD);
    f->bh_low = amt(v, EC_P_BH_LOW);
    f->bh_predelay = amt(v, EC_P_BH_PREDELAY);
    f->modfx_rate = amt(v, EC_P_MODFX_RATE);
    f->modfx_depth = amt(v, EC_P_MODFX_DEPTH);
    f->modfx_tone = amt(v, EC_P_MODFX_TONE);
    f->chorus_width = amt(v, EC_P_CHORUS_WIDTH);
    f->flanger_feedback = bip(v, EC_P_FLANGER_FEEDBACK);
    f->phaser_center = amt(v, EC_P_PHASER_CENTER);
    f->phaser_stages = clampi(iv(v, EC_P_PHASER_STAGES) - 1, 0, 7);
    f->phaser_feedback = amt(v, EC_P_PHASER_FEEDBACK);
    f->eq_low = bip(v, EC_P_EQ_LOW);
    f->eq_mid = bip(v, EC_P_EQ_MID);
    f->eq_high = bip(v, EC_P_EQ_HIGH);
    f->eq_xlow = amt(v, EC_P_EQ_XLOW);
    f->eq_xhi = amt(v, EC_P_EQ_XHI);
    f->duck_amount = amt(v, EC_P_DUCK_AMOUNT);
    f->duck_sens = amt(v, EC_P_DUCK_SENS);
    f->duck_attack = amt(v, EC_P_DUCK_ATTACK);
    f->duck_release = amt(v, EC_P_DUCK_RELEASE);
    f->tape_wow = amt(v, EC_P_TAPE_WOW);
    f->tape_flutter = amt(v, EC_P_TAPE_FLUTTER);
    f->tape_sat = amt(v, EC_P_TAPE_SAT);
    f->cass_noise = amt(v, EC_P_CASS_NOISE);
    f->cass_crackle = amt(v, EC_P_CASS_CRACKLE);
    f->cass_drop = amt(v, EC_P_CASS_DROP);
    f->cass_tone = amt(v, EC_P_CASS_TONE);
    f->motion_width = bip(v, EC_P_MOTION_WIDTH);
    f->motion_beats = DELAY_BEATS[clampi(iv(v, EC_P_MOTION_RATE), 1, 16) - 1];
    f->motion_trem = amt(v, EC_P_MOTION_TREM);
    f->motion_pan = amt(v, EC_P_MOTION_PAN);
    f->motion_shape = clampi(iv(v, EC_P_MOTION_SHAPE) - 1, 0, 2);
    f->rings_mode = clampi(iv(v, EC_P_RINGS_MODE) - 1, 0, 1);
    f->rings_freq = bip(v, EC_P_RINGS_FREQ);
    f->rings_fine = bip(v, EC_P_RINGS_FINE);
    f->rings_feedback = amt(v, EC_P_RINGS_FEEDBACK);
    f->limit_gain = amt(v, EC_P_LIMIT_GAIN);
    f->limit_ceil = amt(v, EC_P_LIMIT_CEIL);
    f->limit_release = amt(v, EC_P_LIMIT_RELEASE);
}

/* ------------------------------------------------------------------------------------- 0.6.1: INS 2 --- */
/* each machine's params in the layout's FX/INSERT order (design/layout/elasticat.json, 0.6.0), the MIX left out.
 * NEVER reorder: a saved INS 2's P1..P8 are these by index. DESTROY has DRIVE (fx_drive) where the others have MIX */
static const uint8_t INS2_MAP[EC_FX_COUNT + 1][EC_INS2_NP] = {
    [EC_FX_DELAY] = {EC_P_DELAY_TIME, EC_P_DELAY_FEEDBACK, EC_P_DELAY_TONE},
    [EC_FX_REVERB] = {EC_P_REVERB_SIZE, EC_P_REVERB_DAMP},
    [EC_FX_LOFI] = {EC_P_LOFI_BITS, EC_P_LOFI_RATE},
    [EC_FX_COMP] = {EC_P_COMP_THRESH, EC_P_COMP_RATIO, EC_P_COMP_ATTACK, EC_P_COMP_RELEASE, EC_P_COMP_MAKEUP},
    [EC_FX_DESTROY] = {EC_P_DESTROY_CRUSH, EC_P_DESTROY_SRR, EC_P_DESTROY_WARBLE, EC_P_FX_DRIVE, EC_P_DESTROY_FOLD,
                       EC_P_DESTROY_SAT, EC_P_DESTROY_TONE, EC_P_DESTROY_LEVEL},
    [EC_FX_ECHO] = {EC_P_ECHO_TIME, EC_P_ECHO_OFFSET, EC_P_DELAY_FEEDBACK, EC_P_ECHO_MODE, EC_P_ECHO_TONE,
                    EC_P_ECHO_WOBBLE},
    [EC_FX_BLACKHOLE] = {EC_P_REVERB_SIZE, EC_P_BH_GRAVITY, EC_P_BH_MOD, EC_P_BH_LOW, EC_P_REVERB_DAMP,
                         EC_P_BH_PREDELAY},
    [EC_FX_CHORUS] = {EC_P_MODFX_RATE, EC_P_MODFX_DEPTH, EC_P_CHORUS_WIDTH, EC_P_MODFX_TONE},
    [EC_FX_FLANGER] = {EC_P_MODFX_RATE, EC_P_MODFX_DEPTH, EC_P_FLANGER_FEEDBACK, EC_P_MODFX_TONE},
    [EC_FX_PHASER] = {EC_P_MODFX_RATE, EC_P_MODFX_DEPTH, EC_P_PHASER_CENTER, EC_P_PHASER_STAGES,
                      EC_P_PHASER_FEEDBACK},
    [EC_FX_DJ_EQ] = {EC_P_EQ_LOW, EC_P_EQ_MID, EC_P_EQ_HIGH, EC_P_EQ_XLOW, EC_P_EQ_XHI},
    [EC_FX_DUCK] = {EC_P_DUCK_AMOUNT, EC_P_DUCK_SENS, EC_P_DUCK_ATTACK, EC_P_DUCK_RELEASE},
    [EC_FX_TAPE_ECHO] = {EC_P_ECHO_TIME, EC_P_DELAY_FEEDBACK, EC_P_TAPE_WOW, EC_P_TAPE_FLUTTER, EC_P_TAPE_SAT,
                         EC_P_ECHO_TONE},
    [EC_FX_CASSETTE] = {EC_P_TAPE_WOW, EC_P_TAPE_FLUTTER, EC_P_CASS_NOISE, EC_P_CASS_CRACKLE, EC_P_CASS_DROP,
                        EC_P_CASS_TONE},
    [EC_FX_MOTION] = {EC_P_MOTION_WIDTH, EC_P_MOTION_RATE, EC_P_MOTION_TREM, EC_P_MOTION_PAN, EC_P_MOTION_SHAPE},
    [EC_FX_RINGS] = {EC_P_RINGS_MODE, EC_P_RINGS_FREQ, EC_P_RINGS_FINE, EC_P_RINGS_FEEDBACK, EC_P_MODFX_TONE},
    [EC_FX_LIMIT] = {EC_P_LIMIT_GAIN, EC_P_LIMIT_CEIL, EC_P_LIMIT_RELEASE},
};

int ec_ins2_param(int m, int k)
{
    if (m < 1 || m > EC_FX_COUNT || k < 0 || k >= EC_INS2_NP || !INS2_MAP[m][k])
        return -1;
    return INS2_MAP[m][k];
}

int ec_ins2_count(int m)
{
    int k = 0;
    while (ec_ins2_param(m, k) >= 0)
        k++;
    return k;
}

int ec_ins2_index(int m, int id)
{
    int k;
    for (k = 0; k < EC_INS2_NP; k++)
        if (ec_ins2_param(m, k) == id)
            return k;
    return -1;
}

int ec_fx_mix_id(int m) { return ec_fx_has_mix(m) || m == EC_FX_NONE ? EC_P_FX_MIX : EC_P_FX_WET; }

int ec_param_def_id(const float *v, int id)
{
    if (id == EC_P_INS2_MIX)
        return ec_fx_mix_id(v ? iv(v, EC_P_INS2_MACHINE) : EC_FX_NONE);
    if (id >= EC_P_INS2_P1 && id < EC_P_INS2_P1 + EC_INS2_NP)
        return v ? ec_ins2_param(iv(v, EC_P_INS2_MACHINE), id - EC_P_INS2_P1) : -1;
    return id;
}

void ec_ins2_reset(float *v)
{
    const int m = iv(v, EC_P_INS2_MACHINE);
    int k;
    v[EC_P_INS2_MIX] = EC_PARAMS[ec_fx_mix_id(m)].def;
    for (k = 0; k < EC_INS2_NP; k++) {
        const int id = ec_ins2_param(m, k);
        v[EC_P_INS2_P1 + k] = id >= 0 ? EC_PARAMS[id].def : EC_PARAMS[EC_P_INS2_P1 + k].def;
    }
}

void ec_params_fx2(const float *v, ec_fx_params_t *f)
{
    float w[EC_NP];
    const int m = clampi(iv(v, EC_P_INS2_MACHINE), 1, EC_FX_COUNT);
    int id, k;
    for (id = 0; id < EC_NP; id++)           /* INS 1's own values must not leak in: every FX param at its default */
        w[id] = (EC_PARAMS[id].flags & EC_PF_FX) ? EC_PARAMS[id].def : v[id];
    w[EC_P_FX_MACHINE] = (float)m;
    w[EC_P_FX_MIX] = w[EC_P_FX_WET] = v[EC_P_INS2_MIX];
    for (k = 0; k < EC_INS2_NP; k++) {
        const int id2 = ec_ins2_param(m, k);
        if (id2 >= 0)
            w[id2] = ec_param_clamp(id2, v[EC_P_INS2_P1 + k]);
    }
    ec_params_fx(w, f);
}

void ec_params_mod(const float *v, ec_mod_params_t *m)
{
    static const int LFO[2][4] = {{EC_P_LFO1_WAVE, EC_P_LFO1_SPEED, EC_P_LFO1_DEPTH, EC_P_LFO1_MODE},
                                  {EC_P_LFO2_WAVE, EC_P_LFO2_SPEED, EC_P_LFO2_DEPTH, EC_P_LFO2_MODE}};
    const int er = iv(v, EC_P_ENV_RANGE);
    int i;
    for (i = 0; i < 2; i++) {
        m->lfo[i].wave = clampi(iv(v, LFO[i][0]) - 1, 0, 5);
        m->lfo[i].beats = MOD_BEATS[clampi(iv(v, LFO[i][1]), 1, 10) - 1];
        m->lfo[i].depth = bip(v, LFO[i][2]);
        m->lfo[i].mode = clampi(iv(v, LFO[i][3]) - 1, 0, 3);
    }
    m->menv_attack = ec_xf_env_seconds(v[EC_P_MENV_ATTACK], er);
    m->menv_decay = ec_xf_env_seconds(v[EC_P_MENV_DECAY], er);
    m->menv_sustain = amt(v, EC_P_MENV_SUSTAIN);
    m->menv_release = ec_xf_env_seconds(v[EC_P_MENV_RELEASE], er);
    m->menv_depth = bip(v, EC_P_MENV_DEPTH);
    m->mvel_depth = bip(v, EC_P_MVEL_DEPTH);
    for (i = 0; i < 4; i++)
        m->macro_base[i] = v[EC_P_MACRO1 + i] / 127.0f;
    for (i = 0; i < EC_MOD_SLOTS; i++) {    /* 0.5.0: the matrix (ec_mtx.h) */
        const int live = ec_mtx_live(v, i);
        m->slot[i].src = live ? ec_mtx_src(v, i) : 0;
        m->slot[i].bus = live ? ec_mtx_bus(ec_mtx_dst(v, i)) : -1;
        m->slot[i].depth = live ? ec_mtx_depth(v, i) : 0.0f;
    }
}

/* SC linexp with clip (in 0.001..1 or 0..1) */
static float linexp(float x, float lo_in, float a, float b)
{
    if (x <= lo_in)
        return a;
    if (x > 1)
        x = 1;
    return a * powf(b / a, (x - lo_in) / (1.0f - lo_in));
}

static void db(float d, char *out, int cap)
{
    if (d > -0.05f && d < 0.05f)
        snprintf(out, (size_t)cap, "0DB");
    else if (fabsf(d) < 9.95f)
        snprintf(out, (size_t)cap, "%+.1fDB", (double)d);
    else
        snprintf(out, (size_t)cap, "%+dDB", (int)floorf(d + (d < 0 ? -0.5f : 0.5f)));
}
static void ms(float s, char *out, int cap)
{
    const float m = s * 1000.0f;
    if (m < 9.95f)
        snprintf(out, (size_t)cap, "%.1fMS", (double)m);
    else
        snprintf(out, (size_t)cap, "%dMS", (int)floorf(m + 0.5f));
}
static void hz(float h, char *out, int cap)
{
    if (h < 1000.0f)
        snprintf(out, (size_t)cap, "%d", (int)floorf(h + 0.5f));
    else if (h < 9950.0f)
        snprintf(out, (size_t)cap, "%.1fK", (double)(h / 1000.0f));
    else
        snprintf(out, (size_t)cap, "%dK", (int)floorf(h / 1000.0f + 0.5f));
}

void ec_param_format_unit(int id, float x, char *out, int cap)
{
    const float a = x / 127.0f;
    switch (id) {
    case EC_P_LOFI_BITS: {
        const float b = lofi_bits(x);
        if (fabsf(b - floorf(b + 0.5f)) < 0.05f)
            snprintf(out, (size_t)cap, "%dBIT", (int)floorf(b + 0.5f));
        else
            snprintf(out, (size_t)cap, "%.1f", (double)b);
    } break;
    case EC_P_LOFI_RATE: hz(lofi_hz(x), out, cap); break;
    case EC_P_COMP_THRESH: db(20.0f * log10f(0.02f + a * 0.98f), out, cap); break;
    case EC_P_COMP_RATIO: {
        const float r = 1.0f / linexp(a, 0.001f, 1.0f, 0.05f);
        if (r < 9.95f)
            snprintf(out, (size_t)cap, "%.1f:1", (double)r);
        else
            snprintf(out, (size_t)cap, "%d:1", (int)floorf(r + 0.5f));
    } break;
    case EC_P_COMP_ATTACK: ms(linexp(a, 0.001f, 0.0005f, 0.1f), out, cap); break;
    case EC_P_COMP_RELEASE: ms(linexp(a, 0.001f, 0.02f, 0.8f), out, cap); break;
    case EC_P_COMP_MAKEUP: db(20.0f * log10f(1.0f + a * 3.0f), out, cap); break;
    case EC_P_EQ_XLOW: hz(linexp(a, 0.0f, 80.0f, 500.0f), out, cap); break;
    case EC_P_EQ_XHI: hz(linexp(a, 0.0f, 1000.0f, 8000.0f), out, cap); break;
    case EC_P_DUCK_AMOUNT: {
        const float g = 1.0f - a;
        db(g > 0.01f ? 20.0f * log10f(g) : -40.0f, out, cap);
    } break;
    case EC_P_DUCK_ATTACK: ms(linexp(a, 0.0f, 0.001f, 0.05f), out, cap); break;
    case EC_P_DUCK_RELEASE: ms(linexp(a, 0.0f, 0.05f, 1.2f), out, cap); break;
    case EC_P_LIMIT_GAIN: db(a * 18.0f, out, cap); break;
    case EC_P_LIMIT_CEIL: db(-12.0f + a * 12.0f, out, cap); break;
    case EC_P_LIMIT_RELEASE: ms(linexp(a, 0.0f, 0.002f, 0.1f), out, cap); break;
    default: snprintf(out, (size_t)cap, "%d", (int)floorf(x + 0.5f)); break;
    }
}

