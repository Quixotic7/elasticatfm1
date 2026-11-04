/* SPDX-License-Identifier: GPL-3.0-only */
/* test_e5: E5 natively, through a recording sink: scene morph values at fader positions (two-sided, one-sided,
 * sparse capture, the knob taking over, a step p-lock over the morph, a pattern switch), the glide's timing,
 * copy / clear / paste (steps with their spacing and locks, page, pattern, slice), page-loop playback order,
 * per-pattern BPM, memorize / recall during playback, undo, slice programs on a firing step, the record's E5 chunk
 * round trip, and an E2 save (host/fixtures/e2_flash.bin, argv[1]) loading with the E5 defaults.
 *   ec_test_e5 <e2_flash.bin> */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ec_e5.h"
#include "ec_project.h"
#include "ec_scene.h"
#include "ec_seq.h"
#include "ec_layout_test.h"

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
#define NEAR(a, b) (fabs((double)(a) - (double)(b)) < 1e-4)

typedef struct {
    uint64_t at;
    ec_event_t ev;
} rec_t;

static ec_project_t P, Q, M;
static ec_seq_t S;
static rec_t R[MAXREC];
static int nrec, nparams;
static ec_track_params_t lastp[EC_NTRACK];

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
    nparams++;
}

static void setup(void)
{
    ec_seq_sink_t k = {0, on_event, on_params, 0, 0, 0};
    ec_project_init(&P);
    ec_seq_init(&S, &P, &k, FS);
    nrec = nparams = 0;
}
static void run(uint64_t n)
{
    while (n > 0) {
        const int b = n > 128 ? 128 : (int)n;
        ec_seq_render(&S, 0, b);
        n -= (uint64_t)b;
    }
}
static float *v0(int t) { return ec_project_track(&P, t)->v; }
static float val(int t, int id) { return ec_seq_value(&S, t, id); }

