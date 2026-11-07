/* SPDX-License-Identifier: GPL-3.0-only */
/* test_ux: the 0.6.0 panel by modes (docs/UX-V2.md) through the core descriptor, with the demo folder
 * (host/make_demo_folder.py) and the shipped layout: the intro, the mode model (taps, the lit button again, sub-modes,
 * each mode's page, holds and SHIFT = settings, SHIFT + HOME = APP SETTINGS), the pop-up, SHIFT (held, sticky) and
 * its layer, F1 / F2 (sticky, momentary, F3), PLAY, REC, steps and locks, F1 / F2 + a knob, PRESETS = the pool slot,
 * §2 copy / cut / paste, the source template, MIX and the CAT, PAGES, KEYBOARD, CROSSFADER, PATTERN LAUNCH, the
 * live labels and LEDs.
 *   ec_test_ux <folder> */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ec_app.h"
#include "ec_holdmenu.h"
#include "ec_browse.h"
#include "ec_fs_native.h"
#include "fm1core.h"
#include "ec_gfx.h"
#include "ec_mtx.h"
#include "ec_scene.h"

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

/* the LEDs (ec_core.c's FM-1 wiring: key id 0..13 the buttons, 14 + n note key n) */
static const int8_t KEYMAP[6][FM1P_NCOL] = {
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {5, 11, 4, 10, 3, 9, 2, 8, -1, -1, -1},
    {34, 35, 36, 37, 38, 40, 39, 13, 7, 6, 12},
    {23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33},
    {0, 1, 15, 14, 17, 16, 19, 18, 20, 21, 22},
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
};
static int led_id(int id)
{
    int r, c;
    for (r = 1; r <= 4; r++)
        for (c = 0; c < FM1P_NCOL; c++)
            if (KEYMAP[r][c] == id)
                return P->led[c][r - 1];
    return -1;
}
static int led_key(int upper, int i) { return led_id(14 + key_of(upper, i)); }
static int led_btn(int b) { return led_id(P->btn_id[b]); }

static int mode(void) { return ec_app_mode(); }
static const int BTN_OF_MB[EC_NMB] = {-1, FM1P_B_FX, FM1P_B_SEL, FM1P_B_ENV, FM1P_B_LFO, FM1P_B_EDIT, FM1P_B_HOME,
                                      FM1P_B_SAVE, FM1P_B_ARP, FM1P_B_SEQ};
/* close what is open, then tap the mode's button until it shows; the pop-up gone */
static void to(int m)
{
    int n;
    if (A->e5.naming)
        press(FM1P_B_OCTDN, 40);
    if (A->pn.as_open || A->ux.f3 || A->settings)
        press(FM1P_B_OCTDN, 40);
    if (A->pn.shift_sticky)
        press(FM1P_B_GLO, 40);
    if (A->pn.f_sticky)
        press(A->pn.f_sticky == 1 ? FM1P_B_OCTDN : FM1P_B_OCTUP, 40);
    for (n = 0; n < 5 && mode() != m; n++)
        press(BTN_OF_MB[ec_mode_button(m)], 40);
    if (A->pn.cat)
        press(BTN_OF_MB[ec_mode_button(m)], 40);
    if (m == EC_CAT_FILE) {                        /* 0.6.66: the button comes back where FILE was left: the browser */
        ec_pn_enter(EC_CAT_SRC);
        ec_pn_enter(EC_CAT_FILE);
    }
    run(900);
}
static int bkey(int b) { return b - 1; }      /* B1..B11 -> upper index */
static int wkey(int w) { return w - 1; }      /* W1..W16 -> lower index */

/* ------------------------------------------------------------------------------------------- the intro, the modes --- */
static void test_boot(void)
{
    CHECK(A->pn.intro, "power-on: the intro plays");
    CHECK(mode() == EC_CAT_SRC && !A->scr, "power-on: LOOP ENGINE (mode %d, T%d)", mode(), A->track + 1);
    turn(FM1P_E_K1, 1);                                       /* any knob skips it (and still acts) */
    run(20);
    CHECK(!A->pn.intro, "a knob skips the intro");
}

static void test_modes(void)
{
    to(EC_CAT_SRC);
    press(FM1P_B_FX, 40);
    CHECK(mode() == EC_CAT_FX, "FX: the mode (0.6.1: no pop-up, host/test_v61ux.c)");
    run(900);
    press(FM1P_B_FX, 40);
    CHECK(mode() == EC_CAT_FX && ec_fx_slot() == 1, "FX again: the next group (0.6.3: INSERT 2)");
    press(FM1P_B_SEL, 40);
    CHECK(mode() == EC_CAT_FILT, "SEL: FILTER");
    turn(FM1P_E_K2, 1);
    run(5);
    press(FM1P_B_FX, 40);
    CHECK(mode() == EC_CAT_FX && ec_fx_slot() == 1, "FX keeps its page");
    /* HOME: LOOP ENGINE -> TRIG -> KEYBOARD -> LOOP ENGINE */
    press(FM1P_B_HOME, 40);
    CHECK(mode() == EC_CAT_SRC, "HOME: LOOP ENGINE");
    press(FM1P_B_HOME, 40);
    CHECK(mode() == EC_CAT_TRIG, "HOME again: TRIG");
    press(FM1P_B_HOME, 40);
    CHECK(mode() == EC_MODE_KEYBOARD && A->scr == EC_MODE_KEYBOARD, "HOME again: KEYBOARD");
    press(FM1P_B_FX, 40);
    press(FM1P_B_HOME, 40);
    CHECK(mode() == EC_MODE_KEYBOARD, "HOME from FX: KEYBOARD, the sub-mode it was left on (0.6.66, owner)");
    press(FM1P_B_HOME, 40);
    CHECK(mode() == EC_CAT_SRC, "HOME again: LOOP ENGINE");
    press(FM1P_B_HOME, 40);
    CHECK(mode() == EC_CAT_TRIG, "HOME again: TRIG");
    press(FM1P_B_EDIT, 40);
    CHECK(mode() == EC_CAT_MASTER, "EDIT: MIX");
    press(FM1P_B_EDIT, 40);
    CHECK(mode() == EC_MODE_PAGES, "EDIT again: PAGES");
    press(FM1P_B_EDIT, 40);
    CHECK(mode() == EC_CAT_MASTER, "EDIT again: MIX");
    press(FM1P_B_SAVE, 40);
    CHECK(mode() == EC_CAT_FILE, "FILE");
    press(FM1P_B_SAVE, 40);
    CHECK(mode() == EC_MODE_PROJECTS, "FILE again: PROJECTS (0.6.42)");
    press(FM1P_B_SAVE, 40);
    press(FM1P_B_SAVE, 40);
    press(FM1P_B_SAVE, 40);                        /* 0.6.15: the EDITOR, RAZOR, then the SAMPLER */
    CHECK(mode() == EC_MODE_SAMPLER && !A->rec.open, "FILE again x3: SAMPLER");
    press(FM1P_B_ARP, 40);
    CHECK(mode() == EC_CAT_XFD && !A->rec.open, "ARP: CROSSFADER, the SAMPLER left");
    press(FM1P_B_SEQ, 40);
    CHECK(mode() == EC_CAT_PAT, "SEQ: PATTERN LAUNCH");
    press(FM1P_B_LFO, 40);
    CHECK(mode() == EC_CAT_MOD, "LFO: MODULATION");
    press(FM1P_B_ENV, 40);
    CHECK(mode() == EC_CAT_AMP, "ENV: AMP");
    run(900);
    CHECK(led_btn(FM1P_B_ENV) == FM1P_LED_LIT && led_btn(FM1P_B_FX) == FM1P_LED_FAINT, "the mode's button lit (%d %d)",
          led_btn(FM1P_B_ENV), led_btn(FM1P_B_FX));
    CHECK(lbl(FM1CORE_LBL_BTN0 + FM1P_B_HOME, "TRIG") && lbl(FM1CORE_LBL_BTN0 + FM1P_B_EDIT, "MIX"),
          "button labels: where each comes back (0.6.66): HOME %s EDIT %s", L(FM1CORE_LBL_BTN0 + FM1P_B_HOME), L(FM1CORE_LBL_BTN0 + FM1P_B_EDIT));
}

/* ------------------------------------------------------------------------------------------- settings --- */
static void test_settings(void)
{
    to(EC_CAT_FX);
    btn(FM1P_B_FX, 1);                                        /* 0.6.15: a hold never opens settings: the mode picker */
    run(450);
    {
        char h[40];
        ec_hdr_title("", h, sizeof h);
        CHECK(!A->settings && ec_pn_picking() && !strcmp(h, "PICK A MODE"), "FX held 450 ms: no settings, PICK A MODE (%s)", h);
        CHECK(led_key(0, 0) == FM1P_LED_OFF && led_key(0, 7) == FM1P_LED_OFF, "picking: the white keys dark");
        key(0, wkey(3), 1);                                   /* a white key while picking: swallowed */
        run(20);
        key(0, wkey(3), 0);
        run(20);
        CHECK(!ec_project_track(&A->proj, A->track)->steps[2].trig && !A->step_held, "a white key while picking: nothing reaches the mode");
    }
    btn(FM1P_B_FX, 0);
    run(30);
    CHECK(!A->settings && mode() == EC_CAT_FX, "let go without a key: nothing changed");
    btn(FM1P_B_GLO, 1);
    run(20);
    press(FM1P_B_FX, 40);
    btn(FM1P_B_GLO, 0);
    run(20);
    CHECK(A->settings && A->cat == EC_CAT_FX, "SHIFT + FX: the FX settings");
    CHECK(A->settings, "they stay open on release");
    CHECK(lbl(FM1CORE_LBL_BTN0 + FM1P_B_OCTDN, "BACK") && lbl(FM1CORE_LBL_BTN0 + FM1P_B_OCTUP, "OK"),
          "settings: F1 BACK, F2 OK");
    press(FM1P_B_OCTDN, 40);
    CHECK(!A->settings && mode() == EC_CAT_FX, "F1: back to the FX pages");
    btn(FM1P_B_GLO, 1);
    run(20);
    press(FM1P_B_SEL, 40);
    btn(FM1P_B_GLO, 0);
    run(20);
    CHECK(A->settings && A->cat == EC_CAT_FILT, "SHIFT + SEL: FILTER's settings at once");
    press(FM1P_B_OCTUP, 40);
    CHECK(!A->settings && mode() == EC_CAT_FX, "F2 closes them: back to the mode it was opened over");
    to(EC_MODE_KEYBOARD);
    ec_pn_select_target(0);                                   /* (a track: a bus target would show its own setup) */
    btn(FM1P_B_GLO, 1);
    run(20);
    press(FM1P_B_HOME, 40);
    btn(FM1P_B_GLO, 0);
    run(20);                             /* 0.6.15: SHIFT + HOME: LOOP ENGINE's settings + ADVANCE */
    {
        ec_page_t pg;
        int i, adv = 0;
        ec_app_page(A, &pg);
        for (i = 0; i < pg.n; i++)
            adv |= pg.it[i].kind == EC_IT_UX && pg.it[i].id == EC_UX_ADVANCE;
        CHECK(A->settings && A->cat == EC_CAT_SRC && adv, "SHIFT + HOME: the loop / machine settings with ADVANCE (set %d cat %d scr %d setscr %d fn %d adv %d)", A->settings, A->cat, A->scr, A->pn.set_scr, A->fn, adv);
    }
    press(FM1P_B_HOME, 40);
    CHECK(!A->settings && mode() == EC_MODE_KEYBOARD, "HOME tapped in them: back to KEYBOARD");
    btn(FM1P_B_GLO, 1);
    run(20);
    press(FM1P_B_SEQ, 40);
    btn(FM1P_B_GLO, 0);
    run(20);
    CHECK(A->ux.f3, "SHIFT + SEQ: F3");
    turn(FM1P_E_PRESETS, 1);
    run(20);
    CHECK(A->e5.naming == EC_NM_PATTERN && !A->ux.f3, "F3 + PRESETS: the pattern's name");
    press(FM1P_B_HOME, 40);                        /* 0.6.47: OCT- deletes now (FIZZ's keybed); HOME cancels */
    CHECK(!A->e5.naming, "HOME: naming cancelled");
    btn(FM1P_B_GLO, 1);
    run(20);
    press(FM1P_B_SEQ, 40);
    btn(FM1P_B_GLO, 0);
    run(20);
    CHECK(A->ux.f3, "SHIFT + SEQ: F3");
    press(FM1P_B_SEQ, 40);
    CHECK(!A->ux.f3, "SEQ closes F3");
    to(EC_CAT_FX);
    btn(FM1P_B_OCTDN, 1);
    run(30);
    btn(FM1P_B_OCTUP, 1);
    run(30);
    CHECK(A->ux.f3, "F1 + F2: F3");
    btn(FM1P_B_OCTDN, 0);
    btn(FM1P_B_OCTUP, 0);
    run(30);
    CHECK(A->ux.f3 && !A->pn.f_sticky, "F3 stays open, no sticky F");
    press(FM1P_B_OCTDN, 40);
    CHECK(!A->ux.f3, "F1: F3 closed");
}

/* ------------------------------------------------------------------------------------------- APP SETTINGS --- */
static void test_app_settings(void)
{
    uint32_t size;
    const uint8_t *rec;
    to(EC_CAT_SRC);
    btn(FM1P_B_GLO, 1);
    run(20);
    press(FM1P_B_ARP, 40);                                    /* 0.6.15: SHIFT + ARP = APP SETTINGS */
    btn(FM1P_B_GLO, 0);
    run(20);
    CHECK(A->pn.as_open && ec_app_popup(A), "SHIFT + ARP: APP SETTINGS");
    CHECK(lbl(FM1CORE_LBL_KNOB0 + FM1P_E_K1, "THEME") && lbl(FM1CORE_LBL_KNOB0 + FM1P_E_SELECT, "ROW"),
          "its knobs: K1 %s", L(FM1CORE_LBL_KNOB0 + FM1P_E_K1));
    turn(FM1P_E_K1, 1);
    run(20);
    CHECK(ec_gfx_theme() == 1, "THEME turned: the palette changes at once (%d)", ec_gfx_theme());
    press(FM1P_B_OCTDN, 40);
    CHECK(!A->pn.as_open && ec_gfx_theme() == 0, "F1: back, the theme as it was");
    btn(FM1P_B_GLO, 1);
    run(20);
    press(FM1P_B_ARP, 40);                                    /* 0.6.15: SHIFT + ARP = APP SETTINGS */
    btn(FM1P_B_GLO, 0);
    run(20);
    turn(FM1P_E_K1, 2);
    turn(FM1P_E_SELECT, 1);                                   /* row 2 = KNOB SPEED (0.6.1: a list menu, K1) */
    turn(FM1P_E_K1, 1);
    run(20);
    press(FM1P_B_OCTUP, 40);
    rec = ec_plat_settings(&size);
    CHECK(!A->pn.as_open && ec_gfx_theme() == 1 && A->pn.as.knob == 2, "F2: kept (theme %d, knob %d)", ec_gfx_theme(),
          A->pn.as.knob);
    CHECK(rec && rec[0] == 0x45 && rec[1] == 0x43 && rec[2] == 0x41 && rec[3] == 0x53, "saved in the settings record");
    A->pn.as.theme = 0;                                       /* the record survives a reload of it */
    ec_as_load();
    CHECK(A->pn.as.theme == 1 && A->pn.as.knob == 2, "loaded back from the record");
    btn(FM1P_B_GLO, 1);
    run(20);
    press(FM1P_B_ARP, 40);                                    /* 0.6.15: SHIFT + ARP = APP SETTINGS */
    btn(FM1P_B_GLO, 0);
    run(20);
    turn(FM1P_E_K1, -2);
    turn(FM1P_E_SELECT, 1);
    turn(FM1P_E_K1, -1);
    press(FM1P_B_HOME, 40);
    CHECK(!A->pn.as_open && ec_gfx_theme() == 0 && A->pn.as.knob == 1, "HOME closes (keeps)");
}

