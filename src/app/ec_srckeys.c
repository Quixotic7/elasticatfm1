/* SPDX-License-Identifier: GPL-3.0-only */
/* 0.6.0: the source template (docs/UX-V2.md §1) in FILTER, AMP, MODULATION, LOOP ENGINE, TRIG and FX:
 *   B1..B5   the mode's pages from the layout (ec_cat_key_page). 0.6.3 FX: B1-B4 the EFFECTS rows INSERT 1, INSERT 2,
 *            SEND 1, SEND 2 (held: the slot's type; on SENDS a hold swaps the inserts / sends), B5 SENDS / MASTER.
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
#include "ec_scene.h"

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

/* ------------------------------------------------------------------- 0.6.3: FX EFFECTS / SENDS / MASTER --- */
/* docs/UX-V2.md §10 "FX layout": one EFFECTS page with four rows (the slots INSERT 1, INSERT 2, SEND 1, SEND 2), B1-B4
 * select them and step their knob pages; B5 toggles SENDS / MASTER (slot 4, the master bus); B11 FX MIX. A bus slot is
 * edited through A->bus (S1 = slot 2, S2 = 3, MST = 4: the bus's record), the inserts with A->bus 0 and pn.ins_slot.
 * Every slot's knobs: K1-K3 its params in threes (the layout's FX/EFFECTS order for its machine), K4 its MIX. */
static int nav_turn;                           /* 0.6.2: the page key went down inside its group: its tap steps it */
static int nav_defer;                          /* 0.6.62: a key with a hold: its page on a quick release */
static int swap_key;                           /* 0.6.3: SENDS: a held B1-B4 swaps (the tap navigates on release) */

static int fx_pic_page(int pic)
{
    int p;
    for (p = 0; p < ec_cat_pages(EC_CAT_FX, 0); p++)
        if (ec_page_pic(EC_CAT_FX, p, 0) == pic)
            return p;
    return -1;
}
int ec_fx_effects_page(void) { return fx_pic_page(EC_PIC_FX); }
int ec_fx_sends_page(void) { return fx_pic_page(EC_PIC_SENDS); }

int ec_fx_slot(void)
{
    if (A->bus >= 1 && A->bus <= 3)
        return A->bus + 1;
    return A->pn.ins_slot == 1 ? 1 : 0;
}

/* FX shows a slot's knobs: the EFFECTS page, or any FX page with a bus targeted (S1 / S2 its row, MST: MASTER) */
int ec_src_insert_page(void)
{
    return ec_app_mode() == EC_CAT_FX && !A->settings && (A->bus || A->page[EC_CAT_FX] == ec_fx_effects_page());
}
int ec_fx_on_sends(void)
{
    return ec_app_mode() == EC_CAT_FX && !A->settings && !A->bus && A->page[EC_CAT_FX] == ec_fx_sends_page();
}

int ec_fx_slot_machine(int slot)
{
    const float *v = ec_project_track(&A->proj, A->track)->v;   /* 0.6.92: the inserts are the track's, always */
    if (slot >= 2 && slot <= 4)
        return iv(A->proj.bus[slot - 2], EC_P_FX_MACHINE);
    return iv(v, slot == 1 ? EC_P_INS2_MACHINE : EC_P_FX_MACHINE);
}

int ec_src_ins_mix(int slot) { return slot == 1 ? EC_P_INS2_MIX : ec_fx_mix_id(ec_fx_slot_machine(slot)); }

/* the slot's params in the layout's FX/EFFECTS order for its machine (its MIX / WET left out): ids (INS 2: the
 * EC_P_INS2_Pn they map to; a bus: the bus record's own ids) and their labels; returns the count */
