/* SPDX-License-Identifier: GPL-3.0-only */
/* 0.5.0: the mod matrix on the panel (ec_modkeys.h, docs/MOD-MATRIX.md) */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ec_app.h"
#include "ec_layout.h"
#include "ec_mtx.h"

#define A (&ec_app)
#define MK (&ec_app.mk)

static int clampi(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }
static float *tv(void) { return ec_project_track(&A->proj, A->track)->v; }

static const int KEY_SRC[EC_MK_N] = {EC_MS_LFO1, EC_MS_LFO2, EC_MS_ENV, EC_MS_VEL, EC_MS_AT, EC_MS_M1, EC_MS_M2,
                                     EC_MS_M3, EC_MS_M4, 0, 0};
int ec_mk_src_of_key(int i) { return i >= 0 && i < EC_MK_N ? KEY_SRC[i] : 0; }
const char *ec_mk_key_name(int i)
{
    static const char *const N[EC_MK_N] = {"LFO 1", "LFO 2", "ENV", "VEL", "AT", "M1", "M2", "M3", "M4",
                                           "SCN A", "SCN B"};
    return i >= 0 && i < EC_MK_N ? N[i] : "";
}

/* ------------------------------------------------------------------------------------------- the DST order --- */
/* every param that can be a destination, in the layout's order (category by category, page by page, first place
 * it shows), then any the layout does not place; with its category and its layout label */
static const ec_layout_t *g_lay;
static uint8_t g_ord[EC_NP], g_cat[EC_NP];
static const char *g_lbl[EC_NP];
static int g_n, g_pos[EC_NP];

static void order_build(void)
{
    const ec_layout_t *L = ec_layout();
    int c, p, vv, k, id;
    if (g_lay == L)
        return;
    g_lay = L;
    g_n = 0;
    for (id = 0; id < EC_NP; id++) {
        g_pos[id] = -1;
        g_lbl[id] = 0;
    }
    for (c = 0; c < EC_NCAT; c++)
        for (p = 0; p < L->cat[c].npages; p++) {
            const ec_lay_page_t *pg = &L->page[L->cat[c].page0 + p];
            for (vv = 0; vv < pg->nvar; vv++) {
                const ec_lay_var_t *x = &L->var[pg->var0 + vv];
                for (k = 0; k < x->n; k++) {
                    const ec_item_t *it = &L->item[x->item0 + k];
                    if (it->kind != EC_IT_PARAM || !ec_param_mod_dest(it->id))
                        continue;
                    if (g_pos[it->id] < 0) {
                        g_pos[it->id] = g_n;
                        g_cat[g_n] = (uint8_t)c;
                        g_ord[g_n++] = it->id;
                    }
                    if (!g_lbl[it->id] && it->label)
                        g_lbl[it->id] = it->label;
                }
            }
        }
    for (id = 0; id < EC_NP; id++)
        if (ec_param_mod_dest(id) && g_pos[id] < 0) {
            g_pos[id] = g_n;
            g_cat[g_n] = EC_NCAT;
            g_ord[g_n++] = (uint8_t)id;
        }
}

float ec_mk_dst_step(float x, int det, int shift)
{
    const int cur = (int)floorf(x + 0.5f) - 1;
    int pos, n = det < 0 ? -det : det;
    order_build();
    pos = cur >= 0 && cur < EC_NP ? g_pos[cur] : -1;      /* -1: OFF (before the first) */
    for (; n > 0; n--) {
        if (!shift)
            pos = clampi(pos + (det > 0 ? 1 : -1), -1, g_n - 1);
        else if (det > 0) {                                /* the next category's first */
            const int c = pos < 0 ? -1 : g_cat[pos];
            int q = pos + 1;
            while (q < g_n && (int)g_cat[q] == c)
                q++;
            if (q < g_n)
                pos = q;
        } else {                                           /* this category's first, else the previous one's */
            int q;
            if (pos < 0)
                break;
            q = pos;
            while (q > 0 && g_cat[q - 1] == g_cat[pos])
                q--;
            if (q == pos) {
                if (q == 0) {
                    pos = -1;
                    continue;
                }
                q--;
                while (q > 0 && g_cat[q - 1] == g_cat[q])
                    q--;
            }
            pos = q;
        }
    }
    return pos < 0 ? 0.0f : (float)(g_ord[pos] + 1);
}

