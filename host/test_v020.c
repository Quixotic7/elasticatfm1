/* SPDX-License-Identifier: GPL-3.0-only */
/* test_v020: the seams between E3, E4 and E5, through the real chain (project -> sequencer -> engine, as ec_app wires
 * it). One project: T1 a loop with an insert (DJ EQ, every band killed) and a pre-insert send to S1 (REVERB), T2 the
 * NEIGH machine behind it, T3 WAVETABLE with LFO 1 on PITCH, T4 R.POLY on the razor table, T5 HARMONIZER, T6 a loop
 * with a DELAY insert. Checks:
 *   - the neighbor hears T1 after its insert (killed: T2 silent) and T1's pre-insert send still feeds S1;
 *   - the pitch LFO moves the wavetable oscillator (E4 mod -> an E3 reader);
 *   - R.POLY slices carry their razor ranges; the pattern's slice choke (E5) wins over the razor table's;
 *   - a scene glide morphs E3 (harm_interval), E4 track (delay_feedback) and bus (S1 reverb_size) params;
 *   - pattern copy / paste carries every E3 / E4 param;
 *   - save -> load -> the same record bytes, the same tables, and a rebooted engine renders the same audio.
 *   ec_test_v020 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ec_dsp.h"
#include "ec_e5.h"
#include "ec_project.h"
#include "ec_razor.h"
#include "ec_scene.h"
#include "ec_seq.h"
#include "ec_layout_test.h"

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

/* ------------------------------------------------------------------------------------------- the rig --- */
typedef struct {
    ec_project_t p;
    ec_seq_t s;
    ec_dsp_t *d;
    int nslice;                 /* razor slice events seen on T4 */
    int slice_ranges_ok;
    int choke_seen;
} rig_t;

static uint8_t g_mem[2][EC_DSP_MEM_MAX] __attribute__((aligned(16)));
static int16_t *g_loop, *g_saw;
static uint32_t g_loop_n, g_saw_n;

static void sk_event(void *ctx, const ec_event_t *e)
{
    rig_t *r = ctx;
    if (e->track == 3 && e->type == EC_EV_TRIG && e->slice >= 1) {
        float s, en;
        ec_razor_range(&r->p, 3, e->slice, &s, &en);
        r->nslice++;
        if (fabsf(e->start - s) < 1e-4f && fabsf(e->end - en) < 1e-4f)
            r->slice_ranges_ok++;
        if (e->slice == 2 && e->choke == 3)
            r->choke_seen++;
    }
    ec_dsp_event(r->d, e);
}
static void sk_params(void *ctx, int t, const ec_track_params_t *p) { ec_dsp_track_params(((rig_t *)ctx)->d, t, p); }
static void sk_process(void *ctx, float *out, int n) { ec_dsp_process(((rig_t *)ctx)->d, out, n); }

/* the app's per-call bus push: the bus records + the crossfader's morph (scene rows 6..8) */
static void push_buses(rig_t *r)
{
    int b, id;
    for (b = 0; b < 3; b++) {
        float v[EC_NP];
        ec_fx_params_t fp;
        memcpy(v, r->p.bus[b], sizeof v);
        for (id = 0; id < EC_NP; id++)
            if (r->p.e5.sc.on[EC_NTRACK + b][id])
                v[id] = r->p.e5.sc.xf[EC_NTRACK + b][id];
        ec_params_fx(v, &fp);
        ec_dsp_bus_params(r->d, b, &fp);
    }
}

static void rig_boot(rig_t *r, int mem)
{
    ec_seq_sink_t k = {r, sk_event, sk_params, sk_process};
    ec_sample_t loop = {g_loop, g_loop_n, (float)FS, 2, 120.f}, saw = {g_saw, g_saw_n, (float)FS, 2, 0.f};
    int t;
    r->d = ec_dsp_init(g_mem[mem], (float)FS);
    ec_seq_init(&r->s, &r->p, &k, FS);
    for (t = 0; t < EC_NTRACK; t++)
        ec_dsp_set_sample(r->d, t, t == 2 ? &saw : &loop);
    r->nslice = r->slice_ranges_ok = r->choke_seen = 0;
}

/* render n frames (1024 a call, the bus push before each, as ec_app_audio) into out (may be 0) */
static void rig_run(rig_t *r, float *out, int n)
{
    static float tmp[2 * 1024];
    while (n > 0) {
        const int b = n > 1024 ? 1024 : n;
        push_buses(r);
        ec_seq_render(&r->s, out ? out : tmp, b);
        if (out)
            out += 2 * b;
        n -= b;
    }
}

