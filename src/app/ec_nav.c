/* SPDX-License-Identifier: GPL-3.0-only */
/* 0.6.2: one page list per mode (docs/UX-V2.md §11, "One page list per mode"; "0.6.2 navigation"). Every mode's
 * pages, every knob page included, in one ordered list (ec_nav_list): a layout page's lanes (its cells in fours), FX's
 * INSERT as INS 1 P1, INS 1 P2 .., INS 2 P1 .., the bus pages S1 / S2 / MST (before the first page whose key is above
 * theirs: FX's MIX on B11), PATTERN LAUNCH's banks. SEL walks it both ways and stops at its ends (ec_nav_sel); a page
 * key jumps to its group's first entry, and tapped again inside its group steps through the group, wrapping
 * (ec_nav_key). The mode button tapped again goes to the next group (wrapping, as before). Nothing else moves the
 * page, so SEL and the keys cannot disagree. The state the entries are made of stays where it was: A->page, the lane
 * (ec_ux.lane), A->bus (FX), pn.ins_slot / ins_page (FX INSERT), pn.bank (PATTERN LAUNCH). */
#include <string.h>

#include "ec_app.h"
#include "ec_mtx.h"

#define A (&ec_app)
#define P (&ec_app.pn)

/* on a bus every mode but MIX, FILE, PATTERN LAUNCH and CROSSFADER shows the bus's page (ec_pages.c) */
static int bus_mode(int m) { return m != EC_CAT_MASTER && m != EC_CAT_FILE && m != EC_CAT_PAT && m != EC_CAT_XFD; }

static int page_key(int m, int p)
{
    int k;
    if (m != EC_CAT_FILE)
        return ec_cat_page_key(m, p);
    for (k = 1; k <= EC_NBKEY; k++)            /* FILE: its own key map (a layout without keys: B3 / B4) */
        if (ec_file_key_page(k) == p)
            return k;
    return 0;
}

static int lanes_of(const ec_page_t *pg) { return ec_ux_lanes(pg->n); }

static int add(ec_nav_t *o, int n, int max, int page, int sub, int slot, int bus, int key, int rows)
{
    if (n >= max)
        return n;
    o[n].page = (int8_t)page;
    o[n].sub = (int8_t)sub;
    o[n].slot = (int8_t)slot;
    o[n].bus = (int8_t)bus;
    o[n].key = (int8_t)key;
    o[n].rows = (int8_t)rows;
    return n + 1;
}

static int add_bus(ec_nav_t *o, int n, int max, int m, int bus)
{
    ec_page_t pg;
    int k, nl;
    ec_page_items_bus(m, 0, ec_app_params(A), bus, A->proj.bus[bus - 1], &pg);
    nl = lanes_of(&pg);
    for (k = 0; k < nl; k++)
        n = add(o, n, max, 0, k, 0, bus, m == EC_CAT_FX ? ec_bus_key(bus) : 0, 0);
    return n;
}

