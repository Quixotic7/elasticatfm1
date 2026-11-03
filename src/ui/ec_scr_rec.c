/* SPDX-License-Identifier: GPL-3.0-only */
/* E6's screens: the REC pop-up (design/mockup/Display.dc.html mode "rec": the source and the target slot, the take's
 * waveform with the record head in the accent, the bar ruler, the IN meters with the threshold's tick, LEN / THR /
 * SYNC) and the INPUT machine's view on SRC (the input's meters, NO INPUT). The 120x120 grid in palette slots (ec_gfx.h). */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ec_gfx.h"
#include "ec_screens.h"
#include "ec_ui_kit.h"

#define W EC_GW
#define H EC_GH
#define R(c, x, y, w, h) ec_gfx_rect((x), (y), (w), (h), (c))
#define T(c, s, x, y, al) ec_gfx_text_al((x), (y), (s), 1, (c), (al))
#define T2(c, s, x, y, al) ec_gfx_text_al((x), (y), (s), 2, (c), (al))
#define TW(s) (ec_gfx_text_w((s), 1))

static float db_frac(float p)
{
    const float db = 20.0f * log10f(p > 1e-4f ? p : 1e-4f);
    const float f = (db + 60.0f) / 60.0f;
    return f < 0 ? 0 : f > 1 ? 1 : f;
}

/* a horizontal level meter in dB (-60..0), its peak bright; full scale lights the accent */
static void meter(int x, int y, int w, int h, float p)
{
    const int n = (int)(db_frac(p) * (float)w + 0.5f);
    R(EC_C_L3, x, y, w, h);
    R(p >= 0.98f ? EC_C_ACC : EC_C_L12, x, y, n, h);
}

static void lat_text(char *b, size_t cap)
{
    const uint32_t l = ec_plat_ain_latency();
    if (l)
        snprintf(b, cap, "LAT %.1fMS", (double)l * 1000.0 / EC_FS);
    else
        snprintf(b, cap, "LAT ?");
}

/* ------------------------------------------------------------------------------------------- INPUT on SRC --- */
void ec_scr_input(const ec_app_t *a, int top, int bot)
{
    const ec_rec_t *r = &a->rec;
    char b[24];
    if (ec_plat_ain_state() != EC_AIN_LIVE) {
        ec_gfx_dots_h(2, (W - 2), ((top + bot) / 2), 2, EC_C_L3);
        T2(EC_C_ALERT, "NO INPUT", W / 2, (top + bot) / 2 - 5, EC_AL_CENTER);
        return;
    }
    T(EC_C_L8, "L", 2, top + 6, EC_AL_LEFT);
    meter(8, top + 5, W - 10, 7, r->in_meter[0]);
    T(EC_C_L8, "R", 2, top + 17, EC_AL_LEFT);
    meter(8, top + 16, W - 10, 7, r->in_meter[1]);
    {   /* this track's output (after its filter, insert and amp) */
        const float o = a->meter[a->track] > 1 ? 1 : a->meter[a->track];
        T(EC_C_L5, "OUT", 2, top + 28, EC_AL_LEFT);
        meter(16, top + 29, W - 18, 2, o);
    }
    lat_text(b, sizeof b);
    T(EC_C_L5, b, W - 2, bot - 5, EC_AL_RIGHT);
}

