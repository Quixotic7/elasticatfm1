/*
 * ec_load.c -- 0.6.1: the counted load (docs/UX-V2.md §9 "Load meter"). After every ec_dsp_process the engine counts
 * what it is running -- the tracks that read (by warp mode), the other awake tracks, the slice voices, the grains,
 * the live mod engines, the FX slots that run (by machine: both inserts, the sends, the master) -- weighs each by its
 * measured cost and smooths the sum (~0.25 s). Nothing is timed: the reading follows the audio state only, so every
 * host shows the same meter (RFM1-FORMAT: the screen never depends on the host clock).
 *
 * The weights are the wasm3 cost of each thing as a share of the hosts' budget, 2x real time: 10000 = the cart then
 * runs at exactly 2x real time (0.5 s of CPU per second of audio). Measured 2026-11-03 on the M-series Mac the
 * owner's hosts run on, with FM1VST's rfm1_run (wasm3) and the .rfm1 built from this tree at MEMORY_MIB 256, over
 * host/make_heavy.c's variants (--plain: six tracks, a trig every 4th step; PLAY at 1.5 s; the playing part's CPU
 * seconds per audio second = the difference of a 4 s and a 24 s run / 20 s, the better of two runs):
 *   base             --plain --tracks 0 (playing, nothing reads: 0.0662 s/s), the same stopped (0.0305 s/s)
 *   warp mode W      (--plain --warp W - --plain --tracks 0) / 6 tracks, + W_CHAIN
 *   machine FX M     (--plain --ins1 M - --plain) / 6 inserts (the same machine in INS 2 measures the same)
 *   grain            the GRAIN warp at density 4 / 16 / 32 / 64 against its counted grains (5.7 .. 5.9 / 10000 a
 *                    grain), OLA at its default and density 32 (5.85); the GRAIN / OLA bases are the rest
 *   slice voice      --plain --machine 3 / 5 against the counted voices
 *   mod              (--plain --mod - --plain) / 6 (LFOs + the 8 matrix slots resolved every chunk); W_MODWARP from
 *                    the heavy project's tracks alone (--only T --nobus) against the rest of this table
 *   bus              (--plain --bus M - --plain) / 3 against W_FX[M]
 * Validation (the model's reading vs the measured load, 1.0 = 2x real time): the heavy project 0.95 vs 0.98 .. 1.03
 * (it measures 1.95x .. 2.05x real time, the host's own noise), without its buses 0.85 vs 0.87, its tracks alone
 * 0.29 .. 0.39 vs 0.27 .. 0.40, the plain variants within 3 % (R.POLY grain clouds: 1.45 vs 1.35).
 */
#include <string.h>

#include "ec_internal.h"
#include "ec_load.h"

/* in 1/10000 of the 2x budget (see above; 10000 = exactly 2x real time). Index: warp mode 1..13 (0 unused); each
 * includes the track's chain (filter, envelopes, the track mix: W_CHAIN) */
static const uint16_t W_MODE[EC_WARP_COUNT + 1] = {
    0,
    124,  /* TAPE      (--warp 1: 0.0945 s/s) */
    124,  /* VARI      (0.0943) */
    148,  /* DOMINO    (0.1015) */
    91,   /* GRAIN     (its grains apart: 24 .. 384 grains at density 4 .. 64, 0.154 .. 1.187 s/s) */
    145,  /* OLA       (its TGrains grains apart: 31 / 77 grains at density default / 32, 0.190 / 0.325 s/s) */
    244,  /* PITCH     (0.1305) */
    268,  /* HARM      (0.1377: one voice, the default; each more voice W_HARMV) */
    315,  /* WTBL      (0.1516) */
    364,  /* FREEZE    (0.1664) */
    373,  /* FORMNT    (0.1690) */
    255,  /* GSTR      (0.1338) */
    253,  /* GSTR2     (0.1332) */
    123,  /* CHOP      (0.0942) */
};
#define W_BASE_PLAY 1140  /* playing: the sequencer, the screens redrawn every frame, the host shim (--tracks 0: 0.0662
                           * s/s with six awake tracks, less their chains) */
#define W_BASE_STOP 610   /* stopped (0.0305 s/s, nothing awake, the screen mostly still) */
#define W_CHAIN 30        /* an awake track that is not reading (slice machines, NEIGH, INPUT, a tail; an estimate:
                           * the --tracks 0 project's six can't be told from the base) */
