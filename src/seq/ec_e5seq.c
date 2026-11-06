/* SPDX-License-Identifier: GPL-3.0-only */
/* E5 on the model / sequencer side (ec_e5.h) */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ec_e5.h"
#include "ec_scene.h"

static int clampi(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }

/* ------------------------------------------------------------------------------------------- the project --- */
void ec_e5_project_init(ec_project_t *p)
{
    int i;
    memset(p->pe5, 0, sizeof p->pe5);
    memset(&p->e5, 0, sizeof p->e5);
    for (i = 0; i < EC_NPAT; i++)
        p->pe5[i].bpm = p->bpm;
}

void ec_e5_pattern_prepared(ec_project_t *p, int idx)
{
    /* a blank pattern: the live BPM (capture_pattern_state's target_bpm), no name, every page, no slice programs */
    memset(&p->pe5[idx], 0, sizeof p->pe5[idx]);
    p->pe5[idx].bpm = p->bpm;
}

void ec_e5_load_begin(ec_project_t *p)
{
    int i;
    for (i = 0; i < EC_NPAT; i++)
        p->pe5[i].bpm = p->bpm;
}

/* bounded little-endian writer / reader */
typedef struct {
    uint8_t *w;
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
    if (o->w)                          /* (NULL: ec_project_save measuring) */
        o->w[o->n] = (uint8_t)v;
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
static void wname(io_t *o, const char *s)
{
    uint32_t n = 0, k;
    while (n < EC_NAME - 1 && s[n])
        n++;
    w8(o, n);
    for (k = 0; k < n; k++)
        w8(o, (uint8_t)s[k]);
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
static void rname(io_t *o, char *s)
{
    const uint32_t n = r8(o);
    uint32_t k;
    for (k = 0; k < n; k++) {
        const char c = (char)r8(o);
        if (k < EC_NAME - 1)
            s[k] = c >= 32 && c < 127 ? c : ' ';
    }
    s[n < EC_NAME - 1 ? n : EC_NAME - 1] = 0;
}

#define CHUNK_VERSION 1

int32_t ec_e5_chunk_save(const ec_project_t *p, uint8_t *buf, uint32_t cap)
{
    io_t o;
    int s, t, i, k, np = 0;
    memset(&o, 0, sizeof o);
    o.w = buf;
    o.cap = cap;
    w8(&o, CHUNK_VERSION);
    wname(&o, p->e5.name);
    w8(&o, (uint32_t)(p->e5.gbpm ? 1 : 0));
    wf(&o, p->e5.sc.pos);
    for (s = 0; s < 2; s++) {
        w16(&o, (uint32_t)ec_scene_count(&p->e5.sc, s));
        for (t = 0; t < EC_SCN_ROWS; t++)   /* rows 6..8: the buses (0.2.0) */
            for (i = 0; i < EC_NP; i++)
                if (p->e5.sc.has[s][t][i]) {
                    w8(&o, (uint32_t)t);
                    w8(&o, (uint32_t)i);
                    wf(&o, p->e5.sc.val[s][t][i]);
                }
    }
    for (i = 0; i < EC_NPAT; i++)
        np += p->pat[i].used != 0;
    w8(&o, (uint32_t)np);
    for (i = 0; i < EC_NPAT; i++) {
        const ec_pat_e5_t *e = &p->pe5[i];
        if (!p->pat[i].used)
            continue;
        w8(&o, (uint32_t)i);
        wname(&o, e->name);
        wf(&o, e->bpm);
        for (t = 0; t < EC_NTRACK; t++) {
            const ec_slicemap_t *m = &e->slices[t];
            int nc = 0, nl = 0;
            w16(&o, e->page_loop[t]);
            w32(&o, m->mute);
            for (s = 0; s < 32; s++) {
                nc += m->choke[s] != 0;
                nl += m->nlock[s] != 0;
            }
            w8(&o, (uint32_t)nc);
            for (s = 0; s < 32; s++)
                if (m->choke[s]) {
                    w8(&o, (uint32_t)s);
                    w8(&o, m->choke[s]);
                }
            w8(&o, (uint32_t)nl);
            for (s = 0; s < 32; s++)
                if (m->nlock[s]) {
                    w8(&o, (uint32_t)s);
                    w8(&o, m->nlock[s]);
                    for (k = 0; k < m->nlock[s]; k++) {
                        w8(&o, m->lock_id[s][k]);
                        wf(&o, m->lock_val[s][k]);
                    }
                }
        }
    }
    return o.bad ? -1 : (int32_t)o.n;
}

int ec_e5_chunk_load(ec_project_t *p, const uint8_t *buf, uint32_t n)
{
    io_t o;
    uint32_t s, t, i, k, cnt, np;
    memset(&o, 0, sizeof o);
    o.r = buf;
    o.cap = n;
    if (r8(&o) != CHUNK_VERSION)
        return -1;
    rname(&o, p->e5.name);
    p->e5.gbpm = r8(&o) != 0;
    p->e5.sc.pos = rf(&o);
    p->e5.sc.pos = p->e5.sc.pos < 0 ? 0 : p->e5.sc.pos > 1 ? 1 : p->e5.sc.pos;
    for (s = 0; s < 2 && !o.bad; s++) {
        cnt = r16(&o);
        for (k = 0; k < cnt && !o.bad; k++) {
            const uint32_t tr = r8(&o), id = r8(&o);
            const float v = rf(&o);
            if (id < EC_NP && ec_scene_row_morphable((int)tr, (int)id)) {
                p->e5.sc.has[s][tr][id] = 1;
                p->e5.sc.val[s][tr][id] = ec_param_clamp((int)id, v);
            }
        }
    }
    np = r8(&o);
    for (i = 0; i < np && !o.bad; i++) {
        const uint32_t idx = r8(&o);
        ec_pat_e5_t tmp, *e = idx < EC_NPAT ? &p->pe5[idx] : &tmp;
        memset(e, 0, sizeof *e);
        rname(&o, e->name);
        e->bpm = rf(&o);
        e->bpm = e->bpm < 20 ? 20 : e->bpm > 300 ? 300 : e->bpm;
        for (t = 0; t < EC_NTRACK && !o.bad; t++) {
            ec_slicemap_t *m = &e->slices[t];
            e->page_loop[t] = (uint16_t)r16(&o);
            m->mute = r32(&o);
            cnt = r8(&o);
            for (k = 0; k < cnt; k++) {
                const uint32_t sl = r8(&o), g = r8(&o);
                if (sl < 32)
                    m->choke[sl] = (uint8_t)(g > 8 ? 8 : g);
            }
            cnt = r8(&o);
            for (k = 0; k < cnt && !o.bad; k++) {
                const uint32_t sl = r8(&o), nl = r8(&o);
                uint32_t j;
                for (j = 0; j < nl; j++) {
                    const uint32_t id = r8(&o);
                    const float v = rf(&o);
                    if (sl < 32 && id < EC_NP)
                        ec_slice_lock_set(m, (int)sl + 1, (int)id, ec_param_clamp((int)id, v));
                }
            }
        }
    }
    return o.bad ? -1 : 0;
}

/* ------------------------------------------------------------------------------------------- page loop --- */
static uint16_t loop_mask(const ec_project_t *p, int t, int pages)
{
    const uint16_t all = (uint16_t)(pages >= 16 ? 0xFFFFu : (1u << pages) - 1u);
    const uint16_t m = p->pe5[p->current].page_loop[t] & all;
    return m ? m : all;
}

int ec_e5_page_in_loop(const ec_project_t *p, int t, int page, int pages)
{
    return page >= 0 && page < pages && (loop_mask(p, t, pages) >> page & 1u);
}

uint16_t ec_e5_page_loop_mask(const ec_project_t *p, int t, int pages) { return loop_mask(p, t, pages); }

void ec_e5_set_page_loop(ec_project_t *p, int t, uint16_t mask, int pages)
{
    const uint16_t all = (uint16_t)(pages >= 16 ? 0xFFFFu : (1u << pages) - 1u);
    mask &= all;
    if (!mask)
        mask = all;
    p->pe5[p->current].page_loop[t] = mask == all ? 0 : mask;
}

void ec_e5_toggle_page_loop(ec_project_t *p, int t, int page, int pages)
{
    const uint16_t all = (uint16_t)(pages >= 16 ? 0xFFFFu : (1u << pages) - 1u);
    uint16_t m = loop_mask(p, t, pages);
    if (page < 0 || page >= pages)
        return;
    m ^= (uint16_t)(1u << page);
    if (!m)
        m = (uint16_t)(1u << page);                  /* (never empty) */
    p->pe5[p->current].page_loop[t] = m == all ? 0 : m;
}

int ec_e5_play_index(const ec_project_t *p, int t, int steps, int64_t pos, int *pass)
{
    const int pages = (steps + 15) / 16;
    const uint16_t m = loop_mask(p, t, pages);
    int total = 0, pg;
    int64_t k;
    if (pos < 0) {
        if (pass)
            *pass = 1;
        return 0;
    }
    if (m == (uint16_t)(pages >= 16 ? 0xFFFFu : (1u << pages) - 1u)) {   /* every page: the plain wrap */
        if (pass)
            *pass = (int)(pos / steps) + 1;
        return (int)(pos % steps);
    }
    for (pg = 0; pg < pages; pg++)
        if (m >> pg & 1u)
            total += steps - pg * 16 < 16 ? steps - pg * 16 : 16;
    if (pass)
        *pass = (int)(pos / total) + 1;
    k = pos % total;
    for (pg = 0; pg < pages; pg++)
        if (m >> pg & 1u) {
            const int len = steps - pg * 16 < 16 ? steps - pg * 16 : 16;
            if (k < len)
                return pg * 16 + (int)k;
            k -= len;
        }
    return 0;
}

/* ------------------------------------------------------------------------------------------- slice programs --- */
int ec_slice_lock_get(const ec_slicemap_t *m, int slice, int id, float *val)
{
    int k;
    if (slice < 1 || slice > 32)
        return 0;
    for (k = 0; k < m->nlock[slice - 1]; k++)
        if (m->lock_id[slice - 1][k] == id) {
            if (val)
                *val = m->lock_val[slice - 1][k];
            return 1;
        }
    return 0;
}

int ec_slice_lock_set(ec_slicemap_t *m, int slice, int id, float val)
{
    int k, s = slice - 1;
    if (slice < 1 || slice > 32)
        return -1;
    for (k = 0; k < m->nlock[s]; k++)
        if (m->lock_id[s][k] == id) {
            m->lock_val[s][k] = val;
            return 0;
        }
    if (m->nlock[s] >= EC_SLICE_LOCKS)
        return -1;
    m->lock_id[s][m->nlock[s]] = (uint8_t)id;
    m->lock_val[s][m->nlock[s]] = val;
    m->nlock[s]++;
    return 0;
}

int ec_slice_lock_clear(ec_slicemap_t *m, int slice, int id)
{
    int k, s = slice - 1;
    if (slice < 1 || slice > 32)
        return 0;
    for (k = 0; k < m->nlock[s]; k++)
        if (m->lock_id[s][k] == id) {
            m->nlock[s]--;
            m->lock_id[s][k] = m->lock_id[s][m->nlock[s]];
            m->lock_val[s][k] = m->lock_val[s][m->nlock[s]];
            return 1;
        }
    return 0;
}

void ec_slice_clear(ec_slicemap_t *m, int slice)
{
    if (slice < 1 || slice > 32)
        return;
    m->nlock[slice - 1] = 0;
    m->choke[slice - 1] = 0;
    m->mute &= ~(1u << (slice - 1));
}

int ec_slice_has_program(const ec_slicemap_t *m, int slice)
{
    return slice >= 1 && slice <= 32 &&
           (m->nlock[slice - 1] || m->choke[slice - 1] || (m->mute >> (slice - 1) & 1u));
}

/* ------------------------------------------------------------------------------------------- seq hooks --- */
void ec_e5_bpm_changed(ec_project_t *p, float bpm) { p->pe5[p->current].bpm = bpm; }

void ec_e5_pattern_loaded(ec_seq_t *s)
{
    ec_project_t *p = s->proj;
    const float b = p->pe5[p->current].bpm;
    /* BPM is per pattern unless the global BPM is on (apply_pattern_state) */
    if (!p->e5.gbpm && b >= 20 && b <= 300 && b != p->bpm)
        ec_seq_set_bpm(s, b);
    /* the scenes morph against THIS pattern's values (scene_store:apply after a pattern load) */
    if (ec_scene_apply(p, p->e5.sc.pos))
        ec_seq_touch_all(s);
}

/* ------------------------------------------------------------------------------------------- clipboards --- */
int ec_clip_copy_steps(ec_clip_steps_t *c, const ec_ptrack_t *tr, int page, uint16_t held)
{
    int i, first = -1;
    c->n = 0;
    for (i = 0; i < 16; i++)
        if (held >> i & 1u) {
            if (first < 0)
                first = i;
            c->off[c->n] = (int8_t)(i - first);
            c->st[c->n] = tr->steps[page * 16 + i];
            c->n++;
        }
    return c->n;
}

uint16_t ec_clip_paste_mask(const ec_clip_steps_t *c, uint16_t held)
{
    uint16_t m = 0;
    int d, k;
    for (d = 0; d < 16; d++)
        if (held >> d & 1u)
            for (k = 0; k < c->n; k++) {
                const int target = d + c->off[k];
                if (target >= 0 && target < 16)
                    m |= (uint16_t)(1u << target);
            }
    return m;
}

int ec_clip_paste_steps(const ec_clip_steps_t *c, ec_ptrack_t *tr, int page, uint16_t held)
{
    int d, k, n = 0;
    for (d = 0; d < 16; d++)
        if (held >> d & 1u)
            for (k = 0; k < c->n; k++) {
                const int target = d + c->off[k];   /* past the page edge: dropped (as the reference boxes) */
                if (target >= 0 && target < 16) {
                    tr->steps[page * 16 + target] = c->st[k];
                    n++;
                }
            }
    return n;
}

void ec_clip_clear_locks(ec_ptrack_t *tr, int page, uint16_t held)
{
    int i;
    for (i = 0; i < 16; i++)
        if (held >> i & 1u) {
            ec_step_t *s = &tr->steps[page * 16 + i];
            const uint8_t trig = s->trig;
            const uint32_t sl = s->slices;
            ec_step_clear(s);
            s->trig = trig;
            s->slices = sl;
        }
}

void ec_clip_copy_page(ec_clip_page_t *c, const ec_ptrack_t *tr, int page)
{
    memcpy(c->st, &tr->steps[page * 16], sizeof c->st);
    c->ok = 1;
}

void ec_clip_paste_page(const ec_clip_page_t *c, ec_ptrack_t *tr, int page)
{
    if (c->ok)
        memcpy(&tr->steps[page * 16], c->st, sizeof c->st);
}

void ec_clip_clear_page(ec_ptrack_t *tr, int page) { memset(&tr->steps[page * 16], 0, 16 * sizeof(ec_step_t)); }

void ec_clip_copy_pattern(ec_clip_pattern_t *c, const ec_project_t *p, int idx)
{
    c->pat = p->pat[idx];
    c->pe5 = p->pe5[idx];
    c->ok = 1;
}

void ec_clip_paste_pattern(const ec_clip_pattern_t *c, ec_project_t *p, int idx)
{
    char name[EC_NAME];
    if (!c->ok)
        return;
    memcpy(name, p->pe5[idx].name, sizeof name);
    p->pat[idx] = c->pat;
    p->pat[idx].used = 1;
    p->pe5[idx] = c->pe5;
    memcpy(p->pe5[idx].name, name, sizeof name);
}

void ec_clip_clear_pattern(ec_project_t *p, int idx) { ec_pattern_clear_steps(&p->pat[idx]); }

void ec_clip_copy_slice(ec_clip_slice_t *c, const ec_slicemap_t *m, int slice)
{
    const int s = clampi(slice, 1, 32) - 1;
    c->ok = 1;
    c->choke = m->choke[s];
    c->mute = (uint8_t)(m->mute >> s & 1u);
    c->nlock = m->nlock[s];
    memcpy(c->lock_id, m->lock_id[s], sizeof c->lock_id);
    memcpy(c->lock_val, m->lock_val[s], sizeof c->lock_val);
}

void ec_clip_paste_slice(const ec_clip_slice_t *c, ec_slicemap_t *m, int slice)
{
    const int s = clampi(slice, 1, 32) - 1;
    if (!c->ok)
        return;
    m->choke[s] = c->choke;
    m->mute = c->mute ? m->mute | 1u << s : m->mute & ~(1u << s);
    m->nlock[s] = c->nlock;
    memcpy(m->lock_id[s], c->lock_id, sizeof c->lock_id);
    memcpy(m->lock_val[s], c->lock_val, sizeof c->lock_val);
}

/* ------------------------------------------------------------------------------------------- namesizer --- */
/* lib/namesizer: NameSizer.rnd() = a random descriptor + a random thing (descriptors.txt / things.txt) */
static const char *const DESC[] = {
    "tulip", "purple", "elongated", "synthetic", "short", "cold", "mischievious", "fighting", "space", "hairy", "phat",
    "spicy", "saucey", "tasty", "fingerlicking", "boom", "fm", "stupid", "burger", "cat", "infinite", "quixotic",
    "flubber", "mr", "dr", "nzice", "you", "synth", "pure", "naughty", "dirty", "crunchy", "crispy", "scrambled",
    "fat", "lame", "hot", "burning", "endless", "stretched", "sizzling", "flaming", "giant", "big", "small",
    "tickled", "fast", "slow", "rapid", "vapid", "clicky", "sugary", "dry", "wet", "extrawet", "am", "nice",
    "friendly", "mean", "grouchy", "smooth", "buttery", "oily", "nutty", "mothering", "nurturing", "cosmic", "astro",
    "cyberpunk", "purple", "blue", "pink", "green", "red", "yellow", "orange", "magenta", "silver", "gold",
    "platinum", "noisy", "nosey", "plastic", "warm", "analog", "digital", "smokey", "birdlike"};
static const char *const THING[] = {
    "flamingo", "lion", "racoon", "dynamite", "walrus", "synthesizer", "monkey", "cat", "furball", "jelly",
    "breakfast", "time", "hashbrowns", "ball", "goth", "emokid", "cheese", "norns", "dust", "meatball", "burger",
    "flubber", "quixote", "kitten", "braingasm", "turtles", "chocofuzz", "punk", "wave", "chips", "circuitry",
    "rzzzzah", "epsiiaomes", "rhino", "sushi", "waffles", "eggs", "computer", "cpu", "microprocessor", "rocket",
    "planet", "iceage", "museum", "beach", "tree", "palmtree", "bird", "macadamia", "almond", "pancake", "car",
    "ride", "playdough", "putty", "watch", "phone", "rock", "pebble", "fountain", "mountain", "waterfall", "pool",
    "lake", "river", "drip", "sunshine", "earth", "ohio", "nebraska", "utah", "california"};

static uint32_t xs(uint32_t *r)
{
    uint32_t x = *r ? *r : 0x9E3779B9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *r = x;
    return x;
}

void ec_e5_random_name(char *out, int cap, uint32_t *rng)
{
    const int nd = (int)(sizeof DESC / sizeof DESC[0]), nt = (int)(sizeof THING / sizeof THING[0]);
    char b[40];
    int tries, i;
    for (tries = 0; tries < 16; tries++) {
        snprintf(b, sizeof b, "%s %s", DESC[xs(rng) % (uint32_t)nd], THING[xs(rng) % (uint32_t)nt]);
        if ((int)strlen(b) <= EC_NAME - 1)
            break;
    }
    b[EC_NAME - 1] = 0;
    for (i = 0; b[i]; i++)
        if (b[i] >= 'a' && b[i] <= 'z')
            b[i] = (char)(b[i] - 32);
    snprintf(out, (size_t)cap, "%s", b);
}

/* 0.6.47 (owner): FIZZ's name roller (FMToneSeq core/seq/names.c, the owner's own port of namesizer.lua's
 * phonic_nonsense and new_word; phonic_nonsense adapts github.com/euangoddard/phonic-nonsense by Euan Goddard, credited
 * there), at Elasticat's 16 characters: a namesizer pair six times in ten, a phonic word two, a cosmic word two */
static const char *const NM_CONS[] = {"b", "c", "d", "f", "g", "h", "j", "k", "l", "m", "n", "p", "r", "s", "t", "v",
                                      "w", "x", "y", "z"};
static const char *const NM_VOW[] = {"a", "e", "i", "o", "u"};
static const char *const NM_FUNKY[] = {"ca", "lll", "moo", "fu", "cha", "cream", "choc", "silk", "ice", "eee", "goo",
    "two", "zzz", "boo", "cats", "boom", "ch", "ck", "digi", "ome", "sh", "ish", "ed", "milk", "cyber", "punk",
    "delta", "epsi", "beta", "fire", "uzz"};
static const char *const NM_START[] = {"j", "qu", "", "m", "s", "d", "t", "n", "p", "g", "c", "k", "b", "f", "l", "h",
    "sh", "r", "v", "y", "w", "th", "z", "ch", "bl", "br", "cl", "cr", "dr", "fl", "fr", "gl", "gr", "pl", "pr", "sl",
    "sm", "sn", "sp", "st", "sw", "tr", "tw"};
static const char *const NM_MID[] = {"ee", "igh", "oo", "ar", "or", "air", "ir", "ou", "oy", "ea", "oi", "aw", "ur",
    "ow", "ai", "oa", "ew", "a", "e", "i", "o", "u"};
static const char *const NM_END[] = {"ss", "ll", "ck", "ng", "nk", "x", "m", "s", "d", "t", "n", "p", "g", "c", "k",
    "b", "f", "l", "h", "sh", "r", "v", "y", "w", "th", "z", "ch"};
static const char *const NM_VCE[] = {"d", "k", "f", "l", "m", "n", "p", "t", "v"};
static const char *const NM_COMP[] = {"are", "er", "ow", "ew", "ire", "ear", "ure"};
#define NM_N(a) ((uint32_t)(sizeof(a) / sizeof((a)[0])))
#define NM_PICK(r, a) ((a)[xs(r) % NM_N(a)])
static float nm_unit(uint32_t *r) { return (float)(xs(r) >> 8) / 16777216.f; }

static void nm_finish(char *w, char *out, int cap)
{
    size_t n, i;
    for (i = 0; w[i]; i++)
        if (w[i] >= 'a' && w[i] <= 'z')
            w[i] = (char)(w[i] - 32);
    n = strlen(w);
    if (n > EC_NAME - 1)
        w[n = EC_NAME - 1] = 0;
    while (n && (w[n - 1] == '-' || w[n - 1] == ' '))
        w[--n] = 0;
    snprintf(out, (size_t)cap, "%s", w);
}

void ec_name_phonic(char *out, int cap, uint32_t *rng)
{
    const float r = nm_unit(rng);
    char w[40] = "";
    int k;
    for (k = 0; k < 8 && !w[0]; k++) {
        if (r < 0.1f)
            snprintf(w, sizeof w, "%s%s%s", NM_PICK(rng, NM_START), NM_PICK(rng, NM_MID), NM_PICK(rng, NM_END));
        else if (r < 0.2f)
            snprintf(w, sizeof w, "%s%s%se", NM_PICK(rng, NM_START), NM_PICK(rng, NM_VOW), NM_PICK(rng, NM_VCE));
        else
            snprintf(w, sizeof w, "%s%s", NM_PICK(rng, NM_START), NM_PICK(rng, NM_COMP));
    }
    nm_finish(w, out, cap);
}

void ec_name_word(char *out, int cap, uint32_t *rng)
{
    char w[64];
    int k;
    for (k = 0; k < 8; k++) {
        const int length = 4 + (int)(xs(rng) % 5u);
        const int hyphen = nm_unit(rng) > 0.5f, hypheni = (length + 1) / 2;
        int i, b = 1, n = 0;
        for (i = 1; i <= length && n < 48; i++) {
            const char *piece = hyphen && i == hypheni ? "-" : nm_unit(rng) < 0.2f ? NM_PICK(rng, NM_FUNKY)
                                : b ? NM_PICK(rng, NM_CONS) : NM_PICK(rng, NM_VOW);
            memcpy(w + n, piece, strlen(piece));
            n += (int)strlen(piece);
            b = nm_unit(rng) > 0.5f;
        }
        w[n] = 0;
        if (n <= EC_NAME - 1)
            break;
    }
    nm_finish(w, out, cap);
}

void ec_name_roll(char *out, int cap, uint32_t *rng, const char *avoid)
{
    int tries;
    for (tries = 0; tries < 6; tries++) {
        const uint32_t r = xs(rng) % 10u;
        if (r < 6)
            ec_e5_random_name(out, cap, rng);
        else if (r < 8)
            ec_name_phonic(out, cap, rng);
        else
            ec_name_word(out, cap, rng);
        if (out[0] && (!avoid || strcmp(out, avoid)))
            return;
    }
}

/* ------------------------------------------------------------------------------------------- undo --- */
void ec_undo_clear(ec_undo_t *u) { memset(u, 0, sizeof *u); }

ec_undo_entry_t *ec_undo_record(ec_undo_t *u, uint32_t key, uint32_t ms)
{
    ec_undo_entry_t *e;
    if (u->last_ok && u->last_key == key && ms - u->last_ms < EC_UNDO_COALESCE_MS) {
        u->last_ms = ms;                         /* the same gesture: keep it alive */
        return 0;
    }
    u->nr = 0;                                   /* a new edit drops what could have been redone */
    if (u->n == EC_UNDO_DEPTH) {
        memmove(&u->e[0], &u->e[1], (EC_UNDO_DEPTH - 1) * sizeof u->e[0]);
        u->n--;
    }
    e = &u->e[u->n++];
    memset(e, 0, sizeof *e);
    e->key = key;
    e->ms = ms;
    u->last_key = key;
    u->last_ms = ms;
    u->last_ok = 1;
    return e;
}

static uint32_t mix(uint32_t a, uint32_t b) { return (a ^ (b + 0x9E3779B9u + (a << 6) + (a >> 2))) * 16777619u; }

void ec_undo_param(ec_undo_t *u, const ec_project_t *p, int t, int id, uint32_t ms)
{
    ec_undo_entry_t *e = ec_undo_record(u, mix(mix(mix(EC_U_PARAM, (uint32_t)p->current), (uint32_t)t), (uint32_t)id), ms);
    if (!e)
        return;
    e->kind = EC_U_PARAM;
    e->track = (int8_t)t;
    e->pat = (int8_t)p->current;
    e->id = (uint8_t)id;
    e->val = p->pat[p->current].tr[t].v[id];
    snprintf(e->label, sizeof e->label, "%s", EC_PARAMS[id].label);
}

void ec_undo_global(ec_undo_t *u, int id, float old, const char *label, uint32_t ms)
{
    ec_undo_entry_t *e = ec_undo_record(u, mix(EC_U_GLOBAL, (uint32_t)id), ms);
    if (!e)
        return;
    e->kind = EC_U_GLOBAL;
    e->id = (uint8_t)id;
    e->val = old;
    snprintf(e->label, sizeof e->label, "%s", label);
}

void ec_undo_xfade(ec_undo_t *u, const ec_project_t *p, uint32_t ms)
{
    ec_undo_entry_t *e = ec_undo_record(u, EC_U_XFADE, ms);
    if (!e)
        return;
    e->kind = EC_U_XFADE;
    e->val = p->e5.sc.pos;
    snprintf(e->label, sizeof e->label, "XFADE");
}

void ec_undo_params(ec_undo_t *u, const ec_project_t *p, int t, const uint8_t *ids, int n, const char *label,
                    uint32_t ms)
{
    ec_undo_entry_t *e;
    int i;
    if (n <= 0)
        return;
    u->last_ok = 0;                            /* (a pick is a gesture of its own: never coalesced) */
    e = ec_undo_record(u, mix(mix(mix(EC_U_PARAMS, (uint32_t)p->current), (uint32_t)t), ids[0]), ms);
    if (!e)
        return;
    e->kind = EC_U_PARAMS;
    e->track = (int8_t)t;
    e->pat = (int8_t)p->current;
    e->nv = (uint8_t)(n < EC_U_NV ? n : EC_U_NV);
    for (i = 0; i < e->nv; i++) {
        e->ids[i] = ids[i];
        e->vals[i] = p->pat[p->current].tr[t].v[ids[i]];
    }
    snprintf(e->label, sizeof e->label, "%s", label);
}

void ec_undo_steps(ec_undo_t *u, const ec_project_t *p, int t, int page, uint16_t mask, const char *label,
                   uint32_t key, uint32_t ms)
{
    ec_undo_entry_t *e;
    int i;
    if (!mask)
        return;
    e = ec_undo_record(u, mix(mix(mix(mix(EC_U_STEPS, key), (uint32_t)p->current), (uint32_t)t), (uint32_t)page), ms);
    if (!e)
        return;
    e->kind = EC_U_STEPS;
    e->track = (int8_t)t;
    e->pat = (int8_t)p->current;
    e->id = (uint8_t)page;
    e->mask = mask;
    for (i = 0; i < 16; i++)
        if (mask >> i & 1u)
            e->st[i] = p->pat[p->current].tr[t].steps[page * 16 + i];
    snprintf(e->label, sizeof e->label, "%s", label);
}

void ec_undo_trim(ec_undo_t *u, const ec_project_t *p, int slot, int what, int fresh, uint32_t ms)
{
    ec_undo_entry_t *e;
    if (slot < 0 || slot >= EC_NSLOT)
        return;
    if (fresh)
        u->last_ok = 0;
    e = ec_undo_record(u, mix(mix(EC_U_TRIM, (uint32_t)slot), (uint32_t)what), ms);
    if (!e)
        return;
    e->kind = EC_U_TRIM;
    e->track = -1;
    e->id = (uint8_t)slot;
    e->val = p->slot[slot].trim_start;
    e->vals[0] = p->slot[slot].trim_end;
    e->vals[1] = p->slot[slot].steps;
    e->vals[2] = p->slot[slot].bpm;               /* 0.6.16: AUTO BPM too */
    snprintf(e->label, sizeof e->label, what == 0 ? "TRIM START" : what == 1 ? "TRIM END" : "STEPS");
}

void ec_undo_slots(ec_undo_t *u, int a, int b, uint32_t ms)
{
    ec_undo_entry_t *e;
    u->last_ok = 0;
    e = ec_undo_record(u, mix(mix(EC_U_SLOTS, (uint32_t)a), (uint32_t)b), ms);
    if (!e)
        return;
    e->kind = EC_U_SLOTS;
    e->track = -1;
    e->ids[0] = (uint8_t)a;
    e->ids[1] = (uint8_t)b;
    snprintf(e->label, sizeof e->label, "MOVE");
}

void ec_undo_fxswap(ec_undo_t *u, int track, uint32_t ms)
{
    ec_undo_entry_t *e;
    u->last_ok = 0;
    e = ec_undo_record(u, mix(mix(EC_U_FXSWAP, (uint32_t)(track + 1)), ms), ms);
    if (!e)
        return;
    e->kind = EC_U_FXSWAP;
    e->track = (int8_t)track;
    snprintf(e->label, sizeof e->label, track >= 0 ? "INS SWAP" : "SEND SWAP");
}

void ec_undo_razor(ec_undo_t *u, const ec_project_t *p, int t, const char *label, uint32_t key, uint32_t ms)
{
    ec_undo_entry_t *e;
    if (t < 0 || t >= EC_NTRACK)
        return;
    if (!key)
        u->last_ok = 0;
    e = ec_undo_record(u, mix(mix(mix(EC_U_RAZOR, (uint32_t)t), key ? key : ms), (uint32_t)p->current), ms);
    if (!e)
        return;
    e->kind = EC_U_RAZOR;
    e->track = (int8_t)t;
    e->pat = (int8_t)p->current;
    e->val = p->pat[p->current].tr[t].v[EC_P_SLICE_COUNT];
    e->rz = p->razor[t];
    snprintf(e->label, sizeof e->label, "%s", label);
}
/* apply entry e to the project, leaving in e what it replaced (so applying it again undoes the apply) */
static int undo_swap(ec_undo_t *u, ec_undo_entry_t *e, ec_project_t *p, int *track, int *gid, float *gval, char *label,
                     int cap)
{
    int i;
    if (label)
        snprintf(label, (size_t)cap, "%s", e->label);
    if (track)
        *track = e->track;
    switch (e->kind) {
    case EC_U_PARAM:
        if (e->pat >= 0 && e->pat < EC_NPAT && e->track >= 0 && e->track < EC_NTRACK) {
            float *v = &p->pat[(int)e->pat].tr[(int)e->track].v[e->id];
            const float old = *v;
            *v = e->val;
            e->val = old;
        }
        break;
    case EC_U_PARAMS:
        if (e->pat >= 0 && e->pat < EC_NPAT && e->track >= 0 && e->track < EC_NTRACK)
            for (i = 0; i < e->nv; i++) {
                float *v = &p->pat[(int)e->pat].tr[(int)e->track].v[e->ids[i]];
                const float old = *v;
                *v = e->vals[i];
                e->vals[i] = old;
            }
        break;
    case EC_U_GLOBAL:
        if (gid)
            *gid = e->id;
        if (gval)
            *gval = e->val;
        if (u->gget)
            e->val = u->gget(e->id);
        break;
    case EC_U_XFADE: {
        const float old = p->e5.sc.pos;
        ec_scene_apply(p, e->val);
        e->val = old;
    } break;
    case EC_U_TRIM: {
        ec_slot_t *sl = &p->slot[e->id < EC_NSLOT ? e->id : 0];
        const float s0 = sl->trim_start, s1 = sl->trim_end, st = sl->steps, bp = sl->bpm;
        sl->trim_start = e->val;
        sl->trim_end = e->vals[0];
        sl->steps = e->vals[1];
        if (e->vals[2] > 0)
            sl->bpm = e->vals[2];
        e->val = s0;
        e->vals[0] = s1;
        e->vals[1] = st;
        e->vals[2] = bp;
    } break;
    case EC_U_RAZOR:                               /* 0.6.16: the table and COUNT back */
        if (e->pat >= 0 && e->pat < EC_NPAT && e->track >= 0 && e->track < EC_NTRACK) {
            float *v = &p->pat[(int)e->pat].tr[(int)e->track].v[EC_P_SLICE_COUNT];
            const ec_razor_t old = p->razor[(int)e->track];
            const float oldc = *v;
            p->razor[(int)e->track] = e->rz;
            *v = e->val;
            e->rz = old;
            e->val = oldc;
        }
        break;
    case EC_U_FXSWAP:                              /* 0.6.3: the swap again */
        if (e->track >= 0)
            ec_project_swap_inserts(p, e->track);
        else
            ec_project_swap_sends(p);
        break;
    case EC_U_SLOTS:
        ec_project_swap_slots(p, e->ids[0], e->ids[1]);
        if (gid)
            *gid = e->ids[0];
        if (gval)
            *gval = (float)e->ids[1];
        break;
    case EC_U_STEPS:
        if (e->pat >= 0 && e->pat < EC_NPAT && e->track >= 0 && e->track < EC_NTRACK)
            for (i = 0; i < 16; i++)
                if (e->mask >> i & 1u) {
                    ec_step_t *s = &p->pat[(int)e->pat].tr[(int)e->track].steps[e->id * 16 + i];
                    const ec_step_t old = *s;
                    *s = e->st[i];
                    e->st[i] = old;
                }
        break;
    default: break;
    }
    return e->kind;
}

int ec_undo_pop(ec_undo_t *u, ec_project_t *p, int *track, int *gid, float *gval, char *label, int cap)
{
    ec_undo_entry_t e;
    int kind;
    u->last_ok = 0;                            /* the next edit always starts a fresh gesture */
    if (u->n <= 0)
        return 0;
    e = u->e[--u->n];
    kind = undo_swap(u, &e, p, track, gid, gval, label, cap);
    if (u->nr == EC_UNDO_DEPTH) {
        memmove(&u->r[0], &u->r[1], (EC_UNDO_DEPTH - 1) * sizeof u->r[0]);
        u->nr--;
    }
    u->r[u->nr++] = e;                         /* e now holds what the undo replaced */
    return kind;
}

int ec_undo_redo(ec_undo_t *u, ec_project_t *p, int *track, int *gid, float *gval, char *label, int cap)
{
    ec_undo_entry_t e;
    int kind;
    u->last_ok = 0;
    if (u->nr <= 0)
        return 0;
    e = u->r[--u->nr];
    kind = undo_swap(u, &e, p, track, gid, gval, label, cap);
    if (u->n == EC_UNDO_DEPTH) {
        memmove(&u->e[0], &u->e[1], (EC_UNDO_DEPTH - 1) * sizeof u->e[0]);
        u->n--;
    }
    u->e[u->n++] = e;
    return kind;
}

/* ------------------------------------------------------------------------------------------- memorize / recall --- */
void ec_e5_recall(ec_project_t *dst, const ec_project_t *memo)
{
    *dst = *memo;
    ec_scene_apply(dst, dst->e5.sc.pos);       /* the scene-locked params land where they were memorized */
}

int32_t ec_e5_memo_save(const ec_project_t *p, uint8_t *buf, uint32_t cap) { return ec_project_save(p, buf, cap); }

int ec_e5_memo_recall(ec_project_t *dst, const uint8_t *memo, uint32_t n)
{
    if (!memo || n < 16 || ec_project_load(dst, memo, n) != 0)   /* (a known record whose checksum holds) */
        return -1;
    ec_scene_apply(dst, dst->e5.sc.pos);
    return 0;
}
