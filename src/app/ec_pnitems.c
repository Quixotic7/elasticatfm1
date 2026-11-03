/* SPDX-License-Identifier: GPL-3.0-only */
/* 0.6.0: the cells of the screens without params of their own (the approved mockups): KEYBOARD (SLICE or OCT, PITCH,
 * VEL, STEP), PAGES (LENGTH, RATE, LOOP), CROSSFADER (the layout's fader cell + GLIDE, A LOCKS, B LOCKS), FILE's
 * browser (SLOT LOAD fills, LENGTH, BPM, FREE), SAMPLER (LENGTH, GAIN, MONITOR, FREE), PATTERN LAUNCH (PLAYING,
 * NEXT, QUANT, LENGTH), and the panel's own items (EC_IT_UX) the cells and settings lists show. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ec_app.h"
#include "ec_browse.h"
#include "ec_scene.h"

#define A (&ec_app)
#define P (&ec_app.pn)

static int clampi(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }

static void add(ec_page_t *o, int kind, int id, const char *label)
{
    ec_item_t *it;
    if (o->n >= EC_PAGE_ITEMS)
        return;
    it = &o->it[o->n++];
    it->kind = (uint8_t)kind;
    it->id = (uint8_t)id;
    it->flags = 0;
    it->label = label;
}

int ec_pn_page(void *vout)
{
    ec_page_t *o = (ec_page_t *)vout;
    switch (A->scr) {
    case EC_MODE_KEYBOARD:
        memset(o, 0, sizeof *o);
        o->title = "KEYBOARD";
        o->pic = EC_PIC_STEPS;
        if (A->bus)
            return 1;
        if (ec_kb_slice_track())
            add(o, EC_IT_PARAM, EC_P_SLICE_INDEX, "SLICE");
        else
            add(o, EC_IT_UX, EC_UX_OCT, "OCTAVE");
        add(o, EC_IT_PARAM, EC_P_PITCH, "PITCH");
        add(o, EC_IT_PARAM, EC_P_VELOCITY, "VEL");
        add(o, EC_IT_UX, EC_UX_TARGET, "STEP");
        return 1;
    case EC_MODE_PAGES:
        memset(o, 0, sizeof *o);
        o->title = "PAGES";
        o->pic = EC_PIC_STEPS;
        if (A->bus)
            return 1;
        add(o, EC_IT_PARAM, EC_P_PATTERN_STEPS, "LENGTH");
        add(o, EC_IT_PARAM, EC_P_RATE, "RATE");
        add(o, EC_IT_UX, EC_UX_PLOOP, "LOOP");
        return 1;
    case EC_MODE_SAMPLER:
        memset(o, 0, sizeof *o);
        o->title = "SAMPLER";
        o->pic = EC_PIC_STEPS;
        add(o, EC_IT_UX, EC_UX_SLEN, "LENGTH");
        add(o, EC_IT_UX, EC_UX_SGAIN, "GAIN");
        add(o, EC_IT_UX, EC_UX_SMON, "MONITOR");
        add(o, EC_IT_UX, EC_UX_FREE, "FREE");
        return 1;
    default: break;
    }
    if (!A->scr && A->cat == EC_CAT_FILE && ec_page_pic(EC_CAT_FILE, A->page[EC_CAT_FILE], 0) == EC_PIC_FILE) {
        memset(o, 0, sizeof *o);                   /* the browser's (fixed) */
        o->title = "BROWSE";
        o->pic = EC_PIC_FILE;
        add(o, EC_IT_UX, EC_UX_FSLOT, "SLOT");
        add(o, EC_IT_UX, EC_UX_FLEN, "LENGTH");
        add(o, EC_IT_UX, EC_UX_FBPM, "BPM");
        add(o, EC_IT_UX, EC_UX_FREE, "FREE");
        return 1;
    }
    if (!A->scr && A->cat == EC_CAT_PAT && !A->bus) {   /* PATTERN LAUNCH (the mock's): PLAYING, NEXT, the layout's */
        ec_page_t lp;
        int i;
        ec_page_items(EC_CAT_PAT, A->page[EC_CAT_PAT], ec_app_params(A), 0, &lp);
        memset(o, 0, sizeof *o);
        o->title = lp.title;
        o->pic = lp.pic;
        add(o, EC_IT_UX, EC_UX_PLAYING, "PLAYING");
        add(o, EC_IT_UX, EC_UX_NEXT, "NEXT");
        for (i = 0; i < 2 && i < lp.n; i++)
            o->it[o->n++] = lp.it[i];
        return 1;
    }
    if (!A->scr && A->cat == EC_CAT_XFD && A->e5.anchor && !A->bus) {   /* A / B held: the page it came from */
        const int c = P->xf_from >= 0 && P->xf_from < EC_NCAT ? P->xf_from : EC_CAT_FILT;
        ec_page_items(c, A->page[c], ec_app_params(A), 0, o);
        if (ec_ux_lanes(o->n) > 1)                 /* (its active lane's four) */
            {
                const int l = A->ux.lane[c] < ec_ux_lanes(o->n) ? A->ux.lane[c] : 0;
                int i;
                for (i = 0; i < 4 && l * 4 + i < o->n; i++)
                    o->it[i] = o->it[l * 4 + i];
                o->n = i;
            }
        return 1;
    }
    if (!A->scr && A->cat == EC_CAT_XFD) {         /* the layout's fader cell, then the scene counts */
        int has_glide = 0, i;
        ec_page_items(EC_CAT_XFD, A->page[EC_CAT_XFD], ec_app_params(A), 0, o);
        for (i = 0; i < o->n; i++)
            has_glide |= o->it[i].kind == EC_IT_UX;
        if (!has_glide && o->n <= 1) {
            if (o->n == 1 && o->it[0].kind == EC_IT_E5 && o->it[0].id == EC_X_XFD)
                o->it[0].label = "POSITION";
            add(o, EC_IT_UX, EC_UX_GLIDE, "GLIDE");
            add(o, EC_IT_UX, EC_UX_ALOCKS, "A LOCKS");
            add(o, EC_IT_UX, EC_UX_BLOCKS, "B LOCKS");
        }
        return 1;
    }
    return 0;
}

