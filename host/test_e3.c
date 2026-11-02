/* SPDX-License-Identifier: GPL-3.0-only */
/* test_e3: the E3 model natively: the razor table (Auto-Chop finds the transients of a synthetic loop, zero
 * crossings, add / delete with the steps following, the RZR1 chunk round trip and an old record without it), the
 * sequencer's slice events (razor ranges respected, per-slice choke / pitch / play mode / mute), the razor machines'
 * voicing in the engine (razor_slice mono, razor_poly capped at 8), the E3 params' spec -> engine mapping. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ec_project.h"
#include "ec_seq.h"
#include "ec_internal.h"   /* the voice pool (test_voicing) */

#define FS 44100.0
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

static ec_project_t P, Q;
static ec_seq_t S;
static ec_event_t EV[1024];
static int nev;
static void on_event(void *c, const ec_event_t *e)
{
    (void)c;
    if (nev < 1024)
        EV[nev++] = *e;
}
static void on_params(void *c, int t, const ec_track_params_t *p) { (void)c, (void)t, (void)p; }
static void on_process(void *c, float *o, int n) { (void)c, (void)o, (void)n; }

/* a 2-bar loop at 120 BPM: 8 hits (noise bursts) at known places, quiet noise between */
#define LOOP_FRAMES (4 * 44100)
static int16_t loop[2 * LOOP_FRAMES];
static const double HITS[8] = {0.0, 0.125, 0.25, 0.375, 0.5, 0.5625, 0.75, 0.875};
static void make_loop(void)
{
    uint32_t r = 1234567u, i;
    int h;
    for (i = 0; i < LOOP_FRAMES; i++) {
        double v;
        r = r * 1664525u + 1013904223u;
        v = (double)(int32_t)r / 2147483648.0 * 0.01;
        for (h = 0; h < 8; h++) {
            const double t0 = HITS[h] * LOOP_FRAMES;
            if (i >= t0 && i < t0 + 0.2 * FS)
                v += (double)(int32_t)(r * 2654435761u) / 2147483648.0 * 0.8 * exp(-(i - t0) / (0.03 * FS));
        }
        loop[2 * i] = loop[2 * i + 1] = (int16_t)(v * 32767);
    }
}

static void test_autochop(void)
{
    ec_onsets_t on;
    float pos[64];
    int n, i, ok = 1;
    printf("razor: Auto-Chop finds the transients of a synthetic loop\n");
    ec_project_init(&P);
    P.pat[0].tr[0].v[EC_P_MACHINE] = EC_MACH_RAZOR_SLICE;
    ec_razor_onsets(loop, LOOP_FRAMES, 44100, &on);
    n = ec_razor_transients(&on, 0.5f, pos, 64);
    printf("  %d transients:", n);
    for (i = 0; i < n; i++)
        printf(" %.4f", pos[i]);
    printf("\n");
    CHECK(n == 7, "%d transients, want 7 (the hit at 0 is the head the original leaves out)", n);
    for (i = 0; i < n && i < 7; i++)
        if (fabs(pos[i] - HITS[i + 1]) > 2.0 / EC_RZ_ONSET_WINDOWS)
            ok = 0;
    CHECK(ok, "a transient is more than 2 windows off its hit");
    n = ec_razor_autochop(&P, 0, pos, n, 0.0, 1.0);
    CHECK(n == 7 && (int)P.pat[0].tr[0].v[EC_P_SLICE_COUNT] == 7, "autochop count %d", n);
    for (i = 0; i < 7; i++) {
        CHECK(fabsf(P.razor[0].start[i] - (float)HITS[i + 1] * 128) < 0.6f, "slice %d start %.3f", i + 1,
              P.razor[0].start[i]);
        CHECK(fabsf(P.razor[0].end[i] - (i < 6 ? (float)HITS[i + 2] * 128 : 128.0f)) < 0.6f, "slice %d end %.3f",
              i + 1, P.razor[0].end[i]);
    }
    /* a track window (trim x Range) of the second half: only the transients inside, in track space */
    n = ec_razor_transients(&on, 0.5f, pos, 64);
    n = ec_razor_autochop(&P, 1, pos, n, 0.5, 1.0);
    CHECK(n == 4 && fabsf(P.razor[1].start[0] - 0) < 1.2f && fabsf(P.razor[1].start[1] - 16) < 1.2f,
          "half window: %d slices, %.2f %.2f", n, P.razor[1].start[0], P.razor[1].start[1]);
    /* sensitivity: 0 finds fewer or the same */
    CHECK(ec_razor_transients(&on, 0.0f, pos, 64) <= 7, "sens 0 found more");
}

