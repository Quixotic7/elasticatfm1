/* SPDX-License-Identifier: GPL-3.0-only */
/* The sequencer (ec_seq.h): lib/sequencer/track_sequencer.lua's enter_step / tick / trig conditions / holds and
 * lib/grid_sequencer.lua's transport, global boundary and pattern switching, on a sample clock. */
#include <math.h>
#include <string.h>

#include "ec_e5.h"
#include "ec_mtx.h"
#include "ec_scene.h"
#include "ec_seq.h"

#define NEVER UINT64_MAX
#define BEAT_EPS 1e-9

/* ------------------------------------------------------------------------------------------- helpers --- */
static ec_ptrack_t *ptr(const ec_seq_t *s, int t) { return ec_project_track(s->proj, t); }
static int iv(const float *v, int id) { return (int)floorf(v[id] + 0.5f); }
static int machine_of(const ec_seq_t *s, int t) { return iv(ptr(s, t)->v, EC_P_MACHINE); }
static int is_slice(int m) { return m >= EC_MACH_GRID_SLICE && m <= EC_MACH_RAZOR_POLY; }
static int clampi(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }

static double spb(const ec_seq_t *s) { return 60.0 * s->fs / (s->bpm > 1.0 ? s->bpm : 1.0); }

static double bps(const ec_seq_t *s, int t)
{
    const int idx = clampi(iv(ptr(s, t)->v, EC_P_RATE), 1, EC_RATES);
    return 0.25 * (double)EC_RATE_FRAC[idx - 1][1] / (double)EC_RATE_FRAC[idx - 1][0];
}

static double swing_ratio(const ec_seq_t *s)
{
    double w = s->proj->swing;
    w = w < 50 ? 50 : w > 75 ? 75 : w;
    return w / 100.0;
}

int ec_seq_pattern_steps(const ec_seq_t *s, int t) { return clampi(iv(ptr(s, t)->v, EC_P_PATTERN_STEPS), 1, EC_MAXSTEPS); }

int64_t ec_seq_position_at(const ec_seq_t *s, int t, double beats)
{
    const double b = bps(s, t), sw = swing_ratio(s);
    double u, pair, frac;
    if (b <= 0)
        return 0;
    u = beats / b + BEAT_EPS;
    if (sw <= 0.5)
        return (int64_t)floor(u);
    pair = floor(u / 2);
    frac = u - pair * 2;
    return (int64_t)(pair * 2) + (frac >= sw * 2 ? 1 : 0);
}

double ec_seq_boundary_beats(const ec_seq_t *s, int t, int64_t n)
{
    const double b = bps(s, t), sw = swing_ratio(s);
    int64_t pair, odd;
    if (sw <= 0.5)
        return (double)n * b;
    pair = n >= 0 ? n / 2 : (n - 1) / 2;
    odd = n - pair * 2;
    return (double)pair * 2 * b + (odd == 1 ? 2 * b * sw : 0);
}

double ec_seq_beats(const ec_seq_t *s)
{
    if (s->playing)
        return s->beat_base + (double)(s->now - s->sample_base) / spb(s);
    return s->paused ? s->paused_beats : 0.0;
}

/* the first sample at or past `beats` */
static uint64_t sample_at(const ec_seq_t *s, double beats)
{
    const double d = (beats - s->beat_base) * spb(s);
    const double c = ceil(d - 1e-6);
    const uint64_t t = c <= 0 ? s->sample_base : s->sample_base + (uint64_t)c;
    return t < s->now ? s->now : t;
}

double ec_seq_step_samples(const ec_seq_t *s, int t) { return bps(s, t) * spb(s); }

static uint32_t rnd(ec_seq_t *s)
{
    uint32_t x = s->rng ? s->rng : 0x9E3779B9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s->rng = x;
    return x;
}

/* ------------------------------------------------------------------------------------------- events --- */
static ec_event_t ev0(int type, int track)
{
    ec_event_t e;
    memset(&e, 0, sizeof e);
    e.type = type;
    e.track = track;
    e.start = e.end = -1.0f;
    e.velocity = 1.0f;
    return e;
}
static void emit(ec_seq_t *s, const ec_event_t *e)
{
    if (s->sink.event)
        s->sink.event(s->sink.ctx, e);
}

/* ------------------------------------------------------------------------------------------- holds --- */
static void hold_min_update(ec_seq_track_t *st)
{
    int i;
    st->hold_min = NEVER;
    for (i = 0; i < EC_NP; i++)
        if (st->hold_on[i] && st->hold_exp[i] < st->hold_min)
            st->hold_min = st->hold_exp[i];
}
static void holds_clear(ec_seq_track_t *st)
{
    memset(st->hold_on, 0, sizeof st->hold_on);
    st->hold_min = NEVER;
    st->dirty = 1;
}
static int region_hold(const ec_seq_track_t *st) { return st->hold_on[EC_P_LOOP_START] || st->hold_on[EC_P_LOOP_END]; }

