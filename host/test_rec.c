/* SPDX-License-Identifier: GPL-3.0-only */
/* test_rec (0.6.0, the data side): the project record version 2 (sizes measured: a realistic dense 64-pattern
 * project, recorded automation, the worst cases; the 64-pattern / 128-slot round trip; a save that does not fit
 * leaves the old record alone), old records loading into bank A and slots 1..16 (0.1.0's e2_flash.bin, 0.5.0's
 * v05_flash.bin made by make_v05_fixture.c, the FM1VST glue's test project), and the sequencer side of live
 * recording: micro timing (late, early, early at the loop's end, never early across a queued pattern switch),
 * lock-only steps, ec_seq_rec_quantize / _consume, the metronome's beats per block, count-in, start_at and the song's
 * beat counter.   ec_test_rec <host/fixtures> */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ec_e5.h"
#include "ec_mtx.h"
#include "ec_plat.h"
#include "ec_project.h"
#include "ec_scene.h"
#include "ec_seq.h"

#define FS 48000.0
#define MAXREC 8192

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
static uint8_t big[32u << 20], buf2[EC_PLAT_RECORD_MAX];
static uint32_t rng = 12345;
static uint32_t rnd(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}

/* ------------------------------------------------------------------------------------------- sizes --- */
/* a realistic dense project: 64 used patterns, 6 tracks with ~30 non-default params, 32 trigs per track with ~4
 * locks (pitch, cutoff, velocity, one more), 128 slots with paths, names, both scenes; `autom` adds recorded
 * automation (two params locked on every one of 64 steps, ghost locks where there is no trig) */
static void dense(ec_project_t *p, int autom)
{
    int i, t, k, s;
    ec_project_init(p);
    for (i = 0; i < EC_NSLOT; i++) {
        snprintf(p->slot[i].path, EC_PATH, "/audio/Breaks/Amen_%03d_bpm%d.wav", i, 90 + i % 80);
        p->slot[i].bpm = (float)(90 + i % 80);
        p->slot[i].trim_end = 0.9f;
    }
    for (i = 0; i < EC_NPAT; i++) {
        ec_pattern_prepare(p, i);
        snprintf(p->pe5[i].name, EC_NAME, "BEAT %d", i);
        for (t = 0; t < EC_NTRACK; t++) {
            ec_ptrack_t *tr = &p->pat[i].tr[t];
            for (k = 0; k < 30; k++) {       /* ~30 params away from their defaults (a third fractional) */
                const int id = (int)(rnd() % (EC_P_MTX_END - 1)) + 1;
                tr->v[id] = ec_param_clamp(id, EC_PARAMS[id].min + (EC_PARAMS[id].max - EC_PARAMS[id].min) *
                                                                       (float)(rnd() % 1000) / 1000.0f);
            }
            tr->v[EC_P_PATTERN_STEPS] = 64;
            for (k = 0; k < 32; k++) {
                ec_step_t *st = &tr->steps[k * 2];
                st->trig = 1;
                ec_step_lock_set(st, EC_P_PITCH, (float)((int)(rnd() % 25) - 12));
                ec_step_lock_set(st, EC_P_FILTER_CUTOFF, (float)(rnd() % 128));
                ec_step_lock_set(st, EC_P_VELOCITY, (float)(rnd() % 100) / 100.0f);
                ec_step_lock_set(st, EC_P_MICRO, (float)((int)(rnd() % 25) - 12));
            }
            if (autom)
                for (s = 0; s < 64; s++) {
                    ec_step_lock_set(&tr->steps[s], EC_P_FILTER_RES, (float)(rnd() % 128));
                    ec_step_lock_set(&tr->steps[s], EC_P_MACRO1, (float)(rnd() % 128));
                }
        }
    }
    for (k = 0; k < 60; k++) {
        ec_scene_set(p, 0, (int)(rnd() % EC_NTRACK), EC_P_FILTER_CUTOFF + (int)(rnd() % 8), (float)(rnd() % 128));
        ec_scene_set(p, 1, (int)(rnd() % EC_NTRACK), EC_P_FILTER_CUTOFF + (int)(rnd() % 8), (float)(rnd() % 128));
    }
}

