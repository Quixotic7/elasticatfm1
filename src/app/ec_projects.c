/* SPDX-License-Identifier: GPL-3.0-only */
/* 0.6.42 (owner, docs/PROJECTS-DESIGN.md): PROJECTS, FILE's second mode: every Elasticat project in
 * data/elasticat/projects/, newest first, the one under the cursor's card (its patterns, tempo, samples), and the
 * actions on the black keys: B1 NEW, B2 RENAME, B3 RANDOM (0.6.43), B8 SAVE +1, B9 SAVE, B10 LOAD. OCT+ is yes / LOAD,
 * OCT- is no / back (to SAMPLES). A question waits in the header (LOAD X?, NEW PROJECT?) with the back / OK arrows:
 * OCT- no, OCT+ yes. The files are ec_projfile.c's. */
#include <stdio.h>
#include <string.h>

#include "ec_app.h"
#include "ec_projfile.h"

#define A (&ec_app)

static struct {
    int cursor;
    char sel[EC_PF_NAME];        /* the name under the cursor: the cursor follows it when the list changes */
    int count;
    int ask;                     /* EC_PRJ_ASK_* */
    char target[EC_PF_NAME];
    char want[EC_PF_NAME];       /* 0.6.45: a name the cursor goes to once it is listed (SAVE +1's copy) */
    int want_gen;
} g;

static int clampi(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }

static void follow(void)
{
    const int n = ec_pf_list_count();
    int i;
    if (g.want[0]) {                               /* 0.6.45: SAVE +1's copy, once listed (a save, then a listing) */
        if (ec_pf_list_find(g.want) >= 0) {
            snprintf(g.sel, sizeof g.sel, "%s", g.want);
            g.want[0] = 0;
        } else if (!ec_pf_saving() && ec_pf_list_busy() == 0 && ++g.want_gen > 400)
            g.want[0] = 0;                         /* (not coming: a failed save) */
    }
    i = g.sel[0] ? ec_pf_list_find(g.sel) : -1;
    if (i < 0)
        i = ec_pf_list_find(ec_pf_name());
    g.cursor = clampi(i >= 0 ? i : g.cursor, 0, n > 0 ? n - 1 : 0);
    g.count = n;
    snprintf(g.sel, sizeof g.sel, "%s", ec_pf_list_name(g.cursor));
}

void ec_prj_enter(void)
{
    g.ask = EC_PRJ_ASK_NONE;
    g.sel[0] = 0;                                  /* (on the project you work on) */
    ec_pf_list_refresh();
    follow();
}

void ec_prj_tick(void)
{
    if (ec_app_mode() != EC_MODE_PROJECTS)
        return;
    if (!ec_pf_list_busy() &&
        (g.want[0] || g.count != ec_pf_list_count() || strcmp(g.sel, ec_pf_list_name(g.cursor))))
        follow();
    if (g.sel[0])
        ec_pf_card_want(g.sel);
}

int ec_prj_cursor(void) { return g.cursor; }
int ec_prj_ask(void) { return g.ask; }
const char *ec_prj_target(void) { return g.target; }

void ec_prj_select(int d)
{
    const int n = ec_pf_list_count();
    if (g.ask || n <= 0)
        return;
    g.cursor = clampi(g.cursor + d, 0, n - 1);
    snprintf(g.sel, sizeof g.sel, "%s", ec_pf_list_name(g.cursor));
    A->redraw = 1;
}

static void ask(int what, const char *name)
{
    g.ask = what;
    snprintf(g.target, sizeof g.target, "%s", name ? name : "");
    A->redraw = 1;
}

static void load(void)
{
    if (!g.sel[0]) {
        ec_app_message(ec_pf_list_busy() ? "READING" : "NO PROJECTS");
        return;
    }
    if (!strcmp(g.sel, ec_pf_name())) {
        ec_app_message("OPEN ALREADY");
        return;
    }
    if (ec_pf_dirty())                             /* edits not written: asked first (yes: saved, then it opens) */
        ask(EC_PRJ_ASK_LOAD, g.sel);
    else
        ec_pf_open(g.sel);
}

static void save_plus(void)
{
    char n[EC_PF_NAME];
    ec_pf_name_plus(ec_pf_name()[0] ? ec_pf_name() : "PROJ", n);
    ec_pf_save_as(n, "SAVED +1");                 /* (the header: SAVING, then SAVED +1) */
    snprintf(g.want, sizeof g.want, "%s", n);      /* 0.6.45 (owner): the cursor goes to the copy once listed */
    g.want_gen = 0;
}

/* 0.6.45 (owner): SAVE on the project under the cursor: the current one saved; on another, asked first (OVERWRITE X?):
 * yes writes this project over that file, which is the project from then on */
static void save(void)
{
    if (!g.sel[0] || !strcmp(g.sel, ec_pf_name()))
        ec_pf_save(0);
    else
        ask(EC_PRJ_ASK_OVERWRITE, g.sel);
}

void ec_prj_black(int key)
{
    if (g.ask) {                                   /* a question waits: only OCT- / OCT+ answer it */
        ec_app_message("OCT- NO  OCT+ YES");
        return;
    }
    switch (key) {
    case 1: ask(EC_PRJ_ASK_NEW, ""); break;
    case 2: ec_prj_rename(g.sel); break;
    case 3: ec_prj_random(g.sel); break;
    case 7:                                        /* 0.6.45 (owner): DELETE, asked */
        if (g.sel[0])
            ask(EC_PRJ_ASK_DELETE, g.sel);
        break;
    case 8: save_plus(); break;
    case 9: save(); break;
    case 10: load(); break;
    default: break;
    }
}

void ec_prj_oct(int up)
{
    const int was = g.ask;
    if (was) {
        g.ask = EC_PRJ_ASK_NONE;
        A->redraw = 1;
        if (!up) {
            ec_app_message("CANCELLED");
            return;
        }
        if (was == EC_PRJ_ASK_LOAD)
            ec_pf_open(g.target);
        else if (was == EC_PRJ_ASK_NEW)
            ec_pf_new();
        else if (was == EC_PRJ_ASK_DELETE)
            ec_pf_delete(g.target);
        else if (was == EC_PRJ_ASK_OVERWRITE) {
            ec_pf_save_as(g.target, "SAVED OVER IT");
            snprintf(g.want, sizeof g.want, "%s", g.target);
            g.want_gen = 0;
        }
        return;
    }
    if (up)
        load();
    else
        ec_pn_enter(EC_CAT_FILE);                  /* back: SAMPLES */
}

/* 0.6.46: the name keybed; until then a message */
void ec_prj_rename(const char *name)
{
    (void)name;
    ec_app_message("RENAME: SOON");
}
void ec_prj_random(const char *name)
{
    (void)name;
    ec_app_message("RANDOM: SOON");
}
