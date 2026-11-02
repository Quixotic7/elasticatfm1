/*
 * ec_voices.c -- slice voices (\elasticatSliceVoice_raw / _grain) and the voice
 * budget of ElasticatTrack.triggerSlice + Engine.registerSliceVoice:
 *   - same-slice retrigger steals the slot's previous voice (20 ms fade);
 *   - mono machines (grid_slice / razor_slice) steal every voice of the track;
 *   - choke group G > 0 steals the track's live voices in G;
 *   - per-track cap 8 (oldest of the track), then global cap 24 (oldest overall).
 * Play modes: one-shot (gate = read reaches the slice end), hold, loop
 * (wrapping phasor), continue (to the sample end), ping-pong (fold), continue-
 * loop. One gated envelope per voice (ADSR, or AHR = ADSR with sustain 1),
 * release bounded to 2 s, the voice frees itself at the end.
 */
#include "ec_internal.h"

static void voice_free(ec_dsp_t *d, int vi)
{
    ec_voice_t *v = &d->v[vi];
    if (!v->used) return;
    ec_track_t *t = &d->tr[v->track];
    if (v->slot >= 1 && v->slot <= 32 && t->slot_voice[v->slot] == vi) t->slot_voice[v->slot] = -1;
    v->used = 0;
    v->counted = 0;
}

static void voice_steal(ec_dsp_t *d, int vi)
{
    ec_voice_t *v = &d->v[vi];
    if (!v->used) return;
    ec_track_t *t = &d->tr[v->track];
    if (v->slot >= 1 && v->slot <= 32 && t->slot_voice[v->slot] == vi) t->slot_voice[v->slot] = -1;
    v->counted = 0;
    if (!v->stealing) {
        v->stealing = 1;
        v->steal_gain = 1.f;
        v->steal_dec = 1.f / (0.02f * d->fs);
    }
}

static void voice_gate_off(ec_dsp_t *d, ec_voice_t *v)
{
    if (v->play_mode == 0) return;   /* one-shot ignores the external gate */
    ec_env_gate_off(&v->env, &v->spec, d->fs);
}

void ec_voice_release_slot(ec_dsp_t *d, int track, int slot)
{
    if (track < 0 || track >= EC_TRACKS || slot < 1 || slot > 32) return;
    ec_track_t *t = &d->tr[track];
    int vi = t->slot_voice[slot];
    if (vi < 0) return;
    ec_voice_t *v = &d->v[vi];
    t->slot_voice[slot] = -1;
    v->counted = 0;              /* releaseSlice forgets it from the budget */
    voice_gate_off(d, v);
}

void ec_voice_release_all(ec_dsp_t *d, int track)
{
    for (int s = 1; s <= 32; ++s) ec_voice_release_slot(d, track, s);
}

void ec_voice_steal_all(ec_dsp_t *d, int track)
{
    for (int i = 0; i < EC_VOICE_POOL; ++i)
        if (d->v[i].used && (track < 0 || d->v[i].track == track)) voice_steal(d, i);
}

void ec_voice_kill_track(ec_dsp_t *d, int track)
{
    for (int i = 0; i < EC_VOICE_POOL; ++i)
        if (d->v[i].used && d->v[i].track == track) voice_free(d, i);
}

static int oldest_counted(ec_dsp_t *d, int track)
{
    int best = -1;
    uint32_t bs = 0;
    for (int i = 0; i < EC_VOICE_POOL; ++i) {
        ec_voice_t *v = &d->v[i];
        if (!v->used || !v->counted) continue;
        if (track >= 0 && v->track != track) continue;
        uint32_t age = d->serial - v->serial;
        if (best < 0 || age > bs) { best = i; bs = age; }
    }
    return best;
}

static int count_counted(ec_dsp_t *d, int track)
{
    int c = 0;
    for (int i = 0; i < EC_VOICE_POOL; ++i)
        if (d->v[i].used && d->v[i].counted && (track < 0 || d->v[i].track == track)) ++c;
    return c;
}

