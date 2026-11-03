/* SPDX-License-Identifier: GPL-3.0-only */
/* The app (ec_app.h): ec_plat.h's ec_app_* entry points. Input -> the project / the sequencer, the sample pool's
 * loading over the file API, the project in the flash record, the LEDs, the screens. docs/FM1-CONTROLS.md is the
 * player's reference for what each control does here. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ec_app.h"
#include "ec_projfile.h"
#include "ec_browse.h"
#include "ec_fs.h"
#include "ec_gfx.h"
#include "ec_leds.h"
#include "ec_loader.h"
#include "ec_mtx.h"
#include "ec_razor_ui.h"   /* E3 */
#include "ec_scene.h"        /* E5 (bus rows: the knob takes over) */
#include "ec_screens.h"
#include "ec_snap.h"
#include "ec_ui_kit.h"
#include "ec_version.h"

ec_app_t ec_app;
#define A (&ec_app)

#define TAP_MS 250            /* STEP_HOLD_SECONDS / PATTERN_TAP_SECONDS */
#define AUTOSAVE_MS 2000

static uint8_t g_dsp_mem[EC_DSP_MEM_MAX] __attribute__((aligned(16)));   /* the engine (ec_dsp.c asserts it fits) */
static float g_buf[1024 * 2];
static float g_inbuf[1024 * 2];   /* E6: the live input, as floats, for the INPUT machine */

/* ------------------------------------------------------------------------------------------- small helpers --- */
static int clampi(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }
static int iv(const float *v, int id) { return (int)floorf(v[id] + 0.5f); }

static void message(const char *s)
{
    snprintf(A->msg, sizeof A->msg, "%s", s);
    A->msg_ms = A->ms ? A->ms : 1;
    A->msg_alert = 0;
    A->redraw = 1;
}
void ec_app_alert(const char *s)
{
    message(s);
    A->msg_alert = 1;
}

static void mark_dirty(void)
{
    A->dirty = 1;
    A->dirty_ms = A->ms;
    A->redraw = 1;
}
void ec_app_message(const char *s) { message(s); }   /* E6: for ec_rec.c */
void ec_app_mark_dirty(void) { mark_dirty(); }

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

/* 0.6.1: the lock of param id on the held steps (the first held step that has one): 1 = locked, *x its value */
int ec_app_held_lock(const ec_app_t *a, int id, float *x)
{
    const ec_ptrack_t *tr = &a->proj.pat[a->proj.current].tr[a->track];
    int i;
    for (i = 0; i < EC_NLOWER; i++)
        if (a->step_held >> i & 1u) {
            const int s = a->step_page * 16 + i;
            if (s < EC_MAXSTEPS && ec_step_lock_get(&tr->steps[s], id, x))
                return 1;
        }
    return 0;
}

static ec_step_t *step_at(int index) { return &ec_project_track(&A->proj, A->track)->steps[index]; }

/* ------------------------------------------------------------------------------------------- items --- */
void ec_app_page(const ec_app_t *a, ec_page_t *out)
{
    const float *v = ec_project_track((ec_project_t *)&a->proj, a->track)->v;
    if (a->settings && a->bus)                                     /* E4: the bus's setup (its machine) */
        if (ec_settings_items_bus(a->cat, a->bus, out))
            return;
    if (a->settings && !a->bus && ec_settings_items(a->cat, v, out)) {
        if (a->cat == EC_CAT_SRC && out->n < EC_PAGE_ITEMS) {   /* 0.6.0: HOME's settings: KEYBOARD's ADVANCE */
            ec_item_t *it = &out->it[out->n++];
            it->kind = EC_IT_UX, it->id = EC_UX_ADVANCE, it->flags = 0, it->label = "ADVANCE";
        }
        return;
    }
    if (!a->settings && a == &ec_app && ec_pn_page(out))           /* 0.6.0: the fixed screens' cells */
        return;
    if (a == &ec_app && ec_src_insert_page()) {                     /* 0.6.1: FX INSERT: the slot's knob page */
        ec_src_ins_items(out);
        return;
    }
    ec_page_items_bus(a->cat, a->page[a->cat], v, a->bus, a->bus ? a->proj.bus[a->bus - 1] : 0, out);
}

/* E4: a bus pseudo-track's FX params live in its record (project.bus), not in the selected track */
float *ec_app_bus_params(ec_app_t *a, int id)
{
    return a->bus && (EC_PARAMS[id].flags & EC_PF_FX) ? a->proj.bus[a->bus - 1] : 0;
}

/* 0.6.1: the definition a param borrows (INS 2's P1..P8 and MIX: the mapped FX param's; ec_param_def_id) */
static int def_id(const ec_app_t *a, int id)
{
    const int d = ec_param_def_id(ec_project_track((ec_project_t *)&a->proj, a->track)->v, id);
    return d >= 0 ? d : id;
}