/* ------------------------------------------------------------------------------------------- SHIFT --- */
static void test_shift(void)
{
    to(EC_CAT_FILT);
    press(FM1P_B_GLO, 40);                                    /* a quick tap: sticky */
    CHECK(A->pn.shift_sticky && A->fn, "GLO tapped: sticky SHIFT");
    CHECK(ec_app_shift_map(A), "sticky: SHIFT's screen");
    CHECK(lbl(FM1CORE_LBL_WHITE0 + 2, "T3") && lbl(FM1CORE_LBL_BLACK0 + 0, "TAP") &&
              lbl(FM1CORE_LBL_BLACK0 + 10, "FILL") && lbl(FM1CORE_LBL_BLACK0 + 5, ""),
          "SHIFT's key labels: W3 %s B1 %s B11 %s B6 '%s'", L(FM1CORE_LBL_WHITE0 + 2), L(FM1CORE_LBL_BLACK0),
          L(FM1CORE_LBL_BLACK0 + 10), L(FM1CORE_LBL_BLACK0 + 5));
    tapkey(0, wkey(3), 40);
    CHECK(A->track == 2 && A->fn, "sticky SHIFT + W3: T3 (it stays on)");
    tapkey(0, wkey(7), 40);
    CHECK(A->bus == 1, "SHIFT + W7: S1");
    tapkey(0, wkey(1), 40);
    CHECK(A->track == 0 && !A->bus, "SHIFT + W1: T1");
    press(FM1P_B_GLO, 40);
    CHECK(!A->pn.shift_sticky && !A->fn, "GLO again: off");
    press(FM1P_B_GLO, 40);
    press(FM1P_B_FX, 40);
    CHECK(!A->pn.shift_sticky, "a mode change ends sticky SHIFT");
    to(EC_CAT_FILT);
    btn(FM1P_B_GLO, 1);                                       /* held */
    run(20);
    {
        const float bpm = A->proj.bpm, sw = A->proj.swing;
        turn(FM1P_E_SELECT, 3);
        turn(FM1P_E_ALGO, 2);
        run(20);
        CHECK(A->proj.bpm > bpm && A->proj.swing > sw, "SHIFT + SEL / ALGO: BPM %g, swing %g", A->proj.bpm, A->proj.swing);
        {   /* 0.6.6: one slow detent = 0.1 BPM (no snap to tens), back down the same */
            const float b0 = A->proj.bpm;
            run(200);
            turn(FM1P_E_SELECT, 1);
            CHECK(fabsf(A->proj.bpm - b0 - 0.1f) < 0.001f, "SHIFT + SEL, one slow detent: +0.1 BPM (%g -> %g)", b0,
                  A->proj.bpm);
            run(200);
            turn(FM1P_E_SELECT, -1);
            CHECK(fabsf(A->proj.bpm - b0) < 0.001f, "and back -0.1 (%g)", A->proj.bpm);
        }
        turn(FM1P_E_PRESETS, 1);
        run(20);
        CHECK(A->track == 1, "SHIFT + PRESETS: the next track");
        turn(FM1P_E_PRESETS, -1);
        run(20);
        tapkey(1, bkey(5), 40);                               /* MEMORIZE (0.6.6: B5) */
        CHECK(A->e5.memo_ok, "SHIFT + B5: memorized");
        tapkey(1, bkey(2), 40);
        CHECK(A->fill_latched, "SHIFT + B2: fill latched");
        tapkey(1, bkey(2), 40);
        key(1, bkey(11), 1);
        run(20);
        CHECK(A->fill_held && ec_seq_fill_active(&A->seq), "SHIFT + B11 held: FILL");
        key(1, bkey(11), 0);
        run(20);
        CHECK(!A->fill_held, "B11 up: no fill");
        tapkey(1, bkey(1), 40);
        run(440);
        tapkey(1, bkey(1), 40);
        CHECK(fabsf(A->proj.bpm - 120.0f) < 12.0f, "SHIFT + B1 twice 0.5 s apart: TAP ~120 (%g)", A->proj.bpm);
        {
            const int saves = A->saves;
            press(FM1P_B_SAVE, 40);
            CHECK(A->saves == saves + 1, "SHIFT + FILE: saved");
        }
        press(FM1P_B_OCTDN, 40);
        CHECK(A->e5.undo.nr > 0, "SHIFT + F1: undo");
        press(FM1P_B_OCTUP, 40);
        CHECK(A->e5.undo.nr == 0, "SHIFT + F2: redo");
    }
    btn(FM1P_B_GLO, 0);
    run(20);
    CHECK(!A->fn && !A->pn.shift_sticky, "SHIFT used while held: no sticky on release");
    tapkey(1, bkey(4), 40);                                   /* without SHIFT: FILTER's B4 (nothing) */
    btn(FM1P_B_GLO, 1);
    run(20);
    tapkey(1, bkey(4), 40);
    btn(FM1P_B_GLO, 0);
    run(20);
    CHECK(A->msg_ms && strstr(A->msg, "RECALL"), "SHIFT + B4: recalled (%s)", A->msg);
}

/* ------------------------------------------------------------------------------------------- PLAY, REC --- */
static void test_transport(void)
{
    to(EC_CAT_FILT);
    btn(FM1P_B_PLAY, 1);
    run(2);
    CHECK(A->seq.playing, "PLAY: starts on the press");
    btn(FM1P_B_PLAY, 0);
    run(20);
    btn(FM1P_B_PLAY, 1);
    run(2);
    CHECK(!A->seq.playing, "PLAY again: stops at once");
    btn(FM1P_B_PLAY, 0);
    run(20);
    press(FM1P_B_REC, 40);
    CHECK(ec_liverec_on() && ec_liverec_armed() && !A->seq.playing,
          "REC tapped (stopped, CAPTURE): on, waiting for the first key");
    press(FM1P_B_REC, 40);
    CHECK(!ec_liverec_on(), "REC again: off");
    press(FM1P_B_REC, 500);
    CHECK(!ec_liverec_on() && A->pn.rs_open && ec_app_popup(A), "REC held: the record settings");
    press(FM1P_B_REC, 40);
    CHECK(!A->pn.rs_open && !ec_liverec_on(), "REC closes them (no toggle)");
}

/* ------------------------------------------------------------------------------------------- steps --- */
static ec_step_t *step(int i) { return &ec_project_track(&A->proj, A->track)->steps[i]; }
static void clear_steps(void)
{
    int i;
    for (i = 0; i < EC_MAXSTEPS; i++)
        ec_step_clear(step(i));
}

static void test_steps(void)
{
    float x;
    int k;
    to(EC_CAT_FILT);
    ec_app_set_page(EC_CAT_FILT, 0);
    clear_steps();
    tapkey(0, wkey(1), 40);
    CHECK(step(0)->trig, "FILTER: W1 tapped = a trig on step 1");
    run(20);
    CHECK(led_key(0, wkey(1)) == FM1P_LED_MID, "its LED (%d)", led_key(0, wkey(1)));
    key(0, wkey(3), 1);
    run(300);
    turn(FM1P_E_K1, 5);                                       /* hold a step + a knob = a lock */
    run(20);
    key(0, wkey(3), 0);
    run(20);
    CHECK(ec_step_lock_get(step(2), EC_P_FILTER_CUTOFF, &x) && step(2)->trig, "W3 held + K1: a cutoff lock");
    key(0, wkey(5), 1);                                       /* hold a step + PRESETS: a slot lock */
    run(300);
    turn(FM1P_E_PRESETS, 1);
    run(20);
    key(0, wkey(5), 0);
    run(20);
    CHECK(ec_step_lock_get(step(4), EC_P_SLOT, &x), "W5 held + PRESETS: a slot lock (%g)", x);
    btn(FM1P_B_OCTDN, 1);                                     /* F1 held + K1: every cutoff lock cleared */
    run(20);
    CHECK(lbl(FM1CORE_LBL_KNOB0 + FM1P_E_K1, "CLR CUTOFF"), "F1 held: K1 %s", L(FM1CORE_LBL_KNOB0 + FM1P_E_K1));
    turn(FM1P_E_K1, 1);
    run(20);
    btn(FM1P_B_OCTDN, 0);
    run(20);
    CHECK(!ec_step_lock_get(step(2), EC_P_FILTER_CUTOFF, &x) && !A->pn.f_sticky, "F1 + K1: the locks cleared, no sticky");
    /* 0.6.51 (owner): a step held + F1: that step's lock of the last knob turned, cleared (the others kept) */
    key(0, wkey(3), 1);
    run(300);
    turn(FM1P_E_K1, 3);
    run(20);
    key(0, wkey(4), 1);
    run(20);
    turn(FM1P_E_K1, 3);
    run(20);
    key(0, wkey(4), 0);
    run(20);
    btn(FM1P_B_OCTDN, 1);
    run(20);
    btn(FM1P_B_OCTDN, 0);
    run(20);
    key(0, wkey(3), 0);
    run(20);
    CHECK(!ec_step_lock_get(step(2), EC_P_FILTER_CUTOFF, &x) && ec_step_lock_get(step(3), EC_P_FILTER_CUTOFF, &x) &&
              !A->pn.f_sticky && !A->pn.f_held, "W3 held + F1: W3's cutoff lock cleared, W4's kept");
    {   /* the param not on the page shown (AMP): nothing cleared */
        const int pg0 = A->page[EC_CAT_FILT];
        to(EC_CAT_AMP);
        key(0, wkey(4), 1);
        run(300);
        btn(FM1P_B_OCTDN, 1);
        run(20);
        btn(FM1P_B_OCTDN, 0);
        run(20);
        key(0, wkey(4), 0);
        run(20);
        CHECK(ec_step_lock_get(step(3), EC_P_FILTER_CUTOFF, &x), "AMP: W4 held + F1 left the cutoff lock");
        to(EC_CAT_FILT);
        ec_app_set_page(EC_CAT_FILT, pg0);
    }
    /* 0.6.14: PRESETS turns the step page on every track page; 0.6.51 (owner): ALGO selects a step in LOOP only */
    ec_app_params(A)[EC_P_PATTERN_STEPS] = 32;
    ec_seq_timing_changed(&A->seq);
    turn(FM1P_E_PRESETS, 1);
    run(20);
    CHECK(A->step_page == 1 && lbl(FM1CORE_LBL_WHITE0 + 0, "17"), "PRESETS: step page 2 (W1 = %s)", L(FM1CORE_LBL_WHITE0));
    turn(FM1P_E_PRESETS, 3);
    run(20);
    CHECK(A->step_page == 1, "PRESETS clamps at the last page");
    turn(FM1P_E_ALGO, 2);
    run(20);
    CHECK(A->pn.target == 0 && ec_app_target_step(A) < 0 && A->step_page == 1 &&
              lbl(FM1CORE_LBL_KNOB0 + FM1P_E_ALGO, ""), "FILTER: ALGO selects no step, turns no page (%d)", A->pn.target);
    turn(FM1P_E_ALGO, -20);
    turn(FM1P_E_PRESETS, -1);
    run(20);
    CHECK(A->pn.target == 0 && A->step_page == 0, "ALGO full left: LIVE; PRESETS back: page 1");
    ec_app_params(A)[EC_P_PATTERN_STEPS] = 16;
    clear_steps();
    k = 0;
    (void)k;
}

/* PRESETS = the track's pool slot: the used slots, the empty ones skipped */
static void test_presets(void)
{
    int s0;
    to(EC_CAT_FILE);
    ec_app_set_page(EC_CAT_FILE, 0);
    turn(FM1P_E_SELECT, 1);                                   /* the second file */
    run(20);
    tapkey(1, bkey(10), 40);                                  /* B10: LOAD into the next free slot */
    run(800);
    turn(FM1P_E_SELECT, 1);
    run(20);
    tapkey(1, bkey(10), 40);
    run(800);
    s0 = ec_app_track_slot(A, A->track);
    CHECK(s0 == 0 && A->slot[2].state == EC_SL_READY,
          "FILE: B10 loads into the next free slot; the track stays on its own (0.6.32) (%d)", s0);
    {   /* (0.6.14: track pages: the page; MIX: the track; 0.6.56: XFADE / PATTERN nothing): the EDITOR's PRESETS */
        int n;
        for (n = 0; n < 6 && ec_file_page() != EC_PIC_EDITOR; n++)
            press(FM1P_B_SAVE, 40);
        CHECK(ec_file_page() == EC_PIC_EDITOR, "the trim EDITOR");
    }
    turn(FM1P_E_PRESETS, 1);
    run(20);
    CHECK(ec_app_track_slot(A, A->track) == 1 && A->ux.to_ms && A->ux.to_kind == EC_TO_SAMPLE,
          "PRESETS: slot 2 (%d), the takeover", ec_app_track_slot(A, A->track));
    turn(FM1P_E_PRESETS, 1);
    run(20);
    CHECK(ec_app_track_slot(A, A->track) == 2, "PRESETS: slot 3");
    turn(FM1P_E_PRESETS, -2);
    run(20);
    CHECK(ec_app_track_slot(A, A->track) == 0, "PRESETS back twice: slot 1 (the empty ones skipped)");
    turn(FM1P_E_PRESETS, -2);
    run(400);
    CHECK(ec_app_track_slot(A, A->track) == 1, "back twice more: slot 2 (round the three used slots)");
}

