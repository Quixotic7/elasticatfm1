/* SPDX-License-Identifier: GPL-3.0-only */
/* 0.6.0: the panel by modes (ec_mode.h, docs/UX-V2.md §0, §1, §3): the input dispatch, the mode model (a tap on a mode
 * button, the lit one again, a hold or SHIFT = the settings, SHIFT + HOME = APP SETTINGS), the mode-entry pop-up, the
 * intro, SHIFT (held / sticky) and its layer, F1 / F2 (tap sticky, hold momentary; both = F3), PLAY, REC, the white
 * keys as steps, PRESETS = the track's pool slot, ALGO = the step page. The modes' own keys are in ec_srckeys.c,
 * ec_mix.c, ec_keyboard.c, ec_xfd.c, ec_patlaunch.c, ec_file.c, ec_sampler.c; live REC in ec_liverec.c.
 * FILE cannot open while playing (STOP FIRST, its button flashes); inside it PLAY and REC do nothing. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ec_app.h"
#include "ec_browse.h"
#include "ec_gfx.h"
#include "ec_razor_ui.h"
#include "ec_snap.h"

#define A (&ec_app)
#define P (&ec_app.pn)
#define UX (&ec_app.ux)
#define E (&ec_app.e5)

#define TAP_MS 250            /* a step key released within this (not edited): a tap toggles the trig */
#define POPUP_MS 800          /* the mode pop-up */

/* what a key does from its press to its release */
enum { R_NONE, R_STEP, R_LOOP_LIVE, R_PAD, R_KB, R_RAZOR, R_F3, R_SWALLOW, R_TRK, R_XFD, R_SRCB, R_PAGEB, R_SHIFTB,
       R_FILEB };

static int clampi(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }
static int iv(const float *v, int id) { return (int)floorf(v[id] + 0.5f); }
static int is_slice_m(int m) { return m >= EC_MACH_GRID_SLICE && m <= EC_MACH_RAZOR_POLY; }

int ec_app_mode(void) { return A->scr ? A->scr : A->cat; }
static void fn_update(void) { A->fn = P->shift_held || P->shift_sticky; }

/* the FM-1 button -> the mode button it is (EC_MB_*), and back */
static int mb_of(int b)
{
    switch (b) {
    case EC_B_FX: return EC_MB_FX;
    case EC_B_SEL: return EC_MB_SEL;
    case EC_B_ENV: return EC_MB_ENV;
    case EC_B_LFO: return EC_MB_LFO;
    case EC_B_EDIT: return EC_MB_EDIT;
    case EC_B_HOME: return EC_MB_HOME;
    case EC_B_SAVE: return EC_MB_FILE;
    case EC_B_ARP: return EC_MB_ARP;
    case EC_B_SEQ: return EC_MB_SEQ;
    default: return EC_MB_NONE;
    }
}

int ec_pn_f(int which)
{
    if (ec_app_mode() == EC_MODE_KEYBOARD || ec_app_popup(A))
        return 0;
    return ((P->f_held | P->f_sticky) & which) != 0;
}

int ec_pn_popup_shows(void) { return P->pop != 0; }

int ec_app_popup(const ec_app_t *a)
{
    return a->e5.naming || a->e5.confirm || a->ux.f3 || a->settings || a->rec.open || a->pn.as_open ||
           a->pn.rs_open;
}

/* ------------------------------------------------------------------------------------------- init --- */
void ec_pn_init(void)
{
    int i;
    for (i = 0; i < EC_NMB; i++)
        P->last[i] = -1;
    P->btn = -1;
    P->xf_key = -1;
    UX->pre_idx = -1;
    A->mk.held = A->mk.hot_id = -1;
    ec_as_load();
    ec_gfx_set_theme(ec_as->theme);
    P->pv_handle = -1;                             /* FILE: the browser at /audio, no preview */
    snprintf(P->fdir, sizeof P->fdir, "/audio");
    P->lr_end = -1;
    ec_click_init(&P->click, (float)EC_FS);
    P->last[EC_MB_HOME] = EC_CAT_SRC;
    if (ec_as->intro) {
        P->intro = 1;
        P->intro_ms = A->ms;
        P->intro_frame = 0;
    }
}

/* ------------------------------------------------------------------------------------------- the modes --- */
static void popup(int mode)
{
    if (!ec_as->popup)
        return;
    P->pop = mode + 1;
    P->pop_ms = A->ms ? A->ms : 1;
}

static void settings_close(void)
{
    A->settings = 0;
    if (P->set_scr) {                              /* back to the mode the settings were opened over */
        const int m = P->set_scr - 1;
        P->set_scr = 0;
        if (m >= EC_NCAT)
            A->scr = m;
        else
            A->cat = m;
    }
    A->redraw = 1;
}

/* a mode's settings over whatever shows (F1 / F2 / its button close it, back to that mode) */
static void settings_open(int cat)
{
    if (A->settings && A->cat == cat) {            /* SHIFT + the same (or a second hold): back */
        settings_close();
        return;
    }
    if (cat < 0 || !ec_app_has_settings(cat)) {
        ec_app_message("NO SETTINGS");
        return;
    }
    if (A->settings)
        settings_close();
    P->set_scr = ec_app_mode() + 1;
    A->scr = 0;
    A->cat = cat;
    A->settings = 1;
    UX->set_lane = 0;
    A->redraw = 1;
}

static void overlays_close(void)
{
    if (A->settings)
        settings_close();
    P->set_scr = 0;
    UX->f3 = 0;
    if (P->as_open)
        ec_as_close(1);
    if (P->rs_open)
        ec_rs_close(1);
}

