/* SPDX-License-Identifier: GPL-3.0-only */
/* 0.6.0 (docs/UX-V2.md §3 KEYBOARD, HOME third): the keybed is a full keyboard for the selected track (C4 = the
 * sample's own pitch; F1 / F2 the octave). On slice tracks (SLICE / RAZOR) the black keys pick the slice, ten at a
 * time (B11 steps the bank), and the white keys play the selected slice at a pitch. ALGO is the target step: below 1
 * is live (no entry); with a target, a key writes a trig with that note (and slice) there, one note per step, then
 * advances (or stays: KEYBOARD's ADVANCE setting in HOME's settings; SHIFT + the key does the other). */
#include <math.h>
#include <stdio.h>

#include "ec_app.h"

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

int ec_kb_semis(int k) { return k - 7 + 12 * P->oct; }   /* key 7 = C4 */

void ec_kb_oct(int d)
{
    char b[12];
    P->oct = clampi(P->oct + d, -3, 3);
    snprintf(b, sizeof b, "OCT %+d", P->oct);
    ec_app_message(b);
}

void ec_kb_target(int det)
{
    const int len = ec_seq_pattern_steps(&A->seq, A->track);
    char b[12];
    P->target = clampi(P->target + (det > 0 ? 1 : -1), 0, len);
    if (P->target)
        A->step_page = (P->target - 1) / 16;
    if (P->target)
        snprintf(b, sizeof b, "%d", P->target);
    else
        snprintf(b, sizeof b, "LIVE");
    ec_ux_takeover("STEP", b, len ? (float)P->target / (float)len : 0);
    A->redraw = 1;
}

/* one note on the target step: its trig, pitch (and slice, velocity) */
static void enter_step(int semis, int slice, int vel)
{
    ec_ptrack_t *tr = ec_project_track(&A->proj, A->track);
    const int len = ec_seq_pattern_steps(&A->seq, A->track);
    const int idx = clampi(P->target, 1, len) - 1;
    ec_step_t *s = &tr->steps[idx];
    int adv = ec_as->advance != 0;
    ec_undo_steps(&E->undo, &A->proj, A->track, idx / 16, (uint16_t)(1u << (idx % 16)), "NOTE", 0xD00u + (uint32_t)idx,
                  A->ms);
    E->undo.last_key = 0;
    s->trig = 1;
    if (ec_step_lock_set(s, EC_P_PITCH, (float)clampi(semis, -24, 24)) < 0)
        ec_app_message("LOCKS FULL");
    if (slice > 0)
        s->slices = 1u << (slice - 1);             /* one note per step: the slice replaces the step's */
    if (vel != 100)
        ec_step_lock_set(s, EC_P_VELOCITY, (float)vel / 127.0f);
    ec_seq_touch(&A->seq, A->track);
    ec_app_mark_dirty();
    if (A->fn)                                     /* SHIFT + the key: the other */
        adv = !adv;
    if (adv) {
        P->target = P->target % len + 1;
        A->step_page = (P->target - 1) / 16;
    }
}

void ec_kb_key(int k, int down, int vel)
{
    const int up = ec_key_upper(k);
    const int slice = ec_kb_slice_track();
    float *v = ec_app_params(A);
    if (A->bus)
        return;
    if (!down) {
        ec_seq_keyboard(&A->seq, A->track, (float)P->kb_semis[k], 0, 0);
        return;
    }
    if (slice && up >= 0) {                        /* slice tracks: the black keys pick the slice */
        const int cnt = clampi(iv(v, EC_P_SLICE_COUNT), 1, 32);
        if (up == 10) {                            /* B11: the bank */
            const int banks = (cnt + 9) / 10;
            char b[16];
            P->kb_bank = (P->kb_bank + 1) % (banks > 0 ? banks : 1);
            snprintf(b, sizeof b, "SLICES %d-%d", P->kb_bank * 10 + 1, P->kb_bank * 10 + 10);
            ec_app_message(b);
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
            ec_seq_keyboard(&A->seq, A->track, 0, 1, (float)vel / 127.0f);   /* heard */
            if (P->target)
                enter_step(0, s, vel);
            A->redraw = 1;
            return;
        }
    }
    {
        const int semis = ec_kb_semis(k);
        P->kb_semis[k] = (int8_t)clampi(semis, -60, 60);
        ec_seq_keyboard(&A->seq, A->track, (float)semis, 1, (float)vel / 127.0f);
        if (P->target)
            enter_step(semis, slice ? clampi(iv(v, EC_P_SLICE_INDEX), 1, 32) : 0, vel);
    }
    A->redraw = 1;
}
