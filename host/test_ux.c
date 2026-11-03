/* SPDX-License-Identifier: GPL-3.0-only */
/* test_ux: the 0.3.0 panel (docs/FM1-CONTROLS.md) through the core descriptor, with the demo folder
 * (host/make_demo_folder.py): lanes and SELECT running on into the next page, setup by a hold and by SHIFT, SRC held
 * and released vs the loop keys, PRESETS = the track's sample (debounced), knob acceleration and SHIFT snap, F3 by
 * both OCT keys and by PLAY held, pop-ups (OCT- back, OCT+ OK, the question), track keys (hold = mute, double-tap =
 * solo, FILL + a track key = all on), OCT+ held + a knob clears a lock, HOME, undo / redo, SHIFT's keys, the live
 * labels.
 *   ec_test_ux <folder> */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ec_app.h"
#include "ec_browse.h"
#include "ec_fs_native.h"
#include "fm1core.h"
#include "ec_layout_test.h"

uint32_t fm1core_labels_epoch(void);
const char *fm1core_label(int32_t which);

static int fails, checks;
#define CHECK(c, ...)                                                                                              \
    do {                                                                                                           \
        checks++;                                                                                                  \
        if (!(c)) {                                                                                                \
            fails++;                                                                                               \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                                                            \
            printf(__VA_ARGS__);                                                                                   \
            printf("\n");                                                                                          \
        }                                                                                                          \
    } while (0)

#define A (&ec_app)
static const fm1core_t *C;
static fm1panel_t *P;
static uint32_t ms;

static void run(int n)
{
    int i;
    for (i = 0; i < n; i++) {
        ec_fs_native_pump();
        C->tick(ms++);
        if (ms % 15 == 0)
            C->frame();
    }
}
static void btn(int b, int down)
{
    const uint32_t m = 1u << P->btn_id[b];
    if (down) {
        P->buttons |= m;
        P->buttons_tap |= m;
    } else
        P->buttons &= ~m;
}
static void press(int b, int dur)
{
    btn(b, 1);
    run(dur);
    btn(b, 0);
    run(20);
}
static int key_of(int upper, int i)
{
    int k, nl = 0, nu = 0;
    for (k = 0; k < FM1P_NKEY; k++) {
        const int s = k % 12;
        const int natural = s == 0 || s == 2 || s == 4 || s == 6 || s == 7 || s == 9 || s == 11;
        if (natural && !upper && nl++ == i)
            return k;
        if (!natural && upper && nu++ == i)
            return k;
    }
    return 0;
}
static void key(int upper, int i, int down)
{
    const int k = key_of(upper, i);
    if (down) {
        P->key_vel[k] = 100;
        P->keys |= 1u << k;
        P->keys_tap |= 1u << k;
    } else
        P->keys &= ~(1u << k);
}
static void tapkey(int upper, int i, int dur)
{
    key(upper, i, 1);
    run(dur);
    key(upper, i, 0);
    run(20);
}
static void turn(int role, int n)
{
    P->enc[P->enc_id[role]] += n * P->enc_dir[role];
    run(1);
}
static float tv(int id) { return ec_project_track(&A->proj, A->track)->v[id]; }
static int lbl(int which, const char *want)
{
    const char *s = fm1core_label(which);
    return s && !strcmp(s, want);
}
static const char *L(int which)
{
    const char *s = fm1core_label(which);
    return s ? s : "(null)";
}
static void home(void) { press(FM1P_B_HOME, 40); }

