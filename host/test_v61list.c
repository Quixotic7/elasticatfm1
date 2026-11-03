/* SPDX-License-Identifier: GPL-3.0-only */
/* test_v61list: 0.6.1's list menus and SHIFT held = sticky (docs/UX-V2.md §10 "List menus", "SHIFT held = SHIFT
 * sticky") through the panel (host/ec_harness.h), the demo folder and the shipped layout, one gesture per check:
 *   every list (APP SETTINGS, the record settings, a mode's settings, the recorder's setup): SEL moves the one
 *   selected row, K1 changes only that row, K2..K4 change nothing, the panel labels say K1 = the row and nothing for
 *   K2..K4; THEME is two lines on the canvas (the band 22 units high, the name on the second line), a short row one;
 *   SHIFT held: its screen from the press on, SEL = BPM, a shortcut (SHIFT + B1 tap tempo, SHIFT + W2 the track) as
 *   with sticky SHIFT, still its screen after use, let go after use: not sticky, the mode back; a quick tap: sticky.
 *   ec_test_v61list <folder> */
#include "ec_harness.h"

#include "ec_gfx.h"
#include "ec_palette.h"

/* the screen now: the canvas after a forced draw */
static const uint8_t *draw(void)
{
    A->redraw = 1;
    C->frame();
    return ec_gfx_canvas();
}
static int px(int x, int y) { return draw()[y * EC_GW + x]; }
/* pixels of slot c in the rectangle */
static int count(int c, int x0, int y0, int x1, int y1)
{
    const uint8_t *cv = draw();
    int x, y, n = 0;
    for (y = y0; y < y1; y++)
        for (x = x0; x < x1; x++)
            n += cv[y * EC_GW + x] == c;
    return n;
}
static int knob_lbl(int k, const char *want) { return lbl(FM1CORE_LBL_KNOB0 + FM1P_E_K1 + k, want); }
static const char *knob_L(int k) { return L(FM1CORE_LBL_KNOB0 + FM1P_E_K1 + k); }
/* K2..K4 have no label */
static int k234_blank(void) { return knob_lbl(1, "") && knob_lbl(2, "") && knob_lbl(3, ""); }

/* ------------------------------------------------------------------------------------------- APP SETTINGS --- */
static void test_appset(void)
{
    ec_appset_t s0;
    int k;
    to(EC_CAT_FILT);
    shift_down(1);
    press(FM1P_B_HOME, 40);
    shift_down(0);
    run(40);
    CHECK(A->pn.as_open && A->pn.as_row == 0, "SHIFT + HOME: APP SETTINGS, THEME selected");
    CHECK(knob_lbl(0, "THEME") && k234_blank(), "labels: K1 %s, K2..K4 '%s' '%s' '%s'", knob_L(0), knob_L(1),
          knob_L(2), knob_L(3));
    /* THEME: two lines (the band y 12..35, the name under the label), the accent edge 2 units */
    CHECK(px(10, 12) == EC_C_L3 && px(10, 35) == EC_C_L3 && px(10, 36) == EC_C_BG && px(10, 11) == EC_C_BG,
          "THEME's band: 24 units (%d %d %d)", px(10, 12), px(10, 35), px(10, 36));
    CHECK(px(0, 20) == EC_C_ACC && px(1, 20) == EC_C_ACC && px(2, 20) != EC_C_ACC, "the accent edge: 2 units");
    CHECK(count(EC_C_L15, 40, 24, 116, 34) > 10 && count(EC_C_L15, 70, 12, 116, 23) == 0,
          "THEME's name on the second line (%d on line 2, %d on line 1)", count(EC_C_L15, 40, 24, 116, 34),
          count(EC_C_L15, 70, 12, 116, 23));
    s0 = A->pn.as;
    for (k = 1; k < 4; k++)
        turn(FM1P_E_K1 + k, 1);
    run(20);
    CHECK(!memcmp(&s0, &A->pn.as, sizeof s0), "K2..K4: nothing changes in a list");
    turn(FM1P_E_SELECT, 1);
    run(40);
    CHECK(A->pn.as_row == 1 && knob_lbl(0, "KNOB SPEED") && k234_blank(), "SEL: KNOB SPEED selected (K1 %s)",
          knob_L(0));
    /* a one-line row: its band 14 units (the row after THEME: y 36..49) */
    CHECK(px(10, 36) == EC_C_L3 && px(10, 49) == EC_C_L3 && px(10, 50) == EC_C_BG && px(10, 35) == EC_C_BG,
          "KNOB SPEED's band: one line, 14 units");
    turn(FM1P_E_K1, -1);
    run(20);
    CHECK(A->pn.as.knob == s0.knob - 1 && A->pn.as.theme == s0.theme && A->pn.as.preview == s0.preview,
          "K1: KNOB SPEED only (%d -> %d)", s0.knob, A->pn.as.knob);
    press(FM1P_B_OCTDN, 40);
    CHECK(!A->pn.as_open && A->pn.as.knob == s0.knob, "F1: back, undone");
}

