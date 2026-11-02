/* SPDX-License-Identifier: GPL-3.0-only */
/* E0 ONLY ("the pipe", PLAN.md milestones): a temporary app that proves the platform end to end. It lists /audio,
 * loads the first WAV / AIFF it finds, loops it at its native speed when PLAY is pressed, takes its volume from the
 * MASTER pot, chases the playhead over the lower keys, and echoes every input on the screen. SELECT moves the file
 * cursor, FILE (SAVE) loads the file under it. With no folder (or no audio files) a built-in synthesized loop plays.
 * Replaced by the sequencer's app (src/seq) from E1 on; the loop player here is not the DSP engine (src/dsp). */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ec_browse.h"
#include "ec_fs.h"
#include "ec_gfx.h"
#include "ec_leds.h"
#include "ec_loader.h"
#include "ec_plat.h"
#include "ec_samplestore.h"
#include "ec_screen_e0.h"

static const char *const BTN_FN[EC_NB] = {"FX", "SRC", "AMP", "MOD", "FILT", "MST", "FN", "FILE", "PAT", "TRIG",
                                          "PLAY", "REC", "OCT-", "OCT+"};
static const char *const ENC_FN[EC_NE] = {"SEL", "TRK", "PAT", "K1", "K2", "K3", "K4"};
static const char *const UPPER_FN[EC_NUPPER] = {"T1", "T2", "T3", "T4", "T5", "T6", "S1", "S2", "MST", "FILL", "PATN"};

static struct {
    int scanning;              /* the /audio scan runs */
    int32_t changed;           /* the host's folder counter when we last scanned */
    int no_folder;             /* the scan failed (no folder, not allowed) */
    int cursor;                /* the browser's cursor */
    int loaded_idx;            /* the browser index of the loaded file, -1 none / built-in */
    int want_first;            /* load the first file when the scan ends */
    int builtin;               /* the built-in loop is the loaded sample */
    char loaded_path[EC_SS_PATH];
    int play_h;                /* the store handle the player reads, -1 none */
    int playing;
    double pos;                /* the player's position in frames */
    float gain, env;           /* the smoothed volume, the declick envelope */
    int32_t master;
    char last[40];
    int dirty;
    uint32_t last_draw;
    uint32_t frames_out;
} E;

/* ------------------------------------------------------------------------------- the built-in loop --- */
/* one bar at 120 BPM (2 s): kick, snare on 2 and 4, eighth hats; synthesized at power-on into the store */
static int make_builtin(void)
{
    const uint32_t n = 88200, beat = n / 4, eighth = n / 8;
    const int h = ec_ss_alloc(n);
    ec_ss_sample_t *s = ec_ss_get(h);
    uint32_t i, rnd = 0x1234567u;
    if (!s)
        return -1;
    for (i = 0; i < n; i++) {
        const uint32_t tb = i % beat, te = i % eighth, b = i / beat;
        const float t = (float)tb / EC_FS;
        float v = 0, noise;
        rnd = rnd * 1664525u + 1013904223u;
        noise = (float)(int32_t)rnd / 2147483648.0f;
        v += 0.9f * sinf(6.2831853f * (45.0f * t + 60.0f * (1.0f - expf(-t * 30.0f)) / 30.0f)) * expf(-t * 9.0f);
        if (b == 1 || b == 3)
            v += 0.5f * noise * expf(-t * 22.0f) + 0.3f * sinf(6.2831853f * 190.0f * t) * expf(-t * 25.0f);
        v += 0.18f * noise * expf(-(float)te / EC_FS * 90.0f);
        v *= 0.7f;
        {
            const int x = (int)(v * 32767.0f);
            const int16_t q = (int16_t)(x > 32767 ? 32767 : x < -32768 ? -32768 : x);
            const uint32_t col = (uint32_t)((uint64_t)i * EC_SS_OVERVIEW / n);
            const int a = q < 0 ? -q : q;
            s->data[2 * i] = s->data[2 * i + 1] = q;
            if ((a >> 7) > s->overview[col])
                s->overview[col] = (uint8_t)((a >> 7) > 255 ? 255 : (a >> 7));
        }
    }
    s->frames_ready = n;
    s->rate = EC_FS;
    s->channels = 1;
    s->bits = 16;
    s->bpm = 120;
    snprintf(s->name, sizeof s->name, "BUILTIN LOOP");
    snprintf(s->path, sizeof s->path, "builtin:loop");
    s->ready = 1;
    return h;
}

/* ------------------------------------------------------------------------------- loading --- */
static void load_index(int i)
{
    if (i < 0 || i >= ec_browse_count())
        return;
    E.play_h = -1;                             /* the player lets go first: the store is about to be reused */
    ec_load_cancel();
    ec_ss_reset();
    E.builtin = 0;
    E.loaded_idx = i;
    snprintf(E.loaded_path, sizeof E.loaded_path, "%s", ec_browse_path(i));
    ec_load_start(E.loaded_path);
    E.pos = 0;
    E.dirty = 1;
}

