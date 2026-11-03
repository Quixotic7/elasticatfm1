/* SPDX-License-Identifier: GPL-3.0-only */
/* The 0.6.0 mockups' names for the firmware's 16-colour palette (src/ui/ec_palette.h holds the slots, the six themes
 * and the pop-up dim map; this header only gives them the mockups' short names). */
#ifndef MOCK_PALETTE_H
#define MOCK_PALETTE_H
#include <stdint.h>

#include "ec_palette.h"

enum {
    PX_BG = EC_C_BG, PX_BLACK = EC_C_BLACK, PX_L3 = EC_C_L3, PX_L5 = EC_C_L5, PX_L8 = EC_C_L8, PX_L12 = EC_C_L12,
    PX_L15 = EC_C_L15, PX_ACC = EC_C_ACC, PX_SRC = EC_C_SRC, PX_SPECIAL = EC_C_SPECIAL, PX_ACTION = EC_C_ACTION,
    PX_LOCK = EC_C_LOCK, PX_ALERT = EC_C_ALERT, PX_ACC_DIM = EC_C_ACC_DIM, PX_SHADE = EC_C_SHADE, PX_OK = EC_C_OK,
    PX_N = EC_NPAL
};
enum { TH_CREAM = EC_TH_CREAM, TH_AMBER, TH_PHOSPHOR, TH_DMG, TH_ICE, TH_PAPER, TH_N = EC_NTHEME };

#define PX_NAME ec_pal_names
#define TH_NAME ec_theme_names
#define PX_THEMES ec_themes
#define PX_DIM ec_pal_dim
#define PX_THEME_DEFAULT PX_THEMES[TH_CREAM]

#endif