/* the lock's value, else the track's */
static float field(const ec_step_t *rec, const float *v, int id)
{
    float x;
    if (rec && ec_step_lock_get(rec, id, &x))
        return x;
    return v[id];
}

/* note length in samples (TrackSequencer:note_seconds) */
static uint64_t note_samples(const ec_seq_t *s, int t, const ec_step_t *rec)
{
    const float *v = ptr(s, t)->v;
    double sec;
    if (is_slice(iv(v, EC_P_MACHINE)) && iv(v, EC_P_SLICE_HOLD_TO_STEP) == 0)
        sec = v[EC_P_SLICE_HOLD];
    else {
        const double len = field(rec, v, EC_P_LENGTH);
        sec = len * bps(s, t) * 60.0 / (s->bpm > 1 ? s->bpm : 1);
    }
    if (sec < 0.01)
        sec = 0.01;
    return (uint64_t)llround(sec * s->fs);
}

/* ------------------------------------------------------------------------------------------- trig conditions --- */
static int condition_result(const ec_seq_t *s, int t, const ec_step_t *rec, int *kind)
{
    const ec_seq_track_t *st = &s->tr[t];
    const int idx = clampi((int)floorf(field(rec, ptr(s, t)->v, EC_P_TRIG_CONDITION) + 0.5f), 1, EC_TRIGCONDS);
    const ec_trigcond_t *c = &EC_TRIGCOND[idx - 1];
    *kind = c->kind;
    switch (c->kind) {
    case EC_TC_AB: return ((st->pass - 1) % c->b) + 1 == c->a;
    case EC_TC_FILL: return ec_seq_fill_active(s);
    case EC_TC_NFILL: return !ec_seq_fill_active(s);
    case EC_TC_PRE: return st->last_cond == 1;
    case EC_TC_NPRE: return st->last_cond != 1;
    case EC_TC_FIRST: return st->pass == 1;
    default: return 1;          /* none; NEI / !NEI: always true until the neighbor machine (as the original) */
    }
}

static int evaluate_trig(ec_seq_t *s, int t, const ec_step_t *rec)
{
    int kind;
    int fired = condition_result(s, t, rec, &kind);
    if (fired) {
        const float chance = field(rec, ptr(s, t)->v, EC_P_TRIG_CHANCE);
        if (chance < 100.0f)
            fired = (float)(rnd(s) % 10000u) < chance * 100.0f;
    }
    if (kind != EC_TC_NONE)
        s->tr[t].last_cond = (int8_t)(fired ? 1 : 0);
    return fired;
}

/* ------------------------------------------------------------------------------------------- triggering --- */
static void fire_slices(ec_seq_t *s, int t, const ec_step_t *rec, uint64_t note)
{
    const float *v = ptr(s, t)->v;
    const int cnt = clampi(iv(v, EC_P_SLICE_COUNT), 1, 32);
    const int mode = iv(v, EC_P_SLICE_PLAY_MODE);
    const ec_slicemap_t *sm = ec_e5_slices(s->proj, t);    /* E5: the slice programs */
    uint32_t slices = rec->slices;
    int k;
    if (!slices && rec->trig)
        slices = 1u << (clampi(iv(v, EC_P_SLICE_INDEX), 1, cnt) - 1);
    slices &= ~sm->mute;                                    /* a muted slice is skipped entirely */
    for (k = 1; k <= cnt; k++)
        if (slices >> (k - 1) & 1u) {
            ec_event_t e = ev0(EC_EV_TRIG, t);
            float x;
            e.slice = k;
            /* per voice: the step's lock > the slice's lock > the track */
            e.velocity = ec_step_lock_get(rec, EC_P_VELOCITY, &x) || ec_slice_lock_get(sm, k, EC_P_VELOCITY, &x)
                             ? x : v[EC_P_VELOCITY];
            if (!ec_step_lock_get(rec, EC_P_PITCH, &x) && !ec_slice_lock_get(sm, k, EC_P_PITCH, &x) &&
                !ec_scene_value(s->proj, t, EC_P_PITCH, &x))
                x = v[EC_P_PITCH];
            e.pitch = x;
            e.choke = sm->choke[k - 1];
            e.length = mode == EC_SLICE_ONESHOT ? 0 : (int)(note > 0x7FFFFFFF ? 0x7FFFFFFF : note);
            if (ec_razor_slice_event(s->proj, t, &e, (int64_t)note))   /* E3: razor range, per-slice settings */
                emit(s, &e);
        }
    s->tr[t].fired_slices = slices;
}