/* ------------------------------------------------------------------------------------------- scenes --- */
static void test_scene_morph(void)
{
    setup();
    v0(0)[EC_P_FILTER_CUTOFF] = 100;
    v0(1)[EC_P_PAN] = 20;
    ec_scene_capture(&P, 0);                          /* A: T1 cut 100, T2 pan 20 */
    CHECK(ec_scene_count(&P.e5.sc, 0) == 2, "capture A: sparse (%d values)", ec_scene_count(&P.e5.sc, 0));
    v0(0)[EC_P_FILTER_CUTOFF] = 40;
    v0(1)[EC_P_PAN] = 100;
    v0(2)[EC_P_FILTER_RES] = 77;
    ec_scene_capture(&P, 1);                          /* B: cut 40, pan 100, T3 res 77 */
    CHECK(ec_scene_count(&P.e5.sc, 1) == 3, "capture B: %d values", ec_scene_count(&P.e5.sc, 1));
    CHECK(ec_scene_targets(&P) == 2, "targets %d (T3 res is one-sided at its base: both ends 77)",
          ec_scene_targets(&P));
    /* morph values at the positions */
    ec_scene_apply(&P, 0.0f);
    CHECK(NEAR(val(0, EC_P_FILTER_CUTOFF), 100) && NEAR(val(1, EC_P_PAN), 20), "pos 0 = A");
    ec_scene_apply(&P, 0.25f);
    CHECK(NEAR(val(0, EC_P_FILTER_CUTOFF), 85) && NEAR(val(1, EC_P_PAN), 40), "pos 0.25: cut %.3f pan %.3f",
          (double)val(0, EC_P_FILTER_CUTOFF), (double)val(1, EC_P_PAN));
    ec_scene_apply(&P, 0.5f);
    CHECK(NEAR(val(0, EC_P_FILTER_CUTOFF), 70) && NEAR(val(1, EC_P_PAN), 60), "pos 0.5");
    ec_scene_apply(&P, 1.0f);
    CHECK(NEAR(val(0, EC_P_FILTER_CUTOFF), 40) && NEAR(val(1, EC_P_PAN), 100), "pos 1 = B");
    CHECK(v0(0)[EC_P_FILTER_CUTOFF] == 40, "the track value is never written");
    /* the engine gets the morph */
    ec_scene_apply(&P, 0.0f);
    ec_seq_touch_all(&S);
    nparams = 0;
    run(128);
    CHECK(nparams >= 2, "params resent after a move (%d)", nparams);
    /* one-sided: only A holds T3 res 10; B falls back to the base (the track value) */
    ec_scene_set(&P, 0, 2, EC_P_FILTER_RES, 10);
    CHECK(val(2, EC_P_FILTER_RES) == 77, "a scene edit alone does not move the sound");
    ec_scene_clear_key(&P, 2, EC_P_FILTER_RES);
    ec_scene_set(&P, 0, 2, EC_P_FILTER_RES, 10);
    ec_scene_apply(&P, 0.5f);
    CHECK(NEAR(val(2, EC_P_FILTER_RES), 43.5), "one-sided at 0.5: %.3f", (double)val(2, EC_P_FILTER_RES));
    /* a hand edit takes the param back until the fader moves again; the base follows the edit */
    ec_scene_knob(&P, 2, EC_P_FILTER_RES);
    v0(2)[EC_P_FILTER_RES] = 100;
    CHECK(val(2, EC_P_FILTER_RES) == 100, "the knob takes over");
    ec_scene_apply(&P, 0.5f);
    CHECK(NEAR(val(2, EC_P_FILTER_RES), 55), "the morph resumes from the new base: %.3f",
          (double)val(2, EC_P_FILTER_RES));
    /* only continuous params morph */
    CHECK(!ec_scene_morphable(EC_P_FILTER_TYPE) && !ec_scene_morphable(EC_P_AMP) &&
              !ec_scene_morphable(EC_P_PATTERN_STEPS) && !ec_scene_morphable(EC_P_TRIG_CHANCE) &&
              ec_scene_morphable(EC_P_FILTER_CUTOFF) && ec_scene_morphable(EC_P_LOOP_START),
          "morphable set");
    ec_scene_set(&P, 0, 0, EC_P_FILTER_TYPE, 3);
    CHECK(!ec_scene_get(&P, 0, 0, EC_P_FILTER_TYPE, 0), "an option never enters a scene");
    /* step p-lock > crossfader > track */
    ec_scene_apply(&P, 0.0f);                         /* T1 cut 100 from A */
    {
        ec_ptrack_t *tr = ec_project_track(&P, 0);
        tr->v[EC_P_PATTERN_STEPS] = 16;
        tr->steps[0].trig = 1;
        ec_step_lock_set(&tr->steps[0], EC_P_FILTER_CUTOFF, 5);
        tr->steps[1].trig = 1;                         /* 0.6.4: a plain trig ends the sticky lock */
        tr->v[EC_P_LENGTH] = 1;
        ec_seq_start(&S);
        run(64);
        CHECK(val(0, EC_P_FILTER_CUTOFF) == 5, "a firing step's lock wins over the morph");
        run(6000);                                     /* step 2 (5512.5): a plain trig, the lock lets go */
        CHECK(NEAR(val(0, EC_P_FILTER_CUTOFF), 100), "the morph resumes at the next plain trig: %.2f",
              (double)val(0, EC_P_FILTER_CUTOFF));
    }
    /* a pattern switch: the one-sided morph against the new pattern's base */
    ec_seq_stop(&S);
    ec_scene_clear(&P, 1);
    ec_scene_clear(&P, 0);
    ec_scene_set(&P, 1, 0, EC_P_FILTER_DRIVE, 100);  /* B only */
    ec_scene_apply(&P, 0.5f);
    CHECK(NEAR(val(0, EC_P_FILTER_DRIVE), 50), "drive 0..100 at 0.5");
    ec_pattern_prepare(&P, 3);
    P.pat[3].tr[0].v[EC_P_FILTER_DRIVE] = 60;
    ec_seq_request_pattern(&S, 3);
    CHECK(P.current == 3 && NEAR(val(0, EC_P_FILTER_DRIVE), 80), "after the switch: base 60 -> %.2f",
          (double)val(0, EC_P_FILTER_DRIVE));
}

