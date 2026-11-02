/* SPDX-License-Identifier: GPL-3.0-only */
/* ec_test_io: the file layer end to end, natively. Writes WAV / AIFF / AIFC files of every supported kind into a
 * temporary RemoteFM1 folder, scans /audio through the async API (ec_fs_native: answers one tick late), loads each
 * file into the sample store and checks frames, rate, channels and the decoded values. Also the bpm-from-name rule.
 *   ec_test_io <scratch dir>     (made if missing; its audio/ is rewritten) */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "ec_audiofile.h"
#include "ec_browse.h"
#include "ec_fs_native.h"
#include "ec_loader.h"
#include "ec_samplestore.h"

static int g_fail;
#define CHECK(c, ...)                                                                                          \
    do {                                                                                                       \
        if (!(c)) {                                                                                            \
            fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__);                                               \
            fprintf(stderr, __VA_ARGS__);                                                                      \
            fputc('\n', stderr);                                                                               \
            g_fail = 1;                                                                                        \
        }                                                                                                      \
    } while (0)

/* the test signal: a value in -16000..16000 per frame and channel */
static int sig(uint32_t i, int c) { return ((int)((i * 37u + (uint32_t)c * 1000u) % 2000u) - 1000) * 16; }

typedef struct {
    const char *name;
    int aiff;             /* 0 WAV, 1 AIFF, 2 AIFC */
    const char *aifc;     /* AIFC compression id */
    int ext;              /* WAV: WAVE_FORMAT_EXTENSIBLE */
    int is_float, bits, ch;
    uint32_t rate, frames;
    int big_list;         /* WAV: a 100 KB LIST chunk before fmt (the header read needs a second read) */
    int tol;              /* expected decode error */
} spec_t;

static void put16le(FILE *f, unsigned v) { fputc(v & 255, f), fputc(v >> 8 & 255, f); }
static void put32le(FILE *f, unsigned long v) { put16le(f, v & 0xFFFF), put16le(f, v >> 16 & 0xFFFF); }
static void put16be(FILE *f, unsigned v) { fputc(v >> 8 & 255, f), fputc(v & 255, f); }
static void put32be(FILE *f, unsigned long v) { put16be(f, v >> 16 & 0xFFFF), put16be(f, v & 0xFFFF); }

/* one sample in the file's encoding */
static void put_sample(FILE *f, const spec_t *s, int v)
{
    const int be = s->aiff && !(s->aifc && !strcmp(s->aifc, "sowt"));
    if (s->is_float) {
        float x = (float)v / 32768.0f;
        uint32_t u;
        memcpy(&u, &x, 4);
        if (be)
            put32be(f, u);
        else
            put32le(f, u);
        return;
    }
    switch (s->bits) {
    case 8:
        fputc(s->aiff ? (v >> 8) & 255 : ((v >> 8) + 128) & 255, f);
        break;
    case 16:
        if (be)
            put16be(f, (unsigned)v & 0xFFFF);
        else
            put16le(f, (unsigned)v & 0xFFFF);
        break;
    case 24: {
        const uint32_t u = (uint32_t)(v * 256) & 0xFFFFFF;
        if (be)
            fputc(u >> 16, f), fputc(u >> 8 & 255, f), fputc(u & 255, f);
        else
            fputc(u & 255, f), fputc(u >> 8 & 255, f), fputc(u >> 16, f);
        break;
    }
    case 32:
        if (be)
            put32be(f, (uint32_t)(v * 65536));
        else
            put32le(f, (uint32_t)(v * 65536));
        break;
    }
}

/* the 80-bit extended float of an AIFF rate */
static void put_ext80(FILE *f, uint32_t rate)
{
    int e = 0;
    uint64_t m = rate;
    while (!(m & 0x8000000000000000ull)) {
        m <<= 1;
        e++;
    }
    put16be(f, (unsigned)(16383 + 63 - e));
    put32be(f, (unsigned long)(m >> 32));
    put32be(f, (unsigned long)(m & 0xFFFFFFFFu));
}

