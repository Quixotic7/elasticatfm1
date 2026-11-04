/* SPDX-License-Identifier: GPL-3.0-only */
/* test_nav: 0.6.2, one page list per mode (docs/UX-V2.md §11 "One page list per mode", "0.6.2 navigation"), through
 * the panel, for every mode:
 *   SEL walks the mode's list (ec_nav_list) forward to its end, one entry a detent, and stops there (no wrap); then
 *   back to its start, and stops there. Lists (BROWSE, POOL) keep SEL for their rows: SEL leaves the page unchanged
 *   there (the walk steps over them); the MATRIX walks its rows, then on.
 *   Every black key that turns a page (the roles 'P' and the sources 'S') is tapped from every entry: wherever it
 *   lands is in the list; a page key jumps to its group's first entry, and tapped again steps through exactly its
 *   group's entries (the same ones SEL passes), wrapping. Every entry is reached by some key, except the exemptions
 *   (MIX: its black keys are the rates, its page follows the target; FILE's EDITOR lanes: its black keys are the zoom).
 *   After a key jump SEL goes on from that page both ways.
 *   FX with two inserts of two knob pages each and a bus with two lanes; the header's place (INS 2 · P2).
 *   A bus target in FILTER: SEL walks the bus page's lanes only.
 *   ec_test_nav <folder> */
#include "ec_harness.h"

static ec_nav_t L_[EC_NAV_MAX];
static int nL;

static int at(void) { return ec_nav_at(mode(), L_, EC_NAV_MAX, &nL); }
static void list(void) { nL = ec_nav_list(mode(), L_, EC_NAV_MAX); }

/* entry i's name: the page's title (P2 .. when its page has several) */
static const char *name_of(int i)
{
    static char b[40];
    ec_page_t pg;
    int k, c;
    if (mode() == EC_CAT_PAT) {
        snprintf(b, sizeof b, "BANK %c", 'A' + L_[i].sub);
        return b;
    }
    ec_app_page(A, &pg);
    if (ec_nav_place(&k, &c) && c > 1)
        snprintf(b, sizeof b, "%s P%d", pg.title ? pg.title : "?", k + 1);
    else
        snprintf(b, sizeof b, "%s", pg.title ? pg.title : "?");
    return b;
}

static void go(int i)
{
    list();
    ec_nav_go(mode(), &L_[i], 0);
    run(5);
}

static void sel(int d) { turn(FM1P_E_SELECT, d); }

/* SEL one way until the entry changes (the MATRIX: its rows first): the entry, or -1 when it never did */
static int sel_leave(int d)
{
    const int i0 = at();
    int n;
    for (n = 0; n < 12; n++) {
        sel(d);
        if (at() != i0)
            return at();
    }
    return -1;
}

/* 0.6.15: FILE's editor pages fence SEL in: at their page's edge it stays */
static int fenced(const char *mn, int i, int dir)
{
    int pic;
    if (strcmp(mn, "FILE") || i + dir < 0 || i + dir >= nL || L_[i + dir].page == L_[i].page)
        return 0;
    pic = ec_page_pic(EC_CAT_FILE, L_[i].page, 0);
    return pic == EC_PIC_EDITOR || pic == EC_PIC_RAZOR;
}