static int pool_alloc(ec_dsp_t *d)
{
    int best = -1;
    uint32_t bs = 0;
    for (int i = 0; i < EC_VOICE_POOL; ++i) if (!d->v[i].used) return i;
    /* all busy: drop the oldest voice that is already fading out after a steal */
    for (int i = 0; i < EC_VOICE_POOL; ++i) {
        uint32_t age = d->serial - d->v[i].serial;
        if (d->v[i].stealing && (best < 0 || age > bs)) { best = i; bs = age; }
    }
    if (best < 0) best = oldest_counted(d, -1);
    if (best < 0) best = 0;
    voice_free(d, best);
    return best;
}

void ec_voice_slice_on(ec_dsp_t *d, int track, const ec_event_t *ev)
{
    if (track < 0 || track >= EC_TRACKS) return;
    ec_track_t *t = &d->tr[track];
    const ec_track_params_t *p = &t->p;
    if (!t->has_smp) return;
    float fs = d->fs;
    int idx = ec_clampi(ev->slice, 1, 32);
    int chk = ec_clampi(ev->choke, 0, 8);
    double s128, e128;
    if (ev->start < 0.f || ev->end < 0.f) {
        int cnt = ec_clampi(p->slice_count, 1, 32);
        double w = 128.0 / cnt;
        s128 = (idx - 1) * w;
        e128 = idx * w;
    } else {
        s128 = ev->start;
        e128 = ev->end;
    }
    /* map_trim_point, then the engine's clip */
    double es = ec_clampd(ec_track_map(t, s128) * 128.0, 0.0, 127.99);
    double ee = ec_clampd(ec_track_map(t, e128) * 128.0, 0.01, 128.0);
    if (ee <= es) ee = es + 0.01 > 128.0 ? 128.0 : es + 0.01;
    /* E3: ev->mode 1..6 = a per-slice play mode (razor editor PLAY), else the track's */
    int mode = ec_clampi((ev->mode >= 1 && ev->mode <= 6 ? ev->mode : p->slice_play_mode) - 1, 0, 5);
    int rev = p->slice_reverse ? 1 : 0;
    float pitch = ec_clampf(ev->pitch, -48.f, 48.f);
    double pr = ec_midiratio(pitch);
    if (pr < 0.001) pr = 0.001;
    double F = t->frames;
    double dur;
    if (ev->length > 0) dur = (double)ev->length / fs;
    else dur = ((F * fabs(ee - es) / 128.0) / (double)t->smp.rate) / pr;
    dur = ec_clampd(dur, 0.005, 60.0);

    if (p->machine == EC_MACH_GRID_SLICE || p->machine == EC_MACH_RAZOR_SLICE)
        ec_voice_steal_all(d, track);
    if (t->slot_voice[idx] >= 0) voice_steal(d, t->slot_voice[idx]);
    if (chk > 0)
        for (int i = 0; i < EC_VOICE_POOL; ++i)
            if (d->v[i].used && d->v[i].counted && d->v[i].track == track && d->v[i].choke == chk)
                voice_steal(d, i);
    while (count_counted(d, track) >= EC_MAX_VOICES_TRACK) voice_steal(d, oldest_counted(d, track));
    while (count_counted(d, -1) >= EC_MAX_VOICES) voice_steal(d, oldest_counted(d, -1));

    int vi = pool_alloc(d);
    ec_voice_t *v = &d->v[vi];
    memset(v, 0, offsetof(ec_voice_t, g));
    v->used = 1;
    v->track = track;
    v->slot = idx;
    v->counted = 1;
    v->choke = chk;
    v->serial = ++d->serial;
    v->play_mode = mode;
    v->reverse = rev;
    v->vel = ec_clampf(ev->velocity, 0.f, 1.f);

    double sf = es / 128.0 * (F - 1.0), ef = ee / 128.0 * (F - 1.0);
    double lo = ec_clampd(sf < ef ? sf : ef, 0.0, F - 2.0);
    double hi = ec_clampd(sf > ef ? sf : ef, lo + 1.0, F - 1.0);
    int cont = (mode == 3 || mode == 5);
    v->read_lo = cont ? 0.0 : lo;
    v->read_hi = cont ? F - 1.0 : hi;
    v->reset_frame = ec_clampd(rev ? ef : sf, v->read_lo, v->read_hi);
    v->range = v->read_hi - v->read_lo;
    if (v->range < 1.0) v->range = 1.0;
    double pitch_ratio = ec_clampd(pr, 1.0 / 32.0, 32.0);

    int warp = t->mode;
    v->kind = (warp == EC_WARP_GRANULAR) ? 1 : 0;
    int tempo_mode = (warp == EC_WARP_TEMPO_VARISPEED);
    double free_pitch = v->kind == 0 ? (tempo_mode ? 1.0 : pitch_ratio) : 1.0;
    v->pitched = v->kind == 1 || !tempo_mode;   /* E4: the PITCH mod bus reaches it */
    double tempo_ratio = (double)d->bpm / (t->src_bpm > 1.0 ? t->src_bpm : 1.0);
    double synced = (v->kind == 0 && !tempo_mode) ? 1.0 : tempo_ratio;
    double rr = p->slice_sync ? synced : (p->slice_rate > 0.03125f ? p->slice_rate : 0.03125);
    v->rate = t->brs * free_pitch * rr * (rev ? -1.0 : 1.0);
    v->loop_pos = v->reset_frame;
    v->sweep = 0.0;
    v->pos = v->reset_frame;

    v->spec.mode = EC_ENV_ADSR;
    v->spec.atk = p->env_attack > 0.0001f ? p->env_attack : 0.0001f;
    if (p->env_mode == EC_ENV_AHR) { v->spec.dec = 0.0001f; v->spec.sus = 1.f; }
    else {
        v->spec.dec = p->env_decay > 0.0001f ? p->env_decay : 0.0001f;
        v->spec.sus = ec_clampf(p->env_sustain, 0.f, 1.f);
    }
    v->spec.rel = ec_clampf(p->env_release, 0.0001f, 2.f);
    v->spec.hold = 0.f;
    ec_env_reset(&v->env);
    ec_env_trigger(&v->env, &v->spec, -1, 0, fs);

    double gate_s = (p->env_mode == EC_ENV_AHR && p->env_hold < dur) ? p->env_hold : dur;
    gate_s = ec_clampd(gate_s, 0.005, 60.0);
    v->slot_left = (int32_t)(gate_s * fs);
    if (v->slot_left < 1) v->slot_left = 1;

    if (v->kind == 1) {
        v->ng = 0;
        v->gspawn = 0.0;
        v->gsize = ec_clampf(p->grain_size, 0.002f, 0.5f);
        v->goverlap = ec_clampf(p->grain_density, 1.f, 64.f);
        v->grandom = ec_clampf(p->grain_jitter + p->mode_macro * 0.03f, 0.f, 0.25f);
        v->grate = t->brs * pitch_ratio;
        v->rng = 0x9E3779B9u ^ (v->serial * 2654435761u);
    }
    t->slot_voice[idx] = (int16_t)vi;
}

