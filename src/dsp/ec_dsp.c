/*
 * ec_dsp.c -- engine core: init, parameters, events, the region layers and the
 * live loop-key scrub, the per-track chain and the master bus.
 *
 * Region model (lib/sequencer/track_sequencer.lua + lib/grid_sequencer.lua):
 * three layers resolve the region a loop track plays, per endpoint:
 *   keys  (held loop keys, live gesture)  >  seq (a firing step's region lock)
 *   >  track (params loop_start / loop_end).
 * Track space 0..128 is mapped through the slot trim and the track Range
 * (map_trim_point) to engine fractions of the sample. Region changes go
 * through the same verbs as the Lua: set_region (fences only), set_region_
 * with_phase (crossfaded jump), within_or_warp (keep the absolute position if
 * it is inside, else jump to the start), preserve (boomerang) and the release
 * modes return / boomerang / reset with anchors. The engine knows the exact
 * playhead, so the "return" dead reckoning and position preservation are
 * sample-exact here (the Lua estimated them from 1-15 Hz reports).
 */
#include "ec_internal.h"

/* ------------------------------------------------------------ xforms */
float ec_xf_amount(float v) { return ec_clampf(v / 127.f, 0.f, 1.f); }
float ec_xf_bipolar(float v) { return (v - 64.f) / 64.f; }
float ec_xf_morph(float v) { return ec_clampf(v / 128.f, 0.f, 1.f); }
float ec_xf_cutoff_hz(float v)
{
    float n = ec_clampf(v / 127.f, 0.f, 1.f);
    return 20.f * powf(1000.f, n);
}
float ec_xf_env_seconds(float v, int env_range)
{
    static const float R[10] = {1, 4, 8, 16, 32, 64, 128, 256, 512, 1024};
    if (v >= 128.f) return EC_ENV_INF_SECONDS;
    float n = ec_clampf(v / 127.f, 0.f, 1.f);
    float curve = (expf(6.f * n) - 1.f) / (expf(6.f) - 1.f);
    return R[ec_clampi(env_range, 1, 10) - 1] * curve;
}

void ec_track_params_default(ec_track_params_t *p)
{
    memset(p, 0, sizeof *p);
    p->play = 1;
    p->machine = EC_MACH_LOOP;
    p->loop_end = 128.f;
    p->range_end = 128.f;
    p->trim_end = 1.f;
    p->sample_steps = 16.f;
    p->xfade = 0.010f;
    p->mode = EC_WARP_TAPE;
    p->chop_steps = 16;
    p->chop_slice_len = 1.f;
    p->chop_loop_mode = 3;
    p->chop_attack = 0.002f;
    p->chop_hold = 0.9f;
    p->chop_release = 0.01f;
    p->grain_size = 0.08f;
    p->grain_density = 8.f;
    p->grain_jitter = 0.01f;
    p->grain_speed = 1.f;
    p->grain_direction = 1.f;
    p->warp_rate = 1.f;
    p->slice_count = 16;
    p->slice_play_mode = EC_SLICE_ONESHOT;
    p->slice_sync = 1;
    p->slice_rate = 1.f;
    p->filter_machine = EC_FILT_CLASSIC;
    p->filter_type = EC_FTYPE_LP;
    p->filter_cutoff = ec_xf_cutoff_hz(127.f);
    p->filter_morph = ec_xf_morph(64.f);
    p->filter_mix = 1.f;
    p->filter_mix_mode = 1;
    p->filter_env_mode = EC_ENV_AHR;
    p->filter_env_attack = ec_xf_env_seconds(0.f, 3);
    p->filter_env_decay = ec_xf_env_seconds(64.f, 3);
    p->filter_env_sustain = ec_xf_amount(100.f);
    p->filter_env_release = ec_xf_env_seconds(0.f, 3);
    p->filter_env_hold = EC_ENV_INF_SECONDS;
    p->filter_slew = 0.09f;
    p->env_mode = EC_ENV_AHR;
    p->env_attack = ec_xf_env_seconds(0.f, 3);
    p->env_decay = ec_xf_env_seconds(64.f, 3);
    p->env_sustain = ec_xf_amount(100.f);
    p->env_release = EC_ENV_INF_SECONDS;
    p->env_hold = EC_ENV_INF_SECONDS;
    p->amp = ec_xf_amount(100.f);
    p->mode_switch_fade = 0.05f;
    p->loop_division = 16;
    p->playhead_return = EC_RETURN_BOOMERANG;
    /* E3 */
    p->harm_interval[0] = 7.f;
    p->wsola_window = 0.08f;
    p->wsola_search = 0.015f;
    p->pv_window = 0.2f;
    p->wt_window = 8;
    p->wt_cycle = 600.f;
    p->wt_lfo_shape = 1;
    p->send_tap = 1;                      /* E4 */
    ec_fx_params_default(&p->fx);
    ec_fx_params_default(&p->fx2);
    ec_mod_params_default(&p->mod);
}

/* ------------------------------------------------------------ lag tables */
void ec_lag_table(float *tab, float lag_s, float fs)
{
    /* SC Lag: y = x + b1 (y - x), b1 = exp(log(0.001) / (lag * sr)) */
    double b1 = lag_s > 0.f ? exp(log(0.001) / ((double)lag_s * fs)) : 0.0;
    double c = 1.0;
    for (int i = 0; i <= EC_KR; ++i) { tab[i] = (float)c; c *= b1; }
}

/* ------------------------------------------------------------ mapping */
double ec_track_map(const ec_track_t *t, double x)
{
    const ec_track_params_t *p = &t->p;
    double ts = ec_clampd(p->trim_start, 0.0, 1.0);
    double te = ec_clampd(p->trim_end, 0.0, 1.0);
    if (te <= ts) te = ts + 1e-6 > 1.0 ? 1.0 : ts + 1e-6;
    double span = te - ts;
    double lo = ts + span * ec_clampd(p->range_start, 0.0, 128.0) / 128.0;
    double hi = ts + span * ec_clampd(p->range_end, 0.0, 128.0) / 128.0;
    if (hi <= lo) hi = (lo + 1e-6 < te) ? lo + 1e-6 : te;
    return lo + (hi - lo) * ec_clampd(x, 0.0, 128.0) / 128.0;
}

static int is_loop_machine(int m) { return m == EC_MACH_LOOP || m == EC_MACH_LOOP_TRIG; }

static int warp_family(int mode) { return ec_w3_family(mode); }   /* E3: every mode has its family */

/* derived per-block values */
static void track_derive(ec_dsp_t *d, ec_track_t *t)
{
    const ec_track_params_t *p = &t->p;
    if (!t->has_smp) { t->frames = 4.0; t->brs = 1.0; t->steps_whole = 16.0; t->src_bpm = 120.0; return; }
    t->frames = (double)t->smp.frames;
    t->brs = (double)t->smp.rate / (double)d->fs;
    double tf = ec_clampd((double)p->trim_end - (double)p->trim_start, 0.0, 1.0);
    double steps = p->sample_steps > 0.f ? p->sample_steps : 16.0;
    if (tf > 0.0) steps /= tf;
    t->steps_whole = ec_clampd(steps, 1.0, 512.0);
    double dur = t->frames / (double)t->smp.rate;
    t->src_bpm = (t->steps_whole / 4.0) * 60.0 / dur;
    if (t->src_bpm < 1.0) t->src_bpm = 1.0;
}

/* loop cycles per second for a region of engine fractions [s,e] */
static double track_cps(const ec_dsp_t *d, const ec_track_t *t, double s, double e)
{
    double region = ec_clampd((e - s), 0.0001, 1.0);
    double beats = (t->steps_whole / 4.0) * region;
    if (beats < 0.03125) beats = 0.03125;
    return ((double)d->bpm / 60.0) / beats * t->rate_lag;
}

static double pitch_ratio(const ec_track_t *t)
{
    /* + the PITCH mod bus, +-12 st (E4) */
    return ec_clampd(ec_midiratio(t->pitch_lag + ec_clampf(t->mods[EC_MOD_PITCH], -1.f, 1.f) * 12.f),
                     1.0 / 32.0, 32.0);
}

