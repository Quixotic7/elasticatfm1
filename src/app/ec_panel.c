/* SPDX-License-Identifier: GPL-3.0-only */
/* 0.6.0: the panel by modes (ec_mode.h, docs/UX-V2.md §0, §1, §3): the input dispatch, the mode model (a tap on a mode
 * button, the lit one again, a hold or SHIFT = the settings, SHIFT + HOME = APP SETTINGS; 0.6.1: no pop-up), the
 * intro, SHIFT (held / sticky) and its layer, F1 / F2 (tap sticky, hold momentary; both = F3), PLAY, REC, the white
 * keys as steps, PRESETS = the track's pool slot, ALGO = the step page. The modes' own keys are in ec_srckeys.c,
 * ec_mix.c, ec_keyboard.c, ec_xfd.c, ec_patlaunch.c, ec_file.c, ec_sampler.c; live REC in ec_liverec.c.
 * 0.6.11: FILE opens while playing (BROWSE, POOL, the SAMPLER); the EDITOR, RAZOR, free, move and purge need a stop
 * (STOP FIRST). Inside FILE PLAY works (from the EDITOR / RAZOR it goes to BROWSE first); REC does nothing. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ec_app.h"
#include "ec_holdmenu.h"
#include "ec_browse.h"
#include "ec_gfx.h"
#include "ec_razor_ui.h"
#include "ec_snap.h"

#define A (&ec_app)
#define P (&ec_app.pn)
#define UX (&ec_app.ux)
#define E (&ec_app.e5)

#define TAP_MS 250            /* a step key released within this (not edited): a tap toggles the trig */

/* what a key does from its press to its release */
enum { R_NONE, R_LP, R_STEP, R_LOOP_LIVE, R_PAD, R_KB, R_RAZOR, R_F3, R_SWALLOW, R_TRK, R_XFD, R_SRCB, R_PAGEB, R_SHIFTB,
       R_FILEB, R_FILEW, R_FILEPV };

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

static void overlays_close(void);
/* F1 / F2 on (held); both = F3 */
static uint8_t f_pend;                             /* 0.6.79: KEYBOARD: F1 / F2 down, not held long enough yet */
static void f_on(int bit)
{
    P->f_held |= bit;
    if (P->f_held == 3) {                          /* F1 + F2: F3 */
        ec_clip_reset();
        P->f_sticky = 0;
        P->f_used = 3;
        overlays_close();
        UX->f3 = 1;
    }
    A->redraw = 1;
}

/* 0.6.79 (owner): KEYBOARD's F1 / F2 come on only once held */
static void f_gate(void)
{
    int bit;
    for (bit = 1; bit <= 2; bit++)
        if ((f_pend & bit) && A->ms - P->f_ms[bit - 1] >= EC_HOLD_MS) {
            f_pend &= (uint8_t)~bit;
            f_on(bit);
        }
}

int ec_pn_f(int which)
{
    if (ec_app_popup(A))                           /* 0.6.78 (owner): KEYBOARD has F1 / F2 too (held) */
        return 0;
    return ((P->f_held | P->f_sticky) & which) != 0;
}

int ec_app_popup(const ec_app_t *a)
{
    return a->e5.naming || a->e5.confirm || a->ux.f3 || a->settings || a->rec.open || a->pn.as_open ||
           a->pn.rs_open;
}

/* ------------------------------------------------------------------------------------------- init --- */
int ec_pn_picking(void) { return P->btn >= 0 && !P->btn_used && !A->fn && A->ms - P->btn_ms >= EC_HDR_MS; }

