/* SPDX-License-Identifier: GPL-3.0-only */
/* The app (ec_app.h): ec_plat.h's ec_app_* entry points. Input -> the project / the sequencer, the sample pool's
 * loading over the file API, the project in the flash record, the LEDs, the screens. docs/FM1-CONTROLS.md is the
 * player's reference for what each control does here. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ec_app.h"
#include "ec_browse.h"
#include "ec_fs.h"
#include "ec_gfx.h"
#include "ec_leds.h"
#include "ec_loader.h"
#include "ec_razor_ui.h"   /* E3 */
#include "ec_scene.h"        /* E5 (bus rows: the knob takes over) */
#include "ec_screens.h"
#include "ec_version.h"

ec_app_t ec_app;
#define A (&ec_app)

#define TAP_MS 250            /* STEP_HOLD_SECONDS / PATTERN_TAP_SECONDS */
#define SEL_TAP_MS 400
#define AUTOSAVE_MS 2000

static uint8_t g_dsp_mem[EC_DSP_MEM_MAX] __attribute__((aligned(16)));   /* the engine (ec_dsp.c asserts it fits) */
static float g_buf[1024 * 2];

/* ------------------------------------------------------------------------------------------- small helpers --- */
static int clampi(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }
static int iv(const float *v, int id) { return (int)floorf(v[id] + 0.5f); }
static int is_slice_m(int m) { return m >= EC_MACH_GRID_SLICE && m <= EC_MACH_RAZOR_POLY; }
static int popcount32(uint32_t x)
{
    int n = 0;
    while (x) {
        x &= x - 1;
        n++;
    }
    return n;
}

static void message(const char *s)
{
    snprintf(A->msg, sizeof A->msg, "%s", s);
    A->msg_ms = A->ms ? A->ms : 1;
    A->redraw = 1;
}

static void mark_dirty(void)
{
    A->dirty = 1;
    A->dirty_ms = A->ms;
    A->redraw = 1;
}

float *ec_app_params(ec_app_t *a) { return ec_project_track(&a->proj, a->track)->v; }
int ec_app_track_slot(const ec_app_t *a, int t)
{
    return clampi(iv(ec_project_track((ec_project_t *)&a->proj, t)->v, EC_P_SLOT), 1, EC_NSLOT) - 1;
}
int ec_app_stopped(const ec_app_t *a) { return !a->seq.playing; }

const ec_ss_sample_t *ec_app_track_sample(const ec_app_t *a, int t)
{
    const ec_slotrt_t *r = &a->slot[ec_app_track_slot(a, t)];
    return r->handle >= 0 ? ec_ss_ready(r->handle) : 0;
}

int ec_app_held_step(const ec_app_t *a)
{
    int i;
    for (i = 0; i < EC_NLOWER; i++)
        if (a->step_held >> i & 1u)
            return a->step_page * 16 + i;
    return -1;
}

static ec_step_t *step_at(int index) { return &ec_project_track(&A->proj, A->track)->steps[index]; }

/* ------------------------------------------------------------------------------------------- items --- */
void ec_app_page(const ec_app_t *a, ec_page_t *out)
{
    const float *v = ec_project_track((ec_project_t *)&a->proj, a->track)->v;
    if (a->settings && a->bus)                                     /* E4: the bus's setup (its machine) */
        if (ec_settings_items_bus(a->cat, a->bus, out))
            return;
    if (a->settings && !a->bus && ec_settings_items(a->cat, v, out))
        return;
    ec_page_items_bus(a->cat, a->page[a->cat], v, a->bus, a->bus ? a->proj.bus[a->bus - 1] : 0, out);
}

/* E4: a bus pseudo-track's FX params live in its record (project.bus), not in the selected track */
float *ec_app_bus_params(ec_app_t *a, int id)
{
    return a->bus && (EC_PARAMS[id].flags & EC_PF_FX) ? a->proj.bus[a->bus - 1] : 0;
}

const char *ec_app_item_label(const ec_item_t *it)
{
    if (it->label)
        return it->label;
    if (it->kind == EC_IT_PARAM)
        return EC_PARAMS[it->id].label;
    return "";
}

static float slot_seconds(const ec_app_t *a, int s)
{
    const ec_slotrt_t *r = &a->slot[s];
    const ec_ss_sample_t *x = r->handle >= 0 ? ec_ss_ready(r->handle) : 0;
    return x && x->rate ? (float)x->frames / (float)x->rate : 0.0f;
}

float ec_app_item_value(const ec_app_t *a, const ec_item_t *it, int *locked)
{
    const ec_project_t *p = &a->proj;
    if (locked)
        *locked = 0;
    {
        float x;                                   /* E5: a held scene anchor / slice pad shows its value */
        if (ec_e5_item_value(it, &x, locked))
            return x;
    }
    switch (it->kind) {
    case EC_IT_PARAM: {
        const ec_ptrack_t *tr = &p->pat[p->current].tr[a->track];
        const int hs = ec_app_held_step(a);
        float x;
        if (ec_app_bus_params((ec_app_t *)a, it->id))
            return ec_app_bus_params((ec_app_t *)a, it->id)[it->id];
        if (hs >= 0 && !a->settings && (EC_PARAMS[it->id].flags & EC_PF_LOCKABLE) &&
            ec_step_lock_get(&tr->steps[hs], it->id, &x)) {
            if (locked)
                *locked = 1;
            return x;
        }
        return tr->v[it->id];
    }
    case EC_IT_GLOBAL:
        switch (it->id) {
        case EC_G_BPM: return p->bpm;
        case EC_G_SWING: return p->swing;
        case EC_G_GLEN: return (float)p->glen;
        case EC_G_QUANT: return (float)p->quant;
        default: return (float)p->rtn;
        }
    case EC_IT_SLOT: {
        const int s = ec_app_track_slot(a, a->track);
        const ec_slot_t *sl = &p->slot[s];
        switch (it->id) {
        case EC_S_BPM: return sl->bpm;
        case EC_S_STEPS: return sl->steps;
        case EC_S_GAIN: return sl->gain;
        case EC_S_TRIM_START: return sl->trim_start;
        default: return sl->trim_end;
        }
    }
    case EC_IT_RAZOR: return ec_rzui_value(a, it->id);   /* E3 */
    default: return 0;
    }
}