void ec_pn_enter(int mode)
{
    const int was = ec_app_mode();
    const int b = ec_mode_button(mode);
    if (mode < 0 || mode >= EC_NMODE)
        return;
    if (mode == EC_CAT_FILE && was != EC_CAT_FILE && (A->seq.playing || ec_seq_counting_in(&A->seq))) {
        ec_app_alert("STOP FIRST");                /* FILE can't open while playing: its button flashes */
        P->file_refused_ms = A->ms ? A->ms : 1;
        return;
    }
    overlays_close();
    ec_mk_off();                                   /* a source key's hold ends (no tap) */
    P->src_key = P->slot_key = 0;
    E->anchor = 0;
    P->cat = 0;
    if (was == EC_MODE_SAMPLER && mode != EC_MODE_SAMPLER)
        ec_sampler_enter(0);
    if (was == EC_CAT_FILE && mode != EC_CAT_FILE)
        ec_file_leave();                           /* the preview stops */
    if (mode >= EC_NCAT)
        A->scr = mode;
    else {
        A->scr = 0;
        A->cat = mode;
    }
    if (b > EC_MB_NONE && b < EC_NMB)
        P->last[b] = mode;
    if (mode == EC_CAT_MASTER)                     /* MIX: MASTER is what it shows on the MST target */
        ec_app_set_page(EC_CAT_MASTER, A->bus == 3 && ec_cat_pages(EC_CAT_MASTER, 0) > 1 ? 1 : 0);
    if (mode == EC_CAT_XFD && was != EC_CAT_XFD)   /* A / B held: the knobs lock the page it came from */
        P->xf_from = ec_src_mode(was) ? was : EC_CAT_FILT;
    if (mode == EC_MODE_SAMPLER && was != EC_MODE_SAMPLER)
        ec_sampler_enter(1);
    if (mode != was && A->rec.open)                /* (the recorder's setup belongs to the mode it opened in) */
        ec_rec_close();
    if (mode != was) {
        P->shift_sticky = 0;                       /* sticky SHIFT ends with the mode */
        fn_update();
        popup(mode);
    }
    A->redraw = 1;
}

void ec_pn_goto(int cat, int page)
{
    if (cat < 0 || cat >= EC_NCAT)
        return;
    ec_rec_close();
    ec_pn_enter(cat);
    ec_app_set_page(cat, page);
    A->redraw = 1;
}

/* the lit button tapped again: the next page (HOME / EDIT / FILE: the next sub-mode); another: its mode */
static void mode_tap(int b)
{
    const int mb = mb_of(b);
    int modes[8], n = ec_button_modes(mb, modes, 8), cur, i;
    if (n <= 0)
        return;
    if (P->cat) {                                  /* the CAT: any mode button leaves it (EDIT: back to MIX) */
        P->cat = 0;
        A->redraw = 1;
        if (mb == EC_MB_EDIT)
            return;
    }
    if (A->settings) {
        const int back = P->set_scr ? P->set_scr - 1 : A->cat;
        settings_close();
        if (ec_mode_button(back) == mb)            /* its own button: back to its pages */
            return;
    }
    cur = ec_app_mode();
    if (ec_mode_button(cur) == mb) {
        if (n > 1) {
            for (i = 0; i < n && modes[i] != cur; i++)
                ;
            ec_pn_enter(modes[(i + 1) % n]);
        } else if (cur < EC_NCAT) {
            if (cur == EC_CAT_FX && A->bus) {      /* FX on a bus: back to the track's pages */
                A->bus = 0;
                ec_app_set_page(cur, 0);
            } else
                ec_app_set_page(cur, (A->page[cur] + 1) % ec_cat_pages(cur, ec_app_params(A)));
        }
        A->redraw = 1;
        return;
    }
    {
        int m = P->last[mb] >= 0 ? P->last[mb] : modes[0];
        if (m == EC_CAT_FILE && (A->seq.playing || ec_seq_counting_in(&A->seq)))
            m = EC_MODE_SAMPLER;                   /* playing: FILE can't open, its SAMPLER (not gated) does */
        ec_pn_enter(m);
    }
}

/* a hold or SHIFT + the button: that mode's settings (SEQ: F3; HOME: LOOP ENGINE's, with KEYBOARD's ADVANCE) */
static void mode_settings(int b)
{
    const int mb = mb_of(b);
    int modes[8];
    const int n = ec_button_modes(mb, modes, 8);
    switch (mb) {
    case EC_MB_SEQ:
        overlays_close();
        UX->f3 = 1;
        break;
    case EC_MB_HOME: settings_open(EC_CAT_SRC); break;
    case EC_MB_EDIT: settings_open(EC_CAT_MASTER); break;   /* track settings: the name, global BPM, return mode */
    case EC_MB_FILE: ec_file_settings(); break;
    default:
        if (n > 0 && modes[0] < EC_NCAT)
            settings_open(modes[0]);
        break;
    }
    A->redraw = 1;
}

/* ------------------------------------------------------------------------------------------- targets --- */
void ec_pn_select_target(int i)      /* 0..5 tracks, 6 S1, 7 S2, 8 MST */
{
    i = clampi(i, 0, EC_NTRACK + 2);
    if (i < EC_NTRACK) {
        A->track = i;
        A->bus = 0;
    } else
        A->bus = i - EC_NTRACK + 1;
    if (ec_app_mode() == EC_CAT_MASTER && !A->settings)
        ec_app_set_page(EC_CAT_MASTER, A->bus == 3 && ec_cat_pages(EC_CAT_MASTER, 0) > 1 ? 1 : 0);
    A->redraw = 1;
}

static const char *target_name(int i)
{
    static const char *const N[9] = {"T1", "T2", "T3", "T4", "T5", "T6", "S1", "S2", "MST"};
    return N[clampi(i, 0, 8)];
}

/* ------------------------------------------------------------------------------------------- SHIFT's layer --- */
static void tap_tempo(void)
{
    char b[16];
    int n;
    if (UX->ntap && A->ms - UX->tap_ms[UX->ntap - 1] > 2000)
        UX->ntap = 0;
    if (UX->ntap == 5) {
        memmove(&UX->tap_ms[0], &UX->tap_ms[1], 4 * sizeof UX->tap_ms[0]);
        UX->ntap = 4;
    }
    UX->tap_ms[UX->ntap++] = A->ms ? A->ms : 1;
    n = UX->ntap - 1;
    if (n >= 1) {   /* the average of the last (up to four) intervals */
        const float avg = (float)(UX->tap_ms[UX->ntap - 1] - UX->tap_ms[0]) / (float)n;
        if (avg > 0) {
            float bpm = 60000.0f / avg;
            bpm = bpm < 20 ? 20 : bpm > 300 ? 300 : bpm;
            ec_e5_edit_global(EC_G_BPM, A->proj.bpm);
            ec_seq_set_bpm(&A->seq, floorf(bpm * 10.0f + 0.5f) / 10.0f);
            ec_app_mark_dirty();
        }
    }
    snprintf(b, sizeof b, "%d", (int)floorf(A->proj.bpm + 0.5f));
    ec_ux_takeover("TAP", b, (A->proj.bpm - 20.0f) / 280.0f);
}