int ec_nav_list(int m, ec_nav_t *o, int max)
{
    const float *v = ec_app_params(A);
    int n = 0, p, k, b, np;
    int bus_done[4] = {0, 0, 0, 0};
    if (m >= EC_NCAT || m == EC_CAT_XFD)       /* the fixed screens, CROSSFADER: one page */
        return add(o, n, max, 0, 0, 0, 0, 0, 0);
    if (m == EC_CAT_PAT) {                     /* PATTERN LAUNCH: the banks, B1..B4 */
        for (b = 0; b < EC_NBANK; b++)
            n = add(o, n, max, 0, b, 0, 0, b + 1 <= EC_NBKEY ? b + 1 : 0, 0);
        return n;
    }
    if (A->bus && bus_mode(m) && m != EC_CAT_FX)   /* a bus is the target: its page (its lanes) whatever the page */
        return add_bus(o, n, max, m, A->bus);
    np = ec_cat_pages(m, v);
    for (p = 0; p < np; p++) {
        const int key = page_key(m, p);
        const int pic = ec_page_pic(m, p, 0);
        ec_page_t pg;
        if (m == EC_CAT_FX)                    /* FX: the buses before the first page keyed above theirs */
            for (b = 1; b <= 3; b++)
                if (!bus_done[b] && ec_bus_key(b) > 0 && key > ec_bus_key(b)) {
                    n = add_bus(o, n, max, m, b);
                    bus_done[b] = 1;
                }
        if (m == EC_CAT_FX && pic == EC_PIC_FX) {   /* INSERT: INS 1 P1 .., INS 2 P1 .. */
            int s;
            for (s = 0; s < 2; s++)
                for (k = 0; k < ec_src_ins_pages_of(s); k++)
                    n = add(o, n, max, p, k, s, 0, key, 0);
            continue;
        }
        if (pic == EC_PIC_FILE || pic == EC_PIC_POOL) {   /* lists: SEL scrolls their rows (a wall) */
            n = add(o, n, max, p, 0, 0, 0, key, EC_NAV_ROWS);
            continue;
        }
        if (pic == EC_PIC_MATRIX) {            /* the MATRIX: SEL its rows, then on past either end */
            n = add(o, n, max, p, 0, 0, 0, key, EC_NAV_ROWS_THRU);
            continue;
        }
        ec_page_items(m, p, v, 0, &pg);
        for (k = 0; k < lanes_of(&pg); k++)
            n = add(o, n, max, p, k, 0, 0, key, 0);
    }
    if (m == EC_CAT_FX)
        for (b = 1; b <= 3; b++)
            if (!bus_done[b])
                n = add_bus(o, n, max, m, b);
    return n;
}

/* the entry the state shows, -1 none */
static int find(int m, const ec_nav_t *o, int n)
{
    int i, lane = 0;
    if (m >= EC_NCAT || m == EC_CAT_XFD)
        return 0;
    if (m == EC_CAT_PAT) {
        for (i = 0; i < n; i++)
            if (o[i].sub == P->bank)
                return i;
        return -1;
    }
    {
        ec_page_t pg;
        ec_app_page(A, &pg);
        lane = ec_app_lane(A, &pg);
    }
    if (A->bus && bus_mode(m)) {
        for (i = 0; i < n; i++)
            if (o[i].bus == A->bus && o[i].sub == lane)
                return i;
        return -1;
    }
    for (i = 0; i < n; i++) {
        if (o[i].bus || o[i].page != A->page[m])
            continue;
        if (m == EC_CAT_FX && ec_page_pic(m, o[i].page, 0) == EC_PIC_FX) {
            if (o[i].slot == P->ins_slot && o[i].sub == P->ins_page)
                return i;
            continue;
        }
        if (o[i].rows || o[i].sub == lane)
            return i;
    }
    return -1;
}

int ec_nav_at(int m, ec_nav_t *o, int max, int *n)
{
    *n = ec_nav_list(m, o, max);
    return find(m, o, *n);
}

void ec_nav_go(int m, const ec_nav_t *e, int dir)
{
    if (m >= EC_NCAT || m == EC_CAT_XFD)
        return;
    if (m == EC_CAT_PAT) {
        P->bank = e->sub;
        A->redraw = 1;
        return;
    }
    if (A->bus && bus_mode(m) && m != EC_CAT_FX) {   /* a bus target: only its lanes */
        A->ux.lane[m] = e->sub;
        A->redraw = 1;
        return;
    }
    if (m == EC_CAT_FX)
        A->bus = e->bus;
    ec_app_set_page(m, e->page);
    A->ux.lane[m] = e->sub;
    if (m == EC_CAT_FX && !e->bus && ec_page_pic(m, e->page, 0) == EC_PIC_FX) {
        A->ux.lane[m] = 0;
        P->ins_slot = e->slot;
        P->ins_page = e->sub;
    }
    if (dir && e->rows == EC_NAV_ROWS_THRU && ec_page_pic(m, e->page, 0) == EC_PIC_MATRIX)
        A->mk.row = dir < 0 ? EC_MTX_SLOTS - 1 : 0;   /* SEL onto the MATRIX: its near row (backwards: the last) */
    else if (e->rows == EC_NAV_ROWS_THRU && A->mk.row < 0)
        A->mk.row = 0;
    if (m == EC_CAT_FILE)
        P->pool_carry = -1;
    A->redraw = 1;
}

