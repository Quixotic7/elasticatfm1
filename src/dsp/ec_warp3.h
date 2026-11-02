/*
 * ec_warp3.h -- E3: state of the remaining warp modes (internal, included by
 * ec_internal.h; the functions are declared at the end of ec_internal.h).
 *
 *   reader family + a post stage (one shared delay line per track):
 *     pitch_corrected, gstretch, gstretch2 : SC PitchShift (4 overlapping grains)
 *     harmonizer                           : dry + 3 PitchShift voices
 *     random_ola                           : dry reader x TGrains cloud (XFade2)
 *   own families:
 *     domino          : ElasticatSlicer (2-head slice player on a step clock)
 *     wavetable       : ElasticatWavetable (region bank, morph LFO, region xfade)
 *     spectral_freeze / formant : \elasticatSpectral (FFT 2048: MagFreeze,
 *                       MagSmear, MagShift, BinShift), mono, centred
 */
#ifndef EC_WARP3_H
#define EC_WARP3_H

#include <stdint.h>
#include "ec_fft.h"

#define EC_PS_LEN     65536          /* post-stage delay line per channel (1.49 s at 44.1 kHz), power of 2 */
#define EC_PS_MAXWIN  ((EC_PS_LEN - 16) / 2)   /* the longest PitchShift frame (samples) */
#define EC_OLA_GRAINS 64             /* TGrains' grain cap (per track) */
#define EC_OLA_BUDGET 80             /* random_ola grains sounding across all tracks (CPU; not in the original) */

/* reader post stages */
enum { W3_ST_DRY = 0, W3_ST_PS = 1, W3_ST_HARM = 2, W3_ST_OLA = 3 };

typedef struct { float dsamp, dslope, ramp, rslope; } ec_psg_t;
typedef struct { ec_psg_t g[4]; int counter, stage; } ec_ps_t;   /* SC PitchShift */

typedef struct { double pos, rate; float ph, inc; } ec_olag_t;   /* a TGrains grain */

typedef struct { double pos, lo, hi, since; int dir, state; float env; } ec_slv_t;  /* a slicer head */

typedef struct {
    /* reader post stage */
    int   rd_mode;                   /* the reader-family warp mode in effect (EC_WARP_*) */
    int   stage, prev_stage;
    float xf, xf_inc;                /* stage crossfade 0..1 */
    uint32_t wr;
    float dl[2][EC_PS_LEN];
    ec_ps_t ps;                      /* pitch_corrected / gstretch / gstretch2 */
    ec_ps_t harm[3];
    float h_iv[3], h_gate[3];        /* lagged intervals (st) and voice gates */
    float win_lag, disp_lag, smth_lag, strch_lag;
    ec_olag_t og[EC_OLA_GRAINS];
    int   ong;
    double otrig;
    float o_dur, o_ov, o_wan;
    uint32_t rng;
    /* domino */
    ec_slv_t sv[2];
    int   sv_active, sv_prev;
    double sph, lph;                 /* slice cursor (0..1 zone space), loop phase (UI) */
    /* wavetable */
    double osc, lfo;
    float lcur, lnext, morph_lag, depth_lag;
    double rs[2], rl[2];
    float wfade[2];
    int   wst[2], wact, wcommitted, wqueued;
    double qs, ql;
    /* spectral */
    float sp_in[EC_FFT_N], sp_out[EC_FFT_N];
    float sp_frz[EC_FFT_HALF + 1];
    int   sp_pos, sp_hop;
    double sp_ph;
    float frz_lag, blur_lag, fsh_lag, pit_lag;
} ec_w3_t;

/* engine-wide scratch for the spectral modes (one track at a time) */
typedef struct {
    ec_rfft_t fft;
    float win[EC_FFT_N];             /* sine window (analysis and synthesis) */
    float x[EC_FFT_N];
    float mag[EC_FFT_HALF + 1], m2[EC_FFT_HALF + 1], ur[EC_FFT_HALF + 1], ui[EC_FFT_HALF + 1];
    float cr[EC_FFT_HALF + 1], ci[EC_FFT_HALF + 1];
    float lag10[17];                 /* Lag 0.1 s coefficients for 0..EC_KR samples */
} ec_w3_shared_t;

/* the spectral budget: tracks 0 .. EC_W3_SPECTRAL_TRACKS-1 may run an FFT chain
 * (the original: track 1 only; measured cheap enough here for every track) */
#define EC_W3_SPECTRAL_TRACKS 6

#endif
