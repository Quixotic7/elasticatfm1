/* SPDX-License-Identifier: GPL-3.0-only */
/* The project model and its record (ec_project.h) */
#include <string.h>

#include "ec_project.h"

void ec_slot_clear(ec_slot_t *s)
{
    memset(s, 0, sizeof *s);
    s->bpm = 120.0f;
    s->steps = 16.0f;
    s->trim_start = 0.0f;
    s->trim_end = 1.0f;
    s->gain = 1.0f;
}

void ec_project_init(ec_project_t *p)
{
    int i, t;
    memset(p, 0, sizeof *p);
    p->bpm = 120.0f;
    p->swing = 50.0f;
    p->glen = 16;
    p->quant = EC_Q_SEQUENTIAL;
    p->rtn = 2;                            /* boomerang (the original's playhead_return default) */
    p->current = 0;
    for (i = 0; i < EC_NSLOT; i++)
        ec_slot_clear(&p->slot[i]);
    p->pat[0].used = 1;
    for (t = 0; t < EC_NTRACK; t++) {
        ec_params_default(p->pat[0].tr[t].v);
        p->pat[0].tr[t].v[EC_P_SLOT] = (float)(t + 1);   /* (each track starts on its own pool slot) */
    }
    for (i = 0; i < 3; i++)                /* E4 */
        ec_bus_default(p->bus[i]);
}

void ec_bus_default(float *v)
{
    ec_params_default(v);
    v[EC_P_LIMIT_CEIL] = 121.0f;           /* (the generated send / master limit_ceil default) */
}

/* ------------------------------------------------------------------------------------------- steps --- */
int ec_step_has_content(const ec_step_t *s) { return s->trig || s->nlock || s->slices; }

int ec_step_lock_get(const ec_step_t *s, int id, float *val)
{
    int i;
    for (i = 0; i < s->nlock; i++)
        if (s->lock_id[i] == id) {
            if (val)
                *val = s->lock_val[i];
            return 1;
        }
    return 0;
}

int ec_step_lock_set(ec_step_t *s, int id, float val)
{
    int i;
    for (i = 0; i < s->nlock; i++)
        if (s->lock_id[i] == id) {
            s->lock_val[i] = val;
            return 0;
        }
    if (s->nlock >= EC_STEP_LOCKS)
        return -1;
    s->lock_id[s->nlock] = (uint8_t)id;
    s->lock_val[s->nlock] = val;
    s->nlock++;
    return 0;
}

int ec_step_lock_clear(ec_step_t *s, int id)
{
    int i;
    for (i = 0; i < s->nlock; i++)
        if (s->lock_id[i] == id) {
            s->nlock--;
            s->lock_id[i] = s->lock_id[s->nlock];
            s->lock_val[i] = s->lock_val[s->nlock];
            return 1;
        }
    return 0;
}

void ec_step_clear(ec_step_t *s) { memset(s, 0, sizeof *s); }

int ec_step_reset_active(const ec_step_t *s, const float *v, int reset_id)
{
    float x;
    if (s && ec_step_lock_get(s, reset_id, &x))
        return x > 0.5f;
    return v[reset_id] > 0.5f;
}

int ec_step_is_ghost(const ec_step_t *s, const float *v)
{
    if (!s || !s->trig)
        return 0;
    if (ec_step_lock_get(s, EC_P_LOOP_START, 0))
        return 0;
    return !(ec_step_reset_active(s, v, EC_P_ENV_RESET) || ec_step_reset_active(s, v, EC_P_LFO_RESET) ||
             ec_step_reset_active(s, v, EC_P_FILTER_RESET));
}

int ec_step_locks_region(const ec_step_t *s)
{
    return ec_step_lock_get(s, EC_P_LOOP_START, 0) || ec_step_lock_get(s, EC_P_LOOP_END, 0);
}

static int is_slice_machine(int m) { return m >= EC_MACH_GRID_SLICE && m <= EC_MACH_RAZOR_POLY; }

int ec_step_toggle(ec_ptrack_t *tr, int index)
{
    ec_step_t *s;
    if (index < 0 || index >= EC_MAXSTEPS)
        return 0;
    s = &tr->steps[index];
    if (ec_step_has_content(s)) {
        ec_step_clear(s);
        return 0;
    }
    s->trig = 1;
    if (is_slice_machine((int)(tr->v[EC_P_MACHINE] + 0.5f))) {
        int si = (int)(tr->v[EC_P_SLICE_INDEX] + 0.5f);
        const int cnt = (int)(tr->v[EC_P_SLICE_COUNT] + 0.5f);
        if (si > cnt)
            si = cnt;
        if (si < 1)
            si = 1;
        s->slices = 1u << (si - 1);
    }
    return 1;
}

