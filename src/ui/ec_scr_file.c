/* SPDX-License-Identifier: GPL-3.0-only */
/* 0.6.0: FILE's browser and editor, the SAMPLER and the record settings, in the approved mockups' geometry
 * (host/mock_ux2.c scr_file_browser, scr_file_editor, scr_sampler, scr_record): the 120x120 grid, palette slots. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ec_browse.h"
#include "ec_gfx.h"
#include "ec_loader.h"
#include "ec_projfile.h"
#include "ec_scr_modes.h"
#include "ec_screens.h"
#include "ec_ui_kit.h"

#define W EC_GW
#define R(c, x, y, w, h) ec_gfx_rect((x), (y), (w), (h), (c))
#define T(c, s, x, y, al) ec_gfx_text_al((x), (y), (s), 1, (c), (al))
#define T2(c, s, x, y, al) ec_gfx_text_al((x), (y), (s), 2, (c), (al))

static int clampi(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }

static void fit(char *out, size_t cap, const char *s, int n)
{
    const int l = (int)strlen(s);
    if (l <= n)
        snprintf(out, cap, "%s", s);
    else
        snprintf(out, cap, "%.*s..", n - 2, s);
}

/* the seconds of a file the browser knows (in the pool, or previewed), 0 unknown */
static float known_len(const ec_app_t *a, const char *path)
{
    const int s = ec_file_pooled(path);
    const ec_ss_sample_t *x = 0;
    if (s >= 0 && a->slot[s].handle >= 0)
        x = ec_ss_ready(a->slot[s].handle);
    else if (a->pn.pv_state == EC_PV_READY && !strcmp(a->pn.pv_path, path))
        x = ec_ss_ready(a->pn.pv_handle);
    return x && x->rate ? (float)x->frames / (float)x->rate : 0;
}

