/* SPDX-License-Identifier: GPL-3.0-only */
/* The 240x240 screens in the mockup's style (design/mockup/Display.dc.html "fm1" / "mix", FM1Screens.dc.html): the
 * header strip on every page, SRC (waveform, region, playhead, loop-key ruler), the generic param pages (TRIG with the
 * step strip, FILT with the filter curve, AMP / F.ENV with the envelope), FILE (the sample browser), MASTER / MIX
 * (meters + faders), PAT (the 16 patterns), the settings lists, and the four cells under them. */
#ifndef EC_SCREENS_H
#define EC_SCREENS_H

#include "ec_app.h"

void ec_screens_draw(const ec_app_t *a);   /* the whole screen (ec_gfx_begin first) */

/* E3: ec_scr_e3.c (the RAZOR page, the neighbor routing view) + the pieces of ec_screens.c it shares */
void ec_scr_razor(const ec_app_t *a, const ec_page_t *pg);
void ec_scr_neighbor(const ec_app_t *a, int top, int bot);
void ec_scr_header(const ec_app_t *a, const char *title, int page, const char *right);
void ec_scr_cells(const ec_app_t *a, const ec_page_t *pg);

/* E6: ec_scr_rec.c (the REC pop-up; the INPUT machine's view on SRC) */
void ec_scr_rec(const ec_app_t *a);
void ec_scr_input(const ec_app_t *a, int top, int bot);

#endif
