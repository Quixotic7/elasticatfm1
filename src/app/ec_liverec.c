/* SPDX-License-Identifier: GPL-3.0-only */
/* 0.6.0 (docs/UX-V2.md §4): live recording. REC toggles it in every mode but FILE (the REC LED: lit while
 * recording, blinking while CAPTURE waits for its first key or the count-in runs). While the transport runs, what is
 * played is written on the selected track:
 *   KEYBOARD's notes        a trig with the note's pitch, velocity (and slice) locked
 *   LOOP ENGINE's keys      a trig with the key's loop position locked (slice tracks: the pad's slice)
 *   knob moves              AUTOMATION: a lock of that param on the step, without a trig where there is none (a
 *                           lock-only "ghost" step); the macro values too; never the crossfader (not a param)
 * Each hit goes to the nearest step of the track's own clock (ec_seq_rec_quantize) and keeps its offset as an
 * EC_P_MICRO lock, unless QUANTIZE is HARD; an early hit's step does not fire again this pass (ec_seq_rec_consume).
 * The modes (the record settings: hold REC / SHIFT + REC, saved in the project, ec_recset_t):
 *   CAPTURE   0.6.59 (owner): REC arms it; the first key / loop key clears the track's pattern and starts playback
 *             from step 1 (restarting it when playing); the track's length grows with the playhead (up to 128 steps,
 *             where recording ends and the take loops). REC again: the length is where it got to (the nearest
 *             step) and the take loops from step 1 at once; PLAY: the same length, playback stops. Quick takes.
 *   FIXED     REC starts playback (after the count-in) when stopped; recording ends after LENGTH bars (playback goes on)
 *   OVERDUB   REC starts playback (after the count-in) when stopped; writes add to and replace steps
 * No mode clears anything: a recorded step replaces only what it writes (its trig, its locks); undo takes it back.
 * PLAY stopping the transport ends recording.
 * The metronome (OFF / REC: while recording / ALWAYS) and the count-in click on the main output after the master bus,
 * sample-accurate at ec_seq_block_beats' frames, in APP SETTINGS' sound at its own level; the count-in clicks even
 * with the metronome off. The SAMPLER's take and resampling never hear it (ec_rec_post runs before). */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ec_app.h"

static uint32_t g_pass;                 /* 0.6.55: live recording passes (REC on), for their undo entries */
static uint32_t g_snap_pass = 0xFFFFFFFFu;
static int g_snap_track = -1, g_snap_pat = -1;

#define A (&ec_app)
#define P (&ec_app.pn)
#define RS (&ec_app.proj.rec)

static int clampi(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }
static int iv(const float *v, int id) { return (int)floorf(v[id] + 0.5f); }

int ec_liverec_on(void) { return P->lr_on; }
int ec_liverec_armed(void) { return P->lr_on && (P->lr_wait || P->lr_ci); }

static void rec_off(const char *msg)
{
    if (P->lr_ci && !A->seq.playing && ec_seq_counting_in(&A->seq))
        ec_seq_stop(&A->seq);                      /* (the count-in REC started is cancelled) */
    P->lr_on = P->lr_wait = P->lr_ci = 0;
    P->lr_cap = 0;
    P->lr_end = -1;
    if (msg)
        ec_app_message(msg);
    A->redraw = 1;
}

#define CAP_MAX 128                              /* (pattern_steps' range) */

/* 0.6.59: CAPTURE's take ends: the track's length where the playhead got to (the nearest step boundary, 1 .. 128) */
static int cap_end(void)
{
    const int t = P->lr_cap_track;
    const double b = ec_seq_step_beats(&A->seq, t);
    float *v = ec_project_track(&A->proj, t)->v;
    int n = b > 0 ? (int)floor(ec_seq_beats(&A->seq) / b + 0.5) : 1;
    n = clampi(n, 1, CAP_MAX);
    v[EC_P_PATTERN_STEPS] = (float)n;
    P->lr_cap = 0;
    ec_seq_timing_changed(&A->seq);
    ec_seq_touch(&A->seq, t);
    ec_app_mark_dirty();
    return n;
}

