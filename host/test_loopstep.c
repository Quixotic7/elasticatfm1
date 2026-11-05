/* SPDX-License-Identifier: GPL-3.0-only */
/* test_loopstep: 0.6.9's LOOP ENGINE step entry (docs/UX-V2.md "LOOP ENGINE step entry", the owner's second pass)
 * through the panel:
 *   the loop keys always play live (a live loop key down) and, with steps selected, also write the entry's region
 *   into every selected step; B11 held = the STEP layer (keys select steps: the first replaces, the next add /
 *   remove), let go = loop keys again, selection kept; a B11 hold with no key keeps the selection; a quick click
 *   latches the STEP layer (another quick click unlatches); latched, B11 held = the loop keys (the reverse); B11 never
 *   goes LIVE, ALGO below step 1 does; ALGO selects one step; B11 + OCT- clears the selected steps (one undo); the
 *   RANGE page writes ranges; the knobs lock onto every selected step.
 *   ec_test_loopstep <folder> */
#include "ec_harness.h"

static float lockv(int idx, int id)
{
    float x = -1;
    return ec_step_lock_get(step(idx), id, &x) ? x : -1;
}
static void hold_b11(int d) { key(1, 10, d); run(20); }
static void wkey(int i, int down) { key(0, i, down); run(20); }
static void wtap(int i) { wkey(i, 1); wkey(i, 0); }
static int live_keys(void) { return (int)A->seq.tr[A->track].live_keys; }

