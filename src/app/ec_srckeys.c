/* SPDX-License-Identifier: GPL-3.0-only */
/* 0.6.0: the source template (docs/UX-V2.md §1) in FILTER, AMP, MODULATION, LOOP ENGINE, TRIG and FX:
 *   B1..B5   the mode's pages from the layout (ec_cat_key_page; FX: B3..B5 the buses S1 / S2 / MST, ec_cat_key_bus).
 *            Hold B1 = the quick setting (FILTER the filter type, AMP the env mode), changed with K1 or SEL.
 *            0.6.1 (§9, §10): FX and LOOP ENGINE hold menus (ec_holdmenu.h). FX: B1 is the insert key: a tap opens
 *            the INSERT page (both slots; K1-K3 the slot's params, K4 its MIX), a hold = the shown slot's effect
 *            type menu; B3..B5 held = that bus's effect type menu. 0.6.2: the pages are the mode's list (ec_nav.c):
 *            INS 1 P1 .. INS 2 P1 .. are its entries, a key tapped inside its group steps through it. LOOP ENGINE: B1 LOOP = loop mode (slice machines: the slice play mode, else the
 *            chop loop mode), B2 MACHINE = machine, B3 WARP = warp mode, B4 RANGE = voice mode (trig polyphony),
 *            B5 = ADVANCE (hold only: LOOP ENGINE has four pages); a tap still turns to the page.
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
#include "ec_holdmenu.h"
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
    if (key != 1)
        return 0;
    switch (mode) {
    case EC_CAT_FILT: return EC_P_FILTER_TYPE;
    case EC_CAT_AMP: return EC_P_ENV_MODE;
    default: return 0;                         /* 0.6.1: FX and LOOP ENGINE: the hold menus */
    }
}

/* ------------------------------------------------------------------------------------- 0.6.1: FX INSERT --- */
static int nav_turn;                           /* 0.6.2: the page key went down inside its group: its tap steps it */

static int fx_insert_page(int page) { return ec_page_pic(EC_CAT_FX, page, 0) == EC_PIC_FX; }

int ec_src_insert_page(void)
{
    return ec_app_mode() == EC_CAT_FX && !A->bus && !A->settings && fx_insert_page(A->page[EC_CAT_FX]);
}

static int slot_machine(int slot)
{
    const float *v = ec_app_params(A);
    return iv(v, slot ? EC_P_INS2_MACHINE : EC_P_FX_MACHINE);
}

int ec_src_ins_mix(int slot) { return slot ? EC_P_INS2_MIX : ec_fx_mix_id(slot_machine(0)); }

/* the slot's params in the layout's FX/INSERT order for its machine (its MIX / WET left out): ids (INS 2: the
 * EC_P_INS2_Pn they map to) and their labels; returns the count */
static int ins_list(int slot, int *ids, const char **lb, int max)
{
    float w[EC_NP];
    ec_page_t pg;
    int i, n = 0, p;
    const int m = slot_machine(slot);
    memcpy(w, ec_app_params(A), sizeof w);
    w[EC_P_FX_MACHINE] = (float)m;
    for (p = 0; p < ec_cat_pages(EC_CAT_FX, w) && !fx_insert_page(p); p++)
        ;
    if (m == EC_FX_NONE)
        return 0;
    ec_page_items(EC_CAT_FX, p, w, 0, &pg);
    for (i = 0; i < pg.n && n < max; i++) {
        const ec_item_t *it = &pg.it[i];
        int id = it->id;
        if (it->kind != EC_IT_PARAM || id == EC_P_FX_MIX || id == EC_P_FX_WET || id == EC_P_FX_MACHINE)
            continue;
        if (slot) {
            const int k = ec_ins2_index(m, id);
            if (k < 0)
                continue;
            lb[n] = it->label ? it->label : EC_PARAMS[id].label;
            id = EC_P_INS2_P1 + k;
        } else
            lb[n] = it->label;
        ids[n++] = id;
    }
    return n;
}

int ec_src_ins_pages_of(int slot)
{
    int ids[EC_PAGE_ITEMS];
    const char *lb[EC_PAGE_ITEMS];
    const int n = ins_list(slot, ids, lb, EC_PAGE_ITEMS);
    return n > 3 ? (n + 2) / 3 : 1;
}

int ec_src_ins_pages(void) { return ec_src_ins_pages_of(A->pn.ins_slot); }

int ec_src_ins_param(int slot, int i)
{
    int ids[EC_PAGE_ITEMS];
    const char *lb[EC_PAGE_ITEMS];
    const int n = ins_list(slot, ids, lb, EC_PAGE_ITEMS);
    return i >= 0 && i < n ? ids[i] : -1;
}

