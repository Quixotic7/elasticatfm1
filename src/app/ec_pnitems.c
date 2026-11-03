/* SPDX-License-Identifier: GPL-3.0-only */
/* 0.6.0: the cells of the screens without params of their own (the approved mockups): KEYBOARD (SLICE or OCT, PITCH,
 * VEL, STEP), PAGES (LENGTH, RATE, LOOP), CROSSFADER (the layout's fader cell + GLIDE, A LOCKS, B LOCKS), and the
 * panel's own items (EC_IT_UX) the cells and settings lists show. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ec_app.h"
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
    case EC_MODE_SAMPLER: return 0;
    default: break;
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

float ec_pn_item_value(int id)
{
    switch (id) {
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
    default: break;
    }
    A->redraw = 1;
}