/* loop cycles per sample (natural_phase_from's loop_rate), for [s,e] */
static double track_cycles_per_sample(const ec_dsp_t *d, const ec_track_t *t, double s, double e)
{
    if (t->fam == FAM_READER && (t->w3.rd_mode == EC_WARP_TAPE || t->w3.rd_mode == EC_WARP_GSTRETCH)) {
        double w = (e - s) * t->frames;
        if (w < 1.0) w = 1.0;
        return fabs(t->w3.rd_mode == EC_WARP_TAPE ? t->brs * pitch_ratio(t) * t->rate_lag
                                                  : t->brs * t->w3.strch_lag * t->rate_lag) / w;
    }
    double c = track_cps(d, t, s, e) / d->fs;
    if (t->fam == FAM_READER && t->w3.rd_mode == EC_WARP_GSTRETCH2) c *= t->w3.strch_lag;   /* E3 */
    if (t->fam == FAM_GRAINS) c *= ec_clampd(t->gspeed_lag, 0.0, 4.0);
    return c;
}

/* ------------------------------------------------------------ playhead */
static double track_pos(const ec_track_t *t)   /* engine fraction */
{
    double F = t->frames > 1.0 ? t->frames : 1.0;
    switch (t->fam) {
    case FAM_GRAINS: return t->lo + t->emit_ph * (t->hi - t->lo);
    case FAM_CHOP: return t->lo + t->ch.ramp * (t->hi - t->lo);
    case FAM_DOMINO: case FAM_WT: case FAM_SPEC: return ec_w3_pos(t, t->fam);   /* E3 */
    default:
        if (ec_reader_idle(&t->rd)) return t->restart_frame / F;
        return t->rd.phase[t->rd.active] / F;
    }
}

static void fam_reset(ec_track_t *t, int fam, double phase)
{
    double F = t->frames;
    switch (fam) {
    case FAM_GRAINS: t->emit_ph = phase - floor(phase); break;
    case FAM_CHOP: t->ch.ramp = phase - floor(phase); break;
    case FAM_READER: {
        double lo = t->lo * F, hi = t->hi * F;
        double target = lo + phase * (hi - lo);
        if (t->p.loop_reverse && phase <= 0.0) target = hi;   /* phase 0 == 1 */
        t->restart_frame = target;
        if (ec_reader_idle(&t->rd)) break;
        int a = t->rd.active;
        if (t->rd.state[a] == RH_PLAY && !t->rd.queued && fabs(target - t->rd.phase[a]) < 0.5) break;
        ec_reader_cut(&t->rd, target);
    } break;
    case FAM_DOMINO: case FAM_WT: case FAM_SPEC: ec_w3_fam_reset(t, fam, phase); break;   /* E3 */
    default: break;
    }
}

static void apply_fences(ec_track_t *t)
{
    double es = ec_clampd(ec_track_map(t, t->cur_s) * 128.0, 0.0, 127.99) / 128.0;
    double ee = ec_clampd(ec_track_map(t, t->cur_e) * 128.0, 0.01, 128.0) / 128.0;
    if (ee <= es) ee = (es + 0.01 / 128.0 < 1.0) ? es + 0.01 / 128.0 : 1.0;
    t->lo = es;
    t->hi = ee;
}

static void resolve(const ec_track_t *t, float *s, float *e)
{
    float a = t->p.loop_start, b = t->p.loop_end;
    if (t->keys) {
        int div = t->p.loop_division;
        div = ec_clampi(((div + 1) / 2) * 2, 2, 32);
        float units = 128.f / (float)div;
        int first = -1, last = -1;
        const uint32_t span = t->keys_span ? t->keys_span : t->keys;   /* 0.6.12: the entry's keys, held or not */
        for (int k = 0; k < 32; ++k)
            if (span & (1u << k)) { if (first < 0) first = k; last = k; }
        a = (float)first * units; if (a > 127.f) a = 127.f;
        b = (float)(last + 1) * units; if (b > 128.f) b = 128.f;
        if (b <= a) { b = a + units; if (b > 128.f) b = 128.f; }
    } else {
        if (t->seq_has_s) a = t->seq_s;
        if (t->seq_has_e) b = t->seq_e;
    }
    if (b <= a) b = (a + 0.01f < 128.f) ? a + 0.01f : 128.f;
    *s = a;
    *e = b;
}

static int region_is_current(const ec_track_t *t, float s, float e)
{
    return t->cur_valid && fabsf(t->cur_s - s) < 0.0001f && fabsf(t->cur_e - e) < 0.0001f;
}

static void set_region(ec_track_t *t, float s, float e)
{
    if (region_is_current(t, s, e)) { apply_fences(t); return; }
    t->cur_s = s; t->cur_e = e; t->cur_valid = 1;
    apply_fences(t);
}

static void set_region_phase(ec_track_t *t, float s, float e, double phase)
{
    t->cur_s = s; t->cur_e = e; t->cur_valid = 1;
    apply_fences(t);
    fam_reset(t, t->fam, phase);
}

static void preserve(ec_track_t *t, float s, float e)
{
    double pos = track_pos(t);
    t->cur_s = s; t->cur_e = e; t->cur_valid = 1;
    apply_fences(t);
    double w = t->hi - t->lo;
    if (w < 1e-12) w = 1e-12;
    double ph = (pos - t->lo) / w;
    fam_reset(t, t->fam, ph - floor(ph));
}

static int within_or_warp(ec_track_t *t, float s, float e)
{
    if (region_is_current(t, s, e)) return 0;
    double pos = track_pos(t);
    t->cur_s = s; t->cur_e = e; t->cur_valid = 1;
    apply_fences(t);
    int inside = pos >= t->lo && pos <= t->hi;
    if (inside) preserve(t, s, e);
    else fam_reset(t, t->fam, 0.0);
    return 1;
}

static ec_anchor_t make_anchor(const ec_dsp_t *d, const ec_track_t *t)
{
    ec_anchor_t a;
    a.valid = 1;
    a.start = t->lo;
    a.end = t->hi;
    a.pos = track_pos(t);
    a.time = d->clock;
    return a;
}

static void release_mode(ec_dsp_t *d, ec_track_t *t, int mode, float s, float e,
                         const ec_anchor_t *an)
{
    if (mode == EC_RETURN_RESET) {
        set_region_phase(t, s, e, 0.0);
    } else if (mode == EC_RETURN_RETURN) {
        if (an && an->valid) {
            double w = an->end - an->start;
            if (w < 1e-12) w = 1e-12;
            double ap = (an->pos - an->start) / w;
            ap -= floor(ap);
            double rate = track_cycles_per_sample(d, t, an->start, an->end);
            double dir = t->p.loop_reverse ? -1.0 : 1.0;
            double ph = ap + dir * (d->clock - an->time) * rate;
            set_region_phase(t, s, e, ph - floor(ph));
        } else {
            set_region_phase(t, s, e, 0.0);
        }
    } else {
        preserve(t, s, e);
    }
}

static int track_playing(const ec_dsp_t *d, const ec_track_t *t)
{
    return d->playing && t->p.play;
}

/* ------------------------------------------------------------ notes */
static void build_specs(ec_track_t *t)
{
    const ec_track_params_t *p = &t->p;
    t->amp_spec.mode = p->env_mode == EC_ENV_ADSR ? EC_ENV_ADSR : EC_ENV_AHR;
    t->amp_spec.atk = p->env_attack > 0.0001f ? p->env_attack : 0.0001f;
    t->amp_spec.dec = p->env_decay > 0.0001f ? p->env_decay : 0.0001f;
    t->amp_spec.sus = ec_clampf(p->env_sustain, 0.f, 1.f);
    t->amp_spec.rel = p->env_release > 0.0001f ? p->env_release : 0.0001f;
    t->amp_spec.hold = p->env_hold > 0.0001f ? p->env_hold : 0.0001f;
    t->fenv_spec.mode = p->filter_env_mode == EC_ENV_ADSR ? EC_ENV_ADSR : EC_ENV_AHR;
    t->fenv_spec.atk = p->filter_env_attack > 0.0001f ? p->filter_env_attack : 0.0001f;
    t->fenv_spec.dec = p->filter_env_decay > 0.0001f ? p->filter_env_decay : 0.0001f;
    t->fenv_spec.sus = ec_clampf(p->filter_env_sustain, 0.f, 1.f);
    t->fenv_spec.rel = p->filter_env_release > 0.0001f ? p->filter_env_release : 0.0001f;
    t->fenv_spec.hold = p->filter_env_hold > 0.0001f ? p->filter_env_hold : 0.0001f;
}

static void note_on(ec_dsp_t *d, ec_track_t *t, int length)
{
    int32_t g = length > 0 ? (length < (int)(0.005f * d->fs) ? (int)(0.005f * d->fs) : length) : -1;
    ec_env_trigger(&t->amp_env, &t->amp_spec, g, t->p.portamento != 0, d->fs);
    ec_env_trigger(&t->fenv, &t->fenv_spec, g, 0, d->fs);
}