/* ------------------------------------------------------------------------------------------- lanes --- */
static void test_lanes(void)
{
    home();
    press(FM1P_B_LFO, 40);                                    /* MOD, LFO 1: 5 cells = 2 lanes */
    CHECK(A->cat == EC_CAT_MOD && A->page[EC_CAT_MOD] == 0 && A->ux.lane[EC_CAT_MOD] == 0, "MOD LFO 1 lane 1");
    turn(FM1P_E_SELECT, 1);
    run(50);
    CHECK(A->page[EC_CAT_MOD] == 0 && A->ux.lane[EC_CAT_MOD] == 1, "SELECT: lane 2 (page %d lane %d)",
          A->page[EC_CAT_MOD], A->ux.lane[EC_CAT_MOD]);
    {
        const float m0 = tv(EC_P_LFO1_MODE);
        turn(FM1P_E_K1, 1);                                   /* lane 2's KN1 = cell 5, LFO 1 MODE */
        run(100);
        CHECK(tv(EC_P_LFO1_MODE) == m0 + 1, "lane 2 KN1 = LFO1 MODE (%g -> %g)", m0, tv(EC_P_LFO1_MODE));
    }
    turn(FM1P_E_SELECT, 1);
    run(50);
    CHECK(A->page[EC_CAT_MOD] == 1 && A->ux.lane[EC_CAT_MOD] == 0, "SELECT runs on into LFO 2 (page %d lane %d)",
          A->page[EC_CAT_MOD], A->ux.lane[EC_CAT_MOD]);
    turn(FM1P_E_SELECT, -1);
    run(50);
    CHECK(A->page[EC_CAT_MOD] == 0 && A->ux.lane[EC_CAT_MOD] == 1, "SELECT back: LFO 1 lane 2");
    press(FM1P_B_LFO, 40);                                    /* the button again: the next page, lane 1 */
    CHECK(A->page[EC_CAT_MOD] == 1 && A->ux.lane[EC_CAT_MOD] == 0, "MOD again: the next page");
    press(FM1P_B_EDIT, 40);
    press(FM1P_B_LFO, 40);
    CHECK(A->cat == EC_CAT_MOD && A->page[EC_CAT_MOD] == 1, "each category keeps its place");
}

/* ------------------------------------------------------------------------------------------- setup --- */
static void test_setup(void)
{
    home();
    btn(FM1P_B_FX, 1);                                        /* hold FX: its setup at 400 ms */
    run(300);
    CHECK(!A->settings, "FX held 300 ms: not yet");
    run(150);
    CHECK(A->settings && A->cat == EC_CAT_FX, "FX held 450 ms: the FX setup");
    btn(FM1P_B_FX, 0);
    run(30);
    CHECK(A->settings && A->cat == EC_CAT_FX, "it stays open on release");
    press(FM1P_B_OCTDN, 40);
    CHECK(!A->settings && A->cat == EC_CAT_FX, "OCT- in the setup: back to the FX pages");
    CHECK(A->keymode == EC_KM_STEPS, "(and no key-mode change)");
    btn(FM1P_B_GLO, 1);
    run(20);
    press(FM1P_B_EDIT, 40);
    CHECK(A->settings && A->cat == EC_CAT_FILT, "SHIFT + EDIT: the FILT setup at once");
    press(FM1P_B_EDIT, 40);
    btn(FM1P_B_GLO, 0);
    run(20);
    CHECK(!A->settings && A->cat == EC_CAT_FILT, "SHIFT + EDIT again: back to the pages");
    press(FM1P_B_LFO, 500);                                   /* 0.5.0: MOD has no setup: a hold is MOD KEYS */
    CHECK(!A->settings && A->cat == EC_CAT_FILT && A->mk.on, "MOD held: MOD KEYS on, the page stays");
    press(FM1P_B_LFO, 500);
    CHECK(!A->mk.on, "MOD held again: MOD KEYS off");
    press(FM1P_B_LFO, 40);
    CHECK(!A->settings && A->cat == EC_CAT_MOD, "MOD tapped: its pages");
    press(FM1P_B_ENV, 450);
    press(FM1P_B_EDIT, 40);
    CHECK(!A->settings && A->cat == EC_CAT_FILT, "in a setup, another button: its pages");
}

/* SRC: held with a lower key = the loop keys; held 400 ms and released with none used = the SRC setup */
static void test_src_hold(void)
{
    int p0;
    home();
    p0 = A->page[EC_CAT_SRC];
    press(FM1P_B_SEL, 60);
    CHECK(A->cat == EC_CAT_SRC && A->page[EC_CAT_SRC] == (p0 + 1) % 4 && !A->settings, "SRC tap: the next page");
    btn(FM1P_B_SEL, 1);
    run(100);
    tapkey(0, 3, 80);
    run(400);
    btn(FM1P_B_SEL, 0);
    run(20);
    CHECK(!A->settings && A->cat == EC_CAT_SRC && A->page[EC_CAT_SRC] == (p0 + 1) % 4,
          "SRC held + a loop key: no setup, no page change");
    btn(FM1P_B_SEL, 1);
    run(500);
    CHECK(!A->settings, "SRC held alone: nothing until its release");
    btn(FM1P_B_SEL, 0);
    run(20);
    CHECK(A->settings && A->cat == EC_CAT_SRC, "SRC held 500 ms, released: the SRC setup");
    home();
    CHECK(!A->settings && A->cat == EC_CAT_SRC, "HOME closes it");
}

