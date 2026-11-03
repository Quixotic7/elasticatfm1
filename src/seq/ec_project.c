/* SPDX-License-Identifier: GPL-3.0-only */
/* The project model and its record (ec_project.h) */
#include <math.h>
#include <string.h>

#include "ec_e5.h"
#include "ec_mtx.h"
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
        ec_razor_init(&p->razor[t]);                     /* E3 */
    }
    for (i = 0; i < 3; i++)                /* E4 */
        ec_bus_default(p->bus[i]);
    ec_e5_project_init(p);                 /* E5 */
    ec_recset_default(&p->rec);            /* 0.6.0 */
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
    ec_e5_pattern_prepared(p, idx);        /* E5 */
}

/* ------------------------------------------------------------------------------------------- the record --- */
/* little-endian writer / reader with bounds; a writer with b == NULL only measures */
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
    if (o->b)
        o->b[o->n] = (uint8_t)v;
    o->n++;
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

/* version 2's values: an integer -64..190 is one byte (value + 64), anything else 0xFF + f32. Most params and locks
 * are whole numbers (0..127 amounts, options, semitones), so a lock is 2 bytes instead of 5 */
#define V_INT_MIN (-64)
#define V_INT_MAX 190
#define V_F32 0xFFu
static void wv(io_t *o, int ver, float f)
{
    if (ver >= 2 && f == floorf(f) && f >= V_INT_MIN && f <= V_INT_MAX)
        w8(o, (uint32_t)((int)f - V_INT_MIN));
    else {
        if (ver >= 2)
            w8(o, V_F32);
        wf(o, f);
    }
}
static float rv(io_t *o, int ver)
{
    uint32_t b;
    if (ver < 2)
        return rf(o);
    b = r8(o);
    return b == V_F32 ? rf(o) : (float)((int)b + V_INT_MIN);
}

static uint32_t fnv1a(const uint8_t *b, uint32_t n)
{
    uint32_t h = 2166136261u, i;
    for (i = 0; i < n; i++)
        h = (h ^ b[i]) * 16777619u;
    return h;
}

#define HDR 16u   /* magic, version, flags, payload length, checksum */
#define TAG_BUS 0x42344345u   /* "EC4B" */
#define REC_CHUNK_VERSION 1

void ec_recset_default(ec_recset_t *r)
{
    memset(r, 0, sizeof *r);
    r->mode = EC_REC_CAPTURE;
    r->bars = 1;
    r->countin = 0;
    r->metro = EC_METRO_REC;
    r->metro_level = 0.5f;
    r->automation = 1;
    r->hard = 0;
}

/* a pattern track: version 1 every param (id, f32) and steps (u16 index, trig, u32 slices, locks (id, f32));
 * version 2 the params that differ from the defaults and steps (u8 index, flags: 1 trig 2 slices, [u32 slices],
 * locks (id, value)) */
static void save_track(io_t *o, const ec_ptrack_t *tr, int ver)
{
    int k, s, nsteps = 0, np = 0;
    for (k = 0; k < EC_NP; k++)
        np += ver < 2 || tr->v[k] != EC_PARAMS[k].def;
    w8(o, (uint32_t)np);
    for (k = 0; k < EC_NP; k++)
        if (ver < 2 || tr->v[k] != EC_PARAMS[k].def) {
            w8(o, (uint32_t)k);
            wv(o, ver, tr->v[k]);
        }
    for (s = 0; s < EC_MAXSTEPS; s++)
        nsteps += ec_step_has_content(&tr->steps[s]);
    w16(o, (uint32_t)nsteps);
    for (s = 0; s < EC_MAXSTEPS && !o->bad; s++) {
        const ec_step_t *st = &tr->steps[s];
        if (!ec_step_has_content(st))
            continue;
        if (ver < 2) {
            w16(o, (uint32_t)s);
            w8(o, st->trig);
            w32(o, st->slices);
        } else {
            w8(o, (uint32_t)s);
            w8(o, (st->trig ? 1u : 0u) | (st->slices ? 2u : 0u));
            if (st->slices)
                w32(o, st->slices);
        }
        w8(o, st->nlock);
        for (k = 0; k < st->nlock; k++) {
            w8(o, st->lock_id[k]);
            wv(o, ver, st->lock_val[k]);
        }
    }
}

