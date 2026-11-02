/*
 * ec_warp3.c -- E3: the warp modes that were falling back to tempo_varispeed.
 * Ports of Engine_Elasticat.sc's \elasticatChopped (domino, on the
 * ElasticatSlicer UGen), \elasticatRandomOla, \elasticatPitchCorrected,
 * \elasticatHarmonizer, \elasticatWavetable (ElasticatWavetable UGen),
 * \elasticatSpectral (spectral_freeze + formant), \elasticatGStretch and
 * \elasticatGStretch2, plus C versions of the SC UGens they lean on:
 * PitchShift (4 overlapping triangular grains on a delay line), TGrains
 * (Hann grains, cap 64) and FFT / PV_MagFreeze / PV_MagSmear / PV_MagShift /
 * PV_BinShift / IFFT (2048, hop 1024, sine windows).
 *
 * The reader-family modes (pitch_corrected, harmonizer, random_ola, gstretch,
 * gstretch2) keep the click-free ElasticatReader of ec_reader.c as their dry
 * read and add a post stage here; the track's ONE delay line is written by
 * every reader mode so a stage switch starts with real history (and the two
 * stages crossfade over mode_switch_fade).
 */
#include "ec_internal.h"

#define PS_MASK (EC_PS_LEN - 1)

/* ------------------------------------------------------------ helpers */
static inline float frand01(uint32_t *s) { return (float)(ec_xorshift(s) & 0xFFFFFFu) / 16777216.f; }

/* sin(2 pi ph), ph in [0,1) */
static inline float sin2pi(double ph)
{
    float x = (float)(ph * 4.0);
    if (x < 1.f) return ec_sin_hp(x);
    if (x < 2.f) return ec_sin_hp(2.f - x);
    if (x < 3.f) return -ec_sin_hp(x - 2.f);
    return -ec_sin_hp(4.f - x);
}
/* sin(pi ph), ph in [0,1] */
static inline float sinpi(double ph)
{
    float x = (float)(ph * 2.0);
    if (x < 0.f) x = 0.f;
    if (x > 2.f) x = 2.f;
    return x <= 1.f ? ec_sin_hp(x) : ec_sin_hp(2.f - x);
}

/* the E4 PITCH mod bus, +-12 st (SC: In.kr(pitchModBus).clip(-1, 1) * 12 in every mode's pitchRatio) */
static inline float pitch_mod_st(const ec_track_t *t)
{
    return ec_clampf(t->mods[EC_MOD_PITCH], -1.f, 1.f) * 12.f;
}

static double pitch_ratio(const ec_track_t *t)
{
    return ec_clampd(ec_midiratio(t->pitch_lag + pitch_mod_st(t)), 1.0 / 32.0, 32.0);
}

/* ------------------------------------------------------------ init */
void ec_w3_init(ec_dsp_t *d)
{
    ec_w3_shared_t *S = &d->w3s;
    ec_rfft_init(&S->fft);
    for (int i = 0; i < EC_FFT_N; ++i)
        S->win[i] = (float)sin(EC_PI * ((double)i + 0.5) / (double)EC_FFT_N);
    ec_lag_table(S->lag10, 0.1f, d->fs);
}

void ec_w3_reset(ec_w3_t *w, uint32_t seed)
{
    memset(w, 0, sizeof *w);
    w->rd_mode = EC_WARP_TAPE;
    w->rng = seed ? seed : 0x9E3779B9u;
    w->h_iv[0] = 7.f;
    w->win_lag = 0.2f;
    w->strch_lag = 1.f;
    w->o_dur = 0.08f;
    w->o_ov = 8.f;
    w->o_wan = 0.015f;
    w->otrig = 1.0;              /* Impulse fires on its first sample */
    w->sv_prev = -1;
    w->sv[0].state = w->sv[1].state = 3;   /* SV_DONE */
    w->lcur = 0.5f; w->lnext = 0.5f;
    for (int h = 0; h < 2; ++h) { w->rl[h] = 8.0; w->wst[h] = 0; }
}

int ec_w3_family(int mode)
{
    switch (mode) {
    case EC_WARP_GRANULAR: return FAM_GRAINS;
    case EC_WARP_CHOPPED: return FAM_CHOP;
    case EC_WARP_DOMINO: return FAM_DOMINO;
    case EC_WARP_WAVETABLE: return FAM_WT;
    case EC_WARP_SPECTRAL_FREEZE:
    case EC_WARP_FORMANT: return FAM_SPEC;
    default: return FAM_READER;  /* tape, tempo_varispeed, random_ola, pitch_corrected,
                                    harmonizer, gstretch, gstretch2 */
    }
}

int ec_w3_stage(int mode)
{
    switch (mode) {
    case EC_WARP_PITCH_CORRECTED:
    case EC_WARP_GSTRETCH:
    case EC_WARP_GSTRETCH2: return W3_ST_PS;
    case EC_WARP_HARMONIZER: return W3_ST_HARM;
    case EC_WARP_RANDOM_OLA: return W3_ST_OLA;
    default: return W3_ST_DRY;
    }
}

