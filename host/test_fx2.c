/* SPDX-License-Identifier: GPL-3.0-only */
/* test_fx2: 0.6.1's two inserts, the FX INSERT page, the hold menus and the load meter (docs/UX-V2.md §9, §10)
 * through the panel, with the demo folder (host/make_demo_folder.py) and the shipped layout, one gesture each:
 *   FX: the insert key opens INSERT (both slots); SEL picks INS 2; the insert key held = the selected slot's type menu
 *   (it shows at 400 ms, or at once on K1 / SEL; K1 moves it; letting go picks; the header says what the key does);
 *   picking INS 2's type resets its MIX and P1..P8 to the machine's defaults (one UNDO puts them back); the insert key
 *   tapped on INSERT turns the knob page P1 -> P2 -> P1; K4 is the MIX on several types (fx_mix, fx_wet, INS 2's).
 *   LOOP ENGINE: B1..B5 held = the loop mode / machine / warp / voice mode / ADVANCE menus; a tap still turns the page
 *   (B5: hold only); the warp menu refuses while playing (STOP FIRST).
 *   INS 2: a step lock on its param, the project saved and loaded back (record round trip); an old project (0.5's
 *   record) loads with INS 2 empty and the wet levels full.
 *   The meter: it rises with the load (inserts, playing) and two runs of the same input read the same.
 *   ec_test_fx2 <folder> */
#include "ec_harness.h"
#include "ec_holdmenu.h"
#include "ec_load.h"
#include "ec_mtx.h"

static float *tv(void) { return ec_app_params(A); }
static int insert_page(void)
{
    int p;
    for (p = 0; p < ec_cat_pages(EC_CAT_FX, tv()); p++)
        if (ec_page_pic(EC_CAT_FX, p, 0) == EC_PIC_FX)
            return p;
    return -1;
}
static void hold_b(int b) { key(1, b - 1, 1); }
static void let_b(int b)
{
    key(1, b - 1, 0);
    run(20);
}
static int cell_id(int k)
{
    ec_page_t pg;
    ec_app_page(A, &pg);
    return k < pg.n && pg.it[k].kind == EC_IT_PARAM ? pg.it[k].id : -1;
}