void ec_app_item_format(const ec_app_t *a, const ec_item_t *it, float x, char *out, int cap)
{
    static const char *const Q[] = {"SEQ", "JUMP", "START", "TEMP"};
    static const char *const RT[] = {"RETURN", "BOOMR", "RESET"};
    char b[24];
    if (it->kind == EC_IT_BLANK) {
        out[0] = 0;
        return;
    }
    if (it->kind == EC_IT_RAZOR) {   /* E3 */
        ec_rzui_format(a, it->id, x, out, cap);
        return;
    }
    if (it->kind == EC_IT_E5) {                    /* E5 */
        ec_e5_item_format(it, x, out, cap);
        return;
    }
    if (it->kind == EC_IT_PARAM) {
        ec_param_format(it->id, x, iv(ec_project_track((ec_project_t *)&a->proj, a->track)->v, EC_P_ENV_RANGE), out,
                        cap);
        return;
    }
    if (it->kind == EC_IT_GLOBAL) {
        switch (it->id) {
        case EC_G_BPM:
            if (fabsf(x - floorf(x + 0.5f)) < 0.05f)
                snprintf(out, (size_t)cap, "%d", (int)floorf(x + 0.5f));
            else
                snprintf(out, (size_t)cap, "%.1f", (double)x);
            return;
        case EC_G_SWING: snprintf(out, (size_t)cap, "%d%%", (int)floorf(x + 0.5f)); return;
        case EC_G_GLEN: snprintf(out, (size_t)cap, "%d", (int)x); return;
        case EC_G_QUANT: snprintf(out, (size_t)cap, "%s", Q[clampi((int)x, 1, 4) - 1]); return;
        default: snprintf(out, (size_t)cap, "%s", RT[clampi((int)x, 1, 3) - 1]); return;
        }
    }
    switch (it->id) {
    case EC_S_BPM:
        snprintf(b, sizeof b, "%.1f", (double)x);
        if (b[strlen(b) - 1] == '0')
            b[strlen(b) - 2] = 0;
        snprintf(out, (size_t)cap, "%s", b);
        return;
    case EC_S_STEPS: snprintf(out, (size_t)cap, "%d", (int)floorf(x + 0.5f)); return;
    case EC_S_GAIN: snprintf(out, (size_t)cap, "%.2fX", (double)x); return;
    default: {
        const float sec = x * slot_seconds(a, ec_app_track_slot(a, a->track));
        snprintf(out, (size_t)cap, "%.2fS", (double)sec);
    }
    }
}

float ec_app_item_frac(const ec_app_t *a, const ec_item_t *it, float x)
{
    (void)a;
    switch (it->kind) {
    case EC_IT_E5: return ec_e5_item_frac(it, x);  /* E5 */
    case EC_IT_PARAM: return ec_param_frac(it->id, x);
    case EC_IT_GLOBAL:
        switch (it->id) {
        case EC_G_BPM: return (x - 20) / 280;
        case EC_G_SWING: return (x - 50) / 25;
        case EC_G_GLEN: return (x - 1) / 255;
        case EC_G_QUANT: return (x - 1) / 3;
        default: return (x - 1) / 2;
        }
    case EC_IT_SLOT:
        switch (it->id) {
        case EC_S_BPM: return (x - 20) / 280;
        case EC_S_STEPS: return (x - 1) / 511;
        case EC_S_GAIN: return x / 4;
        default: return x;
        }
    case EC_IT_RAZOR: return ec_rzui_frac(a, it->id, x);   /* E3 */
    default: return 0;
    }
}

/* ------------------------------------------------------------------------------------------- the engine sink --- */
static void sink_event(void *ctx, const ec_event_t *e)
{
    (void)ctx;
    ec_dsp_event(A->dsp, e);
}
static void sink_params(void *ctx, int t, const ec_track_params_t *p)
{
    (void)ctx;
    ec_dsp_track_params(A->dsp, t, p);
}
static void sink_process(void *ctx, float *out, int n)
{
    (void)ctx;
    ec_dsp_process(A->dsp, out, n);
}

/* ------------------------------------------------------------------------------------------- samples --- */
static int make_builtin(void)
{
    const uint32_t n = 88200, beat = n / 4, eighth = n / 8;
    const int h = ec_ss_alloc(n);
    ec_ss_sample_t *s = ec_ss_get(h);
    uint32_t i, rnd = 0x1234567u;
    if (!s)
        return -1;
    for (i = 0; i < n; i++) {
        const uint32_t tb = i % beat, te = i % eighth, b = i / beat;
        const float t = (float)tb / EC_FS;
        float v = 0, noise;
        rnd = rnd * 1664525u + 1013904223u;
        noise = (float)(int32_t)rnd / 2147483648.0f;
        v += 0.9f * sinf(6.2831853f * (45.0f * t + 60.0f * (1.0f - expf(-t * 30.0f)) / 30.0f)) * expf(-t * 9.0f);
        if (b == 1 || b == 3)
            v += 0.5f * noise * expf(-t * 22.0f) + 0.3f * sinf(6.2831853f * 190.0f * t) * expf(-t * 25.0f);
        v += 0.18f * noise * expf(-(float)te / EC_FS * 90.0f);
        v *= 0.7f;
        {
            const int x = (int)(v * 32767.0f);
            const int16_t q = (int16_t)(x > 32767 ? 32767 : x < -32768 ? -32768 : x);
            const uint32_t col = (uint32_t)((uint64_t)i * EC_SS_OVERVIEW / n);
            const int a = q < 0 ? -q : q;
            s->data[2 * i] = s->data[2 * i + 1] = q;
            if ((a >> 7) > s->overview[col])
                s->overview[col] = (uint8_t)((a >> 7) > 255 ? 255 : (a >> 7));
        }
    }
    s->frames_ready = n;
    s->rate = EC_FS;
    s->channels = 1;
    s->bits = 16;
    s->bpm = 120;
    snprintf(s->name, sizeof s->name, "BUILTIN LOOP");
    snprintf(s->path, sizeof s->path, "builtin:loop");
    s->ready = 1;
    return h;
}

/* give every engine track the sample of its slot (when it changed) */
static void assign_samples(void)
{
    int t;
    for (t = 0; t < EC_NTRACK; t++) {
        const ec_slotrt_t *r = &A->slot[ec_app_track_slot(A, t)];
        const int h = r->state == EC_SL_READY ? r->handle : -1;
        if (h != A->assigned[t]) {
            const ec_ss_sample_t *x = h >= 0 ? ec_ss_ready(h) : 0;
            if (x) {
                ec_sample_t es;
                es.data = x->data;
                es.frames = x->frames;
                es.rate = (float)x->rate;
                es.channels = x->channels;
                es.bpm = x->bpm;
                ec_dsp_set_sample(A->dsp, t, &es);
            } else
                ec_dsp_set_sample(A->dsp, t, 0);
            A->assigned[t] = h;
            ec_seq_touch(&A->seq, t);
            A->redraw = 1;
        }
    }
}

static void queue_slot(int s, const char *path, int newmeta)
{
    ec_slotrt_t *r = &A->slot[s];
    if (A->load_slot == s) {
        ec_load_cancel();
        A->load_slot = -1;
    }
    if (path)
        snprintf(A->proj.slot[s].path, EC_PATH, "%s", path);
    r->old_handle = r->state == EC_SL_READY ? r->handle : -1;
    r->state = EC_SL_QUEUED;
    r->newmeta = newmeta;
    r->builtin = 0;
    r->err[0] = 0;
    A->redraw = 1;
}

static void repack(void)
{
    int t, s;
    message("REPACKING SAMPLES");
    for (t = 0; t < EC_NTRACK; t++) {
        ec_dsp_set_sample(A->dsp, t, 0);
        A->assigned[t] = -1;
    }
    ec_load_cancel();
    ec_ss_reset();
    A->load_slot = -1;
    A->repack = 1;
    for (s = 0; s < EC_NSLOT; s++) {
        ec_slotrt_t *r = &A->slot[s];
        const int nm = r->state == EC_SL_QUEUED || r->state == EC_SL_LOADING ? r->newmeta : 0;
        r->handle = -1;
        r->old_handle = -1;
        if (A->proj.slot[s].path[0]) {
            r->state = EC_SL_QUEUED;
            r->newmeta = nm;
        } else
            r->state = EC_SL_EMPTY;
    }
}

