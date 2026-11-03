/* SPDX-License-Identifier: GPL-3.0-only */
/* test_cells: 0.6.1's param cells (docs/UX-V2.md §10) through the panel (host/ec_harness.h), the demo folder and the
 * shipped layout, read back from the screen's 120x120 canvas:
 *   the step-lock cell's three states: hold a step + turn K1 = the whole K1 box in LOCK with the lock's value (the
 *   value's pixels are those of the track showing that value); let go = the track's value, no fill, a 2-unit LOCK
 *   outline round the whole box; no locks = no outline; two steps held, the lock on the second = filled too;
 *   a held source key (B6 LFO 1): hold + turn K1 = a route; while held its depth bar is SRC, a tiny depth (1 %) is
 *   one 2x2 lattice cell, never 1 unit; another source key held (B7 LFO 2) or none: no SRC in the cells; a held step
 *   with a depth lock under the held source: the cell filled.
 *   ec_test_cells <folder> */
#include "ec_harness.h"
#include "ec_gfx.h"
#include "ec_mtx.h"
#include "ec_ui_kit.h"

static const uint8_t *cv(void) { return ec_gfx_canvas(); }
static int px(int x, int y) { return cv()[y * EC_GW + x]; }
static void redraw(void)
{
    A->redraw = 1;
    run(40);
}

/* K1's box (ec_kit_cell_box) */
static int bx, by, bw, bh;
static int box_filled(void)          /* every interior pixel that isn't text / bar is LOCK: sample the corners */
{
    return px(bx + 1, by + 1) == EC_C_LOCK && px(bx + bw - 2, by + bh - 2) == EC_C_LOCK && px(bx + 2, by + 8) == EC_C_LOCK &&
           px(bx + bw - 3, by + 12) == EC_C_LOCK && px(bx + 30, by + 2 + EC_KIT_BAR_DY - 1) == EC_C_LOCK;
}
static int box_outlined(void)        /* 2 units of LOCK on all four sides, the inside not LOCK */
{
    int k, ok = 1;
    for (k = 0; k < 2; k++) {
        ok &= px(bx + 20, by + k) == EC_C_LOCK && px(bx + 20, by + bh - 1 - k) == EC_C_LOCK;
        ok &= px(bx + k, by + 10) == EC_C_LOCK && px(bx + bw - 1 - k, by + 10) == EC_C_LOCK;
    }
    return ok && px(bx + 2, by + 8) != EC_C_LOCK && px(bx + 30, by + 2 + EC_KIT_BAR_DY - 1) != EC_C_LOCK;
}
static int box_plain(void)
{
    return px(bx + 1, by + 10) != EC_C_LOCK && px(bx + 20, by + bh - 1) != EC_C_LOCK && px(bx + 2, by + 8) != EC_C_LOCK;
}
/* the value's rows of K1 as a mask (text pixels: the value's colour on either ground); a filled (held-lock) cell
 * draws its label and value a unit higher (no outline: 2 units of LOCK above the bar) */
static void value_mask(uint8_t *m, int text)
{
    const int dy = text == EC_C_BG ? -1 : 0;
    int x, y;
    for (y = 0; y < 10; y++)
        for (x = 0; x < 54; x++)
            m[y * 54 + x] = px(bx + 3 + x, by + 8 + dy + y) == text;
}
/* SRC pixels in the four cells' bar rows; *w: the widest SRC run */
static int src_in_bars(int *w)
{
    int i, n = 0, run = 0, x, y;
    *w = 0;
    for (i = 0; i < 4; i++) {
        int cx, cy, cw, ch;
        ec_kit_cell_box(i, &cx, &cy, &cw, &ch);
        for (y = cy + 2 + EC_KIT_BAR_DY; y < cy + 4 + EC_KIT_BAR_DY; y++)
            for (x = cx, run = 0; x < cx + cw; x++) {
                const int s = px(x, y) == EC_C_SRC;
                n += s;
                run = s ? run + 1 : 0;
                *w = run > *w ? run : *w;
            }
    }
    return n;
}

