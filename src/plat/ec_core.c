/* SPDX-License-Identifier: GPL-3.0-only */
/* Elasticat FM-1: the fm1core ABI (fm1core.h, ABI 6). The one file that knows the panel: it turns the host's panel
 * into ec_input_t, owns the LEDs, the LCD and the flash image, and calls the app (ec_plat.h's ec_app_*).
 *
 * Built two ways: by FM1VST's fm1_add_core (core-glue/elasticat: FM1CORE_ID .. come from the build, the descriptor's
 * credits from fm1core_credits.h) and by this repo's own CMake (the native library and host/ec_run, with the ABI
 * header copied in src/plat/abi). */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "fm1core.h"
#include "ec_plat.h"
#include "ec_version.h"

#ifdef FM1CORE_ID            /* FM1VST's build: its version.h (FM1CORE_SOURCE_URL, the credits) */
#include "version.h"
#define EC_SOURCE_URL FM1CORE_SOURCE_URL(EC_REPO)
#define EC_CREDITS FM1CORE_CREDITS_OR_NULL
#define EC_GATED FM1CORE_GATED_OR_NULL
#else
#define FM1CORE_ID "elasticat"
#define FM1CORE_VERSION EC_VERSION
#define FM1CORE_FAMILY "elasticat"
#define FM1CORE_LICENSE "GPL-3.0-only"
#define EC_SOURCE_URL EC_REPO
#define EC_CREDITS "Elasticat by Quixotic7, ported from the norns script Elasticat (GPL-3.0)."
#define EC_GATED ((const char *)0)
#endif

#define EC_FLASH_SIZE 0x100000u      /* 1 MiB image */
#define EC_FLASH_HDR 4096u           /* the header's sector; the record follows */
#define EC_FLASH_MAGIC 0x54414345u   /* "ECAT" little-endian */
#define EC_FLASH_FORMAT 1u           /* bump when the record's layout changes */

/* the image's header (little-endian, at offset 0) */
typedef struct {
    uint32_t magic;          /* EC_FLASH_MAGIC */
    uint16_t format;         /* EC_FLASH_FORMAT */
    uint16_t hdr_size;       /* EC_FLASH_HDR */
    uint32_t image_size;     /* EC_FLASH_SIZE */
    uint32_t record_size;    /* bytes of record in use (0 in E0) */
    char fw_version[16];     /* the firmware that wrote it */
} ec_flash_hdr_t;

/* ------------------------------------------------------------------------------------------- state --- */
static fm1panel_t g_panel;
static uint8_t g_flash[EC_FLASH_SIZE];
static uint8_t g_staged[EC_FLASH_SIZE];
static uint32_t g_staged_n;
static int g_use_staged;
static uint32_t g_flash_dirty;
static int g_fresh;
static char g_flash_path[512];
static int g_save_on_exit, g_no_flash;
static int g_running, g_shut;
static uint32_t g_ms;
static uint32_t g_btn_prev, g_key_prev;
static int32_t g_master_prev = -1;
static ec_input_t g_in;
static uint8_t g_cc_mw, g_cc_expr;             /* 0.5.0: the last CC 1 / CC 11 */

/* the FM-1's wiring (hal/fm1_input.h FM1_KEYMAP): key id (0..13 matrix button, 14 + n note key n) at (row, column);
 * LED (column, line) = (column, row - 1) for rows 1..4 */
static const int8_t KEYMAP[6][FM1P_NCOL] = {
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {5, 11, 4, 10, 3, 9, 2, 8, -1, -1, -1},
    {34, 35, 36, 37, 38, 40, 39, 13, 7, 6, 12},
    {23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33},
    {0, 1, 15, 14, 17, 16, 19, 18, 20, 21, 22},
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
};
/* Felucca's PANEL_DEFAULT: label -> matrix button, role -> matrix encoder (all clockwise +) */
static const uint8_t BTN_ID[FM1P_NB] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 0, 1};
static const uint8_t ENC_ID[FM1P_NE - 1] = {0, 1, 6, 2, 3, 4, 5};

static int8_t g_led_col[41], g_led_line[41];   /* key id -> LED position, -1 none */
static int8_t g_lower_key[EC_NLOWER], g_upper_key[EC_NUPPER];
static int8_t g_key_row[EC_NKEY];               /* key -> lower index, or -(upper index) - 1 */