/* SHIFT + B1 TAP TEMPO, B2 LATCH, B3 MEMORIZE, B4 RECALL, B11 FILL (held); B5-B10 dark */
static void shift_black(int key, int down)
{
    if (!down) {
        if (key == 11 && A->fill_held) {
            A->fill_held = 0;
            ec_seq_set_fill(&A->seq, 0, A->fill_latched);
        }
        return;
    }
    switch (key) {
    case 1: tap_tempo(); break;
    case 2:
        A->fill_latched = !A->fill_latched;
        ec_seq_set_fill(&A->seq, A->fill_held, A->fill_latched);
        ec_app_message(A->fill_latched ? "FILL LATCHED" : "FILL OFF");
        break;
    case 3: ec_e5_memorize(); break;
    case 4: ec_e5_recall_memo(); break;
    case 11:
        A->fill_held = 1;
        ec_seq_set_fill(&A->seq, 1, A->fill_latched);
        break;
    default: break;
    }
    A->redraw = 1;
}

/* ------------------------------------------------------------------------------------------- buttons --- */
/* a pop-up takes F1 (back / cancel), F2 (OK / confirm) and HOME first; 1 = the press is used */
static int popup_button(int b)
{
    const int f = b == EC_B_OCTDN || b == EC_B_OCTUP;
    if (A->e5.naming) {
        if (f || b == EC_B_HOME)
            ec_e5_name_close(b == EC_B_OCTUP);
        return b != EC_B_PLAY && b != EC_B_FN;     /* the transport stays live, the rest waits */
    }
    if (A->e5.confirm) {
        if (f || b == EC_B_HOME) {
            ec_e5_answer(b == EC_B_OCTUP);
            return 1;
        }
        if (b != EC_B_FN)
            ec_e5_answer(0);
        return 0;
    }
    if (P->as_open) {                              /* APP SETTINGS: F1 back, F2 OK, HOME closes (keeps) */
        if (f || b == EC_B_HOME) {
            ec_as_close(b != EC_B_OCTDN);
            return 1;
        }
        if (b == EC_B_PLAY || b == EC_B_FN)
            return 0;
        ec_as_close(1);
        return 0;
    }
    if (P->rs_open) {                              /* the record settings: F1 back, F2 / REC OK, HOME closes (keeps) */
        if (f || b == EC_B_HOME || b == EC_B_REC) {
            ec_rs_close(b != EC_B_OCTDN);
            return 1;
        }
        if (b == EC_B_PLAY || b == EC_B_FN)
            return 0;
        ec_rs_close(1);
        return 0;
    }
    if (A->rec.open && b != EC_B_HOME && b != EC_B_SAVE && !A->fn)   /* the recorder's setup (FILE held): REC, F1, F2 */
        return ec_rec_button(b);
    if (UX->f3 && (f || b == EC_B_HOME || b == EC_B_SEQ)) {
        UX->f3 = 0;
        P->btn = -1;
        A->redraw = 1;
        return 1;
    }
    if (A->settings && f) {
        settings_close();
        return 1;
    }
    return 0;
}

static void button_down(int b)
{
    if (b == EC_B_FN) {
        P->shift_held = 1;
        UX->shift_ms = A->ms;
        UX->shift_used = 0;
        fn_update();
        A->redraw = 1;
        return;
    }
    if (A->fn)
        UX->shift_used = 1;
    if (popup_button(b)) {
        A->redraw = 1;
        return;
    }
    if (b == EC_B_OCTDN || b == EC_B_OCTUP) {
        const int bit = b == EC_B_OCTDN ? 1 : 2;
        if (!A->fn && b == EC_B_OCTUP && ec_app_mode() == EC_CAT_FILE && !A->settings &&
            ec_file_page() == EC_PIC_FILE) {      /* FILE: F2 on a pool file frees its slot */
            ec_file_f2();
        } else if (ec_rzui_active(A)) {                   /* E3, page-local: F2 split / F1 delete (SHIFT: auto-chop / even) */
            ec_rzui_oct(A, b == EC_B_OCTUP);
        } else if (A->fn) {                        /* SHIFT + F1 / F2: undo / redo */
            ec_e5_undo(b == EC_B_OCTUP);
        } else if (ec_app_mode() == EC_MODE_KEYBOARD) {   /* KEYBOARD: the octave */
            ec_kb_oct(b == EC_B_OCTUP ? 1 : -1);
        } else {
            P->f_held |= bit;
            P->f_used &= ~bit;
            P->f_ms[bit - 1] = A->ms;
            if (P->f_held == 3) {                  /* F1 + F2: F3 */
                ec_clip_reset();
                P->f_sticky = 0;
                P->f_used = 3;
                overlays_close();
                UX->f3 = 1;
            }
        }
        A->redraw = 1;
        return;
    }
    switch (b) {
    case EC_B_PLAY:                                /* start / stop at once, on press (FILE: nothing) */
        if (ec_app_mode() == EC_CAT_FILE)
            break;
        if (A->seq.playing) {
            ec_seq_stop(&A->seq);
            ec_liverec_stop();
        } else
            ec_seq_start(&A->seq);
        if (A->dirty)
            A->dirty_ms = A->ms;
        break;
    case EC_B_REC:
        if (ec_app_mode() == EC_CAT_FILE)          /* (FILE: nothing) */
            break;
        if (A->fn)
            ec_liverec_settings();
        else {
            P->rec_down = 1;
            P->rec_used = 0;
            P->rec_ms = A->ms;
        }
        break;
    default:
        if (mb_of(b) == EC_MB_NONE)
            break;
        if (A->fn) {                               /* SHIFT + a mode button */
            if (b == EC_B_HOME) {
                overlays_close();
                ec_as_open();                      /* APP SETTINGS */
            } else if (b == EC_B_SAVE)
                ec_app_save(0);
            else
                mode_settings(b);
            P->shift_sticky = 0;
            fn_update();
            break;
        }
        P->btn = b;                                /* tap on release, hold = the settings */
        P->btn_ms = A->ms;
        P->btn_used = 0;
        break;
    }
    A->redraw = 1;
}

