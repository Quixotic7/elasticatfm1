/* SPDX-License-Identifier: GPL-3.0-only */
/* E3: the razor editor's controls (ec_razor_ui.h). docs/FM1-CONTROLS.md "FILE / RAZOR" is the player's reference. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ec_razor_ui.h"

ec_rzui_t ec_rzui = {1, 1, -1.0f, 0};
#define U (&ec_rzui)

static int clampi(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }
static float clampf(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }
static const float *tv(const ec_app_t *a) { return a->proj.pat[a->proj.current].tr[a->track].v; }

static void say(ec_app_t *a, const char *s)
{
    snprintf(a->msg, sizeof a->msg, "%s", s);
    a->msg_ms = a->ms ? a->ms : 1;
    a->msg_alert = 0;
    a->redraw = 1;
}
static void changed(ec_app_t *a)
{
    ec_seq_touch(&a->seq, a->track);
    a->dirty = 1;
    a->dirty_ms = a->ms;
    a->redraw = 1;
}

int ec_rzui_active(const ec_app_t *a)
{
    return ec_page_pic(a->cat, a->page[a->cat], a->bus) == EC_PIC_RAZOR && !a->settings && !a->bus && !a->scr &&
           !a->e5.naming;   /* (E5's naming screen covers it) */
}

int ec_rzui_count(const ec_app_t *a) { return clampi((int)floorf(tv(a)[EC_P_SLICE_COUNT] + 0.5f), 1, 32); }

static int sel(const ec_app_t *a)
{
    U->sel = clampi(U->sel, 1, ec_rzui_count(a));
    return U->sel;
}

const char *ec_rzui_snap_name(int snap)
{
    static const char *const N[EC_NSNAP] = {"GRID", "ZOOM", "ZERO-X", "TRANS", "FRIEND"};
    return N[clampi(snap, 1, EC_NSNAP) - 1];
}

/* ------------------------------------------------------------------------------------------- the sample --- */
void ec_rzui_window(const ec_app_t *a, int t, double *lo, double *hi)
{
    const float *v = a->proj.pat[a->proj.current].tr[t].v;
    const ec_slot_t *sl = &a->proj.slot[ec_app_track_slot(a, t)];
    const double span = (double)sl->trim_end - sl->trim_start;
    *lo = sl->trim_start + span * v[EC_P_RANGE_START] / 128.0;
    *hi = sl->trim_start + span * v[EC_P_RANGE_END] / 128.0;
    if (*hi <= *lo)
        *hi = *lo + 1e-6;
}

/* the onset envelopes of the last samples analysed (read_wav_onsets is cached at load in the original) */
static struct {
    const int16_t *data;
    uint32_t frames;
    ec_onsets_t on;
} g_cache[4];
static int g_next;

static const ec_onsets_t *onsets(const ec_ss_sample_t *s)
{
    int i;
    if (!s)
        return 0;
    for (i = 0; i < 4; i++)
        if (g_cache[i].data == s->data && g_cache[i].frames == s->frames)
            return &g_cache[i].on;
    i = g_next;
    g_next = (g_next + 1) & 3;
    g_cache[i].data = s->data;
    g_cache[i].frames = s->frames;
    ec_razor_onsets(s->data, s->frames, (float)s->rate, &g_cache[i].on);
    return &g_cache[i].on;
}

int ec_rzui_transients(const ec_app_t *a, int t, float *x128, int cap)
{
    const ec_ss_sample_t *s = ec_app_track_sample(a, t);
    float p[EC_RZ_ONSET_WINDOWS];
    double lo, hi;
    int n, i, k = 0;
    if (!s)
        return 0;
    n = ec_razor_transients(onsets(s), (float)a->proj.razor[t].sens / 100.0f, p, EC_RZ_ONSET_WINDOWS);
    ec_rzui_window(a, t, &lo, &hi);
    for (i = 0; i < n && k < cap; i++) {
        const double x = (p[i] - lo) / (hi - lo) * 128.0;
        if (x >= 0 && x < 128)
            x128[k++] = (float)x;
    }
    return k;
}

