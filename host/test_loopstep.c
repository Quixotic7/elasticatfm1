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
    CHECK(lockv(0, EC_P_LOOP_START) < 0 && !step(0)->trig, "LIVE: nothing written");
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
          "L9 + L12 held together: 9..12 into both (%g..%g)", lockv(2, EC_P_LOOP_START), lockv(2, EC_P_LOOP_END));
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
        turn(FM1P_E_ALGO, 3);                        /* step 3 selected */
        wkey(6, 1);
        run(60);
        for (i = 0; i < 16; i++) {
            sel[i] = led_key(0, i);
            same &= sel[i] == live[i];
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
          "RANGE: W2 + W6 = step 7's range 8..48 (%g..%g), no region lock", lockv(6, EC_P_RANGE_START),
          lockv(6, EC_P_RANGE_END));
    turn(FM1P_E_ALGO, -20);
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
    C->shutdown();
    printf("test_loopstep: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
