/* SPDX-License-Identifier: GPL-3.0-only */
/* 0.6.0: the screens of the modes without param pages of their own (ec_scr_modes.h), in the approved mockups'
 * geometry (host/mock_ux2.c scr_pages, scr_keyboard, scr_crossfader, scr_patterns, scr_app_settings, scr_cat,
 * scr_intro, popup): the 120x120 grid, palette slots only, the kit's header and cells. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ec_gfx.h"
#include "ec_scene.h"
#include "ec_scr_modes.h"
#include "ec_screens.h"
#include "ec_ui_kit.h"

#define W EC_GW
#define H EC_GH
#define R(c, x, y, w, h) ec_gfx_rect((x), (y), (w), (h), (c))
#define F(c, x, y, w, h) ec_gfx_frame((x), (y), (w), (h), (c))
#define T(c, s, x, y, al) ec_gfx_text_al((x), (y), (s), 1, (c), (al))
#define T2(c, s, x, y, al) ec_gfx_text_al((x), (y), (s), 2, (c), (al))

static int clampi(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }
static int iv(const float *v, int id) { return (int)floorf(v[id] + 0.5f); }
static const float *tv(const ec_app_t *a) { return a->proj.pat[a->proj.current].tr[a->track].v; }
static int blink(const ec_app_t *a, uint32_t p) { return (a->ms / p) & 1u; }

static void pat_name(int p, char *b, size_t cap)
{
    snprintf(b, cap, "%c%d", 'A' + p / EC_BANK_PATS, p % EC_BANK_PATS + 1);
}

/* ------------------------------------------------------------------------------------------- the CAT --- */
static void cat_screen(const ec_app_t *a)
{
    char t[24], r[24];
    uint16_t muted = 0;
    int i;
    const int frame = (int)((float)a->ms * 0.001f * (float)EC_KIT_SPR_FPS * a->proj.bpm / 120.0f);
    ec_scr_cat(frame);
    snprintf(t, sizeof t, "%s/CAT", ec_cat_tag(EC_CAT_MASTER));
    snprintf(r, sizeof r, "%d BPM %s", (int)floorf(a->proj.bpm + 0.5f),
             a->bus ? "" : ec_rate_label(iv(tv(a), EC_P_RATE)));
    ec_scr_header(a, t, 0, r);
    for (i = 0; i < EC_NTRACK; i++)
        if (!ec_app_track_audible(a, i))
            muted |= (uint16_t)(1u << i);
    ec_kit_track_keys(99, a->bus ? EC_NTRACK - 1 + a->bus : a->track, muted);
}

/* ------------------------------------------------------------------------------------------- PAGES --- */
static void pages_screen(const ec_app_t *a)
{
    const ec_ptrack_t *tr = &a->proj.pat[a->proj.current].tr[a->track];
    const int len = ec_seq_pattern_steps(&a->seq, a->track);
    const int pages = (len + 15) / 16;
    const int play = ec_seq_step(&a->seq, a->track);
    const int first = clampi(a->step_page - 1, 0, pages > 4 ? pages - 4 : 0);
    ec_page_t pg;
    char r[8];
    int p, j;
    pat_name(a->proj.current, r, sizeof r);
    ec_scr_header(a, ec_mode_name(EC_MODE_PAGES), a->step_page + 1, r);
    for (p = first; p < first + 4 && p < pages; p++) {
        const int y = 13 + (p - first) * 15, sel = p == a->step_page;
        const int loop = ec_e5_page_in_loop(&a->proj, a->track, p, pages);
        const int cut = a->pn.clip_state != EC_CLIP_EMPTY && a->pn.clip_kind == EC_CK_PAGE && a->pn.clip_from == p;
        char n[4];
        snprintf(n, sizeof n, "%d", p + 1);
        if (sel) {
            R(EC_C_L15, 2, y, 11, 12);
            T2(EC_C_BG, n, 8, y + 1, EC_AL_CENTER);
        } else
            T2(loop ? EC_C_L12 : EC_C_L5, n, 8, y + 1, EC_AL_CENTER);
        if (loop)
            R(EC_C_ACC, 15, y, 1, 12);             /* in the loop */
        for (j = 0; j < 16; j++) {
            const int idx = p * 16 + j, x = 18 + j * 6 + (j / 4);
            const int on = idx < len && (tr->steps[idx].trig || tr->steps[idx].slices);
            int c = on ? (loop ? EC_C_L12 : EC_C_L5) : EC_C_L3;
            if (idx == play && (a->seq.playing || a->seq.paused))
                c = EC_C_ACC;
            if (cut)
                c = blink(a, 120) ? EC_C_ACC : c;
            R(c, x, y + (on ? 1 : 8), 5, on ? 10 : 3);
        }
    }
    if (pages > 4) {                               /* where the window is */
        R(EC_C_L3, W - 2, 13, 1, 57);
        R(EC_C_L12, W - 2, 13 + 57 * first / pages, 1, 57 * 4 / pages);
    }
    ec_app_page(a, &pg);
    ec_scr_cells(a, &pg);
}