static void button_up(int b)
{
    switch (b) {
    case EC_B_FN:
        if (P->shift_held && !UX->shift_used && A->ms - UX->shift_ms < EC_HOLD_MS)
            P->shift_sticky = !P->shift_sticky;    /* a quick tap: sticky on / off */
        else if (P->shift_sticky && UX->shift_used)
            P->shift_sticky = 0;
        P->shift_held = 0;
        fn_update();
        break;
    case EC_B_OCTDN:
    case EC_B_OCTUP: {
        const int bit = b == EC_B_OCTDN ? 1 : 2;
        if (P->f_held & bit) {
            P->f_held &= ~bit;
            if (!(P->f_used & bit) && A->ms - P->f_ms[bit - 1] < EC_HOLD_MS)
                P->f_sticky = P->f_sticky & bit ? 0 : bit;   /* a tap: sticky (F1 and F2 one at a time) */
            P->f_used &= ~bit;
            if (!(P->f_held | P->f_sticky))
                ec_clip_reset();                   /* the buffer empties when F1 / F2 turns off */
        }
    } break;
    case EC_B_REC:
        if (P->rec_down && !P->rec_used)
            ec_liverec_toggle();
        P->rec_down = 0;
        break;
    default:
        if (P->btn == b) {
            if (!P->btn_used)
                mode_tap(b);
            P->btn = -1;
        }
        break;
    }
    A->redraw = 1;
}

/* ------------------------------------------------------------------------------------------- white keys --- */
static float key_vel(int vel) { return (float)vel / 127.0f; }

static void step_down(int k, int i)
{
    A->key_role[k] = R_STEP;
    A->step_held |= (uint16_t)(1u << i);
    A->step_edited &= (uint16_t)~(1u << i);
    A->step_ms[i] = A->ms;
    A->tap_key = -1;
    ec_e5_step_down(i);                            /* E5: step preview while stopped */
}

static void step_up(int i, int vel)
{
    const int quick = A->ms - A->step_ms[i] < TAP_MS;
    const int edited = A->step_edited >> i & 1u;
    A->step_held &= (uint16_t)~(1u << i);
    A->step_edited &= (uint16_t)~(1u << i);
    if (!A->step_held)
        A->prog_keys = 0;
    ec_e5_step_up();
    if (quick && !edited && !A->bus) {
        ec_ptrack_t *tr = ec_project_track(&A->proj, A->track);
        const int idx = A->step_page * 16 + i;
        ec_e5_steps_edit((uint16_t)(1u << i), "STEP", 0x900u + (uint32_t)idx);   /* E5: undo */
        if (ec_step_toggle(tr, idx) && vel != 100)          /* (100: a host without key velocity) */
            ec_step_lock_set(&tr->steps[idx], EC_P_VELOCITY, (float)vel / 127.0f);
        ec_seq_touch(&A->seq, A->track);
        ec_app_mark_dirty();
    }
}

/* LOOP ENGINE: the white keys play the loop (slice tracks: the pads) */
static void loop_down(int k, int i, int vel)
{
    const float *v = ec_app_params(A);
    const int lk = ec_e5_loop_key(k, i, 1);
    if (A->bus) {
        A->key_role[k] = R_SWALLOW;
        return;
    }
    ec_liverec_capture();                          /* live REC: CAPTURE's first key starts playback */
    if (is_slice_m(iv(v, EC_P_MACHINE))) {
        ec_seq_slice_pad(&A->seq, A->track, lk, 1, key_vel(vel));
        A->key_role[k] = R_PAD;
    } else {
        ec_seq_loop_key(&A->seq, A->track, lk, 1);
        A->key_role[k] = R_LOOP_LIVE;
    }
    ec_liverec_loop(lk, is_slice_m(iv(v, EC_P_MACHINE)), vel);   /* (recording: a trig + the loop position) */
}