void ec_step_toggle_ghost(ec_ptrack_t *tr, int index)
{
    ec_step_t *s;
    if (index < 0 || index >= EC_MAXSTEPS)
        return;
    s = &tr->steps[index];
    s->trig = 1;
    if (ec_step_is_ghost(s, tr->v)) {
        ec_step_lock_clear(s, EC_P_ENV_RESET);
        ec_step_lock_clear(s, EC_P_LFO_RESET);
        ec_step_lock_clear(s, EC_P_FILTER_RESET);
    } else {
        ec_step_lock_clear(s, EC_P_LOOP_START);
        ec_step_lock_clear(s, EC_P_LOOP_END);
        ec_step_lock_set(s, EC_P_ENV_RESET, 0);
        ec_step_lock_set(s, EC_P_LFO_RESET, 0);
        ec_step_lock_set(s, EC_P_FILTER_RESET, 0);
    }
}

void ec_step_toggle_slice(ec_ptrack_t *tr, int index, int slice)
{
    ec_step_t *s;
    uint32_t bit;
    int m, was;
    if (index < 0 || index >= EC_MAXSTEPS || slice < 1 || slice > 32)
        return;
    s = &tr->steps[index];
    bit = 1u << (slice - 1);
    was = (s->slices & bit) != 0;
    m = (int)(tr->v[EC_P_MACHINE] + 0.5f);
    s->trig = 1;
    if (m == EC_MACH_GRID_SLICE || m == EC_MACH_RAZOR_SLICE)
        s->slices = 0;
    if (was)
        s->slices &= ~bit;
    else
        s->slices |= bit;
    if (!s->slices)
        s->trig = 0;
}

int ec_track_has_trigs(const ec_ptrack_t *tr)
{
    int i;
    for (i = 0; i < EC_MAXSTEPS; i++)
        if (tr->steps[i].trig)
            return 1;
    return 0;
}

/* ------------------------------------------------------------------------------------------- patterns --- */
void ec_pattern_clear_steps(ec_pattern_t *pt)
{
    int t;
    for (t = 0; t < EC_NTRACK; t++)
        memset(pt->tr[t].steps, 0, sizeof pt->tr[t].steps);
}

void ec_pattern_prepare(ec_project_t *p, int idx)
{
    int t;
    if (idx < 0 || idx >= EC_NPAT || p->pat[idx].used)
        return;
    for (t = 0; t < EC_NTRACK; t++)
        memcpy(p->pat[idx].tr[t].v, p->pat[p->current].tr[t].v, sizeof p->pat[idx].tr[t].v);
    ec_pattern_clear_steps(&p->pat[idx]);
    p->pat[idx].used = 1;
}

/* ------------------------------------------------------------------------------------------- the record --- */
/* little-endian writer / reader with bounds */
typedef struct {
    uint8_t *b;
    const uint8_t *r;
    uint32_t n, cap;
    int bad;
} io_t;

static void w8(io_t *o, uint32_t v)
{
    if (o->n + 1 > o->cap) {
        o->bad = 1;
        return;
    }
    o->b[o->n++] = (uint8_t)v;
}
static void w16(io_t *o, uint32_t v) { w8(o, v & 0xFF), w8(o, (v >> 8) & 0xFF); }
static void w32(io_t *o, uint32_t v) { w16(o, v & 0xFFFF), w16(o, v >> 16); }
static void wf(io_t *o, float f)
{
    uint32_t u;
    memcpy(&u, &f, 4);
    w32(o, u);
}
static uint32_t r8(io_t *o)
{
    if (o->n + 1 > o->cap) {
        o->bad = 1;
        return 0;
    }
    return o->r[o->n++];
}
static uint32_t r16(io_t *o)
{
    const uint32_t a = r8(o);
    return a | r8(o) << 8;
}
static uint32_t r32(io_t *o)
{
    const uint32_t a = r16(o);
    return a | r16(o) << 16;
}
static float rf(io_t *o)
{
    const uint32_t u = r32(o);
    float f;
    memcpy(&f, &u, 4);
    return f != f ? 0.0f : f;
}

