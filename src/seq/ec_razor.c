/* SPDX-License-Identifier: GPL-3.0-only */
/* E3: razor slice tables, per-slice settings, onset / zero-crossing analysis, the "RZR1" chunk (ec_razor.h).
 * Ports lib/elasticat.lua's razor_redistribute / razor_autochop / razor_point_edit / seq_slice_range and
 * lib/wav_reader.lua's read_wav_onsets / pick_transients / nearest_zero_cross. */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "ec_project.h"
#include "ec_razor.h"

static int clampi(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }
static float clampf(float x, float lo, float hi) { return x != x ? lo : x < lo ? lo : x > hi ? hi : x; }
static int iv(const float *v, int id) { return (int)floorf(v[id] + 0.5f); }

int ec_razor_is_razor(int m) { return m == EC_MACH_RAZOR_SLICE || m == EC_MACH_RAZOR_POLY; }

void ec_razor_init(ec_razor_t *rz)
{
    int i;
    memset(rz, 0, sizeof *rz);
    for (i = 0; i < EC_RZ_SLICES; i++) {   /* razor_reset's 4-unit grid ... */
        rz->start[i] = (float)i * 4.0f;
        rz->end[i] = (float)(i + 1) * 4.0f;
    }
    ec_razor_redistribute(rz, 16);         /* ... with the first 16 spread over the whole sample (slice_count 16) */
    rz->snap = EC_SNAP_ZOOM;
    rz->sens = 50;
}

void ec_razor_redistribute(ec_razor_t *rz, int count)
{
    const int n = clampi(count, 1, EC_RZ_SLICES);
    const float w = 128.0f / (float)n;
    int i;
    for (i = 0; i < n; i++) {
        rz->start[i] = (float)i * w;
        rz->end[i] = (float)(i + 1) * w;
    }
}

void ec_razor_range(const ec_project_t *p, int t, int slice, float *s, float *e)
{
    const float *v = p->pat[p->current].tr[t].v;
    const int k = clampi(slice, 1, EC_RZ_SLICES) - 1;
    if (ec_razor_is_razor(iv(v, EC_P_MACHINE))) {
        float a = clampf(p->razor[t].start[k], 0, 128), b = clampf(p->razor[t].end[k], 0, 128);
        if (b <= a)
            b = a + 0.01f > 128 ? 128 : a + 0.01f;
        *s = a;
        *e = b;
    } else {
        const int cnt = clampi(iv(v, EC_P_SLICE_COUNT), 1, 32);
        *s = 128.0f * (float)k / (float)cnt;
        *e = 128.0f * (float)(k + 1) / (float)cnt;
    }
}

/* the param actions' clamp: end > start by 0.01 */
static void put(ec_razor_t *rz, int k, int which, float x)
{
    x = clampf(x, 0, 128);
    if (which == 0) {
        if (x > 127.99f)
            x = 127.99f;
        rz->start[k] = x;
        if (rz->end[k] <= x)
            rz->end[k] = x + 0.01f > 128 ? 128 : x + 0.01f;
    } else {
        rz->end[k] = x;
        if (x <= rz->start[k])
            rz->end[k] = rz->start[k] + 0.01f > 128 ? 128 : rz->start[k] + 0.01f;
    }
}

void ec_razor_set_point(ec_razor_t *rz, int slice, int which, float x, int friends, int count)
{
    const int k = clampi(slice, 1, EC_RZ_SLICES) - 1;
    float landed;
    put(rz, k, which, x);
    if (!friends)
        return;
    landed = which == 0 ? rz->start[k] : rz->end[k];
    if (which == 0 && k > 0)
        put(rz, k - 1, 1, landed);
    else if (which == 1 && k + 1 < count && k + 1 < EC_RZ_SLICES)
        put(rz, k + 1, 0, landed);
}

/* move slice k's record to j */
static void copy_slice(ec_razor_t *rz, int j, int k)
{
    rz->start[j] = rz->start[k];
    rz->end[j] = rz->end[k];
    rz->choke[j] = rz->choke[k];
    rz->mute[j] = rz->mute[k];
    rz->play[j] = rz->play[k];
    rz->pitch[j] = rz->pitch[k];
}