/* ------------------------------------------------------------------------------------------- record settings --- */
static void test_recset(void)
{
    ec_recset_t r0;
    int k;
    to(EC_CAT_FILT);
    press(FM1P_B_REC, 500);
    run(40);
    CHECK(A->pn.rs_open && A->pn.rs_row == 0 && knob_lbl(0, "MODE") && k234_blank(),
          "REC held: the record settings, MODE on K1 (K2 '%s')", knob_L(1));
    r0 = A->proj.rec;
    for (k = 1; k < 4; k++)
        turn(FM1P_E_K1 + k, 1);
    run(20);
    CHECK(!memcmp(&r0, &A->proj.rec, sizeof r0), "K2..K4: nothing changes");
    turn(FM1P_E_SELECT, 2);
    run(40);
    CHECK(A->pn.rs_row == 2 && knob_lbl(0, "COUNT-IN"), "SEL 2: COUNT-IN (K1 %s)", knob_L(0));
    turn(FM1P_E_K1, 1);
    run(20);
    CHECK(A->proj.rec.countin == r0.countin + 1 && A->proj.rec.mode == r0.mode && A->proj.rec.bars == r0.bars &&
              A->proj.rec.metro == r0.metro,
          "K1: COUNT-IN only (%d)", A->proj.rec.countin);
    turn(FM1P_E_SELECT, 20);
    run(40);
    CHECK(A->pn.rs_row == EC_RS_NROW - 1 && knob_lbl(0, "QUANTIZE"), "SEL past the end: the last row (%d)",
          A->pn.rs_row);
    CHECK(count(EC_C_L3, 4, 14, 100, 120) > 0, "the selected row is on screen (scrolled)");
    press(FM1P_B_OCTDN, 40);
    CHECK(!A->pn.rs_open && !memcmp(&r0, &A->proj.rec, sizeof r0), "F1: back, undone");
}

/* ------------------------------------------------------------------------------------------- a mode's settings --- */
static void test_mode_settings(void)
{
    float v0[EC_NP];
    float *v;
    int k, i, diff;
    to(EC_CAT_AMP);
    press(BTN_OF_MB[ec_mode_button(EC_CAT_AMP)], 500);
    run(40);
    CHECK(A->settings && A->cat == EC_CAT_AMP && A->ux.set_row == 0, "AMP held: its settings, row 1");
    CHECK(knob_lbl(0, "ENV MODE") && k234_blank() && lbl(FM1CORE_LBL_KNOB0 + FM1P_E_SELECT, "ROW"),
          "labels: SEL ROW, K1 %s, K2 '%s'", knob_L(0), knob_L(1));
    v = ec_app_params(A);
    memcpy(v0, v, sizeof v0);
    for (k = 1; k < 4; k++)
        turn(FM1P_E_K1 + k, 2);
    run(20);
    CHECK(!memcmp(v0, v, sizeof v0), "K2..K4: no param changes");
    turn(FM1P_E_SELECT, 1);
    run(40);
    CHECK(A->ux.set_row == 1 && knob_lbl(0, "ENV RANGE"), "SEL: ENV RANGE (K1 %s)", knob_L(0));
    turn(FM1P_E_K1, 1);
    run(20);
    for (i = 0, diff = 0; i < EC_NP; i++)
        diff += i != EC_P_ENV_RANGE && v[i] != v0[i];
    CHECK(v[EC_P_ENV_RANGE] != v0[EC_P_ENV_RANGE] && !diff, "K1: ENV RANGE only (%g -> %g, %d others)",
          v0[EC_P_ENV_RANGE], v[EC_P_ENV_RANGE], diff);
    turn(FM1P_E_SELECT, 3);
    run(40);
    CHECK(knob_lbl(0, "HOLD TIME"), "SEL on to the 5th row (0.6.0's second lane): K1 %s", knob_L(0));
    press(FM1P_B_OCTDN, 40);
    CHECK(!A->settings, "F1: closed");
    /* FX's settings (0.6.3): every slot's type, then SEND MODE */
    to(EC_CAT_FX);
    press(BTN_OF_MB[ec_mode_button(EC_CAT_FX)], 500);
    run(40);
    turn(FM1P_E_SELECT, 1);
    run(40);
    CHECK(A->settings && knob_lbl(0, "INSERT 2"), "FX's settings, SEL once: INSERT 2 (K1 %s)", knob_L(0));
    turn(FM1P_E_SELECT, 4);
    run(40);
    CHECK(A->settings && knob_lbl(0, "SEND MODE"), "FX's settings, SEL 5: SEND MODE (K1 %s)", knob_L(0));
    press(FM1P_B_OCTDN, 40);
}

