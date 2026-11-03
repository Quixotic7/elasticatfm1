/* SPDX-License-Identifier: GPL-3.0-only */
/* make_heavy: writes a flash image (header + project record) of the heaviest project for CPU measurements
 * (ec_run --flash, FM1VST's rfm1_run --flash-in), for the folder host/make_demo_folder.py writes:
 *   T1 drums  HARMONIZER (3 voices)       insert BLACKHOLE   LFO 1 -> PITCH, LFO 2 -> CUTOFF
 *   T2 bass   RANDOM OLA (density 32)     insert RINGS       LFO 1 -> PITCH, mod env -> RES
 *   T3 chords SPECTRAL FREEZE + formant   insert PHASER      LFO 1 -> PITCH
 *   T4 perc   R.POLY, 8 voices, every 16th, auto-chopped    insert FLANGER     LFO 1 -> CUTOFF
 *   T5 chords WAVETABLE (LFO 300 Hz)      insert REVERB      LFO 1 -> PITCH, LFO 2 -> PAN
 *   T6 drums  GSTRETCH 2                  insert TAPE ECHO   LFO 1 -> PITCH
 *   every track sends to S1 (REVERB) and S2 (TAPE ECHO), POST; the master insert COMP (threshold down).
 *   make_heavy <out.flash> [options]
 * 0.6.1: options for the load meter's cost table (src/dsp/ec_load.c) and the two-insert worst case:
 *   --ins1 M / --ins2 M   every track's INS 1 / INS 2 machine M (1..18) over whatever the project has
 *   --plain               a neutral project instead: six LOOP tracks (TAPE, a trig every 4th step), no inserts, no
 *                         sends, no LFOs or matrix, the buses NONE; then, to isolate one cost:
 *   --warp W              every track's warp mode W (1..13)
 *   --machine K           every track's machine K (1..8; slice machines: 16 slices, a pad a step)
 *   --density D           grain density D (GRAIN warp / grain clouds)
 *   --tracks N            only the first N tracks play (the others: TRIG machine, no trigs: idle)
 *   --bus M               S1, S2 and the master insert all M, every track sending 60 / 40
 *   --mod                 the heavy project's LFOs and full matrix on every track
 *   --only T              (the heavy project) only track T (1..6) plays: the others TRIG, no trigs
 *   --nobus               (the heavy project) no sends, the buses NONE */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ec_mtx.h"
#include "ec_project.h"
#include "ec_razor.h"
#include "ec_version.h"
#include "ec_layout_test.h"

static ec_project_t P;
static uint8_t img[1u << 20];

static void track(int t, int slot, int machine, int mode, int fx, int lfo1, int lfo2)
{
    float *v = P.pat[0].tr[t].v;
    int s;
    v[EC_P_SLOT] = (float)slot;
    v[EC_P_MACHINE] = (float)machine;
    v[EC_P_MODE] = (float)mode;
    v[EC_P_FX_MACHINE] = (float)fx;
    v[EC_P_FX_MIX] = 90;
    v[EC_P_SEND1] = 60;
    v[EC_P_SEND2] = 40;
    v[EC_P_SEND_TAP] = 2;
    v[EC_P_LFO1_DEST] = (float)lfo1;
    v[EC_P_LFO1_WAVE] = 2;
    v[EC_P_LFO1_SPEED] = 6;
    v[EC_P_LFO1_DEPTH] = 72;
    v[EC_P_LFO2_DEST] = (float)lfo2;
    v[EC_P_LFO2_WAVE] = 1;
    v[EC_P_LFO2_SPEED] = 5;
    v[EC_P_LFO2_DEPTH] = lfo2 > 1 ? 100 : 64;
    v[EC_P_FILTER_CUTOFF] = 90;
    for (s = 0; s < 16; s++)        /* a trig on every step (loops: retrigger; slices: one pad each) */
        P.pat[0].tr[t].steps[s].trig = machine != 1 || s % 4 == 0;
}

