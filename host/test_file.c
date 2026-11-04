/* SPDX-License-Identifier: GPL-3.0-only */
/* test_file: 0.6.0's FILE, SAMPLER, slot locks and the 4X rate (docs/UX-V2.md §3, §5) through the panel, with the
 * demo folder (host/make_demo_folder.py) plus a subfolder tree written here (audio/breaks/think_bpm96.wav,
 * audio/breaks/deep/hit.wav) and the shipped layout:
 *   FILE: refused while playing (the SAMPLER opens instead; STOP FIRST in ALERT, the button flashes), PLAY / REC do
 *   nothing inside; the folder walk (SEL, B1 up, B2 in); auto-preview on each move (and off), B11 preview, heard on
 *   the output; B10 LOAD into the next free slot (the preview adopted, the cursor put) x3, FULL; F2 frees a pool
 *   file's slot; SHIFT + B11 purges (unused slots freed, the arena compacted, the samples intact); the editor (zoom
 *   keys, scroll, the trim by white keys); the pop-up roles.
 *   SAMPLER: B11 at once while stopped, the length keys, the take in the next free slot tagged with the BPM, the WAV
 *   in /data/rec; while playing armed to the next bar, B11 again stops at the bar's end; the VU on the white keys.
 *   EC_P_SLOT step locks: the trig plays the locked slot, the next unlocked trig the track's own.
 *   MIX's B8 = the 4X rate.
 *   ec_test_file <folder> */
#include "ec_harness.h"
#include "ec_loader.h"
#include "ec_projfile.h"
#include "ec_razor_ui.h"

static const char *root;

static int find_entry(const char *name)
{
    int i, d, f;
    for (i = 0; i < ec_file_count(); i++)
        if (!strcmp(ec_file_entry(i, &d, &f), name))
            return i;
    return -1;
}
static void cursor_to(const char *name)
{
    const int i = find_entry(name);
    if (i >= 0) {
        turn(FM1P_E_SELECT, i - A->cursor);
        run(10);
    }
}
static const char *cursor_name(void)
{
    int d, f;
    return ec_file_entry(A->cursor, &d, &f);
}
static void settle(int n)
{
    int i;
    for (i = 0; i < n && (A->load_slot >= 0 || A->pn.pv_state == EC_PV_LOADING || A->pn.pv_state == EC_PV_WAIT); i++)
        run(10);
    run(20);
}

/* ------------------------------------------------------------------------------------------- gating --- */
/* 0.6.11 (QA finding 5): FILE opens while playing, on BROWSE; only the EDITOR, RAZOR, FREE, MOVE and purge need a stop */
static void test_gating(void)
{
    to(EC_CAT_SRC);
    press(FM1P_B_PLAY, 40);
    CHECK(A->seq.playing, "playing");
    press(FM1P_B_SAVE, 40);
    CHECK(mode() == EC_CAT_FILE && ec_file_page() == EC_PIC_FILE && A->seq.playing,
          "FILE while playing: BROWSE opens, still playing (%d)", mode());
    B(4, 40);
    CHECK(ec_file_page() == EC_PIC_FILE && A->msg_alert && !strcmp(A->msg, "STOP FIRST"),
          "B4 while playing: the EDITOR refused, STOP FIRST (%s)", A->msg);
    B(5, 40);
    CHECK(ec_file_page() == EC_PIC_FILE && !strcmp(A->msg, "STOP FIRST"), "B5 while playing: RAZOR refused");
    press(FM1P_B_OCTUP, 40);
    CHECK(!strcmp(A->msg, "STOP FIRST"), "F2 (FREE) while playing: refused (%s)", A->msg);
    press(FM1P_B_SAVE, 40);
    CHECK(mode() == EC_MODE_SAMPLER, "FILE again: the SAMPLER (%d)", mode());
    press(FM1P_B_PLAY, 40);                        /* PLAY works in the SAMPLER */
    CHECK(!A->seq.playing, "stopped from the SAMPLER");
    to(EC_CAT_SRC);
    press(FM1P_B_SAVE, 40);
    CHECK(mode() == EC_CAT_FILE && ec_file_page() == EC_PIC_FILE, "stopped, from LOOP ENGINE: FILE on BROWSE (%d)",
          mode());
    press(FM1P_B_PLAY, 40);
    CHECK(A->seq.playing, "inside BROWSE PLAY starts");
    press(FM1P_B_PLAY, 40);
    CHECK(!A->seq.playing, "and stops");
    B(4, 40);
    CHECK(ec_file_page() == EC_PIC_EDITOR, "stopped: B4 opens the EDITOR");
    press(FM1P_B_PLAY, 40);
    CHECK(!A->seq.playing && ec_file_page() == EC_PIC_FILE, "PLAY on the EDITOR: back to BROWSE, not playing (%s)",
          A->msg);
    press(FM1P_B_PLAY, 40);
    CHECK(A->seq.playing, "PLAY again: playing from BROWSE");
    press(FM1P_B_PLAY, 40);
    press(FM1P_B_REC, 40);
    press(FM1P_B_REC, 500);
    CHECK(!ec_liverec_on() && !A->pn.rs_open, "inside FILE REC does nothing (tap, hold)");
    B(4, 40);
    to(EC_CAT_SRC);
    press(FM1P_B_SAVE, 40);
    CHECK(mode() == EC_CAT_FILE && ec_file_page() == EC_PIC_FILE, "FILE from another mode: BROWSE, never the EDITOR");
}
/* ------------------------------------------------------------------------------------------- the browser --- */
static void test_folders(void)
{
    char want[256];
    to(EC_CAT_FILE);
    ec_app_set_page(EC_CAT_FILE, 0);
    A->pn.as.preview = 1;
    turn(FM1P_E_SELECT, -20);
    {
        int d, f;
        const char *n0 = ec_file_entry(0, &d, &f);
        CHECK(!strcmp(n0, "breaks/") && d, "the root: the subfolder first (%s)", n0);
        CHECK(ec_file_count() == 5, "the root: 1 folder + 4 files (%d)", ec_file_count());
    }
    CHECK(A->cursor == 0, "the cursor on the folder");
    B(2, 40);                                      /* B2: into the folder */
    CHECK(!strcmp(A->pn.fdir, "/audio/breaks") && A->cursor == 0, "B2: into /audio/breaks (%s)", A->pn.fdir);
    CHECK(ec_file_count() == 2 && !strcmp(ec_file_entry(0, 0, 0), "deep/") &&
              !strcmp(ec_file_entry(1, 0, 0), "think_bpm96.wav"),
          "breaks: deep/ then think (%d: %s)", ec_file_count(), ec_file_entry(0, 0, 0));
    B(2, 40);
    CHECK(!strcmp(A->pn.fdir, "/audio/breaks/deep") && ec_file_count() == 1, "B2 again: the subfolder's subfolder");
    settle(100);
    snprintf(want, sizeof want, "/audio/breaks/deep/hit.wav");
    CHECK(!strcmp(A->pn.pv_path, want) && A->pn.pv_state == EC_PV_READY, "the move into it previews its file (%s %d)",
          A->pn.pv_path, A->pn.pv_state);
    B(1, 40);                                      /* B1: up, the cursor on where we were */
    CHECK(!strcmp(A->pn.fdir, "/audio/breaks") && !strcmp(cursor_name(), "deep/"), "B1: up, on deep/ (%s %s)",
          A->pn.fdir, cursor_name());
    B(1, 40);
    CHECK(!strcmp(A->pn.fdir, "/audio") && !strcmp(cursor_name(), "breaks/"), "B1: up to /audio, on breaks/");
    B(1, 40);                                      /* 0.6.1: up from /audio: the top level, AUDIO/ and PROJECTS/ */
    CHECK(!strcmp(A->pn.fdir, "/") && ec_file_count() == 2 && !strcmp(cursor_name(), "AUDIO/") &&
              !strcmp(ec_file_entry(1, 0, 0), "PROJECTS/"),
          "B1 at /audio: the top level, on AUDIO/ (%s %d %s)", A->pn.fdir, ec_file_count(), cursor_name());
    B(1, 40);
    CHECK(!strcmp(A->pn.fdir, "/") && !strcmp(A->msg, "TOP FOLDER"), "B1 at the top: stays (%s)", A->msg);
    B(2, 40);
    CHECK(!strcmp(A->pn.fdir, "/audio") && A->cursor == 0, "B2 on AUDIO/: back in /audio (%s)", A->pn.fdir);
    CHECK(lbl(FM1CORE_LBL_BLACK0 + 0, "UP") && lbl(FM1CORE_LBL_BLACK0 + 1, "INTO") &&
              lbl(FM1CORE_LBL_BLACK0 + 9, "LOAD") && lbl(FM1CORE_LBL_BLACK0 + 10, "PREVIEW") &&
              lbl(FM1CORE_LBL_BLACK0 + 3, "EDITOR"),
          "the browser's labels: B1 %s B2 %s B10 %s B11 %s B4 %s", L(FM1CORE_LBL_BLACK0), L(FM1CORE_LBL_BLACK0 + 1),
          L(FM1CORE_LBL_BLACK0 + 9), L(FM1CORE_LBL_BLACK0 + 10), L(FM1CORE_LBL_BLACK0 + 3));
    {
        int cur;
        const char *r = ec_pn_roles(EC_CAT_FILE, &cur);
        CHECK(!strcmp(r, "AAPPPPOOOAX"), "the browser's roles (0.6.1: B6 POOL): %s", r);
    }
}