static void note_off(ec_dsp_t *d, ec_track_t *t)
{
    ec_env_note_off(&t->amp_env, &t->amp_spec, d->fs);
    ec_env_note_off(&t->fenv, &t->fenv_spec, d->fs);
    ec_mod_note_off(&t->mod, &t->p.mod, d->fs);   /* E4: menvReleaseTrig */
}

/* ------------------------------------------------------------ events */
static void ev_loop_key(ec_dsp_t *d, ec_track_t *t, const ec_event_t *ev)
{
    if (!is_loop_machine(t->p.machine)) return;
    if (ev->slice == 0) {                       /* 0.6.1: KEYBOARD, stopped: the whole loop while a key is held */
        float s, e;
        if (ev->value <= 0.5f) {                /* (a release ends it even if the transport started meanwhile) */
            if (t->audition && !t->keys) {
                t->audition = 0;
                if (!track_playing(d, t)) note_off(d, t);
            }
            if (t->live_gate && !t->keys) {     /* 0.6.62: the live note's gate closes */
                t->live_gate = 0;
                note_off(d, t);
            }
            return;
        }
        if (track_playing(d, t) && t->p.machine == EC_MACH_LOOP_TRIG && !t->keys) {
            resolve(t, &s, &e);                  /* 0.6.62 (owner): playing, TRIG: a KEYBOARD note sounds from its
                                                  * region's start while held, as a trig does */
            set_region_phase(t, s, e, 0.0);
            ec_env_trigger(&t->amp_env, &t->amp_spec, -1, 0, d->fs);
            ec_env_trigger(&t->fenv, &t->fenv_spec, -1, 0, d->fs);
            t->live_gate = 1;
            return;
        }
        if (track_playing(d, t) || t->keys || !(ev->flags & EC_TF_AUDITION)) return;
        resolve(t, &s, &e);
        set_region_phase(t, s, e, 0.0);
        ec_env_trigger(&t->amp_env, &t->amp_spec, -1, 0, d->fs);
        ec_env_trigger(&t->fenv, &t->fenv_spec, -1, 0, d->fs);
        t->audition = 1;
        return;
    }
    int k = ec_clampi(ev->slice, 1, 32) - 1;
    float s, e;
    int playing = track_playing(d, t);
    if (ev->value > 0.5f) {
        if (!t->keys && playing) t->keys_anchor = make_anchor(d, t);
        {                                        /* 0.6.21 (owner): a press refits the span to the keys held now
                                                  * (smaller or larger); a release keeps it. 0.6.23: a held key's
                                                  * direct neighbour pressed again, the span being just those keys,
                                                  * takes the span back to the held keys alone */
            const uint32_t with = t->keys | (1u << k);
            const int nb = (k > 0 && (t->keys >> (k - 1) & 1u)) || (t->keys >> (k + 1) & 1u);
            t->keys_span = (t->keys_span == with && t->keys && nb) ? t->keys : with;
            t->keys = with;
        }
        resolve(t, &s, &e);
        if (playing && t->p.machine == EC_MACH_LOOP_TRIG) {   /* 0.6.62 (owner): TRIG, playing: the key's region
                                                  * sounds from its start while held (a step's gate otherwise) */
            set_region_phase(t, s, e, 0.0);
            if (!t->live_gate) { ec_env_trigger(&t->amp_env, &t->amp_spec, -1, 0, d->fs);
                                 ec_env_trigger(&t->fenv, &t->fenv_spec, -1, 0, d->fs); }
            t->live_gate = 1;
        } else if (playing) {
            within_or_warp(t, s, e);
        } else {
            set_region_phase(t, s, e, 0.0);
            if (ev->flags & EC_TF_AUDITION) {
                if (!t->audition) { ec_env_trigger(&t->amp_env, &t->amp_spec, -1, 0, d->fs);
                                    ec_env_trigger(&t->fenv, &t->fenv_spec, -1, 0, d->fs); }
                t->audition = 1;
            }
        }
    } else {
        if (!(t->keys & (1u << k))) return;
        t->keys &= ~(1u << k);
        if (t->keys) return;                     /* a key up keeps the span (0.6.21: the next press refits it) */
        t->keys_span = 0;
        resolve(t, &s, &e);
        {
            if (playing) release_mode(d, t, t->p.playhead_return, s, e, &t->keys_anchor);
            else set_region(t, s, e);
            t->keys_anchor.valid = 0;
            if (t->audition) { t->audition = 0; note_off(d, t); }
            if (t->live_gate) { t->live_gate = 0; note_off(d, t); }   /* 0.6.62 */
        }
    }
}

static void chord_off(ec_dsp_t *d, int ti, int slot);
static void ev_trig(ec_dsp_t *d, int ti, const ec_event_t *ev)
{
    ec_track_t *t = &d->tr[ti];
    int m = t->p.machine;
    int has_lock = ev->start >= 0.f || ev->end >= 0.f;
    float s, e;
    if (m == EC_MACH_LOOP) {
        if (!(ev->flags & EC_TF_GHOST)) note_on(d, t, ev->length);
        if (has_lock) {
            if (ev->start >= 0.f) { t->seq_has_s = 1; t->seq_s = ev->start; }
            if (ev->end >= 0.f) { t->seq_has_e = 1; t->seq_e = ev->end; }
            resolve(t, &s, &e);
            ec_anchor_t pending;
            pending.valid = 0;
            if (!t->seq_anchor.valid) pending = make_anchor(d, t);
            int interrupted;
            if (ev->flags & EC_TF_JUMP) { set_region_phase(t, s, e, 0.0); interrupted = 1; }
            else interrupted = within_or_warp(t, s, e);
            if (interrupted) {
                if (!t->seq_anchor.valid) t->seq_anchor = pending;
                t->seq_release_mode = ev->mode >= 1 && ev->mode <= 3 ? ev->mode : EC_RETURN_RETURN;
            }
        }
    } else if (m == EC_MACH_LOOP_TRIG) {
        if (!(ev->flags & EC_TF_GHOST)) {
            note_on(d, t, ev->length);
            chord_off(d, ti, -1);                    /* 0.6.91: a new trig ends the last chord (as it ends its note) */
        }
        t->seq_has_s = ev->start >= 0.f; t->seq_s = ev->start;
        t->seq_has_e = ev->end >= 0.f; t->seq_e = ev->end;
        resolve(t, &s, &e);
        set_region_phase(t, s, e, 0.0);
        t->lt_open = 1;
        t->lt_left = ev->length > 0 ? ev->length : -1;
    } else if (m == EC_MACH_GRID_SLICE || m == EC_MACH_SLICE_POLY ||
               m == EC_MACH_RAZOR_SLICE || m == EC_MACH_RAZOR_POLY) {
        ec_voice_slice_on(d, ti, ev);
    }
}

/* 0.6.91: a chord's extra note: a copy of the track's engine, from the region's start */
static void chord_on(ec_dsp_t *d, int ti, const ec_event_t *ev)
{
    ec_track_t *t = &d->tr[ti], *c;
    int i, best = -1;
    float s, e;
    if (!t->has_smp || !is_loop_machine(t->p.machine)) return;
    for (i = 0; i < EC_CHORD_VOICES && best < 0; ++i)          /* its slot, else a free one, else the oldest */
        if (d->cvm[i].used && d->cvm[i].track == ti && d->cvm[i].slot == ev->slice) best = i;
    for (i = 0; i < EC_CHORD_VOICES && best < 0; ++i)
        if (!d->cvm[i].used) best = i;
    if (best < 0) {
        best = 0;
        for (i = 1; i < EC_CHORD_VOICES; ++i)
            if (d->cvm[i].serial < d->cvm[best].serial) best = i;
    }
    c = &d->cv[best];
    memcpy(c, t, sizeof *c);
    c->keys = 0; c->keys_span = 0; c->keys_anchor.valid = 0; c->seq_anchor.valid = 0;
    c->audition = 0; c->live_gate = 0; c->prev_fam = FAM_NONE; c->mode_gain = 1.f;
    c->seq_has_s = ev->start >= 0.f; c->seq_s = ev->start;
    c->seq_has_e = ev->end >= 0.f; c->seq_e = ev->end;
    resolve(c, &s, &e);
    set_region_phase(c, s, e, 0.0);
    c->p.pitch = ev->pitch;
    c->pitch_lag = ec_clampf(ev->pitch, -48.f, 48.f);
    ec_env_reset(&c->amp_env);
    ec_env_trigger(&c->amp_env, &c->amp_spec, ev->length > 0 ? ev->length : -1, 0, d->fs);
    d->cvm[best].used = 1;
    d->cvm[best].track = ti;
    d->cvm[best].slot = ev->slice;
    d->cvm[best].pitch = ev->pitch;
    d->cvm[best].vel = ec_clampf(ev->velocity, 0.f, 1.f);
    d->cvm[best].serial = ++d->serial;
    d->cvm[best].on = 1;
    d->cvm[best].left = ev->length > 0 ? ev->length : -1;
    d->cvm[best].gate = 0.f;
}

