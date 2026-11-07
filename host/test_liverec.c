/* SPDX-License-Identifier: GPL-3.0-only */
/* test_liverec: 0.6.0's live recording (docs/UX-V2.md §4) through the panel with audio rendered as a host does
 * (host/ec_harness.h), the demo folder and the shipped layout:
 *   the record settings (hold REC: F1 back, F2 OK); CAPTURE (stopped: the first key starts playback); KEYBOARD's notes
 *   with pitch, velocity and a micro-timing lock (nearest step + offset), the early hit's step not doubled, both played
 *   on time on the next pass; QUANTIZE HARD; LOOP ENGINE's keys (trig + the loop position; a slice pad); AUTOMATION:
 *   knob moves as lock-only steps, a macro, never the crossfader; OVERDUB keeps what is there; FIXED stops recording
 *   after its length while playback goes on; PLAY stopping ends it; the count-in and the metronome on the output after
 *   the master bus (master at 0, every track muted: only the click), sample-accurate, the accent; a SAMPLER take of
 *   the master holds none of it; the metronome OFF is silent.
 *   ec_test_liverec <folder> */
#include "ec_harness.h"

#define STEP 5512.5                               /* frames per step at 120 BPM, 1X */

static ec_step_t *tstep(int t, int i) { return &trk(t)->steps[i]; }
static int lock(const ec_step_t *s, int id, float *x)
{
    float d;
    return ec_step_lock_get(s, id, x ? x : &d);
}
static int count_locks(int t)
{
    int i, n = 0;
    for (i = 0; i < EC_MAXSTEPS; i++)
        n += tstep(t, i)->nlock;
    return n;
}
/* run until the sequencer's sample time reaches the transport's start + `steps` steps */
static void run_to_step(double steps)
{
    const uint64_t at = A->seq.sample_base + (uint64_t)(steps * STEP);
    int i;
    for (i = 0; i < 20000 && frames < at; i++)
        run(1);
}
/* what a hit now would be: the nearest step, its micro units */
static void expect_hit(int *stp, int *mu)
{
    const double st = (double)(A->seq.now - A->seq.sample_base) / STEP;
    const double n = floor(st + 0.5);
    *stp = (int)n % 16;
    *mu = ec_micro_units((float)(st - n));
}
static void stop(void)
{
    if (A->seq.playing)
        press(FM1P_B_PLAY, 40);
}