/* the worst: every param of every track fractional, every step of every pattern 16 fractional locks */
static void worst(ec_project_t *p, int steps)
{
    int i, t, k, s;
    ec_project_init(p);
    for (i = 0; i < EC_NSLOT; i++)
        memset(p->slot[i].path, 'x', EC_PATH - 1);
    for (i = 0; i < EC_NPAT; i++) {
        ec_pattern_prepare(p, i);
        for (t = 0; t < EC_NTRACK; t++) {
            ec_ptrack_t *tr = &p->pat[i].tr[t];
            for (k = 0; k < EC_NP; k++)
                tr->v[k] = EC_PARAMS[k].def + 0.37f;
            for (s = 0; steps && s < EC_MAXSTEPS; s++) {
                tr->steps[s].trig = 1;
                tr->steps[s].slices = 0x80000001u;
                for (k = 0; k < EC_STEP_LOCKS; k++)
                    ec_step_lock_set(&tr->steps[s], k + 1, 0.5f + (float)k);
            }
        }
    }
}

static void test_sizes(void)
{
    const uint32_t cap = EC_PLAT_RECORD_MAX;
    int32_t a, b, c, d, w1, w2, v1;
    dense(&P, 0);
    a = ec_project_save(&P, big, sizeof big);
    v1 = ec_project_save_v1(&P, big, sizeof big);
    CHECK(a > 0 && ec_project_save(&P, buf2, cap) == a, "the dense project fits the record (%d of %u)", a, cap);
    CHECK(ec_project_load(&Q, buf2, (uint32_t)a) == 0 && !memcmp(Q.pat, P.pat, sizeof P.pat) &&
              !memcmp(Q.slot, P.slot, sizeof P.slot) && !memcmp(Q.pe5, P.pe5, sizeof P.pe5),
          "the dense project round-trips");
    dense(&P, 1);
    b = ec_project_save(&P, big, sizeof big);
    CHECK(b > 0 && ec_project_save(&P, buf2, cap) == b, "with recorded automation it fits too (%d)", b);
    ec_project_init(&P);
    c = ec_project_save(&P, big, sizeof big);
    worst(&P, 0);
    w1 = ec_project_save(&P, big, sizeof big);
    worst(&P, 1);
    w2 = ec_project_save(&P, big, sizeof big);
    CHECK(ec_project_save(&P, buf2, cap) == -1, "the worst case does not fit: -1");
    d = 0;
    printf("  record sizes (bytes; the record holds %u): new project %d, dense 64 patterns %d (version 1 would be %d),"
           " dense + automation %d, every param of every track fractional %d, + every step 16 locks %d\n",
           cap, c, a, v1, b, w1, w2);
    (void)d;
}

/* a save that does not fit returns -1 and leaves the record already there alone */
static void test_full(void)
{
    int32_t n, i;
    static uint8_t keep[EC_PLAT_RECORD_MAX];
    dense(&P, 0);
    n = ec_project_save(&P, buf2, sizeof buf2);
    memcpy(keep, buf2, sizeof keep);
    worst(&Q, 1);
    CHECK(ec_project_save(&Q, buf2, sizeof buf2) == -1, "FULL: -1");
    CHECK(!memcmp(keep, buf2, sizeof keep), "FULL: the old record is untouched");
    CHECK(ec_project_load(&Q, buf2, sizeof buf2) == 0 && !memcmp(Q.pat, P.pat, sizeof P.pat),
          "FULL: the old record still loads");
    for (i = 16; i < n; i += n / 7)
        CHECK(ec_project_save(&P, buf2, (uint32_t)i) == -1, "a short cap %d: -1", i);
}

/* ------------------------------------------------------------------------------------------- old records --- */
static int32_t read_file(const char *dir, const char *name, uint8_t *out, uint32_t cap)
{
    char path[1024];
    FILE *f;
    size_t n;
    snprintf(path, sizeof path, "%s/%s", dir, name);
    f = fopen(path, "rb");
    if (!f)
        return -1;
    n = fread(out, 1, cap, f);
    fclose(f);
    return (int32_t)n;
}