static void test_insert(void)
{
    float *v;
    to(EC_CAT_FX);
    v = tv();
    v[EC_P_FX_MACHINE] = EC_FX_DELAY;
    B(5, 40);                                        /* (from SENDS: B1 jumps to its row's first page) */
    B(1, 40);
    CHECK(A->page[EC_CAT_FX] == insert_page() && !A->bus && ec_src_insert_page(), "B1: the EFFECTS page");
    CHECK(ec_fx_slot() == 0 && A->pn.ins_page == 0 && cell_id(3) == EC_P_FX_MIX, "INSERT 1 P1, K4 = its MIX (%d)",
          cell_id(3));
    turn(FM1P_E_SELECT, 1);                          /* 0.6.2: INSERT 2 is the next page of the list (DELAY: one P) */
    CHECK(ec_fx_slot() == 1 && A->page[EC_CAT_FX] == insert_page(), "SEL: INSERT 2 P1 (the page stays)");
    turn(FM1P_E_SELECT, -1);
    CHECK(ec_fx_slot() == 0, "SEL back: INSERT 1");
    B(2, 40);
    CHECK(ec_fx_slot() == 1, "0.6.3: B2: INSERT 2");

    /* the type menu: held 400 ms it shows, K1 moves, letting go picks */
    hold_b(2);
    run(300);
    CHECK(!ec_hm_shown(), "B2 held 300 ms: no menu yet");
    run(150);
    CHECK(ec_hm_shown() && ec_hm_head() && !strcmp(ec_hm_head(), "INSERT 2 TYPE"),
          "B2 held 450 ms: INSERT 2's type menu (%s)", ec_hm_head() ? ec_hm_head() : "-");
    CHECK(ec_hm_count() == EC_FX_COUNT && ec_hm_cursor() == 0, "18 types, the cursor on NONE");
    turn(FM1P_E_K1, 7);
    CHECK(ec_hm_cursor() == 7 && !strcmp(ec_hm_label(7), "BLACKHOLE"), "K1 7: BLACKHOLE (%s)", ec_hm_label(ec_hm_cursor()));
    CHECK(tv()[EC_P_INS2_MACHINE] == EC_FX_NONE, "nothing changes before the release");
    v[EC_P_INS2_P1] = 99;
    let_b(2);
    CHECK(!ec_hm_shown() && v[EC_P_INS2_MACHINE] == EC_FX_BLACKHOLE, "released: INS 2 = BLACKHOLE");
    CHECK(v[EC_P_INS2_P1] == EC_PARAMS[EC_P_REVERB_SIZE].def && v[EC_P_INS2_P2] == EC_PARAMS[EC_P_BH_GRAVITY].def &&
              v[EC_P_INS2_MIX] == EC_PARAMS[EC_P_FX_MIX].def,
          "its MIX and P1..P8 at BLACKHOLE's defaults (P1 %.0f)", v[EC_P_INS2_P1]);
    CHECK(A->pn.ins_page == 0 && cell_id(0) == EC_P_INS2_P1 && cell_id(3) == EC_P_INS2_MIX,
          "the cells: INS 2's P1 (SIZE) .. K4 its MIX");
    {
        ec_page_t pg;
        char b[24];
        ec_app_page(A, &pg);
        ec_app_item_format(A, &pg.it[1], ec_app_item_value(A, &pg.it[1], 0), b, sizeof b);
        CHECK(!strcmp(ec_app_item_label(&pg.it[1]), "GRAV") && !strcmp(b, "64"), "K2 is GRAV, shown as itself (%s %s)",
              ec_app_item_label(&pg.it[1]), b);
    }
    /* K1 on menu: shows at once */
    hold_b(2);
    run(40);
    turn(FM1P_E_K1, -1);
    CHECK(ec_hm_shown(), "B2 held + K1 at 40 ms: the menu at once");
    let_b(2);
    CHECK(v[EC_P_INS2_MACHINE] == EC_FX_ECHO, "K1 -1: ECHO (%g)", v[EC_P_INS2_MACHINE]);
    /* one UNDO takes the pick back: the machine and its params */
    btn(FM1P_B_GLO, 1);
    run(20);
    press(FM1P_B_OCTDN, 40);
    btn(FM1P_B_GLO, 0);
    run(20);
    CHECK(v[EC_P_INS2_MACHINE] == EC_FX_BLACKHOLE && v[EC_P_INS2_P1] == EC_PARAMS[EC_P_REVERB_SIZE].def,
          "SHIFT + F1: INS 2 back to BLACKHOLE with its params (%g %g)", v[EC_P_INS2_MACHINE], v[EC_P_INS2_P1]);

    /* the row's key tapped again: P1 -> P2 -> P1 (BLACKHOLE: six params) */
    B(2, 40);
    CHECK(A->pn.ins_page == 1 && cell_id(0) == EC_P_INS2_P4 && cell_id(3) == EC_P_INS2_MIX,
          "tap: P2 (K1 = P4 %d, K4 still MIX)", cell_id(0));
    B(2, 40);                                        /* 0.6.2: past the group's last page: its first */
    CHECK(ec_fx_slot() == 1 && A->pn.ins_page == 0, "tap again: INSERT 2 P1 (%d %d)", ec_fx_slot(), A->pn.ins_page);
    B(1, 40);

    /* K4 = MIX on several types */
    {
        static const int M[] = {EC_FX_DELAY, EC_FX_LIMIT, EC_FX_DJ_EQ, EC_FX_DESTROY, EC_FX_REVERB};
        unsigned i;
        for (i = 0; i < sizeof M / sizeof M[0]; i++) {
            const int want = ec_fx_has_mix(M[i]) ? EC_P_FX_MIX : EC_P_FX_WET;
            float x0;
            v[EC_P_FX_MACHINE] = (float)M[i];
            x0 = v[want];
            run(300);
            turn(FM1P_E_K4, -1);
            CHECK(cell_id(3) == want && v[want] == x0 - 1, "INS 1 %s: K4 = %s (%g -> %g)", ec_fx_name(M[i]),
                  EC_PARAMS[want].name, x0, v[want]);
        }
        B(2, 40);                                    /* INSERT 2 */
        {
            const float x0 = v[EC_P_INS2_MIX];
            run(300);
            turn(FM1P_E_K4, 1);
            CHECK(cell_id(3) == EC_P_INS2_MIX && v[EC_P_INS2_MIX] == x0 + 1, "INS 2: K4 = its MIX (%g -> %g)", x0,
                  v[EC_P_INS2_MIX]);
        }
        v[EC_P_FX_MACHINE] = EC_FX_DELAY;
    }
}

/* 0.6.3 (docs/UX-V2.md §10 "FX layout"): the bus effects' knobs follow the inserts': K1..K3 the other params in
 * threes, K4 the MIX (fx_wet where the machine has no blend) on every knob page; SENDS: K1 / K2 the inserts' MIX, K3 /
 * K4 the sends; FX MIX the layout's cells */