static void tables(void)
{
    int r, c, k, nl = 0, nu = 0;
    memset(g_led_col, -1, sizeof g_led_col);
    memset(g_led_line, -1, sizeof g_led_line);
    for (r = 1; r <= FM1P_NLINE; r++)
        for (c = 0; c < FM1P_NCOL; c++) {
            const int id = KEYMAP[r][c];
            if (id >= 0 && id < 41) {
                g_led_col[id] = (int8_t)c;
                g_led_line[id] = (int8_t)(r - 1);
            }
        }
    for (k = 0; k < EC_NKEY; k++) {
        const int s = k % 12;        /* semitones above F */
        const int natural = s == 0 || s == 2 || s == 4 || s == 6 || s == 7 || s == 9 || s == 11;
        if (natural) {
            g_lower_key[nl] = (int8_t)k;
            g_key_row[k] = (int8_t)nl++;
        } else {
            g_upper_key[nu] = (int8_t)k;
            g_key_row[k] = (int8_t)(-nu - 1);
            nu++;
        }
    }
}

/* ------------------------------------------------------------------------------------------- ec_plat.h --- */
int ec_key_lower(int key) { return key >= 0 && key < EC_NKEY && g_key_row[key] >= 0 ? g_key_row[key] : -1; }
int ec_key_upper(int key) { return key >= 0 && key < EC_NKEY && g_key_row[key] < 0 ? -g_key_row[key] - 1 : -1; }
int ec_key_of_lower(int i) { return i >= 0 && i < EC_NLOWER ? g_lower_key[i] : -1; }
int ec_key_of_upper(int i) { return i >= 0 && i < EC_NUPPER ? g_upper_key[i] : -1; }

static void led_id(int id, uint8_t level)
{
    if (id < 0 || id >= 41 || g_led_col[id] < 0)
        return;
    g_panel.led[g_led_col[id]][g_led_line[id]] = level;
}
void ec_plat_led_key(int key, uint8_t level)
{
    if (key >= 0 && key < EC_NKEY)
        led_id(14 + key, level);
}
void ec_plat_led_lower(int i, uint8_t level) { ec_plat_led_key(ec_key_of_lower(i), level); }
void ec_plat_led_upper(int i, uint8_t level) { ec_plat_led_key(ec_key_of_upper(i), level); }
void ec_plat_led_button(int label, uint8_t level)
{
    if (label >= 0 && label < EC_NB)
        led_id(g_panel.btn_id[label], level);
}
void ec_plat_led_play_green(uint8_t level) { g_panel.led_play_green = level; }

uint16_t *ec_plat_lcd(void) { return g_panel.lcd; }
void ec_plat_lcd_commit(void) { g_panel.lcd_writes++; }
uint32_t ec_plat_ms(void) { return g_ms; }

uint8_t *ec_plat_record(uint32_t *size)
{
    if (size)
        *size = EC_FLASH_SIZE - EC_FLASH_HDR;
    return g_flash + EC_FLASH_HDR;
}
void ec_plat_record_changed(void) { g_flash_dirty++; }
/* 0.6.0: the APP SETTINGS record: the second half of the header's sector (the header uses its first bytes) */
#define EC_FLASH_SETTINGS (EC_FLASH_HDR - EC_PLAT_SETTINGS_SIZE)
uint8_t *ec_plat_settings(uint32_t *size)
{
    if (size)
        *size = EC_PLAT_SETTINGS_SIZE;
    return g_flash + EC_FLASH_SETTINGS;
}
void ec_plat_settings_changed(void) { g_flash_dirty++; }
int ec_plat_record_fresh(void) { return g_fresh; }

/* ------------------------------------------------------------------------------------------- live labels --- */
/* fm1core_labels_epoch / fm1core_label (fm1core.h optional module symbols) read this table; ec_plat_label fills it */
static char g_lbl[EC_LBL_N][11];
static uint8_t g_lbl_given[EC_LBL_N];
static uint32_t g_lbl_epoch;           /* 0 until the table first holds a label */

