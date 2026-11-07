/* SPDX-License-Identifier: GPL-3.0-only */
/* 0.6.0 APP SETTINGS (docs/UX-V2.md §3, SHIFT + HOME): everything global, in its own record of the flash image
 * (ec_plat_settings: survives project loads and new projects). A pop-up: a list menu (0.6.1, ec_kit_list): SEL the
 * row, K1 its value (K2..K4 nothing), F1 back (every change undone), F2 OK, HOME closes (keeps). THEME
 * changes the palette as it scrolls. KEYBOARD's ADVANCE and CROSSFADER's glide time live in the same record (set in
 * HOME's settings and on W13..W16). 0.6.1: the MODE POP-UP row is gone; its byte stays in the record (unused0, written
 * 0), so a 0.6.0 record (version 1) loads with every other value as it was. */
#include <stdio.h>
#include <string.h>

#include "ec_app.h"
#include "ec_click.h"
#include "ec_fs.h"
#include "ec_gfx.h"

#define A (&ec_app)
#define P (&ec_app.pn)

const ec_appset_t *ec_as = &ec_app.pn.as;

static void defaults(ec_appset_t *s)
{
    memset(s, 0, sizeof *s);
    s->magic = EC_AS_MAGIC;
    s->version = EC_AS_VERSION;
    s->size = (uint16_t)sizeof *s;
    s->theme = 0;            /* CREAM */
    s->knob = 1;
    s->preview = 1;
    s->at = 1;
    s->metro = EC_CLICK_WOOD;
    s->cc = 3;
    s->intro = 1;
    s->advance = 1;
    s->glide = 2;            /* MED */
}

static int clampi(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }

/* 0.6.65 (owner): the host keeps a firmware's flash per version, so a new build starts with fresh settings: they also
 * live in the folder (/data/settings.bin, written with every change) and come back from there when the flash has
 * none */
#define AS_FILE "/data/settings.bin"
static ec_fsreq_t g_req;
static ec_appset_t g_buf;
static int g_read, g_write, g_wait;
static int32_t g_changed;

static int fix(ec_appset_t *s);

void ec_as_load(void)
{
    uint32_t size = 0;
    const uint8_t *rec = ec_plat_settings(&size);
    ec_appset_t s;
    defaults(&P->as);
    g_read = 1;                                    /* (unless the flash has them) */
    if (!rec || size < sizeof s)
        return;
    memcpy(&s, rec, sizeof s);
    if (!fix(&s))
        return;                                    /* never written (0xFF), or another layout: the defaults */
    P->as = s;                                     /* 0.6.65 (owner): until the folder's copy lands (it wins) */
}

void ec_as_tick(void)
{
    if (g_wait && ec_fs_changed() != g_changed) {  /* no folder at power-on: read once one is there */
        g_wait = 0;
        g_read = 1;
    }
    if (!g_read && !g_write)
        return;
    if (g_req.state == EC_FSR_IDLE) {
        if (g_read)
            ec_fs_start(&g_req, FS_READ, AS_FILE, 0, &g_buf, (int32_t)sizeof g_buf);
        else {
            g_buf = P->as;
            ec_fs_start(&g_req, FS_WRITE, AS_FILE, -1, &g_buf, (int32_t)sizeof g_buf);
        }
    }
    if (ec_fs_update(&g_req) != EC_FSR_DONE)
        return;
    g_req.state = EC_FSR_IDLE;
    if (g_read) {
        const int32_t r = g_req.result;
        g_read = 0;
        if (r == FS_ENOFOLDER) {
            g_wait = 1;
            g_changed = ec_fs_changed();
        } else if (r == (int32_t)sizeof g_buf && fix(&g_buf) && !P->as_open) {
            P->as = g_buf;                         /* the folder's: into this version's flash too */
            ec_gfx_set_theme(P->as.theme);
            ec_as_store();
            g_write = 0;
            A->redraw = 1;
        } else if (r != FS_ENOFOLDER)
            g_write = 1;                           /* no copy in the folder yet (or a stale one): this one goes there */
    } else
        g_write = 0;
}

static int fix(ec_appset_t *ps)
{
    ec_appset_t s = *ps;
    if (s.magic != EC_AS_MAGIC || s.version != EC_AS_VERSION || s.size != sizeof s)
        return 0;
    s.theme = (uint8_t)clampi(s.theme, 0, ec_gfx_themes() - 1);
    s.mode_lock = s.mode_lock == 1;              /* (0.6.0 wrote 0 here: MODE LOCK off) */
    s.knob = (uint8_t)clampi(s.knob, 0, 2);
    s.preview = s.preview != 0;
    s.at = s.at != 0;
    s.metro = (uint8_t)clampi(s.metro, 0, EC_CLICK_N - 1);
    s.cc &= 3;
    s.intro = s.intro != 0;
    s.advance = s.advance != 0;
    s.glide &= 3;
    *ps = s;
    return 1;
}