/* the whole record into b (NULL: measure only); returns its length or -1 */
static int32_t emit(const ec_project_t *p, uint8_t *b, uint32_t cap, int ver)
{
    io_t o;
    int i, t, k;
    if (cap < HDR)
        return -1;
    memset(&o, 0, sizeof o);
    o.b = b;
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
    for (i = 0; i < EC_NPAT && !o.bad; i++) {
        const ec_pattern_t *pt = &p->pat[i];
        w8(&o, pt->used);
        if (!pt->used)
            continue;
        for (t = 0; t < EC_NTRACK; t++)
            save_track(&o, &pt->tr[t], ver);
    }
    /* Tagged chunks after the patterns: tag u32, length u32, payload. An old (0.1.0) record ends after the
     * patterns; the loader dispatches every tag it knows and skips the others. */
    if (!o.bad) {   /* E3 "RZR1": the razor tables */
        const int32_t n = ec_razor_save_chunk(p, b ? b + o.n : 0, cap - o.n);
        if (n < 0)
            o.bad = 1;
        else
            o.n += (uint32_t)n;
    }
    {   /* E4 "EC4B": the bus FX chunk: per bus: count, (id, value) pairs */
        uint32_t at, bus;
        w32(&o, TAG_BUS);
        at = o.n;
        w32(&o, 0);
        w8(&o, 3);
        for (bus = 0; bus < 3; bus++) {
            int cnt = 0;
            for (k = 0; k < EC_NP; k++)
                cnt += (EC_PARAMS[k].flags & EC_PF_FX) != 0;
            w8(&o, (uint32_t)cnt);
            for (k = 0; k < EC_NP; k++)
                if (EC_PARAMS[k].flags & EC_PF_FX) {
                    w8(&o, (uint32_t)k);
                    wf(&o, p->bus[bus][k]);
                }
        }
        if (!o.bad) {
            const uint32_t len = o.n - at - 4, end = o.n;
            o.n = at;
            w32(&o, len);
            o.n = end;
        }
    }
    /* E5: the scenes, names, per-pattern BPM, page loops, slice programs */
    if (!o.bad && o.n + 8 <= o.cap) {
        const uint32_t at = o.n;
        const int32_t n = ec_e5_chunk_save(p, b ? b + at + 8 : 0, o.cap - at - 8);
        if (n < 0)
            o.bad = 1;
        else {
            w32(&o, EC_CHUNK_E5);
            w32(&o, (uint32_t)n);
            o.n += (uint32_t)n;
        }
    } else
        o.bad = 1;
    if (ver >= 2) {   /* 0.6.0 "ECRS": the record settings */
        const ec_recset_t *r = &p->rec;
        w32(&o, EC_CHUNK_REC);
        w32(&o, 11);
        w8(&o, REC_CHUNK_VERSION);
        w8(&o, r->mode);
        w8(&o, r->bars);
        w8(&o, r->countin);
        w8(&o, r->metro);
        wf(&o, r->metro_level);
        w8(&o, r->automation);
        w8(&o, r->hard);
    }
    if (o.bad)
        return -1;
    if (b) {
        const uint32_t len = o.n - HDR, n = o.n;
        o.n = 0;
        w32(&o, EC_PROJECT_MAGIC);
        w16(&o, (uint32_t)ver);
        w16(&o, 0);
        w32(&o, len);
        w32(&o, fnv1a(b + HDR, len));
        o.n = n;
    }
    return (int32_t)o.n;
}