static void white_down(int k, int i, int vel, int lean)
{
    const int mode = ec_app_mode();
    if (UX->f3 && !A->e5.naming) {                 /* F3's pages: tap = edit the page, hold = the loop */
        UX->f3_keys |= (uint16_t)(1u << i);
        UX->f3_used &= (uint16_t)~(1u << i);
        UX->f3_ms[i] = A->ms;
        A->key_role[k] = R_F3;
        return;
    }
    if (ec_e5_lower_down(k, i, vel))               /* E5: naming swallows */
        return;
    if (P->as_open || P->cat) {
        A->key_role[k] = R_SWALLOW;
        if (P->cat && mode == EC_CAT_MASTER && i < 9 && !A->fn) {   /* the CAT: the track keys still work */
            ec_mix_track_down(i);
            A->key_role[k] = R_TRK;
        }
        return;
    }
    if (P->slot_key && !A->fn && ec_src_white(i)) {   /* FX's slot key held + a white key: that engine */
        A->key_role[k] = R_SWALLOW;
        return;
    }
    if (A->fn) {                                   /* SHIFT + a white key */
        UX->shift_used = 1;
        A->key_role[k] = R_SWALLOW;
        switch (mode) {
        case EC_CAT_XFD:                           /* jump */
            if (i < 12) {
                ec_xfd_white(i, 1, lean);
                A->key_role[k] = R_XFD;
                return;
            }
            break;
        case EC_CAT_MASTER:                        /* FILL held + a track key: all on; else solo */
            if (i < EC_NTRACK && !A->settings) {
                if (A->fill_held)
                    ec_app_clear_mutes();
                else
                    ec_app_toggle_solo(i);
                return;
            }
            break;
        case EC_MODE_KEYBOARD:                     /* with a target step: the other advance */
            if (P->target) {
                ec_kb_key(k, 1, vel);
                A->key_role[k] = R_KB;
                return;
            }
            break;
        default: break;
        }
        if (i < 9)
            ec_pn_select_target(i);                /* W1..W9: T1..T6 S1 S2 MST */
        return;
    }
    if (!A->settings)
        switch (mode) {
        case EC_CAT_MASTER:
            A->key_role[k] = R_SWALLOW;
            if (i < 9) {
                ec_mix_track_down(i);
                A->key_role[k] = R_TRK;
            }
            return;
        case EC_CAT_SRC: loop_down(k, i, vel); return;
        case EC_MODE_KEYBOARD:
            ec_kb_key(k, 1, vel);
            A->key_role[k] = R_KB;
            return;
        case EC_CAT_XFD:
            ec_xfd_white(i, 1, lean);
            A->key_role[k] = R_XFD;
            return;
        case EC_CAT_PAT:
            A->key_role[k] = R_SWALLOW;
            ec_pat_white(i);
            return;
        case EC_MODE_SAMPLER: A->key_role[k] = R_SWALLOW; return;   /* (the input's VU) */
        case EC_CAT_FILE:
            if (ec_file_page() == EC_PIC_EDITOR) { /* the editor: the trim */
                ec_file_white(i, 1);
                A->key_role[k] = R_SWALLOW;
                return;
            }
            if (ec_file_page() == EC_PIC_FILE) {   /* (the browser: nothing to step) */
                A->key_role[k] = R_SWALLOW;
                return;
            }
            break;
        default:
            if (ec_rzui_active(A)) {               /* E3: the RAZOR page (page-local): the keys are its slices */
                ec_rzui_key(A, i, 1, vel);
                A->key_role[k] = R_RAZOR;
                return;
            }
            break;
        }
    if (ec_pn_f(1) || ec_pn_f(2)) {                /* §2: copy / cut / paste */
        P->f_used = 3;
        ec_clip_tap(EC_CK_STEP, A->step_page * 16 + i);
        A->key_role[k] = R_SWALLOW;
        return;
    }
    step_down(k, i);
}

static void white_up(int k, int i, int vel)
{
    const int r = A->key_role[k];
    if (r == R_F3) {                               /* a tap edits the page (a hold already looped it) */
        if (!(UX->f3_used >> i & 1u))
            ec_e5_page_key(i, EC_PM_SELECT);
        UX->f3_keys &= (uint16_t)~(1u << i);
    } else if (ec_e5_lower_up(k, i))
        return;
    switch (r) {
    case R_STEP: step_up(i, vel); break;
    case R_LOOP_LIVE: ec_seq_loop_key(&A->seq, A->track, ec_e5_loop_key(k, i, 0), 0); break;
    case R_PAD: ec_seq_slice_pad(&A->seq, A->track, ec_e5_loop_key(k, i, 0), 0, 0); break;
    case R_KB: ec_kb_key(k, 0, 0); break;
    case R_RAZOR: ec_rzui_key(A, i, 0, 0); break;
    case R_TRK: ec_mix_track_up(i); break;
    case R_XFD: ec_xfd_white(i, 0, 0); break;
    default: break;
    }
    A->key_role[k] = R_NONE;
}

/* ------------------------------------------------------------------------------------------- black keys --- */
static void black_down(int k, int key, int vel)
{
    const int mode = ec_app_mode();
    const uint16_t bit = (uint16_t)(1u << (key - 1));
    P->bk_held |= bit;
    P->bk_used &= (uint16_t)~bit;
    P->bk_ms[key - 1] = A->ms;
    A->key_role[k] = R_SWALLOW;
    if (ec_e5_upper(key - 1, 1) || P->as_open || UX->f3)
        return;
    if (A->fn) {                                   /* SHIFT + a black key */
        UX->shift_used = 1;
        if (mode == EC_CAT_XFD && (key == 1 || key == 2) && !A->settings) {   /* SHIFT + A / B: clear */
            ec_xfd_black(key, 1);
            A->key_role[k] = R_XFD;
            return;
        }
        if ((mode == EC_CAT_FILE || mode == EC_MODE_SAMPLER) && key == 11) {  /* SHIFT + B11: purge */
            ec_file_black(key, 1);
            A->key_role[k] = R_FILEB;
            return;
        }
        if (mode == EC_MODE_KEYBOARD && P->target && !ec_kb_slice_track()) {  /* a sharp: the other advance */
            ec_kb_key(k, 1, vel);
            A->key_role[k] = R_KB;
            return;
        }
        shift_black(key, 1);
        A->key_role[k] = R_SHIFTB;
        return;
    }
    if (A->settings)
        return;
    switch (mode) {
    case EC_CAT_MASTER: ec_mix_black(key); break;
    case EC_MODE_PAGES:
        ec_pages_black_down(key);
        A->key_role[k] = R_PAGEB;
        break;
    case EC_MODE_KEYBOARD:
        ec_kb_key(k, 1, vel);
        A->key_role[k] = R_KB;
        break;
    case EC_CAT_XFD:
        ec_xfd_black(key, 1);
        A->key_role[k] = R_XFD;
        break;
    case EC_CAT_PAT: ec_pat_black(key); break;
    case EC_CAT_FILE:
    case EC_MODE_SAMPLER:
        ec_file_black(key, 1);
        A->key_role[k] = R_FILEB;
        break;
    default:
        if (ec_src_mode(mode)) {
            ec_src_down(key);
            A->key_role[k] = R_SRCB;
        }
        break;
    }
}

static void black_up(int k, int key)
{
    const int r = A->key_role[k];
    P->bk_held &= (uint16_t)~(1u << (key - 1));
    ec_e5_upper(key - 1, 0);
    switch (r) {
    case R_SRCB: ec_src_up(key); break;
    case R_PAGEB: ec_pages_black_up(key); break;
    case R_KB: ec_kb_key(k, 0, 0); break;
    case R_XFD: ec_xfd_black(key, 0); break;
    case R_SHIFTB: shift_black(key, 0); break;
    case R_FILEB: ec_file_black(key, 0); break;
    default: break;
    }
    A->key_role[k] = R_NONE;
}