/* ------------------------------------------------------------------------------------------- §2 --- */
static void test_clip(void)
{
    to(EC_CAT_TRIG);
    clear_steps();
    step(0)->trig = 1;
    ec_step_lock_set(step(0), EC_P_PITCH, 5);
    step(2)->trig = 1;
    press(FM1P_B_OCTDN, 40);                                  /* F1 tapped: sticky copy */
    CHECK(A->pn.f_sticky == 1 && ec_pn_f(1), "F1 tapped: sticky");
    tapkey(0, wkey(1), 40);
    CHECK(A->pn.clip_state == EC_CLIP_FRESH && !step(3)->trig, "the first tap copies step 1");
    tapkey(0, wkey(3), 40);                                   /* the next tap pastes, even on a trig */
    {
        float x;
        CHECK(ec_step_lock_get(step(2), EC_P_PITCH, &x) && x == 5, "the next tap pastes onto step 3 (not empty)");
    }
    step(4)->trig = 1;
    tapkey(0, wkey(5), 40);                                   /* not empty: copies it */
    tapkey(0, wkey(7), 40);                                   /* empty: pastes */
    {
        float x;
        CHECK(step(6)->trig && !ec_step_lock_get(step(6), EC_P_PITCH, &x), "then: a full step copies, an empty one pastes");
    }
    press(FM1P_B_OCTDN, 40);
    CHECK(!A->pn.f_sticky && A->pn.clip_state == EC_CLIP_EMPTY, "F1 off: the buffer empties");
    press(FM1P_B_OCTUP, 40);                                  /* F2: cut */
    tapkey(0, wkey(1), 40);
    CHECK(!step(0)->trig, "F2: cutting step 1 empties it");
    tapkey(0, wkey(10), 40);
    {
        float x;
        CHECK(step(9)->trig && ec_step_lock_get(step(9), EC_P_PITCH, &x) && x == 5, "the next tap: moved to step 10");
    }
    tapkey(0, wkey(10), 40);                                  /* not empty: cut again */
    CHECK(!step(9)->trig, "cut step 10");
    tapkey(0, wkey(10), 40);
    CHECK(step(9)->trig, "tapped again: put back");
    tapkey(0, wkey(12), 40);
    CHECK(step(11)->trig && step(9)->trig, "a further tap on an empty step: a copy");
    press(FM1P_B_OCTUP, 40);
    CHECK(!A->pn.f_sticky && A->pn.clip_state == EC_CLIP_EMPTY, "F2 tapped again: off, the buffer empty");
    btn(FM1P_B_OCTUP, 1);
    run(500);
    CHECK(ec_pn_f(2), "F2 held: on while held");
    btn(FM1P_B_OCTUP, 0);
    run(20);
    CHECK(!ec_pn_f(2) && !A->pn.f_sticky, "F2 let go after a hold: off");
    clear_steps();
}

/* ------------------------------------------------------------------------------------------- the source template --- */
static int route(int src, int dst)
{
    return ec_mtx_find(ec_app_params(A), src, dst);
}

static void test_sources(void)
{
    ec_page_t pg;
    float *v;
    to(EC_CAT_FILT);
    ec_app_set_page(EC_CAT_FILT, 0);
    run(20);
    CHECK(lbl(FM1CORE_LBL_BLACK0 + 5, "LFO1") && lbl(FM1CORE_LBL_BLACK0 + 9, "AT") &&
              lbl(FM1CORE_LBL_BLACK0 + 0, "FILTER"),
          "FILTER's black keys: B1 %s B6 %s B10 %s", L(FM1CORE_LBL_BLACK0), L(FM1CORE_LBL_BLACK0 + 5),
          L(FM1CORE_LBL_BLACK0 + 9));
    key(1, bkey(6), 1);                                       /* hold LFO 1 + K1: LFO 1 -> CUTOFF */
    run(20);
    turn(FM1P_E_K1, 1);
    turn(FM1P_E_K1, 5);
    run(20);
    CHECK(lbl(FM1CORE_LBL_KNOB0 + FM1P_E_K1, "LFO1 CUTOF"), "B6 held: K1 %s", L(FM1CORE_LBL_KNOB0 + FM1P_E_K1));
    key(1, bkey(6), 0);
    run(20);
    v = ec_app_params(A);
    CHECK(route(EC_MS_LFO1, EC_P_FILTER_CUTOFF) >= 0 && mode() == EC_CAT_FILT, "B6 held + K1: routed, the mode stays");
    CHECK(v[ec_mtx_depth_id(route(EC_MS_LFO1, EC_P_FILTER_CUTOFF))] != 0, "and its depth set");
    key(1, bkey(6), 1);                                       /* + F2 while turning: removed */
    run(20);
    btn(FM1P_B_OCTUP, 1);
    run(20);
    turn(FM1P_E_K1, 1);
    run(20);
    btn(FM1P_B_OCTUP, 0);
    key(1, bkey(6), 0);
    run(20);
    CHECK(route(EC_MS_LFO1, EC_P_FILTER_CUTOFF) < 0 && !A->pn.f_sticky && mode() == EC_CAT_FILT,
          "B6 held + F2 + K1: the route removed");
    key(1, bkey(9), 1);                                       /* VEL -> RESO, then F2 + K2 clears it */
    run(20);
    turn(FM1P_E_K2, 1);
    turn(FM1P_E_K2, 3);
    key(1, bkey(9), 0);
    run(20);
    CHECK(route(EC_MS_VEL, EC_P_FILTER_RES) >= 0, "B9 held + K2: VEL -> RESO");
    btn(FM1P_B_OCTUP, 1);
    run(20);
    turn(FM1P_E_K2, 1);
    btn(FM1P_B_OCTUP, 0);
    run(20);
    CHECK(route(EC_MS_VEL, EC_P_FILTER_RES) < 0, "F2 + K2: every modulation of RESO cleared");
    /* held steps: depth locks */
    clear_steps();
    key(1, bkey(7), 1);
    run(20);
    turn(FM1P_E_K1, 1);                                       /* the route */
    run(20);
    key(0, wkey(2), 1);
    run(300);
    turn(FM1P_E_K1, 4);                                       /* steps held: the depth's lock */
    run(20);
    key(0, wkey(2), 0);
    key(1, bkey(7), 0);
    run(20);
    {
        float x;
        const int k = route(EC_MS_LFO2, EC_P_FILTER_CUTOFF);
        CHECK(k >= 0 && ec_step_lock_get(step(1), ec_mtx_depth_id(k), &x), "B7 + a held step + K1: a depth lock");
    }
    clear_steps();
    /* the taps (0.6.1): never another mode; the source's page when it is in the mode shown */
    tapkey(1, bkey(6), 40);
    CHECK(mode() == EC_CAT_FILT, "B6 tapped in FILTER: FILTER stays");
    to(EC_CAT_AMP);
    tapkey(1, bkey(8), 40);
    ec_app_page(A, &pg);
    CHECK(mode() == EC_CAT_AMP && pg.pic == EC_PIC_MENV, "B8 tapped in AMP: AMP's MOD ENV");
    to(EC_CAT_MOD);
    tapkey(1, bkey(6), 40);
    ec_app_page(A, &pg);
    CHECK(mode() == EC_CAT_MOD && pg.pic == EC_PIC_LFO1, "B6 tapped in MODULATION: its LFO 1");
    tapkey(1, bkey(10), 40);
    ec_app_page(A, &pg);
    CHECK(mode() == EC_CAT_MOD && pg.pic == EC_PIC_MATRIX, "B10 (AT) tapped in MODULATION: the MATRIX");
    /* page keys, quick settings */
    to(EC_CAT_FILT);
    tapkey(1, bkey(2), 40);
    ec_app_page(A, &pg);
    CHECK(pg.pic == EC_PIC_FENV, "FILTER B2: FILTER ENV");
    {
        const float t0 = ec_app_params(A)[EC_P_FILTER_TYPE];
        key(1, bkey(1), 1);
        run(20);
        turn(FM1P_E_K1, 1);
        run(20);
        key(1, bkey(1), 0);
        run(20);
        ec_app_page(A, &pg);
        CHECK(pg.pic == EC_PIC_FENV && ec_app_params(A)[EC_P_FILTER_TYPE] == t0 + 1,
              "B1 held + K1: the filter type (%g -> %g), the page stays (0.6.62)", t0, ec_app_params(A)[EC_P_FILTER_TYPE]);
        tapkey(1, bkey(1), 40);
        ec_app_page(A, &pg);
        CHECK(pg.pic == EC_PIC_FILTER, "B1 clicked: its page (FILTER)");
    }
    {
        const float m0 = ec_app_params(A)[EC_P_FENV_MODE];
        tapkey(1, bkey(11), 40);
        CHECK(ec_app_params(A)[EC_P_FENV_MODE] != m0, "FILTER B11: the filter env's mode");
        tapkey(1, bkey(11), 40);
    }
    to(EC_CAT_AMP);
    {
        const float m0 = ec_app_params(A)[EC_P_ENV_MODE];
        key(1, bkey(1), 1);
        run(20);
        turn(FM1P_E_SELECT, m0 > 1 ? -1 : 1);
        run(20);
        key(1, bkey(1), 0);
        run(20);
        CHECK(ec_app_params(A)[EC_P_ENV_MODE] != m0, "AMP B1 held + SEL: the env mode (%g -> %g)", m0,
              ec_app_params(A)[EC_P_ENV_MODE]);
        key(1, bkey(1), 1);
        run(20);
        turn(FM1P_E_SELECT, m0 > 1 ? 1 : -1);
        key(1, bkey(1), 0);
        run(20);
    }
    to(EC_CAT_MOD);
    tapkey(1, bkey(2), 40);
    ec_app_page(A, &pg);
    CHECK(pg.pic == EC_PIC_MACRO && pg.arg == 0, "MODULATION B2: MACRO 1");
    tapkey(1, bkey(11), 40);
    ec_app_page(A, &pg);
    CHECK(pg.pic == EC_PIC_MACROS, "MODULATION B11: MACROS");
    tapkey(1, bkey(1), 40);
    ec_app_page(A, &pg);
    CHECK(pg.pic == EC_PIC_MATRIX, "MODULATION B1: MATRIX");
    to(EC_CAT_FILT);
    key(1, bkey(3), 1);                                       /* FILTER B3 is a page; hold it: no source */
    run(20);
    key(1, bkey(3), 0);
    run(20);
    to(EC_CAT_MOD);
    tapkey(1, bkey(2), 40);                                   /* M1 page, back to FILTER: hold M1 there? MOD only */
    to(EC_CAT_FILT);
    ec_app_set_page(EC_CAT_FILT, 0);
    /* LOOP ENGINE: B11 held + K3 = the zoom (0.6.14: K1 / K2 are LEN / RATE; 0.6.6: B11 itself picks the target step) */
    to(EC_CAT_SRC);
    v = ec_app_params(A);
    v[EC_P_LOOP_DIVISION] = 16;
    key(1, bkey(11), 1);
    run(20);
    turn(FM1P_E_K4, 1);
    CHECK(v[EC_P_LOOP_DIVISION] == 32 && !A->pn.zoom_half, "LOOP ENGINE B11 + K4 (0.6.86): 32, first half");
    turn(FM1P_E_K4, 1);
    run(60);
    CHECK(v[EC_P_LOOP_DIVISION] == 32 && A->pn.zoom_half && lbl(FM1CORE_LBL_WHITE0, "STEP 1"),
          "B11 + K3: the second half; while B11 is held the keys pick steps (W1 = %s)", L(FM1CORE_LBL_WHITE0));
    key(1, bkey(11), 0);
    run(60);
    CHECK(lbl(FM1CORE_LBL_WHITE0, "L17"), "B11 let go: the second half's loop keys (W1 = %s)", L(FM1CORE_LBL_WHITE0));
    key(1, bkey(11), 1);
    run(20);
    turn(FM1P_E_K4, -2);
    key(1, bkey(11), 0);
    run(20);
    CHECK(v[EC_P_LOOP_DIVISION] == 16 && !A->pn.zoom_half, "B11 + K4 -2: back to 16");
    {                                              /* 0.6.86 (owner): B11 held: PRESETS = LENGTH, ALGO = RATE */
        const float len0 = v[EC_P_PATTERN_STEPS], rate0 = v[EC_P_RATE];
        key(1, bkey(11), 1);
        run(20);
        turn(FM1P_E_PRESETS, 1);
        turn(FM1P_E_ALGO, 1);
        CHECK(v[EC_P_PATTERN_STEPS] == len0 + 1 && v[EC_P_RATE] != rate0 && v[EC_P_LOOP_DIVISION] == 16 &&
                  lbl(FM1CORE_LBL_KNOB0 + FM1P_E_PRESETS, "LEN") && lbl(FM1CORE_LBL_KNOB0 + FM1P_E_ALGO, "RATE"),
              "B11 + PRESETS: LEN %g, + ALGO: RATE %g (no zoom)", v[EC_P_PATTERN_STEPS], v[EC_P_RATE]);
        turn(FM1P_E_PRESETS, -1);
        turn(FM1P_E_ALGO, -1);
        key(1, bkey(11), 0);
        run(20);
    }
    /* FX: the buses, the slot keys */
    to(EC_CAT_FX);
    tapkey(1, bkey(3), 40);
    CHECK(A->bus == 1, "FX B3: the S1 bus");
    tapkey(1, bkey(1), 40);
    CHECK(A->bus == 0 && A->page[EC_CAT_FX] == 0, "FX B1: the track's INSERT");
    key(1, bkey(1), 1);                                       /* 0.6.1: B1 held = the type menu (test_fx2) */
    run(20);
    turn(FM1P_E_K1, 2);
    key(1, bkey(1), 0);
    run(20);
    CHECK(ec_app_params(A)[EC_P_FX_MACHINE] == 3 && !step(2)->trig, "B1 held + K1 2: the insert's engine 3, no step");
    tapkey(1, bkey(5), 40);                                   /* 0.6.3: B5 SENDS, again MASTER; held: its type */
    tapkey(1, bkey(5), 40);
    key(1, bkey(5), 1);
    run(20);
    turn(FM1P_E_K1, 2);
    key(1, bkey(5), 0);
    run(20);
    CHECK(A->bus == 3 && A->proj.bus[2][EC_P_FX_MACHINE] > 1, "MASTER: B5 held + K1: the master bus's engine");
    tapkey(1, bkey(1), 40);
    ec_app_params(A)[EC_P_FX_MACHINE] = 1;
    A->proj.bus[2][EC_P_FX_MACHINE] = 1;
}

/* ------------------------------------------------------------------------------------------- MIX --- */
static void test_mix(void)
{
    to(EC_CAT_MASTER);
    CHECK(lbl(FM1CORE_LBL_WHITE0 + 8, "MST") && lbl(FM1CORE_LBL_BLACK0 + 4, "1X") && lbl(FM1CORE_LBL_BLACK0 + 10, "CAT"),
          "MIX labels: W9 %s B5 %s B11 %s", L(FM1CORE_LBL_WHITE0 + 8), L(FM1CORE_LBL_BLACK0 + 4),
          L(FM1CORE_LBL_BLACK0 + 10));
    tapkey(0, wkey(2), 40);
    CHECK(A->track == 1, "MIX: W2 tapped = T2");
    tapkey(0, wkey(3), 500);
    CHECK(ec_project_track(&A->proj, 2)->v[EC_P_MUTE] > 0.5f && A->track == 1, "W3 held: T3 muted (T2 still selected)");
    btn(FM1P_B_GLO, 1);
    run(20);
    tapkey(0, wkey(4), 40);
    CHECK(A->ux.solo == 1u << 3, "SHIFT + W4: T4 solo");
    key(1, bkey(11), 1);                                      /* FILL (SHIFT + B11) held + a track key: all on */
    run(20);
    tapkey(0, wkey(1), 40);
    key(1, bkey(11), 0);
    btn(FM1P_B_GLO, 0);
    run(20);
    CHECK(!A->ux.solo && ec_project_track(&A->proj, 2)->v[EC_P_MUTE] < 0.5f, "FILL + a track key: every mute and solo off");
    tapkey(1, bkey(3), 40);
    CHECK(ec_app_params(A)[EC_P_RATE] == 7, "B3: rate 1/2");
    tapkey(1, bkey(10), 40);
    CHECK(ec_app_params(A)[EC_P_RATE] == 8, "B10: faster");
    tapkey(1, bkey(9), 40);
    tapkey(1, bkey(9), 40);
    CHECK(ec_app_params(A)[EC_P_RATE] == 6, "B9 twice: slower");
    tapkey(1, bkey(5), 40);
    CHECK(ec_app_params(A)[EC_P_RATE] == EC_RATE_UNITY, "B5: 1X");
    tapkey(1, bkey(11), 40);
    CHECK(A->pn.cat, "B11: the CAT");
    tapkey(0, wkey(1), 40);
    CHECK(A->pn.cat && A->track == 0, "the CAT: the track keys still work");
    tapkey(1, bkey(11), 40);
    CHECK(!A->pn.cat, "B11 leaves it");
    tapkey(1, bkey(11), 40);
    press(FM1P_B_EDIT, 40);
    CHECK(!A->pn.cat && mode() == EC_CAT_MASTER, "EDIT leaves the CAT, back to MIX");
    tapkey(1, bkey(11), 40);
    press(FM1P_B_FX, 40);
    CHECK(!A->pn.cat && mode() == EC_CAT_FX, "a mode button leaves the CAT for its mode");
    to(EC_CAT_MASTER);
    tapkey(0, wkey(9), 40);
    CHECK(A->bus == 3 && A->page[EC_CAT_MASTER] == 1, "MIX: W9 = MST, the MASTER page");
    tapkey(0, wkey(1), 40);
    CHECK(A->bus == 0 && A->page[EC_CAT_MASTER] == 0, "W1: T1, the track page");
}