static void mod_full(int t)
{
    static const int GEN[][3] = {{EC_MS_LFO2 + 1, EC_P_FILTER_DRIVE, 40}, {EC_MS_ENV + 1, EC_P_FX_MIX, -30},
                                 {EC_MS_M1 + 1, EC_P_SEND1, 50},        {EC_MS_LFO1 + 1, EC_P_WARP_RATE, 10},
                                 {EC_MS_VEL + 1, EC_P_SEND2, 25},       {EC_MS_LFO2 + 1, EC_P_FILTER_MIX, -20},
                                 {EC_MS_LFO1 + 1, EC_P_GRAIN_DENSITY, 30}, {EC_MS_M2 + 1, EC_P_FX_DRIVE, 60}};
    float *v = P.pat[0].tr[t].v;
    int k, g = 0;
    ec_mtx_migrate(v);
    v[EC_P_MACRO1] = 64;
    v[EC_P_MENV_DEPTH] = 100;
    for (k = 0; k < EC_MTX_SLOTS; k++)
        if (ec_mtx_src(v, k) == EC_MS_OFF) {
            v[ec_mtx_src_id(k)] = (float)GEN[g][0];
            v[ec_mtx_dst_id(k)] = (float)(GEN[g][1] + 1);
            v[ec_mtx_depth_id(k)] = (float)GEN[g][2];
            g++;
        }
}

/* 0.6.1: the neutral project (--plain) and its one-cost variants */
static void plain(int warp, int machine, int density, int tracks, int bus, int mod)
{
    int t, s;
    for (t = 0; t < EC_NTRACK; t++) {
        float *v = P.pat[0].tr[t].v;
        const int on = t < tracks;
        v[EC_P_SLOT] = (float)(1 + t % 3);
        v[EC_P_MACHINE] = on ? (float)machine : 2.0f;
        v[EC_P_MODE] = (float)warp;
        v[EC_P_FILTER_CUTOFF] = 90;
        if (density > 0)
            v[EC_P_GRAIN_DENSITY] = (float)density;
        if (bus > 1) {
            v[EC_P_SEND1] = 60;
            v[EC_P_SEND2] = 40;
            v[EC_P_SEND_TAP] = 2;
        }
        if (machine >= 3 && machine <= 6)
            v[EC_P_SLICE_COUNT] = 16;
        for (s = 0; s < 16; s++) {
            P.pat[0].tr[t].steps[s].trig = on && (machine != 1 || s % 4 == 0);
            if (on && machine >= 3 && machine <= 6)
                P.pat[0].tr[t].steps[s].slices = 1u << s;
        }
        if (on && machine >= 3 && machine <= 6)
            ec_razor_redistribute(&P.razor[t], 16);
        if (mod) {
            v[EC_P_LFO1_DEST] = 2;
            v[EC_P_LFO1_WAVE] = 2;
            v[EC_P_LFO1_SPEED] = 6;
            v[EC_P_LFO1_DEPTH] = 72;
            v[EC_P_LFO2_DEST] = 3;
            v[EC_P_LFO2_SPEED] = 5;
            v[EC_P_LFO2_DEPTH] = 100;
            mod_full(t);
        }
    }
    for (s = 0; s < 3; s++) {
        P.bus[s][EC_P_FX_MACHINE] = (float)(bus > 1 ? bus : 1);
        P.bus[s][EC_P_FX_MIX] = 127;
    }
}