static void trig_loop_region(ec_seq_t *s, int t, const ec_step_t *rec, uint64_t note, int flags)
{
    const float *v = ptr(s, t)->v;
    ec_event_t e = ev0(EC_EV_TRIG, t);
    float x;
    e.length = (int)(note > 0x7FFFFFFF ? 0x7FFFFFFF : note);
    e.flags = flags;
    if (rec && ec_step_locks_region(rec)) {
        /* a start-only lock: the end follows the track (the engine's seq layer gets the track end) */
        e.start = ec_step_lock_get(rec, EC_P_LOOP_START, &x) ? x : v[EC_P_LOOP_START];
        e.end = ec_step_lock_get(rec, EC_P_LOOP_END, &x) ? x : v[EC_P_LOOP_END];
        e.mode = clampi((int)floorf(field(rec, v, EC_P_TRIG_RELEASE) + 0.5f), 1, 3);
        if (field(rec, v, EC_P_TRIG_JUMP) > 0.5f)
            e.flags |= EC_TF_JUMP;
    }
    emit(s, &e);
}

/* E4: retrigger the mod sources where the step's lfo_reset / env_reset resolve ON (\trModTrig) */
static void mod_trig(ec_seq_t *s, int t, const ec_step_t *rec, uint64_t note, int lfo, int env)
{
    const float *v = ptr(s, t)->v;
    ec_event_t e = ev0(EC_EV_MOD_TRIG, t);
    if (rec) {
        lfo = ec_step_reset_active(rec, v, EC_P_LFO_RESET);
        env = ec_step_reset_active(rec, v, EC_P_ENV_RESET);
    }
    if (!lfo && !env)
        return;
    e.flags = (lfo ? EC_TF_LFO : 0) | (env ? EC_TF_MENV : 0);
    e.length = (int)(note > 0x7FFFFFFF ? 0x7FFFFFFF : note);
    emit(s, &e);
}