void ec_liverec_toggle(void)
{
    if (P->lr_on && P->lr_cap) {                   /* 0.6.59: CAPTURE: the take's length, and it loops from step 1 */
        char b[20];
        const int n = cap_end();
        snprintf(b, sizeof b, "CAPTURED %d", n);
        rec_off(b);
        ec_seq_stop(&A->seq);
        ec_seq_start(&A->seq);
        return;
    }
    if (P->lr_on) {
        rec_off("REC OFF");
        return;
    }
    P->lr_on = 1;
    g_pass++;                                      /* 0.6.55: a new pass (its own undo entry) */
    P->lr_wait = P->lr_ci = 0;
    P->lr_writes = 0;
    P->lr_end = -1;
    if (RS->mode == EC_REC_CAPTURE) {             /* 0.6.59: armed, stopped or playing: the first key takes */
        P->lr_wait = 1;
        ec_app_message("REC: PLAY A KEY");
    } else if (!A->seq.playing) {
        {
            if (RS->countin) {
                ec_seq_count_in(&A->seq, RS->countin * 4);
                P->lr_ci = 1;
                ec_app_message("COUNT-IN");
            } else {
                ec_seq_start(&A->seq);
                ec_app_message("REC");
            }
            if (RS->mode == EC_REC_FIXED)
                P->lr_end = RS->bars * 4.0;        /* (the song's beats count from the start) */
        }
    } else {
        if (RS->mode == EC_REC_FIXED)
            P->lr_end = ec_seq_song_beats(&A->seq) + RS->bars * 4.0;
        ec_app_message("REC ON");
    }
    P->lr_was_playing = A->seq.playing;
    A->redraw = 1;
}

void ec_liverec_stop(void)
{
    if (P->lr_on && P->lr_cap) {                   /* 0.6.59: PLAY in a CAPTURE take: its length, then the stop */
        char b[20];
        snprintf(b, sizeof b, "CAPTURED %d", cap_end());
        rec_off(b);
        return;
    }
    if (P->lr_on)
        rec_off("REC OFF");
}

void ec_liverec_tick(void)
{
    if (!P->lr_on)
        return;
    if (A->seq.playing && P->lr_cap) {             /* 0.6.59: the take's length one step ahead of the playhead */
        const int t = P->lr_cap_track;
        float *v = ec_project_track(&A->proj, t)->v;
        const int64_t p = ec_seq_position_at(&A->seq, t, ec_seq_beats(&A->seq));
        if (p >= CAP_MAX) {                        /* the most: the take ends there and loops */
            v[EC_P_PATTERN_STEPS] = CAP_MAX;
            P->lr_cap = 0;
            ec_seq_timing_changed(&A->seq);
            rec_off("CAPTURED 128");
            return;
        }
        if (p + 2 > (int64_t)v[EC_P_PATTERN_STEPS]) {
            v[EC_P_PATTERN_STEPS] = (float)(p + 2 > CAP_MAX ? CAP_MAX : p + 2);
            ec_seq_timing_changed(&A->seq);
            A->redraw = 1;
        }
    }
    if (A->seq.playing) {
        P->lr_ci = 0;
        if (RS->mode != EC_REC_CAPTURE)            /* 0.6.61: CAPTURE armed while playing waits for its first key */
            P->lr_wait = 0;
        if (RS->mode == EC_REC_FIXED && P->lr_end >= 0 && ec_seq_song_beats(&A->seq) >= P->lr_end - 1e-9) {
            rec_off("REC DONE");
            return;
        }
    } else if (P->lr_was_playing && !P->lr_wait && !P->lr_ci) {
        rec_off("REC OFF");                        /* (stopped some other way) */
        return;
    }
    P->lr_was_playing = A->seq.playing;
}

/* ------------------------------------------------------------------------------------------- writing --- */
void ec_liverec_capture(void)
{
    if (P->lr_on && P->lr_wait && !A->bus) {       /* CAPTURE's first key (0.6.59: also while playing) */
        ec_ptrack_t *tr = ec_project_track(&A->proj, A->track);
        int i;
        ec_app_undo_track("CAPTURE", 0);           /* the track as it was: one undo for the whole take */
        g_snap_pass = g_pass;
        g_snap_track = A->track;
        g_snap_pat = A->proj.current;
        for (i = 0; i < EC_MAXSTEPS; i++)          /* its pattern cleared, every page in the loop */
            ec_step_clear(&tr->steps[i]);
        A->proj.pe5[A->proj.current].page_loop[A->track] = 0;
        tr->v[EC_P_PATTERN_STEPS] = 2;
        ec_seq_timing_changed(&A->seq);
        ec_seq_touch(&A->seq, A->track);
        if (A->seq.playing)
            ec_seq_stop(&A->seq);                  /* (from step 1: the take's first step is this key) */
        ec_seq_start(&A->seq);
        P->lr_wait = 0;
        P->lr_cap = 1;
        P->lr_cap_track = A->track;
        P->lr_was_playing = 1;
        A->step_page = 0;
        ec_app_message("CAPTURE");
    }
}

