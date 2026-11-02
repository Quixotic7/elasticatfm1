/*
 * ec_filter.c -- the per-track FILTER machine + AMP/PAN output stage
 * (\elasticatFilter* SynthDefs with filterPrep / filterDecodeMS /
 * filterBalanceCutoffs / filterModOut), stereo.
 *
 *   drive  : tanh(x * (1 + 12 d)) equal-power blended with the dry signal
 *   cutoff : Lag 0.02, x midiratio(envDepth * fenv * 72), clip 20..20k
 *   res    : rq = 1 - 0.98 res, Lag slew, clip 0.02..1
 *   MSBL   : stereo (A=L, B=R) or mid/side (A=M, B=S); balance spreads the two
 *            cutoffs +-24 st (A down / B up at +1)
 *   classic: SC RLPF / RHPF / BPF / (x - BPF)       morphing: LP->notch->HP
 *   ladder : SC MoogFF (k = 4 res)                   svf: TPT state variable
 *   comb   : tuned feedback comb, DRIVE = damp       formant: 3-BPF vowel bank
 *   out    : Balance2(pan, amp * 2.63), LeakDC, track mix (mute, centre law)
 * Coefficients are computed per control chunk (<= 16 samples) and cached.
 */
#include "ec_internal.h"

#define XF_TYPE_SAMPLES 256

void ec_filter_reset(ec_filter_t *f)
{
    memset(f, 0, sizeof *f);
    f->first = 1;
    f->cur_type = f->prev_type = EC_FTYPE_LP;
    f->type_cached = -1;
    f->mach_cached = -1;
}

/* --- SC two-pole sections ------------------------------------------------ */
static void rlpf_coef(ec_sc2p_t *c, float fc, float rq, float rps)
{
    float qres = rq > 0.001f ? rq : 0.001f;
    float pf = fc * rps;
    float D = tanf(pf * qres * 0.5f);
    float C = (1.f - D) / (1.f + D);
    float cs = cosf(pf);
    c->b1 = (1.f + C) * cs;
    c->b2 = -C;
    c->a0 = (1.f + C - c->b1) * 0.25f;
}
static void rhpf_coef(ec_sc2p_t *c, float fc, float rq, float rps)
{
    rlpf_coef(c, fc, rq, rps);
    c->a0 = (1.f - (c->b2) + c->b1) * 0.25f;   /* (1 + C + b1) / 4, C = -b2 */
}
static void bpf_coef(ec_sc2p_t *c, float fc, float bw, float rps)
{
    float pf = fc * rps;
    float pbw = bw * pf * 0.5f;
    if (pbw > 1.55f) pbw = 1.55f;
    float C = 1.f / tanf(pbw);
    float D = 2.f * cosf(pf);
    c->a0 = 1.f / (1.f + C);
    c->b1 = C * D * c->a0;
    c->b2 = (1.f - C) * c->a0;
}
static inline float rlpf_tick(const ec_sc2p_t *c, float *st, float x)
{
    float y0 = c->a0 * x + c->b1 * st[0] + c->b2 * st[1];
    float o = y0 + 2.f * st[0] + st[1];
    st[1] = st[0]; st[0] = y0;
    return o;
}
static inline float rhpf_tick(const ec_sc2p_t *c, float *st, float x)
{
    float y0 = c->a0 * x + c->b1 * st[0] + c->b2 * st[1];
    float o = y0 - 2.f * st[0] + st[1];
    st[1] = st[0]; st[0] = y0;
    return o;
}
static inline float bpf_tick(const ec_sc2p_t *c, float *st, float x)
{
    float y0 = x + c->b1 * st[0] + c->b2 * st[1];
    float o = c->a0 * (y0 - st[1]);
    st[1] = st[0]; st[0] = y0;
    return o;
}

/* equal-power XFade2 law: pos -1 = a, +1 = b */
static inline void xfade2_gains(float pos, float *ga, float *gb)
{
    float x = (ec_clampf(pos, -1.f, 1.f) + 1.f) * 0.5f;
    *ga = ec_sin_hp(1.f - x);
    *gb = ec_sin_hp(x);
}

static inline float classic_tick(ec_filter_t *f, int ch, int type, float x)
{
    switch (type) {
    case EC_FTYPE_HP: return rhpf_tick(&f->hp[ch], f->st[ch][1], x);
    case EC_FTYPE_BP: return bpf_tick(&f->bp[ch], f->st[ch][2], x);
    case EC_FTYPE_NOTCH: return x - bpf_tick(&f->bp[ch], f->st[ch][2], x);
    default: return rlpf_tick(&f->lp[ch], f->st[ch][0], x);
    }
}

