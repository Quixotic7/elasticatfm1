/*
 * ec_click.h -- the metronome / count-in click (docs/UX-V2.md §4: played on the main output after the master bus,
 * never resampled, with its own level). Short synthesized sounds, no samples, no malloc, no heap state:
 * declare `ec_click_t c;` anywhere (static, in the app's state) and ec_click_init it once.
 *
 *   ec_click_init(&c, 44100.f);
 *   on a beat inside this block at frame k:  ec_click_trigger(&c, bar_start, EC_CLICK_WOOD, level);
 *                                            ec_click_mix(&c, out_s16, frames, k);
 *   every following block:                   ec_click_mix(&c, out_s16, frames, 0);
 *
 * ec_click_mix adds the click into interleaved int16 stereo (the same value on both channels), saturating at
 * +-32767 / -32768. `offset` is where, inside this buffer, a click triggered since the last mix starts (the
 * sample-accurate onset); a click that is still ringing plays on up to that frame and is then replaced. With no
 * pending trigger the ringing click simply continues from frame 0 and `offset` is ignored. With offset >= frames
 * the pending click waits: the next mix's offset places it.
 *
 * Deterministic: every trigger of the same (sound, accent, level) renders the same samples. Each click lasts at
 * most EC_CLICK_MAX_MS. accent = the bar start: a higher and louder variant of the same sound. level 0..1; at
 * level 1 an accented click peaks near -4 dBFS, a plain one near -8.5 dBFS.
 */
#ifndef EC_CLICK_H
#define EC_CLICK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    EC_CLICK_WOOD = 0,     /* woodblock: two damped modes + a noise tap */
    EC_CLICK_BEEP = 1,     /* a sine beep, 880 Hz / accent 1760 Hz */
    EC_CLICK_CLICK = 2,    /* a short bright tick (high-passed noise + a high mode) */
    EC_CLICK_COWBELL = 3,  /* two inharmonic square-ish modes */
    EC_CLICK_N
};
/* display names for the APP SETTINGS list, indexed by EC_CLICK_* */
extern const char *const ec_click_names[EC_CLICK_N];

#define EC_CLICK_MAX_MS 120

typedef struct {
    float c, y1, y2;       /* damped resonator: y = c * y1 - r2 * y2 */
    float r2, g;
} ec_click_mode_t;

typedef struct ec_click {
    float sr;
    /* the sounding click */
    int      active;
    uint32_t t, len;       /* samples played / its length */
    uint32_t atk, rel;     /* attack / release ramps (samples) */
    ec_click_mode_t m[2];
    float    nz_g, nz_d, hp_x, hp_y, hp_a;   /* noise burst: gain, decay per sample, one-pole HP */
    float    sq;           /* 0: sine modes, > 0: soft square drive */
    float    amp;          /* level x accent x sound gain x 32767 */
    uint32_t rng;
    /* a trigger waiting for its onset in the next mix */
    int   pend;
    int   p_sound, p_accent;
    float p_level;
    float norm[EC_CLICK_N][2];   /* 1 / each sound's raw peak (plain, accent), set by ec_click_init */
} ec_click_t;

void ec_click_init(ec_click_t *c, float sr);
/* sound: EC_CLICK_* (out of range -> WOOD); accent: 1 = bar start; level 0..1 (0 = nothing is triggered) */
void ec_click_trigger(ec_click_t *c, int accent, int sound, float level);
/* adds into stereo[2 * frames], saturating; the pending click starts at frame `offset` */
void ec_click_mix(ec_click_t *c, int16_t *stereo, uint32_t frames, uint32_t offset);
/* 1 while a click is sounding or waiting for its onset */
int  ec_click_busy(const ec_click_t *c);

#ifdef __cplusplus
}
#endif
#endif /* EC_CLICK_H */