static void lbl_changed(void)
{
    if (++g_lbl_epoch == 0)
        g_lbl_epoch = 1;                   /* (0 means "no live labels": skip it on a wrap) */
}
void ec_plat_label(int which, const char *s)
{
    char t[11];
    int n = 0;
    if (which < 0 || which >= EC_LBL_N)
        return;
    if (!s) {
        if (g_lbl_given[which]) {
            g_lbl_given[which] = 0;
            g_lbl[which][0] = 0;
            lbl_changed();
        }
        return;
    }
    for (; n < 10 && s[n]; n++) {
        const char ch = s[n] >= 'a' && s[n] <= 'z' ? (char)(s[n] - 32) : s[n];
        t[n] = ch >= ' ' && ch <= '~' ? ch : ' ';
    }
    t[n] = 0;
    if (!g_lbl_given[which] || strcmp(g_lbl[which], t) != 0) {
        g_lbl_given[which] = 1;
        memcpy(g_lbl[which], t, (size_t)n + 1);
        lbl_changed();
    }
}
void ec_plat_labels_clear(void)
{
    int i;
    for (i = 0; i < EC_LBL_N; i++)
        ec_plat_label(i, NULL);
}

/* The default filler (the app may define its own ec_app_labels): while FN (GLO) is held the buttons show what they
 * do with FN, otherwise nothing is given (the host draws button_names). "FN held" is the GLO bit of the buttons this
 * layer hands the app (g_in.btn_held), which the app's A->fn follows exactly (ec_app.c button_down / button_up; E5
 * passes FN through). The FN layer as the code has it: ec_e5.c ec_e5_button_down (SAVE undo, OCT+ memorize, OCT-
 * recall), ec_app.c button_down (PLAY stop, REC save) and category() (FN + a category with a setup list,
 * ec_pages.c ec_settings_items; MOD has none: ""). Page-local meanings (RAZOR's OCT Auto-Chop / Redistribute, PLAY
 * = PASTE with a scope held, FN + the category in a setup list going back) need the app's state: not shown here. */
static const char *const FN_LABEL[EC_NB] = {
    [EC_B_FX] = "SET",    [EC_B_SEL] = "SET",  [EC_B_ENV] = "SET",   [EC_B_LFO] = "",      [EC_B_EDIT] = "SET",
    [EC_B_GLO] = NULL,    [EC_B_HOME] = "SET", [EC_B_SAVE] = "UNDO", [EC_B_ARP] = "SET",   [EC_B_SEQ] = "SET",
    [EC_B_PLAY] = "STOP", [EC_B_REC] = "SAVE", [EC_B_OCTDN] = "RCL", [EC_B_OCTUP] = "MEM",
};
__attribute__((weak)) void ec_app_labels(void)
{
    const int fn = g_in.btn_held >> EC_B_FN & 1u;
    int b;
    for (b = 0; b < EC_NB; b++)
        ec_plat_label(EC_LBL_BTN0 + b, fn ? FN_LABEL[b] : NULL);
}

FM1CORE_EXPORT uint32_t fm1core_labels_epoch(void) { return g_lbl_epoch; }
FM1CORE_EXPORT const char *fm1core_label(int32_t which)
{
    if (which < 0 || which >= EC_LBL_N || !g_lbl_given[which])
        return NULL;
    return g_lbl[which];
}

/* ------------------------------------------------------------------------------------------- audio input --- */
/* fm1core_audio_in / fm1core_audio_in_info: optional module symbols the .rfm1 shim (FM1VST core-glue/common/
 * rfm1_shim.c, built with ain1 in fm1_add_rfm1's WANTS) exports as rfm1_audio_in / rfm1_audio_in_info (FM1VST
 * docs/VIRTUAL-CARTS.md 8; descriptor fields at the next ABI bump). The host calls them on the thread that calls
 * audio, rfm1_audio_in immediately before rfm1_audio: the block is copied here and taken by the next c_audio (one
 * producer, one consumer: the count is published last, taken atomically). */
static int16_t g_ain[EC_AIN_MAX * 2];
static volatile uint32_t g_ain_n;          /* frames waiting for the next audio call (0 none) */
static volatile uint32_t g_ain_state, g_ain_latency, g_ain_epoch;
static const int16_t *g_ain_cur;           /* during c_audio: the block taken, and its frames */
static uint32_t g_ain_cur_n;

