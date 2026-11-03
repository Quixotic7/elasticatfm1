/* SPDX-License-Identifier: GPL-3.0-only */
/* test_layout: the SHIPPED layout (design/layout/elasticat.json, whatever its arrangement) in the app. First the
 * modes (0.6.0): every mode on a button, numbered in order, every page's black key opening that page. Then, for
 * contexts covering every variant (each machine, warp mode, filter machine, env mode, insert FX; each bus with each
 * FX machine), every lane of every page of every mode and of every setup list, opened directly (ec_pages' data and
 * ec_app_goto: the panel gestures that reach them are the modes', tested by test_ux), and every item the layout places
 * in that context found on its knob, by its id: the knob's live panel label is the item's, and a turn of that knob
 * changes that param (then it is set back); F3 too. Nothing here knows where a param sits: a rearranged design is
 * tested the same way. The other tests and the demo scripts run on the fixed 0.4.0 layout (host/ec_layout_test.h).
 *   ec_test_layout <folder> [--layout test]   (host/make_demo_folder.py) */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ec_app.h"
#include "ec_fs_native.h"
#include "ec_layout_test.h"
#include "ec_params.h"
#include "fm1core.h"

uint32_t fm1core_labels_epoch(void);
const char *fm1core_label(int32_t which);

static int fails, checks, found_total, turned_total;
#define CHECK(c, ...)                                                                                              \
    do {                                                                                                           \
        checks++;                                                                                                  \
        if (!(c)) {                                                                                                \
            if (fails++ < 40) {                                                                                    \
                printf("FAIL %s:%d: ", __FILE__, __LINE__);                                                        \
                printf(__VA_ARGS__);                                                                               \
                printf("\n");                                                                                      \
            }                                                                                                      \
        }                                                                                                          \
    } while (0)

#define A (&ec_app)
static const fm1core_t *C;
static fm1panel_t *P;
static uint32_t ms;
static char ctxname[96];

static void run(int n)
{
    int i;
    for (i = 0; i < n; i++) {
        ec_fs_native_pump();
        C->tick(ms++);
        if (ms % 15 == 0)
            C->frame();
    }
}
static void turn(int role, int n)
{
    P->enc[P->enc_id[role]] += n * P->enc_dir[role];
    run(120);                                        /* (slow turns: no knob acceleration) */
}

/* 0.6.0: the pages are opened directly, through ec_pages' data and ec_app_goto (the panel's gestures to reach them
 * are the modes' own, tested by test_ux): the page, its lane (or the setup list and its lane), the target */
static void reset_view(void)
{
    A->ux.f3 = 0;
    A->settings = 0;
    A->redraw = 1;
    run(20);
}
static void open_lane(int c, int page, int setup, int lane)
{
    reset_view();
    ec_app_goto(c, page);
    if (setup) {
        A->settings = 1;
        A->ux.set_row = lane;                        /* 0.6.1: a list menu: the selected row */
    } else
        A->ux.lane[c] = lane;
    A->redraw = 1;
    run(40);
}
static void target(int bus)                          /* 0 the track (T2), 1..3 S1 S2 MST */
{
    A->bus = bus;
    A->redraw = 1;
    run(20);
}

/* the value an item's knob moves: the param in the track or the bus record */
static float value_of(const ec_item_t *it)
{
    const float *bv = ec_app_bus_params(A, it->id);
    return bv ? bv[it->id] : ec_app_params(A)[it->id];
}

