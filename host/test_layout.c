/* SPDX-License-Identifier: GPL-3.0-only */
/* test_layout: the SHIPPED layout (design/layout/elasticat.json, whatever its arrangement) through panel input. For
 * contexts covering every variant (each machine, warp mode, filter machine, env mode, insert FX; each bus with each
 * FX machine), it walks every category as a player would (its button, then SELECT: lane by lane, on into the next
 * page, round; FILE: SAVE; MST: the MST key; setups: the button held 0.4 s; F3: OCT- + OCT+) and finds every item the
 * layout places in that context on a knob, by its id: the knob's live panel label is the item's, and a turn of
 * that knob changes that param (then it is set back). Nothing here knows where a param sits: a rearranged design is
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
static void press(int b, int dur)
{
    const uint32_t m = 1u << P->btn_id[b];
    P->buttons |= m;
    P->buttons_tap |= m;
    run(dur);
    P->buttons &= ~m;
    run(20);
}
static int key_of(int upper, int i)
{
    int k, nl = 0, nu = 0;
    for (k = 0; k < FM1P_NKEY; k++) {
        const int s = k % 12;
        const int natural = s == 0 || s == 2 || s == 4 || s == 6 || s == 7 || s == 9 || s == 11;
        if (natural && !upper && nl++ == i)
            return k;
        if (!natural && upper && nu++ == i)
            return k;
    }
    return 0;
}
static void tapkey(int upper, int i, int dur)
{
    const int k = key_of(upper, i);
    P->key_vel[k] = 100;
    P->keys |= 1u << k;
    P->keys_tap |= 1u << k;
    run(dur);
    P->keys &= ~(1u << k);
    run(20);
}
static void turn(int role, int n)
{
    P->enc[P->enc_id[role]] += n * P->enc_dir[role];
    run(120);                                        /* (slow turns: no knob acceleration) */
}
static void home(void)
{
    press(FM1P_B_HOME, 60);
    run(30);
}

static const int CAT_BTN[EC_NCAT] = {FM1P_B_SEQ, FM1P_B_SEL, FM1P_B_EDIT, FM1P_B_ENV, FM1P_B_FX, FM1P_B_LFO, -1,
                                     FM1P_B_SAVE, FM1P_B_ARP};
static void open_cat(int c, int setup)
{
    home();
    if (CAT_BTN[c] < 0)                              /* MST: the MST upper key (held 0.4 s: its setup) */
        tapkey(1, 8, setup ? 500 : 60);
    else
        press(CAT_BTN[c], setup ? 500 : 60);
    run(40);
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
        const int c = lane * 4 + k;
        const ec_item_t *it = c < pg.n ? &pg.it[c] : 0;
        const char *lb;
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

/* every lane of category c (its pages or its setup), stepped as a player does; the ids reached */
static void walk(int c, int setup, uint8_t *found)
{
    int steps;
    open_cat(c, setup);
    for (steps = 0; !setup && steps < 4 * 16 + 4; steps++) {   /* to the first page's first lane (SELECT goes round) */
        ec_page_t pg;
        ec_app_page(A, &pg);
        if (A->page[c] == 0 && ec_app_lane(A, &pg) == 0)
            break;
        if (c == EC_CAT_FILE)
            press(FM1P_B_SAVE, 60);
        else
            turn(EC_E_SELECT, 1);
    }
    for (steps = 0; steps < 4 * 16 + 4; steps++) {
        ec_page_t pg;
        check_lane(found, c);
        ec_app_page(A, &pg);
        if (setup) {
            if (ec_app_lane(A, &pg) + 1 >= ec_ux_lanes(pg.n))
                break;
            turn(EC_E_SELECT, 1);
            continue;
        }
        /* the last lane of the last page: done (the next step wraps to the first page) */
        if (A->page[c] == ec_cat_pages(c, ec_app_params(A)) - 1 && ec_app_lane(A, &pg) + 1 >= ec_ux_lanes(pg.n))
            break;
        if (c == EC_CAT_FILE)
            press(FM1P_B_SAVE, 60);                  /* (SELECT is the file cursor / the slice there) */
        else
            turn(EC_E_SELECT, 1);
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
    home();
    for (i = 0; i < EC_NP; i++)
        CHECK(!want[i] || found[i], "[%s] %s is on a page but SELECT never reached it", ctxname, EC_PARAMS[i].name);
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
    /* T2 (T1 would be the NEIGH machine's source: any track works) */
    tapkey(1, 1, 60);
    for (i = 1; i <= 8; i++)                     /* every machine */
        set_ctx(i, 1, 1, 2, 2, 1), one_context();
    for (i = 1; i <= 13; i++)                    /* every warp mode (a loop machine) */
        set_ctx(1, i, 1, 2, 2, 1), one_context();
    for (i = 1; i <= 6; i++)                     /* every filter machine, the env modes both ways */
        set_ctx(1, 1, i, 1 + (i & 1), 1 + (i >> 1 & 1), 1), one_context();
    for (i = 1; i <= 18; i++)                    /* every insert FX */
        set_ctx(3, 4, 1, 1, 1, i), one_context();
    set_ctx(1, 1, 1, 2, 2, 1);
    for (b = 1; b <= 3; b++)                     /* each bus with each FX machine (ALGORITHM past T6) */
        for (i = 1; i <= 18; i++) {
            A->proj.bus[b - 1][EC_P_FX_MACHINE] = (float)i;
            turn(EC_E_ALGO, 9);
            turn(EC_E_ALGO, -(3 - b));
            snprintf(ctxname, sizeof ctxname, "bus %d fx %d", b, i);
            CHECK(A->bus == b, "ALGORITHM: bus %d (%d)", b, A->bus);
            one_context();
            turn(EC_E_ALGO, -9);
            tapkey(1, 1, 60);
        }
    {                                            /* F3: OCT- + OCT+ together */
        uint8_t f3[EC_NP];
        const uint32_t m = 1u << P->btn_id[FM1P_B_OCTDN] | 1u << P->btn_id[FM1P_B_OCTUP];
        memset(f3, 0, sizeof f3);
        P->buttons |= m;
        P->buttons_tap |= m;
        run(80);
        P->buttons &= ~m;
        run(40);
        CHECK(A->ux.f3, "F3 opens");
        snprintf(ctxname, sizeof ctxname, "F3");
        check_lane(f3, EC_CAT_PAT);
        home();
    }
    C->shutdown();
    printf("test_layout: %d params found on their knobs and turned (%d turns), %d checks, %d failed\n", found_total,
           turned_total, checks, fails);
    return fails ? 1 : 0;
}