void ec_as_store(void)
{
    uint32_t size = 0;
    uint8_t *rec = ec_plat_settings(&size);
    if (!rec || size < sizeof P->as)
        return;
    P->as.magic = EC_AS_MAGIC;
    P->as.version = EC_AS_VERSION;
    P->as.size = (uint16_t)sizeof P->as;
    if (!memcmp(rec, &P->as, sizeof P->as))
        return;
    memcpy(rec, &P->as, sizeof P->as);
    ec_plat_settings_changed();
    g_write = 1;                                   /* 0.6.65: the folder's copy too */
}

void ec_as_open(void)
{
    P->as_undo = P->as;
    P->as_open = 1;
    P->as_row = 0;
    A->redraw = 1;
}

void ec_as_close(int keep)
{
    if (!P->as_open)
        return;
    if (keep)
        ec_as_store();
    else
        P->as = P->as_undo;
    ec_gfx_set_theme(P->as.theme);
    P->as_open = 0;
    A->redraw = 1;
}

const char *ec_as_row_label(int row)
{
    static const char *const L[EC_AS_NROW] = {"THEME", "KNOB SPEED", "AUTO-PREVIEW", "AFTERTOUCH", "METRO SOUND",
                                              "MIDI CC", "INTRO", "MODE LOCK"};
    return row >= 0 && row < EC_AS_NROW ? L[row] : "";
}

void ec_as_row_value(int row, char *out, int cap)
{
    static const char *const KS[3] = {"SLOW", "MED", "FAST"};
    static const char *const CC[4] = {"OFF", "1", "11", "1+11"};
    const ec_appset_t *s = &P->as;
    switch (row) {
    case EC_AS_THEME: snprintf(out, (size_t)cap, "%s", ec_gfx_theme_name(s->theme)); break;
    case EC_AS_KNOB: snprintf(out, (size_t)cap, "%s", KS[clampi(s->knob, 0, 2)]); break;
    case EC_AS_PREVIEW: snprintf(out, (size_t)cap, "%s", s->preview ? "ON" : "OFF"); break;
    case EC_AS_AT: snprintf(out, (size_t)cap, "%s", s->at ? "ON" : "OFF"); break;
    case EC_AS_METRO: snprintf(out, (size_t)cap, "%s", ec_click_names[clampi(s->metro, 0, EC_CLICK_N - 1)]); break;
    case EC_AS_CC: snprintf(out, (size_t)cap, "%s", CC[s->cc & 3]); break;
    case EC_AS_INTRO: snprintf(out, (size_t)cap, "%s", s->intro ? "ON" : "OFF"); break;
    case EC_AS_LOCK: snprintf(out, (size_t)cap, "%s", s->mode_lock ? "ON" : "OFF"); break;
    default: out[0] = 0; break;
    }
}

static void edit(int row, int d)
{
    ec_appset_t *s = &P->as;
    const int n = ec_gfx_themes();
    switch (row) {
    case EC_AS_THEME:                              /* live while scrolling */
        s->theme = (uint8_t)(((s->theme + (d > 0 ? 1 : -1)) % n + n) % n);
        ec_gfx_set_theme(s->theme);
        break;
    case EC_AS_KNOB: s->knob = (uint8_t)clampi(s->knob + (d > 0 ? 1 : -1), 0, 2); break;
    case EC_AS_PREVIEW: s->preview = d > 0; break;
    case EC_AS_AT: s->at = d > 0; break;
    case EC_AS_METRO: s->metro = (uint8_t)clampi(s->metro + (d > 0 ? 1 : -1), 0, EC_CLICK_N - 1); break;
    case EC_AS_CC: s->cc = (uint8_t)clampi(s->cc + (d > 0 ? 1 : -1), 0, 3); break;
    case EC_AS_INTRO: s->intro = d > 0; break;
    case EC_AS_LOCK: s->mode_lock = d > 0; break;
    default: break;
    }
}

void ec_as_knob(int role, int det)
{
    if (ec_app_list_knob(role, det > 0 ? 1 : -1, &P->as_row, EC_AS_NROW))
        edit(P->as_row, det);
    A->redraw = 1;
}