/* ------------------------------------------------------------------------------------------- cells --- */
float ec_rzui_value(const ec_app_t *a, int id)
{
    const ec_razor_t *rz = &a->proj.razor[a->track];
    const int k = sel(a) - 1;
    switch (id) {
    case EC_RZ_START: return rz->start[k];
    case EC_RZ_END: return rz->end[k];
    case EC_RZ_PLAY: return rz->play[k];
    case EC_RZ_ZOOM: return (float)U->zoom;
    case EC_RZ_SNAP: return rz->snap;
    case EC_RZ_CHOKE: return rz->choke[k];
    case EC_RZ_PITCH: return rz->pitch[k];
    default: return rz->sens;
    }
}

void ec_rzui_format(const ec_app_t *a, int id, float x, char *out, int cap)
{
    static const char *const PM[7] = {"TRACK", "1SHOT", "HOLD", "LOOP", "CONT", "PONG", "CLOOP"};
    (void)a;
    switch (id) {
    case EC_RZ_START:
    case EC_RZ_END: snprintf(out, (size_t)cap, "%.3f", (double)(x / 128.0f)); break;   /* 0..1 of the window */
    case EC_RZ_PLAY: snprintf(out, (size_t)cap, "%s", PM[clampi((int)x, 0, 6)]); break;
    case EC_RZ_ZOOM: snprintf(out, (size_t)cap, "%dX", (int)x); break;
    case EC_RZ_SNAP: snprintf(out, (size_t)cap, "%s", ec_rzui_snap_name((int)x)); break;
    case EC_RZ_CHOKE:
        if ((int)x <= 0)
            snprintf(out, (size_t)cap, "OFF");
        else
            snprintf(out, (size_t)cap, "%c", 'A' + (int)x - 1);
        break;
    case EC_RZ_PITCH: ec_param_format(EC_P_PITCH, x, 3, out, cap); break;
    default: snprintf(out, (size_t)cap, "%d%%", (int)x); break;
    }
}

float ec_rzui_frac(const ec_app_t *a, int id, float x)
{
    (void)a;
    switch (id) {
    case EC_RZ_START:
    case EC_RZ_END: return x / 128.0f;
    case EC_RZ_PLAY: return x / 6.0f;
    case EC_RZ_ZOOM: return log2f(x > 1 ? x : 1) / 6.0f;
    case EC_RZ_SNAP: return (x - 1) / 4.0f;
    case EC_RZ_CHOKE: return x / 8.0f;
    case EC_RZ_PITCH: return (x + 24) / 48.0f;
    default: return x / 100.0f;
    }
}

/* razor_point_edit: one STRT / END turn under the snap mode. Without a snap the original moved 0.5 per detent; here
 * the ZOOM snap is the plain mode and its detent is 0.5 / zoom, so the marker moves the same visible amount at
 * every zoom (the original's FN fine nudge). */
static void edit_point(ec_app_t *a, int which, int det, int mult)
{
    ec_razor_t *rz = &a->proj.razor[a->track];
    const int s = sel(a), k = s - 1, count = ec_rzui_count(a);
    const float cur = which ? rz->end[k] : rz->start[k];
    const float step = 0.5f / (float)U->zoom;
    float x = cur + (float)(det * mult) * step;
    switch (rz->snap) {
    case EC_SNAP_GRID: {
        const float w = 128.0f / (float)count;
        int n;
        x = cur;
        for (n = 0; n < (det < 0 ? -det : det); n++)
            x = det > 0 ? (floorf(x / w + 1e-4f) + 1) * w : (ceilf(x / w - 1e-4f) - 1) * w;
    } break;
    case EC_SNAP_ZEROX: {
        const ec_ss_sample_t *smp = ec_app_track_sample(a, a->track);
        if (smp) {
            double lo, hi, p;
            ec_rzui_window(a, a->track, &lo, &hi);
            p = lo + (hi - lo) * clampf(x, 0, 128) / 128.0;
            p = ec_razor_zero_cross(smp->data, smp->frames, (float)smp->rate, p, 0.01);
            x = (float)((p - lo) / (hi - lo) * 128.0);
        }
    } break;
    case EC_SNAP_TRANSIENT: {
        float tr[EC_RZ_ONSET_WINDOWS];
        const int n = ec_rzui_transients(a, a->track, tr, EC_RZ_ONSET_WINDOWS);
        int i, hit = 0;
        if (det > 0) {
            for (i = 0; i < n && !hit; i++)
                if (tr[i] > cur + 0.05f)
                    x = tr[i], hit = 1;
        } else {
            for (i = n - 1; i >= 0 && !hit; i--)
                if (tr[i] < cur - 0.05f)
                    x = tr[i], hit = 1;
        }
        if (!hit)
            x = cur + (float)det * step * 0.25f;   /* none that way: a fine nudge */
    } break;
    default: break;   /* ZOOM, FRIENDS */
    }
    ec_razor_set_point(rz, s, which, x, rz->snap == EC_SNAP_FRIENDS, count);
    U->focus = which ? rz->end[k] : rz->start[k];
    U->focus_slice = s;
}

