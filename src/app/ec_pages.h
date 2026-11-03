/* SPDX-License-Identifier: GPL-3.0-only */
/* The category / page / cell model (lib/pages/model.lua + the coordinator's page_items_for): which parameters a page
 * shows in its cells, per machine, warp mode, filter machine and envelope mode. KN1-KN4 edit the active lane's four
 * cells (0.3.0: lanes of four, SELECT steps them). Settings layers (FN + category) are the same structure, drawn as a list.
 * Since the layout tool the pages are data: the layout (ec_layout.h, design/layout/elasticat.json). */
#ifndef EC_PAGES_H
#define EC_PAGES_H
#include <stdint.h>

/* the categories: each is a mode (0.6.0, docs/UX-V2.md §3) with its pages. The order is stable (append only); a
 * category's display name is its mode's (ec_cat_name) */
enum { EC_CAT_TRIG,     /* TRIG (HOME, second) */
       EC_CAT_SRC,      /* LOOP ENGINE (HOME, first) */
       EC_CAT_FILT,     /* FILTER (SEL) */
       EC_CAT_AMP,      /* AMP (ENV) */
       EC_CAT_FX,       /* FX (FX) */
       EC_CAT_MOD,      /* MODULATION (LFO) */
       EC_CAT_MASTER,   /* MIX (EDIT, first) */
       EC_CAT_FILE,     /* FILE (FILE, first) */
       EC_CAT_PAT,      /* PATTERN LAUNCH (SEQ) */
       EC_CAT_XFD,      /* 0.6.0: CROSSFADER (ARP): the SCENE page */
       EC_NCAT };
/* 0.6.0: the modes = the categories, then the fixed screens (modes without params of their own) */
enum { EC_MODE_KEYBOARD = EC_NCAT,   /* HOME, third */
       EC_MODE_PAGES,                /* EDIT, second */
       EC_MODE_SAMPLER,              /* FILE, second */
       EC_NMODE };
#define EC_NSCREEN (EC_NMODE - EC_NCAT)
/* the FM-1 buttons a mode sits on (by their printed names; FILE is printed SAVE) */
enum { EC_MB_NONE, EC_MB_FX, EC_MB_SEL, EC_MB_ENV, EC_MB_LFO, EC_MB_EDIT, EC_MB_HOME, EC_MB_FILE, EC_MB_ARP, EC_MB_SEQ,
       EC_NMB };
#define EC_NBKEY 11               /* the black keys B1..B11 */

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
    EC_PIC_EDITOR,         /* 0.6.0, fixed in FILE: the sample editor (black keys zoom / scroll, white keys trim) */
    EC_PIC_POOL,           /* 0.6.1, fixed in FILE: the 128 pool slots (SEL the slot; ASSIGN, MOVE, FREE) */
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

const char *ec_cat_name(int cat);             /* the mode's name: "LOOP ENGINE" (the pop-up) */
const char *ec_cat_tag(int cat);              /* the header's short form: "LOOP" ("LOOP/WARP") */
/* 0.6.0, the modes (mode: EC_CAT_* or EC_MODE_*), from the layout */
const char *ec_mode_name(int mode);           /* "LOOP ENGINE", "KEYBOARD" */
int ec_mode_button(int mode);                 /* the button that opens it: EC_MB_* */
int ec_mode_order(int mode);                  /* its place among that button's modes, 1.. (HOME: LOOP ENGINE 1) */
/* the button's modes in order (tap the lit button again: the next); returns how many (<= max) */
int ec_button_modes(int button, int *modes, int max);
/* the black keys: the key (1..11) that opens page `page` of the mode, 0 none; the page key bkey opens, -1 none */
int ec_cat_page_key(int cat, int page);
int ec_cat_key_page(int cat, int bkey);
/* FX's bus keys: the bus (1 S1, 2 S2, 3 MST) black key bkey targets in mode cat (only FX has them), 0 none */
int ec_cat_key_bus(int cat, int bkey);
int ec_bus_key(int bus);                      /* the key that targets bus 1..3 in FX, 0 none */
/* the first page (in EC_CAT_* order) with picture pic (and arg, unless arg < 0): 1 and *cat / *page, or 0. For the
 * source keys' taps (LFO 1 -> MODULATION's LFO 1 page, MOD ENV -> AMP's MOD ENV, AT -> the MATRIX) */
int ec_page_find(int pic, int arg, int *cat, int *page);
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
