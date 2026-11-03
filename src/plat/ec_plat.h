/* SPDX-License-Identifier: GPL-3.0-only */
/* Elasticat FM-1: the platform as the rest of the code sees it. Only src/plat talks to fm1core.h; everything else
 * (the app, the screens, the loader) uses this header: buttons by label, keys as upper / lower rows, knobs by role,
 * LEDs, the LCD frame buffer, the device clock and the flash record. One thread: every call happens inside one of
 * the core's entry points (tick / frame / idle / audio), never concurrently. */
#ifndef EC_PLAT_H
#define EC_PLAT_H
#include <stdint.h>

/* ---- the panel's sizes ---- */
#define EC_LCD_W 240
#define EC_LCD_H 240
#define EC_FS 44100           /* output sample rate */
#define EC_BLOCK 128          /* frames per audio block */
#define EC_NKEY 27            /* note keys 0 (F3) .. 26 (G5) */
#define EC_NUPPER 11          /* the sharps: T1..T6, S1, S2, MST, FILL, PATN */
#define EC_NLOWER 16          /* the naturals: steps 1..16 */

/* ---- buttons by printed label (same order as fm1core.h's FM1P_B_*); Elasticat's function in the comment ---- */
enum {
    EC_B_FX,      /* FX */
    EC_B_SEL,     /* SRC (hold: loop keys) */
    EC_B_ENV,     /* AMP */
    EC_B_LFO,     /* MOD */
    EC_B_EDIT,    /* FILT */
    EC_B_GLO,     /* SHIFT: EC_B_FN */
    EC_B_HOME,    /* HOME: back to the main view (0.3.0; the MST category is the MST upper key) */
    EC_B_SAVE,    /* FILE */
    EC_B_ARP,     /* PAT */
    EC_B_SEQ,     /* TRIG */
    EC_B_PLAY,    /* PLAY */
    EC_B_REC,     /* REC (reserved: copy / clear / paste scope) */
    EC_B_OCTDN,   /* key mode - */
    EC_B_OCTUP,   /* key mode + */
    EC_NB
};
#define EC_B_FN EC_B_GLO   /* SHIFT (owner, 2026-11-02: GLO, not HOME) */
/* ---- knobs by role (FM1P_E_* order); MASTER is the absolute pot (ec_input_t.master) ---- */
enum { EC_E_SELECT, EC_E_ALGO, EC_E_PRESETS, EC_E_K1, EC_E_K2, EC_E_K3, EC_E_K4, EC_NE };

/* ---- LED levels (fm1core.h FM1P_LED_*; any 0..255 works) ---- */
enum { EC_LED_OFF = 0, EC_LED_FAINT = 12, EC_LED_DIM = 40, EC_LED_MID = 96, EC_LED_LIT = 255 };

/* ---- input: one snapshot per device millisecond (taken at tick) ---- */
typedef struct {
    uint32_t ms;                       /* the device clock */
    uint32_t btn_held, btn_down, btn_up;   /* bit EC_B_x; down / up: edges since the last tick (a tap is a down now
                                            * and an up at the next tick) */
    uint32_t key_held, key_down, key_up;   /* bit k: note key k (see ec_key_* below) */
    uint8_t key_vel[EC_NKEY];          /* velocity 1..127 of a key that went down (100 when the host gave none) */
    int32_t enc[EC_NE];                /* detents this tick by role, + = clockwise */
    int32_t master;                    /* MASTER pot 0..1023 */
    int master_moved;                  /* 1 when master changed this tick */
    uint8_t key_lean[EC_NKEY];         /* 0.5.0: aftertouch per key, 0..100 % (the panel's key_lean) */
    uint8_t cc_mw, cc_expr;            /* 0.5.0: MIDI CC 1 / CC 11 (any channel), 0..127, the last value */
} ec_input_t;

/* ---- keys: the 27 note keys as two rows ---- */
int ec_key_lower(int key);             /* key -> lower-row index 0..15 (a natural), or -1 */
int ec_key_upper(int key);             /* key -> upper-row index 0..10 (a sharp), or -1 */
int ec_key_of_lower(int i);            /* lower index 0..15 -> key */
int ec_key_of_upper(int i);            /* upper index 0..10 -> key */

/* ---- outputs ---- */
void ec_plat_led_key(int key, uint8_t level);          /* a note key's LED */
void ec_plat_led_lower(int i, uint8_t level);          /* lower key i (0..15) */
void ec_plat_led_upper(int i, uint8_t level);          /* upper key i (0..10) */
void ec_plat_led_button(int label, uint8_t level);     /* a button's LED (EC_B_x) */
void ec_plat_led_play_green(uint8_t level);            /* PLAY's green LED */

/* The LCD: 240x240 RGB565 pixels in the panel's own byte order (high byte first in memory); build colours with
 * ec_gfx.h's EC_PX(). Draw only inside frame (ec_app_frame), then call ec_plat_lcd_commit() once. */
