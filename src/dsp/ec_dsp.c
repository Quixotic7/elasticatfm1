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

static int warp_family(int mode)
{
    switch (mode) {
    case EC_WARP_GRANULAR: return FAM_GRAINS;
    case EC_WARP_CHOPPED: return FAM_CHOP;
    default: return FAM_READER;   /* tape, tempo_varispeed + not-yet-ported modes */
    }
}

static int warp_implemented(int mode)
{
    return mode == EC_WARP_TAPE || mode == EC_WARP_TEMPO_VARISPEED ||
           mode == EC_WARP_GRANULAR || mode == EC_WARP_CHOPPED;
}

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
    return ec_clampd(ec_midiratio(t->pitch_lag), 1.0 / 32.0, 32.0);
}

/* loop cycles per sample (natural_phase_from's loop_rate), for [s,e] */
static double track_cycles_per_sample(const ec_dsp_t *d, const ec_track_t *t, double s, double e)
{
    if (t->fam == FAM_READER && t->mode == EC_WARP_TAPE) {
        double w = (e - s) * t->frames;
        if (w < 1.0) w = 1.0;
        return fabs(t->brs * pitch_ratio(t) * t->rate_lag) / w;
    }
    double c = track_cps(d, t, s, e) / d->fs;
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
        for (int k = 0; k < 32; ++k)
            if (t->keys & (1u << k)) { if (first < 0) first = k; last = k; }
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
}

/* ------------------------------------------------------------ events */
static void ev_loop_key(ec_dsp_t *d, ec_track_t *t, const ec_event_t *ev)
{
    if (!is_loop_machine(t->p.machine)) return;
    int k = ec_clampi(ev->slice, 1, 32) - 1;
    float s, e;
    int playing = track_playing(d, t);
    if (ev->value > 0.5f) {
        if (!t->keys && playing) t->keys_anchor = make_anchor(d, t);
        t->keys |= (1u << k);
        resolve(t, &s, &e);
        if (playing) {
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
        resolve(t, &s, &e);
        if (t->keys) {
            set_region(t, s, e);
        } else {
            if (playing) release_mode(d, t, t->p.playhead_return, s, e, &t->keys_anchor);
            else set_region(t, s, e);
            t->keys_anchor.valid = 0;
            if (t->audition) { t->audition = 0; note_off(d, t); }
        }
    }
}

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
        if (!(ev->flags & EC_TF_GHOST)) note_on(d, t, ev->length);
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
            }
        }
        break;
    case EC_EV_TEMPO: d->bpm = ec_clampf(ev->value, 1.f, 999.f); break;
    case EC_EV_PANIC: ec_voice_steal_all(d, -1); break;
    default:
        if (!t) break;
        switch (ev->type) {
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
        case EC_EV_RELEASE_ALL: ec_voice_release_all(d, ti); break;
        default: break;
        }
    }
}

/* ------------------------------------------------------------ init / api */
size_t ec_dsp_mem_size(void) { return sizeof(ec_dsp_t); }

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
    for (int i = 0; i < EC_TRACKS; ++i) track_init(&d->tr[i], i);
    return d;
}