/* THE step routine (TrackSequencer:enter_step) */
static void enter_step(ec_seq_t *s, int t, int reset)
{
    ec_seq_track_t *st = &s->tr[t];
    ec_ptrack_t *tr = ptr(s, t);
    const float *v = tr->v;
    const int m = iv(v, EC_P_MACHINE);
    const ec_step_t *rec = st->play_index >= 0 && st->play_index < EC_MAXSTEPS ? &tr->steps[st->play_index] : 0;
    int active, fires, ghost, had_region = 0, k;
    uint64_t note;

    if (reset) {
        holds_clear(st);
        st->pass = 1;
        st->last_cond = -1;
    }
    active = rec && (is_slice(m) ? (rec->slices != 0 || rec->trig) : rec->trig);
    fires = active && evaluate_trig(s, t, rec);
    ghost = fires && ec_step_is_ghost(rec, v);
    if (fires)
        st->rat_left = 0;                     /* (a new trig supersedes the last one's ratchets) */
    /* monophonic: a fresh non-ghost trig ends the previous note's holds */
    if (!reset && fires && iv(v, EC_P_TRIG_POLY) == 1 && !ghost) {
        had_region = region_hold(st);
        holds_clear(st);
    }
    note = note_samples(s, t, fires ? rec : 0);
    if (fires && is_slice(m) && rec->slices) {    /* E5: the primary slice's program, under the step's own locks */
        const ec_slicemap_t *sm = ec_e5_slices(s->proj, t);
        int sl = 0;
        while (sl < 32 && !(rec->slices >> sl & 1u))
            sl++;
        for (k = 0; sl < 32 && k < sm->nlock[sl]; k++) {
            const int id = sm->lock_id[sl][k];
            if (id >= EC_NP || id == EC_P_PITCH || id == EC_P_VELOCITY ||
                (EC_PARAMS[id].flags & (EC_PF_SEQ | EC_PF_MOMENT | EC_PF_REGION)))
                continue;
            st->hold_on[id] = 1;
            st->hold_val[id] = sm->lock_val[sl][k];
            st->hold_exp[id] = s->now + note;
        }
    }
    if (fires) {
        for (k = 0; k < rec->nlock; k++) {
            const int id = rec->lock_id[k];
            if (id >= EC_NP || (EC_PARAMS[id].flags & (EC_PF_SEQ | EC_PF_MOMENT)))
                continue;
            st->hold_on[id] = 1;
            st->hold_val[id] = rec->lock_val[k];
            st->hold_exp[id] = s->now + note;
        }
        hold_min_update(st);
        st->dirty = 1;
    }

    if (m == EC_MACH_LOOP) {
        if (fires && ec_step_locks_region(rec))
            trig_loop_region(s, t, rec, note, (ghost ? EC_TF_GHOST : 0) | (reset ? EC_TF_JUMP : 0));
        else {
            if (had_region) {
                ec_event_t e = ev0(EC_EV_REGION_RELEASE, t);
                emit(s, &e);
            }
            if (fires)
                trig_loop_region(s, t, 0, note, ghost ? EC_TF_GHOST : 0);
        }
    } else if (m == EC_MACH_LOOP_TRIG) {
        if (fires) {
            ec_event_t e = ev0(EC_EV_TRIG, t);
            float x;
            e.length = (int)(note > 0x7FFFFFFF ? 0x7FFFFFFF : note);
            e.flags = ghost ? EC_TF_GHOST : 0;
            if (ec_step_lock_get(rec, EC_P_LOOP_START, &x))
                e.start = x;
            if (ec_step_lock_get(rec, EC_P_LOOP_END, &x))
                e.end = x;
            emit(s, &e);
        }
    } else if (is_slice(m)) {
        if (fires)
            fire_slices(s, t, rec, note);
        else
            st->fired_slices = 0;
    }

    if (fires)
        mod_trig(s, t, rec, note, 0, 0);    /* E4 */
    if (fires) {
        int hits = clampi((int)floorf(field(rec, v, EC_P_TRIG_RATCHET) + 0.5f), 1, 8);
        if (is_slice(m)) {
            float sr = field(rec, v, EC_P_SLICE_RATCHET), x;
            int sl = 0;                                    /* E5: step lock > slice lock > track */
            while (sl < 32 && !(rec->slices >> sl & 1u))
                sl++;
            if (!ec_step_lock_get(rec, EC_P_SLICE_RATCHET, 0) && sl < 32 &&
                ec_slice_lock_get(ec_e5_slices(s->proj, t), sl + 1, EC_P_SLICE_RATCHET, &x))
                sr = x;
            hits = clampi(hits * clampi((int)floorf(sr + 0.5f), 1, 4), 1, 16);
        }
        st->fired++;
        st->last_fire = s->now;
        if (hits > 1) {
            st->rat_left = hits - 1;
            st->rat_k = 1;
            st->rat_t0 = s->now;
            st->rat_iv = ec_seq_step_samples(s, t) / hits;
            st->rat_index = st->play_index;
        }
    }
}

static void retrigger_ratchet(ec_seq_t *s, int t)
{
    ec_seq_track_t *st = &s->tr[t];
    ec_ptrack_t *tr = ptr(s, t);
    const ec_step_t *rec = &tr->steps[st->rat_index];
    const int m = iv(tr->v, EC_P_MACHINE);
    const uint64_t note = note_samples(s, t, rec);
    mod_trig(s, t, rec, note, 0, 0);          /* E4 (retrigger_ratchet's mod_trig) */
    if (m == EC_MACH_LOOP) {
        ec_event_t e;
        if (!ec_step_is_ghost(rec, tr->v)) {
            e = ev0(EC_EV_NOTE_ON, t);
            e.length = (int)(note > 0x7FFFFFFF ? 0x7FFFFFFF : note);
            emit(s, &e);
        }
        e = ev0(EC_EV_REGION, t);
        e.mode = EC_RM_PHASE;
        e.phase = 0.0f;
        emit(s, &e);
    } else if (m == EC_MACH_LOOP_TRIG) {
        ec_event_t e = ev0(EC_EV_TRIG, t);
        float x;
        e.length = (int)(note > 0x7FFFFFFF ? 0x7FFFFFFF : note);
        if (ec_step_lock_get(rec, EC_P_LOOP_START, &x))
            e.start = x;
        if (ec_step_lock_get(rec, EC_P_LOOP_END, &x))
            e.end = x;
        emit(s, &e);
    } else if (is_slice(m))
        fire_slices(s, t, rec, note);
    st->last_fire = s->now;
}

/* a lock window ended (TrackSequencer:tick) */
static void tick_holds(ec_seq_t *s, int t)
{
    ec_seq_track_t *st = &s->tr[t];
    const int had = region_hold(st);
    int i;
    for (i = 0; i < EC_NP; i++)
        if (st->hold_on[i] && st->hold_exp[i] <= s->now)
            st->hold_on[i] = 0;
    hold_min_update(st);
    st->dirty = 1;
    if (had && !region_hold(st) && machine_of(s, t) == EC_MACH_LOOP) {
        ec_event_t e = ev0(EC_EV_REGION_RELEASE, t);
        emit(s, &e);
    }
}