/* ------------------------------------------------------------------------------------------- KEYBOARD --- */
static void keyboard_screen(const ec_app_t *a)
{
    const float *v = tv(a);
    const ec_ptrack_t *tr = &a->proj.pat[a->proj.current].tr[a->track];
    const int m = iv(v, EC_P_MACHINE);
    const int slice = !a->bus && m >= EC_MACH_GRID_SLICE && m <= EC_MACH_RAZOR_POLY;
    const int len = ec_seq_pattern_steps(&a->seq, a->track);
    const ec_ss_sample_t *smp = a->bus ? 0 : ec_app_track_sample(a, a->track);
    const int tpage = a->pn.target ? (a->pn.target - 1) / 16 : a->step_page;
    ec_page_t pg;
    char b[24], pgs[8];
    int i;
    snprintf(pgs, sizeof pgs, "%d", a->pn.kb_bank + 1);
    ec_scr_header(a, ec_mode_name(EC_MODE_KEYBOARD), slice ? a->pn.kb_bank + 1 : 0, ec_machine_name(m));
    snprintf(b, sizeof b, "OCT %+d", a->pn.oct);
    T(EC_C_L8, b, W - 2, 13, EC_AL_RIGHT);
    if (slice) {
        const int cnt = clampi(iv(v, EC_P_SLICE_COUNT), 1, 32), sel = clampi(iv(v, EC_P_SLICE_INDEX), 1, cnt);
        const int sx = 2 + 112 * (sel - 1) / cnt, ex = 2 + 112 * sel / cnt;
        snprintf(b, sizeof b, "SLICE %d/%d", sel, cnt);
        T(EC_C_L15, b, 2, 13, EC_AL_LEFT);
        R(EC_C_L3, sx, 20, ex - sx, 30);
        if (smp)
            ec_kit_wave(smp->overview, EC_SS_OVERVIEW, 2, 114, 20, 50, EC_C_L15, EC_C_L8, sx, ex);
        for (i = 0; i <= cnt; i++) {               /* the slices (an even grid; RAZOR's own points on FILE) */
            const int x = 2 + 112 * i / cnt;
            R(i == sel - 1 || i == sel ? EC_C_ACC : EC_C_L5, x > 113 ? 113 : x, 18, 1, 34);
        }
    } else {                                       /* a keyboard: the notes held lit */
        const uint32_t held = 0;
        T(EC_C_L15, a->bus ? "TRACKS ONLY" : "KEYS", 2, 13, EC_AL_LEFT);
        for (i = 0; i < 16; i++) {
            const int x = 3 + i * 7;
            R(EC_C_L12, x, 20, 6, 30);
            if (((ec_kb_semis(ec_key_of_lower(i)) % 12) + 12) % 12 == 0)
                R(EC_C_L5, x + 1, 46, 4, 2);       /* the Cs */
        }
        for (i = 0; i < 11; i++) {                 /* the sharps between the naturals (F# G# A# C# D#) */
            static const int GAP[11] = {0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14};
            R(EC_C_BLACK, 3 + GAP[i] * 7 + 4, 20, 4, 18);
        }
        (void)held;
        if (a->seq.tr[a->track].live_pitch_on) {   /* the note playing */
            const int s = (int)floorf(a->seq.tr[a->track].live_pitch - v[EC_P_PITCH] + 0.5f);
            const int k = s + 7 - 12 * a->pn.oct;
            const int lo = k >= 0 && k < EC_NKEY ? ec_key_lower(k) : -1, up = k >= 0 && k < EC_NKEY ? ec_key_upper(k) : -1;
            if (lo >= 0)
                R(EC_C_ACC, 3 + lo * 7 + 1, 40, 4, 8);
            else if (up >= 0) {
                static const int GAP[11] = {0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14};
                R(EC_C_ACC, 3 + GAP[up] * 7 + 4, 30, 4, 7);
            }
        }
    }
    for (i = 0; i < 16; i++) {                     /* step entry: the target's page, the target */
        const int idx = tpage * 16 + i, x = 3 + i * 7;
        const int done = idx < len && (tr->steps[idx].trig || tr->steps[idx].slices);
        const int tgt = a->pn.target && idx == a->pn.target - 1;
        if (idx >= len)
            continue;
        R(tgt ? EC_C_ACC : done ? EC_C_L12 : EC_C_L3, x, tgt || done ? 58 : 64, 5, tgt || done ? 8 : 2);
        if (tgt && i < 15)
            T(EC_C_ACC, ">", x + 6, 59, EC_AL_LEFT);
    }
    if (!a->pn.target)
        T(EC_C_L8, "LIVE", W - 2, 59, EC_AL_RIGHT);
    ec_app_page(a, &pg);
    ec_scr_cells(a, &pg);
}

