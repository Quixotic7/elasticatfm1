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
enum { EC_IT_BLANK, EC_IT_PARAM, EC_IT_GLOBAL, EC_IT_SLOT, EC_IT_RAZOR /* E3: ec_razor_ui.h */ };
/* globals (the project) */
enum { EC_G_BPM, EC_G_SWING, EC_G_GLEN, EC_G_QUANT, EC_G_RTN, EC_NG };
/* E3: razor editor cells (FILE / RAZOR): the selected slice's points + settings, the view */
enum { EC_RZ_START, EC_RZ_END, EC_RZ_PLAY, EC_RZ_ZOOM, EC_RZ_SNAP, EC_RZ_CHOKE, EC_RZ_PITCH, EC_RZ_SENS, EC_NRZ };
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
    int e4;                       /* (unused since E4) */
    int busv;                     /* E4: 1..3 the cells are bus S1 / S2 / master's record (project.bus), 0 the track */
    int kind;                     /* E4: what the screen draws above the cells (EC_PV_*) */
} ec_page_t;

/* E4: page visuals */
enum { EC_PV_NONE, EC_PV_FX, EC_PV_SENDS, EC_PV_LFO1, EC_PV_LFO2, EC_PV_MENV, EC_PV_VEL, EC_PV_MACROS, EC_PV_MACRO };

const char *ec_cat_name(int cat);             /* "SRC" */
int ec_cat_pages(int cat, const float *v);    /* pages of the category for a track's params */
/* the page's cells for a track (v = its params); bus: 0 a track, 1 S1, 2 S2, 3 the master bus; bv = that bus's
 * record (project.bus[bus - 1]) or NULL */
void ec_page_items(int cat, int page, const float *v, int bus, ec_page_t *out);
void ec_page_items_bus(int cat, int page, const float *v, int bus, const float *bv, ec_page_t *out);   /* E4 */
int ec_settings_items_bus(int cat, int bus, ec_page_t *out);                                          /* E4 */
/* the settings layer (FN + category); returns 0 when the category has none */
int ec_settings_items(int cat, const float *v, ec_page_t *out);

#endif