/* the active lane: every item checked on its knob; the found ones marked */
static void check_lane(uint8_t *found, int where_cat)
{
    ec_page_t pg;
    int k, lane;
    char name[64];
    if (A->ux.f3)
        ec_page_f3(&pg), lane = 0;
    else
        ec_app_page(A, &pg), lane = ec_app_lane(A, &pg);
    snprintf(name, sizeof name, "%s/%s lane %d", ec_cat_name(where_cat), A->settings ? "SETUP" : pg.title, lane + 1);
    run(20);
    for (k = 0; k < 4; k++) {
        const int c = A->settings && !A->ux.f3 ? (k ? -1 : ec_app_set_item(A, &pg)) : lane * 4 + k;
        const ec_item_t *it = c >= 0 && c < pg.n ? &pg.it[c] : 0;
        const char *lb;
        if (A->settings && k) {                      /* 0.6.1: a list: K2..K4 have no label */
            lb = fm1core_label(EC_LBL_KNOB0 + EC_E_K1 + k);
            CHECK(!lb || !lb[0], "%s [%s] KNOB %d: a list's K%d is labelled %s", name, ctxname, k + 1, k + 1, lb);
            continue;
        }
        if (!it || it->kind == EC_IT_BLANK)
            continue;
        lb = fm1core_label(EC_LBL_KNOB0 + EC_E_K1 + k);
        CHECK(lb && !strcmp(lb, ec_app_item_label(it)), "%s [%s] KNOB %d: the live label %s, the item's %s", name,
              ctxname, k + 1, lb ? lb : "(none)", ec_app_item_label(it));
        if (it->kind == EC_IT_PARAM) {
            const int id = it->id;
            const float x0 = value_of(it);
            float x1;
            int d = 1;
            if (found[id])
                continue;
            found[id] = 1;
            found_total++;
            turn(EC_E_K1 + k, d);
            x1 = value_of(it);
            if (x1 == x0) {                         /* at the top of its range: the other way */
                d = -1;
                turn(EC_E_K1 + k, d);
                x1 = value_of(it);
            }
            CHECK(x1 != x0, "%s [%s] KNOB %d (%s): a turn did not change it", name, ctxname, k + 1,
                  EC_PARAMS[id].name);
            if (x1 != x0)
                turned_total++;
            {                                        /* back: as it was (a turn back is not exact on curved ranges) */
                float *bv = ec_app_bus_params(A, id);
                (bv ? bv : ec_app_params(A))[id] = x0;
                ec_seq_touch(&A->seq, A->track);
            }
        }
    }
}

/* every lane of every page of category c (or of its setup list), each opened directly; the ids reached */
static void walk(int c, int setup, uint8_t *found)
{
    const int np = setup ? 1 : ec_cat_pages(c, ec_app_params(A));
    int p, lane;
    for (p = 0; p < np; p++) {
        ec_page_t pg;
        open_lane(c, p, setup, 0);
        ec_app_page(A, &pg);
        CHECK(A->cat == c && (setup ? A->settings : A->page[c] == p && !A->settings), "[%s] %s page %d does not open",
              ctxname, ec_cat_name(c), p + 1);
        for (lane = 0; lane < (setup ? ec_app_set_rows(&pg, 0) : ec_ux_lanes(pg.n)); lane++) {
            if (lane)
                open_lane(c, p, setup, lane);
            CHECK(setup || ec_app_lane(A, &pg) == lane, "[%s] %s page %d lane %d does not open", ctxname,
                  ec_cat_name(c), p + 1, lane + 1);
            check_lane(found, c);                    /* (a setup list: row `lane` on K1) */
        }
        if (c == EC_CAT_FX && !setup && ec_src_insert_page()) {   /* 0.6.1: INSERT's knob pages (the insert key's
                                                                    * taps), INS 1's, then INS 2's on the same machine */
            float *v = ec_app_params(A);
            const float m2 = v[EC_P_INS2_MACHINE];
            int slot, kp;
            v[EC_P_INS2_MACHINE] = v[EC_P_FX_MACHINE];
            ec_ins2_reset(v);
            for (slot = 0; slot < 2; slot++) {
                A->pn.ins_slot = slot;
                for (kp = slot ? 0 : 1; kp < ec_src_ins_pages(); kp++) {
                    A->pn.ins_page = kp;
                    A->redraw = 1;
                    run(20);
                    check_lane(found, c);
                }
            }
            A->pn.ins_slot = A->pn.ins_page = 0;
            v[EC_P_INS2_MACHINE] = m2;
            ec_ins2_reset(v);
        }
    }
}