static int handle_in_use(int h, int except)
{
    int s;
    for (s = 0; s < EC_NSLOT; s++)
        if (s != except && A->slot[s].handle == h)
            return 1;
    return 0;
}

static void slot_loaded(int s, int h)
{
    ec_slotrt_t *r = &A->slot[s];
    ec_slot_t *sl = &A->proj.slot[s];
    const ec_ss_sample_t *x = ec_ss_ready(h);
    const int old = r->old_handle;
    r->handle = h;
    r->state = EC_SL_READY;
    r->old_handle = -1;
    if (r->newmeta && x) {
        const float dur = x->rate ? (float)x->frames / (float)x->rate : 0;
        float st;
        sl->bpm = x->bpm > 0 ? x->bpm : 120.0f;
        sl->trim_start = 0;
        sl->trim_end = 1;
        sl->gain = 1;
        st = floorf(dur * sl->bpm / 60.0f * 4.0f + 0.5f);
        sl->steps = st < 1 ? 1 : st > 512 ? 512 : st;
        r->newmeta = 0;
        mark_dirty();
    }
    assign_samples();
    if (old >= 0 && old != h && !handle_in_use(old, -1))
        ec_ss_free(old);
    ec_seq_touch_all(&A->seq);
    A->redraw = 1;
}

static void loader_tick(void)
{
    const int st = ec_load_state();
    if (A->load_slot >= 0 && (st == EC_LD_STAT || st == EC_LD_HEADER || st == EC_LD_DATA)) {
        const int r = ec_load_step();
        ec_slotrt_t *rt = &A->slot[A->load_slot];
        if (r == EC_LD_DONE) {
            const int s = A->load_slot;
            A->load_slot = -1;
            slot_loaded(s, ec_load_handle());
        } else if (r == EC_LD_ERROR) {
            const char *why = ec_load_error();
            const int s = A->load_slot;
            A->load_slot = -1;
            if (!strcmp(why, "too big") && !A->repack) {
                repack();
                return;
            }
            rt = &A->slot[s];
            snprintf(rt->err, sizeof rt->err, "%s", why);
            rt->state = !strcmp(why, "not found") ? EC_SL_MISSING : EC_SL_ERROR;
            if (rt->old_handle >= 0) {        /* (a failed replacement keeps playing the old sample) */
                rt->handle = rt->old_handle;
                rt->state = EC_SL_READY;
                rt->old_handle = -1;
                message(!strcmp(why, "not found") ? "MISSING" : why);
            }
            A->redraw = 1;
        }
        A->redraw = 1;
        return;
    }
    if (A->load_slot < 0) {
        int s;
        for (s = 0; s < EC_NSLOT; s++)
            if (A->slot[s].state == EC_SL_QUEUED) {
                A->slot[s].state = EC_SL_LOADING;
                A->load_slot = s;
                if (ec_load_start(A->proj.slot[s].path) != 0) {
                    A->slot[s].state = EC_SL_ERROR;
                    snprintf(A->slot[s].err, sizeof A->slot[s].err, "%s", ec_load_error());
                    A->load_slot = -1;
                }
                A->redraw = 1;
                return;
            }
        A->repack = 0;
    }
}

static void scan(void)
{
    A->fs_changed = ec_fs_changed();
    ec_browse_start("/audio");
    A->scanning = 1;
}

static void scan_tick(void)
{
    if (!A->scanning && ec_fs_changed() != A->fs_changed)
        scan();
    if (A->scanning) {
        const int st = ec_browse_step();
        if (st == EC_BR_DONE || st == EC_BR_ERROR) {
            A->scanning = 0;
            A->no_folder = st == EC_BR_ERROR;
            if (A->cursor >= ec_browse_count())
                A->cursor = 0;
            /* a new project: the first file on T1, else the built-in loop. The built-in loop is a stand-in: a folder
             * picked after the boot (the web player's first visit) replaces it with the first file. */
            if (A->fresh || (A->slot[0].builtin && !A->proj.slot[0].path[0] && ec_browse_count() > 0)) {
                const int fresh = A->fresh;
                A->fresh = 0;
                if (ec_browse_count() > 0)
                    queue_slot(0, ec_browse_path(0), 1);
                else if (fresh) {
                    const int h = make_builtin();
                    if (h >= 0) {
                        A->slot[0].handle = h;
                        A->slot[0].state = EC_SL_READY;
                        A->slot[0].builtin = 1;
                        A->proj.slot[0].bpm = 120;
                        A->proj.slot[0].steps = 8;
                        assign_samples();
                    }
                }
            }
            A->redraw = 1;
        }
    }
}

/* ------------------------------------------------------------------------------------------- save --- */
static void save_project(int quiet)
{
    uint32_t size;
    uint8_t *rec = ec_plat_record(&size);
    const int32_t n = ec_project_save(&A->proj, rec, size);
    if (n > 0) {
        ec_plat_record_changed();
        A->dirty = 0;
        A->saves++;
        if (!quiet)
            message("SAVED");
    } else
        message("SAVE FULL");
}

/* ------------------------------------------------------------------------------------------- edits --- */
static int accel(int r)
{
    const uint32_t gap = A->ms - A->knob_ms[r];
    A->knob_ms[r] = A->ms;
    return gap < 30 ? 4 : 1;
}

static float nudge_machine(float m, int det)
{
    /* E3: every machine but INPUT (E6); NEIGHBOR from track 2 on (track 1 has no left neighbour) */
    const int top = A->track > 0 ? EC_MACH_NEIGHBOR : EC_MACH_RAZOR_POLY;
    return (float)clampi((int)(m + 0.5f) + det, EC_MACH_LOOP, top);
}

static void edit_param(int id, int det, int mult, int flags)
{
    float *v = ec_app_params(A);
    const ec_param_def_t *d = &EC_PARAMS[id];
    const int hs = ec_app_held_step(A);
    const int n = d->kind == EC_PK_NUM ? det * mult : det;
    float *bv = ec_app_bus_params(A, id);
    if (bv) {                                       /* E4: a bus FX param (global; no p-locks: no bus lanes) */
        bv[id] = ec_param_nudge(id, bv[id], n);
        ec_scene_knob(&A->proj, EC_NTRACK + A->bus - 1, id);   /* the knob takes the bus param back from the fader */
        mark_dirty();
        return;
    }
    if (A->bus)
        return;
    if (hs >= 0 && !A->settings && (d->flags & EC_PF_LOCKABLE)) {   /* hold a step + turn = p-lock it */
        int i;
        ec_e5_edit_param(A->track, id, 1);                           /* E5: undo */
        for (i = 0; i < EC_NLOWER; i++)
            if (A->step_held >> i & 1u) {
                ec_step_t *s = step_at(A->step_page * 16 + i);
                float x;
                if (!ec_step_lock_get(s, id, &x))
                    x = v[id];
                if (ec_step_lock_set(s, id, ec_param_nudge(id, x, n)) < 0)
                    message("LOCKS FULL");
                s->trig = 1;
            }
        A->step_edited |= A->step_held;
        ec_seq_touch(&A->seq, A->track);
        mark_dirty();
        return;
    }
    if ((flags & EC_IF_STOPPED) && A->seq.playing) {
        message("STOP FIRST");
        return;
    }
    ec_e5_edit_param(A->track, id, 0);           /* E5: undo; the knob takes the param back from the fader */
    if (id == EC_P_MACHINE)
        v[id] = nudge_machine(v[id], det);
    else
        v[id] = ec_param_nudge(id, v[id], n);
    if (id == EC_P_RATE)
        ec_seq_timing_changed(&A->seq);
    ec_seq_touch(&A->seq, A->track);
    mark_dirty();
}