static const char *hdr_text(void)                  /* the header's held text ("" none) */
{
    static char b[40];
    b[0] = 0;
    ec_hdr_title("", b, sizeof b);
    return ec_hdr_custom() ? b : "";
}
static int cell_lbl(int k, const char *want)
{
    ec_page_t pg;
    ec_app_page(A, &pg);
    if (k >= pg.n)
        return 0;
    if (!want)
        return pg.it[k].kind == 0;
    return pg.it[k].kind == EC_IT_PARAM && !strcmp(ec_app_item_label(&pg.it[k]), want);
}
static int fx_page_of(int pic)
{
    int p;
    for (p = 0; p < ec_cat_pages(EC_CAT_FX, tv()); p++)
        if (ec_page_pic(EC_CAT_FX, p, 0) == pic)
            return p;
    return -1;
}

static void test_bus_k4(void)
{
    float *s1 = A->proj.bus[0], *s2 = A->proj.bus[1], *mst = A->proj.bus[2];
    float x0;
    int i;
    to(EC_CAT_FX);
    s1[EC_P_FX_MACHINE] = EC_FX_BLACKHOLE;
    s2[EC_P_FX_MACHINE] = EC_FX_DESTROY;
    mst[EC_P_FX_MACHINE] = EC_FX_REVERB;
    B(3, 40);
    CHECK(A->bus == 1 && ec_fx_slot() == 2 && A->pn.ins_page == 0, "B3: SEND 1");
    CHECK(cell_lbl(0, "SIZE") && cell_lbl(1, "GRAV") && cell_lbl(2, "MOD") && cell_id(3) == EC_P_FX_MIX &&
              cell_lbl(3, "MIX"),
          "SEND 1 BLACKHOLE P1: SIZE GRAV MOD, K4 MIX (%d)", cell_id(3));
    x0 = s1[EC_P_FX_MIX];
    run(300);
    turn(FM1P_E_K4, -1);
    CHECK(s1[EC_P_FX_MIX] == x0 - 1, "K4 turns S1's MIX (%g -> %g)", x0, s1[EC_P_FX_MIX]);
    B(3, 40);
    CHECK(A->bus == 1 && A->pn.ins_page == 1 && cell_lbl(0, "LOW") && cell_lbl(1, "HI") && cell_lbl(2, "PRE") &&
              cell_id(3) == EC_P_FX_MIX,
          "B3 again: SEND 1 P2: LOW HI PRE, K4 MIX again (K1 %d)", cell_id(0));
    x0 = s1[EC_P_FX_MIX];
    run(300);
    turn(FM1P_E_K4, 1);
    CHECK(s1[EC_P_FX_MIX] == x0 + 1, "K4 on P2: S1's MIX (%g -> %g)", x0, s1[EC_P_FX_MIX]);
    turn(FM1P_E_SELECT, 1);
    CHECK(A->bus == 2 && A->pn.ins_page == 0, "SEL: SEND 2 P1 (bus %d page %d)", A->bus, A->pn.ins_page);
    for (i = 0; i < 3; i++) {                        /* DESTROY: eight params (LEVL too), no blend: K4 = fx_wet */
        x0 = s2[EC_P_FX_WET];
        CHECK(A->bus == 2 && A->pn.ins_page == i && cell_id(3) == EC_P_FX_WET && cell_lbl(3, "MIX"),
              "SEND 2 DESTROY P%d: K4 = its wet MIX (%d)", i + 1, cell_id(3));
        run(300);
        turn(FM1P_E_K4, -1);
        CHECK(s2[EC_P_FX_WET] == x0 - 1, "K4: S2's fx_wet (%g -> %g)", x0, s2[EC_P_FX_WET]);
        if (i < 2)
            B(4, 40);
    }
    CHECK(cell_lbl(0, "TONE") && cell_lbl(1, "LEVL") && cell_lbl(2, 0), "SEND 2 P3: TONE LEVL, K3 empty");
    B(5, 40);
    B(5, 40);
    CHECK(A->bus == 3 && cell_lbl(0, "SIZE") && cell_lbl(1, "DAMP") && cell_lbl(2, 0) && cell_id(3) == EC_P_FX_MIX,
          "B5 twice: MASTER REVERB: SIZE DAMP, K3 empty, K4 MIX");
    /* SENDS: K1 INS 1's MIX (fx_wet for DESTROY), K2 INS 2's, K3 / K4 the sends */
    tv()[EC_P_FX_MACHINE] = EC_FX_DESTROY;
    B(5, 40);
    CHECK(!A->bus && ec_fx_on_sends() && cell_id(0) == EC_P_FX_WET && cell_id(1) == EC_P_INS2_MIX &&
              cell_id(2) == EC_P_SEND1 && cell_id(3) == EC_P_SEND2 && cell_lbl(0, "INS 1") && cell_lbl(1, "INS 2"),
          "SENDS: K1 INS 1's MIX (DESTROY: fx_wet %d), K2 INS 2's, K3 SEND 1, K4 SEND 2", cell_id(0));
    tv()[EC_P_FX_MACHINE] = EC_FX_DELAY;
    CHECK(cell_id(0) == EC_P_FX_MIX, "SENDS: INS 1 DELAY: K1 fx_mix");
    {   /* FX MIX: the layout's cells, unchanged */
        const int p = fx_page_of(EC_PIC_STEPS);
        ec_page_t a, b;
        A->bus = 0;
        ec_app_set_page(EC_CAT_FX, p);
        ec_app_page(A, &a);
        ec_page_items(EC_CAT_FX, p, tv(), 0, &b);
        CHECK(p >= 0 && a.n == b.n && !memcmp(a.it, b.it, sizeof(ec_item_t) * (size_t)a.n) && cell_lbl(0, "FX MIX") &&
                  cell_lbl(1, "DRIVE") && cell_lbl(2, "SEND 1") && cell_lbl(3, "SEND 2"),
              "FX MIX: the layout's cells (FX MIX DRIVE SEND 1 SEND 2)");
    }
    s1[EC_P_FX_MACHINE] = s2[EC_P_FX_MACHINE] = mst[EC_P_FX_MACHINE] = EC_FX_NONE;
}