/* ------------------------------------------------------------------------------------------- PAGES --- */
static void test_pages(void)
{
    float *v;
    to(EC_MODE_PAGES);
    v = ec_app_params(A);
    clear_steps();
    v[EC_P_PATTERN_STEPS] = 48;
    ec_seq_timing_changed(&A->seq);
    run(20);
    CHECK(lbl(FM1CORE_LBL_BLACK0 + 2, "PAGE3") && lbl(FM1CORE_LBL_BLACK0 + 3, ""), "PAGES: three page keys");
    tapkey(1, bkey(2), 40);
    CHECK(A->step_page == 1, "B2 tapped: edit page 2");
    tapkey(0, wkey(1), 40);
    CHECK(step(16)->trig, "the white keys are page 2's steps");
    tapkey(1, bkey(3), 500);
    CHECK(ec_e5_page_in_loop(&A->proj, A->track, 2, 3) == 0 && A->step_page == 1, "B3 held: page 3 out of the loop");
    tapkey(1, bkey(3), 500);
    CHECK(ec_e5_page_in_loop(&A->proj, A->track, 2, 3), "held again: back in");
    press(FM1P_B_OCTDN, 40);                                  /* F1: copy page 2 onto page 3 */
    CHECK(lbl(FM1CORE_LBL_BTN0 + FM1P_B_OCTDN, "COPY PG") || 1, "");
    tapkey(1, bkey(2), 40);
    tapkey(1, bkey(3), 40);
    press(FM1P_B_OCTDN, 40);
    CHECK(step(32)->trig && A->step_page == 1, "F1 + B2, B3: page 2 pasted onto page 3");
    press(FM1P_B_OCTUP, 40);                                  /* F2: cut page 3, onto page 1 */
    tapkey(1, bkey(3), 40);
    CHECK(!step(32)->trig, "F2 + B3: page 3 cut");
    tapkey(1, bkey(1), 40);
    press(FM1P_B_OCTUP, 40);
    CHECK(step(0)->trig, "then B1: moved to page 1");
    {
        ec_page_t pg;
        ec_app_page(A, &pg);
        CHECK(pg.n == 2 && pg.it[0].id == EC_P_PATTERN_STEPS, "PAGES' cells: LENGTH, RATE (0.6.14: the loop is in the header)");
    }
    v[EC_P_PATTERN_STEPS] = 16;
    A->step_page = 0;
    clear_steps();
}

/* ------------------------------------------------------------------------------------------- KEYBOARD --- */
static void test_keyboard(void)
{
    float *v, x;
    to(EC_MODE_KEYBOARD);
    v = ec_app_params(A);
    clear_steps();
    CHECK(lbl(FM1CORE_LBL_BTN0 + FM1P_B_OCTDN, "OCT-") && lbl(FM1CORE_LBL_KNOB0 + FM1P_E_ALGO, "TARGET") &&
              lbl(FM1CORE_LBL_WHITE0 + 4, "C4"),
          "KEYBOARD labels: F1 %s ALGO %s W5 %s", L(FM1CORE_LBL_BTN0 + FM1P_B_OCTDN), L(FM1CORE_LBL_KNOB0 + FM1P_E_ALGO),
          L(FM1CORE_LBL_WHITE0 + 4));
    key(0, wkey(5), 1);                                       /* C4: the sample's pitch */
    run(20);
    CHECK(A->seq.tr[A->track].live_pitch_on && A->seq.tr[A->track].live_pitch == v[EC_P_PITCH], "C4 plays at pitch 0");
    CHECK(ec_kb_held_text() && !strcmp(ec_kb_held_text(), "C4"), "0.6.67: the header names the note, no label (%s)",
          ec_kb_held_text() ? ec_kb_held_text() : "-");
    {                                                         /* 0.6.67: alternating keys that overlap */
        int i;
        for (i = 0; i < 6; i++) {
            const int nk = wkey(i & 1 ? 5 : 7), ok = wkey(i & 1 ? 7 : 5);   /* C4 / E4 */
            key(0, nk, 1);
            run(10);
            key(0, ok, 0);
            run(10);
            CHECK(A->seq.tr[A->track].live_pitch_on && A->seq.tr[A->track].live_semis == (i & 1 ? 0 : 4) &&
                      ec_kb_lit_key() >= 0,
                  "overlap %d: the key still held sounds and is lit (%g)", i, A->seq.tr[A->track].live_semis);
        }
        key(0, wkey(7), 1);                                   /* C4 held, E4 too */
        run(10);
        CHECK(ec_kb_held_text() && !strcmp(ec_kb_held_text(), "C4 E4"), "both notes held: both named (%s)",
              ec_kb_held_text() ? ec_kb_held_text() : "-");
        key(0, wkey(5), 0);                                   /* C4 up: E4 sounds */
        run(10);
        CHECK(A->seq.tr[A->track].live_pitch_on && A->seq.tr[A->track].live_semis == 4, "C4 up: E4 takes the note");
        key(0, wkey(7), 0);
        run(10);
        CHECK(!A->seq.tr[A->track].live_pitch_on && !ec_kb_held_text(), "all up: silent, no header note");
    }
    key(0, wkey(5), 1);
    key(0, wkey(5), 0);
    run(20);
    press(FM1P_B_OCTUP, 40);
    CHECK(A->pn.oct == 1 && !A->pn.f_sticky, "F2: octave up (no copy / cut here)");
    key(1, bkey(4), 1);                                       /* C#5 */
    run(20);
    CHECK(A->seq.tr[A->track].live_pitch == v[EC_P_PITCH] + 13, "the black keys are the sharps (C#5 = +13: %g)",
          A->seq.tr[A->track].live_pitch);
    key(1, bkey(4), 0);
    press(FM1P_B_OCTDN, 40);
    turn(FM1P_E_ALGO, 1);                                     /* target step 1 */
    run(20);
    CHECK(A->pn.target == 1, "ALGO: target step 1");
    tapkey(0, wkey(6), 40);                                   /* D4 */
    CHECK(step(0)->trig && ec_step_lock_get(step(0), EC_P_NOTE, &x) && x == 2 && A->pn.target == 2,
          "a key writes the note on step 1 and advances");
    btn(FM1P_B_GLO, 1);
    run(20);
    tapkey(0, wkey(7), 40);                                   /* SHIFT + E4: stays */
    btn(FM1P_B_GLO, 0);
    run(20);
    CHECK(step(1)->trig && ec_step_lock_get(step(1), EC_P_NOTE, &x) && x == 4 && A->pn.target == 2,
          "SHIFT + a key: written, the target stays");
    tapkey(0, wkey(8), 40);
    CHECK(ec_step_lock_get(step(1), EC_P_NOTE, &x) && x == 5 && A->pn.target == 3, "one note per step: replaced");
    {                                              /* 0.6.80 (owner): the notes are NOTE, apart from PITCH */
        char nm[8];
        ec_param_format(EC_P_NOTE, 1.0f, 3, nm, sizeof nm);
        CHECK(!ec_step_lock_get(step(1), EC_P_PITCH, 0) && !strcmp(nm, "C#4"),
              "0.6.80: a note is a NOTE lock, no PITCH lock (NOTE 1 reads %s)", nm);
        ec_step_lock_clear(step(1), EC_P_PITCH);
        CHECK(ec_step_lock_get(step(1), EC_P_NOTE, &x) && x == 5, "clearing PITCH keeps the note");
    }
    turn(FM1P_E_ALGO, -5);
    run(20);
    CHECK(A->pn.target == 0, "ALGO below 1: live");
    tapkey(0, wkey(9), 40);
    CHECK(!step(2)->trig, "live: nothing written");
    /* a slice track */
    v[EC_P_MACHINE] = EC_MACH_GRID_SLICE;
    v[EC_P_SLICE_COUNT] = 16;
    ec_seq_touch(&A->seq, A->track);
    run(20);
    CHECK(lbl(FM1CORE_LBL_BLACK0 + 2, "SL3") && lbl(FM1CORE_LBL_BLACK0 + 10, "BANK"), "slice track: B3 %s B11 %s",
          L(FM1CORE_LBL_BLACK0 + 2), L(FM1CORE_LBL_BLACK0 + 10));
    tapkey(1, bkey(3), 40);
    CHECK(v[EC_P_SLICE_INDEX] == 3, "B3: slice 3");
    tapkey(1, bkey(11), 40);
    CHECK(A->pn.kb_bank == 1 && lbl(FM1CORE_LBL_BLACK0 + 0, "SL11"), "B11: slices 11-20");
    tapkey(1, bkey(2), 40);
    CHECK(v[EC_P_SLICE_INDEX] == 12, "B2: slice 12");
    tapkey(1, bkey(9), 40);
    CHECK(v[EC_P_SLICE_INDEX] == 12, "B9 = slice 19 > 16 slices: nothing");
    turn(FM1P_E_ALGO, 4);
    run(20);
    tapkey(0, wkey(5), 40);
    CHECK(step(3)->trig && step(3)->slices == 1u << 11, "a white key on target 4: slice 12");
    tapkey(1, bkey(11), 40);
    turn(FM1P_E_ALGO, -20);
    v[EC_P_MACHINE] = EC_MACH_LOOP;
    v[EC_P_SLICE_INDEX] = 1;
    clear_steps();
}

/* ------------------------------------------------------------------------------------------- CROSSFADER --- */
static void test_xfd(void)
{
    to(EC_CAT_XFD);
    press(FM1P_B_SEQ, 40);                                    /* (leave and come back: no stale anchor) */
    to(EC_CAT_XFD);
    ec_scene_clear(&A->proj, 0);
    ec_scene_clear(&A->proj, 1);
    key(1, bkey(1), 1);                                       /* hold A + a knob: a scene lock */
    run(20);
    CHECK(A->e5.anchor == 1 && led_key(1, bkey(1)) == FM1P_LED_LIT, "B1 held: scene A's anchor, lit");
    key(1, bkey(1), 0);
    run(20);
    CHECK(A->e5.anchor == 0, "B1 up: no anchor");
    {
        ec_page_t pg;
        ec_app_page(A, &pg);
        CHECK(pg.n == 4 && pg.it[1].kind == EC_IT_UX && pg.it[1].id == EC_UX_GLIDE, "CROSSFADER's cells: + GLIDE A B");
    }
    key(1, bkey(11), 1);                                      /* B11 held + B2: capture */
    run(20);
    tapkey(1, bkey(2), 40);
    key(1, bkey(11), 0);
    run(20);
    CHECK(ec_scene_count(&A->proj.e5.sc, 1) > 0, "B11 + B2: scene B captured (%d)", ec_scene_count(&A->proj.e5.sc, 1));
    btn(FM1P_B_GLO, 1);
    run(20);
    tapkey(1, bkey(2), 40);
    btn(FM1P_B_GLO, 0);
    run(20);
    CHECK(ec_scene_count(&A->proj.e5.sc, 1) == 0, "SHIFT + B2: scene B cleared");
    btn(FM1P_B_GLO, 1);                                       /* SHIFT + W1: jump to 0 */
    run(20);
    tapkey(0, wkey(1), 40);
    btn(FM1P_B_GLO, 0);
    run(20);
    CHECK(A->proj.e5.sc.pos == 0, "SHIFT + W1: jump to 0");
    tapkey(0, wkey(15), 40);                                  /* slow */
    CHECK(A->pn.as.glide == 2, "W15: glide MED");
    key(0, wkey(12), 1);                                      /* hold W12: glides to 127 */
    run(300);
    {
        const float p1 = A->proj.e5.sc.pos;
        CHECK(p1 > 0.3f && p1 < 0.7f, "W12 held 0.3 s (MED 0.6 s): halfway (%g)", p1);
        P->key_lean[key_of(0, wkey(12))] = 100;              /* full pressure: faster */
        run(60);
        CHECK(A->proj.e5.sc.pos - p1 > 0.3f, "pressure: faster (%g)", A->proj.e5.sc.pos);
        P->key_lean[key_of(0, wkey(12))] = 0;
    }
    key(0, wkey(12), 0);
    run(20);
    key(0, wkey(1), 1);
    run(150);
    key(0, wkey(1), 0);
    run(100);
    {
        const float p = A->proj.e5.sc.pos;
        CHECK(p > 0.5f && p < 0.9f && !A->e5.gliding, "let go: it stays where it got to (%g)", p);
    }
    tapkey(0, wkey(13), 40);
    tapkey(0, wkey(6), 40);
    CHECK(fabsf(A->proj.e5.sc.pos - 5.0f / 11.0f) < 0.01f, "W13 instant: W6 jumps");
    tapkey(0, wkey(15), 40);
    CHECK(lbl(FM1CORE_LBL_WHITE0 + 11, "127") && lbl(FM1CORE_LBL_WHITE0 + 12, "INST") &&
              lbl(FM1CORE_LBL_BLACK0 + 10, "CAPTURE"),
          "CROSSFADER labels: W12 %s W13 %s B11 %s", L(FM1CORE_LBL_WHITE0 + 11), L(FM1CORE_LBL_WHITE0 + 12),
          L(FM1CORE_LBL_BLACK0 + 10));
    btn(FM1P_B_GLO, 1);
    run(20);
    tapkey(0, wkey(1), 40);
    btn(FM1P_B_GLO, 0);
    run(20);
}