static void test_v05(const char *dir)
{
    int32_t n = read_file(dir, "v05_flash.bin", big, sizeof big), m;
    int i, t, ok;
    CHECK(n > 4096, "v05_flash.bin read");
    if (n <= 4096)
        return;
    CHECK(ec_project_load(&P, big + 4096, (uint32_t)n - 4096) == 0, "0.5.0's record loads");
    CHECK(P.bpm == 128 && P.swing == 55 && P.glen == 64 && P.quant == EC_Q_DIRECT_START && P.current == 5, "globals");
    ok = 1;
    for (i = 0; i < EC_NPAT; i++)
        ok &= P.pat[i].used == (i < 16);
    CHECK(ok, "patterns: bank A used (0..15), the rest empty");
    ok = 1;
    for (i = 0; i < EC_NSLOT; i++) {
        char want[EC_PATH];
        snprintf(want, sizeof want, "/audio/slot%02d.wav", i + 1);
        ok &= i < 16 ? !strcmp(P.slot[i].path, want) && P.slot[i].bpm == 90.0f + (float)i && P.slot[i].gain == 1.5f
                     : P.slot[i].path[0] == 0;
    }
    CHECK(ok, "slots 1..16 hold the old pool, 17..128 empty");
    ok = 1;
    for (i = 0; i < 16; i++)
        for (t = 0; t < EC_NTRACK; t++) {
            const ec_ptrack_t *tr = &P.pat[i].tr[t];
            float x;
            const ec_step_t *s = &tr->steps[4 + (i % 4)];
            ok &= tr->v[EC_P_FILTER_CUTOFF] == (float)(40 + i + t) && tr->v[EC_P_PATTERN_STEPS] == 32;
            ok &= ec_mtx_src(tr->v, 0) == EC_MS_LFO1 && ec_mtx_dst(tr->v, 0) == EC_P_FILTER_CUTOFF &&
                  tr->v[ec_mtx_depth_id(0)] == (float)(10 + t);   /* (0.5.0's matrix as it was: no migration) */
            ok &= s->trig && ec_step_lock_get(s, EC_P_PITCH, &x) && x == -2.0f &&
                  ec_step_lock_get(s, EC_P_VELOCITY, &x) && x == 0.75f;
            ok &= t < 3 || s->slices == 2u;
            ok &= !strncmp(P.pe5[i].name, "PAT ", 4) && P.pe5[i].bpm == 100.0f + (float)i && P.pe5[i].page_loop[0] == 1;
        }
    CHECK(ok, "the 16 patterns' params, matrix, steps, locks, names, BPM, page loops");
    CHECK(ec_project_mtx_dropped() == 0, "no routes dropped");
    CHECK(P.e5.sc.has[0][0][EC_P_FILTER_CUTOFF] && P.e5.sc.val[0][0][EC_P_FILTER_CUTOFF] == 11 &&
              P.e5.sc.has[1][2][EC_P_PAN] && P.e5.sc.val[1][2][EC_P_PAN] == 99 && !strcmp(P.e5.name, "OLD SONG"),
          "the scenes and the project name");
    CHECK(P.bus[0][EC_P_FX_MACHINE] == 2 && P.bus[0][EC_P_DELAY_FEEDBACK] == 77 && P.razor[3].start[1] == 0.25f,
          "the buses and the razor");
    {
        ec_recset_t d;
        ec_recset_default(&d);
        CHECK(!memcmp(&P.rec, &d, sizeof d), "an old record: the record settings' defaults");
    }
    /* resaved as version 2: the same project, smaller */
    m = ec_project_save(&P, buf2, sizeof buf2);
    CHECK(m > 0 && m < n - 4096, "resaved as version 2: %d bytes (version 1: %d)", m, n - 4096);
    CHECK(buf2[4] == 2, "version 2");
    CHECK(ec_project_load(&Q, buf2, (uint32_t)m) == 0 && !memcmp(Q.pat, P.pat, sizeof P.pat) &&
              !memcmp(Q.slot, P.slot, sizeof P.slot) && !memcmp(Q.pe5, P.pe5, sizeof P.pe5) &&
              !memcmp(Q.e5.sc.has, P.e5.sc.has, sizeof P.e5.sc.has) &&
              !memcmp(Q.e5.sc.val, P.e5.sc.val, sizeof P.e5.sc.val) && !memcmp(Q.bus, P.bus, sizeof P.bus) &&
              !memcmp(Q.razor, P.razor, sizeof P.razor) && Q.current == P.current,
          "the version 2 resave loads the same project");
}

