/* SPDX-License-Identifier: GPL-3.0-only */
/* 0.3.0 (ec_ux.h): the lanes, the knob speed, the takeover, the lock chips' lookup. (0.6.0: the live panel labels
 * moved to ec_panel_leds.c.) */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ec_app.h"

#define A (&ec_app)
#define UX (&ec_app.ux)

static int clampi(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }

int ec_ux_lanes(int n) { return n <= 4 ? 1 : (n + 3) / 4; }

/* Slowly a click is the finest step; spun, the steps grow up to ~1/16 of the range a click (FIZZ; ChoralRoot's 5 %
 * is near). The speed is the gap since the knob's last turn (the detents of one tick share its multiplier). */
int ec_ux_accel(int role, int steps, int det)
{
    uint32_t gap = A->ms - UX->knob_ms[clampi(role, 0, 7)];
    int maxm;
    float t;
    UX->knob_ms[clampi(role, 0, 7)] = A->ms;
    if (steps <= 24)                                 /* lists and small ranges: one step a click */
        return 1;
    (void)det;
    maxm = steps / 16 > 1 ? steps / 16 : 1;
    if (ec_as->knob == 0)                            /* 0.6.0, APP SETTINGS' KNOB SPEED: slow / medium / fast */
        maxm = maxm / 3 > 1 ? maxm / 3 : 1;
    else if (ec_as->knob == 2)
        maxm *= 2;
    if (gap >= 90)
        return 1;
    t = (float)(90 - (int)gap) / 80.0f;              /* 90 ms: 0 .. 10 ms: 1 */
    t = t > 1 ? 1 : t;
    return 1 + (int)(t * t * (float)(maxm - 1) + 0.5f);
}

void ec_ux_takeover(const char *label, const char *val, float frac)
{
    snprintf(UX->to_label, sizeof UX->to_label, "%s", label);
    snprintf(UX->to_val, sizeof UX->to_val, "%s", val);
    UX->to_sub[0] = 0;
    UX->to_frac = frac < 0 ? 0 : frac > 1 ? 1 : frac;
    UX->to_kind = EC_TO_VALUE;
    UX->to_ms = A->ms ? A->ms : 1;
    A->redraw = 1;
}

int ec_ux_locked_somewhere(int id)
{
    const ec_ptrack_t *tr = &A->proj.pat[A->proj.current].tr[A->track];
    const int len = ec_seq_pattern_steps(&A->seq, A->track);
    int i, k;
    for (i = 0; i < len && i < EC_MAXSTEPS; i++)
        for (k = 0; k < tr->steps[i].nlock; k++)
            if (tr->steps[i].lock_id[k] == id)
                return 1;
    return 0;
}