static void test_zero_cross(void)
{
    static int16_t sn[2 * 4410];
    int i;
    double p, q;
    printf("razor: zero-x snap lands on a sign change\n");
    for (i = 0; i < 4410; i++)
        sn[2 * i] = sn[2 * i + 1] = (int16_t)(20000 * sin(2 * M_PI * 100 * i / 44100.0 + 0.3));
    p = ec_razor_zero_cross(sn, 4410, 44100, 0.37, 0.01);
    i = (int)(p * 4410 + 0.5);
    CHECK(i > 0 && ((sn[2 * i] >= 0) != (sn[2 * (i - 1)] >= 0) || (sn[2 * i] >= 0) != (sn[2 * (i + 1)] >= 0)),
          "frame %d is not at a crossing", i);
    q = fabs(p - 0.37) * 4410;
    CHECK(q < 0.006 * 44100, "crossing %g frames away", q);
}

static void test_edit(void)
{
    ec_razor_t *rz;
    printf("razor: point clamps, friends, add / delete with the steps following\n");
    ec_project_init(&P);
    rz = &P.razor[0];
    P.pat[0].tr[0].v[EC_P_MACHINE] = EC_MACH_RAZOR_POLY;
    P.pat[0].tr[0].v[EC_P_SLICE_COUNT] = 4;
    ec_razor_redistribute(rz, 4);
    ec_razor_set_point(rz, 2, 1, 10, 0, 4);              /* end before its start: end = start + 0.01 */
    CHECK(fabsf(rz->end[1] - 32.01f) < 1e-3f, "end clamp %.3f", rz->end[1]);
    ec_razor_set_point(rz, 2, 1, 50, 1, 4);              /* friends: the next start follows */
    CHECK(rz->end[1] == 50 && rz->start[2] == 50, "friends %.2f %.2f", rz->end[1], rz->start[2]);
    P.pat[0].tr[0].steps[0].slices = 1u << 2 | 1u << 3; /* slices 3 + 4 */
    P.pat[0].tr[0].steps[1].slices = 1u << 1;           /* slice 2 */
    CHECK(ec_razor_add(&P, 0, 2, 40) == 5, "add count");
    CHECK(rz->start[1] == 32 && rz->end[1] == 40 && rz->start[2] == 40 && rz->end[2] == 50, "split %.1f %.1f %.1f %.1f",
          rz->start[1], rz->end[1], rz->start[2], rz->end[2]);
    CHECK(P.pat[0].tr[0].steps[0].slices == (1u << 3 | 1u << 4), "steps after add %x", P.pat[0].tr[0].steps[0].slices);
    CHECK(P.pat[0].tr[0].steps[1].slices == 1u << 1, "slice 2 stays %x", P.pat[0].tr[0].steps[1].slices);
    CHECK(ec_razor_delete(&P, 0, 2) == 4, "delete count");
    CHECK(P.pat[0].tr[0].steps[1].slices == 0 && P.pat[0].tr[0].steps[0].slices == (1u << 2 | 1u << 3),
          "steps after delete %x %x", P.pat[0].tr[0].steps[1].slices, P.pat[0].tr[0].steps[0].slices);
    CHECK(rz->start[1] == 40 && rz->end[1] == 50, "after delete %.1f %.1f", rz->start[1], rz->end[1]);
}

static uint32_t fnv(const uint8_t *b, uint32_t n)
{
    uint32_t h = 2166136261u, i;
    for (i = 0; i < n; i++)
        h = (h ^ b[i]) * 16777619u;
    return h;
}