void ec_src_ins_items(void *o)
{
    ec_page_t *pg = (ec_page_t *)o;
    int ids[EC_PAGE_ITEMS], i, n;
    const char *lb[EC_PAGE_ITEMS];
    const int slot = A->pn.ins_slot;
    memset(pg, 0, sizeof *pg);
    pg->title = slot ? "INS 2" : "INS 1";
    pg->pic = EC_PIC_FX;
    pg->arg = -1;
    n = ins_list(slot, ids, lb, EC_PAGE_ITEMS);
    if (A->pn.ins_page * 3 >= n)
        A->pn.ins_page = 0;
    for (i = 0; i < 3; i++) {
        const int k = A->pn.ins_page * 3 + i;
        ec_item_t *it = &pg->it[i];
        if (k < n) {
            it->kind = EC_IT_PARAM;
            it->id = (uint8_t)ids[k];
            it->label = lb[k];
        }
    }
    if (slot_machine(slot) != EC_FX_NONE) {        /* K4: the effect's MIX, on every type */
        pg->it[3].kind = EC_IT_PARAM;
        pg->it[3].id = (uint8_t)ec_src_ins_mix(slot);
        pg->it[3].label = "MIX";
    }
    pg->n = 4;
}

float ec_src_load(void) { return ec_dsp_load(A->dsp); }

/* the hold menus' arming (a key going down in FX / LOOP ENGINE) */
static void arm_menu(int mode, int key)
{
    if (mode == EC_CAT_FX) {
        const int bus = ec_cat_key_bus(EC_CAT_FX, key);
        if (bus)
            ec_hm_arm(key, EC_HM_PARAM, EC_P_FX_MACHINE, bus == 1 ? "S1 TYPE" : bus == 2 ? "S2 TYPE" : "MST TYPE");
        else if (ec_cat_key_page(EC_CAT_FX, key) >= 0 && fx_insert_page(ec_cat_key_page(EC_CAT_FX, key)))
            ec_hm_arm(key, A->pn.ins_slot ? EC_HM_INS2 : EC_HM_PARAM, EC_P_FX_MACHINE,
                      A->pn.ins_slot ? "INS 2 TYPE" : "INS 1 TYPE");
        return;
    }
    if (mode != EC_CAT_SRC)
        return;
    switch (key) {
    case 1: {
        const int m = iv(ec_app_params(A), EC_P_MACHINE);
        const int slice = m >= EC_MACH_GRID_SLICE && m <= EC_MACH_RAZOR_POLY;
        ec_hm_arm(key, EC_HM_PARAM, slice ? EC_P_SLICE_PLAY_MODE : EC_P_CHOP_LOOP_MODE, "LOOP MODE");
    } break;
    case 2: ec_hm_arm(key, EC_HM_PARAM, EC_P_MACHINE, "MACHINE"); break;
    case 3: ec_hm_arm(key, EC_HM_PARAM, EC_P_MODE, "WARP MODE"); break;
    case 4: ec_hm_arm(key, EC_HM_PARAM, EC_P_TRIG_POLY, "VOICE MODE"); break;
    case 5: ec_hm_arm(key, EC_HM_ADVANCE, 0, "ADVANCE"); break;
    default: break;
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
    /* 0.6.2: the page list (ec_nav.c): another group's key jumps to its first page now (FX: B1 INS 1 P1, B3..B5 the
     * bus); inside its group the tap (on release, when the hold did nothing) steps through the group */
    nav_turn = ec_nav_key_down(mode, key) == 2 ? key : 0;
    arm_menu(mode, key);                           /* 0.6.1: FX's insert / bus keys, LOOP ENGINE's B1..B5 */
    if (quick_param(mode, key))
        P->slot_key = key;
    A->redraw = 1;
}

void ec_src_up(int key)
{
    int held = 0;
    if (ec_hm_armed() == key)                      /* 0.6.1: a hold menu: let go = the pick; else a tap */
        held = ec_hm_up(key);
    if (nav_turn == key) {                         /* 0.6.2: tapped inside its group: the group's next page */
        if (!held && !(P->bk_used >> (key - 1) & 1u))
            ec_nav_key_tap(ec_app_mode(), key);
        nav_turn = 0;
    }
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
    if (ec_hm_knob(role, det)) {                   /* 0.6.1: a hold menu (SEL / K1 move it) */
        if (ec_hm_armed())
            P->bk_used |= (uint16_t)(1u << (ec_hm_armed() - 1));
        return 1;
    }
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

/* 0.6.1: the FX engine pick by white keys is gone (the type menu replaces it) */
int ec_src_white(int i)
{
    (void)i;
    return 0;
}

int ec_src_hold_tick(void)
{
    ec_hm_tick();
    return 0;
}
