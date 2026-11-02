/* SPDX-License-Identifier: GPL-3.0-only */
/* The category / page / cell model (lib/pages/model.lua + the coordinator's page_items_for): which parameters a page
 * shows in its cells, per machine, warp mode, filter machine and envelope mode. KN1-KN4 edit cells 1-4, with FN
 * held cells 5-8. Settings layers (FN + category) are the same structure, drawn as a list. */
#ifndef EC_PAGES_H
#define EC_PAGES_H
#include <stdint.h>

enum { EC_CAT_TRIG, EC_CAT_SRC, EC_CAT_FILT, EC_CAT_AMP, EC_CAT_FX, EC_CAT_MOD, EC_CAT_MASTER, EC_CAT_FILE,
       EC_CAT_PAT, EC_NCAT };

/* item kinds */
enum { EC_IT_BLANK, EC_IT_PARAM, EC_IT_GLOBAL, EC_IT_SLOT, EC_IT_E5 };
/* E5 items: the crossfader, a slice's choke group, global BPM on / off, the project name */
enum { EC_X_XFD, EC_X_CHOKE, EC_X_GBPM, EC_X_NAME, EC_X_MUTE, EC_NX };
/* globals (the project) */
enum { EC_G_BPM, EC_G_SWING, EC_G_GLEN, EC_G_QUANT, EC_G_RTN, EC_NG };
/* slot fields (the current track's pool slot) */
enum { EC_S_BPM, EC_S_STEPS, EC_S_GAIN, EC_S_TRIM_START, EC_S_TRIM_END, EC_NS };

#define EC_IF_STOPPED 1u          /* changes only while the transport is stopped (warp mode, filter machine) */

typedef struct {
    uint8_t kind, id, flags;
    const char *label;            /* NULL: the param's own label */
} ec_item_t;

#define EC_PAGE_ITEMS 8
typedef struct {
    const char *title;            /* "WARP" */
    int n;                        /* cells in use (<= 8) */
    ec_item_t it[EC_PAGE_ITEMS];
    int e4;                       /* 1: a placeholder (its engine half lands in E4) */
} ec_page_t;

const char *ec_cat_name(int cat);             /* "SRC" */
int ec_cat_pages(int cat, const float *v);    /* pages of the category for a track's params */
/* the page's cells for a track (v = its params); bus: 0 a track, 1 S1, 2 S2, 3 the master bus */
void ec_page_items(int cat, int page, const float *v, int bus, ec_page_t *out);
/* the settings layer (FN + category); returns 0 when the category has none */
int ec_settings_items(int cat, const float *v, ec_page_t *out);

#endif
