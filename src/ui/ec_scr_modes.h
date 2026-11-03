/* SPDX-License-Identifier: GPL-3.0-only */
/* 0.6.0: the mode screens of src/ui/ec_scr_modes.c (APP SETTINGS, MIX's CAT, PAGES, KEYBOARD, CROSSFADER, PATTERN
 * LAUNCH; the SAMPLER's stand-in). 0.6.1: no mode-entry pop-up. */
#ifndef EC_SCR_MODES_H
#define EC_SCR_MODES_H

#include "ec_app.h"

int ec_scr_modes_full(const ec_app_t *a);       /* 1: a mode screen took the display */

#endif