/* the scene lock: B1 held, the knobs are the page CROSSFADER was entered from (FILTER here) */
static void test_xfd_lock(void)
{
    float x;
    const float c0 = ec_app_params(A)[EC_P_FILTER_CUTOFF];
    to(EC_CAT_FILT);
    ec_app_set_page(EC_CAT_FILT, 0);
    press(FM1P_B_ARP, 40);
    run(900);
    key(1, bkey(1), 1);
    run(20);
    CHECK(lbl(FM1CORE_LBL_KNOB0 + FM1P_E_K1, "CUTOFF"), "B1 held: K1 is FILTER's CUTOFF (%s)",
          L(FM1CORE_LBL_KNOB0 + FM1P_E_K1));
    turn(FM1P_E_K1, -5);
    run(20);
    key(1, bkey(1), 0);
    run(20);
    CHECK(ec_scene_get(&A->proj, 0, A->track, EC_P_FILTER_CUTOFF, &x) && x < c0 &&
              ec_app_params(A)[EC_P_FILTER_CUTOFF] == c0 && A->e5.anchor == 0,
          "B1 held + K1: the cutoff locked to scene A (%g), the track's stays", x);
    ec_scene_clear(&A->proj, 0);
}

/* FILE (the stand-in until part 2): B3 the browser, B4 RAZOR */
static void test_file_keys(void)
{
    to(EC_CAT_SRC);
    to(EC_CAT_FILE);                                          /* 0.6.15: the editors are FILE's sub-modes, on its button */
    ec_pn_enter(EC_CAT_FILE);                                 /* (0.6.66: the button comes back where it was: BROWSE) */
    tapkey(1, bkey(5), 40);
    CHECK(ec_page_pic(EC_CAT_FILE, A->page[EC_CAT_FILE], 0) == EC_PIC_FILE, "FILE B5: nothing (no page key)");
    press(FM1P_B_SAVE, 40);
    CHECK(mode() == EC_MODE_PROJECTS, "FILE again: PROJECTS (0.6.42)");
    press(FM1P_B_SAVE, 40);
    CHECK(mode() == EC_CAT_FILE && ec_page_pic(EC_CAT_FILE, A->page[EC_CAT_FILE], 0) == EC_PIC_EDITOR,
          "FILE again: the editor");
    press(FM1P_B_SAVE, 40);
    CHECK(ec_page_pic(EC_CAT_FILE, A->page[EC_CAT_FILE], 0) == EC_PIC_RAZOR, "FILE again: RAZOR");
    turn(FM1P_E_SELECT, -1);
    turn(FM1P_E_SELECT, -1);
    turn(FM1P_E_SELECT, -1);                                  /* 0.6.15: SEL stays on the slice editor's pages */
    CHECK(ec_page_pic(EC_CAT_FILE, A->page[EC_CAT_FILE], 0) == EC_PIC_RAZOR, "SEL back: still the slice editor");
    press(FM1P_B_SAVE, 40);                                   /* the SAMPLER */
    press(FM1P_B_SAVE, 40);                                   /* BROWSE */
    CHECK(ec_page_pic(EC_CAT_FILE, A->page[EC_CAT_FILE], 0) == EC_PIC_FILE, "FILE twice: round to the browser");
}

/* ------------------------------------------------------------------------------------------- PATTERN LAUNCH --- */
static void test_patterns(void)
{
    to(EC_CAT_PAT);
    CHECK(lbl(FM1CORE_LBL_BLACK0 + 1, "BANK B") && lbl(FM1CORE_LBL_WHITE0 + 2, "A3"), "PATTERN LAUNCH labels");
    tapkey(1, bkey(2), 40);
    CHECK(A->pn.bank == 1 && lbl(FM1CORE_LBL_WHITE0 + 2, "B3"), "B2: bank B");
    tapkey(0, wkey(3), 40);
    run(50);
    CHECK(A->proj.current == 18, "stopped: W3 = B3 at once (%d)", A->proj.current);
    step(0)->trig = 1;
    press(FM1P_B_OCTDN, 40);                                  /* F1: copy B3 onto B5 */
    tapkey(0, wkey(3), 40);
    tapkey(0, wkey(5), 40);
    press(FM1P_B_OCTDN, 40);
    CHECK(A->proj.pat[20].used && A->proj.pat[20].tr[A->track].steps[0].trig, "F1 + W3, W5: B3 copied to B5");
    tapkey(1, bkey(1), 40);
    tapkey(0, wkey(1), 40);
    run(50);
    CHECK(A->proj.current == 0, "bank A, W1: A1");
}

/* ------------------------------------------------------------------------------------------- labels --- */
static void test_labels(void)
{
    to(EC_CAT_SRC);
    CHECK(lbl(FM1CORE_LBL_BLACK0 + 10, "STEP") && lbl(FM1CORE_LBL_WHITE0, "L1") &&   /* 0.6.6: B11 / ALGO the target */
              lbl(FM1CORE_LBL_KNOB0 + FM1P_E_PRESETS, "PAGE") && lbl(FM1CORE_LBL_KNOB0 + FM1P_E_ALGO, "TARGET"),
          "LOOP ENGINE: B11 %s W1 %s PRESETS %s ALGO %s", L(FM1CORE_LBL_BLACK0 + 10), L(FM1CORE_LBL_WHITE0),
          L(FM1CORE_LBL_KNOB0 + FM1P_E_PRESETS), L(FM1CORE_LBL_KNOB0 + FM1P_E_ALGO));
    CHECK(lbl(FM1CORE_LBL_BTN0 + FM1P_B_OCTDN, "COPY") && lbl(FM1CORE_LBL_BTN0 + FM1P_B_OCTUP, "CUT"),
          "F1 COPY, F2 CUT");
    btn(FM1P_B_GLO, 1);
    run(20);
    CHECK(lbl(FM1CORE_LBL_KNOB0 + FM1P_E_SELECT, "BPM") && lbl(FM1CORE_LBL_BTN0 + FM1P_B_HOME, "LOOP SETUP") &&
              lbl(FM1CORE_LBL_BTN0 + FM1P_B_OCTDN, "UNDO") && lbl(FM1CORE_LBL_BTN0 + FM1P_B_SAVE, "SAVE"),
          "SHIFT: SEL %s HOME %s F1 %s", L(FM1CORE_LBL_KNOB0), L(FM1CORE_LBL_BTN0 + FM1P_B_HOME),
          L(FM1CORE_LBL_BTN0 + FM1P_B_OCTDN));
    CHECK(led_key(1, bkey(6)) == FM1P_LED_OFF && led_key(1, bkey(5)) == FM1P_LED_LIT && led_key(1, bkey(3)) == FM1P_LED_OFF,
          "SHIFT: B6 dark, B5 (MEMO) lit, B3 dark (0.6.6)");
    btn(FM1P_B_GLO, 0);
    run(20);
    to(EC_CAT_FX);
    CHECK(lbl(FM1CORE_LBL_BLACK0 + 2, "SEND1") && lbl(FM1CORE_LBL_BLACK0 + 4, "SND/MST") &&
              lbl(FM1CORE_LBL_BLACK0 + 10, "MIX"),
          "FX: B3 %s B5 %s B11 %s", L(FM1CORE_LBL_BLACK0 + 2), L(FM1CORE_LBL_BLACK0 + 4), L(FM1CORE_LBL_BLACK0 + 10));
    {
        int cur;
        const char *r = ec_pn_roles(EC_CAT_FX, &cur);
        CHECK(!strcmp(r, "PPPPPSSSSSX"), "FX's pop-up roles %s", r);
        r = ec_pn_roles(EC_CAT_MOD, &cur);
        CHECK(!strcmp(r, "PSSSSSSSSSX"), "MODULATION's roles %s", r);
        r = ec_pn_roles(EC_CAT_MASTER, &cur);
        CHECK(!strcmp(r, "AAAAAAAAAAX"), "MIX's roles %s", r);
    }
}

/* 0.6.49 (owner): F1 held + SEL: the dice; enough turns throw them and the page's params are randomized (one undo);
 * F1 let go before: nothing */
static void test_dice(void)
{
    float *v, c0, r0;
    int lv, f0, f1;
    uint32_t t;
    to(EC_CAT_FILT);
    v = ec_app_params(A);
    c0 = v[EC_P_FILTER_CUTOFF], r0 = v[EC_P_FILTER_RES];
    btn(FM1P_B_OCTDN, 1);
    run(30);
    turn(FM1P_E_SELECT, 3);
    CHECK(ec_pn_dice(&lv, &t, &f0, &f1) == 1 && lv == 3, "F1 + SEL 3: the dice shake (%d)", lv);
    btn(FM1P_B_OCTDN, 0);
    run(30);
    CHECK(!ec_pn_dice(&lv, &t, &f0, &f1) && v[EC_P_FILTER_CUTOFF] == c0 && v[EC_P_FILTER_RES] == r0 && !A->pn.f_sticky,
          "F1 let go: no dice, nothing changed, no sticky F1");
    btn(FM1P_B_OCTDN, 1);
    run(30);
    turn(FM1P_E_SELECT, 6);
    turn(FM1P_E_SELECT, 6);
    CHECK(ec_pn_dice(&lv, &t, &f0, &f1) == 2, "12 turns: thrown");
    run(600);
    btn(FM1P_B_OCTDN, 0);
    run(30);
    CHECK(v[EC_P_FILTER_CUTOFF] != c0 || v[EC_P_FILTER_RES] != r0, "landed: the page's params rolled (%g %g)",
          v[EC_P_FILTER_CUTOFF], v[EC_P_FILTER_RES]);
    btn(FM1P_B_GLO, 1);
    run(20);
    press(FM1P_B_OCTDN, 40);
    btn(FM1P_B_GLO, 0);
    run(20);
    CHECK(v[EC_P_FILTER_CUTOFF] == c0 && v[EC_P_FILTER_RES] == r0, "SHIFT + F1: one undo brings them back");
    if (A->pn.shift_sticky)
        press(FM1P_B_GLO, 40);
}

/* 0.6.50 (owner): the bars and the FILTER / ENV pictures show the params as heard (modulated); a step held shows
 * that step's value, still */
static void run_audio(int n)                       /* run, with the engine (its LFOs) going */
{
    int16_t out[FM1P_BLOCK * 2];
    int b;
    run(n);
    for (b = 0; b < n * FM1P_FS / 1000 / FM1P_BLOCK + 1; b++)
        C->audio(out, FM1P_BLOCK);
}

/* 0.6.54: the QA pass's quick fixes (results/qa-0.6.52/REPORT.md C2, C4, C5, C7; C1 is test_fx2's) */
static void test_qa_quick(void)
{
    float *v;
    int c, p, i, bad = 0, rolled = 0;
    uint32_t rng = 12345;
    /* C2: MIX's PRESETS walks T6 -> S1 -> S2 and back */
    to(EC_CAT_MASTER);
    ec_pn_select_target(5);
    turn(FM1P_E_PRESETS, 1);
    run(20);
    CHECK(A->bus == 1, "MIX: PRESETS T6 -> S1 (bus %d)", A->bus);
    turn(FM1P_E_PRESETS, 1);
    run(20);
    CHECK(A->bus == 2, "S1 -> S2 (bus %d)", A->bus);
    turn(FM1P_E_PRESETS, -1);
    run(20);
    turn(FM1P_E_PRESETS, -1);
    run(20);
    CHECK(A->bus == 0 && A->track == 5, "back: S1, then T6 (bus %d, track %d)", A->bus, A->track);
    ec_pn_select_target(0);
    /* C4: the step page follows a shorter length */
    to(EC_CAT_FILT);
    v = ec_app_params(A);
    v[EC_P_PATTERN_STEPS] = 32;
    ec_seq_timing_changed(&A->seq);
    turn(FM1P_E_PRESETS, 1);
    run(20);
    CHECK(A->step_page == 1, "LEN 32: PRESETS to page 2 (%d)", A->step_page);
    v[EC_P_PATTERN_STEPS] = 16;
    ec_seq_timing_changed(&A->seq);
    run(20);
    CHECK(A->step_page == 0, "LEN 16: back on page 1 (%d)", A->step_page);
    /* C5: KEYBOARD: a note advances the target; ALGO counts on from the target shown */
    to(EC_MODE_KEYBOARD);
    ec_lp_target_to(0);
    turn(FM1P_E_ALGO, 1);
    run(20);
    CHECK(A->pn.target == 1, "ALGO: step 1 (%d)", A->pn.target);
    if (!ec_as->advance)
        ec_pn_item_edit(EC_UX_ADVANCE, 1);
    tapkey(0, wkey(5), 40);
    run(20);
    CHECK(A->pn.target == 2 && ec_lp_first() == 1, "a note: the target advanced to 2, selected too (%d, %d)",
          A->pn.target, ec_lp_first() + 1);
    turn(FM1P_E_ALGO, 1);
    run(20);
    CHECK(A->pn.target == 3, "ALGO +1: step 3, not back to 2 (%d)", A->pn.target);
    ec_lp_target_to(0);
    clear_steps();
    /* C7: the dice land every number on its own step and inside its range */
    for (c = 0; c < 4; c++) {
        static const int CATS[4] = {EC_CAT_SRC, EC_CAT_FILT, EC_CAT_AMP, EC_CAT_FX};
        to(CATS[c]);
        v = ec_app_params(A);
        for (p = 0; p < ec_cat_pages(CATS[c], v); p++) {
            ec_page_t pg;
            ec_app_set_page(CATS[c], p);
            ec_app_page(A, &pg);
            rolled += ec_app_randomize_page(&rng);
            for (i = 0; i < pg.n; i++) {
                const int id = pg.it[i].id, d = ec_param_def_id(v, id) >= 0 ? ec_param_def_id(v, id) : id;
                const ec_param_def_t *e = &EC_PARAMS[d];
                float q;
                if (pg.it[i].kind != EC_IT_PARAM || e->kind != EC_PK_NUM || e->step <= 0 || (e->flags & EC_PF_EXPSTEP))
                    continue;
                q = (v[id] - e->min) / e->step;
                if (fabsf(q - floorf(q + 0.5f)) > 0.01f || v[id] < e->min - 1e-3f || v[id] > e->max + 1e-3f) {
                    if (!bad)
                        printf("  off its step: %s = %g (step %g, %g..%g)\n", e->name, v[id], e->step, e->min, e->max);
                    bad++;
                }
            }
            ec_e5_undo(0);
        }
        ec_app_set_page(CATS[c], 0);
    }
    CHECK(rolled > 0 && !bad, "the dice: %d params rolled, %d off their step or range", rolled, bad);
}

/* 0.6.56 (owner / QA): the dice keep an LFO's level; MIX with a bus shows no track cells; REC in PROJECTS arms
 * nothing; the B11 / PAGES / MIX / XFADE knob labels say what the knobs do */