/* ------------------------------------------------------------------------------------------- PRESETS --- */
static void test_presets_sample(void)
{
    const int s = ec_app_track_slot(A, A->track);
    char before[EC_PATH];
    int i, idx;
    home();
    snprintf(before, sizeof before, "%s", A->proj.slot[s].path);
    idx = ec_browse_find(before);
    CHECK(idx >= 0 && ec_browse_count() == 4, "T1 plays a folder file (%d of %d)", idx, ec_browse_count());
    for (i = 0; i < 3; i++) {                                 /* a quick spin: three files on */
        turn(FM1P_E_PRESETS, 1);
        run(40);
    }
    CHECK(!strcmp(A->proj.slot[s].path, before), "spinning: nothing loads yet");
    CHECK(A->ux.to_kind == EC_TO_SAMPLE && A->ux.to_ms && strstr(A->ux.to_val, "d_perc16"),
          "the takeover shows the file (%s)", A->ux.to_val);
    run(200);
    CHECK(!strcmp(A->proj.slot[s].path, ec_browse_path((idx + 3) % 4)), "at rest: one load of the third file (%s)",
          A->proj.slot[s].path);
    run(800);
    CHECK(A->slot[s].state == EC_SL_READY, "loaded and playing");
    turn(FM1P_E_PRESETS, -3);
    run(300);
    CHECK(!strcmp(A->proj.slot[s].path, before), "back to the first file");
    run(500);
    btn(FM1P_B_GLO, 1);                                       /* SHIFT + PRESETS: the pattern */
    run(20);
    turn(FM1P_E_PRESETS, 2);
    run(20);
    btn(FM1P_B_GLO, 0);
    run(20);
    CHECK(A->seq.pending == 2 || A->proj.current == 2, "SHIFT + PRESETS: pattern 3 (pending %d current %d)",
          A->seq.pending, A->proj.current);
    turn(FM1P_E_PRESETS, 0);
    btn(FM1P_B_GLO, 1);
    run(20);
    turn(FM1P_E_PRESETS, -2);
    run(20);
    btn(FM1P_B_GLO, 0);
    run(1200);
}