static inline float ladder_tick(ec_filter_t *f, int ch, float x, float b0, float a1)
{
    float *s = f->lad[ch];
    float k = f->lk;
    float b0p4 = b0 * b0 * b0 * b0;
    float o = s[3] + b0 * (s[2] + b0 * (s[1] + b0 * s[0]));
    float outs = (b0p4 * x + o) / (1.f + b0p4 * k);
    float u = x - k * outs;
    float past = u, fut;
    fut = b0 * past + s[0]; s[0] = b0 * past - a1 * fut; past = fut;
    fut = b0 * past + s[1]; s[1] = b0 * past - a1 * fut; past = fut;
    fut = b0 * past + s[2]; s[2] = b0 * past - a1 * fut;
    s[3] = b0 * fut - a1 * outs;
    return outs;
}

static inline void svf_tick(ec_filter_t *f, int ch, float v0, float *lp, float *bp, float *hp)
{
    float *ic = f->svf[ch];
    float v3 = v0 - ic[1];
    float v1 = f->sa1[ch] * ic[0] + f->sa2[ch] * v3;
    float v2 = ic[1] + f->sa2[ch] * ic[0] + f->sa3[ch] * v3;
    ic[0] = 2.f * v1 - ic[0];
    ic[1] = 2.f * v2 - ic[1];
    *lp = v2; *bp = v1; *hp = v0 - f->sk * v1 - v2;
}

static inline float comb_tick(ec_filter_t *f, int ch, float x)
{
    /* y = x + fb * LPF(DelayC(y, dt)) */
    float dts = f->comb_dt[ch];
    float rp = (float)f->comb_w - dts;
    int i1 = (int)floorf(rp);
    float fr = rp - (float)i1;
    const float *buf = f->comb[ch];
    int m = EC_COMB_LEN - 1;
    float d = ec_hermite(fr, buf[(i1 - 1) & m], buf[i1 & m], buf[(i1 + 1) & m], buf[(i1 + 2) & m]);
    float *z = f->clp[ch];
    float y0 = d + f->clp_b1 * z[0] + f->clp_b2 * z[1];
    float lpo = f->clp_a0 * (y0 + 2.f * z[0] + z[1]);
    z[1] = z[0]; z[0] = y0;
    float y = x + lpo * f->comb_fb;
    if (y < 1e-20f && y > -1e-20f) y = 0.f;   /* keep the delay line denormal-free */
    f->comb[ch][f->comb_w & m] = y;
    return y;
}

static const float FMT1[5] = {700, 400, 250, 400, 350};
static const float FMT2[5] = {1220, 2000, 2100, 750, 600};
static const float FMT3[5] = {2600, 2600, 2900, 2400, 2400};
static const float AMP2[5] = {0.5f, 0.45f, 0.35f, 0.28f, 0.12f};
static const float AMP3[5] = {0.28f, 0.22f, 0.22f, 0.2f, 0.1f};

static inline float flush(float x) { return (x < 1e-20f && x > -1e-20f) ? 0.f : x; }