static void use_builtin(void)
{
    E.play_h = -1;
    ec_load_cancel();
    ec_ss_reset();
    E.play_h = make_builtin();
    E.builtin = 1;
    E.loaded_idx = -1;
    snprintf(E.loaded_path, sizeof E.loaded_path, "builtin:loop");
    E.pos = 0;
    E.dirty = 1;
}

static void scan(void)
{
    E.changed = ec_fs_changed();
    ec_browse_start("/audio");
    E.scanning = 1;
}

/* ------------------------------------------------------------------------------- ec_app_* --- */
void ec_app_init(void)
{
    memset(&E, 0, sizeof E);
    E.play_h = -1;
    E.loaded_idx = -1;
    E.want_first = 1;
    E.gain = -1;
    snprintf(E.last, sizeof E.last, "POWER ON");
    ec_ss_reset();
    scan();
    E.dirty = 1;
}

static void event(const char *s)
{
    snprintf(E.last, sizeof E.last, "%s", s);
    E.dirty = 1;
}

void ec_app_tick(const ec_input_t *in)
{
    char ev[40];
    int b, k, r;

    /* the folder: the scan, a rescan when the host says it changed */
    if (!E.scanning && ec_fs_changed() != E.changed)
        scan();
    if (E.scanning) {
        const int st = ec_browse_step();
        if (st == EC_BR_DONE || st == EC_BR_ERROR) {
            E.scanning = 0;
            E.no_folder = st == EC_BR_ERROR;
            E.dirty = 1;
            if (E.loaded_path[0] && !E.builtin)
                E.loaded_idx = ec_browse_find(E.loaded_path);   /* (keep the loaded file across a rescan) */
            if (E.cursor >= ec_browse_count())
                E.cursor = 0;
            if (E.want_first) {
                E.want_first = 0;
                if (ec_browse_count() > 0)
                    load_index(0);
                else
                    use_builtin();
            }
        }
    }

    /* the loader */
    if (ec_load_state() == EC_LD_STAT || ec_load_state() == EC_LD_HEADER || ec_load_state() == EC_LD_DATA) {
        const int st = ec_load_step();
        if (st == EC_LD_DONE) {
            E.play_h = ec_load_handle();
            E.pos = 0;
        }
        E.dirty = 1;
    }

    /* input */
    if (in->master_moved) {
        E.master = in->master;
        snprintf(ev, sizeof ev, "MASTER %d%%", (int)(in->master * 100 / 1023));
        if (in->ms > 0)
            event(ev);
        E.dirty = 1;
    }
    for (b = 0; b < EC_NB; b++)
        if (in->btn_down >> b & 1u) {
            snprintf(ev, sizeof ev, "BTN %s", BTN_FN[b]);
            event(ev);
            if (b == EC_B_PLAY) {
                E.playing = !E.playing;
                if (E.playing)
                    E.pos = 0;
            }
        }
    if (in->btn_up >> EC_B_SAVE & 1u) {      /* FILE: load the file under the cursor */
        if (ec_browse_count() > 0 && (E.cursor != E.loaded_idx || ec_load_state() == EC_LD_ERROR))
            load_index(E.cursor);
    }
    for (r = 0; r < EC_NE; r++)
        if (in->enc[r]) {
            snprintf(ev, sizeof ev, "%s %+d", ENC_FN[r], (int)in->enc[r]);
            event(ev);
            if (r == EC_E_SELECT && ec_browse_count() > 0) {
                const int n = ec_browse_count();
                E.cursor = ((E.cursor + in->enc[r]) % n + n) % n;
            }
        }
    for (k = 0; k < EC_NKEY; k++)
        if (in->key_down >> k & 1u) {
            const int lo = ec_key_lower(k);
            if (lo >= 0)
                snprintf(ev, sizeof ev, "KEY STEP %d V%d", lo + 1, in->key_vel[k]);
            else
                snprintf(ev, sizeof ev, "KEY %s V%d", UPPER_FN[ec_key_upper(k)], in->key_vel[k]);
            event(ev);
        }
}

void ec_app_audio(int16_t *out, uint32_t frames)
{
    const ec_ss_sample_t *s = ec_ss_ready(E.play_h);
    const float m = (float)E.master / 1023.0f, target = m * m;   /* (a squared taper) */
    uint32_t i;
    if (E.gain < 0)
        E.gain = target;
    if (!s) {
        memset(out, 0, (size_t)frames * 4u);
        E.env = 0;
        return;
    }
    {
        const double step = (double)s->rate / EC_FS, len = (double)s->frames;
        const float gstep = (target - E.gain) / (float)frames;
        for (i = 0; i < frames; i++) {
            float l = 0, r = 0, g;
            const float env_target = E.playing ? 1.0f : 0.0f;
            E.env += (env_target - E.env) * 0.01f;   /* (a ~2 ms declick) */
            if (E.env < 0.0001f && !E.playing)
                E.env = 0;
            E.gain += gstep;
            if (E.env > 0) {
                const uint32_t i0 = (uint32_t)E.pos;
                const uint32_t i1 = i0 + 1 < s->frames ? i0 + 1 : 0;
                const float f = (float)(E.pos - i0);
                l = s->data[2 * i0] + f * (s->data[2 * i1] - s->data[2 * i0]);
                r = s->data[2 * i0 + 1] + f * (s->data[2 * i1 + 1] - s->data[2 * i0 + 1]);
                E.pos += step;
                if (E.pos >= len)
                    E.pos -= len;
            }
            g = E.gain * E.env;
            l *= g;
            r *= g;
            out[2 * i] = (int16_t)(l > 32767 ? 32767 : l < -32768 ? -32768 : l);
            out[2 * i + 1] = (int16_t)(r > 32767 ? 32767 : r < -32768 ? -32768 : r);
        }
        E.gain = target;
    }
    E.frames_out += frames;
}