static inline double voice_pos(ec_voice_t *v)
{
    double pos;
    if (v->play_mode == 2 || v->play_mode == 5) pos = v->loop_pos;
    else if (v->play_mode == 4) {
        double m = fmod(v->sweep, 2.0 * v->range);
        pos = v->read_lo + (m <= v->range ? m : 2.0 * v->range - m);
    } else {
        pos = v->reverse ? v->reset_frame - v->sweep : v->reset_frame + v->sweep;
        pos = ec_clampd(pos, v->read_lo, v->read_hi);
    }
    return pos;
}

static void grain_cloud(ec_dsp_t *d, ec_voice_t *v, const ec_sample_t *s, double pointer,
                        float *l, float *r)
{
    /* Warp1-like: one grain every size/overlaps, read from the pointer at the
     * pitch rate, Hann window, size randomised by windowRandRatio. */
    v->gspawn -= 1.0;
    if (v->gspawn <= 0.0) {
        double win = v->gsize * d->fs * (1.0 + v->grandom * ec_frand2(&v->rng));
        if (win < 4.0) win = 4.0;
        double step = win / v->goverlap;
        v->gspawn += step < 1.0 ? 1.0 : step;
        if (v->ng < EC_VGRAINS) {
            ec_grain_t *g = &v->g[v->ng++];
            g->pos = pointer;
            g->rate = v->grate * v->pmod;
            g->phase = 0.f;
            g->inc = (float)(1.0 / win);
        }
    }
    float ml = 0.f, mr = 0.f, a, b;
    int k = 0;
    while (k < v->ng) {
        ec_grain_t *g = &v->g[k];
        float w = ec_hann_lerp(d->hann, g->phase);
        ec_peek_wrap(s, g->pos, &a, &b);
        ml += a * w;
        mr += b * w;
        g->pos += g->rate;
        g->phase += g->inc;
        if (g->phase >= 1.f) v->g[k] = v->g[--v->ng];
        else ++k;
    }
    *l = ml;
    *r = mr;
}

