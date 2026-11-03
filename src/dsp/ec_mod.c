/*
 * ec_mod.c -- E4: the per-track modulation (\elasticatMod), ec_mod.h.
 * Control rate = the engine's chunk (<= EC_KR samples): each source is
 * evaluated at the chunk start, the phases advance by the chunk length.
 */
#include "ec_internal.h"

void ec_mod_params_default(ec_mod_params_t *p)
{
    memset(p, 0, sizeof *p);
    for (int i = 0; i < 2; ++i) p->lfo[i].beats = 4.f;   /* "1 BAR" (option 4) */
    for (int k = 0; k < EC_MOD_SLOTS; ++k) p->slot[k].bus = -1;
    p->menv_attack = 0.0001f;
    p->menv_decay = 0.15f;
    p->menv_sustain = 100.f / 127.f;
    p->menv_release = 0.15f;
}

int ec_mod_needed(const ec_mod_params_t *p)
{
    for (int k = 0; k < EC_MOD_SLOTS; ++k)
        if (p->slot[k].src > 0 && p->slot[k].depth != 0.f) return 1;
    return 0;
}

/* a fresh "synth": the SC mod synth spawns when a depth turns non-zero */
static void mod_spawn(ec_mod_t *m)
{
    uint32_t rng = m->rng ? m->rng : 0x9E3779B9u;
    float vel = m->vel, live[3];
    memcpy(live, m->live, sizeof live);
    memset(m, 0, sizeof *m);
    m->rng = rng;
    m->vel = m->vel_lag = vel;
    for (int i = 0; i < 3; ++i) m->live[i] = m->live_lag[i] = live[i];
    ec_env_reset(&m->env);
    for (int i = 0; i < 2; ++i) m->rnd[i] = ec_frand2(&m->rng);
    m->alive = 1;
}

void ec_mod_init(ec_mod_t *m, uint32_t seed)
{
    memset(m, 0, sizeof *m);
    m->rng = seed | 1u;
    ec_env_reset(&m->env);
}

static void menv_spec(const ec_mod_params_t *p, ec_envspec_t *sp)
{
    sp->mode = EC_ENV_ADSR;
    sp->atk = p->menv_attack > 0.0001f ? p->menv_attack : 0.0001f;
    sp->dec = p->menv_decay > 0.0001f ? p->menv_decay : 0.0001f;
    sp->sus = ec_clampf(p->menv_sustain, 0.f, 1.f);
    sp->rel = p->menv_release > 0.0001f ? p->menv_release : 0.0001f;
    sp->hold = 0.0001f;
}

static float lfo_freq(float bpm, float beats)
{
    float b = beats > 0.0625f ? beats : 0.0625f;
    return ec_clampf((bpm > 1.f ? bpm : 1.f) / 60.f / b, 0.001f, 40.f);
}

/* the free-running wave at phase ph (lfoValue in the SynthDef) */
static float wave_at(int wave, float ph, float rnd)
{
    switch (wave) {
    case EC_LW_TRI: return 1.f - fabsf(ph * 2.f - 1.f) * 2.f;
    case EC_LW_SAW: return ph * 2.f - 1.f;
    case EC_LW_RSAW: return 1.f - ph * 2.f;
    case EC_LW_SQR: return ph < 0.5f ? 1.f : -1.f;
    case EC_LW_RAND: return rnd;
    default: return sinf(ph * 2.f * (float)EC_PI);
    }
}

static float lfo_value(const ec_mod_t *m, const ec_mod_params_t *p, int i)
{
    int mode = ec_clampi(p->lfo[i].mode, 0, 3), wave = ec_clampi(p->lfo[i].wave, 0, 5);
    if (mode == EC_LM_HOLD) return m->held[i];
    float ph = mode == EC_LM_ONE ? ec_clampf(m->sweep[i], 0.f, 1.f) : m->phase[i];
    return wave_at(wave, ph, m->rnd[i]);
}