void ec_pn_init(void)
{
    int i;
    for (i = 0; i < EC_NMB; i++)
        P->last[i] = -1;
    P->btn = -1;
    P->last_fpage = -1;
    P->xf_key = -1;
    UX->pre_idx = -1;
    A->mk.held = A->mk.hot_id = -1;
    ec_as_load();
    ec_ui_init();                                  /* 0.6.75: where the user was: read again at every power-on */
    ec_gfx_set_theme(ec_as->theme);
    P->pv_handle = -1;                             /* FILE: the browser at /audio, no preview */
    P->ed_hold = P->pool_carry = -1;
    ec_file_hdr_init();
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
    UX->set_row = 0;
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
    overlays_close();
    ec_mk_off();                                   /* a source key's hold ends (no tap) */
    P->src_key = P->slot_key = 0;
    ec_hm_cancel();                                /* 0.6.1: a hold menu goes with its mode */
    E->anchor = 0;
    P->cat = 0;
    if (was == EC_MODE_SAMPLER && mode != EC_MODE_SAMPLER)
        ec_sampler_enter(0);
    if (was == EC_CAT_FILE && mode != EC_CAT_FILE) {
        ec_file_leave();                           /* the preview stops */
        P->last_fpage = (int8_t)A->page[EC_CAT_FILE];   /* 0.6.66 (owner): FILE's button comes back to this page */
    }
    if (mode == EC_CAT_FILE && was != EC_CAT_FILE) {
        int p;
        ec_liverec_stop();                         /* (no live REC inside FILE: a CAPTURE waiting ends) */
        for (p = 0; p < ec_cat_pages(EC_CAT_FILE, 0); p++)   /* 0.6.11 (QA finding 2): it opens on BROWSE, always */
            if (ec_page_pic(EC_CAT_FILE, p, 0) == EC_PIC_FILE) {
                ec_app_set_page(EC_CAT_FILE, p);
                break;
            }
    }
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
    if (mode == EC_MODE_PROJECTS && was != EC_MODE_PROJECTS)
        ec_prj_enter();                            /* 0.6.42: the list read again, the cursor on this project */
    if (mode != was && A->rec.open)                /* (the recorder's setup belongs to the mode it opened in) */
        ec_rec_close();
    if (mode != was) {
        P->shift_sticky = 0;                       /* sticky SHIFT ends with the mode */
        fn_update();
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
    if (ec_as->mode_lock) {                        /* 0.6.15: MODE LOCK: a click opens the button's mode, never cycles */
        if (ec_mode_button(cur) == mb)
            return;
        if (mb == EC_MB_FILE && P->last[mb] == EC_CAT_FILE && P->last_fpage >= 0) {
            const int pic = ec_page_pic(EC_CAT_FILE, P->last_fpage, 0);
            if ((pic == EC_PIC_EDITOR || pic == EC_PIC_RAZOR) && (A->seq.playing || ec_seq_counting_in(&A->seq))) {
                ec_pn_enter(EC_CAT_FILE);          /* 0.6.16: the editors need a stop: BROWSE instead */
                ec_app_message("STOP FIRST");
                return;
            }
            ec_pn_goto(EC_CAT_FILE, P->last_fpage);
            return;
        }
        ec_pn_enter(P->last[mb] >= 0 ? P->last[mb] : modes[0]);
        return;
    }
    if (mb == EC_MB_FILE && (cur == EC_CAT_FILE || cur == EC_MODE_PROJECTS) && !A->bus) {   /* 0.6.15 (owner): FILE's
                                                    * sub-modes on its button: BROWSE / POOL -> 0.6.42 PROJECTS -> the
                                                    * EDITOR -> RAZOR -> the SAMPLER -> BROWSE; playing, the editors are
                                                    * skipped (they need a stop) */
        const int pic = cur == EC_MODE_PROJECTS ? -1 : ec_file_page();
        const int playing = A->seq.playing || ec_seq_counting_in(&A->seq);
        int want = pic == EC_PIC_EDITOR ? EC_PIC_RAZOR : pic == EC_PIC_RAZOR ? -1 : EC_PIC_EDITOR, p;
        if (cur == EC_CAT_FILE && (pic == EC_PIC_FILE || pic == EC_PIC_POOL)) {
            ec_pn_enter(EC_MODE_PROJECTS);
            return;
        }
        if (playing)
            want = -1;
        if (cur == EC_MODE_PROJECTS && want >= 0)
            ec_pn_enter(EC_CAT_FILE);              /* (into FILE first: the editor is its page) */
        if (want >= 0) {
            for (p = 0; p < ec_cat_pages(EC_CAT_FILE, 0); p++)
                if (ec_page_pic(EC_CAT_FILE, p, 0) == want) {
                    ec_app_set_page(EC_CAT_FILE, p);
                    P->pool_carry = -1;
                    A->redraw = 1;
                    return;
                }
        }
        ec_pn_enter(EC_MODE_SAMPLER);
        return;
    }
    if (ec_mode_button(cur) == mb) {
        if (n > 1) {
            for (i = 0; i < n && modes[i] != cur; i++)
                ;
            ec_pn_enter(modes[(i + 1) % n]);
        } else if (cur < EC_NCAT)
            ec_nav_next_group(cur);                /* 0.6.2: the next group of the page list (wraps) */
        A->redraw = 1;
        return;
    }
    {
        /* 0.6.66 (owner's design rule): every mode button comes back to the mode it was left on, on its page (HOME's
         * KEYBOARD, FILE's EDITOR ..): no menu diving. (0.6.11 opened HOME / FILE's first sub-mode.) */
        const int m = P->last[mb] >= 0 ? P->last[mb] : modes[0];
        if (mb == EC_MB_FILE && m == EC_CAT_FILE && P->last_fpage >= 0) {
            const int pic = ec_page_pic(EC_CAT_FILE, P->last_fpage, 0);
            if ((pic == EC_PIC_EDITOR || pic == EC_PIC_RAZOR) && (A->seq.playing || ec_seq_counting_in(&A->seq))) {
                ec_pn_enter(EC_CAT_FILE);          /* the editors need a stop: BROWSE instead */
                ec_app_message("STOP FIRST");
                return;
            }
            ec_pn_goto(EC_CAT_FILE, P->last_fpage);
            return;
        }
        ec_pn_enter(m);
    }
}

/* 0.6.15 (owner): a mode button held: the black keys are its modes (FILE: BROWSE, EDITOR, RAZOR, SAMPLER); the one
 * pressed opens, and becomes the button's own (MODE LOCK: what a click opens). Letting go without a key: nothing */
int ec_pn_pick_list(int b, int *modes, int *pages, int max)
{
    const int mb = mb_of(b);
    int n, i;
    if (mb == EC_MB_FILE) {
        static const int PICS[3] = {EC_PIC_FILE, EC_PIC_EDITOR, EC_PIC_RAZOR};
        int p, k;
        n = 0;
        for (k = 0; k < 3 && n < max; k++)
            for (p = 0; p < ec_cat_pages(EC_CAT_FILE, 0); p++)
                if (ec_page_pic(EC_CAT_FILE, p, 0) == PICS[k]) {
                    modes[n] = EC_CAT_FILE;
                    pages[n++] = p;
                    break;
                }
        if (n < max) {
            modes[n] = EC_MODE_SAMPLER;
            pages[n++] = -1;
        }
        if (n > 1 && n < max) {                    /* 0.6.42 (owner): PROJECTS second: BROWSE, PROJECTS, EDITOR .. */
            memmove(modes + 2, modes + 1, sizeof modes[0] * (size_t)(n - 1));
            memmove(pages + 2, pages + 1, sizeof pages[0] * (size_t)(n - 1));
            modes[1] = EC_MODE_PROJECTS;
            pages[1] = -1;
            n++;
        }
        return n;
    }
    n = ec_button_modes(mb, modes, max);
    for (i = 0; i < n; i++)
        pages[i] = -1;
    return n;
}

void ec_pn_pick(int b, int key)
{
    int modes[8], pages[8];
    const int n = ec_pn_pick_list(b, modes, pages, 8), mb = mb_of(b);
    if (key < 1 || key > n)
        return;
    if (pages[key - 1] >= 0) {
        if (modes[key - 1] == EC_CAT_FILE && ec_page_pic(EC_CAT_FILE, pages[key - 1], 0) != EC_PIC_FILE &&
            (A->seq.playing || ec_seq_counting_in(&A->seq))) {
            ec_app_alert("STOP FIRST");            /* (the editors need a stop) */
            return;
        }
        ec_pn_goto(modes[key - 1], pages[key - 1]);
    } else
        ec_pn_enter(modes[key - 1]);
    P->last[mb] = modes[key - 1];
    P->last_fpage = (int8_t)pages[key - 1];
}

/* SHIFT + the button: that mode's settings (SEQ: F3; HOME: LOOP ENGINE's, with KEYBOARD's ADVANCE) */
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
        if (i != A->track)
            ec_lp_track(A->track, i);              /* 0.6.11: the step selection and target are the track's */
        A->track = i;
        A->bus = 0;
    } else
        A->bus = i - EC_NTRACK + 1;
    if (ec_app_mode() == EC_CAT_MASTER && !A->settings && ec_cat_pages(EC_CAT_MASTER, 0) > 1) {
        /* 0.6.14 (owner): the page stays; only MST has the MASTER page: onto it, there; off it, back */
        if (A->bus == 3)
            ec_app_set_page(EC_CAT_MASTER, 1);
        else if (A->page[EC_CAT_MASTER] == 1)
            ec_app_set_page(EC_CAT_MASTER, 0);
    }
    A->redraw = 1;
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

