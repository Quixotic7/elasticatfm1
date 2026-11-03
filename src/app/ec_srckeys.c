/* SPDX-License-Identifier: GPL-3.0-only */
/* 0.6.0: the source template (docs/UX-V2.md §1) in FILTER, AMP, MODULATION, LOOP ENGINE, TRIG and FX:
 *   B1..B5   the mode's pages from the layout (ec_cat_key_page; FX: B3..B5 the buses S1 / S2 / MST, ec_cat_key_bus).
 *            Hold B1 = the quick setting (FILTER the filter type, AMP the env mode, LOOP ENGINE the machine, FX the
 *            insert's engine; FX's bus keys: that bus's engine), changed with K1 or SEL (FX: also a white key).
 *   B6..B10  the sources LFO 1, LFO 2, MOD ENV, VEL, AT (MODULATION: B2..B5 too, the macros M1..M4): hold one + turn a
 *            value knob = route that source to that param, its depth (ec_mk_turn, the 0.5.0 matrix; F2 while turning
 *            removes the route; held steps = depth locks). 0.6.1: a tap or hold never changes the mode (the header shows
 *            the source on SRC while held, ec_header.c); a tap opens the source's page only when it is a page of the
 *            mode shown (MODULATION: LFO 1, LFO 2, VEL, AT = the MATRIX, M1..M4 = the macro's page; AMP: MOD ENV).
 *   B11      the mode's special key: FX the MIX page, FILTER the filter envelope's mode, MODULATION the MACROS page,
 *            LOOP ENGINE the zoom (16 -> 32 first half -> 32 second half). */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ec_app.h"
#include "ec_mtx.h"

#define A (&ec_app)
#define P (&ec_app.pn)

static int iv(const float *v, int id) { return (int)floorf(v[id] + 0.5f); }

int ec_src_mode(int mode)
{
    return mode == EC_CAT_FX || mode == EC_CAT_FILT || mode == EC_CAT_AMP || mode == EC_CAT_MOD ||
           mode == EC_CAT_SRC || mode == EC_CAT_TRIG;
}

int ec_src_mk_index(int key)
{
    const int mode = ec_app_mode();
    if (!ec_src_mode(mode))
        return -1;
    if (mode == EC_CAT_MOD && key >= 2 && key <= 5)
        return EC_MK_M1 + key - 2;
    switch (key) {
    case 6: return EC_MK_LFO1;
    case 7: return EC_MK_LFO2;
    case 8: return EC_MK_ENV;
    case 9: return EC_MK_VEL;
    case 10: return EC_MK_AT;
    default: return -1;
    }
}

/* the quick setting B1 (or FX's slot key) holds: the param, 0 none */
static int quick_param(int mode, int key)
{
    if (mode == EC_CAT_FX && (key == 1 || ec_cat_key_bus(EC_CAT_FX, key)))
        return EC_P_FX_MACHINE;
    if (key != 1)
        return 0;
    switch (mode) {
    case EC_CAT_FILT: return EC_P_FILTER_TYPE;
    case EC_CAT_AMP: return EC_P_ENV_MODE;
    case EC_CAT_SRC: return EC_P_MACHINE;
    default: return 0;
    }
}

static int special(int mode)
{
    return mode == EC_CAT_FILT || mode == EC_CAT_SRC || ec_cat_key_page(mode, 11) >= 0;
}

char ec_src_role(int mode, int key)
{
    if (mode == EC_CAT_MOD && key >= 2 && key <= 5)
        return 'S';
    if (key >= 6 && key <= 10)
        return 'S';
    if (key == 11)
        return special(mode) ? 'X' : 'O';
    if (mode == EC_CAT_FX && ec_cat_key_bus(EC_CAT_FX, key))
        return 'P';
    return ec_cat_key_page(mode, key) >= 0 ? 'P' : 'O';
}

const char *ec_src_label(int mode, int key)
{
    static const char *const SRC[5] = {"LFO1", "LFO2", "MENV", "VEL", "AT"};
    static char b[12];
    int p;
    if (mode == EC_CAT_MOD && key >= 2 && key <= 5) {
        snprintf(b, sizeof b, "M%d", key - 1);
        return b;
    }
    if (key >= 6 && key <= 10)
        return SRC[key - 6];
    if (key == 11) {
        if (mode == EC_CAT_SRC)
            return "ZOOM";
        if (mode == EC_CAT_FILT)
            return iv(ec_app_params(A), EC_P_FENV_MODE) == 2 ? "ENV AHR" : "ENV ADSR";
    }
    if (mode == EC_CAT_FX) {
        const int bus = ec_cat_key_bus(EC_CAT_FX, key);
        if (bus)
            return bus == 1 ? "SEND1" : bus == 2 ? "SEND2" : "MSTR";
    }
    p = ec_cat_key_page(mode, key);
    if (p < 0)
        return "";
    {
        ec_page_t pg;
        ec_page_items(mode, p, ec_app_params(A), 0, &pg);
        snprintf(b, sizeof b, "%.10s", pg.title ? pg.title : "");
    }
    return b;
}