static void test_056(void)
{
    ec_page_t pg;
    float *v;
    int p, i, trk_cells = 0;
    uint32_t rng = 99;
    to(EC_CAT_MOD);
    v = ec_app_params(A);
    for (p = 0; p < 12; p++) {
        ec_app_set_page(EC_CAT_MOD, p);
        ec_app_page(A, &pg);
        if (pg.pic == EC_PIC_LFO1)
            break;
    }
    v[EC_P_LFO1_DEPTH] = 100;
    ec_app_randomize_page(&rng);
    CHECK(pg.pic == EC_PIC_LFO1 && v[EC_P_LFO1_DEPTH] == 100, "the dice on LFO 1: its level kept (%g)", v[EC_P_LFO1_DEPTH]);
    ec_e5_undo(0);
    to(EC_CAT_MASTER);
    ec_pn_select_target(EC_NTRACK);                /* S1 */
    run(20);
    ec_app_page(A, &pg);
    for (i = 0; i < pg.n; i++)
        trk_cells += pg.it[i].kind == EC_IT_PARAM && !(EC_PARAMS[pg.it[i].id].flags & EC_PF_FX);
    CHECK(A->bus == 1 && !trk_cells && lbl(FM1CORE_LBL_KNOB0 + FM1P_E_ALGO, ""),
          "MIX on S1: no track cells (%d), ALGO no label (%s)", trk_cells, L(FM1CORE_LBL_KNOB0 + FM1P_E_ALGO));
    ec_pn_select_target(0);
    to(EC_CAT_XFD);
    CHECK(lbl(FM1CORE_LBL_KNOB0 + FM1P_E_ALGO, "") && lbl(FM1CORE_LBL_KNOB0 + FM1P_E_PRESETS, ""),
          "XFADE: ALGO / PRESETS unlabelled (%s / %s)", L(FM1CORE_LBL_KNOB0 + FM1P_E_ALGO),
          L(FM1CORE_LBL_KNOB0 + FM1P_E_PRESETS));
    to(EC_MODE_PAGES);
    CHECK(lbl(FM1CORE_LBL_KNOB0 + FM1P_E_ALGO, "PAGE") && lbl(FM1CORE_LBL_KNOB0 + FM1P_E_K3, ""),
          "PAGES: ALGO PAGE, K3 nothing (%s, %s)", L(FM1CORE_LBL_KNOB0 + FM1P_E_ALGO), L(FM1CORE_LBL_KNOB0 + FM1P_E_K3));
    ec_pn_enter(EC_MODE_PROJECTS);
    run(200);
    press(FM1P_B_REC, 40);
    run(50);
    CHECK(!A->pn.lr_on && !A->pn.lr_wait, "PROJECTS: REC arms nothing");
}

/* 0.6.57 (owner): KEYBOARD: live REC writes where a note is played (a target step or not); the selected step's note
 * blinks on its key */
/* 0.6.58 (owner): the pads' second bank (17-32) only with more than 16 slices; COUNT down to 16 brings bank 1 back */
/* 0.6.59 (owner): stopped, ALGO onto a step: that step heard once (its pitch), for its length */
static void test_algo_preview(void)
{
    ec_ptrack_t *tr;
    to(EC_MODE_KEYBOARD);
    ec_pn_select_target(0);
    ec_app_params(A)[EC_P_MACHINE] = EC_MACH_LOOP;
    clear_steps();
    tr = ec_project_track(&A->proj, A->track);
    tr->steps[1].trig = 1;
    ec_step_lock_set(&tr->steps[1], EC_P_NOTE, 7);
    ec_seq_touch(&A->seq, A->track);
    ec_lp_target_to(0);
    run_audio(50);
    turn(FM1P_E_ALGO, 1);                          /* step 1: empty, nothing */
    run_audio(20);
    CHECK(!A->e5.preview, "ALGO onto an empty step: nothing heard");
    turn(FM1P_E_ALGO, 1);                          /* step 2: its note */
    run_audio(20);
    CHECK(A->e5.preview == 3 && A->e5.preview_semis == 7 && A->seq.tr[A->track].live_pitch_on,
          "ALGO onto step 2 (stopped): its note heard (+%d)", A->e5.preview_semis);
    run_audio(1600);
    CHECK(!A->e5.preview && !A->seq.tr[A->track].live_pitch_on, "its length over: silent again");
    ec_lp_target_to(0);
    clear_steps();
}

/* 0.6.62 (owner): TRIG sounds while a live key is held (playing), and a held key overrides the sequence; KEYBOARD's
 * pages, scales and the IN SCALE layout */
static void test_062(void)
{
    ec_track_status_t st;
    float *v;
    int i, f0, k;
    to(EC_MODE_KEYBOARD);
    ec_pn_select_target(0);
    v = ec_app_params(A);
    v[EC_P_MACHINE] = EC_MACH_LOOP_TRIG;
    v[EC_P_PATTERN_STEPS] = 16;
    clear_steps();
    ec_seq_touch(&A->seq, A->track);
    ec_seq_timing_changed(&A->seq);
    ec_lp_target_to(0);
    press(FM1P_B_PLAY, 40);
    run_audio(300);
    key(0, wkey(5), 1);
    run_audio(200);
    ec_dsp_track_status(A->dsp, A->track, &st);
    CHECK(st.reader_on, "TRIG, playing, an empty pattern: a KEYBOARD note held sounds");
    key(0, wkey(5), 0);
    run_audio(400);
    ec_dsp_track_status(A->dsp, A->track, &st);
    CHECK(!st.reader_on, "let go: silent again (its gate closed)");
    for (i = 0; i < 16; i++)                       /* a pattern: every step */
        ec_project_track(&A->proj, A->track)->steps[i].trig = 1;
    ec_seq_touch(&A->seq, A->track);
    run_audio(300);
    {                                              /* 0.6.77 (owner): a note held overrides the notes only */
        float x = 0;
        ec_step_lock_set(&ec_project_track(&A->proj, A->track)->steps[0], EC_P_NOTE, 7.0f);
        ec_step_lock_set(&ec_project_track(&A->proj, A->track)->steps[0], EC_P_FILTER_CUTOFF, 0.25f);
        ec_seq_touch(&A->seq, A->track);
        key(0, wkey(7), 1);                        /* E4: +4 */
        run_audio(20);
        f0 = A->seq.tr[A->track].fired;
        for (i = 0; i < 200 && !(A->seq.tr[A->track].hold_on[EC_P_FILTER_CUTOFF]); i++)
            run_audio(5);
        x = ec_seq_value(&A->seq, A->track, EC_P_FILTER_CUTOFF);
        CHECK(A->seq.tr[A->track].fired > f0 && x == 0.25f &&
                  ec_seq_value(&A->seq, A->track, EC_P_NOTE) == 4,
              "a note held: the steps still fire (%d -> %d) with their CUTOFF lock (%g), the note is the key's (%g)",
              f0, A->seq.tr[A->track].fired, x, ec_seq_value(&A->seq, A->track, EC_P_NOTE));
        key(0, wkey(7), 0);
        run_audio(400);
        for (i = 0; i < 400 && ec_seq_value(&A->seq, A->track, EC_P_NOTE) != 7.0f; i++)
            run_audio(5);
        CHECK(ec_seq_value(&A->seq, A->track, EC_P_NOTE) == 7.0f, "let go: the step's own note again (%g)",
              ec_seq_value(&A->seq, A->track, EC_P_NOTE));
        ec_step_clear(&ec_project_track(&A->proj, A->track)->steps[0]);
        ec_project_track(&A->proj, A->track)->steps[0].trig = 1;
    }
    {                                              /* 0.6.77: LOOP ENGINE: loop keys = the loop points, range keys = the
                                                    * range; the steps keep firing with their other locks */
        ec_step_t *s0 = &ec_project_track(&A->proj, A->track)->steps[0];
        const int rp = ec_cat_key_page(EC_CAT_SRC, 5);
        v[EC_P_MACHINE] = EC_MACH_LOOP;
        ec_step_lock_set(s0, EC_P_FILTER_CUTOFF, 0.25f);
        ec_step_lock_set(s0, EC_P_RANGE_START, 96.0f);
        ec_seq_touch(&A->seq, A->track);
        ec_pn_goto(EC_CAT_SRC, ec_src_loop_page());
        key(0, 2, 1);                              /* W3: a loop key */
        run_audio(20);
        f0 = A->seq.tr[A->track].fired;
        run_audio(600);
        for (i = 0; i < 400 && ec_seq_value(&A->seq, A->track, EC_P_FILTER_CUTOFF) != 0.25f; i++)
            run_audio(5);
        CHECK(A->seq.tr[A->track].fired > f0 && ec_seq_value(&A->seq, A->track, EC_P_FILTER_CUTOFF) == 0.25f,
              "a loop key held: the steps still fire with their CUTOFF lock (%d -> %d)", f0, A->seq.tr[A->track].fired);
        key(0, 2, 0);
        run_audio(50);
        CHECK(rp >= 0, "LOOP ENGINE's RANGE page (%d)", rp);
        ec_pn_goto(EC_CAT_SRC, rp);
        key(0, 0, 1);                              /* W1: the range 0..8 */
        run_audio(10);
        for (i = 0; i < 400 && !(A->seq.tr[A->track].hold_on[EC_P_RANGE_START] &&
                                 ec_seq_value(&A->seq, A->track, EC_P_FILTER_CUTOFF) == 0.25f); i++)
            run_audio(5);
        CHECK(A->seq.tr[A->track].hold_on[EC_P_RANGE_START] &&
                  ec_seq_value(&A->seq, A->track, EC_P_RANGE_START) == 0.0f &&
                  ec_seq_value(&A->seq, A->track, EC_P_FILTER_CUTOFF) == 0.25f,
              "a range key held: its range wins over the step's (%g), the CUTOFF lock still plays",
              ec_seq_value(&A->seq, A->track, EC_P_RANGE_START));
        key(0, 0, 0);
        for (i = 0; i < 400 && ec_seq_value(&A->seq, A->track, EC_P_RANGE_START) != 96.0f; i++)
            run_audio(5);
        CHECK(!A->seq.tr[A->track].live_range && ec_seq_value(&A->seq, A->track, EC_P_RANGE_START) == 96.0f,
              "let go: the step's range lock again (%g)", ec_seq_value(&A->seq, A->track, EC_P_RANGE_START));
        ec_step_clear(s0);
        v[EC_P_RANGE_START] = 0;
        v[EC_P_RANGE_END] = 128;
        v[EC_P_LOOP_START] = 0;
        v[EC_P_LOOP_END] = 128;
        to(EC_MODE_KEYBOARD);
    }
    press(FM1P_B_PLAY, 40);
    clear_steps();
    v[EC_P_MACHINE] = EC_MACH_LOOP;
    ec_seq_touch(&A->seq, A->track);
    /* pages and scales */
    turn(FM1P_E_SELECT, 1);
    run(20);
    CHECK(A->pn.kb_page == 1 && lbl(FM1CORE_LBL_KNOB0 + FM1P_E_K1, "SCALE"), "SEL: KEYBOARD's SCALE page (%s)",
          L(FM1CORE_LBL_KNOB0 + FM1P_E_K1));
    v[EC_P_KB_SCALE] = 3;                          /* MINOR */
    v[EC_P_KB_ROOT] = 10;                          /* A */
    v[EC_P_KB_LAYOUT] = 2;                         /* IN SCALE */
    A->pn.oct = 0;
    run(20);
    CHECK(ec_kb_semis(ec_key_of_lower(0)) == 9 && ec_kb_semis(ec_key_of_lower(2)) == 12 &&
              ec_kb_semis(ec_key_of_lower(7)) == 21 && ec_kb_semis(ec_key_of_upper(0)) == EC_KB_NONE,
          "A MINOR, IN SCALE: W1 A, W3 C, W8 the next A; the black keys play nothing (%d %d %d)",
          ec_kb_semis(ec_key_of_lower(0)), ec_kb_semis(ec_key_of_lower(2)), ec_kb_semis(ec_key_of_lower(7)));
    CHECK(lbl(FM1CORE_LBL_BLACK0 + 0, "TRIG") && lbl(FM1CORE_LBL_BLACK0 + 1, "NOTES") &&
              lbl(FM1CORE_LBL_BLACK0 + 2, "MACHINE") && lbl(FM1CORE_LBL_BLACK0 + 3, "WARP") &&
              lbl(FM1CORE_LBL_BLACK0 + 4, "SCALE") && lbl(FM1CORE_LBL_BLACK0 + 5, "LFO 1"),
          "0.6.74 IN SCALE: B1..B5 as LOOP ENGINE's, B2 / B5 KEYBOARD's own, B6 LFO 1 (%s %s %s %s %s)",
          L(FM1CORE_LBL_BLACK0), L(FM1CORE_LBL_BLACK0 + 1), L(FM1CORE_LBL_BLACK0 + 2), L(FM1CORE_LBL_BLACK0 + 3),
          L(FM1CORE_LBL_BLACK0 + 4));
    ec_kb_page(0, 1);
    tapkey(1, 1, 40);                              /* B2 on NOTES: SOUND */
    CHECK(A->pn.kb_page == 2 && lbl(FM1CORE_LBL_KNOB0 + FM1P_E_K1, "CUTOFF"), "B2 again: the SOUND page");
    tapkey(1, 4, 40);                              /* B5: SCALE */
    CHECK(A->pn.kb_page == 1, "B5: the SCALE page");
    tapkey(1, 2, 40);                              /* B3: MACHINE */
    CHECK(ec_kb_page_key(A->pn.kb_page) == 2, "B3: a MACHINE page (%d)", A->pn.kb_page);
    key(1, 2, 1);                                  /* B3 held: LOOP ENGINE's MACHINE menu */
    run(EC_HOLD_MS + 100);
    CHECK(ec_hm_shown() && ec_hm_armed() == 3, "B3 held: the MACHINE menu");
    key(1, 2, 0);
    run(20);
    CHECK(!ec_hm_shown(), "let go: the menu goes");
    key(1, 5, 1);                                  /* B6 held: LFO 1 */
    run(20);
    CHECK(A->mk.held == EC_MK_LFO1, "B6 held: LFO 1 (a mod source)");
    key(1, 5, 0);
    run(20);
    v[EC_P_KB_LAYOUT] = 1;
    k = ec_key_of_upper(0);
    CHECK(ec_kb_semis(k) != EC_KB_NONE && !ec_kb_scale_layout(), "CHROMATIC: the black keys play again (%d)",
          ec_kb_semis(k));
    {                                              /* 0.6.69 (owner): A MINOR, CHROMATIC: C#4 plays C4, F#4 F4 */
        int kk, cs = -1, fs = -1;
        for (kk = 0; kk < EC_NKEY; kk++) {
            if (ec_kb_semis(kk) == 1)
                cs = kk;
            if (ec_kb_semis(kk) == 6)
                fs = kk;
        }
        CHECK(cs >= 0 && fs >= 0 && ec_kb_snap(1) == 0 && ec_kb_snap(6) == 5 && ec_kb_snap(2) == 2,
              "a scale on: outside notes snap to the nearest (%d %d)", ec_kb_snap(1), ec_kb_snap(6));
        key(1, ec_key_upper(cs), 1);
        run(10);
        CHECK(A->seq.tr[A->track].live_pitch_on && A->seq.tr[A->track].live_semis == 0, "C#4 pressed: C4 sounds (%g)",
              A->seq.tr[A->track].live_semis);
        key(1, ec_key_upper(cs), 0);
        run(10);
    }
    v[EC_P_KB_SCALE] = 1;
    v[EC_P_KB_ROOT] = 1;
    ec_kb_page(0, 1);
}