/* ------------------------------------------------------------------------------------------- the REC pop-up --- */
/* the take's waveform from the store (each column's peak, a few hundred frames read) up to the head */
static void take_wave(const int16_t *d, uint32_t n, uint32_t total, int x0, int cols, int top, int bot)
{
    const int mid = (top + bot) / 2, half = (bot - top) / 2;
    const int used = total ? (int)((uint64_t)cols * n / total) : 0;
    int c;
    for (c = 0; c + 1 < used && c + 1 < cols; c += 2) {          /* 0.6.1: 2-unit columns */
        const uint32_t a = (uint32_t)((uint64_t)c * total / (uint32_t)cols);
        uint32_t e = (uint32_t)((uint64_t)(c + 2) * total / (uint32_t)cols), i, step;
        int pk = 0, h;
        if (e > n)
            e = n;
        step = (e > a ? e - a : 1) / 256u + 1u;
        for (i = a; i < e; i += step) {
            const int l = d[2 * i] < 0 ? -d[2 * i] : d[2 * i], rr = d[2 * i + 1] < 0 ? -d[2 * i + 1] : d[2 * i + 1];
            pk = l > pk ? l : pk;
            pk = rr > pk ? rr : pk;
        }
        h = (int)((float)pk / 32768.0f * (float)(half - 1) + 0.5f);
        R(EC_C_L15, x0 + c, mid - h, 2, 2 * h + 2);
    }
}

