/* SPDX-License-Identifier: GPL-3.0-only */
/* The layout designer's preview: Elasticat itself (every unit of src/, through its own fm1core descriptor, on the
 * ABI's device clock as host/ec_run.c drives it) compiled to WebAssembly, with a small API for the page
 * (tools/layout/designer.js): load a layout, pick the variant's context (machine, warp, filter, envs, FX; a bus), go
 * to a category / page / lane, turn knobs and press buttons as on the unit, run the demo project, read the screen.
 * The demo: the folder of host/make_demo_folder.py at /demo (MEMFS) and a project built through panel input
 * (tools/layout/build_layout.sh), booted from /demo.flash and playing. The preview never writes the flash. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define API EMSCRIPTEN_KEEPALIVE
#else
#define API
#endif

#include "ec_app.h"
#include "ec_fs_native.h"
#include "ec_layout.h"
#include "ec_layout_pack.h"
#include "ec_mode.h"
#include "ec_params.h"
#include "ec_rec.h"
#include "fm1core.h"

uint32_t fm1core_labels_epoch(void);
const char *fm1core_label(int32_t which);

static const fm1core_t *C;
static fm1panel_t *P;
static uint32_t g_ms, g_last_frame;
static uint64_t g_frames;
static uint8_t g_rgba[FM1P_LCD_W * FM1P_LCD_H * 4];
static char g_out[65536];
static uint32_t g_release[FM1P_NB];               /* a pressed button's release time (0: none) */

API int lw_boot(const char *folder, const char *flash)
{
    ec_fs_native_root(folder && folder[0] ? folder : NULL, "elasticat");
    C = fm1core_get(FM1CORE_ABI);
    if (!C)
        return -1;
    P = C->panel;
    C->options(flash && flash[0] ? flash : NULL, flash && flash[0] ? 0 : 1, 0, 1);
    P->ready = 2;
    C->init(0);
    g_ms = 0;
    return 0;
}

/* device time on: ms milliseconds (ticks, frames every 15 ms, audio blocks; the sound is dropped) */
API void lw_run(int ms)
{
    int k;
    for (; ms > 0; ms--, g_ms++) {
        ec_fs_native_pump();
        for (k = 0; k < FM1P_NB; k++)
            if (g_release[k] && g_ms >= g_release[k]) {
                P->buttons &= ~(1u << P->btn_id[k]);
                g_release[k] = 0;
            }
        C->tick(g_ms);
        if (g_ms == 0 || g_ms - g_last_frame >= 15u) {
            g_last_frame = g_ms;
            C->frame();
        } else
            C->idle();
        while (g_frames + FM1P_BLOCK <= (uint64_t)g_ms * FM1P_FS / 1000u) {
            int16_t blk[FM1P_BLOCK * 2];
            C->audio(blk, FM1P_BLOCK);
            g_frames += FM1P_BLOCK;
        }
    }
}

API uint32_t lw_ms(void) { return g_ms; }

/* the screen, 240 x 240 RGBA */
API uint8_t *lw_rgba(void)
{
    int i;
    for (i = 0; i < FM1P_LCD_W * FM1P_LCD_H; i++) {
        const uint8_t *px = (const uint8_t *)&P->lcd[i];
        const unsigned v = (unsigned)px[0] << 8 | px[1];
        g_rgba[4 * i] = (uint8_t)((v >> 11) * 255 / 31);
        g_rgba[4 * i + 1] = (uint8_t)(((v >> 5) & 63) * 255 / 63);
        g_rgba[4 * i + 2] = (uint8_t)((v & 31) * 255 / 31);
        g_rgba[4 * i + 3] = 255;
    }
    return g_rgba;
}

/* ------------------------------------------------------------------------------------------- the layout --- */
API int lw_load(const char *pack)
{
    const ec_layout_t *l = ec_layout_unpack(pack);
    if (!l)
        return -1;
    ec_layout_use(l);
    ec_app.redraw = 1;
    return 0;
}
API const char *lw_error(void) { return ec_layout_pack_error(); }
API void lw_use_shipped(void)
{
    ec_layout_use(NULL);
    ec_app.redraw = 1;
}
/* the layout in use, packed (the compiled one: the same text gen_layout.py --pack writes) */
API const char *lw_pack(void)
{
    return ec_layout_pack(ec_layout(), g_out, sizeof g_out) < 0 ? "" : g_out;
}