/* SENDS: ALGO = the send mode PRE / POST (an edit like a knob's: undo; a held step: a lock) */
static void test_send_mode(void)
{
    float *v;
    to(EC_CAT_FX);
    B(5, 40);
    if (A->bus == 3)
        B(5, 40);
    v = tv();
    v[EC_P_SEND_TAP] = 1;
    CHECK(ec_fx_on_sends(), "B5: SENDS");
    turn(FM1P_E_ALGO, 1);
    CHECK(v[EC_P_SEND_TAP] == 2, "ALGO +1: POST (%g)", v[EC_P_SEND_TAP]);
    turn(FM1P_E_ALGO, -1);
    CHECK(v[EC_P_SEND_TAP] == 1, "ALGO -1: PRE (%g)", v[EC_P_SEND_TAP]);
    run(1200);
    turn(FM1P_E_ALGO, 1);
    btn(FM1P_B_GLO, 1);
    run(20);
    press(FM1P_B_OCTDN, 40);
    btn(FM1P_B_GLO, 0);
    run(20);
    CHECK(v[EC_P_SEND_TAP] == 1, "SHIFT + F1: undone (PRE)");
    CHECK(EC_PARAMS[EC_P_SEND_TAP].flags & EC_PF_LOCKABLE, "send_tap is p-lockable");
}

/* SENDS (0.6.4): a held B1-B4 raises its box; K1 moves it left / right, each step swapping the inserts (machines,
 * params, locks, routes; every pattern) or the sends (the buses and every track's send levels and locks: the sound
 * unchanged); letting go lowers it, one undo for the move; a tap still goes to the row */