/* the step a hit now lands on and its micro offset; 0: nothing is recorded now */
static int hit(int *step, int *mu)
{
    float off = 0;
    if (!P->lr_on || P->lr_wait || A->bus || !A->seq.playing)
        return 0;
    if (RS->mode == EC_REC_FIXED && P->lr_end >= 0 && ec_seq_song_beats(&A->seq) >= P->lr_end)
        return 0;
    if (ec_seq_rec_quantize(&A->seq, A->track, step, &off) < 0 || *step < 0 || *step >= EC_MAXSTEPS)
        return 0;
    *mu = RS->hard ? 0 : ec_micro_units(off);
    return 1;
}

/* 0.6.55 (QA B9): one undo entry a pass and track (the track as it was before its first recorded step), not one a
 * step: a bar of automation no longer pushes the rest of the history out */
static ec_step_t *step_rec(int step)
{
    ec_ptrack_t *tr = ec_project_track(&A->proj, A->track);
    if (g_snap_pass != g_pass || g_snap_track != A->track || g_snap_pat != A->proj.current) {
        g_snap_pass = g_pass;
        g_snap_track = A->track;
        g_snap_pat = A->proj.current;
        ec_app_undo_track("REC", 0);
    }
    (void)step;
    return &tr->steps[step];
}

static void locked(ec_step_t *s, int id, float v)
{
    if (ec_step_lock_set(s, id, v) < 0)
        ec_app_message("LOCKS FULL");
}

static ec_step_t *trig_at(int step, int mu, int vel)
{
    ec_step_t *s = step_rec(step);
    s->trig = 1;
    if (mu)
        locked(s, EC_P_MICRO, (float)mu);
    else
        ec_step_lock_clear(s, EC_P_MICRO);
    if (vel != 100)
        locked(s, EC_P_VELOCITY, (float)vel / 127.0f);
    else
        ec_step_lock_clear(s, EC_P_VELOCITY);
    return s;
}

/* 0.6.59 (owner): a take plays back as played: each key's note keeps how long it was held (a LENGTH lock on its step,
 * in quarter steps, 0.25 .. 16) */
static int g_last_step = -1;                       /* the step the last note / loop key wrote, this call (-1 none) */
static int g_key_step[64];                        /* (the step + 1; 0: none) */
static double g_key_beats[64];

void ec_liverec_key(int k, int down)
{
    if (k < 0 || k >= 64)
        return;
    if (down) {
        g_key_step[k] = g_last_step + 1;
        g_key_beats[k] = ec_seq_beats(&A->seq);
        g_last_step = -1;
        return;
    }
    if (g_key_step[k] > 0 && P->lr_on && A->seq.playing) {
        const double b = ec_seq_step_beats(&A->seq, A->track);
        double len = b > 0 ? (ec_seq_beats(&A->seq) - g_key_beats[k]) / b : 1.0;
        ec_step_t *s = &ec_project_track(&A->proj, A->track)->steps[g_key_step[k] - 1];
        len = floor(len * 4.0 + 0.5) / 4.0;
        len = len < 0.25 ? 0.25 : len > 16.0 ? 16.0 : len;
        if (s->trig)
            locked(s, EC_P_LENGTH, (float)len);
        ec_seq_touch(&A->seq, A->track);
    }
    g_key_step[k] = 0;
}

static void written(int step, int trig)
{
    g_last_step = trig ? step : -1;
    if (trig)
        ec_seq_rec_consume(&A->seq, A->track, step);   /* (heard live: an early hit's step does not fire again) */
    ec_seq_touch(&A->seq, A->track);
    P->lr_writes++;
    ec_app_mark_dirty();
}