/* ------------------------------------------------------------------------------------------- settings --- */
static void test_settings(void)
{
    ec_recset_t r0;
    to(EC_CAT_FILT);
    r0 = A->proj.rec;
    CHECK(r0.mode == EC_REC_CAPTURE && r0.bars == 1 && r0.countin == 0 && r0.metro == EC_METRO_REC && r0.automation &&
              !r0.hard,
          "the defaults: CAPTURE, 1 bar, no count-in, the metronome while recording, AUTOMATION, KEEP");
    press(FM1P_B_REC, 500);
    CHECK(A->pn.rs_open && lbl(FM1CORE_LBL_KNOB0 + FM1P_E_K1, "MODE") && lbl(FM1CORE_LBL_KNOB0 + FM1P_E_K3, ""),
          "REC held: the record settings (K1 %s, K3 %s)", L(FM1CORE_LBL_KNOB0 + FM1P_E_K1),
          L(FM1CORE_LBL_KNOB0 + FM1P_E_K3));
    turn(FM1P_E_K1, 1);                            /* 0.6.1: a list menu: K1 the selected row, SEL the next */
    turn(FM1P_E_SELECT, 1);
    turn(FM1P_E_K1, 2);
    turn(FM1P_E_SELECT, 1);
    turn(FM1P_E_K1, 1);
    turn(FM1P_E_SELECT, 1);
    turn(FM1P_E_K1, 1);
    CHECK(A->proj.rec.mode == EC_REC_FIXED && A->proj.rec.bars == 4 && A->proj.rec.countin == 1 &&
              A->proj.rec.metro == EC_METRO_ALWAYS,
          "FIXED, 4 bars, 1 bar, ALWAYS (%d %d %d %d)", A->proj.rec.mode, A->proj.rec.bars,
          A->proj.rec.countin, A->proj.rec.metro);
    press(FM1P_B_OCTDN, 40);                       /* F1: back, nothing kept */
    CHECK(!A->pn.rs_open && !memcmp(&A->proj.rec, &r0, sizeof r0), "F1: back (unchanged)");
    btn(FM1P_B_GLO, 1);                            /* SHIFT + REC opens them too */
    run(20);
    press(FM1P_B_REC, 40);
    btn(FM1P_B_GLO, 0);
    run(20);
    if (A->pn.shift_sticky)
        press(FM1P_B_GLO, 40);
    CHECK(A->pn.rs_open, "SHIFT + REC: the record settings");
    turn(FM1P_E_SELECT, 4);                        /* row 5: CLICK LVL */
    run(40);
    CHECK(lbl(FM1CORE_LBL_KNOB0 + FM1P_E_K1, "CLICK LVL") && lbl(FM1CORE_LBL_KNOB0 + FM1P_E_K3, ""),
          "SEL: row 5 (K1 %s)", L(FM1CORE_LBL_KNOB0 + FM1P_E_K1));
    turn(FM1P_E_K1, 1);
    {
        char b[16];
        ec_rs_row_value(4, b, sizeof b);
        CHECK(!strcmp(b, "-3DB") && fabsf(A->proj.rec.metro_level - 0.708f) < 0.01f, "CLICK LVL -6 -> -3 dB (%s)", b);
    }
    turn(FM1P_E_K1, -1);
    turn(FM1P_E_K3, 1);                            /* K3: nothing in a list */
    CHECK(!A->proj.rec.hard, "K3 does nothing in a list");
    turn(FM1P_E_SELECT, 2);
    turn(FM1P_E_K1, 1);
    CHECK(A->proj.rec.hard, "SEL to QUANTIZE, K1: HARD");
    press(FM1P_B_OCTUP, 40);                       /* F2: OK */
    CHECK(!A->pn.rs_open && A->dirty && A->proj.rec.hard, "F2: kept, the project dirty");
    A->proj.rec.hard = 0;
    CHECK(lbl(FM1CORE_LBL_BTN0 + FM1P_B_REC, "REC"), "REC's label");
    {
        char b[16], c[16];
        ec_param_format(EC_P_MICRO, 5, 0, b, sizeof b);
        ec_param_format(EC_P_MICRO, 0, 0, c, sizeof c);
        CHECK(!strcmp(b, "+5/24") && !strcmp(c, "0") && (EC_PARAMS[EC_P_MICRO].flags & EC_PF_LOCKABLE),
              "MICRO's format: %s, %s", b, c);
    }
}