static void test_swaps(void)
{
    float *v, keep_ins[EC_NP];
    ec_project_t *p = &A->proj;
    int k, mt;
    to(EC_CAT_FX);
    v = tv();
    v[EC_P_FX_MACHINE] = EC_FX_REVERB;              /* INS 1 REVERB (SIZE 90, MIX 40), INS 2 DELAY */
    v[EC_P_REVERB_SIZE] = 90;
    v[EC_P_FX_MIX] = 40;
    v[EC_P_INS2_MACHINE] = EC_FX_DELAY;
    ec_ins2_reset(v);
    v[EC_P_INS2_MIX] = 100;
    clear_steps(A->track);
    step(3)->trig = 1;
    ec_step_lock_set(step(3), EC_P_REVERB_SIZE, 11);   /* a lock on INS 1's SIZE */
    mt = ec_mtx_free(v);
    v[ec_mtx_src_id(mt)] = EC_MS_LFO1 + 1;
    v[ec_mtx_dst_id(mt)] = EC_P_REVERB_SIZE + 1;       /* a route onto INS 1's SIZE */
    v[ec_mtx_depth_id(mt)] = 50;
    memcpy(keep_ins, v, sizeof keep_ins);
    B(5, 40);
    if (A->bus == 3)
        B(5, 40);
    CHECK(ec_fx_on_sends(), "SENDS");
    hold_b(1);
    run(250);
    CHECK(!strcmp(hdr_text(), "MOVE INSERT") && ec_hm_raised() < 0 && v[EC_P_FX_MACHINE] == EC_FX_REVERB,
          "B1 held 250 ms: the header (%s), not raised yet", hdr_text());
    run(250);
    CHECK(ec_hm_raised() == 0 && v[EC_P_FX_MACHINE] == EC_FX_REVERB && !strcmp(hdr_text(), "MOVE REVERB"),
          "B1 held 500 ms: I 1 raised (%s), nothing moved", hdr_text());
    turn(FM1P_E_K1, 1);
    CHECK(ec_hm_raised() == 1 && v[EC_P_FX_MACHINE] == EC_FX_DELAY && v[EC_P_INS2_MACHINE] == EC_FX_REVERB,
          "K1 +1: the raised box moves right: REVERB is INSERT 2");
    CHECK(ec_hm_raised_from() == 0 && ec_hm_slide() >= 0 && ec_hm_slide() < 0.5f,
          "it keeps its name (FX 1) and slides: just begun (%g)", ec_hm_slide());
    run(80);
    CHECK(ec_hm_slide() > 0.2f && ec_hm_slide() < 1.0f, "80 ms on: on its way (%g)", ec_hm_slide());
    run(200);
    CHECK(ec_hm_slide() == 1.0f && ec_hm_raised_from() == 0, "the slide done: on the right, still FX 1");
    turn(FM1P_E_K1, 3);
    CHECK(ec_hm_raised() == 1 && v[EC_P_INS2_MACHINE] == EC_FX_REVERB, "K1 +3: already on the right, stays");
    turn(FM1P_E_K1, -1);
    CHECK(ec_hm_raised() == 0 && v[EC_P_FX_MACHINE] == EC_FX_REVERB, "K1 -1: back left");
    turn(FM1P_E_K1, 1);
    let_b(1);
    CHECK(ec_hm_raised() < 0, "let go: lowered");
    k = ec_ins2_index(EC_FX_REVERB, EC_P_REVERB_SIZE);
    CHECK(ec_fx_on_sends() && v[EC_P_FX_MACHINE] == EC_FX_DELAY && v[EC_P_INS2_MACHINE] == EC_FX_REVERB,
          "B1 held 500 ms: INSERT 1 DELAY, INSERT 2 REVERB, still SENDS");
    CHECK(k >= 0 && v[EC_P_INS2_P1 + k] == 90 && v[EC_P_INS2_MIX] == 40 && v[ec_fx_mix_id(EC_FX_DELAY)] == 100,
          "the params moved: INS 2 SIZE 90 MIX 40, INS 1 MIX 100");
    CHECK(step(3)->nlock == 1 && step(3)->lock_id[0] == EC_P_INS2_P1 + k && step(3)->lock_val[0] == 11,
          "the lock on SIZE follows to INS 2 (%d)", step(3)->lock_id[0]);
    CHECK(v[ec_mtx_dst_id(mt)] == EC_P_INS2_P1 + k + 1, "the route onto SIZE follows to INS 2");
    btn(FM1P_B_GLO, 1);
    run(20);
    press(FM1P_B_OCTDN, 40);
    btn(FM1P_B_GLO, 0);
    run(20);
    CHECK(v[EC_P_FX_MACHINE] == EC_FX_REVERB && v[EC_P_REVERB_SIZE] == 90 && v[EC_P_FX_MIX] == 40 &&
              v[EC_P_INS2_MACHINE] == EC_FX_DELAY && v[EC_P_INS2_MIX] == 100 && step(3)->lock_id[0] == EC_P_REVERB_SIZE &&
              v[ec_mtx_dst_id(mt)] == EC_P_REVERB_SIZE + 1,
          "SHIFT + F1: one undo puts the inserts back (machines, params, lock, route)");
    /* the sends */
    p->bus[0][EC_P_FX_MACHINE] = EC_FX_REVERB;
    p->bus[1][EC_P_FX_MACHINE] = EC_FX_TAPE_ECHO;
    p->bus[1][EC_P_DELAY_FEEDBACK] = 77;
    p->pat[p->current].tr[2].v[EC_P_SEND1] = 30;
    p->pat[p->current].tr[2].v[EC_P_SEND2] = 90;
    ec_step_lock_set(step(3), EC_P_SEND2, 5);
    hold_b(4);
    run(500);
    CHECK(ec_hm_raised() == 3 && p->bus[0][EC_P_FX_MACHINE] == EC_FX_REVERB, "B4 held: S 2 raised, nothing moved");
    turn(FM1P_E_K1, 1);
    CHECK(ec_hm_raised() == 3 && p->bus[0][EC_P_FX_MACHINE] == EC_FX_REVERB, "K1 +1: S 2 is already right, stays");
    turn(FM1P_E_K1, -1);
    CHECK(ec_hm_raised() == 2, "K1 -1: moves left");
    let_b(4);
    CHECK(p->bus[0][EC_P_FX_MACHINE] == EC_FX_TAPE_ECHO && p->bus[0][EC_P_DELAY_FEEDBACK] == 77 &&
              p->bus[1][EC_P_FX_MACHINE] == EC_FX_REVERB,
          "S 2 moved left: S1 TAPE ECHO (its settings), S2 REVERB");
    CHECK(p->pat[p->current].tr[2].v[EC_P_SEND1] == 90 && p->pat[p->current].tr[2].v[EC_P_SEND2] == 30 &&
              step(3)->lock_id[1] == EC_P_SEND1,
          "every track's sends swap with them (T3 90 / 30), the lock too: the sound unchanged");
    btn(FM1P_B_GLO, 1);
    run(20);
    press(FM1P_B_OCTDN, 40);
    btn(FM1P_B_GLO, 0);
    run(20);
    CHECK(p->bus[0][EC_P_FX_MACHINE] == EC_FX_REVERB && p->pat[p->current].tr[2].v[EC_P_SEND1] == 30 &&
              step(3)->lock_id[1] == EC_P_SEND2,
          "SHIFT + F1: the sends back");
    hold_b(2);
    run(500);
    CHECK(ec_hm_raised() == 1, "B2 held: I 2 raised");
    let_b(2);
    CHECK(v[EC_P_FX_MACHINE] == EC_FX_REVERB && v[EC_P_INS2_MACHINE] == EC_FX_DELAY && ec_fx_on_sends(),
          "let go without K1: nothing moved, still SENDS");
    B(3, 40);                                        /* a tap: the row */
    CHECK(A->bus == 1 && ec_fx_slot() == 2 && p->bus[0][EC_P_FX_MACHINE] == EC_FX_REVERB, "SENDS: B3 tapped: SEND 1, no swap");
    clear_steps(A->track);
    v[ec_mtx_src_id(mt)] = 1;
    v[ec_mtx_dst_id(mt)] = 0;
    p->bus[0][EC_P_FX_MACHINE] = p->bus[1][EC_P_FX_MACHINE] = EC_FX_NONE;
    p->pat[p->current].tr[2].v[EC_P_SEND1] = p->pat[p->current].tr[2].v[EC_P_SEND2] = 0;
    (void)keep_ins;
}