static void test_preview(void)
{
    uint64_t f0;
    to(EC_CAT_FILE);
    cursor_to("c_chords_bpm120.wav");
    settle(200);
    CHECK(A->pn.pv_state == EC_PV_READY && strstr(A->pn.pv_path, "c_chords") && A->pn.pv_play == 1,
          "auto-preview: the move loads and plays it (%d %s %d)", A->pn.pv_state, A->pn.pv_path, A->pn.pv_play);
    f0 = frames;
    run(200);
    CHECK(!A->seq.playing && out_peak(f0, frames) > 2000, "heard on the output, stopped (peak %d)",
          out_peak(f0, frames));
    B(11, 40);                                     /* B11: stop */
    f0 = frames;
    run(100);
    CHECK(!A->pn.pv_play && out_peak(f0 + 256, frames) == 0, "B11: the preview stops (peak %d)",
          out_peak(f0 + 256, frames));
    B(11, 40);
    CHECK(A->pn.pv_play == 1, "B11 again: it plays");
    CHECK(fabsf(ec_pn_item_value(EC_UX_FLEN) - 4.0f) < 0.05f && fabsf(ec_pn_item_value(EC_UX_FBPM) - 120.0f) < 0.01f,
          "LENGTH %.2f s, BPM %.1f of the file", ec_pn_item_value(EC_UX_FLEN), ec_pn_item_value(EC_UX_FBPM));
    A->pn.as.preview = 0;                          /* APP SETTINGS' AUTO-PREVIEW off: a move does not */
    turn(FM1P_E_SELECT, 1);
    settle(50);
    CHECK(strstr(A->pn.pv_path, "c_chords") && !strstr(cursor_name(), "c_chords"),
          "auto-preview off: the move does not preview (%s)", A->pn.pv_path);
    A->pn.as.preview = 1;
    turn(FM1P_E_SELECT, -1);
    settle(200);
}