void ec_mk_dst_name(float x, char *out, int cap)
{
    const int id = (int)floorf(x + 0.5f) - 1;
    order_build();
    if (id < 0 || id >= EC_NP)
        snprintf(out, (size_t)cap, "OFF");
    else if (id == EC_P_INS2_MIX || (id >= EC_P_INS2_P1 && id < EC_P_INS2_P1 + EC_INS2_NP)) {
        const int d = ec_param_def_id(tv(), id);   /* 0.6.1: INS 2's: "2:" + the param it stands for now */
        snprintf(out, (size_t)cap, "2:%s", d >= 0 ? EC_PARAMS[d].label : EC_PARAMS[id].label);
    } else
        snprintf(out, (size_t)cap, "%s", id == EC_P_PITCH ? "PITCH" : g_lbl[id] ? g_lbl[id] : EC_PARAMS[id].label);
}

/* ------------------------------------------------------------------------------------------- slots --- */
void ec_mk_slot_edited(int k)
{
    float *v = tv();
    const int lv = ec_mtx_level_id(ec_mtx_src(v, k));
    if (lv >= 0 && ec_mtx_dst(v, k) >= 0 && fabsf(v[lv] - 64.0f) < 0.5f) {   /* a silent source: full level */
        v[lv] = 128.0f;
        ec_seq_touch(&A->seq, A->track);
    }
}

static void slot_clear(float *v, int k)
{
    v[ec_mtx_src_id(k)] = EC_PARAMS[ec_mtx_src_id(k)].def;
    v[ec_mtx_dst_id(k)] = EC_PARAMS[ec_mtx_dst_id(k)].def;
    v[ec_mtx_depth_id(k)] = EC_PARAMS[ec_mtx_depth_id(k)].def;
}

/* 0.6.1: a refusal in the header (no takeover): NOT MODDABLE, MATRIX FULL (an alert) */
static void bad(const char *why, int alert)
{
    if (alert)
        ec_app_alert(why);
    else
        ec_app_message(why);
    MK->bad_ms = A->ms ? A->ms : 1;
}

/* ------------------------------------------------------------------------------------------- the source keys --- */
/* a hold ends: free the slots it made and left at 0 (no locks on their depth). 0.6.55 (QA B3 / C9): the routes it
 * made and kept are one undo entry (with the source's level, when the hold raised it); a hold that kept none puts the
 * level back as it was */
static void release(void)
{
    const int t = MK->trk >= 0 && MK->trk < EC_NTRACK ? MK->trk : A->track;
    float *v = ec_project_track(&A->proj, t)->v;
    uint8_t ids[EC_U_NV];
    float after[EC_U_NV];
    int k, i, m = 0, kept = 0;
    for (k = 0; k < EC_MTX_SLOTS; k++)
        if ((MK->created >> k & 1u) && v[ec_mtx_depth_id(k)] == 0.0f && !ec_ux_locked_somewhere(ec_mtx_depth_id(k)))
            slot_clear(v, k);
    for (k = 0; k < EC_MTX_SLOTS && m + 3 <= EC_U_NV - 1; k++)
        if ((MK->created >> k & 1u) && ec_mtx_src(v, k) != EC_MS_OFF) {
            ids[m++] = (uint8_t)ec_mtx_src_id(k);
            ids[m++] = (uint8_t)ec_mtx_dst_id(k);
            ids[m++] = (uint8_t)ec_mtx_depth_id(k);
            kept++;
        }
    if (kept) {                                           /* the entry holds the values from before the hold */
        if (MK->lvl_id >= 0)
            ids[m++] = (uint8_t)MK->lvl_id;
        for (i = 0; i < m; i++) {
            after[i] = v[ids[i]];
            v[ids[i]] = ids[i] == MK->lvl_id ? MK->lvl_old : EC_PARAMS[ids[i]].def;
        }
        ec_e5_edit_params(t, ids, m, "MOD");
        for (i = 0; i < m; i++)
            v[ids[i]] = after[i];
    } else if (MK->lvl_id >= 0) {                         /* nothing kept: the source's level as it was */
        const int src = ec_mk_src_of_key(MK->held);
        int live = 0;
        for (k = 0; k < EC_MTX_SLOTS; k++)
            live |= ec_mtx_src(v, k) == src && ec_mtx_dst(v, k) >= 0;
        if (!live)
            v[MK->lvl_id] = MK->lvl_old;
    }
    if (MK->created) {
        ec_seq_touch(&A->seq, t);
        ec_app_mark_dirty();
    }
    MK->held = -1;
    MK->created = 0;
    MK->hot_id = -1;
    MK->lvl_id = -1;
    A->redraw = 1;
}

void ec_mk_hold(int i)
{
    if (MK->held >= 0)
        release();
    MK->held = i;
    MK->used = 0;
    MK->created = 0;
    MK->hot_id = -1;
    MK->trk = A->track;
    MK->lvl_id = -1;
    A->redraw = 1;
}

