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

double ec_seq_step_beats(const ec_seq_t *s, int t) { return t >= 0 && t < EC_NTRACK ? bps(s, t) : 0.25; }   /* 0.6.59 */

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
static void slides_clear(ec_seq_track_t *st)
{
    memset(st->slide_on, 0, sizeof st->slide_on);
    st->sliding = 0;
}
static void slide_end(ec_seq_track_t *st, int id)
{
    if (st->slide_on[id]) {
        st->slide_on[id] = 0;
        st->sliding--;
    }
}
static void holds_clear(ec_seq_track_t *st)
{
    memset(st->hold_on, 0, sizeof st->hold_on);
    slides_clear(st);
    st->hold_min = NEVER;
    st->dirty = 1;
}
static int is_region(int id) { return id == EC_P_LOOP_START || id == EC_P_LOOP_END; }
static int region_hold(const ec_seq_track_t *st) { return st->hold_on[EC_P_LOOP_START] || st->hold_on[EC_P_LOOP_END]; }
/* 0.6.4: a trig resets the sound back to the track values (Elektron: an unlocked trig wipes the slate): every sticky
 * hold goes; the region holds keep their own note window (monophonic: the caller ends those) */
static void holds_clear_sound(ec_seq_track_t *st)
{
    int i;
    for (i = 0; i < EC_NP; i++)
        if (!is_region(i))
            st->hold_on[i] = 0;
    slides_clear(st);
    hold_min_update(st);
    st->dirty = 1;
}
/* a lock takes hold: sticky (until a trig or lock changes it), a region lock for its note window */
static void hold_set(ec_seq_t *s, ec_seq_track_t *st, int id, float val, uint64_t note)
{
    st->hold_on[id] = 1;
    st->hold_val[id] = val;
    st->hold_exp[id] = is_region(id) ? s->now + note : NEVER;
    slide_end(st, id);
}

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
    case EC_TC_AB: return ((st->fire_pass - 1) % c->b) + 1 == c->a;
    case EC_TC_FILL: return ec_seq_fill_active(s);
    case EC_TC_NFILL: return !ec_seq_fill_active(s);
    case EC_TC_PRE: return st->last_cond == 1;
    case EC_TC_NPRE: return st->last_cond != 1;
    case EC_TC_FIRST: return st->fire_pass == 1;
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

/* 0.6.0: a lock-only step (no trig, no slices: Elektron's trigless lock; live recording writes them from knob
 * moves) applies its locks as the playhead passes, without a voice. 0.6.4: they stick, as Elektron's, until a trig
 * or another lock changes them; the params it does not lock are left as they are. Region, sequencer and
 * trigger-time locks need a trig: skipped. */
static void ghost_locks(ec_seq_t *s, int t, const ec_step_t *rec, int64_t pos)
{
    ec_seq_track_t *st = &s->tr[t];
    int k;
    (void)pos;
    for (k = 0; k < rec->nlock; k++) {
        const int id = rec->lock_id[k];
        if (id >= EC_NP || id == EC_P_SLOT || (EC_PARAMS[id].flags & (EC_PF_SEQ | EC_PF_MOMENT | EC_PF_REGION)))
            continue;
        hold_set(s, st, id, rec->lock_val[k], 0);
    }
    hold_min_update(st);
    st->dirty = 1;
}

/* 0.6.4: Elektron's slide. The step at clock position pos has SLIDE on: each sound param it locks ramps from its lock
 * to the lock of the next step that sets that param (a step locking it; a trig without that lock resets it to the
 * track value, so there is nothing to slide to), arriving on that step's grid boundary. The look-ahead follows the
 * play order (the page loop, the pattern length) up to one pass. */