static void test_glide(void)
{
    float x = 0;
    int k, land = -1;
    for (k = 1; k <= 60; k++) {
        x = ec_scene_glide_step(x, 1.0f, 0);
        if (k == 9)
            CHECK(fabsf(x - (1.0f - powf(5.0f / 6.0f, 9))) < 1e-4, "after 0.3 s (9 steps at 30 Hz): %.4f", (double)x);
        if (x == 1.0f && land < 0)
            land = k;
    }
    CHECK(land >= 28 && land <= 32, "lands on the target in %d steps (~1 s)", land);
    x = 0.5f;
    x = ec_scene_glide_step(x, 0.0f, 0);
    CHECK(NEAR(x, 0.5f * 5.0f / 6.0f), "glides back down");
    x = 0;
    for (k = 1, land = -1; k <= 200; k++) {
        x = ec_scene_glide_step(x, 1.0f, 1);
        if (k == 15)
            CHECK(fabsf(x - 0.1f) < 1e-4, "FN: linear, 0.1 after 0.5 s (%.4f)", (double)x);
        if (x == 1.0f && land < 0)
            land = k;
    }
    CHECK(land >= 149 && land <= 151, "FN: the whole travel in 5 s (%d steps)", land);
}

/* ------------------------------------------------------------------------------------------- copy / paste --- */
static void test_clipboards(void)
{
    static ec_clip_steps_t cs;
    static ec_clip_page_t cp;
    static ec_clip_pattern_t cpat;
    static ec_clip_slice_t csl;
    ec_ptrack_t *tr;
    float x;
    int n;
    setup();
    tr = ec_project_track(&P, 0);
    tr->steps[0].trig = 1;
    ec_step_lock_set(&tr->steps[0], EC_P_FILTER_CUTOFF, 50);
    tr->steps[3].trig = 1;
    tr->steps[3].slices = 0x4;
    /* copy steps 1 and 4 (held), paste on held 10: 10 and 13, with the lock */
    n = ec_clip_copy_steps(&cs, tr, 0, (1u << 0) | (1u << 3));
    CHECK(n == 2 && cs.off[0] == 0 && cs.off[1] == 3, "copied 2, offsets %d %d", cs.off[0], cs.off[1]);
    CHECK(ec_clip_paste_mask(&cs, 1u << 9) == ((1u << 9) | (1u << 12)), "paste mask");
    n = ec_clip_paste_steps(&cs, tr, 0, 1u << 9);
    CHECK(n == 2 && tr->steps[9].trig && ec_step_lock_get(&tr->steps[9], EC_P_FILTER_CUTOFF, &x) && x == 50 &&
              tr->steps[12].trig && tr->steps[12].slices == 0x4 && !tr->steps[10].trig && !tr->steps[11].trig,
          "pasted on 10 and 13 with the spacing and the lock");
    /* near the edge the overflow is dropped */
    n = ec_clip_paste_steps(&cs, tr, 0, 1u << 14);
    CHECK(n == 1 && tr->steps[14].trig && !tr->steps[16].trig && !tr->steps[17].trig, "past the page edge: dropped");
    /* an empty copied step clears its target */
    ec_clip_copy_steps(&cs, tr, 0, 1u << 1);
    ec_clip_paste_steps(&cs, tr, 0, 1u << 9);
    CHECK(!ec_step_has_content(&tr->steps[9]), "an empty step pastes as empty");
    /* paste on another page */
    ec_clip_copy_steps(&cs, tr, 0, (1u << 0) | (1u << 3));
    ec_clip_paste_steps(&cs, tr, 1, 1u << 0);
    CHECK(tr->steps[16].trig && tr->steps[19].trig, "pasted on page 2");
    /* clear = the locks go, the trig stays */
    ec_clip_clear_locks(tr, 0, 1u << 0);
    CHECK(tr->steps[0].trig && tr->steps[0].nlock == 0, "clear keeps the trig");
    /* page scope */
    ec_clip_copy_page(&cp, tr, 0);
    ec_clip_paste_page(&cp, tr, 2);
    CHECK(!memcmp(&tr->steps[32], &tr->steps[0], 16 * sizeof(ec_step_t)), "page 1 pasted to page 3");
    ec_clip_clear_page(tr, 2);
    CHECK(!ec_step_has_content(&tr->steps[32]) && !ec_step_has_content(&tr->steps[35]), "page 3 cleared");
    /* pattern scope: params + steps + its E5 data, the destination keeps its name; clear = the trigs only */
    P.pe5[0].bpm = 97;
    snprintf(P.pe5[0].name, EC_NAME, "SOURCE");
    v0(0)[EC_P_FILTER_RES] = 33;
    ec_clip_copy_pattern(&cpat, &P, 0);
    snprintf(P.pe5[5].name, EC_NAME, "DEST");
    ec_clip_paste_pattern(&cpat, &P, 5);
    CHECK(P.pat[5].used && P.pat[5].tr[0].v[EC_P_FILTER_RES] == 33 && P.pat[5].tr[0].steps[3].trig &&
              P.pe5[5].bpm == 97 && !strcmp(P.pe5[5].name, "DEST"),
          "pattern pasted (name kept)");
    ec_clip_clear_pattern(&P, 5);
    CHECK(!ec_track_has_trigs(&P.pat[5].tr[0]) && P.pat[5].tr[0].v[EC_P_FILTER_RES] == 33, "pattern clear: trigs only");
    /* slice scope */
    {
        ec_slicemap_t *m = ec_e5_slices(&P, 1);
        ec_slice_lock_set(m, 3, EC_P_PITCH, 7);
        m->choke[2] = 2;
        m->mute = 1u << 2;
        ec_clip_copy_slice(&csl, m, 3);
        ec_clip_paste_slice(&csl, m, 9);
        CHECK(ec_slice_lock_get(m, 9, EC_P_PITCH, &x) && x == 7 && m->choke[8] == 2 && (m->mute >> 8 & 1u),
              "slice program pasted");
        ec_slice_clear(m, 3);
        CHECK(!ec_slice_has_program(m, 3) && ec_slice_has_program(m, 9), "slice cleared");
    }
}

