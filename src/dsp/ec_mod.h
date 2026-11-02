/*
 * ec_mod.h -- E4: the MOD category (C port of \elasticatMod in
 * Engine_Elasticat.sc): 2 LFOs + a mod envelope + velocity + 4 macros with a
 * 4 x 5 depth matrix, summed onto five destinations per track.
 *
 * Destinations (the SC mod buses, -1..1 each, clipped where they are read):
 *   pitch  -> +-12 semitones on the readers and slice voices
 *   cutoff -> +-120 semitones on the filter cutoff (the LFO / env / macro
 *             terms are pre-scaled x0.3, velocity at full weight)
 *   res    -> +-0.5 on the 0..1 resonance
 *   amp    -> amp * (1 + mod), floored at 0 (tremolo)
 *   pan    -> pan + mod, clipped -1..1
 * A track computes nothing while every depth is 0 (SC needsMod: no synth).
 */
#ifndef EC_MOD_H
#define EC_MOD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* destination index = the Lua option index - 1 ("OFF" = 0) */
enum { EC_MD_OFF = 0, EC_MD_PITCH, EC_MD_CUTOFF, EC_MD_RES, EC_MD_AMP, EC_MD_PAN,
       EC_MD_MACRO1, EC_MD_MACRO2, EC_MD_MACRO3, EC_MD_MACRO4 };
enum { EC_MOD_PITCH = 0, EC_MOD_CUTOFF, EC_MOD_RES, EC_MOD_AMP, EC_MOD_PAN, EC_MOD_NDEST };
/* LFO wave (0-based): sine, tri, saw, ramp down, square, S&H random */
enum { EC_LW_SINE = 0, EC_LW_TRI, EC_LW_SAW, EC_LW_RSAW, EC_LW_SQR, EC_LW_RAND };
/* LFO mode (0-based): FREE never resets, TRIG resets the phase on a note trig,
 * ONE runs a single cycle per trig, HOLD samples & holds the free wave at a trig */
enum { EC_LM_FREE = 0, EC_LM_TRIG, EC_LM_ONE, EC_LM_HOLD };

typedef struct ec_mod_params {
    struct { int dest, wave, mode; float beats, depth; } lfo[2];   /* beats per cycle; depth -1..1 */
    int   menv_dest;
    float menv_attack, menv_decay, menv_sustain, menv_release, menv_depth;   /* s, s, 0..1, s, -1..1 */
    int   mvel_dest;
    float mvel_depth;
    float macro_base[4];                  /* 0..1 */
    float macro_depth[4][EC_MOD_NDEST];   /* -1..1, [macro][pitch cutoff res amp pan] */
} ec_mod_params_t;

void ec_mod_params_default(ec_mod_params_t *p);
int  ec_mod_needed(const ec_mod_params_t *p);    /* any depth != 0 */

#ifdef __cplusplus
}
#endif
#endif
