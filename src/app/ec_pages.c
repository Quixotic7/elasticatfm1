/* SPDX-License-Identifier: GPL-3.0-only */
/* Categories, pages and cells (ec_pages.h) from the layout (ec_layout.h): the page's variant for the track's (or the
 * bus's) params, its items. What stays code: on a bus every category but MST / FILE / PAT shows the bus's page (the
 * master bus's SRC: the mix), and the setup / F3 titles. */
#include <string.h>

#include "ec_dsp.h"
#include "ec_layout.h"
#include "ec_pages.h"
#include "ec_params.h"

static const char *const CAT[EC_NCAT] = {"TRIG", "SRC", "FILT", "AMP", "FX", "MOD", "MST", "FILE", "PAT"};
const char *ec_cat_name(int cat) { return cat >= 0 && cat < EC_NCAT ? CAT[cat] : "?"; }

static const ec_layout_t *L = &EC_LAYOUT_SHIPPED;
const ec_layout_t *ec_layout(void) { return L; }
void ec_layout_use(const ec_layout_t *l) { L = l ? l : &EC_LAYOUT_SHIPPED; }

static int iv(const float *v, int id) { return (int)(v[id] + 0.5f); }
static int has(uint32_t mask, int value) { return !mask || (value >= 1 && value <= 32 && (mask >> (value - 1) & 1u)); }

static int bus_cat(int cat) { return cat != EC_CAT_MASTER && cat != EC_CAT_FILE && cat != EC_CAT_PAT; }

int ec_cat_pages(int cat, const float *v)
{
    (void)v;
    return cat >= 0 && cat < EC_NCAT && L->cat[cat].npages ? L->cat[cat].npages : 1;
}

static int page_index(int cat, int page)
{
    const int np = ec_cat_pages(cat, 0);
    return L->cat[cat].page0 + (page < 0 ? 0 : page >= np ? np - 1 : page);
}

/* the layout's page p for params v: its title, picture and the first matching variant's items */
static void fill(int p, const float *v, ec_page_t *o)
{
    const ec_lay_page_t *pg;
    int i, k;
    if (p < 0 || p >= L->npage)
        return;
    pg = &L->page[p];
    o->title = pg->title;
    o->pic = pg->pic;
    o->arg = pg->arg;
    if (!v)
        return;
    for (i = 0; i < pg->nvar; i++) {
        const ec_lay_var_t *x = &L->var[pg->var0 + i];
        if (!has(x->mach, iv(v, EC_P_MACHINE)) || !has(x->warp, iv(v, EC_P_MODE)) ||
            !has(x->filt, iv(v, EC_P_FILTER_MACHINE)) || !has(x->fenv, iv(v, EC_P_FENV_MODE)) ||
            !has(x->env, iv(v, EC_P_ENV_MODE)) || !has(x->fx, iv(v, EC_P_FX_MACHINE)))
            continue;
        if (x->title)
            o->title = x->title;
        for (k = 0; k < x->n && o->n < EC_PAGE_ITEMS; k++)
            o->it[o->n++] = L->item[x->item0 + k];
        return;
    }
}

int ec_page_pic(int cat, int page, int bus)
{
    if (cat < 0 || cat >= EC_NCAT)
        return EC_PIC_STEPS;
    if (bus >= 1 && bus <= 3 && bus_cat(cat)) {
        const int p = bus == 3 && cat == EC_CAT_SRC ? L->mst_src : L->bus_page[bus - 1];
        return p >= 0 && p < L->npage ? L->page[p].pic : EC_PIC_FX;
    }
    return L->page[page_index(cat, page)].pic;
}

void ec_page_items_bus(int cat, int page, const float *v, int bus, const float *bv, ec_page_t *o)
{
    memset(o, 0, sizeof *o);
    o->title = "";
    if (cat < 0 || cat >= EC_NCAT)
        return;
    if (bus && bus_cat(cat)) {
        if (bus == 3 && cat == EC_CAT_SRC) {   /* the master bus's SOURCE is the mix overview */
            fill(L->mst_src, v, o);
            return;
        }
        if (bus < 1 || bus > 3)
            return;
        fill(L->bus_page[bus - 1], bv, o);   /* E4: the bus's record decides (its FX machine) */
        o->busv = bus;
        return;
    }
    fill(page_index(cat, page), v, o);
}

void ec_page_items(int cat, int page, const float *v, int bus, ec_page_t *o)
{
    ec_page_items_bus(cat, page, v, bus, 0, o);
}

int ec_settings_items_bus(int cat, int bus, ec_page_t *o)
{
    memset(o, 0, sizeof *o);
    o->title = "SETUP";
    if (!bus || bus > 3 || !bus_cat(cat) || (bus == 3 && cat == EC_CAT_SRC) || L->bus_setup[bus - 1] < 0)
        return 0;
    {
        float v[EC_NP];
        ec_params_default(v);
        fill(L->bus_setup[bus - 1], v, o);
    }
    o->title = "SETUP";
    o->pic = EC_PIC_STEPS;
    o->busv = bus;
    return 1;
}

void ec_page_f3(ec_page_t *o)
{
    float v[EC_NP];
    memset(o, 0, sizeof *o);
    ec_params_default(v);
    fill(L->f3, v, o);
    o->title = "F3";
    o->pic = EC_PIC_STEPS;
}

int ec_settings_items(int cat, const float *v, ec_page_t *o)
{
    memset(o, 0, sizeof *o);
    o->title = "SETUP";
    if (cat < 0 || cat >= EC_NCAT || L->cat[cat].setup < 0)
        return 0;
    fill(L->cat[cat].setup, v, o);
    o->title = "SETUP";
    o->pic = EC_PIC_STEPS;
    return 1;
}