/* ------------------------------------------------------------------------------------------- knobs --- */
static void test_accel_snap(void)
{
    float x;
    home();
    press(FM1P_B_EDIT, 40);                                   /* FILT: KN1 = CUT (0..127) */
    if (A->page[EC_CAT_FILT] != 0) {
        A->page[EC_CAT_FILT] = 0;
        A->ux.lane[EC_CAT_FILT] = 0;
    }
    ec_project_track(&A->proj, A->track)->v[EC_P_FILTER_CUTOFF] = 40;
    run(200);
    turn(FM1P_E_K1, 1);
    run(200);
    turn(FM1P_E_K1, 1);
    run(200);
    CHECK(tv(EC_P_FILTER_CUTOFF) == 42, "slow: one step a click (%g)", tv(EC_P_FILTER_CUTOFF));
    for (int i = 0; i < 6; i++) {                             /* spun: 10 ms apart */
        turn(FM1P_E_K1, 1);
        run(9);
    }
    x = tv(EC_P_FILTER_CUTOFF);
    CHECK(x > 42 + 6 && x <= 42 + 6 * 8, "spun: bigger steps (42 -> %g; at most ~1/16 of the range a click)", x);
    run(300);
    ec_project_track(&A->proj, A->track)->v[EC_P_FILTER_CUTOFF] = 50;
    btn(FM1P_B_GLO, 1);
    run(20);
    turn(FM1P_E_K1, 1);
    run(150);
    CHECK(tv(EC_P_FILTER_CUTOFF) == 64, "SHIFT + KN1: snaps 50 -> 64 (%g)", tv(EC_P_FILTER_CUTOFF));
    turn(FM1P_E_K1, -1);
    run(150);
    CHECK(tv(EC_P_FILTER_CUTOFF) == 32, "SHIFT + KN1 back: 32 (%g)", tv(EC_P_FILTER_CUTOFF));
    turn(FM1P_E_K3, 1);                                       /* TYPE: a list, one item */
    run(150);
    btn(FM1P_B_GLO, 0);
    run(20);
    CHECK(tv(EC_P_FILTER_TYPE) == 2, "a list under SHIFT: one item (%g)", tv(EC_P_FILTER_TYPE));
    CHECK(!A->ux.to_ms, "the cells show the value: no takeover");
    /* lists never speed up */
    {
        const float t0 = tv(EC_P_FILTER_TYPE);
        turn(FM1P_E_K3, -1);
        run(5);
        turn(FM1P_E_K3, 1);
        run(5);
        turn(FM1P_E_K3, 1);
        run(200);
        CHECK(tv(EC_P_FILTER_TYPE) == t0 + 1, "list spun: one item a click (%g)", tv(EC_P_FILTER_TYPE));
    }
    /* undo / redo (SHIFT + OCT- / OCT+) */
    run(1200);
    {
        const float c0 = tv(EC_P_FILTER_CUTOFF);
        turn(FM1P_E_K1, 3);
        run(1200);
        CHECK(tv(EC_P_FILTER_CUTOFF) == c0 + 3, "turned +3");
        btn(FM1P_B_GLO, 1);
        run(20);
        press(FM1P_B_OCTDN, 40);
        CHECK(tv(EC_P_FILTER_CUTOFF) == c0, "SHIFT + OCT-: undo (%g)", tv(EC_P_FILTER_CUTOFF));
        press(FM1P_B_OCTUP, 40);
        CHECK(tv(EC_P_FILTER_CUTOFF) == c0 + 3, "SHIFT + OCT+: redo (%g)", tv(EC_P_FILTER_CUTOFF));
        btn(FM1P_B_GLO, 0);
        run(20);
        CHECK(A->keymode == EC_KM_STEPS, "undo / redo leave the key mode alone");
    }
    /* the MIX page shows no cells: a knob takes over the screen */
    press(FM1P_B_LFO, 40);
    tapkey(1, 8, 60);                                          /* MST key: the MST category, MIX */
    CHECK(A->cat == EC_CAT_MASTER && A->page[EC_CAT_MASTER] == 0, "MST key: MIX (cat %d page %d)", A->cat,
          A->page[EC_CAT_MASTER]);
    turn(FM1P_E_K2, 1);
    run(20);
    CHECK(A->ux.to_ms && A->ux.to_kind == EC_TO_VALUE && !strcmp(A->ux.to_label, "PAN"), "MIX + KN2: PAN taken over");
    run(1100);
    CHECK(!A->ux.to_ms, "the takeover ends after a second");
    turn(FM1P_E_K2, -1);
    run(1100);
}