static void walk(const char *mn)
{
    char line[1024];
    int i, l = 0;
    list();
    for (i = 0; i < nL; i++) {
        go(i);
        CHECK(at() == i, "%s: entry %d is found where it was put (%d)", mn, i, at());
        l += snprintf(line + l, sizeof line - (size_t)l, "%s%s", i ? ", " : "", name_of(i));
        if (L_[i].key)
            l += snprintf(line + l, sizeof line - (size_t)l, " [B%d]", L_[i].key);
        if (L_[i].rows == EC_NAV_ROWS)
            l += snprintf(line + l, sizeof line - (size_t)l, " (list)");
    }
    printf("  %-14s %s\n", mn, line);
    /* forward to the end */
    go(0);
    for (i = 0; i + 1 < nL; i++) {
        if (L_[i].rows == EC_NAV_ROWS) {               /* a list: SEL scrolls its rows, the page stays */
            sel(1);
            CHECK(at() == i, "%s: SEL on the list %d keeps the page (%d)", mn, i, at());
            go(i + 1);
            continue;
        }
        if (fenced(mn, i, 1)) {
            CHECK(sel_leave(1) < 0 && at() == i, "%s: SEL + from %d stays on its editor page (%d)", mn, i, at());
            go(i + 1);
            continue;
        }
        CHECK(sel_leave(1) == i + 1, "%s: SEL + from %d: %d (want %d)", mn, i, at(), i + 1);
        if (at() != i + 1)
            go(i + 1);
    }
    if (L_[nL - 1].rows != EC_NAV_ROWS) {
        sel(1);
        sel(1);
        CHECK(at() == nL - 1, "%s: SEL + at the end stays (%d of %d)", mn, at(), nL);
    }
    /* back to the start */
    go(nL - 1);
    for (i = nL - 1; i > 0; i--) {
        if (L_[i].rows == EC_NAV_ROWS) {
            sel(-1);
            CHECK(at() == i, "%s: SEL - on the list %d keeps the page (%d)", mn, i, at());
            go(i - 1);
            continue;
        }
        if (fenced(mn, i, -1)) {
            CHECK(sel_leave(-1) < 0 && at() == i, "%s: SEL - from %d stays on its editor page (%d)", mn, i, at());
            go(i - 1);
            continue;
        }
        CHECK(sel_leave(-1) == i - 1, "%s: SEL - from %d: %d (want %d)", mn, i, at(), i - 1);
        if (at() != i - 1)
            go(i - 1);
    }
    if (L_[0].rows != EC_NAV_ROWS) {
        sel(-1);
        sel(-1);
        CHECK(at() == 0, "%s: SEL - at the start stays (%d)", mn, at());
    }
}

/* the keys that turn pages here: the roles 'P' (page) and 'S' (a source's tap opens its page) */
static int page_keys(int *keys)
{
    int cur, k, n = 0;
    const char *r = ec_pn_roles(mode(), &cur);
    for (k = 1; k <= EC_NBKEY; k++)
        if (r[k - 1] == 'P' || r[k - 1] == 'S')
            keys[n++] = k;
    return n;
}

static void keys(const char *mn, int exempt_mix, int exempt_editor)
{
    int reached[EC_NAV_MAX], i, j, kk, nk, ks[EC_NBKEY];
    memset(reached, 0, sizeof reached);
    list();
    reached[0] = 1;                                    /* (the page the mode opens on is reachable as it is) */
    for (i = 0; i < nL; i++) {
        go(i);
        nk = page_keys(ks);
        for (kk = 0; kk < nk; kk++) {
            const int k = ks[kk];
            int a;
            go(i);
            B(k, 40);
            a = at();
            CHECK(a >= 0, "%s: B%d from %d lands on a page of the list (%d)", mn, k, i, a);
            if (a < 0)
                continue;
            reached[a] = 1;
            if (L_[a].key == k && L_[i].key != k) {    /* a jump into its group: the group's first */
                for (j = 0; j < nL && L_[j].key != k; j++)
                    ;
                CHECK(a == j, "%s: B%d from %d: its group's first page %d (%d)", mn, k, i, j, a);
            }
        }
    }
    /* each page key tapped again steps through its group (the same entries SEL passes), wrapping */
    for (kk = 1; kk <= EC_NBKEY; kk++) {
        int first = -1, cnt = 0, n, cur;
        for (j = 0; j < nL; j++)
            if (L_[j].key == kk) {
                cnt++;
                if (first < 0)
                    first = j;
            }
        if (first < 0)
            continue;
        go(first);
        if (ec_pn_roles(mode(), &cur)[kk - 1] != 'P' && ec_pn_roles(mode(), &cur)[kk - 1] != 'S')
            continue;
        if (exempt_editor && (ec_file_page() == EC_PIC_EDITOR || ec_file_page() == EC_PIC_RAZOR))   /* (the editors: no page keys) */
            continue;
        for (n = 0, j = first; n < cnt; n++) {
            int nx;
            B(kk, 40);
            for (nx = j + 1; nx < nL && L_[nx].key != kk; nx++)
                ;
            if (nx >= nL)
                nx = first;
            if (cnt > 1 && L_[first].rows == 0) {
                CHECK(at() == nx, "%s: B%d tapped in its group: %d -> %d (%d)", mn, kk, j, nx, at());
                reached[nx] = 1;
            }
            j = at() >= 0 ? at() : nx;
        }
        /* after the jump SEL goes on from there, both ways */
        go(first == nL - 1 ? 0 : nL - 1);
        B(kk, 40);
        if (at() == first && L_[first].rows != EC_NAV_ROWS) {
            if (first + 1 < nL)
                CHECK(sel_leave(1) == first + 1, "%s: B%d then SEL +: %d (want %d)", mn, kk, at(), first + 1);
            go(first == nL - 1 ? 0 : nL - 1);
            B(kk, 40);
            if (first > 0)
                CHECK(sel_leave(-1) == first - 1, "%s: B%d then SEL -: %d (want %d)", mn, kk, at(), first - 1);
        }
    }
    for (i = 0; i < nL; i++) {
        int ex = 0;
        go(i);
        ex = exempt_mix || (exempt_editor && (ec_file_page() == EC_PIC_EDITOR || ec_file_page() == EC_PIC_RAZOR));   /* (0.6.15: RAZOR too: FILE's button) */
        CHECK(reached[i] || ex, "%s: entry %d (%s) is reached by a key", mn, i, name_of(i));
    }
}

