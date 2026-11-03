/* SPDX-License-Identifier: GPL-3.0-only */
/* test_seq_e4: E4 on the model side, through a recording sink: a firing step emits the mod retrigger (lfo_reset /
 * env_reset, ghosts none), a p-locked LFO depth / FX param reaches the engine params for the note's length and
 * reverts, the bus records survive save / load, and a record without the bus chunk loads with the defaults. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ec_project.h"
#include "ec_seq.h"

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
static int nmod, modflags[64];
static uint64_t modat[64];
static ec_track_params_t lastp;

static void on_event(void *ctx, const ec_event_t *e)
{
    (void)ctx;
    if (e->type == EC_EV_MOD_TRIG && e->track == 0 && nmod < 64) {
        modflags[nmod] = e->flags;
        modat[nmod++] = S.now;
    }
}
static void on_params(void *ctx, int t, const ec_track_params_t *p)
{
    (void)ctx;
    if (t == 0)
        lastp = *p;
}
static void on_process(void *ctx, float *out, int n) { (void)ctx, (void)out, (void)n; }

static void run(uint64_t n)
{
    while (n > 0) {
        const int b = n > 128 ? 128 : (int)n;
        ec_seq_render(&S, 0, b);
        n -= (uint64_t)b;
    }
}

static uint8_t buf[1 << 20];

int main(void)
{
    ec_seq_sink_t k = {0, on_event, on_params, on_process, 0};
    ec_ptrack_t *tr;
    const double step = FS * 60.0 / 120.0 / 4.0;
    ec_project_init(&P);
    ec_seq_init(&S, &P, &k, FS);
    tr = ec_project_track(&P, 0);
    /* steps 1 (plain), 5 (lfo_reset off), 9 (a ghost: no mod trig), 13 (LFO depth + delay feedback locked) */
    ec_step_toggle(tr, 0);
    ec_step_toggle(tr, 4);
    ec_step_lock_set(&tr->steps[4], EC_P_LFO_RESET, 0);
    ec_step_toggle(tr, 8);
    ec_step_toggle_ghost(tr, 8);
    ec_step_toggle(tr, 12);
    ec_step_lock_set(&tr->steps[12], EC_P_LFO1_DEPTH, 128);
    ec_step_lock_set(&tr->steps[12], EC_P_DELAY_FEEDBACK, 127);
    tr->v[EC_P_FX_MACHINE] = EC_FX_DELAY;
    ec_seq_touch(&S, 0);
    ec_seq_start(&S);
    run((uint64_t)(step * 12.5));
    CHECK(nmod == 3, "mod trigs on steps 1, 5, 13 (not the ghost): %d", nmod);
    CHECK(nmod >= 1 && modflags[0] == (EC_TF_LFO | EC_TF_MENV) && modat[0] == 0, "step 1: lfo + env at 0");
    CHECK(nmod >= 2 && modflags[1] == EC_TF_MENV && llabs((long long)modat[1] - (long long)llround(4 * step)) <= 1,
          "step 5: env only (lfo_reset locked off) at its boundary");
    CHECK(lastp.mod.lfo[0].depth == 1.0f && lastp.fx.machine == EC_FX_DELAY && lastp.fx.delay_feedback == 1.0f,
          "the step 13 locks reach the engine: depth %.3f fb %.3f", lastp.mod.lfo[0].depth, lastp.fx.delay_feedback);
    run((uint64_t)(step * 2));
    CHECK(lastp.mod.lfo[0].depth == 0.0f && fabsf(lastp.fx.delay_feedback - 38.0f / 127.0f) < 1e-6f,
          "after the note the base values return: depth %.3f fb %.3f", lastp.mod.lfo[0].depth,
          lastp.fx.delay_feedback);

    /* the bus records: save / load */
    P.bus[0][EC_P_FX_MACHINE] = EC_FX_REVERB;
    P.bus[0][EC_P_REVERB_SIZE] = 100;
    P.bus[2][EC_P_FX_MACHINE] = EC_FX_LIMIT;
    P.bus[2][EC_P_LIMIT_GAIN] = 42;
    {
        const int32_t n = ec_project_save(&P, buf, sizeof buf);
        CHECK(n > 0, "saved");
        CHECK(ec_project_load(&Q, buf, (uint32_t)n) == 0, "loaded");
        CHECK(Q.bus[0][EC_P_FX_MACHINE] == EC_FX_REVERB && Q.bus[0][EC_P_REVERB_SIZE] == 100 &&
                  Q.bus[2][EC_P_FX_MACHINE] == EC_FX_LIMIT && Q.bus[2][EC_P_LIMIT_GAIN] == 42 &&
                  Q.bus[1][EC_P_FX_MACHINE] == EC_FX_NONE && Q.bus[1][EC_P_LIMIT_CEIL] == 121,
              "bus records round trip");
        CHECK(ec_project_track(&Q, 0)->v[EC_P_FX_MACHINE] == EC_FX_DELAY, "the track's insert round trips");
    }
    /* a 0.1.0-style record: the patterns only (the chunk cut off, the header fixed up) -> bus defaults */
    {
        int32_t n = ec_project_save(&P, buf, sizeof buf);
        uint32_t len, i, h = 2166136261u, cut = 0;
        for (i = 16; i + 4 <= (uint32_t)n; i++)
            if (buf[i] == 'E' && buf[i + 1] == 'C' && buf[i + 2] == '4' && buf[i + 3] == 'B')
                cut = i;
        CHECK(cut > 0, "chunk found");
        len = cut - 16;
        buf[8] = (uint8_t)len, buf[9] = (uint8_t)(len >> 8), buf[10] = (uint8_t)(len >> 16), buf[11] = (uint8_t)(len >> 24);
        for (i = 16; i < cut; i++)
            h = (h ^ buf[i]) * 16777619u;
        buf[12] = (uint8_t)h, buf[13] = (uint8_t)(h >> 8), buf[14] = (uint8_t)(h >> 16), buf[15] = (uint8_t)(h >> 24);
        CHECK(ec_project_load(&Q, buf, cut) == 0, "an old record loads");
        CHECK(Q.bus[0][EC_P_FX_MACHINE] == EC_FX_NONE, "with the bus defaults");
    }
    printf("test_seq_e4: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