/* ------------------------------------------------------------------------------------------- CROSSFADER --- */
static void xfd_screen(const ec_app_t *a)
{
    static const char *const G[4] = {"INST", "FAST", "MED", "SLOW"};
    const float pos = a->proj.e5.sc.pos;
    const int x0 = 16, len = 88, kx = x0 + (int)(len * pos + 0.5f);
    const ec_scenes_t *sc = &a->proj.e5.sc;
    ec_page_t pg;
    char b[12];
    int i;
    ec_scr_header(a, ec_cat_tag(EC_CAT_XFD), 0, G[a->pn.as.glide & 3]);
    T2(a->e5.anchor == 1 ? EC_C_LOCK : EC_C_L15, "A", 3, 22, EC_AL_LEFT);
    T2(a->e5.anchor == 2 ? EC_C_LOCK : EC_C_L15, "B", W - 3, 22, EC_AL_RIGHT);
    R(EC_C_L3, x0, 26, len, 3);
    R(EC_C_L8, x0, 26, kx - x0, 3);
    if (a->e5.gliding) {                           /* gliding to the held key */
        const int tx = x0 + (int)(len * a->e5.glide_target + 0.5f);
        for (i = kx < tx ? kx + 4 : tx; i < (kx < tx ? tx : kx - 3); i += 2)
            R(EC_C_ACC, i, 27, 1, 1);
    }
    R(EC_C_L15, kx - 2, 20, 5, 15);
    for (i = 0; i < 12; i++) {                     /* W1..W12 */
        const int held = a->pn.xf_key == i;
        R(held ? EC_C_ACC : EC_C_L5, x0 + i * 8 - (i == 11 ? 1 : 0), 39, 2, held ? 5 : 3);
    }
    snprintf(b, sizeof b, "%d", (int)floorf(pos * 127.0f + 0.5f));
    T2(EC_C_L15, b, clampi(kx, 12, W - 12), 47, EC_AL_CENTER);
    snprintf(b, sizeof b, "%d", ec_scene_count(sc, 0));
    T2(EC_C_L8, b, 3, 62, EC_AL_LEFT);
    snprintf(b, sizeof b, "%d", ec_scene_count(sc, 1));
    T2(EC_C_L8, b, W - 3, 62, EC_AL_RIGHT);
    for (i = 0; i < 4; i++)                        /* W13..W16: the glide times */
        R(i == (a->pn.as.glide & 3) ? EC_C_L15 : EC_C_L3, 40 + i * 11, 64, 9, 6);
    ec_app_page(a, &pg);
    ec_scr_cells(a, &pg);
}

