/* SPDX-License-Identifier: GPL-3.0-only */
/* The 0.6.0 gesture tests' harness (host/test_file.c, host/test_liverec.c): the core descriptor driven like a host
 * (tick every ms, frame / idle, audio() blocks as they fall due), every output frame kept, a stand-in input
 * (a buffer, or a tone), the panel's buttons, keys (by W / B number), knobs, LEDs and labels. Static: one copy per
 * test program. */
#ifndef EC_HARNESS_H
#define EC_HARNESS_H
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ec_app.h"
#include "ec_browse.h"
#include "ec_fs_native.h"
#include "fm1core.h"

#pragma GCC diagnostic ignored "-Wunused-function"

uint32_t fm1core_labels_epoch(void);
const char *fm1core_label(int32_t which);

static int fails, checks;
#define CHECK(c, ...)                                                                                              \
    do {                                                                                                           \
        checks++;                                                                                                  \
        if (!(c)) {                                                                                                \
            fails++;                                                                                               \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                                                            \
            printf(__VA_ARGS__);                                                                                   \
            printf("\n");                                                                                          \
        }                                                                                                          \
    } while (0)

#define A (&ec_app)
#define MAXF (44100u * 60u)
static const fm1core_t *C;
static fm1panel_t *P;
static uint32_t ms;
static uint64_t frames;                 /* rendered since boot (= the sequencer's sample time) */
static int16_t outrec[MAXF * 2];        /* every output frame since boot */
static float in_tone;                   /* the stand-in input: a 441 Hz sine of this amplitude (0: none) */

static void audio_block(void)
{
    int16_t in[FM1P_BLOCK * 2], out[FM1P_BLOCK * 2];
    uint32_t i;
    if (in_tone > 0 && ec_plat_ain_state()) {
        for (i = 0; i < FM1P_BLOCK; i++)
            in[2 * i] = in[2 * i + 1] =
                (int16_t)(in_tone * 32767.0f * sinf(6.2831853f * 441.0f * (float)(frames + i) / 44100.0f));
        fm1core_audio_in(in, FM1P_BLOCK);
    }
    C->audio(out, FM1P_BLOCK);
    if (frames + FM1P_BLOCK <= MAXF)
        memcpy(outrec + 2 * frames, out, sizeof out);
    frames += FM1P_BLOCK;
}

static void run(int n)
{
    int i;
    for (i = 0; i < n; i++) {
        ec_fs_native_pump();
        C->tick(ms);
        if (ms % 15 == 0)
            C->frame();
        else
            C->idle();
        ms++;
        while (frames + FM1P_BLOCK <= (uint64_t)ms * FM1P_FS / 1000u)
            audio_block();
    }
}
static void btn(int b, int down)
{
    const uint32_t m = 1u << P->btn_id[b];
    if (down) {
        P->buttons |= m;
        P->buttons_tap |= m;
    } else
        P->buttons &= ~m;
}
static void press(int b, int dur)
{
    btn(b, 1);
    run(dur);
    btn(b, 0);
    run(20);
}
static int key_of(int upper, int i)
{
    int k, nl = 0, nu = 0;
    for (k = 0; k < FM1P_NKEY; k++) {
        const int s = k % 12;
        const int natural = s == 0 || s == 2 || s == 4 || s == 6 || s == 7 || s == 9 || s == 11;
        if (natural && !upper && nl++ == i)
            return k;
        if (!natural && upper && nu++ == i)
            return k;
    }
    return 0;
}
static void keyv(int upper, int i, int down, int vel)
{
    const int k = key_of(upper, i);
    if (down) {
        P->key_vel[k] = (uint8_t)vel;
        P->keys |= 1u << k;
        P->keys_tap |= 1u << k;
    } else
        P->keys &= ~(1u << k);
}
static void key(int upper, int i, int down) { keyv(upper, i, down, 100); }
static void tapkey(int upper, int i, int dur)
{
    key(upper, i, 1);
    run(dur);
    key(upper, i, 0);
    run(20);
}
static void W(int w, int dur) { tapkey(0, w - 1, dur); }   /* tap white key W1..W16 */
static void B(int b, int dur) { tapkey(1, b - 1, dur); }   /* tap black key B1..B11 */
static void turn(int role, int n)
{
    P->enc[P->enc_id[role]] += n * P->enc_dir[role];
    run(2);
}
static const char *L(int which)
{
    const char *s = fm1core_label(which);
    return s ? s : "(null)";
}
static int lbl(int which, const char *want) { return !strcmp(L(which), want); }

/* the LEDs (ec_core.c's FM-1 wiring: key id 0..13 the buttons, 14 + n note key n) */
static const int8_t KEYMAP[6][FM1P_NCOL] = {
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {5, 11, 4, 10, 3, 9, 2, 8, -1, -1, -1},
    {34, 35, 36, 37, 38, 40, 39, 13, 7, 6, 12},
    {23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33},
    {0, 1, 15, 14, 17, 16, 19, 18, 20, 21, 22},
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
};
static int led_id(int id)
{
    int r, c;
    for (r = 1; r <= 4; r++)
        for (c = 0; c < FM1P_NCOL; c++)
            if (KEYMAP[r][c] == id)
                return P->led[c][r - 1];
    return -1;
}
static int led_key(int upper, int i) { return led_id(14 + key_of(upper, i)); }
static int led_btn(int b) { return led_id(P->btn_id[b]); }