/* ------------------------------------------------------------------------------------------- F3 --- */
static void test_f3(void)
{
    float bpm;
    home();
    btn(FM1P_B_OCTDN, 1);
    run(30);
    btn(FM1P_B_OCTUP, 1);
    run(30);
    CHECK(A->ux.f3, "OCT- + OCT+: F3");
    btn(FM1P_B_OCTDN, 0);
    btn(FM1P_B_OCTUP, 0);
    run(30);
    CHECK(A->ux.f3 && A->keymode == EC_KM_STEPS, "F3 stays open on release, the key mode unchanged");
    bpm = A->proj.bpm;
    turn(FM1P_E_SELECT, 2);
    run(200);
    CHECK(A->proj.bpm == bpm + 2, "F3 + SELECT: the tempo (%g -> %g)", bpm, A->proj.bpm);
    turn(FM1P_E_SELECT, -2);
    run(200);
    {
        const float sw = A->proj.swing;
        turn(FM1P_E_ALGO, 3);
        run(50);
        CHECK(A->proj.swing == sw + 3, "F3 + ALGORITHM: the swing");
        turn(FM1P_E_ALGO, -3);
        run(50);
    }
    {
        const float len = tv(EC_P_PATTERN_STEPS);
        turn(FM1P_E_K1, 16);
        run(300);
        CHECK(tv(EC_P_PATTERN_STEPS) == len + 16, "F3 + KN1: the pattern LEN (%g)", tv(EC_P_PATTERN_STEPS));
    }
    tapkey(0, 1, 60);                                         /* tap page 2: edit it */
    CHECK(A->step_page == 1, "F3 + lower key 2 tapped: page 2 (%d)", A->step_page);
    CHECK(lbl(FM1CORE_LBL_KNOB0 + FM1P_E_SELECT, "TEMPO") && lbl(FM1CORE_LBL_BTN0 + FM1P_B_OCTDN, "BACK") &&
              lbl(FM1CORE_LBL_WHITE0 + 3, "PAGE 4"),
          "F3 labels: SELECT %s, OCT- %s, W4 %s", L(FM1CORE_LBL_KNOB0 + FM1P_E_SELECT),
          L(FM1CORE_LBL_BTN0 + FM1P_B_OCTDN), L(FM1CORE_LBL_WHITE0 + 3));
    tapkey(0, 0, 500);                                        /* hold page 1: out of the loop */
    CHECK(!ec_e5_page_in_loop(&A->proj, A->track, 0, 2) && A->step_page == 1, "F3 + key 1 held: page 1 leaves the loop");
    tapkey(0, 0, 500);
    CHECK(ec_e5_page_in_loop(&A->proj, A->track, 0, 2), "held again: back in");
    tapkey(0, 0, 60);
    press(FM1P_B_OCTDN, 40);
    CHECK(!A->ux.f3 && A->keymode == EC_KM_STEPS, "OCT- closes F3 (no key-mode change)");
    turn(FM1P_E_K1, 0);
    /* PLAY held: F3, without starting the transport; a tap plays */
    btn(FM1P_B_PLAY, 1);
    run(450);
    CHECK(A->ux.f3 && !A->seq.playing, "PLAY held: F3, still stopped");
    btn(FM1P_B_PLAY, 0);
    run(30);
    CHECK(A->ux.f3 && !A->seq.playing, "released: F3 stays, still stopped");
    press(FM1P_B_PLAY, 60);
    CHECK(A->seq.playing, "PLAY tapped in F3: plays");
    press(FM1P_B_HOME, 40);
    CHECK(!A->ux.f3 && A->cat == EC_CAT_SRC, "HOME closes F3");
    btn(FM1P_B_GLO, 1);
    run(20);
    press(FM1P_B_PLAY, 40);                                   /* SHIFT + PLAY: stop + step 1 */
    btn(FM1P_B_GLO, 0);
    run(20);
    CHECK(!A->seq.playing && !A->seq.paused, "SHIFT + PLAY: stopped");
    /* LEN back */
    press(FM1P_B_OCTDN, 30);                                  /* a tap: LOOP KEYS */
    CHECK(A->keymode == EC_KM_LOOP, "OCT- tapped: loop keys");
    press(FM1P_B_OCTUP, 30);
    CHECK(A->keymode == EC_KM_SCENE, "OCT+ tapped too: scene (both)");
    press(FM1P_B_OCTDN, 30);
    press(FM1P_B_OCTUP, 30);
    CHECK(A->keymode == EC_KM_STEPS, "both again: steps");
    ec_project_track(&A->proj, A->track)->v[EC_P_PATTERN_STEPS] = 16;
    A->step_page = 0;
}