FM1CORE_EXPORT void fm1core_audio_in(const int16_t *stereo, uint32_t frames)
{
    if (!stereo || !frames)
        return;
    if (frames > EC_AIN_MAX)
        frames = EC_AIN_MAX;
    memcpy(g_ain, stereo, (size_t)frames * 4u);
    __atomic_store_n(&g_ain_n, frames, __ATOMIC_RELEASE);
}
FM1CORE_EXPORT void fm1core_audio_in_info(uint32_t state, uint32_t latency_frames)
{
    state = state == EC_AIN_LIVE ? EC_AIN_LIVE : EC_AIN_OFF;
    if (state != g_ain_state || latency_frames != g_ain_latency) {
        __atomic_store_n(&g_ain_state, state, __ATOMIC_RELEASE);
        __atomic_store_n(&g_ain_latency, latency_frames, __ATOMIC_RELEASE);
        __atomic_add_fetch(&g_ain_epoch, 1u, __ATOMIC_RELEASE);
    }
    if (state == EC_AIN_OFF)
        __atomic_store_n(&g_ain_n, 0u, __ATOMIC_RELEASE);
}
int ec_plat_ain_state(void) { return (int)__atomic_load_n(&g_ain_state, __ATOMIC_ACQUIRE); }
uint32_t ec_plat_ain_latency(void) { return __atomic_load_n(&g_ain_latency, __ATOMIC_ACQUIRE); }
uint32_t ec_plat_ain_epoch(void) { return __atomic_load_n(&g_ain_epoch, __ATOMIC_ACQUIRE); }
const int16_t *ec_plat_ain(uint32_t *n)
{
    if (n)
        *n = g_ain_cur ? g_ain_cur_n : 0;
    return g_ain_cur;
}

/* ------------------------------------------------------------------------------------------- the flash --- */
static void flash_format(void)
{
    ec_flash_hdr_t h;
    memset(g_flash, 0xFF, sizeof g_flash);
    memset(&h, 0, sizeof h);
    h.magic = EC_FLASH_MAGIC;
    h.format = EC_FLASH_FORMAT;
    h.hdr_size = EC_FLASH_HDR;
    h.image_size = EC_FLASH_SIZE;
    h.record_size = 0;
    snprintf(h.fw_version, sizeof h.fw_version, "%s", EC_VERSION);
    memcpy(g_flash, &h, sizeof h);
    g_flash_dirty++;
}
static int flash_valid(void)
{
    ec_flash_hdr_t h;
    memcpy(&h, g_flash, sizeof h);
    return h.magic == EC_FLASH_MAGIC && h.format == EC_FLASH_FORMAT && h.hdr_size == EC_FLASH_HDR &&
           h.image_size == EC_FLASH_SIZE;
}
static void flash_load(void)
{
    memset(g_flash, 0xFF, sizeof g_flash);
    if (g_use_staged) {
        if (g_staged_n)
            memcpy(g_flash, g_staged, g_staged_n);
    }
#ifndef __wasm__
    else if (g_flash_path[0] && !g_no_flash) {
        FILE *f = fopen(g_flash_path, "rb");
        if (f) {
            size_t n = fread(g_flash, 1, sizeof g_flash, f);
            (void)n;
            fclose(f);
        }
    }
#endif
    g_fresh = !flash_valid();
    if (g_fresh)
        flash_format();
}
static void flash_save(void)
{
    const int save = g_flash_path[0] && !g_no_flash && !g_use_staged && g_save_on_exit;
#ifdef __wasm__
    (void)save;                            /* (no files in a module: the host persists the image) */
#else
    if (save) {
        FILE *f = fopen(g_flash_path, "wb");
        if (f) {
            fwrite(g_flash, 1, sizeof g_flash, f);
            fclose(f);
        }
    }
#endif
}

/* ------------------------------------------------------------------------------------------- input --- */
static uint32_t take_u32(volatile uint32_t *v) { return __atomic_exchange_n(v, 0u, __ATOMIC_SEQ_CST); }
static int32_t take_i32(volatile int32_t *v) { return __atomic_exchange_n(v, 0, __ATOMIC_SEQ_CST); }