/* ------------------------------------------------------------------------------------------- the context --- */
/* the selected track's machine / warp / filter / env modes / insert FX (0: leave it) */
API void lw_context(int mach, int warp, int filt, int fenv, int env, int fx)
{
    float *v = ec_app_params(&ec_app);
    if (mach) v[EC_P_MACHINE] = (float)mach;
    if (warp) v[EC_P_MODE] = (float)warp;
    if (filt) v[EC_P_FILTER_MACHINE] = (float)filt;
    if (fenv) v[EC_P_FENV_MODE] = (float)fenv;
    if (env) v[EC_P_ENV_MODE] = (float)env;
    if (fx) v[EC_P_FX_MACHINE] = (float)fx;
    ec_seq_touch(&ec_app.seq, ec_app.track);
    ec_app.redraw = 1;
}
/* bus 1..3's FX machine */
API void lw_bus_fx(int bus, int fx)
{
    if (bus >= 1 && bus <= 3 && fx >= 1 && fx <= 18)
        ec_app.proj.bus[bus - 1][EC_P_FX_MACHINE] = (float)fx;
    ec_app.redraw = 1;
}
/* 0..5 a track, 6 S1, 7 S2, 8 the master bus */
API void lw_target(int i)
{
    if (i >= 0 && i < EC_NTRACK) {
        ec_app.track = i;
        ec_app.bus = 0;
    } else if (i >= EC_NTRACK && i < EC_NTRACK + 3)
        ec_app.bus = i - EC_NTRACK + 1;
    ec_app.redraw = 1;
}
/* a category's page and lane (settings: its setup), every pop-up closed: its mode entered as the panel enters it
 * (ec_app_goto), without the mode's pop-up (the designer jumps, the player doesn't); FILE while playing too (the
 * unit refuses it, STOP FIRST: the designer shows the page anyway) */
API void lw_goto(int cat, int page, int lane, int settings)
{
    if (cat < 0 || cat >= EC_NCAT)
        return;
    ec_app.ux.f3 = 0;
    ec_app_goto(cat, page < 0 ? 0 : page);
    if (ec_app.cat != cat || ec_app.scr) {
        ec_app.scr = 0;
        ec_app.cat = cat;
        ec_app_set_page(cat, page < 0 ? 0 : page);
    }
    ec_app.settings = settings != 0;
    if (settings)
        ec_app.ux.set_row = lane < 0 ? 0 : lane * 4;   /* 0.6.1: settings are a list; a lane = its first row */
    else
        ec_app.ux.lane[cat] = lane < 0 ? 0 : lane;
    ec_app.redraw = 1;
}

/* F3 open (1) or closed (0) */
API void lw_f3(int on)
{
    ec_app.ux.f3 = on != 0;
    ec_app.redraw = 1;
}

/* ------------------------------------------------------------------------------------------- the panel --- */
/* a knob role (0 SELECT, 1 ALGORITHM, 2 PRESETS, 3..6 KNOB 1-4) turned det detents */
API void lw_turn(int role, int det)
{
    if (role >= 0 && role < FM1P_NE - 1)
        P->enc[P->enc_id[role]] += det * P->enc_dir[role];
}
/* a button by label (FX SEL ENV LFO EDIT GLO HOME SAVE ARP SEQ PLAY REC OCT- OCT+) held ms */
API void lw_press(int b, int ms)
{
    if (b < 0 || b >= FM1P_NB)
        return;
    P->buttons |= 1u << P->btn_id[b];
    P->buttons_tap |= 1u << P->btn_id[b];
    g_release[b] = g_ms + (uint32_t)(ms > 0 ? ms : 60);
}

/* ------------------------------------------------------------------------------------------- reading --- */
static int jstr(char *o, int n, int cap, const char *s)
{
    if (n < cap)
        n += snprintf(o + n, (size_t)(cap - n), "\"");
    for (; s && *s && n < cap - 2; s++) {
        if (*s == '"' || *s == '\\')
            o[n++] = '\\';
        o[n++] = *s;
    }
    if (n < cap)
        n += snprintf(o + n, (size_t)(cap - n), "\"");
    return n;
}

#define OPT(v, id) ((int)((v)[id] + 0.5f))

/* 0.6.0: the loaded layout's modes as the firmware reads them (ec_pages.h): JSON [{mode, name, tag, button, order,
 * keys: [the page each black key B1..B11 opens, -1 none], bus: [the bus each key targets, 0 none]}] */
