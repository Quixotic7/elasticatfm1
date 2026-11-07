/* SPDX-License-Identifier: GPL-3.0-only */
/* 0.6.0 (docs/UX-V2.md §3 KEYBOARD, HOME third): the keybed is a full keyboard for the selected track (C4 = the
 * sample's own pitch; F1 / F2 the octave). On slice tracks (SLICE / RAZOR) the black keys pick the slice, ten at a
 * time (B11 steps the bank), and the white keys play the selected slice at a pitch. ALGO is the target step: below 1
 * is live (no entry); with a target, a key writes a trig with that note (and slice) there, one note per step, then
 * advances (or stays: KEYBOARD's ADVANCE setting in HOME's settings; SHIFT + the key does the other). Live (no
 * target), REC records the notes (ec_liverec.c). 0.6.1: stopped, a key plays the loop (slice tracks: the slice) at its
 * pitch while held (ec_seq_keyboard); the key lit is the note sounding (the key + PITCH, so a positive PITCH lights a
 * higher key); ALGO shows nothing but the step; the header names the key held. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ec_app.h"
#include "ec_holdmenu.h"

#define A (&ec_app)
#define P (&ec_app.pn)
#define E (&ec_app.e5)

static int clampi(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }
static int iv(const float *v, int id) { return (int)floorf(v[id] + 0.5f); }

int ec_kb_slice_track(void)
{
    const int m = iv(ec_app_params(A), EC_P_MACHINE);
    return !A->bus && m >= EC_MACH_GRID_SLICE && m <= EC_MACH_RAZOR_POLY;
}

/* 0.6.62 (owner): the scales (semitones up from the root) */
static const int8_t SC[][12] = {{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}, {0, 2, 4, 5, 7, 9, 11}, {0, 2, 3, 5, 7, 8, 10},
                                {0, 2, 3, 5, 7, 9, 10}, {0, 1, 3, 5, 7, 8, 10}, {0, 2, 4, 6, 7, 9, 11},
                                {0, 2, 4, 5, 7, 9, 10}, {0, 1, 3, 5, 6, 8, 10}, {0, 2, 3, 5, 7, 8, 11},
                                {0, 2, 3, 5, 7, 9, 11}, {0, 2, 4, 7, 9}, {0, 3, 5, 7, 10}, {0, 3, 5, 6, 7, 10},
                                {0, 2, 4, 6, 8, 10}};
static const uint8_t SCN[] = {12, 7, 7, 7, 7, 7, 7, 7, 7, 7, 5, 5, 6, 6};
static int scale_of(int *n, const int8_t **st)
{
    const int sc = clampi(iv(ec_app_params(A), EC_P_KB_SCALE), 1, (int)sizeof SCN) - 1;
    *n = SCN[sc];
    *st = SC[sc];
    return sc;
}
int ec_kb_root(void) { return A->bus ? 0 : clampi(iv(ec_app_params(A), EC_P_KB_ROOT), 1, 12) - 1; }
/* 0.6.64: KEYBOARD on the STEP layer (B11) on a loop track: the black keys play the scale (grouped) */
int ec_kb_step_notes(void)
{
    return !A->bus && !ec_kb_slice_track() && ec_app_mode() == EC_MODE_KEYBOARD && ec_lp_step_layer();
}

