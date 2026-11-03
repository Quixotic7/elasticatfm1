/* SPDX-License-Identifier: GPL-3.0-only */
/* 0.6.0 part 1: the hooks part 2 fills (docs/UX-V2.md §3 FILE / SAMPLER, §4 live recording). Until then:
 *   REC tapped      ec_liverec_toggle(): a flag the REC LED shows (no recording yet)
 *   REC held        ec_liverec_settings(): a message
 *   FILE held       ec_file_settings(): a message
 *   FILE's keys     ec_file_black(): B10 = LOAD the browser's file into the next free pool slot (the track plays it)
 *   SAMPLER         ec_sampler_enter(): 0.4's REC pop-up stands in (sampling the input / the master) */
#include <string.h>

#include "ec_app.h"
#include "ec_browse.h"

#define A (&ec_app)

static int g_rec;

void ec_liverec_toggle(void)
{
    g_rec = !g_rec;
    ec_app_message(g_rec ? "REC ON" : "REC OFF");
}

void ec_liverec_settings(void) { ec_app_message("REC SETTINGS"); }
int ec_liverec_on(void) { return g_rec; }
void ec_file_settings(void) { ec_app_message("FILE SETTINGS"); }

static int free_slot(void)
{
    int s;
    for (s = 0; s < EC_NSLOT; s++)
        if (!A->proj.slot[s].path[0] && !A->slot[s].builtin && A->slot[s].state == EC_SL_EMPTY)
            return s;
    return -1;
}

/* the stand-in's page keys (the mock's B3 browser / B4 RAZOR) until the layout gives FILE's pages keys */
static void file_page(int pic)
{
    int p;
    for (p = 0; p < ec_cat_pages(EC_CAT_FILE, 0); p++)
        if (ec_page_pic(EC_CAT_FILE, p, 0) == pic) {
            ec_app_set_page(EC_CAT_FILE, p);
            A->redraw = 1;
            return;
        }
}

void ec_file_black(int key, int down)
{
    if (!down || A->fn || ec_app_mode() != EC_CAT_FILE)
        return;
    if (ec_cat_key_page(EC_CAT_FILE, key) >= 0) {
        ec_app_set_page(EC_CAT_FILE, ec_cat_key_page(EC_CAT_FILE, key));
        return;
    }
    if (key == 3 || key == 4) {
        file_page(key == 3 ? EC_PIC_FILE : EC_PIC_RAZOR);
        return;
    }
    if (key != 10)
        return;
    if (ec_page_pic(EC_CAT_FILE, A->page[EC_CAT_FILE], 0) != EC_PIC_FILE)
        return;
    if (ec_browse_count() <= 0) {
        ec_app_message(A->no_folder ? "NO FOLDER" : "NO FILES");
        return;
    }
    {
        const char *path = ec_browse_path(A->cursor);
        int s, k;
        for (k = 0; k < EC_NSLOT; k++)             /* already in the pool: the track plays that slot */
            if (A->proj.slot[k].path[0] && !strcmp(A->proj.slot[k].path, path))
                break;
        s = k < EC_NSLOT ? k : free_slot();
        if (s < 0) {
            ec_app_message("FULL");
            return;
        }
        if (k >= EC_NSLOT)
            ec_app_load_file(s, path);
        if (!A->bus) {
            ec_e5_edit_param(A->track, EC_P_SLOT, 0);
            ec_app_params(A)[EC_P_SLOT] = (float)(s + 1);
            ec_seq_touch(&A->seq, A->track);
        }
        ec_app_mark_dirty();
        ec_app_message("LOADED");
    }
}

void ec_sampler_enter(int on)
{
    if (on)
        ec_rec_open();
    else
        ec_rec_close();
}