/* ------------------------------------------------------------------------------------------- PATTERNS --- */
static void patterns_screen(const ec_app_t *a)
{
    static const char *const Q[] = {"SEQ", "JUMP", "START", "TEMP"};
    const int bank = a->pn.bank;
    ec_page_t pg;
    char r[16], pgs[4];
    int i;
    snprintf(r, sizeof r, "Q %s", Q[clampi(a->proj.quant, 1, 4) - 1]);
    snprintf(pgs, sizeof pgs, "%c", 'A' + bank);
    {
        char t[24];
        const char *n = ec_e5_pattern_name(a->proj.current);
        snprintf(t, sizeof t, "%s", n[0] ? n : ec_cat_tag(EC_CAT_PAT));
        static const char *const N[9] = {"T1", "T2", "T3", "T4", "T5", "T6", "S1", "S2", "MST"};
        ec_kit_header(N[a->bus ? EC_NTRACK - 1 + a->bus : a->track], a->msg_ms ? a->msg : t, pgs, r, 0,
                      ec_app_popup(a));
    }
    for (i = 0; i < 16; i++) {
        const int p = ec_pat_index(bank, i), col = i % 4, row = i / 4, x = 2 + col * 29, y = 12 + row * 14, w = 27,
                  h = 12;
        const int cut = a->pn.clip_state != EC_CLIP_EMPTY && a->pn.clip_kind == EC_CK_PATTERN && a->pn.clip_from == p;
        char n[8];
        pat_name(p, n, sizeof n);
        if (p == a->proj.current) {
            R(EC_C_L15, x, y, w, h);
            T2(EC_C_BG, n, x + w / 2, y + 1, EC_AL_CENTER);
        } else if (p == a->seq.pending) {
            R(blink(a, 120) ? EC_C_ACC : EC_C_L5, x, y, w, h);
            T2(EC_C_BG, n, x + w / 2, y + 1, EC_AL_CENTER);
        } else {
            F(cut && blink(a, 120) ? EC_C_ACC : a->proj.pat[p].used ? EC_C_L8 : EC_C_L3, x, y, w, h);
            T2(a->proj.pat[p].used ? EC_C_L12 : EC_C_L5, n, x + w / 2, y + 1, EC_AL_CENTER);
        }
    }
    {   /* to the launch: the global cycle (a pending switch engages at its end) */
        const int gl = clampi(a->proj.glen, 1, 256);
        const int pos = a->seq.playing || a->seq.paused ? a->seq.global_pos : 0;
        R(EC_C_L3, 2, 70, W - 4, 2);
        R(a->seq.pending >= 0 ? EC_C_ACC : EC_C_L8, 2, 70, (W - 4) * (pos + 1) / gl, 2);
    }
    ec_app_page(a, &pg);
    ec_scr_cells(a, &pg);
}

/* ------------------------------------------------------------------------------------------- APP SETTINGS --- */
static void appset_screen(const ec_app_t *a)
{
    static char val[EC_AS_NROW][16];
    ec_kit_row_t rows[EC_AS_NROW];
    int i;
    for (i = 0; i < EC_AS_NROW; i++) {
        ec_as_row_value(i, val[i], sizeof val[i]);
        rows[i].label = ec_as_row_label(i);
        rows[i].value = val[i];
        rows[i].dim = 0;
        rows[i].note = 0;
    }
    ec_scr_header(a, "APP/SETTINGS", 0, "");
    ec_kit_settings(rows, EC_AS_NROW, a->pn.as_row / 4, a->pn.as_row);
    for (i = 0; i < ec_gfx_themes(); i++)          /* THEME: the six, the current one marked */
        R(i == a->pn.as.theme ? EC_C_L15 : EC_C_L5, 26 + i * 4, 21, 3, i == a->pn.as.theme ? 3 : 2);
}

/* ------------------------------------------------------------------------------------------- dispatch --- */
int ec_scr_modes_full(const ec_app_t *a)
{
    if (a->pn.as_open) {
        appset_screen(a);
        return 1;
    }
    if (a->pn.cat && !a->settings) {
        cat_screen(a);
        return 1;
    }
    if (a->settings)
        return 0;
    switch (a->scr) {
    case EC_MODE_PAGES: pages_screen(a); return 1;
    case EC_MODE_KEYBOARD: keyboard_screen(a); return 1;
    case EC_MODE_SAMPLER: ec_scr_rec(a); return 1;   /* (part 2: the SAMPLER; 0.4's REC pop-up stands in) */
    default: break;
    }
    if (a->scr)
        return 0;
    switch (a->cat) {
    case EC_CAT_XFD: xfd_screen(a); return 1;
    case EC_CAT_PAT: patterns_screen(a); return 1;
    default: return 0;
    }
}

void ec_scr_modes_overlay(const ec_app_t *a)
{
    int cur = -1;
    const char *roles;
    const int mode = a->pn.pop - 1;
    if (!a->pn.pop || ec_app_popup(a) || ec_app_shift_map(a))
        return;
    roles = ec_pn_roles(mode, &cur);
    ec_gfx_dim();
    ec_kit_popup(ec_mode_name(mode), ec_pn_page_name(mode), roles, cur);
}