/* ------------------------------------------------------------------------------------------- pop-ups --- */
static void test_popups(void)
{
    ec_ptrack_t *t;
    int i, trigs = 0;
    home();
    tapkey(1, 10, 60);                                        /* PATN tapped: the overlay stays */
    CHECK(A->patn_open, "PATN tapped: the overlay stays open");
    CHECK(lbl(FM1CORE_LBL_BTN0 + FM1P_B_OCTUP, "OK"), "pop-up: OCT+ OK (%s)", L(FM1CORE_LBL_BTN0 + FM1P_B_OCTUP));
    press(FM1P_B_OCTDN, 40);
    CHECK(!A->patn_open && A->keymode == EC_KM_STEPS, "OCT- closes it");
    tapkey(1, 10, 600);                                       /* held long, nothing used: stays too */
    CHECK(A->patn_open, "PATN held 600 ms: stays open");
    home();
    CHECK(!A->patn_open, "HOME closes it");
    /* the question: hold PATN + PLAY = CLEAR P01?; OCT+ answers yes */
    tapkey(0, 0, 40);
    tapkey(0, 4, 40);
    key(1, 10, 1);
    run(100);
    press(FM1P_B_PLAY, 40);
    key(1, 10, 0);
    run(100);
    CHECK(A->e5.confirm && ec_app_popup(A), "PATN + PLAY: the question stays open after PATN's release");
    CHECK(lbl(FM1CORE_LBL_BTN0 + FM1P_B_OCTUP, "YES"), "the question's OCT+: YES (%s)",
          L(FM1CORE_LBL_BTN0 + FM1P_B_OCTUP));
    press(FM1P_B_OCTDN, 40);
    t = ec_project_track(&A->proj, 0);
    for (i = 0; i < 16; i++)
        trigs += t->steps[i].trig;
    CHECK(!A->e5.confirm && trigs == 2, "OCT-: no (trigs %d)", trigs);
    key(1, 10, 1);
    run(100);
    press(FM1P_B_PLAY, 40);
    key(1, 10, 0);
    run(100);
    press(FM1P_B_OCTUP, 40);
    for (i = 0, trigs = 0; i < 16; i++)
        trigs += t->steps[i].trig;
    CHECK(!A->e5.confirm && trigs == 0, "OCT+: yes, cleared (trigs %d)", trigs);
    /* naming: SHIFT + PATN; OCT+ keeps */
    btn(FM1P_B_GLO, 1);
    run(20);
    tapkey(1, 10, 40);
    btn(FM1P_B_GLO, 0);
    run(20);
    CHECK(A->e5.naming == EC_NM_PATTERN, "SHIFT + PATN: name the pattern");
    turn(FM1P_E_K3, 1);
    run(20);
    press(FM1P_B_OCTUP, 40);
    CHECK(!A->e5.naming && A->proj.pe5[A->proj.current].name[0], "OCT+ keeps the name (%s)",
          A->proj.pe5[A->proj.current].name);
}

/* ------------------------------------------------------------------------------------------- tracks --- */
static void test_tracks(void)
{
    home();
    tapkey(1, 2, 40);
    CHECK(A->track == 2, "T3 tapped: selected");
    tapkey(1, 1, 500);
    CHECK(tv(EC_P_MUTE) < 0.5f && ec_project_track(&A->proj, 1)->v[EC_P_MUTE] > 0.5f && A->track == 2,
          "T2 held: muted, not selected");
    tapkey(1, 3, 40);
    run(60);
    tapkey(1, 3, 40);
    CHECK(A->ux.solo == 1u << 3, "T4 double-tapped: solo (%02x)", A->ux.solo);
    CHECK(!ec_app_track_audible(A, 0) && ec_app_track_audible(A, 3), "a solo silences the others");
    run(400);
    key(1, 9, 1);                                             /* FILL held + a track key: all on */
    run(50);
    tapkey(1, 0, 40);
    key(1, 9, 0);
    run(30);
    CHECK(!A->ux.solo && ec_project_track(&A->proj, 1)->v[EC_P_MUTE] < 0.5f, "FILL + T1: every mute and solo off");
    CHECK(!A->fill_latched && !ec_seq_fill_active(&A->seq), "(no fill left on)");
    tapkey(1, 0, 40);
    run(400);
}