static uint32_t labels_of(uint32_t matrix)
{
    uint32_t out = 0;
    int b;
    for (b = 0; b < EC_NB; b++)
        if (matrix & (1u << g_panel.btn_id[b]))
            out |= 1u << b;
    return out;
}

static void read_input(uint32_t ms)
{
    const uint32_t btn_now = labels_of(g_panel.buttons | take_u32(&g_panel.buttons_tap));
    const uint32_t key_now = (g_panel.keys | take_u32(&g_panel.keys_tap)) & ((1u << EC_NKEY) - 1u);
    int r, k;
    g_in.ms = ms;
    g_in.btn_down = btn_now & ~g_btn_prev;
    g_in.btn_up = g_btn_prev & ~btn_now;
    g_in.btn_held = btn_now;
    g_btn_prev = btn_now;
    g_in.key_down = key_now & ~g_key_prev;
    g_in.key_up = g_key_prev & ~key_now;
    g_in.key_held = key_now;
    g_key_prev = key_now;
    for (k = 0; k < EC_NKEY; k++)
        if (g_in.key_down >> k & 1u) {
            const uint8_t v = g_panel.key_vel[k];
            g_in.key_vel[k] = v ? (v > 127 ? 127 : v) : 100;
        }
    for (k = 0; k < EC_NKEY; k++)                  /* 0.5.0: aftertouch, CC 1, CC 11 */
        g_in.key_lean[k] = g_panel.key_lean[k];
    g_in.cc_mw = g_cc_mw;
    g_in.cc_expr = g_cc_expr;
    for (r = 0; r < EC_NE; r++)
        g_in.enc[r] = take_i32(&g_panel.enc[g_panel.enc_id[r]]) * (g_panel.enc_dir[r] < 0 ? -1 : 1);
    g_in.master = g_panel.master < 0 ? 0 : g_panel.master > 1023 ? 1023 : g_panel.master;
    g_in.master_moved = g_in.master != g_master_prev;
    g_master_prev = g_in.master;
}

/* ------------------------------------------------------------------------------------------- entry points --- */
static void c_options(const char *flash_path, int no_flash, int save_on_exit, int headless)
{
    (void)headless;
    g_use_staged = flash_path && !strcmp(flash_path, FM1CORE_FLASH_STAGED);
    g_flash_path[0] = 0;
    if (flash_path && !g_use_staged)
        snprintf(g_flash_path, sizeof g_flash_path, "%s", flash_path);
    g_no_flash = no_flash;
    g_save_on_exit = save_on_exit;
}
static int c_boot_options(int fail, const char *reason, int stage)
{
    (void)fail, (void)reason, (void)stage;
    return 1;
}
static void c_init(int demo)
{
    (void)demo;
    tables();
    memcpy(g_panel.btn_id, BTN_ID, sizeof BTN_ID);
    memcpy(g_panel.enc_id, ENC_ID, sizeof ENC_ID);
    memset(g_panel.enc_dir, 1, sizeof g_panel.enc_dir);
    if (g_panel.master <= 0)
        g_panel.master = 820;              /* the default volume, ~80 % */
    memset(g_panel.led, 0, sizeof g_panel.led);
    g_ms = 0;
    g_btn_prev = g_key_prev = 0;
    g_master_prev = -1;
    memset(&g_in, 0, sizeof g_in);
    g_cc_mw = g_cc_expr = 0;
    g_shut = 0;
    flash_load();
    ec_plat_labels_clear();
    ec_app_init();
    ec_app_frame();                        /* the first screen */
    ec_app_labels();
    g_running = 1;
    if (g_panel.ready == 0)
        g_panel.ready = 1;
}
static void c_tick(uint32_t ms)
{
    if (!g_running)
        return;
    g_ms = ms;
    read_input(ms);
    ec_app_tick(&g_in);
}
static void c_frame(void)
{
    if (!g_running)
        return;
    ec_app_frame();
    ec_app_labels();
}
static void c_idle(void) {}
static void c_audio(int16_t *stereo, uint32_t frames)
{
    const uint32_t ain = __atomic_exchange_n(&g_ain_n, 0u, __ATOMIC_ACQ_REL);   /* this block's input, once */
    if (!g_running) {
        memset(stereo, 0, (size_t)frames * 4u);
        return;
    }
    g_ain_cur = ain && ec_plat_ain_state() == EC_AIN_LIVE ? g_ain : NULL;
    g_ain_cur_n = ain < frames ? ain : frames;
    ec_app_audio(stereo, frames);
    g_ain_cur = NULL;
    g_ain_cur_n = 0;
}
/* 0.5.0: USB-MIDI packets (cable 0: CIN | status << 8 | d1 << 16 | d2 << 24): CC 1 (mod wheel) and CC 11
 * (expression) on any channel feed the mod matrix's MW / EXPR; the rest is taken and ignored */