/* ------------------------------------------------------------------------------------------- positions --- */
static void anchor(ec_seq_t *s, int t, int64_t pos)
{
    ec_seq_track_t *st = &s->tr[t];
    const int steps = ec_seq_pattern_steps(s, t);
    st->position = pos;
    st->play_index = ec_e5_play_index(s->proj, t, steps, pos, &st->pass);   /* E5: PAGE > LOOP */
}

static void enter_position(ec_seq_t *s, int t, int64_t pos)
{
    anchor(s, t, pos);
    enter_step(s, t, 0);
}

static void reset_track(ec_seq_t *s, int t)
{
    ec_seq_track_t *st = &s->tr[t];
    holds_clear(st);
    st->position = -1;
    st->play_index = 0;
    st->pass = 1;
    st->last_cond = -1;
    st->rat_left = 0;
    st->fired_slices = 0;
}

static uint64_t track_next(const ec_seq_t *s, int t)
{
    const ec_seq_track_t *st = &s->tr[t];
    if (st->position < 0)
        return NEVER;
    return sample_at(s, ec_seq_boundary_beats(s, t, st->position + 1));
}

static uint64_t rat_next(const ec_seq_track_t *st)
{
    if (st->rat_left <= 0)
        return NEVER;
    return st->rat_t0 + (uint64_t)llround(st->rat_k * st->rat_iv);
}

static uint64_t global_next(const ec_seq_t *s)
{
    const int glen = clampi(s->proj->glen, 1, 256);
    return sample_at(s, (double)(s->global_cycle + 1) * glen * 0.25);
}

static void global_rederive(ec_seq_t *s)
{
    const int glen = clampi(s->proj->glen, 1, 256);
    const int64_t gp = (int64_t)floor(ec_seq_beats(s) / 0.25 + BEAT_EPS);
    s->global_cycle = gp / glen;
    s->global_pos = (int)(gp % glen);
}

/* ------------------------------------------------------------------------------------------- patterns --- */
static void restart_clock(ec_seq_t *s)
{
    int t;
    ec_event_t e;
    s->beat_base = 0;
    s->sample_base = s->now;
    s->global_cycle = 0;
    s->global_pos = 0;
    e = ev0(EC_EV_TRANSPORT, -1);
    e.value = 1;
    e.flags = EC_TF_RESET;
    emit(s, &e);
    for (t = 0; t < EC_NTRACK; t++) {
        reset_track(s, t);
        anchor(s, t, 0);
        enter_step(s, t, 1);
    }
}

static void load_pattern(ec_seq_t *s, int idx, int restart)
{
    int t;
    ec_pattern_prepare(s->proj, idx);
    s->proj->current = idx;
    s->switches++;
    ec_seq_touch_all(s);
    ec_e5_pattern_loaded(s);                       /* E5: the pattern's BPM, the scenes re-applied */
    if (s->playing && restart)
        restart_clock(s);
    else if (s->playing)
        for (t = 0; t < EC_NTRACK; t++) {    /* direct jump: keep the clock, fold the index into the new length */
            if (s->tr[t].position >= 0)
                anchor(s, t, s->tr[t].position);
        }
}

static int on_global_boundary(ec_seq_t *s)
{
    if (s->temp_return >= 0) {
        const int r = s->temp_return;
        s->temp_return = -1;
        load_pattern(s, r, 1);
        return 1;
    }
    if (s->pending >= 0) {
        const int r = s->pending;
        s->pending = -1;
        load_pattern(s, r, 1);
        return 1;
    }
    return 0;
}

void ec_seq_request_pattern(ec_seq_t *s, int idx)
{
    if (idx < 0 || idx >= EC_NPAT)
        return;
    if (!s->playing) {
        s->pending = s->temp_return = -1;
        load_pattern(s, idx, 1);
        return;
    }
    switch (s->proj->quant) {
    case EC_Q_DIRECT_JUMP:
        s->pending = s->temp_return = -1;
        load_pattern(s, idx, 0);
        break;
    case EC_Q_DIRECT_START:
        s->pending = s->temp_return = -1;
        load_pattern(s, idx, 1);
        break;
    case EC_Q_TEMP_JUMP: {
        const int from = s->proj->current;
        s->pending = -1;
        load_pattern(s, idx, 1);
        s->temp_return = from;
    } break;
    default: s->pending = idx == s->proj->current ? -1 : idx; break;   /* (asking for the live one cancels) */
    }
}

