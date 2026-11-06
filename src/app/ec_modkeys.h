/* SPDX-License-Identifier: GPL-3.0-only */
/* 0.5.0: the mod matrix on the panel (docs/MOD-MATRIX.md): the MATRIX page (SELECT the row, K1 SRC, K2 DST,
 * K3 DEPTH), the source keys (0.6.0: the source template's B6..B10 and MODULATION's M1..M4, ec_srckeys.c: hold one +
 * turn a value knob = that source onto that param, its depth; F2 while turning removes it), the cells' marks, the
 * live controls (aftertouch, CC 1, CC 11). The state lives in ec_app_t.mk; ec_app.c's input
 * dispatch calls the functions below. */
#ifndef EC_MODKEYS_H
#define EC_MODKEYS_H
#include <stdint.h>

#include "ec_pages.h"

/* the black keys in MOD KEYS: B1..B11 */
enum { EC_MK_LFO1, EC_MK_LFO2, EC_MK_ENV, EC_MK_VEL, EC_MK_AT, EC_MK_M1, EC_MK_M2, EC_MK_M3, EC_MK_M4, EC_MK_SCNA,
       EC_MK_SCNB, EC_MK_N };

typedef struct {
    int held;                    /* the black key held (EC_MK_*), -1 none */
    int used;                    /* a knob turned during the hold (no tap) */
    uint16_t created;            /* slots this hold created (bit k) */
    int hot_id;                  /* the param the hold turned last (-1) */
    int row;                     /* the MATRIX page's selected row 0..7 */
    uint32_t bad_ms;             /* "NOT MODDABLE" / "MATRIX FULL" shown since */
    /* the live controls (0..1): aftertouch on the selected track, CC 1, CC 11 */
    float at, mw, expr;
} ec_mk_t;

int ec_mk_src_of_key(int i);                 /* EC_MK_* -> ec_mtx.h EC_MS_*, 0 for the scene keys */
const char *ec_mk_key_name(int i);           /* "LFO 1" .. "SCN B" */

void ec_mk_hold(int i);                      /* a source key (EC_MK_*) went down */
int ec_mk_release(void);                     /* it came up: 1 = a tap (no knob turned during the hold) */
void ec_mk_off(void);                        /* (a mode change: the hold ends, no tap) */
/* a cell turned while a source key is held: 1 = taken (mapped / refused / cleared) */
int ec_mk_turn(const ec_item_t *it, int role, int det);
/* 0.6.51: a source's own page (LFO 1, LFO 2, MOD ENV, VEL): its EC_MS_* (0 another page); the route shown (its
 * slot, -1 none; *idx of *n); SEL through the routes (0 past the ends), ALGO the route's depth (1 = taken) */
int ec_mk_route_src(void);
int ec_mk_route(const float *v, int src, int *idx, int *n);
int ec_mk_route_select(int d);
int ec_mk_route_depth(int d);
/* SELECT on the MATRIX page: 1 = taken (the row; past the ends the caller steps the page) */
int ec_mk_select(int d);
/* an EC_IT_MTX item -> the selected row's param item (other kinds copied) */
void ec_mk_item(const ec_item_t *in, ec_item_t *out);
/* the matrix DST knob: one destination in the layout's order a click, SHIFT a category */
float ec_mk_dst_step(float x, int det, int shift);
/* a DST value's name: the layout's label of that param (else its own), "OFF" */
void ec_mk_dst_name(float x, char *out, int cap);
/* after a SRC / DST edit of slot k on the selected track: the source's own level up from 0 */
void ec_mk_slot_edited(int k);

/* the screens */
/* the cell's mark: the sources aimed at param id on the selected track (bit EC_MS_*), 0 none */
uint32_t ec_mk_marks(int id);
/* the held source's slot on param id (the cell shows its depth), -1 none */
int ec_mk_hot_slot(int id);

void ec_mk_tick(void);                       /* the live controls -> the engine, the release of a hold */
void ec_mk_labels(void);                     /* a source held: the knobs' labels */

#endif