int ec_kb_scale_layout(void)
{
    return !A->bus && !ec_kb_slice_track() && iv(ec_app_params(A), EC_P_KB_LAYOUT) == 2;
}
/* the note semis is in the scale (CHROM: all) */
int ec_kb_in_scale(int semis)
{
    const int8_t *st;
    int n, i, d;
    scale_of(&n, &st);
    d = ((semis - ec_kb_root()) % 12 + 12) % 12;
    for (i = 0; i < n; i++)
        if (st[i] == d)
            return 1;
    return 0;
}
/* 0.6.69 (owner): a scale on: a note outside it plays the nearest note in it (a tie: the lower) */
int ec_kb_snap(int semis)
{
    int d;
    if (semis == EC_KB_NONE || ec_kb_in_scale(semis))
        return semis;
    for (d = 1; d < 12; d++) {
        if (ec_kb_in_scale(semis - d))
            return semis - d;
        if (ec_kb_in_scale(semis + d))
            return semis + d;
    }
    return semis;
}
/* key k's note in semitones from C4: CHROMATIC the keybed (key 7 = C4); IN SCALE the white keys the scale's degrees up
 * from the root (W1 = the root at the octave shown), the black keys none (EC_KB_NONE: they are shortcuts) */
int ec_kb_semis(int k)
{
    const int up0 = ec_key_upper(k);
    if (up0 >= 0 && up0 < 10 && ec_kb_step_notes()) {   /* 0.6.64 (owner, FIZZ): the STEP layer: B1..B10 the scale's
                                                    * notes up from the root (the white keys select steps) */
        const int8_t *st;
        int n;
        scale_of(&n, &st);
        return ec_kb_root() + 12 * (up0 / n) + st[up0 % n] + 12 * P->oct;
    }
    if (ec_kb_scale_layout()) {
        const int8_t *st;
        int n;
        const int lo = ec_key_lower(k);
        if (lo < 0)
            return EC_KB_NONE;
        scale_of(&n, &st);
        return ec_kb_root() + 12 * (lo / n) + st[lo % n] + 12 * P->oct;
    }
    return k - 7 + 12 * P->oct;
}

/* 0.6.62 (owner): KEYBOARD's knob pages (SEL; IN SCALE: B1..B3 too): the notes, the scale, the sound */
void ec_kb_page(int d, int absolute)
{
    const int np = ec_kb_pages();
    const int p = absolute ? clampi(d, 0, np - 1) : clampi(P->kb_page + d, 0, np - 1);
    if (p != P->kb_page) {
        P->kb_page = p;
        ec_app_message(ec_kb_page_name(p));        /* (0.6.66: TRIG, COND, PLAYHEAD, WARP lanes after its own) */
    }
    A->redraw = 1;
}

/* IN SCALE's black keys: B1..B3 the knob pages (a click), B6..B10 the mod sources (held + a knob: that source onto
 * the knob's param, as the source keys elsewhere) */
static void scale_black(int up, int down)
{
    if (up >= 0 && up < 5) {                       /* 0.6.74 (owner): as LOOP ENGINE's B1..B5: a hold its menu
                                                    * (ADVANCE, LOOP MODE, MACHINE, WARP MODE, VOICE MODE), a quick
                                                    * click the key's pages (B2 NOTES / SOUND, B5 SCALE: KEYBOARD's) */
        if (down) {
            ec_src_arm_loop_menu(up + 1);
            return;
        }
        if (ec_hm_armed() == up + 1 && ec_hm_up(up + 1))
            return;                                /* (the menu picked) */
        {
            const int p = ec_kb_page_key(P->kb_page) == up ? ec_kb_group_next(up, P->kb_page) : ec_kb_group_page(up);
            if (p >= 0)
                ec_kb_page(p, 1);
        }
        return;
    }
    if (up >= 5 && up <= 9) {
        if (down)
            ec_mk_hold(EC_MK_LFO1 + (up - 5));
        else
            ec_mk_release();
    }
}

void ec_kb_oct(int d)
{
    char b[12];
    P->oct = clampi(P->oct + d, -3, 3);
    snprintf(b, sizeof b, "OCT %+d", P->oct);
    ec_app_message(b);
}

void ec_kb_bank_next(void)
{
    const int cnt = clampi(iv(ec_app_params(A), EC_P_SLICE_COUNT), 1, 32);
    const int banks = (cnt + 9) / 10;
    char b[16];
    P->kb_bank = (P->kb_bank + 1) % (banks > 0 ? banks : 1);
    snprintf(b, sizeof b, "SLICES %d-%d", P->kb_bank * 10 + 1, P->kb_bank * 10 + 10);
    ec_app_message(b);
}