/* the track's steps and slice_index follow an insert (dir +1 after slice k0) or a delete (dir -1 of slice k0) */
static void remap_steps(ec_project_t *p, int t, int k0, int dir)
{
    int i, s;
    for (i = 0; i < EC_NPAT; i++) {
        ec_ptrack_t *tr = &p->pat[i].tr[t];
        float *si = &tr->v[EC_P_SLICE_INDEX];
        if (!p->pat[i].used)
            continue;
        for (s = 0; s < EC_MAXSTEPS; s++) {
            uint32_t m = tr->steps[s].slices;
            if (!m)
                continue;
            if (dir > 0) {                     /* slices after k0 (0-based) move up one */
                const uint32_t low = k0 + 1 >= 32 ? 0xFFFFFFFFu : (1u << (k0 + 1)) - 1u;
                m = (m & low) | ((m & ~low) << 1);
            } else {                           /* slice k0 goes, the ones after move down */
                const uint32_t low = (1u << k0) - 1u;
                m = (m & low) | ((m >> 1) & ~low);
            }
            tr->steps[s].slices = m;
            if (!m && !tr->steps[s].trig && !tr->steps[s].nlock)
                memset(&tr->steps[s], 0, sizeof tr->steps[s]);
        }
        if (dir > 0 && *si > (float)(k0 + 1))
            *si += 1;
        else if (dir < 0 && *si > (float)(k0 + 1))
            *si -= 1;
        *si = clampf(*si, 1, 32);
    }
}

int ec_razor_add(ec_project_t *p, int t, int slice, float x)
{
    ec_razor_t *rz = &p->razor[t];
    float *v = p->pat[p->current].tr[t].v;
    const int count = clampi(iv(v, EC_P_SLICE_COUNT), 1, 32);
    const int k = clampi(slice, 1, count) - 1;
    int j;
    float a = rz->start[k], b = rz->end[k];
    if (count >= EC_RZ_SLICES)
        return count;
    if (b < a) {
        const float tmp = a;
        a = b;
        b = tmp;
    }
    if (!(x > a + 0.01f && x < b - 0.01f))
        x = 0.5f * (a + b);
    for (j = EC_RZ_SLICES - 1; j > k + 1; j--)
        copy_slice(rz, j, j - 1);
    copy_slice(rz, k + 1, k);
    rz->mute[k + 1] = 0;
    rz->start[k] = a;
    rz->end[k] = x;
    rz->start[k + 1] = x;
    rz->end[k + 1] = b;
    remap_steps(p, t, k, +1);
    v[EC_P_SLICE_COUNT] = (float)(count + 1);
    return count + 1;
}

int ec_razor_delete(ec_project_t *p, int t, int slice)
{
    ec_razor_t *rz = &p->razor[t];
    float *v = p->pat[p->current].tr[t].v;
    const int count = clampi(iv(v, EC_P_SLICE_COUNT), 1, 32);
    const int k = clampi(slice, 1, count) - 1;
    int j;
    if (count <= 1)
        return 1;
    for (j = k; j < EC_RZ_SLICES - 1; j++)
        copy_slice(rz, j, j + 1);
    rz->start[EC_RZ_SLICES - 1] = 124;     /* (the freed last record: a 4-unit grid slice) */
    rz->end[EC_RZ_SLICES - 1] = 128;
    rz->choke[EC_RZ_SLICES - 1] = 0;
    rz->mute[EC_RZ_SLICES - 1] = 0;
    rz->play[EC_RZ_SLICES - 1] = 0;
    rz->pitch[EC_RZ_SLICES - 1] = 0;
    remap_steps(p, t, k, -1);
    v[EC_P_SLICE_COUNT] = (float)(count - 1);
    return count - 1;
}

int ec_razor_play_mode(const ec_project_t *p, int t, int slice)
{
    const int k = clampi(slice, 1, EC_RZ_SLICES) - 1;
    const int pm = p->razor[t].play[k];
    return pm >= 1 && pm <= 6 ? pm : clampi(iv(p->pat[p->current].tr[t].v, EC_P_SLICE_PLAY_MODE), 1, 6);
}

int ec_razor_slice_event(const ec_project_t *p, int t, ec_event_t *e, int64_t note)
{
    const ec_razor_t *rz = &p->razor[t];
    const float *v = p->pat[p->current].tr[t].v;
    const int k = clampi(e->slice, 1, EC_RZ_SLICES) - 1;
    const int pm = rz->play[k];
    if (rz->mute[k])
        return 0;
    if (ec_razor_is_razor(iv(v, EC_P_MACHINE)))
        ec_razor_range(p, t, k + 1, &e->start, &e->end);
    if (!e->choke)   /* the pattern's slice program (E5, the original's slice_choke) wins; else the razor table's */
        e->choke = clampi(rz->choke[k], 0, 8);
    e->pitch += clampf(rz->pitch[k], -24, 24);
    e->mode = pm >= 1 && pm <= 6 ? pm : 0;
    e->length = ec_razor_play_mode(p, t, k + 1) == EC_SLICE_ONESHOT ? 0
              : (int)(note > 0x7FFFFFFF ? 0x7FFFFFFF : note < 0 ? 0 : note);
    return 1;
}