/* ------------------------------------------------------------------------------------------- CAPTURE, notes --- */
static void test_capture_notes(void)
{
    int s4, mu4, s8, mu8, f0;
    float x;
    to(EC_MODE_KEYBOARD);
    A->track = 0;
    A->bus = 0;
    clear_steps(0);
    trk(0)->v[EC_P_MACHINE] = EC_MACH_LOOP_TRIG;
    trk(0)->v[EC_P_PATTERN_STEPS] = 16;
    ec_seq_touch(&A->seq, 0);
    CHECK(!A->seq.playing && A->pn.target == 0, "KEYBOARD, stopped, live");
    press(FM1P_B_REC, 40);
    CHECK(ec_liverec_on() && ec_liverec_armed() && !A->seq.playing, "REC (CAPTURE, stopped): waiting");
    {
        int lit = 0, dim = 0, i;
        for (i = 0; i < 40; i++) {
            run(10);
            lit += led_btn(FM1P_B_REC) == FM1P_LED_LIT;
            dim += led_btn(FM1P_B_REC) == FM1P_LED_DIM;
        }
        CHECK(lit && dim, "its LED blinks (%d / %d)", lit, dim);
    }
    key(0, 4, 1);                                  /* W5 = C4: the first note starts playback */
    run(5);
    CHECK(A->seq.playing && !ec_liverec_armed(), "the first key: playback starts");
    CHECK(tstep(0, 0)->trig && lock(tstep(0, 0), EC_P_NOTE, &x) && x == 0 && !lock(tstep(0, 0), EC_P_MICRO, 0),
          "step 1: the note, on time (no micro)");
    key(0, 4, 0);
    run(20);
    CHECK(led_btn(FM1P_B_REC) == FM1P_LED_LIT, "recording: REC lit");
    /* a late hit (step 5 + 0.2) and an early one (step 9 - 0.3) */
    run_to_step(4.2);
    expect_hit(&s4, &mu4);
    keyv(0, 5, 1, 90);                             /* W6 = D4, velocity 90 */
    run(1);
    keyv(0, 5, 0, 90);
    run(20);
    CHECK(s4 == 4 && mu4 == 5, "(the expectation: step 5, +5/24: %d %d)", s4 + 1, mu4);
    CHECK(tstep(0, 4)->trig && lock(tstep(0, 4), EC_P_NOTE, &x) && x == 2.0f,
          "a note: a trig on the nearest step (5) with its pitch (%g)", x);
    CHECK(lock(tstep(0, 4), EC_P_MICRO, &x) && (int)x == mu4, "  its offset as a micro lock (%g, want %d)", x, mu4);
    CHECK(lock(tstep(0, 4), EC_P_VELOCITY, &x) && fabsf(x - 90.0f / 127.0f) < 1e-4f, "  its velocity (%g)", x);
    run_to_step(7.7);
    expect_hit(&s8, &mu8);
    f0 = A->seq.tr[0].fired;
    key(0, 4, 1);
    run(1);
    key(0, 4, 0);
    CHECK(s8 == 8 && mu8 == -7 && tstep(0, 8)->trig && lock(tstep(0, 8), EC_P_MICRO, &x) && (int)x == mu8,
          "an early hit: step 9, %d/24 (%g)", mu8, x);
    run_to_step(9.5);
    CHECK(A->seq.tr[0].fired == f0, "  heard live: step 9 does not fire again this pass (%d -> %d)", f0,
          A->seq.tr[0].fired);
    CHECK(lock(tstep(0, 4), EC_P_LENGTH, &x) && x == 0.25f, "0.6.59: the note's held length kept (%g steps)", x);
    /* 0.6.59 (owner): the take's length grows with the playhead; REC near step 16: 16 steps, looping from step 1 */
    run_to_step(15.8);
    CHECK(trk(0)->v[EC_P_PATTERN_STEPS] >= 16, "the take grows with the playhead (LEN %g)", trk(0)->v[EC_P_PATTERN_STEPS]);
    press(FM1P_B_REC, 40);
    CHECK(trk(0)->v[EC_P_PATTERN_STEPS] == 16 && A->seq.playing && !ec_liverec_on() && !strcmp(A->msg, "CAPTURED 16"),
          "REC again: CAPTURED 16, looping (LEN %g, %s)", trk(0)->v[EC_P_PATTERN_STEPS], A->msg);
    /* the loop: both play at their offsets */
    run_to_step(4.5);
    {
        const double want = (double)A->seq.sample_base + (4 + 5.0 / 24.0) * STEP;
        CHECK(fabs((double)A->seq.tr[0].last_fire - want) <= 1.0, "the loop: step 5 fires +5/24 late (%llu, want %.1f)",
              (unsigned long long)A->seq.tr[0].last_fire, want);
    }
    run_to_step(8.2);
    {
        const double want = (double)A->seq.sample_base + (8 - 7.0 / 24.0) * STEP;
        CHECK(fabs((double)A->seq.tr[0].last_fire - want) <= 1.0, "the loop: step 9 fires 7/24 early (%llu, want %.1f)",
              (unsigned long long)A->seq.tr[0].last_fire, want);
    }
    /* 0.6.59: QUANTIZE 100 %: on the grid; F1 + the knob at 50 %: the timing kept, the knob back to 0 */
    trk(0)->v[EC_P_QUANT] = 100;
    run_to_step(16 + 4.5);
    {
        const double want = (double)A->seq.sample_base + (16 + 4) * STEP;
        CHECK(fabs((double)A->seq.tr[0].last_fire - want) <= 1.0, "QUANT 100: step 5 on its boundary (%llu, want %.1f)",
              (unsigned long long)A->seq.tr[0].last_fire, want);
    }
    trk(0)->v[EC_P_QUANT] = 50;
    ec_app_quant_apply();
    CHECK(lock(tstep(0, 4), EC_P_MICRO, &x) && x == 3 && trk(0)->v[EC_P_QUANT] == 0,
          "QUANTIZE applied at 50 %%: step 5 +3/24, the knob 0 (%g)", x);
    /* QUANTIZE HARD (OVERDUB): no offset */
    A->proj.rec.mode = EC_REC_OVERDUB;
    A->proj.rec.hard = 1;
    press(FM1P_B_REC, 40);
    run_to_step(32 + 10.3);
    key(0, 4, 1);
    run(1);
    key(0, 4, 0);
    run(20);
    CHECK(tstep(0, 10)->trig && !lock(tstep(0, 10), EC_P_MICRO, 0), "QUANTIZE HARD: step 11, no micro lock");
    A->proj.rec.hard = 0;
    A->proj.rec.mode = EC_REC_CAPTURE;
    press(FM1P_B_PLAY, 40);
    CHECK(!A->seq.playing && !ec_liverec_on(), "PLAY stops: recording ends");
    CHECK(A->e5.undo.n > 0, "the writes are undoable (%d)", A->e5.undo.n);
    /* a slice track: the slice too */
    clear_steps(0);
    trk(0)->v[EC_P_MACHINE] = EC_MACH_GRID_SLICE;
    trk(0)->v[EC_P_SLICE_INDEX] = 3;
    ec_seq_touch(&A->seq, 0);
    run(20);
    press(FM1P_B_REC, 40);
    key(0, 4, 1);
    run(5);
    key(0, 4, 0);
    run(20);
    CHECK(tstep(0, 0)->trig && tstep(0, 0)->slices == 1u << 2, "KEYBOARD on a slice track: the slice (%x)",
          tstep(0, 0)->slices);
    stop();
    trk(0)->v[EC_P_MACHINE] = EC_MACH_LOOP;
    clear_steps(0);
}

