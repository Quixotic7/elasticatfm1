/* SPDX-License-Identifier: GPL-3.0-only */
/* 0.6.1 (docs/UX-V2.md §10): the header's title line (ec_mode.h "ec_header.c"). There are no message boxes and no
 * mode pop-up: a message shows in the header for its time (an alert on ALERT), and while a key that does something
 * when held is held, the header says what it does (a source key on SRC, held steps on LOCK, the rest on the plain
 * bar), back to the screen's title when it is let go. A held key's text shows EC_HDR_MS after the last press, so a
 * tap shows none; sticky SHIFT / F1 / F2 show at once. A message newer than the last press wins over a held key's
 * text; an older one gives way to it.
 * Other modules add their own held keys through ec_hdr_provider() (asked first, newest first). */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ec_app.h"
#include "ec_palette.h"

#define A (&ec_app)
#define P (&ec_app.pn)
#define UX (&ec_app.ux)
#define E (&ec_app.e5)

#define NPROV 4
static ec_hdr_fn g_prov[NPROV];
static int g_nprov;

void ec_hdr_provider(ec_hdr_fn fn)
{
    int i;
    for (i = 0; i < g_nprov; i++)
        if (g_prov[i] == fn)
            return;
    if (fn && g_nprov < NPROV)
        g_prov[g_nprov++] = fn;
}

int ec_hdr_holding(void) { return P->down_ms && A->ms - P->down_ms >= EC_HDR_MS; }

static int say(char *out, int cap, int bar, const char *fmt, int n)
{
    snprintf(out, (size_t)cap, fmt, n);
    return bar;
}

/* the mode button held (before its settings open): what the hold opens */
static const char *button_text(int b)
{
    switch (b) {
    case EC_B_FX: return "FX SETUP";
    case EC_B_SEL: return "FILTER SETUP";
    case EC_B_ENV: return "AMP SETUP";
    case EC_B_EDIT: return "PROJECT SETUP";
    case EC_B_HOME: return "LOOP SETUP";
    case EC_B_SAVE: return "RECORDER SETUP";
    case EC_B_SEQ: return "F3";
    default: return 0;                             /* LFO, ARP: no settings */
    }
}

/* the quick setting a held B1 (or FX's bus key) changes */
static const char *quick_text(int mode, int key)
{
    static const char *const BUS[3] = {"S1 FX TYPE", "S2 FX TYPE", "MST FX TYPE"};
    if (mode == EC_CAT_FX) {                       /* 0.6.3: B1-B4 the slots' types (SENDS: the swaps), B5 MASTER */
        static const char *const SLOT[2] = {"INSERT 1 TYPE", "INSERT 2 TYPE"};
        if (key >= 1 && key <= 4 && ec_fx_on_sends())
            return key <= 2 ? "MOVE INSERT" : "MOVE SEND";
        return key >= 1 && key <= 2 ? SLOT[key - 1] : key >= 3 && key <= 4 ? BUS[key - 3] : key == 5 && A->bus == 3
                                                                                                 ? BUS[2] : 0;
    }
    if (key != 1)
        return 0;
    switch (mode) {
    case EC_CAT_FILT: return "FILTER TYPE";
    case EC_CAT_AMP: return "ENV MODE";
    case EC_CAT_SRC: return "MACHINE";
    default: return 0;
    }
}

static int lowest(uint32_t m) { return m ? __builtin_ctz(m) : -1; }

/* F1 / F2 on (held or sticky): copy / cut (and paste once the buffer holds something), clear a lock / a modulation */
static int f_text(char *out, int cap, int mode)
{
    const int f1 = ec_pn_f(1), f2 = ec_pn_f(2), full = P->clip_state != EC_CLIP_EMPTY;
    const char *what;
    if (!(f1 || f2) || (P->f_held == 3))
        return -1;
    if (!(P->f_sticky) && !ec_hdr_holding())
        return -1;
    switch (mode) {
    case EC_MODE_PAGES: what = "PAGE"; break;
    case EC_CAT_PAT: what = "PATTERN"; break;
    default: what = 0; break;
    }
    if (what)
        snprintf(out, (size_t)cap, "%s %s", full ? "PASTE" : f1 ? "COPY" : "CUT", what);
    else if (full)
        snprintf(out, (size_t)cap, "PASTE / %s", f1 ? "CLR LOCK" : "CLR MOD");
    else
        snprintf(out, (size_t)cap, "%s", f1 ? "COPY / CLR LOCK" : "CUT / CLR MOD");
    return EC_C_L12;
}