static void test_locks(void)
{
    ec_ptrack_t *tr;
    float lockv, trackv;
    uint8_t held_m[540], track_m[540];
    to(EC_CAT_FILT);
    clear_steps(A->track);
    tr = trk(A->track);
    ec_kit_cell_box(0, &bx, &by, &bw, &bh);
    redraw();
    CHECK(box_plain(), "no locks: K1 (CUTOFF) plain, no outline");
    trackv = tr->v[EC_P_FILTER_CUTOFF];
    key(0, 0, 1);                                  /* hold W1 (step 1) + turn K1: a CUTOFF lock */
    run(30);
    turn(FM1P_E_K1, -12);
    redraw();
    CHECK(ec_step_lock_get(&tr->steps[0], EC_P_FILTER_CUTOFF, &lockv) && lockv != trackv,
          "W1 held + K1: a CUTOFF lock on step 1 (%g, track %g)", lockv, trackv);
    CHECK(tr->v[EC_P_FILTER_CUTOFF] == trackv, "the track's CUTOFF unchanged");
    CHECK(box_filled(), "step held, param locked on it: the whole K1 box in LOCK");
    value_mask(held_m, EC_C_BG);
    key(0, 0, 0);                                  /* let go */
    run(30);
    redraw();
    CHECK(!box_filled() && box_outlined(), "no step held, a step locks CUTOFF: a 2-unit outline round the whole box");
    value_mask(track_m, EC_C_L15);
    CHECK(memcmp(held_m, track_m, sizeof held_m) != 0, "let go: the track's value, not the lock's");
    tr->v[EC_P_FILTER_CUTOFF] = lockv;             /* the track showing the lock's value draws what the held cell drew */
    redraw();
    value_mask(track_m, EC_C_L15);
    CHECK(!memcmp(held_m, track_m, sizeof held_m), "held: the cell's value was the lock's");
    tr->v[EC_P_FILTER_CUTOFF] = trackv;
    key(0, 3, 1);                                  /* hold W4 (no lock): no fill, no outline while a step is held */
    run(30);
    redraw();
    CHECK(box_plain(), "a held step without the lock: the track's value, plain");
    key(0, 0, 1);                                  /* W4 and W1 held: the lock on one of them fills */
    run(30);
    redraw();
    CHECK(box_filled(), "two steps held, one locks CUTOFF: filled");
    key(0, 0, 0);
    key(0, 3, 0);
    run(30);
    clear_steps(A->track);
    redraw();
    CHECK(box_plain(), "no locks again: no outline");
}

static void test_source(void)
{
    ec_ptrack_t *tr = trk(A->track);
    int n, w, k;
    to(EC_CAT_FILT);
    clear_steps(A->track);
    for (k = 0; k < EC_MTX_SLOTS; k++) {
        tr->v[ec_mtx_src_id(k)] = 1;
        tr->v[ec_mtx_dst_id(k)] = 0;
        tr->v[ec_mtx_depth_id(k)] = 0;
    }
    key(1, 5, 1);                                  /* hold B6 (LFO 1) + turn K1: LFO 1 > CUTOFF */
    run(30);
    turn(FM1P_E_K1, 1);
    turn(FM1P_E_K1, 20);
    redraw();
    k = ec_mtx_find(tr->v, EC_MS_LFO1, EC_P_FILTER_CUTOFF);
    CHECK(k >= 0 && tr->v[ec_mtx_depth_id(k)] != 0, "B6 held + K1: LFO 1 > CUTOFF (slot %d)", k);
    n = src_in_bars(&w);
    CHECK(n > 0 && w >= 2, "B6 held: its depth bar in SRC (%d px, run %d)", n, w);
    if (k >= 0) {
        tr->v[ec_mtx_depth_id(k)] = 1;             /* a tiny depth: one lattice cell, 2x2 */
        redraw();
        n = src_in_bars(&w);
        CHECK(n == 4 && w == 2, "a 1 %% depth: one 2x2 cell of SRC (%d px, run %d)", n, w);
        tr->v[ec_mtx_depth_id(k)] = -1;
        redraw();
        n = src_in_bars(&w);
        CHECK(n == 4 && w == 2, "a -1 %% depth: one 2x2 cell of SRC (%d px, run %d)", n, w);
        tr->v[ec_mtx_depth_id(k)] = 40;
        key(0, 1, 1);                              /* + hold W2 + turn K1: a depth lock, the cell filled */
        run(30);
        turn(FM1P_E_K1, -5);
        redraw();
        ec_kit_cell_box(0, &bx, &by, &bw, &bh);
        {
            float d;
            CHECK(ec_step_lock_get(&tr->steps[1], ec_mtx_depth_id(k), &d) && d != 40,
                  "B6 + W2 held + K1: a depth lock on step 2");
        }
        CHECK(box_filled(), "the held source's cell with a depth lock on the held step: filled");
        key(0, 1, 0);
        run(30);
        redraw();
        CHECK(box_outlined(), "W2 let go, B6 still held: the depth's outline");
    }
    key(1, 5, 0);
    run(30);
    clear_steps(A->track);
    redraw();
    n = src_in_bars(&w);
    CHECK(n == 0, "B6 let go: no SRC in the cells (%d)", n);
    key(1, 6, 1);                                  /* B7 (LFO 2) held: LFO 1's route isn't its bar */
    run(30);
    redraw();
    n = src_in_bars(&w);
    CHECK(n == 0, "B7 held: no SRC (LFO 1's route stays as it is) (%d)", n);
    key(1, 6, 0);
    run(30);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("usage: ec_test_cells <folder>\n");
        return 2;
    }
    ec_fs_native_root(argv[1], "elasticat");
    boot(NULL, 0);
    test_locks();
    test_source();
    C->shutdown();
    printf("test_cells: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