API const char *lw_modes(void)
{
    int n = 0, m, k;
    const int cap = (int)sizeof g_out;
    n += snprintf(g_out + n, (size_t)(cap - n), "[");
    for (m = 0; m < EC_NMODE && n < cap - 200; m++) {
        n += snprintf(g_out + n, (size_t)(cap - n), "%s{\"mode\":%d,\"name\":", m ? "," : "", m);
        n = jstr(g_out, n, cap, ec_mode_name(m));
        n += snprintf(g_out + n, (size_t)(cap - n), ",\"tag\":");
        n = jstr(g_out, n, cap, m < EC_NCAT ? ec_cat_tag(m) : "");
        n += snprintf(g_out + n, (size_t)(cap - n), ",\"button\":%d,\"order\":%d,\"keys\":[", ec_mode_button(m),
                      ec_mode_order(m));
        for (k = 1; k <= EC_NBKEY; k++)
            n += snprintf(g_out + n, (size_t)(cap - n), "%s%d", k > 1 ? "," : "", ec_cat_key_page(m, k));
        n += snprintf(g_out + n, (size_t)(cap - n), "],\"bus\":[");
        for (k = 1; k <= EC_NBKEY; k++)
            n += snprintf(g_out + n, (size_t)(cap - n), "%s%d", k > 1 ? "," : "", ec_cat_key_bus(m, k));
        n += snprintf(g_out + n, (size_t)(cap - n), "]}");
    }
    if (n < cap)
        snprintf(g_out + n, (size_t)(cap - n), "]");
    return g_out;
}

/* the panel's state and the page shown: JSON */
API const char *lw_state(void)
{
    ec_page_t pg;
    int n = 0, i;
    const int cap = (int)sizeof g_out;
    const float *v = ec_app_params(&ec_app);
    ec_app_page(&ec_app, &pg);
    n += snprintf(g_out + n, (size_t)(cap - n),
                  "{\"cat\":%d,\"page\":%d,\"lane\":%d,\"lanes\":%d,\"settings\":%d,\"bus\":%d,\"track\":%d,"
                  "\"pic\":%d,\"playing\":%d,\"f3\":%d,\"ms\":%u,\"ctx\":[%d,%d,%d,%d,%d,%d],"
                  "\"busfx\":[%d,%d,%d],\"title\":",
                  ec_app.cat, ec_app.page[ec_app.cat], ec_app_lane(&ec_app, &pg), ec_ux_lanes(pg.n),
                  ec_app.settings, ec_app.bus, ec_app.track, pg.pic, ec_app.seq.playing, ec_app.ux.f3, g_ms,
                  OPT(v, EC_P_MACHINE), OPT(v, EC_P_MODE), OPT(v, EC_P_FILTER_MACHINE), OPT(v, EC_P_FENV_MODE),
                  OPT(v, EC_P_ENV_MODE), OPT(v, EC_P_FX_MACHINE), OPT(ec_app.proj.bus[0], EC_P_FX_MACHINE),
                  OPT(ec_app.proj.bus[1], EC_P_FX_MACHINE), OPT(ec_app.proj.bus[2], EC_P_FX_MACHINE));
    n = jstr(g_out, n, cap, pg.title);
    n += snprintf(g_out + n, (size_t)(cap - n), ",\"scr\":%d,\"mode\":", ec_app.scr);   /* 0.6.0: the mode shown */
    n = jstr(g_out, n, cap, ec_mode_name(ec_app_mode()));
    n += snprintf(g_out + n, (size_t)(cap - n), ",\"key\":%d,\"items\":[",
                  ec_app.settings ? 0 : ec_cat_page_key(ec_app.cat, ec_app.page[ec_app.cat]));
    for (i = 0; i < pg.n && n < cap - 64; i++) {
        char val[24];
        const ec_item_t *it = &pg.it[i];
        const float x = it->kind == EC_IT_BLANK ? 0 : ec_app_item_value(&ec_app, it, 0);
        val[0] = 0;
        if (it->kind != EC_IT_BLANK)
            ec_app_item_format(&ec_app, it, x, val, sizeof val);
        n += snprintf(g_out + n, (size_t)(cap - n), "%s{\"kind\":%d,\"id\":%d,\"label\":", i ? "," : "", it->kind,
                      it->id);
        n = jstr(g_out, n, cap, it->kind == EC_IT_BLANK ? "" : ec_app_item_label(it));
        n += snprintf(g_out + n, (size_t)(cap - n), ",\"value\":");
        n = jstr(g_out, n, cap, val);
        n += snprintf(g_out + n, (size_t)(cap - n), "}");
    }
    n += snprintf(g_out + n, (size_t)(cap - n), "],\"labels\":[");
    for (i = 0; i < 8; i++) {                       /* the live knob labels (ec_app_labels): FM1P_E_* order */
        const char *s = fm1core_label(EC_LBL_KNOB0 + i);
        if (i)
            n += snprintf(g_out + n, (size_t)(cap - n), ",");
        n = jstr(g_out, n, cap, s ? s : "");
    }
    if (n < cap - 3)
        n += snprintf(g_out + n, (size_t)(cap - n), "]}");
    return g_out;
}