void ec_kb_target(int det)
{
    const int len = ec_seq_pattern_steps(&A->seq, A->track);
    P->target = clampi(P->target + det, 0, len);   /* 0.6.1: no takeover, no message: the screen's target step */
    if (P->target)
        A->step_page = (P->target - 1) / 16;
    A->redraw = 1;
}

/* 0.6.82 (owner): VOICE MODE POLY on a loop track: keys held together are one chord on one step */
int ec_kb_chords(void)
{
    return !A->bus && iv(ec_app_params(A), EC_P_MACHINE) == EC_MACH_LOOP_TRIG &&
           iv(ec_app_params(A), EC_P_TRIG_POLY) == 2;   /* (TRIG only: LOOP is one playhead, slices one pitch each) */
}
static int g_chord_idx = -1;                       /* the step a chord being entered writes into (-1 none) */
static int g_chord_adv;                            /* its advance, once every key is up */

/* one note on the target step: its trig, pitch (and slice, velocity) */
static void enter_step(int semis, int slice, int vel)
{
    ec_ptrack_t *tr = ec_project_track(&A->proj, A->track);
    const int len = ec_seq_pattern_steps(&A->seq, A->track);
    const int chord = ec_kb_chords() && g_chord_idx >= 0 && __builtin_popcount(P->kb_held) > 1;
    const int idx = chord ? g_chord_idx : clampi(P->target, 1, len) - 1;
    ec_step_t *s = &tr->steps[idx];
    int adv = ec_as->advance != 0;
    ec_undo_steps(&E->undo, &A->proj, A->track, idx / 16, (uint16_t)(1u << (idx % 16)), "NOTE", 0xD00u + (uint32_t)idx,
                  A->ms);
    E->undo.last_key = 0;
    s->trig = 1;
    if ((chord ? ec_step_note_add(s, (float)clampi(semis, -36, 36))
               : ec_step_note_set(s, (float)clampi(semis, -36, 36))) < 0)   /* 0.6.80: the NOTE, not PITCH */
        ec_app_message("LOCKS FULL");
    if (slice > 0)
        s->slices = 1u << (slice - 1);             /* one note per step: the slice replaces the step's */
    if (vel != 100)
        ec_step_lock_set(s, EC_P_VELOCITY, (float)vel / 127.0f);
    else
        ec_step_lock_clear(s, EC_P_VELOCITY);     /* 0.6.56 (QA C15): a full-velocity note replaces a softer one's */
    ec_seq_touch(&A->seq, A->track);
    ec_app_mark_dirty();
    if (A->fn)                                     /* SHIFT + the key: the other */
        adv = !adv;
    if (ec_kb_chords()) {                          /* 0.6.82: a chord: the target moves once every key is up */
        if (!chord)
            g_chord_idx = idx;
        g_chord_adv = adv;
        return;
    }
    if (adv) {
        ec_lp_target_to(P->target % len + 1);     /* 0.6.54 (QA C5): the selection moves with it (ALGO's start) */
        A->step_page = (P->target - 1) / 16;
    }
}

/* 0.6.82: every key up after a chord's entry: the target moves on (ADVANCE) */
static void chord_done(void)
{
    const int len = ec_seq_pattern_steps(&A->seq, A->track);
    if (g_chord_idx < 0 || P->kb_held)
        return;
    g_chord_idx = -1;
    if (g_chord_adv && P->target && len > 0) {
        ec_lp_target_to(P->target % len + 1);
        A->step_page = (P->target - 1) / 16;
    }
    A->redraw = 1;
}