void ec_rzui_edit(ec_app_t *a, int id, int det, int mult)
{
    ec_razor_t *rz = &a->proj.razor[a->track];
    const int k = sel(a) - 1;
    switch (id) {
    case EC_RZ_START: edit_point(a, 0, det, mult); break;
    case EC_RZ_END: edit_point(a, 1, det, mult); break;
    case EC_RZ_PLAY: rz->play[k] = (int8_t)clampi(rz->play[k] + det, 0, 6); break;
    case EC_RZ_ZOOM: {
        int z = U->zoom;
        if (det > 0)
            z = z << (det > 6 ? 6 : det);
        else
            z = z >> (-det > 6 ? 6 : -det);
        U->zoom = clampi(z, 1, 64);
        a->redraw = 1;
        return;   /* (a view, not the project) */
    }
    case EC_RZ_SNAP: rz->snap = (uint8_t)clampi(rz->snap + det, 1, EC_NSNAP); break;
    case EC_RZ_CHOKE: rz->choke[k] = (int8_t)clampi(rz->choke[k] + det, 0, 8); break;
    case EC_RZ_PITCH: rz->pitch[k] = clampf(floorf(rz->pitch[k] + 0.5f) + (float)det, -24, 24); break;
    default:
        rz->sens = (uint8_t)clampi(rz->sens + det * mult, 0, 100);
        ec_rzui.sens_ms = a->ms ? a->ms : 1;
        break;
    }
    changed(a);
}

/* ------------------------------------------------------------------------------------------- gestures --- */
static uint16_t g_pads;    /* lower keys playing a slice */

void ec_rzui_select(ec_app_t *a, int det)
{
    U->sel = clampi(sel(a) + det, 1, ec_rzui_count(a));
    U->focus = -1;
    a->redraw = 1;
}

static int is_slice_machine(const ec_app_t *a)
{
    const int m = (int)floorf(tv(a)[EC_P_MACHINE] + 0.5f);
    return m >= EC_MACH_GRID_SLICE && m <= EC_MACH_RAZOR_POLY;
}

void ec_rzui_key(ec_app_t *a, int i, int down, int vel)
{
    const int bank = (sel(a) - 1) / 16, slice = bank * 16 + i + 1;
    char m[32];
    if (!down) {
        if (g_pads >> i & 1u) {
            g_pads &= (uint16_t)~(1u << i);
            ec_seq_slice_pad(&a->seq, a->track, slice, 0, 0);
        }
        a->redraw = 1;
        return;
    }
    if (slice > ec_rzui_count(a))
        return;
    if (a->fn) {                                      /* FN + key: mute the slice (FN + slice key in the original) */
        ec_razor_t *rz = &a->proj.razor[a->track];
        rz->mute[slice - 1] = !rz->mute[slice - 1];
        snprintf(m, sizeof m, "SLICE %d %s", slice, rz->mute[slice - 1] ? "MUTED" : "ON");
        say(a, m);
        changed(a);
        return;
    }
    U->sel = slice;
    U->focus = -1;
    if (is_slice_machine(a)) {
        ec_seq_slice_pad(&a->seq, a->track, slice, 1, (float)vel / 127.0f);
        g_pads |= (uint16_t)(1u << i);
    } else
        ec_rzui_preview(a, slice);                   /* 0.6.15 (owner): any machine: the slice heard as a preview */
    a->redraw = 1;
}