/* an item's value as its cell shows it, for the selected track / bus (kind EC_IT_*, id) */
API const char *lw_value(int kind, int id)
{
    ec_item_t it;
    float x;
    memset(&it, 0, sizeof it);
    it.kind = (uint8_t)kind;
    it.id = (uint8_t)id;
    if (kind == EC_IT_BLANK || (kind == EC_IT_PARAM && (id < 0 || id >= EC_NP)))
        return "";
    x = ec_app_item_value(&ec_app, &it, 0);
    ec_app_item_format(&ec_app, &it, x, g_out, 24);
    return g_out;
}

/* the parameters: [{"i", "name", "label", "kind", "fx", "lock"}] (the firmware's own table) */
API const char *lw_catalog(void)
{
    int n = 0, i;
    const int cap = (int)sizeof g_out;
    n += snprintf(g_out + n, (size_t)(cap - n), "[");
    for (i = 0; i < EC_NP && n < cap - 160; i++) {
        const ec_param_def_t *d = &EC_PARAMS[i];
        n += snprintf(g_out + n, (size_t)(cap - n), "%s{\"i\":%d,\"name\":", i ? "," : "", i);
        n = jstr(g_out, n, cap, d->name);
        n += snprintf(g_out + n, (size_t)(cap - n), ",\"label\":");
        n = jstr(g_out, n, cap, d->label);
        n += snprintf(g_out + n, (size_t)(cap - n), ",\"kind\":%d,\"fx\":%d,\"lock\":%d}", d->kind,
                      (d->flags & EC_PF_FX) ? 1 : 0, (d->flags & EC_PF_LOCKABLE) ? 1 : 0);
    }
    n += snprintf(g_out + n, (size_t)(cap - n), "]");
    return g_out;
}

/* The drawn FM-1 (tools/layout/fm1/): the whole panel as the unit has it. A button by label held / let go (a press
 * also marks a tap, so a click shorter than a millisecond of device time still counts); a note key 0..26 held with
 * a velocity 1..127 / let go; the MASTER pot 0..1023. Knobs: lw_turn (the firmware accelerates by the detents'
 * timing, as on the unit). */
API void lw_button(int b, int down)
{
    if (b < 0 || b >= FM1P_NB)
        return;
    g_release[b] = 0;
    if (down) {
        P->buttons |= 1u << P->btn_id[b];
        P->buttons_tap |= 1u << P->btn_id[b];
    } else
        P->buttons &= ~(1u << P->btn_id[b]);
}
API void lw_key(int k, int down, int vel)
{
    if (k < 0 || k >= FM1P_NKEY)
        return;
    if (down) {
        P->key_vel[k] = (uint8_t)(vel < 1 ? 100 : vel > 127 ? 127 : vel);
        P->keys |= 1u << k;
        P->keys_tap |= 1u << k;
    } else {
        P->keys &= ~(1u << k);
        P->key_vel[k] = 0;
    }
}
API void lw_master(int v) { P->master = v < 0 ? 0 : v > 1023 ? 1023 : v; }
/* the LEDs: 44 levels (column * 4 + line, the panel's wiring) then PLAY's green */
API const uint8_t *lw_leds(void) { return &P->led[0][0]; }
/* the wiring: 66 ids (row * 11 + column; 0..13 matrix button, 14 + n note key n, -1 none) and the 14 buttons'
 * matrix ids by label: JSON {"keymap":[...],"btn":[...]} */
API const char *lw_wiring(void)
{
    int n = 0, i;
    const int cap = (int)sizeof g_out;
    n += snprintf(g_out + n, (size_t)(cap - n), "{\"keymap\":[");
    for (i = 0; i < 6 * FM1P_NCOL; i++)
        n += snprintf(g_out + n, (size_t)(cap - n), "%s%d", i ? "," : "", C->keymap ? C->keymap[i] : -1);
    n += snprintf(g_out + n, (size_t)(cap - n), "],\"btn\":[");
    for (i = 0; i < FM1P_NB; i++)
        n += snprintf(g_out + n, (size_t)(cap - n), "%s%d", i ? "," : "", P->btn_id[i]);
    n += snprintf(g_out + n, (size_t)(cap - n), "]}");
    return g_out;
}
/* the 49 live labels (fm1core_label: knobs, buttons, white keys, black keys; null = not given) and their epoch */
API const char *lw_labels(void)
{
    int n = 0, i;
    const int cap = (int)sizeof g_out;
    n += snprintf(g_out + n, (size_t)(cap - n), "{\"epoch\":%u,\"list\":[", fm1core_labels_epoch());
    for (i = 0; i < FM1CORE_LBL_N; i++) {
        const char *s = fm1core_label(i);
        if (i)
            n += snprintf(g_out + n, (size_t)(cap - n), ",");
        if (s)
            n = jstr(g_out, n, cap, s);
        else
            n += snprintf(g_out + n, (size_t)(cap - n), "null");
    }
    n += snprintf(g_out + n, (size_t)(cap - n), "]}");
    return g_out;
}