static void test_chunk(void)
{
    static uint8_t buf[1 << 20];
    int32_t n, n2;
    uint32_t len;
    printf("record: the RZR1 chunk round trip; an old record (no chunk) loads with the default table\n");
    ec_project_init(&P);
    P.razor[3].start[5] = 12.5f;
    P.razor[3].end[5] = 19.25f;
    P.razor[3].choke[5] = 3;
    P.razor[3].mute[6] = 1;
    P.razor[3].play[7] = EC_SLICE_LOOP;
    P.razor[3].pitch[8] = -7;
    P.razor[3].snap = EC_SNAP_TRANSIENT;
    P.razor[3].sens = 80;
    P.pat[0].tr[2].v[EC_P_HARM_INTERVAL2] = 12;
    n = ec_project_save(&P, buf, sizeof buf);
    CHECK(n > 0, "save");
    CHECK(ec_project_load(&Q, buf, (uint32_t)n) == 0, "load");
    CHECK(!memcmp(&P.razor, &Q.razor, sizeof P.razor), "razor tables differ after the round trip");
    CHECK(Q.pat[0].tr[2].v[EC_P_HARM_INTERVAL2] == 12, "E3 param lost");
    /* the same record cut before the chunk (= a 0.1.0 record): header length + checksum fixed */
    /* the first tagged chunk (RZR1) starts where a 0.1.0 record ended; later chunks (EC4B, E5) follow it */
    {
        const uint32_t tag = EC_RAZOR_TAG;
        n2 = 0;
        for (int32_t at = 16; at + 4 <= n; at++)
            if (!memcmp(buf + at, &tag, 4)) {
                n2 = at;
                break;
            }
        CHECK(n2 > 16, "RZR1 chunk not found");
    }
    len = (uint32_t)n2 - 16;
    memcpy(buf + 8, &len, 4);
    {
        const uint32_t sum = fnv(buf + 16, len);
        memcpy(buf + 12, &sum, 4);
    }
    CHECK(ec_project_load(&Q, buf, (uint32_t)n2) == 0, "old record does not load");
    {
        ec_razor_t d;
        ec_razor_init(&d);
        CHECK(!memcmp(&Q.razor[3], &d, sizeof d), "old record: razor not default");
    }
    /* an unknown chunk after ours is skipped */
    n = ec_project_save(&P, buf, sizeof buf);
    {
        const uint32_t tag = 0x58585858u, l = 4, junk = 7;
        memcpy(buf + n, &tag, 4), memcpy(buf + n + 4, &l, 4), memcpy(buf + n + 8, &junk, 4);
        len = (uint32_t)n + 12 - 16;
        memcpy(buf + 8, &len, 4);
        {
            const uint32_t sum = fnv(buf + 16, len);
            memcpy(buf + 12, &sum, 4);
        }
        CHECK(ec_project_load(&Q, buf, (uint32_t)n + 12) == 0 && !memcmp(&P.razor, &Q.razor, sizeof P.razor),
              "unknown chunk not skipped");
    }
}

static void test_seq_events(void)
{
    ec_seq_sink_t k = {0, on_event, on_params, on_process};
    ec_ptrack_t *tr;
    int i, seen3 = 0, seen4 = 0, seen5 = 0;
    printf("seq: razor slice events carry the slice's own range + per-slice choke / pitch / play mode; mute skips\n");
    ec_project_init(&P);
    tr = &P.pat[0].tr[0];
    tr->v[EC_P_MACHINE] = EC_MACH_RAZOR_POLY;
    tr->v[EC_P_SLICE_COUNT] = 6;
    P.razor[0].start[2] = 10.5f, P.razor[0].end[2] = 23.25f;
    P.razor[0].choke[2] = 2;
    P.razor[0].pitch[2] = 5;
    P.razor[0].play[3] = EC_SLICE_LOOP;
    P.razor[0].mute[4] = 1;
    tr->steps[0].trig = 1;
    tr->steps[0].slices = 1u << 2 | 1u << 3 | 1u << 4;
    ec_seq_init(&S, &P, &k, FS);
    nev = 0;
    ec_seq_start(&S);
    ec_seq_render(&S, 0, 512);
    for (i = 0; i < nev; i++) {
        const ec_event_t *e = &EV[i];
        if (e->type != EC_EV_TRIG || e->track != 0)
            continue;
        if (e->slice == 3) {
            seen3 = 1;
            CHECK(e->start == 10.5f && e->end == 23.25f, "slice 3 range %.2f..%.2f", e->start, e->end);
            CHECK(e->choke == 2 && e->pitch == 5 && e->length == 0 && e->mode == 0, "slice 3 choke %d pitch %.1f len %d",
                  e->choke, e->pitch, e->length);
        }
        if (e->slice == 4) {
            seen4 = 1;
            CHECK(e->mode == EC_SLICE_LOOP && e->length > 0, "slice 4 play mode %d length %d", e->mode, e->length);
            CHECK(e->start == P.razor[0].start[3] && e->end == P.razor[0].end[3], "slice 4 range");
        }
        if (e->slice == 5)
            seen5 = 1;
    }
    CHECK(seen3 && seen4, "slices 3 / 4 not fired");
    CHECK(!seen5, "muted slice 5 fired");
    /* grid machine: no explicit range (the engine divides), per-slice settings still apply */
    tr->v[EC_P_MACHINE] = EC_MACH_GRID_SLICE;
    ec_seq_init(&S, &P, &k, FS);
    nev = 0;
    ec_seq_start(&S);
    ec_seq_render(&S, 0, 512);
    for (i = 0; i < nev; i++)
        if (EV[i].type == EC_EV_TRIG && EV[i].slice == 3)
            CHECK(EV[i].start < 0 && EV[i].choke == 2, "grid slice 3: start %.1f choke %d", EV[i].start, EV[i].choke);
}