static void edit_global(int id, int det, int mult)
{
    ec_project_t *p = &A->proj;
    ec_e5_edit_global(id, id == EC_G_BPM ? p->bpm : id == EC_G_SWING ? p->swing : id == EC_G_GLEN ? (float)p->glen
                          : id == EC_G_QUANT ? (float)p->quant : (float)p->rtn);   /* E5: undo */
    switch (id) {
    case EC_G_BPM: ec_seq_set_bpm(&A->seq, floorf(p->bpm + 0.5f) + (float)(det * mult)); break;
    case EC_G_SWING:
        p->swing = (float)clampi((int)p->swing + det, 50, 75);
        ec_seq_timing_changed(&A->seq);
        break;
    case EC_G_GLEN:
        p->glen = clampi(p->glen + det * mult, 1, 256);
        ec_seq_timing_changed(&A->seq);
        break;
    case EC_G_QUANT: p->quant = clampi(p->quant + det, 1, 4); break;
    default:
        p->rtn = clampi(p->rtn + det, 1, 3);
        ec_seq_touch_all(&A->seq);
        break;
    }
    mark_dirty();
}

static void edit_slot(int id, int det, int mult)
{
    const int s = ec_app_track_slot(A, A->track);
    ec_slot_t *sl = &A->proj.slot[s];
    const float dur = slot_seconds(A, s);
    switch (id) {
    case EC_S_BPM: {
        float st;
        sl->bpm = floorf(sl->bpm + 0.5f) + (float)(det * mult);
        sl->bpm = sl->bpm < 20 ? 20 : sl->bpm > 300 ? 300 : sl->bpm;
        if (dur > 0) {                          /* (RECALC: the trim's steps at this BPM) */
            st = floorf(dur * (sl->trim_end - sl->trim_start) * sl->bpm / 60.0f * 4.0f + 0.5f);
            sl->steps = st < 1 ? 1 : st > 512 ? 512 : st;
        }
    } break;
    case EC_S_STEPS: sl->steps = (float)clampi((int)(sl->steps + 0.5f) + det * mult, 1, 512); break;
    case EC_S_GAIN:
        sl->gain += 0.05f * (float)det;
        sl->gain = sl->gain < 0 ? 0 : sl->gain > 4 ? 4 : sl->gain;
        break;
    case EC_S_TRIM_START:
        sl->trim_start += 0.005f * (float)(det * mult);
        sl->trim_start = sl->trim_start < 0 ? 0 : sl->trim_start > sl->trim_end - 0.005f ? sl->trim_end - 0.005f
                                                                                           : sl->trim_start;
        break;
    default:
        sl->trim_end += 0.005f * (float)(det * mult);
        sl->trim_end = sl->trim_end > 1 ? 1 : sl->trim_end < sl->trim_start + 0.005f ? sl->trim_start + 0.005f
                                                                                       : sl->trim_end;
        break;
    }
    ec_seq_touch_all(&A->seq);
    mark_dirty();
}

static void knob_cell(int k, int det)
{
    ec_page_t pg;
    const int cell = (A->fn ? 4 : 0) + k;
    const int mult = accel(EC_E_K1 + k);
    ec_app_page(A, &pg);
    if (cell >= pg.n)
        return;
    if (ec_e5_knob_cell(&pg.it[cell], det, mult))     /* E5: scene / slice edits, E5 cells */
        return;
    switch (pg.it[cell].kind) {
    case EC_IT_PARAM: edit_param(pg.it[cell].id, det, mult, pg.it[cell].flags); break;
    case EC_IT_GLOBAL: edit_global(pg.it[cell].id, det, mult); break;
    case EC_IT_SLOT: edit_slot(pg.it[cell].id, det, mult); break;
    case EC_IT_RAZOR: ec_rzui_edit(A, pg.it[cell].id, det, mult); break;   /* E3 */
    default: break;
    }
    A->redraw = 1;
}

/* ------------------------------------------------------------------------------------------- categories --- */
static int has_settings(int cat)
{
    ec_page_t pg;
    if (A->bus && ec_settings_items_bus(cat, A->bus, &pg))     /* E4 */
        return 1;
    return ec_settings_items(cat, ec_app_params(A), &pg);
}

static void load_cursor_file(void)
{
    const int s = ec_app_track_slot(A, A->track);
    if (ec_browse_count() <= 0) {
        message(A->no_folder ? "NO FOLDER" : "NO FILES");
        return;
    }
    queue_slot(s, ec_browse_path(A->cursor), 1);
    mark_dirty();
}

static void category(int c)
{
    if (A->settings) {
        if (A->fn) {                                   /* FN + category in settings: back to its main page */
            A->settings = 0;
            A->cat = c;
        } else {
            A->cat = c;
            A->settings = has_settings(c);
        }
    } else if (A->fn && has_settings(c)) {
        A->settings = 1;
        A->cat = c;
    } else if (c == EC_CAT_FILE && A->cat == EC_CAT_FILE && A->page[c] == 0 && ec_browse_count() > 0 &&
               strcmp(A->proj.slot[ec_app_track_slot(A, A->track)].path, ec_browse_path(A->cursor)) != 0)
        load_cursor_file();                            /* SAVE on SAMPLE: load the file under the cursor ... */
    else if (c == EC_CAT_FILE && A->cat == EC_CAT_FILE)
        A->page[c] ^= 1;                               /* ... already the track's file: next page, RAZOR (E3); back */
    else if (c == A->cat)
        A->page[c] = (A->page[c] + 1) % ec_cat_pages(c, ec_app_params(A));
    else
        A->cat = c;
    A->redraw = 1;
}

static void select_target(int i)      /* 0..5 tracks, 6 S1, 7 S2, 8 MST */
{
    if (i < EC_NTRACK) {
        A->track = i;
        A->bus = 0;
    } else
        A->bus = i - EC_NTRACK + 1;
    A->redraw = 1;
}

/* ------------------------------------------------------------------------------------------- keys --- */
enum { R_NONE, R_STEP, R_LOOP_LIVE, R_LOOP_PROG, R_PAD, R_SLICE_PROG, R_KEY, R_KEY_PROG, R_PAT, R_RAZOR /* E3 */ };

static int loop_mode(void) { return A->sel_held || A->keymode == EC_KM_LOOP; }

static float key_semis(int i)
{
    static const int W[7] = {0, 2, 4, 5, 7, 9, 11};
    return (float)(W[i % 7] + 12 * (i / 7) - 12);
}

