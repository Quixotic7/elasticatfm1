/* SPDX-License-Identifier: GPL-3.0-only */
/* Categories (0.6.0: the modes), pages and cells (ec_pages.h) from the layout (ec_layout.h): the page's variant for
 * the track's (or the bus's) params, its items; the modes' names, buttons and black keys. What stays code: on a bus
 * every mode but MIX / FILE / PATTERN LAUNCH / CROSSFADER shows the bus's page (the master bus's LOOP ENGINE: the
 * mix), and the setup / F3 titles. */
#include <string.h>

#include "ec_dsp.h"
#include "ec_layout.h"
#include "ec_pages.h"
#include "ec_params.h"

static const ec_layout_t *L = &EC_LAYOUT_SHIPPED;
const ec_layout_t *ec_layout(void) { return L; }
void ec_layout_use(const ec_layout_t *l) { L = l ? l : &EC_LAYOUT_SHIPPED; }

static int iv(const float *v, int id) { return (int)(v[id] + 0.5f); }
static int has(uint32_t mask, int value) { return !mask || (value >= 1 && value <= 32 && (mask >> (value - 1) & 1u)); }

/* on a bus every mode but MIX, FILE, PATTERN LAUNCH and CROSSFADER shows the bus's page */
static int bus_cat(int cat)
{
    return cat != EC_CAT_MASTER && cat != EC_CAT_FILE && cat != EC_CAT_PAT && cat != EC_CAT_XFD;
}

/* ------------------------------------------------------------------------------------------- the modes --- */
static int is_cat(int cat) { return cat >= 0 && cat < EC_NCAT; }

const char *ec_cat_name(int cat) { return is_cat(cat) && L->cat[cat].name ? L->cat[cat].name : "?"; }
const char *ec_cat_tag(int cat) { return is_cat(cat) && L->cat[cat].tag ? L->cat[cat].tag : ec_cat_name(cat); }

const char *ec_mode_name(int mode)
{
    if (mode >= EC_NCAT && mode < EC_NMODE)
        return L->screen[mode - EC_NCAT].name ? L->screen[mode - EC_NCAT].name : "?";
    return ec_cat_name(mode);
}

int ec_mode_button(int mode)
{
    if (mode >= EC_NCAT && mode < EC_NMODE)
        return L->screen[mode - EC_NCAT].button;
    return is_cat(mode) ? L->cat[mode].button : EC_MB_NONE;
}

int ec_mode_order(int mode)
{
    if (mode >= EC_NCAT && mode < EC_NMODE)
        return L->screen[mode - EC_NCAT].order;
    return is_cat(mode) ? L->cat[mode].order : 0;
}

int ec_button_modes(int button, int *modes, int max)
{
    int n = 0, o, m;
    if (button <= EC_MB_NONE || button >= EC_NMB)
        return 0;
    for (o = 1; o <= EC_NMODE && n < max; o++)          /* (the generator checks: orders 1..n, each once) */
        for (m = 0; m < EC_NMODE && n < max; m++)
            if (ec_mode_button(m) == button && ec_mode_order(m) == o)
                modes[n++] = m;
    return n;
}

int ec_cat_page_key(int cat, int page)
{
    if (!is_cat(cat) || page < 0 || page >= L->cat[cat].npages)
        return 0;
    return L->page[L->cat[cat].page0 + page].key;
}

int ec_cat_key_page(int cat, int bkey)
{
    int p;
    if (!is_cat(cat) || bkey < 1 || bkey > EC_NBKEY)
        return -1;
    for (p = 0; p < L->cat[cat].npages; p++)
        if (L->page[L->cat[cat].page0 + p].key == bkey)
            return p;
    return -1;
}

int ec_cat_key_bus(int cat, int bkey)
{
    int b;
    if (cat != EC_CAT_FX || bkey < 1 || bkey > EC_NBKEY)
        return 0;
    for (b = 0; b < 3; b++)
        if (L->bus_key[b] == bkey)
            return b + 1;
    return 0;
}

int ec_bus_key(int bus) { return bus >= 1 && bus <= 3 ? L->bus_key[bus - 1] : 0; }

int ec_page_find(int pic, int arg, int *cat, int *page)
{
    int c, p;
    for (c = 0; c < EC_NCAT; c++)
        for (p = 0; p < L->cat[c].npages; p++) {
            const ec_lay_page_t *pg = &L->page[L->cat[c].page0 + p];
            if (pg->pic == pic && (arg < 0 || pg->arg == arg)) {
                if (cat)
                    *cat = c;
                if (page)
                    *page = p;
                return 1;
            }
        }
    return 0;
}

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

/* 0.6.3 (docs/UX-V2.md §10 FX): a bus effect's knobs follow the inserts': K1..K3 its other params in threes, K4 its
 * MIX on every lane (fx_wet for the machines without a blend of their own). Applied by the app (ec_app_page, ec_nav)
 * on top of the layout's bus page, so the layout's pages (and host/dump_pages.c's 0.4.0 hash) stay as they are */
void ec_page_bus_k4(ec_page_t *o, int m)
{
    ec_item_t rest[EC_PAGE_ITEMS];
    int i, n = 0, k;
    if (m == EC_FX_NONE)
        return;
    for (i = 0; i < o->n; i++) {
        const ec_item_t *it = &o->it[i];
        if (it->kind == EC_IT_PARAM && (it->id == EC_P_FX_MIX || it->id == EC_P_FX_WET))
            continue;
        rest[n++] = *it;
    }
    n = n > EC_PAGE_LANES * 3 ? EC_PAGE_LANES * 3 : n;
    memset(o->it, 0, sizeof o->it);
    o->n = 0;
    for (k = 0; k == 0 || k < n; k += 3) {
        for (i = 0; i < 3; i++)
            if (k + i < n)
                o->it[o->n + i] = rest[k + i];
        o->it[o->n + 3].kind = EC_IT_PARAM;
        o->it[o->n + 3].id = (uint8_t)ec_fx_mix_id(m);
        o->it[o->n + 3].label = "MIX";
        o->n += 4;
    }
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