/* ------------------------------------------------------------------------------------------- the browser --- */
void ec_scr_file_browser(const ec_app_t *a, const ec_page_t *pg)
{
    const int n = ec_file_count();
    const int nf = ec_file_next_free();
    char t[32], r[16], buf[64];
    int i;
    (void)pg;
    snprintf(t, sizeof t, "%s/SAMPLES", ec_cat_tag(EC_CAT_FILE));   /* 0.6.43 (owner): FILE's halves: SAMPLES, PROJECTS */
    if (n > 0)
        snprintf(r, sizeof r, "%d/%d", a->cursor + 1, n);
    else
        r[0] = 0;
    ec_scr_header(a, t, 1, r);
    {   /* 0.6.42: rooted at AUDIO/: "AUDIO/BREAKS/" (no leading slash, upper case by the font) */
        const char *d = a->pn.fdir[0] && !strncmp(a->pn.fdir, "/audio", 6) ? a->pn.fdir : "/audio";
        snprintf(buf, sizeof buf, "%s/", d + 1);
    }
    {
        char f[32];
        const int l = (int)strlen(buf);
        if (l > 20)                                /* (the folder's end: where you are) */
            snprintf(f, sizeof f, "..%s", buf + l - 18);
        else
            snprintf(f, sizeof f, "%s", buf);
        T(EC_C_L5, f, 2, 13, EC_AL_LEFT);
    }
    if (nf >= 0)
        snprintf(r, sizeof r, "SLOT %d", nf + 1);
    else
        snprintf(r, sizeof r, "FULL");
    T(nf >= 0 ? EC_C_L8 : EC_C_ALERT, r, W - 2, 13, EC_AL_RIGHT);
    if (n > 0) {
        int first = a->cursor - 2;
        if (first > n - 5)
            first = n - 5;
        if (first < 0)
            first = 0;
        for (i = 0; i < 5 && first + i < n; i++) {
            const int idx = first + i, y = 21 + i * 8, cur = idx == a->cursor;
            int isd, f;
            const char *name = ec_file_entry(idx, &isd, &f);
            const char *path = !isd && f >= 0 ? ec_browse_path(f) : 0;
            const int pooled = path && ec_file_pooled(path) >= 0;
            const float sec = path ? known_len(a, path) : 0;
            if (f == EC_FILE_UP) {                 /* 0.6.42 (owner): UP, an arrow, where it goes on the right */
                const int c = cur ? EC_C_BG : EC_C_L12;
                char par[24];
                const char *d = a->pn.fdir, *sl = strrchr(d, '/');
                int l = sl ? (int)(sl - d) : 0;
                const char *ps = l > 0 ? d : "/audio";
                const char *pl = ps;
                if (l <= 0)
                    l = 6;
                {   /* the parent's own name: the last component before the folder shown */
                    int k;
                    for (k = l - 1; k > 0 && ps[k - 1] != '/'; k--)
                        ;
                    pl = ps + k;
                    l -= k;
                }
                snprintf(par, sizeof par, "%.*s/", l > 18 ? 18 : l, pl);
                if (cur)
                    R(EC_C_L15, 0, y - 2, W, 9);
                R(c, 2, y - 1, 2, 2), R(c, 0, y + 1, 6, 2), R(c, 2, y + 3, 2, 3);   /* (2-unit lattice) */
                T(c, "UP", 8, y, EC_AL_LEFT);
                T(cur ? EC_C_BG : EC_C_L5, par, W - 2, y, EC_AL_RIGHT);
                continue;
            }
            fit(buf, sizeof buf, name, sec > 0 ? 20 : 26);
            if (cur)                               /* 0.6.1: 2 units round the name */
                R(EC_C_L15, 0, y - 2, W, 9);
            T(cur ? EC_C_BG : isd ? EC_C_L12 : EC_C_L8, buf, 6, y, EC_AL_LEFT);
            if (sec > 0) {
                snprintf(r, sizeof r, "%.1fS", (double)sec);
                T(cur ? EC_C_BG : EC_C_L5, r, W - 2, y, EC_AL_RIGHT);
            }
            if (pooled)
                R(cur ? EC_C_BG : EC_C_ACC, 2, y + 1, 2, 3);
            if (cur && a->pn.pv_play && path && !strcmp(path, a->pn.pv_path))
                R(EC_C_ACC, 0, y - 2, 2, 9);       /* previewing */
        }
    } else
        T2(EC_C_L5, a->scanning ? "SCANNING"
                    : a->no_folder ? "NO FOLDER" : "NO FILES", W / 2, 32, EC_AL_CENTER);
    if (a->load_slot >= 0) {                       /* a slot loading, else the pool meter */
        const int pm = (int)ec_load_permille();
        snprintf(buf, sizeof buf, "LOADING %d %d%%", a->load_slot + 1, pm / 10);
        T(EC_C_L8, buf, 2, 61, EC_AL_LEFT);
        R(EC_C_L3, 2, 68, W - 4, 4);
        R(EC_C_L15, 2, 68, (W - 4) * pm / 1000, 4);
    } else {
        const uint32_t cap = ec_ss_capacity_bytes(), use = cap - ec_ss_free_frames() * 4u;
        const int pc = cap ? (int)((uint64_t)use * 100u / cap) : 0;
        const int bw = cap ? (int)((uint64_t)use * (uint32_t)(W - 4) / cap) : 0;
        snprintf(buf, sizeof buf, "POOL %d/%d", ec_file_slots_used(), EC_NSLOT);
        T(EC_C_L8, buf, 2, 61, EC_AL_LEFT);
        snprintf(buf, sizeof buf, "%d%%", pc);
        T(EC_C_L8, buf, W - 2, 61, EC_AL_RIGHT);
        R(EC_C_L3, 2, 68, W - 4, 4);
        R(EC_C_L12, 2, 68, bw, 4);
        if (bw > 1)
            R(EC_C_ACC, 2 + bw - 2, 68, 2, 4);
    }
    {   /* 0.6.15 (owner): no knob edits these, so no cell boxes: plain rows (label, value) in the cells area */
        int k;
        R(EC_C_L3, 0, 76, W, 2);
        for (k = 0; k < pg->n && k < 4; k++) {
            const ec_item_t *it = &pg->it[k];
            const int x = k % 2 ? 62 : 2, y = k < 2 ? 82 : 102;
            char val[24];
            if (it->kind == EC_IT_BLANK)
                continue;
            ec_app_item_format(a, it, ec_app_item_value(a, it, 0), val, sizeof val);
            T(EC_C_L8, ec_app_item_label(it), x, y, EC_AL_LEFT);
            T2(EC_C_L15, val, x, y + 7, EC_AL_LEFT);
        }
    }
}