/* loop keys held while a step is held: that step's region lock (GridSequencer:lock_held_steps + tap cycle) */
static void program_region(void)
{
    const float *v = ec_app_params(A);
    const int div = clampi((iv(v, EC_P_LOOP_DIVISION) + 1) / 2 * 2, 2, 32);
    const float units = 128.0f / (float)div;
    const int count = popcount32(A->prog_keys);
    int first = -1, last = -1, k, i, start_only = 0;
    float s, e;
    for (k = 0; k < 32; k++)
        if (A->prog_keys >> k & 1u) {
            if (first < 0)
                first = k;
            last = k;
        }
    if (first < 0)
        return;
    s = (float)first * units;
    s = s > 127 ? 127 : s;
    e = (float)(last + 1) * units;
    e = e > 128 ? 128 : e;
    if (count > 1)
        A->tap_key = -1;
    else if (A->tap_key == first && A->tap_kind == 0) {
        A->tap_kind = 1;
        start_only = 1;
    } else if (A->tap_key == first && A->tap_kind == 1)
        A->tap_kind = 0;
    else {
        A->tap_key = first;
        A->tap_kind = 0;
    }
    if (iv(v, EC_P_MACHINE) != EC_MACH_LOOP)
        start_only = 0;
    ec_e5_steps_edit(A->step_held, "REGION", 0x600u);  /* E5: undo */
    for (i = 0; i < EC_NLOWER; i++)
        if (A->step_held >> i & 1u) {
            ec_step_t *st = step_at(A->step_page * 16 + i);
            st->trig = 1;
            ec_step_lock_set(st, EC_P_LOOP_START, s);
            if (start_only)
                ec_step_lock_clear(st, EC_P_LOOP_END);
            else
                ec_step_lock_set(st, EC_P_LOOP_END, e);
        }
    A->step_edited |= A->step_held;
    message(start_only ? "STEP LOCK START" : "STEP LOCK");
    mark_dirty();
}

static void lower_down(int k, int i, int vel)
{
    const float *v = ec_app_params(A);
    const int m = iv(v, EC_P_MACHINE);
    const float fv = (float)vel / 127.0f;
    if (ec_rzui_active(A) && !A->sel_held) {         /* E3: the RAZOR page (page-local): the keys are its slices */
        ec_rzui_key(A, i, 1, vel);
        A->key_role[k] = R_RAZOR;
        return;
    }
    if (ec_e5_lower_down(k, i, vel))               /* E5: naming, PAGE, the scene row, FN + pad = slice mute */
        return;
    if (A->patn_open) {
        ec_seq_request_pattern(&A->seq, i);
        A->patn_used = 1;
        if (A->patn_latched)
            A->patn_open = A->patn_latched = 0;
        A->key_role[k] = R_PAT;
        mark_dirty();
        return;
    }
    if (loop_mode()) {
        const int lk = ec_e5_loop_key(k, i, 1);    /* E5: 1..32 (FN = keys / pads 17-32 when the track has them) */
        if (A->sel_held)
            A->sel_used = 1;
        if (is_slice_m(m)) {
            if (A->step_held) {
                int j;
                ec_e5_steps_edit(A->step_held, "SLICE", 0x700u);   /* E5: undo */
                for (j = 0; j < EC_NLOWER; j++)
                    if (A->step_held >> j & 1u)
                        ec_step_toggle_slice(ec_project_track(&A->proj, A->track), A->step_page * 16 + j, lk);
                A->step_edited |= A->step_held;
                A->key_role[k] = R_SLICE_PROG;
                mark_dirty();
            } else {
                ec_seq_slice_pad(&A->seq, A->track, lk, 1, fv);
                A->key_role[k] = R_PAD;
            }
        } else if (A->step_held) {
            A->prog_keys |= 1u << (lk - 1);
            A->key_role[k] = R_LOOP_PROG;
            program_region();
        } else {
            ec_seq_loop_key(&A->seq, A->track, lk, 1);
            A->key_role[k] = R_LOOP_LIVE;
        }
        A->redraw = 1;
        return;
    }
    if (A->keymode == EC_KM_KEYS) {
        if (A->step_held) {
            int j;
            ec_e5_steps_edit(A->step_held, "PITCH", 0x800u);       /* E5: undo */
            for (j = 0; j < EC_NLOWER; j++)
                if (A->step_held >> j & 1u) {
                    ec_step_t *st = step_at(A->step_page * 16 + j);
                    st->trig = 1;
                    ec_step_lock_set(st, EC_P_PITCH, key_semis(i));
                }
            A->step_edited |= A->step_held;
            A->key_role[k] = R_KEY_PROG;
            mark_dirty();
        } else {
            ec_seq_keyboard(&A->seq, A->track, key_semis(i), 1, fv);
            A->key_role[k] = R_KEY;
        }
        A->redraw = 1;
        return;
    }
    A->key_role[k] = R_STEP;
    A->step_held |= (uint16_t)(1u << i);
    A->step_edited &= (uint16_t)~(1u << i);
    A->step_ms[i] = A->ms;
    A->tap_key = -1;
    A->redraw = 1;
    ec_e5_step_down(i);                            /* E5: step preview while stopped */
}

static void lower_up(int k, int i, int vel)
{
    if (ec_e5_lower_up(k, i))                      /* E5 roles */
        return;
    switch (A->key_role[k]) {
    case R_STEP: {
        const int quick = A->ms - A->step_ms[i] < TAP_MS;
        const int edited = A->step_edited >> i & 1u;
        A->step_held &= (uint16_t)~(1u << i);
        A->step_edited &= (uint16_t)~(1u << i);
        A->tap_key = -1;
        if (!A->step_held)
            A->prog_keys = 0;
        ec_e5_step_up();                           /* E5: the preview ends with the last step */
        if (quick && !edited) {
            ec_ptrack_t *tr = ec_project_track(&A->proj, A->track);
            const int idx = A->step_page * 16 + i;
            ec_e5_steps_edit((uint16_t)(1u << i), "STEP", 0x900u + (uint32_t)idx);   /* E5: undo */
            if (A->fn)
                ec_step_toggle_ghost(tr, idx);
            else if (ec_step_toggle(tr, idx) && vel != 100)      /* (100: a host without key velocity) */
                ec_step_lock_set(&tr->steps[idx], EC_P_VELOCITY, (float)vel / 127.0f);
            ec_seq_touch(&A->seq, A->track);
            mark_dirty();
        }
    } break;
    case R_LOOP_LIVE: ec_seq_loop_key(&A->seq, A->track, ec_e5_loop_key(k, i, 0), 0); break;
    case R_LOOP_PROG: A->prog_keys &= ~(1u << (ec_e5_loop_key(k, i, 0) - 1)); break;
    case R_PAD: ec_seq_slice_pad(&A->seq, A->track, ec_e5_loop_key(k, i, 0), 0, 0); break;
    case R_KEY: ec_seq_keyboard(&A->seq, A->track, key_semis(i), 0, 0); break;
    case R_RAZOR: ec_rzui_key(A, i, 0, 0); break;   /* E3 */
    default: break;
    }
    A->key_role[k] = R_NONE;
    A->redraw = 1;
}