/* 0.6.61 (owner): CAPTURE while playing an older pattern: from the first key, the old notes are gone (heard too) */
static void test_capture_over_old(void)
{
    int i, f0, f1;
    to(EC_MODE_KEYBOARD);
    A->track = 0;
    A->bus = 0;
    clear_steps(0);
    trk(0)->v[EC_P_MACHINE] = EC_MACH_LOOP;
    trk(0)->v[EC_P_PATTERN_STEPS] = 16;
    for (i = 1; i < 16; i += 2) {                  /* the old take: every odd step */
        tstep(0, i)->trig = 1;
        ec_step_lock_set(tstep(0, i), EC_P_NOTE, 7);
    }
    ec_seq_touch(&A->seq, 0);
    ec_seq_timing_changed(&A->seq);
    A->proj.rec.mode = EC_REC_CAPTURE;
    press(FM1P_B_PLAY, 40);
    run_to_step(5.5);
    press(FM1P_B_REC, 40);                         /* 0.6.85 (owner): playing: an overdub at once */
    run_to_step(8.0);
    expect_hit(&f0, &f1);
    key(0, 4, 1);                                  /* a key: written where it lands, nothing cleared */
    run(5);
    key(0, 4, 0);
    run(5);
    f0 = A->seq.tr[0].fired;
    run_to_step(12.5);
    f1 = A->seq.tr[0].fired;
    CHECK(!A->pn.lr_cap && A->seq.playing && f1 > f0 && tstep(0, 1)->trig && tstep(0, 11)->trig && tstep(0, 8)->trig &&
              trk(0)->v[EC_P_PATTERN_STEPS] == 16,
          "CAPTURE while playing: an overdub (the old notes play on, %d -> %d fired; step 9 written; length 16)", f0, f1);
    press(FM1P_B_REC, 40);
    stop();
    for (i = 1; i < 16; i += 2) {                  /* again, stopped */
        tstep(0, i)->trig = 1;
        ec_step_lock_set(tstep(0, i), EC_P_NOTE, 7);
    }
    trk(0)->v[EC_P_PATTERN_STEPS] = 16;
    ec_seq_touch(&A->seq, 0);
    ec_seq_timing_changed(&A->seq);
    press(FM1P_B_REC, 40);
    run(100);
    key(0, 4, 1);
    run(5);
    key(0, 4, 0);
    run(5);
    f0 = A->seq.tr[0].fired;
    run_to_step(12.5);
    f1 = A->seq.tr[0].fired;
    CHECK(A->pn.lr_cap && f1 == f0 && tstep(0, 0)->trig && !tstep(0, 1)->trig,
          "CAPTURE stopped: cleared, the old notes silent (%d -> %d fired)", f0, f1);
    stop();
    clear_steps(0);
    trk(0)->v[EC_P_PATTERN_STEPS] = 16;
    ec_seq_timing_changed(&A->seq);
}