static void test_load(void)
{
    int h, s;
    to(EC_CAT_FILE);
    cursor_to("c_chords_bpm120.wav");
    settle(200);
    h = A->pn.pv_handle;
    s = ec_file_next_free();
    CHECK(s == 1, "the next free slot: 2 (%d)", s + 1);
    B(10, 40);                                     /* B10: LOAD */
    settle(100);
    CHECK(!strcmp(A->proj.slot[1].path, "/audio/c_chords_bpm120.wav") && A->slot[1].state == EC_SL_READY &&
              A->slot[1].handle == h && !A->pn.pv_owned,
          "B10: slot 2 holds it, the preview's sample adopted (no second load) (%d %d)", A->slot[1].handle, h);
    CHECK(ec_app_track_slot(A, A->track) == 1 && strstr(cursor_name(), "c_chords"),
          "the track plays it, the cursor stays put (%s)", cursor_name());
    CHECK(fabsf(A->proj.slot[1].bpm - 120.0f) < 0.01f && A->proj.slot[1].steps == 32, "its BPM / steps (%g, %g)",
          A->proj.slot[1].bpm, A->proj.slot[1].steps);
    turn(FM1P_E_SELECT, 1);                        /* load, scroll, load .. */
    settle(200);
    B(10, 40);
    turn(FM1P_E_SELECT, -2);
    settle(200);
    B(10, 40);
    settle(200);
    CHECK(strstr(A->proj.slot[2].path, "d_perc16") && strstr(A->proj.slot[3].path, "b_bass") &&
              A->slot[2].state == EC_SL_READY && A->slot[3].state == EC_SL_READY && strstr(cursor_name(), "b_bass"),
          "x3: slots 3 and 4 (%s, %s)", A->proj.slot[2].path, A->proj.slot[3].path);
    B(10, 40);
    CHECK(A->slot[4].state == EC_SL_EMPTY && !strcmp(A->msg, "IN SLOT 4"), "LOAD of a pool file: no copy (%s)",
          A->msg);
    CHECK(ec_file_slots_used() == 4 && ec_pn_item_value(EC_UX_FSLOT) == 5.0f, "the pool: 4 used, SLOT 5 next");
    {   /* FULL: the other 124 slots taken (missing files) */
        int k;
        for (k = 4; k < EC_NSLOT; k++) {
            snprintf(A->proj.slot[k].path, EC_PATH, "/audio/gone%03d.wav", k);
            A->slot[k].state = EC_SL_MISSING;
        }
        cursor_to("a_drums_bpm120.wav");
        ec_slot_clear(&A->proj.slot[0]);           /* (slot 1's path forgotten: a_drums is "not in the pool") */
        snprintf(A->proj.slot[0].path, EC_PATH, "/audio/elsewhere.wav");
        settle(200);
        B(10, 40);
        CHECK(!strcmp(A->msg, "FULL") && A->msg_alert, "a full pool: FULL in ALERT (%s)", A->msg);
        CHECK(lbl(FM1CORE_LBL_BLACK0 + 9, "LOAD") && ec_pn_item_value(EC_UX_FSLOT) == 0.0f,
              "SLOT cell: FULL (%g)", ec_pn_item_value(EC_UX_FSLOT));
        snprintf(A->proj.slot[0].path, EC_PATH, "/audio/a_drums_bpm120.wav");
        for (k = 4; k < EC_NSLOT; k++) {
            ec_slot_clear(&A->proj.slot[k]);
            A->slot[k].state = EC_SL_EMPTY;
        }
    }
}

static void test_free_purge(void)
{
    uint32_t live0;
    int16_t first[8];
    int i;
    to(EC_CAT_FILE);
    cursor_to("d_perc16_bpm120.wav");
    settle(200);
    CHECK(ec_file_pooled("/audio/d_perc16_bpm120.wav") == 2, "d_perc16 in slot 3");
    press(FM1P_B_OCTUP, 40);                       /* F2: free its slot */
    CHECK(ec_file_pooled("/audio/d_perc16_bpm120.wav") < 0 && A->slot[2].state == EC_SL_EMPTY &&
              !strcmp(A->msg, "SLOT 3 FREED") && !A->pn.f_sticky,
          "F2 on a pool file: its slot freed (%s)", A->msg);
    press(FM1P_B_OCTUP, 40);
    CHECK(!strcmp(A->msg, "NOT IN THE POOL"), "F2 again: not in the pool (%s)", A->msg);
    /* purge: T1 on slot 2 (chords), slot 1 (drums) locked on a step of pattern A3, slot 4 (bass) used by nothing */
    trk(0)->v[EC_P_SLOT] = 2;
    for (i = 1; i < EC_NTRACK; i++)
        trk(i)->v[EC_P_SLOT] = 2;
    A->proj.pat[2].used = 1;
    for (i = 0; i < EC_NTRACK; i++)
        A->proj.pat[2].tr[i].v[EC_P_SLOT] = 2;
    ec_step_lock_set(&A->proj.pat[2].tr[3].steps[7], EC_P_SLOT, 1);
    for (i = 3; i < EC_NPAT; i++)
        A->proj.pat[i].used = 0;
    A->proj.pat[1].used = 0;
    snprintf(A->proj.slot[60].path, EC_PATH, "/audio/missing.wav");
    A->slot[60].state = EC_SL_MISSING;
    ec_seq_touch_all(&A->seq);
    run(30);
    memcpy(first, ec_app_track_sample(A, 0)->data + 2000, sizeof first);
    live0 = ec_ss_live_bytes();
    CHECK(A->pn.pv_handle >= 0 && A->pn.pv_owned && strstr(A->pn.pv_path, "d_perc16"),
          "slot 3's freed sample stays with the preview (it was previewing it) until the preview moves on");
    shift_down(1);
    B(11, 40);
    shift_down(0);
    CHECK(A->slot[3].state == EC_SL_EMPTY && !A->proj.slot[3].path[0] && !A->proj.slot[60].path[0],
          "SHIFT + B11: slot 4 (unused) and slot 61 freed (%s)", A->msg);
    CHECK(A->slot[0].state == EC_SL_READY && A->slot[1].state == EC_SL_READY,
          "slot 1 (a step lock in A3) and slot 2 (the tracks') kept");
    CHECK(ec_ss_hole_bytes() == 0 && ec_ss_live_bytes() < live0, "the arena compacted (holes %u, live %u < %u)",
          ec_ss_hole_bytes(), ec_ss_live_bytes(), live0);
    run(30);
    CHECK(ec_app_track_sample(A, 0) && !memcmp(ec_app_track_sample(A, 0)->data + 2000, first, sizeof first) &&
              A->assigned[0] == A->slot[1].handle,
          "the samples moved intact, the tracks on them again");
    {
        int cur;
        const char *r;
        shift_down(1);
        CHECK(lbl(FM1CORE_LBL_BLACK0 + 10, "PURGE"), "SHIFT: B11 PURGE (%s)", L(FM1CORE_LBL_BLACK0 + 10));
        shift_down(0);
        r = ec_pn_roles(EC_MODE_SAMPLER, &cur);
        CHECK(!strcmp(r, "AAAAAOOOOOX"), "the SAMPLER's roles (the mock's): %s", r);
    }
}