static void loop_pages(char *out, int cap)
{
    const int pages = ec_pages_count();
    int i, first = -1, last = -1, n = 0;
    for (i = 0; i < pages && i < 16; i++)
        if (ec_e5_page_in_loop(&A->proj, A->track, i, pages)) {
            if (first < 0)
                first = i;
            last = i;
            n++;
        }
    if (n == pages || first < 0)
        snprintf(out, (size_t)cap, "ALL");
    else if (n == last - first + 1)
        snprintf(out, (size_t)cap, first == last ? "%d" : "%d-%d", first + 1, last + 1);
    else
        snprintf(out, (size_t)cap, "%d PGS", n);
}

/* FILE: the file under the browser's cursor: its pool slot's sample, or the preview's */
static const ec_ss_sample_t *cursor_sample(const char **path)
{
    int d, f, s;
    const char *p;
    ec_file_entry(A->cursor, &d, &f);
    p = !d && f >= 0 ? ec_browse_path(f) : 0;
    if (path)
        *path = p;
    if (!p)
        return 0;
    s = ec_file_pooled(p);
    if (s >= 0 && A->slot[s].handle >= 0 && ec_ss_ready(A->slot[s].handle))
        return ec_ss_ready(A->slot[s].handle);
    if (P->pv_state == EC_PV_READY && P->pv_handle >= 0 && !strcmp(P->pv_path, p))
        return ec_ss_ready(P->pv_handle);
    return 0;
}

static float free_mib(void) { return (float)ec_ss_free_frames() * 4.0f / 1048576.0f; }

float ec_pn_item_value(int id)
{
    switch (id) {
    case EC_UX_FSLOT: return (float)(ec_file_next_free() + 1);
    case EC_UX_FLEN: {
        const ec_ss_sample_t *x = cursor_sample(0);
        return x && x->rate ? (float)x->frames / (float)x->rate : 0;
    }
    case EC_UX_FBPM: {
        const char *p;
        const ec_ss_sample_t *x = cursor_sample(&p);
        const int s = p ? ec_file_pooled(p) : -1;
        if (s >= 0)
            return A->proj.slot[s].bpm;
        if (x && x->bpm > 0)
            return x->bpm;
        return p ? ec_bpm_from_name(strrchr(p, '/') ? strrchr(p, '/') + 1 : p) : 0;
    }
    case EC_UX_FREE: return free_mib();
    case EC_UX_SLEN: return (float)A->rec.len;
    case EC_UX_SGAIN: return P->smp_gain_db;
    case EC_UX_SMON: return (float)P->smp_mon;
    case EC_UX_TARGET: return (float)P->target;
    case EC_UX_OCT: return (float)P->oct;
    case EC_UX_GLIDE: return (float)ec_as->glide;
    case EC_UX_ALOCKS: return (float)ec_scene_count(&A->proj.e5.sc, 0);
    case EC_UX_BLOCKS: return (float)ec_scene_count(&A->proj.e5.sc, 1);
    case EC_UX_ADVANCE: return (float)ec_as->advance;
    case EC_UX_PLAYING: return (float)A->proj.current;
    case EC_UX_NEXT: return (float)A->seq.pending;
    default: return 0;
    }
}