static void start_slides(ec_seq_t *s, int t, const ec_step_t *rec, int64_t pos)
{
    ec_seq_track_t *st = &s->tr[t];
    ec_ptrack_t *tr = ptr(s, t);
    const int steps = ec_seq_pattern_steps(s, t), slice = is_slice(machine_of(s, t));
    int k, j;
    for (k = 0; k < rec->nlock; k++) {
        const int id = rec->lock_id[k];
        if (id >= EC_NP || id == EC_P_SLOT || is_region(id) ||
            (EC_PARAMS[id].flags & (EC_PF_SEQ | EC_PF_MOMENT | EC_PF_REGION)) || !st->hold_on[id])
            continue;
        for (j = 1; j <= steps; j++) {
            int pass;
            const int idx = ec_e5_play_index(s->proj, t, steps, pos + j, &pass);
            const ec_step_t *nx = idx >= 0 && idx < EC_MAXSTEPS ? &tr->steps[idx] : 0;
            float to;
            if (!nx)
                break;
            if (ec_step_lock_get(nx, id, &to)) {
                const uint64_t t1 = sample_at(s, ec_seq_boundary_beats(s, t, pos + j));
                if (t1 > s->now && to != st->hold_val[id]) {
                    if (!st->slide_on[id])
                        st->sliding++;
                    st->slide_on[id] = 1;
                    st->slide_to[id] = to;
                    st->slide_t0[id] = s->now;
                    st->slide_t1[id] = t1;
                }
                break;
            }
            if (nx->trig || (slice && nx->slices))   /* a trig without this lock: the track value, no slide */
                break;
        }
    }
    st->dirty = 1;
}

/* THE step routine (TrackSequencer:enter_step): the step at index idx (pattern pass `pass`) of clock position pos
 * fires now (0.6.0: at its grid boundary shifted by its micro timing) */