void ec_w3_set_reader_mode(ec_w3_t *w, int mode, float fade_s, float fs)
{
    int st = ec_w3_stage(mode);
    w->rd_mode = mode;
    if (st == w->stage) return;
    /* a crossfade already running: the newest stage wins, from where the old one is */
    w->prev_stage = w->stage;
    w->stage = st;
    w->xf = 0.f;
    w->xf_inc = (float)EC_KR / (ec_clampf(fade_s, 0.001f, 0.25f) * fs);   /* per chunk of <= EC_KR */
    if (st == W3_ST_OLA) { w->ong = 0; w->otrig = 1.0; }
    if (st == W3_ST_PS) memset(&w->ps, 0, sizeof w->ps);
    if (st == W3_ST_HARM) memset(w->harm, 0, sizeof w->harm);
}

double ec_w3_reader_rate(const ec_dsp_t *d, const ec_track_t *t, double cps)
{
    double w = (t->hi - t->lo) * t->frames;
    switch (t->w3.rd_mode) {
    case EC_WARP_TAPE: return t->brs * pitch_ratio(t) * t->rate_lag;
    case EC_WARP_GSTRETCH: return t->brs * t->w3.strch_lag * t->rate_lag;
    case EC_WARP_GSTRETCH2: return w * cps / d->fs * t->w3.strch_lag;
    default: return w * cps / d->fs;   /* tempo-synced: tempo_varispeed, random_ola, pitch_corrected, harmonizer */
    }
}

/* ------------------------------------------------------------ PitchShift (SC DelayUGens.cpp) */
static int ps_frame(float win_s, float fs)
{
    int f = ((int)(win_s * fs) + 2) & ~3;
    if (f < 8) f = 8;
    if (f > EC_PS_MAXWIN) f = EC_PS_MAXWIN & ~3;
    return f;
}

static void ps_trigger(ec_ps_t *ps, int fsz, float ratio, float pdisp, float tdisp, uint32_t *rng)
{
    ps->counter = fsz >> 2;
    ps->stage = (ps->stage + 1) & 3;
    float r = ratio;
    if (pdisp != 0.f) r += pdisp * ec_frand2(rng);
    r = ec_clampf(r, 0.f, 4.f);
    float r1 = r - 1.f;
    float start = r1 < 0.f ? 2.f : (float)fsz * r1 + 2.f;
    if (tdisp > 0.f) start += tdisp * frand01(rng);
    if (start > (float)(EC_PS_LEN - 8)) start = (float)(EC_PS_LEN - 8);
    float slope = 2.f / (float)fsz;
    ec_psg_t *g = &ps->g[ps->stage];
    g->dsamp = start;
    g->dslope = -r1;
    g->ramp = 0.f;
    g->rslope = slope;
    ps->g[(ps->stage + 2) & 3].rslope = -slope;
}

/* n samples written at wr0+1 .. wr0+n; reads both channels with one grain schedule */
static void ps_run(ec_ps_t *ps, ec_w3_t *w, uint32_t wr0, int n, float ratio, int fsz, float pdisp, float tdisp,
                   float *oL, float *oR)
{
    for (int i = 0; i < n; ++i) {
        if (ps->counter <= 0) ps_trigger(ps, fsz, ratio, pdisp, tdisp, &w->rng);
        ps->counter--;
        uint32_t wi = wr0 + 1u + (uint32_t)i;
        float l = 0.f, r = 0.f;
        for (int k = 0; k < 4; ++k) {
            ec_psg_t *g = &ps->g[k];
            g->dsamp += g->dslope;
            float ramp = g->ramp;
            g->ramp += g->rslope;
            if (g->ramp < 0.f) g->ramp = 0.f;
            else if (g->ramp > 1.f) g->ramp = 1.f;
            if (ramp <= 0.f) continue;
            float ds = g->dsamp;
            if (ds < 1.f) ds = 1.f;
            if (ds > (float)(EC_PS_LEN - 4)) ds = (float)(EC_PS_LEN - 4);
            int id = (int)ds;
            float fr = ds - (float)id;
            uint32_t a = (wi - (uint32_t)id) & PS_MASK, b = (a - 1u) & PS_MASK;
            l += (w->dl[0][a] + fr * (w->dl[0][b] - w->dl[0][a])) * ramp;
            r += (w->dl[1][a] + fr * (w->dl[1][b] - w->dl[1][a])) * ramp;
        }
        oL[i] = 0.5f * l;
        oR[i] = 0.5f * r;
    }
}