void ec_dsp_track_params(ec_dsp_t *d, int track, const ec_track_params_t *p)
{
    if (track < 0 || track >= EC_TRACKS || !p) return;
    ec_track_t *t = &d->tr[track];
    int old_machine = t->p.machine;
    t->p = *p;
    build_specs(t);
    if (old_machine != p->machine) {
        t->keys = 0;
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
    ec_voice_kill_track(d, track);
    /* declick: the old audio cannot be read any more; ramp its last value out */
    t->dk_l = t->last_l; t->dk_r = t->last_r;
    t->dk_left = 128;
    t->dk_step = 1.f / 128.f;
    ec_reader_init(&t->rd);
    t->gr.n = 0;
    t->ch.h[0].stage = t->ch.h[1].stage = 0;
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
        double lo = t->lo * F, hi = t->hi * F, rate;
        if (t->mode == EC_WARP_TAPE)
            rate = t->brs * pitch_ratio(t) * t->rate_lag * dir * run;
        else
            rate = (hi - lo) * cps / d->fs * dir * run;
        float fi = 1.f / (ec_clampf(p->xfade, 0.0005f, 0.25f) * d->fs);
        ec_reader_process(&t->rd, &t->smp, lo, hi, rate, t->restart_frame, fi, L, R, n);
    } break;
    case FAM_GRAINS: {
        double inc = cps * ec_clampd(t->gspeed_lag, 0.0, 4.0) * run * dir / d->fs;
        double rp = ec_clampd(t->lo + t->emit_ph * (t->hi - t->lo), 0.0, 0.999999);
        double einc = inc * (t->hi - t->lo) * F;
        double density = ec_clampd(p->grain_density, 1.0, 64.0);
        ec_grains_process(&t->gr, &t->smp, d->hann, rp * F, einc, density,
                          (double)t->gsize_lag * d->fs, (double)t->gspread_lag * F,
                          t->brs * pitch_ratio(t), p->grain_speed_rand, p->grain_direction,
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
              ((track_playing(d, t) && (p->machine == EC_MACH_LOOP || t->lt_open)) || t->audition);

    /* kr smoothers */
    float pg0 = t->play_gate;
    t->play_gate = ec_lag(t->play_gate, run ? 1.f : 0.f, d->lag01, n);
    t->rate_lag = ec_lag(t->rate_lag, ec_clampf(p->warp_rate, 0.03125f, 8.f), d->lag02, n);
    t->pitch_lag = ec_lag(t->pitch_lag, ec_clampf(p->pitch, -48.f, 48.f), d->lag03, n);
    t->gsize_lag = ec_lag(t->gsize_lag, ec_clampf(p->grain_size, 0.002f, 0.5f), d->lag05, n);
    t->gspread_lag = ec_lag(t->gspread_lag, ec_clampf(2.f * p->grain_jitter, 0.f, 1.f), d->lag05, n);
    t->gspeed_lag = ec_lag(t->gspeed_lag, ec_clampf(p->grain_speed, 0.f, 4.f), d->lag05, n);
    t->mute_gain = ec_lag(t->mute_gain, p->mute ? 0.f : 1.f, d->lag03, n);
    float pg1 = t->play_gate;

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

    /* E3 hook: neighbor machine (track ti-1's output as the source) goes here. */

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

    /* E4 hooks: per-track insert FX, then send taps S1 / S2 (pre/post insert). */

    float pl = 0.f, pr = 0.f;
    for (int i = 0; i < n; ++i) {
        float a = fabsf(L[i]), b = fabsf(R[i]);
        if (a > pl) pl = a;
        if (b > pr) pr = b;
    }
    if (pl > t->peak_l) t->peak_l = pl;
    if (pr > t->peak_r) t->peak_r = pr;
}

static inline float soft_limit(float x)
{
    float a = fabsf(x);
    if (a <= 0.8f) return x;
    float y = 0.8f + 0.2f * ec_tanh((a - 0.8f) * 5.f);
    return x < 0.f ? -y : y;
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

    int pos = 0, ei = 0;
    while (pos < frames) {
        while (ei < q->n && q->ev[ei].offset <= pos) handle_event(d, &q->ev[ei++]);
        int end = pos + EC_KR;
        if (end > frames) end = frames;
        if (ei < q->n && q->ev[ei].offset < end) end = q->ev[ei].offset;
        int n = end - pos;

        for (int i = 0; i < n; ++i) d->ml[i] = d->mr[i] = 0.f;
        for (int ti = 0; ti < EC_TRACKS; ++ti) {
            track_chunk(d, ti, d->bl, d->br, n);
            for (int i = 0; i < n; ++i) { d->ml[i] += d->bl[i]; d->mr[i] += d->br[i]; }
        }
        /* E4 hooks: send buses S1/S2 return here, then the master insert. */
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
        pos = end;
    }
    while (ei < q->n) handle_event(d, &q->ev[ei++]);
    q->n = 0;
    for (int i = 0; i < EC_TRACKS; ++i) d->tr[i].ui_pos = track_pos(&d->tr[i]);
}

/* ------------------------------------------------------------ status */
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
    st->peak_l = t->peak_l;
    st->peak_r = t->peak_r;
    st->filter_env = t->f.env_semis;
    st->mode_implemented = warp_implemented(t->mode);
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