static void write_file(const char *path, const spec_t *s)
{
    FILE *f = fopen(path, "wb");
    const uint32_t ba = (uint32_t)s->ch * (uint32_t)s->bits / 8u, data = s->frames * ba;
    uint32_t i;
    int c;
    if (!f) {
        CHECK(0, "cannot write %s", path);
        return;
    }
    if (!s->aiff) {
        const uint32_t fmt = s->ext ? 40 : 16, list = s->big_list ? 100000u : 0;
        put32le(f, 0x46464952u);   /* RIFF */
        put32le(f, 4 + (8 + fmt) + (list ? 8 + list : 0) + 8 + data + (data & 1));
        fwrite("WAVE", 1, 4, f);
        if (list) {
            fwrite("LIST", 1, 4, f);
            put32le(f, list);
            for (i = 0; i < list; i++)
                fputc('x', f);
        }
        fwrite("fmt ", 1, 4, f);
        put32le(f, fmt);
        put16le(f, s->ext ? 0xFFFE : s->is_float ? 3 : 1);
        put16le(f, (unsigned)s->ch);
        put32le(f, s->rate);
        put32le(f, s->rate * ba);
        put16le(f, ba);
        put16le(f, (unsigned)s->bits);
        if (s->ext) {
            put16le(f, 22);
            put16le(f, (unsigned)s->bits);
            put32le(f, s->ch == 1 ? 4 : 3);
            put16le(f, s->is_float ? 3 : 1);   /* the sub-format GUID's first two bytes, then its fixed rest */
            fwrite("\x00\x00\x00\x00\x10\x00\x80\x00\x00\xAA\x00\x38\x9B\x71", 1, 14, f);
        }
        fwrite("data", 1, 4, f);
        put32le(f, data);
    } else {
        const int aifc = s->aiff == 2;
        const uint32_t comm = aifc ? 24 : 18;
        fwrite("FORM", 1, 4, f);
        put32be(f, 4 + (8 + comm) + 8 + 8 + data);
        fwrite(aifc ? "AIFC" : "AIFF", 1, 4, f);
        fwrite("COMM", 1, 4, f);
        put32be(f, comm);
        put16be(f, (unsigned)s->ch);
        put32be(f, s->frames);
        put16be(f, (unsigned)s->bits);
        put_ext80(f, s->rate);
        if (aifc) {
            fwrite(s->aifc, 1, 4, f);
            put16be(f, 0);                     /* (an empty pstring, padded) */
        }
        fwrite("SSND", 1, 4, f);
        put32be(f, 8 + data);
        put32be(f, 0);
        put32be(f, 0);
    }
    for (i = 0; i < s->frames; i++)
        for (c = 0; c < s->ch; c++)
            put_sample(f, s, sig(i, c));
    if (!s->aiff && (data & 1))
        fputc(0, f);
    fclose(f);
}

static int run_until(int (*step)(void), int stop_a, int stop_b, int limit)
{
    int st = -1, n;
    for (n = 0; n < limit; n++) {
        ec_fs_native_pump();
        st = step();
        if (st == stop_a || st == stop_b)
            return st;
    }
    return st;
}

