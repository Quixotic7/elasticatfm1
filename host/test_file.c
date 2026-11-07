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
#include <math.h>
#include "ec_scene.h"
#include "ec_mtx.h"
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
    B(9, 40);                                      /* 0.6.42: FREE is B9 (OCT+ opens) */
    CHECK(!strcmp(A->msg, "STOP FIRST"), "B9 (FREE) while playing: refused (%s)", A->msg);
    press(FM1P_B_SAVE, 40);
    CHECK(mode() == EC_MODE_PROJECTS, "FILE again: PROJECTS (0.6.42; %d)", mode());
    press(FM1P_B_SAVE, 40);
    CHECK(mode() == EC_MODE_SAMPLER, "FILE again, playing: the SAMPLER, past the editors (%d)", mode());
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
    press(FM1P_B_SAVE, 40);
    press(FM1P_B_SAVE, 40);
    CHECK(mode() == EC_CAT_FILE && ec_file_page() == EC_PIC_EDITOR,
          "stopped: FILE again (past PROJECTS) opens the EDITOR (0.6.15)");
    press(FM1P_B_PLAY, 40);
    CHECK(!A->seq.playing && ec_file_page() == EC_PIC_FILE, "PLAY on the EDITOR: back to BROWSE, not playing (%s)",
          A->msg);
    press(FM1P_B_PLAY, 40);
    CHECK(A->seq.playing, "PLAY again: playing from BROWSE");
    press(FM1P_B_PLAY, 40);
    press(FM1P_B_REC, 40);
    press(FM1P_B_REC, 500);
    CHECK(!ec_liverec_on() && !A->pn.rs_open, "inside FILE REC does nothing (tap, hold)");
    press(FM1P_B_SAVE, 40);
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
    {
        int d, f;
        ec_file_entry(0, &d, &f);
        CHECK(ec_file_count() == 3 && !strcmp(ec_file_entry(0, 0, 0), "UP") && d && f == EC_FILE_UP &&
                  !strcmp(ec_file_entry(1, 0, 0), "deep/") && !strcmp(ec_file_entry(2, 0, 0), "think_bpm96.wav"),
              "breaks (0.6.42): UP, deep/, think (%d: %s)", ec_file_count(), ec_file_entry(0, 0, 0));
    }
    turn(FM1P_E_SELECT, 1);                        /* past UP, on deep/ */
    B(2, 40);
    CHECK(!strcmp(A->pn.fdir, "/audio/breaks/deep") && ec_file_count() == 2, "B2 again: the subfolder's subfolder");
    turn(FM1P_E_SELECT, 1);                        /* (past UP) onto its file */
    settle(100);
    snprintf(want, sizeof want, "/audio/breaks/deep/hit.wav");
    CHECK(!strcmp(A->pn.pv_path, want) && A->pn.pv_state == EC_PV_READY, "the move into it previews its file (%s %d)",
          A->pn.pv_path, A->pn.pv_state);
    B(1, 40);                                      /* B1: up, the cursor on where we were */
    CHECK(!strcmp(A->pn.fdir, "/audio/breaks") && !strcmp(cursor_name(), "deep/"), "B1: up, on deep/ (%s %s)",
          A->pn.fdir, cursor_name());
    B(1, 40);
    CHECK(!strcmp(A->pn.fdir, "/audio") && !strcmp(cursor_name(), "breaks/"), "B1: up to /audio, on breaks/");
    B(1, 40);                                      /* 0.6.42 (owner): AUDIO/ is the top: nothing above it */
    CHECK(!strcmp(A->pn.fdir, "/audio") && !strcmp(A->msg, "AUDIO") && ec_file_count() == 5,
          "B1 at /audio: stays, AUDIO is the top (%s %s %d)", A->pn.fdir, A->msg, ec_file_count());
    B(2, 40);                                      /* OCT- up and OCT+ open, as B1 / B2; OCT+ on UP goes up */
    press(FM1P_B_OCTDN, 40);
    CHECK(!strcmp(A->pn.fdir, "/audio") && !strcmp(cursor_name(), "breaks/"), "OCT-: up, on breaks/ (%s)", A->pn.fdir);
    press(FM1P_B_OCTUP, 40);
    CHECK(!strcmp(A->pn.fdir, "/audio/breaks") && A->cursor == 0, "OCT+ on breaks/: into it (%s)", A->pn.fdir);
    press(FM1P_B_OCTUP, 40);
    CHECK(!strcmp(A->pn.fdir, "/audio") && !strcmp(cursor_name(), "breaks/"), "OCT+ on UP: up (%s)", A->pn.fdir);
    turn(FM1P_E_SELECT, -20);
    CHECK(lbl(FM1CORE_LBL_BLACK0 + 0, "UP") && lbl(FM1CORE_LBL_BLACK0 + 1, "INTO") &&
              lbl(FM1CORE_LBL_BLACK0 + 9, "ADD") && lbl(FM1CORE_LBL_BLACK0 + 8, "REMOVE") &&
              lbl(FM1CORE_LBL_BLACK0 + 10, "+/- POOL") && lbl(FM1CORE_LBL_BLACK0 + 3, "") &&
              lbl(FM1CORE_LBL_KNOB0 + FM1P_E_ALGO, "+/- POOL") && lbl(FM1CORE_LBL_KNOB0 + FM1P_E_PRESETS, "PREVIEW"),
          "the browser's labels: B1 %s B2 %s B9 %s B10 %s B11 %s; ALGO %s PRESETS %s", L(FM1CORE_LBL_BLACK0),
          L(FM1CORE_LBL_BLACK0 + 1), L(FM1CORE_LBL_BLACK0 + 8), L(FM1CORE_LBL_BLACK0 + 9), L(FM1CORE_LBL_BLACK0 + 10),
          L(FM1CORE_LBL_KNOB0 + FM1P_E_ALGO), L(FM1CORE_LBL_KNOB0 + FM1P_E_PRESETS));
    {
        int cur;
        const char *r = ec_pn_roles(EC_CAT_FILE, &cur);
        CHECK(!strcmp(r, "AAOOOPPOAAX"), "the browser's roles (0.6.15: B6 POOL, B7 BROWSE, B9 / B10 remove / add): %s", r);
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
    turn(FM1P_E_PRESETS, 1);                       /* PRESETS: the preview off (0.6.44: B11 is the pool) */
    f0 = frames;
    run(100);
    CHECK(!A->pn.pv_play && out_peak(f0 + 256, frames) == 0, "PRESETS: the preview stops (peak %d)",
          out_peak(f0 + 256, frames));
    {   /* 0.6.44 (owner): B11 held = the preview while held; a click = in / out of the pool (as OCT+) */
        const char *path = "/audio/c_chords_bpm120.wav";
        const int was = ec_file_pooled(path);
        key(1, 10, 1);
        run(400);
        CHECK(A->pn.pv_play == 1 && ec_file_pooled(path) == was, "B11 held: it previews, the pool as it was");
        key(1, 10, 0);
        run(20);
        CHECK(!A->pn.pv_play && ec_file_pooled(path) == was, "B11 let go: silence, the pool as it was");
        B(11, 40);
        settle(100);
        CHECK((ec_file_pooled(path) >= 0) != (was >= 0), "B11 click: in / out of the pool (%d -> %d)", was,
              ec_file_pooled(path));
        press(FM1P_B_OCTUP, 40);
        settle(100);
        CHECK((ec_file_pooled(path) >= 0) == (was >= 0), "OCT+: back as it was (%d)", ec_file_pooled(path));
    }
    turn(FM1P_E_PRESETS, 1);
    CHECK(A->pn.as.preview == 1, "PRESETS again: AUTO-PREVIEW on");
    B(11, 40);                                     /* (in / out once more: as it was) */
    settle(100);
    B(11, 40);
    settle(100);
    key(1, 10, 1);                                 /* (playing again, for the checks below) */
    run(400);
    CHECK(A->pn.pv_play == 1, "B11 held: it plays");
    key(1, 10, 0);
    turn(FM1P_E_SELECT, 1);
    turn(FM1P_E_SELECT, -1);
    settle(200);
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
    CHECK(ec_app_track_slot(A, A->track) == 0 && strstr(cursor_name(), "c_chords"),
          "the track stays on its slot (0.6.32), the cursor stays put (%s)", cursor_name());
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
        CHECK(lbl(FM1CORE_LBL_BLACK0 + 9, "ADD") && ec_pn_item_value(EC_UX_FSLOT) == 0.0f,
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
    B(9, 40);                                      /* B9 (0.6.42: F2 opens): free its slot */
    CHECK(ec_file_pooled("/audio/d_perc16_bpm120.wav") < 0 && A->slot[2].state == EC_SL_EMPTY &&
              !strcmp(A->msg, "SLOT 3 FREED") && !A->pn.f_sticky,
          "B9 on a pool file: its slot freed (%s)", A->msg);
    shift_down(1);                                 /* 0.6.55 (QA B10): FREE undone: the slot and its file back */
    press(FM1P_B_OCTDN, 40);
    shift_down(0);
    settle(300);
    CHECK(ec_file_pooled("/audio/d_perc16_bpm120.wav") == 2 && A->slot[2].state == EC_SL_READY,
          "SHIFT + F1: slot 3 back, loaded (state %d)", A->slot[2].state);
    shift_down(1);
    press(FM1P_B_OCTUP, 40);
    shift_down(0);
    settle(100);
    CHECK(ec_file_pooled("/audio/d_perc16_bpm120.wav") < 0 && A->slot[2].state == EC_SL_EMPTY,
          "SHIFT + F2: freed again (state %d)", A->slot[2].state);
    B(9, 40);
    CHECK(!strcmp(A->msg, "NOT IN THE POOL"), "B9 again: not in the pool (%s)", A->msg);
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
    press(FM1P_B_SAVE, 40);                        /* 0.6.42: past PROJECTS */
    press(FM1P_B_SAVE, 40);
    CHECK(ec_file_page() == EC_PIC_EDITOR, "FILE again: the editor");
    sl = &A->proj.slot[ec_app_track_slot(A, 0)];
    /* 0.6.15 (owner): K1 START, K2 END (the last turned is the marker the keys place; the view follows it), K4 ZOOM
     * round that marker; B1 / B2 the pages, B9 AUTO BPM; no scroll or zoom keys */
    CHECK(lbl(FM1CORE_LBL_BLACK0 + 0, "P1") && lbl(FM1CORE_LBL_BLACK0 + 1, "P2") && lbl(FM1CORE_LBL_BLACK0 + 8, "AUTO BPM") &&
              lbl(FM1CORE_LBL_BLACK0 + 3, "") && lbl(FM1CORE_LBL_KNOB0 + FM1P_E_K4, "ZOOM") &&
              lbl(FM1CORE_LBL_KNOB0 + FM1P_E_K1, "START"),
          "the editor's keys: B1 %s B2 %s B9 %s; K1 %s K4 %s", L(FM1CORE_LBL_BLACK0), L(FM1CORE_LBL_BLACK0 + 1),
          L(FM1CORE_LBL_BLACK0 + 8), L(FM1CORE_LBL_KNOB0 + FM1P_E_K1), L(FM1CORE_LBL_KNOB0 + FM1P_E_K4));
    turn(FM1P_E_K4, 1);
    turn(FM1P_E_K4, 1);
    CHECK(ec_file_zoom() == 4.0f && A->pn.ed_view == 0.0f, "K4 twice: 4X round START (at 0) (%g %g)", ec_file_zoom(),
          A->pn.ed_view);
    turn(FM1P_E_K2, -1);                           /* END: a nudge selects it and the view follows it */
    CHECK(A->pn.ed_point == 1 && fabsf(sl->trim_end - 0.995f) < 1e-4f && fabsf(A->pn.ed_view - 0.75f) < 1e-4f,
          "K2: END selected, the view follows to the end (%d %g %g)", A->pn.ed_point, sl->trim_end, A->pn.ed_view);
    press(FM1P_B_OCTDN, 40);                       /* F1: START */
    CHECK(A->pn.ed_point == 0 && lbl(FM1CORE_LBL_BTN0 + FM1P_B_OCTDN, "START") &&
              lbl(FM1CORE_LBL_BTN0 + FM1P_B_OCTUP, "END") && lbl(FM1CORE_LBL_WHITE0 + 4, "START"),
          "F1 START / F2 END (%s %s)", L(FM1CORE_LBL_BTN0 + FM1P_B_OCTDN), L(FM1CORE_LBL_BTN0 + FM1P_B_OCTUP));
    W(1, 40);                                      /* START: the view's left edge */
    CHECK(fabsf(sl->trim_start - 0.75f) < 1e-4f && fabsf(sl->trim_end - 0.995f) < 1e-4f && A->pn.ed_point == 0,
          "START: W1 places the start at the view's edge (%g %g)", sl->trim_start, sl->trim_end);
    W(1, 40);
    CHECK(fabsf(sl->trim_start - 0.75f) < 1e-4f && A->pn.ed_point == 0, "START: a second tap places START again");
    press(FM1P_B_OCTUP, 40);                       /* F2: END */
    CHECK(A->pn.ed_point == 1 && !A->pn.f_sticky && lbl(FM1CORE_LBL_WHITE0 + 4, "END"),
          "F2: the keys place END (no CUT) (%d %d)", A->pn.ed_point, A->pn.f_sticky);
    W(16, 40);                                     /* END: the view's right edge */
    CHECK(fabsf(sl->trim_end - 1.0f) < 1e-4f && fabsf(sl->trim_start - 0.75f) < 1e-4f,
          "END: W16 places the end (%g)", sl->trim_end);
    B(10, 40);                                     /* 0.6.15: B10 RECALC: the steps from the trim and the BPM */
    CHECK(sl->steps == 8 && !strcmp(A->msg, "STEPS 8"), "B10: the steps from the trim (%g)", sl->steps);
    CHECK(led_key(0, 0) == FM1P_LED_LIT && led_key(0, 15) == FM1P_LED_LIT && led_key(0, 7) == FM1P_LED_FAINT,
          "the white keys at the trim lit");
    turn(FM1P_E_K4, -1);
    turn(FM1P_E_K4, -1);
    CHECK(ec_file_zoom() == 1.0f && A->pn.ed_view == 0.0f, "K4 back: 1X");
    B(2, 40);
    CHECK(A->ux.lane[EC_CAT_FILE] == 1 && lbl(FM1CORE_LBL_KNOB0 + FM1P_E_K1, "BPM"), "B2: page 2, K1 BPM (%s)",
          L(FM1CORE_LBL_KNOB0 + FM1P_E_K1));
    B(1, 40);
    CHECK(A->ux.lane[EC_CAT_FILE] == 0, "B1: page 1");
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
    CHECK(ec_file_page() == EC_PIC_EDITOR && ec_file_zoom() == 1.0f, "editor: B3 does nothing (0.6.15: no page or zoom keys)");
    turn(FM1P_E_SELECT, -1);
    CHECK(ec_file_page() == EC_PIC_EDITOR, "SEL -: the editor keeps its page (0.6.15: the FILE button leaves)");
}

/* ------------------------------------------------------------------------------------------- FILE's pages --- */
/* 0.6.1: SEL turns the page on EDITOR and RAZOR (lanes, then pages) both ways; on BROWSE / POOL it is the cursor and
 * the page keys B3 B4 B5 B6 leave; RAZOR's slice is ALGO's */
static void test_file_pages(void)
{
    const int t = A->track;
    int sel0, cur0;
    to(EC_CAT_SRC);
    to(EC_CAT_FILE);                               /* (from another mode: BROWSE) */
    B(7, 40);
    CHECK(ec_file_page() == EC_PIC_FILE && lbl(FM1CORE_LBL_BLACK0 + 5, "POOL") &&
              lbl(FM1CORE_LBL_BLACK0 + 3, "") && lbl(FM1CORE_LBL_BLACK0 + 4, ""),
          "BROWSE: B6 POOL, no B4 / B5 (0.6.15: the editors are on the FILE button) (%s)", L(FM1CORE_LBL_BLACK0 + 5));
    B(6, 40);
    CHECK(ec_file_page() == EC_PIC_POOL, "BROWSE: B6 leaves for POOL");
    B(7, 40);
    CHECK(ec_file_page() == EC_PIC_FILE, "POOL: B7 leaves for BROWSE (0.6.15)");
    B(6, 40);
    press(FM1P_B_SAVE, 40);                        /* 0.6.42: past PROJECTS */
    press(FM1P_B_SAVE, 40);
    CHECK(ec_file_page() == EC_PIC_EDITOR, "POOL: FILE again = the EDITOR (0.6.15)");
    press(FM1P_B_SAVE, 40);
    CHECK(ec_file_page() == EC_PIC_RAZOR, "EDITOR: FILE again = the slice editor");
    B(6, 40);
    CHECK(ec_file_page() == EC_PIC_RAZOR, "the slice editor: B6 does nothing (0.6.15: its keys are its own)");
    press(FM1P_B_SAVE, 40);                        /* the SAMPLER */
    press(FM1P_B_SAVE, 40);                        /* BROWSE */
    B(6, 40);
    press(FM1P_B_SAVE, 40);                        /* 0.6.42: past PROJECTS */
    press(FM1P_B_SAVE, 40);
    CHECK(ec_file_page() == EC_PIC_EDITOR, "POOL: FILE again = the EDITOR");
    turn(FM1P_E_SELECT, 1);                        /* EDITOR's second lane; 0.6.15: SEL stays on the editor's page */
    turn(FM1P_E_SELECT, 1);
    CHECK(ec_file_page() == EC_PIC_EDITOR && A->ux.lane[EC_CAT_FILE] == 1, "EDITOR: SEL stays on its pages (lane %d)",
          A->ux.lane[EC_CAT_FILE]);
    press(FM1P_B_SAVE, 40);                        /* FILE again: the slice editor */
    CHECK(ec_file_page() == EC_PIC_RAZOR && ec_rzui_active(A), "FILE again: the slice editor");
    trk(t)->v[EC_P_SLICE_COUNT] = 16;
    ec_rzui.sel = 3;
    sel0 = ec_rzui.sel;
    turn(FM1P_E_SELECT, -1);
    CHECK(ec_file_page() == EC_PIC_RAZOR && ec_rzui.sel == sel0, "the slice editor: SEL back stays (the slice stays)");
    turn(FM1P_E_SELECT, 1);
    run(20);
    CHECK(ec_file_page() == EC_PIC_RAZOR && A->ux.lane[EC_CAT_FILE] == 1, "SEL on: its second page (lane %d)",
          A->ux.lane[EC_CAT_FILE]);
    B(1, 40);
    CHECK(A->ux.lane[EC_CAT_FILE] == 0 && lbl(FM1CORE_LBL_KNOB0 + FM1P_E_K1, "SLCE START") &&
              lbl(FM1CORE_LBL_KNOB0 + FM1P_E_K4, "ZOOM") && lbl(FM1CORE_LBL_BLACK0 + 8, "EVEN") &&
              lbl(FM1CORE_LBL_BLACK0 + 9, "CHOP") && lbl(FM1CORE_LBL_BLACK0 + 10, "PREVIEW"),
          "B1: page 1: K1 SLCE START, K4 ZOOM; B9 EVEN, B10 CHOP, B11 PREVIEW (%s %s)", L(FM1CORE_LBL_KNOB0 + FM1P_E_K1),
          L(FM1CORE_LBL_KNOB0 + FM1P_E_K4));
    CHECK(lbl(FM1CORE_LBL_KNOB0 + FM1P_E_ALGO, "SLICE"), "RAZOR: ALGO's label SLICE (%s)",
          L(FM1CORE_LBL_KNOB0 + FM1P_E_ALGO));
    turn(FM1P_E_ALGO, 1);
    CHECK(ec_rzui.sel == sel0 + 1 && ec_file_page() == EC_PIC_RAZOR, "RAZOR: ALGO picks the next slice (%d)",
          ec_rzui.sel);
    turn(FM1P_E_ALGO, -2);
    CHECK(ec_rzui.sel == sel0 - 1, "RAZOR: ALGO back two (%d)", ec_rzui.sel);
    turn(FM1P_E_SELECT, 1);                        /* its pages 2 and 3; 0.6.15: SEL never leaves the editor */
    turn(FM1P_E_SELECT, 1);
    turn(FM1P_E_SELECT, 1);
    CHECK(ec_file_page() == EC_PIC_RAZOR && A->ux.lane[EC_CAT_FILE] == 2, "the slice editor: SEL to page 3 and stays (%d)",
          A->ux.lane[EC_CAT_FILE]);
    press(FM1P_B_SAVE, 40);                        /* the SAMPLER, BROWSE, then B6: POOL */
    press(FM1P_B_SAVE, 40);
    B(6, 40);
    CHECK(ec_file_page() == EC_PIC_POOL, "FILE twice, B6: POOL");
    cur0 = A->pn.pool_cur;
    turn(FM1P_E_SELECT, 1);
    CHECK(ec_file_page() == EC_PIC_POOL && A->pn.pool_cur == cur0 + 1, "POOL: SEL scrolls the slots (%d)",
          A->pn.pool_cur);
    turn(FM1P_E_SELECT, -1);
    B(7, 40);
}

/* ------------------------------------------------------------------------------------------- POOL --- */
static void test_pool(void)
{
    uint8_t tracks[EC_NSLOT], locks[EC_NSLOT];
    const int h0 = A->slot[0].handle, t = A->track;
    char drums[EC_PATH];
    int i;
    to(EC_CAT_SRC);
    to(EC_CAT_FILE);
    A->pn.as.preview = 1;
    B(6, 40);
    CHECK(ec_file_page() == EC_PIC_POOL && lbl(FM1CORE_LBL_BLACK0 + 9, "ASSIGN") &&
              lbl(FM1CORE_LBL_BLACK0 + 10, "MOVE") && lbl(FM1CORE_LBL_BLACK0 + 8, "FREE") &&
              lbl(FM1CORE_LBL_BTN0 + FM1P_B_OCTUP, "FREE") && lbl(FM1CORE_LBL_KNOB0 + FM1P_E_SELECT, "SLOT"),
          "B6: POOL, B10 ASSIGN, B11 MOVE (held), B9 / F2 FREE, SEL SLOT (0.6.15)");
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
    turn(FM1P_E_PRESETS, 1);                       /* 0.6.15: PRESETS = the audible preview on / off */
    CHECK(!A->pn.as.preview && !A->pn.pv_play, "PRESETS: the preview off, silent");
    turn(FM1P_E_PRESETS, 1);
    CHECK(A->pn.as.preview, "PRESETS again: on");
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
    key(1, 10, 1);                                 /* 0.6.15: B11 held = slot 1 in hand */
    run(20);
    CHECK(A->pn.pool_carry == 0 && ec_file_pool_carrying(), "B11 held: slot 1 in hand (MOVE 1)");
    turn(FM1P_E_SELECT, 4);
    turn(FM1P_E_K1, 5);                            /* SEL and K1 both move it on the way */
    CHECK(A->pn.pool_cur == 9 && A->pn.pool_carry == 0, "SEL 4 + K1 5: on slot 10, still in hand (%d)", A->pn.pool_cur);
    key(1, 10, 0);                                 /* let go: dropped there */
    run(20);
    {
        float lk = 0;
        ec_step_lock_get(&A->proj.pat[2].tr[3].steps[7], EC_P_SLOT, &lk);
        CHECK(A->pn.pool_carry < 0 && !strcmp(A->proj.slot[9].path, drums) && !A->proj.slot[0].path[0] &&
                  A->slot[9].handle == h0 && A->slot[0].state == EC_SL_EMPTY,
              "B11 let go on slot 10: slot 1 moved there (%s)", A->msg);
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
    key(1, 10, 1);
    run(20);
    B(7, 40);
    key(1, 10, 0);
    run(20);
    CHECK(A->pn.pool_carry < 0 && ec_file_page() == EC_PIC_FILE, "B7 with a slot in hand: BROWSE, the move dropped");
    /* 0.6.11: POOL opens while playing; MOVE and FREE do not */
    to(EC_CAT_SRC);
    press(FM1P_B_PLAY, 40);
    press(FM1P_B_SAVE, 40);
    B(6, 40);
    CHECK(mode() == EC_CAT_FILE && ec_file_page() == EC_PIC_POOL && A->seq.playing, "playing: POOL opens (%d)", mode());
    key(1, 10, 1);
    run(20);
    key(1, 10, 0);
    run(20);
    CHECK(A->pn.pool_carry < 0 && !strcmp(A->msg, "STOP FIRST"), "B11 (the move) while playing: STOP FIRST");
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
    to(EC_CAT_SRC);
    to(EC_CAT_FILE);
    press(FM1P_B_SAVE, 40);                        /* 0.6.42: past PROJECTS */
    press(FM1P_B_SAVE, 40);
    press(FM1P_B_SAVE, 40);
    CHECK(ec_rzui_active(A), "FILE twice from BROWSE: RAZOR");
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
    B(7, 40);
}

/* 0.6.55 (QA B1-B10): the undo pass */
static void undo_k(int redo)
{
    shift_down(1);
    press(redo ? FM1P_B_OCTUP : FM1P_B_OCTDN, 40);
    shift_down(0);
    run(20);
}
static void test_undo_pass(void)
{
    const int t = A->track;
    ec_razor_t *rz = &A->proj.razor[t], before;
    float *v;
    int k;
    /* B1: the SLICE editor: SHIFT + F1 / F2 are undo / redo (EVEN is B9) */
    to(EC_CAT_SRC);
    to(EC_CAT_FILE);
    press(FM1P_B_SAVE, 40);
    press(FM1P_B_SAVE, 40);
    press(FM1P_B_SAVE, 40);
    CHECK(ec_rzui_active(A), "undo pass: the slice editor");
    ec_razor_redistribute(rz, 16);
    trk(t)->v[EC_P_SLICE_COUNT] = 4;
    before = *rz;
    B(9, 40);                                      /* EVEN: 4 */
    CHECK(memcmp(rz, &before, sizeof before) != 0, "B9 EVEN changed the table");
    shift_down(1);
    run(400);
    CHECK(lbl(FM1CORE_LBL_BTN0 + FM1P_B_OCTDN, "UNDO") && lbl(FM1CORE_LBL_BTN0 + FM1P_B_OCTUP, "REDO"),
          "SHIFT in the slice editor: UNDO / REDO (%s / %s)", L(FM1CORE_LBL_BTN0 + FM1P_B_OCTDN),
          L(FM1CORE_LBL_BTN0 + FM1P_B_OCTUP));
    press(FM1P_B_OCTDN, 40);
    shift_down(0);
    CHECK(!memcmp(rz, &before, sizeof before) && ec_rzui_active(A), "SHIFT + F1: the table back (%s)", A->msg);
    undo_k(1);
    CHECK(memcmp(rz, &before, sizeof before) != 0, "SHIFT + F2: EVEN again (%s)", A->msg);
    /* B2: F2 + a knob (no source key) clears the routes: undoable */
    to(EC_CAT_FILT);
    ec_app_set_page(EC_CAT_FILT, 0);
    v = ec_app_params(A);
    for (k = 0; k < EC_MTX_SLOTS; k++)
        v[ec_mtx_src_id(k)] = 0, v[ec_mtx_dst_id(k)] = 0, v[ec_mtx_depth_id(k)] = 0;
    v[EC_P_LFO1_DEPTH] = 64;
    v[ec_mtx_src_id(0)] = (float)(EC_MS_LFO1 + 1);
    v[ec_mtx_dst_id(0)] = (float)(EC_P_FILTER_CUTOFF + 1);
    v[ec_mtx_depth_id(0)] = 50;
    ec_seq_touch(&A->seq, t);
    run(1200);
    btn(FM1P_B_OCTUP, 1);
    run(20);
    turn(FM1P_E_K1, 1);
    btn(FM1P_B_OCTUP, 0);
    run(20);
    CHECK(ec_mtx_find(v, EC_MS_LFO1, EC_P_FILTER_CUTOFF) < 0, "F2 + K1: the route cleared (%s)", A->msg);
    undo_k(0);
    CHECK(ec_mtx_find(v, EC_MS_LFO1, EC_P_FILTER_CUTOFF) == 0 && v[ec_mtx_depth_id(0)] == 50, "undo: the route back (%s)",
          A->msg);
    v[ec_mtx_src_id(0)] = 0, v[ec_mtx_dst_id(0)] = 0, v[ec_mtx_depth_id(0)] = 0;
    run(1200);
    /* B3: a route a source key made: one undo entry (with the level it raised) */
    key(1, 5, 1);                                  /* B6 held: LFO 1 */
    run(50);
    turn(FM1P_E_K1, 1);
    run(150);
    turn(FM1P_E_K1, 1);
    run(150);
    turn(FM1P_E_K1, 1);
    run(150);
    key(1, 5, 0);
    run(50);
    k = ec_mtx_find(v, EC_MS_LFO1, EC_P_FILTER_CUTOFF);
    CHECK(k >= 0 && fabsf(v[ec_mtx_depth_id(k)] - 0.2f) < 0.01f && v[EC_P_LFO1_DEPTH] == 128,
          "B6 + K1 x3: LFO1 > CUTOFF 0.2, LFO 1 up to full (%d, %g, %g)", k, k >= 0 ? v[ec_mtx_depth_id(k)] : -1,
          v[EC_P_LFO1_DEPTH]);
    undo_k(0);
    CHECK(ec_mtx_find(v, EC_MS_LFO1, EC_P_FILTER_CUTOFF) < 0 && v[EC_P_LFO1_DEPTH] == 64,
          "one undo: the route and the level gone (%s, level %g)", A->msg, v[EC_P_LFO1_DEPTH]);
    undo_k(1);
    k = ec_mtx_find(v, EC_MS_LFO1, EC_P_FILTER_CUTOFF);
    CHECK(k >= 0 && fabsf(v[ec_mtx_depth_id(k)] - 0.2f) < 0.01f && v[EC_P_LFO1_DEPTH] == 128, "redo: back (%s)", A->msg);
    for (k = 0; k < EC_MTX_SLOTS; k++)
        v[ec_mtx_src_id(k)] = 0, v[ec_mtx_dst_id(k)] = 0, v[ec_mtx_depth_id(k)] = 0;
    v[EC_P_LFO1_DEPTH] = 64;
    run(1200);
    /* C9: a hold that keeps no route leaves the level as it was, and no entry */
    {
        const int n0 = A->e5.undo.n;
        key(1, 5, 1);
        run(50);
        turn(FM1P_E_K1, 1);
        run(50);
        key(1, 5, 0);
        run(50);
        CHECK(ec_mtx_find(v, EC_MS_LFO1, EC_P_FILTER_CUTOFF) < 0 && v[EC_P_LFO1_DEPTH] == 64 && A->e5.undo.n == n0,
              "B6 + K1 once, let go: no route, LFO 1 level as it was (%g), no entry", v[EC_P_LFO1_DEPTH]);
    }
    /* B10: mutes */
    trk(2)->v[EC_P_MUTE] = 0;
    ec_app_toggle_mute(2);
    run(1200);
    ec_e5_undo(0);
    CHECK(trk(2)->v[EC_P_MUTE] == 0, "T3 mute undone");
    trk(1)->v[EC_P_MUTE] = 1;
    trk(3)->v[EC_P_MUTE] = 1;
    run(1200);
    ec_app_clear_mutes();
    ec_e5_undo(0);
    CHECK(trk(1)->v[EC_P_MUTE] == 1 && trk(3)->v[EC_P_MUTE] == 1, "ALL TRACKS ON undone");
    trk(1)->v[EC_P_MUTE] = 0;
    trk(3)->v[EC_P_MUTE] = 0;
    /* the snapshot ring: a pattern and the scenes come back, redo goes forward; an entry older than the ring says so */
    {
        ec_pattern_t *pt = &A->proj.pat[5];
        const uint8_t tr0 = pt->tr[0].steps[3].trig;
        int i, n;
        run(1200);
        ec_undo_blob(&A->e5.undo, &A->proj, EC_UB_PATTERN, 5, -1, "PST PAT", 0, A->ms);
        pt->tr[0].steps[3].trig = (uint8_t)!tr0;
        ec_e5_undo(0);
        CHECK(pt->tr[0].steps[3].trig == tr0, "a pattern snapshot: undone (%s)", A->msg);
        ec_e5_undo(1);
        CHECK(pt->tr[0].steps[3].trig == !tr0, "redone (%s)", A->msg);
        pt->tr[0].steps[3].trig = tr0;
        A->proj.e5.sc.has[0][0][EC_P_PITCH] = 1;
        A->proj.e5.sc.val[0][0][EC_P_PITCH] = 7;
        run(1200);
        ec_undo_blob(&A->e5.undo, &A->proj, EC_UB_SCENES, A->proj.current, -1, "SCN CLEAR", 0, A->ms);
        ec_scene_clear(&A->proj, 0);
        ec_e5_undo(0);
        CHECK(A->proj.e5.sc.has[0][0][EC_P_PITCH] && A->proj.e5.sc.val[0][0][EC_P_PITCH] == 7, "the scenes undone (%s)",
              A->msg);
        for (n = 0; n < EC_UNDO_BLOBS + 1; n++)
            ec_undo_blob(&A->e5.undo, &A->proj, EC_UB_TRACK, A->proj.current, n % EC_NTRACK, "T", 0, A->ms + (uint32_t)n);
        for (i = 0; i < EC_UNDO_BLOBS; i++)
            ec_e5_undo(0);
        ec_e5_undo(0);
        CHECK(strstr(A->msg, "TOO OLD") != NULL, "the ring's oldest: TOO OLD (%s)", A->msg);
    }
    B(7, 40);
}

/* 0.6.16 (QA): the slice table is on the undo stack, with the COUNT knob; the trim editor's START and END are gestures
 * of their own; a held menu button let go with no key changes nothing */
static void test_qa616(void)
{
    ec_razor_t *rz = &A->proj.razor[A->track];
    ec_slot_t *sl;
    float s0, e0;
    int t = A->track, k, undo0;
    /* the slice editor: EVEN 4 -> UNDO = the 16 before; COUNT -12 -> UNDO = 16 again, the edges as they were */
    to(EC_CAT_SRC);
    to(EC_CAT_FILE);
    press(FM1P_B_SAVE, 40);                        /* 0.6.42: past PROJECTS */
    press(FM1P_B_SAVE, 40);
    press(FM1P_B_SAVE, 40);
    CHECK(ec_rzui_active(A), "QA: the slice editor");
    ec_razor_redistribute(rz, 16);
    ec_project_track(&A->proj, t)->v[EC_P_SLICE_COUNT] = 16;
    s0 = rz->start[5];
    e0 = rz->end[5];
    B(3, 40);                                      /* page 3: COUNT, TRANS SENS */
    turn(FM1P_E_K1, -12);
    run(20);
    CHECK(ec_rzui_count(A) == 4, "QA: COUNT 4 (%d)", ec_rzui_count(A));
    B(9, 40);                                      /* EVEN 4 */
    run(20);
    CHECK(fabsf(rz->end[0] - 32.0f) < 0.01f, "QA: EVEN 4: slice 1 ends at 32 (%g)", rz->end[0]);
    undo0 = A->e5.undo.n;
    to(EC_CAT_SRC);
    shift_down(1);
    press(FM1P_B_OCTDN, 40);
    shift_down(0);
    CHECK(ec_rzui_count(A) == 4 && fabsf(rz->end[0] - 8.0f) < 0.01f, "QA: UNDO EVEN: the 16 edges back (%g, %s)",
          rz->end[0], A->msg);
    shift_down(1);
    press(FM1P_B_OCTDN, 40);
    shift_down(0);
    CHECK(ec_rzui_count(A) == 16 && rz->start[5] == s0 && rz->end[5] == e0 && A->e5.undo.n == undo0 - 2,
          "QA: UNDO CNT: 16 slices, slice 6 as it was (%d, %g..%g, %s)", ec_rzui_count(A), rz->start[5], rz->end[5], A->msg);
    /* the trim editor: K1 then K2 within a second = two undos */
    to(EC_CAT_FILE);
    press(FM1P_B_SAVE, 40);                        /* 0.6.42: past PROJECTS */
    press(FM1P_B_SAVE, 40);
    CHECK(ec_file_page() == EC_PIC_EDITOR, "QA: the trim editor");
    sl = &A->proj.slot[ec_app_track_slot(A, A->track)];
    s0 = sl->trim_start;
    e0 = sl->trim_end;
    turn(FM1P_E_K1, 4);
    run(300);
    turn(FM1P_E_K2, -4);
    run(20);
    CHECK(sl->trim_start > s0 && sl->trim_end < e0, "QA: START and END moved (%g %g)", sl->trim_start, sl->trim_end);
    shift_down(1);
    press(FM1P_B_OCTDN, 40);
    shift_down(0);
    CHECK(sl->trim_start > s0 && sl->trim_end == e0, "QA: UNDO: END back, START still moved (%g %g, %s)", sl->trim_start,
          sl->trim_end, A->msg);
    shift_down(1);
    press(FM1P_B_OCTDN, 40);
    shift_down(0);
    CHECK(sl->trim_start == s0, "QA: UNDO again: START back (%g, %s)", sl->trim_start, A->msg);
    /* a hold with no key: nothing (a click still cycles) */
    k = mode();
    press(FM1P_B_ENV, 700);
    run(20);
    CHECK(mode() == k, "QA: ENV held 700 ms, no key: the mode stays (%d)", mode());
    press(FM1P_B_ENV, 40);
    run(20);
    CHECK(mode() != k, "QA: ENV clicked: AMP (%d)", mode());
    to(EC_CAT_FILE);
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
    test_qa616();
    test_undo_pass();
    test_sampler();
    test_slot_locks();
    test_rate4x();
    test_patterns_full();
    C->shutdown();
    printf("test_file: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
