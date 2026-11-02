/* SPDX-License-Identifier: GPL-3.0-only */
/* test_e6: audio input and sampling (E6) through the core descriptor and the stand-in input (fm1core_audio_in before
 * each audio block, as a host calls rfm1_audio_in): the INPUT machine's alignment (a click in the input at the same
 * output frame), no input = silence, the REC pop-up's gestures and live labels, a take synced on PLAY of a click
 * track looped back through the input with a latency (it lands on frame 0), a resample of the master, the threshold
 * arm, the WAV written to /data/rec over the file API and the reboot that reloads it.
 *   ec_test_e6 <folder>   (a fresh folder: the demo files + click.wav are written into it) */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ec_app.h"
#include "ec_fs_native.h"
#include "fm1core.h"
#include "ec_layout_test.h"

uint32_t fm1core_labels_epoch(void);
const char *fm1core_label(int32_t which);

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

#define A (&ec_app)
#define MAXF (44100u * 40u)
static const fm1core_t *C;
static fm1panel_t *P;
static uint32_t ms;
static uint64_t frames;                 /* rendered since this boot (= the sequencer's sample time) */
static int16_t outrec[MAXF * 2];        /* every output frame since this boot */
static int16_t inbuf[MAXF * 2];         /* IN_BUFFER: the input by frame */
enum { IN_NONE, IN_BUFFER, IN_LOOP };
static int in_mode, in_force;           /* in_force: call fm1core_audio_in even with the state 0 */
static uint32_t loop_d;                 /* IN_LOOP: the output comes back this many frames later */
static const char *root;

static void audio_block(void)
{
    int16_t in[FM1P_BLOCK * 2], out[FM1P_BLOCK * 2];
    uint32_t i;
    if (in_mode != IN_NONE && (ec_plat_ain_state() || in_force)) {
        for (i = 0; i < FM1P_BLOCK; i++) {
            const uint64_t f = frames + i;
            in[2 * i] = in[2 * i + 1] = 0;
            if (in_mode == IN_BUFFER && f < MAXF) {
                in[2 * i] = inbuf[2 * f];
                in[2 * i + 1] = inbuf[2 * f + 1];
            } else if (in_mode == IN_LOOP && f >= loop_d && f - loop_d < MAXF) {
                in[2 * i] = outrec[2 * (f - loop_d)];
                in[2 * i + 1] = outrec[2 * (f - loop_d) + 1];
            }
        }
        fm1core_audio_in(in, FM1P_BLOCK);
    }
    C->audio(out, FM1P_BLOCK);
    if (frames + FM1P_BLOCK <= MAXF)
        memcpy(outrec + 2 * frames, out, sizeof out);
    frames += FM1P_BLOCK;
}

static void run(int n)
{
    int i;
    for (i = 0; i < n; i++) {
        ec_fs_native_pump();
        C->tick(ms);
        if (ms % 15 == 0)
            C->frame();
        else
            C->idle();
        ms++;
        while (frames + FM1P_BLOCK <= (uint64_t)ms * FM1P_FS / 1000u)
            audio_block();
    }
}
static void btn(int b, int down)
{
    const uint32_t m = 1u << P->btn_id[b];
    if (down) {
        P->buttons |= m;
        P->buttons_tap |= m;
    } else
        P->buttons &= ~m;
}
static void press(int b, int dur)
{
    btn(b, 1);
    run(dur);
    btn(b, 0);
    run(20);
}
static void turn(int role, int n)
{
    P->enc[P->enc_id[role]] += n * P->enc_dir[role];
    run(2);
}
static const char *L(int which)
{
    const char *s = fm1core_label(which);
    return s ? s : "(null)";
}
static int lbl(int which, const char *want) { return !strcmp(L(which), want); }
static float *tvp(int t) { return ec_project_track(&A->proj, t)->v; }
static void info(char *b, size_t n) { C->ui_info(b, (uint32_t)n); }

