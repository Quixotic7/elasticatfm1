/* SPDX-License-Identifier: GPL-3.0-only */
/* E3: the razor editor on the FM-1 (FILE category, page 2 "RAZOR"): the selected slice, the zoom view, the snap
 * edits of STRT / END (lib/elasticat.lua razor_point_edit), Auto-Chop / Redistribute / add / delete, the lower keys
 * as slice pads, the transient cache. ec_app.c dispatches here; src/ui/ec_scr_e3.c draws it. */
#ifndef EC_RAZOR_UI_H
#define EC_RAZOR_UI_H

#include "ec_app.h"

typedef struct {
    int sel;              /* the selected slice 1..32 */
    int zoom;             /* 1, 2, 4 .. 64 */
    float focus;          /* the last edited point (track space), -1 none: the zoom centres on it */
    int focus_slice;
    uint32_t sens_ms;     /* 0.6.15: SENS last turned (the transient ticks show for a moment) */
} ec_rzui_t;

extern ec_rzui_t ec_rzui;

int ec_rzui_active(const ec_app_t *a);                 /* the RAZOR page is on screen (lower keys are its pads) */
int ec_rzui_count(const ec_app_t *a);                  /* slices in use (slice_count) */
/* cells (EC_IT_RAZOR) */
float ec_rzui_value(const ec_app_t *a, int id);
void ec_rzui_format(const ec_app_t *a, int id, float x, char *out, int cap);
float ec_rzui_frac(const ec_app_t *a, int id, float x);
void ec_rzui_edit(ec_app_t *a, int id, int det, int mult);
/* gestures */
void ec_rzui_select(ec_app_t *a, int det);             /* SELECT knob */
void ec_rzui_key(ec_app_t *a, int i, int down, int vel);   /* lower key i (0..15): select + play; FN: mute */
void ec_rzui_oct(ec_app_t *a, int up);                 /* OCT+ add, OCT- delete; FN: Auto-Chop / Redistribute */
void ec_rzui_leds(const ec_app_t *a);
/* the track window of the sample (0..1, trim x Range) and the transients inside it (track space 0..128) */
void ec_rzui_window(const ec_app_t *a, int track, double *lo, double *hi);
int ec_rzui_transients(const ec_app_t *a, int track, float *x128, int cap);
const char *ec_rzui_snap_name(int snap);

#endif
