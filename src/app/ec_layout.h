/* SPDX-License-Identifier: GPL-3.0-only */
/* The layout: which items each page shows, in which lane, on which knob; the pages' titles, order and pictures; the
 * setup lists, the bus pages, F3. It is data: design/layout/elasticat.json, compiled into src/app/ec_layout_gen.c by
 * tools/layout/gen_layout.py (CMake re-runs it when the JSON changes). ec_pages.c reads it; nothing else should.
 *
 * A page is a list of variants, checked in order; the first whose conditions all hold gives the page's items (and
 * may rename it). A condition is a bit mask over an option param's values (bit k: value k + 1); 0 = any. The last
 * variant of every page has no conditions (the page's own lanes). Items are lanes of four: item i is on lane i / 4,
 * KNOB i % 4 + 1. Bus pages test their conditions on the bus's own record (project.bus), the others on the track. */
#ifndef EC_LAYOUT_H
#define EC_LAYOUT_H
#include <stdint.h>

#include "ec_pages.h"   /* (the pictures: EC_PIC_*) */

typedef struct {
    uint32_t warp, fx;          /* masks over mode 1..13, fx_insert1_machine 1..18 (0: any) */
    uint8_t mach, filt, fenv, env;   /* machine 1..8, filter_machine 1..6, filter_env_mode / env_mode 1..2 */
    const char *title;          /* NULL: the page's title */
    uint16_t item0;
    uint8_t n;                  /* items (the last lane may be short; a blank item is an empty knob) */
} ec_lay_var_t;

typedef struct {
    const char *title;
    uint8_t pic, arg;
    uint16_t var0;
    uint8_t nvar;
} ec_lay_page_t;

typedef struct {
    uint16_t page0;
    uint8_t npages;
    int16_t setup;              /* the setup list (a page), -1: none */
} ec_lay_cat_t;

typedef struct {
    const char *name;
    ec_lay_cat_t cat[EC_NCAT];
    int16_t bus_page[3];        /* S1, S2, MST: the page every category but MST / FILE / PAT shows on that bus */
    int16_t bus_setup[3];       /* .. its setup (the bus's machine) */
    int16_t mst_src;            /* the master bus's SRC (the mix) */
    int16_t f3;                 /* F3's lane */
    const ec_lay_page_t *page;
    const ec_lay_var_t *var;
    const ec_item_t *item;
    uint16_t npage, nvar, nitem;
} ec_layout_t;

extern const ec_layout_t EC_LAYOUT_SHIPPED;           /* src/app/ec_layout_gen.c */

const ec_layout_t *ec_layout(void);
void ec_layout_use(const ec_layout_t *l);             /* NULL: the shipped one (tests: a fixed one; the designer) */

#endif