#define W_VOICE 96        /* a sounding slice voice (S.POLY / SLICE: 25 / 7 voices, 0.1857 / 0.1000 s/s) */
#define W_GRAIN 58        /* a sounding grain (GRAIN warp, OLA, a voice's grain cloud: the density slopes) */
#define W_HARMV 130       /* a harmonizer voice past the first (a pitch shifter) */
#define W_MOD 169         /* a track's live mod engine (LFOs, envelope, the matrix: --mod 0.1453 s/s) */
#define W_MODWARP 250     /* .. on a track reading a warp other than TAPE / VARI / CHOP (the heavy project's tracks
                           * alone, --only T: the modulated pitch / rate costs those warps more) */
#define W_BUS 60          /* a running send / master slot beyond its machine (the sends into it: --bus) */
/* per FX machine 1..18 (NONE 0): (--ins1 M - --plain) / 6 */
static const uint16_t W_FX[EC_FX_COUNT + 1] = {
    0,
    0,    /* NONE */
    178,  /* DELAY     (0.1479) */
    399,  /* REVERB    (0.2142; in INS 2: 0.2110) */
    95,   /* LOFI      (0.1230) */
    108,  /* COMP      (0.1268) */
    202,  /* DESTROY   (0.1551) */
    223,  /* ECHO      (0.1614) */
    640,  /* BLACKHOLE (0.2938; in INS 2: 0.2776) */
    174,  /* CHORUS    (0.1469) */
    159,  /* FLANGER   (0.1421) */
    186,  /* PHASER    (0.1504) */
    117,  /* DJ EQ     (0.1297) */
    104,  /* DUCK      (0.1258) */
    278,  /* TAPE ECHO (0.1778) */
    280,  /* CASSETTE  (0.1785) */
    67,   /* MOTION    (0.1145) */
    112,  /* RINGS     (0.1282) */
    110,  /* LIMIT     (0.1275) */
};

static int fx_counts(const ec_fx_t *fx)
{
    return fx->running != EC_FX_NONE && (!fx->asleep || fx->fdir != 0);
}

void ec_dsp_load_counts(const ec_dsp_t *d, ec_load_counts_t *c)
{
    memset(c, 0, sizeof *c);
    for (int ti = 0; ti < EC_TRACKS; ++ti) {
        const ec_track_t *t = &d->tr[ti];
        if (t->idle) continue;
        const int reads = t->reader_on && t->mode >= 1 && t->mode <= EC_WARP_COUNT;
        if (reads) {
            c->mode[t->mode]++;
            if (t->fam == FAM_GRAINS) c->grains += t->gr.n;
            if (t->fam == FAM_READER && t->w3.stage == W3_ST_OLA) c->grains += t->w3.ong;
            if (t->fam == FAM_READER && t->w3.stage == W3_ST_HARM)
                for (int v = 1; v < 3; ++v) c->harm += t->w3.h_gate[v] > 0.5f;
        } else
            c->chain++;
        if (t->mod.alive) {
            c->mods++;
            if (reads && t->mode != EC_WARP_TAPE && t->mode != EC_WARP_TEMPO_VARISPEED && t->mode != EC_WARP_CHOPPED)
                c->modwarps++;
        }
        if (fx_counts(&t->fx)) c->fx[t->fx.running]++;
        if (fx_counts(&t->fx2)) c->fx[t->fx2.running]++;
    }
    for (int i = 0; i < EC_VOICE_POOL; ++i) {
        const ec_voice_t *v = &d->v[i];
        if (!v->used || v->track < 0 || v->track >= EC_TRACKS || d->tr[v->track].idle) continue;
        c->voices++;
        if (v->kind == 1) c->grains += v->ng;
    }
    for (int b = 0; b < 3; ++b)
        if (fx_counts(&d->bus[b])) { c->fx[d->bus[b].running]++; c->buses++; }
    c->playing = d->playing != 0;
}

float ec_load_weigh(const ec_load_counts_t *c)
{
    int32_t s = (c->playing ? W_BASE_PLAY : W_BASE_STOP) + W_CHAIN * c->chain + W_VOICE * c->voices +
                W_GRAIN * c->grains + W_HARMV * c->harm + W_MOD * c->mods + W_MODWARP * c->modwarps + W_BUS * c->buses;
    for (int m = 1; m <= EC_WARP_COUNT; ++m) s += W_MODE[m] * c->mode[m];
    for (int m = 1; m <= EC_FX_COUNT; ++m) s += W_FX[m] * c->fx[m];
    return (float)s * 0.0001f;
}

void ec_load_update(ec_dsp_t *d, int frames)
{
    ec_load_counts_t c;
    ec_dsp_load_counts(d, &c);
    const float x = ec_load_weigh(&c);
    if (!d->load_init) { d->load = x; d->load_init = 1; return; }
    float k = (float)frames / (0.25f * d->fs);    /* ~0.25 s */
    if (k > 1.f) k = 1.f;
    d->load += (x - d->load) * k;
}

float ec_dsp_load(const ec_dsp_t *d) { return d ? d->load : 0.f; }