/* release (a timed note too: a key up, the transport stopping, the track's next trig) */
static void chord_off(ec_dsp_t *d, int ti, int slot)
{
    for (int i = 0; i < EC_CHORD_VOICES; ++i)
        if (d->cvm[i].used && d->cvm[i].track == ti && (slot < 0 || d->cvm[i].slot == slot)) {
            ec_env_gate_off(&d->cv[i].amp_env, &d->cv[i].amp_spec, d->fs);
            d->cvm[i].on = 0;                      /* (its play gate fades, as the track's when its trig ends) */
        }
}

static void ev_region_release(ec_dsp_t *d, ec_track_t *t)
{
    float s, e;
    t->seq_has_s = t->seq_has_e = 0;
    resolve(t, &s, &e);
    if (t->seq_anchor.valid && !t->keys && track_playing(d, t)) {
        release_mode(d, t, t->seq_release_mode ? t->seq_release_mode : EC_RETURN_RETURN, s, e,
                     &t->seq_anchor);
    } else {
        set_region(t, s, e);
    }
    t->seq_anchor.valid = 0;
}

static void handle_event(ec_dsp_t *d, const ec_event_t *ev)
{
    int ti = ev->track;
    ec_track_t *t = (ti >= 0 && ti < EC_TRACKS) ? &d->tr[ti] : 0;
    switch (ev->type) {
    case EC_EV_TRANSPORT:
        if (ev->value > 0.5f) {
            d->playing = 1;
            for (int i = 0; i < EC_TRACKS; ++i) d->tr[i].idle = d->tr[i].quiet = 0;   /* E4 */
            if (ev->flags & EC_TF_RESET) {
                for (int i = 0; i < EC_TRACKS; ++i) {
                    ec_track_t *x = &d->tr[i];
                    float s, e;
                    x->seq_has_s = x->seq_has_e = 0;
                    x->seq_anchor.valid = 0;
                    x->keys_anchor.valid = 0;
                    x->ch.imp = 0.0; x->ch.primed = 1;
                    resolve(x, &s, &e);
                    if (is_loop_machine(x->p.machine)) set_region_phase(x, s, e, 0.0);
                    else set_region(x, s, e);
                }
            }
        } else {
            d->playing = 0;
            for (int i = 0; i < EC_TRACKS; ++i) {
                ec_voice_release_all(d, i);
                d->tr[i].lt_open = 0;
                chord_off(d, i, -1);                  /* 0.6.91: the chord notes release too */
            }
        }
        break;
    case EC_EV_TEMPO: d->bpm = ec_clampf(ev->value, 1.f, 999.f); break;
    case EC_EV_PANIC:
        ec_voice_steal_all(d, -1);
        /* E4 (panicClearFx): ringing insert / send / master tails stop too */
        for (int i = 0; i < EC_TRACKS; ++i) { ec_fx_clear(&d->tr[i].fx); ec_fx_clear(&d->tr[i].fx2); }
        for (int b = 0; b < 3; ++b) ec_fx_clear(&d->bus[b]);
        break;
    default:
        if (!t) break;
        t->idle = 0; t->quiet = 0;            /* E4: idleWake */
        if ((ev->type == EC_EV_SLICE_ON || (ev->type == EC_EV_TRIG && !is_loop_machine(t->p.machine))))
            t->mod.vel = ec_clampf(ev->velocity, 0.f, 1.f);   /* E4: modVelocity latch */
        switch (ev->type) {
        case EC_EV_MOD_TRIG:
            ec_mod_trig(&t->mod, &t->p.mod, (ev->flags & EC_TF_LFO) != 0, (ev->flags & EC_TF_MENV) != 0,
                        ev->length > 0 ? ev->length : -1, d->fs);
            break;
        case EC_EV_TRIG: ev_trig(d, ti, ev); break;
        case EC_EV_NOTE_ON: note_on(d, t, ev->length); break;
        case EC_EV_NOTE_OFF: note_off(d, t); break;
        case EC_EV_REGION_RELEASE: ev_region_release(d, t); break;
        case EC_EV_REGION: {
            if (ev->start >= 0.f) t->p.loop_start = ev->start;
            if (ev->end >= 0.f) t->p.loop_end = ev->end;
            float s, e;
            resolve(t, &s, &e);
            switch (ev->mode) {
            case EC_RM_PHASE: set_region_phase(t, s, e, ev->phase); break;
            case EC_RM_WITHIN: within_or_warp(t, s, e); break;
            case EC_RM_PRESERVE: preserve(t, s, e); break;
            default: set_region(t, s, e); break;
            }
        } break;
        case EC_EV_SLICE_ON: ec_voice_slice_on(d, ti, ev); break;
        case EC_EV_SLICE_OFF: ec_voice_release_slot(d, ti, ev->slice); break;
        case EC_EV_LOOP_KEY: ev_loop_key(d, t, ev); break;
        case EC_EV_CHORD_ON: chord_on(d, ti, ev); break;      /* 0.6.91 */
        case EC_EV_CHORD_OFF: chord_off(d, ti, ev->slice); break;
        case EC_EV_RELEASE_ALL: ec_voice_release_all(d, ti); break;
        default: break;
        }
    }
}

/* ------------------------------------------------------------ init / api */
/* E4: the state, then the FX slots' delay memory (inserts 0..5, S1, S2, master; 0.6.1: then the second inserts) */
#define EC_DSP_STATE_BYTES ((sizeof(ec_dsp_t) + 15u) & ~(size_t)15u)
#define EC_DSP_MEM_BYTES (EC_DSP_STATE_BYTES + (size_t)EC_FX_SLOTS * EC_FX_MEM_FLOATS * sizeof(float))
/* compile-time: the engine fits the static arenas sized EC_DSP_MEM_MAX (C99: a negative array size fails) */
#define EC_FX_SLOTS (2 * EC_TRACKS + 3)
typedef char ec_dsp_mem_fits[EC_DSP_MEM_BYTES <= EC_DSP_MEM_MAX ? 1 : -1];
size_t ec_dsp_mem_size(void)
{
    return EC_DSP_MEM_BYTES;
}

static void track_init(ec_track_t *t, int i)
{
    memset(t, 0, sizeof *t);
    ec_track_params_default(&t->p);
    t->frames = 4.0;
    t->brs = 1.0;
    t->steps_whole = 16.0;
    t->src_bpm = 120.0;
    t->cur_e = 128.f;
    t->cur_valid = 1;
    t->lo = 0.0; t->hi = 1.0;
    t->fam = FAM_READER;
    t->mode = EC_WARP_TAPE;
    t->mode_gain = 1.f;
    ec_reader_init(&t->rd);
    t->gr.rng = 0x2545F491u ^ (uint32_t)(i * 7919 + 1);
    ec_chop_reset(&t->ch, 0.0);
    ec_w3_reset(&t->w3, 0x85EBCA6Bu ^ (uint32_t)(i * 2246822519u + 7u));   /* E3 */
    t->rate_lag = 1.f;
    t->gsize_lag = 0.08f;
    t->gspread_lag = 0.02f;
    t->gspeed_lag = 1.f;
    t->mute_gain = 1.f;
    t->slew_cached = -1.f;
    ec_env_reset(&t->amp_env);
    ec_env_reset(&t->fenv);
    ec_filter_reset(&t->f);
    for (int s = 0; s <= 32; ++s) t->slot_voice[s] = -1;
    build_specs(t);
}

