/* SPDX-License-Identifier: GPL-3.0-only */
/* 0.6.0 (docs/UX-V2.md §3 CROSSFADER, ARP): B1 = scene A, B2 = scene B: hold one + turn a knob = lock that param to
 * the scene (the selected track; ec_e5_knob_cell's anchor path), B11 held + A / B = capture every param of every
 * track into it, SHIFT + A / B = clear it. W1..W12 = the fader (W1 = 0, W12 = 127): hold a key and it glides there,
 * faster with more pressure; let go and it stays. SHIFT + W1..W12 = jump. W13..W16 = the base glide time (instant /
 * fast / medium / slow, the time with no pressure; an APP SETTINGS value, global). Never sequenced or recorded. */
#include <math.h>
#include <stdio.h>

#include "ec_app.h"
#include "ec_scene.h"

#define A (&ec_app)
#define P (&ec_app.pn)
#define E (&ec_app.e5)

#define XF_TICK_MS 10

static void toast(const char *s)
{
    snprintf(E->toast, sizeof E->toast, "%s", s);
    E->toast_ms = A->ms ? A->ms : 1;
    E->toast_keep = 0;
    A->redraw = 1;
}

float ec_xfd_key_value(int i) { return (float)(i < 0 ? 0 : i > 11 ? 11 : i) / 11.0f; }

int ec_xfd_glide_ms(void)
{
    static const int G[4] = {0, 150, 600, 2000};
    return G[ec_as->glide & 3];
}

static void jump(float pos)
{
    E->gliding = 0;
    ec_e5_xf_apply(pos);
    ec_app_mark_dirty();
}

void ec_xfd_white(int i, int down, int lean)
{
    if (i >= 12) {                                 /* W13..W16: the base glide time */
        static const char *const N[4] = {"GLIDE INSTANT", "GLIDE FAST", "GLIDE MED", "GLIDE SLOW"};
        if (down) {
            P->as.glide = (uint8_t)(i - 12);
            ec_as_store();
            ec_app_message(N[i - 12]);
        }
        return;
    }
    if (!down) {
        if (P->xf_key == i) {                      /* let go: it stays where it got to */
            P->xf_key = -1;
            E->gliding = 0;
            E->glide_held = 0;
        }
        return;
    }
    if (A->fn) {                                   /* SHIFT: jump */
        jump(ec_xfd_key_value(i));
        return;
    }
    P->xf_key = i;
    P->xf_lean = (uint8_t)lean;
    E->glide_target = ec_xfd_key_value(i);
    E->glide_held = 1;
    E->glide_ms = A->ms;
    E->gliding = 1;
    if (ec_xfd_glide_ms() == 0)
        jump(E->glide_target);
}

void ec_xfd_black(int key, int down)
{
    if (key == 11) {
        P->xf_b11 = down;
        return;
    }
    if (key != 1 && key != 2)
        return;
    if (!down) {
        if (E->anchor == key)
            E->anchor = 0;
        return;
    }
    if (A->fn) {                                   /* SHIFT + A / B: clear that scene */
        ec_scene_clear(&A->proj, key - 1);
        ec_e5_xf_apply(A->proj.e5.sc.pos);
        toast(key == 1 ? "SCENE A CLEARED" : "SCENE B CLEARED");
        ec_app_mark_dirty();
        return;
    }
    if (P->xf_b11) {                               /* B11 held + A / B: capture */
        ec_scene_capture(&A->proj, key - 1);
        toast(key == 1 ? "SCENE A CAPTURED" : "SCENE B CAPTURED");
        ec_app_mark_dirty();
        return;
    }
    E->anchor = key;                               /* held: the knobs lock params to it */
    A->redraw = 1;
}

/* the glide: full travel in the base time, faster with pressure (100 %: 0.15 of it) */
void ec_xfd_tick(const uint8_t *lean)
{
    float pos, step, eff, d;
    uint32_t dt;
    if (!E->gliding || P->xf_key < 0) {
        E->gliding = E->gliding && P->xf_key >= 0;
        return;
    }
    dt = A->ms - E->glide_ms;
    if (dt < XF_TICK_MS)
        return;
    E->glide_ms = A->ms;
    if (lean) {
        const int k = ec_key_of_lower(P->xf_key);
        if (k >= 0 && k < EC_NKEY)
            P->xf_lean = lean[k];
    }
    eff = (float)ec_xfd_glide_ms() * (1.0f - 0.85f * (float)(P->xf_lean > 100 ? 100 : P->xf_lean) / 100.0f);
    pos = A->proj.e5.sc.pos;
    d = E->glide_target - pos;
    step = eff > 0 ? (float)dt / eff : 1.0f;
    if (fabsf(d) <= step)
        pos = E->glide_target;
    else
        pos += d > 0 ? step : -step;
    if (pos != A->proj.e5.sc.pos) {
        ec_e5_xf_apply(pos);
        ec_app_mark_dirty();
    }
}