static void test_voicing(void)
{
    static uint8_t mem[EC_DSP_MEM_MAX] __attribute__((aligned(16)));
    static int16_t smp[2 * 44100];
    ec_dsp_t *d;
    ec_track_params_t p;
    ec_sample_t s;
    float out[256];
    int i, m;
    printf("engine: razor_slice is mono, razor_poly stacks up to 8, slices read their own ranges\n");
    CHECK(ec_dsp_mem_size() <= sizeof mem, "engine needs %zu bytes", ec_dsp_mem_size());
    for (i = 0; i < 44100; i++)
        smp[2 * i] = smp[2 * i + 1] = (int16_t)(8000 * sin(i * 0.05));
    s.data = smp, s.frames = 44100, s.rate = 44100, s.channels = 1, s.bpm = 0;
    for (m = 0; m < 2; m++) {
        d = ec_dsp_init(mem, 44100);
        ec_track_params_default(&p);
        p.machine = m ? EC_MACH_RAZOR_POLY : EC_MACH_RAZOR_SLICE;
        p.slice_play_mode = EC_SLICE_LOOP;
        ec_dsp_track_params(d, 0, &p);
        ec_dsp_set_sample(d, 0, &s);
        for (i = 0; i < 10; i++) {
            ec_event_t e;
            memset(&e, 0, sizeof e);
            e.type = EC_EV_SLICE_ON, e.slice = i + 1, e.velocity = 1, e.length = 44100;
            e.start = (float)(i * 12), e.end = (float)(i * 12 + 5);
            ec_dsp_event(d, &e);
        }
        ec_dsp_process(d, out, 128);
        {
            ec_track_status_t st;
            int counted = 0, k, inside = 1;
            ec_dsp_track_status(d, 0, &st);
            for (k = 0; k < EC_VOICE_POOL; k++)
                if (((const struct ec_dsp *)d)->v[k].used && ((const struct ec_dsp *)d)->v[k].counted)
                    counted++;
            for (k = 0; k < st.voices && k < EC_MAX_VOICES_TRACK; k++) {
                const int sl = st.voice_slice[k];
                const float lo = (float)((sl - 1) * 12) / 128.0f, hi = (float)((sl - 1) * 12 + 5) / 128.0f;
                if (st.voice_pos[k] < lo - 0.001f || st.voice_pos[k] > hi + 0.001f)
                    inside = 0;
            }
            if (m == 0)
                CHECK(counted == 1, "razor_slice: %d live voices", counted);
            else
                CHECK(counted == EC_MAX_VOICES_TRACK, "razor_poly: %d live voices (cap %d)", counted,
                      EC_MAX_VOICES_TRACK);
            CHECK(inside, "a voice reads outside its razor slice");
        }
    }
}

static void test_params(void)
{
    float v[EC_NP];
    ec_track_params_t p;
    printf("params: the E3 rows map to the engine; exp detents\n");
    ec_params_default(v);
    v[EC_P_HARM_INTERVAL3] = -5;
    v[EC_P_WT_LFO_DEPTH] = 96;
    v[EC_P_WT_LFO_SHAPE] = 4;
    v[EC_P_FORMANT_SHIFT] = 3;
    ec_params_engine(v, 0, 2, &p);
    CHECK(p.harm_interval[0] == 7 && p.harm_interval[2] == -5, "harm");
    CHECK(fabsf(p.wt_lfo_depth - 0.5f) < 1e-6f && p.wt_lfo_shape == 4 && p.wt_cycle == 600, "wavetable");
    CHECK(p.formant_shift == 3 && fabsf(p.wsola_window - 0.08f) < 1e-6f, "formant / ola");
    CHECK(ec_param_nudge(EC_P_WT_CYCLE, 600, 12) == 1200, "cycle +12 detents = x2 (%g)", ec_param_nudge(EC_P_WT_CYCLE, 600, 12));
    CHECK(ec_param_nudge(EC_P_WT_LFO_RATE, 0, 1) == 0.01f && ec_param_nudge(EC_P_WT_LFO_RATE, 0.01f, -1) == 0,
          "lfo rate from / to 0");
}

int main(void)
{
    make_loop();
    test_autochop();
    test_zero_cross();
    test_edit();
    test_chunk();
    test_seq_events();
    test_voicing();
    test_params();
    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