ec_dsp_t *ec_dsp_init(void *mem, float fs)
{
    ec_dsp_t *d = (ec_dsp_t *)mem;
    memset(d, 0, sizeof *d);
    d->fs = fs > 1000.f ? fs : 44100.f;
    d->bpm = 120.f;
    d->master_vol = d->master_lag = 1.f;
    d->rng = 0x1B873593u;
    for (int i = 0; i <= EC_HANN_SIZE; ++i)
        d->hann[i] = 0.5f * (1.f - cosf(2.f * (float)EC_PI * (float)i / (float)EC_HANN_SIZE));
    ec_lag_table(d->lag002, 0.002f, d->fs);
    ec_lag_table(d->lag01, 0.01f, d->fs);
    ec_lag_table(d->lag02, 0.02f, d->fs);
    ec_lag_table(d->lag03, 0.03f, d->fs);
    ec_lag_table(d->lag05, 0.05f, d->fs);
    ec_w3_init(d);   /* E3 */
    for (int i = 0; i < EC_TRACKS; ++i) track_init(&d->tr[i], i);
    /* E4: FX slots, mod */
    float *fxmem = (float *)((uint8_t *)mem + EC_DSP_STATE_BYTES);
    for (int i = 0; i < EC_TRACKS; ++i) {
        ec_fx_init(&d->tr[i].fx, fxmem + (size_t)i * EC_FX_MEM_FLOATS, d->fs, 0, 0xA5A5u + (uint32_t)i * 977u);
        ec_mod_init(&d->tr[i].mod, 0x51ED27u + (uint32_t)i * 7919u);
    }
    for (int b = 0; b < 3; ++b) {
        ec_fx_init(&d->bus[b], fxmem + (size_t)(EC_TRACKS + b) * EC_FX_MEM_FLOATS, d->fs, b < 2, 0xBEEFu + (uint32_t)b);
        ec_fx_params_default(&d->busp[b]);
    }
    for (int i = 0; i < EC_TRACKS; ++i)   /* 0.6.1: the second inserts */
        ec_fx_init(&d->tr[i].fx2, fxmem + (size_t)(EC_TRACKS + 3 + i) * EC_FX_MEM_FLOATS, d->fs, 0,
                   0x5A5Au + (uint32_t)i * 1031u);
    d->idle_grace = 4.0f;
    return d;
}

void ec_dsp_track_params(ec_dsp_t *d, int track, const ec_track_params_t *p)
{
    if (track < 0 || track >= EC_TRACKS || !p) return;
    ec_track_t *t = &d->tr[track];
    int old_machine = t->p.machine;
    t->p = *p;
    t->idle = 0; t->quiet = 0;   /* E4: a param change wakes an idle track */
    build_specs(t);
    if (old_machine != p->machine) {
        t->keys = 0;
        t->keys_span = 0;
        t->keys_anchor.valid = 0;
        t->seq_anchor.valid = 0;
        t->seq_has_s = t->seq_has_e = 0;
        t->lt_open = 0;
        if (!is_loop_machine(p->machine)) ec_voice_release_all(d, track);
    }
}

const ec_track_params_t *ec_dsp_get_track_params(const ec_dsp_t *d, int track)
{
    if (track < 0 || track >= EC_TRACKS) return 0;
    return &d->tr[track].p;
}

void ec_dsp_set_sample(ec_dsp_t *d, int track, const ec_sample_t *s)
{
    if (track < 0 || track >= EC_TRACKS) return;
    ec_track_t *t = &d->tr[track];
    double frac = track_pos(t);
    t->idle = 0; t->quiet = 0;   /* E4 */
    ec_voice_kill_track(d, track);
    /* declick: the old audio cannot be read any more; ramp its last value out */
    t->dk_l = t->last_l; t->dk_r = t->last_r;
    t->dk_left = 128;
    t->dk_step = 1.f / 128.f;
    ec_reader_init(&t->rd);
    t->gr.n = 0;
    t->ch.h[0].stage = t->ch.h[1].stage = 0;
    t->w3.ong = 0;                                    /* E3: grains / slicer heads read the old memory */
    t->w3.sv[0].state = t->w3.sv[1].state = 3;
    t->w3.sv_prev = -1;
    if (s && s->data && s->frames >= 4 && s->rate > 0.f) {
        t->smp = *s;
        if (t->smp.channels != 1) t->smp.channels = 2;
        t->has_smp = 1;
    } else {
        memset(&t->smp, 0, sizeof t->smp);
        t->has_smp = 0;
    }
    track_derive(d, t);
    t->restart_frame = ec_clampd(frac, 0.0, 1.0) * t->frames;
}

void ec_dsp_set_master_volume(ec_dsp_t *d, float v) { d->master_vol = ec_clampf(v, 0.f, 1.5f); }
void ec_dsp_set_tempo(ec_dsp_t *d, float bpm) { d->bpm = ec_clampf(bpm, 1.f, 999.f); }
float ec_dsp_tempo(const ec_dsp_t *d) { return d->bpm; }
int ec_dsp_playing(const ec_dsp_t *d) { return d->playing; }

int ec_dsp_event(ec_dsp_t *d, const ec_event_t *ev)
{
    if (d->q.n >= EC_MAX_EVENTS) return -1;
    d->q.ev[d->q.n++] = *ev;
    return 0;
}

static ec_event_t ev_blank(int offset, int type, int track)
{
    ec_event_t e;
    memset(&e, 0, sizeof e);
    e.offset = offset; e.type = type; e.track = track;
    e.start = e.end = -1.f;
    e.velocity = 1.f;
    return e;
}
int ec_dsp_note_on(ec_dsp_t *d, int offset, int track, int length)
{
    ec_event_t e = ev_blank(offset, EC_EV_NOTE_ON, track);
    e.length = length;
    return ec_dsp_event(d, &e);
}
int ec_dsp_loop_key(ec_dsp_t *d, int offset, int track, int key, int pressed, int flags)
{
    ec_event_t e = ev_blank(offset, EC_EV_LOOP_KEY, track);
    e.slice = key; e.value = pressed ? 1.f : 0.f; e.flags = flags;
    return ec_dsp_event(d, &e);
}
int ec_dsp_transport(ec_dsp_t *d, int offset, int start, int reset)
{
    ec_event_t e = ev_blank(offset, EC_EV_TRANSPORT, 0);
    e.value = start ? 1.f : 0.f;
    e.flags = reset ? EC_TF_RESET : 0;
    return ec_dsp_event(d, &e);
}

/* ------------------------------------------------------------ track chain */
static void block_prep(ec_dsp_t *d, ec_track_t *t)
{
    const ec_track_params_t *p = &t->p;
    track_derive(d, t);
    float fsl = ec_clampf(p->filter_slew, 0.001f, 0.5f);
    if (fsl != t->slew_cached) { ec_lag_table(t->slew_tab, fsl, d->fs); t->slew_cached = fsl; }
    float s, e;
    resolve(t, &s, &e);
    set_region(t, s, e);   /* follows live edits of the base region (fences only) */

    int mode = ec_clampi(p->mode, 1, EC_WARP_COUNT);
    /* E3: the FFT budget (the original ran the spectral chain on track 1 only, tape elsewhere) */
    t->w3_fallback = 0;
    if ((mode == EC_WARP_SPECTRAL_FREEZE || mode == EC_WARP_FORMANT) && (t - d->tr) >= EC_W3_SPECTRAL_TRACKS) {
        mode = EC_WARP_TAPE;
        t->w3_fallback = 1;
    }
    if (warp_family(mode) == FAM_READER && mode != t->w3.rd_mode)
        ec_w3_set_reader_mode(&t->w3, mode, p->mode_switch_fade, d->fs);
    if (mode != t->mode) {
        int nf = warp_family(mode);
        if (nf != t->fam) {
            double w = t->hi - t->lo;
            double ph = w > 1e-12 ? (track_pos(t) - t->lo) / w : 0.0;
            ph -= floor(ph);
            t->prev_fam = t->fam;
            t->prev_gain = t->mode_gain;
            t->mode_gain = 0.f;
            t->mode_fade_inc = 1.f / (ec_clampf(p->mode_switch_fade, 0.001f, 0.25f) * d->fs);
            t->fam = nf;
            if (nf == FAM_READER) ec_reader_init(&t->rd);
            if (nf == FAM_GRAINS) t->gr.n = 0;
            if (nf == FAM_CHOP) { t->ch.h[0].stage = t->ch.h[1].stage = 0; t->ch.primed = 1; }
            if (nf == FAM_DOMINO) { t->w3.sv[0].state = t->w3.sv[1].state = 3; t->w3.sv_prev = -1; }   /* E3 */
            if (nf == FAM_WT) { t->w3.wcommitted = 0; t->w3.wst[0] = t->w3.wst[1] = 0; t->w3.wqueued = 0; }
            if (nf == FAM_SPEC) { memset(t->w3.sp_in, 0, sizeof t->w3.sp_in);
                                  memset(t->w3.sp_out, 0, sizeof t->w3.sp_out); t->w3.sp_hop = 0; }
            t->mode = mode;
            fam_reset(t, nf, ph);
        }
        t->mode = mode;
    }
}