static void boot(const uint8_t *flash, uint32_t n)
{
    C = fm1core_get(FM1CORE_ABI);
    P = C->panel;
    C->flash_stage(flash, n);
    C->options(FM1CORE_FLASH_STAGED, 0, 0, 1);
    P->ready = 2;
    ms = 0;
    frames = 0;
    memset(outrec, 0, sizeof outrec);
    C->init(0);
}

static void write_click(const char *path)
{
    /* 0.5 s: a click at frame 0 (0.9, decaying over ~2 ms), then silence */
    const uint32_t n = 22050;
    FILE *f = fopen(path, "wb");
    uint8_t h[44];
    uint32_t i;
    if (!f)
        return;
    memcpy(h, "RIFF", 4);
    h[4] = (uint8_t)(36 + n * 4), h[5] = (uint8_t)((36 + n * 4) >> 8), h[6] = (uint8_t)((36 + n * 4) >> 16), h[7] = 0;
    memcpy(h + 8, "WAVEfmt \x10\0\0\0\x01\0\x02\0\x44\xac\0\0\x10\xb1\x02\0\x04\0\x10\0data", 32);
    h[40] = (uint8_t)(n * 4), h[41] = (uint8_t)((n * 4) >> 8), h[42] = (uint8_t)((n * 4) >> 16), h[43] = 0;
    fwrite(h, 1, 44, f);
    for (i = 0; i < n; i++) {
        const int16_t v = (int16_t)(i < 100 ? 0.9 * 32767.0 * exp(-(double)i / 20.0) * (i % 2 ? -1 : 1) : 0);
        fwrite(&v, 2, 1, f);
        fwrite(&v, 2, 1, f);
    }
    fclose(f);
}

static uint32_t first_over(const int16_t *d, uint32_t n, float frac_of_peak)
{
    uint32_t i, pk = 0;
    for (i = 0; i < n; i++)
        pk = (uint32_t)abs(d[2 * i]) > pk ? (uint32_t)abs(d[2 * i]) : pk;
    for (i = 0; i < n; i++)
        if ((float)abs(d[2 * i]) >= frac_of_peak * (float)pk && pk > 0)
            return i;
    return UINT32_MAX;
}

/* ------------------------------------------------------------------------------------------- INPUT machine --- */
static void test_input_machine(void)
{
    const uint64_t at = 44100u * 3u + 777u;      /* a click in the input at this frame */
    uint32_t pk_i = 0, i;
    int pk = 0;
    char b[2048];
    fm1core_audio_in_info(1, 256);
    in_mode = IN_BUFFER;
    memset(inbuf, 0, sizeof inbuf);
    inbuf[2 * at] = inbuf[2 * at + 1] = 20000;
    /* T1 on INPUT through the panel: SRC setup (SHIFT + SRC), K1 = MACHINE, LOOP -> .. -> INPUT (T1 skips NEIGH) */
    btn(FM1P_B_GLO, 1);
    run(20);
    press(FM1P_B_SEL, 40);
    btn(FM1P_B_GLO, 0);
    run(20);
    CHECK(A->settings && A->cat == EC_CAT_SRC, "SRC setup");
    turn(FM1P_E_K1, 5);
    CHECK((int)tvp(0)[EC_P_MACHINE] == EC_MACH_RAZOR_POLY, "K1 +5: R.POLY (%g)", tvp(0)[EC_P_MACHINE]);
    turn(FM1P_E_K1, 1);
    CHECK((int)tvp(0)[EC_P_MACHINE] == EC_MACH_INPUT, "T1: +1 past R.POLY = INPUT (no NEIGH on T1) (%g)",
          tvp(0)[EC_P_MACHINE]);
    press(FM1P_B_HOME, 40);
    run(3200 - (int)ms > 0 ? 3200 - (int)ms : 0);
    run(300);
    for (i = (uint32_t)at - 2000u; i < (uint32_t)at + 2000u; i++)
        if (abs(outrec[2 * i]) > pk) {
            pk = abs(outrec[2 * i]);
            pk_i = i;
        }
    CHECK(pk > 1000 && pk_i >= at && pk_i <= at + 3, "the input's click out at frame %u (in at %llu, peak %d)", pk_i,
          (unsigned long long)at, pk);
    info(b, sizeof b);
    CHECK(strstr(b, "ain:1") && strstr(b, "lat:256"), "ui_info: ain:1 lat:256");
    /* the input off (state 0): silence even if a host still sent blocks, and NO INPUT */
    fm1core_audio_in_info(0, 0);
    in_force = 1;
    for (i = 0; i < 44100u; i++)
        inbuf[2 * (at + 22050u + i)] = inbuf[2 * (at + 22050u + i) + 1] = (int16_t)(i % 100 < 50 ? 12000 : -12000);
    {
        const uint64_t f0 = frames;
        run(1000);
        pk = 0;
        for (i = (uint32_t)f0 + 4410u; i < (uint32_t)frames; i++)
            pk = abs(outrec[2 * i]) > pk ? abs(outrec[2 * i]) : pk;
        CHECK(pk == 0, "state 0: the INPUT track is silent (peak %d)", pk);
    }
    info(b, sizeof b);
    CHECK(strstr(b, "ain:0") != NULL, "ui_info: ain:0");
    in_force = 0;
    fm1core_audio_in_info(1, 256);
    {   /* back on: the square wave is heard again */
        const uint64_t f0 = frames;
        run(300);
        pk = 0;
        for (i = (uint32_t)f0 + 512u; i < (uint32_t)frames; i++)
            pk = abs(outrec[2 * i]) > pk ? abs(outrec[2 * i]) : pk;
        CHECK(pk > 1000, "state 1 again: heard (peak %d)", pk);
    }
    tvp(0)[EC_P_MACHINE] = EC_MACH_LOOP;
    ec_seq_touch(&A->seq, 0);
    in_mode = IN_NONE;
}

