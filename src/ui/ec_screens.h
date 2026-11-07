/* SPDX-License-Identifier: GPL-3.0-only */
/* The screens (120x120 grid, 16 colours, shown at 2x: ec_gfx.h) in the mockups' style (design/mockup/Display.dc.html "fm1" / "mix", FM1Screens.dc.html): the
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
void ec_scr_header_s(const ec_app_t *a, const char *title, const char *page, const char *right);   /* page as text */
/* 0.6.2: a mode page's header: its place among the page's entries (P1, P2 ..: lanes, FX INSERT's knob pages) when
 * it has several, else the number given */
void ec_scr_header_page(const ec_app_t *a, const char *title, int page, const char *right);
void ec_scr_cells(const ec_app_t *a, const ec_page_t *pg);
void ec_scr_pages_panel(const ec_app_t *a);
void ec_scr_step_strip(const ec_app_t *a, int top, int bot, int pages_y);   /* 0.6.57: LOOP ENGINE's strip */   /* 0.6.14: B11 held: the pages, LEN, RATE in the cells area */
/* 0.6.1: MIX's pan tracks: track t's pan now (-1..1: the resolved value incl. a playing lock, plus the modulation),
 * and the marker's x (2 wide) on the track of the strip at x (strip i: x = 2 + 13 i) */
float ec_scr_mix_pan_now(const ec_app_t *a, int t);
int ec_scr_mix_pan_x(int x, float pan);

/* 0.6.0: ec_scr_file.c (FILE's browser and editor, the SAMPLER, the record settings) */
void ec_scr_file_browser(const ec_app_t *a, const ec_page_t *pg);
void ec_scr_file_editor(const ec_app_t *a, const ec_page_t *pg);
void ec_scr_file_pool(const ec_app_t *a, const ec_page_t *pg);   /* 0.6.1: FILE / POOL */
void ec_scr_sampler(const ec_app_t *a);
void ec_scr_projects(const ec_app_t *a);    /* 0.6.42 */
void ec_scr_record(const ec_app_t *a);

/* E6: ec_scr_rec.c (the REC pop-up; the INPUT machine's view on SRC) */
void ec_scr_rec(const ec_app_t *a);
void ec_scr_input(const ec_app_t *a, int top, int bot);

#endif