int ec_mk_release(void)
{
    const int tap = MK->held >= 0 && !MK->used;
    if (MK->held >= 0)
        release();
    return tap;
}

void ec_mk_off(void)
{
    if (MK->held >= 0)
        release();
}

int ec_mk_turn(const ec_item_t *it, int role, int det)
{
    const int i = MK->held;
    int src, id, k;
    float *v;
    char lb[24], val[16];
    if (i < 0 || !KEY_SRC[i])
        return 0;
    MK->used = 1;
    src = KEY_SRC[i];
    if (it->kind != EC_IT_PARAM || A->bus || A->settings || !ec_param_mod_dest(it->id)) {
        bad("NOT MODDABLE", 0);
        return 1;
    }
    id = it->id;
    v = tv();
    k = ec_mtx_find(v, src, id);
    MK->hot_id = id;
    if (ec_pn_f(2)) {                                     /* 0.6.0, F2 while turning: remove that route */
        A->pn.f_used = 3;
        if (k >= 0) {
            const uint8_t ids[3] = {(uint8_t)ec_mtx_src_id(k), (uint8_t)ec_mtx_dst_id(k), (uint8_t)ec_mtx_depth_id(k)};
            ec_e5_edit_params(A->track, ids, 3, "MOD CLEAR");   /* 0.6.16: undoable */
            slot_clear(v, k);
            ec_seq_touch(&A->seq, A->track);
            ec_app_mark_dirty();
        }
        ec_app_message(k >= 0 ? "MOD CLEARED" : "NO MOD");
        return 1;
    }
    if (k < 0) {                                          /* the first detent: a free slot takes the route */
        k = ec_mtx_free(v);
        if (k < 0) {
            bad("MATRIX FULL", 1);
            return 1;
        }
        slot_clear(v, k);
        v[ec_mtx_src_id(k)] = (float)(src + 1);
        v[ec_mtx_dst_id(k)] = (float)(id + 1);
        MK->created |= (uint16_t)(1u << k);
        if (MK->lvl_id < 0 && ec_mtx_level_id(src) >= 0) {   /* 0.6.55: the level before ec_mk_slot_edited raises it */
            MK->lvl_id = ec_mtx_level_id(src);
            MK->lvl_old = v[MK->lvl_id];
        }
        ec_mk_slot_edited(k);
        ec_seq_touch(&A->seq, A->track);
        ec_app_mark_dirty();
    } else if ((MK->created >> k & 1u) && ec_app_held_step(A) < 0) {   /* 0.6.55: a route this hold made: its depth
                                                    * moves without entries of its own (the release records the route) */
        const int did = ec_mtx_depth_id(k);
        v[did] = ec_app_move_param(did, v[did], det, ec_ux_accel(role, ec_app_param_steps(did), det));
        ec_seq_touch(&A->seq, A->track);
        ec_app_mark_dirty();
    } else                                                /* later detents (or a route there before): DEPTH */
        ec_app_edit_param(ec_mtx_depth_id(k), det, ec_ux_accel(role, ec_app_param_steps(ec_mtx_depth_id(k)), det));
    if (!ec_app_cells_visible(A)) {                       /* the param isn't on screen: the takeover shows it */
        const int hs = ec_app_held_step(A);
        float d = v[ec_mtx_depth_id(k)];
        if (hs >= 0)
            ec_step_lock_get(&ec_project_track(&A->proj, A->track)->steps[hs], ec_mtx_depth_id(k), &d);
        snprintf(lb, sizeof lb, "%s > %s", ec_mtx_src_name(src), EC_PARAMS[id].label);
        ec_param_format(ec_mtx_depth_id(k), d, 3, val, sizeof val);
        ec_ux_takeover(lb, val, (d + 100.0f) / 200.0f);
    }
    A->redraw = 1;
    return 1;
}

int ec_mk_hot_slot(int id)
{
    const int i = MK->held;
    if (i < 0 || !KEY_SRC[i] || A->bus)
        return -1;
    return ec_mtx_find(tv(), KEY_SRC[i], id);
}

uint32_t ec_mk_marks(int id)
{
    const float *v = tv();
    uint32_t m = 0;
    int k;
    if (A->bus)
        return 0;
    for (k = 0; k < EC_MTX_SLOTS; k++)
        if (ec_mtx_src(v, k) != EC_MS_OFF && ec_mtx_dst(v, k) == id)
            m |= 1u << ec_mtx_src(v, k);
    return m;
}

