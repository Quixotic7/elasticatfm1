/* SPDX-License-Identifier: GPL-3.0-only */
/* test_seq: the sequencer and the project model natively, through a recording sink (no engine): sample-accurate
 * step timing at several tempos / rates / swing, tempo changes, p-lock windows and the resolver, region locks and
 * their release, trig conditions + FILL + chance, ratchets, pattern switches (sequential at the global boundary,
 * direct start, temp jump), the save / load round trip. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ec_project.h"
#include "ec_seq.h"

#define FS 44100.0
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

typedef struct {
    uint64_t at;
    ec_event_t ev;
} rec_t;

static ec_project_t P, Q;
static ec_seq_t S;
static rec_t R[MAXREC];
static int nrec;
static ec_track_params_t lastp[EC_NTRACK];
static uint64_t lastp_at[EC_NTRACK];
static uint64_t clock_now;

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
    lastp[t] = *p;
    lastp_at[t] = S.now;
}
static void on_process(void *ctx, float *out, int n)
{
    (void)ctx, (void)out;
    clock_now += (uint64_t)n;
}

static void setup(void)
{
    ec_seq_sink_t k = {0, on_event, on_params, on_process, 0, 0};
    ec_project_init(&P);
    ec_seq_init(&S, &P, &k, FS);
    nrec = 0;
    clock_now = 0;
}

/* run in 128-frame blocks for n samples */
static void run(uint64_t n)
{
    while (n > 0) {
        const int b = n > 128 ? 128 : (int)n;
        ec_seq_render(&S, 0, b);
        n -= (uint64_t)b;
    }
}

static void run_to(uint64_t t)
{
    if (t > S.now)
        run(t - S.now);
}

static int count(int type, int track) /* events of a type on a track (-1 any) */
{
    int i, c = 0;
    for (i = 0; i < nrec; i++)
        if (R[i].ev.type == type && (track < 0 || R[i].ev.track == track))
            c++;
    return c;
}
static const rec_t *nth(int type, int track, int k)
{
    int i;
    for (i = 0; i < nrec; i++)
        if (R[i].ev.type == type && (track < 0 || R[i].ev.track == track) && k-- == 0)
            return &R[i];
    return 0;
}

static uint64_t expect_at(double beats, double bpm) { return (uint64_t)ceil(beats * (60.0 * FS / (double)(float)bpm) - 1e-6); }

static void trig_all(int t, int steps)
{
    int i;
    for (i = 0; i < steps; i++)
        P.pat[P.current].tr[t].steps[i].trig = 1;
}

/* ------------------------------------------------------------------------------------------- timing --- */
static void test_timing(double bpm, int rate, double swing)
{
    int k, bad = 0;
    const double bps = 0.25 * EC_RATE_FRAC[rate - 1][1] / EC_RATE_FRAC[rate - 1][0];
    setup();
    P.bpm = (float)bpm;
    P.swing = (float)swing;
    ec_project_track(&P, 0)->v[EC_P_RATE] = (float)rate;
    ec_project_track(&P, 0)->v[EC_P_PATTERN_STEPS] = 16;
    trig_all(0, 16);
    run(1000);
    ec_seq_start(&S);
    {
        const uint64_t t0 = S.now;
        run((uint64_t)(FS * 60.0));                /* a minute */
        for (k = 0; k < count(EC_EV_TRIG, 0); k++) {
            const rec_t *r = nth(EC_EV_TRIG, 0, k);
            double b;
            const int64_t pair = k / 2, odd = k % 2;
            if (swing <= 50)
                b = k * bps;
            else
                b = pair * 2 * bps + (odd ? 2 * bps * swing / 100.0 : 0);
            if (r->at - t0 != expect_at(b, bpm)) {
                if (bad++ < 3)
                    printf("  step %d at %llu, expected %llu\n", k, (unsigned long long)(r->at - t0),
                           (unsigned long long)expect_at(b, bpm));
            }
        }
        CHECK(bad == 0, "timing bpm %.2f rate %d swing %.0f: %d steps off of %d", bpm, rate, swing, bad,
              count(EC_EV_TRIG, 0));
        CHECK(count(EC_EV_TRIG, 0) >= (int)(60.0 * bpm / 60.0 / bps) - 1, "timing bpm %.2f: %d trigs", bpm,
              count(EC_EV_TRIG, 0));
    }
}