/* ------------------------------------------------------------------------------------------- page loop --- */
static void test_page_loop(void)
{
    ec_ptrack_t *tr;
    int seen[64], n = 0, last = -1, i, ok = 1;
    setup();
    tr = ec_project_track(&P, 0);
    tr->v[EC_P_PATTERN_STEPS] = 40;                 /* pages 1, 2, 3 (8 steps) */
    for (i = 0; i < 40; i++)
        tr->steps[i].trig = 1;
    ec_e5_toggle_page_loop(&P, 0, 1, 3);            /* page 2 out: 1 and 3 loop */
    CHECK(ec_e5_page_in_loop(&P, 0, 0, 3) && !ec_e5_page_in_loop(&P, 0, 1, 3) && ec_e5_page_in_loop(&P, 0, 2, 3),
          "loop = pages 1, 3");
    ec_seq_start(&S);
    while (n < 50) {
        const int st = ec_seq_step(&S, 0);
        if (st != last) {
            seen[n++] = st;
            last = st;
        }
        run(128);
    }
    for (i = 0; i < 50; i++) {
        const int k = i % 24, want = k < 16 ? k : 32 + (k - 16);
        if (seen[i] != want) {
            if (ok)
                printf("  position %d played %d, wanted %d\n", i, seen[i], want);
            ok = 0;
        }
    }
    CHECK(ok, "page loop order 1..16, 33..40, 1..");
    CHECK(S.tr[0].pass == 3, "pass counts the looped length (%d)", S.tr[0].pass);
    /* never empty; a full set is stored as 0 */
    ec_e5_toggle_page_loop(&P, 0, 0, 3);
    ec_e5_toggle_page_loop(&P, 0, 2, 3);
    CHECK(P.pe5[0].page_loop[0] == 4, "the last page cannot be dropped (%x)", P.pe5[0].page_loop[0]);
    ec_e5_toggle_page_loop(&P, 0, 0, 3);
    ec_e5_toggle_page_loop(&P, 0, 1, 3);
    CHECK(P.pe5[0].page_loop[0] == 0, "every page = 0");
}