static void enter_step(ec_seq_t *s, int t, int reset, int idx, int pass, int64_t pos)
{
    ec_seq_track_t *st = &s->tr[t];
    ec_ptrack_t *tr = ptr(s, t);
    const float *v = tr->v;
    const int m = iv(v, EC_P_MACHINE);
    const ec_step_t *rec = idx >= 0 && idx < EC_MAXSTEPS ? &tr->steps[idx] : 0;
    int active, fires, ghost, had_region = 0, k;
    uint64_t note;

    if (reset) {
        holds_clear(st);
        st->last_cond = -1;
    }
    st->fire_pass = pass;
    active = rec && (is_slice(m) ? (rec->slices != 0 || rec->trig) : rec->trig);
    if (rec && !active && rec->nlock) {
        ghost_locks(s, t, rec, pos);
        if (ec_step_reset_active(rec, v, EC_P_SLIDE))
            start_slides(s, t, rec, pos);
    }
    fires = active && evaluate_trig(s, t, rec);
    ghost = fires && ec_step_is_ghost(rec, v);
    if (fires)
        st->rat_left = 0;                     /* (a new trig supersedes the last one's ratchets) */
    /* 0.6.4: a trig puts the sound back to the track values first (its own locks then apply), as Elektron's unlocked
     * trig; monophonic: it ends the previous note's region holds too. A ghost trig (no new note) carries them */
    if (!reset && fires && !ghost) {
        if (iv(v, EC_P_TRIG_POLY) == 1) {
            had_region = region_hold(st);
            holds_clear(st);
        } else
            holds_clear_sound(st);
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
            hold_set(s, st, id, sm->lock_val[sl][k], note);
        }
    }
    if (fires) {
        float x;                              /* 0.6.0: a slot lock: this trig (and the next ones without) play it */
        const int sl = ec_step_lock_get(rec, EC_P_SLOT, &x) ? clampi((int)floorf(x + 0.5f), 1, EC_NSLOT) : 0;
        if (sl != st->slot_lock) {
            st->slot_lock = sl;
            if (s->sink.slot)
                s->sink.slot(s->sink.ctx, t, ec_seq_track_slot(s, t));
        }
        for (k = 0; k < rec->nlock; k++) {
            const int id = rec->lock_id[k];
            if (id >= EC_NP || id == EC_P_SLOT || (EC_PARAMS[id].flags & (EC_PF_SEQ | EC_PF_MOMENT)))
                continue;
            hold_set(s, st, id, rec->lock_val[k], note);
        }
        hold_min_update(st);
        st->dirty = 1;
        if (ec_step_reset_active(rec, v, EC_P_SLIDE))
            start_slides(s, t, rec, pos);
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
            st->rat_index = idx;
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

/* ---- 0.6.0: micro timing. The grid (position, play_index, pass: what the screens show) moves on the step
 * boundaries as before; the firing has its own cursor, fpos, the next position to fire. Position n fires at its
 * boundary + its step's EC_P_MICRO (lock, else the track value) in 1/EC_MICRO_DIV of a step, so up to half a step
 * early: the cursor may run one position ahead of the grid (the lookahead reads position n + 1's step through the
 * pattern length and the page loop, so the loop's first step can fire early at the loop's end) or one behind (a late
 * step). Positions fire in order; one that is due late fires at once. An early step never fires before a queued
 * pattern switch at the global boundary (the new pattern's first step fires on the boundary instead), nor before the
 * transport starts. */
static int micro_of(const ec_seq_t *s, int t, int idx)
{
    const ec_ptrack_t *tr = ptr(s, t);
    const ec_step_t *rec = idx >= 0 && idx < EC_MAXSTEPS ? &tr->steps[idx] : 0;
    const float q = 1.0f - (tr->v[EC_P_QUANT] < 0 ? 0 : tr->v[EC_P_QUANT] > 100 ? 100 : tr->v[EC_P_QUANT]) / 100.0f;
    /* 0.6.59 (owner): QUANTIZE morphs the micro timing onto the grid (100 %: every step on its boundary) */
    return clampi((int)floorf(field(rec, tr->v, EC_P_MICRO) * q + 0.5f), -EC_MICRO_DIV / 2, EC_MICRO_DIV / 2);
}

static int switch_pending(const ec_seq_t *s) { return s->pending >= 0 || s->temp_return >= 0; }
static double global_boundary_beats(const ec_seq_t *s)
{
    return (double)(s->global_cycle + 1) * clampi(s->proj->glen, 1, 256) * 0.25;
}

static uint64_t fire_time(const ec_seq_t *s, int t, int64_t n)
{
    const ec_seq_track_t *st = &s->tr[t];
    const double b = ec_seq_boundary_beats(s, t, n);
    int pass, idx, mu;
    if (st->fpos < 0 || st->position < 0 || n > st->position + 1)
        return NEVER;
    idx = ec_e5_play_index(s->proj, t, ec_seq_pattern_steps(s, t), n, &pass);
    mu = micro_of(s, t, idx);
    if (mu < 0 && switch_pending(s) && b >= global_boundary_beats(s) - BEAT_EPS)
        mu = 0;
    return sample_at(s, b + (double)mu * bps(s, t) / EC_MICRO_DIV);
}
static uint64_t fire_next(const ec_seq_t *s, int t) { return fire_time(s, t, s->tr[t].fpos); }

static void fire(ec_seq_t *s, int t)
{
    ec_seq_track_t *st = &s->tr[t];
    const int64_t n = st->fpos;
    int pass, idx;
    const int reset = st->fire_reset;
    st->fpos = n + 1;
    st->fire_reset = 0;
    if (n == st->skip_pos) {               /* recorded live: already heard (ec_seq_rec_consume) */
        st->skip_pos = -1;
        return;
    }
    idx = ec_e5_play_index(s->proj, t, ec_seq_pattern_steps(s, t), n, &pass);
    enter_step(s, t, reset, idx, pass, n);
}

static void reset_track(ec_seq_t *s, int t)
{
    ec_seq_track_t *st = &s->tr[t];
    if (st->slot_lock) {                  /* 0.6.0: the track's own slot back */
        st->slot_lock = 0;
        st->dirty = 1;
        if (s->sink.slot)
            s->sink.slot(s->sink.ctx, t, ec_seq_track_slot(s, t));
    }
    holds_clear(st);
    st->position = -1;
    st->fpos = -1;
    st->skip_pos = -1;
    st->fire_reset = 0;
    st->fire_pass = 1;
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
    s->song_base += ec_seq_beats(s);       /* 0.6.0: ec_seq_song_beats keeps counting across pattern restarts */
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
        s->tr[t].fpos = 0;
        s->tr[t].fire_reset = 1;
        if (fire_next(s, t) <= s->now)     /* (no micro: the first step fires at once, as before 0.6.0) */
            fire(s, t);
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
        x = fire_next(s, t);
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
        int guard;
        if (st->position >= 0 && track_next(s, t) <= s->now) {
            anchor(s, t, st->position + 1);
            while (st->fpos >= 0 && st->fpos < st->position)   /* (a late step overtaken by the grid: now) */
                fire(s, t);
        }
        for (guard = 0; guard < 2 && fire_next(s, t) <= s->now; guard++)
            fire(s, t);
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
        sl = &s->proj->slot[clampi(ec_seq_track_slot(s, t), 1, EC_NSLOT) - 1];   /* (OFF: slot 1's meta, unused) */
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

/* 0.6.0: the metronome's beats in [now, now + n) (the clock is linear inside a piece: rebased only between pieces) */
static void add_beat(ec_seq_t *s, uint64_t t, uint64_t block0, int32_t beat, int accent, int countin)
{
    ec_seq_beat_t *b;
    if (s->nbeat >= EC_SEQ_BEATS_MAX)
        return;
    b = &s->beat[s->nbeat++];
    b->offset = (uint32_t)(t - block0);
    b->beat = beat;
    b->accent = (uint8_t)(accent ? 1 : 0);
    b->countin = (uint8_t)(countin ? 1 : 0);
}
static void collect_beats(ec_seq_t *s, uint64_t block0, int n)
{
    const uint64_t a = s->now, z = s->now + (uint64_t)n;
    if (s->start_at != NEVER && s->ci_n > 0) {          /* count-in beats */
        int j;
        for (j = 0; j < s->ci_n; j++) {
            const uint64_t t = s->ci_t0 + (uint64_t)llround(j * s->ci_spb);
            if (t >= a && t < z)
                add_beat(s, t, block0, (int32_t)(j - s->ci_n), j % 4 == 0, 1);
        }
    }
    if (s->playing) {
        const double sp = spb(s);
        int64_t k = (int64_t)floor(s->beat_base + (double)(a - s->sample_base) / sp) - 1;
        for (;; k++) {
            const double d = ((double)k - s->beat_base) * sp;
            uint64_t t;
            if (d < -1e-6)
                continue;                                    /* (before this clock's origin: the last segment's) */
            t = s->sample_base + (uint64_t)ceil(d - 1e-6);
            if (t >= z)
                break;
            if (t >= a)
                add_beat(s, t, block0, (int32_t)k, k % 4 == 0, 0);
        }
    }
}

static void start_now(ec_seq_t *s);

void ec_seq_render(ec_seq_t *s, float *out, int frames)
{
    int done = 0;
    const uint64_t block0 = s->now;
    s->nbeat = 0;
    while (done < frames) {
        uint64_t next = NEVER;
        int n = frames - done;
        if (s->start_at != NEVER && s->start_at <= s->now)   /* 0.6.0: an armed start (count-in, start_at) */
            start_now(s);
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
        for (int t = 0; t < EC_NTRACK; t++)       /* 0.5.0: the modulated params follow their sources (0.6.4: and
                                                   * the slides their ramps) */
            if (s->tr[t].mtx_gen || s->tr[t].sliding)
                s->tr[t].dirty = 1;
        flush(s);
        if (s->start_at != NEVER && s->start_at > s->now && (next == NEVER || s->start_at < next))
            next = s->start_at;
        if (next != NEVER && next - s->now < (uint64_t)n)
            n = (int)(next - s->now);
        collect_beats(s, block0, n);
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
    s->start_at = NEVER;
    for (t = 0; t < EC_NTRACK; t++) {
        reset_track(s, t);
        s->tr[t].aud_step = -1;
        s->tr[t].dirty = 1;
    }
}

void ec_seq_start(ec_seq_t *s)
{
    int t;
    ec_event_t e;
    s->start_at = NEVER;                          /* (an armed start is replaced) */
    s->ci_n = 0;
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
    s->beat_base = 0;                             /* 0.6.0: the song's beats count from here */
    s->sample_base = s->now;
    s->song_base = 0;
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
    s->start_at = NEVER;                          /* 0.6.0: an armed start is cancelled */
    s->ci_n = 0;
    s->song_base = 0;
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

/* 0.6.0: armed starts */
static void start_now(ec_seq_t *s)
{
    s->start_at = NEVER;
    s->ci_n = 0;
    s->paused = 0;                                /* (from the top) */
    ec_seq_start(s);
}

void ec_seq_count_in(ec_seq_t *s, int beats)
{
    if (beats <= 0) {
        start_now(s);
        return;
    }
    if (s->playing)
        return;
    s->ci_n = beats;
    s->ci_spb = 60.0 * s->fs / (s->proj->bpm > 1.0f ? s->proj->bpm : 1.0f);
    s->ci_t0 = s->now;
    s->start_at = s->now + (uint64_t)llround(beats * s->ci_spb);
}

void ec_seq_start_at(ec_seq_t *s, uint32_t frame)
{
    if (s->playing)
        return;
    s->ci_n = 0;
    s->start_at = s->now + frame;
}

int ec_seq_counting_in(const ec_seq_t *s)
{
    int j, left = 0;
    if (s->start_at == NEVER || s->ci_n <= 0)
        return 0;
    for (j = 0; j < s->ci_n; j++)
        left += s->ci_t0 + (uint64_t)llround(j * s->ci_spb) >= s->now;
    return left;
}

int ec_seq_block_beats(const ec_seq_t *s, ec_seq_beat_t *out, int max)
{
    int n = s->nbeat < max ? s->nbeat : max;
    if (n > 0 && out)
        memcpy(out, s->beat, (size_t)n * sizeof *out);
    return n < 0 ? 0 : n;
}

double ec_seq_song_beats(const ec_seq_t *s)
{
    if (!s->playing && !s->paused)
        return 0;
    return s->song_base + ec_seq_beats(s);
}

int ec_seq_rec_quantize(const ec_seq_t *s, int t, int *step, float *offset)
{
    double beats, b0, b1, off;
    int64_t p, n;
    int pass, idx;
    if (t < 0 || t >= EC_NTRACK || !s->playing)
        return -1;
    beats = ec_seq_beats(s);
    p = ec_seq_position_at(s, t, beats);
    b0 = ec_seq_boundary_beats(s, t, p);
    b1 = ec_seq_boundary_beats(s, t, p + 1);
    if (beats - b0 <= b1 - beats) {
        n = p;
        off = (beats - b0) / bps(s, t);
    } else {
        n = p + 1;
        off = (beats - b1) / bps(s, t);
    }
    idx = ec_e5_play_index(s->proj, t, ec_seq_pattern_steps(s, t), n, &pass);
    if (step)
        *step = idx;
    if (offset)
        *offset = (float)(off < -0.5 ? -0.5 : off > 0.5 ? 0.5 : off);
    return 0;
}

void ec_seq_rec_consume(ec_seq_t *s, int t, int step)
{
    ec_seq_track_t *st;
    int pass;
    if (t < 0 || t >= EC_NTRACK)
        return;
    st = &s->tr[t];
    if (st->fpos >= 0 && st->position >= 0 && st->fpos <= st->position + 1 &&
        ec_e5_play_index(s->proj, t, ec_seq_pattern_steps(s, t), st->fpos, &pass) == step)
        st->skip_pos = st->fpos;
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

void ec_seq_param_edited(ec_seq_t *s, int t, int id)
{
    ec_seq_track_t *st;
    if (t < 0 || t >= EC_NTRACK || id < 0 || id >= EC_NP || is_region(id))
        return;
    st = &s->tr[t];
    st->hold_on[id] = 0;
    slide_end(st, id);
    hold_min_update(st);
    st->dirty = 1;
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
        if (s->tr[t].position >= 0) {
            anchor(s, t, ec_seq_position_at(s, t, ec_seq_beats(s)));
            s->tr[t].fpos = s->tr[t].position + 1;   /* (the step under the playhead counts as played) */
        }
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
        st->live_semis = semis;
        st->live_pitch_on = 1;
        st->kb_down++;
        e.length = 0;
        emit(s, &e);
        if (!s->playing) {                         /* 0.6.1: stopped: the loop itself, from its start, while held */
            e = ev0(EC_EV_LOOP_KEY, t);
            e.slice = 0;
            e.value = 1.0f;
            e.flags = EC_TF_AUDITION;
            emit(s, &e);
        }
    } else {
        st->live_pitch_on = 0;
        st->kb_down = st->kb_down > 0 ? st->kb_down - 1 : 0;
        if (!st->kb_down && s->playing) {          /* 0.6.59 (owner): playing, the note ends as the key comes up (it
                                                    * held on; a recorded step ends after its length the same way) */
            ec_event_t e = ev0(EC_EV_NOTE_OFF, t);
            emit(s, &e);
        }
        if (!st->kb_down) {                        /* the last key up: the audition ends (a no-op when none) */
            ec_event_t e = ev0(EC_EV_LOOP_KEY, t);
            e.slice = 0;
            e.value = 0.0f;
            emit(s, &e);
        }
    }
    st->dirty = 1;
}

/* ------------------------------------------------------------------------------------------- reads --- */
int ec_seq_track_slot(const ec_seq_t *s, int t)
{
    if (t < 0 || t >= EC_NTRACK)
        return 1;
    return s->tr[t].slot_lock ? s->tr[t].slot_lock : clampi(iv(ptr(s, t)->v, EC_P_SLOT), 0, EC_NSLOT);   /* 0 = OFF */
}

void ec_seq_audition_step(ec_seq_t *s, int t, int idx)
{
    if (t < 0 || t >= EC_NTRACK || s->tr[t].aud_step == idx)
        return;
    s->tr[t].aud_step = idx;
    s->tr[t].dirty = 1;
}

float ec_seq_value(const ec_seq_t *s, int t, int id)
{
    const ec_seq_track_t *st = &s->tr[t];
    if (!s->playing && st->aud_step >= 0 && (EC_PARAMS[id].flags & EC_PF_REGION)) {   /* 0.6.30: stopped, a selected
                                                    * step: its region / range locks are what the pads and keys hear */
        const ec_ptrack_t *tr = ptr(s, t);
        float x;
        if (st->aud_step < EC_MAXSTEPS && ec_step_lock_get(&tr->steps[st->aud_step], id, &x))
            return x;
    }
    if (id == EC_P_PITCH && st->live_pitch_on)   /* 0.6.1: the track's PITCH now + the key (PITCH turned while held) */
        return ptr(s, t)->v[EC_P_PITCH] + st->live_semis;
    if (st->hold_on[id] && id != EC_P_LOOP_START && id != EC_P_LOOP_END) {
        if (st->slide_on[id]) {                    /* 0.6.4: a slide: the lock ramps to the next one */
            const uint64_t t0 = st->slide_t0[id], t1 = st->slide_t1[id];
            if (s->now >= t1)
                return st->slide_to[id];
            return st->hold_val[id] +
                   (st->slide_to[id] - st->hold_val[id]) * (float)((double)(s->now - t0) / (double)(t1 - t0));
        }
        return st->hold_val[id];
    }
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