/* ------------------------------------------------------------------------------------------- knobs --- */
/* PRESETS: the track's pool slot, walking the used ones (empty ones skipped), heard at once; a held step locks it */
static int slot_used(int s)
{
    const ec_slotrt_t *r = &A->slot[s];
    return A->proj.slot[s].path[0] || r->builtin || r->state == EC_SL_READY;
}

static int next_used_slot(int from, int d)
{
    int s = from, n;
    for (n = 0; n < EC_NSLOT; n++) {
        s = ((s + (d > 0 ? 1 : -1)) % EC_NSLOT + EC_NSLOT) % EC_NSLOT;
        if (slot_used(s))
            return s;
    }
    return from;
}

static void presets(int d)
{
    float *v = ec_app_params(A);
    int s, used = 0, pos = 0, i, steps = d < 0 ? -d : d;
    char lb[16], sub[16];
    const ec_ss_sample_t *x;
    if (A->bus)
        return;
    {   /* the slot the knob starts from: the held step's lock, else the track's */
        const int hs = ec_app_held_step(A);
        float lk;
        s = clampi(iv(v, EC_P_SLOT), 1, EC_NSLOT) - 1;
        if (hs >= 0 && ec_step_lock_get(&ec_project_track(&A->proj, A->track)->steps[hs], EC_P_SLOT, &lk))
            s = clampi((int)(lk + 0.5f), 1, EC_NSLOT) - 1;
    }
    for (i = 0; i < steps; i++)
        s = next_used_slot(s, d);
    if (!slot_used(s)) {
        ec_app_message("POOL EMPTY");
        return;
    }
    if (A->step_held) {                            /* hold a step + PRESETS: a slot lock on the held steps */
        ec_e5_edit_param(A->track, EC_P_SLOT, 1);
        for (i = 0; i < EC_NLOWER; i++)
            if (A->step_held >> i & 1u) {
                ec_step_t *st = &ec_project_track(&A->proj, A->track)->steps[A->step_page * 16 + i];
                if (ec_step_lock_set(st, EC_P_SLOT, (float)(s + 1)) < 0)
                    ec_app_message("LOCKS FULL");
                st->trig = 1;
            }
        A->step_edited |= A->step_held;
    } else {
        ec_e5_edit_param(A->track, EC_P_SLOT, 0);
        v[EC_P_SLOT] = (float)(s + 1);
    }
    ec_seq_touch(&A->seq, A->track);
    ec_app_mark_dirty();
    for (i = 0; i < EC_NSLOT; i++)
        if (slot_used(i)) {
            if (i < s)
                pos++;
            used++;
        }
    x = A->slot[s].handle >= 0 ? ec_ss_ready(A->slot[s].handle) : 0;
    snprintf(lb, sizeof lb, "%sSLOT %d", A->step_held ? "LOCK " : "", s + 1);
    ec_ux_takeover(lb, x ? x->name : A->proj.slot[s].path[0] ? strrchr(A->proj.slot[s].path, '/') ?
                                                              strrchr(A->proj.slot[s].path, '/') + 1 :
                                                              A->proj.slot[s].path : "EMPTY",
                   used > 1 ? (float)pos / (float)(used - 1) : 1.0f);
    {   /* the pool's use under the name */
        const uint32_t cap = ec_ss_capacity_bytes();
        snprintf(sub, sizeof sub, "%d/%d %d%%", pos + 1, used, cap ? (int)((uint64_t)ec_ss_used_bytes() * 100u / cap) : 0);
    }
    UX->to_kind = EC_TO_SAMPLE;
    snprintf(UX->to_sub, sizeof UX->to_sub, "%s", sub);
}

static void select_knob(int d)
{
    const int mode = ec_app_mode();
    if (ec_rzui_active(A)) {
        ec_rzui_select(A, d);                      /* E3: SELECT picks the slice on the RAZOR page */
        return;
    }
    if (A->settings) {
        ec_page_t pg;
        ec_app_page(A, &pg);
        UX->set_lane = clampi(ec_app_lane(A, &pg) + d, 0, ec_ux_lanes(pg.n) - 1);
        return;
    }
    switch (mode) {
    case EC_MODE_KEYBOARD:
    case EC_MODE_PAGES:
    case EC_MODE_SAMPLER:
    case EC_CAT_XFD: return;
    case EC_CAT_PAT:                               /* the bank */
        P->bank = clampi(P->bank + (d > 0 ? 1 : -1), 0, EC_NBANK - 1);
        A->redraw = 1;
        return;
    default: break;
    }
    if (mode == EC_CAT_FILE && ec_file_page() == EC_PIC_FILE) {
        ec_file_select(d);                         /* the browser: the cursor (folders, files) */
    } else if (!A->bus && ec_page_pic(A->cat, A->page[A->cat], 0) == EC_PIC_MATRIX) {
        if (!ec_mk_select(d))                      /* 0.5.0: the MATRIX page's row, then on to the next page */
            ec_app_lane_step(d > 0 ? 1 : -1);
    } else {
        ec_app_lane_step(d);
        if (!A->bus && ec_page_pic(A->cat, A->page[A->cat], 0) == EC_PIC_MATRIX)
            A->mk.row = d < 0 ? 7 : 0;             /* (onto MATRIX backwards: its last row) */
    }
}

