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
 *     --key K@MS[+D][:V]  note key K (0..26) held from MS for D ms (default 100), velocity V (default 100); repeat
 *     --lower I@MS[+D][:V] lower key I (0..15, the steps) held from MS; --upper I@MS[+D] upper key I (0..10:
 *                         T1..T6 S1 S2 MST FILL PATN); repeat
 *     --master V@MS       the MASTER pot set to V (0..1023) at MS; repeat
 *     --script FILE       more options, one per line, without the dashes ("press PLAY@1000", "lower 4@2000+80",
 *                         "png 3000:out.png", "info 3000"); # starts a comment
 *     --flash FILE        the flash image: read at power-on (when it exists), written at the end
 *     --out DIR           relative --png-at files go into DIR (give it before them)
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

#define MAXEV 4096
#define MAXPNG 128

typedef struct {
    int kind;          /* 0 button, 1 turn, 2 key, 3 master, 4 lower, 5 upper */
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

static const char *folder, *wav_path, *png_path, *flash_path, *out_dir, *expect[32];
static struct { uint32_t at; char path[512]; } png_at[MAXPNG];
static uint32_t info_at[64], run_ms = 6000, rms_from;
static int npng, ninfo, nexpect, nev, png_scale = 1;
static double min_rms = -1;
static ev_t ev[MAXEV];

static int parse_script(const char *path);

/* one option (a = "--press" or "press"), its argument v; returns 0 ok, 2 usage */
static int option(const char *a, const char *v)
{
    while (*a == '-')
        a++;
    if (!v)
        return usage(a);
    if (!strcmp(a, "folder")) folder = strdup(v);
    else if (!strcmp(a, "ms")) run_ms = (uint32_t)strtoul(v, NULL, 10);
    else if (!strcmp(a, "wav")) wav_path = strdup(v);
    else if (!strcmp(a, "png") && !(v[0] >= '0' && v[0] <= '9' && strchr(v, ':'))) png_path = strdup(v);
    else if (!strcmp(a, "flash")) flash_path = strdup(v);
    else if (!strcmp(a, "out")) out_dir = strdup(v);
    else if (!strcmp(a, "png-scale")) png_scale = atoi(v);
    else if (!strcmp(a, "rms-from")) rms_from = (uint32_t)strtoul(v, NULL, 10);
    else if (!strcmp(a, "min-rms")) min_rms = atof(v);
    else if (!strcmp(a, "expect-ui") && nexpect < 32) expect[nexpect++] = strdup(v);
    else if ((!strcmp(a, "info-at") || !strcmp(a, "info")) && ninfo < 64) info_at[ninfo++] = (uint32_t)strtoul(v, NULL, 10);
    else if (!strcmp(a, "script")) return parse_script(v);
    else if ((!strcmp(a, "png-at") || !strcmp(a, "png")) && npng < MAXPNG) {
        const char *colon = strchr(v, ':');
        if (!colon)
            return usage("png-at MS:FILE");
        png_at[npng].at = (uint32_t)strtoul(v, NULL, 10);
        if (out_dir && colon[1] != '/')
            snprintf(png_at[npng].path, sizeof png_at[npng].path, "%s/%s", out_dir, colon + 1);
        else
            snprintf(png_at[npng].path, sizeof png_at[npng].path, "%s", colon + 1);
        npng++;
    } else if ((!strcmp(a, "press") || !strcmp(a, "turn") || !strcmp(a, "key") || !strcmp(a, "master") ||
                !strcmp(a, "lower") || !strcmp(a, "upper")) && nev < MAXEV) {
        const char *at = strchr(v, '@');
        char *e;
        ev_t *x = &ev[nev];
        if (!at)
            return usage("event without @MS");
        memset(x, 0, sizeof *x);
        x->n = 100;
        if (!strcmp(a, "press")) {
            x->kind = 0;
            x->what = find(BTN, FM1P_NB, v, (size_t)(at - v));
            x->dur = 60;
        } else if (!strcmp(a, "turn")) {
            x->kind = 1;
            x->what = find(ROLE, FM1P_NE - 1, v, (size_t)(at - v));
            x->n = 0;
        } else if (!strcmp(a, "key") || !strcmp(a, "lower") || !strcmp(a, "upper")) {
            const int lim = !strcmp(a, "key") ? FM1P_NKEY : !strcmp(a, "lower") ? 16 : 11;
            x->kind = !strcmp(a, "key") ? 2 : !strcmp(a, "lower") ? 4 : 5;
            x->what = atoi(v);
            x->dur = 100;
            if (x->what < 0 || x->what >= lim)
                x->what = -1;
        } else {
            x->kind = 3;
            x->n = atoi(v);
            x->what = 0;
        }
        if (x->what < 0)
            return usage("unknown button, role or key");
        x->at = (uint32_t)strtoul(at + 1, &e, 10);
        if (*e == '+')
            x->dur = (uint32_t)strtoul(e + 1, &e, 10);
        if (*e == ':')
            x->n = atoi(e + 1);
        nev++;
    } else
        return usage(a);
    return 0;
}

static int parse_script(const char *path)
{
    char line[1024];
    FILE *f = fopen(path, "r");
    int r = 0;
    if (!f)
        return usage("cannot read the script");
    while (!r && fgets(line, sizeof line, f)) {
        char *s = line, *sp, *hash = strchr(line, '#');
        if (hash)
            *hash = 0;
        while (*s == ' ' || *s == '\t')
            s++;
        s[strcspn(s, "\r\n")] = 0;
        if (!*s)
            continue;
        sp = strchr(s, ' ');
        if (!sp) {
            r = usage(s);
            break;
        }
        *sp++ = 0;
        while (*sp == ' ')
            sp++;
        sp[strcspn(sp, " \t")] = 0;
        r = option(s, sp);
    }
    fclose(f);
    return r;
}

/* lower / upper key index -> the note key (the FM-1's rows: naturals below, sharps above, from F) */
static int key_of(int kind, int i)
{
    int k, nl = 0, nu = 0;
    for (k = 0; k < FM1P_NKEY; k++) {
        const int s = k % 12;
        const int natural = s == 0 || s == 2 || s == 4 || s == 6 || s == 7 || s == 9 || s == 11;
        if (natural && kind == 4 && nl++ == i)
            return k;
        if (!natural && kind == 5 && nu++ == i)
            return k;
    }
    return 0;
}

int main(int argc, char **argv)
{
    int i, bad = 0;
    const fm1core_t *c;
    fm1panel_t *p;
    FILE *wav = NULL;
    uint64_t frames_done = 0, rms_frames = 0;
    double sumsq = 0, peak = 0, t0, wall;
    uint32_t ms, last_frame = 0, lcd0;
    char info[2048];

    for (i = 1; i < argc; i += 2) {
        const int r = option(argv[i], i + 1 < argc ? argv[i + 1] : NULL);
        if (r)
            return r;
    }

    ec_fs_native_root(folder, "elasticat");
    c = fm1core_get(FM1CORE_ABI);
    if (!c)
        return usage("fm1core_get refused the ABI");
    p = c->panel;
    if (flash_path)
        c->options(flash_path, 0, 1, 1);
    else {
        if (c->flash_stage)
            c->flash_stage(NULL, 0);
        c->options(FM1CORE_FLASH_STAGED, 0, 0, 1);
    }
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
                    p->key_vel[v->what] = (uint8_t)v->n;
                    p->keys |= 1u << v->what;
                    p->keys_tap |= 1u << v->what;
                } else if (ms == v->at + v->dur)
                    p->keys &= ~(1u << v->what);
            } else if (v->kind == 3 && ms == v->at)
                p->master = v->n;
            else if (v->kind == 4 || v->kind == 5) {
                const int key = key_of(v->kind, v->what);
                if (ms == v->at) {
                    p->key_vel[key] = (uint8_t)v->n;
                    p->keys |= 1u << key;
                    p->keys_tap |= 1u << key;
                } else if (ms == v->at + v->dur)
                    p->keys &= ~(1u << key);
            }
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