void ec_kb_key(int k, int down, int vel)
{
    int entry;
    const int up = ec_key_upper(k);
    const int slice = ec_kb_slice_track();
    float *v = ec_app_params(A);
    if (A->bus)
        return;
    if (!down) {
        if (ec_kb_scale_layout() && up >= 0 && !(P->kb_held >> k & 1u) && !ec_kb_step_notes()) {
            scale_black(up, 0);                    /* 0.6.62: a shortcut key up */
            return;
        }
        if (P->kb_held >> k & 1u) {
            P->kb_held &= ~(1u << k);
            if (slice && P->kb_slice[k] > 0)       /* 0.6.81: the slice this key started (not the one picked now) */
                ec_seq_slice_pad(&A->seq, A->track, P->kb_slice[k], 0, 0);
            else if (P->kb_voice[k] > 0) {         /* 0.6.82: a chord key's own voice */
                ec_seq_chord_key(&A->seq, A->track, P->kb_voice[k], 0, 0, 0);
                P->kb_voice[k] = 0;
            } else if (!slice)
                ec_seq_keyboard(&A->seq, A->track, (float)P->kb_semis[k], 0, 0);
            P->kb_slice[k] = 0;
            ec_liverec_key(k, 0);                  /* 0.6.59: live REC: the note's length */
            chord_done();                          /* 0.6.82 */
            if (!slice && P->kb_held && !ec_kb_chords()) {   /* 0.6.67: keys overlapping: the newest still held sounds
                                                    * (0.6.82: POLY: each key its own voice instead) */
                int j, best = -1;
                for (j = 0; j < EC_NKEY && j < 32; j++)
                    if ((P->kb_held >> j & 1u) && (best < 0 || P->kb_stamp[j] > P->kb_stamp[best]))
                        best = j;
                if (best >= 0) {
                    P->kb_last = best;
                    ec_seq_keyboard_retune(&A->seq, A->track, (float)P->kb_semis[best]);
                }
            }
        }
        A->redraw = 1;
        return;
    }
    if (ec_kb_scale_layout() && up >= 0 && !ec_kb_step_notes()) {   /* 0.6.62: IN SCALE: the black keys are
                                                    * shortcuts (not on the STEP layer: there they are the notes) */
        scale_black(up, 1);
        return;
    }
    P->kb_held |= 1u << k;                         /* 0.6.1: the header's key */
    P->kb_last = k;
    P->kb_stamp[k] = ++P->kb_clock;
    entry = P->target && !ec_liverec_on();         /* 0.6.57 (owner): live REC records what is played where it is
                                                    * played, a target step or not (it wrote the target and advanced) */
    if (!entry && !(slice && up == 10))
        ec_liverec_capture();                      /* live REC: CAPTURE's first note starts playback */
    if (slice && up >= 0) {                        /* slice tracks: the black keys pick the slice */
        const int cnt = clampi(iv(v, EC_P_SLICE_COUNT), 1, 32);
        if (up == 10) {                            /* B11: the bank (0.6.14: a quick click; held = the pages) */
            ec_kb_bank_next();
            P->kb_semis[k] = 0;
            return;
        }
        {
            const int s = P->kb_bank * 10 + up + 1;
            if (s > cnt) {
                P->kb_semis[k] = 0;
                return;
            }
            v[EC_P_SLICE_INDEX] = (float)s;        /* (a pick, not an edit: no undo) */
            ec_seq_touch(&A->seq, A->track);
            P->kb_semis[k] = 0;
            P->kb_slice[k] = (int8_t)s;
            ec_seq_keyboard(&A->seq, A->track, 0, 1, (float)vel / 127.0f);   /* heard */
            if (entry)
                enter_step(0, s, vel);
            else {
                ec_liverec_note(0, s, vel);        /* live REC: the slice */
                ec_liverec_key(k, 1);
            }
            A->redraw = 1;
            return;
        }
    }
    {
        const int semis = ec_kb_snap(ec_kb_semis(k));   /* 0.6.69: outside the scale: its nearest note */
        P->kb_semis[k] = (int8_t)clampi(semis, -60, 60);
        P->kb_slice[k] = (int8_t)(slice ? clampi(iv(v, EC_P_SLICE_INDEX), 1, 32) : 0);
        P->kb_voice[k] = 0;
        if (ec_kb_chords() && A->seq.tr[A->track].kb_down > 0) {   /* 0.6.82: POLY TRIG: a key past the first sounds
                                                    * as its own voice (the first plays the track) */
            int j, slot = 9;
            for (j = 0; j < EC_NKEY && j < 32; j++)
                if (P->kb_voice[j] >= slot && P->kb_voice[j] < 32)
                    slot = P->kb_voice[j] + 1;
            P->kb_voice[k] = (int8_t)(slot < 32 ? slot : 32);
            ec_seq_chord_key(&A->seq, A->track, P->kb_voice[k], (float)semis, 1, (float)vel / 127.0f);
        } else
            ec_seq_keyboard(&A->seq, A->track, (float)semis, 1, (float)vel / 127.0f);
        if (entry)
            enter_step(semis, slice ? clampi(iv(v, EC_P_SLICE_INDEX), 1, 32) : 0, vel);
        else {                                     /* live REC: the note (pitch, velocity, slice) */
            ec_liverec_note(semis, slice ? clampi(iv(v, EC_P_SLICE_INDEX), 1, 32) : 0, vel);
            ec_liverec_key(k, 1);
        }
    }
    A->redraw = 1;
}