/* ------------------------------------------------------------------------------------------- locks --- */
static void test_locks(void)
{
    ec_step_t *s;
    float x;
    home();
    press(FM1P_B_EDIT, 40);                                   /* FILT: KN1 = CUT */
    A->page[EC_CAT_FILT] = 0;
    A->ux.lane[EC_CAT_FILT] = 0;
    key(0, 2, 1);                                             /* hold step 3, turn KN1: a lock */
    run(300);
    CHECK(lbl(FM1CORE_LBL_BTN0 + FM1P_B_REC, "COPY"), "a step held: REC = COPY (%s)", L(FM1CORE_LBL_BTN0 + FM1P_B_REC));
    turn(FM1P_E_K1, -10);
    run(100);
    s = &ec_project_track(&A->proj, A->track)->steps[2];
    CHECK(ec_step_lock_get(s, EC_P_FILTER_CUTOFF, &x), "held step + KN1: locked");
    btn(FM1P_B_OCTUP, 1);                                     /* OCT+ held + KN1: clear that lock */
    run(50);
    turn(FM1P_E_K1, 1);
    run(50);
    btn(FM1P_B_OCTUP, 0);
    run(30);
    key(0, 2, 0);
    run(30);
    CHECK(!ec_step_lock_get(s, EC_P_FILTER_CUTOFF, &x), "OCT+ held + KN1: the lock cleared");
    CHECK(A->keymode == EC_KM_STEPS, "(OCT+ used: no key-mode change)");
    CHECK(s->trig, "the trig stays");
    tapkey(0, 2, 40);                                         /* remove the trig */
}

/* ------------------------------------------------------------------------------------------- SHIFT --- */
static void test_shift(void)
{
    uint32_t e;
    home();
    btn(FM1P_B_GLO, 1);
    run(300);
    CHECK(ec_app_shift_map(A), "SHIFT held alone: its map");
    CHECK(lbl(FM1CORE_LBL_BTN0 + FM1P_B_OCTDN, "UNDO") && lbl(FM1CORE_LBL_BTN0 + FM1P_B_OCTUP, "REDO") &&
              lbl(FM1CORE_LBL_BTN0 + FM1P_B_PLAY, "STOP") && lbl(FM1CORE_LBL_BTN0 + FM1P_B_REC, "SAVE") &&
              lbl(FM1CORE_LBL_BTN0 + FM1P_B_EDIT, "SETUP") && lbl(FM1CORE_LBL_BTN0 + FM1P_B_LFO, ""),
          "SHIFT's buttons: %s %s %s %s %s '%s'", L(FM1CORE_LBL_BTN0 + FM1P_B_OCTDN), L(FM1CORE_LBL_BTN0 + FM1P_B_OCTUP),
          L(FM1CORE_LBL_BTN0 + FM1P_B_PLAY), L(FM1CORE_LBL_BTN0 + FM1P_B_REC), L(FM1CORE_LBL_BTN0 + FM1P_B_EDIT),
          L(FM1CORE_LBL_BTN0 + FM1P_B_LFO));
    CHECK(lbl(FM1CORE_LBL_BLACK0 + 0, "MEMORIZE") && lbl(FM1CORE_LBL_BLACK0 + 8, "TAP") &&
              lbl(FM1CORE_LBL_BLACK0 + 3, "") && lbl(FM1CORE_LBL_KNOB0 + FM1P_E_PRESETS, "PATTERN"),
          "SHIFT's keys: %s %s '%s' %s", L(FM1CORE_LBL_BLACK0), L(FM1CORE_LBL_BLACK0 + 8), L(FM1CORE_LBL_BLACK0 + 3),
          L(FM1CORE_LBL_KNOB0 + FM1P_E_PRESETS));
    e = fm1core_labels_epoch();
    run(100);
    CHECK(fm1core_labels_epoch() == e, "nothing changed: the epoch stays");
    tapkey(1, 0, 40);                                         /* SHIFT + T1: memorize */
    CHECK(A->e5.memo_ok && !ec_app_shift_map(A), "SHIFT + T1: memorized (the map gives way)");
    tapkey(1, 8, 40);                                         /* tap tempo: 500 ms = 120 */
    run(440);
    tapkey(1, 8, 40);
    run(440);
    tapkey(1, 8, 40);
    CHECK(fabsf(A->proj.bpm - 120.0f) < 0.6f, "SHIFT + MST tapped at 500 ms: 120 BPM (%g)", A->proj.bpm);
    btn(FM1P_B_GLO, 0);
    run(30);
    CHECK(lbl(FM1CORE_LBL_BTN0 + FM1P_B_OCTDN, "LOOP KEYS") && lbl(FM1CORE_LBL_BLACK0, "T1") &&
              lbl(FM1CORE_LBL_KNOB0 + FM1P_E_PRESETS, "SAMPLE") && lbl(FM1CORE_LBL_KNOB0 + FM1P_E_ALGO, "TRACK") &&
              lbl(FM1CORE_LBL_WHITE0 + 4, "5"),
          "released: OCT- %s, B1 %s, PRESETS %s, W5 %s", L(FM1CORE_LBL_BTN0 + FM1P_B_OCTDN), L(FM1CORE_LBL_BLACK0),
          L(FM1CORE_LBL_KNOB0 + FM1P_E_PRESETS), L(FM1CORE_LBL_WHITE0 + 4));
    /* the RAZOR page: SHIFT + OCT keep Auto-Chop / Redistribute */
    press(FM1P_B_SAVE, 40);
    A->page[EC_CAT_FILE] = 1;
    A->ux.lane[EC_CAT_FILE] = 0;
    run(40);
    btn(FM1P_B_GLO, 1);
    run(40);
    CHECK(lbl(FM1CORE_LBL_BTN0 + FM1P_B_OCTUP, "AUTO-CHOP") && lbl(FM1CORE_LBL_BTN0 + FM1P_B_OCTDN, "EVEN"),
          "RAZOR + SHIFT: OCT+ %s, OCT- %s", L(FM1CORE_LBL_BTN0 + FM1P_B_OCTUP), L(FM1CORE_LBL_BTN0 + FM1P_B_OCTDN));
    btn(FM1P_B_GLO, 0);
    run(40);
    CHECK(lbl(FM1CORE_LBL_BTN0 + FM1P_B_OCTUP, "SPLIT") && lbl(FM1CORE_LBL_KNOB0 + FM1P_E_SELECT, "SLICE"),
          "RAZOR: OCT+ %s, SELECT %s", L(FM1CORE_LBL_BTN0 + FM1P_B_OCTUP), L(FM1CORE_LBL_KNOB0 + FM1P_E_SELECT));
    press(FM1P_B_SAVE, 40);                                   /* SAVE on RAZOR: its lane 2 */
    CHECK(A->page[EC_CAT_FILE] == 1 && A->ux.lane[EC_CAT_FILE] == 1, "SAVE on RAZOR: lane 2");
    CHECK(lbl(FM1CORE_LBL_KNOB0 + FM1P_E_K1, "SNAP"), "RAZOR lane 2 KN1: %s", L(FM1CORE_LBL_KNOB0 + FM1P_E_K1));
    press(FM1P_B_SAVE, 40);
    CHECK(A->page[EC_CAT_FILE] == 0, "SAVE again: the SAMPLE page");
    home();
}