int ec_nav_sel(int d)
{
    const int m = ec_app_mode();
    ec_nav_t o[EC_NAV_MAX];
    int n, i = ec_nav_at(m, o, EC_NAV_MAX, &n), moved = 0;
    const int s = d > 0 ? 1 : -1;
    if (n <= 0)
        return 0;
    if (i < 0)
        i = 0;
    for (; d; d -= s) {
        if (i + s < 0 || i + s >= n)           /* the ends: no wrap */
            break;
        i += s;
        moved = 1;
        if (o[i].rows)                         /* a list stops the turn on it */
            break;
    }
    if (moved)
        ec_nav_go(m, &o[i], s);
    return moved;
}

int ec_nav_key_down(int m, int key)
{
    ec_nav_t o[EC_NAV_MAX];
    int n, i = ec_nav_at(m, o, EC_NAV_MAX, &n), j;
    if (key < 1)
        return 0;
    if (i >= 0 && o[i].key == key)
        return 2;                              /* in its group already: the tap steps (ec_nav_key_tap) */
    for (j = 0; j < n; j++)
        if (o[j].key == key) {
            ec_nav_go(m, &o[j], 0);
            return 1;
        }
    return 0;
}

int ec_nav_key_tap(int m, int key)
{
    ec_nav_t o[EC_NAV_MAX];
    int n, i = ec_nav_at(m, o, EC_NAV_MAX, &n), j;
    if (key < 1 || i < 0 || o[i].key != key)
        return 0;
    for (j = i + 1; j < n && o[j].key != key; j++)
        ;
    if (j >= n)                                /* past the group's last: its first */
        for (j = 0; j < n && o[j].key != key; j++)
            ;
    if (j == i)
        return 0;
    ec_nav_go(m, &o[j], 0);
    return 1;
}

int ec_nav_key(int m, int key)
{
    const int r = ec_nav_key_down(m, key);
    return r == 2 ? ec_nav_key_tap(m, key) : r;
}

int ec_nav_next_group(int m)
{
    ec_nav_t o[EC_NAV_MAX];
    int n, i, j;
    if (m >= EC_NCAT || m == EC_CAT_XFD || m == EC_CAT_PAT)
        return 0;
    i = ec_nav_at(m, o, EC_NAV_MAX, &n);
    if (n <= 1)
        return 0;
    if (i < 0)
        i = 0;
    for (j = i + 1; j < n && o[j].page == o[i].page && o[j].bus == o[i].bus; j++)
        ;
    ec_nav_go(m, &o[j < n ? j : 0], 0);
    return 1;
}

int ec_nav_place(int *k, int *count)
{
    const int m = ec_app_mode();
    ec_nav_t o[EC_NAV_MAX];
    int n, i = ec_nav_at(m, o, EC_NAV_MAX, &n), j, c = 0, at = 0;
    if (i < 0)
        return 0;
    for (j = 0; j < n; j++)                    /* the entries of the same page (and slot / bus): P1, P2 .. */
        if (o[j].page == o[i].page && o[j].bus == o[i].bus && o[j].slot == o[i].slot && o[j].key == o[i].key) {
            if (j == i)
                at = c;
            c++;
        }
    if (m == EC_CAT_PAT)
        c = 1;
    *k = at;
    *count = c;
    return 1;
}