void ec_filter_process(ec_dsp_t *d, ec_track_t *t, float *L, float *R, int n)
{
    const ec_track_params_t *p = &t->p;
    ec_filter_t *f = &t->f;
    float fs = d->fs;
    float rps = (float)(2.0 * EC_PI) / fs;
    int mach = ec_clampi(p->filter_machine, 1, 6);

    /* ---- kr controls ---- */
    float cut = ec_clampf(p->filter_cutoff, 20.f, 20000.f);
    /* E4: RES mod adds +-0.5 to the 0..1 resonance (before the Lag, as filterPrep) */
    float res_m = ec_clampf(ec_clampf(p->filter_res, 0.f, 1.f) + ec_clampf(t->mods[EC_MOD_RES], -1.f, 1.f) * 0.5f,
                            0.f, 1.f);
    float rq_t = ec_clampf(1.f - res_m * 0.98f, 0.02f, 1.f);
    float amp_t = (p->amp > 0.f ? p->amp : 0.f) * 2.63f;
    float pan_t = ec_clampf(p->pan, -1.f, 1.f);
    float morph_t = ec_clampf(p->filter_morph, 0.f, 1.f);
    if (f->first) {
        f->cut_lag = cut; f->rq_lag = rq_t; f->amp_lag = amp_t; f->pan_lag = pan_t;
        f->morph_lag = morph_t;
    }
    f->cut_lag = ec_lag(f->cut_lag, cut, d->lag02, n);
    f->rq_lag = ec_lag(f->rq_lag, rq_t, t->slew_tab, n);
    f->amp_lag = ec_lag(f->amp_lag, amp_t, t->slew_tab, n);
    f->pan_lag = ec_lag(f->pan_lag, pan_t, t->slew_tab, n);
    f->morph_lag = ec_lag(f->morph_lag, morph_t, t->slew_tab, n);

    /* filter envelope (kr: value at chunk start) */
    float fe = 0.f;
    for (int i = 0; i < n; ++i) {
        float v = ec_env_tick(&t->fenv, &t->fenv_spec, fs);
        if (i == 0) fe = v;
    }
    float semis = ec_clampf(p->filter_env_depth, -1.f, 1.f) * fe * 72.f;
    f->env_semis = semis;
    semis += ec_clampf(t->mods[EC_MOD_CUTOFF], -1.f, 1.f) * 120.f;   /* E4: the CUTOFF mod bus */
    float fc = f->cut_lag;
    if (semis != 0.f) fc *= ec_midiratio(semis);
    fc = ec_clampf(fc, 20.f, 20000.f);
    float bal = ec_clampf(p->filter_balance, -1.f, 1.f);
    float fca = fc, fcb = fc;
    if (bal != 0.f) {
        fca = ec_clampf(fc * ec_midiratio(-bal * 24.f), 20.f, 20000.f);
        fcb = ec_clampf(fc * ec_midiratio(bal * 24.f), 20.f, 20000.f);
    }
    float rq = f->rq_lag;
    int type = ec_clampi(p->filter_type, 1, 4);
    if (f->first) { f->cur_type = f->prev_type = type; }
    if (type != f->cur_type) {
        f->prev_type = f->cur_type;
        f->cur_type = type;
        f->xf_left = XF_TYPE_SAMPLES;
        int si = type == EC_FTYPE_HP ? 1 : (type == EC_FTYPE_LP ? 0 : 2);
        int pi = f->prev_type == EC_FTYPE_HP ? 1 : (f->prev_type == EC_FTYPE_LP ? 0 : 2);
        if (si != pi) for (int ch = 0; ch < 2; ++ch) f->st[ch][si][0] = f->st[ch][si][1] = 0.f;
    }

    int recalc = f->first || fca != f->fc_a || fcb != f->fc_b || rq != f->rq || mach != f->mach_cached;
    if (recalc) {
        f->fc_a = fca; f->fc_b = fcb; f->rq = rq; f->mach_cached = mach;
        float fcs[2] = {fca, fcb};
        for (int ch = 0; ch < 2; ++ch) {
            float c = fcs[ch];
            switch (mach) {
            case EC_FILT_CLASSIC:
            case EC_FILT_MORPHING:
                rlpf_coef(&f->lp[ch], c, rq, rps);
                rhpf_coef(&f->hp[ch], c, rq, rps);
                bpf_coef(&f->bp[ch], c, rq, rps);
                break;
            case EC_FILT_LADDER: {
                float twc = 2.f * tanf((float)EC_PI * ec_clampf(c, 20.f, 18000.f) / fs);
                f->sg[ch] = twc / (twc + 2.f);                /* b0 */
                f->sa1[ch] = (twc - 2.f) / (twc + 2.f);       /* a1 */
            } break;
            case EC_FILT_SVF: {
                float g = tanf((float)EC_PI * ec_clampf(c, 20.f, 0.45f * fs) / fs);
                float k = rq;
                float a1 = 1.f / (1.f + g * (g + k));
                f->sa1[ch] = a1; f->sa2[ch] = g * a1; f->sa3[ch] = g * g * a1;
                f->sk = k;
            } break;
            case EC_FILT_COMB: {
                float tune = ec_clampf(c, 40.f, 4000.f);
                f->comb_dt[ch] = ec_clampf(1.f / tune, 3.f / fs, 0.05f) * fs;
            } break;
            case EC_FILT_FORMANT:
                break;
            }
        }
        if (mach == EC_FILT_LADDER)
            f->lk = ec_clampf((1.f - rq) / 0.98f * 4.f, 0.f, 3.98f);
        if (mach == EC_FILT_COMB) f->comb_fb = ec_clampf(1.f - rq, 0.f, 0.97f);
    }
    if (mach == EC_FILT_COMB) {
        /* damp: drive linexp 0..1 -> 16000..600 Hz, SC LPF */
        float df = 16000.f * powf(600.f / 16000.f, ec_clampf(p->filter_drive, 0.f, 1.f));
        float pf = df * rps * 0.5f;
        float C = 1.f / tanf(pf), C2 = C * C, s2c = C * 1.41421356f;
        f->clp_a0 = 1.f / (1.f + s2c + C2);
        f->clp_b1 = -2.f * (1.f - C2) * f->clp_a0;
        f->clp_b2 = -(1.f - s2c + C2) * f->clp_a0;
    }
    if (mach == EC_FILT_FORMANT) {
        float vp = f->morph_lag * 4.f;
        int va = ec_clampi((int)vp, 0, 4), vb = ec_clampi(va + 1, 0, 4);
        float vf = vp - (float)va;
        float rqb = ec_clampf(rq, 0.03f, 0.5f);
        float fcs[2] = {fca, fcb};
        for (int ch = 0; ch < 2; ++ch) {
            float sh = ec_clampf(fcs[ch] / 1000.f, 0.25f, 4.f);
            float f1 = ec_clampf((FMT1[va] * (1 - vf) + FMT1[vb] * vf) * sh, 60.f, 18000.f);
            float f2 = ec_clampf((FMT2[va] * (1 - vf) + FMT2[vb] * vf) * sh, 60.f, 18000.f);
            float f3 = ec_clampf((FMT3[va] * (1 - vf) + FMT3[vb] * vf) * sh, 60.f, 18000.f);
            bpf_coef(&f->fm[ch][0], f1, rqb, rps);
            bpf_coef(&f->fm[ch][1], f2, rqb, rps);
            bpf_coef(&f->fm[ch][2], f3, rqb, rps);
        }
        f->famp2 = AMP2[va] * (1 - vf) + AMP2[vb] * vf;
        f->famp3 = AMP3[va] * (1 - vf) + AMP3[vb] * vf;
    }

    float drive = (mach == EC_FILT_COMB || mach == EC_FILT_FORMANT) ? 0.f
                  : ec_clampf(p->filter_drive, 0.f, 1.f);
    float dry_g = 1.f, wet_g = 0.f, dmul = 1.f + drive * 12.f;
    if (drive > 0.f) xfade2_gains(drive * 2.f - 1.f, &dry_g, &wet_g);
    int ms = (p->filter_mix_mode == 2);
    float mix_a = 1.f, mix_b = 0.f;   /* dry / wet for comb + formant */
    xfade2_gains(ec_clampf(p->filter_mix, 0.f, 1.f) * 2.f - 1.f, &mix_a, &mix_b);
    float m_lo_a = 0, m_lo_b = 0, m_hi_a = 0, m_hi_b = 0;
    int m_high = f->morph_lag >= 0.5f;
    if (mach == EC_FILT_MORPHING) {
        xfade2_gains(ec_clampf(f->morph_lag * 4.f - 1.f, -1.f, 1.f), &m_lo_a, &m_lo_b);
        xfade2_gains(ec_clampf((f->morph_lag - 0.5f) * 4.f - 1.f, -1.f, 1.f), &m_hi_a, &m_hi_b);
    }

    /* output gains: Balance2(pan + PAN mod, amp * (1 + AMP mod) * 2.63) (filterModOut). The TrackMix
     * stage (centre Balance2 x mute) runs after the insert and the send taps (E4, ec_dsp.c). */
    float th = (ec_clampf(f->pan_lag + ec_clampf(t->mods[EC_MOD_PAN], -1.f, 1.f), -1.f, 1.f) + 1.f) * 0.5f;
    float tm = 1.f + ec_clampf(t->mods[EC_MOD_AMP], -1.f, 1.f);
    float gl1 = ec_sin_hp(1.f - th) * f->amp_lag * tm;
    float gr1 = ec_sin_hp(th) * f->amp_lag * tm;
    if (f->first) { f->gl = gl1; f->gr = gr1; }
    float gl0 = f->gl, gr0 = f->gr;
    float ginc_l = (gl1 - gl0) / (float)n, ginc_r = (gr1 - gr0) / (float)n;
    f->gl = gl1; f->gr = gr1;
    f->first = 0;

    for (int i = 0; i < n; ++i) {
        float a = L[i], b = R[i];
        if (drive > 0.f) {
            a = a * dry_g + ec_tanh(a * dmul) * wet_g;
            b = b * dry_g + ec_tanh(b * dmul) * wet_g;
        }
        if (ms) { float m = (a + b) * 0.5f, s = (a - b) * 0.5f; a = m; b = s; }
        float fa, fb;
        switch (mach) {
        case EC_FILT_MORPHING: {
            float lpa = rlpf_tick(&f->lp[0], f->st[0][0], a), lpb = rlpf_tick(&f->lp[1], f->st[1][0], b);
            float hpa = rhpf_tick(&f->hp[0], f->st[0][1], a), hpb = rhpf_tick(&f->hp[1], f->st[1][1], b);
            float na = a - bpf_tick(&f->bp[0], f->st[0][2], a), nb = b - bpf_tick(&f->bp[1], f->st[1][2], b);
            if (m_high) { fa = na * m_hi_a + hpa * m_hi_b; fb = nb * m_hi_a + hpb * m_hi_b; }
            else { fa = lpa * m_lo_a + na * m_lo_b; fb = lpb * m_lo_a + nb * m_lo_b; }
        } break;
        case EC_FILT_LADDER:
            fa = ladder_tick(f, 0, a, f->sg[0], f->sa1[0]);
            fb = ladder_tick(f, 1, b, f->sg[1], f->sa1[1]);
            break;
        case EC_FILT_SVF: {
            float lp, bp, hp;
            svf_tick(f, 0, a, &lp, &bp, &hp);
            fa = type == EC_FTYPE_HP ? hp : type == EC_FTYPE_BP ? bp : type == EC_FTYPE_NOTCH ? lp + hp : lp;
            svf_tick(f, 1, b, &lp, &bp, &hp);
            fb = type == EC_FTYPE_HP ? hp : type == EC_FTYPE_BP ? bp : type == EC_FTYPE_NOTCH ? lp + hp : lp;
        } break;
        case EC_FILT_COMB: {
            float ya = comb_tick(f, 0, a), yb = comb_tick(f, 1, b);
            f->comb_w = (f->comb_w + 1) & (EC_COMB_LEN - 1);
            fa = a * mix_a + ya * mix_b;
            fb = b * mix_a + yb * mix_b;
        } break;
        case EC_FILT_FORMANT: {
            float wa = (bpf_tick(&f->fm[0][0], f->fst[0][0], a)
                       + bpf_tick(&f->fm[0][1], f->fst[0][1], a) * f->famp2
                       + bpf_tick(&f->fm[0][2], f->fst[0][2], a) * f->famp3) * 2.f;
            float wb = (bpf_tick(&f->fm[1][0], f->fst[1][0], b)
                       + bpf_tick(&f->fm[1][1], f->fst[1][1], b) * f->famp2
                       + bpf_tick(&f->fm[1][2], f->fst[1][2], b) * f->famp3) * 2.f;
            fa = a * mix_a + wa * mix_b;
            fb = b * mix_a + wb * mix_b;
        } break;
        default: /* classic */
            fa = classic_tick(f, 0, f->cur_type, a);
            fb = classic_tick(f, 1, f->cur_type, b);
            if (f->xf_left > 0) {
                float x = (float)f->xf_left / (float)XF_TYPE_SAMPLES;
                float pa = classic_tick(f, 0, f->prev_type, a);
                float pb = classic_tick(f, 1, f->prev_type, b);
                fa = fa * (1.f - x) + pa * x;
                fb = fb * (1.f - x) + pb * x;
                f->xf_left--;
            }
            break;
        }
        float l, r;
        if (ms) { l = fa + fb; r = fa - fb; } else { l = fa; r = fb; }
        gl0 += ginc_l; gr0 += ginc_r;
        l *= gl0; r *= gr0;
        /* LeakDC (coef 0.995) */
        float yl = l - f->dc_x1[0] + 0.995f * f->dc_y1[0];
        float yr = r - f->dc_x1[1] + 0.995f * f->dc_y1[1];
        f->dc_x1[0] = l; f->dc_y1[0] = yl;
        f->dc_x1[1] = r; f->dc_y1[1] = yr;
        L[i] = yl;
        R[i] = yr;
    }

    /* denormal flush (states decay slowly enough that per-chunk is plenty) */
    for (int ch = 0; ch < 2; ++ch) {
        for (int k = 0; k < 3; ++k) {
            f->st[ch][k][0] = flush(f->st[ch][k][0]); f->st[ch][k][1] = flush(f->st[ch][k][1]);
            f->fst[ch][k][0] = flush(f->fst[ch][k][0]); f->fst[ch][k][1] = flush(f->fst[ch][k][1]);
        }
        for (int k = 0; k < 4; ++k) f->lad[ch][k] = flush(f->lad[ch][k]);
        f->svf[ch][0] = flush(f->svf[ch][0]); f->svf[ch][1] = flush(f->svf[ch][1]);
        f->clp[ch][0] = flush(f->clp[ch][0]); f->clp[ch][1] = flush(f->clp[ch][1]);
        f->dc_x1[ch] = flush(f->dc_x1[ch]); f->dc_y1[ch] = flush(f->dc_y1[ch]);
    }
}