/* 0.6.1: the key lit as sounding: the note held plus the track's PITCH (ec_seq_value: v[PITCH] + the key's semitones),
 * on the keybed at the octave shown; -1 none (or off the keybed) */
int ec_kb_lit_key(void)
{
    int k;
    if (A->bus || ec_kb_slice_track() || !A->seq.tr[A->track].live_pitch_on)
        return -1;
    if (ec_kb_scale_layout()) {                    /* 0.6.62: IN SCALE: the white key that plays the note held */
        for (k = 0; k < EC_NKEY; k++)
            if (ec_key_lower(k) >= 0 && ec_kb_semis(k) == A->seq.tr[A->track].live_semis)
                return k;
        return -1;
    }
    k = (int)floorf(ec_seq_value(&A->seq, A->track, EC_P_NOTE) + 0.5f) + 7 - 12 * P->oct;   /* 0.6.80: the note */
    return k >= 0 && k < EC_NKEY ? k : -1;
}

const char *ec_kb_held_text(void)
{
    static const char *const N[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    static char b[24];
    const int k = P->kb_last;
    int j, n = 0, used[32] = {0};
    if (A->bus || ec_app_mode() != EC_MODE_KEYBOARD || !P->kb_held)
        return 0;
    if (ec_kb_slice_track()) {                     /* a slice pick (B11: the bank) */
        if (k < 0 || k >= EC_NKEY || !(P->kb_held >> k & 1u) || ec_key_upper(k) < 0 || ec_key_upper(k) == 10)
            return 0;
        snprintf(b, sizeof b, "SLICE %d", P->kb_bank * 10 + ec_key_upper(k) + 1);
        return b;
    }
    b[0] = 0;                                      /* 0.6.67 (owner): the notes held, in the order pressed, no label */
    for (;;) {
        int best = -1, s, len;
        for (j = 0; j < EC_NKEY && j < 32; j++)
            if ((P->kb_held >> j & 1u) && !used[j] && (best < 0 || P->kb_stamp[j] < P->kb_stamp[best]))
                best = j;
        if (best < 0)
            break;
        used[best] = 1;
        s = P->kb_semis[best];
        len = (int)strlen(b);
        if (len + 6 >= (int)sizeof b)
            break;
        snprintf(b + len, sizeof b - (size_t)len, "%s%s%d", n++ ? " " : "", N[(s % 12 + 12) % 12],
                 4 + (s >= 0 ? s / 12 : -((11 - s) / 12)));
    }
    return n ? b : 0;
}
