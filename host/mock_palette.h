/* SPDX-License-Identifier: GPL-3.0-only */
/* The 0.6.0 mockups' 16-colour palette (docs/UX-V2.md; owner 2026-11-03: "use a 16 color palette for the UI to enforce
 * a retro UI and also make it easy to recolor/retheme"). Screens are drawn as 4-bit indices into a 120x120 buffer and
 * expanded through one table to RGB565 at 2x, so a theme is a table swap. Mock-only for now; the 0.6 build moves it to
 * src/ui. The current EC_C_* colours (ec_gfx.h) are the slots BG, BLACK, L3, L5, L8, L12, L15 and ACC of the default
 * theme, unchanged. */
#ifndef MOCK_PALETTE_H
#define MOCK_PALETTE_H
#include <stdint.h>

enum {
    PX_BG,       /* the ground                    EC_C_BG */
    PX_BLACK,    /*                               EC_C_BLACK */
    PX_L3,       /* rules, inactive marks         EC_C_L3 */
    PX_L5,       /* dim content                   EC_C_L5 */
    PX_L8,       /* labels                        EC_C_L8 */
    PX_L12,      /* the header bar, markers       EC_C_L12 */
    PX_L15,      /* values, active content        EC_C_L15 */
    PX_ACC,      /* the accent: playhead, alerts  EC_C_ACC */
    PX_SRC,      /* role: a modulation source key */
    PX_SPECIAL,  /* role: the mode's special key */
    PX_ACTION,   /* role: an action key */
    PX_LOCK,     /* a parameter lock (chips) */
    PX_ALERT,    /* errors, STOP FIRST, FULL */
    PX_ACC_DIM,  /* the accent behind a pop-up */
    PX_SHADE,    /* rules behind a pop-up */
    PX_OK,       /* spare: good / armed-ok */
    PX_N
};

static const char *const PX_NAME[PX_N] = {"BG", "BLACK", "L3", "L5", "L8", "L12", "L15", "ACC",
                                          "SRC", "SPECIAL", "ACTION", "LOCK", "ALERT", "ACC_DIM", "SHADE", "OK"};

/* RGB888 per slot, one table per theme (docs/UX-V2.md §3 APP SETTINGS: THEME). CREAM is today's look. Every theme keeps
 * the roles apart: page (L15), source (SRC), special (SPECIAL), action (ACTION), lock (LOCK), alert (ALERT). */
enum { TH_CREAM, TH_AMBER, TH_PHOSPHOR, TH_DMG, TH_ICE, TH_PAPER, TH_N };
static const char *const TH_NAME[TH_N] = {"CREAM", "AMBER", "PHOSPHOR", "DMG", "ICE", "PAPER"};
static const uint32_t PX_THEMES[TH_N][PX_N] = {
    /*  BG        BLACK     L3        L5        L8        L12       L15       ACC
        SRC       SPECIAL   ACTION    LOCK      ALERT     ACC_DIM   SHADE     OK */
    {0x0B0B0A, 0x000000, 0x34332E, 0x5E5C54, 0x9A978B, 0xCFCBBE, 0xF6F3E8, 0xFF6A1F,   /* CREAM (default) */
     0xFF6A1F, 0xFFD23F, 0x6FC3DF, 0xFF6A1F, 0xFF3B30, 0x6B3013, 0x1C1B19, 0x7BD389},
    {0x0D0802, 0x000000, 0x3A2408, 0x6B4210, 0xA8681A, 0xE09A2A, 0xFFC45C, 0xFFF1C9,   /* AMBER: amber mono CRT */
     0xFF5A1A, 0xFFE040, 0x8FC8FF, 0xFFF1C9, 0xFF3030, 0x5A4A30, 0x1E1406, 0xB8E05A},
    {0x020A04, 0x000000, 0x0D3315, 0x1A5E28, 0x2FA044, 0x5AD86E, 0xB6FFC0, 0xF4FF80,   /* PHOSPHOR: green on black */
     0xF4FF80, 0xFFB03C, 0x5AC8FF, 0xF4FF80, 0xFF4A3A, 0x4A5214, 0x06180A, 0x9CFF6E},
    {0x0F380F, 0x081808, 0x2A5A2A, 0x4E7A2A, 0x6E9420, 0x8BAC0F, 0xC4D86A, 0xEAF8CC,   /* DMG: four-green handheld */
     0xEAF8CC, 0xF8E048, 0x50C8A8, 0xEAF8CC, 0xF86040, 0x4C6C30, 0x173F17, 0xA8E040},
    {0x06101F, 0x000000, 0x152C4A, 0x23497A, 0x3F7FB0, 0x7CC4E8, 0xD6F4FF, 0xFF8A3D,   /* ICE: cyan on navy */
     0xFF8A3D, 0xFFE15A, 0xB48CFF, 0xFF8A3D, 0xFF3B5C, 0x5A3220, 0x0B1A2E, 0x7CFFB0},
    {0xF2EEDF, 0x000000, 0xD0CBB8, 0xA8A290, 0x6E6A5C, 0x34332E, 0x0B0B0A, 0xE0480A,   /* PAPER: dark on cream */
     0xE0480A, 0xC89400, 0x1F7FA8, 0xE0480A, 0xC8201A, 0xF0B898, 0xE4DFCD, 0x3A8A3A},
};
#define PX_THEME_DEFAULT PX_THEMES[TH_CREAM]

/* behind a pop-up every slot drops to a darker one (still the 16) */
static const uint8_t PX_DIM[PX_N] = {
    PX_BG, PX_BLACK, PX_SHADE, PX_SHADE, PX_L3, PX_L3, PX_L5, PX_ACC_DIM,
    PX_ACC_DIM, PX_L5, PX_L3, PX_ACC_DIM, PX_ACC_DIM, PX_ACC_DIM, PX_BG, PX_L3,
};

#endif
