/* SPDX-License-Identifier: GPL-3.0-only */
/* 0.6.0: the panel by modes (docs/UX-V2.md). Every button opens a mode (ec_button_modes: the layout's), each mode
 * remembers its page, a tap on the lit button turns the page (HOME / EDIT / FILE: the next sub-mode), a hold (400 ms)
 * or SHIFT + the button opens its settings, SHIFT + HOME the APP SETTINGS. Entering a mode shows its pop-up. The
 * global rules (§1: steps, F1 / F2, SHIFT, PLAY, REC), copy / cut / paste (§2) and every mode's keys (§3) are here:
 *   ec_panel.c      the input dispatch, the mode model, the holds, SHIFT, F1 / F2, the pop-up, the intro
 *   ec_srckeys.c    the source template (B1..B5 pages, B6..B10 sources, B11), FX's slot keys, the quick settings
 *   ec_clip.c       §2: the one-step buffer (steps, PAGES' pages, PATTERN LAUNCH's patterns)
 *   ec_mix.c        MIX (track keys, rates) and the CAT; PAGES
 *   ec_keyboard.c   KEYBOARD
 *   ec_xfd.c        CROSSFADER (scenes, the pressure glide)
 *   ec_patlaunch.c  PATTERN LAUNCH
 *   ec_appset.c     APP SETTINGS (the global record in ec_plat_settings)
 *   ec_panel_leds.c the LEDs and the live labels of every mode
 *   ec_liverec.c    part 2's hooks (live REC, FILE / SAMPLER): stubs
 * The state lives in ec_app_t.pn (and .scr: the fixed screen shown, EC_MODE_*, 0 = the category's). */
#ifndef EC_MODE_H
#define EC_MODE_H
#include <stdint.h>

#include "ec_pages.h"
#include "ec_project.h"

/* items the panel makes itself (beside the layout's EC_IT_*): ec_app.c formats / edits them through ec_pn_item_* */
#define EC_IT_UX 40
enum { EC_UX_TARGET,   /* KEYBOARD: the target step (0 = live) */
       EC_UX_OCT,      /* KEYBOARD: the octave */
       EC_UX_PLOOP,    /* PAGES: the pages in the loop ("1-2") */
       EC_UX_GLIDE,    /* CROSSFADER: the base glide time */
       EC_UX_ALOCKS,   /* CROSSFADER: scene A's values */
       EC_UX_BLOCKS,   /*   .. scene B's */
       EC_UX_ADVANCE,  /* HOME settings: KEYBOARD's ADVANCE */
       EC_UX_PLAYING,  /* PATTERN LAUNCH: the pattern playing */
       EC_UX_NEXT,     /*   .. the one queued */
       EC_NUX };

/* APP SETTINGS (docs/UX-V2.md §3): the global record (ec_plat_settings), magic + version */
#define EC_AS_MAGIC 0x53414345u   /* "ECAS" */
#define EC_AS_VERSION 1
enum { EC_AS_THEME, EC_AS_POPUP, EC_AS_KNOB, EC_AS_PREVIEW, EC_AS_AT, EC_AS_METRO, EC_AS_CC, EC_AS_INTRO,
       EC_AS_NROW };
typedef struct {
    uint32_t magic;
    uint16_t version, size;
    uint8_t theme;     /* ec_gfx theme 0..EC_NTHEME-1 */
    uint8_t popup;     /* the mode pop-up on */
    uint8_t knob;      /* knob speed: 0 slow, 1 medium, 2 fast */
    uint8_t preview;   /* FILE's auto-preview */
    uint8_t at;        /* aftertouch on */
    uint8_t metro;     /* the metronome sound (ec_click.h EC_CLICK_*) */
    uint8_t cc;        /* MIDI CC inputs: bit 0 CC 1, bit 1 CC 11 */
    uint8_t intro;     /* the intro at power-on */
    uint8_t advance;   /* KEYBOARD: step entry advances (SHIFT + key: the other) */
    uint8_t glide;     /* CROSSFADER: the base glide time 0 instant, 1 fast, 2 medium, 3 slow */
    uint8_t pad[2];
} ec_appset_t;