static void test_tempo_change(void)
{
    uint64_t t0, tc;
    const rec_t *a, *b;
    int k0;
    setup();
    trig_all(0, 16);
    ec_seq_start(&S);
    t0 = S.now;
    run(30000);                      /* 120 BPM: steps every 5512.5 */
    tc = S.now;
    ec_seq_set_bpm(&S, 90.0f);
    k0 = count(EC_EV_TRIG, 0);
    run(60000);
    /* beats at the change, then the next boundary at the new tempo */
    {
        const double beats = (double)(tc - t0) / (60.0 * FS / 120.0);
        const double next = ceil(beats / 0.25 - 1e-9) * 0.25;
        a = nth(EC_EV_TRIG, 0, k0);
        b = nth(EC_EV_TRIG, 0, k0 + 1);
        CHECK(a && a->at == tc + (uint64_t)ceil((next - beats) * 60.0 * FS / 90.0 - 1e-6),
              "tempo change: first step after at %llu", a ? (unsigned long long)(a->at - tc) : 0ull);
        CHECK(a && b && (b->at - a->at == 7350), "tempo change: spacing %llu (want 7350)",
              a && b ? (unsigned long long)(b->at - a->at) : 0ull);
    }
}

/* ------------------------------------------------------------------------------------------- locks --- */
static void test_plocks(void)
{
    ec_ptrack_t *tr;
    const uint64_t step = 5512;   /* (120 BPM: 5512.5) */
    uint64_t t0;
    const rec_t *r;
    setup();
    tr = ec_project_track(&P, 0);
    tr->steps[0].trig = 1;
    ec_step_lock_set(&tr->steps[0], EC_P_FILTER_CUTOFF, 40);
    ec_step_lock_set(&tr->steps[0], EC_P_LENGTH, 2);       /* two steps long */
    tr->steps[4].trig = 1;
    ec_step_lock_set(&tr->steps[4], EC_P_LOOP_START, 32);
    ec_step_lock_set(&tr->steps[4], EC_P_LOOP_END, 64);
    ec_step_lock_set(&tr->steps[4], EC_P_TRIG_RELEASE, 3);
    tr->steps[8].trig = 1;
    ec_step_lock_set(&tr->steps[8], EC_P_PITCH, 7);
    ec_step_lock_set(&tr->steps[8], EC_P_LENGTH, 8);
    tr->steps[9].trig = 1;                                  /* mono: ends step 8's window early */
    tr->steps[12].trig = 1;
    ec_step_lock_set(&tr->steps[12], EC_P_AMP, 20);
    ec_step_lock_set(&tr->steps[12], EC_P_LENGTH, 4);
    ec_step_toggle_ghost(tr, 13);                           /* a ghost: the window carries */
    run(256);
    ec_seq_start(&S);
    t0 = S.now;
    run(64);
    CHECK(fabsf(lastp[0].filter_cutoff - ec_xf_cutoff_hz(40)) < 0.01f, "step 1 lock: cutoff %.1f", lastp[0].filter_cutoff);
    CHECK(tr->v[EC_P_FILTER_CUTOFF] == 127, "the track value is never written");
    CHECK(fabsf(ec_seq_value(&S, 0, EC_P_FILTER_CUTOFF) - 40) < 0.01f, "resolved value");
    run(2 * step + 100);
    CHECK(fabsf(lastp[0].filter_cutoff - ec_xf_cutoff_hz(127)) < 0.01f, "lock window over: cutoff %.1f",
          lastp[0].filter_cutoff);
    CHECK(lastp_at[0] - t0 == 11025, "the window ends on its sample (%llu, want 11025)",
          (unsigned long long)(lastp_at[0] - t0));
    run(2 * step);
    r = nth(EC_EV_TRIG, 0, 1);
    CHECK(r && r->ev.start == 32 && r->ev.end == 64 && (r->ev.flags & EC_TF_JUMP) && r->ev.mode == 3,
          "region lock: TRIG start %.0f end %.0f flags %d mode %d", r ? r->ev.start : -9, r ? r->ev.end : -9,
          r ? r->ev.flags : -9, r ? r->ev.mode : -9);
    CHECK(r && r->at - t0 == 22050, "region step at %llu", r ? (unsigned long long)(r->at - t0) : 0ull);
    run_to(t0 + 30000);
    r = nth(EC_EV_REGION_RELEASE, 0, 0);
    CHECK(r && r->at - t0 == 22050 + 5513, "region release at the window's end (%llu)",
          r ? (unsigned long long)(r->at - t0) : 0ull);
    run_to(t0 + 50000);                                     /* past step 10 (49613) */
    CHECK(fabsf(lastp[0].pitch) < 0.01f, "mono: step 10 cleared step 9's pitch window (pitch %.1f)", lastp[0].pitch);
    run_to(t0 + 75000);                                     /* step 13 (amp lock) + 14 (ghost) */
    CHECK(fabsf(lastp[0].amp - 20.0f / 127.0f) < 0.001f, "ghost carries the window: amp %.3f", lastp[0].amp);
    {
        const rec_t *g = nth(EC_EV_TRIG, 0, 5);
        CHECK(g && (g->ev.flags & EC_TF_GHOST), "ghost TRIG flagged");
    }
}