static void test_layers(void)
{
    float *v;
    to(EC_CAT_SRC);
    v = ec_app_params(A);
    clear_steps(A->track);
    v[EC_P_LOOP_DIVISION] = 16;
    v[EC_P_PATTERN_STEPS] = 16;
    CHECK(ec_lp_first() < 0 && !ec_lp_step_layer(), "LOOP ENGINE starts LIVE, loop keys");
    /* LIVE: a key is the live loop key, nothing written */
    wkey(2, 1);
    CHECK(live_keys() != 0, "LIVE: the key plays the loop live");
    wkey(2, 0);
    CHECK(lockv(0, EC_P_LOOP_START) < 0 && !step(0)->trig, "LIVE: no step written");
    CHECK(v[EC_P_LOOP_START] == 16 && v[EC_P_LOOP_END] == 24, "0.6.12 LIVE: the entry set the track's region (%g %g)",
          v[EC_P_LOOP_START], v[EC_P_LOOP_END]);
    v[EC_P_LOOP_START] = 0, v[EC_P_LOOP_END] = 128;
    ec_seq_touch(&A->seq, A->track);
    /* hold B11: the STEP layer; W3 + W5 select steps 3 and 5 */
    hold_b11(1);
    CHECK(ec_lp_step_layer(), "B11 held: the STEP layer");
    run(60);
    CHECK(lbl(FM1CORE_LBL_WHITE0 + 2, "STEP 3"), "the keys say STEP n (%s)", L(FM1CORE_LBL_WHITE0 + 2));
    wkey(2, 1);                                      /* held together: one selection (FIZZ) */
    wkey(4, 1);
    wkey(2, 0);
    wkey(4, 0);
    CHECK(ec_lp_is_sel(2) && ec_lp_is_sel(4) && ec_lp_count() == 2 && live_keys() == 0,
          "W3 + W5 held together: steps 3 and 5 selected, nothing played");
    hold_b11(0);
    CHECK(!ec_lp_step_layer() && ec_lp_count() == 2, "B11 let go: loop keys again, the selection kept");
    /* a loop key: live AND written into both */
    wkey(6, 1);
    CHECK(live_keys() != 0, "a loop key with steps selected still plays live");
    wkey(6, 0);
    CHECK(lockv(2, EC_P_LOOP_START) == 48 && lockv(2, EC_P_LOOP_END) == 56 && lockv(4, EC_P_LOOP_START) == 48 &&
              step(2)->trig && step(4)->trig,
          "L7: steps 3 and 5 = division 7 (%g..%g), trigs", lockv(2, EC_P_LOOP_START), lockv(2, EC_P_LOOP_END));
    /* a span */
    wkey(8, 1);
    wkey(11, 1);
    wkey(8, 0);
    wkey(11, 0);
    CHECK(lockv(2, EC_P_LOOP_START) == 64 && lockv(2, EC_P_LOOP_END) == 96 && lockv(4, EC_P_LOOP_END) == 96,
          "L9 + L12 let go together: 9..12 into both (%g..%g)", lockv(2, EC_P_LOOP_START), lockv(2, EC_P_LOOP_END));
    wkey(8, 1);                                    /* 0.6.21 (owner): L12 let go, L9 held: the span is kept; L10
                                                    * pressed: it refits to L9 + L10 */
    wkey(11, 1);
    wkey(11, 0);
    run(200);
    CHECK(live_keys() != 0, "L9 still held");
    wkey(9, 1);
    wkey(9, 0);
    wkey(8, 0);
    CHECK(lockv(2, EC_P_LOOP_START) == 64 && lockv(2, EC_P_LOOP_END) == 80, "a press refits the span: L9 + L10 (%g..%g)",
          lockv(2, EC_P_LOOP_START), lockv(2, EC_P_LOOP_END));
    CHECK(ec_lp_count() == 2, "nothing advances: still steps 3 and 5");
    /* a B11 hold with no key keeps the selection */
    hold_b11(1);
    run(500);
    hold_b11(0);
    CHECK(ec_lp_count() == 2 && !ec_lp_step_layer(), "B11 held, nothing pressed: the selection kept, never LIVE");
    /* FIZZ: every step key let go, the next press starts a new selection */
    hold_b11(1);
    wtap(7);
    CHECK(ec_lp_count() == 1 && ec_lp_is_sel(7), "W8: a new selection, step 8 alone (%d)", ec_lp_count());
    wtap(7);
    CHECK(ec_lp_count() == 1 && ec_lp_is_sel(7), "W8 again: still step 8 (no toggle)");
    wtap(9);
    CHECK(ec_lp_count() == 1 && ec_lp_is_sel(9), "W10 after letting go: step 10 alone");
    hold_b11(0);
    CHECK(ec_lp_count() == 1 && ec_lp_is_sel(9) && A->pn.target == 10, "W10: step 10 (target %d)", A->pn.target);
    /* a quick click: STEP latched; a key selects; B11 held there = loop keys (the reverse) */
    B(11, 40);
    CHECK(ec_lp_step_layer(), "a quick click: STEP latched");
    wtap(0);
    CHECK(ec_lp_count() == 1 && ec_lp_is_sel(0) && live_keys() == 0, "latched: W1 selects step 1 (a new selection)");
    hold_b11(1);
    CHECK(!ec_lp_step_layer(), "latched + B11 held: the loop keys");
    wkey(1, 1);
    CHECK(live_keys() != 0, "...live");
    wkey(1, 0);
    hold_b11(0);
    CHECK(lockv(0, EC_P_LOOP_START) == 8 && lockv(0, EC_P_LOOP_END) == 16 && ec_lp_step_layer(),
          "L2 written into step 1; B11 let go (used): still STEP latched");
    B(11, 40);
    CHECK(!ec_lp_step_layer() && ec_lp_count() == 1, "a quick click again: loop keys, the selection kept");
    /* B11 + OCT-: clear the selected steps; undo */
    hold_b11(1);
    press(FM1P_B_OCTDN, 40);
    hold_b11(0);
    CHECK(!step(0)->trig && lockv(0, EC_P_LOOP_START) < 0 && !ec_lp_step_layer(),
          "B11 + OCT-: step 1 cleared (trig and locks), not a quick click");
    btn(FM1P_B_GLO, 1);
    run(20);
    press(FM1P_B_OCTDN, 40);
    btn(FM1P_B_GLO, 0);
    run(20);
    CHECK(step(0)->trig && lockv(0, EC_P_LOOP_START) == 8, "SHIFT + F1: step 1 back");
    if (A->pn.shift_sticky)
        press(FM1P_B_GLO, 40);
    {   /* 0.6.9: a loop key held with steps selected lights the keys exactly as live does */
        int live[16], sel[16], i, same = 1;
        turn(FM1P_E_ALGO, -20);                      /* LIVE */
        wkey(6, 1);
        run(60);
        for (i = 0; i < 16; i++)
            live[i] = led_key(0, i);
        wkey(6, 0);
        v[EC_P_LOOP_START] = 0, v[EC_P_LOOP_END] = 128;   /* (0.6.12: LIVE set the region to key 7; the same base) */
        ec_seq_touch(&A->seq, A->track);
        turn(FM1P_E_ALGO, 3);                        /* step 3 selected */
        wkey(6, 1);
        run(60);
        for (i = 0; i < 16; i++) {
            sel[i] = led_key(0, i);
            same &= i == 2 || sel[i] == live[i];   /* (0.6.14: the selected step's own key blinks) */
        }
        wkey(6, 0);
        CHECK(same && ec_lp_count() == 1, "L7 held: the same key LEDs with a step selected as live");
        clear_steps(A->track);
        turn(FM1P_E_ALGO, -20);
        turn(FM1P_E_ALGO, 1);                        /* back on step 1 for the next checks */
        wtap(1);
    }
    /* ALGO: one step; below 1 = LIVE */
    turn(FM1P_E_ALGO, 2);
    CHECK(ec_lp_count() == 1 && ec_lp_is_sel(2), "ALGO +2 from step 1: step 3 alone");
    turn(FM1P_E_ALGO, -5);
    CHECK(ec_lp_count() == 0 && ec_lp_first() < 0, "ALGO below step 1: LIVE");
}