void ec_pn_item_format(int id, float x, char *out, int cap)
{
    static const char *const G[4] = {"INST", "FAST", "MED", "SLOW"};
    const int n = (int)floorf(x + 0.5f);
    switch (id) {
    case EC_UX_TARGET:
        if (n <= 0)
            snprintf(out, (size_t)cap, "LIVE");
        else
            snprintf(out, (size_t)cap, "%d", n);
        break;
    case EC_UX_OCT: snprintf(out, (size_t)cap, "%+d", n); break;
    case EC_UX_PLOOP: loop_pages(out, cap); break;
    case EC_UX_GLIDE: snprintf(out, (size_t)cap, "%s", G[clampi(n, 0, 3)]); break;
    case EC_UX_ADVANCE: snprintf(out, (size_t)cap, "%s", n ? "ON" : "OFF"); break;
    case EC_UX_FSLOT:
        if (n <= 0)
            snprintf(out, (size_t)cap, "FULL");
        else
            snprintf(out, (size_t)cap, "%d", n);
        break;
    case EC_UX_FLEN:
        if (x <= 0)
            snprintf(out, (size_t)cap, "--");
        else
            snprintf(out, (size_t)cap, "%.1fS", (double)x);
        break;
    case EC_UX_FBPM:
        if (x <= 0)
            snprintf(out, (size_t)cap, "--");
        else if (fabsf(x - floorf(x + 0.5f)) < 0.05f)
            snprintf(out, (size_t)cap, "%d", n);
        else
            snprintf(out, (size_t)cap, "%.1f", (double)x);
        break;
    case EC_UX_FREE: snprintf(out, (size_t)cap, "%dM", (int)floorf(x)); break;
    case EC_UX_SLEN: {
        static const int B[6] = {0, 1, 2, 4, 8, 16};
        const int b = B[clampi(n, 0, 5)];
        if (b)
            snprintf(out, (size_t)cap, "%d BAR%s", b, b > 1 ? "S" : "");
        else
            snprintf(out, (size_t)cap, "FREE");
    } break;
    case EC_UX_SGAIN: snprintf(out, (size_t)cap, "%+dDB", n); break;
    case EC_UX_SMON: snprintf(out, (size_t)cap, "%s", n ? "ON" : "OFF"); break;
    case EC_UX_PLAYING:
    case EC_UX_NEXT:
        if (n < 0)
            snprintf(out, (size_t)cap, "--");
        else
            snprintf(out, (size_t)cap, "%c%d", 'A' + n / EC_BANK_PATS, n % EC_BANK_PATS + 1);
        break;
    default: snprintf(out, (size_t)cap, "%d", n); break;
    }
}

float ec_pn_item_frac(int id, float x)
{
    switch (id) {
    case EC_UX_TARGET: {
        const int len = ec_seq_pattern_steps(&A->seq, A->track);
        return len ? x / (float)len : 0;
    }
    case EC_UX_OCT: return (x + 3.0f) / 6.0f;
    case EC_UX_GLIDE: return x / 3.0f;
    case EC_UX_ALOCKS:
    case EC_UX_BLOCKS: return x > 64 ? 1.0f : x / 64.0f;
    case EC_UX_ADVANCE: return x;
    case EC_UX_PLAYING:
    case EC_UX_NEXT: return x < 0 ? 0 : x / (float)(EC_NPAT - 1);
    case EC_UX_FSLOT: return x / (float)EC_NSLOT;
    case EC_UX_FLEN: return x > 16 ? 1.0f : x / 16.0f;
    case EC_UX_FBPM: return x <= 0 ? 0 : (x - 20.0f) / 280.0f;
    case EC_UX_FREE: {
        const float cap = (float)ec_ss_capacity_bytes() / 1048576.0f;
        return cap > 0 ? x / cap : 0;
    }
    case EC_UX_SLEN: return x / 4.0f;
    case EC_UX_SGAIN: return (x + 24.0f) / 48.0f;
    case EC_UX_SMON: return x;
    default: return 0;
    }
}

void ec_pn_item_edit(int id, int det)
{
    switch (id) {
    case EC_UX_TARGET: ec_kb_target(det); break;
    case EC_UX_OCT: ec_kb_oct(det > 0 ? 1 : -1); break;
    case EC_UX_GLIDE:
        P->as.glide = (uint8_t)clampi(P->as.glide + (det > 0 ? 1 : -1), 0, 3);
        ec_as_store();
        break;
    case EC_UX_ADVANCE:
        P->as.advance = det > 0;
        ec_as_store();
        break;
    case EC_UX_SLEN:                               /* (not while a take is armed or running) */
        if (A->rec.state == EC_RS_IDLE)
            A->rec.len = clampi(A->rec.len + (det > 0 ? 1 : -1), 0, 4);
        break;
    case EC_UX_SGAIN: P->smp_gain_db = (float)clampi((int)P->smp_gain_db + det, -24, 24); break;
    case EC_UX_SMON: P->smp_mon = det > 0; break;
    default: break;
    }
    A->redraw = 1;
}