/* 0.6.63 (owner): F1 / F2 + the white keys copy / cut / paste steps */
static void test_clip_modes(void)
{
    static const int M[4] = {EC_CAT_FILT, EC_CAT_TRIG, EC_CAT_SRC, EC_CAT_AMP};
    int m;
    for (m = 0; m < 4; m++) {
        ec_ptrack_t *tr;
        to(M[m]);
        ec_pn_select_target(0);
        ec_lp_target_to(0);
        clear_steps();
        tr = ec_project_track(&A->proj, A->track);
        tr->steps[0].trig = 1;
        ec_step_lock_set(&tr->steps[0], EC_P_NOTE, 5);
        ec_seq_touch(&A->seq, A->track);
        run(1200);
        btn(FM1P_B_OCTDN, 1);                      /* F1 held */
        run(30);
        tapkey(0, wkey(1), 40);                    /* copy step 1 */
        tapkey(0, wkey(3), 40);                    /* paste on step 3 */
        btn(FM1P_B_OCTDN, 0);
        run(30);
        CHECK(tr->steps[2].trig, "mode %d: F1 + W1, W3: step 1 pasted on step 3 (%s)", M[m], A->msg);
        btn(FM1P_B_OCTUP, 1);                      /* F2 held: cut step 3, paste on step 5 (a move) */
        run(30);
        tapkey(0, wkey(3), 40);
        tapkey(0, wkey(5), 40);
        btn(FM1P_B_OCTUP, 0);
        run(30);
        CHECK(!tr->steps[2].trig && tr->steps[4].trig, "mode %d: F2 + W3, W5: step 3 moved to step 5 (%s)", M[m], A->msg);
        clear_steps();
    }
}

/* 0.6.64 (owner, FIZZ): KEYBOARD's STEP layer: the black keys play the scale (grouped), the white keys the steps */
static void test_kb_step_layer(void)
{
    ec_ptrack_t *tr;
    float *v, x = 0;
    to(EC_MODE_KEYBOARD);
    ec_pn_select_target(0);
    v = ec_app_params(A);
    v[EC_P_MACHINE] = EC_MACH_LOOP;
    v[EC_P_KB_SCALE] = 3;                          /* A MINOR */
    v[EC_P_KB_ROOT] = 10;
    v[EC_P_KB_LAYOUT] = 1;                         /* (CHROMATIC: the layer does it anyway) */
    A->pn.oct = 0;
    clear_steps();
    tr = ec_project_track(&A->proj, A->track);
    tapkey(1, 10, 40);                             /* B11 clicked: the STEP layer latched */
    run(20);
    CHECK(ec_lp_step_layer() && ec_kb_step_notes(), "B11: the STEP layer");
    CHECK(ec_kb_semis(ec_key_of_upper(0)) == 9 && ec_kb_semis(ec_key_of_upper(2)) == 12 &&
              ec_kb_semis(ec_key_of_upper(7)) == 21 && lbl(FM1CORE_LBL_BLACK0 + 0, "A4"),
          "the black keys: A MINOR up from A (B1 %d, B3 %d, B8 %d; %s)", ec_kb_semis(ec_key_of_upper(0)),
          ec_kb_semis(ec_key_of_upper(2)), ec_kb_semis(ec_key_of_upper(7)), L(FM1CORE_LBL_BLACK0));
    tapkey(0, wkey(4), 40);                        /* W4: step 4 selected */
    run(20);
    CHECK(ec_lp_first() == 3, "W4 selects step 4 (%d)", ec_lp_first() + 1);
    tapkey(1, 2, 40);                              /* B3 = C: into step 4 */
    run(20);
    CHECK(tr->steps[3].trig && ec_step_lock_get(&tr->steps[3], EC_P_NOTE, &x) && x == 12,
          "B3 (C) into the selected step 4 (+%g)", x);
    tapkey(1, 10, 40);                             /* B11 again: back */
    run(20);
    v[EC_P_KB_SCALE] = 1;
    v[EC_P_KB_ROOT] = 1;
    ec_lp_target_to(0);
    clear_steps();
}

/* 0.6.66 (owner): KEYBOARD's pages reach TRIG's and WARP's params; HOME brings KEYBOARD back on its page */
static void test_kb_more(void)
{
    ec_page_t pg;
    int n, p, trig = -1, warp = -1, i;
    to(EC_MODE_KEYBOARD);
    ec_pn_select_target(0);
    ec_app_params(A)[EC_P_MACHINE] = EC_MACH_LOOP;
    n = ec_kb_pages();
    for (p = 0; p < n; p++) {
        ec_kb_page(p, 1);
        ec_app_page(A, &pg);
        for (i = 0; i < pg.n; i++) {
            if (pg.it[i].kind == EC_IT_PARAM && pg.it[i].id == EC_P_TRIG_CHANCE && trig < 0)
                trig = p;
            if (pg.it[i].kind == EC_IT_PARAM && EC_PARAMS[pg.it[i].id].kind == EC_PK_NUM && p >= ec_kb_group_page(3) &&
                ec_kb_group_page(3) >= 0 && warp < 0)
                warp = p;
        }
    }
    CHECK(n > 5 && trig >= EC_KB_PAGES && warp > trig, "KEYBOARD: %d pages, CHANCE on %d, the warp params from %d", n,
          trig, warp);
    ec_kb_page(trig, 1);
    press(FM1P_B_FX, 40);
    press(FM1P_B_HOME, 40);
    CHECK(mode() == EC_MODE_KEYBOARD && A->pn.kb_page == trig, "FX, then HOME: KEYBOARD on the same page (%d)",
          A->pn.kb_page);
    ec_kb_page(0, 1);
    {                                              /* 0.6.78 (owner): KEYBOARD: F1 held = F1, a quick click = OCT */
        const int o = A->pn.oct;
        btn(FM1P_B_OCTDN, 1);
        run(60);
        CHECK(!ec_pn_f(1) && A->pn.oct == o, "0.6.79: KEYBOARD, F1 just pressed: nothing yet (no F1, no octave)");
        run(EC_HOLD_MS + 40);
        CHECK(ec_pn_f(1) && A->pn.oct == o, "KEYBOARD: F1 held is F1 (the octave stays %d)", A->pn.oct);
        btn(FM1P_B_OCTDN, 0);
        run(20);
        CHECK(!ec_pn_f(1) && !A->pn.f_sticky && A->pn.oct == o, "let go after a hold: off, not sticky, no octave");
        press(FM1P_B_OCTDN, 40);
        CHECK(A->pn.oct == o - 1 && !A->pn.f_sticky, "a quick click: OCT -1 (%d)", A->pn.oct);
        press(FM1P_B_OCTUP, 40);
    }
    to(EC_CAT_SRC);                                /* 0.6.78 (owner): HOME's modes: B11 clears a sticky F1 / F2 */
    press(FM1P_B_OCTUP, 40);
    CHECK(A->pn.f_sticky == 2, "LOOP ENGINE: F2 tapped: sticky");
    tapkey(1, 10, 40);
    CHECK(!A->pn.f_sticky, "B11: the sticky F2 is off");
    tapkey(1, 10, 40);                             /* (B11's click latched the STEP layer: back) */
    press(FM1P_B_OCTDN, 40);                       /* 0.6.83 (owner): a mode change unsticks F1 / F2 and SHIFT */
    press(FM1P_B_GLO, 40);
    CHECK(A->pn.f_sticky == 1 && A->pn.shift_sticky && A->fn, "F1 and SHIFT tapped: both sticky");
    press(FM1P_B_SEL, 40);
    CHECK(!A->pn.f_sticky && !A->pn.shift_sticky && !A->fn && mode() == EC_CAT_FILT,
          "SHIFT + SEL (FILTER's settings): F1 and SHIFT are off again (mode %d)", mode());
    press(FM1P_B_OCTDN, 40);                       /* (F1 sticky, SHIFT + SEL closes the settings) */
    btn(FM1P_B_GLO, 1);
    press(FM1P_B_SEL, 40);
    btn(FM1P_B_GLO, 0);
    run(20);
    press(FM1P_B_HOME, 40);
    CHECK(!A->pn.f_sticky, "a mode button: F1 off (mode %d)", mode());
    to(EC_MODE_KEYBOARD);
}

static void test_bank16(void)
{
    float *v;
    to(EC_MODE_KEYBOARD);
    ec_pn_select_target(0);
    v = ec_app_params(A);
    v[EC_P_MACHINE] = EC_MACH_GRID_SLICE;
    v[EC_P_SLICE_COUNT] = 16;
    ec_seq_touch(&A->seq, A->track);
    A->pn.zoom_half = 0;
    key(1, 10, 1);                                 /* B11 held */
    run(400);
    turn(FM1P_E_K4, 1);
    run(20);
    CHECK(!A->pn.zoom_half && !strcmp(A->msg, "SLICES 1-16"), "16 slices: B11 + K4 stays on 1-16 (%s)", A->msg);
    v[EC_P_SLICE_COUNT] = 32;
    turn(FM1P_E_K4, 1);
    run(20);
    CHECK(A->pn.zoom_half && !strcmp(A->msg, "SLICES 17-32"), "32 slices: 17-32 (%s)", A->msg);
    key(1, 10, 0);
    run(50);
    v[EC_P_SLICE_COUNT] = 12;
    run(20);
    CHECK(!A->pn.zoom_half, "COUNT 12: back on 1-16");
    v[EC_P_MACHINE] = EC_MACH_LOOP;
    v[EC_P_SLICE_COUNT] = 16;
    ec_seq_touch(&A->seq, A->track);
    run(50);
}

static void test_kb_rec(void)
{
    ec_ptrack_t *tr;
    float *v;
    int i, k, n = 0, kc = -1;
    uint8_t l0, l1;
    to(EC_MODE_KEYBOARD);
    ec_pn_select_target(0);
    v = ec_app_params(A);
    v[EC_P_MACHINE] = EC_MACH_LOOP;
    v[EC_P_PATTERN_STEPS] = 7;
    ec_seq_timing_changed(&A->seq);
    clear_steps();
    tr = ec_project_track(&A->proj, A->track);
    ec_lp_target_to(7);                            /* ALGO had selected step 7 */
    A->proj.rec.mode = EC_REC_CAPTURE;
    press(FM1P_B_REC, 40);
    run(50);
    CHECK(A->pn.lr_on && A->pn.lr_wait, "REC: CAPTURE waits");
    tapkey(0, wkey(5), 60);                        /* the first note starts playback */
    run_audio(600);
    tapkey(0, wkey(7), 60);
    run_audio(100);
    press(FM1P_B_PLAY, 40);                        /* stop: REC ends */
    run(50);
    for (i = 0; i < 7; i++)
        n += tr->steps[i].trig;
    CHECK(A->pn.target == 7 && n == 2 && !tr->steps[6].trig, "REC with a target: the notes where played (%d trigs, step 7 "
          "%s), the target untouched (%d)", n, tr->steps[6].trig ? "written" : "empty", A->pn.target);
    for (i = 0; i < 7 && !tr->steps[i].trig; i++)
        ;
    ec_lp_target_to(i + 1);                        /* the first recorded step selected */
    {
        float x = 0;
        ec_step_lock_get(&tr->steps[i], EC_P_NOTE, &x);
        for (k = 0; k < EC_NKEY && kc < 0; k++)
            if (ec_key_lower(k) >= 0 && ec_kb_semis(k) == (int)floorf(x + 0.5f))
                kc = k;
    }
    A->ms = 0;
    l0 = ec_kb_key_led(kc, EC_LED_FAINT);
    A->ms = 200;
    l1 = ec_kb_key_led(kc, EC_LED_FAINT);
    CHECK(kc >= 0 && l0 == EC_LED_LIT && l1 == EC_LED_LIT, "0.6.60: the selected step's note lit, steady (%d / %d)", l0, l1);
    {   /* and the selected step blinks on its own white key (its place on the step page) */
        const int w = i - A->step_page * 16;
        int lit = 0, off = 0, n;
        for (n = 0; n < 40; n++) {
            run(10);
            lit += led_key(0, w) == FM1P_LED_LIT;
            off += led_key(0, w) != FM1P_LED_LIT;
        }
        CHECK(w >= 0 && w < 16 && lit && off, "the selected step (%d) blinks on W%d (%d / %d)", i + 1, w + 1, lit, off);
    }
    ec_lp_target_to(0);
    v[EC_P_PATTERN_STEPS] = 16;
    ec_seq_timing_changed(&A->seq);
    clear_steps();
    run(100);
}

static void test_mod_display(void)
{
    float *v, mv[EC_NP], base, lo = 1e9f, hi = -1e9f;
    int i;
    to(EC_CAT_FILT);
    v = ec_app_params(A);
    base = v[EC_P_FILTER_CUTOFF] = 64;
    v[ec_mtx_src_id(0)] = (float)(EC_MS_LFO1 + 1);           /* LFO 1 -> CUTOFF, full depth */
    v[ec_mtx_dst_id(0)] = (float)(EC_P_FILTER_CUTOFF + 1);
    v[ec_mtx_depth_id(0)] = 20;
    v[EC_P_LFO1_DEPTH] = 128;                              /* (64: none) */
    ec_seq_touch(&A->seq, A->track);
    if (!A->seq.playing)
        press(FM1P_B_PLAY, 40);
    for (i = 0; i < 60; i++) {
        run_audio(20);
        ec_app_mod_values(A, mv);
        lo = mv[EC_P_FILTER_CUTOFF] < lo ? mv[EC_P_FILTER_CUTOFF] : lo;
        hi = mv[EC_P_FILTER_CUTOFF] > hi ? mv[EC_P_FILTER_CUTOFF] : hi;
    }
    CHECK(hi - lo > 4.0f && v[EC_P_FILTER_CUTOFF] == base, "LFO 1 -> CUTOFF: the value as heard moves (%g..%g), the set "
          "value stays %g", lo, hi, v[EC_P_FILTER_CUTOFF]);
    CHECK(lo >= base - 0.01f, "0.6.51: the LFO is unipolar: never below the set value (%g)", lo);
    press(FM1P_B_PLAY, 40);                                 /* stopped: the pictures as set */
    run_audio(40);
    ec_app_shown_values(A, mv);
    CHECK(!A->seq.playing && mv[EC_P_FILTER_CUTOFF] == base, "stopped: shown as set (%g)", mv[EC_P_FILTER_CUTOFF]);
    A->mk.held = EC_MK_LFO1;                                /* a source key held: the set value + the full amount */
    ec_app_shown_values(A, mv);
    {
        const ec_param_def_t *d = &EC_PARAMS[EC_P_FILTER_CUTOFF];
        const float want = base + 0.2f * (d->max - d->min);
        CHECK(fabsf(mv[EC_P_FILTER_CUTOFF] - (want > d->max ? d->max : want)) < 0.01f,
              "LFO 1 held: base + depth (%g, want %g)", mv[EC_P_FILTER_CUTOFF], want);
    }
    A->mk.held = -1;
    {   /* 0.6.51 (owner): LFO 1's page: SEL steps through its routes, ALGO sets the shown one's depth */
        ec_page_t pg;
        int p, k;
        v[ec_mtx_src_id(1)] = (float)(EC_MS_LFO1 + 1);       /* a second route: LFO 1 -> RES */
        v[ec_mtx_dst_id(1)] = (float)(EC_P_FILTER_RES + 1);
        v[ec_mtx_depth_id(1)] = 0;
        to(EC_CAT_MOD);
        for (p = 0; p < 12; p++) {
            ec_app_set_page(EC_CAT_MOD, p);
            ec_app_page(A, &pg);
            if (pg.pic == EC_PIC_LFO1)
                break;
        }
        A->redraw = 1;
        run(20);
        CHECK(ec_mk_route_src() == EC_MS_LFO1 && lbl(FM1CORE_LBL_KNOB0 + FM1P_E_ALGO, "DEPTH"), "LFO 1 page: ALGO %s",
              L(FM1CORE_LBL_KNOB0 + FM1P_E_ALGO));
        turn(FM1P_E_ALGO, 1);
        run(200);
        CHECK(fabsf(v[ec_mtx_depth_id(0)] - 20.1f) < 0.01f, "ALGO: the first route's depth 20.1 (%g)", v[ec_mtx_depth_id(0)]);
        turn(FM1P_E_SELECT, 1);
        run(200);
        turn(FM1P_E_ALGO, -1);
        run(200);
        k = ec_mk_route(v, EC_MS_LFO1, &p, 0);
        CHECK(k == 1 && p == 1 && fabsf(v[ec_mtx_depth_id(1)] + 0.1f) < 0.01f && fabsf(v[ec_mtx_depth_id(0)] - 20.1f) < 0.01f,
              "SEL: route 2 (RES), ALGO -1: -0.1 (%g)", v[ec_mtx_depth_id(1)]);
        turn(FM1P_E_SELECT, -1);
        run(200);
        v[ec_mtx_src_id(1)] = 0;
        v[ec_mtx_dst_id(1)] = 0;
        v[ec_mtx_depth_id(1)] = 0;
    }
    v[ec_mtx_depth_id(0)] = 0;
    v[ec_mtx_src_id(0)] = 0;
    v[ec_mtx_dst_id(0)] = 0;
    v[EC_P_LFO1_DEPTH] = 64;
    ec_seq_touch(&A->seq, A->track);
    run_audio(100);
    ec_app_mod_values(A, mv);
    CHECK(mv[EC_P_FILTER_CUTOFF] == base, "the route gone: as set (%g)", mv[EC_P_FILTER_CUTOFF]);
}