static void test_old(const char *dir)
{
    int32_t n = read_file(dir, "e2_flash.bin", big, sizeof big);
    int i, ok = 1;
    CHECK(n > 4096 && ec_project_load(&P, big + 4096, (uint32_t)n - 4096) == 0, "0.1.0's e2_flash.bin loads");
    for (i = 16; i < EC_NPAT; i++)
        ok &= !P.pat[i].used;
    for (i = 16; i < EC_NSLOT; i++)
        ok &= !P.slot[i].path[0];
    CHECK(ok && P.pat[P.current].used && P.current < 16, "0.1.0: bank A, slots 1..16");
    n = read_file(dir, "glue_test_project.flash", big, sizeof big);
    CHECK(n > 4096 && ec_project_load(&P, big + 4096, (uint32_t)n - 4096) == 0, "the glue's test project loads");
    n = ec_project_save(&P, buf2, sizeof buf2);
    CHECK(n > 0 && ec_project_load(&Q, buf2, (uint32_t)n) == 0 && !memcmp(Q.pat, P.pat, sizeof P.pat),
          "the glue's test project round-trips as version 2");
}

/* every pattern and slot used, the record settings changed: version 2 round trip */
static void test_full_roundtrip(void)
{
    int32_t n;
    int i, t;
    ec_project_init(&P);
    for (i = 0; i < EC_NSLOT; i++) {
        snprintf(P.slot[i].path, EC_PATH, "/audio/s%d.wav", i);
        P.slot[i].trim_start = 0.001f * (float)i;
    }
    for (i = 0; i < EC_NPAT; i++) {
        ec_pattern_prepare(&P, i);
        for (t = 0; t < EC_NTRACK; t++) {
            P.pat[i].tr[t].v[EC_P_SLOT] = (float)(1 + (i * EC_NTRACK + t) % EC_NSLOT);
            P.pat[i].tr[t].steps[i * 3 % EC_MAXSTEPS].trig = 1;
            ec_step_lock_set(&P.pat[i].tr[t].steps[255], EC_P_FILTER_CUTOFF, 3.25f);   /* a lock-only step */
            ec_step_lock_set(&P.pat[i].tr[t].steps[i * 3 % EC_MAXSTEPS], EC_P_MICRO, -7);
        }
        P.pe5[i].bpm = 60.0f + (float)i;
        snprintf(P.pe5[i].name, EC_NAME, "P%02d", i);
    }
    P.current = ec_pat_index(3, 15);
    P.rec.mode = EC_REC_OVERDUB;
    P.rec.bars = 8;
    P.rec.countin = 2;
    P.rec.metro = EC_METRO_ALWAYS;
    P.rec.metro_level = 0.25f;
    P.rec.automation = 0;
    P.rec.hard = 1;
    n = ec_project_save(&P, buf2, sizeof buf2);
    CHECK(n > 0, "64 patterns / 128 slots saved (%d bytes)", n);
    memset(&Q, 0x5A, sizeof Q);
    CHECK(ec_project_load(&Q, buf2, (uint32_t)n) == 0, "loaded");
    CHECK(!memcmp(Q.pat, P.pat, sizeof P.pat), "every pattern survives");
    CHECK(!memcmp(Q.slot, P.slot, sizeof P.slot), "every slot survives");
    CHECK(!memcmp(Q.pe5, P.pe5, sizeof P.pe5), "every name / BPM survives");
    CHECK(Q.current == 63, "the current pattern 63 survives");
    CHECK(!memcmp(&Q.rec, &P.rec, sizeof P.rec), "the record settings survive");
}

/* ------------------------------------------------------------------------------------------- the sequencer --- */
typedef struct {
    uint64_t at;
    ec_event_t ev;
} rec_t;
static ec_seq_t S;
static rec_t R[MAXREC];
static int nrec;
static float cut_now[EC_NTRACK];