/* ------------------------------------------------------------------------------------------- the pop-up --- */
static void test_popup(void)
{
    press(FM1P_B_HOME, 40);
    CHECK(lbl(FM1CORE_LBL_BTN0 + FM1P_B_REC, "SAMPLING"), "REC's label closed: %s", L(FM1CORE_LBL_BTN0 + FM1P_B_REC));
    press(FM1P_B_REC, 40);
    CHECK(A->rec.open && ec_app_popup(A), "REC: the pop-up");
    CHECK(lbl(FM1CORE_LBL_BTN0 + FM1P_B_OCTUP, "ARM") && lbl(FM1CORE_LBL_BTN0 + FM1P_B_OCTDN, "CLOSE") &&
              lbl(FM1CORE_LBL_BTN0 + FM1P_B_REC, "ARM") && lbl(FM1CORE_LBL_KNOB0 + FM1P_E_K2, "LENGTH") &&
              lbl(FM1CORE_LBL_KNOB0 + FM1P_E_SELECT, "SLOT"),
          "labels open: OCT+ %s OCT- %s REC %s K2 %s SELECT %s", L(FM1CORE_LBL_BTN0 + FM1P_B_OCTUP),
          L(FM1CORE_LBL_BTN0 + FM1P_B_OCTDN), L(FM1CORE_LBL_BTN0 + FM1P_B_REC), L(FM1CORE_LBL_KNOB0 + FM1P_E_K2),
          L(FM1CORE_LBL_KNOB0 + FM1P_E_SELECT));
    turn(FM1P_E_K2, -2);
    CHECK(A->rec.len == 1, "K2: LEN 1 BAR (%d)", A->rec.len);
    press(FM1P_B_OCTUP, 40);
    CHECK(A->rec.state == EC_RS_ARMED && A->rec.handle >= 0, "OCT+: armed");
    CHECK(lbl(FM1CORE_LBL_BTN0 + FM1P_B_OCTUP, "REC NOW") && lbl(FM1CORE_LBL_BTN0 + FM1P_B_OCTDN, "DISARM"),
          "labels armed: %s / %s", L(FM1CORE_LBL_BTN0 + FM1P_B_OCTUP), L(FM1CORE_LBL_BTN0 + FM1P_B_OCTDN));
    {
        const uint32_t fr = ec_ss_free_frames();
        press(FM1P_B_OCTDN, 40);
        CHECK(A->rec.state == EC_RS_IDLE && A->rec.open && ec_ss_free_frames() > fr, "OCT-: disarmed, the reserve back");
    }
    press(FM1P_B_OCTDN, 40);
    CHECK(!A->rec.open, "OCT- idle: closed");
    press(FM1P_B_REC, 40);
    press(FM1P_B_HOME, 40);
    CHECK(!A->rec.open && A->cat == EC_CAT_SRC, "HOME closes it");
}