/* ------------------------------------------------------------------------------------------- the recorder's setup --- */
static void test_recorder(void)
{
    int src, len, thr, sync, k;
    to(EC_MODE_SAMPLER);
    press(FM1P_B_SAVE, 500);
    run(40);
    CHECK(A->rec.open && A->rec.row == EC_REC_ROW_SRC && knob_lbl(0, "SOURCE") && k234_blank() &&
              lbl(FM1CORE_LBL_KNOB0 + FM1P_E_SELECT, "ROW"),
          "FILE held: the recorder's setup, SOURCE on K1 (K3 '%s')", knob_L(2));
    src = A->rec.src, len = A->rec.len, thr = A->rec.thr, sync = A->rec.sync;
    for (k = 1; k < 4; k++)
        turn(FM1P_E_K1 + k, 1);
    run(20);
    CHECK(A->rec.src == src && A->rec.len == len && A->rec.thr == thr && A->rec.sync == sync,
          "K2..K4: nothing changes");
    turn(FM1P_E_SELECT, 1);
    run(40);
    CHECK(A->rec.row == EC_REC_ROW_LEN && knob_lbl(0, "LENGTH"), "SEL: LENGTH (K1 %s)", knob_L(0));
    turn(FM1P_E_K1, 1);
    run(20);
    CHECK(A->rec.len == len + 1 && A->rec.src == src && A->rec.sync == sync, "K1: LENGTH only (%d)", A->rec.len);
    turn(FM1P_E_SELECT, 3);
    run(40);
    {
        const int slot = A->rec.slot;
        CHECK(A->rec.row == EC_REC_ROW_SLOT && knob_lbl(0, "SLOT"), "SEL on: SLOT (K1 %s)", knob_L(0));
        turn(FM1P_E_K1, 1);
        run(20);
        CHECK(A->rec.slot == slot + 1 && A->rec.slot_set, "K1: the target slot (%d -> %d)", slot + 1, A->rec.slot + 1);
    }
    CHECK(count(EC_C_L3, 4, 78, 100, 120) > 0, "the selected row is on screen (scrolled)");
    press(FM1P_B_OCTDN, 40);
    CHECK(!A->rec.open, "F1: closed");
}

/* ------------------------------------------------------------------------------------------- SHIFT held --- */
static void test_shift_held(void)
{
    to(EC_CAT_FILT);
    shift_down(1);
    CHECK(A->fn && ec_app_shift_map(A), "GLO held: SHIFT's screen at once");
    {
        const float bpm = A->proj.bpm;
        turn(FM1P_E_SELECT, 3);
        run(20);
        CHECK(A->proj.bpm > bpm, "held + SEL: BPM (%g -> %g)", bpm, A->proj.bpm);
    }
    CHECK(ec_app_shift_map(A), "used: still SHIFT's screen while held");
    W(2, 40);
    CHECK(A->track == 1 && ec_app_shift_map(A), "held + W2: T2 (the screen stays)");
    shift_down(0);
    run(20);
    CHECK(!A->fn && !A->pn.shift_sticky && !ec_app_shift_map(A) && mode() == EC_CAT_FILT,
          "let go after use: not sticky, FILTER back");
    shift_down(1);
    B(1, 40);
    run(440);
    B(1, 40);
    run(20);
    CHECK(fabsf(A->proj.bpm - 120.0f) < 12.0f, "held + B1 twice 0.5 s apart: TAP ~120 (%g)", A->proj.bpm);
    shift_down(0);
    run(20);
    CHECK(!A->pn.shift_sticky && !ec_app_shift_map(A), "let go after a shortcut: not sticky");
    shift_down(1);
    run(500);
    shift_down(0);
    run(20);
    CHECK(!A->pn.shift_sticky && !A->fn, "held long, unused: not sticky");
    press(FM1P_B_GLO, 40);
    CHECK(A->pn.shift_sticky && A->fn && ec_app_shift_map(A), "a quick tap: sticky, its screen");
    W(1, 40);
    CHECK(A->track == 0 && A->pn.shift_sticky, "sticky + W1: T1, still sticky");
    press(FM1P_B_GLO, 40);
    CHECK(!A->pn.shift_sticky && !A->fn && !ec_app_shift_map(A), "tapped again: off");
    {   /* the header says SHIFT while it is held (from EC_HDR_MS on) */
        char t[40];
        shift_down(1);
        run(300);
        ec_hdr_title("FILTER", t, sizeof t);
        CHECK(strstr(t, "SHIFT") != NULL, "held: the header says SHIFT (%s)", t);
        shift_down(0);
        run(20);
    }
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("usage: ec_test_v61list <folder>\n");
        return 2;
    }
    ec_fs_native_root(argv[1], "elasticat");
    boot(NULL, 0);
    test_appset();
    test_recset();
    test_mode_settings();
    test_recorder();
    test_shift_held();
    C->shutdown();
    printf("test_v61list: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