/* ------------------------------------------------------------------------------------------- the editor --- */
static void test_editor(void)
{
    ec_slot_t *sl;
    to(EC_CAT_FILE);
    A->track = 0;
    B(4, 40);
    CHECK(ec_file_page() == EC_PIC_EDITOR, "B4: the editor");
    sl = &A->proj.slot[ec_app_track_slot(A, 0)];
    CHECK(lbl(FM1CORE_LBL_BLACK0 + 0, "<") && lbl(FM1CORE_LBL_BLACK0 + 3, "4X") && lbl(FM1CORE_LBL_BLACK0 + 10, ">"),
          "the editor's black keys: %s %s %s", L(FM1CORE_LBL_BLACK0), L(FM1CORE_LBL_BLACK0 + 3),
          L(FM1CORE_LBL_BLACK0 + 10));
    B(4, 40);                                      /* B4: 4X */
    CHECK(ec_file_zoom() == 4.0f && fabsf(A->pn.ed_view - 0.375f) < 1e-4f, "B4: 4X, round the middle (%g %g)",
          ec_file_zoom(), A->pn.ed_view);
    B(11, 40);                                     /* B11: right half a view */
    CHECK(fabsf(A->pn.ed_view - 0.5f) < 1e-4f, "B11: scrolled right (%g)", A->pn.ed_view);
    B(11, 40);
    B(11, 40);
    CHECK(fabsf(A->pn.ed_view - 0.75f) < 1e-4f, "B11: stops at the end (%g)", A->pn.ed_view);
    B(1, 40);
    CHECK(fabsf(A->pn.ed_view - 0.625f) < 1e-4f, "B1: scrolled left (%g)", A->pn.ed_view);
    /* 0.6.1: F1 / F2 pick the point the keys place (START / END); a tap places it */
    CHECK(A->pn.ed_point == 0 && lbl(FM1CORE_LBL_BTN0 + FM1P_B_OCTDN, "START") &&
              lbl(FM1CORE_LBL_BTN0 + FM1P_B_OCTUP, "END") && lbl(FM1CORE_LBL_WHITE0 + 4, "START"),
          "the editor: START by default, F1 START / F2 END (%s %s)", L(FM1CORE_LBL_BTN0 + FM1P_B_OCTDN),
          L(FM1CORE_LBL_BTN0 + FM1P_B_OCTUP));
    W(1, 40);                                      /* START: the view's left edge */
    CHECK(fabsf(sl->trim_start - 0.625f) < 1e-4f && fabsf(sl->trim_end - 1.0f) < 1e-4f && A->pn.ed_point == 0,
          "START: W1 places the start at the view's edge (%g %g)", sl->trim_start, sl->trim_end);
    W(1, 40);
    CHECK(fabsf(sl->trim_start - 0.625f) < 1e-4f && A->pn.ed_point == 0, "START: a second tap places START again");
    press(FM1P_B_OCTUP, 40);                       /* F2: END */
    CHECK(A->pn.ed_point == 1 && !A->pn.f_sticky && lbl(FM1CORE_LBL_WHITE0 + 4, "END"),
          "F2: the keys place END (no CUT) (%d %d)", A->pn.ed_point, A->pn.f_sticky);
    W(16, 40);                                     /* END: the view's right edge */
    CHECK(fabsf(sl->trim_end - 0.875f) < 1e-4f && fabsf(sl->trim_start - 0.625f) < 1e-4f,
          "END: W16 places the end (%g)", sl->trim_end);
    CHECK(sl->steps == 8, "the steps follow the trim (%g)", sl->steps);
    CHECK(led_key(0, 0) == FM1P_LED_LIT && led_key(0, 15) == FM1P_LED_LIT && led_key(0, 7) == FM1P_LED_FAINT,
          "the white keys at the trim lit");
    B(2, 40);                                      /* B2: 1X */
    CHECK(ec_file_zoom() == 1.0f && A->pn.ed_view == 0.0f, "B2: 1X");
    press(FM1P_B_OCTDN, 40);                       /* F1: START */
    W(4, 40);
    press(FM1P_B_OCTUP, 40);
    W(13, 40);
    CHECK(fabsf(sl->trim_start - 0.2f) < 1e-4f && fabsf(sl->trim_end - 0.8f) < 1e-4f && !A->pn.f_sticky,
          "1X: START W4, END W13 = 0.2 / 0.8");
    W(2, 40);                                      /* END before START: they swap */
    CHECK(fabsf(sl->trim_start - 1.0f / 15.0f) < 1e-4f && fabsf(sl->trim_end - 0.2f) < 1e-4f,
          "END at W2, before START: swapped (%g %g)", sl->trim_start, sl->trim_end);
    press(FM1P_B_OCTDN, 40);
    W(16, 40);                                     /* START after END: they swap */
    CHECK(fabsf(sl->trim_start - 0.2f) < 1e-4f && fabsf(sl->trim_end - 1.0f) < 1e-4f,
          "START at W16, after END: swapped (%g %g)", sl->trim_start, sl->trim_end);
    /* two keys: hold one (the chosen point), a second places the other */
    key(0, 4, 1);                                  /* START: W5 held = the start */
    run(350);                                      /* (0.6.14: the header text past 300 ms) */
    {
        char h[40];
        CHECK(ec_hdr_held(h, sizeof h) >= 0 && !strcmp(h, "START + KEY = END"), "W5 held: the header says (%s)", h);
    }
    W(9, 40);                                      /*   W9 = the end */
    key(0, 4, 0);
    run(10);
    CHECK(fabsf(sl->trim_start - 4.0f / 15.0f) < 1e-4f && fabsf(sl->trim_end - 8.0f / 15.0f) < 1e-4f &&
              A->pn.ed_hold < 0,
          "START: W5 held + W9 = start / end (%g %g)", sl->trim_start, sl->trim_end);
    press(FM1P_B_OCTUP, 40);                       /* END: W12 held = the end, W3 = the start */
    key(0, 11, 1);
    run(20);
    W(3, 40);
    key(0, 11, 0);
    run(10);
    CHECK(fabsf(sl->trim_start - 2.0f / 15.0f) < 1e-4f && fabsf(sl->trim_end - 11.0f / 15.0f) < 1e-4f,
          "END: W12 held + W3 = end / start (%g %g)", sl->trim_start, sl->trim_end);
    press(FM1P_B_OCTDN, 40);                       /* START: W12 held = start (after END: swapped), W3 = end */
    key(0, 11, 1);
    run(20);
    W(3, 40);
    key(0, 11, 0);
    run(10);
    CHECK(fabsf(sl->trim_start - 2.0f / 15.0f) < 1e-4f && fabsf(sl->trim_end - 11.0f / 15.0f) < 1e-4f,
          "START: W12 held + W3 (the end before the start): swapped (%g %g)", sl->trim_start, sl->trim_end);
    key(0, 0, 1);                                  /* one more gesture, then UNDO takes it back whole */
    run(20);
    W(16, 40);
    key(0, 0, 0);
    run(10);
    CHECK(sl->trim_start == 0.0f && sl->trim_end == 1.0f, "W1 held + W16: all of it");
    shift_down(1);
    press(FM1P_B_OCTDN, 40);
    shift_down(0);
    CHECK(fabsf(sl->trim_start - 2.0f / 15.0f) < 1e-4f && fabsf(sl->trim_end - 11.0f / 15.0f) < 1e-4f,
          "UNDO: the two-key gesture undone as one (%g %g, %s)", sl->trim_start, sl->trim_end, A->msg);
    shift_down(1);
    press(FM1P_B_OCTDN, 40);
    shift_down(0);
    CHECK(fabsf(sl->trim_start - 2.0f / 15.0f) < 1e-4f && fabsf(sl->trim_end - 11.0f / 15.0f) < 1e-4f,
          "UNDO again: the gesture before (the same points, swapped) (%g %g)", sl->trim_start, sl->trim_end);
    shift_down(1);
    press(FM1P_B_OCTDN, 40);
    shift_down(0);
    CHECK(fabsf(sl->trim_start - 4.0f / 15.0f) < 1e-4f && fabsf(sl->trim_end - 8.0f / 15.0f) < 1e-4f,
          "UNDO: the END gesture's before (%g %g)", sl->trim_start, sl->trim_end);
    {
        int cur;
        const char *r = ec_pn_roles(EC_CAT_FILE, &cur);
        CHECK(!strcmp(r, "APPPPPPPPPA") && cur == 1, "the editor's roles (the mock's): %s %d", r, cur);
    }
    sl->trim_start = 0, sl->trim_end = 1;
    press(FM1P_B_OCTDN, 40);
    ec_seq_touch_all(&A->seq);
    B(3, 40);
    CHECK(ec_file_page() == EC_PIC_EDITOR && ec_file_zoom() == 2.0f, "editor: B3 = 2X (no page key here)");
    turn(FM1P_E_SELECT, -1);
    CHECK(ec_file_page() == EC_PIC_FILE, "SEL: back to the browser");
}

