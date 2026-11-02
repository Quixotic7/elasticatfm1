/* SPDX-License-Identifier: GPL-3.0-only */
/* E4: the FX and MOD params -> engine units (lib/elasticat.lua param_xforms), their real-unit display
 * (ID_FORMATTERS), and the FX pages' cells per machine (the lib/fx_modes display order). */
#include <math.h>
#include <stdio.h>

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

void ec_params_mod(const float *v, ec_mod_params_t *m)
{
    static const int LFO[2][5] = {{EC_P_LFO1_DEST, EC_P_LFO1_WAVE, EC_P_LFO1_SPEED, EC_P_LFO1_DEPTH, EC_P_LFO1_MODE},
                                  {EC_P_LFO2_DEST, EC_P_LFO2_WAVE, EC_P_LFO2_SPEED, EC_P_LFO2_DEPTH, EC_P_LFO2_MODE}};
    const int er = iv(v, EC_P_ENV_RANGE);
    int i, k;
    for (i = 0; i < 2; i++) {
        m->lfo[i].dest = clampi(iv(v, LFO[i][0]) - 1, 0, 9);
        m->lfo[i].wave = clampi(iv(v, LFO[i][1]) - 1, 0, 5);
        m->lfo[i].beats = MOD_BEATS[clampi(iv(v, LFO[i][2]), 1, 10) - 1];
        m->lfo[i].depth = bip(v, LFO[i][3]);
        m->lfo[i].mode = clampi(iv(v, LFO[i][4]) - 1, 0, 3);
    }
    m->menv_dest = clampi(iv(v, EC_P_MENV_DEST) - 1, 0, 9);
    m->menv_attack = ec_xf_env_seconds(v[EC_P_MENV_ATTACK], er);
    m->menv_decay = ec_xf_env_seconds(v[EC_P_MENV_DECAY], er);
    m->menv_sustain = amt(v, EC_P_MENV_SUSTAIN);
    m->menv_release = ec_xf_env_seconds(v[EC_P_MENV_RELEASE], er);
    m->menv_depth = bip(v, EC_P_MENV_DEPTH);
    m->mvel_dest = clampi(iv(v, EC_P_MVEL_DEST) - 1, 0, 9);
    m->mvel_depth = bip(v, EC_P_MVEL_DEPTH);
    for (i = 0; i < 4; i++) {
        m->macro_base[i] = v[EC_P_MACRO1 + i] / 127.0f;
        for (k = 0; k < EC_MOD_NDEST; k++)
            m->macro_depth[i][k] = bip(v, EC_P_MACRO_DEPTH + i * 5 + k);
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

int ec_fx_page_items(int m, int *ids, const char **lb)
{
    int n = 0, i;
#define I(id, l) (ids[n] = (id), lb[n] = (l), n++)
    for (i = 0; i < 8; i++)
        lb[i] = 0;
    switch (m) {
    case EC_FX_DELAY: I(EC_P_DELAY_TIME, 0), I(EC_P_DELAY_FEEDBACK, 0), I(EC_P_DELAY_TONE, 0), I(EC_P_FX_MIX, 0); break;
    case EC_FX_REVERB: I(EC_P_REVERB_SIZE, 0), I(EC_P_REVERB_DAMP, 0), I(EC_P_FX_MIX, 0); break;
    case EC_FX_LOFI: I(EC_P_LOFI_BITS, 0), I(EC_P_LOFI_RATE, 0), I(EC_P_FX_MIX, 0); break;
    case EC_FX_COMP:
        I(EC_P_COMP_THRESH, 0), I(EC_P_COMP_RATIO, 0), I(EC_P_COMP_ATTACK, 0), I(EC_P_COMP_RELEASE, 0);
        I(EC_P_COMP_MAKEUP, 0), I(EC_P_FX_MIX, 0);
        break;
    case EC_FX_DESTROY:
        I(EC_P_DESTROY_CRUSH, 0), I(EC_P_DESTROY_SRR, 0), I(EC_P_DESTROY_WARBLE, 0), I(EC_P_FX_DRIVE, 0);
        I(EC_P_DESTROY_FOLD, 0), I(EC_P_DESTROY_SAT, 0), I(EC_P_DESTROY_TONE, 0), I(EC_P_DESTROY_LEVEL, 0);
        break;
    case EC_FX_ECHO:
        I(EC_P_ECHO_TIME, 0), I(EC_P_ECHO_OFFSET, 0), I(EC_P_DELAY_FEEDBACK, 0), I(EC_P_ECHO_MODE, 0);
        I(EC_P_ECHO_TONE, 0), I(EC_P_ECHO_WOBBLE, 0), I(EC_P_FX_MIX, 0);
        break;
    case EC_FX_BLACKHOLE:
        I(EC_P_REVERB_SIZE, 0), I(EC_P_BH_GRAVITY, 0), I(EC_P_BH_MOD, 0), I(EC_P_BH_LOW, 0);
        I(EC_P_REVERB_DAMP, "HI"), I(EC_P_BH_PREDELAY, 0), I(EC_P_FX_MIX, 0);
        break;
    case EC_FX_CHORUS:
        I(EC_P_MODFX_RATE, 0), I(EC_P_MODFX_DEPTH, 0), I(EC_P_CHORUS_WIDTH, 0), I(EC_P_MODFX_TONE, 0);
        I(EC_P_FX_MIX, 0);
        break;
    case EC_FX_FLANGER:
        I(EC_P_MODFX_RATE, 0), I(EC_P_MODFX_DEPTH, 0), I(EC_P_FLANGER_FEEDBACK, 0), I(EC_P_MODFX_TONE, 0);
        I(EC_P_FX_MIX, 0);
        break;
    case EC_FX_PHASER:
        I(EC_P_MODFX_RATE, 0), I(EC_P_MODFX_DEPTH, 0), I(EC_P_PHASER_CENTER, 0), I(EC_P_PHASER_STAGES, 0);
        I(EC_P_PHASER_FEEDBACK, 0), I(EC_P_FX_MIX, 0);
        break;
    case EC_FX_DJ_EQ:
        I(EC_P_EQ_LOW, 0), I(EC_P_EQ_MID, 0), I(EC_P_EQ_HIGH, 0), I(EC_P_EQ_XLOW, 0), I(EC_P_EQ_XHI, 0);
        break;
    case EC_FX_DUCK: I(EC_P_DUCK_AMOUNT, 0), I(EC_P_DUCK_SENS, 0), I(EC_P_DUCK_ATTACK, 0), I(EC_P_DUCK_RELEASE, 0); break;
    case EC_FX_TAPE_ECHO:
        I(EC_P_ECHO_TIME, 0), I(EC_P_DELAY_FEEDBACK, 0), I(EC_P_TAPE_WOW, 0), I(EC_P_TAPE_FLUTTER, 0);
        I(EC_P_TAPE_SAT, 0), I(EC_P_ECHO_TONE, 0), I(EC_P_FX_MIX, 0);
        break;
    case EC_FX_CASSETTE:
        I(EC_P_TAPE_WOW, 0), I(EC_P_TAPE_FLUTTER, 0), I(EC_P_CASS_NOISE, 0), I(EC_P_CASS_CRACKLE, 0);
        I(EC_P_CASS_DROP, 0), I(EC_P_CASS_TONE, 0), I(EC_P_FX_MIX, 0);
        break;
    case EC_FX_MOTION:
        I(EC_P_MOTION_WIDTH, 0), I(EC_P_MOTION_RATE, 0), I(EC_P_MOTION_TREM, 0), I(EC_P_MOTION_PAN, 0);
        I(EC_P_MOTION_SHAPE, 0);
        break;
    case EC_FX_RINGS:
        I(EC_P_RINGS_MODE, 0), I(EC_P_RINGS_FREQ, 0), I(EC_P_RINGS_FINE, 0), I(EC_P_RINGS_FEEDBACK, 0);
        I(EC_P_MODFX_TONE, 0), I(EC_P_FX_MIX, 0);
        break;
    case EC_FX_LIMIT: I(EC_P_LIMIT_GAIN, 0), I(EC_P_LIMIT_CEIL, 0), I(EC_P_LIMIT_RELEASE, 0); break;
    default: break;
    }
#undef I
    return n;
}