/* the one-step (page, pattern) buffer of §2 */
enum { EC_CLIP_EMPTY, EC_CLIP_FRESH /* the next tap pastes anywhere */, EC_CLIP_SMART /* empty: paste, else take */ };
enum { EC_CK_STEP, EC_CK_PAGE, EC_CK_PATTERN };

typedef struct {
    /* the mode model */
    int last[EC_NMB];          /* each button's last mode (EC_CAT_* / EC_MODE_*), -1 none yet */
    int btn;                   /* the mode button held (EC_B_*), -1 none */
    uint32_t btn_ms;
    int btn_used;
    int set_scr;               /* a settings list opened over a fixed screen: the screen to return to */
    int rec_down, rec_used;    /* REC: tap = live record, hold = its settings */
    uint32_t rec_ms;
    /* the mode-entry pop-up */
    int pop;                   /* the mode shown + 1, 0 none */
    uint32_t pop_ms;
    /* the intro */
    int intro;                 /* 1: playing */
    uint32_t intro_ms;
    int intro_frame;
    /* SHIFT (GLO): held, or sticky after a quick tap */
    int shift_held, shift_sticky;
    /* F1 / F2 (OCT- / OCT+): held (bits 1, 2), sticky after a tap, used since the press */
    int f_held, f_sticky, f_used;
    uint32_t f_ms[2];
    /* §2 */
    int clip_state, clip_kind, clip_cut, clip_from;
    ec_step_t clip_step;
    /* black keys held (bit i = B(i+1)), since when, used (a knob / key acted during the hold) */
    uint16_t bk_held, bk_used;
    uint32_t bk_ms[EC_NBKEY];
    int src_key;               /* the source key held (B6..B10, MODULATION's B2..B5): its index 1..11, 0 none */
    int slot_key;              /* FX's slot key held (B1, B3..B5) / FILTER, AMP's B1: 1..5, 0 none */
    /* MIX: the track keys W1..W9 held (tap select, hold mute) */
    uint16_t trk_down, trk_used;
    uint32_t trk_ms[9];
    int cat;                   /* MIX's CAT shows */
    uint32_t cat_ms;
    /* LOOP ENGINE: the zoom's half (32: 0 first, 1 second) */
    int zoom_half;
    /* KEYBOARD */
    int oct;                   /* -3..3 */
    int target;                /* the target step 1..; 0 = live */
    int kb_bank;               /* the slice bank (10 a bank) */
    int8_t kb_semis[32];       /* the note a key started (its release) */
    /* CROSSFADER */
    int xf_key;                /* the W key held gliding (0..11), -1 none */
    float xf_target;
    uint32_t xf_ms;
    uint8_t xf_lean;           /* its pressure 0..100 */
    int xf_b11;                /* B11 (CAPTURE) held */
    int xf_from;               /* the param mode CROSSFADER was entered from: B1 / B2 held, the knobs are its page */
    /* PATTERN LAUNCH */
    int bank;                  /* 0..3 */
    /* APP SETTINGS */
    int as_open, as_row;
    ec_appset_t as, as_undo;   /* the live values, the values when it opened (F1 = back) */
} ec_pn_t;

