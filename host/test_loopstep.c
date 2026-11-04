/* SPDX-License-Identifier: GPL-3.0-only */
/* test_loopstep: 0.6.6's LOOP ENGINE step entry (docs/UX-V2.md "LOOP ENGINE step entry") through the panel:
 *   ALGO picks the target step (LIVE below step 1); a white key writes the step's region (one key: its division),
 *   puts a trig on it and moves on (ADVANCE); keys held together write their span; SHIFT does the other advance;
 *   B11 held + a white key jumps to that step, a B11 tap goes LIVE; SHIFT + B11 is the zoom; the RANGE page writes the
 *   range; with a target the knobs lock onto it (and F1 + knob clears only its lock); LIVE keys stay the live loop
 *   keys (no lock written); one UNDO takes an entry back.
 *   ec_test_loopstep <folder> */
#include "ec_harness.h"

static float lockv(int idx, int id)
{
    float x = -1;
    return ec_step_lock_get(step(idx), id, &x) ? x : -1;
}
static int iv_div(void) { return (int)floorf(ec_app_params(A)[EC_P_LOOP_DIVISION] + 0.5f); }
static void hold_b(int b) { key(1, b - 1, 1); run(20); }
static void let_b(int b) { key(1, b - 1, 0); run(20); }
static void wkey(int i, int down) { key(0, i, down); run(20); }

static void test_entry(void)
{
    float *v;
    to(EC_CAT_SRC);
    v = ec_app_params(A);
    clear_steps(A->track);
    v[EC_P_LOOP_DIVISION] = 16;
    v[EC_P_PATTERN_STEPS] = 16;
    CHECK(ec_as->advance, "ADVANCE is on by default");
    CHECK(A->pn.target == 0, "LOOP ENGINE starts LIVE");
    /* LIVE: a key is the live loop key, nothing written */
    wkey(2, 1);
    wkey(2, 0);
    CHECK(lockv(0, EC_P_LOOP_START) < 0 && !step(0)->trig, "LIVE: a key writes nothing");
    turn(FM1P_E_ALGO, 1);
    CHECK(A->pn.target == 1, "ALGO +1: step 1 (%d)", A->pn.target);
    wkey(2, 1);
    wkey(2, 0);
    CHECK(step(0)->trig && lockv(0, EC_P_LOOP_START) == 16 && lockv(0, EC_P_LOOP_END) == 24,
          "W3: step 1 = division 3 (start %g end %g), a trig", lockv(0, EC_P_LOOP_START), lockv(0, EC_P_LOOP_END));
    CHECK(A->pn.target == 2, "ADVANCE: on to step 2 (%d)", A->pn.target);
    /* a span: W5 + W8 held together */
    wkey(4, 1);
    wkey(7, 1);
    CHECK(lockv(1, EC_P_LOOP_START) < 0, "nothing written while keys are still held");
    wkey(4, 0);
    CHECK(lockv(1, EC_P_LOOP_START) < 0, "one key let go, one still held: still nothing");
    wkey(7, 0);
    CHECK(lockv(1, EC_P_LOOP_START) == 32 && lockv(1, EC_P_LOOP_END) == 64 && A->pn.target == 3,
          "the last key let go: step 2 = 5..8 (%g..%g), on to 3", lockv(1, EC_P_LOOP_START), lockv(1, EC_P_LOOP_END));
    /* SHIFT: the other advance */
    shift_down(1);
    wkey(0, 1);
    wkey(0, 0);
    shift_down(0);
    CHECK(lockv(2, EC_P_LOOP_START) == 0 && lockv(2, EC_P_LOOP_END) == 8 && A->pn.target == 3,
          "SHIFT + W1: step 3 = 1, stays on 3 (%d)", A->pn.target);
    /* undo */
    btn(FM1P_B_GLO, 1);
    run(20);
    press(FM1P_B_OCTDN, 40);
    btn(FM1P_B_GLO, 0);
    run(20);
    CHECK(lockv(2, EC_P_LOOP_START) < 0 && !step(2)->trig, "SHIFT + F1: the entry undone");
    if (A->pn.shift_sticky)
        press(FM1P_B_GLO, 40);
    /* B11 held + a key: jump; a B11 tap: LIVE */
    hold_b(11);
    wkey(9, 1);
    wkey(9, 0);
    let_b(11);
    CHECK(A->pn.target == 10, "B11 + W10: step 10 (%d)", A->pn.target);
    CHECK(iv_div() == 16, "B11 is not the zoom any more");
    B(11, 40);
    CHECK(A->pn.target == 0, "B11 tapped: LIVE (%d)", A->pn.target);
    /* B11 held + K1: the zoom (not a LIVE tap) */
    A->pn.target = 4;
    hold_b(11);
    turn(FM1P_E_K1, 1);
    CHECK(iv_div() == 32 && !A->pn.zoom_half, "B11 + K1 +1: 32, first half (LOOP DIV %d)", iv_div());
    turn(FM1P_E_K1, 1);
    CHECK(iv_div() == 32 && A->pn.zoom_half, "K1 +1 again: the second half");
    turn(FM1P_E_K1, -2);
    let_b(11);
    CHECK(iv_div() == 16 && A->pn.target == 4, "K1 -2: back to 16; letting go of B11 after K1 is not LIVE (%d)",
          A->pn.target);
    A->pn.target = 0;
}

/* the knobs lock onto the target; F1 + knob clears only its lock */
static void test_knobs(void)
{
    float *v = ec_app_params(A);
    const float track = v[EC_P_PITCH];
    A->pn.target = 5;
    A->step_page = 0;
    turn(FM1P_E_K1, 3);                              /* LOOP page K1: PITCH */
    CHECK(lockv(4, EC_P_PITCH) >= 0 && v[EC_P_PITCH] == track,
          "with a target, K1 locks PITCH on step 5 (%g), the track value untouched", lockv(4, EC_P_PITCH));
    CHECK(ec_app_held_step(A) == 4, "the target counts as the held step (the cells show its locks)");
    A->pn.target = 0;
    turn(FM1P_E_K1, 1);
    CHECK(v[EC_P_PITCH] != track, "LIVE: K1 is the track's PITCH again");
    v[EC_P_PITCH] = track;
}

/* RANGE page: the keys write the range over the whole sample */
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
    {
        ec_page_t pg;
        ec_app_page(A, &pg);
        CHECK(pg.pic == EC_PIC_WAVE_RANGE, "RANGE page");
    }
    A->pn.target = 7;
    wkey(1, 1);
    wkey(5, 1);
    wkey(1, 0);
    wkey(5, 0);
    CHECK(lockv(6, EC_P_RANGE_START) == 8 && lockv(6, EC_P_RANGE_END) == 48 && lockv(6, EC_P_LOOP_START) < 0,
          "RANGE: W2 + W6 = step 7's range 8..48 (%g..%g), no region lock", lockv(6, EC_P_RANGE_START),
          lockv(6, EC_P_RANGE_END));
    A->pn.target = 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("usage: ec_test_loopstep <folder>\n");
        return 2;
    }
    ec_fs_native_root(argv[1], "elasticat");
    boot(NULL, 0);
    test_entry();
    test_knobs();
    test_range();
    C->shutdown();
    printf("test_loopstep: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