/* ------------------------------------------------------------------------------------------- analysis --- */
void ec_razor_onsets(const int16_t *data, uint32_t frames, float rate, ec_onsets_t *out)
{
    const int W = EC_RZ_ONSET_WINDOWS;
    float prev = 0, sum = 0;
    int w;
    memset(out, 0, sizeof *out);
    if (!data || frames < (uint32_t)W || rate <= 0)
        return;
    for (w = 0; w < W; w++) {
        const uint32_t s0 = (uint32_t)((uint64_t)w * frames / (uint32_t)W);
        uint32_t s1 = (uint32_t)((uint64_t)(w + 1) * frames / (uint32_t)W);
        uint32_t span, reads, stride, f;
        float e = 0;
        int n = 0;
        s1 = s1 > s0 ? s1 - 1 : s0;
        span = s1 - s0 + 1;
        reads = span < 32 ? span : 32;        /* (the Lua read 8 frames per window; 32 is steadier) */
        stride = span / reads > 0 ? span / reads : 1;
        for (f = s0; f <= s1 && n < (int)reads; f += stride, n++) {
            const int x = data[2 * (size_t)f];
            e += (float)(x < 0 ? -x : x) / 32768.0f;
        }
        e = n ? e / (float)n : 0;
        {
            float d = w ? e - prev : e;
            d = d < 0 ? 0 : d;
            out->rise[w] = d;
            if (d > out->max_rise)
                out->max_rise = d;
            sum += d;
        }
        prev = e;
    }
    out->mean_rise = sum / (float)W;
    out->frames = frames;
    out->rate = rate;
    out->valid = 1;
}

int ec_razor_transients(const ec_onsets_t *on, float sens, float *pos01, int cap)
{
    const int W = EC_RZ_ONSET_WINDOWS;
    float th, spw;
    int gap, last, w, n = 0;
    if (!on || !on->valid || on->max_rise <= 0)
        return 0;
    sens = clampf(sens, 0, 1);
    th = on->max_rise * (0.05f + (1.0f - sens) * 0.3f);
    if (on->mean_rise * 3.0f > th)
        th = on->mean_rise * 3.0f;
    spw = (float)on->frames / (float)W / on->rate;
    gap = (int)ceilf(0.03f / (spw > 1e-6f ? spw : 1e-6f));
    gap = gap < 1 ? 1 : gap;
    last = -gap;
    for (w = 1; w < W - 1 && n < cap; w++)
        if (on->rise[w] >= th && on->rise[w] >= on->rise[w - 1] && on->rise[w] > on->rise[w + 1] && w - last >= gap) {
            pos01[n++] = (float)w / (float)W;
            last = w;
        }
    return n;
}

double ec_razor_zero_cross(const int16_t *data, uint32_t frames, float rate, double pos01, double window_s)
{
    long target, half, lo, hi, i, best = -1, bestd = 0;
    int prev = 0, have = 0;
    if (!data || frames < 2 || rate <= 0)
        return pos01;
    target = (long)floor((pos01 < 0 ? 0 : pos01 > 1 ? 1 : pos01) * (double)frames + 0.5);
    half = (long)floor(window_s * rate);
    half = half < 4 ? 4 : half;
    lo = target - half < 0 ? 0 : target - half;
    hi = target + half > (long)frames - 1 ? (long)frames - 1 : target + half;
    for (i = lo; i <= hi; i++) {
        const int v = data[2 * (size_t)i];
        if (have && ((prev < 0 && v >= 0) || (prev >= 0 && v < 0))) {
            const long cross = abs(v) <= abs(prev) ? i : i - 1;
            const long d = cross > target ? cross - target : target - cross;
            if (best < 0 || d < bestd) {
                best = cross;
                bestd = d;
            }
        }
        prev = v;
        have = 1;
    }
    return best < 0 ? pos01 : (double)best / (double)frames;
}