/* ------------------------------------------------------------------------------------------- loop keys --- */
static void test_loop_keys(void)
{
    float a, b;
    to(EC_CAT_SRC);
    A->track = 0;
    clear_steps(0);
    trk(0)->v[EC_P_MACHINE] = EC_MACH_LOOP;
    trk(0)->v[EC_P_LOOP_DIVISION] = 16;
    ec_seq_touch(&A->seq, 0);
    press(FM1P_B_REC, 40);
    key(0, 2, 1);                                  /* W3: loop key 3 starts it (CAPTURE) */
    run(5);
    key(0, 2, 0);
    run(20);
    CHECK(A->seq.playing && tstep(0, 0)->trig && lock(tstep(0, 0), EC_P_LOOP_START, &a) &&
              lock(tstep(0, 0), EC_P_LOOP_END, &b) && a == 16.0f && b == 24.0f,
          "LOOP ENGINE: loop key 3 = a trig with its loop position (%g..%g of 128)", a, b);
    stop();
    clear_steps(0);
    trk(0)->v[EC_P_MACHINE] = EC_MACH_SLICE_POLY;
    ec_seq_touch(&A->seq, 0);
    press(FM1P_B_REC, 40);
    key(0, 1, 1);                                  /* W2 = pad 2 */
    run(5);
    key(0, 1, 0);
    run(20);
    CHECK(tstep(0, 0)->trig && tstep(0, 0)->slices == 2u, "a slice track: the pad's slice (%x)", tstep(0, 0)->slices);
    stop();
    trk(0)->v[EC_P_MACHINE] = EC_MACH_LOOP;
    clear_steps(0);
}

/* ------------------------------------------------------------------------------------------- automation --- */
static void test_automation(void)
{
    int s, mu, i, k, nl, w;
    float x;
    ec_page_t pg;
    to(EC_CAT_FILT);
    ec_app_set_page(EC_CAT_FILT, 0);
    A->track = 0;
    clear_steps(0);
    trk(0)->v[EC_P_PATTERN_STEPS] = 16;            /* (0.6.59: the CAPTURE takes before set their own lengths) */
    ec_seq_timing_changed(&A->seq);
    tstep(0, 6)->trig = 1;                         /* OVERDUB keeps this one */
    ec_step_lock_set(tstep(0, 6), EC_P_NOTE, 5);
    A->proj.rec.mode = EC_REC_OVERDUB;
    press(FM1P_B_REC, 40);
    CHECK(A->seq.playing && ec_liverec_on() && !ec_liverec_armed(), "OVERDUB, stopped: REC starts playback at once");
    run_to_step(2.9);
    expect_hit(&s, &mu);
    turn(FM1P_E_K1, 3);                            /* FILTER's K1: the cutoff */
    CHECK(s == 3 && !tstep(0, 3)->trig && lock(tstep(0, 3), EC_P_FILTER_CUTOFF, &x) &&
              x == trk(0)->v[EC_P_FILTER_CUTOFF],
          "AUTOMATION: a knob move = a lock on step 4 without a trig (lock-only) (%g)", x);
    CHECK(!lock(tstep(0, 3), EC_P_MICRO, 0), "  (locks keep no offset)");
    CHECK(tstep(0, 6)->trig && lock(tstep(0, 6), EC_P_NOTE, &x) && x == 5, "OVERDUB: step 7 untouched");
    /* a macro (MODULATION's MACROS page, B11) */
    to(EC_CAT_MOD);
    B(11, 40);
    ec_app_page(A, &pg);
    for (k = 0; k < pg.n && !(pg.it[k].kind == EC_IT_PARAM && pg.it[k].id == EC_P_MACRO1); k++)
        ;
    CHECK(k < 4, "MACROS: M1 on K%d", k + 1);
    run_to_step(16 + 9.1);
    expect_hit(&s, &mu);
    turn(FM1P_E_K1 + k, 10);
    CHECK(s == 9 && lock(tstep(0, 9), EC_P_MACRO1, &x) && x == trk(0)->v[EC_P_MACRO1] && x > 0,
          "a macro value recorded as a lock on step 10 (%g)", x);
    /* the crossfader: never */
    to(EC_CAT_XFD);
    nl = count_locks(0);
    w = A->pn.lr_writes;
    {
        const float p0 = A->proj.e5.sc.pos;
        key(0, 11, 1);                             /* W12: glide to 127 */
        run(600);
        key(0, 11, 0);
        run(20);
        CHECK(A->proj.e5.sc.pos != p0 && count_locks(0) == nl && A->pn.lr_writes == w,
              "the crossfader moved (%g) but nothing was recorded", A->proj.e5.sc.pos);
    }
    for (i = 0; i < EC_NTRACK; i++)
        CHECK(i == 0 || count_locks(i) == 0, "T%d: nothing", i + 1);
    stop();
    A->proj.rec.mode = EC_REC_CAPTURE;
    clear_steps(0);
}

