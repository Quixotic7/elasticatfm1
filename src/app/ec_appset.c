/* SPDX-License-Identifier: GPL-3.0-only */
/* 0.6.0 APP SETTINGS (docs/UX-V2.md §3, SHIFT + HOME): everything global, in its own record of the flash image
 * (ec_plat_settings: survives project loads and new projects). A pop-up: SEL the row, K1..K4 the values of the rows
 * in the cursor's lane (row i is on K(i % 4 + 1)), F1 back (every change undone), F2 OK, HOME closes (keeps). THEME
 * changes the palette as it scrolls. KEYBOARD's ADVANCE and CROSSFADER's glide time live in the same record (set in
 * HOME's settings and on W13..W16). 0.6.1: the MODE POP-UP row is gone; its byte stays in the record (unused0, written
 * 0), so a 0.6.0 record (version 1) loads with every other value as it was. */
#include <stdio.h>
#include <string.h>

#include "ec_app.h"
#include "ec_click.h"
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

void ec_as_load(void)
{
    uint32_t size = 0;
    const uint8_t *rec = ec_plat_settings(&size);
    ec_appset_t s;
    defaults(&P->as);
    if (!rec || size < sizeof s)
        return;
    memcpy(&s, rec, sizeof s);
    if (s.magic != EC_AS_MAGIC || s.version != EC_AS_VERSION || s.size != sizeof s)
        return;                                    /* never written (0xFF), or another layout: the defaults */
    s.theme = (uint8_t)clampi(s.theme, 0, ec_gfx_themes() - 1);
    s.unused0 = 0;                                 /* (0.6.0's MODE POP-UP: ignored) */
    s.knob = (uint8_t)clampi(s.knob, 0, 2);
    s.preview = s.preview != 0;
    s.at = s.at != 0;
    s.metro = (uint8_t)clampi(s.metro, 0, EC_CLICK_N - 1);
    s.cc &= 3;
    s.intro = s.intro != 0;
    s.advance = s.advance != 0;
    s.glide &= 3;
    P->as = s;
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
                                              "MIDI CC", "INTRO"};
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
    default: break;
    }
}

void ec_as_knob(int role, int det)
{
    if (role == EC_E_SELECT)
        P->as_row = clampi(P->as_row + (det > 0 ? 1 : -1), 0, EC_AS_NROW - 1);
    else if (role >= EC_E_K1) {
        const int row = P->as_row / 4 * 4 + (role - EC_E_K1);
        if (row < EC_AS_NROW) {
            edit(row, det);
            P->as_row = row;
        }
    }
    A->redraw = 1;
}