int ec_razor_autochop(ec_project_t *p, int t, const float *pos01, int n, double lo01, double hi01)
{
    ec_razor_t *rz = &p->razor[t];
    float x[EC_RZ_SLICES];
    int i, cnt = 0;
    const double span = hi01 - lo01;
    if (span <= 1e-9)
        return 0;
    for (i = 0; i < n && cnt < EC_RZ_SLICES; i++) {
        const double tx = (pos01[i] - lo01) / span * 128.0;
        if (tx >= 0 && tx < 128.0)
            x[cnt++] = (float)tx;
    }
    if (!cnt)
        return 0;
    for (i = 0; i < cnt; i++) {
        const float a = x[i];
        float b = i + 1 < cnt ? x[i + 1] : 128.0f;
        if (b <= a)
            b = a + 0.01f > 128 ? 128 : a + 0.01f;
        rz->start[i] = a;
        rz->end[i] = b;
    }
    p->pat[p->current].tr[t].v[EC_P_SLICE_COUNT] = (float)cnt;
    return cnt;
}

/* ------------------------------------------------------------------------------------------- the chunk --- */
static uint8_t *put8(uint8_t *d, uint32_t v)
{
    *d = (uint8_t)v;
    return d + 1;
}
static uint8_t *put32(uint8_t *d, uint32_t v)
{
    d[0] = (uint8_t)v, d[1] = (uint8_t)(v >> 8), d[2] = (uint8_t)(v >> 16), d[3] = (uint8_t)(v >> 24);
    return d + 4;
}
static uint8_t *putf(uint8_t *d, float f)
{
    uint32_t u;
    memcpy(&u, &f, 4);
    return put32(d, u);
}
static uint32_t get32(const uint8_t *s) { return s[0] | s[1] << 8 | s[2] << 16 | (uint32_t)s[3] << 24; }
static float getf(const uint8_t *s)
{
    const uint32_t u = get32(s);
    float f;
    memcpy(&f, &u, 4);
    return f != f ? 0.0f : f;
}

#define SLICE_BYTES 15u   /* start, end, choke, mute, play, pitch */
#define TRACK_BYTES (3u + EC_RZ_SLICES * SLICE_BYTES)

int32_t ec_razor_save_chunk(const ec_project_t *p, uint8_t *dst, uint32_t cap)
{
    const uint32_t len = 2u + EC_NTRACK * TRACK_BYTES;
    uint8_t *d = dst;
    int t, k;
    if (cap < 8u + len)
        return -1;
    d = put32(d, EC_RAZOR_TAG);
    d = put32(d, len);
    d = put8(d, 1);                    /* chunk version */
    d = put8(d, EC_NTRACK);
    for (t = 0; t < EC_NTRACK; t++) {
        const ec_razor_t *rz = &p->razor[t];
        d = put8(d, rz->snap);
        d = put8(d, rz->sens);
        d = put8(d, EC_RZ_SLICES);
        for (k = 0; k < EC_RZ_SLICES; k++) {
            d = putf(d, rz->start[k]);
            d = putf(d, rz->end[k]);
            d = put8(d, (uint8_t)rz->choke[k]);
            d = put8(d, rz->mute[k]);
            d = put8(d, (uint8_t)rz->play[k]);
            d = putf(d, rz->pitch[k]);
        }
    }
    return (int32_t)(d - dst);
}

void ec_razor_load_chunk(ec_project_t *p, const uint8_t *s, uint32_t len)
{
    uint32_t nt, t, k, ns, off = 2;
    if (len < 2 || s[0] != 1)
        return;
    nt = s[1];
    for (t = 0; t < nt; t++) {
        ec_razor_t tmp, *rz = t < EC_NTRACK ? &p->razor[t] : &tmp;
        if (off + 3 > len)
            return;
        ec_razor_init(rz);
        rz->snap = (uint8_t)clampi(s[off], 1, EC_NSNAP);
        rz->sens = (uint8_t)clampi(s[off + 1], 0, 100);
        ns = s[off + 2];
        off += 3;
        for (k = 0; k < ns; k++, off += SLICE_BYTES) {
            if (off + SLICE_BYTES > len)
                return;
            if (k >= EC_RZ_SLICES)
                continue;
            rz->start[k] = clampf(getf(s + off), 0, 128);
            rz->end[k] = clampf(getf(s + off + 4), 0, 128);
            rz->choke[k] = (int8_t)clampi((int8_t)s[off + 8], 0, 8);
            rz->mute[k] = s[off + 9] ? 1 : 0;
            rz->play[k] = (int8_t)clampi((int8_t)s[off + 10], 0, 6);
            rz->pitch[k] = clampf(getf(s + off + 11), -24, 24);
        }
    }
}