/* ------------------------------------------------------------------------------------------- FILE's pages --- */
/* 0.6.1: SEL turns the page on EDITOR and RAZOR (lanes, then pages) both ways; on BROWSE / POOL it is the cursor and
 * the page keys B3 B4 B5 B6 leave; RAZOR's slice is ALGO's */
static void test_file_pages(void)
{
    const int t = A->track;
    int sel0, cur0;
    to(EC_CAT_FILE);
    B(3, 40);
    CHECK(ec_file_page() == EC_PIC_FILE && lbl(FM1CORE_LBL_BLACK0 + 5, "POOL") &&
              lbl(FM1CORE_LBL_BLACK0 + 3, "EDITOR") && lbl(FM1CORE_LBL_BLACK0 + 4, "RAZOR"),
          "BROWSE: B4 EDITOR, B5 RAZOR, B6 POOL (%s)", L(FM1CORE_LBL_BLACK0 + 5));
    B(6, 40);
    CHECK(ec_file_page() == EC_PIC_POOL, "BROWSE: B6 leaves for POOL");
    B(3, 40);
    CHECK(ec_file_page() == EC_PIC_FILE, "POOL: B3 leaves for BROWSE");
    B(6, 40);
    B(5, 40);
    CHECK(ec_file_page() == EC_PIC_RAZOR, "POOL: B5 leaves for RAZOR");
    B(6, 40);
    B(4, 40);
    CHECK(ec_file_page() == EC_PIC_EDITOR, "POOL: B4 leaves for EDITOR");
    turn(FM1P_E_SELECT, 1);                        /* EDITOR's second lane, then RAZOR */
    turn(FM1P_E_SELECT, 1);
    CHECK(ec_file_page() == EC_PIC_RAZOR && ec_rzui_active(A), "EDITOR: SEL on to RAZOR");
    trk(t)->v[EC_P_SLICE_COUNT] = 16;
    ec_rzui.sel = 3;
    sel0 = ec_rzui.sel;
    turn(FM1P_E_SELECT, -1);
    CHECK(ec_file_page() == EC_PIC_EDITOR && ec_rzui.sel == sel0, "RAZOR: SEL back to EDITOR (the slice stays)");
    turn(FM1P_E_SELECT, 1);
    CHECK(ec_file_page() == EC_PIC_RAZOR, "EDITOR: SEL to RAZOR again");
    CHECK(lbl(FM1CORE_LBL_KNOB0 + FM1P_E_ALGO, "SLICE"), "RAZOR: ALGO's label SLICE (%s)",
          L(FM1CORE_LBL_KNOB0 + FM1P_E_ALGO));
    turn(FM1P_E_ALGO, 1);
    CHECK(ec_rzui.sel == sel0 + 1 && ec_file_page() == EC_PIC_RAZOR, "RAZOR: ALGO picks the next slice (%d)",
          ec_rzui.sel);
    turn(FM1P_E_ALGO, -2);
    CHECK(ec_rzui.sel == sel0 - 1, "RAZOR: ALGO back two (%d)", ec_rzui.sel);
    turn(FM1P_E_SELECT, 1);                        /* RAZOR's second lane, then POOL */
    turn(FM1P_E_SELECT, 1);
    CHECK(ec_file_page() == EC_PIC_POOL, "RAZOR: SEL on to POOL");
    cur0 = A->pn.pool_cur;
    turn(FM1P_E_SELECT, 1);
    CHECK(ec_file_page() == EC_PIC_POOL && A->pn.pool_cur == cur0 + 1, "POOL: SEL scrolls the slots (%d)",
          A->pn.pool_cur);
    turn(FM1P_E_SELECT, -1);
    B(3, 40);
}