/* ------------------------------------------------------------------------------------------- FIXED --- */
static void test_fixed(void)
{
    int w;
    to(EC_MODE_KEYBOARD);
    clear_steps(0);
    A->proj.rec.mode = EC_REC_FIXED;
    A->proj.rec.bars = 1;
    press(FM1P_B_REC, 40);
    CHECK(A->seq.playing && ec_liverec_on(), "FIXED, stopped: REC starts playback");
    run_to_step(14.0);
    CHECK(ec_liverec_on(), "recording through its bar");
    run_to_step(16.3);
    CHECK(!ec_liverec_on() && A->seq.playing && !strcmp(A->msg, "REC DONE"),
          "after LENGTH (1 bar): recording ends, playback goes on (%s)", A->msg);
    w = A->pn.lr_writes;
    key(0, 4, 1);
    run(2);
    key(0, 4, 0);
    run(20);
    CHECK(A->pn.lr_writes == w && !tstep(0, 1)->trig, "a note after it: not recorded");
    stop();
    A->proj.rec.mode = EC_REC_CAPTURE;
    clear_steps(0);
}

/* ------------------------------------------------------------------------------------------- the click --- */
static void test_click(void)
{
    uint64_t ci0, sb, t;
    int i, pk0, pk1;
    to(EC_CAT_FILT);
    for (i = 0; i < EC_NTRACK; i++) {              /* only the click can sound: every track muted, master at 0 */
        trk(i)->v[EC_P_MUTE] = 1;
        clear_steps(i);
    }
    ec_seq_touch_all(&A->seq);
    A->master = 0;
    run(400);
    A->proj.rec.mode = EC_REC_OVERDUB;
    A->proj.rec.countin = 1;
    A->proj.rec.metro = EC_METRO_REC;
    A->proj.rec.metro_level = 0.5f;
    press(FM1P_B_REC, 40);
    ci0 = A->seq.ci_t0;
    CHECK(!A->seq.playing && A->seq.ci_n == 4 && ec_seq_counting_in(&A->seq) == 3 && ec_liverec_armed(),
          "COUNT-IN 1 bar: four beats before the start (one gone after 60 ms: %d left)", ec_seq_counting_in(&A->seq));
    run(2200);
    sb = A->seq.sample_base;
    CHECK(A->seq.playing && sb == ci0 + 4 * 22050, "the transport starts after the count-in (%llu, want %llu)",
          (unsigned long long)sb, (unsigned long long)(ci0 + 4 * 22050));
    for (i = 0; i < 4; i++) {
        const uint64_t want = ci0 + (uint64_t)i * 22050;
        t = out_onset(want > 50 ? want - 50 : 0, want + 2000, 20);
        CHECK(t == want, "count-in click %d on its frame (%llu, want %llu)", i + 1, (unsigned long long)t,
              (unsigned long long)want);
    }
    pk0 = out_peak(ci0, ci0 + 4000);
    pk1 = out_peak(ci0 + 22050, ci0 + 22050 + 4000);
    CHECK(pk0 > pk1 && pk1 > 1000, "the bar's first beat accented (%d > %d)", pk0, pk1);
    run(1200);
    for (i = 0; i < 2; i++) {
        const uint64_t want = sb + (uint64_t)i * 22050;
        t = out_onset(want - 50, want + 2000, 20);
        CHECK(t == want, "the metronome while recording: beat %d on its frame (%llu, want %llu)", i + 1,
              (unsigned long long)t, (unsigned long long)want);
    }
    CHECK(out_peak(sb, sb + 44100) > 1000, "heard with the master at 0 and every track muted: after the master bus");
    /* a SAMPLER take of the master (the recorder's setup: FILE held, K1 MASTER) holds no click */
    A->proj.rec.metro = EC_METRO_ALWAYS;
    to(EC_MODE_SAMPLER);
    CHECK(mode() == EC_MODE_SAMPLER, "playing: FILE = the SAMPLER");
    press(FM1P_B_REC, 500);                        /* 0.6.15: REC held in the SAMPLER = the recorder's setup */
    CHECK(A->rec.open, "REC held: the recorder's setup");
    turn(FM1P_E_K1, 1);
    CHECK(A->rec.src == EC_RSRC_MST, "K1: the master out");
    press(FM1P_B_OCTDN, 40);
    B(2, 40);
    B(11, 40);
    {
        const uint32_t takes = A->rec.takes;
        const ec_ss_sample_t *x;
        uint64_t st;
        uint32_t k;
        int pk = 0;
        for (i = 0; i < 600 && A->rec.takes == takes; i++)
            run(10);
        st = A->rec.start_at;
        x = ec_ss_ready(A->slot[A->rec.last_slot].handle);
        CHECK(A->rec.takes == takes + 1 && x && x->frames == 88200, "the take: one bar of the master");
        for (k = 0; x && k < x->frames; k++)
            pk = abs(x->data[2 * k]) > pk ? abs(x->data[2 * k]) : pk;
        CHECK(pk == 0 && out_peak(st, st + 88200) > 1000,
              "the take holds no click (peak %d) while the output had them (%d)", pk, out_peak(st, st + 88200));
    }
    A->rec.src = EC_RSRC_IN;
    press(FM1P_B_PLAY, 40);
    CHECK(!ec_liverec_on(), "PLAY: stopped, REC off");
    /* the metronome OFF, not recording: silence */
    A->proj.rec.metro = EC_METRO_OFF;
    to(EC_CAT_FILT);
    press(FM1P_B_PLAY, 40);
    t = frames;
    run(1100);
    CHECK(out_peak(t + 256, frames) == 0, "METRONOME OFF: no click (%d)", out_peak(t + 256, frames));
    A->proj.rec.metro = EC_METRO_ALWAYS;
    t = frames;
    run(1100);
    CHECK(out_peak(t, frames) > 1000, "METRONOME ALWAYS: clicks while not recording");
    stop();
    A->proj.rec.metro = EC_METRO_REC;
    A->proj.rec.countin = 0;
    A->proj.rec.mode = EC_REC_CAPTURE;
    A->master = 820;
    for (i = 0; i < EC_NTRACK; i++)
        trk(i)->v[EC_P_MUTE] = 0;
}