/* ------------------------------------------------------------------------------------------- synced take --- */
static uint64_t play_base;               /* the sample time of step 1 (the PLAY that started the take) */
static int take_check(const char *what, uint32_t want_frames, uint32_t tol)
{
    const ec_ss_sample_t *s = ec_app_track_sample(A, 0);
    /* the reference: where the click track's step 1 is in the output itself (from step 1's sample on) */
    const uint32_t ref = first_over(outrec + 2 * play_base, want_frames, 0.25f);
    uint32_t i0;
    CHECK(s && s->frames == want_frames, "%s: the take in T1's slot, %u frames (want %u)", what, s ? s->frames : 0,
          want_frames);
    if (!s)
        return 0;
    i0 = first_over(s->data, s->frames, 0.25f);
    CHECK(ref < 1000 && i0 <= ref + tol && i0 + tol >= ref,
          "%s: the click on frame %u of the take, as on step 1's frame %u of the output (+-%u)", what, i0, ref, tol);
    {   /* and the next bar's? (1 bar: none) the take holds one click only */
        uint32_t i, n = 0;
        for (i = 4410; i < s->frames; i++)
            n += abs(s->data[2 * i]) > 8000;
        CHECK(n == 0, "%s: nothing loud after the first click (%u frames)", what, n);
    }
    return 1;
}

static void click_track(void)
{
    /* T2 plays click.wav (slot 2) as LOOP_TRIG on step 1; T1 muted (nothing else sounds) */
    float *v2 = tvp(1);
    snprintf(A->proj.slot[1].path, EC_PATH, "/audio/click.wav");
    A->slot[1].state = EC_SL_QUEUED;
    A->slot[1].newmeta = 1;
    v2[EC_P_SLOT] = 2;
    v2[EC_P_MACHINE] = EC_MACH_LOOP_TRIG;
    ec_project_track(&A->proj, 1)->steps[0].trig = 1;
    tvp(0)[EC_P_MUTE] = 1;
    ec_seq_touch_all(&A->seq);
    run(800);
    CHECK(A->slot[1].state == EC_SL_READY, "click.wav loaded (%d)", A->slot[1].state);
}

static void test_synced_take(void)
{
    char b[2048];
    const uint32_t lat = 300, bar = 88200;        /* 120 BPM: a bar = 2 s */
    click_track();
    in_mode = IN_LOOP;
    loop_d = lat;                                  /* the round trip, as the web player reports it */
    fm1core_audio_in_info(1, lat);
    press(FM1P_B_REC, 40);
    CHECK(A->rec.src == EC_RSRC_IN && A->rec.sync == EC_RSYNC_PLAY && A->rec.len == 1, "IN, PLAY, 1 BAR");
    press(FM1P_B_OCTUP, 40);
    CHECK(A->rec.state == EC_RS_ARMED, "armed");
    run(200);
    CHECK(A->rec.state == EC_RS_ARMED, "still armed while stopped");
    press(FM1P_B_PLAY, 40);
    play_base = A->seq.sample_base;
    run(100);
    CHECK(A->rec.state == EC_RS_RUN && A->rec.shift == lat, "PLAY: the take runs, shift %u",
          A->rec.shift);
    CHECK(lbl(FM1CORE_LBL_BTN0 + FM1P_B_OCTUP, "STOP") && lbl(FM1CORE_LBL_BTN0 + FM1P_B_OCTDN, "DISCARD"),
          "labels taking: %s / %s", L(FM1CORE_LBL_BTN0 + FM1P_B_OCTUP), L(FM1CORE_LBL_BTN0 + FM1P_B_OCTDN));
    run(2300);
    CHECK(A->rec.state == EC_RS_IDLE && A->rec.takes == 1, "the bar is in: landed (%s, takes %u)",
          ec_rec_state_name(), A->rec.takes);
    take_check("synced input take", bar, 2);
    press(FM1P_B_PLAY, 40);                        /* stop */
    info(b, sizeof b);
    CHECK(strstr(b, "rshift:300") != NULL, "ui_info rshift:300");
    in_mode = IN_NONE;
}

