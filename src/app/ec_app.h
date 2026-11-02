/* SPDX-License-Identifier: GPL-3.0-only */
/* The app: the FM-1 panel mapped onto Elasticat (PLAN.md, design/mockup/FM1Panel.dc.html, docs/FM1-CONTROLS.md),
 * the sample pool's loading, the project's save, the LEDs. The screens (src/ui/ec_screens.c) read this state. */
#ifndef EC_APP_H
#define EC_APP_H
#include <stdint.h>

#include "ec_dsp.h"
#include "ec_pages.h"
#include "ec_plat.h"
#include "ec_project.h"
#include "ec_samplestore.h"
#include "ec_seq.h"

enum { EC_KM_STEPS, EC_KM_LOOP, EC_KM_KEYS, EC_NKM };     /* what the lower keys are (OCT-/OCT+) */
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
    int cat;                     /* EC_CAT_* */
    int page[EC_NCAT];
    int settings;                /* the FN + category layer is open */
    int fn;                      /* HOME held */
    int keymode;                 /* EC_KM_* */
    int step_page;               /* the step page the lower keys edit (0..15) */
    int sel_held, sel_used, sel_fn;   /* SEL (SRC) held: the loop keys; FN at its press */
    uint32_t sel_ms;
    uint16_t step_held;          /* lower keys held as steps (bit i) */
    uint16_t step_edited;
    uint32_t step_ms[EC_NLOWER];
    uint8_t key_role[EC_NKEY];
    uint32_t prog_keys;          /* loop keys held while programming a step's region */
    int tap_key, tap_kind;       /* the region tap cycle (loop_tap_start_only) */
    int patn_open, patn_latched, patn_used;
    uint32_t patn_ms;
    int fill_held, fill_latched;
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
    int redraw;
    uint32_t last_draw;
    float meter[9], meter_hold[9];
    uint32_t frames_out;
    uint32_t ms;
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
const ec_ss_sample_t *ec_app_track_sample(const ec_app_t *a, int track);
int ec_app_track_slot(const ec_app_t *a, int track);         /* 0-based pool slot */
int ec_app_stopped(const ec_app_t *a);

#endif