static void fx_setup(void)
{
    float *v = ec_app_params(A);
    v[EC_P_FX_MACHINE] = EC_FX_BLACKHOLE;              /* six params: P1, P2 */
    v[EC_P_INS2_MACHINE] = EC_FX_BLACKHOLE;
    ec_ins2_reset(v);
    A->proj.bus[0][EC_P_FX_MACHINE] = EC_FX_BLACKHOLE;
    A->proj.bus[1][EC_P_FX_MACHINE] = EC_FX_NONE;
    A->proj.bus[2][EC_P_FX_MACHINE] = EC_FX_COMP;     /* (five params: P1, P2) */
}

/* 0.6.3 (docs/UX-V2.md §10 "FX layout"): INSERT 1 P1.., INSERT 2 P1.., SEND 1 P1.., SEND 2 P1.., SENDS, MASTER P1..,
 * FX MIX; B1-B4 the EFFECTS rows (tapped again: their knob pages), B5 SENDS / MASTER, the header's place */
static void test_fx_header(void)
{
    static const struct {
        int slot, bus, sub, key;
        const char *title;
    } W[] = {{0, 0, 0, 1, "INSERT 1"}, {0, 0, 1, 1, "INSERT 1"}, {1, 0, 0, 2, "INSERT 2"}, {1, 0, 1, 2, "INSERT 2"},
             {2, 1, 0, 3, "SEND 1"},   {2, 1, 1, 3, "SEND 1"},   {3, 2, 0, 4, "SEND 2"},   {0, 0, 0, 5, "SENDS"},
             {4, 3, 0, 5, "MASTER"},   {4, 3, 1, 5, "MASTER"},   {0, 0, 0, 11, "MIX"}};
    ec_page_t pg;
    int i, k, c, ok = 1;
    to(EC_CAT_FX);
    list();
    CHECK(nL == (int)(sizeof W / sizeof W[0]), "FX: %d entries", nL);
    for (i = 0; i < nL && i < (int)(sizeof W / sizeof W[0]); i++) {
        go(i);
        ec_app_page(A, &pg);
        ok &= L_[i].slot == W[i].slot && L_[i].bus == W[i].bus && L_[i].sub == W[i].sub && L_[i].key == W[i].key &&
              !strcmp(pg.title, W[i].title) && at() == i;
        if (!ok) {
            printf("  entry %d: slot %d bus %d sub %d key %d %s\n", i, L_[i].slot, L_[i].bus, L_[i].sub, L_[i].key,
                   pg.title);
            break;
        }
    }
    CHECK(ok, "FX: the list INSERT 1 P1 P2, INSERT 2 P1 P2, SEND 1 P1 P2, SEND 2, SENDS, MASTER P1 P2, MIX");
    go(0);
    B(1, 40);
    CHECK(at() == 1 && A->pn.ins_page == 1, "FX B1 on INSERT 1 P1: P2 (%d)", at());
    go(nL - 1);
    B(2, 40);
    B(2, 40);
    ec_app_page(A, &pg);
    CHECK(ec_fx_slot() == 1 && A->pn.ins_page == 1 && !strcmp(pg.title, "INSERT 2") && ec_nav_place(&k, &c) && k == 1 &&
              c == 2,
          "B2 from FX MIX, then a tap: INSERT 2 P2 (%s P%d/%d)", pg.title, k + 1, c);
    sel(-1);
    CHECK(ec_fx_slot() == 1 && A->pn.ins_page == 0, "SEL back: INSERT 2 P1");
    sel(-1);
    CHECK(ec_fx_slot() == 0 && A->pn.ins_page == 1, "SEL back: INSERT 1 P2");
    B(3, 40);
    CHECK(A->bus == 1 && ec_fx_slot() == 2 && A->pn.ins_page == 0, "B3: SEND 1 (bus %d)", A->bus);
    B(4, 40);
    CHECK(A->bus == 2 && ec_fx_slot() == 3, "B4: SEND 2 (bus %d)", A->bus);
    B(5, 40);
    ec_app_page(A, &pg);
    CHECK(!A->bus && !strcmp(pg.title, "SENDS"), "B5: SENDS (%s)", pg.title);
    B(5, 40);
    ec_app_page(A, &pg);
    CHECK(A->bus == 3 && !strcmp(pg.title, "MASTER"), "B5 again: MASTER (%s)", pg.title);
    sel(-1);
    CHECK(!A->bus && A->page[EC_CAT_FX] == ec_fx_sends_page(), "SEL back: SENDS, the track again");
    /* the FX button tapped again: the next group (INSERT 1 -> INSERT 2 -> SEND 1) */
    go(0);
    press(FM1P_B_FX, 40);
    CHECK(ec_fx_slot() == 1 && !A->bus, "FX tapped again on INSERT 1: INSERT 2 (slot %d)", ec_fx_slot());
    press(FM1P_B_FX, 40);
    CHECK(A->bus == 1, "FX again: SEND 1");
}