int main(int argc, char **argv)
{
    int32_t n;
    FILE *f;
    int s, i;
    int o_plain = 0, o_warp = 1, o_machine = 1, o_density = 0, o_tracks = EC_NTRACK, o_bus = 1, o_mod = 0;
    int o_ins1 = 0, o_ins2 = 0, o_only = 0, o_nobus = 0;
    ec_layout_use(&EC_LAYOUT_TEST);   /* the tests' fixed layout: 0.4.0's pages (host/ec_layout_test.h) */
    if (argc < 2) {
        fprintf(stderr, "make_heavy <out.flash> [--ins1 M] [--ins2 M] [--plain [--warp W] [--machine K] "
                        "[--density D] [--tracks N] [--bus M] [--mod]]\n");
        return 2;
    }
    for (i = 2; i < argc; i++) {
        const char *a = argv[i];
        const int x = i + 1 < argc ? atoi(argv[i + 1]) : 0;
        if (!strcmp(a, "--plain"))
            o_plain = 1;
        else if (!strcmp(a, "--mod"))
            o_mod = 1;
        else if (!strcmp(a, "--nobus"))
            o_nobus = 1;
        else if (i + 1 < argc && !strcmp(a, "--only"))
            o_only = x, i++;
        else if (i + 1 < argc && !strcmp(a, "--warp"))
            o_warp = x, i++;
        else if (i + 1 < argc && !strcmp(a, "--machine"))
            o_machine = x, i++;
        else if (i + 1 < argc && !strcmp(a, "--density"))
            o_density = x, i++;
        else if (i + 1 < argc && !strcmp(a, "--tracks"))
            o_tracks = x, i++;
        else if (i + 1 < argc && !strcmp(a, "--bus"))
            o_bus = x, i++;
        else if (i + 1 < argc && !strcmp(a, "--ins1"))
            o_ins1 = x, i++;
        else if (i + 1 < argc && !strcmp(a, "--ins2"))
            o_ins2 = x, i++;
        else {
            fprintf(stderr, "make_heavy: unknown option %s\n", a);
            return 2;
        }
    }
    ec_project_init(&P);
    snprintf(P.slot[0].path, EC_PATH, "/audio/a_drums_bpm120.wav");
    snprintf(P.slot[1].path, EC_PATH, "/audio/b_bass_bpm120.wav");
    snprintf(P.slot[2].path, EC_PATH, "/audio/c_chords_bpm120.wav");
    snprintf(P.slot[3].path, EC_PATH, "/audio/d_perc16_bpm120.wav");
    for (s = 0; s < 4; s++) {
        P.slot[s].bpm = 120;
        P.slot[s].steps = s == 3 ? 16 : 32;
    }
    /* option indexes: machine LOOP 1 .. R.POLY 6; warp TAPE 1, VARI 2, DOMINO 3, GRAIN 4, OLA 5, PITCH 6, HARM 7,
     * WTBL 8, FREEZE 9, FORMNT 10, GSTR 11, GSTR2 12; FX NONE 1, DELAY 2, REVERB 3, .. B.HOLE 8, FLANGR 10, PHASER 11,
     * T.ECHO 14, RINGS 17; LFO dest OFF 1, PITCH 2, CUT 3, RES 4, AMP 5, PAN 6 */
    track(0, 1, 1, 7, 8, 2, 3);
    P.pat[0].tr[0].v[EC_P_HARM_INTERVAL] = 7;
    P.pat[0].tr[0].v[EC_P_HARM_INTERVAL2] = -5;
    P.pat[0].tr[0].v[EC_P_HARM_INTERVAL3] = 12;
    P.pat[0].tr[0].v[EC_P_MODE_MACRO] = 0.7f;
    track(1, 2, 1, 5, 17, 2, 1);
    P.pat[0].tr[1].v[EC_P_GRAIN_DENSITY] = 32;
    P.pat[0].tr[1].v[EC_P_MENV_DEST] = 4;
    P.pat[0].tr[1].v[EC_P_MENV_DEPTH] = 100;
    track(2, 3, 1, 9, 11, 2, 1);
    P.pat[0].tr[2].v[EC_P_FREEZE_AMOUNT] = 0.6f;
    P.pat[0].tr[2].v[EC_P_FORMANT_SHIFT] = 5;
    track(3, 4, 6, 1, 10, 3, 1);
    P.pat[0].tr[3].v[EC_P_SLICE_COUNT] = 16;
    for (s = 0; s < 16; s++)
        P.pat[0].tr[3].steps[s].slices = 1u << s | 1u << ((s + 5) % 16);
    ec_razor_redistribute(&P.razor[3], 16);
    track(4, 3, 1, 8, 3, 2, 6);
    P.pat[0].tr[4].v[EC_P_WT_LFO_RATE] = 300;
    P.pat[0].tr[4].v[EC_P_WT_LFO_DEPTH] = 100;
    track(5, 1, 1, 12, 14, 2, 1);
    P.bus[0][EC_P_FX_MACHINE] = 3;      /* S1 REVERB */
    P.bus[0][EC_P_FX_MIX] = 127;
    P.bus[1][EC_P_FX_MACHINE] = 14;     /* S2 TAPE ECHO */
    P.bus[1][EC_P_FX_MIX] = 127;
    P.bus[2][EC_P_FX_MACHINE] = 5;      /* MST COMP */
    P.bus[2][EC_P_COMP_THRESH] = 20;
    P.bus[2][EC_P_COMP_RATIO] = 60;
    for (int t = 0; t < EC_NTRACK; t++)   /* 0.5.0: the 0.4 routes as a load converts them, then every slot full:
                                           * generic destinations on every track (resolved every chunk) */
        mod_full(t);
    if (o_plain) {                          /* 0.6.1: the neutral project instead (the cost table's variants) */
        for (int t = 0; t < EC_NTRACK; t++) {
            ec_params_default(P.pat[0].tr[t].v);
            memset(P.pat[0].tr[t].steps, 0, sizeof P.pat[0].tr[t].steps);
        }
        for (s = 0; s < 3; s++)
            ec_params_default(P.bus[s]);
        plain(o_warp, o_machine, o_density, o_tracks, o_bus, o_mod);
    }
    for (int t = 0; t < EC_NTRACK; t++) {   /* 0.6.1: one track alone, no buses */
        float *v = P.pat[0].tr[t].v;
        if (o_only && t != o_only - 1) {
            v[EC_P_MACHINE] = 2;
            memset(P.pat[0].tr[t].steps, 0, sizeof P.pat[0].tr[t].steps);
        }
        if (o_nobus)
            v[EC_P_SEND1] = v[EC_P_SEND2] = 0;
    }
    if (o_nobus)
        for (s = 0; s < 3; s++)
            P.bus[s][EC_P_FX_MACHINE] = 1;
    for (int t = 0; t < EC_NTRACK; t++) {   /* 0.6.1: the inserts asked for */
        float *v = P.pat[0].tr[t].v;
        if (o_ins1) {
            v[EC_P_FX_MACHINE] = (float)o_ins1;
            v[EC_P_FX_MIX] = 90;
        }
        if (o_ins2) {
            v[EC_P_INS2_MACHINE] = (float)o_ins2;
            ec_ins2_reset(v);
            v[EC_P_INS2_MIX] = 90;
        }
    }
    memset(img, 0xFF, sizeof img);
    n = ec_project_save(&P, img + 4096, sizeof img - 4096);
    if (n <= 0) {
        fprintf(stderr, "save failed\n");
        return 1;
    }
    {   /* the flash header (src/plat/ec_core.c: magic "ECAT", format 1, header 4096, image 1 MiB, record size) */
        const uint32_t magic = 0x54414345u, isz = 1u << 20, rsz = (uint32_t)n;
        const uint16_t fmt = 1, hsz = 4096;
        char ver[16] = {0};
        memset(img, 0, 64);
        memcpy(img, &magic, 4);
        memcpy(img + 4, &fmt, 2);
        memcpy(img + 6, &hsz, 2);
        memcpy(img + 8, &isz, 4);
        memcpy(img + 12, &rsz, 4);
        snprintf(ver, sizeof ver, "%s", EC_VERSION);
        memcpy(img + 16, ver, 16);
    }
    f = fopen(argv[1], "wb");
    if (!f || fwrite(img, 1, 4096 + (size_t)n, f) != 4096 + (size_t)n) {
        fprintf(stderr, "cannot write %s\n", argv[1]);
        return 1;
    }
    fclose(f);
    printf("%s: heavy project, record %d bytes\n", argv[1], (int)n);
    return 0;
}
