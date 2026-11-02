/* SPDX-License-Identifier: GPL-3.0-only */
/* 0.3.0: the panel along ChoralRoot and FIZZ (docs/UX-REDESIGN.md, docs/FM1-CONTROLS.md): lanes, setup by SHIFT or
 * a hold, HOME, the pop-ups (F3, the PATN overlay, naming, questions) with OCT- back / OCT+ OK, SHIFT's keys and
 * map, knob acceleration and snap, the knob takeover, PRESETS = the track's sample, the held-step frame, track keys
 * (tap select, hold mute, double-tap solo), the live panel labels. The state lives in ec_app_t.ux; ec_app.c's
 * input dispatch calls the functions below. */
#ifndef EC_UX_H
#define EC_UX_H
#include <stdint.h>

#define EC_HOLD_MS 400          /* a hold (FIZZ / the owner: setup, F3 by PLAY, mute, a page in the loop) */
#define EC_DTAP_MS 300          /* a second tap within this of the first's release: double-tap (solo) */
#define EC_TAKEOVER_MS 1000     /* a knob's value big on the screen */
#define EC_SAMPLE_DEBOUNCE_MS 150   /* PRESETS = the sample: the load waits for the knob to rest this long */
#define EC_SHIFTMAP_MS 250      /* SHIFT held alone this long: its map */

enum { EC_TO_NONE, EC_TO_VALUE, EC_TO_SAMPLE, EC_TO_TRACKMUTE };

typedef struct {
    /* lanes: the active lane of each category's page (reset by a page change), of the setup list */
    int lane[16];
    int set_lane;
    /* category buttons: the one held (EC_B_x, -1 none), since when, its hold already acted */
    int cat_btn;
    uint32_t cat_ms;
    int cat_used;
    int mst_down, mst_used;     /* the MST upper key: tap = MST category, hold = MST setup */
    uint32_t mst_ms;
    /* OCT keys outside the RAZOR page: held bits (1 OCT-, 2 OCT+), used since their press (no key-mode toggle) */
    int oct_held, oct_used;
    /* F3 (OCT- + OCT+, or PLAY held) */
    int f3;
    int play_down, play_used;
    uint32_t play_ms;
    uint16_t f3_keys, f3_used;  /* lower keys held in F3 (tap: edit the page, hold: the page in / out of the loop) */
    uint32_t f3_ms[16];
    /* track keys */
    uint32_t trk_ms[6], trk_up[6];
    uint8_t trk_down, trk_used;
    uint8_t solo;               /* bit t: soloed (runtime only) */
    int fill_used;
    /* SHIFT */
    uint32_t shift_ms;
    int shift_used;
    uint32_t tap_ms[5];
    int ntap;
    /* the takeover */
    int to_kind;
    uint32_t to_ms;
    char to_label[16], to_val[40], to_sub[16];
    float to_frac;
    /* PRESETS = the sample (debounced) */
    int pre_idx;                /* the file the knob points at, -1 none pending */
    uint32_t pre_ms;
    /* knob acceleration: the last detent's time per role */
    uint32_t knob_ms[8];
} ec_ux_t;

/* lanes */
int ec_ux_lanes(int n);                                  /* lanes of a page with n cells */
/* the knob speed: steps a click for a value with `steps` finest steps over its range (lists: 1) */
int ec_ux_accel(int role, int steps, int det);
/* the takeover */
void ec_ux_takeover(const char *label, const char *val, float frac);
/* the lock chips: id is locked on some step of the selected track (outlined chip) */
int ec_ux_locked_somewhere(int id);

/* the live panel labels (plat calls it after every frame) */
void ec_app_labels(void);

#endif