/* FX SETUP (hold FX): INSERT 1, INSERT 2, SEND 1, SEND 2, MASTER (their types), SEND MODE; K1 changes the row's */
static void test_fx_setup(void)
{
    static const char *const ROW[6] = {"INSERT 1", "INSERT 2", "SEND 1", "SEND 2", "MASTER", "SEND MODE"};
    ec_page_t pg;
    int i, ok = 1;
    to(EC_CAT_FX);
    A->bus = 0;
    press(FM1P_B_FX, 500);
    run(40);
    ec_app_page(A, &pg);
    CHECK(A->settings && pg.n == 6, "FX held: its settings, six rows (%d)", pg.n);
    for (i = 0; i < 6 && i < pg.n; i++)
        ok &= !strcmp(ec_app_item_label(&pg.it[i]), ROW[i]);
    CHECK(ok, "the rows: INSERT 1, INSERT 2, SEND 1, SEND 2, MASTER, SEND MODE");
    turn(FM1P_E_SELECT, 2);
    run(40);
    {
        const float m0 = A->proj.bus[0][EC_P_FX_MACHINE];
        turn(FM1P_E_K1, 1);
        run(40);
        CHECK(A->proj.bus[0][EC_P_FX_MACHINE] == m0 + 1, "SEL 2, K1: SEND 1's type one on (%g -> %g)", m0,
              A->proj.bus[0][EC_P_FX_MACHINE]);
        A->proj.bus[0][EC_P_FX_MACHINE] = m0;
    }
    turn(FM1P_E_SELECT, -1);
    run(40);
    turn(FM1P_E_K1, 3);
    run(40);
    CHECK(tv()[EC_P_INS2_MACHINE] > 1 && tv()[EC_P_INS2_MIX] == EC_PARAMS[ec_fx_mix_id((int)tv()[EC_P_INS2_MACHINE])].def,
          "INSERT 2's row: K1 picks a type, its params at that type's defaults");
    press(FM1P_B_OCTDN, 40);
    tv()[EC_P_INS2_MACHINE] = EC_FX_NONE;
}