/* 0.6.15 (owner): B10 (and FN + OCT+): Auto-Chop at the transients; B9 (FN + OCT-): COUNT even slices */
void ec_rzui_autochop(ec_app_t *a)
{
    ec_razor_t *rz = &a->proj.razor[a->track];
    const ec_ss_sample_t *smp = ec_app_track_sample(a, a->track);
    float p[EC_RZ_ONSET_WINDOWS];
    double lo, hi;
    char m[32];
    int n;
    if (!smp) {
        say(a, "NO SAMPLE");
        return;
    }
    n = ec_razor_transients(onsets(smp), (float)rz->sens / 100.0f, p, EC_RZ_ONSET_WINDOWS);
    ec_rzui_window(a, a->track, &lo, &hi);
    n = ec_razor_autochop(&a->proj, a->track, p, n, lo, hi);
    if (!n) {
        say(a, "NO TRANSIENTS");
        return;
    }
    snprintf(m, sizeof m, "AUTO-CHOP %d", n);
    U->sel = 1;
    U->zoom = 1;
    U->focus = -1;
    say(a, m);
    changed(a);
}

void ec_rzui_even(ec_app_t *a)
{
    ec_razor_t *rz = &a->proj.razor[a->track];
    const int count = ec_rzui_count(a);
    char m[32];
    ec_razor_redistribute(rz, count);
    snprintf(m, sizeof m, "EVEN %d", count);
    U->focus = -1;
    say(a, m);
    changed(a);
}

/* the selected slice (or slice k) heard through the preview player (any machine): its span of the sample */
void ec_rzui_preview(ec_app_t *a, int slice)
{
    const ec_razor_t *rz = &a->proj.razor[a->track];
    const int count = ec_rzui_count(a);
    double lo, hi;
    if (slice < 1 || slice > count)
        return;
    ec_rzui_window(a, a->track, &lo, &hi);
    ec_file_preview_region((float)(lo + (hi - lo) * rz->start[slice - 1] / 128.0),   /* (the edges: 0..128 of the window) */
                           (float)(lo + (hi - lo) * rz->end[slice - 1] / 128.0));
}

void ec_rzui_oct(ec_app_t *a, int up)
{
    ec_razor_t *rz = &a->proj.razor[a->track];
    const int s = sel(a), count = ec_rzui_count(a);
    char m[32];
    if (a->fn && up) {                                /* FN + OCT+: Auto-Chop */
        ec_rzui_autochop(a);
        return;
    } else if (a->fn) {                               /* FN + OCT-: Redistribute */
        ec_rzui_even(a);
        return;
    } else if (up) {                                  /* OCT+: split the slice at the focus (else its middle) */
        const float x = U->focus_slice == s ? U->focus : -1;
        if (count >= EC_RZ_SLICES) {
            say(a, "32 SLICES");
            return;
        }
        ec_razor_add(&a->proj, a->track, s, x);
        snprintf(m, sizeof m, "SPLIT %d", s);
        U->focus = -1;
    } else {                                          /* OCT-: delete the slice */
        if (count <= 1) {
            say(a, "LAST SLICE");
            return;
        }
        ec_razor_delete(&a->proj, a->track, s);
        snprintf(m, sizeof m, "DELETE %d", s);
        U->sel = clampi(s, 1, count - 1);
        U->focus = -1;
    }
    say(a, m);
    changed(a);
}

void ec_rzui_leds(const ec_app_t *a)
{
    const ec_razor_t *rz = &a->proj.razor[a->track];
    const int count = ec_rzui_count(a), s = sel(a), bank = (s - 1) / 16;
    uint32_t sounding = a->seq.tr[a->track].pads;
    ec_track_status_t st;
    int i;
    ec_dsp_track_status(a->dsp, a->track, &st);
    for (i = 0; i < st.voices && i < EC_MAX_VOICES_TRACK; i++)
        if (st.voice_slice[i] >= 1 && st.voice_slice[i] <= 32)
            sounding |= 1u << (st.voice_slice[i] - 1);
    for (i = 0; i < EC_NLOWER; i++) {
        const int sl = bank * 16 + i + 1;
        uint8_t l = EC_LED_DIM;
        if (sl > count)
            l = EC_LED_OFF;
        else if (sl == s)
            l = EC_LED_LIT;
        else if (sounding >> (sl - 1) & 1u)
            l = EC_LED_MID;
        else if (rz->mute[sl - 1])
            l = EC_LED_FAINT;
        ec_plat_led_lower(i, l);
    }
    ec_plat_led_button(EC_B_OCTDN, EC_LED_DIM);       /* add / delete are there */
    ec_plat_led_button(EC_B_OCTUP, EC_LED_DIM);
}
