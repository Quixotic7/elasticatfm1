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
    B(2, 40);                                        /* (from SENDS: B1 jumps to its group's first page) */
    B(1, 40);
    CHECK(A->page[EC_CAT_FX] == insert_page() && !A->bus && ec_src_insert_page(), "B1: the INSERT page");
    CHECK(A->pn.ins_slot == 0 && A->pn.ins_page == 0 && cell_id(3) == EC_P_FX_MIX, "INS 1 P1, K4 = its MIX (%d)",
          cell_id(3));
    turn(FM1P_E_SELECT, 1);                          /* 0.6.2: INS 2 is the next page of the list (DELAY: one P) */
    CHECK(A->pn.ins_slot == 1 && A->page[EC_CAT_FX] == insert_page(), "SEL: INS 2 P1 (the page stays)");
    turn(FM1P_E_SELECT, -1);
    CHECK(A->pn.ins_slot == 0, "SEL back: INS 1");
    turn(FM1P_E_SELECT, 1);

    /* the type menu: held 400 ms it shows, K1 moves, letting go picks */
    hold_b(1);
    run(300);
    CHECK(!ec_hm_shown(), "B1 held 300 ms: no menu yet");
    run(150);
    CHECK(ec_hm_shown() && ec_hm_head() && !strcmp(ec_hm_head(), "INS 2 TYPE"), "B1 held 450 ms: INS 2's type menu (%s)",
          ec_hm_head() ? ec_hm_head() : "-");
    CHECK(ec_hm_count() == EC_FX_COUNT && ec_hm_cursor() == 0, "18 types, the cursor on NONE");
    turn(FM1P_E_K1, 7);
    CHECK(ec_hm_cursor() == 7 && !strcmp(ec_hm_label(7), "BLACKHOLE"), "K1 7: BLACKHOLE (%s)", ec_hm_label(ec_hm_cursor()));
    CHECK(tv()[EC_P_INS2_MACHINE] == EC_FX_NONE, "nothing changes before the release");
    v[EC_P_INS2_P1] = 99;
    let_b(1);
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
    hold_b(1);
    run(40);
    turn(FM1P_E_K1, -1);
    CHECK(ec_hm_shown(), "B1 held + K1 at 40 ms: the menu at once");
    let_b(1);
    CHECK(v[EC_P_INS2_MACHINE] == EC_FX_ECHO, "K1 -1: ECHO (%g)", v[EC_P_INS2_MACHINE]);
    /* one UNDO takes the pick back: the machine and its params */
    btn(FM1P_B_GLO, 1);
    run(20);
    press(FM1P_B_OCTDN, 40);
    btn(FM1P_B_GLO, 0);
    run(20);
    CHECK(v[EC_P_INS2_MACHINE] == EC_FX_BLACKHOLE && v[EC_P_INS2_P1] == EC_PARAMS[EC_P_REVERB_SIZE].def,
          "SHIFT + F1: INS 2 back to BLACKHOLE with its params (%g %g)", v[EC_P_INS2_MACHINE], v[EC_P_INS2_P1]);

    /* the insert key tapped on INSERT: P1 -> P2 -> P1 (BLACKHOLE: six params) */
    B(1, 40);
    CHECK(A->pn.ins_page == 1 && cell_id(0) == EC_P_INS2_P4 && cell_id(3) == EC_P_INS2_MIX,
          "tap: P2 (K1 = P4 %d, K4 still MIX)", cell_id(0));
    B(1, 40);                                        /* 0.6.2: past the group's last page: its first */
    CHECK(A->pn.ins_slot == 0 && A->pn.ins_page == 0, "tap again: INS 1 P1 (%d %d)", A->pn.ins_slot,
          A->pn.ins_page);

    /* K4 = MIX on several types */
    {
        static const int M[] = {EC_FX_DELAY, EC_FX_LIMIT, EC_FX_DJ_EQ, EC_FX_DESTROY, EC_FX_REVERB};
        unsigned i;
        turn(FM1P_E_SELECT, -1);
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
        for (i = 0; i < 4 && A->pn.ins_slot != 1; i++)   /* SEL on through INS 1's pages to INS 2 */
            turn(FM1P_E_SELECT, 1);
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
    C->shutdown();
    printf("test_fx2: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