/* ------------------------------------------------------------------------------------------- BPM --- */
static void test_pattern_bpm(void)
{
    setup();
    ec_seq_set_bpm(&S, 128);
    CHECK(P.pe5[0].bpm == 128, "the BPM is the pattern's");
    ec_pattern_prepare(&P, 1);
    CHECK(P.pe5[1].bpm == 128, "a new pattern takes the live BPM");
    P.pe5[1].bpm = 90;
    ec_seq_start(&S);
    ec_seq_request_pattern(&S, 1);
    P.quant = EC_Q_DIRECT_START;
    ec_seq_request_pattern(&S, 1);
    CHECK(P.current == 1 && S.bpm == 90 && P.bpm == 90, "pattern 2 plays at its 90 (%.1f)", S.bpm);
    ec_seq_request_pattern(&S, 0);
    CHECK(S.bpm == 128, "back to 128");
    P.e5.gbpm = 1;
    ec_seq_request_pattern(&S, 1);
    CHECK(S.bpm == 128, "global BPM on: the patterns' BPM is ignored");
}

/* ------------------------------------------------------------------------------------------- memorize --- */
static void test_memorize(void)
{
    ec_ptrack_t *tr;
    int i, restarted = 0;
    setup();
    tr = ec_project_track(&P, 0);
    for (i = 0; i < 16; i++)
        tr->steps[i].trig = 1;
    v0(0)[EC_P_FILTER_CUTOFF] = 90;
    ec_scene_set(&P, 1, 0, EC_P_PAN, 0);
    ec_scene_apply(&P, 1.0f);
    ec_seq_start(&S);
    run(44100);
    M = P;                                            /* memorize */
    v0(0)[EC_P_FILTER_CUTOFF] = 10;
    tr->steps[3].trig = 0;
    ec_scene_apply(&P, 0.0f);
    snprintf(P.pe5[0].name, EC_NAME, "CHANGED");
    run(44100);
    CHECK(S.tr[0].play_index != 0, "playing mid-pattern");
    ec_e5_recall(&P, &M);
    ec_seq_reload(&S);
    for (i = 0; i < nrec; i++)
        if (R[i].ev.type == EC_EV_TRANSPORT && (R[i].ev.flags & EC_TF_RESET) && R[i].at == S.now)
            restarted = 1;
    CHECK(S.playing && S.tr[0].play_index == 0 && restarted, "recall during playback: still playing, step 1");
    CHECK(v0(0)[EC_P_FILTER_CUTOFF] == 90 && tr->steps[3].trig && P.pe5[0].name[0] == 0, "the project came back");
    CHECK(P.e5.sc.pos == 1.0f && val(0, EC_P_PAN) == 0, "the scenes came back, applied");
    run(44100);
    CHECK(S.tr[0].fired > 0 && S.playing, "plays on");
}