/* ------------------------------------------------------------------------------------------- conditions --- */
static void test_conditions(void)
{
    ec_ptrack_t *tr;
    int passes[8], i;
    setup();
    P.glen = 4;
    for (i = 0; i < 6; i++) {
        tr = ec_project_track(&P, i);
        tr->v[EC_P_PATTERN_STEPS] = 4;
        tr->steps[0].trig = 1;
    }
    ec_step_lock_set(&ec_project_track(&P, 0)->steps[0], EC_P_TRIG_CONDITION, 2);   /* 1:2 */
    ec_step_lock_set(&ec_project_track(&P, 1)->steps[0], EC_P_TRIG_CONDITION, 3);   /* 2:2 */
    ec_step_lock_set(&ec_project_track(&P, 2)->steps[0], EC_P_TRIG_CONDITION, 17);  /* 1ST */
    ec_step_lock_set(&ec_project_track(&P, 3)->steps[0], EC_P_TRIG_CONDITION, 11);  /* FILL */
    ec_step_lock_set(&ec_project_track(&P, 4)->steps[0], EC_P_TRIG_CHANCE, 0);      /* never */
    ec_step_lock_set(&ec_project_track(&P, 5)->steps[0], EC_P_TRIG_CHANCE, 50);     /* about half */
    ec_seq_start(&S);
    run(22050 * 8 - 100);                                   /* passes 1..8 (one beat each) */
    for (i = 0; i < 8; i++)
        passes[i] = 0;
    for (i = 0; i < nrec; i++)
        if (R[i].ev.type == EC_EV_TRIG && R[i].ev.track == 0)
            passes[(int)(R[i].at / 22050)]++;
    CHECK(count(EC_EV_TRIG, 0) == 4 && passes[0] && !passes[1] && passes[2], "1:2 fires on odd passes (%d)",
          count(EC_EV_TRIG, 0));
    CHECK(count(EC_EV_TRIG, 1) == 4, "2:2: %d", count(EC_EV_TRIG, 1));
    CHECK(count(EC_EV_TRIG, 2) == 1, "1ST: %d", count(EC_EV_TRIG, 2));
    CHECK(count(EC_EV_TRIG, 3) == 0, "FILL off: %d", count(EC_EV_TRIG, 3));
    CHECK(count(EC_EV_TRIG, 4) == 0, "chance 0: %d", count(EC_EV_TRIG, 4));
    CHECK(count(EC_EV_TRIG, 5) >= 1 && count(EC_EV_TRIG, 5) <= 7, "chance 50: %d of 8", count(EC_EV_TRIG, 5));
    ec_seq_set_fill(&S, 1, 0);
    run(22050);
    CHECK(count(EC_EV_TRIG, 3) == 1, "FILL held: %d", count(EC_EV_TRIG, 3));
    ec_seq_set_fill(&S, 0, 0);
    /* PRE: step 2 follows step 1's outcome (1:2) */
    setup();
    tr = ec_project_track(&P, 0);
    tr->v[EC_P_PATTERN_STEPS] = 2;
    tr->steps[0].trig = tr->steps[1].trig = 1;
    ec_step_lock_set(&tr->steps[0], EC_P_TRIG_CONDITION, 2);
    ec_step_lock_set(&tr->steps[1], EC_P_TRIG_CONDITION, 14);   /* !PRE */
    ec_seq_start(&S);
    run(11025 * 4 - 100);
    {
        int k, pattern = 0;
        for (k = 0; k < count(EC_EV_TRIG, 0); k++)
            pattern = pattern * 10 + (int)(nth(EC_EV_TRIG, 0, k)->at / 5512.5 + 0.5);
        CHECK(pattern == 347 && count(EC_EV_TRIG, 0) == 4, "!PRE: trig steps %d (want 0 3 4 7)", pattern);
    }
}