/* the params the layout places in the selected context (pages of every category, setups; bus: its pages) */
static int expected(uint8_t *want)
{
    ec_page_t pg;
    int c, p, i, n = 0;
    memset(want, 0, EC_NP);
    for (c = 0; c < EC_NCAT; c++) {
        if (A->bus && c != EC_CAT_FX)
            continue;                                /* (a bus: its page, the same in every category) */
        for (p = 0; p < ec_cat_pages(c, ec_app_params(A)); p++) {
            ec_page_items_bus(c, p, ec_app_params(A), A->bus, A->bus ? A->proj.bus[A->bus - 1] : 0, &pg);
            for (i = 0; i < pg.n; i++)
                if (pg.it[i].kind == EC_IT_PARAM)
                    want[pg.it[i].id] = 1;
        }
        if (A->bus ? ec_settings_items_bus(c, A->bus, &pg) : ec_settings_items(c, ec_app_params(A), &pg))
            for (i = 0; i < pg.n; i++)
                if (pg.it[i].kind == EC_IT_PARAM)
                    want[pg.it[i].id] = 1;
    }
    for (i = 0; i < EC_NP; i++)
        n += want[i];
    return n;
}

static void one_context(void)
{
    uint8_t want[EC_NP], found[EC_NP];
    int c, i, n;
    float keep[EC_NP];
    memcpy(keep, ec_app_params(A), sizeof keep);
    n = expected(want);
    memset(found, 0, sizeof found);
    for (c = 0; c < EC_NCAT; c++) {
        if (A->bus && c != EC_CAT_FX)
            continue;
        walk(c, 0, found);
        if (A->bus ? (c == EC_CAT_FX) : 1) {
            ec_page_t pg;
            if (A->bus ? ec_settings_items_bus(c, A->bus, &pg) : ec_settings_items(c, ec_app_params(A), &pg))
                walk(c, 1, found);
        }
    }
    reset_view();
    for (i = 0; i < EC_NP; i++)
        CHECK(!want[i] || found[i], "[%s] %s is on a page but never on a knob there", ctxname, EC_PARAMS[i].name);
    (void)n;
    memcpy(ec_app_params(A), keep, sizeof keep);
}

static void set_ctx(int m, int w, int f, int fe, int e, int fx)
{
    float *v = ec_app_params(A);
    v[EC_P_MACHINE] = (float)m, v[EC_P_MODE] = (float)w, v[EC_P_FILTER_MACHINE] = (float)f;
    v[EC_P_FENV_MODE] = (float)fe, v[EC_P_ENV_MODE] = (float)e, v[EC_P_FX_MACHINE] = (float)fx;
    ec_seq_touch(&A->seq, A->track);
    snprintf(ctxname, sizeof ctxname, "T%d mach %d warp %d filt %d fenv %d env %d fx %d", A->track + 1, m, w, f, fe, e,
             fx);
}

/* 0.6.0: the modes as the layout gives them: every mode on a button, a button's modes numbered 1..n; every page's
 * black key opens that page (and no other); FX's bus keys; the modes listed */