static void upper_down(int i)
{
    if (ec_e5_upper(i, 1))                         /* E5: PATN held (pattern scope), naming */
        return;
    if (i < EC_NTRACK) {
        if (A->fn) {                                  /* FN + track: mute */
            float *v = ec_project_track(&A->proj, i)->v;
            v[EC_P_MUTE] = v[EC_P_MUTE] > 0.5f ? 0.0f : 1.0f;
            ec_seq_touch(&A->seq, i);
            message(v[EC_P_MUTE] > 0.5f ? "MUTED" : "UNMUTED");
            mark_dirty();
        } else
            select_target(i);
    } else if (i < 9)
        select_target(i);
    else if (i == 9) {                                /* FILL: hold, FN + tap latches */
        if (A->fn)
            A->fill_latched = !A->fill_latched;
        else
            A->fill_held = 1;
        ec_seq_set_fill(&A->seq, A->fill_held, A->fill_latched);
    } else {                                          /* PATN: tap latches the overlay, hold = momentary */
        if (A->patn_latched) {
            A->patn_open = A->patn_latched = 0;
            A->patn_ms = 0;
        } else {
            A->patn_open = 1;
            A->patn_used = 0;
            A->patn_ms = A->ms ? A->ms : 1;
        }
    }
    A->redraw = 1;
}

static void upper_up(int i)
{
    if (ec_e5_upper(i, 0))                         /* E5 */
        return;
    if (i == 9) {
        A->fill_held = 0;
        ec_seq_set_fill(&A->seq, 0, A->fill_latched);
    } else if (i == 10 && A->patn_ms) {
        const int quick = A->ms - A->patn_ms < TAP_MS;
        A->patn_ms = 0;
        if (!A->patn_used && quick && A->patn_open)
            A->patn_latched = 1;
        else
            A->patn_open = 0;
    }
    A->redraw = 1;
}

/* ------------------------------------------------------------------------------------------- buttons --- */
static const int CAT_OF[EC_NB] = {EC_CAT_FX, EC_CAT_SRC, EC_CAT_AMP, EC_CAT_MOD, EC_CAT_FILT, EC_CAT_MASTER, -1,
                                  EC_CAT_FILE, EC_CAT_PAT, EC_CAT_TRIG, -1, -1, -1, -1};

static void button_down(int b)
{
    if ((b == EC_B_OCTDN || b == EC_B_OCTUP) && ec_rzui_active(A)) {
        /* E3, page-local: OCT+ split / OCT- delete; FN: Auto-Chop / Redistribute. E5 never sees the press, so a
         * held OCT here opens no PAGE overlay and its release changes no key mode. */
        ec_rzui_oct(A, b == EC_B_OCTUP);
        A->redraw = 1;
        return;
    }
    if (ec_e5_button_down(b)) {                    /* E5: copy / clear / paste, PAGE, memorize, undo, naming */
        A->redraw = 1;
        return;
    }
    switch (b) {
    case EC_B_HOME: A->fn = 1; break;
    case EC_B_SEL:
        A->sel_held = 1;
        A->sel_used = 0;
        A->sel_fn = A->fn;                           /* (its tap acts on release: FN counts from the press) */
        A->sel_ms = A->ms;
        break;
    case EC_B_PLAY:
        if (A->step_held) {                          /* hold step(s) + PLAY: clear their p-locks (keep the trig) */
            int i;
            for (i = 0; i < EC_NLOWER; i++)
                if (A->step_held >> i & 1u) {
                    ec_step_t *s = step_at(A->step_page * 16 + i);
                    const uint8_t trig = s->trig;
                    const uint32_t sl = s->slices;
                    ec_step_clear(s);
                    s->trig = trig;
                    s->slices = sl;
                }
            A->step_edited |= A->step_held;
            message("LOCKS CLEARED");
            ec_seq_touch(&A->seq, A->track);
            mark_dirty();
        } else if (A->fn) {
            ec_seq_stop(&A->seq);
            message("STOP");
            if (A->dirty)
                A->dirty_ms = A->ms;
        } else
            ec_seq_toggle(&A->seq);
        break;
    case EC_B_REC:
        if (A->fn)
            save_project(0);
        break;
    /* OCT- / OCT+: E5 (ec_e5.c) -- a tap changes the key mode on release, a hold is PAGE, FN + OCT memorize / recall;
     * on the RAZOR page they are E3's (button_down, before E5) */
    default:
        if (CAT_OF[b] >= 0)
            category(CAT_OF[b]);
        break;
    }
    A->redraw = 1;
}

static void button_up(int b)
{
    if (ec_e5_button_up(b)) {                      /* E5: OCT taps (key mode), FN tap cancels naming */
        A->redraw = 1;
        return;
    }
    if (b == EC_B_HOME)
        A->fn = 0;
    else if (b == EC_B_SEL) {
        A->sel_held = 0;
        if (!A->sel_used && A->ms - A->sel_ms < SEL_TAP_MS) {
            const int fn = A->fn;
            A->fn = A->sel_fn;
            category(EC_CAT_SRC);
            A->fn = fn;
        }
    }
    A->redraw = 1;
}

/* ------------------------------------------------------------------------------------------- ec_app_* --- */
void ec_app_init(void)
{
    uint32_t size;
    const uint8_t *rec;
    ec_seq_sink_t sink;
    int i;
    memset(A, 0, sizeof *A);
    if (ec_dsp_mem_size() > sizeof g_dsp_mem)   /* (cannot happen: ec_dsp.c checks it at compile time) */
        return;
    A->dsp = ec_dsp_init(g_dsp_mem, (float)EC_FS);
    A->load_slot = -1;
    A->tap_key = -1;
    A->master = 820;
    A->redraw = 1;
    for (i = 0; i < EC_NSLOT; i++)
        A->slot[i].handle = A->slot[i].old_handle = -1;
    for (i = 0; i < EC_NTRACK; i++)
        A->assigned[i] = -1;
    ec_ss_reset();
    rec = ec_plat_record(&size);
    if (ec_project_load(&A->proj, rec, size) == 0) {
        for (i = 0; i < EC_NSLOT; i++)
            if (A->proj.slot[i].path[0])
                A->slot[i].state = EC_SL_QUEUED;
        message("PROJECT LOADED");
    } else {
        ec_project_init(&A->proj);
        A->fresh = 1;
    }
    sink.ctx = 0;
    sink.event = sink_event;
    sink.params = sink_params;
    sink.process = sink_process;
    ec_seq_init(&A->seq, &A->proj, &sink, EC_FS);
    ec_dsp_set_tempo(A->dsp, A->proj.bpm);
    ec_e5_init();                                  /* E5 */
    A->cat = EC_CAT_SRC;
    A->page[EC_CAT_SRC] = 0;
    scan();
}

