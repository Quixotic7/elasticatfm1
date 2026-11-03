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
 *   CAPTURE   stopped: the first key / loop key starts playback (no count-in); then as OVERDUB
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
    P->lr_end = -1;
    if (msg)
        ec_app_message(msg);
    A->redraw = 1;
}

void ec_liverec_toggle(void)
{
    if (P->lr_on) {
        rec_off("REC OFF");
        return;
    }
    P->lr_on = 1;
    P->lr_wait = P->lr_ci = 0;
    P->lr_writes = 0;
    P->lr_end = -1;
    if (!A->seq.playing) {
        if (RS->mode == EC_REC_CAPTURE) {
            P->lr_wait = 1;                        /* the first key starts it */
            ec_app_message("REC: PLAY TO START");
        } else {
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
    if (P->lr_on)
        rec_off("REC OFF");
}

void ec_liverec_tick(void)
{
    if (!P->lr_on)
        return;
    if (A->seq.playing) {
        P->lr_ci = 0;
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
    if (P->lr_on && P->lr_wait && !A->seq.playing && !A->bus) {
        ec_seq_start(&A->seq);                     /* CAPTURE: the first key starts playback */
        P->lr_wait = 0;
        P->lr_was_playing = 1;
        ec_app_message("REC");
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

static ec_step_t *step_rec(int step)
{
    ec_ptrack_t *tr = ec_project_track(&A->proj, A->track);
    ec_undo_steps(&A->e5.undo, &A->proj, A->track, step / 16, (uint16_t)(1u << (step % 16)), "REC",
                  0xE000u + (uint32_t)step, A->ms);
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

static void written(int step, int trig)
{
    if (trig)
        ec_seq_rec_consume(&A->seq, A->track, step);   /* (heard live: an early hit's step does not fire again) */
    ec_seq_touch(&A->seq, A->track);
    P->lr_writes++;
    ec_app_mark_dirty();
}

void ec_liverec_note(int semis, int slice, int vel)
{
    int step, mu;
    ec_step_t *s;
    if (!hit(&step, &mu))
        return;
    s = trig_at(step, mu, vel);
    locked(s, EC_P_PITCH, (float)clampi(semis, -24, 24));
    if (slice > 0)
        s->slices = 1u << (clampi(slice, 1, 32) - 1);   /* (one note per step, as step entry) */
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
        const int poly = iv(v, EC_P_MACHINE) == EC_MACH_SLICE_POLY || iv(v, EC_P_MACHINE) == EC_MACH_RAZOR_POLY;
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
    if (role == EC_E_SELECT)
        P->rs_row = clampi(P->rs_row + (det > 0 ? 1 : -1), 0, EC_RS_NROW - 1);
    else if (role >= EC_E_K1) {
        const int row = P->rs_row / 4 * 4 + (role - EC_E_K1);
        if (row < EC_RS_NROW) {
            edit(row, det);
            P->rs_row = row;
        }
    }
    A->redraw = 1;
}
