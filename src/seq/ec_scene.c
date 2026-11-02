/* SPDX-License-Identifier: GPL-3.0-only */
/* The A/B scenes and the crossfader layer (ec_scene.h) */
#include <math.h>
#include <string.h>

#include "ec_scene.h"

int ec_scene_morphable(int id)
{
    const ec_param_def_t *d;
    if (id < 0 || id >= EC_NP)
        return 0;
    d = &EC_PARAMS[id];
    if (d->kind != EC_PK_NUM || (d->flags & (EC_PF_SEQ | EC_PF_MOMENT)))
        return 0;
    switch (id) {                  /* MORPH_EXCLUDE + the track volume (a mixer setting, PATTERN_GLOBAL_SUFFIXES) */
    case EC_P_PATTERN_STEPS:
    case EC_P_SLOT:
    case EC_P_SLICE_COUNT:
    case EC_P_SLICE_INDEX:
    case EC_P_CHOP_STEPS:
    case EC_P_LOOP_DIVISION:
    case EC_P_RATE:
    case EC_P_AMP: return 0;
    default: return 1;
    }
}

void ec_scene_reset(ec_scenes_t *sc) { memset(sc, 0, sizeof *sc); }

int ec_scene_count(const ec_scenes_t *sc, int scene)
{
    int t, i, n = 0;
    for (t = 0; t < EC_SCN_ROWS; t++)
        for (i = 0; i < EC_NP; i++)
            n += sc->has[scene & 1][t][i] != 0;
    return n;
}

/* a row's live values: a track's (the current pattern) or a bus record's (rows EC_NTRACK.. = S1, S2, MST) */
static const float *tv(const ec_project_t *p, int t)
{
    return t < EC_NTRACK ? p->pat[p->current].tr[t].v : p->bus[t - EC_NTRACK];
}

/* a row's default (a track's: params_spec; a bus record's: ec_bus_default) -- the sparse capture leaves it out */
static float row_def(int t, int id)
{
    static float bus[EC_NP];
    static int ready;
    if (t < EC_NTRACK)
        return EC_PARAMS[id].def;
    if (!ready) {
        ec_bus_default(bus);
        ready = 1;
    }
    return bus[id];
}

int ec_scene_row_morphable(int row, int id)
{
    if (row < 0 || row >= EC_SCN_ROWS || !ec_scene_morphable(id))
        return 0;
    return row < EC_NTRACK || (EC_PARAMS[id].flags & EC_PF_FX);   /* a bus holds only its FX params */
}

void ec_scene_capture(ec_project_t *p, int scene)
{
    ec_scenes_t *sc = &p->e5.sc;
    const int s = scene & 1, o = s ^ 1;
    int t, i;
    for (t = 0; t < EC_SCN_ROWS; t++)
        for (i = 0; i < EC_NP; i++) {
            float x;
            if (!ec_scene_row_morphable(t, i))
                continue;
            x = sc->on[t][i] ? sc->xf[t][i] : tv(p, t)[i];
            if (x != row_def(t, i) || sc->has[o][t][i]) {
                sc->has[s][t][i] = 1;
                sc->val[s][t][i] = x;
            } else
                sc->has[s][t][i] = 0;
        }
}

void ec_scene_set(ec_project_t *p, int scene, int t, int id, float value)
{
    if (!ec_scene_row_morphable(t, id))
        return;
    p->e5.sc.has[scene & 1][t][id] = 1;
    p->e5.sc.val[scene & 1][t][id] = ec_param_clamp(id, value);
}

int ec_scene_get(const ec_project_t *p, int scene, int t, int id, float *value)
{
    if (t < 0 || t >= EC_SCN_ROWS || id < 0 || id >= EC_NP || !p->e5.sc.has[scene & 1][t][id])
        return 0;
    if (value)
        *value = p->e5.sc.val[scene & 1][t][id];
    return 1;
}

void ec_scene_clear(ec_project_t *p, int scene)
{
    memset(p->e5.sc.has[scene & 1], 0, sizeof p->e5.sc.has[0]);
}

void ec_scene_clear_key(ec_project_t *p, int t, int id)
{
    if (t < 0 || t >= EC_SCN_ROWS || id < 0 || id >= EC_NP)
        return;
    p->e5.sc.has[0][t][id] = p->e5.sc.has[1][t][id] = 0;
}

uint32_t ec_scene_apply(ec_project_t *p, float pos)
{
    ec_scenes_t *sc = &p->e5.sc;
    uint32_t changed = 0;
    int t, i;
    pos = pos < 0 ? 0 : pos > 1 ? 1 : pos;
    sc->pos = pos;
    for (t = 0; t < EC_SCN_ROWS; t++) {
        const float *v = tv(p, t);
        for (i = 0; i < EC_NP; i++) {
            const int ha = sc->has[0][t][i], hb = sc->has[1][t][i];
            float a, b, x;
            if (!ha && !hb) {
                if (sc->on[t][i]) {             /* left the morph set: the knob (source 1) regains control */
                    sc->on[t][i] = 0;
                    changed |= 1u << t;
                }
                continue;
            }
            a = ha ? sc->val[0][t][i] : v[i];
            b = hb ? sc->val[1][t][i] : v[i];
            if (a == b) {
                if (sc->on[t][i]) {
                    sc->on[t][i] = 0;
                    changed |= 1u << t;
                }
                continue;
            }
            x = a + (b - a) * pos;
            if (!sc->on[t][i] || sc->xf[t][i] != x) {
                sc->on[t][i] = 1;
                sc->xf[t][i] = x;
                changed |= 1u << t;
            }
        }
    }
    return changed;
}

void ec_scene_knob(ec_project_t *p, int t, int id)
{
    if (t >= 0 && t < EC_SCN_ROWS && id >= 0 && id < EC_NP)
        p->e5.sc.on[t][id] = 0;
}

int ec_scene_targets(const ec_project_t *p)
{
    const ec_scenes_t *sc = &p->e5.sc;
    int t, i, n = 0;
    for (t = 0; t < EC_SCN_ROWS; t++) {
        const float *v = tv(p, t);
        for (i = 0; i < EC_NP; i++) {
            const int ha = sc->has[0][t][i], hb = sc->has[1][t][i];
            if ((ha || hb) && (ha ? sc->val[0][t][i] : v[i]) != (hb ? sc->val[1][t][i] : v[i]))
                n++;
        }
    }
    return n;
}

float ec_scene_glide_step(float cur, float target, int slow)
{
    float next;
    if (slow) {
        const float step = 1.0f / (5.0f * EC_GLIDE_HZ);
        next = target >= cur ? (cur + step > target ? target : cur + step) : (cur - step < target ? target : cur - step);
    } else {
        const float factor = 5.0f / EC_GLIDE_HZ;
        next = cur + (target - cur) * factor;
        if (fabsf(target - next) < 0.004f)
            next = target;
    }
    return next;
}
