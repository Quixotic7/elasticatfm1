/* SPDX-License-Identifier: GPL-3.0-only */
/* 0.6.0: the panel by modes (docs/UX-V2.md). Every button opens a mode (ec_button_modes: the layout's), each mode
 * remembers its page, a tap on the lit button turns the page (HOME / EDIT / FILE: the next sub-mode), a hold (400 ms)
 * or SHIFT + the button opens its settings, SHIFT + HOME the APP SETTINGS. 0.6.1: entering a mode shows nothing extra
 * (no pop-up), messages and held keys show in the header (ec_header.c). The global rules (§1: steps, F1 / F2,
 * SHIFT, PLAY, REC), copy / cut / paste (§2) and every mode's keys (§3) are here:
 *   ec_panel.c      the input dispatch, the mode model, the holds, SHIFT, F1 / F2, the intro
 *   ec_header.c     0.6.1: the header's title line (a message > a held key's text > the screen's title)
 *   ec_srckeys.c    the source template (B1..B5 pages, B6..B10 sources, B11), FX's slot keys, the quick settings
 *   ec_clip.c       §2: the one-step buffer (steps, PAGES' pages, PATTERN LAUNCH's patterns)
 *   ec_mix.c        MIX (track keys, rates) and the CAT; PAGES
 *   ec_keyboard.c   KEYBOARD
 *   ec_xfd.c        CROSSFADER (scenes, the pressure glide)
 *   ec_patlaunch.c  PATTERN LAUNCH
 *   ec_appset.c     APP SETTINGS (the global record in ec_plat_settings)
 *   ec_panel_leds.c the LEDs and the live labels of every mode
 *   ec_file.c       FILE: the browser (folders, preview, LOAD, free, purge), the editor, the pool meter
 *   ec_sampler.c    SAMPLER: B11 arms / stops on the bar, the take into the next free slot, the input VU
 *   ec_liverec.c    live REC (§4): the hits and knob moves written, CAPTURE / FIXED / OVERDUB, the count-in and the
 *                   click after the master bus, the record settings
 * The state lives in ec_app_t.pn (and .scr: the fixed screen shown, EC_MODE_*, 0 = the category's). */
#ifndef EC_MODE_H
#define EC_MODE_H
#include <stdint.h>

#include "ec_click.h"
#include "ec_pages.h"
#include "ec_project.h"

/* items the panel makes itself (beside the layout's EC_IT_*): ec_app.c formats / edits them through ec_pn_item_* */
#define EC_IT_UX 40
#define EC_CAT_T_BEAT 1.5   /* 0.6.3: the CAT fractal's evolution a beat played (stopped: 1 a second) */
enum { EC_UX_TARGET,   /* KEYBOARD: the target step (0 = live) */
       EC_UX_OCT,      /* KEYBOARD: the octave */
       EC_UX_PLOOP,    /* PAGES: the pages in the loop ("1-2") */
       EC_UX_GLIDE,    /* CROSSFADER: the base glide time */
       EC_UX_ALOCKS,   /* CROSSFADER: scene A's values */
       EC_UX_BLOCKS,   /*   .. scene B's */
       EC_UX_ADVANCE,  /* HOME settings: KEYBOARD's ADVANCE */
       EC_UX_PLAYING,  /* PATTERN LAUNCH: the pattern playing */
       EC_UX_NEXT,     /*   .. the one queued */
       EC_UX_FSLOT,    /* FILE browser: the slot LOAD fills (the next free) */
       EC_UX_FLEN,     /*   .. the length of the file under the cursor (seconds; 0 unknown) */
       EC_UX_FBPM,     /*   .. its BPM (from its name, or the pooled sample's) */
       EC_UX_FREE,     /* FILE / SAMPLER: the arena's free MiB */
       EC_UX_SLEN,     /* SAMPLER: the take's length (FREE, 1, 2, 4, 8 bars) */
       EC_UX_SGAIN,    /*   .. the input gain (dB) */
       EC_UX_SMON,     /*   .. the input monitor on / off */
       EC_UX_FXT0,     /* 0.6.3: FX SETUP: the effect type of slot 0 INSERT 1 .. 4 MASTER (EC_UX_FXT0 + slot) */
       EC_UX_FXT4 = EC_UX_FXT0 + 4,
       EC_UX_SMODE,    /* 0.6.3: FX SETUP with a bus targeted: the track's SEND MODE (send_tap) */
       EC_NUX };

