/* SPDX-License-Identifier: GPL-3.0-only */
/* 0.3.0: SHIFT + a value knob snaps to useful values (src/seq/ec_snap.c has the tables and the rules). */
#ifndef EC_SNAP_H
#define EC_SNAP_H

#include "ec_params.h"

/* param id from x, |det| snap steps in det's direction (lists: one item a click, as ec_param_nudge) */
float ec_param_snap(int id, float x, int det);
/* the values that are not params (the project globals, the slot, the crossfader) */
enum { EC_SNAP_BPM, EC_SNAP_GLEN, EC_SNAP_SWING, EC_SNAP_GAIN, EC_SNAP_STEPS, EC_SNAP_XFD };
float ec_global_snap(int which, float x, int det);
/* the building blocks (one step): the next value of the list past x, the next multiple of m, a range's rule */
float ec_snap_list(const float *v, int n, float x, int det);
float ec_snap_multiple(float x, int det, float m, float lo, float hi);
float ec_snap_range(float lo, float hi, float step, int fmt, float x, int det);

#endif