/* ------------------------------------------------------------------------------------------- undo --- */
static void test_undo(void)
{
    static ec_undo_t u;
    int t, gid, kind, i;
    float gv;
    char lb[16];
    setup();
    ec_undo_clear(&u);
    /* one gesture (the same param inside the window) = one entry */
    ec_undo_param(&u, &P, 0, EC_P_FILTER_CUTOFF, 1000);
    v0(0)[EC_P_FILTER_CUTOFF] = 90;
    ec_undo_param(&u, &P, 0, EC_P_FILTER_CUTOFF, 1300);
    v0(0)[EC_P_FILTER_CUTOFF] = 20;
    CHECK(u.n == 1, "coalesced: %d entries", u.n);
    ec_undo_param(&u, &P, 0, EC_P_FILTER_CUTOFF, 2500);   /* a pause starts a new gesture */
    v0(0)[EC_P_FILTER_CUTOFF] = 60;
    CHECK(u.n == 2, "after a pause: %d", u.n);
    kind = ec_undo_pop(&u, &P, &t, 0, 0, lb, sizeof lb);
    CHECK(kind == EC_U_PARAM && v0(0)[EC_P_FILTER_CUTOFF] == 20 && !strcmp(lb, "CUT"), "undo 1: back to 20");
    ec_undo_pop(&u, &P, &t, 0, 0, lb, sizeof lb);
    CHECK(v0(0)[EC_P_FILTER_CUTOFF] == 127, "undo 2: back to the start (127)");
    CHECK(ec_undo_pop(&u, &P, &t, 0, 0, lb, sizeof lb) == 0, "nothing left");
    /* steps */
    ec_project_track(&P, 0)->steps[2].trig = 1;
    ec_undo_steps(&u, &P, 0, 0, 1u << 2, "STEP", 7, 5000);
    ec_project_track(&P, 0)->steps[2].trig = 0;
    ec_undo_pop(&u, &P, &t, 0, 0, lb, sizeof lb);
    CHECK(ec_project_track(&P, 0)->steps[2].trig == 1 && t == 0, "a step toggle undone");
    /* the crossfader: one move = one entry, the position comes back */
    ec_scene_set(&P, 1, 0, EC_P_PAN, 0);
    ec_scene_apply(&P, 0.2f);
    ec_undo_xfade(&u, &P, 6000);
    ec_scene_apply(&P, 0.5f);
    ec_undo_xfade(&u, &P, 6030);
    ec_scene_apply(&P, 0.9f);
    CHECK(u.n == 1, "a glide is one entry");
    ec_undo_pop(&u, &P, 0, 0, 0, lb, sizeof lb);
    CHECK(NEAR(P.e5.sc.pos, 0.2f) && NEAR(val(0, EC_P_PAN), 64 - 64 * 0.2f), "the fader came back");
    /* a global (the app applies it) */
    ec_undo_global(&u, 0, 133, "BPM", 7000);
    kind = ec_undo_pop(&u, &P, 0, &gid, &gv, lb, sizeof lb);
    CHECK(kind == EC_U_GLOBAL && gid == 0 && gv == 133, "global entry");
    /* 16 deep */
    for (i = 0; i < 20; i++)
        ec_undo_param(&u, &P, 0, EC_P_PAN, 10000 + (uint32_t)i * 2000);
    CHECK(u.n == EC_UNDO_DEPTH, "depth %d", u.n);
}