/* APP SETTINGS (docs/UX-V2.md §3): the global record (ec_plat_settings), magic + version */
#define EC_AS_MAGIC 0x53414345u   /* "ECAS" */
#define EC_AS_VERSION 1
/* 0.6.1: the MODE POP-UP row is gone; the record keeps its layout (and version 1), so older records load as they are */
enum { EC_AS_THEME, EC_AS_KNOB, EC_AS_PREVIEW, EC_AS_AT, EC_AS_METRO, EC_AS_CC, EC_AS_INTRO, EC_AS_NROW };
typedef struct {
    uint32_t magic;
    uint16_t version, size;
    uint8_t theme;     /* ec_gfx theme 0..EC_NTHEME-1 */
    uint8_t unused0;   /* 0.6.0's mode pop-up on / off (gone in 0.6.1; kept for the layout, written 0) */
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
    /* 0.6.1: the header */
    uint32_t down_ms;          /* the last key or button press (a message older than it gives way to a held key's text) */
    int hdr_late;              /* the redraw EC_HDR_MS after the last press (a held key's text shows) is done */
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
    int ins_slot;              /* 0.6.1: FX's INSERT page: the slot shown, 0 INS 1, 1 INS 2 (0.6.2: an ec_nav entry) */
    int ins_page;              /*   its knob page (P1..) */
    /* MIX: the track keys W1..W9 held (tap select, hold mute) */
    uint16_t trk_down, trk_used;
    uint32_t trk_ms[9];
    int cat;                   /* MIX's CAT shows */
    uint32_t cat_ms;
    uint32_t cat_n;            /* 0.6.2: the CAT's redraws */
    /* 0.6.3 (docs/UX-V2.md §10 "CAT fractals"): the fractal's algorithm (+1: 0 none yet) and seed, picked on each entry
     * by the app's generator (cat_rng, seeded from the tick count); its evolution clock (cat_t: stopped, 1/40 a redraw;
     * playing, CAT_T_BEAT a beat of the transport, cat_beats the last beats seen) */
    int cat_algo1;
    uint32_t cat_seed, cat_rng;
    double cat_t, cat_beats;
    /* LOOP ENGINE: the zoom's half (32: 0 first, 1 second) */
    int zoom_half;
    /* KEYBOARD */
    int oct;                   /* -3..3 */
    int target;                /* the target step 1..; 0 = live */
    int kb_bank;               /* the slice bank (10 a bank) */
    int8_t kb_semis[32];       /* the note a key started (its release) */
    uint32_t kb_held;          /* 0.6.1: the keys held (bit k), the last one pressed (the header) */
    int kb_last;
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
    /* FILE (ec_file.c): the browser's folder (the cursor is ec_app.cursor, the row), the preview, the editor */
    char fdir[160];            /* "/audio", "/audio/breaks" */
    uint32_t file_refused_ms;  /* FILE refused while playing: its button flashes */
    int pv_state;              /* EC_PV_* */
    int pv_handle;             /* the store handle the preview holds (-1) */
    int pv_owned;              /* 1: the preview's own (freed when it moves on); 0: a pool slot adopted it */
    int pv_play;               /* sounding */
    double pv_pos;             /* its read position (frames) */
    char pv_path[256];
    int ed_zoom;               /* the editor: 0..8 = 1X..256X */
    float ed_view;             /*   its left edge 0..1 */
    int ed_point;              /*   0.6.1: the point a white key places: 0 START (F1), 1 END (F2) */
    int ed_hold;               /*   the white key held (0..15; the second key places the other point), -1 */
    int pool_cur;              /* 0.6.1: POOL's cursor (slot 0..127) */
    int pool_carry;            /*   MOVE: the slot picked up, -1 none */
    /* SAMPLER (ec_sampler.c) */
    float smp_gain_db;         /* the input gain */
    int smp_mon;               /* the input monitor */
    /* live REC (ec_liverec.c) */
    int lr_on;                 /* recording (the REC LED) */
    int lr_wait;               /* CAPTURE, stopped: the first key / loop key starts playback */
    int lr_ci;                 /* a count-in REC started (the transport starts after it) */
    double lr_end;             /* FIXED: the song beat it ends on (ec_seq_song_beats) */
    int lr_writes;             /* hits / locks written since REC went on (ui_info, tests) */
    int lr_was_playing;
    ec_click_t click;          /* the metronome (mixed after the master bus) */
    int rs_open, rs_row;       /* the record settings list (hold REC / SHIFT + REC) */
    ec_recset_t rs_undo;
} ec_pn_t;

enum { EC_PV_IDLE, EC_PV_WAIT /* a slot load first */, EC_PV_LOADING, EC_PV_READY, EC_PV_ERROR };
#define EC_ED_ZOOMS 9         /* the editor's zoom levels B2..B10: 1X 2X 4X .. 256X */