void ec_app_tick(const ec_input_t *in)
{
    int b, k, r;
    A->ms = in->ms;
    scan_tick();
    loader_tick();
    assign_samples();

    if (in->master_moved) {
        A->master = in->master;
        A->redraw = 1;
    }
    for (b = 0; b < EC_NB; b++)
        if (in->btn_down >> b & 1u)
            button_down(b);
    for (k = 0; k < EC_NKEY; k++) {
        if (in->key_down >> k & 1u) {
            const int lo = ec_key_lower(k), up = ec_key_upper(k);
            if (lo >= 0)
                lower_down(k, lo, in->key_vel[k]);
            else if (up >= 0)
                upper_down(up);
        }
        if (in->key_up >> k & 1u) {
            const int lo = ec_key_lower(k), up = ec_key_upper(k);
            if (lo >= 0)
                lower_up(k, lo, in->key_vel[k]);
            else if (up >= 0)
                upper_up(up);
        }
    }
    for (r = 0; r < EC_NE; r++) {
        const int d = in->enc[r];
        if (!d)
            continue;
        if (ec_e5_knob(r, d)) {                    /* E5: naming, PAGE mode */
            A->redraw = 1;
            continue;
        }
        if (r == EC_E_SELECT) {
            if (ec_rzui_active(A))
                ec_rzui_select(A, d);                 /* E3: SELECT picks the slice on the RAZOR page */
            else if (A->cat == EC_CAT_FILE && !A->settings) {
                const int n = ec_browse_count();
                if (n > 0)
                    A->cursor = ((A->cursor + d) % n + n) % n;
            } else if (!A->settings) {
                const int n = ec_cat_pages(A->cat, ec_app_params(A));
                A->page[A->cat] = ((A->page[A->cat] + d) % n + n) % n;
            }
        } else if (r == EC_E_ALGO) {
            const int cur = A->bus ? EC_NTRACK - 1 + A->bus : A->track;
            select_target(clampi(cur + d, 0, EC_NTRACK + 2));
        } else if (r == EC_E_PRESETS) {
            const int base = A->seq.pending >= 0 ? A->seq.pending : A->proj.current;
            ec_seq_request_pattern(&A->seq, clampi(base + d, 0, EC_NPAT - 1));
            mark_dirty();
        } else
            knob_cell(r - EC_E_K1, d);
        A->redraw = 1;
    }
    for (b = 0; b < EC_NB; b++)
        if (in->btn_up >> b & 1u)
            button_up(b);

    ec_e5_tick();                                  /* E5: the glide, prompts */
    /* autosave: stopped, dirty, quiet for a moment */
    if (A->dirty && !A->seq.playing && A->ms - A->dirty_ms > AUTOSAVE_MS && A->load_slot < 0)
        save_project(1);
}

void ec_app_audio(int16_t *out, uint32_t frames)
{
    const float m = (float)A->master / 1023.0f;
    uint32_t done = 0;
    ec_dsp_set_master_volume(A->dsp, m * m);
    {   /* E4: the bus FX records -> the engine (cheap: three struct fills per call) */
        int b;
        for (b = 0; b < 3; b++) {
            ec_fx_params_t fp;
            float v[EC_NP];
            const ec_scenes_t *sc = &A->proj.e5.sc;
            int id;
            memcpy(v, A->proj.bus[b], sizeof v);
            for (id = 0; id < EC_NP; id++)       /* the crossfader's morph of the bus FX (scene rows 6..8) */
                if (sc->on[EC_NTRACK + b][id])
                    v[id] = sc->xf[EC_NTRACK + b][id];
            ec_params_fx(v, &fp);
            ec_dsp_bus_params(A->dsp, b, &fp);
        }
    }
    while (done < frames) {
        const uint32_t n = frames - done > 1024 ? 1024 : frames - done;
        uint32_t i;
        ec_seq_render(&A->seq, g_buf, (int)n);
        for (i = 0; i < 2 * n; i++) {
            const float x = g_buf[i] * 32767.0f;
            out[2 * done + i] = (int16_t)(x > 32767.0f ? 32767 : x < -32768.0f ? -32768 : (int)x);
        }
        done += n;
    }
    A->frames_out += frames;
}

/* ------------------------------------------------------------------------------------------- LEDs --- */
static int blink(uint32_t period) { return (A->ms / period) & 1u; }

static void leds_lower(void)
{
    const ec_ptrack_t *tr = ec_project_track(&A->proj, A->track);
    const float *v = tr->v;
    const int m = iv(v, EC_P_MACHINE);
    int i;
    if (ec_e5_leds_lower())                        /* E5: naming, PAGE, the scene row */
        return;
    if (A->patn_open) {
        for (i = 0; i < EC_NLOWER; i++)
            ec_plat_led_lower(i, i == A->proj.current ? EC_LED_LIT
                                 : i == A->seq.pending ? (blink(120) ? EC_LED_LIT : EC_LED_DIM)
                                 : A->proj.pat[i].used ? EC_LED_DIM
                                                       : EC_LED_FAINT);
        return;
    }
    if (loop_mode()) {
        ec_track_status_t st;
        ec_dsp_track_status(A->dsp, A->track, &st);
        if (is_slice_m(m)) {
            const int cnt = clampi(iv(v, EC_P_SLICE_COUNT), 1, 32);
            uint32_t sounding = A->seq.tr[A->track].pads;
            int j;
            for (j = 0; j < st.voices && j < EC_MAX_VOICES_TRACK; j++)
                if (st.voice_slice[j] >= 1 && st.voice_slice[j] <= 32)
                    sounding |= 1u << (st.voice_slice[j] - 1);
            for (i = 0; i < EC_NLOWER; i++)
                ec_plat_led_lower(i, i >= cnt ? EC_LED_OFF : (sounding >> i & 1u) ? EC_LED_LIT : EC_LED_DIM);
        } else {
            const int div = clampi((iv(v, EC_P_LOOP_DIVISION) + 1) / 2 * 2, 2, 32);
            const float units = 128.0f / (float)div;
            const float ls = ec_seq_value(&A->seq, A->track, EC_P_LOOP_START);
            const float le = ec_seq_value(&A->seq, A->track, EC_P_LOOP_END);
            int ph = -1;
            if (st.reader_on) {   /* the playhead in track space -> its key */
                const ec_slot_t *sl = &A->proj.slot[ec_app_track_slot(A, A->track)];
                const float span = sl->trim_end - sl->trim_start;
                const float lo = sl->trim_start + span * v[EC_P_RANGE_START] / 128.0f;
                const float hi = sl->trim_start + span * v[EC_P_RANGE_END] / 128.0f;
                if (hi > lo)
                    ph = (int)((st.pos_sample - lo) / (hi - lo) * 128.0f / units);
            }
            for (i = 0; i < EC_NLOWER; i++) {
                const float ks = (float)i * units, ke = ks + units;
                uint8_t l = i < div ? EC_LED_FAINT : EC_LED_OFF;
                if (i < div && ke > ls + 0.001f && ks < le - 0.001f)
                    l = EC_LED_DIM;
                if (i == ph)
                    l = EC_LED_MID;
                if (st.keys_held >> i & 1u)
                    l = EC_LED_LIT;
                ec_plat_led_lower(i, l);
            }
        }
        return;
    }
    if (A->keymode == EC_KM_KEYS) {
        for (i = 0; i < EC_NLOWER; i++)
            ec_plat_led_lower(i, i % 7 == 0 ? EC_LED_DIM : EC_LED_FAINT);
        return;
    }
    {
        const int len = ec_seq_pattern_steps(&A->seq, A->track);
        const int play = ec_seq_step(&A->seq, A->track);
        for (i = 0; i < EC_NLOWER; i++) {
            const int idx = A->step_page * 16 + i;
            const ec_step_t *s = &tr->steps[idx];
            uint8_t l = EC_LED_OFF;
            if (idx < len) {
                if (s->trig || s->slices)
                    l = ec_step_is_ghost(s, v) ? EC_LED_DIM : s->nlock ? (blink(160) ? EC_LED_MID : EC_LED_DIM)
                                                                       : EC_LED_MID;
                if (idx == play && A->seq.playing)
                    l = EC_LED_LIT;
            }
            if (A->step_held >> i & 1u)
                l = EC_LED_LIT;
            ec_plat_led_lower(i, l);
        }
    }
}

