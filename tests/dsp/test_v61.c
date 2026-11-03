/*
 * test_v61.c -- 0.6.1: the two inserts (INS 1 then INS 2 in series; an empty slot is skipped: bit-exact, no state
 * touched), the K4 MIX of the machines without a blend (fx wet), and the counted load (ec_load.c).
 * Exit code 0 = all passed.
 */
#include "ec_testkit.h"
#include "../../src/dsp/ec_internal.h"
#include "../../src/dsp/ec_load.h"

static int g_fail = 0, g_pass = 0;
#define CHECK(cond, ...) do { if (cond) { g_pass++; } else { g_fail++; \
    printf("  FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

#define SEC TK_FS
#define BLK 128
#define NBLK 400

static int16_t *g_loop;
static uint32_t g_frames;

/* track 0 plays the drum loop (TAPE), its inserts fx / fx2 (machines, DJ EQ params), transport running */
static ec_dsp_t *engine(int fx, int fx2, float wet)
{
    ec_dsp_t *d = tk_engine();
    ec_sample_t s = {g_loop, g_frames, (float)TK_FS, 2, 0};
    ec_dsp_set_sample(d, 0, &s);
    ec_track_params_t p = *ec_dsp_get_track_params(d, 0);
    p.amp = 0.2f;
    p.fx.machine = fx;
    p.fx.wet = wet;
    p.fx.eq_low = -1.f;                 /* (DJ EQ in INS 1: the lows killed) */
    p.fx2.machine = fx2;
    p.fx2.eq_mid = p.fx2.eq_high = -1.f;   /* (DJ EQ in INS 2: the mids and highs killed) */
    ec_dsp_track_params(d, 0, &p);
    ec_dsp_transport(d, 0, 1, 1);
    ec_dsp_note_on(d, 0, 0, 0);
    return d;
}

static float *render(ec_dsp_t *d, int nblk)
{
    float *o = calloc((size_t)nblk * BLK * 2, sizeof(float));
    for (int b = 0; b < nblk; ++b) ec_dsp_process(d, o + (size_t)b * BLK * 2, BLK);
    return o;
}

static int same(const float *a, const float *b, size_t n) { return memcmp(a, b, n * sizeof(float)) == 0; }

static double rms(const float *a, size_t n)
{
    double s = 0;
    for (size_t i = 0; i < n; ++i) s += (double)a[i] * a[i];
    return sqrt(s / (double)(n ? n : 1));
}

static void test_series(void)
{
    printf("two inserts in series\n");
    const size_t n = (size_t)NBLK * BLK * 2;
    /* an empty INS 2 is skipped: its state is never touched */
    ec_dsp_t *d0 = engine(EC_FX_LOFI, EC_FX_NONE, 1.f);
    ec_fx_t keep = d0->tr[0].fx2;
    float *o0 = render(d0, NBLK);
    CHECK(memcmp(&keep, &d0->tr[0].fx2, sizeof keep) == 0, "an empty INS 2: its state untouched after %d blocks", NBLK);
    CHECK(ec_fx_bypassed(&d0->tr[0].fx2, &d0->tr[0].p.fx2), "an empty INS 2 is bypassed");
    CHECK(rms(o0, n) > 0.01, "the loop sounds (rms %.4f)", rms(o0, n));

    /* the same machine in either slot, the other empty: bit-exact (an empty slot passes the signal untouched) */
    ec_dsp_t *d1 = engine(EC_FX_NONE, EC_FX_DJ_EQ, 1.f);
    ec_dsp_t *d2 = engine(EC_FX_DJ_EQ, EC_FX_NONE, 1.f);
    {
        ec_track_params_t p = d2->tr[0].p;   /* (INS 2's EQ settings in INS 1) */
        p.fx.eq_low = 0.f;
        p.fx.eq_mid = p.fx.eq_high = -1.f;
        ec_dsp_track_params(d2, 0, &p);
    }
    float *o1 = render(d1, NBLK), *o2 = render(d2, NBLK);
    CHECK(same(o1, o2, n), "DJ EQ in INS 2 alone = DJ EQ in INS 1 alone, bit-exact");
    ec_dsp_t *dn = engine(EC_FX_NONE, EC_FX_NONE, 1.f);
    float *on = render(dn, NBLK);
    CHECK(!same(on, o1, n), "the EQ changes the sound");

    /* series: INS 1 (lows killed) then INS 2 (highs killed) = INS 2's EQ applied to INS 1's output. DJ EQ is linear,
     * the track gain and the master are linear under the soft limit: B(out_A) == out_AB within float rounding */
    ec_dsp_t *dab = engine(EC_FX_DJ_EQ, EC_FX_DJ_EQ, 1.f), *da = engine(EC_FX_DJ_EQ, EC_FX_NONE, 1.f);
    float *oab = render(dab, NBLK), *oa = render(da, NBLK);
    {
        void *mem = NULL;
        ec_fx_t *b = calloc(1, sizeof *b);
        if (posix_memalign(&mem, 16, EC_FX_MEM_FLOATS * sizeof(float)) != 0) return;
        ec_fx_init(b, (float *)mem, (float)TK_FS, 0, 0x5A5Au);
        float L[EC_KR], R[EC_KR], err = 0.f, pk = 0.f;
        size_t f = 0;
        const size_t nf = (size_t)NBLK * BLK;
        for (; f + EC_KR <= nf; f += EC_KR) {      /* the engine's own chunks: EC_KR frames */
            for (int i = 0; i < EC_KR; ++i) { L[i] = oa[2 * (f + i)]; R[i] = oa[2 * (f + i) + 1]; }
            ec_fx_process(b, &dab->tr[0].p.fx2, 120.f, NULL, NULL, L, R, EC_KR);
            for (int i = 0; i < EC_KR; ++i) {
                const float e = fabsf(L[i] - oab[2 * (f + i)]) + fabsf(R[i] - oab[2 * (f + i) + 1]);
                if (e > err) err = e;
                if (fabsf(oab[2 * (f + i)]) > pk) pk = fabsf(oab[2 * (f + i)]);
            }
        }
        CHECK(pk < 0.8f, "under the soft limit (peak %.3f)", pk);
        CHECK(err < 1e-4f, "INS 1 -> INS 2 = INS 2 over INS 1's output (max error %g)", err);
        CHECK(rms(oab, n) < rms(oa, n), "the second insert acts (rms %.4f < %.4f)", rms(oab, n), rms(oa, n));
        free(mem);
        free(b);
    }

    /* INS 2 swapped in and back out: bypassed again, and bit-exact with an engine that never had it */
    ec_dsp_t *ds = engine(EC_FX_LOFI, EC_FX_NONE, 1.f), *dr = engine(EC_FX_LOFI, EC_FX_NONE, 1.f);
    {
        ec_track_params_t p = ds->tr[0].p;
        float *x;
        p.fx2.machine = EC_FX_LOFI;
        p.fx2.lofi_bits = 4.f;
        ec_dsp_track_params(ds, 0, &p);
        free(render(ds, 100));
        CHECK(ec_fx_running(&ds->tr[0].fx2) == EC_FX_LOFI, "INS 2 runs LOFI");
        p.fx2.machine = EC_FX_NONE;
        ec_dsp_track_params(ds, 0, &p);
        free(render(ds, 100));
        CHECK(ec_fx_bypassed(&ds->tr[0].fx2, &ds->tr[0].p.fx2), "back to NONE: bypassed after the fade");
        free(render(dr, 200));
        ec_dsp_track_params(dr, 0, &ds->tr[0].p);  /* (the same params from here on) */
        x = render(ds, 100);
        float *y = render(dr, 100);
        CHECK(same(x, y, (size_t)100 * BLK * 2), "after the swap back: bit-exact with no INS 2 at all");
        free(x);
        free(y);
    }
    free(o0); free(o1); free(o2); free(on); free(oab); free(oa);
}

static void test_wet(void)
{
    printf("K4 MIX on the machines without a blend\n");
    const size_t n = (size_t)NBLK * BLK * 2;
    ec_dsp_t *d0 = engine(EC_FX_NONE, EC_FX_NONE, 1.f), *dw0 = engine(EC_FX_DJ_EQ, EC_FX_NONE, 0.f);
    ec_dsp_t *dw1 = engine(EC_FX_DJ_EQ, EC_FX_NONE, 1.f), *dh = engine(EC_FX_DJ_EQ, EC_FX_NONE, 0.5f);
    float *o0 = render(d0, NBLK), *ow0 = render(dw0, NBLK), *ow1 = render(dw1, NBLK), *oh = render(dh, NBLK);
    CHECK(same(o0 + n / 2, ow0 + n / 2, n / 2), "DJ EQ at MIX 0 = dry, bit-exact (past the engine's fade-in)");
    CHECK(!same(ow1, o0, n) && !same(oh, ow1, n) && !same(oh, o0, n), "MIX 0.5 sits between dry and wet");
    CHECK(!ec_fx_has_mix(EC_FX_DJ_EQ) && !ec_fx_has_mix(EC_FX_LIMIT) && ec_fx_has_mix(EC_FX_DELAY),
          "which machines blend by mix");
    free(o0); free(ow0); free(ow1); free(oh);
}

/* 0.6.3 (docs/UX-V2.md §10 SENDS): the send mode moves the S1 / S2 tap: PRE after the filter (and the track's level
 * and pan, applied at the filter's output), before the inserts; POST after both inserts. With an
 * insert that kills every band, POST sends (almost) nothing, PRE the whole loop. (The track's mute acts after both
 * taps: muted, the output is S1's return alone.) */
static void test_send_tap(void)
{
    printf("the send mode PRE / POST\n");
    const size_t n = (size_t)NBLK * BLK * 2;
    double r[2];
    for (int post = 0; post < 2; ++post) {
        ec_dsp_t *d = engine(EC_FX_DJ_EQ, EC_FX_NONE, 1.f);
        ec_track_params_t p = d->tr[0].p;
        p.fx.eq_low = p.fx.eq_mid = p.fx.eq_high = -1.f;   /* INS 1: every band killed */
        p.send1_level = 1.f;
        p.send_tap = post ? 2 : 1;
        p.mute = 1;                                        /* (the track's own mix silent: S1's return alone) */
        ec_dsp_track_params(d, 0, &p);
        {   /* S1: a flat DJ EQ, fully wet (its return = what the tap sends) */
            ec_fx_params_t f = d->busp[0];
            f.machine = EC_FX_DJ_EQ;
            f.eq_low = f.eq_mid = f.eq_high = 0.f;
            f.wet = 1.f;
            ec_dsp_bus_params(d, 0, &f);
        }
        float *o = render(d, NBLK);
        r[post] = rms(o + n / 2, n / 2);
        free(o);
    }
    CHECK(r[0] > 0.005, "PRE: S1 gets the loop before the inserts (rms %.4f)", r[0]);
    CHECK(r[1] < r[0] * 0.25, "POST: S1 gets the inserts' output (all bands killed: rms %.4f vs PRE %.4f)", r[1], r[0]);
}

static void test_load(void)
{
    printf("the counted load\n");
    ec_load_counts_t c;
    ec_dsp_t *a = engine(EC_FX_REVERB, EC_FX_NONE, 1.f), *b = engine(EC_FX_REVERB, EC_FX_BLACKHOLE, 1.f);
    ec_dsp_t *a2 = engine(EC_FX_REVERB, EC_FX_NONE, 1.f), *z = tk_engine();
    free(render(a, 200)); free(render(b, 200)); free(render(a2, 200)); free(render(z, 200));
    ec_dsp_load_counts(a, &c);
    CHECK(c.mode[EC_WARP_TAPE] == 1 && c.fx[EC_FX_REVERB] == 1 && c.fx[EC_FX_BLACKHOLE] == 0,
          "counts: a TAPE track, REVERB (tape %d reverb %d bh %d)", c.mode[EC_WARP_TAPE], c.fx[EC_FX_REVERB],
          c.fx[EC_FX_BLACKHOLE]);
    ec_dsp_load_counts(b, &c);
    CHECK(c.fx[EC_FX_REVERB] == 1 && c.fx[EC_FX_BLACKHOLE] == 1, "INS 2's BLACKHOLE counted");
    CHECK(ec_dsp_load(z) > 0.f && ec_dsp_load(z) < ec_dsp_load(a) && ec_dsp_load(a) < ec_dsp_load(b),
          "the load rises: idle %.3f < one insert %.3f < two %.3f", ec_dsp_load(z), ec_dsp_load(a), ec_dsp_load(b));
    CHECK(ec_dsp_load(a) == ec_dsp_load(a2), "deterministic: the same audio, the same reading (%.6f %.6f)",
          ec_dsp_load(a), ec_dsp_load(a2));
}

int main(void)
{
    (void)tk_ev;
    g_loop = tk_drum_loop(120.f, &g_frames);
    test_series();
    test_wet();
    test_send_tap();
    test_load();
    printf("test_v61: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