static void on_event(void *ctx, const ec_event_t *e)
{
    (void)ctx;
    if (nrec < MAXREC) {
        R[nrec].at = S.now;
        R[nrec].ev = *e;
        nrec++;
    }
}
static void on_params(void *ctx, int t, const ec_track_params_t *p)
{
    (void)ctx;
    cut_now[t] = p->filter_cutoff;
}
static void setup(float bpm)
{
    ec_seq_sink_t k = {0, on_event, on_params, 0, 0};
    ec_project_init(&P);
    P.bpm = bpm;
    ec_seq_init(&S, &P, &k, FS);
    nrec = 0;
}
static void run(uint64_t n, int block)
{
    while (n > 0) {
        const int b = n > (uint64_t)block ? block : (int)n;
        ec_seq_render(&S, 0, b);
        n -= (uint64_t)b;
    }
}
/* the sample times of track t's trigs, in order */
static int trigs(int t, uint64_t *at, int max)
{
    int i, n = 0;
    for (i = 0; i < nrec && n < max; i++)
        if (R[i].ev.type == EC_EV_TRIG && R[i].ev.track == t)
            at[n++] = R[i].at;
    return n;
}

/* 120 BPM at 48 kHz: a step (1/16) is 6000 samples, a micro unit 250 */
static void test_micro(void)
{
    uint64_t at[64];
    int n;
    ec_ptrack_t *tr;
    setup(120);
    tr = ec_project_track(&P, 0);
    tr->v[EC_P_PATTERN_STEPS] = 16;
    tr->steps[0].trig = 1;
    tr->steps[4].trig = 1;
    tr->steps[8].trig = 1;
    ec_step_lock_set(&tr->steps[0], EC_P_MICRO, -6);    /* a quarter step early */
    ec_step_lock_set(&tr->steps[4], EC_P_MICRO, 5);     /* late */
    ec_step_lock_set(&tr->steps[8], EC_P_MICRO, -12);   /* half a step early */
    ec_seq_start(&S);
    run(16 * 6000 * 2 + 100, 128);
    n = trigs(0, at, 64);
    CHECK(n == 7, "6 trigs in two passes + the third pass's first, early (%d)", n);
    CHECK(at[0] == 0, "the first step cannot fire before the start (%llu)", (unsigned long long)at[0]);
    CHECK(at[1] == 4 * 6000 + 5 * 250, "late +5: %llu", (unsigned long long)at[1]);
    CHECK(at[2] == 8 * 6000 - 12 * 250, "early -12: %llu", (unsigned long long)at[2]);
    CHECK(at[3] == 16 * 6000 - 6 * 250, "the loop's first step fires early at the loop's end: %llu",
          (unsigned long long)at[3]);
    CHECK(at[4] == 20 * 6000 + 5 * 250 && at[5] == 24 * 6000 - 12 * 250 && at[6] == 32 * 6000 - 6 * 250,
          "the second pass the same");
    /* the track value shifts every trig */
    setup(120);
    tr = ec_project_track(&P, 0);
    tr->v[EC_P_PATTERN_STEPS] = 16;
    tr->v[EC_P_MICRO] = 4;
    tr->steps[2].trig = 1;
    ec_seq_start(&S);
    run(6000 * 4, 64);
    n = trigs(0, at, 64);
    CHECK(n == 1 && at[0] == 2 * 6000 + 4 * 250, "the track's micro +4: %llu", (unsigned long long)at[0]);
}

/* a queued pattern switch at the global boundary: no early step of the old pattern's loop, the new pattern's first
 * step on the boundary */