/* SHIFT + B1 TAP TEMPO, B2 LATCH, B4 RECALL, B5 MEMORIZE (0.6.6, owner: was B3 / B4), B11 FILL (held); the rest dark */
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
    case 4: ec_e5_recall_memo(); break;
    case 5: ec_e5_memorize(); break;
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
    if (A->e5.naming) {                            /* 0.6.47 (owner, FIZZ's keybed): OCT+ / FILE OK, OCT- delete (SHIFT:
                                                    * clear; an empty name: no), HOME cancel */
        if (b == EC_B_OCTUP || b == EC_B_SAVE)
            ec_e5_name_close(1);
        else if (b == EC_B_OCTDN) {
            if (A->fn)
                UX->shift_used = 1;
            ec_e5_name_back(A->fn);
        } else if (b == EC_B_HOME)
            ec_e5_name_close(0);
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
        const int fpic = !A->fn && ec_app_mode() == EC_CAT_FILE && !A->settings ? ec_file_page() : -1;
        if (!A->fn && ec_app_mode() == EC_MODE_PROJECTS) {   /* 0.6.42: OCT+ yes / LOAD, OCT- no / back */
            ec_prj_oct(b == EC_B_OCTUP);
        } else if (fpic == EC_PIC_FILE) {          /* 0.6.42 (owner): OCT+ yes / open, OCT- no / up (FREE: B9) */
            if (b == EC_B_OCTUP)
                ec_file_open();
            else
                ec_file_up();
        } else if (fpic == EC_PIC_EDITOR) {        /* 0.6.1: the EDITOR: F1 START, F2 END (what a white key places) */
            ec_file_ed_point(b == EC_B_OCTUP);
        } else if (fpic == EC_PIC_POOL) {          /* 0.6.1: POOL: F2 FREE; 0.6.15: the move is B11 held */
            if (b == EC_B_OCTUP)
                ec_file_pool_f2();
            else
                ec_app_message("HOLD B11 TO MOVE");
        } else if (b == EC_B_OCTDN && !A->fn && ec_lp_b11_held()) {   /* 0.6.9: LOOP ENGINE: B11 held + F1 clears */
            ec_lp_clear();                         /* the selected steps */
        } else if (b == EC_B_OCTUP && !A->fn && ec_lp_b11_held()) {   /* 0.6.14: B11 held + F2 = LIVE */
            ec_lp_live();
        } else if (b == EC_B_OCTDN && !A->fn && A->step_held && ec_app_clear_held_lock()) {
            ;                                      /* 0.6.51 (owner): a step held + F1: the last knob's lock */
        } else if (ec_rzui_active(A) && !A->fn) {  /* E3, page-local: F2 split / F1 delete (0.6.55 (owner, QA B1): SHIFT
                                                    * is undo / redo here too; EVEN / AUTO-CHOP are B9 / B10) */
            ec_rzui_oct(A, b == EC_B_OCTUP);
        } else if (A->fn) {                        /* SHIFT + F1 / F2: undo / redo */
            ec_e5_undo(b == EC_B_OCTUP);
        } else if (ec_app_mode() == EC_MODE_KEYBOARD) {   /* 0.6.79 (owner): KEYBOARD: nothing at the press; held
                                                    * EC_HOLD_MS F1 / F2 come on (f_gate); let go before: the octave */
            f_pend |= (uint8_t)bit;
            P->f_used &= ~bit;
            P->f_ms[bit - 1] = A->ms;
        } else {
            P->f_used &= ~bit;
            P->f_ms[bit - 1] = A->ms;
            f_on(bit);
        }
        A->redraw = 1;
        return;
    }
    switch (b) {
    case EC_B_PLAY:                                /* start / stop at once, on press */
        if (ec_app_mode() == EC_CAT_FILE && !A->seq.playing && ec_file_play_leave())
            break;                                 /* 0.6.11: the EDITOR / RAZOR can't play: it goes to BROWSE first */
        if (A->seq.playing) {
            ec_liverec_stop();                     /* (0.6.59: first: a CAPTURE take's length needs the clock) */
            ec_seq_stop(&A->seq);
        } else
            ec_seq_start(&A->seq);
        if (A->dirty)
            A->dirty_ms = A->ms;
        break;
    case EC_B_REC:
        if (ec_app_mode() == EC_CAT_FILE || ec_app_mode() == EC_MODE_PROJECTS)   /* (FILE: nothing; 0.6.56, QA C14:
                                                    * PROJECTS too) */
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
        if (A->fn) {                               /* SHIFT + a mode button: its settings (0.6.15: the only way) */
            if (b == EC_B_ARP) {                   /* 0.6.15: SHIFT + ARP = APP SETTINGS (SHIFT + HOME is LOOP SETUP) */
                overlays_close();
                ec_as_open();
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
        if (f_pend & bit) {                        /* 0.6.79: KEYBOARD, let go before the hold: the octave */
            f_pend &= (uint8_t)~bit;
            ec_kb_oct(bit == 2 ? 1 : -1);
            A->redraw = 1;
        } else if (P->f_held & bit) {
            P->f_held &= ~bit;
            if (!(P->f_used & bit) && A->ms - P->f_ms[bit - 1] < EC_HOLD_MS && ec_app_mode() != EC_MODE_KEYBOARD)
                P->f_sticky = P->f_sticky & bit ? 0 : bit;   /* a tap: sticky (F1 and F2 one at a time) */
            P->f_used &= ~bit;
            if (!(P->f_held | P->f_sticky))
                ec_clip_reset();                   /* the buffer empties when F1 / F2 turns off */
        }
    } break;
    case EC_B_REC:
        if (P->rec_down && !P->rec_used && ec_app_mode() != EC_MODE_SAMPLER)   /* 0.6.56 (QA C14): the SAMPLER's
                                                    * REC is its recorder's (held: the setup), never live REC */
            ec_liverec_toggle();
        P->rec_down = 0;
        break;
    default:
        if (P->btn == b) {
            if (!P->btn_used && A->ms - P->btn_ms < EC_HDR_MS)   /* 0.6.16: a hold (the picker) let go with no
                                                                  * key changes nothing */
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
    if (ec_lp_on_range()) {                        /* 0.6.21 / 0.6.23: the RANGE page: the keys set the range on every
                                                    * machine (no loop scrub, no pads) */
        A->key_role[k] = R_LOOP_LIVE;
        return;
    }
    if (is_slice_m(iv(v, EC_P_MACHINE))) {
        ec_seq_slice_pad(&A->seq, A->track, lk, 1, key_vel(vel));
        A->key_role[k] = R_PAD;
    } else {
        ec_seq_loop_key(&A->seq, A->track, lk, 1);
        A->key_role[k] = R_LOOP_LIVE;
    }
    ec_liverec_loop(lk, is_slice_m(iv(v, EC_P_MACHINE)), vel);   /* (recording: a trig + the loop position) */
    ec_liverec_key(k, 1);                          /* 0.6.59: its held length on release */
}

static void white_down(int k, int i, int vel, int lean)
{
    const int mode = ec_app_mode();
    if (P->btn >= 0 && !A->fn) {                   /* 0.6.15 (owner): a mode button held: the keybed is the picker's,
                                                    * nothing reaches the mode; the release is no click either */
        P->btn_used = 1;
        A->key_role[k] = R_SWALLOW;
        return;
    }
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
        if (mode == EC_CAT_FILE && ec_rzui_active(A)) {   /* RAZOR: SHIFT + a slice key mutes the slice */
            ec_rzui_key(A, i, 1, vel);
            A->key_role[k] = R_RAZOR;
            return;
        }
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
    if (!A->settings && mode == EC_CAT_SRC && !A->bus && (ec_pn_f(1) || ec_pn_f(2))) {   /* 0.6.63 (owner): LOOP
                                                    * ENGINE's keys are the loop keys, so F1 / F2 on (copy / cut) never
                                                    * reached the steps: with F1 / F2 on, the keys are the steps */
        P->f_used = 3;
        ec_clip_tap(EC_CK_STEP, A->step_page * 16 + i);
        A->key_role[k] = R_SWALLOW;
        return;
    }
    if (!A->settings && ec_lp_sel_mode() && ec_lp_white(k, i, 1, vel)) {   /* 0.6.9 / 0.6.14: B11's STEP layer selects
                                                    * steps in LOOP ENGINE, TRIG and KEYBOARD */
        A->key_role[k] = R_LP;
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
        case EC_CAT_SRC:
            loop_down(k, i, vel);                  /* the LOOP layer: live, always */
            if (A->key_role[k] == R_LOOP_LIVE || A->key_role[k] == R_PAD)
                ec_lp_loop_note(i, 1);             /* (and written into the selected steps) */
            return;
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
        case EC_MODE_PROJECTS: A->key_role[k] = R_SWALLOW; return;  /* 0.6.43 (owner): the white keys do nothing */
        case EC_CAT_FILE:
            if (ec_file_page() == EC_PIC_EDITOR) { /* the editor: the trim */
                ec_file_white(i, 1);
                A->key_role[k] = R_FILEW;
                return;
            }
            if (ec_file_page() == EC_PIC_FILE || ec_file_page() == EC_PIC_POOL) {   /* 0.6.15: the preview's loop keys */
                ec_file_pv_key(i, 1);
                A->key_role[k] = R_FILEPV;
                return;
            }
            /* fall through: RAZOR's keys are its slices */
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
    case R_LP: ec_lp_white(k, i, 0, vel); break;
    case R_STEP: step_up(i, vel); break;
    case R_LOOP_LIVE:
        if (A->seq.tr[A->track].live_keys >> (ec_e5_loop_key(k, i, 0) - 1) & 1u)   /* (not a RANGE key) */
            ec_seq_loop_key(&A->seq, A->track, ec_e5_loop_key(k, i, 0), 0);
        ec_lp_loop_note(i, 0);
        ec_liverec_key(k, 0);
        break;
    case R_PAD:
        ec_seq_slice_pad(&A->seq, A->track, ec_e5_loop_key(k, i, 0), 0, 0);
        ec_lp_loop_note(i, 0);
        ec_liverec_key(k, 0);
        break;
    case R_KB: ec_kb_key(k, 0, 0); break;
    case R_RAZOR: ec_rzui_key(A, i, 0, 0); break;
    case R_FILEW: ec_file_white(i, 0); break;
    case R_FILEPV: ec_file_pv_key(i, 0); break;
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
    if (P->btn >= 0 && !A->fn) {                   /* 0.6.15: a mode button held: the black keys pick its mode */
        P->btn_used = 1;
        ec_pn_pick(P->btn, key);
        return;
    }
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
    if (mode == EC_CAT_TRIG && A->step_held && !A->bus) {   /* 0.6.14 (owner): a step held: the black keys are the
                                                    * note, B6 (F#) = 0, B1 -5 .. B11 +5 */
        char b[16];
        const int st = key - 6;
        ec_app_lock_held_set(EC_P_NOTE, (float)st);   /* 0.6.80: the NOTE */
        ec_param_format(EC_P_NOTE, (float)st, 3, b + 5, sizeof b - 5);
        memcpy(b, "NOTE ", 5);
        ec_app_message(b);
        A->redraw = 1;
        return;
    }
    if (ec_lp_b11_held() && key <= 10 && !A->bus) {   /* 0.6.13 / 0.6.14: B11 held: B1..B10 the pages (the step modes) */
        ec_lp_b11_used();
        ec_pages_black_down(key);
        A->key_role[k] = R_PAGEB;
        return;
    }
    switch (mode) {
    case EC_CAT_MASTER: ec_mix_black(key); break;
    case EC_MODE_PAGES:
        ec_pages_black_down(key);
        A->key_role[k] = R_PAGEB;
        break;
    case EC_MODE_KEYBOARD:
        if (key == 11) {                           /* 0.6.14: B11 = the layers / pages (a quick click: the bank) */
            ec_lp_b11(1);
            A->key_role[k] = R_SRCB;
            break;
        }
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
    case EC_MODE_PROJECTS:                         /* 0.6.42: NEW, RENAME, RANDOM .. SAVE +1, SAVE, LOAD */
        ec_prj_black(key);
        A->key_role[k] = R_SWALLOW;
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

static void step_page_turn(int d)
{
    const int pages = (ec_seq_pattern_steps(&A->seq, A->track) + 15) / 16;
    const int p = clampi(A->step_page + d, 0, pages - 1);
    char b[16];
    if (p != A->step_page) {
        A->step_page = p;
        A->step_held = 0;
        snprintf(b, sizeof b, "PAGE %d/%d", p + 1, pages);
        ec_app_message(b);
    }
    A->redraw = 1;
}

static void presets(int d)
{
    float *v = ec_app_params(A);
    int s, used = 0, pos = 0, i, steps = d < 0 ? -d : d;
    char lb[16], sub[16];
    const ec_ss_sample_t *x;
    const int mode = ec_app_mode();
    if (mode == EC_CAT_MASTER && !A->settings) {   /* 0.6.14 (owner): MIX: PRESETS selects the track, no pop-up (the
                                                    * header's target box says it); 0.6.54 (QA C2): before the bus
                                                    * return, so it walks on from S1 / S2 too */
        const int cur = A->bus ? EC_NTRACK - 1 + A->bus : A->track;
        ec_pn_select_target(cur + (d > 0 ? 1 : -1));
        return;
    }
    if (A->bus || ((mode == EC_CAT_XFD || mode == EC_CAT_PAT) && !A->settings))
        return;                                    /* 0.6.56 (QA C17): XFADE / PATTERN: never the track's sample */
    if ((ec_lp_active() || mode == EC_MODE_PAGES) && !A->step_held) {   /* 0.6.14: every track page, PAGES */
        step_page_turn(d);                         /* 0.6.14 (owner): PRESETS turns the step page (SLOT is a param) */
        return;
    }
    if (mode == EC_CAT_FILE && !A->settings && (ec_file_page() == EC_PIC_FILE || ec_file_page() == EC_PIC_POOL)) {
        ec_file_preview_toggle();                  /* 0.6.15 (owner): BROWSE / POOL: PRESETS = the preview on / off;
                                                    * the editors: the track's slot (below, the pool list pop-up) */
        return;
    }
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
    if (A->settings) {
        ec_page_t pg;
        ec_app_page(A, &pg);
        ec_app_list_knob(EC_E_SELECT, d, &UX->set_row, ec_app_set_rows(&pg, 0));
        return;
    }
    switch (mode) {
    case EC_MODE_KEYBOARD: ec_kb_page(d > 0 ? 1 : -1, 0); return;   /* 0.6.62 (owner): its knob pages */
    case EC_MODE_PAGES:
    case EC_MODE_SAMPLER:
    case EC_CAT_XFD: return;
    case EC_CAT_PAT: ec_nav_sel(d > 0 ? 1 : -1); return;   /* the bank (B1..B4 its keys) */
    default: break;
    }
    /* 0.6.2: SEL walks the mode's page list (ec_nav.c), both ways, no wrap; the lists keep SEL for their rows */
    if (mode == EC_CAT_FILE && ec_file_page() == EC_PIC_FILE) {
        ec_file_select(d);                         /* the browser: the cursor (folders, files) */
    } else if (mode == EC_CAT_FILE && ec_file_page() == EC_PIC_POOL) {
        ec_file_pool_select(d);                    /* 0.6.1: POOL: the slot */
    } else if (mode == EC_CAT_FILE) {
        ec_nav_sel(d > 0 ? 1 : -1);                /* 0.6.1: EDITOR, RAZOR: one page a turn */
    } else if (ec_mk_route_select(d)) {
        ;                                          /* 0.6.51 (owner): an LFO's page: its routes, then the pages */
    } else if (!A->bus && ec_page_pic(A->cat, A->page[A->cat], 0) == EC_PIC_MATRIX) {
        const int row = A->mk.row;
        if (!ec_mk_select(d) && !ec_nav_sel(d > 0 ? 1 : -1))   /* 0.5.0: the rows, then on to the next page */
            A->mk.row = row;                       /* (the list's end: the row stays) */
    } else
        ec_nav_sel(d);
}

/* 0.6.49 (owner): F1 held + SEL turned on a page with params: the dice shake, harder as it turns; DICE_THROW detents
 * and they are thrown; as they land the page's params get random values (one undo). F1 let go before: nothing */
#define DICE_THROW 10
#define DICE_LAND_MS 450u
#define DICE_GONE_MS 1000u
static struct {
    int acc, applied, n, f0, f1;
    uint32_t throw_ms;
    uint32_t rng;
} g_dice;

static void dice_turn(int d)
{
    P->f_used |= 1;                                /* (F1 used: no sticky toggle on release) */
    if (g_dice.throw_ms)
        return;
    g_dice.acc += d < 0 ? -d : d;
    g_dice.rng ^= A->ms * 2654435761u + 0x2545f491u;
    if (g_dice.acc >= DICE_THROW) {
        g_dice.throw_ms = A->ms ? A->ms : 1;
        g_dice.applied = 0;
        g_dice.f0 = 1 + (int)(g_dice.rng % 6u);
        g_dice.f1 = 1 + (int)((g_dice.rng >> 8) % 6u);
    }
    A->redraw = 1;
}

static void dice_tick(void)
{
    if (g_dice.throw_ms) {
        const uint32_t t = A->ms - g_dice.throw_ms;
        if (!g_dice.applied && t >= DICE_LAND_MS) {
            char m[20];
            g_dice.applied = 1;
            g_dice.n = ec_app_randomize_page(&g_dice.rng);
            snprintf(m, sizeof m, "RANDOM %d", g_dice.n);
            ec_app_message(g_dice.n ? m : "NOTHING TO ROLL");
        }
        if (t >= DICE_GONE_MS) {
            g_dice.throw_ms = 0;
            g_dice.acc = 0;
        }
        A->redraw = 1;
    } else if (g_dice.acc) {
        if (!(P->f_held & 1)) {
            g_dice.acc = 0;                        /* F1 let go before the throw: nothing */
            A->redraw = 1;
        } else
            A->redraw = 1;                         /* (the shaking) */
    }
}

/* the dice for the screen: 0 none, 1 shaking (*level 1..DICE_THROW), 2 thrown (*t ms since) */
int ec_pn_dice(int *level, uint32_t *t, int *f0, int *f1)
{
    *f0 = g_dice.f0, *f1 = g_dice.f1;
    if (g_dice.throw_ms) {
        *t = A->ms - g_dice.throw_ms;
        *level = DICE_THROW;
        return 2;
    }
    *level = g_dice.acc;
    *t = 0;
    return g_dice.acc > 0;
}

static void knob(int r, int d)
{
    const int mode = ec_app_mode();
    if (ec_e5_knob(r, d))                          /* E5: naming */
        return;
    if (r == EC_E_SELECT && (P->f_held & 1) && !A->fn && ec_app_page_randomizable()) {
        dice_turn(d);                              /* 0.6.49: F1 + SEL: the dice */
        return;
    }
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
            ec_app_edit_global(id, d, ec_ux_accel(r, id == EC_G_BPM ? EC_BPM_ACCEL : 1, d));
        } else if (r == EC_E_PRESETS)
            ec_e5_name_pattern();
        else
            ec_app_knob_cell(r - EC_E_K1, d);
        return;
    }
    if (A->fn) {                                   /* SHIFT: SELECT BPM, ALGO swing, PRESETS the track, K snap */
        UX->shift_used = 1;
        if (r == EC_E_SELECT || r == EC_E_ALGO) { /* 0.6.6: no snap here (SHIFT is how you get to them): BPM in
                                                    * 0.1 steps, accelerated */
            const int id = r == EC_E_SELECT ? EC_G_BPM : EC_G_SWING;
            ec_app_edit_global_f(id, d, ec_ux_accel(r, id == EC_G_BPM ? EC_BPM_ACCEL : 1, d), 0);
            return;
        }
        if (r == EC_E_PRESETS) {
            const int cur = A->bus ? EC_NTRACK - 1 + A->bus : A->track;
            ec_pn_select_target(cur + (d > 0 ? 1 : -1));   /* 0.6.34 (owner): no TRACK takeover (it showed after
                                                                * SHIFT closed); the track keys and header show it */
            return;
        }
    }
    if (ec_src_knob(r, d))                         /* a slot key / quick setting held */
        return;
    if (r == EC_E_K1 && ec_file_pool_carrying()) { /* 0.6.15: POOL, B11 held: K1 moves the slot in hand (as SEL) */
        ec_file_pool_select(d);
        return;
    }
    if (ec_app_mode() == EC_MODE_PROJECTS && !A->settings) {   /* 0.6.42: SEL / ALGO the list; nothing else */
        if (r == EC_E_SELECT || r == EC_E_ALGO)
            ec_prj_select(d);
        return;
    }
    switch (r) {
    case EC_E_SELECT: select_knob(d); break;
    case EC_E_ALGO:
        if (ec_rzui_active(A))
            ec_rzui_select(A, d);                  /* 0.6.1: RAZOR: ALGO picks the slice (SEL turns the page) */
        else if (ec_fx_on_sends())
            ec_fx_send_mode(d);                    /* 0.6.3: FX SENDS: ALGO = the send mode PRE / POST */
        else if (ec_lp_sel_mode())
            ec_lp_algo(d); 
        else if (ec_mk_route_depth(d))
            ;                                      /* 0.6.51 (owner): an LFO's page: the shown route's depth */                        /* 0.6.9 / 0.6.14: the step modes: the page's step, below 1 LIVE */
        else if (mode == EC_CAT_FILE && ec_file_page() == EC_PIC_FILE)
            ec_file_algo(d);                       /* 0.6.15: the browser: CW adds to the pool, CCW removes */
        else if (mode == EC_CAT_FILE && ec_file_page() == EC_PIC_EDITOR)   /* 0.6.15: the EDITOR: the selected marker */
            ec_app_edit_slot(P->ed_point ? EC_S_TRIM_END : EC_S_TRIM_START, d, ec_ux_accel(r, 200, d));
        else if (mode == EC_CAT_FILE || ec_lp_active() || mode == EC_CAT_MASTER || mode == EC_CAT_XFD ||
                 mode == EC_CAT_PAT)                /* 0.6.56 (QA C17): MIX, XFADE, PATTERN: no hidden step page */
            ;                                      /* (the other FILE pages; 0.6.52 (owner): the track pages, where
                                                    * PRESETS turns the step page: nothing) */
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
    dice_tick();
    if (P->rec_down && !P->rec_used && A->ms - P->rec_ms >= EC_HOLD_MS) {
        P->rec_used = 1;
        if (ec_app_mode() == EC_MODE_SAMPLER)
            ec_file_settings();                    /* 0.6.15: REC held in the SAMPLER: the recorder's setup */
        else
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
    if (!P->hdr_late && ec_hdr_holding()) {        /* 0.6.1: a held key's text in the header */
        P->hdr_late = 1;
        A->redraw = 1;
    }
}

/* any knob, key or button: the intro goes (what was pressed still acts) */
static void dismiss(void)
{
    if (P->intro) {
        P->intro = 0;
        A->redraw = 1;
    }
}

/* ------------------------------------------------------------------------------------------- the tick --- */
void ec_pn_tick(const void *vin)
{
    const ec_input_t *in = (const ec_input_t *)vin;
    int b, k, r;
    f_gate();                                      /* 0.6.79: KEYBOARD's F1 / F2 held */
    if (in->btn_down || in->key_down) {
        ec_ui_touched();
        dismiss();
        P->down_ms = A->ms ? A->ms : 1;            /* 0.6.1: an older message gives way to a held key's text */
        P->hdr_late = 0;
    }
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
            if (P->src_key || P->slot_key)         /* (a key during a hold: the hold was used; 0.6.2: not the key
                                                      itself, so a quick-setting key's tap still steps its group) */
                P->bk_used |= (uint16_t)(P->bk_held & ~(up >= 0 && lo < 0 ? 1u << up : 0u));
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

/* ------------------------------------------------------------------------------------------- the roles --- */
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
        if (pic == EC_PIC_FILE) {                  /* B1 up, B2 in, B9 REMOVE, B10 LOAD, B11 preview */
            r[0] = r[1] = r[8] = r[9] = 'A';
            r[10] = 'X';
        } else if (pic == EC_PIC_POOL) {           /* 0.6.1: B10 ASSIGN, B11 preview; the page's key */
            r[9] = 'A';
            r[10] = 'X';
            *cur = ec_cat_page_key(EC_CAT_FILE, A->page[EC_CAT_FILE]) - 1;
        } else if (ec_cat_page_key(EC_CAT_FILE, A->page[EC_CAT_FILE]) > 0)
            *cur = ec_cat_page_key(EC_CAT_FILE, A->page[EC_CAT_FILE]) - 1;
    } break;
    case EC_MODE_PROJECTS:                         /* 0.6.42: B1-B3, B8-B10 */
        r[0] = r[1] = r[5] = r[8] = r[9] = r[10] = 'A';   /* (0.6.46: B6, B9, B10, B11; 0.6.48: no B3) */
        break;
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
                const int key = mode == EC_CAT_FX ? ec_fx_cur_key() : ec_cat_page_key(mode, A->page[mode]);
                if (key > 0)
                    *cur = key - 1;
            }
        }
        break;
    }
    return r;
}