static void knob(int r, int d)
{
    const int mode = ec_app_mode();
    if (ec_e5_knob(r, d))                          /* E5: naming */
        return;
    if (P->as_open) {
        ec_as_knob(r, d);
        return;
    }
    if (P->rs_open) {                              /* the record settings */
        ec_rs_knob(r, d);
        return;
    }
    if (!A->fn && ec_rec_knob(r, d))               /* the SAMPLER's stand-in: the REC pop-up's knobs */
        return;
    if (UX->f3) {                                  /* F3: SELECT tempo, ALGO swing, PRESETS the name, K1-K4 */
        if (r == EC_E_SELECT || r == EC_E_ALGO) {
            const int id = r == EC_E_SELECT ? EC_G_BPM : EC_G_SWING;
            ec_app_edit_global(id, d, ec_ux_accel(r, id == EC_G_BPM ? 280 : 1, d));
        } else if (r == EC_E_PRESETS)
            ec_e5_name_pattern();
        else
            ec_app_knob_cell(r - EC_E_K1, d);
        return;
    }
    if (A->fn) {                                   /* SHIFT: SELECT BPM, ALGO swing, PRESETS the track, K snap */
        UX->shift_used = 1;
        if (r == EC_E_SELECT || r == EC_E_ALGO) {
            const int id = r == EC_E_SELECT ? EC_G_BPM : EC_G_SWING;
            ec_app_edit_global(id, d, ec_ux_accel(r, id == EC_G_BPM ? 280 : 1, d));
            return;
        }
        if (r == EC_E_PRESETS) {
            const int cur = A->bus ? EC_NTRACK - 1 + A->bus : A->track;
            ec_pn_select_target(cur + (d > 0 ? 1 : -1));
            {
                const int now = A->bus ? EC_NTRACK - 1 + A->bus : A->track;
                ec_ux_takeover("TRACK", target_name(now), (float)now / 8.0f);
            }
            return;
        }
    }
    if (ec_src_knob(r, d))                         /* a slot key / quick setting held */
        return;
    switch (r) {
    case EC_E_SELECT: select_knob(d); break;
    case EC_E_ALGO:
        if (mode == EC_MODE_KEYBOARD)
            ec_kb_target(d);                       /* KEYBOARD: the target step */
        else {                                     /* the step page */
            const int pages = (ec_seq_pattern_steps(&A->seq, A->track) + 15) / 16;
            const int p = clampi(A->step_page + (d > 0 ? 1 : -1), 0, pages - 1);
            char b[16];
            if (p != A->step_page) {
                A->step_page = p;
                A->step_held = 0;
            }
            snprintf(b, sizeof b, "PAGE %d/%d", p + 1, pages);
            ec_app_message(b);
        }
        break;
    case EC_E_PRESETS: presets(d); break;
    default:
        if (P->cat)
            break;
        ec_app_knob_cell(r - EC_E_K1, d);
        break;
    }
}

/* ------------------------------------------------------------------------------------------- holds --- */
static void holds_tick(void)
{
    int i;
    if (P->btn >= 0 && !P->btn_used && A->ms - P->btn_ms >= EC_HOLD_MS) {
        P->btn_used = 1;                           /* a mode button held 400 ms: its settings (they stay open) */
        mode_settings(P->btn);
    }
    if (P->rec_down && !P->rec_used && A->ms - P->rec_ms >= EC_HOLD_MS) {
        P->rec_used = 1;
        ec_liverec_settings();
    }
    ec_mix_hold_tick();
    ec_pages_hold_tick();
    ec_src_hold_tick();
    for (i = 0; i < EC_NLOWER; i++)                /* F3: a page key held: the page in / out of the loop */
        if ((UX->f3_keys >> i & 1u) && !(UX->f3_used >> i & 1u) && A->ms - UX->f3_ms[i] >= EC_HOLD_MS) {
            UX->f3_used |= (uint16_t)(1u << i);
            ec_e5_page_key(i, EC_PM_LOOP);
            A->redraw = 1;
        }
    if (UX->to_ms && A->ms - UX->to_ms >= EC_TAKEOVER_MS) {
        UX->to_ms = 0;
        A->redraw = 1;
    }
    if (P->pop && A->ms - P->pop_ms >= POPUP_MS) {
        P->pop = 0;
        A->redraw = 1;
    }
    if (P->shift_held && !UX->shift_used && A->ms - UX->shift_ms == EC_SHIFTMAP_MS)
        A->redraw = 1;
}

/* any knob, key or button: the intro and the pop-up go (what was pressed still acts) */
static void dismiss(void)
{
    if (P->intro) {
        P->intro = 0;
        A->redraw = 1;
    }
    if (P->pop) {
        P->pop = 0;
        A->redraw = 1;
    }
}

/* ------------------------------------------------------------------------------------------- the tick --- */
void ec_pn_tick(const void *vin)
{
    const ec_input_t *in = (const ec_input_t *)vin;
    int b, k, r;
    if (in->btn_down || in->key_down)
        dismiss();
    for (r = 0; r < EC_NE; r++)
        if (in->enc[r])
            dismiss();
    if ((in->btn_up >> EC_B_FN & 1u) && !(in->btn_down >> EC_B_FN & 1u))
        button_up(EC_B_FN);                        /* SHIFT let go in the tick another button went down: let go first */
    for (b = 0; b < EC_NB; b++)
        if (in->btn_down >> b & 1u)
            button_down(b);
    for (k = 0; k < EC_NKEY; k++) {
        if (in->key_down >> k & 1u) {
            const int lo = ec_key_lower(k), up = ec_key_upper(k);
            if (lo >= 0)
                white_down(k, lo, in->key_vel[k], in->key_lean[k]);
            else if (up >= 0)
                black_down(k, up + 1, in->key_vel[k]);
            if (P->src_key || P->slot_key)         /* (a key during a hold: the hold was used) */
                P->bk_used |= P->bk_held;
            A->redraw = 1;
        }
        if (in->key_up >> k & 1u) {
            const int lo = ec_key_lower(k), up = ec_key_upper(k);
            if (lo >= 0)
                white_up(k, lo, in->key_vel[k]);
            else if (up >= 0)
                black_up(k, up + 1);
            A->redraw = 1;
        }
    }
    for (r = 0; r < EC_NE; r++) {
        const int d = in->enc[r];
        if (!d)
            continue;
        if (A->fn && r >= EC_E_K1)
            UX->shift_used = 1;
        if (P->bk_held)
            P->bk_used |= P->bk_held;              /* a knob during a black key's hold: no tap */
        knob(r, d);
        A->redraw = 1;
    }
    for (b = 0; b < EC_NB; b++)
        if ((in->btn_up >> b & 1u) && (b != EC_B_FN || (in->btn_down >> EC_B_FN & 1u)))
            button_up(b);
    {   /* 0.5.0: the live controls: aftertouch (the highest held key, on the selected track), CC 1, CC 11; APP
         * SETTINGS turns them off. In CROSSFADER the pressure is the glide's, not aftertouch */
        int at = 0;
        for (k = 0; k < EC_NKEY; k++)
            if ((in->key_held >> k & 1u) && in->key_lean[k] > at)
                at = in->key_lean[k];
        if (!ec_as->at || ec_app_mode() == EC_CAT_XFD)
            at = 0;
        A->mk.at = (float)(at > 100 ? 100 : at) / 100.0f;
        A->mk.mw = ec_as->cc & 1 ? (float)in->cc_mw / 127.0f : 0.0f;
        A->mk.expr = ec_as->cc & 2 ? (float)in->cc_expr / 127.0f : 0.0f;
        ec_mk_tick();
    }
    ec_xfd_tick(in->key_lean);
    holds_tick();
}