/* measured first: a record that does not fit leaves buf (the flash's old record) untouched */
static int32_t save_ver(const ec_project_t *p, uint8_t *buf, uint32_t cap, int ver)
{
    if (!buf || emit(p, 0, cap, ver) < 0)
        return -1;
    return emit(p, buf, cap, ver);
}
int32_t ec_project_save(const ec_project_t *p, uint8_t *buf, uint32_t cap)
{
    return save_ver(p, buf, cap, EC_PROJECT_VERSION);
}
int32_t ec_project_save_v1(const ec_project_t *p, uint8_t *buf, uint32_t cap) { return save_ver(p, buf, cap, 1); }

static float clampf(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }

static int g_dropped;
int ec_project_mtx_dropped(void) { return g_dropped; }

static void load_rec_chunk(ec_project_t *p, const uint8_t *b, uint32_t n)
{
    ec_recset_t *r = &p->rec;
    float lv;
    if (n < 11 || b[0] != REC_CHUNK_VERSION)
        return;
    r->mode = b[1] <= EC_REC_OVERDUB ? b[1] : EC_REC_CAPTURE;
    r->bars = b[2] == 1 || b[2] == 2 || b[2] == 4 || b[2] == 8 ? b[2] : 1;
    r->countin = b[3] <= 2 ? b[3] : 0;
    r->metro = b[4] <= EC_METRO_ALWAYS ? b[4] : EC_METRO_REC;
    memcpy(&lv, b + 5, 4);
    r->metro_level = lv == lv ? clampf(lv, 0, 1) : 0.5f;
    r->automation = b[9] ? 1 : 0;
    r->hard = b[10] ? 1 : 0;
}

/* 0.6.1: a record's header and checksum, without parsing it: its whole length, or -1 */
int32_t ec_project_check(const uint8_t *buf, uint32_t n)
{
    io_t o;
    uint32_t len, sum;
    int ver;
    if (!buf || n < HDR)
        return -1;
    memset(&o, 0, sizeof o);
    o.r = buf;
    o.cap = n;
    if (r32(&o) != EC_PROJECT_MAGIC)
        return -1;
    ver = (int)r16(&o);
    (void)r16(&o);
    len = r32(&o);
    sum = r32(&o);
    if (ver < 1 || ver > EC_PROJECT_VERSION || len > n - HDR || fnv1a(buf + HDR, len) != sum)
        return -1;
    return (int32_t)(HDR + len);
}

