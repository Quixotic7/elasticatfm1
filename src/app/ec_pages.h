/* SPDX-License-Identifier: GPL-3.0-only */
/* The category / page / cell model (lib/pages/model.lua + the coordinator's page_items_for): which parameters a page
 * shows in its cells, per machine, warp mode, filter machine and envelope mode. KN1-KN4 edit the active lane's four
 * cells (0.3.0: lanes of four, SELECT steps them). Settings layers (FN + category) are the same structure, drawn as a list.
 * Since the layout tool the pages are data: the layout (ec_layout.h, design/layout/elasticat.json). */
#ifndef EC_PAGES_H
#define EC_PAGES_H
#include <stdint.h>

enum { EC_CAT_TRIG, EC_CAT_SRC, EC_CAT_FILT, EC_CAT_AMP, EC_CAT_FX, EC_CAT_MOD, EC_CAT_MASTER, EC_CAT_FILE,
       EC_CAT_PAT, EC_NCAT };

/* item kinds */
enum { EC_IT_BLANK, EC_IT_PARAM, EC_IT_GLOBAL, EC_IT_SLOT, EC_IT_RAZOR /* E3: ec_razor_ui.h */, EC_IT_E5,
       EC_IT_MTX /* 0.5.0: the MATRIX page's row (ec_modkeys.h) */ };
/* 0.5.0: the MATRIX page's cells: the selected row's SRC, DST, DEPTH */
enum { EC_MX_SRC, EC_MX_DST, EC_MX_DEPTH, EC_NMX };
/* E5 items: the crossfader, a slice's choke group, global BPM on / off, the project name */
enum { EC_X_XFD, EC_X_CHOKE, EC_X_GBPM, EC_X_NAME, EC_X_MUTE, EC_NX };
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

/* the pictures a page can draw above its cells (the screens: ec_screens.c, ec_scr_e3/e4/e5.c). FILE, RAZOR, MIX,
 * SCENE and PATTERN pages have behaviour of their own (the file list, the slice editor, the mixer, the crossfader, the
 * pattern grid): the designer keeps them in their category and their picture fixed */
enum {
    EC_PIC_STEPS,          /* the step strip (the category's caption: TRIG the machine / length / rate, FILT the
                              filter machine, AMP the env mode, else the title) */
    EC_PIC_WAVE,           /* the track's waveform, region, loop keys; caption: the sample's name */
    EC_PIC_WAVE_MACHINE,   /*   .. caption: the machine */
    EC_PIC_WAVE_WARP,      /*   .. caption: the warp mode (TAPE in orange when it falls back) */
    EC_PIC_WAVE_RANGE,     /*   .. caption: RANGE */
    EC_PIC_FILTER,         /* the filter curve */
    EC_PIC_FENV,           /* the filter envelope's shape */
    EC_PIC_AENV,           /* the amp envelope's shape */
    EC_PIC_FX,             /* the FX machine's picture (the track's insert, or the bus's) */
    EC_PIC_SENDS,
    EC_PIC_LFO1,
    EC_PIC_LFO2,
    EC_PIC_MENV,
    EC_PIC_VEL,
    EC_PIC_MACROS,
    EC_PIC_MACRO,          /* one macro's matrix: arg 0..3 */
    EC_PIC_MIX,            /* fixed: the mixer (no cells drawn; the knobs still turn them) */
    EC_PIC_SCENE,          /* fixed: the A/B crossfader */
    EC_PIC_FILE,           /* fixed: the file browser (SELECT is its cursor, SAVE loads) */
    EC_PIC_RAZOR,          /* fixed: the razor slice editor (SELECT picks the slice) */
    EC_PIC_PATTERN,        /* fixed: the pattern grid */
    EC_PIC_MATRIX,         /* 0.5.0, fixed in MOD: the mod matrix (SELECT picks the row; K1 SRC, K2 DST, K3 DEPTH) */
    EC_NPIC
};

#define EC_PAGE_LANES 4           /* a page's lanes at most (a setup list: 2, F3: 1) */
#define EC_PAGE_ITEMS (EC_PAGE_LANES * 4)
typedef struct {
    const char *title;            /* "WARP" */
    int n;                        /* cells in use (<= EC_PAGE_ITEMS) */
    ec_item_t it[EC_PAGE_ITEMS];
    int busv;                     /* E4: 1..3 the cells are bus S1 / S2 / master's record (project.bus), 0 the track */
    int pic;                      /* what the screen draws above the cells (ec_layout.h EC_PIC_*) */
    int arg;                      /* the picture's argument (EC_PIC_MACRO: the macro 0..3) */
} ec_page_t;

const char *ec_cat_name(int cat);             /* "SRC" */
int ec_cat_pages(int cat, const float *v);    /* pages of the category for a track's params */
/* the picture of the category's page as shown on target bus (0 a track): the page's own, or the bus page's */
int ec_page_pic(int cat, int page, int bus);
/* the page's cells for a track (v = its params); bus: 0 a track, 1 S1, 2 S2, 3 the master bus; bv = that bus's
 * record (project.bus[bus - 1]) or NULL */
void ec_page_items(int cat, int page, const float *v, int bus, ec_page_t *out);
void ec_page_items_bus(int cat, int page, const float *v, int bus, const float *bv, ec_page_t *out);   /* E4 */
int ec_settings_items_bus(int cat, int bus, ec_page_t *out);                                          /* E4 */
/* 0.3.0: F3's cells: the track's pattern LEN and RATE, the global length, the quantize */
void ec_page_f3(ec_page_t *out);
/* the settings layer (FN + category); returns 0 when the category has none */
int ec_settings_items(int cat, const float *v, ec_page_t *out);

#endif
