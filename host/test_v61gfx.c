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

/* the CAT: full screen (no header), 0.6.2: a greyscale fractal by the transport, B11 / a mode button leave, the track
 * keys work */
static int count(int col)
{
    int i, n = 0;
    for (i = 0; i < EC_GW * EC_GH; i++)
        n += ec_gfx_canvas()[i] == col;
    return n;
}
static int is_grey(int c)
{
    return c == EC_C_BG || c == EC_C_BLACK || c == EC_C_L3 || c == EC_C_L5 || c == EC_C_L8 || c == EC_C_L12 ||
           c == EC_C_L15;
}
static int all_grey(void)
{
    int i;
    for (i = 0; i < EC_GW * EC_GH; i++)
        if (!is_grey(ec_gfx_canvas()[i]))
            return 0;
    return 1;
}
static int top_is_stage(void)   /* no header: the top rows are the stage (the fractal's greys) */
{
    int x, y;
    for (y = 0; y < 10; y++)
        for (x = 0; x < EC_GW; x++)
            if (!is_grey(px(x, y)))
                return 0;
    return 1;
}
static int greys(void) { return count(EC_C_L3) + count(EC_C_L5) + count(EC_C_L8) + count(EC_C_L12) + count(EC_C_L15); }
static int changed(const uint8_t *was)   /* canvas pixels that differ from was */
{
    int i, n = 0;
    for (i = 0; i < EC_GW * EC_GH; i++)
        n += ec_gfx_canvas()[i] != was[i];
    return n;
}