/* ------------------------------------------------------------------------------------------- slices --- */
static void test_slice_programs(void)
{
    ec_ptrack_t *tr;
    ec_slicemap_t *m;
    const rec_t *e[4];
    int i, k = 0;
    setup();
    tr = ec_project_track(&P, 0);
    tr->v[EC_P_MACHINE] = EC_MACH_SLICE_POLY;
    tr->v[EC_P_SLICE_COUNT] = 8;
    tr->steps[0].trig = 1;
    tr->steps[0].slices = (1u << 2) | (1u << 4);    /* slices 3 and 5 */
    m = ec_e5_slices(&P, 0);
    ec_slice_lock_set(m, 3, EC_P_PITCH, 7);
    ec_slice_lock_set(m, 3, EC_P_VELOCITY, 0.5f);
    ec_slice_lock_set(m, 3, EC_P_FILTER_CUTOFF, 30);
    m->choke[2] = 2;
    m->mute = 1u << 4;                               /* slice 5 muted */
    ec_seq_start(&S);
    run(64);
    for (i = 0; i < nrec && k < 4; i++)
        if (R[i].ev.type == EC_EV_TRIG && R[i].ev.track == 0)
            e[k++] = &R[i];
    CHECK(k == 1 && e[0]->ev.slice == 3, "the muted slice is skipped (%d trigs)", k);
    CHECK(k >= 1 && e[0]->ev.pitch == 7 && e[0]->ev.velocity == 0.5f && e[0]->ev.choke == 2,
          "the slice's pitch / velocity / choke");
    CHECK(val(0, EC_P_FILTER_CUTOFF) == 30, "the slice's track-level lock holds for the note");
    /* the step's own lock wins */
    ec_step_lock_set(&tr->steps[1], EC_P_PITCH, -3);
    tr->steps[1].trig = 1;
    tr->steps[1].slices = 1u << 2;
    run(6000);
    for (i = 0, k = 0; i < nrec; i++)
        if (R[i].ev.type == EC_EV_TRIG && R[i].ev.track == 0)
            e[k < 3 ? k++ : 3] = &R[i];
    CHECK(k >= 2 && e[1]->ev.pitch == -3, "the step's lock over the slice's");
    /* pads: mute and choke */
    nrec = 0;
    ec_seq_slice_pad(&S, 0, 5, 1, 1.0f);
    ec_seq_slice_pad(&S, 0, 3, 1, 1.0f);
    for (i = 0, k = 0; i < nrec; i++)
        if (R[i].ev.type == EC_EV_SLICE_ON)
            e[k < 3 ? k++ : 3] = &R[i];
    CHECK(k == 1 && e[0]->ev.slice == 3 && e[0]->ev.choke == 2 && e[0]->ev.pitch == 7, "pads: muted silent, lock");
}

/* ------------------------------------------------------------------------------------------- the record --- */
static uint8_t buf[1 << 21], buf2[1 << 21];

static void test_record(void)
{
    int32_t n;
    int i;
    ec_project_init(&P);
    snprintf(P.e5.name, EC_NAME, "CRISPY ROCKET");
    P.e5.gbpm = 1;
    ec_pattern_prepare(&P, 2);
    snprintf(P.pe5[2].name, EC_NAME, "BREAK");
    P.pe5[2].bpm = 94.5f;
    P.pe5[2].page_loop[3] = 0x5;
    ec_slice_lock_set(&P.pe5[2].slices[1], 4, EC_P_PITCH, -2);
    ec_slice_lock_set(&P.pe5[2].slices[1], 4, EC_P_FILTER_CUTOFF, 64);
    P.pe5[2].slices[1].choke[3] = 3;
    P.pe5[2].slices[1].mute = 0x80000001u;
    P.pat[0].tr[0].v[EC_P_FILTER_CUTOFF] = 99;
    ec_scene_capture(&P, 0);
    ec_scene_set(&P, 1, 4, EC_P_PAN, 3);
    ec_scene_apply(&P, 0.375f);
    n = ec_project_save(&P, buf, sizeof buf);
    CHECK(n > 0, "saved %d bytes", (int)n);
    memset(&Q, 0x33, sizeof Q);
    CHECK(ec_project_load(&Q, buf, (uint32_t)n) == 0, "loaded");
    CHECK(!strcmp(Q.e5.name, "CRISPY ROCKET") && Q.e5.gbpm == 1 && Q.e5.sc.pos == 0.375f, "project E5 fields");
    CHECK(!memcmp(Q.e5.sc.has, P.e5.sc.has, sizeof P.e5.sc.has) && !memcmp(Q.e5.sc.val, P.e5.sc.val, sizeof P.e5.sc.val),
          "scenes");
    for (i = 0; i < EC_NPAT; i++)
        if (P.pat[i].used)
            CHECK(!memcmp(&Q.pe5[i], &P.pe5[i], sizeof P.pe5[i]), "pattern %d E5 data", i + 1);
    CHECK(!memcmp(&Q.pat[2], &P.pat[2], sizeof P.pat[2]), "pattern content unchanged");
    /* a truncated chunk does not take the rest down with it */
    CHECK(ec_e5_chunk_load(&Q, buf + 16, 3) == -1, "a short chunk is refused");
    printf("  record with the E5 chunk: %d bytes\n", (int)n);
}