/* 0.6.84 (owner): POLY recording: keys held together land on one step together (TRIG: a chord; SLICE: the slices) */
static void test_capture_poly(void)
{
    float nt[EC_STEP_NOTES];
    int n, st;
    stop();
    if (ec_liverec_on())
        press(FM1P_B_REC, 40);
    to(EC_MODE_KEYBOARD);
    A->track = 0;
    A->bus = 0;
    clear_steps(0);
    trk(0)->v[EC_P_MACHINE] = EC_MACH_LOOP_TRIG;
    trk(0)->v[EC_P_TRIG_POLY] = 2;
    trk(0)->v[EC_P_PATTERN_STEPS] = 16;
    trk(0)->v[EC_P_KB_LAYOUT] = 1;
    A->pn.oct = 0;
    ec_seq_touch(&A->seq, 0);
    press(FM1P_B_REC, 40);
    key(0, 4, 1);                                  /* C4 starts it */
    run(3);
    key(0, 6, 1);                                  /* E4 with it */
    run(3);
    key(0, 8, 1);                                  /* G4 */
    run(30);
    key(0, 4, 0);
    key(0, 6, 0);
    key(0, 8, 0);
    run(20);
    n = ec_step_notes(tstep(0, 0), nt);
    CHECK(n == 3, "TRIG, POLY, CAPTURE: three keys together = a chord on step 1 (%d notes)", n);
    run_to_step(4.1);
    expect_hit(&st, &n);
    key(0, 5, 1);                                  /* D4 + F4, later in the pass */
    run(2);
    key(0, 7, 1);
    run(20);
    key(0, 5, 0);
    key(0, 7, 0);
    run(20);
    n = ec_step_notes(tstep(0, st), nt);
    CHECK(n == 2, "a second chord on step %d (%d notes)", st + 1, n);
    press(FM1P_B_REC, 40);
    stop();
    clear_steps(0);
    trk(0)->v[EC_P_MACHINE] = EC_MACH_GRID_SLICE;  /* SLICE, POLY: the slice keys together */
    trk(0)->v[EC_P_SLICE_COUNT] = 8;
    ec_seq_touch(&A->seq, 0);
    press(FM1P_B_REC, 40);
    key(1, 0, 1);                                  /* B1 = slice 1 starts it */
    run(3);
    key(1, 2, 1);                                  /* B3 = slice 3 with it */
    run(30);
    key(1, 0, 0);
    key(1, 2, 0);
    run(20);
    CHECK(tstep(0, 0)->slices == 0x5u, "SLICE, POLY, CAPTURE: slices 1 and 3 on step 1 (%x)", tstep(0, 0)->slices);
    press(FM1P_B_REC, 40);
    stop();
    clear_steps(0);
    trk(0)->v[EC_P_TRIG_POLY] = 1;
    trk(0)->v[EC_P_MACHINE] = EC_MACH_LOOP;
    ec_seq_touch(&A->seq, 0);
}