static void test_cat(void)
{
    static uint8_t was[EC_GW * EC_GH];
    int beat, n;
    to(EC_CAT_MASTER);
    W(1, 40);
    clear_steps(0);
    B(11, 40);
    redraw();
    CHECK(A->pn.cat, "B11: the CAT");
    CHECK(top_is_stage() && all_grey(), "the CAT has no header, greys only");
    CHECK(greys() > 800, "stopped: the fractal shows (%d grey px, %s)", greys(), ec_scr_cat_algo_name(A->pn.cat_algo1 - 1));
    memcpy(was, ec_gfx_canvas(), sizeof was);
    run(2000);
    redraw();
    n = changed(was);
    CHECK(n > 50 && all_grey(), "stopped: it drifts (%d px in 2 s)", n);
    press(FM1P_B_PLAY, 20);
    run(100);
    redraw();
    CHECK(A->seq.playing && top_is_stage() && all_grey() && greys() > 800, "playing: greys only, the fractal (%d px)",
          greys());
    beat = (int)floor(ec_seq_beats(&A->seq));
    while ((int)floor(ec_seq_beats(&A->seq)) == beat)          /* the next beat: the pulse */
        run(10);
    redraw();
    memcpy(was, ec_gfx_canvas(), sizeof was);
    run(300);
    redraw();
    n = changed(was);
    CHECK(n > 200, "the beat's pulse eases out (%d px in 300 ms)", n);
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

/* 0.6.3 (§10 "CAT fractals"): every algorithm draws the greys only, with detail, and keeps evolving without repeating;
 * entries pick a new algorithm each time, never the last one; playing evolves faster than stopped */
static uint32_t frame_hash(void)
{
    uint32_t h = 2166136261u;
    int i;
    for (i = 0; i < EC_GW * EC_GH; i++)
        h = (h ^ ec_gfx_canvas()[i]) * 16777619u;
    return h;
}
static int distinct_greys(void)
{
    return (count(EC_C_L3) > 0) + (count(EC_C_L5) > 0) + (count(EC_C_L8) > 0) + (count(EC_C_L12) > 0) +
           (count(EC_C_L15) > 0) + (count(EC_C_BG) > 0);
}
static void cat_draw(double t, int algo, uint32_t seed)
{
    ec_gfx_begin(ec_plat_lcd());
    ec_scr_cat((int)(t * 10.0), 0.0, 0, t, algo, seed);
}

static void test_cat_algos(void)
{
    static uint8_t was[EC_GW * EC_GH];
    static uint32_t seen[4000];
    const int na = ec_scr_cat_algos();
    int a, i, j;
    CHECK(na >= 6, "%d fractal algorithms", na);
    for (a = 0; a < na; a++) {
        const uint32_t seed = 0x5EED0000u + (uint32_t)a * 77u;
        int detail = 0, shots = 0, grey_ok = 1, dupes = 0, minchg = 1 << 30, worst_top = 0;
        double t;
        /* 30 minutes of stopped evolution, a frame every 0.5 (t units: ~0.6 s), every frame's hash */
        for (i = 0, t = 0.0; i < 3600; i++, t += 0.5) {
            cat_draw(t, a, seed);
            seen[i] = frame_hash();
            if (i % 120 == 0) {                        /* every minute: greys only, detail */
                const ec_cat_stats_t *st = ec_scr_cat_stats();
                shots++;
                grey_ok &= all_grey();
                detail += distinct_greys() >= 4 && st->top < 0.9 * st->cells;
                worst_top = st->top * 100 / st->cells > worst_top ? st->top * 100 / st->cells : worst_top;
            }
        }
        for (i = 0; i < 3600; i++)                     /* no frame twice (a hash set would do; 3600^2 / 2 is fine) */
            for (j = i + 1; j < 3600; j++)
                if (seen[i] == seen[j]) {
                    dupes++;
                    if (dupes < 4)
                        printf("  %s: frames %d and %d (t %.1f, %.1f) match\n", ec_scr_cat_algo_name(a), i, j, i * 0.5,
                               j * 0.5);
                }
        for (i = 0; i < 6; i++) {                      /* frames two minutes apart differ */
            int n;
            cat_draw(200.0 * i, a, seed);
            memcpy(was, ec_gfx_canvas(), sizeof was);
            cat_draw(200.0 * i + 120.0, a, seed);
            n = changed(was);
            minchg = n < minchg ? n : minchg;
        }
        printf("cat %-7s: detail %d/%d minutes (worst top %d %%), 2-minute change >= %d px, %d repeats\n",
               ec_scr_cat_algo_name(a), detail, shots, worst_top, minchg, dupes);
        CHECK(grey_ok, "%s: the theme's greys only", ec_scr_cat_algo_name(a));
        CHECK(detail >= shots - 2, "%s: detail (4+ greys, no class over 90 %%) in %d of %d minutes (worst top %d %%)",
              ec_scr_cat_algo_name(a), detail, shots, worst_top);
        CHECK(dupes == 0, "%s: 3600 frames over 30 minutes, no frame twice (%d repeats)", ec_scr_cat_algo_name(a), dupes);
        CHECK(minchg > 300, "%s: frames two minutes apart differ (at least %d px)", ec_scr_cat_algo_name(a), minchg);
    }
}

static void test_cat_entries(void)
{
    int k, prev = -1, same = 0, hit[16] = {0}, kinds = 0;
    double t0;
    to(EC_CAT_MASTER);
    W(1, 40);
    for (k = 0; k < 24; k++) {
        B(11, 40);                                     /* on */
        CHECK(A->pn.cat && A->pn.cat_algo1 >= 1 && A->pn.cat_algo1 <= ec_scr_cat_algos(), "B11: the CAT, an algorithm");
        same += A->pn.cat_algo1 - 1 == prev;
        prev = A->pn.cat_algo1 - 1;
        hit[prev & 15]++;
        run(37 + 13 * k);
        B(11, 40);                                     /* off */
        run(25);
    }
    for (k = 0; k < 16; k++)
        kinds += hit[k] > 0;
    CHECK(same == 0, "24 entries: never the same algorithm twice running (%d)", same);
    CHECK(kinds >= 5, "24 entries: %d different algorithms", kinds);
    B(11, 40);                                         /* stopped vs playing: the evolution clock */
    t0 = A->pn.cat_t;
    run(2000);
    t0 = A->pn.cat_t - t0;
    CHECK(t0 > 1.0 && t0 < 2.2, "stopped: the fractal evolves ~1 a second (%.2f in 2 s)", t0);
    press(FM1P_B_PLAY, 20);
    run(100);
    {
        const double t1 = A->pn.cat_t;
        run(2000);
        printf("cat entries: %d kinds; evolution in 2 s: stopped %.2f, playing %.2f\n", kinds, t0, A->pn.cat_t - t1);
        CHECK(A->pn.cat_t - t1 > 2.0 * t0, "playing (120 BPM): faster (%.2f in 2 s)", A->pn.cat_t - t1);
    }
    press(FM1P_B_PLAY, 20);
    run(300);
    B(11, 40);
    CHECK(!A->pn.cat, "B11: MIX");
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
    test_cat_algos();
    test_cat_entries();
    C->shutdown();
    printf("test_v61gfx: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
