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
    snprintf(t, sizeof t, "%s/%s", ec_cat_tag(EC_CAT_FILE), pg->title ? pg->title : "BROWSE");
    if (n > 0)
        snprintf(r, sizeof r, "%d/%d", a->cursor + 1, n);
    else
        r[0] = 0;
    ec_scr_header(a, t, 1, r);
    snprintf(buf, sizeof buf, "%s/",
             !a->pn.fdir[0] ? "/audio" : strcmp(a->pn.fdir, "/") ? a->pn.fdir : "");   /* (0.6.1: "/" the top) */
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
            const int pooled = path ? ec_file_pooled(path) >= 0      /* (0.6.1: in PROJECTS/, the open project) */
                                    : !isd && !strcmp(a->pn.fdir, EC_PF_DIR) && !strcmp(name, ec_pf_name());
            const float sec = path ? known_len(a, path) : 0;
            fit(buf, sizeof buf, name, sec > 0 ? 20 : 26);
            if (cur)
                R(EC_C_L15, 0, y - 1, W, 7);
            T(cur ? EC_C_BG : isd ? EC_C_L12 : EC_C_L8, buf, 6, y, EC_AL_LEFT);
            if (sec > 0) {
                snprintf(r, sizeof r, "%.1fS", (double)sec);
                T(cur ? EC_C_BG : EC_C_L5, r, W - 2, y, EC_AL_RIGHT);
            }
            if (pooled)
                R(cur ? EC_C_BG : EC_C_ACC, 2, y + 1, 2, 3);
            if (cur && a->pn.pv_play && path && !strcmp(path, a->pn.pv_path))
                R(EC_C_ACC, 0, y - 1, 2, 7);       /* previewing */
        }
    } else
        T2(EC_C_L5, a->scanning || (!strcmp(a->pn.fdir, EC_PF_DIR) && ec_pf_list_busy()) ? "SCANNING"
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
    ec_scr_cells(a, pg);
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
    snprintf(r, sizeof r, "%dX", (int)zf);
    ec_scr_header(a, t, ec_cat_page_key(EC_CAT_FILE, a->page[EC_CAT_FILE]), r);
    fit(buf, sizeof buf, x ? x->name : sl->path[0] ? (strrchr(sl->path, '/') ? strrchr(sl->path, '/') + 1 : sl->path)
                                                 : "EMPTY", 20);
    T(x ? EC_C_L15 : EC_C_L5, buf, 2, 13, EC_AL_LEFT);
    snprintf(r, sizeof r, "SLOT %d", s + 1);
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
        if (sx >= x0 && sx < x1) {                 /* the trim's start and end markers */
            R(EC_C_ACC, sx, 20, 1, 40);
            R(EC_C_ACC, sx, 20, 4, 3);
        }
        if (ex > x0 && ex <= x1) {
            const int e = ex > x1 - 1 ? x1 - 1 : ex;
            R(EC_C_ACC, e, 20, 1, 40);
            R(EC_C_ACC, e - 3, 20, 4, 3);
        }
    }
    for (i = 0; i < 16; i++) {                     /* the trim keys W1..W16 (across the view) */
        const float pos = v0 + (float)i / 15.0f * vw;
        const int set = fabsf(pos - sl->trim_start) < vw / 30.0f || fabsf(pos - sl->trim_end) < vw / 30.0f;
        R(set ? EC_C_ACC : EC_C_L5, 3 + i * 7, 64, 5, set ? 4 : 2);
    }
    R(EC_C_L3, 2, 71, W - 4, 2);                   /* where the view sits in the file */
    R(EC_C_L12, 2 + (int)(v0 * (float)(W - 4)), 71, clampi((int)(vw * (float)(W - 4) + 0.5f), 1, W - 4), 2);
    if (a->pn.ed_tap)
        T(EC_C_ACC, "END", W - 2, 64, EC_AL_RIGHT);
    ec_scr_cells(a, pg);
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
            R(i < lit ? (i >= 18 ? EC_C_ACC : EC_C_L15) : EC_C_L3, 12 + i * 4 + (i / 6), y, 3, 10);
    }
    {
        const uint32_t bar = (uint32_t)floor(4.0 * 60.0 * EC_FS / (a->seq.bpm > 1 ? a->seq.bpm : a->proj.bpm) + 0.5);
        const int armed = rc->state == EC_RS_ARMED, run = rc->state == EC_RS_RUN || rc->state == EC_RS_STOPPED;
        const int cur = bar ? (int)(rc->n / bar) : 0;
        if (armed || run) {                        /* armed: a blinking dot; taking: the dot lit */
            const int c = run || ((a->ms / 250) & 1u) ? EC_C_ACC : EC_C_L5;
            R(c, 4, 52, 6, 6), R(c, 3, 53, 8, 4), R(c, 5, 51, 4, 8);
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
    for (i = 0; i < EC_RS_NROW; i++) {
        ec_rs_row_value(i, val[i], sizeof val[i]);
        rows[i].label = ec_rs_row_label(i);
        rows[i].value = val[i];
        rows[i].dim = (uint8_t)ec_rs_row_dim(i);
        rows[i].note = 0;
    }
    ec_scr_header(a, "REC/SETUP", 0, "");
    ec_kit_settings(rows, EC_RS_NROW, a->pn.rs_row / 4, a->pn.rs_row);
}
