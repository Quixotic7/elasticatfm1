/* SPDX-License-Identifier: GPL-3.0-only */
/* The palette's tables (ec_palette.h). Every theme keeps the roles apart: page (L15), source (SRC), special (SPECIAL),
 * action (ACTION), lock (LOCK), alert (ALERT). 0.6.1 (owner, docs/UX-V2.md §10): SRC is each theme's blue (a held mod key's
 * header and depth bars), kept clear of ACC, LOCK and ALERT (host/test_grid.c checks the distance).
 * 0.6.1 (owner): pure black. The FM-1's IPS LCD shows a near-black as grey, so every colour whose R, G and B are all
 * <= 0x20 is 0x000000 (BG and SHADE in CREAM / AMBER / PHOSPHOR / ICE, DMG's BLACK, PAPER's L15; ICE's navy SHADE too);
 * host/test_grid.c checks that no slot has all three channels in 1..0x20. SHADE is then BG in those themes: the CAT's
 * ground uses it as a key (not drawn), and the pop-up dims L3 / L5 to L3, not SHADE, so its rules still show. */
#include "ec_palette.h"

const char *const ec_pal_names[EC_NPAL] = {"BG",  "BLACK",   "L3",     "L5",   "L8",    "L12",     "L15",   "ACC",
                                           "SRC", "SPECIAL", "ACTION", "LOCK", "ALERT", "ACC_DIM", "SHADE", "OK"};

const char *const ec_theme_names[EC_NTHEME] = {"CREAM", "AMBER", "PHOSPHOR", "DMG", "ICE", "PAPER"};

const uint32_t ec_themes[EC_NTHEME][EC_NPAL] = {
    /*  BG        BLACK     L3        L5        L8        L12       L15       ACC
        SRC       SPECIAL   ACTION    LOCK      ALERT     ACC_DIM   SHADE     OK */
    {0x000000, 0x000000, 0x34332E, 0x5E5C54, 0x9A978B, 0xCFCBBE, 0xF6F3E8, 0xFF6A1F,   /* CREAM (default) */
     0x3F8CFF, 0xFFD23F, 0x6FC3DF, 0xFF6A1F, 0xFF3B30, 0x6B3013, 0x000000, 0x7BD389},
    {0x000000, 0x000000, 0x3A2408, 0x6B4210, 0xA8681A, 0xE09A2A, 0xFFC45C, 0xFFF1C9,   /* AMBER: amber mono CRT */
     0x5A96FF, 0xFFE040, 0x8FC8FF, 0xFFF1C9, 0xFF3030, 0x5A4A30, 0x000000, 0xB8E05A},
    {0x000000, 0x000000, 0x0D3315, 0x1A5E28, 0x2FA044, 0x5AD86E, 0xB6FFC0, 0xF4FF80,   /* PHOSPHOR: green on black */
     0x4A86FF, 0xFFB03C, 0x5AC8FF, 0xF4FF80, 0xFF4A3A, 0x4A5214, 0x000000, 0x9CFF6E},
    {0x0F380F, 0x000000, 0x2A5A2A, 0x4E7A2A, 0x6E9420, 0x8BAC0F, 0xC4D86A, 0xEAF8CC,   /* DMG: four-green handheld */
     0x5890E8, 0xF8E048, 0x50C8A8, 0xEAF8CC, 0xF86040, 0x4C6C30, 0x173F17, 0xA8E040},
    {0x000000, 0x000000, 0x152C4A, 0x23497A, 0x3F7FB0, 0x7CC4E8, 0xD6F4FF, 0xFF8A3D,   /* ICE: cyan on navy */
     0x3C64FF, 0xFFE15A, 0xB48CFF, 0xFF8A3D, 0xFF3B5C, 0x5A3220, 0x000000, 0x7CFFB0},
    {0xF2EEDF, 0x000000, 0xD0CBB8, 0xA8A290, 0x6E6A5C, 0x34332E, 0x000000, 0xE0480A,   /* PAPER: dark on cream */
     0x1F4FD0, 0xC89400, 0x1F7FA8, 0x9A2E08, 0xC8201A, 0xF0B898, 0xE4DFCD, 0x3A8A3A},
};

const uint8_t ec_pal_dim[EC_NPAL] = {
    EC_C_BG,      EC_C_BLACK, EC_C_L3,      EC_C_L3,   EC_C_L3,      EC_C_L3,      EC_C_L5, EC_C_ACC_DIM,
    EC_C_ACC_DIM, EC_C_L5,    EC_C_L3,      EC_C_ACC_DIM, EC_C_ACC_DIM, EC_C_ACC_DIM, EC_C_BG, EC_C_L3,
};