/* ------------------------------------------------------------------------------------------- POOL --- */
static void test_pool(void)
{
    uint8_t tracks[EC_NSLOT], locks[EC_NSLOT];
    const int h0 = A->slot[0].handle, t = A->track;
    char drums[EC_PATH];
    int i;
    to(EC_CAT_FILE);
    A->pn.as.preview = 1;
    B(6, 40);
    CHECK(ec_file_page() == EC_PIC_POOL && lbl(FM1CORE_LBL_BLACK0 + 9, "ASSIGN") &&
              lbl(FM1CORE_LBL_BLACK0 + 10, "PREVIEW") && lbl(FM1CORE_LBL_BTN0 + FM1P_B_OCTDN, "MOVE") &&
              lbl(FM1CORE_LBL_BTN0 + FM1P_B_OCTUP, "FREE") && lbl(FM1CORE_LBL_KNOB0 + FM1P_E_SELECT, "SLOT"),
          "B6: POOL, B10 ASSIGN, B11 PREVIEW, F1 MOVE, F2 FREE, SEL SLOT");
    {
        int cur;
        const char *r = ec_pn_roles(EC_CAT_FILE, &cur);
        CHECK(r[9] == 'A' && r[10] == 'X' && r[5] == 'P' && cur == 5, "POOL's roles: %s %d", r, cur);
    }
    turn(FM1P_E_SELECT, -A->pn.pool_cur);
    snprintf(drums, sizeof drums, "%s", A->proj.slot[0].path);
    /* the list: slot 2 played by every track, slot 1 only by a step lock (pattern A3), slot 5 empty */
    ec_file_pool_scan(tracks, locks);
    CHECK(tracks[1] == 0x3F && !tracks[0] && locks[0] && !tracks[4] && !locks[4] && !A->proj.slot[4].path[0],
          "POOL lists slot 2 on T1-T6, slot 1 lock-only, slot 5 empty (%x %x %d)", tracks[1], tracks[0], locks[0]);
    turn(FM1P_E_SELECT, 1);                        /* onto slot 2 (used): auto-preview */
    run(20);
    CHECK(A->pn.pool_cur == 1 && A->pn.pv_play && A->pn.pv_handle == A->slot[1].handle,
          "SEL onto slot 2: it previews (%d %d)", A->pn.pv_play, A->pn.pv_handle);
    B(11, 40);
    CHECK(!A->pn.pv_play, "B11: the preview stops");
    B(11, 40);
    CHECK(A->pn.pv_play, "B11: it plays again");
    turn(FM1P_E_SELECT, 3);                        /* slot 5: empty, the preview stops */
    CHECK(A->pn.pool_cur == 4 && !A->pn.pv_play, "SEL onto an empty slot: silence");
    B(10, 40);
    CHECK(!strcmp(A->msg, "EMPTY SLOT") && ec_app_track_slot(A, t) == 1, "ASSIGN on an empty slot: refused (%s)",
          A->msg);
    turn(FM1P_E_SELECT, -4);                       /* slot 1: ASSIGN */
    B(10, 40);
    CHECK(ec_app_track_slot(A, t) == 0 && A->assigned[t] == h0, "B10 ASSIGN: T%d plays slot 1 (%s)", t + 1, A->msg);
    /* MOVE: slot 1 (T1's, and a step lock in pattern A3, not the current one) to slot 10 */
    CHECK(A->proj.current != 2 && ec_step_lock_get(&A->proj.pat[2].tr[3].steps[7], EC_P_SLOT, &(float){0}),
          "(a slot-1 lock in A3, which is not playing)");
    press(FM1P_B_OCTDN, 40);
    CHECK(A->pn.pool_carry == 0 && lbl(FM1CORE_LBL_BTN0 + FM1P_B_OCTDN, "DROP"), "F1: slot 1 picked up (MOVE 1)");
    turn(FM1P_E_SELECT, 9);
    press(FM1P_B_OCTDN, 40);
    {
        float lk = 0;
        ec_step_lock_get(&A->proj.pat[2].tr[3].steps[7], EC_P_SLOT, &lk);
        CHECK(A->pn.pool_carry < 0 && !strcmp(A->proj.slot[9].path, drums) && !A->proj.slot[0].path[0] &&
                  A->slot[9].handle == h0 && A->slot[0].state == EC_SL_EMPTY,
              "F1 on slot 10: slot 1 moved there (%s)", A->msg);
        CHECK(ec_app_track_slot(A, t) == 9 && A->assigned[t] == h0, "the track follows: slot 10, the same sample");
        CHECK((int)lk == 10, "the step lock in A3 follows: slot 10 (%g)", lk);
        CHECK(A->proj.pat[2].tr[0].v[EC_P_SLOT] == 2.0f, "A3's tracks on slot 2 untouched");
    }
    shift_down(1);                                 /* UNDO: one step back */
    press(FM1P_B_OCTDN, 40);
    shift_down(0);
    {
        float lk = 0;
        ec_step_lock_get(&A->proj.pat[2].tr[3].steps[7], EC_P_SLOT, &lk);
        CHECK(!strcmp(A->proj.slot[0].path, drums) && !A->proj.slot[9].path[0] && A->slot[0].handle == h0 &&
                  ec_app_track_slot(A, t) == 0 && (int)lk == 1 && A->assigned[t] == h0,
              "UNDO: MOVE undone (slot 1, the track, the lock) (%s)", A->msg);
    }
    /* FREE: a slot whose file went missing */
    snprintf(A->proj.slot[20].path, EC_PATH, "/audio/gone.wav");
    A->slot[20].state = EC_SL_MISSING;
    turn(FM1P_E_SELECT, 20 - A->pn.pool_cur);
    press(FM1P_B_OCTUP, 40);
    CHECK(!A->proj.slot[20].path[0] && A->slot[20].state == EC_SL_EMPTY && !strcmp(A->msg, "SLOT 21 FREED"),
          "F2: FREE slot 21 (%s)", A->msg);
    press(FM1P_B_OCTUP, 40);
    CHECK(!strcmp(A->msg, "EMPTY SLOT"), "F2 on an empty slot (%s)", A->msg);
    shift_down(1);                                 /* SHIFT + B11: purge, as in BROWSE */
    B(11, 40);
    shift_down(0);
    CHECK(!strncmp(A->msg, "PURGED", 6) && A->slot[0].state == EC_SL_READY && A->slot[1].state == EC_SL_READY,
          "SHIFT + B11: purge (%s)", A->msg);
    /* a slot in hand is put down when POOL is left */
    turn(FM1P_E_SELECT, -A->pn.pool_cur);
    press(FM1P_B_OCTDN, 40);
    B(3, 40);
    CHECK(A->pn.pool_carry < 0 && ec_file_page() == EC_PIC_FILE, "B3 with a slot in hand: BROWSE, MOVE dropped");
    /* 0.6.11: POOL opens while playing; MOVE and FREE do not */
    to(EC_CAT_SRC);
    press(FM1P_B_PLAY, 40);
    press(FM1P_B_SAVE, 40);
    B(6, 40);
    CHECK(mode() == EC_CAT_FILE && ec_file_page() == EC_PIC_POOL && A->seq.playing, "playing: POOL opens (%d)", mode());
    press(FM1P_B_OCTDN, 40);
    CHECK(A->pn.pool_carry < 0 && !strcmp(A->msg, "STOP FIRST"), "MOVE while playing: STOP FIRST");
    press(FM1P_B_OCTUP, 40);
    CHECK(!strcmp(A->msg, "STOP FIRST"), "FREE while playing: STOP FIRST");
    press(FM1P_B_PLAY, 40);
    for (i = 0; i < 2 && mode() != EC_CAT_FILE; i++)
        to(EC_CAT_FILE);
}