/* ------------------------------------------------------------------------------------------- the pop-up --- */
const char *ec_pn_page_name(int mode)
{
    static char b[24];
    ec_page_t pg;
    switch (mode) {
    case EC_CAT_MASTER: {
        const int sel = A->bus ? EC_NTRACK - 1 + A->bus : A->track;
        snprintf(b, sizeof b, "%s  %s", target_name(sel),
                 A->bus ? "" : ec_rate_label(iv(ec_app_params(A), EC_P_RATE)));
        return b;
    }
    case EC_MODE_PAGES: snprintf(b, sizeof b, "PAGE %d", A->step_page + 1); return b;
    case EC_MODE_KEYBOARD:
        if (ec_kb_slice_track())
            snprintf(b, sizeof b, "SLICE %d", iv(ec_app_params(A), EC_P_SLICE_INDEX));
        else
            snprintf(b, sizeof b, "OCT %+d", P->oct);
        return b;
    case EC_CAT_XFD: {
        static const char *const G[4] = {"INSTANT", "FAST", "MED", "SLOW"};
        snprintf(b, sizeof b, "GLIDE %s", G[clampi(ec_as->glide, 0, 3)]);
        return b;
    }
    case EC_CAT_PAT: snprintf(b, sizeof b, "BANK %c", 'A' + P->bank); return b;
    case EC_MODE_SAMPLER:
        if (ec_sampler_len())
            snprintf(b, sizeof b, "%d BAR%s", ec_sampler_len(), ec_sampler_len() > 1 ? "S" : "");
        else
            snprintf(b, sizeof b, "FREE");
        return b;
    case EC_CAT_FILE:
        if (ec_file_page() == EC_PIC_EDITOR) {
            snprintf(b, sizeof b, "EDITOR %dX", (int)ec_file_zoom());
            return b;
        }
        break;
    default: break;
    }
    if (mode < 0 || mode >= EC_NCAT)
        return "";
    ec_page_items_bus(mode, A->page[mode], ec_app_params(A), A->bus, A->bus ? A->proj.bus[A->bus - 1] : 0, &pg);
    snprintf(b, sizeof b, "%s", pg.title ? pg.title : "");
    return b;
}

const char *ec_pn_roles(int mode, int *cur)
{
    static char r[EC_NBKEY + 1];
    int i;
    *cur = -1;
    memset(r, 'O', EC_NBKEY);
    r[EC_NBKEY] = 0;
    switch (mode) {
    case EC_CAT_MASTER:
        memset(r, 'A', 10);
        r[10] = 'X';
        if (!A->bus)
            for (i = 0; i < 8; i++)
                if (ec_mix_rate_index(i + 1) == iv(ec_app_params(A), EC_P_RATE))
                    *cur = i;
        break;
    case EC_MODE_PAGES:
        for (i = 0; i < EC_NBKEY && i < ec_pages_count(); i++)
            r[i] = 'P';
        if (A->step_page < EC_NBKEY)
            *cur = A->step_page;
        break;
    case EC_MODE_KEYBOARD:
        memset(r, 'A', EC_NBKEY);
        if (ec_kb_slice_track()) {
            const int s = iv(ec_app_params(A), EC_P_SLICE_INDEX) - 1 - P->kb_bank * 10;
            r[10] = 'X';
            if (s >= 0 && s < 10)
                *cur = s;
        }
        break;
    case EC_CAT_XFD:
        r[0] = r[1] = r[10] = 'X';
        break;
    case EC_CAT_PAT:
        for (i = 0; i < EC_NBANK && i < EC_NBKEY; i++)
            r[i] = 'P';
        *cur = P->bank;
        break;
    case EC_CAT_FILE: {
        const int pic = ec_file_page();
        if (pic == EC_PIC_EDITOR) {                /* B1 / B11 scroll, B2..B10 the zoom */
            memset(r, 'P', EC_NBKEY);
            r[0] = r[10] = 'A';
            *cur = 1 + A->pn.ed_zoom;
            break;
        }
        for (i = 1; i <= EC_NBKEY; i++)
            if (ec_file_key_page(i) >= 0)
                r[i - 1] = 'P';
        if (pic == EC_PIC_FILE) {                  /* B1 up, B2 in, B10 LOAD, B11 preview */
            r[0] = r[1] = r[9] = 'A';
            r[10] = 'X';
        } else if (ec_cat_page_key(EC_CAT_FILE, A->page[EC_CAT_FILE]) > 0)
            *cur = ec_cat_page_key(EC_CAT_FILE, A->page[EC_CAT_FILE]) - 1;
    } break;
    case EC_MODE_SAMPLER:                          /* B1..B5 the length, B11 arm / stop */
        memset(r, 'A', 5);
        r[10] = 'X';
        {
            static const int K[6] = {0, 1, 2, 3, 4, 4};
            *cur = K[A->rec.len < 0 ? 0 : A->rec.len > 5 ? 5 : A->rec.len];
        }
        break;
    default:
        if (ec_src_mode(mode)) {
            for (i = 0; i < EC_NBKEY; i++)
                r[i] = ec_src_role(mode, i + 1);
            if (mode < EC_NCAT) {
                const int key = mode == EC_CAT_FX && A->bus ? ec_bus_key(A->bus) : ec_cat_page_key(mode, A->page[mode]);
                if (key > 0)
                    *cur = key - 1;
            }
        }
        break;
    }
    return r;
}