static int mode(void) { return ec_app_mode(); }
static const int BTN_OF_MB[EC_NMB] = {-1, FM1P_B_FX, FM1P_B_SEL, FM1P_B_ENV, FM1P_B_LFO, FM1P_B_EDIT, FM1P_B_HOME,
                                      FM1P_B_SAVE, FM1P_B_ARP, FM1P_B_SEQ};
/* close what is open, then tap the mode's button until it shows; the pop-up gone */
static void to(int m)
{
    int n;
    if (A->pn.as_open || A->ux.f3 || A->settings || A->pn.rs_open || A->rec.open)
        press(FM1P_B_OCTDN, 40);
    if (A->pn.shift_sticky)
        press(FM1P_B_GLO, 40);
    if (A->pn.f_sticky)
        press(A->pn.f_sticky == 1 ? FM1P_B_OCTDN : FM1P_B_OCTUP, 40);
    for (n = 0; n < 5 && mode() != m; n++)
        press(BTN_OF_MB[ec_mode_button(m)], 40);
    if (m == EC_CAT_FILE) {                        /* 0.6.66: the button comes back where FILE was left: the browser */
        ec_pn_enter(EC_CAT_SRC);
        ec_pn_enter(EC_CAT_FILE);
    }
    run(900);
}
static void shift_down(int d)
{
    btn(FM1P_B_GLO, d);
    run(20);
}

static ec_ptrack_t *trk(int t) { return ec_project_track(&A->proj, t); }
static ec_step_t *step(int i) { return &trk(A->track)->steps[i]; }
static void clear_steps(int t)
{
    int i;
    for (i = 0; i < EC_MAXSTEPS; i++)
        ec_step_clear(&trk(t)->steps[i]);
    ec_seq_touch(&A->seq, t);
}
static double spb(void) { return 60.0 * 44100.0 / A->seq.bpm; }

/* power on from a flash image (NULL: none) */
static void boot(const uint8_t *flash, uint32_t n)
{
    C = fm1core_get(FM1CORE_ABI);
    P = C->panel;
    C->flash_stage(flash, n);
    C->options(FM1CORE_FLASH_STAGED, 0, 0, 1);
    P->ready = 2;
    ms = 0;
    frames = 0;
    memset(outrec, 0, sizeof outrec);
    C->init(0);
    run(1500);                                    /* the scan, T1's first file */
    turn(FM1P_E_ALGO, -1);                        /* (any knob skips the intro: ALGO at step page 1 clamps) */
    run(900);
}

/* a 16-bit stereo WAV of n frames of a decaying tone (deterministic) */
static void write_wav(const char *path, uint32_t n, float hz)
{
    FILE *f = fopen(path, "wb");
    uint8_t h[44];
    uint32_t i;
    if (!f)
        return;
    memcpy(h, "RIFF", 4);
    h[4] = (uint8_t)(36 + n * 4), h[5] = (uint8_t)((36 + n * 4) >> 8), h[6] = (uint8_t)((36 + n * 4) >> 16),
    h[7] = (uint8_t)((36 + n * 4) >> 24);
    memcpy(h + 8, "WAVEfmt \x10\0\0\0\x01\0\x02\0\x44\xac\0\0\x10\xb1\x02\0\x04\0\x10\0data", 32);
    h[40] = (uint8_t)(n * 4), h[41] = (uint8_t)((n * 4) >> 8), h[42] = (uint8_t)((n * 4) >> 16),
    h[43] = (uint8_t)((n * 4) >> 24);
    fwrite(h, 1, 44, f);
    for (i = 0; i < n; i++) {
        const int16_t v = (int16_t)(0.5 * 32767.0 * sin(6.2831853 * hz * i / 44100.0) * exp(-(double)i / 20000.0));
        fwrite(&v, 2, 1, f);
        fwrite(&v, 2, 1, f);
    }
    fclose(f);
}

/* the output's peak |sample| over frames [a, b) */
static int out_peak(uint64_t a, uint64_t b)
{
    uint64_t i;
    int pk = 0;
    for (i = a; i < b && i < MAXF; i++) {
        const int l = abs(outrec[2 * i]), r = abs(outrec[2 * i + 1]);
        pk = l > pk ? l : pk;
        pk = r > pk ? r : pk;
    }
    return pk;
}
/* the first frame in [a, b) over thr, else UINT64_MAX */
static uint64_t out_onset(uint64_t a, uint64_t b, int thr)
{
    uint64_t i;
    for (i = a; i < b && i < MAXF; i++)
        if (abs(outrec[2 * i]) > thr)
            return i;
    return UINT64_MAX;
}

#endif