static void test_ratchet(void)
{
    ec_ptrack_t *tr;
    int k;
    setup();
    tr = ec_project_track(&P, 0);
    tr->steps[0].trig = 1;
    ec_step_lock_set(&tr->steps[0], EC_P_TRIG_RATCHET, 4);
    ec_seq_start(&S);
    run(5000);
    CHECK(count(EC_EV_NOTE_ON, 0) == 3, "ratchet: %d extra notes", count(EC_EV_NOTE_ON, 0));
    for (k = 0; k < 3; k++) {
        const rec_t *r = nth(EC_EV_NOTE_ON, 0, k);
        CHECK(r && r->at == (uint64_t)llround((k + 1) * 5512.5 / 4), "ratchet hit %d at %llu", k + 1,
              r ? (unsigned long long)r->at : 0ull);
    }
}

/* ------------------------------------------------------------------------------------------- patterns --- */
static void test_patterns(void)
{
    const rec_t *r;
    int i;
    setup();
    trig_all(0, 16);
    ec_project_track(&P, 0)->v[EC_P_FILTER_RES] = 33;
    ec_seq_start(&S);
    run(30000);
    ec_seq_request_pattern(&S, 3);                         /* sequential: at the global boundary (beat 4) */
    CHECK(P.current == 0 && S.pending == 3, "queued");
    run(60000);
    CHECK(P.current == 3, "switched (%d)", P.current);
    r = 0;
    for (i = 0; i < nrec; i++)
        if (R[i].ev.type == EC_EV_TRANSPORT && R[i].at > 0)
            r = &R[i];
    CHECK(r && r->at == 88200, "switch at the bar line: %llu (want 88200)", r ? (unsigned long long)r->at : 0ull);
    CHECK(P.pat[3].used && P.pat[3].tr[0].v[EC_P_FILTER_RES] == 33 && !P.pat[3].tr[0].steps[0].trig,
          "a blank pattern copies the live params with empty steps");
    /* direct start: immediate */
    P.quant = EC_Q_DIRECT_START;
    ec_seq_request_pattern(&S, 0);
    CHECK(P.current == 0, "direct start");
    /* temp jump: returns after one global pass */
    P.quant = EC_Q_TEMP_JUMP;
    run(1000);
    ec_seq_request_pattern(&S, 5);
    CHECK(P.current == 5, "temp jump goes");
    run(88200 + 10);
    CHECK(P.current == 0, "temp jump returns (%d)", P.current);
    /* stopped: at once */
    ec_seq_stop(&S);
    P.quant = EC_Q_SEQUENTIAL;
    ec_seq_request_pattern(&S, 7);
    CHECK(P.current == 7, "stopped switch");
}