static uint32_t fnv1a(const uint8_t *b, uint32_t n)
{
    uint32_t h = 2166136261u, i;
    for (i = 0; i < n; i++)
        h = (h ^ b[i]) * 16777619u;
    return h;
}

#define HDR 16u   /* magic, version, flags, payload length, checksum */

int32_t ec_project_save(const ec_project_t *p, uint8_t *buf, uint32_t cap)
{
    io_t o;
    int i, t, k, s;
    if (cap < HDR)
        return -1;
    memset(&o, 0, sizeof o);
    o.b = buf;
    o.cap = cap;
    o.n = HDR;
    wf(&o, p->bpm);
    wf(&o, p->swing);
    w16(&o, (uint32_t)p->glen);
    w8(&o, (uint32_t)p->quant);
    w8(&o, (uint32_t)p->rtn);
    w8(&o, (uint32_t)p->current);
    w8(&o, EC_NSLOT);
    for (i = 0; i < EC_NSLOT; i++) {
        const ec_slot_t *sl = &p->slot[i];
        uint32_t len = 0;
        while (len < EC_PATH - 1 && sl->path[len])
            len++;
        w8(&o, len);
        for (k = 0; k < (int)len; k++)
            w8(&o, (uint8_t)sl->path[k]);
        wf(&o, sl->bpm);
        wf(&o, sl->steps);
        wf(&o, sl->trim_start);
        wf(&o, sl->trim_end);
        wf(&o, sl->gain);
    }
    w8(&o, EC_NPAT);
    w8(&o, EC_NTRACK);
    for (i = 0; i < EC_NPAT; i++) {
        const ec_pattern_t *pt = &p->pat[i];
        w8(&o, pt->used);
        if (!pt->used)
            continue;
        for (t = 0; t < EC_NTRACK; t++) {
            const ec_ptrack_t *tr = &pt->tr[t];
            int nsteps = 0;
            w8(&o, EC_NP);
            for (k = 0; k < EC_NP; k++) {
                w8(&o, (uint32_t)k);
                wf(&o, tr->v[k]);
            }
            for (s = 0; s < EC_MAXSTEPS; s++)
                nsteps += ec_step_has_content(&tr->steps[s]);
            w16(&o, (uint32_t)nsteps);
            for (s = 0; s < EC_MAXSTEPS; s++) {
                const ec_step_t *st = &tr->steps[s];
                if (!ec_step_has_content(st))
                    continue;
                w16(&o, (uint32_t)s);
                w8(&o, st->trig);
                w32(&o, st->slices);
                w8(&o, st->nlock);
                for (k = 0; k < st->nlock; k++) {
                    w8(&o, st->lock_id[k]);
                    wf(&o, st->lock_val[k]);
                }
            }
        }
    }
    {   /* E4: the bus FX chunk: tag, length, then per bus: count, (id, value) pairs */
        uint32_t at, b;
        w32(&o, 0x42344345u);                /* "EC4B" */
        at = o.n;
        w32(&o, 0);
        w8(&o, 3);
        for (b = 0; b < 3; b++) {
            int cnt = 0;
            for (k = 0; k < EC_NP; k++)
                cnt += (EC_PARAMS[k].flags & EC_PF_FX) != 0;
            w8(&o, (uint32_t)cnt);
            for (k = 0; k < EC_NP; k++)
                if (EC_PARAMS[k].flags & EC_PF_FX) {
                    w8(&o, (uint32_t)k);
                    wf(&o, p->bus[b][k]);
                }
        }
        if (!o.bad) {
            const uint32_t len = o.n - at - 4, end = o.n;
            o.n = at;
            w32(&o, len);
            o.n = end;
        }
    }
    if (o.bad)
        return -1;
    {
        const uint32_t len = o.n - HDR, n = o.n;
        o.n = 0;
        w32(&o, EC_PROJECT_MAGIC);
        w16(&o, EC_PROJECT_VERSION);
        w16(&o, 0);
        w32(&o, len);
        w32(&o, fnv1a(buf + HDR, len));
        return (int32_t)n;
    }
}

static float clampf(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }

