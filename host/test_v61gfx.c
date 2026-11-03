/* SPDX-License-Identifier: GPL-3.0-only */
/* test_v61gfx: 0.6.1's MIX pan and CAT stage (docs/UX-V2.md §10 "MIX shows each track's pan", "CAT stage") through
 * the core descriptor, the demo folder (host/make_demo_folder.py) and the shipped layout, one gesture per check. The
 * screen is read from the canvas (ec_gfx_canvas: the 120x120 palette slots).
 *   ec_test_v61gfx <folder> */
#include "ec_harness.h"
#include "ec_gfx.h"
#include "ec_screens.h"
#include "ec_ui_kit.h"

static uint8_t px(int x, int y) { return ec_gfx_canvas()[y * EC_GW + x]; }
static void redraw(void)
{
    A->redraw = 1;
    run(20);
}

/* MIX strip t's pan marker: the left x of the 2-wide run of col in rows 55..60 of the strip (x+1..x+10), -1 none */
static int marker_x(int t, int col)
{
    const int x = 2 + t * 13;
    int i;
    for (i = x + 1; i <= x + 10; i++)
        if (px(i, 55) == col && px(i, 60) == col)
            return i;
    return -1;
}

static void test_mix_pan(void)
{
    const int x0 = 2;
    int m0, m1;
    to(EC_CAT_MASTER);
    W(1, 40);                                                 /* T1 */
    trk(0)->v[EC_P_PAN] = 64;
    redraw();
    CHECK(A->track == 0 && A->pn.cat == 0, "MIX on T1");
    m0 = marker_x(0, EC_C_ACC);
    CHECK(m0 == ec_scr_mix_pan_x(x0, 0) && m0 == x0 + 5, "T1 centred: its marker (ACC, the selected track) at %d", m0);
    turn(FM1P_E_K2, 40);                                      /* PAN (K2) right */
    redraw();
    m1 = marker_x(0, EC_C_ACC);
    CHECK(trk(0)->v[EC_P_PAN] > 64 && m1 > m0 && m1 == ec_scr_mix_pan_x(x0, ec_xf_bipolar(trk(0)->v[EC_P_PAN])),
          "PAN turned right (%.0f): the marker moves right (%d -> %d)", trk(0)->v[EC_P_PAN], m0, m1);
    turn(FM1P_E_K2, -200);                                    /* all the way left */
    redraw();
    CHECK(trk(0)->v[EC_P_PAN] == 0 && marker_x(0, EC_C_ACC) == x0 + 1, "PAN hard left: the marker at the left end (%d)",
          marker_x(0, EC_C_ACC));
    trk(1)->v[EC_P_PAN] = 128;                                /* another track: its marker in L12, at the right end */
    redraw();
    CHECK(marker_x(1, EC_C_L12) == 2 + 13 + 9, "T2 hard right, not selected: an L12 marker at the right end (%d)",
          marker_x(1, EC_C_L12));
    trk(1)->v[EC_P_PAN] = 64;
    trk(0)->v[EC_P_PAN] = 64;
    redraw();
}

/* a step's PAN lock shows while the step plays (the live value), the track's own PAN unchanged */
static void test_mix_pan_lock(void)
{
    int m;
    to(EC_CAT_MASTER);
    W(1, 40);
    clear_steps(0);
    step(0)->trig = 1;
    ec_step_lock_set(step(0), EC_P_PAN, 128);
    ec_seq_touch(&A->seq, 0);
    press(FM1P_B_PLAY, 20);                                   /* step 1 plays (125 ms at 120 BPM) */
    redraw();
    m = marker_x(0, EC_C_ACC);
    CHECK(A->seq.playing && trk(0)->v[EC_P_PAN] == 64 && ec_scr_mix_pan_now(A, 0) > 0.95f && m == 2 + 9,
          "playing a step with PAN locked to 128: the marker at the right end (%d, pan now %.2f)", m,
          ec_scr_mix_pan_now(A, 0));
    press(FM1P_B_PLAY, 20);
    run(300);
    clear_steps(0);
    redraw();
    CHECK(!A->seq.playing && marker_x(0, EC_C_ACC) == 2 + 5, "stopped, no lock: the marker back at the centre (%d)",
          marker_x(0, EC_C_ACC));
}

/* the CAT: full screen (no header), beams by the transport, B11 / a mode button leave, the track keys work */
static int count(int col)
{
    int i, n = 0;
    for (i = 0; i < EC_GW * EC_GH; i++)
        n += ec_gfx_canvas()[i] == col;
    return n;
}
static int top_is_stage(void)
{
    int x, y;
    for (y = 0; y < 10; y++)
        for (x = 0; x < EC_GW; x++) {
            const int c = px(x, y);
            if (c != EC_C_BG && c != EC_C_BLACK && c != EC_C_ACC && c != EC_C_SPECIAL && c != EC_C_ACTION &&
                c != EC_C_SRC && c != EC_C_ACC_DIM)
                return 0;
        }
    return 1;
}

static void test_cat(void)
{
    int beam, c0, c1;
    double b0;
    to(EC_CAT_MASTER);
    W(1, 40);
    clear_steps(0);
    B(11, 40);
    redraw();
    CHECK(A->pn.cat, "B11: the CAT");
    CHECK(top_is_stage(), "the CAT has no header: the top rows are the stage");
    CHECK(count(EC_C_ACC_DIM) > 200 && !count(EC_C_SPECIAL) && !count(EC_C_SRC) && !count(EC_C_ACTION),
          "stopped: the beams rest, dimmed (ACC_DIM %d)", count(EC_C_ACC_DIM));
    press(FM1P_B_PLAY, 20);
    run(100);
    redraw();
    b0 = ec_seq_beats(&A->seq);
    c0 = ec_scr_cat_beam_colour(b0, 1, 0);
    CHECK(A->seq.playing && top_is_stage() && !count(EC_C_ACC_DIM) && count(c0) > 50,
          "playing: lit beams, beam 1 in its beat's colour (%d px)", count(c0));
    beam = (int)floor(b0);
    while ((int)floor(ec_seq_beats(&A->seq)) == beam)          /* the next beat: the colours turn */
        run(10);
    run(30);
    redraw();
    c1 = ec_scr_cat_beam_colour(ec_seq_beats(&A->seq), 1, 0);
    CHECK(c1 != c0 && count(c1) > 50, "the next beat: beam 1's colour changes (%d -> %d)", c0, c1);
    W(3, 40);                                                 /* a track key still selects the track */
    redraw();
    CHECK(A->pn.cat && A->track == 2 && top_is_stage(), "W3 on the CAT: T3, still the CAT, no header");
    W(1, 40);
    press(FM1P_B_PLAY, 20);
    run(300);
    B(11, 40);
    CHECK(!A->pn.cat && mode() == EC_CAT_MASTER, "B11 again: MIX");
    B(11, 40);
    CHECK(A->pn.cat, "B11: the CAT again");
    press(FM1P_B_SEL, 40);
    redraw();
    CHECK(!A->pn.cat && mode() == EC_CAT_FILT, "a mode button (SEL) leaves the CAT");
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("usage: ec_test_v61gfx <folder>\n");
        return 2;
    }
    ec_fs_native_root(argv[1], "elasticat");
    boot(NULL, 0);
    test_mix_pan();
    test_mix_pan_lock();
    test_cat();
    C->shutdown();
    printf("test_v61gfx: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