/* ------------------------------------------------------------------------------------------- SAMPLER --- */
static void wait_landed(int takes)
{
    int i;
    for (i = 0; i < 800 && A->rec.takes < (uint32_t)takes; i++)
        run(10);
    for (i = 0; i < 400 && A->rec.w; i++)
        run(10);
}

static void test_sampler(void)
{
    int s;
    char p[512];
    FILE *f;
    fm1core_audio_in_info(1, 0);
    in_tone = 0.5f;
    to(EC_CAT_FILE);
    to(EC_MODE_SAMPLER);
    CHECK(mode() == EC_MODE_SAMPLER && !A->rec.open, "FILE again: the SAMPLER");
    run(200);
    CHECK(led_key(0, 0) != FM1P_LED_OFF && led_key(0, 12) != FM1P_LED_OFF && led_key(0, 15) == FM1P_LED_OFF,
          "the white keys: the input's VU (-6 dB: 14 of 16)");
    B(2, 40);                                      /* B2: 1 bar */
    CHECK(ec_sampler_len() == 1 && lbl(FM1CORE_LBL_BLACK0 + 1, "1 BAR"), "B2: 1 BAR");
    s = ec_file_next_free();
    B(11, 40);                                     /* stopped: at once */
    CHECK(A->rec.state == EC_RS_RUN && A->rec.slot == s, "B11 stopped: taking at once, into slot %d (%d)", s + 1,
          A->rec.state);
    CHECK(lbl(FM1CORE_LBL_BLACK0 + 10, "STOP"), "B11's label: STOP (%s)", L(FM1CORE_LBL_BLACK0 + 10));
    wait_landed(1);
    CHECK(A->rec.takes == 1 && A->slot[s].state == EC_SL_READY && A->rec.last_frames == 88200,
          "1 bar later: the take in slot %d (%u frames)", s + 1, A->rec.last_frames);
    CHECK(fabsf(A->proj.slot[s].bpm - 120.0f) < 0.01f && strstr(A->proj.slot[s].path, "/data/rec/rec-"),
          "tagged with the BPM, saved to /data/rec (%g %s)", A->proj.slot[s].bpm, A->proj.slot[s].path);
    snprintf(p, sizeof p, "%s/data/elasticat/rec/%s", root, strrchr(A->proj.slot[s].path, '/') + 1);
    f = fopen(p, "rb");
    CHECK(f != NULL, "%s written", p);
    if (f)
        fclose(f);
    CHECK(ec_app_track_slot(A, A->track) != s, "the track's slot unchanged");
    /* playing: armed to the next bar, B11 again ends it at its bar's end */
    B(1, 40);                                      /* B1: FREE */
    press(FM1P_B_PLAY, 40);
    run(700);
    s = ec_file_next_free();
    B(11, 40);
    CHECK(A->rec.state == EC_RS_ARMED, "B11 playing: armed (waits for the bar)");
    run(1500);
    CHECK(A->rec.state == EC_RS_RUN && A->rec.start_at == A->seq.sample_base + 88200,
          "the take started on the bar line (%llu, want %llu)", (unsigned long long)A->rec.start_at,
          (unsigned long long)(A->seq.sample_base + 88200));
    run(2700);                                     /* into the second bar */
    B(11, 40);
    CHECK(A->rec.state == EC_RS_RUN && A->rec.target == 2u * 88200u, "B11 again: it ends at the bar's end (%u)",
          A->rec.target);
    wait_landed(2);
    CHECK(A->rec.takes == 2 && A->rec.last_frames == 2u * 88200u && A->slot[s].state == EC_SL_READY,
          "two whole bars in slot %d (%u)", s + 1, A->rec.last_frames);
    press(FM1P_B_PLAY, 40);
    in_tone = 0;
}

/* ------------------------------------------------------------------------------------------- slot locks --- */
static void test_slot_locks(void)
{
    const int h1 = A->slot[0].handle, h2 = A->slot[1].handle;
    to(EC_CAT_TRIG);
    A->track = 0;
    trk(0)->v[EC_P_SLOT] = 1;
    trk(0)->v[EC_P_MACHINE] = EC_MACH_LOOP_TRIG;
    clear_steps(0);
    step(0)->trig = 1;
    ec_step_lock_set(step(0), EC_P_SLOT, 2);       /* step 1 plays slot 2 */
    step(8)->trig = 1;                             /* step 9: its own */
    ec_seq_touch_all(&A->seq);
    run(50);
    CHECK(h1 >= 0 && h2 >= 0 && h1 != h2 && A->assigned[0] == h1, "stopped: T1 plays slot 1");
    press(FM1P_B_PLAY, 40);
    run(200);
    CHECK(ec_seq_track_slot(&A->seq, 0) == 2 && A->assigned[0] == h2, "step 1's trig: slot 2's sample (%d)",
          ec_seq_track_slot(&A->seq, 0));
    run(900);
    CHECK(ec_seq_track_slot(&A->seq, 0) == 1 && A->assigned[0] == h1, "step 9's trig: the track's slot 1 again");
    run(1000);
    CHECK(ec_seq_track_slot(&A->seq, 0) == 2 && A->assigned[0] == h2, "the next pass: slot 2 again");
    press(FM1P_B_PLAY, 40);
    run(30);
    CHECK(ec_seq_track_slot(&A->seq, 0) == 1 && A->assigned[0] == h1, "stopped: the track's own");
    CHECK(EC_PARAMS[EC_P_SLOT].flags & EC_PF_LOCKABLE, "EC_P_SLOT is lockable");
    trk(0)->v[EC_P_MACHINE] = EC_MACH_LOOP;
    clear_steps(0);
}