/* E5: the project was replaced under the sequencer (recall): reload the live pattern, from step 1 when playing */
void ec_seq_reload(ec_seq_t *s)
{
    s->pending = s->temp_return = -1;
    load_pattern(s, s->proj->current, 1);
}

void ec_seq_set_fill(ec_seq_t *s, int held, int latched)
{
    s->fill_held = held;
    s->fill_latched = latched;
}
int ec_seq_fill_active(const ec_seq_t *s) { return s->fill_held || s->fill_latched; }

/* ------------------------------------------------------------------------------------------- the clock --- */
static uint64_t next_due(const ec_seq_t *s)
{
    uint64_t n = global_next(s), x;
    int t;
    for (t = 0; t < EC_NTRACK; t++) {
        const ec_seq_track_t *st = &s->tr[t];
        if (st->hold_min < n)
            n = st->hold_min;
        x = track_next(s, t);
        if (x < n)
            n = x;
        x = rat_next(st);
        if (x < n)
            n = x;
    }
    return n;
}

static void handle_due(ec_seq_t *s)
{
    int t;
    /* the note-length windows end first (the Lua's tick before advance) */
    for (t = 0; t < EC_NTRACK; t++)
        if (s->tr[t].hold_min <= s->now)
            tick_holds(s, t);
    /* the global pattern boundary: a queued switch re-anchors every track (this moment is complete) */
    if (global_next(s) <= s->now) {
        s->global_cycle++;
        if (on_global_boundary(s))
            return;
    }
    for (t = 0; t < EC_NTRACK; t++) {
        ec_seq_track_t *st = &s->tr[t];
        if (st->position >= 0 && track_next(s, t) <= s->now)
            enter_position(s, t, st->position + 1);
    }
    for (t = 0; t < EC_NTRACK; t++) {
        ec_seq_track_t *st = &s->tr[t];
        if (rat_next(st) <= s->now) {
            retrigger_ratchet(s, t);
            st->rat_k++;
            st->rat_left--;
        }
    }
}

static void flush(ec_seq_t *s)
{
    int t, i;
    for (t = 0; t < EC_NTRACK; t++) {
        ec_seq_track_t *st = &s->tr[t];
        float v[EC_NP];
        ec_track_params_t p;
        ec_slotmeta_t meta;
        const ec_slot_t *sl;
        int region;
        if (!st->dirty && st->sent_ok)
            continue;
        st->dirty = 0;
        for (i = 0; i < EC_NP; i++)
            v[i] = ec_seq_value(s, t, i);
        st->mtx_gen = ec_mtx_generic(v);         /* 0.5.0: the matrix's generic destinations, per chunk */
        if (st->mtx_gen) {
            float live[EC_MSRC_N];
            memset(live, 0, sizeof live);
            if (s->sink.sources)
                s->sink.sources(s->sink.ctx, t, live);
            ec_mtx_apply(v, live);
        }
        sl = &s->proj->slot[clampi(iv(v, EC_P_SLOT), 1, EC_NSLOT) - 1];
        meta.trim_start = sl->trim_start;
        meta.trim_end = sl->trim_end;
        meta.steps = sl->steps;
        meta.gain = sl->gain;
        ec_params_engine(v, &meta, s->proj->rtn, &p);
        if (st->sent_ok && !memcmp(&p, &st->sent, sizeof p))
            continue;
        region = !st->sent_ok || p.loop_start != st->sent.loop_start || p.loop_end != st->sent.loop_end ||
                 p.range_start != st->sent.range_start || p.range_end != st->sent.range_end ||
                 p.trim_start != st->sent.trim_start || p.trim_end != st->sent.trim_end ||
                 p.machine != st->sent.machine;
        st->sent = p;
        st->sent_ok = 1;
        if (s->sink.params)
            s->sink.params(s->sink.ctx, t, &p);
        if (region) {                            /* refresh_track_region: the fences follow (KEEP) */
            ec_event_t e = ev0(EC_EV_REGION, t);
            e.mode = EC_RM_KEEP;
            emit(s, &e);
        }
    }
}

void ec_seq_render(ec_seq_t *s, float *out, int frames)
{
    int done = 0;
    while (done < frames) {
        uint64_t next = NEVER;
        int n = frames - done;
        if (s->playing) {
            int guard;
            for (guard = 0; guard < 64; guard++) {
                next = next_due(s);
                if (next > s->now)
                    break;
                handle_due(s);
            }
            if (next <= s->now)
                next = s->now + 1;
            global_rederive(s);
        }
        for (int t = 0; t < EC_NTRACK; t++)       /* 0.5.0: the modulated params follow their sources */
            if (s->tr[t].mtx_gen)
                s->tr[t].dirty = 1;
        flush(s);
        if (next != NEVER && next - s->now < (uint64_t)n)
            n = (int)(next - s->now);
        if (s->sink.process)
            s->sink.process(s->sink.ctx, out ? out + 2 * done : 0, n);
        s->now += (uint64_t)n;
        done += n;
    }
}