/* the built-in held keys, most specific first */
static int builtin(char *out, int cap)
{
    static const char *const SRC[EC_MK_M4 + 1] = {"LFO 1", "LFO 2", "MOD ENV", "VEL", "AT", "M1", "M2", "M3", "M4"};
    const int mode = ec_app_mode(), late = ec_hdr_holding();
    const int plain = !A->settings && !A->ux.f3 && !A->e5.naming && !P->as_open && !P->rs_open;
    int i;
    if (late && A->mk.held >= 0 && A->mk.held <= EC_MK_M4 && ec_mk_src_of_key(A->mk.held))   /* a source key */
        return say(out, cap, EC_C_SRC, SRC[A->mk.held], 0);
    if (late && P->btn >= 0 && !P->btn_used && button_text(P->btn))   /* a mode button: its settings at 400 ms */
        return say(out, cap, EC_C_L12, button_text(P->btn), 0);
    if (late && P->rec_down && !P->rec_used)
        return say(out, cap, EC_C_L12, "REC SETUP", 0);
    if (late && plain && mode == EC_CAT_XFD) {     /* CROSSFADER: capture, the scenes, the glide */
        if (P->xf_b11)
            return say(out, cap, EC_C_L12, "CAPTURE", 0);
        if (E->anchor == 1 || E->anchor == 2)
            return say(out, cap, EC_C_L12, E->anchor == 1 ? "SCENE A" : "SCENE B", 0);
        if (P->xf_key >= 0)
            return say(out, cap, EC_C_L12, "GLIDE TO %d", (int)floorf(ec_xfd_key_value(P->xf_key) * 127.0f + 0.5f));
    }
    if (plain && ec_lp_step_layer())               /* 0.6.9: LOOP ENGINE's STEP layer (B11) */
        return say(out, cap, EC_C_L12, "SELECT STEPS", 0);
    if (late && A->fill_held)
        return say(out, cap, EC_C_L12, "FILL", 0);
    if (late && plain && P->slot_key && quick_text(mode, P->slot_key))   /* the quick setting (K1 / SEL) */
        return say(out, cap, EC_C_L12, quick_text(mode, P->slot_key), 0);
    if (late && plain && mode == EC_MODE_KEYBOARD && ec_kb_held_text())
        return say(out, cap, EC_C_L12, ec_kb_held_text(), 0);
    if (late && A->step_held && !A->bus && !A->settings && !A->ux.f3) {   /* steps: their locks */
        const int n = __builtin_popcount(A->step_held);
        if (n == 1)
            return say(out, cap, EC_C_LOCK, "STEP %d", A->step_page * 16 + lowest(A->step_held) + 1);
        return say(out, cap, EC_C_LOCK, "%d STEPS", n);
    }
    if (late && plain && mode == EC_CAT_SRC && !A->bus) {   /* LOOP ENGINE: the loop keys / pads play */
        const ec_seq_track_t *st = &A->seq.tr[A->track];
        if (st->live_keys)
            return say(out, cap, EC_C_L12, "LOOP KEY %d", lowest(st->live_keys) + 1);
        if (st->pads)
            return say(out, cap, EC_C_L12, "PAD %d", lowest(st->pads) + 1);
    }
    if (late && plain && mode == EC_MODE_PAGES && !ec_pn_f(1) && !ec_pn_f(2))   /* a page key: the loop */
        for (i = 0; i < EC_NBKEY && i < ec_pages_count(); i++)
            if ((P->bk_held >> i & 1u) && !(P->bk_used >> i & 1u))
                return say(out, cap, EC_C_L12, "LOOP PAGE %d", i + 1);
    if (late && A->ux.f3 && !A->e5.naming)         /* F3's page keys: the loop */
        for (i = 0; i < EC_NLOWER; i++)
            if ((A->ux.f3_keys >> i & 1u) && !(A->ux.f3_used >> i & 1u))
                return say(out, cap, EC_C_L12, "LOOP PAGE %d", i + 1);
    if (late && plain && mode == EC_CAT_MASTER)    /* MIX: a track key held mutes */
        for (i = 0; i < EC_NTRACK; i++)
            if ((P->trk_down >> i & 1u) && !(P->trk_used >> i & 1u)) {
                snprintf(out, (size_t)cap, "%s T%d",
                         ec_project_track(&A->proj, i)->v[EC_P_MUTE] > 0.5f ? "UNMUTE" : "MUTE", i + 1);
                return EC_C_L12;
            }
    if (plain && f_text(out, cap, mode) >= 0)
        return EC_C_L12;
    if (P->shift_sticky || (late && P->shift_held))
        return say(out, cap, EC_C_L12, "SHIFT", 0);
    return -1;
}

int ec_hdr_held(char *out, int cap)
{
    int i, bar;
    if (A->pn.intro)
        return -1;
    for (i = g_nprov - 1; i >= 0; i--)
        if ((bar = g_prov[i](out, cap)) >= 0)
            return bar;
    return builtin(out, cap);
}

/* what the line shows: a message newer than the last press, or with nothing held; a held key's text; the title */
static int line(const char *title, char *out, int cap, int *custom)
{
    char held[40];
    const int bar = ec_hdr_held(held, sizeof held);
    *custom = 1;
    if (A->msg_ms && (bar < 0 || A->msg_ms >= P->down_ms)) {
        snprintf(out, (size_t)cap, "%s", A->msg);
        return A->msg_alert ? EC_C_ALERT : EC_C_L12;
    }
    if (bar >= 0) {
        snprintf(out, (size_t)cap, "%s", held);
        return bar;
    }
    *custom = 0;
    snprintf(out, (size_t)cap, "%s", title ? title : "");
    return EC_C_L12;
}

int ec_hdr_title(const char *title, char *out, int cap)
{
    int custom;
    return line(title, out, cap, &custom);
}

int ec_hdr_custom(void)
{
    char b[40];
    int custom;
    line("", b, sizeof b, &custom);
    return custom;
}