static void test_wav_and_reboot(void)
{
    char path[1024], b[2048];
    const ec_ss_sample_t *s = ec_app_track_sample(A, 0);
    int16_t *take = NULL;
    uint32_t n = s ? s->frames : 0;
    FILE *f;
    long sz = 0;
    int i;
    for (i = 0; i < 400 && A->rec.saved < 1; i++)
        run(10);
    CHECK(A->rec.saved == 1 && !A->rec.w, "the WAV written (saved %d, fail %d %s)", A->rec.saved, A->rec.wfail,
          A->rec.werr);
    CHECK(!strcmp(A->proj.slot[0].path, "/data/rec/rec-0001.wav"), "slot 1's path %s", A->proj.slot[0].path);
    snprintf(path, sizeof path, "%s/data/elasticat/rec/rec-0001.wav", root);
    f = fopen(path, "rb");
    CHECK(f != NULL, "%s exists", path);
    if (f) {
        fseek(f, 0, SEEK_END);
        sz = ftell(f);
        CHECK(sz == 44 + (long)n * 4, "its size %ld (want %ld)", sz, 44 + (long)n * 4);
        if (s && sz == 44 + (long)n * 4) {
            uint8_t *d = malloc((size_t)sz);
            fseek(f, 0, SEEK_SET);
            if (d && fread(d, 1, (size_t)sz, f) == (size_t)sz)
                CHECK(!memcmp(d, "RIFF", 4) && !memcmp(d + 8, "WAVEfmt ", 8) && !memcmp(d + 36, "data", 4) &&
                          !memcmp(d + 44, s->data, (size_t)n * 4),
                      "the WAV's header and frames = the take");
            free(d);
        }
        fclose(f);
    }
    if (s) {
        take = malloc((size_t)n * 4);
        memcpy(take, s->data, (size_t)n * 4);
    }
    /* SHIFT + REC: save the project, then reboot from the flash */
    btn(FM1P_B_GLO, 1);
    run(20);
    press(FM1P_B_REC, 40);
    btn(FM1P_B_GLO, 0);
    run(20);
    {
        const uint32_t fs = C->flash_size;
        uint8_t *img = malloc(fs);
        memcpy(img, C->flash(), fs);
        C->shutdown();
        boot(img, fs);
        free(img);
    }
    run(3000);
    s = ec_app_track_sample(A, 0);
    CHECK(s && s->frames == n && take && !memcmp(s->data, take, (size_t)n * 4) && A->slot[0].state == EC_SL_READY,
          "the reboot reloads /data/rec/rec-0001.wav into slot 1, frame for frame (%u frames)", s ? s->frames : 0);
    info(b, sizeof b);
    CHECK(strstr(b, "file:rec-0001.wav") != NULL, "T1 plays rec-0001.wav");
    free(take);
}

static void dbg(const char *w)
{
    char b[2048];
    info(b, sizeof b);
    if (getenv("E6_DEBUG"))
        printf("[%s] ms %u: %s\n", w, ms, b);
}

static void test_resample(void)
{
    /* the master out: SRC MST, synced on PLAY, no shift: the click track's step 1 on frame 0 */
    click_track();
    dbg("resample start");
    press(FM1P_B_REC, 40);
    turn(FM1P_E_K1, 1);
    dbg("resample K1");
    turn(FM1P_E_K2, -2);
    CHECK(A->rec.src == EC_RSRC_MST && A->rec.len == 1, "K1: MST, 1 BAR");
    press(FM1P_B_OCTUP, 40);
    press(FM1P_B_PLAY, 40);
    play_base = A->seq.sample_base;
    run(2400);
    CHECK(A->rec.takes == 1 && A->rec.shift == 0, "the resample landed (takes %u, shift %u)", A->rec.takes,
          A->rec.shift);
    take_check("resample", 88200, 0);
    press(FM1P_B_PLAY, 40);
    for (int i = 0; i < 400 && A->rec.w; i++)
        run(10);
    CHECK(!strcmp(A->proj.slot[0].path, "/data/rec/rec-0002.wav"), "the next name: %s", A->proj.slot[0].path);
}

