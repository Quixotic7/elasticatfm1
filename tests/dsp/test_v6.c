/*
 * test_v6.c -- 0.6.0: the live grain snapshot (ec_dsp_grains) and the metronome click (ec_click).
 * Exit code 0 = all passed.
 */
#include "ec_testkit.h"
#include "../../src/dsp/ec_internal.h"
#include "../../src/dsp/ec_click.h"

static int g_fail = 0, g_pass = 0;
#define CHECK(cond, ...) do { if (cond) { g_pass++; } else { g_fail++; \
    printf("  FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

#define SEC TK_FS
#define BLK 128

static float g_out[2 * BLK];
static void blocks(ec_dsp_t *d, int nblk)
{
    for (int b = 0; b < nblk; ++b) ec_dsp_process(d, g_out, BLK);
}

static int count_newest(const ec_grain_view_t *g, int n)
{
    int k = 0;
    for (int i = 0; i < n; ++i) k += g[i].newest ? 1 : 0;
    return k;
}

/* track 0: a 1 s sine loop in `mode`, forward grains, no randomness, transport running */
static ec_dsp_t *grain_engine(int mode, float density, float size, int16_t **keep)
{
    ec_dsp_t *d = tk_engine();
    int16_t *sn = tk_sine(220.f, 0.5f, SEC);
    ec_sample_t s = {sn, SEC, (float)TK_FS, 1, 0};
    ec_dsp_set_sample(d, 0, &s);
    ec_track_params_t p = *ec_dsp_get_track_params(d, 0);
    p.mode = mode;
    p.grain_density = density;
    p.grain_size = size;
    p.grain_jitter = 0.f;
    p.grain_speed_rand = 0.f;
    p.grain_direction = 1.f;
    ec_dsp_track_params(d, 0, &p);
    ec_dsp_transport(d, 0, 1, 1);
    ec_dsp_note_on(d, 0, 0, 0);
    *keep = sn;
    return d;
}

static void test_grains(void)
{
    printf("grain snapshot\n");
    ec_grain_view_t g[EC_GRAIN_VIEW_MAX], h[EC_GRAIN_VIEW_MAX];
    int16_t *sn;

    /* a sparse cloud: every grain is reported, exactly one newest, sane fields */
    ec_dsp_t *d = grain_engine(EC_WARP_GRANULAR, 4.f, 0.2f, &sn);
    blocks(d, 200);
    int n = ec_dsp_grains(d, 0, g);
    const ec_track_t *t = &d->tr[0];
    CHECK(n > 0 && n == t->gr.n, "sparse cloud: n %d (engine %d)", n, t->gr.n);
    CHECK(count_newest(g, n) == 1, "exactly one newest (%d)", count_newest(g, n));
    for (int i = 0; i < n; ++i) {
        CHECK(g[i].pos >= 0.f && g[i].pos < 1.f, "pos %f in 0..1", g[i].pos);
        CHECK(g[i].level >= 0.f && g[i].level <= 1.f, "level %f in 0..1", g[i].level);
        /* 0.2 s at rate 1 over a 1 s sample (grain_size is lagged from its 0.1 default: allow the lag) */
        CHECK(g[i].len > 0.09f && g[i].len <= 0.2001f, "len %f ~ 0.2", g[i].len);
    }
    /* the newest = the engine's youngest grain (smallest phase / inc) */
    int yi = 0;
    for (int k = 1; k < t->gr.n; ++k)
        if (t->gr.g[k].phase / t->gr.g[k].inc < t->gr.g[yi].phase / t->gr.g[yi].inc) yi = k;
    float ypos = (float)(t->gr.g[yi].pos / (double)SEC);
    int ok = 0;
    for (int i = 0; i < n; ++i) if (g[i].newest && fabsf(g[i].pos - ypos) < 1e-6f) ok = 1;
    CHECK(ok, "the newest flag sits on the youngest grain");
    CHECK(ec_dsp_grains(d, 1, g) == 0, "a track without a sample: 0");
    CHECK(ec_dsp_grains(d, -1, g) == 0 && ec_dsp_grains(d, EC_TRACKS, g) == 0, "bad track: 0");

    /* positions move at the read rate: 1 frame per sample = BLK / SEC per block */
    n = ec_dsp_grains(d, 0, g);
    blocks(d, 1);
    int m = ec_dsp_grains(d, 0, h), matched = 0, alive = 0;
    float step = (float)BLK / (float)SEC;
    for (int i = 0; i < n; ++i) {
        if (g[i].level < 0.05f) continue;           /* may have ended (or just started) within the block */
        alive++;
        float want = g[i].pos + step;
        if (want >= 1.f) want -= 1.f;
        for (int j = 0; j < m; ++j)
            if (fabsf(h[j].pos - want) < 2e-6f && fabsf(h[j].len - g[i].len) < 1e-3f) { matched++; break; }
    }
    CHECK(alive > 0 && matched == alive, "grains advanced by %f: %d of %d matched", step, matched, alive);

    /* level follows the Hann window: a grain at mid-life is near 1 somewhere in a 0.2 s window */
    float top = 0.f;
    for (int b = 0; b < 100; ++b) {
        blocks(d, 1);
        n = ec_dsp_grains(d, 0, g);
        for (int i = 0; i < n; ++i) if (g[i].level > top) top = g[i].level;
    }
    CHECK(top > 0.98f, "a grain reaches its window peak (%f)", top);

    /* pitch +12: the read rate doubles */
    {
        ec_track_params_t p = *ec_dsp_get_track_params(d, 0);
        p.pitch = 12.f;
        ec_dsp_track_params(d, 0, &p);
        blocks(d, 80);   /* old grains die out (0.2 s) */
        n = ec_dsp_grains(d, 0, g);
        blocks(d, 1);
        m = ec_dsp_grains(d, 0, h);
        matched = alive = 0;
        for (int i = 0; i < n; ++i) {
            if (g[i].level < 0.05f) continue;
            alive++;
            float want = g[i].pos + 2.f * step;
            if (want >= 1.f) want -= 1.f;
            for (int j = 0; j < m; ++j) if (fabsf(h[j].pos - want) < 1e-4f) { matched++; break; }
        }
        CHECK(alive > 0 && matched == alive, "pitch +12: grains advance 2x (%d of %d)", matched, alive);
    }

    /* stopped: nothing (once the play gate has closed) */
    ec_dsp_transport(d, 0, 0, 0);
    blocks(d, 200);
    CHECK(ec_dsp_grains(d, 0, g) == 0, "stopped: 0 grains (%d)", ec_dsp_grains(d, 0, g));
    free(d); free(sn);

    /* a dense cloud: capped at EC_GRAIN_VIEW_MAX, still exactly one newest, the loudest kept */
    d = grain_engine(EC_WARP_GRANULAR, 64.f, 0.3f, &sn);
    blocks(d, 300);
    t = &d->tr[0];
    n = ec_dsp_grains(d, 0, g);
    CHECK(t->gr.n > EC_GRAIN_VIEW_MAX && n == EC_GRAIN_VIEW_MAX, "dense: %d of %d", n, t->gr.n);
    CHECK(count_newest(g, n) == 1, "dense: exactly one newest (%d)", count_newest(g, n));
    {
        /* the 15 others are the loudest of the engine's grains */
        float lv[EC_MAX_GRAINS];
        int ne = t->gr.n;
        for (int k = 0; k < ne; ++k) lv[k] = ec_hann_lerp(d->hann, t->gr.g[k].phase);
        float minkept = 2.f;
        for (int i = 0; i < n; ++i) if (!g[i].newest && g[i].level < minkept) minkept = g[i].level;
        int louder = 0;
        for (int k = 0; k < ne; ++k) if (lv[k] > minkept + 1e-6f) louder++;
        CHECK(louder <= EC_GRAIN_VIEW_MAX - 1, "dense: kept the loudest (%d louder than the quietest kept)", louder);
    }
    free(d); free(sn);

    /* TAPE: none (the playhead stays); random_ola: its TGrains grains */
    d = grain_engine(EC_WARP_TAPE, 4.f, 0.2f, &sn);
    blocks(d, 100);
    CHECK(d->tr[0].reader_on && ec_dsp_grains(d, 0, g) == 0, "TAPE: 0 grains");
    free(d); free(sn);
    d = grain_engine(EC_WARP_RANDOM_OLA, 4.f, 0.2f, &sn);
    blocks(d, 200);
    n = ec_dsp_grains(d, 0, g);
    CHECK(n > 0 && n == (d->tr[0].w3.ong < EC_GRAIN_VIEW_MAX ? d->tr[0].w3.ong : EC_GRAIN_VIEW_MAX) &&
          count_newest(g, n) == 1, "random_ola: %d grains (engine %d)", n, d->tr[0].w3.ong);
    free(d); free(sn);
}

/* ------------------------------------------------------------------ click */
#define CF 8192
static int16_t cb[2 * CF], cr[2 * CF];

static int peak16(const int16_t *b, int frames)
{
    int pk = 0;
    for (int i = 0; i < 2 * frames; ++i) { int a = abs(b[i]); if (a > pk) pk = a; }
    return pk;
}

static void test_click(void)
{
    printf("metronome click\n");
    static ec_click_t c;
    ec_click_init(&c, (float)TK_FS);
    CHECK(EC_CLICK_N >= 3, "at least 3 sounds");
    for (int s = 0; s < EC_CLICK_N; ++s) CHECK(ec_click_names[s] && ec_click_names[s][0], "name %d", s);
    CHECK(!ec_click_busy(&c), "idle after init");

    for (int s = 0; s < EC_CLICK_N; ++s) {
        /* onset exactly at the offset, both channels equal, silence before */
        memset(cb, 0, sizeof cb);
        ec_click_trigger(&c, 0, s, 1.f);
        ec_click_mix(&c, cb, CF, 777);
        int pre = 0, lr = 1;
        for (int i = 0; i < 777; ++i) pre |= cb[2 * i] | cb[2 * i + 1];
        for (int i = 0; i < CF; ++i) if (cb[2 * i] != cb[2 * i + 1]) lr = 0;
        CHECK(pre == 0, "%s: silent before the offset", ec_click_names[s]);
        CHECK(cb[2 * 777] != 0, "%s: sounds at the offset frame (%d)", ec_click_names[s], cb[2 * 777]);
        CHECK(lr, "%s: L == R", ec_click_names[s]);
        int plain = peak16(cb, CF);
        CHECK(!ec_click_busy(&c) || CF - 777 < (int)(EC_CLICK_MAX_MS * TK_FS / 1000), "%s: ended", ec_click_names[s]);
        int len = 0;
        for (int i = CF - 1; i >= 0; --i) if (cb[2 * i]) { len = i - 777 + 1; break; }
        CHECK(len > 0 && len <= EC_CLICK_MAX_MS * TK_FS / 1000, "%s: %d samples long", ec_click_names[s], len);

        /* the accent is louder; peaks near the targets */
        memset(cr, 0, sizeof cr);
        ec_click_trigger(&c, 1, s, 1.f);
        ec_click_mix(&c, cr, CF, 0);
        int acc = peak16(cr, CF);
        CHECK(acc > plain * 3 / 2, "%s: accent %d louder than %d", ec_click_names[s], acc, plain);
        CHECK(abs(acc - 20644) < 300 && abs(plain - 12451) < 300, "%s: peaks %d / %d", ec_click_names[s], acc, plain);
        /* and higher: more zero crossings in the first 20 ms */
        int za = 0, zp = 0;
        for (int i = 1; i < TK_FS / 50; ++i) {
            za += (cr[2 * i] > 0) != (cr[2 * i - 2] > 0);
            zp += (cb[2 * (777 + i)] > 0) != (cb[2 * (777 + i - 1)] > 0);
        }
        CHECK(za > zp, "%s: accent higher (%d vs %d crossings)", ec_click_names[s], za, zp);

        /* level scales; 0 triggers nothing */
        memset(cr, 0, sizeof cr);
        ec_click_trigger(&c, 1, s, 0.25f);
        ec_click_mix(&c, cr, CF, 0);
        CHECK(abs(peak16(cr, CF) - acc / 4) <= 2, "%s: level 0.25 -> %d", ec_click_names[s], peak16(cr, CF));
    }
    memset(cr, 0, sizeof cr);
    ec_click_trigger(&c, 1, EC_CLICK_WOOD, 0.f);
    ec_click_mix(&c, cr, CF, 0);
    CHECK(peak16(cr, CF) == 0 && !ec_click_busy(&c), "level 0: silent");

    /* deterministic, and split blocks render the same as one buffer */
    memset(cb, 0, sizeof cb);
    ec_click_trigger(&c, 1, EC_CLICK_COWBELL, 0.8f);
    ec_click_mix(&c, cb, CF, 300);
    memset(cr, 0, sizeof cr);
    ec_click_trigger(&c, 1, EC_CLICK_COWBELL, 0.8f);
    ec_click_mix(&c, cr, 256, 999);                 /* past the buffer: waits */
    CHECK(peak16(cr, 256) == 0 && ec_click_busy(&c), "an offset past the buffer waits");
    ec_click_mix(&c, cr + 512, 128, 300 - 256);
    for (int p = 384; p < CF; p += 128) ec_click_mix(&c, cr + 2 * p, 128, 0);
    CHECK(memcmp(cb, cr, sizeof cb) == 0, "same samples in 128-frame blocks");

    /* saturation: no wrap-around near full scale */
    static int16_t ref[2 * CF];
    memset(ref, 0, sizeof ref);
    ec_click_trigger(&c, 1, EC_CLICK_BEEP, 1.f);
    ec_click_mix(&c, ref, CF, 0);
    for (int i = 0; i < 2 * CF; ++i) cb[i] = (int16_t)((i & 2) ? -30000 : 30000);
    ec_click_trigger(&c, 1, EC_CLICK_BEEP, 1.f);
    ec_click_mix(&c, cb, CF, 0);
    int sat_ok = 1, clipped = 0;
    for (int i = 0; i < 2 * CF; ++i) {
        int32_t want = (int32_t)((i & 2) ? -30000 : 30000) + ref[i];
        if (want > 32767) { want = 32767; clipped++; }
        if (want < -32768) { want = -32768; clipped++; }
        if (cb[i] != want) sat_ok = 0;
    }
    CHECK(sat_ok && clipped > 0, "saturates (%d clipped samples)", clipped);

    /* a new trigger replaces a ringing click at its offset; before it, the old one rings on */
    memset(ref, 0, sizeof ref);
    ec_click_trigger(&c, 0, EC_CLICK_BEEP, 1.f);
    ec_click_mix(&c, ref, 200, 0);
    ec_click_mix(&c, ref + 400, 200, 0);            /* the old click alone, frames 200..399 */
    memset(cb, 0, sizeof cb);
    ec_click_trigger(&c, 0, EC_CLICK_BEEP, 1.f);
    ec_click_mix(&c, cb, 200, 0);
    ec_click_trigger(&c, 1, EC_CLICK_WOOD, 1.f);
    ec_click_mix(&c, cb + 400, 200, 50);
    CHECK(memcmp(cb + 400, ref + 400, 50 * 4) == 0, "the ringing click plays up to the offset");
    CHECK(memcmp(cb + 500, ref + 500, 4) != 0 && cb[500] != 0, "the new click starts at the offset");
}

int main(void)
{
    (void)tk_ev;
    test_grains();
    test_click();
    printf("test_v6: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