const char *ec_app_item_label(const ec_item_t *it)
{
    ec_item_t mxi_;
    if (it->kind == EC_IT_MTX) {                   /* 0.5.0: the MATRIX row's param */
        ec_mk_item(it, &mxi_);
        it = &mxi_;
    }
    if (it->label)
        return it->label;
    if (it->kind == EC_IT_PARAM)
        return EC_PARAMS[def_id(&ec_app, it->id)].label;
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
    ec_item_t mxi_;
    if (it->kind == EC_IT_MTX) {                   /* 0.5.0: the MATRIX row's param */
        ec_mk_item(it, &mxi_);
        it = &mxi_;
    }
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
        float x;
        if (ec_app_bus_params((ec_app_t *)a, it->id))
            return ec_app_bus_params((ec_app_t *)a, it->id)[it->id];
        if (!a->settings && (EC_PARAMS[it->id].flags & EC_PF_LOCKABLE) && ec_app_held_lock(a, it->id, &x)) {
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
    case EC_IT_UX: return ec_pn_item_value(it->id);      /* 0.6.0 */
    default: return 0;
    }
}

void ec_app_item_format(const ec_app_t *a, const ec_item_t *it, float x, char *out, int cap)
{
    static const char *const Q[] = {"SEQ", "JUMP", "START", "TEMP"};
    static const char *const RT[] = {"RETURN", "BOOMR", "RESET"};
    char b[24];
    ec_item_t mxi_;
    if (it->kind == EC_IT_MTX) {                   /* 0.5.0: the MATRIX row's param */
        ec_mk_item(it, &mxi_);
        it = &mxi_;
    }
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
    if (it->kind == EC_IT_UX) {                    /* 0.6.0 */
        ec_pn_item_format(it->id, x, out, cap);
        return;
    }
    if (it->kind == EC_IT_PARAM && EC_PARAMS[it->id].fmt == EC_PF_MDST) {   /* 0.5.0: the layout's name */
        ec_mk_dst_name(x, out, cap);
        return;
    }
    if (it->kind == EC_IT_PARAM) {
        ec_param_format(def_id(a, it->id), x, iv(ec_project_track((ec_project_t *)&a->proj, a->track)->v,
                                                 EC_P_ENV_RANGE), out, cap);
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
    ec_item_t mxi_;
    if (it->kind == EC_IT_MTX) {                   /* 0.5.0: the MATRIX row's param */
        ec_mk_item(it, &mxi_);
        it = &mxi_;
    }
    switch (it->kind) {
    case EC_IT_E5: return ec_e5_item_frac(it, x);  /* E5 */
    case EC_IT_PARAM: return ec_param_frac(def_id(a, it->id), x);
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
    case EC_IT_UX: return ec_pn_item_frac(it->id, x);      /* 0.6.0 */
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
    if (A->ux.solo && !(A->ux.solo >> t & 1u)) {   /* 0.3.0: a solo silences the others (runtime, not saved) */
        ec_track_params_t q = *p;
        q.mute = 1;
        ec_dsp_track_params(A->dsp, t, &q);
        return;
    }
    ec_dsp_track_params(A->dsp, t, p);
}
static void sink_process(void *ctx, float *out, int n)
{
    (void)ctx;
    ec_dsp_process(A->dsp, out, n);
}
static void sink_sources(void *ctx, int t, float *src)
{
    (void)ctx;
    ec_dsp_mod_sources(A->dsp, t, src);           /* 0.5.0: the matrix's generic destinations */
}
static void engine_sample(int t);
static void sink_slot(void *ctx, int t, int slot)
{
    (void)ctx;
    (void)slot;
    engine_sample(t);                             /* 0.6.0: a slot lock's trig (sample-accurate: before its events) */
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

/* give an engine track the sample of the slot it plays (its own, or a step's slot lock: ec_seq_track_slot), when it
 * changed */
static void engine_sample(int t)
{
    {
        const ec_slotrt_t *r = &A->slot[ec_seq_track_slot(&A->seq, t) - 1];
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
static void assign_samples(void)
{
    int t;
    for (t = 0; t < EC_NTRACK; t++)
        engine_sample(t);
}
void ec_app_assign_samples(void) { assign_samples(); }

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
    if (old >= 0 && old != h && !handle_in_use(old, -1) && !ec_rec_holds(old))   /* (E6: a take being saved) */
        ec_ss_free(old);
    ec_seq_touch_all(&A->seq);
    A->redraw = 1;
}

/* E6: a recorded take (ready in the store) into slot s, as a finished load: the track plays it at once */
int ec_app_handle_used(int h) { return handle_in_use(h, -1); }
void ec_app_slot_take(int s, int h)
{
    ec_slotrt_t *r = &A->slot[s];
    if (A->load_slot == s) {                       /* (a load into that slot gives way) */
        ec_load_cancel();
        A->load_slot = -1;
    }
    r->old_handle = r->state == EC_SL_READY || r->state == EC_SL_QUEUED || r->state == EC_SL_LOADING ? r->handle : -1;
    r->builtin = 0;
    r->newmeta = 0;
    r->err[0] = 0;
    slot_loaded(s, h);
}

/* 0.6.0: FILE's preview of path (ready in the store) becomes slot s, as a fresh load (BPM, steps from the file) */
void ec_app_slot_adopt(int s, int h, const char *path)
{
    ec_slotrt_t *r = &A->slot[s];
    if (A->load_slot == s) {
        ec_load_cancel();
        A->load_slot = -1;
    }
    snprintf(A->proj.slot[s].path, EC_PATH, "%s", path);
    r->old_handle = r->state == EC_SL_READY ? r->handle : -1;
    r->builtin = 0;
    r->newmeta = 1;
    r->err[0] = 0;
    slot_loaded(s, h);
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
            if (!strcmp(why, "too big") && !A->repack && !ec_rec_busy()) {   /* (E6: not under a take) */
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
                ec_file_preview_yield();       /* 0.6.0: a preview loading gives way (the loader does one at a time) */
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
/* 0.6.1: the project is a file in /data/projects (ec_projfile.c: packed, then written a chunk a tick) */
void ec_app_save(int quiet) { ec_pf_save(quiet); }

/* 0.6.1: no project after all (the last one's file is gone): a new one, the first file on T1 once the scan ends */
void ec_app_fresh_project(void)
{
    A->fresh = 1;
    scan();
}

/* ------------------------------------------------------------------------------------------- edits --- */
#define UX (&ec_app.ux)

static float nudge_machine(float m, int det)
{
    /* every machine (E6: INPUT, the live input, last); NEIGHBOR from track 2 on (track 1 has no left neighbour) */
    int x = clampi((int)(m + 0.5f) + det, EC_MACH_LOOP, EC_MACH_INPUT);
    if (x == EC_MACH_NEIGHBOR && A->track == 0)
        x = det > 0 ? EC_MACH_INPUT : EC_MACH_RAZOR_POLY;
    return (float)x;
}

/* 0.3.0: a knob turn on a param: SHIFT snaps, else det clicks of mult finest steps (lists: one item a click) */
float ec_app_move_param(int id, float x, int det, int mult)
{
    if (EC_PARAMS[id].fmt == EC_PF_MDST)          /* 0.5.0: a matrix DST: the layout's order, SHIFT a category */
        return ec_mk_dst_step(x, det, A->fn);
    id = def_id(A, id);                           /* 0.6.1: INS 2's params move as the FX param they stand for */
    if (A->fn)
        return ec_param_clamp(id, ec_param_snap(id, x, det));
    return ec_param_clamp(id, ec_param_nudge(id, x, EC_PARAMS[id].kind == EC_PK_NUM ? det * mult : det));
}

/* the finest steps over a param's range (the knob speed: lists never speed up) */
static int param_steps(int id)
{
    const ec_param_def_t *d = &EC_PARAMS[def_id(A, id)];
    if (d->kind != EC_PK_NUM || (d->flags & EC_PF_EXPSTEP))
        return 1;
    return (int)((d->max - d->min) / (d->step > 0 ? d->step : 1) + 0.5f);
}

void ec_app_edit_param_f(int id, int det, int mult, int flags)
{
    float *v = ec_app_params(A);
    const ec_param_def_t *d = &EC_PARAMS[id];
    const int hs = ec_app_held_step(A);
    float *bv = ec_app_bus_params(A, id);
    if (bv) {                                       /* E4: a bus FX param (global; no p-locks: no bus lanes) */
        bv[id] = ec_app_move_param(id, bv[id], det, mult);
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
                if (ec_step_lock_set(s, id, ec_app_move_param(id, x, det, mult)) < 0)
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
        v[id] = ec_app_move_param(id, v[id], det, mult);
    if (d->flags & EC_PF_LOCKABLE)
        ec_liverec_param(id, v[id]);              /* 0.6.0: live REC's AUTOMATION (a lock on the step) */
    if (id == EC_P_RATE)
        ec_seq_timing_changed(&A->seq);
    {
        int col;                                     /* 0.5.0: a SRC / DST set: the source's level up from 0 */
        const int k = ec_mtx_slot_of(id, &col);
        if (k >= 0 && col < 2)
            ec_mk_slot_edited(k);
    }
    ec_seq_touch(&A->seq, A->track);
    mark_dirty();
}

/* 0.5.0: for ec_modkeys.c */
void ec_app_edit_param(int id, int det, int mult) { ec_app_edit_param_f(id, det, mult, 0); }
int ec_app_param_steps(int id) { return param_steps(id); }

/* 0.6.0: F1 + a knob: clear that param's lock on the held steps (nothing held: on every step of the track) */
void ec_app_clear_lock(int id)
{
    ec_ptrack_t *tr = ec_project_track(&A->proj, A->track);
    const int len = ec_seq_pattern_steps(&A->seq, A->track);
    int i, n = 0;
    if (A->bus || !(EC_PARAMS[id].flags & EC_PF_LOCKABLE))
        return;
    if (A->step_held) {
        ec_e5_steps_edit(A->step_held, "UNLOCK", 0xA00u + (uint32_t)id);
        for (i = 0; i < EC_NLOWER; i++)
            if (A->step_held >> i & 1u)
                n += ec_step_lock_clear(step_at(A->step_page * 16 + i), id);
        A->step_edited |= A->step_held;
    } else {
        int pg;
        for (pg = 0; pg * 16 < len; pg++) {
            uint16_t mask = 0;
            for (i = 0; i < 16; i++) {
                float x;
                if (ec_step_lock_get(&tr->steps[pg * 16 + i], id, &x))
                    mask |= (uint16_t)(1u << i);
            }
            if (!mask)
                continue;
            ec_undo_steps(&A->e5.undo, &A->proj, A->track, pg, mask, "UNLOCK", 0xB0000u + ((uint32_t)pg << 8) + (uint32_t)id,
                          A->ms);
            for (i = 0; i < 16; i++)
                if (mask >> i & 1u)
                    n += ec_step_lock_clear(&tr->steps[pg * 16 + i], id);
        }
    }
    message(n ? "LOCK CLEARED" : "NO LOCK");
    ec_seq_touch(&A->seq, A->track);
    mark_dirty();
}

static float global_value(int id)
{
    const ec_project_t *p = &A->proj;
    return id == EC_G_BPM ? p->bpm : id == EC_G_SWING ? p->swing : id == EC_G_GLEN ? (float)p->glen
         : id == EC_G_QUANT ? (float)p->quant : (float)p->rtn;
}

void ec_app_edit_global(int id, int det, int mult)
{
    ec_project_t *p = &A->proj;
    ec_e5_edit_global(id, global_value(id));     /* E5: undo */
    switch (id) {
    case EC_G_BPM:
        ec_seq_set_bpm(&A->seq, A->fn ? ec_global_snap(EC_SNAP_BPM, p->bpm, det)
                                      : floorf(p->bpm + 0.5f) + (float)(det * mult));
        break;
    case EC_G_SWING:
        p->swing = A->fn ? ec_global_snap(EC_SNAP_SWING, p->swing, det) : (float)clampi((int)p->swing + det, 50, 75);
        ec_seq_timing_changed(&A->seq);
        break;
    case EC_G_GLEN:
        p->glen = A->fn ? (int)ec_global_snap(EC_SNAP_GLEN, (float)p->glen, det) : clampi(p->glen + det * mult, 1, 256);
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
        sl->bpm = A->fn ? ec_global_snap(EC_SNAP_BPM, sl->bpm, det) : floorf(sl->bpm + 0.5f) + (float)(det * mult);
        sl->bpm = sl->bpm < 20 ? 20 : sl->bpm > 300 ? 300 : sl->bpm;
        if (dur > 0) {                          /* (RECALC: the trim's steps at this BPM) */
            st = floorf(dur * (sl->trim_end - sl->trim_start) * sl->bpm / 60.0f * 4.0f + 0.5f);
            sl->steps = st < 1 ? 1 : st > 512 ? 512 : st;
        }
    } break;
    case EC_S_STEPS:
        sl->steps = A->fn ? ec_global_snap(EC_SNAP_STEPS, sl->steps, det)
                          : (float)clampi((int)(sl->steps + 0.5f) + det * mult, 1, 512);
        break;
    case EC_S_GAIN:
        sl->gain = A->fn ? ec_global_snap(EC_SNAP_GAIN, sl->gain, det) : sl->gain + 0.05f * (float)(det * mult);
        sl->gain = sl->gain < 0 ? 0 : sl->gain > 4 ? 4 : sl->gain;
        break;
    case EC_S_TRIM_START:   /* (SHIFT: the finest step, the original's trim scan) */
        sl->trim_start += (A->fn ? 0.001f : 0.005f) * (float)(det * mult);
        sl->trim_start = sl->trim_start < 0 ? 0 : sl->trim_start > sl->trim_end - 0.005f ? sl->trim_end - 0.005f
                                                                                           : sl->trim_start;
        break;
    default:
        sl->trim_end += (A->fn ? 0.001f : 0.005f) * (float)(det * mult);
        sl->trim_end = sl->trim_end > 1 ? 1 : sl->trim_end < sl->trim_start + 0.005f ? sl->trim_start + 0.005f
                                                                                       : sl->trim_end;
        break;
    }
    ec_seq_touch_all(&A->seq);
    mark_dirty();
}

static int item_steps(const ec_item_t *it)
{
    switch (it->kind) {
    case EC_IT_PARAM: return param_steps(it->id);
    case EC_IT_GLOBAL: return it->id == EC_G_BPM ? 280 : it->id == EC_G_GLEN ? 255 : 1;
    case EC_IT_SLOT: return it->id == EC_S_BPM ? 280 : it->id == EC_S_STEPS ? 511 : it->id == EC_S_GAIN ? 80 : 200;
    case EC_IT_RAZOR: return it->id == EC_RZ_START || it->id == EC_RZ_END ? 256 : 1;
    case EC_IT_E5: return it->id == EC_X_XFD ? 128 : 1;
    case EC_IT_UX: return 1;
    default: return 1;
    }
}

int ec_app_lane(const ec_app_t *a, const ec_page_t *pg)
{
    const int n = ec_ux_lanes(pg->n);
    const int l = a->settings ? 0 : a->scr ? 0 : a->ux.lane[a->cat];   /* (a setup list: rows, ec_app_set_item) */
    return clampi(l, 0, n - 1);
}

/* 0.6.1, the list menus: the setup list's rows are its items without the blanks */
int ec_app_set_rows(const ec_page_t *pg, int *idx)
{
    int i, n = 0;
    for (i = 0; i < pg->n; i++)
        if (pg->it[i].kind != EC_IT_BLANK) {
            if (idx)
                idx[n] = i;
            n++;
        }
    return n;
}

int ec_app_set_item(const ec_app_t *a, const ec_page_t *pg)
{
    int idx[EC_PAGE_ITEMS];
    const int n = ec_app_set_rows(pg, idx);
    return n ? idx[clampi(a->ux.set_row, 0, n - 1)] : -1;
}

int ec_app_list_knob(int role, int det, int *row, int n)
{
    if (role == EC_E_SELECT) {
        *row = clampi(*row + det, 0, n > 0 ? n - 1 : 0);
        A->redraw = 1;
        return 0;
    }
    return role == EC_E_K1 && n > 0;                /* K2..K4: nothing in a list */
}

int ec_app_cells_visible(const ec_app_t *a)
{
    ec_page_t pg;
    if (a->e5.naming || ec_app_shift_map(a) || a->pn.cat || a->pn.as_open)
        return 0;
    if (a->settings)
        return 1;
    ec_app_page(a, &pg);
    return a->scr || pg.n > 0;
}

/* 0.6.0, F2 + a knob: every modulation of that param (the matrix slots aimed at it, the selected track) */
static void clear_mod(int id)
{
    float *v = ec_app_params(A);
    int k, n = 0;
    if (A->bus)
        return;
    if (!ec_param_mod_dest(id)) {                    /* 0.6.1: in the header */
        message("NOT MODDABLE");
        return;
    }
    for (k = 0; k < EC_MTX_SLOTS; k++)
        if (ec_mtx_src(v, k) != EC_MS_OFF && ec_mtx_dst(v, k) == id) {
            v[ec_mtx_src_id(k)] = EC_PARAMS[ec_mtx_src_id(k)].def;
            v[ec_mtx_dst_id(k)] = EC_PARAMS[ec_mtx_dst_id(k)].def;
            v[ec_mtx_depth_id(k)] = EC_PARAMS[ec_mtx_depth_id(k)].def;
            n++;
        }
    message(n ? "MOD CLEARED" : "NO MOD");
    if (n) {
        ec_seq_touch(&A->seq, A->track);
        mark_dirty();
    }
}

/* a cell turned (the page's, or F3's): its lane, the knob's speed; a source key held routes it, F1 clears its locks,
 * F2 its modulation; the takeover */
static void turn_cell(const ec_page_t *pg, int cell, int role, int det)
{
    const ec_item_t *it;
    int mult;
    ec_item_t mx;
    if (cell >= pg->n || pg->it[cell].kind == EC_IT_BLANK)
        return;
    ec_mk_item(&pg->it[cell], &mx);                  /* 0.5.0: a MATRIX cell is its row's param */
    it = &mx;
    if (!UX->f3 && ec_mk_turn(it, role, det)) {      /* 0.6.0: a source key held (the source template) */
        A->redraw = 1;
        return;
    }
    if (!UX->f3 && !A->settings && it->kind == EC_IT_PARAM && (ec_pn_f(1) || ec_pn_f(2))) {
        A->pn.f_used = 3;                            /* (F1 / F2 held: no sticky toggle on release) */
        if (ec_pn_f(1))
            ec_app_clear_lock(it->id);
        else
            clear_mod(it->id);
        A->redraw = 1;
        return;
    }
    mult = ec_ux_accel(role, item_steps(it), det);
    if (!ec_e5_knob_cell(it, det, mult)) {        /* E5: scene / slice edits, E5 cells */
        switch (it->kind) {
        case EC_IT_PARAM: ec_app_edit_param_f(it->id, det, mult, it->flags); break;
        case EC_IT_UX: ec_pn_item_edit(it->id, det); break;
        case EC_IT_GLOBAL: ec_app_edit_global(it->id, det, mult); break;
        case EC_IT_SLOT: edit_slot(it->id, det, mult); break;
        case EC_IT_RAZOR: ec_rzui_edit(A, it->id, det, A->fn ? 1 : mult); break;   /* E3 (SHIFT: the finest) */
        default: break;
        }
    }
    if (!UX->f3 && !ec_app_cells_visible(A) && !A->e5.naming) {   /* the value isn't on screen: take it over */
        char val[24];
        const float x = ec_app_item_value(A, it, 0);
        ec_app_item_format(A, it, x, val, sizeof val);
        ec_ux_takeover(ec_app_item_label(it), val, ec_app_item_frac(A, it, x));
    }
    A->redraw = 1;
}

void ec_app_knob_cell(int k, int det)
{
    ec_page_t pg;
    if (UX->f3) {
        ec_page_f3(&pg);
        turn_cell(&pg, k, EC_E_K1 + k, det);
        return;
    }
    ec_app_page(A, &pg);
    if (A->settings) {                               /* 0.6.1: a list: K1 the selected row, K2..K4 nothing */
        const int it = ec_app_set_item(A, &pg);
        if (k == 0 && it >= 0)
            turn_cell(&pg, it, EC_E_K1, det);
        return;
    }
    turn_cell(&pg, ec_app_lane(A, &pg) * 4 + k, EC_E_K1 + k, det);
}

/* ------------------------------------------------------------------------------------------- categories --- */
int ec_app_has_settings(int cat)
{
    ec_page_t pg;
    if (A->bus && ec_settings_items_bus(cat, A->bus, &pg))     /* E4 */
        return 1;
    return ec_settings_items(cat, ec_app_params(A), &pg);
}

/* a file of the folder into pool slot s (the loader; the track keeps its slot) */
void ec_app_load_file(int s, const char *path)
{
    queue_slot(s, path, 1);
    mark_dirty();
}

static int page_lanes(void)
{
    ec_page_t pg;
    ec_app_page(A, &pg);
    return ec_ux_lanes(pg.n);
}

void ec_app_set_page(int c, int page)
{
    A->page[c] = page;
    UX->lane[c] = 0;
}

/* a page of a category, the settings closed (the source keys' taps, the layout tool's walk) */
void ec_app_goto(int c, int page)
{
    ec_pn_goto(c, page);
}

/* SELECT: the next lane, on past the last into the next page (and back: the previous page's last lane) */
void ec_app_lane_step(int d)
{
    const int c = A->cat;
    const int np = ec_cat_pages(c, ec_app_params(A));
    for (; d > 0; d--) {
        if (UX->lane[c] + 1 < page_lanes())
            UX->lane[c]++;
        else
            ec_app_set_page(c, (A->page[c] + 1) % np);
    }
    for (; d < 0; d++) {
        if (UX->lane[c] > 0)
            UX->lane[c]--;
        else {
            ec_app_set_page(c, (A->page[c] + np - 1) % np);
            UX->lane[c] = page_lanes() - 1;
        }
    }
}

/* ------------------------------------------------------------------------------------------- mute / solo --- */
int ec_app_track_audible(const ec_app_t *a, int t)
{
    if (ec_project_track((ec_project_t *)&a->proj, t)->v[EC_P_MUTE] > 0.5f)
        return 0;
    return !a->ux.solo || (a->ux.solo >> t & 1u);
}

void ec_app_toggle_mute(int t)
{
    float *v = ec_project_track(&A->proj, t)->v;
    char b[16];
    v[EC_P_MUTE] = v[EC_P_MUTE] > 0.5f ? 0.0f : 1.0f;
    ec_seq_touch(&A->seq, t);
    snprintf(b, sizeof b, "T%d %s", t + 1, v[EC_P_MUTE] > 0.5f ? "MUTED" : "ON");
    message(b);
    mark_dirty();
}

void ec_app_toggle_solo(int t)
{
    char b[16];
    UX->solo ^= (uint8_t)(1u << t);
    ec_seq_touch_all(&A->seq);
    snprintf(b, sizeof b, "T%d %s", t + 1, UX->solo >> t & 1u ? "SOLO" : "UNSOLO");
    message(b);
}

void ec_app_clear_mutes(void)
{
    int t;
    for (t = 0; t < EC_NTRACK; t++)
        ec_project_track(&A->proj, t)->v[EC_P_MUTE] = 0;
    UX->solo = 0;
    ec_seq_touch_all(&A->seq);
    message("ALL TRACKS ON");
    mark_dirty();
}

/* SHIFT's screen: SHIFT held alone a moment, or sticky */
int ec_app_shift_map(const ec_app_t *a)
{
    if (a->e5.naming || a->pn.as_open || a->pn.rs_open || a->rec.open || a->ux.f3)
        return 0;                                    /* (a list SHIFT + a key opened shows over it) */
    return a->pn.shift_sticky || a->pn.shift_held;   /* 0.6.1: held = sticky, from the press, used or not */
}

/* ------------------------------------------------------------------------------------------- ec_app_* --- */
void ec_app_init(void)
{
    uint32_t size;
    const uint8_t *rec;
    ec_seq_sink_t sink;
    memset(&sink, 0, sizeof sink);
    int i;
    memset(A, 0, sizeof *A);
    if (ec_dsp_mem_size() > sizeof g_dsp_mem)   /* (cannot happen: ec_dsp.c checks it at compile time) */
        return;
    A->dsp = ec_dsp_init(g_dsp_mem, (float)EC_FS);
    A->load_slot = -1;
    A->tap_key = -1;
    A->master = 820;
    A->redraw = 1;
    UX->pre_idx = -1;
    A->mk.held = A->mk.hot_id = -1;
    for (i = 0; i < EC_NSLOT; i++)
        A->slot[i].handle = A->slot[i].old_handle = -1;
    for (i = 0; i < EC_NTRACK; i++)
        A->assigned[i] = -1;
    ec_ss_reset();
    rec = ec_plat_record(&size);
    i = ec_pf_boot();                              /* 0.6.1: the flash holds the last project's name */
    if (i == EC_PF_BOOT_FLASH && ec_project_load(&A->proj, rec, size) == 0) {   /* (0.6.0 or older: loads once) */
        for (i = 0; i < EC_NSLOT; i++)
            if (A->proj.slot[i].path[0])
                A->slot[i].state = EC_SL_QUEUED;
        message("PROJECT LOADED");
    } else {
        ec_project_init(&A->proj);
        A->fresh = i != EC_PF_BOOT_FILE;           /* (a file: it lands from ec_pf_tick) */
    }
    sink.ctx = 0;
    sink.event = sink_event;
    sink.params = sink_params;
    sink.process = sink_process;
    sink.sources = sink_sources;
    sink.slot = sink_slot;
    ec_seq_init(&A->seq, &A->proj, &sink, EC_FS);
    ec_dsp_set_tempo(A->dsp, A->proj.bpm);
    ec_e5_init();                                  /* E5 */
    ec_rec_init();                                 /* E6 */
    A->cat = EC_CAT_SRC;                           /* 0.6.0: power-on / project load = LOOP ENGINE */
    A->page[EC_CAT_SRC] = 0;
    ec_pn_init();                                  /* 0.6.0: the modes, APP SETTINGS, the intro */
    scan();
}

void ec_app_tick(const ec_input_t *in)
{
    A->ms = in->ms;
    scan_tick();
    loader_tick();
    assign_samples();

    if (in->master_moved) {
        A->master = in->master;
        A->redraw = 1;
    }
    ec_pn_tick(in);                                /* 0.6.0: the panel (ec_panel.c), the live controls */
    ec_rec_tick();                                 /* E6: a take ended -> its slot, the WAV writer */
    ec_file_tick();                                /* 0.6.0: FILE's preview */
    ec_liverec_tick();                             /* 0.6.0: live REC (FIXED's end) */
    ec_e5_tick();                                  /* E5: prompts, the preview */
    ec_pf_tick();                                  /* 0.6.1: the project file's save / load */
    /* autosave: stopped, dirty, quiet for a moment */
    if (A->dirty && !A->seq.playing && A->ms - A->dirty_ms > AUTOSAVE_MS && A->load_slot < 0)
        ec_app_save(1);
}

void ec_app_audio(int16_t *out, uint32_t frames)
{
    const float m = (float)A->master / 1023.0f;
    uint32_t done = 0, ain_n = 0;
    const int16_t *ain = ec_plat_ain(&ain_n);      /* E6: the host's input for this very block (NULL: silence) */
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
    A->rec.host_block = frames;                    /* E6 (informational: the shift is the host's latency alone) */
    while (done < frames) {
        const uint32_t n = frames - done > 1024 ? 1024 : frames - done;
        /* E6: this chunk's input (frame i of it lines up with output frame i), silence past what the host gave */
        const uint32_t in_n = ain && ain_n > done ? (ain_n - done < n ? ain_n - done : n) : 0;
        const int16_t *in = in_n ? ain + 2 * done : NULL;
        static int16_t gin[1024 * 2];
        uint32_t i;
        if (in) {                                  /* 0.6.0: the SAMPLER's input gain (the take, the INPUT machine) */
            memcpy(gin, in, (size_t)in_n * 4u);
            ec_sampler_input(gin, in_n);
            in = gin;
        }
        for (i = 0; in && i < 2 * n; i++)
            g_inbuf[i] = i < 2 * in_n ? (float)in[i] / 32768.0f : 0.0f;
        ec_dsp_input(A->dsp, in ? g_inbuf : NULL, (int)n);
        ec_rec_pre(n);
        ec_seq_render(&A->seq, g_buf, (int)n);
        for (i = 0; i < 2 * n; i++) {
            const float x = g_buf[i] * 32767.0f;
            out[2 * done + i] = (int16_t)(x > 32767.0f ? 32767 : x < -32768.0f ? -32768 : (int)x);
        }
        ec_rec_post(in, in_n, out + 2 * done, n);
        ec_liverec_audio(out + 2 * done, n, in, in_n);   /* 0.6.0: the click, the monitor, the preview (after) */
        done += n;
    }
    ec_dsp_input(A->dsp, NULL, 0);
    A->frames_out += frames;
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
    const int loading = A->load_slot >= 0 || A->scanning || A->rec.open || A->rec.state != EC_RS_IDLE ||
                        (A->cat == EC_CAT_SRC && !A->bus &&      /* E6: the live meters */
                         iv(ec_app_params(A), EC_P_MACHINE) == EC_MACH_INPUT);
    meters();
    ec_rec_frame();                                /* E6: the input meters */
    ec_pn_leds();                                  /* 0.6.0 (ec_panel_leds.c) */
    if (A->pn.intro) {                             /* the intro: a new frame every 100 ms */
        const int f = (int)((now - A->pn.intro_ms) / (1000 / EC_KIT_SPR_FPS));
        if (f >= ec_scr_intro_frames())
            A->pn.intro = 0, A->redraw = 1;
        else if (f != A->pn.intro_frame)
            A->pn.intro_frame = f, A->redraw = 1;
    }
    if (A->pn.cat && now - A->pn.cat_ms >= 25)   /* the CAT dances */
        A->pn.cat_ms = now, A->redraw = 1;
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
             ec_mode_name(ec_app_mode()), A->settings ? "/SET" : "", A->page[A->cat] + 1,
             ec_app_mode() == EC_CAT_SRC ? "loop" : ec_app_mode() == EC_MODE_KEYBOARD ? "keys"
                 : ec_app_mode() == EC_CAT_XFD ? "scene" : "steps",
             ec_seq_fill_active(&A->seq), ec_browse_count(),
             A->scanning ? "busy" : A->no_folder ? "nofolder" : "done", s ? s->name : "-",
             LD[ec_load_state()], sl + 1, SL[A->slot[sl].state], ready, missing, trigs ? "on" : "off", trigs, hs, used,
             A->saves, A->dirty,
             ec_dsp_total_voices(A->dsp), ec_seq_step(&A->seq, A->track) + 1);
    {   /* 0.3.0: the lane, F3, mutes and solos, the takeover, SHIFT's map, the sample PRESETS points at */
        ec_page_t pg;
        uint32_t l = (uint32_t)strlen(buf);
        int mutes = 0;
        for (t = 0; t < EC_NTRACK; t++)
            mutes |= (ec_project_track(&A->proj, t)->v[EC_P_MUTE] > 0.5f) << t;
        ec_app_page(A, &pg);
        if (l < n)
            snprintf(buf + l, n - l, " lane:%d/%d f3:%d solo:%02x mutes:%02x tko:%d smap:%d popup:%d cursor:%d",
                     ec_app_lane(A, &pg) + 1, ec_ux_lanes(pg.n), UX->f3, UX->solo, mutes, UX->to_ms ? UX->to_kind : 0,
                     ec_app_shift_map(A), ec_app_popup(A), A->cursor + 1);
        l = (uint32_t)strlen(buf);   /* E5 */
        if (l < n)
            ec_e5_ui_info(buf + l, n - l);
        l = (uint32_t)strlen(buf);   /* E6 */
        if (l < n)
            ec_rec_ui_info(buf + l, n - l);
        l = (uint32_t)strlen(buf);   /* 0.5.0 */
        if (l < n)
            snprintf(buf + l, n - l, " mkheld:%d mrow:%d", A->mk.held + 1, A->mk.row + 1);
        l = (uint32_t)strlen(buf);   /* 0.6.0 */
        if (l < n)
            snprintf(buf + l, n - l,
                     " mode:%d scr:%d shift:%d f:%d/%d clip:%d target:%d oct:%d bank:%d zoom:%d cat_on:%d "
                     "appset:%d theme:%d intro:%d stepp:%d",
                     ec_app_mode(), A->scr, A->pn.shift_sticky ? 2 : A->pn.shift_held,
                     A->pn.f_held, A->pn.f_sticky, A->pn.clip_state, A->pn.target, A->pn.oct, A->pn.bank,
                     A->pn.zoom_half, A->pn.cat, A->pn.as_open, ec_gfx_theme(), A->pn.intro, A->step_page + 1);
        l = (uint32_t)strlen(buf);   /* 0.6.1: the project file */
        if (l < n)
            snprintf(buf + l, n - l, " pfile:%s psave:%d pload:%d", ec_pf_name()[0] ? ec_pf_name() : "-",
                     ec_pf_saving(), ec_pf_loading());
        l = (uint32_t)strlen(buf);   /* 0.6.0 part 2: live REC, FILE */
        if (l < n)
            snprintf(buf + l, n - l, " lrec:%d lwait:%d lwrites:%d fdir:%s pv:%d/%d edzoom:%d pool:%d",
                     A->pn.lr_on, A->pn.lr_wait || A->pn.lr_ci, A->pn.lr_writes, A->pn.fdir, A->pn.pv_state,
                     A->pn.pv_play, 1 << A->pn.ed_zoom, ec_file_slots_used());
        l = (uint32_t)strlen(buf);   /* 0.6.1: the header's title line when it shows a message / a held key ('_' for ' ') */
        if (l < n) {
            char h[40], *c;
            const int bar = ec_hdr_title("", h, sizeof h);
            for (c = h; *c; c++)
                if (*c == ' ')
                    *c = '_';
            snprintf(buf + l, n - l, " hdr:%s hdrbar:%d", ec_hdr_custom() ? h : "-", bar);
        }
    }
}

void ec_app_shutdown(void)
{
    ec_load_cancel();
    ec_pf_shutdown();
}
uint32_t ec_app_cpu_hint(void) { return 0; }