/* ------------------------------------------------------------ the reader post stages */
static void stage_ps(ec_dsp_t *d, ec_track_t *t, double cps, uint32_t wr0, int n, float *oL, float *oR)
{
    ec_w3_t *w = &t->w3;
    const ec_track_params_t *p = &t->p;
    float fs = d->fs, ratio, win, pdisp = 0.f, tdisp = 0.f;
    double pr = pitch_ratio(t);
    switch (w->rd_mode) {
    case EC_WARP_PITCH_CORRECTED:
        /* ratio = derivedSourceBpm / targetBpm * pitch (clip 0.5..2); window x (1 + SMEAR) */
        ratio = (float)ec_clampd(t->src_bpm / (d->bpm > 1.f ? d->bpm : 1.f) * pr, 0.5, 2.0);
        win = w->win_lag * (1.f + ec_clampf(p->mode_macro, 0.f, 1.f));
        pdisp = w->disp_lag;
        break;
    case EC_WARP_GSTRETCH: {
        double rate = (double)w->strch_lag * (t->rate_lag > 0.001f ? t->rate_lag : 0.001f);
        ratio = (float)ec_clampd(pr / rate, 0.25, 4.0);
        win = w->win_lag;
        tdisp = w->smth_lag * win * fs;
    } break;
    default: {   /* gstretch2: correct the grid-synced read back to the native rate */
        double w_fr = (t->hi - t->lo) * t->frames;
        double base = w_fr * cps / fs * w->strch_lag;
        if (base < 0.0001) base = 0.0001;
        ratio = (float)ec_clampd(pr * t->brs / base, 0.25, 4.0);
        win = w->win_lag;
        tdisp = w->smth_lag * win * fs;
    } break;
    }
    ps_run(&w->ps, w, wr0, n, ratio, ps_frame(win, fs), pdisp, tdisp, oL, oR);
}

static void stage_harm(ec_dsp_t *d, ec_track_t *t, uint32_t wr0, const float *inL, const float *inR, int n,
                       float *oL, float *oR)
{
    ec_w3_t *w = &t->w3;
    float tl[EC_KR], tr[EC_KR], wl[EC_KR], wrr[EC_KR];
    float mix = ec_clampf(t->p.mode_macro, 0.f, 1.f);
    int fsz = ps_frame(0.2f, d->fs);
    for (int i = 0; i < n; ++i) wl[i] = wrr[i] = 0.f;
    for (int v = 0; v < 3; ++v) {
        float g = w->h_gate[v];
        if (g < 1e-4f) { memset(&w->harm[v], 0, sizeof w->harm[v]); continue; }
        float ratio = ec_clampf(ec_midiratio(w->h_iv[v] + pitch_mod_st(t)), 0.25f, 4.f);   /* + pitch mod (SC) */
        ps_run(&w->harm[v], w, wr0, n, ratio, fsz, 0.f, 0.004f * d->fs, tl, tr);
        for (int i = 0; i < n; ++i) { wl[i] += tl[i] * g; wrr[i] += tr[i] * g; }
    }
    for (int i = 0; i < n; ++i) {
        oL[i] = (inL[i] + wl[i] * mix) * 0.6f;
        oR[i] = (inR[i] + wrr[i] * mix) * 0.6f;
    }
}

static void stage_ola(ec_dsp_t *d, ec_track_t *t, const float *inL, const float *inR, int n, float *oL, float *oR)
{
    ec_w3_t *w = &t->w3;
    const ec_sample_t *s = &t->smp;
    double fs = d->fs, F = t->frames, srate = s->rate > 0.f ? s->rate : fs;
    double bufdur = F / srate;
    double step_dur = 15.0 / (d->bpm > 1.f ? d->bpm : 1.f);
    double trig_rate = ec_clampd(w->o_ov / step_dur, 1.0, 240.0);
    double dur = floor(w->o_dur * fs);
    if (dur < 4.0) dur = 4.0;
    double rate = pitch_ratio(t) * t->brs;
    float chaos = ec_clampf(t->p.mode_macro, 0.f, 1.f);
    double rp = ec_reader_idle(&t->rd) ? t->restart_frame / F : t->rd.phase[t->rd.active] / F;
    rp = ec_clampd(rp, 0.0, 0.999999);
    /* XFade2(direct, wet, macro.linlin(0, 1, -0.75, 0.25)), equal power; x gainNorm */
    float th = (chaos - 0.75f + 1.f) * 0.5f;          /* (pan + 1) / 2 in 0..1 -> sin/cos of th*pi/2 */
    float gd = ec_sin_hp(1.f - th), gw = ec_sin_hp(th);
    float norm = 2.5f / sqrtf(w->o_ov > 1.f ? w->o_ov : 1.f);
    for (int i = 0; i < n; ++i) {
        w->otrig += trig_rate / fs;
        if (w->otrig >= 1.0) {
            w->otrig -= floor(w->otrig);
            int total = 0;   /* the engine-wide grain budget (CPU): one TGrains' worth across all tracks */
            for (int k = 0; k < EC_TRACKS; ++k) total += d->tr[k].w3.ong;
            if (w->ong < EC_OLA_GRAINS && total < EC_OLA_BUDGET) {
                double off = (double)ec_frand2(&w->rng) * w->o_wan * (0.25 + chaos);
                double pos = rp * bufdur + off;
                pos = fmod(pos, bufdur);
                if (pos < 0.0) pos += bufdur;
                ec_olag_t *g = &w->og[w->ong++];
                g->rate = rate;
                g->pos = pos * srate - 0.5 * dur * rate;
                g->ph = 0.f;
                g->inc = (float)(1.0 / dur);
            }
        }
        float l = 0.f, r = 0.f;
        for (int k = 0; k < w->ong;) {
            ec_olag_t *g = &w->og[k];
            float h = ec_hann_lerp(d->hann, g->ph), a, b;
            ec_peek_wrap(s, g->pos, &a, &b);
            l += a * h;
            r += b * h;
            g->pos += g->rate;
            g->ph += g->inc;
            if (g->ph >= 1.f) { *g = w->og[--w->ong]; continue; }
            ++k;
        }
        oL[i] = (inL[i] * gd + l * EC_S16_SCALE * gw) * norm;
        oR[i] = (inR[i] * gd + r * EC_S16_SCALE * gw) * norm;
    }
}