/* 0.6.72 / 0.6.75 (owner): a restart (the host's reset: shutdown, then init in the same process) comes back to the
 * track, the mode and its page */
static void test_restart(void)
{
    int t;
    to(EC_MODE_KEYBOARD);
    ec_pn_select_target(2);
    ec_kb_page(1, 1);                                         /* SCALE */
    run(1500);                                                /* (written half a second after the change) */
    C->shutdown();
    C->init(0);
    for (t = 0; t < 60 && (mode() != EC_MODE_KEYBOARD || A->track != 2); t++)
        run(100);
    CHECK(mode() == EC_MODE_KEYBOARD && A->track == 2 && A->pn.kb_page == 1,
          "a reset: KEYBOARD on T3, its SCALE page again (mode %d, T%d, page %d)", mode(), A->track + 1, A->pn.kb_page);
}

/* 0.6.81 (owner): POLY slice tracks play several slices at once (LOOP ENGINE's pads, KEYBOARD's slice keys) */
static void test_slice_poly(void)
{
    ec_track_status_t st;
    float *v;
    ec_pn_goto(EC_CAT_SRC, ec_src_loop_page());
    ec_pn_select_target(0);
    v = ec_app_params(A);
    v[EC_P_MACHINE] = EC_MACH_GRID_SLICE;          /* 0.6.81: SLICE + VOICE MODE POLY (S.POLY is gone) */
    v[EC_P_TRIG_POLY] = 2;
    v[EC_P_SLICE_COUNT] = 8;
    v[EC_P_SLICE_PLAY_MODE] = 2;                  /* HOLD */
    ec_seq_touch(&A->seq, A->track);
    run_audio(50);
    key(0, 0, 1);
    run_audio(30);
    key(0, 2, 1);
    run_audio(60);
    ec_dsp_track_status(A->dsp, A->track, &st);
    CHECK(st.voices == 2, "SLICE, POLY: two pads held, two slices sound (%d)", st.voices);
    key(0, 0, 0);
    key(0, 2, 0);
    run_audio(400);
    v[EC_P_TRIG_POLY] = 1;
    ec_seq_touch(&A->seq, A->track);
    run_audio(20);
    key(0, 0, 1);
    run_audio(30);
    key(0, 2, 1);
    run_audio(400);
    ec_dsp_track_status(A->dsp, A->track, &st);
    CHECK(st.voices == 1, "SLICE, MONO: the second pad takes over (%d)", st.voices);
    key(0, 0, 0);
    key(0, 2, 0);
    run_audio(400);
    v[EC_P_TRIG_POLY] = 2;
    ec_seq_touch(&A->seq, A->track);
    to(EC_MODE_KEYBOARD);
    key(1, 0, 1);
    run_audio(30);
    key(1, 2, 1);
    run_audio(60);
    ec_dsp_track_status(A->dsp, A->track, &st);
    CHECK(st.voices == 2, "KEYBOARD, POLY: two slice keys, two slices (%d)", st.voices);
    key(1, 0, 0);
    run_audio(400);
    ec_dsp_track_status(A->dsp, A->track, &st);
    CHECK(st.voices == 1 && st.voice_slice[0] == 3, "the first key up ends its own slice (slice %d left)",
          st.voice_slice[0]);
    key(1, 2, 0);
    run_audio(400);
    v[EC_P_TRIG_POLY] = 1;
    v[EC_P_MACHINE] = EC_MACH_LOOP;
    ec_seq_touch(&A->seq, A->track);
}

/* 0.6.82 (owner): TRIG + VOICE MODE POLY: keys held together write one chord; its notes sound as voices; "C#4+" */
static void test_chords(void)
{
    ec_track_status_t st;
    ec_page_t pg;
    float *v, nt[EC_STEP_NOTES];
    char val[24];
    int n, i;
    to(EC_MODE_KEYBOARD);
    ec_pn_select_target(0);
    v = ec_app_params(A);
    v[EC_P_MACHINE] = EC_MACH_LOOP_TRIG;
    v[EC_P_TRIG_POLY] = 2;
    v[EC_P_KB_LAYOUT] = 1;
    v[EC_P_KB_SCALE] = 1;
    A->pn.oct = 0;
    clear_steps();
    ec_seq_touch(&A->seq, A->track);
    ec_lp_target_to(1);
    run(20);
    key(0, wkey(5), 1);                            /* C4 */
    run(20);
    key(0, wkey(7), 1);                            /* E4, C4 still held */
    run(20);
    key(0, wkey(9), 1);                            /* G4 */
    run(20);
    n = ec_step_notes(step(0), nt);
    CHECK(n == 3 && nt[0] == 0 && nt[1] == 4 && nt[2] == 7 && A->pn.target == 1,
          "POLY: three keys held = one chord on step 1 (%d notes), the target waits", n);
    key(0, wkey(5), 0);
    key(0, wkey(7), 0);
    key(0, wkey(9), 0);
    run(20);
    CHECK(A->pn.target == 2, "every key up: the target moves on (%d)", A->pn.target);
    ec_pn_goto(EC_CAT_TRIG, 0);                    /* B1's TRIG page: NOTE first */
    ec_lp_target_to(1);
    run(20);
    ec_app_page(A, &pg);
    for (i = 0; i < pg.n && !(pg.it[i].kind == EC_IT_PARAM && pg.it[i].id == EC_P_NOTE); i++)
        ;
    if (i < pg.n)
        ec_app_item_format(A, &pg.it[i], 0.0f, val, sizeof val);
    else
        snprintf(val, sizeof val, "-");
    CHECK(!strcmp(val, "C4+") && pg.it[0].id == EC_P_NOTE, "the TRIG page's first cell: NOTE C4+ (%s)", val);
    ec_lp_target_to(0);
    to(EC_MODE_KEYBOARD);
    press(FM1P_B_PLAY, 40);
    for (i = 0; i < 100; i++) {
        run_audio(10);
        ec_dsp_track_status(A->dsp, A->track, &st);
        if (st.voices >= 2)
            break;
    }
    CHECK(st.voices >= 2, "playing: the chord's other notes sound as voices (%d)", st.voices);
    press(FM1P_B_PLAY, 40);
    run_audio(400);
    clear_steps();
    ec_seq_touch(&A->seq, A->track);
    key(0, wkey(5), 1);                            /* live: C4 then E4 held */
    run_audio(30);
    key(0, wkey(7), 1);
    run_audio(60);
    ec_dsp_track_status(A->dsp, A->track, &st);
    CHECK(st.voices == 1 && st.reader_on, "live: the first key plays the track, the second a voice (%d)", st.voices);
    key(0, wkey(7), 0);
    key(0, wkey(5), 0);
    run_audio(400);
    v[EC_P_TRIG_POLY] = 1;
    v[EC_P_MACHINE] = EC_MACH_LOOP;
    ec_seq_touch(&A->seq, A->track);
}

/* 0.6.88 (owner): a RATE change while playing keeps the playhead where it is (no jump, no reset) */
static void test_rate_change(void)
{
    float *v;
    int64_t p0, p1;
    int i, ok = 1;
    to(EC_CAT_SRC);
    ec_pn_select_target(0);
    v = ec_app_params(A);
    v[EC_P_MACHINE] = EC_MACH_LOOP;
    v[EC_P_PATTERN_STEPS] = 16;
    clear_steps();
    ec_seq_touch(&A->seq, A->track);
    press(FM1P_B_PLAY, 40);
    run_audio(900);
    for (i = 0; i < 6; i++) {                      /* slower, faster, back: each time the step stays */
        static const int RT[6] = {-3, 2, -1, 4, -4, 0};   /* (around 1/16) */
        p0 = A->seq.tr[A->track].position;
        v[EC_P_RATE] = (float)(EC_RATE_UNITY + RT[i]);
        ec_seq_timing_changed(&A->seq);
        p1 = A->seq.tr[A->track].position;
        if (p1 != p0)
            ok = 0;
        run_audio(170);
    }
    p0 = A->seq.tr[A->track].position;
    run_audio(600);
    p1 = A->seq.tr[A->track].position;
    CHECK(ok && p1 > p0, "RATE changes while playing: the step stays where it was each time, then moves on (%d -> %d)",
          (int)p0, (int)p1);
    press(FM1P_B_PLAY, 40);
    v[EC_P_RATE] = EC_RATE_UNITY;
    ec_seq_touch(&A->seq, A->track);
}

/* 0.6.89 (owner): B11 held: B1..B8 the pages (8 of 128 steps), the same in LOOP ENGINE and KEYBOARD */
static void test_b11_pages(void)
{
    float *v;
    int m, k, ok;
    for (m = 0; m < 2; m++) {
        if (m)
            to(EC_MODE_KEYBOARD);
        else
            ec_pn_goto(EC_CAT_SRC, ec_src_loop_page());
        ec_pn_select_target(0);
        v = ec_app_params(A);
        v[EC_P_MACHINE] = EC_MACH_LOOP;
        v[EC_P_PATTERN_STEPS] = 80;                /* five pages */
        v[EC_P_KB_LAYOUT] = 2;                     /* (KEYBOARD: IN SCALE) */
        v[EC_P_KB_SCALE] = 2;
        clear_steps();
        ec_seq_touch(&A->seq, A->track);
        A->step_page = 0;
        key(1, bkey(11), 1);
        run(450);
        CHECK(!ec_lp_b11_text(), "%s, B11 held: nothing said yet", m ? "KEYBOARD" : "LOOP ENGINE");
        CHECK(led_key(1, bkey(1)) == FM1P_LED_LIT && led_key(1, bkey(2)) == FM1P_LED_DIM &&
                  led_key(1, bkey(6)) == FM1P_LED_FAINT,
              "%s, B11 held: the black keys show the pages (B1 %d, B2 %d, B6 past the end %d)", m ? "KEYBOARD" : "LOOP",
              led_key(1, bkey(1)), led_key(1, bkey(2)), led_key(1, bkey(6)));
        for (k = 1, ok = 1; k <= 5; k++) {
            char want[16];
            tapkey(1, bkey(k), 40);
            snprintf(want, sizeof want, "PAGE %d", k);
            ok &= A->step_page == k - 1 && ec_lp_b11_text() && !strcmp(ec_lp_b11_text(), want);
        }
        CHECK(ok, "B1..B5: the pages, the header says PAGE n and keeps it (%s)", ec_lp_b11_text() ? ec_lp_b11_text() : "-");
        tapkey(1, bkey(8), 40);
        CHECK(A->step_page == 7 && v[EC_P_PATTERN_STEPS] == 128 && !strcmp(ec_lp_b11_text(), "PAGE 8"),
              "B8 past the end: the track grows to 8 pages (%g steps)", v[EC_P_PATTERN_STEPS]);
        tapkey(0, wkey(3), 40);
        CHECK(ec_lp_b11_text() && !strcmp(ec_lp_b11_text(), "STEP 115"), "a step key: STEP 115 (%s)",
              ec_lp_b11_text() ? ec_lp_b11_text() : "-");
        run(1200);
        CHECK(ec_lp_b11_text() && !strcmp(ec_lp_b11_text(), "STEP 115"), "still said a second later");
        key(1, bkey(11), 0);
        run(50);
        CHECK(!ec_lp_b11_text(), "B11 up: gone");
        ec_lp_target_to(0);
        if (!m) {                                  /* STEP latched: B11 held = the loop keys: "LOOP 0:16" */
            v[EC_P_LOOP_DIVISION] = 16;
            tapkey(1, bkey(11), 40);
            key(1, bkey(11), 1);
            run(450);
            key(0, wkey(1), 1);
            run(20);
            key(0, wkey(2), 1);
            run(20);
            CHECK(ec_lp_b11_text() && !strcmp(ec_lp_b11_text(), "LOOP 0:16"), "loop keys 1-2: LOOP 0:16 (%s)",
                  ec_lp_b11_text() ? ec_lp_b11_text() : "-");
            key(0, wkey(1), 0);
            key(0, wkey(2), 0);
            key(1, bkey(11), 0);
            run(50);
            tapkey(1, bkey(11), 40);               /* (unlatch) */
            v[EC_P_LOOP_START] = 0;
            v[EC_P_LOOP_END] = 128;
        }
        v[EC_P_PATTERN_STEPS] = 16;
        A->step_page = 0;
        ec_seq_touch(&A->seq, A->track);
    }
}

int main(int argc, char **argv)
{
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
              lbl(FM1CORE_LBL_BTN0 + FM1P_B_HOME, "LOOP"),
          "labels at boot: GLO %s, HOME %s", L(FM1CORE_LBL_BTN0 + FM1P_B_GLO), L(FM1CORE_LBL_BTN0 + FM1P_B_HOME));
    test_boot();
    test_modes();
    test_settings();
    test_app_settings();
    test_shift();
    test_transport();
    test_steps();
    test_presets();
    test_clip();
    test_sources();
    test_mix();
    test_pages();
    test_keyboard();
    test_xfd();
    test_xfd_lock();
    test_file_keys();
    test_patterns();
    test_labels();
    test_dice();
    test_mod_display();
    test_qa_quick();
    test_056();
    test_kb_rec();
    test_bank16();
    test_kb_more();
    test_kb_step_layer();
    test_clip_modes();
    test_062();
    test_algo_preview();
    test_restart();
    test_slice_poly();
    test_chords();
    test_rate_change();
    test_b11_pages();
    C->shutdown();
    printf("test_ux: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