void ec_liverec_note(int semis, int slice, int vel)
{
    static int last = -1;                          /* 0.6.84: the step a chord being played writes into */
    static uint32_t last_ms;
    const int poly = iv(ec_app_params(A), EC_P_TRIG_POLY) == 2;
    int step, mu, chord;
    ec_step_t *s;
    if (!hit(&step, &mu))
        return;
    /* 0.6.82 / 0.6.84 (owner): POLY, keys held together: one step (a key within 80 ms of the chord's first joins its
     * step even when it falls nearer the next one) */
    chord = poly && (slice > 0 || ec_kb_chords()) && __builtin_popcount(P->kb_held) > 1 && last >= 0 &&
            (step == last || A->ms - last_ms < 80);
    if (chord)
        step = last;
    else {
        last = step;
        last_ms = A->ms;
    }
    s = chord ? step_rec(step) : trig_at(step, mu, vel);
    if (!slice) {
        if ((chord ? ec_step_note_add(s, (float)clampi(semis, -36, 36))
                   : ec_step_note_set(s, (float)clampi(semis, -36, 36))) < 0)   /* 0.6.80: the NOTE */
            ec_app_message("LOCKS FULL");
    } else {                                       /* a slice: its pitch (one a step) and the slice; 0.6.84: POLY,
                                                    * the slices held together stack */
        if (!chord && ec_step_note_set(s, (float)clampi(semis, -36, 36)) < 0)
            ec_app_message("LOCKS FULL");
        s->slices = (chord ? s->slices : 0u) | 1u << (clampi(slice, 1, 32) - 1);
    }
    written(step, 1);
}

void ec_liverec_loop(int key, int slice_track, int vel)
{
    const float *v = ec_app_params(A);
    int step, mu;
    ec_step_t *s;
    if (!hit(&step, &mu))
        return;
    s = trig_at(step, mu, vel);
    key = clampi(key, 1, 32);
    if (slice_track) {                             /* a pad: its slice (poly machines stack) */
        const int poly = iv(v, EC_P_TRIG_POLY) == 2;   /* 0.6.81: VOICE MODE */
        s->slices = (poly ? s->slices : 0u) | 1u << (key - 1);
    } else {                                       /* the loop key's span (the loop keys' division, as E5's preview) */
        const int div = clampi((iv(v, EC_P_LOOP_DIVISION) + 1) / 2 * 2, 2, 32);
        const float units = 128.0f / (float)div;
        const int k = clampi(key, 1, div);
        locked(s, EC_P_LOOP_START, (float)(k - 1) * units);
        locked(s, EC_P_LOOP_END, (float)k * units);
    }
    written(step, 1);
}

void ec_liverec_param(int id, float value)
{
    int step, mu;
    if (!RS->automation || id < 0 || id >= EC_NP || id == EC_P_SLOT ||
        (EC_PARAMS[id].flags & (EC_PF_SEQ | EC_PF_MOMENT | EC_PF_REGION)) || !(EC_PARAMS[id].flags & EC_PF_LOCKABLE))
        return;
    if (!hit(&step, &mu))
        return;
    locked(step_rec(step), id, value);             /* (no trig: a lock-only step where there is none) */
    written(step, 0);
}

/* ------------------------------------------------------------------------------------------- the audio --- */
void ec_liverec_audio(int16_t *out, uint32_t n, const int16_t *in, uint32_t in_n)
{
    ec_seq_beat_t b[EC_SEQ_BEATS_MAX];
    int idx[EC_SEQ_BEATS_MAX];
    const int nb = ec_seq_block_beats(&A->seq, b, EC_SEQ_BEATS_MAX);
    int i, k = 0;
    for (i = 0; i < nb; i++) {
        const int want = b[i].countin || RS->metro == EC_METRO_ALWAYS ||
                         (RS->metro == EC_METRO_REC && P->lr_on && !P->lr_wait);
        if (want && RS->metro_level > 0.0f && b[i].offset < n)
            idx[k++] = i;
    }
    if (!k)
        ec_click_mix(&P->click, out, n, 0);
    else {
        uint32_t s = 0;
        for (i = 0; i < k; i++) {
            const ec_seq_beat_t *bb = &b[idx[i]];
            const uint32_t e = i + 1 < k ? b[idx[i + 1]].offset : n;
            ec_click_trigger(&P->click, bb->accent, ec_as->metro, RS->metro_level);
            ec_click_mix(&P->click, out + 2 * s, e - s, bb->offset - s);
            s = e;
        }
    }
    if (P->smp_mon && in) {                        /* the SAMPLER's input monitor */
        uint32_t j;
        for (j = 0; j < 2 * in_n && j < 2 * n; j++) {
            const int x = out[j] + in[j];
            out[j] = (int16_t)(x > 32767 ? 32767 : x < -32768 ? -32768 : x);
        }
    }
    ec_file_preview_mix(out, n);                   /* FILE's preview */
}

/* ------------------------------------------------------------------------------------------- the settings --- */
static const float LVL_DB[] = {-99, -36, -30, -24, -18, -12, -9, -6, -3, 0};   /* -99: OFF */
#define NLVL ((int)(sizeof LVL_DB / sizeof LVL_DB[0]))

