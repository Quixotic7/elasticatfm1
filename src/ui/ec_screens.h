/* SPDX-License-Identifier: GPL-3.0-only */
/* The 240x240 screens in the mockup's style (design/mockup/Display.dc.html "fm1" / "mix", FM1Screens.dc.html): the
 * header strip on every page, SRC (waveform, region, playhead, loop-key ruler), the generic param pages (TRIG with the
 * step strip, FILT with the filter curve, AMP / F.ENV with the envelope), FILE (the sample browser), MASTER / MIX
 * (meters + faders), PAT (the 16 patterns), the settings lists, and the four cells under them. */
#ifndef EC_SCREENS_H
#define EC_SCREENS_H

#include "ec_app.h"

void ec_screens_draw(const ec_app_t *a);   /* the whole screen (ec_gfx_begin first) */

#endif