uint16_t *ec_plat_lcd(void);
void ec_plat_lcd_commit(void);

uint32_t ec_plat_ms(void);             /* the device clock (ms since power-on) */

/* ---- live panel labels (fm1core.h fm1core_label: FIZZ's 49 slots) ----
 * One table the app fills from ec_app_labels(), which ec_core.c calls once per frame (after ec_app_frame). Slots:
 * EC_LBL_KNOB0 + role (FM1P_E_* order), EC_LBL_BTN0 + EC_B_x, EC_LBL_WHITE0 + lower key 0..15, EC_LBL_BLACK0 +
 * upper key 0..10. s: up to 10 chars (upper case ASCII, longer is cut); "" = no job here; NULL = not given (the
 * host's own name). Set every slot each call: the table keeps what it was given, and the host's epoch moves only
 * when its contents change. ec_core.c has a weak default ec_app_labels (the FN-held button layer); the app may
 * define its own. */
enum { EC_LBL_KNOB0 = 0, EC_LBL_BTN0 = 8, EC_LBL_WHITE0 = 22, EC_LBL_BLACK0 = 38, EC_LBL_N = 49 };
void ec_plat_label(int which, const char *s);
void ec_plat_labels_clear(void);       /* every slot NULL */
void ec_app_labels(void);              /* weak default in ec_core.c */

/* ---- audio input (E6; FM1VST docs/VIRTUAL-CARTS.md 8, capability "ain1") ----
 * The host calls the .rfm1's rfm1_audio_in (the core's fm1core_audio_in) immediately before rfm1_audio for the same
 * block: input frame i lines up with output frame i. ec_core.c keeps that block and hands it to the app inside the
 * same ec_app_audio call; a block with no input before it is silence. Hosts do not monitor the input themselves: what
 * is heard of it is what the app plays (the INPUT machine). */
enum { EC_AIN_OFF = 0, EC_AIN_LIVE = 1 };
#define EC_AIN_MAX 4096                     /* the longest input block kept (frames); more is cut */
int ec_plat_ain_state(void);                /* EC_AIN_*: 0 no input (off, denied, unplugged), 1 live */
uint32_t ec_plat_ain_latency(void);         /* the host's input latency estimate, frames at 44.1 kHz (0 unknown) */
uint32_t ec_plat_ain_epoch(void);           /* moves when the state or the latency changes */
/* inside ec_app_audio only: this call's input (interleaved stereo int16) and its frames (*n <= the call's frames; the
 * rest is silence); NULL when there is none (state 0, or no rfm1_audio_in before this block) */
const int16_t *ec_plat_ain(uint32_t *n);
/* the native stand-ins (host/ec_run, the tests): what fm1core_audio_in / fm1core_audio_in_info do in an .rfm1 */
void fm1core_audio_in(const int16_t *stereo, uint32_t frames);
void fm1core_audio_in_info(uint32_t state, uint32_t latency_frames);

/* ---- the flash record: the part of the flash image after the plat's versioned header (project data, later) ---- */
/* the flash image: EC_PLAT_FLASH_SIZE bytes (fm1core flash_size; hosts take <= 4 MiB and save a full copy every
 * time), the header's 4 KiB sector first, then the record (EC_PLAT_RECORD_MAX bytes) */
#define EC_PLAT_FLASH_SIZE 0x100000u
#define EC_PLAT_RECORD_MAX (EC_PLAT_FLASH_SIZE - 4096u)
uint8_t *ec_plat_record(uint32_t *size);   /* the record's bytes and their size (EC_PLAT_RECORD_MAX) */
void ec_plat_record_changed(void);         /* the record was written: the host persists the image */
int ec_plat_record_fresh(void);            /* 1: the image had no valid header at power-on (a fresh unit) */
/* 0.6.0: the APP SETTINGS record (docs/UX-V2.md §3): EC_PLAT_SETTINGS_SIZE bytes in the header's sector, apart from
 * the project record, so it survives project loads and new projects. 0xFF bytes when never written (and in an image
 * written before 0.6.0). The app owns its layout (put a magic + version in it). */
#define EC_PLAT_SETTINGS_SIZE 2048u
uint8_t *ec_plat_settings(uint32_t *size);
void ec_plat_settings_changed(void);       /* written: the host persists the image */

/* ---- what the app implements (src/app/ec_app.c). Called by ec_core.c ---- */
void ec_app_init(void);
void ec_app_tick(const ec_input_t *in);    /* once per device ms: input, the loader, the clock */
void ec_app_frame(void);                   /* every 15 ms: draw (if needed) and set the LEDs */
void ec_app_audio(int16_t *stereo, uint32_t frames);   /* s16 interleaved, frames a multiple of EC_BLOCK */
void ec_app_ui_info(char *buf, uint32_t n);            /* one line of state for tests ("key:value" words) */
void ec_app_shutdown(void);
uint32_t ec_app_cpu_hint(void);            /* optional load estimate in % (0: unknown) */

#endif