static double rms(const float *b, int n)
{
    double s = 0;
    int i;
    for (i = 0; i < 2 * n; i++)
        s += (double)b[i] * b[i];
    return sqrt(s / (2.0 * (n > 0 ? n : 1)));
}
/* |DFT| of the left channel at f (Goertzel), normalised */
static double tone(const float *b, int n, double f)
{
    const double w = 2 * 3.14159265358979 * f / FS, c = 2 * cos(w);
    double s1 = 0, s2 = 0;
    int i;
    for (i = 0; i < n; i++) {
        const double s0 = b[2 * i] + c * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    return sqrt(s1 * s1 + s2 * s2 - c * s1 * s2) / n;
}

/* ------------------------------------------------------------------------------------------- material --- */
static void make_material(void)
{
    uint32_t i;
    g_loop_n = (uint32_t)(2 * FS);   /* 2 s = one bar at 120: a kick + noisy hat + a bass tone */
    g_loop = calloc(2 * (size_t)g_loop_n, sizeof(int16_t));
    for (i = 0; i < g_loop_n; i++) {
        const uint32_t ph = i % (uint32_t)(FS / 4);
        const double env = exp(-(double)ph / 3000.0), t = (double)ph / FS;
        const double hat = ((i * 1103515245u + 12345u) >> 16 & 0x7FFF) / 16384.0 - 1.0;
        const double x = 0.6 * env * sin(2 * 3.14159265358979 * (50 + 80 * env) * t) +
                         0.2 * exp(-(double)((i + 5512) % 11025) / 800.0) * hat +
                         0.15 * sin(2 * 3.14159265358979 * 110.0 * (double)i / FS);
        g_loop[2 * i] = g_loop[2 * i + 1] = (int16_t)(x * 26000);
    }
    g_saw_n = (uint32_t)FS;          /* 1 s of a 20-partial saw (the wavetable's bank) */
    g_saw = calloc(2 * (size_t)g_saw_n, sizeof(int16_t));
    for (i = 0; i < g_saw_n; i++) {
        double v = 0;
        int k;
        for (k = 1; k <= 20; k++)
            v += sin(2 * 3.14159265358979 * 220.0 * k * (double)i / FS) / k;
        g_saw[2 * i] = g_saw[2 * i + 1] = (int16_t)(v * 0.5 * 26000 / 1.8);
    }
}

static void every_step(ec_ptrack_t *tr, uint32_t slices)
{
    int s;
    for (s = 0; s < 16; s++)
        if (s % 2 == 0) {
            tr->steps[s].trig = 1;
            tr->steps[s].slices = slices ? (s / 2 % 2 ? slices : 1u) : 0;
        }
}

static void build(ec_project_t *p)
{
    float *v;
    int t;
    ec_project_init(p);
    for (t = 0; t < EC_NTRACK; t++)
        p->pat[0].tr[t].v[EC_P_MACHINE] = 1;                         /* LOOP */
    v = p->pat[0].tr[0].v;                                          /* T1: DJ EQ insert, all bands killed, wet */
    v[EC_P_FX_MACHINE] = 12;
    v[EC_P_EQ_LOW] = v[EC_P_EQ_MID] = v[EC_P_EQ_HIGH] = 0;
    v[EC_P_FX_MIX] = 127;
    v[EC_P_SEND1] = 127;                                            /* + a PRE send to S1 */
    v[EC_P_SEND_TAP] = 1;
    p->pat[0].tr[1].v[EC_P_MACHINE] = 7;                            /* T2: NEIGH */
    v = p->pat[0].tr[2].v;                                          /* T3: WAVETABLE + LFO 1 on PITCH */
    v[EC_P_MODE] = 8;
    v[EC_P_LFO1_DEST] = 2;                                          /* PITCH */
    v[EC_P_LFO1_WAVE] = 5;                                          /* SQR */
    v[EC_P_LFO1_SPEED] = 1;                                         /* 8 bars */
    v[EC_P_LFO1_DEPTH] = 64;                                        /* 0 (the run turns it up) */
    v = p->pat[0].tr[3].v;                                          /* T4: R.POLY, 8 razor slices */
    v[EC_P_MACHINE] = 6;
    v[EC_P_SLICE_COUNT] = 8;
    ec_razor_redistribute(&p->razor[3], 8);
    ec_razor_set_point(&p->razor[3], 2, 0, 20.0f, 0, 8);            /* slice 2 starts later than the grid */
    p->razor[3].choke[1] = 5;                                       /* razor choke E for slice 2 ... */
    p->pe5[0].slices[3].choke[1] = 3;                               /* ... the pattern's C wins */
    every_step(&p->pat[0].tr[3], 1u << 1);
    v = p->pat[0].tr[4].v;                                          /* T5: HARMONIZER */
    v[EC_P_MODE] = 7;
    v[EC_P_HARM_INTERVAL] = 7;
    v = p->pat[0].tr[5].v;                                          /* T6: DELAY insert */
    v[EC_P_FX_MACHINE] = 2;
    v[EC_P_DELAY_FEEDBACK] = 40;
    p->bus[0][EC_P_FX_MACHINE] = 3;                                 /* S1: REVERB */
    p->bus[0][EC_P_REVERB_SIZE] = 64;
    p->bus[0][EC_P_FX_MIX] = 127;
}

/* ------------------------------------------------------------------------------------------- the tests --- */
static rig_t R1, R2;
static float g_out[2][2 * 44100 * 3];

static void test_routing(void)
{
    const int n = (int)FS * 2;
    ec_track_status_t st;
    ec_bus_status_t bs;
    double r_on, r_off;
    int t;
    printf("routing: T2 (NEIGH) hears T1 after its insert; T1's PRE send still feeds S1\n");
    build(&R1.p);
    for (t = 2; t < EC_NTRACK; t++)
        R1.p.pat[0].tr[t].v[EC_P_MUTE] = 1;                         /* T1 + T2 only */
    rig_boot(&R1, 0);
    ec_seq_start(&R1.s);
    rig_run(&R1, g_out[0], n);
    ec_dsp_bus_status(R1.d, 0, &bs);
    r_on = rms(g_out[0] + (int)FS, n - (int)FS);
    ec_dsp_track_status(R1.d, 1, &st);
    printf("  insert killing: master %.4f (S1 only), T2 peak %.5f, S1 return peak %.4f\n", r_on, st.peak_l, bs.peak_l);
    CHECK(st.peak_l < 1e-3f, "T2 hears T1 before its insert (%.5f)", st.peak_l);
    CHECK(bs.peak_l > 0.01f, "T1's PRE send does not reach S1 (%.5f)", bs.peak_l);
    /* the same with the insert off: T2 carries T1 */
    build(&R1.p);
    for (t = 2; t < EC_NTRACK; t++)
        R1.p.pat[0].tr[t].v[EC_P_MUTE] = 1;
    R1.p.pat[0].tr[0].v[EC_P_FX_MACHINE] = 1;
    R1.p.pat[0].tr[0].v[EC_P_SEND1] = 0;
    rig_boot(&R1, 0);
    ec_seq_start(&R1.s);
    rig_run(&R1, g_out[0], n);
    ec_dsp_track_status(R1.d, 1, &st);
    r_off = rms(g_out[0] + (int)FS, n - (int)FS);
    printf("  no insert: master %.4f, T2 peak %.4f\n", r_off, st.peak_l);
    CHECK(st.peak_l > 0.05f && r_off > 0.02, "T2 silent without T1's insert (%.4f)", st.peak_l);
    /* T1 muted: the tap is post-mix (SC: the mix synth is re-pointed), T2 silent */
    R1.p.pat[0].tr[0].v[EC_P_MUTE] = 1;
    ec_seq_touch(&R1.s, 0);
    rig_run(&R1, g_out[0], n);
    r_off = rms(g_out[0] + (int)FS, n - (int)FS);
    printf("  T1 muted: master %.6f\n", r_off);
    CHECK(r_off < 1e-4, "a muted T1 still reaches T2 (%.6f)", r_off);
}

static void test_pitch_lfo(void)
{
    const int n = (int)FS;
    double base[3], mod[3];
    const double f[3] = {130.81, 261.63, 523.25};   /* (the oscillator plays C4 at pitch 0 here) */
    ec_track_status_t st;
    int t, k, pass;
    printf("pitch LFO -> WAVETABLE: the oscillator moves an octave\n");
    for (pass = 0; pass < 2; pass++) {
        build(&R1.p);
        for (t = 0; t < EC_NTRACK; t++)
            R1.p.pat[0].tr[t].v[EC_P_MUTE] = t != 2;
        R1.p.pat[0].tr[2].v[EC_P_LFO1_DEPTH] = pass ? 128 : 64;     /* +1 = 12 st */
        rig_boot(&R1, 0);
        ec_seq_start(&R1.s);
        rig_run(&R1, 0, n / 2);
        rig_run(&R1, g_out[0], n);
        ec_dsp_track_status(R1.d, 2, &st);
        for (k = 0; k < 3; k++)
            (pass ? mod : base)[k] = tone(g_out[0], n, f[k]);
        printf("  depth %s: pitch mod %+.2f, |131| %.4f |262| %.4f |523| %.4f\n", pass ? "+1" : " 0",
               st.mod[EC_MOD_PITCH], (pass ? mod : base)[0], (pass ? mod : base)[1], (pass ? mod : base)[2]);
        if (pass)
            CHECK(fabsf(st.mod[EC_MOD_PITCH]) > 0.5f, "the LFO does not reach PITCH (%.3f)", st.mod[EC_MOD_PITCH]);
    }
    CHECK(base[1] > 0.02 && base[1] > 10 * base[0], "no 262 Hz fundamental without mod");
    CHECK(mod[1] < base[1] / 10 && mod[2] > 0.02, "the pitch mod does not move the fundamental an octave up");
}

static void test_razor_and_scenes(void)
{
    ec_clip_pattern_t clip;
    float harm, fb;
    int i, ok;
    printf("razor poly + scenes + copy / paste\n");
    build(&R1.p);
    rig_boot(&R1, 0);
    ec_seq_start(&R1.s);
    rig_run(&R1, g_out[0], (int)FS * 2);
    printf("  T4: %d slice trigs, %d with their razor range, %d slice-2 trigs with choke C\n", R1.nslice,
           R1.slice_ranges_ok, R1.choke_seen);
    CHECK(R1.nslice >= 6 && R1.slice_ranges_ok == R1.nslice, "razor ranges (%d / %d)", R1.slice_ranges_ok, R1.nslice);
    CHECK(R1.choke_seen >= 2, "the pattern's slice choke does not win (%d)", R1.choke_seen);
    CHECK(rms(g_out[0], (int)FS * 2) > 0.02, "the full project is silent");
    /* scene A: as built; scene B: harm 12, delay feedback 100, S1 reverb size 127 */
    ec_scene_capture(&R1.p, 0);
    ec_scene_set(&R1.p, 1, 4, EC_P_HARM_INTERVAL, 12);
    ec_scene_set(&R1.p, 1, 5, EC_P_DELAY_FEEDBACK, 100);
    ec_scene_set(&R1.p, 1, EC_NTRACK + 0, EC_P_REVERB_SIZE, 127);
    {   /* the glide from A (0) toward B, stopped at the middle */
        float pos = 0;
        for (i = 0; i < 300 && pos < 0.5f; i++) {
            pos = ec_scene_glide_step(pos, 1.0f, 1);
            if (ec_scene_apply(&R1.p, pos))
                for (int t = 0; t < EC_NTRACK; t++)
                    ec_seq_touch(&R1.s, t);
            rig_run(&R1, 0, (int)(FS / EC_GLIDE_HZ));
        }
        printf("  glide: %d steps to %.3f\n", i, pos);
    }
    harm = ec_seq_value(&R1.s, 4, EC_P_HARM_INTERVAL);
    fb = ec_seq_value(&R1.s, 5, EC_P_DELAY_FEEDBACK);
    printf("  at the middle: harm %.2f (7..12), delay fbk %.1f (40..100), S1 size %.1f (64..127) on:%d\n", harm, fb,
           R1.p.e5.sc.xf[EC_NTRACK][EC_P_REVERB_SIZE], R1.p.e5.sc.on[EC_NTRACK][EC_P_REVERB_SIZE]);
    CHECK(harm > 9.0f && harm < 10.0f, "E3 harm_interval does not morph (%.2f)", harm);
    CHECK(fb > 68.0f && fb < 72.0f, "E4 delay_feedback does not morph (%.1f)", fb);
    CHECK(R1.p.e5.sc.on[EC_NTRACK][EC_P_REVERB_SIZE] && R1.p.e5.sc.xf[EC_NTRACK][EC_P_REVERB_SIZE] > 94.0f &&
              R1.p.e5.sc.xf[EC_NTRACK][EC_P_REVERB_SIZE] < 97.5f,
          "the S1 bus param does not morph");
    CHECK(!ec_scene_row_morphable(EC_NTRACK, EC_P_FILTER_CUTOFF) && ec_scene_row_morphable(EC_NTRACK, EC_P_FX_MIX) &&
              !ec_scene_row_morphable(EC_NTRACK, EC_P_FX_MACHINE) && ec_scene_row_morphable(0, EC_P_WT_CYCLE),
          "row morphable rules");
    /* copy pattern 1 -> pattern 2: every param (E3 + E4 ids) */
    ec_clip_copy_pattern(&clip, &R1.p, 0);
    ec_clip_paste_pattern(&clip, &R1.p, 1);
    ok = R1.p.pat[1].used;
    for (i = 0; i < EC_NTRACK; i++)
        ok &= !memcmp(R1.p.pat[1].tr[i].v, R1.p.pat[0].tr[i].v, sizeof R1.p.pat[0].tr[i].v) &&
              !memcmp(R1.p.pat[1].tr[i].steps, R1.p.pat[0].tr[i].steps, sizeof R1.p.pat[0].tr[i].steps);
    CHECK(ok, "the pasted pattern differs");
    CHECK(R1.p.pe5[1].slices[3].choke[1] == 3, "the slice program did not travel with the pattern");
}

static uint8_t g_rec[2][1 << 20];

static void test_save_reboot(void)
{
    const int n = (int)FS * 3;
    int32_t a, b;
    double diff = 0;
    int i;
    printf("save -> reboot -> the same state, the same sound\n");
    /* R1 holds the project of test_razor_and_scenes (scenes, the morph at the middle, two patterns) */
    a = ec_project_save(&R1.p, g_rec[0], sizeof g_rec[0]);
    CHECK(a > 0, "save");
    CHECK(ec_project_load(&R2.p, g_rec[0], (uint32_t)a) == 0, "load");
    b = ec_project_save(&R2.p, g_rec[1], sizeof g_rec[1]);
    printf("  record %d bytes, resave %d\n", (int)a, (int)b);
    CHECK(a == b && !memcmp(g_rec[0], g_rec[1], (size_t)a), "the reloaded project saves differently");
    CHECK(!memcmp(R1.p.razor, R2.p.razor, sizeof R1.p.razor), "razor tables");
    CHECK(!memcmp(R1.p.bus, R2.p.bus, sizeof R1.p.bus), "bus records");
    CHECK(!memcmp(R1.p.e5.sc.has, R2.p.e5.sc.has, sizeof R1.p.e5.sc.has) &&
              !memcmp(R1.p.e5.sc.val, R2.p.e5.sc.val, sizeof R1.p.e5.sc.val) && R1.p.e5.sc.pos == R2.p.e5.sc.pos,
          "scenes (bus rows included)");
    CHECK(!memcmp(R1.p.pe5, R2.p.pe5, sizeof R1.p.pe5), "pattern extras, slice programs");
    for (i = 0; i < 2; i++)
        CHECK(!memcmp(&R1.p.pat[i], &R2.p.pat[i], sizeof R1.p.pat[i]), "pattern %d", i + 1);
    /* both booted fresh from the same record: the same audio, sample for sample */
    CHECK(ec_project_load(&R1.p, g_rec[0], (uint32_t)a) == 0, "load 2");
    rig_boot(&R1, 0);
    rig_boot(&R2, 1);
    ec_scene_apply(&R1.p, R1.p.e5.sc.pos);
    ec_scene_apply(&R2.p, R2.p.e5.sc.pos);
    ec_seq_start(&R1.s);
    ec_seq_start(&R2.s);
    rig_run(&R1, g_out[0], n);
    rig_run(&R2, g_out[1], n);
    for (i = 0; i < 2 * n; i++)
        diff += fabs((double)g_out[0][i] - g_out[1][i]);
    printf("  rebooted twice: rms %.4f / %.4f, sum |diff| %.3g\n", rms(g_out[0], n), rms(g_out[1], n), diff);
    CHECK(rms(g_out[0], n) > 0.02 && diff == 0.0, "the two boots differ");
}

int main(void)
{
    ec_layout_use(&EC_LAYOUT_TEST);   /* the tests' fixed layout: 0.4.0's pages (host/ec_layout_test.h) */
    make_material();
    test_routing();
    test_pitch_lfo();
    test_razor_and_scenes();
    test_save_reboot();
    printf("test_v020: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