/* ------------------------------------------------------------------------------------------- transport --- */
void ec_seq_init(ec_seq_t *s, ec_project_t *proj, const ec_seq_sink_t *sink, double fs)
{
    int t;
    memset(s, 0, sizeof *s);
    s->proj = proj;
    if (sink)
        s->sink = *sink;
    s->fs = fs;
    s->bpm = proj->bpm;
    s->pending = s->temp_return = -1;
    s->rng = 0x2545F491u;
    for (t = 0; t < EC_NTRACK; t++) {
        reset_track(s, t);
        s->tr[t].dirty = 1;
    }
}

void ec_seq_start(ec_seq_t *s)
{
    int t;
    ec_event_t e;
    if (s->playing)
        return;
    s->stops = 0;
    if (s->paused) {                              /* resume: the origin shifts by the paused span */
        s->beat_base = s->paused_beats;
        s->sample_base = s->now;
        s->paused = 0;
        s->playing = 1;
        e = ev0(EC_EV_TRANSPORT, -1);
        e.value = 1;
        emit(s, &e);
        global_rederive(s);
        return;
    }
    s->bpm = s->proj->bpm;
    e = ev0(EC_EV_TEMPO, -1);
    e.value = (float)s->bpm;
    emit(s, &e);
    s->playing = 1;
    restart_clock(s);
    /* a loop track with no trigs at all drones from the start (start_sequence's note_on) */
    for (t = 0; t < EC_NTRACK; t++) {
        const int m = machine_of(s, t);
        if (m == EC_MACH_LOOP && !ec_track_has_trigs(ptr(s, t))) {
            e = ev0(EC_EV_NOTE_ON, t);
            e.length = 0;
            emit(s, &e);
            mod_trig(s, t, 0, 0, 1, 1);           /* E4: the droning start counts as one note */
        }
    }
}

void ec_seq_pause(ec_seq_t *s)
{
    ec_event_t e;
    if (!s->playing)
        return;
    s->paused_beats = ec_seq_beats(s);
    s->playing = 0;
    s->paused = 1;
    e = ev0(EC_EV_TRANSPORT, -1);
    e.value = 0;
    emit(s, &e);
}

void ec_seq_stop(ec_seq_t *s)
{
    int t;
    ec_event_t e;
    s->playing = 0;
    s->paused = 0;
    s->pending = s->temp_return = -1;
    s->global_cycle = 0;
    s->global_pos = 0;
    e = ev0(EC_EV_TRANSPORT, -1);
    e.value = 0;
    emit(s, &e);
    for (t = 0; t < EC_NTRACK; t++) {
        reset_track(s, t);
        e = ev0(EC_EV_REGION_RELEASE, t);       /* back to the base region at its start */
        emit(s, &e);
        e = ev0(EC_EV_REGION, t);
        e.mode = EC_RM_PHASE;
        e.phase = 0.0f;
        emit(s, &e);
    }
    if (++s->stops >= 2) {                      /* stop twice: every voice out */
        e = ev0(EC_EV_PANIC, -1);
        emit(s, &e);
    }
}

void ec_seq_toggle(ec_seq_t *s)
{
    if (s->playing)
        ec_seq_pause(s);
    else
        ec_seq_start(s);
}

void ec_seq_set_bpm(ec_seq_t *s, float bpm)
{
    ec_event_t e;
    if (bpm < 20)
        bpm = 20;
    if (bpm > 300)
        bpm = 300;
    if (s->playing) {
        s->beat_base = ec_seq_beats(s);
        s->sample_base = s->now;
    }
    s->bpm = bpm;
    s->proj->bpm = bpm;
    ec_e5_bpm_changed(s->proj, bpm);               /* E5: per-pattern BPM */
    e = ev0(EC_EV_TEMPO, -1);
    e.value = bpm;
    emit(s, &e);
}

void ec_seq_touch(ec_seq_t *s, int t)
{
    if (t >= 0 && t < EC_NTRACK)
        s->tr[t].dirty = 1;
}
void ec_seq_touch_all(ec_seq_t *s)
{
    int t;
    for (t = 0; t < EC_NTRACK; t++)
        s->tr[t].dirty = 1;
}