/* ---- ec_panel.c ---- */
void ec_pn_init(void);
void ec_pn_tick(const void *in);              /* (const ec_input_t *): every edge, the holds, the glide */
int ec_app_mode(void);                        /* the mode shown: A->scr, else A->cat */
void ec_pn_enter(int mode);                   /* that mode, the settings closed */
void ec_pn_goto(int cat, int page);           /* a page of a category: the mode, that page */
int ec_pn_f(int which);                       /* F1 (1) / F2 (2) on: held or sticky */
void ec_pn_select_target(int i);              /* 0..5 tracks, 6 S1, 7 S2, 8 MST */
const char *ec_pn_roles(int mode, int *cur);  /* the 11 black-key roles (the LEDs), the current page's key - 1 */
/* ---- ec_header.c (0.6.1): the header's title line. Priority: a message (ec_app_message / ec_app_alert, for its time;
 * one older than the last key press gives way while a held key has text) > a held key's text > the screen's title.
 * A held key's text: a provider's (ec_hdr_provider, asked newest first), else the built-in ones (docs/FM1-CONTROLS.md
 * "The header"). The bar's colour is a palette slot: EC_C_L12 the plain bar, EC_C_SRC a source key, EC_C_LOCK held
 * steps, EC_C_ALERT an alert. ---- */
#define EC_HDR_MS 150                         /* a held key's text shows this long after the last press (a tap: none) */
typedef int (*ec_hdr_fn)(char *out, int cap); /* a held key's text into out: the bar's slot (EC_C_*), -1 nothing held */
void ec_hdr_provider(ec_hdr_fn fn);           /* register (once; up to 4): asked before the built-in held keys */
int ec_hdr_held(char *out, int cap);          /* the held keys' text now: the bar's slot, -1 none */
int ec_hdr_title(const char *title, char *out, int cap);   /* the title line: its slot (EC_C_L12: plain) */
int ec_hdr_custom(void);                      /* the title line shows a message or a held key's text */
int ec_hdr_holding(void);                     /* EC_HDR_MS since the last key / button press (a provider may gate on it) */
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
/* 0.6.1: FX's INSERT page (the track's, not a bus's) is shown: its cells are the selected slot's knob page */
int ec_src_insert_page(void);
/* 0.6.3 (docs/UX-V2.md §10 "FX layout"): the slots 0 INSERT 1, 1 INSERT 2, 2 SEND 1, 3 SEND 2, 4 MASTER */
int ec_fx_effects_page(void);
int ec_fx_sends_page(void);
int ec_fx_slot(void);                          /* the slot shown (A->bus 1..3: 2..4, else pn.ins_slot) */
int ec_fx_on_sends(void);
int ec_fx_slot_machine(int slot);
const char *ec_fx_slot_name(int slot);         /* "INSERT 1" .. "MASTER" */
void ec_fx_sends_items(void *out);
void ec_fx_setup_items(void *out);             /* (ec_page_t *) FX SETUP: the five slots' types, SEND MODE */
void ec_fx_type_step(int slot, int det);       /* a slot's effect type one on (INSERT 2: its params reset) */             /* (ec_page_t *) SENDS's cells */
void ec_fx_send_mode(int det);                 /* SENDS: ALGO, the send mode PRE / POST */
void ec_fx_swap_do(int which);                /* SENDS: swap the inserts (0) / the sends (1), no undo */
void ec_fx_swap_undo(int which);              /* SENDS: record that swap's undo */
int ec_fx_cur_key(void);                       /* the key of the place shown (1..5, 11) */
int ec_src_ins_pages(void);                   /* the selected slot's knob pages (P1..): its params in threes, >= 1 */
int ec_src_ins_param(int slot, int i);        /* the id knob K(i%3+1) of page i/3 edits (INS 2: EC_P_INS2_Pn), -1 */
int ec_src_ins_mix(int slot);                 /* the slot's K4 MIX param (INS 1: fx_mix / fx_wet, INS 2: its MIX) */
void ec_src_ins_items(void *out);             /* (ec_page_t *) the INSERT page's four cells */
int ec_src_ins_pages_of(int slot);            /* slot 0 / 1's knob pages, >= 1 */
/* ---- ec_nav.c (0.6.2): one page list per mode; SEL walks it (no wrap), a page key jumps into its group, tapped
 * again steps through the group (docs/UX-V2.md §11 "0.6.2 navigation") ---- */