static void leds(void)
{
    int i, b;
    leds_lower();
    for (i = 0; i < 9; i++) {
        uint8_t l;
        const int sel = i < EC_NTRACK ? (!A->bus && A->track == i) : A->bus == i - EC_NTRACK + 1;
        if (sel)
            l = EC_LED_LIT;
        else if (i < EC_NTRACK && ec_project_track(&A->proj, i)->v[EC_P_MUTE] > 0.5f)
            l = EC_LED_OFF;
        else {
            const float m = A->meter[i];
            l = m > 0.5f ? EC_LED_MID : m > 0.05f ? EC_LED_DIM : EC_LED_FAINT;
        }
        ec_plat_led_upper(i, l);
    }
    ec_plat_led_upper(9, ec_seq_fill_active(&A->seq) ? EC_LED_LIT : EC_LED_FAINT);
    ec_plat_led_upper(10, A->patn_open ? EC_LED_LIT : A->seq.pending >= 0 ? (blink(120) ? EC_LED_LIT : EC_LED_DIM)
                                                                          : EC_LED_FAINT);
    for (b = 0; b < EC_NB; b++)
        if (CAT_OF[b] >= 0)
            ec_plat_led_button(b, CAT_OF[b] == A->cat ? (A->settings && blink(250) ? EC_LED_MID : EC_LED_LIT)
                                                      : EC_LED_FAINT);
    ec_plat_led_button(EC_B_HOME, A->fn ? EC_LED_LIT : EC_LED_FAINT);
    ec_plat_led_button(EC_B_PLAY, A->seq.playing ? EC_LED_LIT : A->seq.paused ? (blink(400) ? EC_LED_MID : EC_LED_DIM)
                                                                              : EC_LED_DIM);
    ec_plat_led_play_green(A->seq.playing ? EC_LED_LIT : EC_LED_OFF);
    ec_plat_led_button(EC_B_REC, A->dirty ? EC_LED_DIM : EC_LED_FAINT);
    ec_plat_led_button(EC_B_OCTDN, A->keymode == EC_KM_LOOP ? EC_LED_LIT : EC_LED_FAINT);
    ec_plat_led_button(EC_B_OCTUP, A->keymode == EC_KM_KEYS ? EC_LED_LIT : EC_LED_FAINT);
    ec_e5_leds();                                  /* E5 */
    if (ec_rzui_active(A))                         /* E3: the RAZOR page's keys and OCT buttons (page-local) */
        ec_rzui_leds(A);
}

static void meters(void)
{
    int t;
    float l, r;
    for (t = 0; t < 9; t++) {
        float pk = 0;
        if (t < EC_NTRACK) {
            ec_track_status_t st;
            ec_dsp_track_status(A->dsp, t, &st);
            pk = st.peak_l > st.peak_r ? st.peak_l : st.peak_r;
        } else if (t == 8) {
            ec_dsp_master_peak(A->dsp, &l, &r);
            pk = l > r ? l : r;
        } else {                                    /* E4: the S1 / S2 returns */
            ec_bus_status_t bs;
            ec_dsp_bus_status(A->dsp, t - EC_NTRACK, &bs);
            pk = bs.peak_l > bs.peak_r ? bs.peak_l : bs.peak_r;
        }
        pk = pk > 1 ? 1 : pk;
        A->meter[t] = pk > A->meter[t] ? pk : A->meter[t] * 0.85f;
    }
}

void ec_app_frame(void)
{
    const uint32_t now = A->ms;
    const int loading = A->load_slot >= 0 || A->scanning;
    meters();
    leds();
    if (!A->redraw && !((A->seq.playing || loading || A->msg_ms) && now - A->last_draw >= 30))
        return;
    if (A->msg_ms && now - A->msg_ms > 1200)
        A->msg_ms = 0;
    A->redraw = 0;
    A->last_draw = now;
    ec_gfx_begin(ec_plat_lcd());
    ec_screens_draw(A);
    ec_plat_lcd_commit();
}

void ec_app_ui_info(char *buf, uint32_t n)
{
    static const char *const LD[] = {"idle", "stat", "header", "data", "done", "error"};
    static const char *const KM[] = {"steps", "loop", "keys", "scene"};
    static const char *const SL[] = {"empty", "queued", "loading", "ready", "missing", "error"};
    const ec_ss_sample_t *s = ec_app_track_sample(A, A->track);
    const int sl = ec_app_track_slot(A, A->track);
    int t, trigs = 0, missing = 0, ready = 0, used = 0;
    char hs[64];
    for (t = 0; t < EC_NTRACK; t++)
        trigs += A->seq.tr[t].fired;
    for (t = 0; t < EC_NSLOT; t++) {
        missing += A->slot[t].state == EC_SL_MISSING;
        ready += A->slot[t].state == EC_SL_READY;
    }
    for (t = 0; t < EC_NPAT; t++)
        used += A->proj.pat[t].used;
    hs[0] = 0;
    for (t = 0; t < EC_NTRACK; t++) {
        char x[12];
        snprintf(x, sizeof x, "%s%d", t ? "," : "", A->seq.tr[t].fired);
        strncat(hs, x, sizeof hs - strlen(hs) - 1);
    }
    snprintf(buf, n,
             "ec%s play:%d bpm:%.1f pat:%d pend:%d trk:%d bus:%d cat:%s%s page:%d keys:%s fill:%d files:%d scan:%s "
             "file:%s load:%s slot%d:%s ready:%d missing:%d seq:%s trigs:%d fired:%s patterns:%d saves:%d dirty:%d "
             "voices:%d step:%d",
             EC_VERSION, A->seq.playing, A->proj.bpm, A->proj.current + 1, A->seq.pending + 1, A->track + 1, A->bus,
             ec_cat_name(A->cat), A->settings ? "/SET" : "", A->page[A->cat] + 1, KM[A->keymode],
             ec_seq_fill_active(&A->seq), ec_browse_count(),
             A->scanning ? "busy" : A->no_folder ? "nofolder" : "done", s ? s->name : "-",
             LD[ec_load_state()], sl + 1, SL[A->slot[sl].state], ready, missing, trigs ? "on" : "off", trigs, hs, used,
             A->saves, A->dirty,
             ec_dsp_total_voices(A->dsp), ec_seq_step(&A->seq, A->track) + 1);
    {
        const uint32_t l = (uint32_t)strlen(buf);   /* E5 */
        if (l < n)
            ec_e5_ui_info(buf + l, n - l);
    }
}

void ec_app_shutdown(void) { ec_load_cancel(); }
uint32_t ec_app_cpu_hint(void) { return 0; }