static void stage_run(ec_dsp_t *d, ec_track_t *t, int st, double cps, uint32_t wr0, const float *inL,
                      const float *inR, int n, float *oL, float *oR)
{
    switch (st) {
    case W3_ST_PS: stage_ps(d, t, cps, wr0, n, oL, oR); break;
    case W3_ST_HARM: stage_harm(d, t, wr0, inL, inR, n, oL, oR); break;
    case W3_ST_OLA: stage_ola(d, t, inL, inR, n, oL, oR); break;
    default:
        for (int i = 0; i < n; ++i) { oL[i] = inL[i]; oR[i] = inR[i]; }
        break;
    }
}

void ec_w3_post(ec_dsp_t *d, ec_track_t *t, double cps, float *L, float *R, int n)
{
    ec_w3_t *w = &t->w3;
    const ec_track_params_t *p = &t->p;
    /* controls (Lag as the SynthDefs) */
    w->strch_lag = ec_lag(w->strch_lag, ec_clampf(p->grain_speed, 0.0625f, 4.f), d->lag05, n);
    if (w->rd_mode == EC_WARP_PITCH_CORRECTED) {
        w->win_lag = ec_lag(w->win_lag, ec_clampf(p->pv_window, 0.005f, 2.f), d->lag05, n);
        w->disp_lag = ec_lag(w->disp_lag, ec_clampf(p->pv_dispersion, 0.f, 1.f), d->lag05, n);
    } else {
        w->win_lag = ec_lag(w->win_lag, ec_clampf(p->grain_size, 0.02f, 0.5f), d->lag05, n);
    }
    w->smth_lag = ec_lag(w->smth_lag, (ec_clampf(p->grain_density, 1.f, 64.f) - 1.f) / 63.f, d->lag05, n);
    for (int v = 0; v < 3; ++v) {
        float iv = p->harm_interval[v];
        w->h_iv[v] = ec_lag(w->h_iv[v], ec_clampf(iv, -24.f, 24.f), d->lag05, n);
        w->h_gate[v] = ec_lag(w->h_gate[v], fabsf(iv) > 0.5f ? 1.f : 0.f, d->lag02, n);
    }
    w->o_dur = ec_lag(w->o_dur, ec_clampf(p->wsola_window, 0.03f, 0.6f), d->lag05, n);
    w->o_ov = ec_lag(w->o_ov, ec_clampf(p->grain_density, 1.f, 64.f), d->lag05, n);
    w->o_wan = ec_lag(w->o_wan, ec_clampf(p->wsola_search, 0.f, 0.25f), d->lag05, n);

    /* the shared delay line: the dry read, always (a later stage switch has history) */
    uint32_t wr0 = w->wr;
    for (int i = 0; i < n; ++i) {
        uint32_t k = (wr0 + 1u + (uint32_t)i) & PS_MASK;
        w->dl[0][k] = L[i];
        w->dl[1][k] = R[i];
    }
    w->wr = (wr0 + (uint32_t)n) & PS_MASK;

    if (w->stage == W3_ST_DRY && w->prev_stage == W3_ST_DRY) return;
    float aL[EC_KR], aR[EC_KR];
    stage_run(d, t, w->stage, cps, wr0, L, R, n, aL, aR);
    if (w->prev_stage != w->stage) {
        float bL[EC_KR], bR[EC_KR];
        stage_run(d, t, w->prev_stage, cps, wr0, L, R, n, bL, bR);
        float x0 = w->xf, x1 = ec_clampf(w->xf + w->xf_inc * (float)n / (float)EC_KR, 0.f, 1.f);
        for (int i = 0; i < n; ++i) {
            float x = x0 + (x1 - x0) * (float)(i + 1) / (float)n;
            float gn = ec_sin_hp(x), go = ec_sin_hp(1.f - x);
            L[i] = aL[i] * gn + bL[i] * go;
            R[i] = aR[i] * gn + bR[i] * go;
        }
        w->xf = x1;
        if (x1 >= 1.f) w->prev_stage = w->stage;
        return;
    }
    for (int i = 0; i < n; ++i) { L[i] = aL[i]; R[i] = aR[i]; }
}