static const ec_ss_sample_t *shown(void)
{
    const ec_ss_sample_t *s = ec_ss_ready(E.play_h);
    return s ? s : NULL;
}

void ec_app_frame(void)
{
    const ec_ss_sample_t *s = shown();
    const uint32_t now = ec_plat_ms();
    const int ld = ec_load_state();
    const int step = s && E.playing ? (int)(E.pos * 16.0 / s->frames) & 15 : -1;
    char msg[48];
    ec_e0_view_t v;

    /* the LEDs: the step chase, T1 selected, PLAY, FILE when the cursor is off the loaded file */
    ec_leds_steps(E.playing ? 0x1111u : 0, step);
    ec_leds_upper(0, 0);
    ec_leds_button(EC_B_PLAY, E.playing ? EC_LED_LIT : EC_LED_DIM);
    ec_plat_led_play_green(E.playing ? EC_LED_LIT : EC_LED_OFF);
    ec_leds_button(EC_B_SAVE, ec_browse_count() > 0 && E.cursor != E.loaded_idx ? EC_LED_MID : EC_LED_OFF);

    /* the screen: on a change, and at ~30 fps while playing */
    if (!E.dirty && !(E.playing && now - E.last_draw >= 30))
        return;
    E.dirty = 0;
    E.last_draw = now;
    memset(&v, 0, sizeof v);
    v.load_permille = -1;
    v.pos = -1;
    v.step = step;
    msg[0] = 0;
    if (ld == EC_LD_STAT || ld == EC_LD_HEADER || ld == EC_LD_DATA)
        v.load_permille = (int)ec_load_permille();
    else if (ld == EC_LD_ERROR)
        snprintf(msg, sizeof msg, "ERROR: %s", ec_load_error());
    else if (E.scanning)
        snprintf(msg, sizeof msg, "SCANNING /AUDIO");
    v.message = msg;
    if (s) {
        v.file = s->name;
        v.overview = s->overview;
        v.overview_n = EC_SS_OVERVIEW;
        v.pos = E.playing ? (float)(E.pos / s->frames) : -1.0f;
        v.bpm = s->bpm;
        v.rate = s->rate;
        v.channels = s->channels;
    } else if (ld != EC_LD_IDLE)
        v.file = ec_load_path()[0] == '/' ? strrchr(ec_load_path(), '/') + 1 : ec_load_path();
    if (E.builtin && !E.scanning)
        snprintf(msg, sizeof msg, E.no_folder ? "NO FOLDER: BUILTIN LOOP" : "NO FILES IN /AUDIO");
    if (E.builtin && s)
        msg[0] = 0;                            /* (the built-in loop's waveform; the reason is the FILE line) */
    v.playing = E.playing;
    v.vol_pct = (int)(E.master * 100 / 1023);
    v.nfiles = ec_browse_count();
    v.cursor = E.cursor;
    v.cursor_name = ec_browse_name(E.cursor);
    v.cursor_loaded = E.cursor == E.loaded_idx;
    v.last_event = E.last;
    v.files_note = E.scanning ? "SCANNING" : E.no_folder ? "NO FOLDER" : "NO FILES IN /AUDIO";
    ec_gfx_begin(ec_plat_lcd());
    ec_screen_e0_draw(&v);
    ec_plat_lcd_commit();
}

void ec_app_ui_info(char *buf, uint32_t n)
{
    const ec_ss_sample_t *s = shown();
    static const char *const LD[] = {"idle", "stat", "header", "data", "done", "error"};
    snprintf(buf, n, "e0 files:%d scan:%s file:%s load:%s%s%s play:%d vol:%d pos:%.3f frames:%u rate:%u bpm:%.1f "
             "last:%s",
             ec_browse_count(), E.scanning ? "busy" : E.no_folder ? "nofolder" : "done",
             s ? s->name : "-", LD[ec_load_state()], ec_load_state() == EC_LD_ERROR ? ":" : "",
             ec_load_state() == EC_LD_ERROR ? ec_load_error() : "", E.playing, (int)(E.master * 100 / 1023),
             s ? E.pos / s->frames : 0.0, s ? s->frames : 0u, s ? s->rate : 0u, s ? s->bpm : 0.0f, E.last);
}

void ec_app_shutdown(void) { ec_load_cancel(); }
uint32_t ec_app_cpu_hint(void) { return 0; }