int main(int argc, char **argv)
{
    ec_layout_use(&EC_LAYOUT_TEST);   /* the tests' fixed layout: 0.4.0's pages (host/ec_layout_test.h) */
    if (argc < 2) {
        printf("usage: ec_test_ux <folder>\n");
        return 2;
    }
    ec_fs_native_root(argv[1], "elasticat");
    C = fm1core_get(FM1CORE_ABI);
    if (!C)
        return 1;
    P = C->panel;
    C->flash_stage(NULL, 0);
    C->options(FM1CORE_FLASH_STAGED, 0, 0, 1);
    P->ready = 2;
    C->init(0);
    run(1500);                                                /* the scan, T1's first file */
    CHECK(fm1core_labels_epoch() != 0 && lbl(FM1CORE_LBL_BTN0 + FM1P_B_GLO, "SHIFT") &&
              lbl(FM1CORE_LBL_BTN0 + FM1P_B_HOME, "HOME"),
          "labels at boot: GLO %s, HOME %s", L(FM1CORE_LBL_BTN0 + FM1P_B_GLO), L(FM1CORE_LBL_BTN0 + FM1P_B_HOME));
    test_lanes();
    test_setup();
    test_src_hold();
    test_presets_sample();
    test_accel_snap();
    test_f3();
    test_popups();
    test_tracks();
    test_locks();
    test_shift();
    C->shutdown();
    printf("test_ux: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