int main(int argc, char **argv)
{
    static const spec_t S[] = {
        {"a_pcm16_st.wav", 0, NULL, 0, 0, 16, 2, 44100, 30000, 0, 0},
        {"b_pcm24_mono_48k.wav", 0, NULL, 0, 0, 24, 1, 48000, 20000, 0, 0},
        {"c_float_st.WAV", 0, NULL, 0, 1, 32, 2, 44100, 10000, 0, 1},
        {"d_pcm32_st.wav", 0, NULL, 0, 0, 32, 2, 96000, 9000, 1, 0},
        {"e_u8_mono_bpm92.wav", 0, NULL, 0, 0, 8, 1, 22050, 5001, 0, 256},
        {"f_ext24_st.wave", 0, NULL, 1, 0, 24, 2, 44100, 300000, 0, 0},
        {"sub/g_aiff16_st.aif", 1, NULL, 0, 0, 16, 2, 22050, 7000, 0, 0},
        {"sub/h_sowt_st.aifc", 2, "sowt", 0, 0, 16, 2, 44100, 6000, 0, 0},
        {"sub/i_fl32_mono.aifc", 2, "fl32", 0, 1, 32, 1, 32000, 4000, 0, 1},
        {"sub/deeper/j_aiff24_mono.aiff", 1, NULL, 0, 0, 24, 1, 44100, 3000, 0, 0},
    };
    const int N = (int)(sizeof S / sizeof S[0]);
    char root[512], p[1024];
    int i, st;
    if (argc < 2) {
        fprintf(stderr, "usage: ec_test_io <scratch dir>\n");
        return 2;
    }
    snprintf(root, sizeof root, "%s", argv[1]);
    mkdir(root, 0755);
    snprintf(p, sizeof p, "%s/audio", root);
    mkdir(p, 0755);
    snprintf(p, sizeof p, "%s/audio/sub", root);
    mkdir(p, 0755);
    snprintf(p, sizeof p, "%s/audio/sub/deeper", root);
    mkdir(p, 0755);
    for (i = 0; i < N; i++) {
        snprintf(p, sizeof p, "%s/audio/%s", root, S[i].name);
        write_file(p, &S[i]);
    }
    snprintf(p, sizeof p, "%s/audio/notes.txt", root);       /* not audio: skipped */
    fclose(fopen(p, "w"));
    snprintf(p, sizeof p, "%s/audio/.hidden.wav", root);     /* hidden: not listed */
    fclose(fopen(p, "w"));

    /* bpm from names (Elasticat's bpm_from_filename) */
    CHECK(ec_bpm_from_name("testloop_bpm120.wav") == 120.0f, "bpm120");
    CHECK(ec_bpm_from_name("amen 136BPM.wav") == 136.0f, "136BPM");
    CHECK(fabsf(ec_bpm_from_name("x_BPM-92.5_y.aif") - 92.5f) < 0.01f, "BPM-92.5");
    CHECK(ec_bpm_from_name("kick.wav") == 0.0f, "none");

    /* no folder */
    ec_fs_native_root(NULL, "elasticat");
    ec_browse_start("/audio");
    st = run_until(ec_browse_step, EC_BR_DONE, EC_BR_ERROR, 10);
    CHECK(st == EC_BR_ERROR && ec_browse_error() == -8, "no folder: state %d error %d", st, ec_browse_error());

    /* the scan */
    ec_fs_native_root(root, "elasticat");
    ec_browse_start("/audio");
    st = run_until(ec_browse_step, EC_BR_DONE, EC_BR_ERROR, 1000);
    CHECK(st == EC_BR_DONE, "scan state %d", st);
    CHECK(ec_browse_count() == N, "scan found %d files, expected %d", ec_browse_count(), N);
    for (i = 0; i < ec_browse_count() && i < N; i++) {
        snprintf(p, sizeof p, "/audio/%s", S[i].name);
        CHECK(!strcmp(ec_browse_path(i), p), "file %d is %s, expected %s", i, ec_browse_path(i), p);
    }

    /* each file loaded and checked */
    for (i = 0; i < N; i++) {
        const ec_sample_t *s;
        uint32_t f, bad = 0;
        snprintf(p, sizeof p, "/audio/%s", S[i].name);
        ec_ss_reset();
        CHECK(ec_load_start(p) == 0, "%s: start", p);
        st = run_until(ec_load_step, EC_LD_DONE, EC_LD_ERROR, 100000);
        CHECK(st == EC_LD_DONE, "%s: state %d (%s)", p, st, ec_load_error());
        s = ec_ss_ready(ec_load_handle());
        CHECK(s != NULL, "%s: not ready", p);
        if (!s)
            continue;
        CHECK(s->frames == S[i].frames, "%s: %u frames, expected %u", p, s->frames, S[i].frames);
        CHECK(s->rate == S[i].rate, "%s: rate %u, expected %u", p, s->rate, S[i].rate);
        CHECK(s->channels == S[i].ch, "%s: %d channels", p, s->channels);
        for (f = 0; f < s->frames; f++) {
            const int l = sig(f, 0), r = S[i].ch > 1 ? sig(f, 1) : l;
            if (abs(s->data[2 * f] - l) > S[i].tol || abs(s->data[2 * f + 1] - r) > S[i].tol) {
                if (!bad)
                    fprintf(stderr, "%s: frame %u is %d %d, expected %d %d\n", p, f, s->data[2 * f],
                            s->data[2 * f + 1], l, r);
                bad++;
            }
        }
        CHECK(!bad, "%s: %u frames decoded wrong", p, bad);
        printf("ok %-32s %6u frames %5u Hz %d ch %2d-bit%s bpm %.1f\n", S[i].name, s->frames, s->rate, s->channels,
               s->bits, s->is_float ? " float" : "", s->bpm);
    }

    /* a missing file */
    ec_ss_reset();
    ec_load_start("/audio/missing.wav");
    st = run_until(ec_load_step, EC_LD_DONE, EC_LD_ERROR, 100);
    CHECK(st == EC_LD_ERROR && !strcmp(ec_load_error(), "not found"), "missing: %d %s", st, ec_load_error());

    printf(g_fail ? "ec_test_io: FAILED\n" : "ec_test_io: all passed\n");
    return g_fail;
}