static void test_bus_target(void)
{
    int i;
    to(EC_CAT_FILT);
    ec_pn_select_target(EC_NTRACK);                    /* S1 */
    run(20);
    list();
    for (i = 0; i < nL; i++)
        CHECK(L_[i].bus == 1 && L_[i].sub == i, "FILTER on S1: the list is the bus page's lanes (%d)", i);
    go(0);
    sel(1);
    CHECK(at() == (nL > 1 ? 1 : 0) && A->bus == 1 && ec_app_mode() == EC_CAT_FILT, "SEL on S1 stays on the bus page");
    ec_pn_select_target(0);
    run(20);
}

int main(int argc, char **argv)
{
    int m;
    if (argc < 2) {
        printf("usage: ec_test_nav <folder>\n");
        return 2;
    }
    ec_fs_native_root(argv[1], "elasticat");
    boot(NULL, 0);
    fx_setup();
    printf("the page lists (track T1):\n");
    for (m = 0; m < EC_NMODE; m++) {
        if (m == EC_CAT_FILE && A->seq.playing)
            continue;
        to(m);
        if (mode() != m) {
            CHECK(0, "mode %s opens", ec_mode_name(m));
            continue;
        }
        ec_pn_select_target(0);
        run(10);
        walk(ec_mode_name(m));
        keys(ec_mode_name(m), m == EC_CAT_MASTER, m == EC_CAT_FILE);
    }
    test_fx_header();
    test_bus_target();
    C->shutdown();
    printf("test_nav: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