/* ------------------------------------------------------------ domino (ElasticatSlicer) */
enum { SV_ATTACK = 0, SV_HOLD = 1, SV_RELEASE = 2, SV_DONE = 3 };

static void play_window(const ec_track_t *t, double *lo, double *hi)
{
    double a = ec_clampd(t->cur_s / 128.0, 0.0, 1.0), b = ec_clampd(t->cur_e / 128.0, 0.0, 1.0);
    if (b < a + 0.0001) b = a + 0.0001;
    *lo = a;
    *hi = b;
}

void ec_w3_domino(ec_dsp_t *d, ec_track_t *t, double cps, double run, double dir, float *L, float *R, int n)
{
    ec_w3_t *w = &t->w3;
    const ec_track_params_t *p = &t->p;
    const ec_sample_t *s = &t->smp;
    double fs = d->fs, fmx = t->frames - 1.0;
    /* the RANGE (trim x Range = track space 0..128) is the slice area; the resolved loop is the window */
    double sf = ec_track_map(t, 0.0), ef = ec_track_map(t, 128.0);
    if (ef < sf + 1e-6) ef = sf + 1e-6;
    double reg = ef - sf;
    double plo, phi;
    play_window(t, &plo, &phi);
    double pw = phi - plo;
    int ns = ec_clampi(p->chop_steps, 1, 256);
    double wr = t->rate_lag > 0.001f ? t->rate_lag : 0.001;
    double step_s = 15.0 * fs / ((d->bpm > 1.f ? d->bpm : 1.0) * wr);
    if (step_s < 1.0) step_s = 1.0;
    double slot = ec_clampd(p->chop_slice_len, 0.05, 64.0) * step_s;
    if (slot < 1.0) slot = 1.0;
    double inc = 1.0 / ((double)ns * slot) * dir * run;
    double atk = 1.0 / fmax(1.0, ec_clampd(p->chop_attack, 0.0001, 0.5) * fs);
    double rel = 1.0 / fmax(1.0, ec_clampd(p->chop_release, 0.0001, 0.5) * fs);
    double gate_close = ec_clampd(p->chop_hold, 0.0, 1.0) * slot;
    int cm = ec_clampi(p->chop_loop_mode - 1, 0, 7);
    int loop_mode = cm / 2, srev = cm % 2;
    double prate = t->brs * pitch_ratio(t);
    double linc = cps / fs * dir * run;
    for (int i = 0; i < n; ++i) {
        double sp = w->sph;
        if (sp < plo || sp >= phi) {               /* the window moved: wrap the cursor into it */
            sp = plo + fmod(sp - plo, pw);
            if (sp < plo) sp += pw;
            w->sph = sp;
        }
        double spc = ec_clampd(sp, 0.0, 0.999999);
        int idx = ec_clampi((int)(spc * (double)ns), 0, ns - 1);
        if (idx != w->sv_prev) {                   /* slice change: the active head releases, a new one attacks */
            if (w->sv[w->sv_active].state != SV_DONE) w->sv[w->sv_active].state = SV_RELEASE;
            int nv = w->sv_active ^ 1;
            ec_slv_t *v = &w->sv[nv];
            v->lo = (sf + ((double)idx / (double)ns) * reg) * fmx;
            v->hi = (sf + ((double)(idx + 1) / (double)ns) * reg) * fmx;
            v->dir = srev ? -1 : 1;
            v->pos = srev ? v->hi : v->lo;
            v->env = 0.f;
            v->state = SV_ATTACK;
            v->since = 0.0;
            w->sv_active = nv;
            w->sv_prev = idx;
        }
        float ml = 0.f, mr = 0.f;
        for (int k = 0; k < 2; ++k) {
            ec_slv_t *v = &w->sv[k];
            if (v->state == SV_DONE) continue;
            float a, b;
            ec_peek_clamp(s, ec_clampd(v->pos, 0.0, fmx), &a, &b);
            ml += a * v->env;
            mr += b * v->env;
            v->pos += prate * (double)v->dir;
            if (loop_mode == 0) {                  /* chop: hold at the slice bound */
                if (v->pos >= v->hi) v->pos = v->hi;
                else if (v->pos <= v->lo) v->pos = v->lo;
            } else if (loop_mode == 1) {           /* loop: wrap within the slice */
                double sw = v->hi - v->lo;
                if (sw < 1.0) sw = 1.0;
                if (v->pos >= v->hi) v->pos -= sw;
                else if (v->pos < v->lo) v->pos += sw;
            } else if (loop_mode == 2) {           /* ping-pong */
                if (v->pos >= v->hi) { v->pos = v->hi; v->dir = -1; }
                else if (v->pos <= v->lo) { v->pos = v->lo; v->dir = 1; }
            }                                      /* 3 runaway: no slice bound */
            v->since += 1.0;
            if (v->state == SV_ATTACK) {
                v->env += (float)atk;
                if (v->env >= 1.f) { v->env = 1.f; v->state = SV_HOLD; }
            } else if (v->state == SV_HOLD) {
                if (v->since >= gate_close) v->state = SV_RELEASE;
            } else if (v->state == SV_RELEASE) {
                v->env -= (float)rel;
                if (v->env <= 0.f) { v->env = 0.f; v->state = SV_DONE; }
            }
        }
        L[i] = ml * EC_S16_SCALE;
        R[i] = mr * EC_S16_SCALE;
        w->sph += inc;
        if (w->sph >= phi) w->sph -= pw;
        else if (w->sph < plo) w->sph += pw;
        w->lph += linc;
        w->lph -= floor(w->lph);
    }
}