#define EC_NAV_MAX 48
enum { EC_NAV_ROWS = 1,        /* a list (BROWSE, POOL): SEL scrolls its rows; SEL stops on it, keys leave it */
       EC_NAV_ROWS_THRU };     /* the MATRIX: SEL its rows, then on past either end */
typedef struct {
    int8_t page;               /* the mode's page (FX on a bus: 0) */
    int8_t sub;                /* the lane; FX INSERT: the knob page (P1 = 0); PATTERN LAUNCH: the bank */
    int8_t slot;               /* FX INSERT: 0 INS 1, 1 INS 2 */
    int8_t bus;                /* FX: 1..3 the bus page S1 / S2 / MST, 0 the track's */
    int8_t key;                /* the black key of its group (1..11), 0 none */
    int8_t rows;               /* EC_NAV_ROWS* */
} ec_nav_t;
int ec_nav_list(int mode, ec_nav_t *out, int max);   /* the mode's pages in order (for the state now): the count */
int ec_nav_at(int mode, ec_nav_t *out, int max, int *n);   /* the list, and the entry shown (-1 none) */
void ec_nav_go(int mode, const ec_nav_t *e, int dir);   /* show entry e (dir < 0: arrived backwards) */
int ec_nav_sel(int d);                        /* SEL in the mode shown: d entries, stops at the ends: 1 = moved */
int ec_nav_key_down(int mode, int key);       /* a page key down: 1 jumped to its group, 2 already in it, 0 none */
int ec_nav_key_tap(int mode, int key);        /* the key tapped inside its group: the next entry of it (wraps) */
int ec_nav_key(int mode, int key);            /* down + tap at once (keys without a hold) */
int ec_nav_next_group(int mode);              /* the mode button tapped again: the next group's first (wraps) */
int ec_nav_place(int *k, int *count);         /* the entry's place among its page's entries (P1 = 0), how many */
float ec_src_load(void);                      /* the load meter's reading 0..1 (the counted engine load) */
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
/* 0.6.6 / 0.6.9: LOOP ENGINE step entry (ec_loopstep.c): B11 switches the white keys between LOOP and STEP layers;
 * the selected steps get the loop keys' regions / ranges and the knobs' locks */
int ec_lp_active(void);                       /* LOOP ENGINE shown (a track, no settings) */
void ec_lp_track(int from, int to);           /* 0.6.11: the selected track changed: its own selection / target */
int ec_lp_target(void);                       /* the first selected step 1.., 0 = LIVE (or not LOOP ENGINE) */
int ec_lp_first(void);                        /* the first selected step's index, -1 none */
int ec_lp_count(void);                        /* steps selected */
int ec_lp_is_sel(int idx);
uint16_t ec_lp_page_mask(int page);           /* the selected steps of a step page */
int ec_lp_step_layer(void);                   /* the white keys are the STEP layer now */
void ec_lp_algo(int det);                     /* ALGO: one step selected, below 1 = LIVE */
void ec_lp_b11(int down);                     /* B11: held = the other layer, a quick click latches it */
int ec_lp_b11_held(void);                     /* B11 physically held */
void ec_lp_b11_used(void);                    /* something done while B11 was down: not a quick click */
void ec_lp_clear(void);                       /* B11 + OCT-: the selected steps cleared */
int ec_lp_white(int k, int i, int down, int vel);   /* STEP layer: 1 = taken */
void ec_lp_loop_note(int i, int down);        /* LOOP layer: a loop key (writes into the selection) */
uint32_t ec_lp_keys(void);                    /* the entry's keys held now */
int ec_kb_slice_track(void);
int ec_kb_semis(int k);                       /* key k's note in semitones from C4, the octave applied */
int ec_kb_lit_key(void);                      /* the key lit as sounding (the note + PITCH), -1 none */
const char *ec_kb_held_text(void);            /* the header's text for the last key held ("NOTE C#4", "SLICE 3"), 0 */
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

/* ---- ec_liverec.c: live REC (docs/UX-V2.md §4) ---- */
void ec_liverec_toggle(void);                 /* REC tapped (not in FILE) */
void ec_liverec_settings(void);               /* REC held / SHIFT + REC: the record settings (open / close) */
int ec_liverec_on(void);                      /* recording (the REC LED) */
int ec_liverec_armed(void);                   /* waiting for CAPTURE's first key, or counting in (the LED blinks) */
void ec_liverec_capture(void);                /* a key / loop key goes down: CAPTURE's first one starts playback */
void ec_liverec_note(int semis, int slice, int vel);   /* KEYBOARD played a note (slice > 0: that slice) */
void ec_liverec_loop(int key, int slice_track, int vel);   /* LOOP ENGINE: loop key 1..32 (a pad on slice tracks) */
void ec_liverec_param(int id, float value);   /* a knob moved a track param (AUTOMATION: a lock on the step) */
void ec_liverec_tick(void);
void ec_liverec_stop(void);                   /* the transport stopped by PLAY: recording ends */
/* the audio call, per chunk after the capture (ec_rec_post): the click, the input monitor, FILE's preview, all on
 * the main output after the master bus (never in a take) */