/* the knobs lock onto every selected step */
static void test_knobs(void)
{
    float *v = ec_app_params(A);
    const float track = v[EC_P_PITCH];
    hold_b11(1);
    wkey(4, 1);
    wkey(5, 1);
    wkey(4, 0);
    wkey(5, 0);
    hold_b11(0);
    turn(FM1P_E_K1, 3);                              /* LOOP page K1: PITCH */
    CHECK(lockv(4, EC_P_PITCH) >= 0 && lockv(5, EC_P_PITCH) == lockv(4, EC_P_PITCH) && v[EC_P_PITCH] == track,
          "K1 locks PITCH on steps 5 and 6 (%g), the track value untouched", lockv(4, EC_P_PITCH));
    CHECK(ec_app_held_step(A) == 4, "the first selected counts as the held step (the cells)");
    turn(FM1P_E_ALGO, -20);
    turn(FM1P_E_K1, 1);
    CHECK(v[EC_P_PITCH] != track, "LIVE: K1 is the track's PITCH again");
    v[EC_P_PITCH] = track;
}

/* RANGE page: the keys write the range over the whole sample into the selection */
static void test_range(void)
{
    int n;
    to(EC_CAT_SRC);
    for (n = 0; n < 6; n++) {                        /* tap B4 (RANGE) */
        ec_page_t pg;
        ec_app_page(A, &pg);
        if (pg.pic == EC_PIC_WAVE_RANGE)
            break;
        B(4, 40);
    }
    hold_b11(1);
    wtap(6);
    hold_b11(0);
    wkey(1, 1);
    wkey(5, 1);
    wkey(1, 0);
    wkey(5, 0);
    CHECK(lockv(6, EC_P_RANGE_START) == 8 && lockv(6, EC_P_RANGE_END) == 48 && lockv(6, EC_P_LOOP_START) < 0,
          "RANGE: W2 + W6 let go together = step 7's range 8..48 (%g..%g), no region lock",
          lockv(6, EC_P_RANGE_START), lockv(6, EC_P_RANGE_END));
    turn(FM1P_E_ALGO, -20);
}

/* 0.6.11 (QA findings 1 / 4): the selection and the target step are each track's own */
static void test_tracks(void)
{
    to(EC_CAT_SRC);
    ec_pn_select_target(0);
    hold_b11(1);
    wtap(2);
    hold_b11(0);
    CHECK(ec_lp_count() == 1 && ec_lp_is_sel(2) && A->pn.target == 3, "T1: step 3 selected (target 3)");
    ec_pn_select_target(1);
    CHECK(A->track == 1 && ec_lp_count() == 0 && A->pn.target == 0, "T2: nothing selected, LIVE (%d %d)",
          ec_lp_count(), A->pn.target);
    hold_b11(1);
    wtap(4);
    hold_b11(0);
    CHECK(ec_lp_count() == 1 && ec_lp_is_sel(4) && A->pn.target == 5, "T2: step 5 selected, its own");
    ec_pn_select_target(0);
    CHECK(ec_lp_count() == 1 && ec_lp_is_sel(2) && A->pn.target == 3, "back on T1: step 3 again (target %d)",
          A->pn.target);
    ec_pn_select_target(1);
    CHECK(ec_lp_is_sel(4) && A->pn.target == 5, "T2 again: step 5 kept");
    ec_pn_select_target(0);
}