/* ------------------------------------------------------------ wavetable (ElasticatWavetable) */
static inline float wt_read(const ec_sample_t *s, int ch, double reg_start, double reg_len, int slices,
                            double cyc, double morph, double osc, float w1)
{
    if (cyc > reg_len) cyc = reg_len;
    if (cyc < 4.0) cyc = 4.0;
    int div = slices - 1 > 0 ? slices - 1 : 1;
    double step = (reg_len - cyc) / (double)div;
    if (step < 0.0) step = 0.0;
    double sl = morph * (double)(slices - 1);
    if (sl < 0.0) sl = 0.0;
    int si = (int)sl;
    if (si > slices - 1) si = slices - 1;
    double fr = sl - (double)si;
    int si2 = si + 1 > slices - 1 ? slices - 1 : si + 1;
    double a0 = reg_start + (double)si * step, b0 = reg_start + (double)si2 * step;
    double ph = osc * cyc;
    double p2ph = osc + 0.5;
    if (p2ph >= 1.0) p2ph -= 1.0;
    double p2 = p2ph * cyc;
    float w2 = 1.f - w1, x0, x1, y0, y1, dummy;
    if (ch == 0) {
        ec_peek_clamp(s, a0 + ph, &x0, &dummy);
        ec_peek_clamp(s, a0 + p2, &x1, &dummy);
        ec_peek_clamp(s, b0 + ph, &y0, &dummy);
        ec_peek_clamp(s, b0 + p2, &y1, &dummy);
    } else {
        ec_peek_clamp(s, a0 + ph, &dummy, &x0);
        ec_peek_clamp(s, a0 + p2, &dummy, &x1);
        ec_peek_clamp(s, b0 + ph, &dummy, &y0);
        ec_peek_clamp(s, b0 + p2, &dummy, &y1);
    }
    float fa = x0 * w1 + x1 * w2, fb = y0 * w1 + y1 * w2;
    return fa * (float)(1.0 - fr) + fb * (float)fr;
}

enum { WT_STOP = 0, WT_FADEIN = 1, WT_FADEOUT = 2, WT_PLAY = 3 };

static void wt_xfade(ec_w3_t *w, double ns, double nl)
{
    int nv = w->wact ^ 1;
    if (w->wst[w->wact] != WT_STOP) w->wst[w->wact] = WT_FADEOUT;
    w->rs[nv] = ns;
    w->rl[nv] = nl;
    w->wfade[nv] = 0.f;
    w->wst[nv] = WT_FADEIN;
    w->wact = nv;
}

static inline int wt_fading(const ec_w3_t *w)
{
    return w->wst[0] == WT_FADEIN || w->wst[0] == WT_FADEOUT || w->wst[1] == WT_FADEIN || w->wst[1] == WT_FADEOUT;
}