/* ---- ec_panel.c ---- */
void ec_pn_init(void);
void ec_pn_tick(const void *in);              /* (const ec_input_t *): every edge, the holds, the glide */
int ec_app_mode(void);                        /* the mode shown: A->scr, else A->cat */
void ec_pn_enter(int mode);                   /* that mode (its pop-up), the settings closed */
void ec_pn_goto(int cat, int page);           /* a page of a category (a source key's tap): the mode + its pop-up */
int ec_pn_f(int which);                       /* F1 (1) / F2 (2) on: held or sticky */
void ec_pn_select_target(int i);              /* 0..5 tracks, 6 S1, 7 S2, 8 MST */
const char *ec_pn_roles(int mode, int *cur);  /* the pop-up's 11 black-key roles, the current page's key - 1 */
const char *ec_pn_page_name(int mode);        /* the pop-up's page line */
int ec_pn_popup_shows(void);                  /* the mode pop-up is up */
/* ---- ec_srckeys.c: the source template ---- */
int ec_src_mode(int mode);                    /* FILTER AMP MODULATION LOOP ENGINE TRIG FX: the template */
char ec_src_role(int mode, int key);          /* 'P' 'S' 'X' 'A' 'O' for B1..B11 (key 1..11) */
const char *ec_src_label(int mode, int key);  /* the black key's live label */
void ec_src_down(int key);                    /* key 1..11 */
void ec_src_up(int key);
int ec_src_knob(int role, int det);           /* a hold's knob (slot engine, quick setting): 1 = used */
int ec_src_white(int i);                      /* a white key while a slot key is held: 1 = used */
int ec_src_hold_tick(void);
int ec_src_mk_index(int key);                 /* the ec_mk source (EC_MK_*) a black key is in this mode, -1 */
/* ---- ec_clip.c ---- */
void ec_clip_reset(void);
void ec_clip_tap(int kind, int index);        /* a tap with F1 / F2 on: a step (0..), a page (0..), a pattern (0..63) */
/* ---- ec_mix.c: MIX, the CAT, PAGES ---- */
void ec_mix_track_down(int i);
void ec_mix_track_up(int i);
void ec_mix_black(int key);                   /* B1..B8 rates, B9 slower, B10 faster, B11 the CAT */
void ec_mix_hold_tick(void);
int ec_mix_rate_index(int key);               /* B1..B8 -> the rate index (0: not in the table) */
void ec_pages_black_down(int key);
void ec_pages_black_up(int key);
void ec_pages_hold_tick(void);
int ec_pages_count(void);                     /* the selected track's pattern pages */
/* ---- ec_keyboard.c ---- */
void ec_kb_key(int k, int down, int vel);     /* any of the 27 keys (k: the note key) */
void ec_kb_oct(int d);
void ec_kb_target(int det);                   /* ALGO */
int ec_kb_slice_track(void);
int ec_kb_semis(int k);                       /* key k's note in semitones from C4, the octave applied */
/* ---- ec_xfd.c ---- */
void ec_xfd_white(int i, int down, int lean);
void ec_xfd_black(int key, int down);
void ec_xfd_tick(const uint8_t *lean);        /* the glide (key_lean of the held keys) */
float ec_xfd_key_value(int i);                /* W1..W12 -> 0..1 */
int ec_xfd_glide_ms(void);                    /* the base glide time (full travel) */
/* ---- ec_patlaunch.c ---- */
void ec_pat_white(int i);
void ec_pat_black(int key);
/* ---- ec_appset.c ---- */
void ec_as_load(void);                        /* from ec_plat_settings (defaults when unwritten or foreign) */
void ec_as_open(void);
void ec_as_close(int keep);                   /* F2 / HOME keep (and save), F1 back */
void ec_as_store(void);                       /* the live values (ec_app.pn.as) to the record */
void ec_as_knob(int role, int det);
const char *ec_as_row_label(int row);
void ec_as_row_value(int row, char *out, int cap);
extern const ec_appset_t *ec_as;              /* the live values */
/* ---- ec_panel_leds.c ---- */
void ec_pn_leds(void);
/* ---- the items ---- */
float ec_pn_item_value(int id);
void ec_pn_item_format(int id, float x, char *out, int cap);
float ec_pn_item_frac(int id, float x);
void ec_pn_item_edit(int id, int det);
int ec_pn_page(void *out);                    /* (ec_page_t *) a fixed screen's (or CROSSFADER's) cells: 1 = given */

/* ---- part 2's hooks (src/app/ec_liverec.c: stubs until part 2) ---- */
void ec_liverec_toggle(void);                 /* REC tapped (not in FILE) */
void ec_liverec_settings(void);               /* REC held / SHIFT + REC: the record settings */
int ec_liverec_on(void);                      /* recording (the REC LED) */
void ec_file_settings(void);                  /* FILE held / SHIFT + FILE */
void ec_file_black(int key, int down);        /* FILE's black keys (the stand-in: B10 LOAD) */
void ec_sampler_enter(int on);                /* SAMPLER entered (1) / left (0) */

#endif