/* 0.6.87 (owner): REC SETUP's NOTES: REPLACE (a note replaces the step's) / ADD (POLY: onto the step) */
static void test_rec_notes(void)
{
    float nt[EC_STEP_NOTES];
    int add, st, mu, n;
    char b[16];
    ec_rs_row_value(7, b, sizeof b);
    CHECK(!strcmp(ec_rs_row_label(7), "NOTES") && !strcmp(b, "REPLACE"), "REC SETUP: NOTES, REPLACE by default (%s)", b);
    for (add = 0; add < 2; add++) {
        stop();
        to(EC_MODE_KEYBOARD);
        A->track = 0;
        A->bus = 0;
        clear_steps(0);
        trk(0)->v[EC_P_MACHINE] = EC_MACH_LOOP_TRIG;
        trk(0)->v[EC_P_TRIG_POLY] = 2;
        trk(0)->v[EC_P_PATTERN_STEPS] = 16;
        trk(0)->v[EC_P_KB_LAYOUT] = 1;
        A->pn.oct = 0;
        tstep(0, 8)->trig = 1;                     /* step 9: C4 already */
        ec_step_note_set(tstep(0, 8), 0);
        ec_seq_touch(&A->seq, 0);
        A->proj.rec.mode = EC_REC_CAPTURE;
        A->proj.rec.add = (uint8_t)add;
        press(FM1P_B_PLAY, 40);
        press(FM1P_B_REC, 40);                     /* playing: an overdub */
        run_to_step(8.0);
        expect_hit(&st, &mu);
        key(0, 6, 1);                              /* E4 onto step 9 */
        run(5);
        key(0, 6, 0);
        run(20);
        n = ec_step_notes(tstep(0, st), nt);
        CHECK(st == 8 && (add ? n == 2 && nt[0] == 0 && nt[1] == 4 : n == 1 && nt[0] == 4),
              "NOTES %s: step %d holds %d note(s)", add ? "ADD" : "REPLACE", st + 1, n);
        press(FM1P_B_REC, 40);
        stop();
    }
    A->proj.rec.add = 0;
    clear_steps(0);
    trk(0)->v[EC_P_TRIG_POLY] = 1;
    trk(0)->v[EC_P_MACHINE] = EC_MACH_LOOP;
    ec_seq_touch(&A->seq, 0);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("usage: ec_test_liverec <folder>\n");
        return 2;
    }
    ec_fs_native_root(argv[1], "elasticat");
    boot(NULL, 0);
    test_settings();
    test_capture_notes();
    test_capture_over_old();
    test_loop_keys();
    test_automation();
    test_fixed();
    test_click();
    test_capture_poly();
    test_rec_notes();
    C->shutdown();
    printf("test_liverec: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