void ec_w3_wavetable(ec_dsp_t *d, ec_track_t *t, float *L, float *R, int n)
{
    ec_w3_t *w = &t->w3;
    const ec_track_params_t *p = &t->p;
    const ec_sample_t *s = &t->smp;
    double fs = d->fs, fmx = t->frames - 1.0;
    double sf = ec_clampd(t->lo, 0.0, 1.0), ef = ec_clampd(t->hi, 0.0, 1.0);
    if (ef < sf + 1e-4) ef = sf + 1e-4;
    int slices = ec_clampi(p->wt_window, 2, 64);
    double cyc = ec_clampd(p->wt_cycle, 16.0, 8192.0);
    w->morph_lag = ec_lag(w->morph_lag, ec_clampf(p->mode_macro, 0.f, 1.f), d->lag02, n);
    w->depth_lag = ec_lag(w->depth_lag, ec_clampf(p->wt_lfo_depth, -1.f, 1.f), d->lag05, n);
    double freq = ec_clampd(130.81 * pitch_ratio(t), 1.0, 12000.0);
    double lrate = p->wt_lfo_rate > 0.f ? p->wt_lfo_rate : 0.0;
    int shape = ec_clampi(p->wt_lfo_shape - 1, 0, 4);
    double xs = ec_clampd(p->xfade, 0.0005, 0.5) * fs;
    if (xs < 1.0) xs = 1.0;
    float finc = (float)(1.0 / xs);
    double tgt_s = sf * fmx, tgt_l = (ef - sf) * fmx;
    if (tgt_l < 8.0) tgt_l = 8.0;
    if (!w->wcommitted) {
        w->rs[w->wact] = tgt_s; w->rl[w->wact] = tgt_l;
        w->wfade[w->wact] = 1.f; w->wst[w->wact] = WT_PLAY;
        w->wcommitted = 1;
    }
    if (fabs(tgt_s - w->rs[w->wact]) > 1.0 || fabs(tgt_l - w->rl[w->wact]) > 1.0) {
        if (wt_fading(w)) { w->qs = tgt_s; w->ql = tgt_l; w->wqueued = 1; }
        else wt_xfade(w, tgt_s, tgt_l);
    }
    double oinc = freq / fs, linc = lrate / fs;
    int mono = s->channels == 1;
    for (int i = 0; i < n; ++i) {
        w->lfo += linc;
        if (w->lfo >= 1.0) {
            w->lfo -= floor(w->lfo);
            w->lcur = w->lnext;
            w->lnext = ec_frand2(&w->rng) * 0.5f + 0.5f;
        }
        float raw;
        switch (shape) {
        case 0: raw = 0.5f + 0.5f * sin2pi(w->lfo); break;
        case 1: raw = 1.f - fabsf((float)(2.0 * w->lfo - 1.0)); break;
        case 2: raw = (float)w->lfo; break;
        case 3: raw = w->lcur; break;
        default: raw = w->lcur + (w->lnext - w->lcur) * (float)w->lfo; break;
        }
        double mp = ec_clampd((double)w->morph_lag + (double)raw * w->depth_lag, 0.0, 1.0);
        float sp = sinpi(w->osc);
        float w1 = sp * sp;
        float ml = 0.f, mr = 0.f;
        for (int h = 0; h < 2; ++h) {
            if (w->wst[h] == WT_STOP) continue;
            float g = ec_sin_hp(w->wfade[h]);
            float a = wt_read(s, 0, w->rs[h], w->rl[h], slices, cyc, mp, w->osc, w1);
            float b = mono ? a : wt_read(s, 1, w->rs[h], w->rl[h], slices, cyc, mp, w->osc, w1);
            ml += a * g;
            mr += b * g;
        }
        L[i] = ml * EC_S16_SCALE;
        R[i] = mr * EC_S16_SCALE;
        w->osc += oinc;
        w->osc -= floor(w->osc);
        for (int h = 0; h < 2; ++h) {
            if (w->wst[h] == WT_FADEIN) {
                w->wfade[h] += finc;
                if (w->wfade[h] >= 1.f) { w->wfade[h] = 1.f; w->wst[h] = WT_PLAY; }
            } else if (w->wst[h] == WT_FADEOUT) {
                w->wfade[h] -= finc;
                if (w->wfade[h] <= 0.f) { w->wfade[h] = 0.f; w->wst[h] = WT_STOP; }
            }
        }
        if (w->wqueued && !wt_fading(w)) {
            w->wqueued = 0;
            if (fabs(w->qs - w->rs[w->wact]) > 1.0 || fabs(w->ql - w->rl[w->wact]) > 1.0) wt_xfade(w, w->qs, w->ql);
        }
    }
}