void ec_mod_trig(ec_mod_t *m, const ec_mod_params_t *p, int lfo, int env, int32_t gate, float fs)
{
    if (!m->alive && ec_mod_needed(p)) mod_spawn(m);   /* (the synth spawns as soon as a depth is set) */
    if (!m->alive) return;
    if (lfo) {
        for (int i = 0; i < 2; ++i) {
            int mode = ec_clampi(p->lfo[i].mode, 0, 3);
            if (mode == EC_LM_TRIG) m->phase[i] = 0.f;
            else if (mode == EC_LM_ONE) m->sweep[i] = 0.f;
            else if (mode == EC_LM_HOLD)
                m->held[i] = wave_at(ec_clampi(p->lfo[i].wave, 0, 5), m->phase[i], m->rnd[i]);
        }
    }
    if (env) {
        ec_envspec_t sp;
        menv_spec(p, &sp);
        ec_env_trigger(&m->env, &sp, gate, 0, fs);
    }
}

void ec_mod_note_off(ec_mod_t *m, const ec_mod_params_t *p, float fs)
{
    if (!m->alive) return;
    ec_envspec_t sp;
    menv_spec(p, &sp);
    ec_env_note_off(&m->env, &sp, fs);
}

void ec_mod_tick(ec_mod_t *m, const ec_mod_params_t *p, float bpm, float fs, const float *lag01, int n,
                 float *out)
{
    for (int d = 0; d < EC_MOD_NDEST; ++d) out[d] = 0.f;
    if (!ec_mod_needed(p)) {
        if (m->alive) {
            m->alive = 0;
            for (int i = 0; i < EC_MSRC_N; ++i) m->src[i] = 0.f;
        }
        return;
    }
    if (!m->alive) mod_spawn(m);

    /* the sources at the chunk start, each at its level */
    float src[1 + EC_MSRC_N + 4];
    src[0] = 0.f;
    for (int i = 0; i < 2; ++i)
        src[1 + EC_MSRC_LFO1 + i] = lfo_value(m, p, i) * ec_clampf(p->lfo[i].depth, -1.f, 1.f);
    ec_envspec_t sp;
    menv_spec(p, &sp);
    float ev = 0.f;
    for (int i = 0; i < n; ++i) {
        float v = ec_env_tick(&m->env, &sp, fs);
        if (i == 0) ev = v;
    }
    src[1 + EC_MSRC_ENV] = ev * ec_clampf(p->menv_depth, -1.f, 1.f);
    m->vel_lag = ec_lag(m->vel_lag, ec_clampf(m->vel, 0.f, 1.f), lag01, n);
    src[1 + EC_MSRC_VEL] = m->vel_lag * ec_clampf(p->mvel_depth, -1.f, 1.f);
    for (int i = 0; i < 3; ++i) {
        m->live_lag[i] = ec_lag(m->live_lag[i], ec_clampf(m->live[i], 0.f, 1.f), lag01, n);
        src[1 + EC_MSRC_AT + i] = m->live_lag[i];
    }
    for (int k = 0; k < 4; ++k) {
        m->macro_val[k] = ec_clampf(p->macro_base[k], 0.f, 1.f);
        src[1 + EC_MSRC_N + k] = m->macro_val[k];
    }
    for (int i = 0; i < EC_MSRC_N; ++i) m->src[i] = src[1 + i];

    /* the slots on the fast buses */
    for (int k = 0; k < EC_MOD_SLOTS; ++k) {
        const int s = p->slot[k].src, b = p->slot[k].bus;
        if (s > 0 && s <= EC_MSRC_N + 4 && b >= 0 && b < EC_MOD_NDEST) out[b] += src[s] * p->slot[k].depth;
    }

    /* advance */
    for (int i = 0; i < 2; ++i) {
        float f = lfo_freq(bpm, p->lfo[i].beats), inc = f * (float)n / fs;
        m->phase[i] += inc;
        m->phase[i] -= floorf(m->phase[i]);
        if (m->sweep[i] < 1.f) m->sweep[i] += inc;
        m->rnd_ph[i] += inc;
        if (m->rnd_ph[i] >= 1.f) {
            m->rnd_ph[i] -= floorf(m->rnd_ph[i]);
            m->rnd[i] = ec_frand2(&m->rng);
        }
    }
}