/* ------------------------------------------------------------------------------------------- save / load --- */
static uint8_t buf[1 << 20];

static void test_save_load(void)
{
    int32_t n;
    int i, t;
    ec_project_init(&P);
    P.bpm = 136;
    P.swing = 58;
    P.glen = 32;
    P.quant = EC_Q_DIRECT_JUMP;
    P.rtn = 3;
    snprintf(P.slot[0].path, EC_PATH, "/audio/breaks/amen_bpm136.wav");
    P.slot[0].steps = 32;
    P.slot[0].trim_end = 0.75f;
    snprintf(P.slot[5].path, EC_PATH, "/audio/pad.aif");
    ec_pattern_prepare(&P, 4);
    for (i = 0; i < 2; i++) {
        P.current = i ? 4 : 0;
        for (t = 0; t < EC_NTRACK; t++) {
            ec_ptrack_t *tr = ec_project_track(&P, t);
            tr->v[EC_P_MACHINE] = (float)(1 + (t % 3));
            tr->v[EC_P_FILTER_CUTOFF] = (float)(20 + t * 7 + i);
            tr->steps[t].trig = 1;
            tr->steps[t + 16].slices = 0x80000001u;
            ec_step_lock_set(&tr->steps[t], EC_P_PITCH, -5.5f);
            ec_step_lock_set(&tr->steps[t], EC_P_LOOP_START, 12.0f);
            ec_step_toggle_ghost(tr, 200 + t);
        }
    }
    P.current = 4;
    n = ec_project_save(&P, buf, sizeof buf);
    CHECK(n > 0, "saved (%d bytes)", (int)n);
    memset(&Q, 0x5A, sizeof Q);
    CHECK(ec_project_load(&Q, buf, (uint32_t)n) == 0, "loaded");
    CHECK(Q.bpm == 136 && Q.swing == 58 && Q.glen == 32 && Q.quant == EC_Q_DIRECT_JUMP && Q.rtn == 3 &&
              Q.current == 4,
          "globals");
    CHECK(!memcmp(Q.slot, P.slot, sizeof P.slot), "slots");
    for (i = 0; i < EC_NPAT; i++) {
        CHECK(Q.pat[i].used == P.pat[i].used, "pattern %d used", i);
        if (P.pat[i].used)
            CHECK(!memcmp(&Q.pat[i], &P.pat[i], sizeof P.pat[i]), "pattern %d content", i);
    }
    buf[n / 2] ^= 0x40;
    CHECK(ec_project_load(&Q, buf, (uint32_t)n) == -1, "a corrupt record is refused");
    CHECK(ec_project_save(&P, buf, 64) == -1, "too small a record");
    printf("  project record: %d bytes\n", (int)n);
}

int main(void)
{
    static const double bpms[] = {120.0, 100.0, 137.3, 174.0, 60.5};
    int i;
    for (i = 0; i < 5; i++)
        test_timing(bpms[i], EC_RATE_UNITY, 50);
    test_timing(120.0, 9, 50);          /* 2/3: triplets stay exact */
    test_timing(97.0, 21, 50);          /* 2x */
    test_timing(120.0, 7, 50);          /* 1/2 */
    test_timing(120.0, EC_RATE_UNITY, 60);   /* swing */
    test_timing(133.0, 9, 66);
    test_tempo_change();
    test_plocks();
    test_conditions();
    test_ratchet();
    test_patterns();
    test_save_load();
    printf("test_seq: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
