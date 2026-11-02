/* SPDX-License-Identifier: GPL-3.0-only */
/* ec_run: Elasticat FM-1 natively, headless, through its own fm1core descriptor, on the ABI's device clock (tick
 * once per ms, frame every 15 ms else idle, audio per 128-frame block due at 44.1 kHz), with the file API served
 * from a real folder one tick late (src/io/ec_fs_native.c), and scripted input.
 *
 *   ec_run [options]
 *     --folder DIR        the RemoteFM1 root (DIR/audio is /audio); none: no folder
 *     --ms N              device milliseconds to run (default 6000)
 *     --press B@MS[+D]    button B by label (FX SEL ENV LFO EDIT GLO HOME SAVE ARP SEQ PLAY REC OCT- OCT+) held from
 *                         MS for D ms (default 60); repeat
 *     --turn R@MS:N       knob role R (SELECT ALGO PRESETS K1 K2 K3 K4) turned N detents at MS; repeat
 *     --key K@MS[+D]      note key K (0..26) held from MS for D ms (default 100); repeat
 *     --master V@MS       the MASTER pot set to V (0..1023) at MS; repeat
 *     --wav FILE          the whole run's audio (44.1 kHz 16-bit stereo)
 *     --png FILE          the screen at the end (240x240; --png-scale 2 for 480x480)
 *     --png-at MS:FILE    the screen at device ms MS; repeat
 *     --info-at MS        ui_info printed at MS; repeat
 *     --rms-from MS       the RMS is measured from MS on (default 0)
 *     --min-rms X         fail when the RMS is below X (full scale 1.0)
 *     --expect-ui S       fail unless the final ui_info contains S; repeat
 * Prints: ui_info, RMS / peak, LCD writes, x real time. Exit 0 ok, 1 a check failed, 2 usage. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ec_fs_native.h"
#include "ec_png.h"
#include "fm1core.h"

#define MAXEV 256

typedef struct {
    int kind;          /* 0 button, 1 turn, 2 key, 3 master */
    int what;
    uint32_t at, dur;
    int32_t n;
} ev_t;

static const char *const BTN[FM1P_NB] = {"FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ",
                                         "PLAY", "REC", "OCT-", "OCT+"};
static const char *const ROLE[FM1P_NE - 1] = {"SELECT", "ALGO", "PRESETS", "K1", "K2", "K3", "K4"};

static double now_s(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + t.tv_nsec * 1e-9;
}

static void wav_header(FILE *f, uint32_t frames)
{
    uint8_t h[44];
    const uint32_t data = frames * 4u;
    memcpy(h, "RIFF", 4);
    h[4] = (uint8_t)(data + 36), h[5] = (uint8_t)((data + 36) >> 8), h[6] = (uint8_t)((data + 36) >> 16),
    h[7] = (uint8_t)((data + 36) >> 24);
    memcpy(h + 8, "WAVEfmt ", 8);
    h[16] = 16, h[17] = h[18] = h[19] = 0;
    h[20] = 1, h[21] = 0, h[22] = 2, h[23] = 0;
    h[24] = 0x44, h[25] = 0xAC, h[26] = 0, h[27] = 0;            /* 44100 */
    h[28] = 0x10, h[29] = 0xB1, h[30] = 0x02, h[31] = 0;         /* 176400 */
    h[32] = 4, h[33] = 0, h[34] = 16, h[35] = 0;
    memcpy(h + 36, "data", 4);
    h[40] = (uint8_t)data, h[41] = (uint8_t)(data >> 8), h[42] = (uint8_t)(data >> 16), h[43] = (uint8_t)(data >> 24);
    fseek(f, 0, SEEK_SET);
    fwrite(h, 1, 44, f);
    fseek(f, 0, SEEK_END);
}

static int find(const char *const *names, int n, const char *s, size_t len)
{
    int i;
    for (i = 0; i < n; i++)
        if (strlen(names[i]) == len && !strncmp(names[i], s, len))
            return i;
    return -1;
}

