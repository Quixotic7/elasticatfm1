/* SPDX-License-Identifier: GPL-3.0-only */
/* 0.6.1: the hold menu (ec_holdmenu.h). */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ec_app.h"
#include "ec_holdmenu.h"
#include "ec_scene.h"

#define A (&ec_app)

static struct {
    int key;              /* the black key holding it, 0 none */
    int on;               /* showing */
    int kind, id;
    int n, cur, now;      /* values, the cursor, the value set when it armed */
    int moved;
    uint32_t ms;
    char head[24];
} g;

static int iv(float x) { return (int)floorf(x + 0.5f); }

/* the value array the menu edits (a bus's record for FX's INS 1 on a bus) */
static float *values(void)
{
    if (g.kind == EC_HM_PARAM) {
        float *bv = ec_app_bus_params(A, g.id);
        if (bv)
            return bv;
    }
    return ec_app_params(A);
}

static int skip(int i)                    /* NEIGHBOR needs a left neighbour: never on T1 */
{
    return g.kind == EC_HM_PARAM && g.id == EC_P_MACHINE && A->track == 0 && i + 1 == EC_MACH_NEIGHBOR;
}

void ec_hm_arm(int key, int kind, int id, const char *head)
{
    memset(&g, 0, sizeof g);
    g.key = key;
    g.kind = kind;
    g.id = kind == EC_HM_INS2 ? EC_P_INS2_MACHINE : id;
    g.ms = A->ms;
    snprintf(g.head, sizeof g.head, "%s", head ? head : "");
    if (kind == EC_HM_ADVANCE) {
        g.n = 2;
        g.now = ec_as->advance ? 1 : 0;
    } else {
        g.n = EC_PARAMS[g.id].nopt;
        g.now = iv(values()[g.id]) - 1;
        if (g.now < 0 || g.now >= g.n)
            g.now = 0;
    }
    g.cur = g.now;
}

void ec_hm_cancel(void)
{
    if (g.on)
        A->redraw = 1;
    memset(&g, 0, sizeof g);
}

int ec_hm_armed(void) { return g.key; }
int ec_hm_shown(void) { return g.key && g.on; }
const char *ec_hm_head(void) { return g.key && g.on ? g.head : 0; }
int ec_hm_count(void) { return g.n; }
int ec_hm_cursor(void) { return g.cur; }
int ec_hm_current(void) { return g.now; }

const char *ec_hm_label(int i)
{
    if (i < 0 || i >= g.n)
        return "";
    if (g.kind == EC_HM_ADVANCE)
        return i ? "ON" : "OFF";
    if (g.id == EC_P_FX_MACHINE || g.id == EC_P_INS2_MACHINE)
        return i == 0 ? "NONE" : ec_fx_name(i + 1);   /* the long names: "TAPE ECHO" */
    if (g.id == EC_P_MODE)
        return ec_warp_name(i + 1);
    return EC_PARAMS[g.id].opts[i];
}

static void show(void)
{
    if (!g.on) {
        g.on = 1;
        A->redraw = 1;
    }
}

void ec_hm_tick(void)
{
    if (g.key && !g.on && A->ms - g.ms >= EC_HOLD_MS)
        show();
}

int ec_hm_knob(int role, int det)
{
    int c;
    if (!g.key)
        return 0;
    if (role != EC_E_SELECT && role != EC_E_K1)
        return g.on;                              /* the other knobs do nothing while it shows */
    show();
    for (; det; det += det > 0 ? -1 : 1) {        /* a detent a value */
        c = g.cur;
        do
            c += det > 0 ? 1 : -1;
        while (c >= 0 && c < g.n && skip(c));
        if (c >= 0 && c < g.n)
            g.cur = c;
    }
    g.moved = 1;
    A->redraw = 1;
    return 1;
}

/* the pick */
static void apply(void)
{
    float *v;
    const int val = g.cur + 1;
    if (!g.moved || g.cur == g.now)
        return;
    if (g.kind == EC_HM_ADVANCE) {
        ec_pn_item_edit(EC_UX_ADVANCE, g.cur ? 1 : -1);
        return;
    }
    v = values();
    if (g.kind == EC_HM_INS2) {                   /* INS 2: the machine, then its params to that machine's defaults */
        uint8_t ids[2 + EC_INS2_NP];
        int k;
        ids[0] = EC_P_INS2_MACHINE;
        ids[1] = EC_P_INS2_MIX;
        for (k = 0; k < EC_INS2_NP; k++)
            ids[2 + k] = (uint8_t)(EC_P_INS2_P1 + k);
        ec_e5_edit_params(A->track, ids, 2 + EC_INS2_NP, "INS2 TYPE");
        v[EC_P_INS2_MACHINE] = (float)val;
        ec_ins2_reset(v);
        A->pn.ins_page = 0;
        ec_seq_touch(&A->seq, A->track);
        ec_app_mark_dirty();
        return;
    }
    if (g.id == EC_P_MODE && A->seq.playing) {   /* the warp mode changes only while stopped (EC_IF_STOPPED) */
        ec_app_alert("STOP FIRST");
        return;
    }
    if (v != ec_app_params(A)) {                  /* a bus's record (global; no undo, as its knobs) */
        v[g.id] = (float)val;
        ec_scene_knob(&A->proj, EC_NTRACK + A->bus - 1, g.id);
    } else {
        ec_e5_edit_param(A->track, g.id, 0);
        v[g.id] = (float)val;
        ec_seq_touch(&A->seq, A->track);
    }
    if (g.id == EC_P_FX_MACHINE)
        A->pn.ins_page = 0;
    ec_app_mark_dirty();
}

int ec_hm_up(int key)
{
    int shown;
    if (!g.key || g.key != key)
        return 0;
    shown = g.on;
    if (shown)
        apply();
    ec_hm_cancel();
    A->redraw = 1;
    return shown;
}