/* ------------------------------------------------------------------------------------------- the editor --- */
void ec_scr_file_editor(const ec_app_t *a, const ec_page_t *pg)
{
    const int s = ec_app_track_slot(a, a->track);
    const ec_slot_t *sl = &a->proj.slot[s];
    const ec_ss_sample_t *x = ec_app_track_sample(a, a->track);
    const float zf = ec_file_zoom(), v0 = a->pn.ed_view, vw = 1.0f / zf;
    const int x0 = 2, x1 = W - 2, cols = x1 - x0;
    char t[32], r[16], buf[64];
    int i;
    snprintf(t, sizeof t, "%s/EDIT", ec_cat_tag(EC_CAT_FILE));
    ec_scr_header_page(a, t, ec_cat_page_key(EC_CAT_FILE, a->page[EC_CAT_FILE]), a->pn.ed_point ? "END" : "START");
    fit(buf, sizeof buf, x ? x->name : sl->path[0] ? (strrchr(sl->path, '/') ? strrchr(sl->path, '/') + 1 : sl->path)
                                                 : "EMPTY", 16);
    T(x ? EC_C_L15 : EC_C_L5, buf, 2, 13, EC_AL_LEFT);
    snprintf(r, sizeof r, "SLOT %d %dX", s + 1, (int)zf);   /* (0.6.1: the slot, the zoom; the header the point) */
    T(EC_C_L8, r, W - 2, 13, EC_AL_RIGHT);
    {   /* the view's columns: each one's peak, read from the sample (at most 256 frames a column) */
        static uint8_t ov[EC_GW];
        const int sx = x0 + (int)floorf((sl->trim_start - v0) / vw * (float)cols + 0.5f);
        const int ex = x0 + (int)floorf((sl->trim_end - v0) / vw * (float)cols + 0.5f);
        memset(ov, 0, sizeof ov);
        if (x && x->frames) {
            for (i = 0; i < cols; i++) {
                const double f0 = ((double)v0 + (double)vw * i / cols) * x->frames;
                const double f1 = ((double)v0 + (double)vw * (i + 1) / cols) * x->frames;
                uint32_t a0 = (uint32_t)f0, a1 = (uint32_t)f1, k, st;
                int pk = 0;
                if (a1 <= a0)
                    a1 = a0 + 1;
                if (a1 > x->frames)
                    a1 = x->frames;
                st = (a1 - a0) / 256u + 1u;
                for (k = a0; k < a1; k += st) {
                    const int l = abs(x->data[2 * k]), rr = abs(x->data[2 * k + 1]);
                    pk = l > pk ? l : pk;
                    pk = rr > pk ? rr : pk;
                }
                ov[i] = (uint8_t)((pk >> 7) > 255 ? 255 : pk >> 7);
            }
            ec_kit_wave(ov, cols, x0, x1, 22, 58, EC_C_L15, EC_C_L5, sx, ex);
        } else
            T2(EC_C_L5, "NO SAMPLE", W / 2, 34, EC_AL_CENTER);
        if (sx >= x0 && sx < x1) {                 /* the trim's start and end markers (the point F1 / F2 picked: ACC) */
            const int c = a->pn.ed_point ? EC_C_L12 : EC_C_ACC;
            const int m = sx > x1 - 2 ? x1 - 2 : sx;
            R(c, m, 20, 2, 40);
            R(c, m, 20, 5, 3);
        }
        if (ex > x0 && ex <= x1) {
            const int c = a->pn.ed_point ? EC_C_ACC : EC_C_L12;
            const int e = ex > x1 - 2 ? x1 - 2 : ex;
            R(c, e, 20, 2, 40);
            R(c, e - 3, 20, 5, 3);
        }
    }
    for (i = 0; i < 16; i++) {                     /* the trim keys W1..W16 (across the view) */
        const float pos = v0 + (float)i / 15.0f * vw;
        const int set = fabsf(pos - sl->trim_start) < vw / 30.0f || fabsf(pos - sl->trim_end) < vw / 30.0f;
        R(set ? EC_C_ACC : EC_C_L5, 3 + i * 7, 64, 5, set ? 4 : 2);
    }
    R(EC_C_L3, 2, 69, W - 4, 2);                   /* where the view sits in the file */
    R(EC_C_L12, 2 + (int)(v0 * (float)(W - 4)), 69, clampi((int)(vw * (float)(W - 4) + 0.5f), 2, W - 4), 2);
    ec_scr_cells(a, pg);
}

