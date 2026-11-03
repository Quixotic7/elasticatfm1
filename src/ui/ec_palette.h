/* SPDX-License-Identifier: GPL-3.0-only */
/* The 16-colour palette (docs/UX-V2.md §7; owner 2026-11-03: "use a 16 color palette for the UI to enforce a retro UI
 * and also make it easy to recolor/retheme"). Screens draw 4-bit palette indices (the EC_C_* slots) into a 120x120
 * canvas (ec_gfx.h); one theme table expands it to the LCD's RGB565 at exactly 2x, so a theme is a table swap.
 * host/mock_palette.h (the 0.6.0 mockups) uses these tables too. */
#ifndef EC_PALETTE_H
#define EC_PALETTE_H
#include <stdint.h>

/* the slots: a screen names a colour by its role */
enum {
    EC_C_BG,       /* the ground */
    EC_C_BLACK,
    EC_C_L3,       /* rules, inactive marks */
    EC_C_L5,       /* dim content */
    EC_C_L8,       /* labels */
    EC_C_L12,      /* the header bar, markers */
    EC_C_L15,      /* values, active content */
    EC_C_ACC,      /* the accent: playhead, alerts */
    EC_C_SRC,      /* role: a modulation source key; 0.6.1: each theme's blue (a held source's depth bars) */
    EC_C_SPECIAL,  /* role: the mode's special key */
    EC_C_ACTION,   /* role: an action key */
    EC_C_LOCK,     /* a parameter lock (a step-locked cell fills with it) */
    EC_C_ALERT,    /* errors, STOP FIRST, FULL */
    EC_C_ACC_DIM,  /* the accent behind a pop-up */
    EC_C_SHADE,    /* 0.6.1: black in most themes (= BG); the CAT sprite's ground (a key, not drawn) */
    EC_C_OK,       /* good / armed-ok */
    EC_NPAL
};

/* the themes (docs/UX-V2.md §3 APP SETTINGS: THEME), in the picker's order; CREAM is the default */
enum { EC_TH_CREAM, EC_TH_AMBER, EC_TH_PHOSPHOR, EC_TH_DMG, EC_TH_ICE, EC_TH_PAPER, EC_NTHEME };

extern const char *const ec_pal_names[EC_NPAL];               /* "BG", "BLACK", .. "OK" */
extern const char *const ec_theme_names[EC_NTHEME];           /* "CREAM" .. "PAPER" */
extern const uint32_t ec_themes[EC_NTHEME][EC_NPAL];          /* RGB888 per slot */
extern const uint8_t ec_pal_dim[EC_NPAL];                     /* behind a pop-up: each slot's darker slot */

#endif
