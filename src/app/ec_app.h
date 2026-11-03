/* SPDX-License-Identifier: GPL-3.0-only */
/* The app: the FM-1 panel mapped onto Elasticat (PLAN.md, design/mockup/FM1Panel.dc.html, docs/FM1-CONTROLS.md),
 * the sample pool's loading, the project's save, the LEDs. The screens (src/ui/ec_screens.c) read this state. */
#ifndef EC_APP_H
#define EC_APP_H
#include <stdint.h>

#include "ec_dsp.h"
#include "ec_e5app.h"
#include "ec_mode.h"
#include "ec_modkeys.h"
#include "ec_pages.h"
#include "ec_plat.h"
#include "ec_project.h"
#include "ec_rec.h"
#include "ec_samplestore.h"
#include "ec_seq.h"
#include "ec_ux.h"

enum { EC_SL_EMPTY, EC_SL_QUEUED, EC_SL_LOADING, EC_SL_READY, EC_SL_MISSING, EC_SL_ERROR };

typedef struct {
    int handle;                  /* the sample store's handle (ready), -1 none */
    int state;                   /* EC_SL_* */
    int builtin;                 /* the synthesized loop (no folder) */
    int newmeta;                 /* a user load: BPM / steps / trim from the file (else the project's) */
    int old_handle;              /* the sample it replaces (freed once the new one plays) */
    char err[24];
} ec_slotrt_t;

typedef struct {
    /* the model */
    ec_project_t proj;
    ec_seq_t seq;
    ec_dsp_t *dsp;
    /* the panel */
    int track;                   /* the selected real track 0..5 (steps, params) */
    int bus;                     /* 0 a track, 1 S1, 2 S2, 3 MST selected */
    int cat;                     /* EC_CAT_*: the category (0.6.0: the mode, when scr is 0) */
    int scr;                     /* 0.6.0: a fixed screen's mode shown (EC_MODE_KEYBOARD ..), 0 = the category's */
    int page[EC_NCAT];
    int settings;                /* the category's setup is open (SHIFT + the button, or a hold) */
    int fn;                      /* SHIFT (GLO): held, or sticky (0.6.0) */
    int step_page;               /* the step page the white keys edit (0..15) */
    uint16_t step_held;          /* lower keys held as steps (bit i) */
    uint16_t step_edited;
    uint32_t step_ms[EC_NLOWER];
    uint8_t key_role[EC_NKEY];
    uint32_t prog_keys;          /* loop keys held while programming a step's region */
    int tap_key, tap_kind;       /* the region tap cycle (loop_tap_start_only) */
    int fill_held, fill_latched; /* 0.6.0: SHIFT + B11 held, SHIFT + B2 (LATCH) */
    uint32_t knob_ms[EC_NE];
    int master;                  /* the MASTER pot 0..1023 */
    /* samples */
    ec_slotrt_t slot[EC_NSLOT];
    int load_slot;               /* the slot being loaded (-1) */
    int repack;                  /* the store is being rebuilt (full) */
    int assigned[EC_NTRACK];     /* the store handle each engine track plays */
    int scanning, no_folder, cursor;
    int32_t fs_changed;
    int fresh;                   /* no project in the flash: load the first file when the scan ends */
    /* save */
    int dirty;
    uint32_t dirty_ms;
    int saves;
    /* the screen */
    char msg[32];
    uint32_t msg_ms;
    int msg_alert;               /* 0.6.0: the message in ALERT (STOP FIRST, FULL) */
    int redraw;
    uint32_t last_draw;
    float meter[9], meter_hold[9];
    uint32_t frames_out;
    uint32_t ms;
    ec_e5app_t e5;               /* E5 */
    ec_ux_t ux;                  /* 0.3.0 */
    ec_rec_t rec;                /* E6: sampling (the REC pop-up), the input meters */
    ec_mk_t mk;                  /* 0.5.0: the mod matrix's MATRIX page, the source keys, the live controls */
    ec_pn_t pn;                  /* 0.6.0: the modes (ec_mode.h) */
} ec_app_t;

extern ec_app_t ec_app;

/* the page shown now (the settings layer when open) */
void ec_app_page(const ec_app_t *a, ec_page_t *out);
float *ec_app_params(ec_app_t *a);                            /* the selected track's params */
/* a cell's value: the stored value, the held step's lock (*locked = 1) */
float ec_app_item_value(const ec_app_t *a, const ec_item_t *it, int *locked);
void ec_app_item_format(const ec_app_t *a, const ec_item_t *it, float x, char *out, int cap);
const char *ec_app_item_label(const ec_item_t *it);
float ec_app_item_frac(const ec_app_t *a, const ec_item_t *it, float x);
int ec_app_held_step(const ec_app_t *a);                      /* the first held step's index in the pattern, -1 */
int ec_app_held_lock(const ec_app_t *a, int id, float *x);   /* 0.6.1: id locked on a held step: its value */
const ec_ss_sample_t *ec_app_track_sample(const ec_app_t *a, int track);
int ec_app_track_slot(const ec_app_t *a, int track);         /* 0-based pool slot */
int ec_app_stopped(const ec_app_t *a);
float *ec_app_bus_params(ec_app_t *a, int id);              /* E4: the bus record holding param id, or NULL */
/* 0.3.0 */
int ec_app_lane(const ec_app_t *a, const ec_page_t *pg);     /* the active lane of the page shown (clamped) */
float ec_app_move_param(int id, float x, int det, int mult); /* a knob turn: snap with SHIFT, else det x mult steps */
int ec_app_cells_visible(const ec_app_t *a);                 /* the active lane's cells are on screen */
int ec_app_popup(const ec_app_t *a);                         /* a pop-up is open (F1 back / F2 OK; E6: REC) */
int ec_app_shift_map(const ec_app_t *a);                     /* SHIFT held alone (or sticky): its screen shows */
int ec_app_track_audible(const ec_app_t *a, int t);          /* not muted, not silenced by a solo */
/* 0.5.0 (ec_modkeys.c) */
void ec_app_edit_param(int id, int det, int mult);           /* a knob turn on a track param (locks, undo) */
int ec_app_param_steps(int id);                              /* its finest steps (the knob speed) */
void ec_app_goto(int cat, int page);                         /* that page (its mode), the setup / pop-ups closed */
void ec_app_message(const char *s);
void ec_app_mark_dirty(void);
/* 0.6.0: the model for the panel (src/app/ec_panel.c ..) */
void ec_app_edit_param_f(int id, int det, int mult, int flags);   /* flags: the item's (EC_IF_STOPPED) */
void ec_app_clear_lock(int id);                              /* F1 + a knob */
void ec_app_edit_global(int id, int det, int mult);
void ec_app_knob_cell(int k, int det);                       /* K1..K4 (k 0..3) on the page shown, F3's */
void ec_app_lane_step(int d);                                /* SELECT on a page */
void ec_app_set_page(int cat, int page);
int ec_app_has_settings(int cat);
void ec_app_save(int quiet);
void ec_app_fresh_project(void);                          /* 0.6.1: a new project (the last one's file gone) */
void ec_app_load_file(int slot, const char *path);
void ec_app_toggle_mute(int t);
void ec_app_toggle_solo(int t);
void ec_app_clear_mutes(void);

#endif