void ec_voices_process(ec_dsp_t *d, int track, float *L, float *R, int n)
{
    ec_track_t *t = &d->tr[track];
    const ec_sample_t *s = &t->smp;
    float fs = d->fs;
    for (int vi = 0; vi < EC_VOICE_POOL; ++vi) {
        ec_voice_t *v = &d->v[vi];
        if (!v->used || v->track != track) continue;
        if (!t->has_smp) { voice_free(d, vi); continue; }
        /* E4: the track's PITCH mod (+-12 st), read every chunk like In.kr(pitchModBus) */
        double pm = (v->pitched && t->mods[EC_MOD_PITCH] != 0.f)
                        ? ec_midiratio(ec_clampf(t->mods[EC_MOD_PITCH], -1.f, 1.f) * 12.f) : 1.0;
        v->pmod = pm;
        double vrate = v->kind == 0 ? v->rate * pm : v->rate;
        double arate = fabs(vrate);
        double span = v->read_hi - v->read_lo;
        if (span < 1.0) span = 1.0;
        for (int i = 0; i < n; ++i) {
            double pos = voice_pos(v);
            v->pos = pos;
            float l, r;
            if (v->kind == 0) ec_peek_wrap(s, pos, &l, &r);
            else grain_cloud(d, v, s, pos, &l, &r);
            float e = ec_env_tick(&v->env, &v->spec, fs);
            float g = v->vel * e * EC_S16_SCALE;
            if (v->stealing) {
                g *= v->steal_gain;
                v->steal_gain -= v->steal_dec;
            }
            L[i] += l * g;
            R[i] += r * g;
            v->sweep += arate;
            v->loop_pos += vrate;
            if (v->loop_pos >= v->read_hi) v->loop_pos -= span;
            else if (v->loop_pos < v->read_lo) v->loop_pos += span;
            if (v->play_mode == 0 && v->env.gate && v->sweep >= v->range)
                ec_env_gate_off(&v->env, &v->spec, fs);
            if (v->slot_left > 0 && --v->slot_left == 0) {
                if (t->slot_voice[v->slot] == vi) {
                    t->slot_voice[v->slot] = -1;
                    voice_gate_off(d, v);
                }
            }
            if ((v->stealing && v->steal_gain <= 0.f) || v->env.stage == ENV_DONE) {
                voice_free(d, vi);
                break;
            }
        }
    }
}