static int c_midi_in(uint32_t pkt)
{
    const uint32_t cin = pkt & 15u, st = (pkt >> 8) & 0xFFu, cc = (pkt >> 16) & 0x7Fu, val = (pkt >> 24) & 0x7Fu;
    if (cin == 0xBu && (st & 0xF0u) == 0xB0u) {
        if (cc == 1)
            g_cc_mw = (uint8_t)val;
        else if (cc == 11)
            g_cc_expr = (uint8_t)val;
    }
    return 1;
}
static int c_midi_out_take(uint32_t *pkt)
{
    (void)pkt;
    return 0;
}
static void c_dump(void) {}
static void c_stats(uint32_t *shed, uint32_t *cpu_pct)
{
    if (shed)
        *shed = 0;
    if (cpu_pct)
        *cpu_pct = ec_app_cpu_hint();
}
static void c_ui_info(char *buf, uint32_t n)
{
    if (!buf || !n)
        return;
    buf[0] = 0;
    if (g_running)
        ec_app_ui_info(buf, n);
}
static void c_shutdown(void)
{
    if (g_shut || !g_running)
        return;
    g_shut = 1;
    ec_app_shutdown();
    flash_save();
}
static int c_halted(void) { return 0; }
static int c_flash_stage(const uint8_t *bytes, uint32_t n)
{
    if (n > EC_FLASH_SIZE)
        n = EC_FLASH_SIZE;
    g_staged_n = bytes ? n : 0;
    if (g_staged_n)
        memcpy(g_staged, bytes, g_staged_n);
    return 1;
}
static uint8_t *c_flash(void) { return g_flash; }
static uint32_t c_flash_dirty(void) { return g_flash_dirty; }
static int c_flash_sync(void) { return 1; }   /* (nothing deferred: the record is written in place) */

static fm1core_t CORE = {
    .abi_version = FM1CORE_ABI,
    .id = FM1CORE_ID,
    .name = "Elasticat",
    .version = FM1CORE_VERSION,
    .source_url = EC_SOURCE_URL,
    .family = FM1CORE_FAMILY,
    .license = FM1CORE_LICENSE,
    .gated = EC_GATED,
    .flash_size = EC_FLASH_SIZE,
    /* Elasticat's own names (hosts draw them on the panel, the printed name beside a renamed button) */
    .button_names = {"FX", "SRC", "AMP", "MOD", "FILT", "SHIFT", "HOME", "FILE", "PAT", "TRIG", "PLAY", "REC", "OCT-",
                     "OCT+"},
    .params = NULL,
    .nparams = 0,
    .panel = &g_panel,
    .keymap = &KEYMAP[0][0],
    .options = c_options,
    .boot_options = c_boot_options,
    .init = c_init,
    .tick = c_tick,
    .frame = c_frame,
    .idle = c_idle,
    .audio = c_audio,
    .midi_in = c_midi_in,
    .midi_out_take = c_midi_out_take,
    .dump = c_dump,
    .stats = c_stats,
    .ui_info = c_ui_info,
    .shutdown = c_shutdown,
    .halted = c_halted,
    .flash_stage = c_flash_stage,
    .flash = c_flash,
    .flash_dirty = c_flash_dirty,
    .flash_sync = c_flash_sync,
    .param_epoch = NULL,
    .knob_target = NULL,
    .credits = EC_CREDITS,
};

FM1CORE_EXPORT const fm1core_t *fm1core_get(uint32_t abi_version)
{
    if (abi_version != FM1CORE_ABI)
        return NULL;
    return &CORE;
}
