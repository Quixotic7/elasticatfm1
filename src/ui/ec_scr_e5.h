/* SPDX-License-Identifier: GPL-3.0-only */
/* E5's screens (src/ui/ec_scr_e5.c): MST > SCENE (the A/B fader, its glide keys, the params that morph), the scene
 * strip on any page while the lower keys are the scene row, the PAGE overlay (page select / page loop / pattern
 * rate), the naming screen, toasts (copy / clear / paste, memorize, undo) and the pattern name on the PAT screens. */
#ifndef EC_SCR_E5_H
#define EC_SCR_E5_H

#include "ec_app.h"

int ec_scr_e5_full(const ec_app_t *a);       /* 1: an E5 screen took the whole display */
void ec_scr_e5_overlay(const ec_app_t *a);   /* drawn over every screen */

/* from ec_screens.c */
void ec_screens_header(const ec_app_t *a, const char *title, int page, const char *right);
void ec_screens_cells(const ec_app_t *a, const ec_page_t *pg);

#endif