/* ------------------------------------------------------------------------------------------- the pool --- */
/* 0.6.1 (docs/UX-V2.md 10 "FILE / POOL page"): the 128 slots, empty ones included: the number, the file (or a dash),
 * its length, the six tracks of the live pattern that play it (T1..T6 marks; one only step-locked: a LOCK bar). The
 * header: the pool meter (slots used, the arena's use, its bar under the header); MOVE n while a slot is carried. */
void ec_scr_file_pool(const ec_app_t *a, const ec_page_t *pg)
{
    static uint8_t tracks[EC_NSLOT], locks[EC_NSLOT];
    const int cur = clampi(a->pn.pool_cur, 0, EC_NSLOT - 1), carry = a->pn.pool_carry, rows = 12;
    const uint32_t cap = ec_ss_capacity_bytes(), use = cap - ec_ss_free_frames() * 4u;
    const int bw = cap ? (int)((uint64_t)use * (uint32_t)(W - 4) / cap) : 0;
    char t[24], r[16], buf[40];
    int first = cur - rows / 2, i, k;
    (void)pg;
    ec_file_pool_scan(tracks, locks);
    if (carry >= 0)
        snprintf(t, sizeof t, "MOVE %d", carry + 1);
    else
        snprintf(t, sizeof t, "POOL %d/%d", ec_file_slots_used(), EC_NSLOT);
    snprintf(r, sizeof r, "%d%%", cap ? (int)((uint64_t)use * 100u / cap) : 0);
    ec_scr_header(a, t, ec_cat_page_key(EC_CAT_FILE, a->page[EC_CAT_FILE]), r);
    R(EC_C_L3, 2, 12, W - 4, 4);                   /* the arena: used / free */
    R(EC_C_L12, 2, 12, bw, 4);
    if (bw > 1)
        R(EC_C_ACC, 2 + bw - 2, 12, 2, 4);
    if (first > EC_NSLOT - rows)
        first = EC_NSLOT - rows;
    if (first < 0)
        first = 0;
    for (i = 0; i < rows; i++) {
        const int s = first + i, y = 21 + i * 8, on = s == cur;
        /* 0.6.15 (owner): a slot in hand: the list shows the result (the swap applied), the moving rows nudged */
        const int vs = carry >= 0 ? (s == cur ? carry : s == carry ? cur : s) : s;
        const int moving = carry >= 0 && (s == cur || s == carry);
        const int dx = moving && ((a->ms / 90) % 4 == 1 || (a->ms / 90) % 4 == 2) ? 2 : 0;
        const ec_slot_t *sl = &a->proj.slot[vs];
        const ec_ss_sample_t *x = a->slot[vs].handle >= 0 ? ec_ss_ready(a->slot[vs].handle) : 0;
        const int used = sl->path[0] || x || a->slot[s].state != EC_SL_EMPTY;
        const int fg = on ? EC_C_BG : used ? EC_C_L15 : EC_C_L5;
        if (on)
            R(carry >= 0 ? EC_C_ACC : EC_C_L15, 0, y - 2, W, 9);   /* the cursor (carrying: where it drops) */
        snprintf(r, sizeof r, "%d", s + 1);
        T(on ? EC_C_BG : s == carry ? EC_C_ACC : EC_C_L8, r, 14, y, EC_AL_RIGHT);
        if (s == carry && !on)
            R(EC_C_ACC, 0, y - 2, 2, 9);           /* the slot in hand: where it came from */
        if (used) {
            const char *n = x ? x->name : sl->path[0] ? (strrchr(sl->path, '/') ? strrchr(sl->path, '/') + 1
                                                                                  : sl->path) : "...";
            fit(buf, sizeof buf, n, 13);
            T(fg, buf, 18 + dx, y, EC_AL_LEFT);
            if (x && x->rate) {
                const float sec = (float)x->frames / (float)x->rate;
                snprintf(r, sizeof r, sec < 10.0f ? "%.1fS" : "%.0fS", (double)sec);
                T(on ? EC_C_BG : EC_C_L8, r, 94, y, EC_AL_RIGHT);
            }
        } else
            R(fg, 18, y + 2, 6, 2);                /* (empty: a dash) */
        if (tracks[vs]) {
            for (k = 0; k < EC_NTRACK; k++)        /* T1..T6 */
                R(tracks[vs] >> k & 1u ? (on ? EC_C_BG : k == a->track && !a->bus ? EC_C_ACC : EC_C_L15)
                                       : on ? EC_C_L12 : EC_C_L3, 98 + k * 4, y, 2, 5);   /* (the selected: ACC) */
        } else if (locks[vs])
            R(EC_C_LOCK, 98, y, 22, 5);            /* only a step lock plays it */
    }
}