/* INS 2 locked on a step, saved and loaded back; an old project */
static void test_ins2_data(void)
{
    static uint8_t rec[1u << 20];
    static ec_project_t q;
    float *v = tv(), x;
    int32_t n;
    to(EC_CAT_FX);
    clear_steps(A->track);
    v[EC_P_INS2_MACHINE] = EC_FX_DELAY;
    ec_ins2_reset(v);
    A->pn.ins_slot = 1;
    A->pn.ins_page = 0;
    run(20);
    key(0, 0, 1);                                    /* W1 held + K2 = a lock on INS 2's P2 (FBK) */
    run(30);
    turn(FM1P_E_K2, 5);
    key(0, 0, 0);
    run(30);
    CHECK(ec_step_lock_get(step(0), EC_P_INS2_P2, &x) && x == EC_PARAMS[EC_P_DELAY_FEEDBACK].def + 5,
          "W1 held + K2: INS 2's FBK locked on step 1 (%g)", x);
    n = ec_project_save(&A->proj, rec, sizeof rec);
    CHECK(n > 0 && ec_project_load(&q, rec, (uint32_t)n) == 0, "saved and loaded (%d bytes)", (int)n);
    {
        const float *w = q.pat[q.current].tr[A->track].v;
        float y = 0;
        CHECK(w[EC_P_INS2_MACHINE] == EC_FX_DELAY && w[EC_P_INS2_P1] == v[EC_P_INS2_P1] &&
                  w[EC_P_INS2_MIX] == v[EC_P_INS2_MIX],
              "INS 2 comes back");
        CHECK(ec_step_lock_get(&q.pat[q.current].tr[A->track].steps[0], EC_P_INS2_P2, &y) && y == x,
              "its lock comes back (%g)", y);
    }
    {   /* 0.5.0's record (host/fixtures/v05_flash.bin): no INS 2, no wet */
        FILE *f = fopen(FIXTURES "/v05_flash.bin", "rb");
        uint32_t len = 0, rsz = 0;
        int t, p, ok = 1;
        if (f) {
            len = (uint32_t)fread(rec, 1, sizeof rec, f);
            fclose(f);
        }
        memcpy(&rsz, rec + 12, 4);
        CHECK(len > 4096 && rsz && ec_project_load(&q, rec + 4096, rsz) == 0, "the 0.5 record loads");
        for (p = 0; p < EC_NPAT; p++)
            for (t = 0; t < EC_NTRACK && q.pat[p].used; t++) {   /* (an unused pattern is prepared from the current) */
                const float *w = q.pat[p].tr[t].v;
                ok &= w[EC_P_INS2_MACHINE] == EC_FX_NONE && w[EC_P_FX_WET] == 127;
            }
        for (t = 0; t < 3; t++)
            ok &= q.bus[t][EC_P_FX_WET] == 127;
        CHECK(ok, "an old project: INS 2 empty, the wet levels full, on every pattern and bus");
    }
    v[EC_P_INS2_MACHINE] = EC_FX_NONE;
    clear_steps(A->track);
    A->pn.ins_slot = 0;
}

