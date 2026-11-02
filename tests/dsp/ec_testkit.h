/*
 * ec_testkit.h -- shared helpers for the DSP tests / render / bench:
 * a synthesized drum + bass loop (no third-party audio), sine samples, a WAV
 * writer, engine setup shortcuts. Header-only (static functions).
 */
#ifndef EC_TESTKIT_H
#define EC_TESTKIT_H

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../src/dsp/ec_dsp.h"

#define TK_FS 44100
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static uint32_t tk_rng = 12345u;
static float tk_noise(void)
{
    tk_rng = tk_rng * 1664525u + 1013904223u;
    return (float)((int32_t)tk_rng) / 2147483648.f;
}

static int16_t tk_s16(float x)
{
    if (x > 1.f) x = 1.f;
    if (x < -1.f) x = -1.f;
    return (int16_t)lrintf(x * 32767.f);
}

/* One bar (16 steps) of drums + bass at `bpm`, stereo, rate TK_FS.
 * Returns malloc'd interleaved int16, *frames set. */
static inline int16_t *tk_drum_loop(float bpm, uint32_t *frames)
{
    double step = 15.0 * TK_FS / bpm;
    uint32_t n = (uint32_t)(16.0 * step + 0.5);
    float *L = calloc(n, sizeof(float)), *R = calloc(n, sizeof(float));
    static const int kick[16] = {1,0,0,0, 0,0,1,0, 1,0,0,0, 0,0,0,0};
    static const int snare[16] = {0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,1};
    static const int bass[16] = {36,0,36,0, 0,39,0,36, 41,0,0,39, 0,36,0,43};
    for (int s = 0; s < 16; ++s) {
        uint32_t st = (uint32_t)(s * step);
        if (kick[s]) {
            double ph = 0;
            for (uint32_t i = 0; i < (uint32_t)(0.35 * TK_FS) && st + i < n; ++i) {
                double t = (double)i / TK_FS;
                double f = 45.0 + 110.0 * exp(-t * 30.0);
                ph += 2 * M_PI * f / TK_FS;
                float v = (float)(sin(ph) * exp(-t * 7.0)) * 0.9f;
                L[st + i] += v; R[st + i] += v;
            }
        }
        if (snare[s]) {
            for (uint32_t i = 0; i < (uint32_t)(0.25 * TK_FS) && st + i < n; ++i) {
                double t = (double)i / TK_FS;
                float nz = tk_noise() * (float)exp(-t * 18.0) * 0.45f;
                float tone = (float)(sin(2 * M_PI * 185.0 * t) * exp(-t * 25.0)) * 0.3f;
                L[st + i] += nz * 0.9f + tone; R[st + i] += nz * 1.1f + tone;
            }
        }
        /* hats every step, open on the off-beats */
        {
            float hp = 0, prev = 0;
            double dec = (s % 4 == 2) ? 12.0 : 45.0;
            for (uint32_t i = 0; i < (uint32_t)(0.15 * TK_FS) && st + i < n; ++i) {
                double t = (double)i / TK_FS;
                float x = tk_noise();
                hp = 0.6f * (hp + x - prev); prev = x;
                float v = hp * (float)exp(-t * dec) * 0.22f;
                L[st + i] += v * 0.7f; R[st + i] += v * 1.2f;
            }
        }
        if (bass[s]) {
            double f = 440.0 * pow(2.0, (bass[s] - 69) / 12.0);
            double ph = 0;
            for (uint32_t i = 0; i < (uint32_t)(step * 1.6) && st + i < n; ++i) {
                double t = (double)i / TK_FS;
                ph += 2 * M_PI * f / TK_FS;
                double saw = fmod(ph / (2 * M_PI), 1.0) * 2.0 - 1.0;
                float v = (float)((0.6 * sin(ph) + 0.25 * saw) * exp(-t * 4.0)) * 0.35f;
                L[st + i] += v; R[st + i] += v;
            }
        }
    }
    int16_t *d = malloc(n * 2 * sizeof(int16_t));
    for (uint32_t i = 0; i < n; ++i) { d[2 * i] = tk_s16(L[i] * 0.8f); d[2 * i + 1] = tk_s16(R[i] * 0.8f); }
    free(L); free(R);
    *frames = n;
    return d;
}

static inline int16_t *tk_sine(float hz, float amp, uint32_t frames)
{
    int16_t *d = malloc(frames * 2 * sizeof(int16_t));
    for (uint32_t i = 0; i < frames; ++i) {
        int16_t v = tk_s16(amp * (float)sin(2 * M_PI * hz * i / TK_FS));
        d[2 * i] = v; d[2 * i + 1] = v;
    }
    return d;
}

static void tk_put32(FILE *f, uint32_t v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); fputc((v >> 16) & 255, f); fputc((v >> 24) & 255, f); }
static void tk_put16(FILE *f, uint16_t v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); }

static inline int tk_write_wav(const char *path, const float *lr, uint32_t frames)
{
    FILE *f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "cannot write %s\n", path); return -1; }
    fwrite("RIFF", 1, 4, f); tk_put32(f, 36 + frames * 4); fwrite("WAVEfmt ", 1, 8, f);
    tk_put32(f, 16); tk_put16(f, 1); tk_put16(f, 2); tk_put32(f, TK_FS); tk_put32(f, TK_FS * 4);
    tk_put16(f, 4); tk_put16(f, 16); fwrite("data", 1, 4, f); tk_put32(f, frames * 4);
    for (uint32_t i = 0; i < frames * 2; ++i) tk_put16(f, (uint16_t)tk_s16(lr[i]));
    fclose(f);
    return 0;
}

static inline int tk_write_wav_s16(const char *path, const int16_t *lr, uint32_t frames)
{
    float *t = malloc(frames * 2 * sizeof(float));
    for (uint32_t i = 0; i < frames * 2; ++i) t[i] = lr[i] / 32768.f;
    int r = tk_write_wav(path, t, frames);
    free(t);
    return r;
}

static ec_dsp_t *tk_engine(void)
{
    void *mem = NULL;
    if (posix_memalign(&mem, 16, ec_dsp_mem_size()) != 0) return NULL;
    return ec_dsp_init(mem, (float)TK_FS);
}

static ec_event_t tk_ev(int offset, int type, int track)
{
    ec_event_t e;
    memset(&e, 0, sizeof e);
    e.offset = offset; e.type = type; e.track = track;
    e.start = e.end = -1.f;
    e.velocity = 1.f;
    return e;
}

#endif