static void test_threshold(void)
{
    /* FREE + THR -24 dB: silence, then a burst at a known frame; the take starts on its first frame over */
    uint64_t at;
    uint32_t i;
    tvp(1)[EC_P_MUTE] = 1;                        /* (the click track quiet) */
    ec_seq_touch_all(&A->seq);
    memset(inbuf, 0, sizeof inbuf);
    in_mode = IN_BUFFER;
    fm1core_audio_in_info(1, 0);
    CHECK(A->rec.open, "(the pop-up stays open after a take)");
    press(FM1P_B_HOME, 40);
    press(FM1P_B_REC, 40);
    turn(FM1P_E_K1, -1);
    turn(FM1P_E_K4, 2);
    turn(FM1P_E_K3, 7);
    CHECK(A->rec.src == EC_RSRC_IN && A->rec.sync == EC_RSYNC_FREE && ec_rec_thr_db(A->rec.thr) == -24,
          "IN, FREE, THR %d dB", ec_rec_thr_db(A->rec.thr));
    run(20);
    CHECK(lbl(FM1CORE_LBL_KNOB0 + FM1P_E_K3, "THRESHOLD"), "K3's label: %s", L(FM1CORE_LBL_KNOB0 + FM1P_E_K3));
    at = frames + 44100u + 4321u;
    for (i = 0; i < 1000; i++) {                  /* a quiet hiss under the threshold (-40 dB), then the burst */
        inbuf[2 * (at - 1000 + i)] = (int16_t)(i % 2 ? 300 : -300);
        inbuf[2 * (at - 1000 + i) + 1] = 0;
    }
    for (i = 0; i < 4410; i++)
        inbuf[2 * (at + i)] = inbuf[2 * (at + i) + 1] = (int16_t)(i % 40 < 20 ? 16000 : -16000);
    press(FM1P_B_OCTUP, 40);
    run(500);
    CHECK(A->rec.state == EC_RS_ARMED, "armed, waiting under the threshold");
    run(800);
    CHECK(A->rec.state == EC_RS_RUN && A->rec.start_at == at, "the burst started it at its first frame (%llu, want %llu)",
          (unsigned long long)A->rec.start_at, (unsigned long long)at);
    run(2300);
    {
        const ec_ss_sample_t *s = ec_app_track_sample(A, 0);
        CHECK(A->rec.takes == 2 && s && s->data[0] == 16000 && s->data[2 * 20] == -16000 && s->data[2 * 40] == 16000,
              "the take begins with the burst (%d %d)", s ? s->data[0] : 0, s ? s->data[2 * 20] : 0);
    }
    for (int k = 0; k < 400 && A->rec.w; k++)
        run(10);
    in_mode = IN_NONE;
}

int main(int argc, char **argv)
{
    char cmd[2048], p[1024];
    ec_layout_use(&EC_LAYOUT_TEST);   /* the tests' fixed layout: 0.4.0's pages (host/ec_layout_test.h) */
    if (argc < 2) {
        printf("usage: ec_test_e6 <folder>\n");
        return 2;
    }
    root = argv[1];
    snprintf(p, sizeof p, "%s/audio/click.wav", root);
    write_click(p);
    snprintf(cmd, sizeof cmd, "rm -f %s/data/elasticat/rec/rec-*.wav", root);   /* (a fresh run: names from 0001) */
    if (system(cmd) != 0)
        return 2;
    ec_fs_native_root(root, "elasticat");
    boot(NULL, 0);
    run(1500);
    test_input_machine();
    test_popup();
    test_synced_take();
    test_wav_and_reboot();
    test_resample();
    test_threshold();
    C->shutdown();
    printf("test_e6: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