/* ------------------------------------------------------------------------------------------- a source's page --- */
/* 0.6.51 (owner): LFO 1 / LFO 2 / MOD ENV / VEL show their routes one at a time: SEL steps through them (past the
 * ends: the page, as MATRIX's rows), ALGO sets the shown route's depth */
static int g_rsel[EC_MS_N];

int ec_mk_route_src(void)
{
    ec_page_t pg;
    if (A->bus || A->settings || A->ux.f3)
        return 0;
    ec_app_page(A, &pg);
    switch (pg.pic) {
    case EC_PIC_LFO1: return EC_MS_LFO1;
    case EC_PIC_LFO2: return EC_MS_LFO2;
    case EC_PIC_MENV: return EC_MS_ENV;
    case EC_PIC_VEL: return EC_MS_VEL;
    default: return 0;
    }
}

int ec_mk_route(const float *v, int src, int *idx, int *n)
{
    int k, c = 0, sel, slot = -1;
    for (k = 0; k < EC_MTX_SLOTS; k++)
        if (ec_mtx_src(v, k) == src && ec_mtx_dst(v, k) >= 0)
            c++;
    sel = src > 0 && src < EC_MS_N ? clampi(g_rsel[src], 0, c > 0 ? c - 1 : 0) : 0;
    for (k = 0, c = 0; k < EC_MTX_SLOTS; k++)
        if (ec_mtx_src(v, k) == src && ec_mtx_dst(v, k) >= 0) {
            if (c == sel)
                slot = k;
            c++;
        }
    if (idx)
        *idx = sel;
    if (n)
        *n = c;
    return slot;
}

int ec_mk_route_select(int d)
{
    const int src = ec_mk_route_src();
    int idx, n;
    if (!src)
        return 0;
    ec_mk_route(tv(), src, &idx, &n);
    if (idx + d < 0 || idx + d >= n)
        return 0;                                          /* (past the ends: the page turns) */
    g_rsel[src] = idx + d;
    A->redraw = 1;
    return 1;
}

int ec_mk_route_depth(int d)
{
    const int src = ec_mk_route_src();
    int k;
    if (!src)
        return 0;
    k = ec_mk_route(tv(), src, 0, 0);
    if (k < 0)
        ec_app_message("NO ROUTE");
    else
        ec_app_edit_param(ec_mtx_depth_id(k), d, ec_ux_accel(EC_E_ALGO, ec_app_param_steps(ec_mtx_depth_id(k)), d));
    A->redraw = 1;
    return 1;
}

/* ------------------------------------------------------------------------------------------- the MATRIX page --- */
int ec_mk_select(int d)
{
    const int r = MK->row + d;
    if (r >= 0 && r < EC_MTX_SLOTS) {
        MK->row = r;
        A->redraw = 1;
        return 1;
    }
    MK->row = d > 0 ? 0 : EC_MTX_SLOTS - 1;               /* past the ends: the next / previous page */
    return 0;
}

void ec_mk_item(const ec_item_t *in, ec_item_t *out)
{
    static const char *const LB[EC_NMX] = {"SRC", "DST", "DEPTH"};
    *out = *in;
    if (in->kind != EC_IT_MTX)
        return;
    out->kind = EC_IT_PARAM;
    out->id = (uint8_t)(EC_P_MTX1_SRC + 3 * clampi(MK->row, 0, EC_MTX_SLOTS - 1) + clampi(in->id, 0, 2));
    out->label = in->label ? in->label : LB[clampi(in->id, 0, 2)];
}

/* ------------------------------------------------------------------------------------------- tick, LEDs, labels --- */
void ec_mk_tick(void)
{
    int t;
    for (t = 0; t < EC_NTRACK; t++)
        ec_dsp_mod_live(A->dsp, t, t == A->track ? MK->at : 0.0f, MK->mw, MK->expr);
}

void ec_mk_labels(void)
{
    int i;
    if (MK->held < 0 || !KEY_SRC[MK->held] || A->fn || A->e5.naming || A->ux.f3)
        return;
    {
        ec_page_t pg;
        int lane;
        ec_app_page(A, &pg);
        lane = ec_app_lane(A, &pg);
        for (i = 0; i < 4; i++) {
            const int c = lane * 4 + i;
            char b[24];
            ec_item_t it;
            if (c >= pg.n || pg.it[c].kind == EC_IT_BLANK)
                continue;
            ec_mk_item(&pg.it[c], &it);
            snprintf(b, sizeof b, "%s %s", ec_mtx_src_name(KEY_SRC[MK->held]), ec_app_item_label(&it));
            ec_plat_label(EC_LBL_KNOB0 + EC_E_K1 + i, b);
        }
    }
}