/* 0.6.13 (owner): B11 held, B1..B10 are the pages as FIZZ: tap select, hold mute, double-tap solo, two held = loop */
static void test_pages_b11(void)
{
    float *v;
    uint16_t m;
    to(EC_CAT_SRC);
    ec_pn_select_target(0);
    v = ec_app_params(A);
    v[EC_P_PATTERN_STEPS] = 64;
    ec_seq_timing_changed(&A->seq);
    ec_e5_set_page_loop(&A->proj, A->track, 0, 4);
    hold_b11(1);
    key(1, 1, 1);
    run(40);
    key(1, 1, 0);
    run(20);
    CHECK(A->step_page == 1 && ec_lp_b11_held(), "B11 + B2 tap: page 2 (%d)", A->step_page);
    key(1, 2, 1);
    run(450);
    key(1, 2, 0);
    run(20);
    m = ec_e5_page_loop_mask(&A->proj, A->track, 4);
    CHECK(m == 0xB && !strcmp(A->msg, "PAGE 3 MUTED"), "B3 held: page 3 muted (mask %x, %s)", m, A->msg);
    key(1, 3, 1);
    run(40);
    key(1, 3, 0);
    run(60);
    key(1, 3, 1);
    run(40);
    key(1, 3, 0);
    run(20);
    m = ec_e5_page_loop_mask(&A->proj, A->track, 4);
    CHECK(m == 0x8 && A->step_page == 3, "B4 double-tap: page 4 solo (mask %x)", m);
    key(1, 0, 1);
    run(40);
    key(1, 1, 1);
    run(40);
    key(1, 0, 0);
    key(1, 1, 0);
    run(20);
    m = ec_e5_page_loop_mask(&A->proj, A->track, 4);
    CHECK(m == 0x3 && !strcmp(A->msg, "LOOP PAGES 1-2"), "B1 + B2 held: loop pages 1-2 (mask %x, %s)", m, A->msg);
    key(1, 0, 1);                                  /* 0.6.14: B1 + B6 past the end (4 pages): the track grows */
    run(40);
    key(1, 5, 1);
    run(40);
    key(1, 0, 0);
    key(1, 5, 0);
    run(20);
    CHECK((int)v[EC_P_PATTERN_STEPS] == 96 && ec_e5_page_loop_mask(&A->proj, A->track, 6) == 0x3F,
          "B1 + B6 held: LEN 96, pages 1-6 loop (%g %x)", v[EC_P_PATTERN_STEPS],
          ec_e5_page_loop_mask(&A->proj, A->track, 6));
    turn(FM1P_E_K1, 1);
    CHECK((int)v[EC_P_PATTERN_STEPS] == 97 && !strcmp(A->msg, "LEN 97"), "B11 + K1: LEN 97 (%s)", A->msg);
    turn(FM1P_E_K2, 1);
    CHECK((int)v[EC_P_RATE] == EC_RATE_UNITY + 1 && !strncmp(A->msg, "RATE", 4), "B11 + K2: the rate up (%s)", A->msg);
    turn(FM1P_E_K2, -1);
    turn(FM1P_E_K1, -1);
    hold_b11(0);
    CHECK(!ec_lp_step_layer(), "B11 let go after page keys: no layer latch");
    v[EC_P_PATTERN_STEPS] = 64;                    /* (back to 4 pages for the checks below) */
    ec_seq_timing_changed(&A->seq);
    ec_e5_set_page_loop(&A->proj, A->track, 0, 4);
    /* 0.6.14: ALGO stays on the page; PRESETS turns it; B11 + OCT+ = LIVE */
    A->step_page = 1;
    turn(FM1P_E_ALGO, 3);
    CHECK(A->pn.target == 19 && ec_lp_is_sel(18), "ALGO +3 on page 2: step 19 (target %d)", A->pn.target);
    turn(FM1P_E_ALGO, 20);
    CHECK(A->pn.target == 32 && A->step_page == 1, "ALGO past the page's end: step 32, the page stays (%d)",
          A->pn.target);
    turn(FM1P_E_ALGO, -20);
    CHECK(A->pn.target == 0 && ec_lp_first() < 0, "ALGO full left: LIVE");
    turn(FM1P_E_PRESETS, 1);
    CHECK(A->step_page == 2 && !strcmp(A->msg, "PAGE 3/4"), "PRESETS: page 3 (%d %s)", A->step_page, A->msg);
    turn(FM1P_E_PRESETS, -5);
    CHECK(A->step_page == 0, "PRESETS left: page 1 (stops there)");
    turn(FM1P_E_ALGO, 2);
    hold_b11(1);
    press(FM1P_B_OCTUP, 40);
    hold_b11(0);
    CHECK(A->pn.target == 0 && ec_lp_first() < 0 && !strcmp(A->msg, "LIVE") && !ec_lp_step_layer(),
          "B11 + OCT+: LIVE, no latch (%s)", A->msg);
    ec_e5_set_page_loop(&A->proj, A->track, 0, 4);
    v[EC_P_PATTERN_STEPS] = 16;
    ec_seq_timing_changed(&A->seq);
    A->step_page = 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("usage: ec_test_loopstep <folder>\n");
        return 2;
    }
    ec_fs_native_root(argv[1], "elasticat");
    boot(NULL, 0);
    test_layers();
    test_knobs();
    test_range();
    test_tracks();
    test_pages_b11();
    C->shutdown();
    printf("test_loopstep: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