/* ------------------------------------------------------------------------------------------- SAMPLER --- */
void ec_scr_sampler(const ec_app_t *a)
{
    const ec_rec_t *rc = &a->rec;
    ec_page_t pg;
    char r[24], buf[32];
    int i, ch;
    const int bars = ec_sampler_len();
    const int slot = rc->state != EC_RS_IDLE ? rc->slot : ec_file_next_free();
    if (slot >= 0)
        snprintf(r, sizeof r, "SLOT %d", slot + 1);
    else
        snprintf(r, sizeof r, "FULL");
    ec_scr_header(a, ec_mode_name(EC_MODE_SAMPLER), 0, r);
    T(EC_C_L8, rc->src == EC_RSRC_MST ? "IN MASTER" : "IN AIN1", 2, 13, EC_AL_LEFT);
    T(EC_C_L8, "/DATA/REC", W - 2, 13, EC_AL_RIGHT);
    for (ch = 0; ch < 2; ch++) {                   /* the VU: 24 segments, -48 .. 0 dB */
        const float pk = rc->in_meter[ch];
        const float db = pk > 1e-6f ? 20.0f * log10f(pk) : -120.0f;
        const int lit = clampi((int)floorf((db + 48.0f) / 2.0f + 0.5f), 0, 24);
        const int y = 22 + ch * 13;
        T2(EC_C_L8, ch ? "R" : "L", 2, y, EC_AL_LEFT);
        for (i = 0; i < 24; i++)
            R(i < lit ? (i >= 18 ? EC_C_ACC : EC_C_L15) : EC_C_L3, 12 + i * 4 + (i / 6), y, 2, 10);   /* 2-unit gaps */
    }
    {
        const uint32_t bar = (uint32_t)floor(4.0 * 60.0 * EC_FS / (a->seq.bpm > 1 ? a->seq.bpm : a->proj.bpm) + 0.5);
        const int armed = rc->state == EC_RS_ARMED, run = rc->state == EC_RS_RUN || rc->state == EC_RS_STOPPED;
        const int cur = bar ? (int)(rc->n / bar) : 0;
        if (armed || run) {                        /* armed: a blinking dot; taking: the dot lit */
            const int c = run || ((a->ms / 250) & 1u) ? EC_C_ACC : EC_C_L5;
            R(c, 3, 53, 8, 4), R(c, 5, 51, 4, 8);  /* 0.6.1: corners cut in 2-unit steps */
            T2(c, armed ? "ARMED" : "REC", 14, 50, EC_AL_LEFT);
            /* taking: the take's bar; armed while playing: the song's bar it starts on (the next) */
            snprintf(buf, sizeof buf, "BAR %d", run ? cur + 1 : (int)floor(ec_seq_song_beats(&a->seq) / 4.0) + 2);
            if (run || a->seq.playing)
                T2(EC_C_L15, buf, W - 2, 50, EC_AL_RIGHT);
        } else
            T2(EC_C_L5, rc->takes ? "READY" : "B11 ARMS", 14, 50, EC_AL_LEFT);
        for (i = 0; i < 8; i++) {                  /* the take's bars: the length lit (FREE: all 8), taking: done / now */
            const int want = bars ? i < bars : 1;
            const int done = run && i < cur, now = run && i == cur;
            R(done ? EC_C_L12 : now ? EC_C_ACC : want ? (run ? EC_C_L5 : EC_C_L12) : EC_C_L3, 3 + i * 15, 66, 13, 4);
        }
    }
    ec_app_page(a, &pg);
    ec_scr_cells(a, &pg);
}