static void fam_render(ec_dsp_t *d, ec_track_t *t, int fam, double run, float *L, float *R, int n)
{
    const ec_track_params_t *p = &t->p;
    double F = t->frames;
    double dir = p->loop_reverse ? -1.0 : 1.0;
    double cps = track_cps(d, t, t->lo, t->hi);
    switch (fam) {
    case FAM_READER: {
        double lo = t->lo * F, hi = t->hi * F;
        double rate = ec_w3_reader_rate(d, t, cps) * dir * run;   /* E3: per reader-family mode */
        float fi = 1.f / (ec_clampf(p->xfade, 0.0005f, 0.25f) * d->fs);
        ec_reader_process(&t->rd, &t->smp, lo, hi, rate, t->restart_frame, fi, L, R, n);
        ec_w3_post(d, t, cps, L, R, n);   /* E3: pitch_corrected / harmonizer / random_ola / gstretch(2) */
    } break;
    case FAM_DOMINO: ec_w3_domino(d, t, cps, run, dir, L, R, n); break;    /* E3 */
    case FAM_WT: ec_w3_wavetable(d, t, L, R, n); break;
    case FAM_SPEC: ec_w3_spectral(d, t, cps, run, dir, L, R, n); break;
    case FAM_GRAINS: {
        double inc = cps * ec_clampd(t->gspeed_lag, 0.0, 4.0) * run * dir / d->fs;
        double rp = ec_clampd(t->lo + t->emit_ph * (t->hi - t->lo), 0.0, 0.999999);
        double einc = inc * (t->hi - t->lo) * F;
        double density = ec_clampd(p->grain_density, 1.0, 64.0);
        /* 0.6.15 (owner): GSPD slows each grain's own read as well as the cursor (0.05 at the floor: a crawl, never
         * a stall); 1 = the track's pitch, 4 = four times */
        double gread = ec_clampd(t->gspeed_lag, 0.05, 4.0);
        ec_grains_process(&t->gr, &t->smp, d->hann, rp * F, einc, density,
                          (double)t->gsize_lag * d->fs, (double)t->gspread_lag * (double)t->smp.rate,   /* 0.6.14: seconds, not a file fraction */
                          t->brs * pitch_ratio(t) * gread, p->grain_speed_rand, p->grain_direction,
                          L, R, n);
        t->emit_ph += inc * n;
        t->emit_ph -= floor(t->emit_ph);
        float g = (float)(3.0 / sqrt(density)) * EC_S16_SCALE;
        for (int i = 0; i < n; ++i) { L[i] *= g; R[i] *= g; }
    } break;
    case FAM_CHOP:
        ec_chop_process(d, t, cps, t->brs * pitch_ratio(t), run, dir, L, R, n);
        break;
    default:
        for (int i = 0; i < n; ++i) L[i] = R[i] = 0.f;
        break;
    }
}

/* E4: one track's send taps into the S1 / S2 buses (\elasticatSendTap, levels ramped over the chunk) */
static void send_taps(ec_dsp_t *d, const ec_track_t *t, float s10, float s20, const float *L, const float *R,
                      int n)
{
    float i1 = (t->snd1 - s10) / (float)n, i2 = (t->snd2 - s20) / (float)n, a = s10, b = s20;
    for (int i = 0; i < n; ++i) {
        a += i1; b += i2;
        d->s1l[i] += L[i] * a; d->s1r[i] += R[i] * a;
        d->s2l[i] += L[i] * b; d->s2r[i] += R[i] * b;
    }
}

static int track_has_voices(const ec_dsp_t *d, int ti)
{
    for (int i = 0; i < EC_VOICE_POOL; ++i)
        if (d->v[i].used && d->v[i].track == ti) return 1;
    for (int i = 0; i < EC_CHORD_VOICES; ++i)      /* 0.6.91: the chord copies too (the idle saver waits for them) */
        if (d->cvm[i].used && d->cvm[i].track == ti) return 1;
    return 0;
}

/* 0.6.91: the track's chord notes for this chunk: each copy follows the track's params, mods and smoothers (its own
 * pitch, region, phase and envelope), rendered by the same warp family, added in before the filter */
static void chord_render(ec_dsp_t *d, int ti, float *L, float *R, int n)
{
    const ec_track_t *t = &d->tr[ti];
    for (int k = 0; k < EC_CHORD_VOICES; ++k) {
        ec_track_t *c = &d->cv[k];
        if (!d->cvm[k].used || d->cvm[k].track != ti) continue;
        if (!t->has_smp || t->smp.frames != c->smp.frames || !is_loop_machine(t->p.machine)) {
            d->cvm[k].used = 0;                       /* (the sample or the machine changed: gone) */
            continue;
        }
        {
            const ec_anchor_t ka = c->keys_anchor;
            const int hs = c->seq_has_s, he = c->seq_has_e;
            const float ss = c->seq_s, se = c->seq_e;
            c->p = t->p;
            c->p.pitch = d->cvm[k].pitch;
            c->keys_anchor = ka;
            c->seq_has_s = hs; c->seq_has_e = he; c->seq_s = ss; c->seq_e = se;
        }
        c->amp_spec = t->amp_spec;
        c->smp = t->smp;
        c->frames = t->frames; c->brs = t->brs; c->steps_whole = t->steps_whole; c->src_bpm = t->src_bpm;
        c->rate_lag = t->rate_lag; c->gsize_lag = t->gsize_lag; c->gspread_lag = t->gspread_lag;
        c->gspeed_lag = t->gspeed_lag;
        memcpy(c->mods, t->mods, sizeof c->mods);
        c->pitch_lag = ec_lag(c->pitch_lag, ec_clampf(c->p.pitch, -48.f, 48.f), d->lag03, n);
        if (d->cvm[k].on && d->cvm[k].left > 0 && (d->cvm[k].left -= n) <= 0)
            d->cvm[k].on = 0;                      /* its length over */
        fam_render(d, c, c->fam, 1.0, d->tl, d->tr2, n);
        {
            const float v = d->cvm[k].vel, g0 = d->cvm[k].gate;
            const float g1 = ec_lag(g0, d->cvm[k].on ? 1.f : 0.f, d->lag01, n);
            const float gi = (g1 - g0) / (float)n;
            float g = g0;
            for (int i = 0; i < n; ++i) {
                const float a = ec_env_tick(&c->amp_env, &c->amp_spec, d->fs) * v * (g += gi);
                L[i] += d->tl[i] * a;
                R[i] += d->tr2[i] * a;
            }
            d->cvm[k].gate = g1;
            if ((!d->cvm[k].on && g1 < 1e-4f) || c->amp_env.stage == ENV_DONE)
                d->cvm[k].used = 0;                /* faded out (or its envelope ended) */
        }
    }
}