static int usage(const char *why)
{
    fprintf(stderr, "ec_run: %s (see host/ec_run.c for the options)\n", why);
    return 2;
}

int main(int argc, char **argv)
{
    const char *folder = NULL, *wav_path = NULL, *png_path = NULL, *expect[16];
    struct { uint32_t at; const char *path; } png_at[32];
    uint32_t info_at[32], run_ms = 6000, rms_from = 0;
    int npng = 0, ninfo = 0, nexpect = 0, nev = 0, png_scale = 1, i, bad = 0;
    double min_rms = -1;
    static ev_t ev[MAXEV];
    const fm1core_t *c;
    fm1panel_t *p;
    FILE *wav = NULL;
    uint64_t frames_done = 0, rms_frames = 0;
    double sumsq = 0, peak = 0, t0, wall;
    uint32_t ms, last_frame = 0, lcd0;
    char info[1024];

    for (i = 1; i < argc; i++) {
        const char *a = argv[i];
        const int more = i + 1 < argc;
        if (!strcmp(a, "--folder") && more) folder = argv[++i];
        else if (!strcmp(a, "--ms") && more) run_ms = (uint32_t)strtoul(argv[++i], NULL, 10);
        else if (!strcmp(a, "--wav") && more) wav_path = argv[++i];
        else if (!strcmp(a, "--png") && more) png_path = argv[++i];
        else if (!strcmp(a, "--png-scale") && more) png_scale = atoi(argv[++i]);
        else if (!strcmp(a, "--rms-from") && more) rms_from = (uint32_t)strtoul(argv[++i], NULL, 10);
        else if (!strcmp(a, "--min-rms") && more) min_rms = atof(argv[++i]);
        else if (!strcmp(a, "--expect-ui") && more && nexpect < 16) expect[nexpect++] = argv[++i];
        else if (!strcmp(a, "--info-at") && more && ninfo < 32) info_at[ninfo++] = (uint32_t)strtoul(argv[++i], NULL, 10);
        else if (!strcmp(a, "--png-at") && more && npng < 32) {
            char *s = argv[++i], *colon = strchr(s, ':');
            if (!colon)
                return usage("--png-at MS:FILE");
            png_at[npng].at = (uint32_t)strtoul(s, NULL, 10);
            png_at[npng++].path = colon + 1;
        } else if ((!strcmp(a, "--press") || !strcmp(a, "--turn") || !strcmp(a, "--key") || !strcmp(a, "--master")) &&
                   more && nev < MAXEV) {
            char *s = argv[++i], *at = strchr(s, '@'), *e;
            ev_t *v = &ev[nev];
            if (!at)
                return usage("event without @MS");
            memset(v, 0, sizeof *v);
            if (!strcmp(a, "--press")) {
                v->kind = 0;
                v->what = find(BTN, FM1P_NB, s, (size_t)(at - s));
                v->dur = 60;
            } else if (!strcmp(a, "--turn")) {
                v->kind = 1;
                v->what = find(ROLE, FM1P_NE - 1, s, (size_t)(at - s));
            } else if (!strcmp(a, "--key")) {
                v->kind = 2;
                v->what = atoi(s);
                v->dur = 100;
                if (v->what < 0 || v->what >= FM1P_NKEY)
                    v->what = -1;
            } else {
                v->kind = 3;
                v->n = atoi(s);
                v->what = 0;
            }
            if (v->what < 0)
                return usage("unknown button, role or key");
            v->at = (uint32_t)strtoul(at + 1, &e, 10);
            if (*e == '+')
                v->dur = (uint32_t)strtoul(e + 1, NULL, 10);
            else if (*e == ':')
                v->n = atoi(e + 1);
            nev++;
        } else
            return usage(a);
    }

    ec_fs_native_root(folder, "elasticat");
    c = fm1core_get(FM1CORE_ABI);
    if (!c)
        return usage("fm1core_get refused the ABI");
    p = c->panel;
    if (c->flash_stage)
        c->flash_stage(NULL, 0);
    c->options(FM1CORE_FLASH_STAGED, 0, 0, 1);
    p->ready = 2;
    lcd0 = p->lcd_writes;
    c->init(0);
    if (wav_path) {
        wav = fopen(wav_path, "wb");
        if (!wav)
            return usage("cannot write the WAV");
        wav_header(wav, 0);
    }
    t0 = now_s();
    for (ms = 0; ms < run_ms; ms++) {
        int k;
        ec_fs_native_pump();                    /* (between two calls: what was requested before is answered) */
        for (k = 0; k < nev; k++) {
            const ev_t *v = &ev[k];
            if (v->kind == 0) {
                const uint32_t m = 1u << p->btn_id[v->what];
                if (ms == v->at) {
                    p->buttons |= m;
                    p->buttons_tap |= m;
                } else if (ms == v->at + v->dur)
                    p->buttons &= ~m;
            } else if (v->kind == 1 && ms == v->at)
                p->enc[p->enc_id[v->what]] += v->n * p->enc_dir[v->what];
            else if (v->kind == 2) {
                if (ms == v->at) {
                    p->key_vel[v->what] = 100;
                    p->keys |= 1u << v->what;
                    p->keys_tap |= 1u << v->what;
                } else if (ms == v->at + v->dur)
                    p->keys &= ~(1u << v->what);
            } else if (v->kind == 3 && ms == v->at)
                p->master = v->n;
        }
        for (k = 0; k < ninfo; k++)
            if (ms == info_at[k]) {
                c->ui_info(info, sizeof info);
                printf("ui@%u: %s\n", ms, info);
            }
        c->tick(ms);
        if (ms == 0 || ms - last_frame >= 15u) {
            last_frame = ms;
            c->frame();
        } else
            c->idle();
        for (k = 0; k < npng; k++)
            if (ms == png_at[k].at && ec_png_write_lcd(png_at[k].path, p->lcd, FM1P_LCD_W, FM1P_LCD_H, png_scale))
                fprintf(stderr, "ec_run: %s not written\n", png_at[k].path);
        while (frames_done + FM1P_BLOCK <= (uint64_t)ms * FM1P_FS / 1000u) {
            int16_t blk[FM1P_BLOCK * 2];
            c->audio(blk, FM1P_BLOCK);
            if (ms >= rms_from) {
                int j;
                for (j = 0; j < FM1P_BLOCK * 2; j++) {
                    const double x = blk[j] / 32768.0;
                    sumsq += x * x;
                    if (fabs(x) > peak)
                        peak = fabs(x);
                }
                rms_frames += FM1P_BLOCK;
            }
            if (wav)
                fwrite(blk, sizeof blk, 1, wav);
            frames_done += FM1P_BLOCK;
        }
    }
    wall = now_s() - t0;
    if (wav) {
        wav_header(wav, (uint32_t)frames_done);
        fclose(wav);
    }
    c->ui_info(info, sizeof info);
    if (png_path && ec_png_write_lcd(png_path, p->lcd, FM1P_LCD_W, FM1P_LCD_H, png_scale))
        fprintf(stderr, "ec_run: %s not written\n", png_path);
    c->shutdown();
    {
        const double rms = rms_frames ? sqrt(sumsq / (double)(rms_frames * 2u)) : 0;
        printf("%s %s: rms %.4f (peak %.3f) from %u ms, %u LCD writes, %.1fx real time, %u file requests; "
               "ui_info: %s\n",
               c->id, c->version, rms, peak, rms_from, p->lcd_writes - lcd0, wall > 0 ? run_ms / 1000.0 / wall : 0,
               ec_fs_native_served(), info);
        if (min_rms >= 0 && rms < min_rms) {
            fprintf(stderr, "ec_run: silent: RMS %.6f under %.6f\n", rms, min_rms);
            bad = 1;
        }
    }
    for (i = 0; i < nexpect; i++)
        if (!strstr(info, expect[i])) {
            fprintf(stderr, "ec_run: ui_info has no \"%s\"\n", expect[i]);
            bad = 1;
        }
    return bad;
}