/* ------------------------------------------------------------------------------------------- REC settings --- */
void ec_scr_record(const ec_app_t *a)
{
    static char val[EC_RS_NROW][16];
    ec_kit_row_t rows[EC_RS_NROW];
    int i;
    memset(rows, 0, sizeof rows);
    for (i = 0; i < EC_RS_NROW; i++) {
        ec_rs_row_value(i, val[i], sizeof val[i]);
        rows[i].label = ec_rs_row_label(i);
        rows[i].value = val[i];
        rows[i].dim = (uint8_t)ec_rs_row_dim(i);
    }
    ec_scr_header(a, "REC/SETUP", 0, "");
    ec_kit_list(rows, EC_RS_NROW, a->pn.rs_row, 12, EC_GH, 0);   /* 0.6.1: a list menu */
}

/* ------------------------------------------------------------------------------------------- PROJECTS --- */
/* 0.6.42 (owner, docs/PROJECTS-DESIGN.md): the project you work on and whether it is saved; every project, newest first
 * (the loaded one marked OPEN); the one under the cursor's card: its name big, its 64 patterns (a block each, written
 * cream, the one it opens on in the accent), its tempo and samples. A question waiting: the header asks it with the
 * back / OK arrows (OCT- no, OCT+ yes), the line under the card says what yes does */