void ec_scr_rec(const ec_app_t *a)
{
    static const char *const SRC[] = {"IN", "MST"}, *const SYNC[] = {"PLAY", "BAR", "FREE"};
    const ec_rec_t *r = &a->rec;
    const int x0 = 2, x1 = W - 2, cols = x1 - x0, top = 24, bot = 56;
    const int live = ec_plat_ain_state() == EC_AIN_LIVE;
    const int taking = r->state == EC_RS_RUN || r->state == EC_RS_STOPPED;
    const int slot = r->state == EC_RS_IDLE && !r->slot_set ? ec_app_track_slot(a, a->track) : r->slot;
    const float bpm = (float)(a->seq.bpm > 1 ? a->seq.bpm : a->proj.bpm);
    uint32_t total = r->target ? r->target : ec_rec_len_frames(r->len, bpm);
    char b[40], t[24];
    int i, nt;

    snprintf(t, sizeof t, "REC/%s", SRC[r->src]);
    ec_scr_header(a, t, 1, r->state == EC_RS_ARMED ? "ARMED" : taking ? "REC" : r->w ? "SAVING" : "");
    snprintf(b, sizeof b, "%s > POOL %02d", r->src == EC_RSRC_IN ? "IN L+R" : "MASTER", slot + 1);
    T(EC_C_L15, b, 2, 13, EC_AL_LEFT);
    if (taking) {                                  /* the clock, in the accent box */
        const uint32_t s = r->n / EC_FS;
        snprintf(b, sizeof b, "REC %u:%02u", s / 60u, s % 60u);
        R(EC_C_ACC, W - 46, 12, 44, 7);
        T(EC_C_BG, b, W - 24, 13, EC_AL_CENTER);
    } else if (r->state == EC_RS_ARMED) {           /* what it waits for */
        if (r->sync == EC_RSYNC_FREE)
            snprintf(b, sizeof b, "WAIT %dDB", ec_rec_thr_db(r->thr));
        else
            snprintf(b, sizeof b, "WAIT %s", SYNC[r->sync]);
        T((a->ms / 250u) & 1u ? EC_C_ACC : EC_C_L8, b, W - 2, 13, EC_AL_RIGHT);
    } else {                                        /* the longest take that fits now */
        const uint32_t s = ec_ss_free_frames() / EC_FS;
        snprintf(b, sizeof b, "MAX %u:%02u", s / 60u, s % 60u);
        T(EC_C_L8, b, W - 2, 13, EC_AL_RIGHT);
    }

    /* the ruler: the length's beats (bars taller); FREE: seconds */
    if (!total)
        total = taking && r->n > 8u * EC_FS ? r->n + r->n / 4u : 8u * EC_FS;
    {
        static const int BARS[EC_REC_NLEN] = {0, 1, 2, 4, 8, 16};
        nt = r->len ? 4 * BARS[r->len] : (int)(total / EC_FS);
    }
    if (nt > 64)
        nt = 64;
    {   /* 0.6.1: 2-unit ticks at least 4 apart (else every 2nd / 4th: the bars) */
        int skip = 1;
        while (nt / skip > cols / 4)
            skip *= 2;
        for (i = 0; i <= nt; i += skip) {
            const int big = r->len ? i % 4 == 0 : 1;
            R(big ? EC_C_L8 : EC_C_L3, x0 + cols * i / (nt ? nt : 1) - (i == nt), top - 4, 2, big ? 4 : 2);
        }
    }
    ec_gfx_dots_h(x0, x1, (top + 3), 3, EC_C_L3);
    ec_gfx_dots_h(x0, x1, (bot - 3), 3, EC_C_L3);
    if (taking) {
        const ec_ss_sample_t *x = ec_ss_get(r->handle);
        const int xr = x0 + (int)((uint64_t)cols * (r->n < total ? r->n : total) / total);
        if (x)
            take_wave(x->data, r->n < total ? r->n : total, total, x0, cols, top, bot);
        ec_gfx_dots_h(xr, x1, ((top + bot) / 2), 2, EC_C_L3);
        R(EC_C_ACC, xr, top - 2, 2, bot - top + 4);
    } else {
        const ec_ss_sample_t *s = a->slot[slot].handle >= 0 ? ec_ss_ready(a->slot[slot].handle) : 0;
        if (s && r->state == EC_RS_IDLE) {          /* what the slot holds now (a take replaces it) */
            const int mid = (top + bot) / 2, half = (bot - top) / 2;
            int c;
            for (c = 0; c + 1 < cols; c += 2) {                   /* 0.6.1: 2-unit columns */
                const int k = c * EC_SS_OVERVIEW / cols, h = (s->overview[k] * (half - 1) + 127) / 255;
                R(EC_C_L5, x0 + c, mid - h, 2, 2 * h + 2);
            }
            snprintf(b, sizeof b, "%.24s", s->name);
            T(EC_C_L8, b, W / 2, bot + 1, EC_AL_CENTER);
        } else
            ec_gfx_dots_h(x0, x1, ((top + bot) / 2), 2, EC_C_L3);
        if (r->state == EC_RS_ARMED)
            R(EC_C_ACC, x0, top - 2, 2, bot - top + 4);
    }
    if (r->src == EC_RSRC_IN && !live)              /* no input from the host: a take is silence */
        T2(EC_C_ALERT, "NO INPUT", W / 2, (top + bot) / 2 - 5, EC_AL_CENTER);

    /* the meters: the input (or the master out) with the threshold's tick, the latency */
    ec_gfx_dots_h(0, W, 63, 2, EC_C_L3);
    if (r->src == EC_RSRC_IN) {
        T(EC_C_L8, "IN", 2, 66, EC_AL_LEFT);
        meter(12, 66, 60, 2, r->in_meter[0]);
        meter(12, 70, 60, 2, r->in_meter[1]);
    } else {
        const float m = a->meter[8] > 1 ? 1 : a->meter[8];
        T(EC_C_L8, "MST", 2, 66, EC_AL_LEFT);
        meter(16, 66, 56, 5, m);
    }
    if (r->thr && r->sync == EC_RSYNC_FREE) {
        const int tx = 12 + (int)(db_frac(powf(10.0f, (float)ec_rec_thr_db(r->thr) / 20.0f)) * 60.0f);
        R(EC_C_L15, tx, 65, 2, 8);
    }
    if (r->src == EC_RSRC_IN) {
        lat_text(b, sizeof b);
        T(EC_C_L5, b, W - 2, 66, EC_AL_RIGHT);
    }

    /* 0.6.1: the settings, a list menu (SEL the row, K1 its value) */
    {
        static char val[EC_REC_NROW][16];
        ec_kit_row_t rows[EC_REC_NROW];
        memset(rows, 0, sizeof rows);
        for (i = 0; i < EC_REC_NROW; i++) {
            ec_rec_row_value(i, val[i], sizeof val[i]);
            rows[i].label = ec_rec_row_label(i);
            rows[i].value = val[i];
            rows[i].dim = (uint8_t)ec_rec_row_dim(i);
        }
        ec_kit_list(rows, EC_REC_NROW, r->row, 78, H, 0);
    }
}