static void test_e2_save(const char *path)
{
    FILE *f = path ? fopen(path, "rb") : 0;
    size_t n;
    uint32_t len_e2, len;
    int32_t m;
    int i, used = 0;
    if (!f) {
        CHECK(0, "no E2 fixture (%s)", path ? path : "-");
        return;
    }
    n = fread(buf2, 1, sizeof buf2, f);
    fclose(f);
    CHECK(n > 4096 + 16, "fixture size");
    memset(&P, 0x77, sizeof P);
    CHECK(ec_project_load(&P, buf2 + 4096, (uint32_t)(n - 4096)) == 0, "an E2 save loads");
    for (i = 0; i < EC_NPAT; i++)
        used += P.pat[i].used;
    CHECK(used == 2 && P.slot[3].path[0], "its patterns (%d) and samples", used);
    for (i = 0; i < EC_NPAT; i++)
        CHECK(P.pe5[i].bpm == P.bpm && P.pe5[i].name[0] == 0 && P.pe5[i].page_loop[0] == 0, "E5 defaults, pattern %d",
              i + 1);
    CHECK(ec_scene_count(&P.e5.sc, 0) == 0 && ec_scene_count(&P.e5.sc, 1) == 0 && !P.e5.gbpm && P.e5.name[0] == 0,
          "no scenes, per-pattern BPM");
    /* saved again (0.2.0 writes every param id, E3 + E4 included, then the tagged chunks): it reloads to the same
     * project, saves to the same bytes, and carries the E5 chunk */
    m = ec_project_save(&P, buf, sizeof buf);
    memcpy(&len_e2, buf2 + 4096 + 8, 4);
    memcpy(&len, buf + 8, 4);
    CHECK(m > 0 && len > len_e2, "the resave is larger (new params, chunks)");
    CHECK(ec_project_load(&Q, buf, (uint32_t)m) == 0, "the resave loads");
    for (i = 0; i < EC_NPAT; i++)
        CHECK(!memcmp(&Q.pat[i], &P.pat[i], sizeof P.pat[i]), "pattern %d unchanged across the resave", i + 1);
    CHECK(!memcmp(Q.slot, P.slot, sizeof P.slot) && Q.bpm == P.bpm && Q.current == P.current, "slots, globals");
    {
        const uint32_t tag = EC_CHUNK_E5;
        int32_t at, found = 0;
        for (at = 16; at + 4 <= m; at++)
            if (!memcmp(buf + at, &tag, 4)) {
                found = 1;
                break;
            }
        CHECK(found && ec_project_save(&Q, buf2, sizeof buf2) == m && !memcmp(buf, buf2, (size_t)m),
              "then the E5 chunk; the reload saves byte for byte");
    }
}

int main(int argc, char **argv)
{
    ec_layout_use(&EC_LAYOUT_TEST);   /* the tests' fixed layout: 0.4.0's pages (host/ec_layout_test.h) */
    test_scene_morph();
    test_glide();
    test_clipboards();
    test_page_loop();
    test_pattern_bpm();
    test_memorize();
    test_undo();
    test_slice_programs();
    test_record();
    test_e2_save(argc > 1 ? argv[1] : 0);
    printf("test_e5: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