static void projects_body(const ec_app_t *a)
{
    static const char *const TN[9] = {"T1", "T2", "T3", "T4", "T5", "T6", "S1", "S2", "MST"};
    const int n = ec_pf_list_count(), cur = ec_prj_cursor(), ask = ec_prj_ask();
    const char *open = ec_pf_name();
    const char *sel = ec_pf_list_name(cur);
    char buf[48], r[16];
    int i;
    if (ask) {
        snprintf(buf, sizeof buf, ask == EC_PRJ_ASK_NEW ? "NEW PROJECT?" : ask == EC_PRJ_ASK_DELETE ? "DELETE %s?"
                                  : ask == EC_PRJ_ASK_OVERWRITE ? "OVERWRITE %s?" : "LOAD %s?", ec_prj_target());
        fit(r, sizeof r, buf, 15);
        ec_kit_header(TN[a->bus ? EC_NTRACK - 1 + a->bus : a->track], r, "", "", 0, 1);
    } else {
        if (n > 0)
            snprintf(r, sizeof r, "%d/%d", cur + 1, n);
        else
            r[0] = 0;
        ec_scr_header(a, "FILE/PROJECTS", 0, r);
    }
    T(EC_C_L5, "NOW", 2, 13, EC_AL_LEFT);
    fit(buf, sizeof buf, open[0] ? open : "NOT SAVED YET", 16);
    T(EC_C_L15, buf, 18, 13, EC_AL_LEFT);
    if (ec_pf_saving())
        T(EC_C_L8, "SAVING", W - 2, 13, EC_AL_RIGHT);
    else if (ec_pf_dirty())
        T(EC_C_ACC, "EDITED", W - 2, 13, EC_AL_RIGHT);
    else
        T(EC_C_L5, "SAVED", W - 2, 13, EC_AL_RIGHT);
    if (n > 0) {
        int first = cur - 2;
        if (first > n - 5)
            first = n - 5;
        if (first < 0)
            first = 0;
        for (i = 0; i < 5 && first + i < n; i++) {
            const int idx = first + i, y = 22 + i * 8, c = idx == cur;
            const char *nm = ec_pf_list_name(idx);
            const int isopen = !strcmp(nm, open);
            fit(buf, sizeof buf, nm, 22);
            if (c)
                R(EC_C_L15, 0, y - 2, W, 9);
            T(c ? EC_C_BG : EC_C_L8, buf, 6, y, EC_AL_LEFT);
            if (isopen) {
                R(c ? EC_C_BG : EC_C_ACC, 2, y, 2, 4);
                T(c ? EC_C_BG : EC_C_ACC, "OPEN", W - 2, y, EC_AL_RIGHT);
            }
        }
    } else
        T2(EC_C_L5, ec_pf_list_busy() ? "READING" : "NO PROJECTS", W / 2, 34, EC_AL_CENTER);
    R(EC_C_L3, 0, 62, W, 2);
    if (n <= 0)
        return;
    {   /* the card */
        const ec_project_card_t *cd = ec_pf_card(sel);
        const int big = (int)strlen(sel) <= 13;
        fit(buf, sizeof buf, sel, big ? 13 : 28);
        if (big)
            T2(ask ? EC_C_ACTION : EC_C_L15, buf, 2, 67, EC_AL_LEFT);
        else
            T(ask ? EC_C_ACTION : EC_C_L15, buf, 2, 70, EC_AL_LEFT);
        for (i = 0; i < 64; i++) {             /* the patterns: 16 x 4 blocks of 4, 2 apart (the 2-unit lattice) */
            const int x = 12 + (i % 16) * 6, y = 80 + (i / 16) * 6;
            const int used = cd && cd->patterns_known && (cd->used >> i & 1u);
            R(!cd ? EC_C_L3 : used && i == cd->current ? EC_C_ACC : used ? EC_C_L12 : EC_C_L3, x, y, 4, 4);
        }
        if (ask == EC_PRJ_ASK_DELETE)          /* 0.6.45: what yes does */
            T(EC_C_ALERT, "GONE FOR GOOD", 2, 110, EC_AL_LEFT);
        else if (ask == EC_PRJ_ASK_OVERWRITE)
            T(EC_C_ACC, "THIS PROJECT REPLACES IT", 2, 110, EC_AL_LEFT);
        else if (ask) {                        /* what yes does to the project you leave */
            if (ec_pf_dirty()) {
                fit(buf, sizeof buf, open[0] ? open : "THIS ONE", 14);
                snprintf(r, sizeof r, "%.14s", buf);
                snprintf(buf, sizeof buf, "%s SAVED FIRST", r);
                T(EC_C_ACC, buf, 2, 110, EC_AL_LEFT);
            }
        } else if (cd) {
            snprintf(buf, sizeof buf, "%d BPM", (int)(cd->bpm + 0.5f));
            T(EC_C_L8, buf, 2, 110, EC_AL_LEFT);
            snprintf(buf, sizeof buf, "%d SAMPLE%s", cd->samples, cd->samples == 1 ? "" : "S");
            T(EC_C_L8, buf, W - 2, 110, EC_AL_RIGHT);
        }
    }
}

void ec_scr_projects(const ec_app_t *a)
{
    const int f = ec_prj_popup_frame();
    projects_body(a);
    if (f >= 0) {                                  /* 0.6.46 (owner): loading: the cat dances over the card */
        R(EC_C_BG, 0, 52, W, EC_GH - 52);
        R(EC_C_ACC, 0, 52, W, 2);
        ec_kit_cat_frame(f, 0, EC_GH - 64);
    }
}