int ec_project_load(ec_project_t *p, const uint8_t *buf, uint32_t n)
{
    uint32_t migrated = 0;
    io_t o;
    uint32_t len, sum, nslot, npat, ntr;
    uint32_t i, t, k, s;
    int ver;
    if (!buf || n < HDR)
        return -1;
    memset(&o, 0, sizeof o);
    o.r = buf;
    o.cap = n;
    if (r32(&o) != EC_PROJECT_MAGIC)
        return -1;
    ver = (int)r16(&o);
    if (ver < 1 || ver > EC_PROJECT_VERSION)
        return -1;
    (void)r16(&o);
    len = r32(&o);
    sum = r32(&o);
    if (len > n - HDR || fnv1a(buf + HDR, len) != sum)
        return -1;
    o.cap = HDR + len;
    g_dropped = 0;
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
    ec_e5_load_begin(p);                   /* E5: the defaults an older record implies (pattern BPM = the BPM) */
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
            int has_mtx = 0;
            if (tr)
                ec_params_default(tr->v);
            for (k = 0; k < np; k++) {
                const uint32_t id = r8(&o);
                const float v = rv(&o, ver);
                if (tr && id < EC_NP)
                    tr->v[id] = ec_param_clamp((int)id, v);
                has_mtx |= id >= EC_P_MTX1_SRC && id < EC_P_MTX_END;
            }
            /* 0.5.0: a 0.4 track (a version 1 record without the matrix ids): its fixed mod routes into the
             * matrix. (Version 2 leaves default params out, so a missing id means nothing there.) */
            if (ver == 1 && tr && !has_mtx && !o.bad) {
                g_dropped += ec_mtx_migrate(tr->v);
                if (i == (uint32_t)p->current)
                    migrated |= 1u << t;
            }
            nsteps = r16(&o);
            for (k = 0; k < nsteps && !o.bad; k++) {
                uint32_t idx, trig, slices = 0, nl;
                ec_step_t tmp, *st;
                if (ver < 2) {
                    idx = r16(&o);
                    trig = r8(&o);
                    slices = r32(&o);
                } else {
                    const uint32_t fl = (idx = r8(&o), r8(&o));
                    trig = fl & 1u;
                    if (fl & 2u)
                        slices = r32(&o);
                }
                nl = r8(&o);
                st = tr && idx < EC_MAXSTEPS ? &tr->steps[idx] : &tmp;
                memset(st, 0, sizeof *st);
                st->trig = (uint8_t)(trig ? 1 : 0);
                st->slices = slices;
                for (s = 0; s < nl; s++) {
                    const uint32_t id = r8(&o);
                    const float v = rv(&o, ver);
                    if (id < EC_NP)
                        ec_step_lock_set(st, (int)id, ec_param_clamp((int)id, v));
                }
            }
        }
    }
    /* tagged chunks after the patterns: one loop, every known tag dispatched, unknown ones skipped */
    while (!o.bad && o.n + 8 <= o.cap) {
        const uint32_t tag = r32(&o), clen = r32(&o), end = o.n + clen;
        if (o.bad || clen > o.cap - o.n) {
            o.bad = 1;
            break;
        }
        if (tag == EC_RAZOR_TAG) {          /* E3 "RZR1": the razor tables */
            ec_razor_load_chunk(p, o.r + o.n, clen);
        } else if (tag == TAG_BUS) {        /* E4 "EC4B": the buses */
            const uint32_t cap0 = o.cap, nb = (o.cap = end, r8(&o));
            for (i = 0; i < nb && !o.bad; i++) {
                const uint32_t cnt = r8(&o);
                for (k = 0; k < cnt && !o.bad; k++) {
                    const uint32_t id = r8(&o);
                    const float v = rf(&o);
                    if (i < 3 && id < EC_NP && (EC_PARAMS[id].flags & EC_PF_FX))
                        p->bus[i][id] = ec_param_clamp((int)id, v);
                }
            }
            o.cap = cap0;
        } else if (tag == EC_CHUNK_E5) {    /* E5: scenes, names, per-pattern BPM, page loops, slice programs */
            ec_e5_chunk_load(p, buf + o.n, clen);
        } else if (tag == EC_CHUNK_REC) {   /* 0.6.0: the record settings */
            load_rec_chunk(p, buf + o.n, clen);
        }
        o.n = end;
    }
    if (o.bad)
        return -1;
    for (t = 0; t < EC_NTRACK; t++)               /* 0.5.0: the scenes' old macro depths follow their slots */
        if (migrated >> t & 1u) {
            const float *v = p->pat[p->current].tr[t].v;
            uint32_t sc;
            for (k = EC_P_MACRO_DEPTH; k < EC_P_MACRO_DEPTH + 20; k++)
                for (sc = 0; sc < 2; sc++)
                    if (p->e5.sc.has[sc][t][k]) {
                        float d;
                        const int slot = ec_mtx_migrated_slot(v, (int)k, p->e5.sc.val[sc][t][k], &d);
                        p->e5.sc.has[sc][t][k] = 0;
                        if (slot >= 0) {
                            p->e5.sc.has[sc][t][ec_mtx_depth_id(slot)] = 1;
                            p->e5.sc.val[sc][t][ec_mtx_depth_id(slot)] = d;
                        }
                    }
        }
    if (!p->pat[p->current].used) {
        p->pat[p->current].used = 1;
        for (t = 0; t < EC_NTRACK; t++) {
            ec_params_default(p->pat[p->current].tr[t].v);
            p->pat[p->current].tr[t].v[EC_P_SLOT] = (float)(t + 1);
        }
    }
    return 0;
}