static void test_micro_switch(void)
{
    uint64_t at[64];
    int n, i;
    ec_ptrack_t *tr;
    setup(120);
    P.glen = 16;
    tr = ec_project_track(&P, 0);
    tr->v[EC_P_PATTERN_STEPS] = 16;
    tr->steps[0].trig = 1;
    ec_step_lock_set(&tr->steps[0], EC_P_MICRO, -6);
    ec_pattern_prepare(&P, 17);
    P.pat[17].tr[0].steps[0].trig = 1;
    ec_step_lock_set(&P.pat[17].tr[0].steps[0], EC_P_MICRO, -6);
    P.pat[17].tr[0].steps[0].slices = 0;
    ec_seq_start(&S);
    run(6000 * 8, 128);
    ec_seq_request_pattern(&S, 17);
    run(6000 * 8 + 1000, 128);
    n = trigs(0, at, 64);
    CHECK(n == 2 && at[1] == 16 * 6000, "the switch: the new first step on the boundary (%d, %llu)", n,
          (unsigned long long)(n > 1 ? at[1] : 0));
    CHECK(P.current == 17, "switched to 17 (bank B, pattern 2)");
    run(6000 * 16, 128);
    n = trigs(0, at, 64);
    CHECK(n == 3 && at[2] == 32 * 6000 - 6 * 250, "then early again at the loop's end (%llu)",
          (unsigned long long)(n > 2 ? at[2] : 0));
    for (i = 0; i < nrec; i++)
        if (R[i].ev.type == EC_EV_TRIG && R[i].ev.track != 0)
            break;
    CHECK(i == nrec, "no other track fired");
}

/* lock-only steps: the locks apply as the playhead passes (one step), no voice */
static void test_ghost_locks(void)
{
    uint64_t at[8];
    float base, locked = -1, after = -1;
    ec_ptrack_t *tr;
    setup(120);
    tr = ec_project_track(&P, 0);
    tr->v[EC_P_PATTERN_STEPS] = 16;
    tr->v[EC_P_FILTER_CUTOFF] = 100;
    ec_step_lock_set(&tr->steps[2], EC_P_FILTER_CUTOFF, 20);
    ec_step_lock_set(&tr->steps[2], EC_P_LOOP_START, 50);    /* (a region lock needs a trig: ignored) */
    ec_seq_start(&S);
    run(6000, 100);
    base = cut_now[0];
    run(6000 * 1 + 3000, 100);
    locked = cut_now[0];
    CHECK(ec_seq_value(&S, 0, EC_P_FILTER_CUTOFF) == 20 && ec_seq_value(&S, 0, EC_P_LOOP_START) != 50,
          "step 3 (lock only): cutoff 20 while it plays");
    run(6000, 100);
    after = cut_now[0];
    CHECK(locked < base && after == base, "the engine's cutoff: %g -> %g -> %g", base, locked, after);
    CHECK(trigs(0, at, 8) == 0, "no voice");
}

static void test_quantize(void)
{
    int step = -1, r;
    float off = 9;
    uint64_t at[8];
    ec_ptrack_t *tr;
    setup(120);
    tr = ec_project_track(&P, 0);
    tr->v[EC_P_PATTERN_STEPS] = 16;
    CHECK(ec_seq_rec_quantize(&S, 0, &step, &off) == -1, "stopped: -1");
    ec_seq_start(&S);
    run(6000 + 600, 600);          /* step 1 + 0.1 */
    r = ec_seq_rec_quantize(&S, 0, &step, &off);
    CHECK(r == 0 && step == 1 && fabsf(off - 0.1f) < 1e-4f && ec_micro_units(off) == 2,
          "late: step 2, +0.1 (%d %g)", step, off);
    run(3600, 600);                 /* 1.7 */
    r = ec_seq_rec_quantize(&S, 0, &step, &off);
    CHECK(r == 0 && step == 2 && fabsf(off + 0.3f) < 1e-4f && ec_micro_units(off) == -7,
          "early: step 3, -0.3 (%d %g)", step, off);
    /* recorded there: it does not fire again on this pass, it does on the next */
    tr->steps[2].trig = 1;
    ec_step_lock_set(&tr->steps[2], EC_P_MICRO, (float)ec_micro_units(off));
    ec_seq_rec_consume(&S, 0, step);
    run(6000 * 16 + 1200, 600);
    CHECK(trigs(0, at, 8) == 1 && at[0] == 18 * 6000 - 7 * 250, "consumed this pass, fires the next (%llu)",
          (unsigned long long)at[0]);
    /* the track's rate: at 1/2 x a step is 12000 samples; the page loop folds the index */
    setup(120);
    tr = ec_project_track(&P, 0);
    tr->v[EC_P_PATTERN_STEPS] = 4;
    tr->v[EC_P_RATE] = 7;         /* 1/2 */
    ec_seq_start(&S);
    run(12000 * 3 + 9000, 1000);  /* 3.75 steps: the nearest is step 4 = index 0 (wrapped) */
    r = ec_seq_rec_quantize(&S, 0, &step, &off);
    CHECK(r == 0 && step == 0 && fabsf(off + 0.25f) < 1e-4f, "rate 1/2, length 4: wraps to step 1, -0.25 (%d %g)",
          step, off);
}