static void track_chunk(ec_dsp_t *d, int ti, float *L, float *R, int n)
{
    ec_track_t *t = &d->tr[ti];
    const ec_track_params_t *p = &t->p;
    float fs = d->fs;
    for (int i = 0; i < n; ++i) L[i] = R[i] = 0.f;

    /* loop_trig gate */
    if (t->lt_open && t->lt_left > 0) {
        t->lt_left -= n;
        if (t->lt_left <= 0) {
            float s, e;
            t->lt_open = 0;
            t->seq_has_s = t->seq_has_e = 0;
            resolve(t, &s, &e);
            set_region(t, s, e);
        }
    }
    int loopm = is_loop_machine(p->machine);
    int run = t->has_smp && loopm &&
              ((track_playing(d, t) && (p->machine == EC_MACH_LOOP || t->lt_open || t->live_gate)) || t->audition);

    /* kr smoothers */
    float pg0 = t->play_gate;
    t->play_gate = ec_lag(t->play_gate, run ? 1.f : 0.f, d->lag01, n);
    t->rate_lag = ec_lag(t->rate_lag, ec_clampf(p->warp_rate, 0.03125f, 8.f), d->lag02, n);
    t->pitch_lag = ec_lag(t->pitch_lag, ec_clampf(p->pitch, -48.f, 48.f), d->lag03, n);
    t->gsize_lag = ec_lag(t->gsize_lag, ec_clampf(p->grain_size, 0.002f, 0.5f), d->lag05, n);
    t->gspread_lag = ec_lag(t->gspread_lag, ec_clampf(p->grain_jitter, 0.f, 0.5f), d->lag05, n);   /* +-SPRD seconds */
    t->gspeed_lag = ec_lag(t->gspeed_lag, ec_clampf(p->grain_speed, 0.f, 4.f), d->lag05, n);
    float mute0 = t->mute_gain;
    t->mute_gain = ec_lag(t->mute_gain, p->mute ? 0.f : 1.f, d->lag03, n);
    float pg1 = t->play_gate;
    /* E4: the mod sources for this chunk (pitch reaches the readers below) */
    ec_mod_tick(&t->mod, &p->mod, d->bpm, fs, d->lag01, n, t->mods);

    t->reader_on = t->has_smp && loopm && (run || pg0 > 0.f || pg1 > 0.f);
    if (t->reader_on) {
        float *tl = d->tl, *tr = d->tr2;
        fam_render(d, t, t->fam, run ? 1.0 : 0.0, tl, tr, n);
        float mg0 = t->mode_gain, prev0 = t->prev_gain;
        if (t->prev_fam != FAM_NONE) {
            fam_render(d, t, t->prev_fam, run ? 1.0 : 0.0, d->ol, d->orr, n);
        }
        float pgi = (pg1 - pg0) / (float)n, pg = pg0;
        for (int i = 0; i < n; ++i) {
            pg += pgi;
            float ae = ec_env_tick(&t->amp_env, &t->amp_spec, fs);
            float gcur = 1.f, gprev = 0.f;
            if (t->prev_fam != FAM_NONE) {
                mg0 += t->mode_fade_inc; if (mg0 > 1.f) mg0 = 1.f;
                prev0 -= t->mode_fade_inc; if (prev0 < 0.f) prev0 = 0.f;
                gcur = sqrtf(mg0);
                gprev = sqrtf(prev0);
            }
            float g = pg * ae;
            L[i] = (tl[i] * gcur + (t->prev_fam != FAM_NONE ? d->ol[i] * gprev : 0.f)) * g;
            R[i] = (tr[i] * gcur + (t->prev_fam != FAM_NONE ? d->orr[i] * gprev : 0.f)) * g;
        }
        if (t->prev_fam != FAM_NONE) {
            t->mode_gain = mg0;
            t->prev_gain = prev0;
            if (prev0 <= 0.f) t->prev_fam = FAM_NONE;
        } else {
            t->mode_gain = 1.f;
        }
        if (!run && pg1 < 1e-4f) t->play_gate = 0.f;
    } else {
        for (int i = 0; i < n; ++i) ec_env_tick(&t->amp_env, &t->amp_spec, fs);
        if (t->prev_fam != FAM_NONE) { t->prev_fam = FAM_NONE; t->mode_gain = 1.f; }
    }

    /* slice voices of this track (pre-filter, like the SC sourceGroup) */
    ec_voices_process(d, ti, L, R, n);
    chord_render(d, ti, L, R, n);                  /* 0.6.91: a chord's extra notes, through the same warp engine */
    /* 0.6.56 (owner): 6 dB of headroom: the sources enter the chain (filter, drive, inserts, sends, the buses) at half
     * level, so nothing inside clips; the neighbour's pass-through (already lowered) is not lowered again */
    for (int i = 0; i < n; ++i) { L[i] *= EC_HEADROOM; R[i] *= EC_HEADROOM; }

    /* E3: neighbor -- the source is the LEFT track's whole output for this chunk (d->nbl / nbr, filled by
     * ec_dsp_process; that track then skips the master). A pass-through like \elasticatNeighborReader:
     * independent of this track's transport, no envelope, warp or pitch -- this track's filter / amp / pan
     * (and E4 insert) shape it. */
    if (p->machine == EC_MACH_NEIGHBOR && ti > 0)
        for (int i = 0; i < n; ++i) { L[i] += d->nbl[i]; R[i] += d->nbr[i]; }
    /* E6: INPUT -- the live input (\elasticatInputReader, SoundIn [0, 1]): the same pass-through, frame i of the
     * caller's input block at output frame i (ec_dsp_input); past its end, or with none, silence */
    if (p->machine == EC_MACH_INPUT && d->in) {
        const int m = d->in_frames - d->in_at < n ? d->in_frames - d->in_at : n;
        const float *x = d->in + 2 * d->in_at;
        for (int i = 0; i < m; ++i) { L[i] += x[2 * i] * EC_HEADROOM; R[i] += x[2 * i + 1] * EC_HEADROOM; }
    }

    if (t->dk_left > 0) {
        for (int i = 0; i < n && t->dk_left > 0; ++i) {
            float g = (float)t->dk_left * t->dk_step;
            L[i] += t->dk_l * g; R[i] += t->dk_r * g;
            t->dk_left--;
        }
    }
    t->last_l = L[n - 1];
    t->last_r = R[n - 1];

    ec_filter_process(d, t, L, R, n);

    /* E4: send taps (pre / post insert), the insert, the track mix (mute, centre Balance2) */
    float s10 = t->snd1, s20 = t->snd2;
    t->snd1 = ec_lag(t->snd1, ec_clampf(p->send1_level, 0.f, 1.f), d->lag02, n);
    t->snd2 = ec_lag(t->snd2, ec_clampf(p->send2_level, 0.f, 1.f), d->lag02, n);
    int post = p->send_tap == 2, sends = s10 > 0.f || s20 > 0.f || t->snd1 > 0.f || t->snd2 > 0.f;
    if (sends && !post) send_taps(d, t, s10, s20, L, R, n);
    if (!ec_fx_bypassed(&t->fx, &p->fx))                          /* 0.6.1: an empty insert costs nothing */
        ec_fx_process(&t->fx, &p->fx, d->bpm, d->ml, d->mr, L, R, n);   /* duck key: the tracks mixed so far */
    if (!ec_fx_bypassed(&t->fx2, &p->fx2))                        /* 0.6.1: INS 2, after INS 1 */
        ec_fx_process(&t->fx2, &p->fx2, d->bpm, d->ml, d->mr, L, R, n);
    if (sends && post) send_taps(d, t, s10, s20, L, R, n);
    {
        float g0 = mute0 * 0.70710678f, g1 = t->mute_gain * 0.70710678f, gi = (g1 - g0) / (float)n, g = g0;
        for (int i = 0; i < n; ++i) { g += gi; L[i] *= g; R[i] *= g; }
    }

    float pl = 0.f, pr = 0.f, pm = 0.f;
    for (int i = 0; i < n; ++i) {
        float a = fabsf(L[i]), b = fabsf(R[i]), m = fabsf(L[i] + R[i]);
        if (a > pl) pl = a;
        if (b > pr) pr = b;
        if (m > pm) pm = m;
    }
    if (pl > t->peak_l) t->peak_l = pl;
    if (pr > t->peak_r) t->peak_r = pr;

    /* E4: the idle detector (\elasticatTrackMix's Amplitude(out, 0.01, 0.3) + Engine.idleScan): a stopped
     * track, no voices, no reader, silent (<= 0.001) for idle_grace seconds pauses until something wakes it */
    t->mix_l = pm > t->mix_l ? pm : t->mix_l * powf(0.1f, (float)n / (0.3f * fs));
    /* (idleEligible: a routing track -- NEIGH, INPUT -- passes audio regardless of its transport, never paused) */
    if (!track_playing(d, t) && !t->reader_on && !t->audition && t->dk_left <= 0 && t->mix_l <= 0.001f &&
        p->machine != EC_MACH_NEIGHBOR && p->machine != EC_MACH_INPUT && !track_has_voices(d, ti)) {
        t->quiet += n;
        if ((float)t->quiet > d->idle_grace * fs) t->idle = 1;
    } else
        t->quiet = 0;
}

static void bus_peak(float *pk, const float *L, const float *R, int n)
{
    for (int i = 0; i < n; ++i) {
        float a = fabsf(L[i]), b = fabsf(R[i]);
        if (a > pk[0]) pk[0] = a;
        if (b > pk[1]) pk[1] = b;
    }
}

/* 0.6.56 (owner): the output's soft limiter: linear to half scale, then a smooth knee (slope 1 at the knee) that never
 * reaches full scale (0.98), so the output never hard-clips */
static inline float soft_limit(float x)
{
    const float k = 0.5f, c = 0.98f;
    float a = fabsf(x);
    if (a <= k) return x;
    float y = k + (c - k) * ec_tanh((a - k) / (c - k));
    return x < 0.f ? -y : y;
}

/* E6: the live input for the next ec_dsp_process calls (consumed frame by frame as they render) */
void ec_dsp_input(ec_dsp_t *d, const float *in_lr, int frames)
{
    d->in = in_lr && frames > 0 ? in_lr : 0;
    d->in_frames = d->in ? frames : 0;
    d->in_at = 0;
}

