/*
 * ec_mod.h -- E4: the MOD category (C port of \elasticatMod in
 * Engine_Elasticat.sc): 2 LFOs + a mod envelope + velocity + 4 macros.
 * 0.5.0: the routing is the mod matrix (src/seq/ec_mtx.h, docs/MOD-MATRIX.md):
 * 8 slots of source -> destination x depth. The engine computes the sources
 * (LFO 1 / 2, ENV, VEL each at its own level, AT, MW, EXPR, M1..M4) and sums
 * the slots aimed at its five fast buses; the slots aimed at any other param
 * are applied by the sequencer, which reads the sources back
 * (ec_dsp_mod_sources).
 *
 * The fast buses (the SC mod buses, -1..1 each, clipped where they are read):
 *   pitch  -> +-12 semitones on the readers and slice voices
 *   cutoff -> +-120 semitones on the filter cutoff
 *   res    -> +-0.5 on the 0..1 resonance
 *   amp    -> amp * (1 + mod), floored at 0 (tremolo)
 *   pan    -> pan + mod, clipped -1..1
 * A slot adds source x depth to its bus. A track computes nothing while no
 * slot is live (SC needsMod: no synth).
 */
#ifndef EC_MOD_H
#define EC_MOD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { EC_MOD_PITCH = 0, EC_MOD_CUTOFF, EC_MOD_RES, EC_MOD_AMP, EC_MOD_PAN, EC_MOD_NDEST };
/* the sources the engine computes (ec_mtx.h EC_MS_LFO1 .. EC_MS_EXPR, 0-based here) */
enum { EC_MSRC_LFO1 = 0, EC_MSRC_LFO2, EC_MSRC_ENV, EC_MSRC_VEL, EC_MSRC_AT, EC_MSRC_MW, EC_MSRC_EXPR, EC_MSRC_N };
#define EC_MOD_SLOTS 8
/* LFO wave (0-based): sine, tri, saw, ramp down, square, S&H random */
enum { EC_LW_SINE = 0, EC_LW_TRI, EC_LW_SAW, EC_LW_RSAW, EC_LW_SQR, EC_LW_RAND };
/* LFO mode (0-based): FREE never resets, TRIG resets the phase on a note trig,
 * ONE runs a single cycle per trig, HOLD samples & holds the free wave at a trig */
enum { EC_LM_FREE = 0, EC_LM_TRIG, EC_LM_ONE, EC_LM_HOLD };

typedef struct ec_mod_params {
    struct { int wave, mode; float beats, depth; } lfo[2];   /* beats per cycle; depth (the level) -1..1 */
    float menv_attack, menv_decay, menv_sustain, menv_release, menv_depth;   /* s, s, 0..1, s, -1..1 */
    float mvel_depth;
    float macro_base[4];                  /* 0..1 (the VALs, the sequencer's matrix offsets included) */
    /* the matrix: src 0 = off, 1..11 = ec_mtx.h EC_MS_LFO1..EC_MS_M4; bus EC_MOD_* or -1 (a generic
     * destination: the source still runs); depth -1..1 (0: the slot is off) */
    struct { int src, bus; float depth; } slot[EC_MOD_SLOTS];
} ec_mod_params_t;

void ec_mod_params_default(ec_mod_params_t *p);
int  ec_mod_needed(const ec_mod_params_t *p);    /* some slot is live */

#ifdef __cplusplus
}
#endif
#endif