/* a source key's tap: its page, when that page is in the mode shown (0.6.1: never another mode) */
static void goto_source(int i)
{
    int pic = EC_PIC_MATRIX, arg = -1, c, p;
    switch (i) {
    case EC_MK_LFO1: pic = EC_PIC_LFO1; break;
    case EC_MK_LFO2: pic = EC_PIC_LFO2; break;
    case EC_MK_ENV: pic = EC_PIC_MENV; break;
    case EC_MK_VEL: pic = EC_PIC_VEL; break;
    case EC_MK_AT: break;
    default: pic = EC_PIC_MACRO, arg = i - EC_MK_M1; break;
    }
    if ((ec_page_find(pic, arg, &c, &p) || (pic == EC_PIC_MACRO && ec_page_find(EC_PIC_MACROS, -1, &c, &p))) &&
        c == ec_app_mode() && !A->scr && !A->bus) {
        if (pic == EC_PIC_MATRIX && c == EC_CAT_MOD)
            A->mk.row = 0;
        ec_app_set_page(c, p);
        A->redraw = 1;
    }
}

/* LOOP ENGINE's B11: 16 loop keys -> 32 (the first 16) -> 32 (the second 16) */
static void zoom(void)
{
    float *v = ec_app_params(A);
    const int div = iv(v, EC_P_LOOP_DIVISION);
    int nd = 16, half = 0;
    char b[16];
    if (div == 16)
        nd = 32;
    else if (div == 32 && !P->zoom_half)
        nd = 32, half = 1;
    if (nd != div) {
        ec_e5_edit_param(A->track, EC_P_LOOP_DIVISION, 0);
        v[EC_P_LOOP_DIVISION] = (float)nd;
        ec_seq_touch(&A->seq, A->track);
        ec_app_mark_dirty();
    }
    P->zoom_half = half;
    snprintf(b, sizeof b, "%s", nd == 16 ? "ZOOM 16" : half ? "ZOOM 32 B" : "ZOOM 32 A");
    ec_app_message(b);
}

void ec_src_down(int key)
{
    const int mode = ec_app_mode();
    const int mk = ec_src_mk_index(key);
    int p;
    if (mk >= 0) {                                 /* a source: held (a tap on release: its page) */
        ec_mk_hold(mk);
        P->src_key = key;
        return;
    }
    if (key == 11 && mode == EC_CAT_SRC) {
        zoom();
        return;
    }
    if (key == 11 && mode == EC_CAT_FILT) {        /* the filter envelope's mode */
        float *v = ec_app_params(A);
        ec_e5_edit_param(A->track, EC_P_FENV_MODE, 0);
        v[EC_P_FENV_MODE] = iv(v, EC_P_FENV_MODE) == 2 ? 1.0f : 2.0f;
        ec_seq_touch(&A->seq, A->track);
        ec_app_mark_dirty();
        ec_app_message(iv(v, EC_P_FENV_MODE) == 2 ? "FILTER ENV AHR" : "FILTER ENV ADSR");
        return;
    }
    if (mode == EC_CAT_FX) {                       /* FX: the buses, or the track */
        const int bus = ec_cat_key_bus(EC_CAT_FX, key);
        if (bus) {
            A->bus = bus;
            ec_app_set_page(EC_CAT_FX, 0);
            P->slot_key = key;
            A->redraw = 1;
            return;
        }
        if (ec_cat_key_page(EC_CAT_FX, key) >= 0)
            A->bus = 0;
    }
    p = ec_cat_key_page(mode, key);
    if (p >= 0) {
        ec_app_set_page(mode, p);
        if (mode == EC_CAT_MOD && ec_page_pic(mode, p, 0) == EC_PIC_MATRIX)
            A->mk.row = A->mk.row < 0 ? 0 : A->mk.row;
    }
    if (quick_param(mode, key))
        P->slot_key = key;
    A->redraw = 1;
}

void ec_src_up(int key)
{
    if (P->src_key == key) {
        const int mk = ec_src_mk_index(key);
        P->src_key = 0;
        if (ec_mk_release() && mk >= 0)
            goto_source(mk);
    }
    if (P->slot_key == key)
        P->slot_key = 0;
    A->redraw = 1;
}

/* the quick setting's value as the takeover shows it */
static void show(int id)
{
    char val[24];
    const float *bv = ec_app_bus_params(A, id);
    const float x = bv ? bv[id] : ec_app_params(A)[id];
    ec_param_format(id, x, 3, val, sizeof val);
    ec_ux_takeover(EC_PARAMS[id].label, val, ec_param_frac(id, x));
}

int ec_src_knob(int role, int det)
{
    const int mode = ec_app_mode();
    int id;
    if (!P->slot_key || (role != EC_E_SELECT && role != EC_E_K1))
        return 0;
    id = quick_param(mode, P->slot_key);
    if (!id)
        return 0;
    P->bk_used |= (uint16_t)(1u << (P->slot_key - 1));
    ec_app_edit_param_f(id, det > 0 ? 1 : -1, 1, 0);
    show(id);
    return 1;
}

int ec_src_white(int i)
{
    const int mode = ec_app_mode();
    float *bv;
    if (mode != EC_CAT_FX || quick_param(mode, P->slot_key) != EC_P_FX_MACHINE)
        return 0;
    if ((float)(i + 1) > EC_PARAMS[EC_P_FX_MACHINE].max)
        return 1;
    bv = ec_app_bus_params(A, EC_P_FX_MACHINE);
    if (bv)
        bv[EC_P_FX_MACHINE] = (float)(i + 1);
    else {
        ec_e5_edit_param(A->track, EC_P_FX_MACHINE, 0);
        ec_app_params(A)[EC_P_FX_MACHINE] = (float)(i + 1);
        ec_seq_touch(&A->seq, A->track);
    }
    P->bk_used |= (uint16_t)(1u << (P->slot_key - 1));
    ec_app_mark_dirty();
    show(EC_P_FX_MACHINE);
    return 1;
}

int ec_src_hold_tick(void) { return 0; }
