/* SPDX-License-Identifier: GPL-3.0-only */
/* 0.6.0 (docs/UX-V2.md §3 SAMPLER, FILE second, not gated): B11 arms; the take starts on the next bar (at once while
 * stopped); B11 again stops it at the end of the bar (counted from the take's start: a stopped take is a whole number
 * of bars too); B1..B5 the length FREE / 1 / 2 / 4 / 8 bars (it ends by itself). The take goes into the next free pool
 * slot, tagged with the BPM, and is written to /data/rec (ec_rec.c's take and WAV writer, 0.4's). The source is the
 * input (ain1; the recorder's setup, FILE held, can pick the master out), with a gain; MONITOR plays the input on the
 * main output after the master bus. The white keys are the input's VU. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ec_app.h"

#define A (&ec_app)
#define P (&ec_app.pn)
#define R (&ec_app.rec)

static int clampi(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }

static const int LEN_KEY[5] = {0, 1, 2, 3, 4};    /* B1..B5 -> ec_rec's LEN index: FREE, 1, 2, 4, 8 bars */

int ec_sampler_len(void)
{
    static const int B[6] = {0, 1, 2, 4, 8, 16};
    return B[clampi(R->len, 0, 5)];
}

void ec_sampler_enter(int on)
{
    if (!on && R->open)
        ec_rec_close();                            /* (a take goes on) */
}

float ec_sampler_vu(void)
{
    const float pk = R->in_meter[0] > R->in_meter[1] ? R->in_meter[0] : R->in_meter[1];
    const float db = pk > 1e-6f ? 20.0f * log10f(pk) : -120.0f;
    const float f = (db + 48.0f) / 48.0f;          /* -48 dB .. 0 dB */
    return f < 0 ? 0 : f > 1 ? 1 : f;
}

void ec_sampler_input(int16_t *in, uint32_t n)
{
    float g;
    uint32_t i;
    if (P->smp_gain_db == 0.0f)
        return;
    g = powf(10.0f, P->smp_gain_db / 20.0f);
    for (i = 0; i < 2 * n; i++) {
        const float x = (float)in[i] * g;
        in[i] = (int16_t)(x > 32767.0f ? 32767 : x < -32768.0f ? -32768 : (int)x);
    }
}

static uint32_t bar_frames(void)
{
    const double bpm = A->seq.bpm > 1 ? A->seq.bpm : A->proj.bpm;
    return (uint32_t)floor(4.0 * 60.0 * EC_FS / bpm + 0.5);
}

static void arm(void)
{
    const int s = ec_file_next_free();
    if (s < 0) {
        ec_app_alert("FULL");
        return;
    }
    if (ec_rec_arm_to(s, A->seq.playing ? EC_RSYNC_BAR : EC_RSYNC_FREE) == 0) {
        char m[24];
        snprintf(m, sizeof m, A->seq.playing ? "ARMED: NEXT BAR" : "REC > SLOT %d", s + 1);
        ec_app_message(m);
    }
}

/* B11 while taking: the take ends at the end of its bar */
static void stop_bar(void)
{
    const uint32_t bar = bar_frames();
    uint32_t t;
    if (!bar)
        return;
    t = (R->n + bar - 1) / bar * bar;
    if (!t)
        t = bar;
    if (t > R->cap)
        t = R->cap;
    if (t <= R->n) {
        if (R->n)
            R->state = EC_RS_STOPPED;
        else
            ec_rec_drop();
    } else
        R->target = t;
    ec_app_message("STOPS AT THE BAR");
}

void ec_sampler_black(int key, int down)
{
    if (!down)
        return;
    if (key >= 1 && key <= 5) {                    /* the length (not while a take is armed or running) */
        if (R->state == EC_RS_IDLE) {
            char m[16];
            R->len = LEN_KEY[key - 1];
            if (ec_sampler_len())
                snprintf(m, sizeof m, "%d BAR%s", ec_sampler_len(), ec_sampler_len() > 1 ? "S" : "");
            else
                snprintf(m, sizeof m, "FREE");
            ec_app_message(m);
        }
        return;
    }
    if (key != 11)
        return;
    switch (R->state) {
    case EC_RS_IDLE: arm(); break;
    case EC_RS_ARMED:
        ec_rec_drop();
        ec_app_message("DISARMED");
        break;
    case EC_RS_RUN: stop_bar(); break;
    default: break;
    }
    A->redraw = 1;
}