int ec_project_load(ec_project_t *p, const uint8_t *buf, uint32_t n)
{
    io_t o;
    uint32_t len, sum, nslot, npat, ntr;
    uint32_t i, t, k, s;
    if (!buf || n < HDR)
        return -1;
    memset(&o, 0, sizeof o);
    o.r = buf;
    o.cap = n;
    if (r32(&o) != EC_PROJECT_MAGIC || r16(&o) != EC_PROJECT_VERSION)
        return -1;
    (void)r16(&o);
    len = r32(&o);
    sum = r32(&o);
    if (len > n - HDR || fnv1a(buf + HDR, len) != sum)
        return -1;
    o.cap = HDR + len;
    ec_project_init(p);
    p->pat[0].used = 0;
    p->bpm = clampf(rf(&o), 20, 300);
    p->swing = clampf(rf(&o), 50, 75);
    p->glen = (int)r16(&o);
    p->quant = (int)r8(&o);
    p->rtn = (int)r8(&o);
    p->current = (int)r8(&o);
    if (p->glen < 1 || p->glen > 256)
        p->glen = 16;
    if (p->quant < 1 || p->quant > 4)
        p->quant = EC_Q_SEQUENTIAL;
    if (p->rtn < 1 || p->rtn > 3)
        p->rtn = 2;
    if (p->current < 0 || p->current >= EC_NPAT)
        p->current = 0;
    nslot = r8(&o);
    for (i = 0; i < nslot && !o.bad; i++) {
        ec_slot_t tmp, *sl = i < EC_NSLOT ? &p->slot[i] : &tmp;
        const uint32_t pl = r8(&o);
        ec_slot_clear(sl);
        for (k = 0; k < pl; k++) {
            const char c = (char)r8(&o);
            if (k < EC_PATH - 1)
                sl->path[k] = c;
        }
        sl->bpm = clampf(rf(&o), 20, 300);
        sl->steps = clampf(rf(&o), 1, 512);
        sl->trim_start = clampf(rf(&o), 0, 1);
        sl->trim_end = clampf(rf(&o), 0, 1);
        sl->gain = clampf(rf(&o), 0, 4);
    }
    npat = r8(&o);
    ntr = r8(&o);
    for (i = 0; i < npat && !o.bad; i++) {
        const uint32_t used = r8(&o);
        if (!used)
            continue;
        if (i < EC_NPAT)
            p->pat[i].used = 1;
        for (t = 0; t < ntr && !o.bad; t++) {
            ec_ptrack_t *tr = i < EC_NPAT && t < EC_NTRACK ? &p->pat[i].tr[t] : 0;
            const uint32_t np = r8(&o);
            uint32_t nsteps;
            if (tr)
                ec_params_default(tr->v);
            for (k = 0; k < np; k++) {
                const uint32_t id = r8(&o);
                const float v = rf(&o);
                if (tr && id < EC_NP)
                    tr->v[id] = ec_param_clamp((int)id, v);
            }
            nsteps = r16(&o);
            for (k = 0; k < nsteps && !o.bad; k++) {
                const uint32_t idx = r16(&o), trig = r8(&o), slices = r32(&o), nl = r8(&o);
                ec_step_t tmp, *st = tr && idx < EC_MAXSTEPS ? &tr->steps[idx] : &tmp;
                memset(st, 0, sizeof *st);
                st->trig = (uint8_t)(trig ? 1 : 0);
                st->slices = slices;
                for (s = 0; s < nl; s++) {
                    const uint32_t id = r8(&o);
                    const float v = rf(&o);
                    if (id < EC_NP)
                        ec_step_lock_set(st, (int)id, ec_param_clamp((int)id, v));
                }
            }
        }
    }
    /* tagged chunks after the patterns (unknown ones skipped) */
    while (!o.bad && o.n + 8 <= o.cap) {
        const uint32_t tag = r32(&o), clen = r32(&o), end = o.n + clen;
        if (o.bad || clen > o.cap - o.n)
            break;
        if (tag == 0x42344345u) {           /* E4 "EC4B": the buses */
            const uint32_t nb = r8(&o);
            for (i = 0; i < nb && !o.bad; i++) {
                const uint32_t cnt = r8(&o);
                for (k = 0; k < cnt && !o.bad; k++) {
                    const uint32_t id = r8(&o);
                    const float v = rf(&o);
                    if (i < 3 && id < EC_NP && (EC_PARAMS[id].flags & EC_PF_FX))
                        p->bus[i][id] = ec_param_clamp((int)id, v);
                }
            }
        }
        o.n = end;
    }
    if (o.bad)
        return -1;
    if (!p->pat[p->current].used) {
        p->pat[p->current].used = 1;
        for (t = 0; t < EC_NTRACK; t++) {
            ec_params_default(p->pat[p->current].tr[t].v);
            p->pat[p->current].tr[t].v[EC_P_SLOT] = (float)(t + 1);
        }
    }
    return 0;
}