/* ------------------------------------------------------------------------------------------- 4X --- */
static void test_rate4x(void)
{
    to(EC_CAT_MASTER);
    A->track = 0;
    A->bus = 0;
    B(8, 40);
    CHECK((int)trk(0)->v[EC_P_RATE] == 22 && !strcmp(ec_rate_label(22), "4") &&
              fabs(ec_seq_step_samples(&A->seq, 0) - 44100.0 * 0.5 / 16.0) < 1e-6,
          "MIX B8: 4X (rate %g, a step %g frames)", trk(0)->v[EC_P_RATE], ec_seq_step_samples(&A->seq, 0));
    CHECK(lbl(FM1CORE_LBL_BLACK0 + 7, "4X") && led_key(1, 7) == FM1P_LED_LIT, "B8's label 4X, lit");
    B(5, 40);
    CHECK((int)trk(0)->v[EC_P_RATE] == EC_RATE_UNITY, "B5: 1X");
}

/* ------------------------------------------------------------------------------------------- PATTERN LAUNCH, FULL --- */
static void test_patterns_full(void)
{
    ec_page_t pg;
    uint32_t size;
    int p, t, i, k, saves;
    ec_project_t *chk = malloc(sizeof *chk);
    to(EC_CAT_PAT);
    ec_app_page(A, &pg);
    CHECK(pg.n == 4 && !strcmp(pg.it[0].label, "PLAYING") && !strcmp(pg.it[1].label, "NEXT") &&
              !strcmp(pg.it[2].label, "QUANT") && !strcmp(pg.it[3].label, "LENGTH"),
          "PATTERN LAUNCH's cells: PLAYING NEXT QUANT LENGTH (the mock's)");
    /* 0.6.1: a project bigger than the old 1 MiB record: no FULL, it saves (projects are files, host/test_proj.c) */
    shift_down(1);
    press(FM1P_B_SAVE, 40);
    shift_down(0);
    saves = A->saves;
    CHECK(!strcmp(A->msg, "SAVED"), "SHIFT + FILE: saved (%s)", A->msg);
    for (p = 0; p < EC_NPAT; p++) {
        A->proj.pat[p].used = 1;
        for (t = 0; t < EC_NTRACK; t++)
            for (i = 0; i < EC_MAXSTEPS; i++) {
                ec_step_t *s = &A->proj.pat[p].tr[t].steps[i];
                s->trig = 1;
                for (k = 0; k < EC_STEP_LOCKS; k++)
                    ec_step_lock_set(s, EC_P_FILTER_CUTOFF + k, 0.123f + (float)(i + k) * 0.001f);
            }
    }
    shift_down(1);
    press(FM1P_B_SAVE, 40);
    shift_down(0);
    CHECK(!A->msg_alert && !strcmp(A->msg, "SAVING"), "big: SAVING, no FULL (%s)", A->msg);
    for (i = 0; i < 400 && A->saves == saves; i++)
        run(5);
    CHECK(A->saves == saves + 1 && !strcmp(A->msg, "SAVED") && ec_pf_pack(&size) && size > EC_PLAT_RECORD_MAX,
          "big: SAVED (%u bytes > the old 1 MiB record)", size);
    CHECK(chk && ec_project_load(chk, ec_pf_pack(0), size) == 0 && chk->pat[EC_NPAT - 1].used, "the big one packed");
    for (p = 0; p < EC_NPAT; p++)
        for (t = 0; t < EC_NTRACK; t++)
            for (i = 0; i < EC_MAXSTEPS; i++)
                ec_step_clear(&A->proj.pat[p].tr[t].steps[i]);
    free(chk);
}

/* RAZOR (B5): the white keys are its slices (select / pad), SHIFT + one mutes the slice; never steps */
static void test_razor_keys(void)
{
    ec_razor_t *rz = &A->proj.razor[A->track];
    const int t = A->track, m0 = rz->mute[1];
    to(EC_CAT_FILE);
    B(5, 40);
    CHECK(ec_rzui_active(A), "B5: RAZOR");
    clear_steps(t);
    W(2, 40);
    CHECK(!ec_step_has_content(step(A->step_page * 16 + 1)) && A->track == t, "RAZOR: W2 is a slice, not a step");
    shift_down(1);
    W(2, 40);
    shift_down(0);
    CHECK(rz->mute[1] == !m0 && A->track == t, "RAZOR: SHIFT + W2 mutes slice 2, the track stays");
    shift_down(1);
    W(2, 40);
    shift_down(0);
    B(3, 40);
}

int main(int argc, char **argv)
{
    char p[1024];
    if (argc < 2) {
        printf("usage: ec_test_file <folder>\n");
        return 2;
    }
    root = argv[1];
    snprintf(p, sizeof p, "mkdir -p %s/audio/breaks/deep && rm -f %s/data/elasticat/rec/rec-*.wav", root, root);
    if (system(p) != 0)
        return 2;
    snprintf(p, sizeof p, "%s/audio/breaks/think_bpm96.wav", root);
    write_wav(p, 88200, 220);
    snprintf(p, sizeof p, "%s/audio/breaks/deep/hit.wav", root);
    write_wav(p, 22050, 880);
    ec_fs_native_root(root, "elasticat");
    boot(NULL, 0);
    test_gating();
    test_folders();
    test_preview();
    test_load();
    test_free_purge();
    test_editor();
    test_file_pages();
    test_pool();
    test_razor_keys();
    test_sampler();
    test_slot_locks();
    test_rate4x();
    test_patterns_full();
    C->shutdown();
    printf("test_file: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