static void modes(void)
{
    static const char *const BN[EC_NMB] = {"-", "FX", "SEL", "ENV", "LFO", "EDIT", "HOME", "FILE", "ARP", "SEQ"};
    int bt, m, c, p, k;
    for (bt = EC_MB_FX; bt < EC_NMB; bt++) {
        int ms[EC_NMODE], n = ec_button_modes(bt, ms, EC_NMODE);
        printf("test_layout: %-4s", BN[bt]);
        for (m = 0; m < n; m++) {
            CHECK(ec_mode_button(ms[m]) == bt && ec_mode_order(ms[m]) == m + 1, "%s: mode %d is %s (order %d)", BN[bt],
                  m + 1, ec_mode_name(ms[m]), ec_mode_order(ms[m]));
            printf("%s %s", m ? " ->" : "", ec_mode_name(ms[m]));
            if (ms[m] < EC_NCAT) {
                printf(" [");
                for (p = 0; p < ec_cat_pages(ms[m], ec_app_params(A)); p++) {
                    ec_page_t pg;
                    ec_page_items(ms[m], p, ec_app_params(A), 0, &pg);
                    k = ec_cat_page_key(ms[m], p);
                    printf("%s%s", p ? ", " : "", pg.title);
                    if (k)
                        printf(" B%d", k);
                }
                printf("]");
            }
        }
        printf("\n");
    }
    for (m = 0; m < EC_NMODE; m++)
        CHECK(ec_mode_button(m) > EC_MB_NONE && ec_mode_button(m) < EC_NMB && ec_mode_order(m) >= 1,
              "mode %s: no button", ec_mode_name(m));
    for (c = 0; c < EC_NCAT; c++) {
        for (p = 0; p < ec_cat_pages(c, ec_app_params(A)); p++) {
            k = ec_cat_page_key(c, p);
            CHECK(!k || ec_cat_key_page(c, k) == p, "%s page %d: B%d opens page %d", ec_cat_name(c), p + 1, k,
                  ec_cat_key_page(c, k) + 1);
        }
        for (k = 1; k <= EC_NBKEY; k++) {
            p = ec_cat_key_page(c, k);
            CHECK(p < 0 || ec_cat_page_key(c, p) == k, "%s: B%d", ec_cat_name(c), k);
            CHECK(!(p >= 0 && ec_cat_key_bus(c, k)), "%s: B%d opens a page and a bus", ec_cat_name(c), k);
        }
    }
    for (k = 1; k <= 3; k++)
        CHECK(!ec_bus_key(k) || ec_cat_key_bus(EC_CAT_FX, ec_bus_key(k)) == k, "bus %d's key", k);
}

int main(int argc, char **argv)
{
    int i, b;
    if (argc < 2) {
        printf("usage: ec_test_layout <folder> [--layout test]\n");
        return 2;
    }
    if (argc > 3 && !strcmp(argv[2], "--layout") && !strcmp(argv[3], "test"))
        ec_layout_use(&EC_LAYOUT_TEST);
    printf("test_layout: the layout \"%s\"\n", ec_layout()->name);
    ec_fs_native_root(argv[1], "elasticat");
    C = fm1core_get(FM1CORE_ABI);
    if (!C)
        return 1;
    P = C->panel;
    C->flash_stage(NULL, 0);
    C->options(FM1CORE_FLASH_STAGED, 0, 0, 1);
    P->ready = 2;
    C->init(0);
    run(1500);
    modes();
    /* T2 (T1 would be the NEIGH machine's source: any track works) */
    A->track = 1;
    target(0);
    for (i = 1; i <= 8; i++)                     /* every machine */
        set_ctx(i, 1, 1, 2, 2, 1), one_context();
    for (i = 1; i <= 13; i++)                    /* every warp mode (a loop machine) */
        set_ctx(1, i, 1, 2, 2, 1), one_context();
    for (i = 1; i <= 6; i++)                     /* every filter machine, the env modes both ways */
        set_ctx(1, 1, i, 1 + (i & 1), 1 + (i >> 1 & 1), 1), one_context();
    for (i = 1; i <= 18; i++)                    /* every insert FX */
        set_ctx(3, 4, 1, 1, 1, i), one_context();
    set_ctx(1, 1, 1, 2, 2, 1);
    for (b = 1; b <= 3; b++)                     /* each bus with each FX machine */
        for (i = 1; i <= 18; i++) {
            A->proj.bus[b - 1][EC_P_FX_MACHINE] = (float)i;
            target(b);
            snprintf(ctxname, sizeof ctxname, "bus %d fx %d", b, i);
            one_context();
            target(0);
        }
    {                                            /* F3 */
        uint8_t f3[EC_NP];
        memset(f3, 0, sizeof f3);
        reset_view();
        A->ux.f3 = 1;
        A->redraw = 1;
        run(40);
        snprintf(ctxname, sizeof ctxname, "F3");
        check_lane(f3, EC_CAT_PAT);
        reset_view();
    }
    C->shutdown();
    printf("test_layout: %d params found on their knobs and turned (%d turns), %d checks, %d failed\n", found_total,
           turned_total, checks, fails);
    return fails ? 1 : 0;
}