void ec_liverec_audio(int16_t *out, uint32_t n, const int16_t *in, uint32_t in_n);
void ec_rs_close(int keep);                   /* the record settings: F2 / REC keep, F1 back */
void ec_rs_knob(int role, int det);
const char *ec_rs_row_label(int row);
void ec_rs_row_value(int row, char *out, int cap);
int ec_rs_row_dim(int row);
#define EC_RS_NROW 7
/* ---- ec_file.c: FILE (docs/UX-V2.md §3) ---- */
void ec_file_settings(void);                  /* FILE held / SHIFT + FILE: the recorder's setup (0.4's REC pop-up) */
void ec_file_black(int key, int down);        /* FILE's and SAMPLER's black keys (SHIFT + B11: purge) */
void ec_file_white(int i, int down);          /* the editor's white keys (the trim): press and release */
void ec_file_hdr_init(void);                  /* 0.6.1: the header's text for a held trim key (ec_hdr_provider) */
void ec_file_ed_point(int end);               /* 0.6.1: the EDITOR's F1 / F2: a white key places 0 START, 1 END */
int ec_file_f2(void);                         /* F2 in the browser: free the pool slot of the file under the cursor */
int ec_file_play_leave(void);                 /* 0.6.11: PLAY in FILE on the EDITOR / RAZOR: to BROWSE, 1 = press used */
void ec_file_select(int d);                   /* SEL in the browser */
int ec_file_page(void);                       /* FILE's page: its picture (EC_PIC_FILE / EDITOR / RAZOR / POOL) */
int ec_file_key_page(int key);                /* the FILE page black key 1..11 opens, -1 */
int ec_file_count(void);                      /* the browser's rows in this folder */
const char *ec_file_entry(int i, int *is_dir, int *file);   /* row i's name, a folder?, its file (ec_browse index) */
int ec_file_pooled(const char *path);         /* the pool slot (0-based) holding that file, -1 */
int ec_file_next_free(void);                  /* the next free pool slot, -1 full */
int ec_file_slots_used(void);
int ec_file_purge(void);                      /* frees every slot no pattern uses, compacts: the slots freed (-1 busy) */
int ec_file_free_slot(int s);                 /* 0 freed, -1 busy */
void ec_file_tick(void);                      /* the preview's loader */
void ec_file_leave(void);                     /* FILE left: the preview stops (its sample freed) */
void ec_file_preview_yield(void);             /* a slot load starts: a preview load in flight gives way */
void ec_file_preview_mix(int16_t *out, uint32_t n);
float ec_file_zoom(void);                     /* the editor's zoom factor 1..256 */
/* 0.6.1: FILE / POOL (docs/UX-V2.md 10): the 128 slots; SEL the cursor, B10 ASSIGN, B11 preview, F1 MOVE, F2 FREE */
void ec_file_pool_select(int d);
void ec_file_pool_f1(void);                   /* MOVE: pick the slot up / drop it (a swap, every reference follows) */
void ec_file_pool_f2(void);                   /* FREE the slot under the cursor */
void ec_file_pool_scan(uint8_t *tracks, uint8_t *locks);   /* per slot: the tracks playing it (bits), step-locked */
void ec_file_slots_swapped(int a, int b);     /* slots a, b swapped in the project: the runtime follows (undo too) */
/* ---- ec_sampler.c: SAMPLER ---- */
void ec_sampler_enter(int on);                /* SAMPLER entered (1) / left (0) */
void ec_sampler_black(int key, int down);     /* B11 arms / stops */
int ec_sampler_len(void);                     /* the take's length in bars (0 free) */
float ec_sampler_vu(void);                    /* the input's level 0..1 on a dB scale (the white keys, the screen) */
void ec_sampler_input(int16_t *in, uint32_t n);   /* the input gain, before the capture */
/* ---- the app (ec_app.c) for them ---- */
void ec_app_alert(const char *s);             /* a message in ALERT ("STOP FIRST", "FULL") */
void ec_app_assign_samples(void);             /* every engine track gets its slot's sample now */
void ec_app_slot_adopt(int s, int h, const char *path);   /* a ready store handle as a fresh load of path into slot s */

#endif