static int ins_list(int slot, int *ids, const char **lb, int max)
{
    float w[EC_NP];
    ec_page_t pg;
    int i, n = 0;
    const int m = ec_fx_slot_machine(slot), p = ec_fx_effects_page();
    if (m == EC_FX_NONE || p < 0)
        return 0;
    memcpy(w, ec_app_params(A), sizeof w);
    w[EC_P_FX_MACHINE] = (float)m;
    ec_page_items(EC_CAT_FX, p, w, 0, &pg);
    for (i = 0; i < pg.n && n < max; i++) {
        const ec_item_t *it = &pg.it[i];
        int id = it->id;
        if (it->kind != EC_IT_PARAM || id == EC_P_FX_MIX || id == EC_P_FX_WET || id == EC_P_FX_MACHINE)
            continue;
        if (slot == 1) {
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

int ec_src_ins_pages(void) { return ec_src_ins_pages_of(ec_fx_slot()); }

int ec_src_ins_param(int slot, int i)
{
    int ids[EC_PAGE_ITEMS];
    const char *lb[EC_PAGE_ITEMS];
    const int n = ins_list(slot, ids, lb, EC_PAGE_ITEMS);
    return i >= 0 && i < n ? ids[i] : -1;
}

const char *ec_fx_slot_name(int slot)
{
    static const char *const N[5] = {"INSERT 1", "INSERT 2", "SEND 1", "SEND 2", "MASTER"};
    return slot >= 0 && slot < 5 ? N[slot] : "";
}

void ec_src_ins_items(void *o)
{
    ec_page_t *pg = (ec_page_t *)o;
    int ids[EC_PAGE_ITEMS], i, n;
    const char *lb[EC_PAGE_ITEMS];
    const int slot = ec_fx_slot();
    memset(pg, 0, sizeof *pg);
    pg->title = ec_fx_slot_name(slot);
    pg->pic = EC_PIC_FX;
    pg->arg = -1;
    pg->busv = slot >= 2 ? slot - 1 : 0;
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
    if (ec_fx_slot_machine(slot) != EC_FX_NONE) {  /* K4: the effect's MIX, on every type and slot */
        pg->it[3].kind = EC_IT_PARAM;
        pg->it[3].id = (uint8_t)ec_src_ins_mix(slot);
        pg->it[3].label = "MIX";
    }
    pg->n = 4;
}

/* FX SETUP (§10): every slot's effect type, then SEND MODE */
void ec_fx_setup_items(void *o)
{
    ec_page_t *pg = (ec_page_t *)o;
    int i;
    memset(pg, 0, sizeof *pg);
    pg->title = "SETUP";
    pg->pic = EC_PIC_STEPS;
    pg->arg = -1;
    /* the row of the place the knobs edit is its own param (INSERT 1's fx_insert1_machine on the track; with a bus
     * targeted, that bus's record), as the layout places them; the others are the panel's (EC_UX_FXT0 + slot) */
    const int own = A->bus ? A->bus + 1 : 0;
    for (i = 0; i < 5; i++)
        pg->it[i].kind = i == own ? EC_IT_PARAM : EC_IT_UX,
        pg->it[i].id = (uint8_t)(i == own ? EC_P_FX_MACHINE : EC_UX_FXT0 + i), pg->it[i].label = ec_fx_slot_name(i);
    pg->it[5].kind = A->bus ? EC_IT_UX : EC_IT_PARAM, pg->it[5].id = A->bus ? EC_UX_SMODE : EC_P_SEND_TAP,
    pg->it[5].label = "SEND MODE";
    pg->n = 6;
}

void ec_fx_type_step(int slot, int det)
{
    ec_fx_type_set(slot, ec_fx_slot_machine(slot) + (det > 0 ? 1 : -1));
}

/* 0.6.92: slot's effect type = nm (1..EC_FX_COUNT), whichever slot the knobs show (a bus: its record, A->bus or not) */
void ec_fx_type_set(int slot, int nm)
{
    float *v = ec_project_track(&A->proj, A->track)->v;
    if (nm < 1 || nm > EC_FX_COUNT || slot < 0 || slot > 4 || nm == ec_fx_slot_machine(slot))
        return;
    if (slot == 1) {                               /* INSERT 2: its MIX and P1..P8 to the machine's defaults */
        uint8_t ids[2 + EC_INS2_NP];
        int k;
        ids[0] = EC_P_INS2_MACHINE;
        ids[1] = EC_P_INS2_MIX;
        for (k = 0; k < EC_INS2_NP; k++)
            ids[2 + k] = (uint8_t)(EC_P_INS2_P1 + k);
        ec_e5_edit_params(A->track, ids, 2 + EC_INS2_NP, "INS2 TYPE");
        v[EC_P_INS2_MACHINE] = (float)nm;
        ec_ins2_reset(v);
        ec_seq_touch(&A->seq, A->track);
    } else if (slot == 0) {
        ec_e5_edit_param(A->track, EC_P_FX_MACHINE, 0);
        v[EC_P_FX_MACHINE] = (float)nm;
        ec_seq_touch(&A->seq, A->track);
    } else {                                       /* a bus's record (global; no undo, as its knobs) */
        A->proj.bus[slot - 2][EC_P_FX_MACHINE] = (float)nm;
        ec_scene_knob(&A->proj, EC_NTRACK + slot - 2, EC_P_FX_MACHINE);
    }
    if (slot == ec_fx_slot())
        A->pn.ins_page = 0;
    ec_app_mark_dirty();
}

/* 0.6.92 (owner): the EFFECTS page, ALGO = the shown slot's level: an insert's MIX, a send's level from the track
 * (SEND 1 / 2 of the track selected), MASTER its MIX */
void ec_fx_algo(int det)
{
    const int slot = ec_fx_slot();
    if (slot == 2 || slot == 3) {                  /* 0.6.93 (owner): no pop-up: the page shows the level */
        const int b = A->bus;
        A->bus = 0;                                /* (the track's own send, though the knobs show the bus) */
        ec_app_edit_param_f(slot == 2 ? EC_P_SEND1 : EC_P_SEND2, det, ec_ux_accel(EC_E_ALGO, 100, det), 0);
        A->bus = b;
    } else if (ec_fx_slot_machine(slot) != EC_FX_NONE)
        ec_app_edit_param_f(ec_src_ins_mix(slot), det, ec_ux_accel(EC_E_ALGO, 100, det), 0);
    A->redraw = 1;
}

/* 0.6.92 (owner): the EFFECTS page, PRESETS = the shown slot's effect type */
void ec_fx_presets(int det)
{
    const int slot = ec_fx_slot();
    char b[32];
    ec_fx_type_step(slot, det);
    snprintf(b, sizeof b, "%s %s", ec_fx_slot_name(slot),
             ec_fx_slot_machine(slot) > 1 ? ec_fx_short_name(ec_fx_slot_machine(slot)) : "OFF");
    ec_app_message(b);
}

/* SENDS (§10): K1 / K2 the inserts' MIX (in series: their "send"), K3 / K4 the send levels */
void ec_fx_sends_items(void *o)
{
    ec_page_t *pg = (ec_page_t *)o;
    static const char *const LB[4] = {"INS 1", "INS 2", "SEND 1", "SEND 2"};
    const int id[4] = {ec_src_ins_mix(0), EC_P_INS2_MIX, EC_P_SEND1, EC_P_SEND2};
    int i;
    memset(pg, 0, sizeof *pg);
    pg->title = "SENDS";
    pg->pic = EC_PIC_SENDS;
    pg->arg = -1;
    for (i = 0; i < 4; i++)
        pg->it[i].kind = EC_IT_PARAM, pg->it[i].id = (uint8_t)id[i], pg->it[i].label = LB[i];
    pg->n = 4;
}

/* SENDS: ALGO = the send mode PRE / POST (send_tap; an edit like a knob's: undo, a lock on held steps) */
void ec_fx_send_mode(int det)
{
    ec_app_edit_param_f(EC_P_SEND_TAP, det > 0 ? 1 : -1, 1, 0);
    ec_app_message(iv(ec_app_params(A), EC_P_SEND_TAP) == 2 ? "SEND POST" : "SEND PRE");
}

/* SENDS (0.6.4): a raised box moved by K1: which 0 swaps the inserts, 1 the sends; the undo (the swap again) is
 * recorded once, when the key is let go with the box moved */
void ec_fx_swap_do(int which)
{
    if (which)
        ec_project_swap_sends(&A->proj);
    else
        ec_project_swap_inserts(&A->proj, A->track);
    ec_seq_touch_all(&A->seq);
    ec_e5_xf_apply(A->proj.e5.sc.pos);
    ec_app_mark_dirty();
}

void ec_fx_swap_undo(int which) { ec_undo_fxswap(&A->e5.undo, which ? -1 : A->track, A->ms); }

/* FX's key for the place shown (the lit key): B1..B4 the EFFECTS rows, B5 SENDS / MASTER, else the layout's */
int ec_fx_cur_key(void)
{
    if (A->bus == 3)
        return 5;
    if (ec_src_insert_page())
        return ec_fx_slot() + 1;
    if (A->page[EC_CAT_FX] == ec_fx_sends_page())
        return 5;
    return ec_cat_page_key(EC_CAT_FX, A->page[EC_CAT_FX]);
}

float ec_src_load(void) { return ec_dsp_load(A->dsp); }

/* the hold menus' arming (a key going down in FX / LOOP ENGINE) */
static void arm_menu(int mode, int key)
{
    if (mode == EC_CAT_FX) {                       /* 0.6.3: B1-B4 the slot's type, B5 on MASTER the master's */
        static const char *const HEAD[5] = {"INSERT 1 TYPE", "INSERT 2 TYPE", "SEND 1 TYPE", "SEND 2 TYPE",
                                            "MASTER TYPE"};
        if (key >= 1 && key <= 5)                  /* 0.6.92 (owner): each key its own slot's type (not the slot the
                                                    * knobs show); B5 MASTER's from every FX page */
            ec_hm_arm(key, EC_HM_FXTYPE, key - 1, HEAD[key - 1]);
        return;
    }
    if (mode != EC_CAT_SRC)
        return;
    switch (key) {                                 /* 0.6.41 (owner): B1 TRIG, B2-B5 LOOP, MACHINE, WARP, RANGE */
    case 1: ec_hm_arm(key, EC_HM_ADVANCE, 0, "ADVANCE"); break;
    case 2: {
        const int m = iv(ec_app_params(A), EC_P_MACHINE);
        const int slice = m >= EC_MACH_GRID_SLICE && m <= EC_MACH_RAZOR_POLY;
        ec_hm_arm(key, EC_HM_PARAM, slice ? EC_P_SLICE_PLAY_MODE : EC_P_CHOP_LOOP_MODE, "LOOP MODE");
    } break;
    case 3: ec_hm_arm(key, EC_HM_PARAM, EC_P_MACHINE, "MACHINE"); break;
    case 4: ec_hm_arm(key, EC_HM_PARAM, EC_P_MODE, "WARP MODE"); break;
    case 5: ec_hm_arm(key, EC_HM_PARAM, EC_P_TRIG_POLY, "VOICE MODE"); break;
    default: break;
    }
}

/* 0.6.74: KEYBOARD's IN SCALE black keys B1..B5 hold LOOP ENGINE's menus */
void ec_src_arm_loop_menu(int key)
{
    arm_menu(EC_CAT_SRC, key);
    A->redraw = 1;
}

static int special(int mode)
{
    return mode == EC_CAT_FILT || mode == EC_CAT_SRC || mode == EC_CAT_TRIG || ec_cat_key_page(mode, 11) >= 0;
}

char ec_src_role(int mode, int key)
{
    if (mode == EC_CAT_MOD && key >= 2 && key <= 5)
        return 'S';
    if (key >= 6 && key <= 10)
        return 'S';
    if (key == 11)
        return special(mode) ? 'X' : 'O';
    if (mode == EC_CAT_FX && key >= 1 && key <= 5)  /* 0.6.3: the EFFECTS rows, SENDS / MASTER */
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
        if (mode == EC_CAT_SRC || mode == EC_CAT_TRIG)
            return "STEP";                         /* 0.6.6: the step layer (held: the pages) */
        if (mode == EC_CAT_FILT)
            return iv(ec_app_params(A), EC_P_FENV_MODE) == 2 ? "ENV AHR" : "ENV ADSR";
    }
    if (mode == EC_CAT_FX && key >= 1 && key <= 5) {   /* 0.6.3 */
        static const char *const FXK[5] = {"INS1", "INS2", "SEND1", "SEND2", "SND/MST"};
        return FXK[key - 1];
    }
    p = ec_cat_key_page(mode, key);
    if (p < 0)
        return "";
    {
        ec_page_t pg;
        const int m = iv(ec_app_params(A), EC_P_MACHINE);
        ec_page_items(mode, p, ec_app_params(A), 0, &pg);
        if (mode == EC_CAT_SRC && !A->bus && pg.title && !strcmp(pg.title, "LOOP"))
            return ec_machine_name(m);             /* 0.6.19 (owner): the LOOP page is named after the machine */
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

/* LOOP ENGINE's zoom: 16 loop keys -> 32 (the first 16) -> 32 (the second 16). 0.6.6: B11 held + K1 (B11 itself
 * picks the target step; SHIFT + B11 is FILL): K1 right steps it on, left back */
static void zoom(int det)
{
    float *v = ec_app_params(A);
    {
        const int m = iv(v, EC_P_MACHINE);
        if (m >= EC_MACH_GRID_SLICE && m <= EC_MACH_RAZOR_POLY) {   /* 0.6.40 (owner): slice tracks: the pads' bank,
                                                    * 1-16 / 17-32 (LOOP DIV is the loop keys', not touched) */
            P->zoom_half = det > 0 && iv(v, EC_P_SLICE_COUNT) > 16;   /* 0.6.58 (owner): 16 slices or fewer: only
                                                    * the first bank */
            ec_app_message(P->zoom_half ? "SLICES 17-32" : "SLICES 1-16");
            return;
        }
    }
    const int div = iv(v, EC_P_LOOP_DIVISION);
    const int at = div == 32 ? (P->zoom_half ? 2 : 1) : 0;
    const int to = at + (det > 0 ? 1 : -1) < 0 ? 0 : at + (det > 0 ? 1 : -1) > 2 ? 2 : at + (det > 0 ? 1 : -1);
    int nd = to ? 32 : 16, half = to == 2;
    char b[16];
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

/* B11's own tap on a page that is not a step mode: FILTER the envelope's mode, FX / MOD its page key */
static void special_tap(int mode)
{
    if (mode == EC_CAT_FILT) {                     /* the filter envelope's mode */
        float *v = ec_app_params(A);
        ec_e5_edit_param(A->track, EC_P_FENV_MODE, 0);
        v[EC_P_FENV_MODE] = iv(v, EC_P_FENV_MODE) == 2 ? 1.0f : 2.0f;
        ec_seq_touch(&A->seq, A->track);
        ec_app_mark_dirty();
        ec_app_message(iv(v, EC_P_FENV_MODE) == 2 ? "FILTER ENV AHR" : "FILTER ENV ADSR");
        return;
    }
    ec_nav_key(mode, 11);
    A->redraw = 1;
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
    if (key == 11 && ec_lp_sel_mode()) {             /* 0.6.6 / 0.6.14: B11 held: the layers, the pages, K1 / K2 (every
                                                    * track page); a quick click: the page's own B11 (ec_src_up) */
        ec_lp_b11(1);
        return;
    }
    if (key == 11 && mode == EC_CAT_FILT) {
        special_tap(mode);
        return;
    }
    /* 0.6.2: the page list (ec_nav.c): another group's key jumps to its first page now (FX: B1 INS 1 P1, B3..B5 the
     * bus); inside its group the tap (on release, when the hold did nothing) steps through the group */
    if (mode == EC_CAT_FX && key >= 1 && key <= 4 && ec_fx_on_sends()) {   /* 0.6.3: SENDS: a hold swaps */
        swap_key = key;
        ec_hm_arm(key, EC_HM_SWAP, key <= 2 ? 0 : 1, key <= 2 ? "MOVE INSERT" : "MOVE SEND");
        A->redraw = 1;
        return;
    }
    arm_menu(mode, key);                           /* 0.6.1: FX's insert / bus keys, LOOP ENGINE's B1..B5 */
    {
        const int qp = quick_param(mode, key);
        if (qp)
            P->slot_key = key;
        if (qp || ec_hm_armed() == key)            /* 0.6.62 (owner): a key with a hold (its menu, its quick
                                                    * setting): the page only on a quick click, on release */
            nav_defer = key;
        else
            nav_turn = ec_nav_key_down(mode, key) == 2 ? key : 0;
    }
    A->redraw = 1;
}

void ec_src_up(int key)
{
    int held = 0;
    if (key == 11 && ec_lp_b11_held()) {           /* 0.6.6: B11 let go; 0.6.14: a quick click = the page's own tap */
        if (ec_lp_b11(0))
            special_tap(ec_app_mode());
        return;
    }
    if (ec_hm_armed() == key)                      /* 0.6.1: a hold menu: let go = the pick; else a tap */
        held = ec_hm_up(key);
    if (swap_key == key) {                         /* 0.6.3: SENDS: not held long enough to swap: a tap */
        swap_key = 0;
        if (!held)
            ec_nav_key(ec_app_mode(), key);
    }
    if (nav_defer == key) {                        /* 0.6.62: a quick click (no menu, no knob): the page */
        if (!held && !(P->bk_used >> (key - 1) & 1u))
            ec_nav_key(ec_app_mode(), key);
        nav_defer = 0;
    }
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

static int clampi(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }

int ec_src_knob(int role, int det)
{
    const int mode = ec_app_mode();
    int id;
    if (ec_lp_b11_held() && role == EC_E_K4) {    /* 0.6.6: LOOP ENGINE: B11 held + a knob = the zoom (slice tracks:
                                                    * the pads' bank); 0.6.86 (owner): K4 only */
        ec_lp_b11_used();
        for (; det; det += det > 0 ? -1 : 1)       /* a detent a step */
            zoom(det);
        return 1;
    }
    if (ec_lp_b11_held() && role == EC_E_PRESETS) /* 0.6.86 (owner): B11 held: PRESETS = K1 (LENGTH), ALGO = K2 (RATE) */
        role = EC_E_K1;
    else if (ec_lp_b11_held() && role == EC_E_ALGO)
        role = EC_E_K2;
    if ((ec_lp_b11_held() || ec_app_mode() == EC_MODE_PAGES) && (role == EC_E_K1 || role == EC_E_K2) && !A->bus) {   /* 0.6.14 (owner): B11 held (PAGES: always): K1
                                                    * the pattern length, K2 the track rate (the pages panel) */
        float *v = ec_app_params(A);
        char b[20];
        ec_lp_b11_used();
        if (role == EC_E_K1) {
            const int n = clampi(iv(v, EC_P_PATTERN_STEPS) + det * ec_ux_accel(role, 128, det), 1, 128);
            if (n != iv(v, EC_P_PATTERN_STEPS)) {
                ec_e5_edit_param(A->track, EC_P_PATTERN_STEPS, 0);
                v[EC_P_PATTERN_STEPS] = (float)n;
                ec_seq_timing_changed(&A->seq);
                ec_seq_touch(&A->seq, A->track);
                ec_app_mark_dirty();
            }
            snprintf(b, sizeof b, "LEN %d", n);
        } else {
            const int n = clampi(iv(v, EC_P_RATE) + (det > 0 ? 1 : -1), 1, EC_RATES);
            if (n != iv(v, EC_P_RATE)) {
                ec_e5_edit_param(A->track, EC_P_RATE, 0);
                v[EC_P_RATE] = (float)n;
                ec_seq_timing_changed(&A->seq);
                ec_seq_touch(&A->seq, A->track);
                ec_app_mark_dirty();
            }
            snprintf(b, sizeof b, "RATE %s", ec_rate_label(n));
        }
        ec_app_message(b);
        return 1;
    }
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