/* ------------------------------------------------------------ spectral (\elasticatSpectral) */
static void spectral_frame(ec_dsp_t *d, ec_track_t *t)
{
    ec_w3_t *w = &t->w3;
    ec_w3_shared_t *S = &d->w3s;
    const int N = EC_FFT_N, H = EC_FFT_HALF, NB = EC_FFT_HALF - 1;   /* SC numbins: no DC / Nyquist */
    float *x = S->x;
    for (int k = 0; k < N; ++k) x[k] = w->sp_in[(w->sp_pos + k) & (N - 1)] * S->win[k];
    ec_rfft_fwd(&S->fft, x);
    float dc = x[0], ny = x[1];
    for (int k = 1; k < H; ++k) {
        float re = x[2 * k], im = x[2 * k + 1];
        float m = sqrtf(re * re + im * im);
        S->mag[k] = m;
        if (m > 1e-20f) { S->ur[k] = re / m; S->ui[k] = im / m; }
        else { S->ur[k] = 1.f; S->ui[k] = 0.f; }
    }
    /* PV_MagFreeze */
    if (w->frz_lag > 1e-3f) {
        for (int k = 1; k < H; ++k) S->mag[k] = w->sp_frz[k];
        dc = w->sp_frz[0];
        ny = w->sp_frz[H];
    } else {
        for (int k = 1; k < H; ++k) w->sp_frz[k] = S->mag[k];
        w->sp_frz[0] = dc;
        w->sp_frz[H] = ny;
    }
    /* PV_MagSmear(bins) */
    int bins = (int)(w->blur_lag * 16.f);
    if (bins > 0) {
        float scale = 1.f / (float)(2 * bins + 1), sum = 0.f;
        for (int j = 1; j <= bins && j < H; ++j) sum += S->mag[j];
        for (int k = 1; k < H; ++k) {
            int add = k + bins, sub = k - bins - 1;
            if (add < H) sum += S->mag[add];
            if (sub >= 1) sum -= S->mag[sub];
            S->m2[k] = (sum > 0.f ? sum : 0.f) * scale;
        }
        for (int k = 1; k < H; ++k) S->mag[k] = S->m2[k];
    }
    /* PV_MagShift(stretch): magnitudes move, phases stay */
    float fsh = ec_clampf(ec_midiratio(w->fsh_lag), 0.25f, 4.f);
    if (fabsf(fsh - 1.f) > 1e-4f) {
        for (int k = 1; k < H; ++k) S->m2[k] = 0.f;
        float fpos = 0.f;
        for (int i = 0; i < NB; ++i) {
            int pos = (int)(fpos + 0.5f);
            if (pos >= 0 && pos < NB) S->m2[pos + 1] += S->mag[i + 1];
            fpos += fsh;
        }
        for (int k = 1; k < H; ++k) S->mag[k] = S->m2[k];
    }
    for (int k = 1; k < H; ++k) { S->cr[k] = S->mag[k] * S->ur[k]; S->ci[k] = S->mag[k] * S->ui[k]; }
    /* PV_BinShift(stretch): whole bins (mag + phase) move */
    float pit = ec_clampf(ec_midiratio(w->pit_lag + pitch_mod_st(t)), 0.25f, 4.f);   /* + pitch mod (SC) */
    if (fabsf(pit - 1.f) > 1e-4f) {
        for (int k = 1; k < H; ++k) { x[2 * k] = 0.f; x[2 * k + 1] = 0.f; }
        float fpos = 0.f;
        for (int i = 0; i < NB; ++i) {
            int pos = (int)(fpos + 0.5f);
            if (pos >= 0 && pos < NB) { x[2 * (pos + 1)] += S->cr[i + 1]; x[2 * (pos + 1) + 1] += S->ci[i + 1]; }
            fpos += pit;
        }
    } else {
        for (int k = 1; k < H; ++k) { x[2 * k] = S->cr[k]; x[2 * k + 1] = S->ci[k]; }
    }
    x[0] = dc;
    x[1] = ny;
    ec_rfft_inv(&S->fft, x);
    float g = 2.f / (float)N;
    for (int k = 0; k < N; ++k) w->sp_out[(w->sp_pos + k) & (N - 1)] += x[k] * S->win[k] * g;
}

void ec_w3_spectral(ec_dsp_t *d, ec_track_t *t, double cps, double run, double dir, float *L, float *R, int n)
{
    ec_w3_t *w = &t->w3;
    const ec_track_params_t *p = &t->p;
    const ec_sample_t *s = &t->smp;
    const int N = EC_FFT_N;
    w->frz_lag = ec_lag(w->frz_lag, ec_clampf(p->freeze_amount, 0.f, 1.f), d->w3s.lag10, n);
    w->blur_lag = ec_lag(w->blur_lag, ec_clampf(p->spectral_blur, 0.f, 1.f), d->w3s.lag10, n);
    w->fsh_lag = ec_lag(w->fsh_lag, ec_clampf(p->formant_shift, -24.f, 24.f), d->w3s.lag10, n);
    w->pit_lag = ec_lag(w->pit_lag, ec_clampf(p->pitch, -48.f, 48.f), d->lag05, n);
    double lo = t->lo, span = t->hi - t->lo, fmx = t->frames - 1.0;
    double inc = cps / d->fs * run * dir;
    for (int i = 0; i < n; ++i) {
        float a, b;
        ec_peek_wrap(s, (lo + w->sp_ph * span) * fmx, &a, &b);
        w->sp_in[w->sp_pos] = 0.5f * (a + b) * EC_S16_SCALE;
        float o = w->sp_out[w->sp_pos];
        w->sp_out[w->sp_pos] = 0.f;
        L[i] = R[i] = o;
        w->sp_pos = (w->sp_pos + 1) & (N - 1);
        if (++w->sp_hop >= N / 2) { w->sp_hop = 0; spectral_frame(d, t); }
        w->sp_ph += inc;
        w->sp_ph -= floor(w->sp_ph);
    }
}

/* ------------------------------------------------------------ playhead / region verbs */
double ec_w3_pos(const ec_track_t *t, int fam)
{
    const ec_w3_t *w = &t->w3;
    double span = t->hi - t->lo;
    switch (fam) {
    case FAM_DOMINO: return t->lo + w->lph * span;
    case FAM_WT: return t->lo + (double)w->morph_lag * span;
    case FAM_SPEC: return t->lo + w->sp_ph * span;
    default: return t->lo;
    }
}

void ec_w3_fam_reset(ec_track_t *t, int fam, double phase)
{
    ec_w3_t *w = &t->w3;
    phase -= floor(phase);
    switch (fam) {
    case FAM_DOMINO: {
        double plo, phi;
        play_window(t, &plo, &phi);
        w->lph = phase;
        w->sph = plo + phase * (phi - plo);
        if (t->p.loop_reverse && phase <= 0.0) w->sph = phi - 1e-9;
    } break;
    case FAM_SPEC: w->sp_ph = phase; break;
    default: break;   /* wavetable: no playhead */
    }
}