static int lvl_index(float lv)
{
    int i, best = 0;
    float bd = 1e9f;
    for (i = 0; i < NLVL; i++) {
        const float x = i ? powf(10.0f, LVL_DB[i] / 20.0f) : 0.0f;
        if (fabsf(x - lv) < bd) {
            bd = fabsf(x - lv);
            best = i;
        }
    }
    return best;
}

void ec_liverec_settings(void)
{
    if (P->rs_open) {
        ec_rs_close(1);
        return;
    }
    P->rs_undo = A->proj.rec;
    P->rs_open = 1;
    P->rs_row = 0;
    A->redraw = 1;
}

void ec_rs_close(int keep)
{
    if (!P->rs_open)
        return;
    if (!keep)
        A->proj.rec = P->rs_undo;
    else if (memcmp(&P->rs_undo, &A->proj.rec, sizeof A->proj.rec))
        ec_app_mark_dirty();
    P->rs_open = 0;
    A->redraw = 1;
}

const char *ec_rs_row_label(int row)
{
    static const char *const L[EC_RS_NROW] = {"MODE", "LENGTH", "COUNT-IN", "METRONOME", "CLICK LVL", "AUTOMATION",
                                              "QUANTIZE"};
    return row >= 0 && row < EC_RS_NROW ? L[row] : "";
}

void ec_rs_row_value(int row, char *out, int cap)
{
    static const char *const M[3] = {"CAPTURE", "FIXED", "OVERDUB"}, *const MT[3] = {"OFF", "REC", "ALWAYS"};
    const ec_recset_t *r = RS;
    switch (row) {
    case 0: snprintf(out, (size_t)cap, "%s", M[clampi(r->mode, 0, 2)]); break;
    case 1: snprintf(out, (size_t)cap, "%d BAR%s", r->bars, r->bars > 1 ? "S" : ""); break;
    case 2:
        if (r->countin)
            snprintf(out, (size_t)cap, "%d BAR%s", r->countin, r->countin > 1 ? "S" : "");
        else
            snprintf(out, (size_t)cap, "OFF");
        break;
    case 3: snprintf(out, (size_t)cap, "%s", MT[clampi(r->metro, 0, 2)]); break;
    case 4: {
        const int i = lvl_index(r->metro_level);
        if (!i)
            snprintf(out, (size_t)cap, "OFF");
        else
            snprintf(out, (size_t)cap, "%+dDB", (int)LVL_DB[i]);
        if (i == NLVL - 1)
            snprintf(out, (size_t)cap, "0DB");
    } break;
    case 5: snprintf(out, (size_t)cap, "%s", r->automation ? "ON" : "OFF"); break;
    case 6: snprintf(out, (size_t)cap, "%s", r->hard ? "HARD" : "KEEP"); break;
    default: out[0] = 0; break;
    }
}

int ec_rs_row_dim(int row) { return row == 1 && RS->mode != EC_REC_FIXED; }

static void edit(int row, int d)
{
    static const uint8_t BARS[4] = {1, 2, 4, 8};
    ec_recset_t *r = RS;
    const int s = d > 0 ? 1 : -1;
    switch (row) {
    case 0: r->mode = (uint8_t)clampi(r->mode + s, 0, 2); break;
    case 1: {
        int i = 0;
        while (i < 3 && BARS[i] < r->bars)
            i++;
        r->bars = BARS[clampi(i + s, 0, 3)];
    } break;
    case 2: r->countin = (uint8_t)clampi(r->countin + s, 0, 2); break;
    case 3: r->metro = (uint8_t)clampi(r->metro + s, 0, 2); break;
    case 4: {
        const int i = clampi(lvl_index(r->metro_level) + s, 0, NLVL - 1);
        r->metro_level = i ? powf(10.0f, LVL_DB[i] / 20.0f) : 0.0f;
    } break;
    case 5: r->automation = d > 0; break;
    case 6: r->hard = d > 0; break;
    default: break;
    }
}

void ec_rs_knob(int role, int det)
{
    int n = det < 0 ? -det : det;
    if (ec_app_list_knob(role, det, &P->rs_row, EC_RS_NROW))   /* 0.6.1: a list menu: SEL the row, K1 its value */
        for (; n > 0; n--)                         /* (a detent a step) */
            edit(P->rs_row, det);
    A->redraw = 1;
}