void ec_seq_timing_changed(ec_seq_t *s)
{
    int t;
    if (!s->playing && !s->paused)
        return;
    for (t = 0; t < EC_NTRACK; t++)
        if (s->tr[t].position >= 0)
            anchor(s, t, ec_seq_position_at(s, t, ec_seq_beats(s)));
    global_rederive(s);
}

/* ------------------------------------------------------------------------------------------- live --- */
void ec_seq_loop_key(ec_seq_t *s, int t, int key, int pressed)
{
    ec_event_t e;
    if (t < 0 || t >= EC_NTRACK || key < 1 || key > 32)
        return;
    e = ev0(EC_EV_LOOP_KEY, t);
    e.slice = key;
    e.value = pressed ? 1.0f : 0.0f;
    e.flags = pressed && !s->playing ? EC_TF_AUDITION : 0;
    if (pressed)
        s->tr[t].live_keys |= 1u << (key - 1);
    else
        s->tr[t].live_keys &= ~(1u << (key - 1));
    emit(s, &e);
}

static void pad(ec_seq_t *s, int t, int slice, int pressed, float vel, float pitch)
{
    const int mode = ec_razor_play_mode(s->proj, t, slice);   /* E3: per-slice play mode */
    const ec_slicemap_t *sm = ec_e5_slices(s->proj, t);     /* E5: the slice's program */
    ec_event_t e;
    if (pressed) {
        if (sm->mute >> (slice - 1) & 1u)
            return;
        e = ev0(EC_EV_SLICE_ON, t);
        e.slice = slice;
        e.velocity = vel;
        e.pitch = pitch;
        e.choke = sm->choke[slice - 1];
        e.length = mode == EC_SLICE_ONESHOT ? 0 : (int)(60.0 * s->fs);   /* held until the key comes up */
        s->tr[t].pads |= 1u << (slice - 1);
        s->tr[t].last_fire = s->now;
        if (ec_razor_slice_event(s->proj, t, &e, (int64_t)(60.0 * s->fs)))   /* E3: razor range, per-slice */
            emit(s, &e);
    } else {
        s->tr[t].pads &= ~(1u << (slice - 1));
        if (mode != EC_SLICE_ONESHOT) {
            e = ev0(EC_EV_SLICE_OFF, t);
            e.slice = slice;
            emit(s, &e);
        }
    }
}

void ec_seq_slice_pad(ec_seq_t *s, int t, int slice, int pressed, float vel)
{
    if (t < 0 || t >= EC_NTRACK || slice < 1 || slice > 32)
        return;
    float x;
    if (pressed && ec_slice_lock_get(ec_e5_slices(s->proj, t), slice, EC_P_PITCH, &x))   /* E5 */
        pad(s, t, slice, pressed, vel, x);
    else
        pad(s, t, slice, pressed, vel, ec_seq_value(s, t, EC_P_PITCH));
}

void ec_seq_keyboard(ec_seq_t *s, int t, float semis, int pressed, float vel)
{
    const float *v;
    ec_seq_track_t *st;
    if (t < 0 || t >= EC_NTRACK)
        return;
    v = ptr(s, t)->v;
    st = &s->tr[t];
    if (is_slice(iv(v, EC_P_MACHINE))) {
        const int sl = clampi(iv(v, EC_P_SLICE_INDEX), 1, clampi(iv(v, EC_P_SLICE_COUNT), 1, 32));
        pad(s, t, sl, pressed, vel, v[EC_P_PITCH] + semis);
        return;
    }
    if (pressed) {
        ec_event_t e = ev0(EC_EV_NOTE_ON, t);
        st->live_pitch = v[EC_P_PITCH] + semis;
        st->live_pitch_on = 1;
        e.length = 0;
        emit(s, &e);
    } else
        st->live_pitch_on = 0;
    st->dirty = 1;
}

/* ------------------------------------------------------------------------------------------- reads --- */
float ec_seq_value(const ec_seq_t *s, int t, int id)
{
    const ec_seq_track_t *st = &s->tr[t];
    if (id == EC_P_PITCH && st->live_pitch_on)
        return st->live_pitch;
    if (st->hold_on[id] && id != EC_P_LOOP_START && id != EC_P_LOOP_END)
        return st->hold_val[id];
    {
        float x;                                   /* E5: the crossfader, source 2 (BASE_VALUE_RESOLVER.md) */
        if (ec_scene_value(s->proj, t, id, &x))
            return x;
    }
    return ptr(s, t)->v[id];
}

int ec_seq_step(const ec_seq_t *s, int t)
{
    if (!s->playing && !s->paused)
        return -1;
    return s->tr[t].play_index;
}
