/* SPDX-License-Identifier: GPL-3.0-only */
/* 0.3.0's screens (src/ui/ec_scr_ux.c): F3 (tempo, swing, the pattern's length and rate, the pages), the held-step
 * frame, the knob takeover (a value big for a second; PRESETS' sample name), SHIFT's map, the pop-up arrows. */
#ifndef EC_SCR_UX_H
#define EC_SCR_UX_H

#include "ec_app.h"

int ec_scr_ux_full(const ec_app_t *a);       /* 1: F3 took the whole display */
void ec_scr_ux_overlay(const ec_app_t *a);   /* drawn over every screen, last */

#endif