void ec_dsp_process(ec_dsp_t *d, float *out, int frames)
{
    if (frames <= 0) return;
    /* stable sort of the queued events by offset (insertion; n is small) */
    ec_evq_t *q = &d->q;
    for (int i = 0; i < q->n; ++i) q->ev[i].offset = ec_clampi(q->ev[i].offset, 0, frames - 1);
    for (int i = 1; i < q->n; ++i) {
        ec_event_t x = q->ev[i];
        int j = i - 1;
        while (j >= 0 && q->ev[j].offset > x.offset) { q->ev[j + 1] = q->ev[j]; --j; }
        q->ev[j + 1] = x;
    }
    for (int i = 0; i < EC_TRACKS; ++i) {
        ec_track_t *t = &d->tr[i];
        t->peak_l *= 0.95f;
        t->peak_r *= 0.95f;
        block_prep(d, t);
    }
    d->peak_l *= 0.95f;
    d->peak_r *= 0.95f;
    for (int b = 0; b < 3; ++b) { d->bus_peak[b][0] *= 0.95f; d->bus_peak[b][1] *= 0.95f; }

    int pos = 0, ei = 0;
    while (pos < frames) {
        while (ei < q->n && q->ev[ei].offset <= pos) handle_event(d, &q->ev[ei++]);
        int end = pos + EC_KR;
        if (end > frames) end = frames;
        if (ei < q->n && q->ev[ei].offset < end) end = q->ev[ei].offset;
        int n = end - pos;

        for (int i = 0; i < n; ++i) d->ml[i] = d->mr[i] = d->s1l[i] = d->s1r[i] = d->s2l[i] = d->s2r[i] = 0.f;
        for (int ti = 0; ti < EC_TRACKS; ++ti) {
            const int to_neighbor = ti + 1 < EC_TRACKS && d->tr[ti + 1].p.machine == EC_MACH_NEIGHBOR;
            if (d->tr[ti].idle) {                      /* E4: the idle CPU saver */
                if (to_neighbor)
                    for (int i = 0; i < n; ++i) d->nbl[i] = d->nbr[i] = 0.f;
                continue;
            }
            track_chunk(d, ti, d->bl, d->br, n);
            /* Neighbor tap (SC applyNeighborRouting: the left track's \elasticatTrackMix output is re-pointed
             * at the right track's neighborBus): track_chunk's output = after the filter, the send taps, the
             * insert FX and the track mix (mute, Balance2). The left track's sends were fed inside track_chunk
             * as usual; only its master contribution moves to the neighbor. */
            if (to_neighbor) {
                for (int i = 0; i < n; ++i) { d->nbl[i] = d->bl[i]; d->nbr[i] = d->br[i]; }
                continue;
            }
            for (int i = 0; i < n; ++i) { d->ml[i] += d->bl[i]; d->mr[i] += d->br[i]; }
        }
        /* E4: the S1 / S2 returns (DUCK keyed by the tracks' sum), then the master insert (key = its input) */
        ec_fx_process(&d->bus[0], &d->busp[0], d->bpm, d->ml, d->mr, d->s1l, d->s1r, n);
        ec_fx_process(&d->bus[1], &d->busp[1], d->bpm, d->ml, d->mr, d->s2l, d->s2r, n);
        bus_peak(d->bus_peak[0], d->s1l, d->s1r, n);
        bus_peak(d->bus_peak[1], d->s2l, d->s2r, n);
        for (int i = 0; i < n; ++i) {
            d->ml[i] += d->s1l[i] + d->s2l[i];
            d->mr[i] += d->s1r[i] + d->s2r[i];
            d->kl[i] = d->ml[i];
            d->kr[i] = d->mr[i];
        }
        ec_fx_process(&d->bus[2], &d->busp[2], d->bpm, d->kl, d->kr, d->ml, d->mr, n);
        bus_peak(d->bus_peak[2], d->ml, d->mr, n);
        float v0 = d->master_lag;
        d->master_lag = ec_lag(d->master_lag, d->master_vol, d->lag02, n);
        float vi = (d->master_lag - v0) / (float)n, v = v0;
        float *o = out + 2 * pos;
        for (int i = 0; i < n; ++i) {
            v += vi;
            float l = soft_limit(d->ml[i] * v), r = soft_limit(d->mr[i] * v);
            o[2 * i] = l;
            o[2 * i + 1] = r;
            float a = fabsf(l), b = fabsf(r);
            if (a > d->peak_l) d->peak_l = a;
            if (b > d->peak_r) d->peak_r = b;
        }
        d->clock += n;
        d->in_at += n;                                 /* E6 */
        pos = end;
    }
    while (ei < q->n) handle_event(d, &q->ev[ei++]);
    q->n = 0;
    for (int i = 0; i < EC_TRACKS; ++i) d->tr[i].ui_pos = track_pos(&d->tr[i]);
    ec_load_update(d, frames);                         /* 0.6.1: the counted load */
}

/* ------------------------------------------------------------ status */
void ec_dsp_mod_live(ec_dsp_t *d, int track, float at, float mw, float expr)
{
    if (!d || track < 0 || track >= EC_TRACKS) return;
    d->tr[track].mod.live[0] = ec_clampf(at, 0.f, 1.f);
    d->tr[track].mod.live[1] = ec_clampf(mw, 0.f, 1.f);
    d->tr[track].mod.live[2] = ec_clampf(expr, 0.f, 1.f);
}

void ec_dsp_mod_sources(const ec_dsp_t *d, int track, float *src)
{
    for (int k = 0; k < EC_MSRC_N; ++k)
        src[k] = d && track >= 0 && track < EC_TRACKS && d->tr[track].mod.alive ? d->tr[track].mod.src[k] : 0.f;
}

void ec_dsp_track_status(const ec_dsp_t *d, int track, ec_track_status_t *st)
{
    memset(st, 0, sizeof *st);
    if (track < 0 || track >= EC_TRACKS) return;
    const ec_track_t *t = &d->tr[track];
    double w = t->hi - t->lo;
    st->pos_sample = (float)t->ui_pos;
    double pr = w > 1e-12 ? (t->ui_pos - t->lo) / w : 0.0;
    st->pos_region = (float)(pr - floor(pr));
    st->region_start = (float)t->lo;
    st->region_end = (float)t->hi;
    st->reader_on = t->reader_on;
    st->keys_held = t->keys;
    st->chord_voices = 0;                          /* 0.6.91 */
    for (int k = 0; k < EC_CHORD_VOICES; ++k)
        st->chord_voices += d->cvm[k].used && d->cvm[k].track == track;
    st->peak_l = t->peak_l;
    st->peak_r = t->peak_r;
    st->filter_env = t->f.env_semis;
    st->mode_implemented = !t->w3_fallback;
    st->fx_running = ec_fx_running(&t->fx);               /* E4 */
    st->fx2_running = ec_fx_running(&t->fx2);             /* 0.6.1 */
    ec_fx_meter(&t->fx, &st->fx_meter);
    for (int k = 0; k < EC_MOD_NDEST; ++k) st->mod[k] = t->mods[k];
    for (int k = 0; k < 4; ++k) st->macro[k] = t->mod.alive ? t->mod.macro_val[k] : 0.f;
    for (int k = 0; k < EC_MSRC_N; ++k) st->src[k] = t->mod.alive ? t->mod.src[k] : 0.f;
    st->idle = t->idle;
    double F = t->frames > 1.0 ? t->frames : 1.0;
    for (int i = 0; i < EC_VOICE_POOL; ++i) {
        const ec_voice_t *v = &d->v[i];
        if (!v->used || v->track != track) continue;
        if (st->voices < EC_MAX_VOICES_TRACK) {
            st->voice_slice[st->voices] = v->slot;
            st->voice_pos[st->voices] = (float)(v->pos / F);
        }
        st->voices++;
    }
}

void ec_dsp_bus_params(ec_dsp_t *d, int bus, const ec_fx_params_t *p)
{
    if (bus < 0 || bus > 2 || !p) return;
    d->busp[bus] = *p;
}

void ec_dsp_bus_status(const ec_dsp_t *d, int bus, ec_bus_status_t *st)
{
    memset(st, 0, sizeof *st);
    if (bus < 0 || bus > 2) return;
    st->peak_l = d->bus_peak[bus][0];
    st->peak_r = d->bus_peak[bus][1];
    st->fx_running = ec_fx_running(&d->bus[bus]);
    ec_fx_meter(&d->bus[bus], &st->fx_meter);
}

void ec_dsp_master_peak(const ec_dsp_t *d, float *l, float *r)
{
    if (l) *l = d->peak_l;
    if (r) *r = d->peak_r;
}

int ec_dsp_total_voices(const ec_dsp_t *d)
{
    int c = 0;
    for (int i = 0; i < EC_VOICE_POOL; ++i) c += d->v[i].used ? 1 : 0;
    return c;
}