static void test_beats(void)
{
    ec_seq_beat_t b[EC_SEQ_BEATS_MAX];
    uint64_t when[64], t0;
    int32_t num[64];
    int acc[64], ci[64], nb = 0, i, k, ok;
    uint64_t at[8];
    setup(120);                    /* a beat = 24000 samples */
    ec_project_track(&P, 0)->steps[0].trig = 1;
    ec_project_track(&P, 0)->v[EC_P_PATTERN_STEPS] = 16;
    run(1000, 1000);
    ec_seq_count_in(&S, 4);
    t0 = S.now;
    CHECK(ec_seq_counting_in(&S) == 4, "4 beats to count");
    for (i = 0; i < 320 && nb < 64; i++) {
        const uint64_t blk = S.now;
        ec_seq_render(&S, 0, 997);
        k = ec_seq_block_beats(&S, b, EC_SEQ_BEATS_MAX);
        for (int j = 0; j < k && nb < 64; j++) {
            when[nb] = blk + b[j].offset;
            num[nb] = b[j].beat;
            acc[nb] = b[j].accent;
            ci[nb] = b[j].countin;
            nb++;
        }
    }
    ok = nb >= 12;
    for (i = 0; ok && i < 12; i++)
        ok &= when[i] == t0 + (uint64_t)i * 24000u && num[i] == i - 4 && acc[i] == (i % 4 == 0) && ci[i] == (i < 4);
    CHECK(ok, "count-in -4..-1 then beats 0.. every 24000 samples, accents per bar (%d beats)", nb);
    CHECK(trigs(0, at, 8) >= 1 && at[0] == t0 + 96000, "the transport starts after the count-in (%llu)",
          (unsigned long long)at[0]);
    CHECK(fabs(ec_seq_song_beats(&S) - (double)(S.now - (t0 + 96000)) / 24000.0) < 1e-6, "song beats");
    /* start_at: the frame inside the next block */
    setup(120);
    ec_project_track(&P, 0)->steps[0].trig = 1;
    run(500, 500);
    ec_seq_start_at(&S, 123);
    ec_seq_render(&S, 0, 256);
    CHECK(trigs(0, at, 8) == 1 && at[0] == 623, "start_at 123: the first step at 623 (%llu)", (unsigned long long)at[0]);
    k = ec_seq_block_beats(&S, b, EC_SEQ_BEATS_MAX);
    CHECK(k == 1 && b[0].offset == 123 && b[0].beat == 0 && b[0].accent && !b[0].countin, "beat 0 at frame 123");
    /* the song's beats keep counting across a pattern restart; the clock's beat starts again */
    setup(120);
    P.glen = 12;                   /* 3 beats */
    P.quant = EC_Q_SEQUENTIAL;
    ec_seq_start(&S);
    run(24000, 1000);
    ec_seq_request_pattern(&S, 1);
    run(24000 * 3, 1000);
    CHECK(fabs(ec_seq_song_beats(&S) - 4.0) < 1e-6 && fabs(ec_seq_beats(&S) - 1.0) < 1e-6,
          "song 4 beats, the clock 1 after the switch at 3 (%g %g)", ec_seq_song_beats(&S), ec_seq_beats(&S));
    ec_seq_stop(&S);
    CHECK(ec_seq_song_beats(&S) == 0, "stopped: 0");
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "host/fixtures";
    test_sizes();
    test_full();
    test_v05(dir);
    test_old(dir);
    test_full_roundtrip();
    test_micro();
    test_micro_switch();
    test_ghost_locks();
    test_quantize();
    test_beats();
    printf("test_rec: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