static void test_loop_menus(void)
{
    static const char *const HEAD[6] = {"", "LOOP MODE", "MACHINE", "WARP MODE", "VOICE MODE", "ADVANCE"};
    float *v;
    int b;
    to(EC_CAT_SRC);
    v = tv();
    A->track = 1;                                    /* (T2: NEIGHBOR is a machine there) */
    v = tv();
    v[EC_P_MACHINE] = EC_MACH_LOOP;
    for (b = 1; b <= 5; b++) {
        const int id = b == 1 ? EC_P_CHOP_LOOP_MODE : b == 2 ? EC_P_MACHINE : b == 3 ? EC_P_MODE : EC_P_TRIG_POLY;
        const float x0 = b == 5 ? (float)ec_as->advance : v[id];
        hold_b(b);
        run(450);
        CHECK(ec_hm_shown() && !strcmp(ec_hm_head(), HEAD[b]), "LOOP ENGINE B%d held: the %s menu", b, HEAD[b]);
        turn(FM1P_E_SELECT, x0 >= (b == 5 ? 1 : EC_PARAMS[id].nopt) ? -1 : 1);   /* SEL moves it too */
        let_b(b);
        if (b == 5)
            CHECK((float)ec_as->advance != x0, "B5 released: ADVANCE flipped (%d)", ec_as->advance);
        else
            CHECK(v[id] != x0 && fabsf(v[id] - x0) == 1, "B%d released: %s %g -> %g", b, EC_PARAMS[id].name, x0, v[id]);
        if (b == 5)
            ec_pn_item_edit(EC_UX_ADVANCE, x0 > 0 ? 1 : -1);
        else
            v[id] = x0;
    }
    /* a slice machine: B1 is the slice play mode */
    v[EC_P_MACHINE] = EC_MACH_GRID_SLICE;
    hold_b(1);
    run(450);
    turn(FM1P_E_K1, 1);
    let_b(1);
    CHECK(v[EC_P_SLICE_PLAY_MODE] == 2, "SLICE: B1 = the slice play mode (%g)", v[EC_P_SLICE_PLAY_MODE]);
    v[EC_P_SLICE_PLAY_MODE] = 1;
    v[EC_P_MACHINE] = EC_MACH_LOOP;
    /* a tap still turns the page */
    B(2, 40);
    CHECK(!ec_hm_shown() && A->page[EC_CAT_SRC] == ec_cat_key_page(EC_CAT_SRC, 2) && v[EC_P_MACHINE] == EC_MACH_LOOP,
          "B2 tapped: the MACHINE page, nothing picked");
    B(1, 40);
    CHECK(A->page[EC_CAT_SRC] == ec_cat_key_page(EC_CAT_SRC, 1), "B1 tapped: the LOOP page");
    {
        const int pg = A->page[EC_CAT_SRC];
        B(5, 40);
        CHECK(A->page[EC_CAT_SRC] == pg && !ec_hm_shown(), "B5 tapped: nothing (hold only)");
    }
    /* the warp menu while playing: STOP FIRST */
    press(FM1P_B_PLAY, 40);
    hold_b(3);
    run(450);
    turn(FM1P_E_K1, 1);
    let_b(3);
    CHECK(v[EC_P_MODE] == 1 && A->msg_ms && !strcmp(A->msg, "STOP FIRST"), "playing: the warp menu refuses (%s)", A->msg);
    press(FM1P_B_PLAY, 40);
    A->track = 0;
}

/* the meter: it rises with the load (stopped, playing, + two inserts); the same input from power-on reads the same */
static void meter_run(float *r)
{
    float *v;
    to(EC_CAT_FX);
    B(1, 40);
    v = tv();
    run(1000);
    r[0] = ec_src_load();
    press(FM1P_B_PLAY, 40);
    run(1500);
    r[1] = ec_src_load();
    v[EC_P_FX_MACHINE] = EC_FX_REVERB;
    v[EC_P_INS2_MACHINE] = EC_FX_BLACKHOLE;
    ec_ins2_reset(v);
    ec_seq_touch(&A->seq, A->track);
    run(1500);
    r[2] = ec_src_load();
    press(FM1P_B_PLAY, 40);
}

static void test_meter(void)
{
    float a[3], b[3];
    C->shutdown();
    boot(NULL, 0);
    meter_run(a);
    CHECK(a[0] > 0 && a[1] > a[0] && a[2] > a[1], "the meter rises: stopped %.3f, playing %.3f, + REVERB + BLACKHOLE %.3f",
          a[0], a[1], a[2]);
    CHECK(ec_src_load() == ec_dsp_load(A->dsp), "it reads the engine's counted load");
    C->shutdown();
    boot(NULL, 0);                                   /* the same input again from power-on */
    meter_run(b);
    CHECK(!memcmp(a, b, sizeof a), "deterministic: the same input, the same readings (%.6f %.6f)", a[2], b[2]);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("usage: ec_test_fx2 <folder>\n");
        return 2;
    }
    ec_fs_native_root(argv[1], "elasticat");
    boot(NULL, 0);
    test_insert();
    test_ins2_data();
    test_loop_menus();
    test_meter();
    test_bus_k4();
    test_send_mode();
    test_swaps();
    test_fx_setup();
    C->shutdown();
    printf("test_fx2: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
