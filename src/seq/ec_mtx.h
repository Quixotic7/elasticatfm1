/* SPDX-License-Identifier: GPL-3.0-only */
/* 0.5.0: the mod matrix (docs/MOD-MATRIX.md). Eight slots per track, each SRC / DST / DEPTH, stored as the params
 * EC_P_MTX1_SRC .. EC_P_MTX8_DEPTH (ec_params.h).
 *
 *   SRC   option 1..12 = OFF, LFO 1, LFO 2, ENV (the mod envelope), VEL, AT (aftertouch), MW (CC 1), EXPR (CC 11),
 *         M1..M4. The LFOs are bipolar, the rest unipolar; LFO 1 / LFO 2 / ENV / VEL keep their own DEPTH param as
 *         the source's level.
 *   DST   0 OFF, else the destination param id + 1 (ec_param_mod_dest: every continuous track param). PITCH, CUT,
 *         RES, AMP and PAN ride the engine's fast mod buses (ec_mod.h); every other one is a generic destination.
 *   DEPTH -100..+100 % (lockable, morphed by the scenes).
 *
 * The scale: a generic destination moves by source x depth x its whole range (max - min) and is clamped. The fast
 * buses keep their 0.4 swing at 100 %: PITCH +-12 semitones, CUT +-120 semitones (the whole range), RES +-0.5,
 * AMP x(1 +- 1), PAN the whole width. */
#ifndef EC_MTX_H
#define EC_MTX_H

#include "ec_params.h"

#define EC_MTX_SLOTS 8

/* sources (the SRC option - 1) */
enum { EC_MS_OFF, EC_MS_LFO1, EC_MS_LFO2, EC_MS_ENV, EC_MS_VEL, EC_MS_AT, EC_MS_MW, EC_MS_EXPR,
       EC_MS_M1, EC_MS_M2, EC_MS_M3, EC_MS_M4, EC_MS_N };
#define EC_MS_LIVE 7          /* the sources the engine computes: LFO1 .. EXPR (src[0] = LFO1); M1..M4 are params */

static inline int ec_mtx_src_id(int k) { return EC_P_MTX1_SRC + 3 * k; }
static inline int ec_mtx_dst_id(int k) { return EC_P_MTX1_DST + 3 * k; }
static inline int ec_mtx_depth_id(int k) { return EC_P_MTX1_DEPTH + 3 * k; }
/* a matrix param id -> its slot (0..7) and column (0 SRC, 1 DST, 2 DEPTH); -1 when id is not one */
int ec_mtx_slot_of(int id, int *col);

int ec_mtx_src(const float *v, int k);       /* EC_MS_* */
int ec_mtx_dst(const float *v, int k);       /* the destination param id, -1 OFF */
float ec_mtx_depth(const float *v, int k);   /* -1..1 */
/* the slot is live: a source, a destination, a depth */
int ec_mtx_live(const float *v, int k);
/* the fast bus (ec_mod.h EC_MOD_PITCH..PAN) of a destination param, -1 a generic one */
int ec_mtx_bus(int id);
const char *ec_mtx_src_name(int src);        /* "LFO1" .. ("" for OFF) */
/* the source's own level param (LFO DEPTH, ENV DEPTH, VEL DEPTH), -1 none */
int ec_mtx_level_id(int src);

/* the slot with src -> dst, else -1; the first free slot (SRC OFF), else -1 */
int ec_mtx_find(const float *v, int src, int dst);
int ec_mtx_free(const float *v);

/* generic destinations: 1 when some live slot has one */
int ec_mtx_generic(const float *v);
/* v: a track's resolved values; live: the engine's sources (EC_MS_LIVE values, LFO1 first). Adds every generic
 * slot's offset to v, clamped (the macro VALs first: M1..M4 as sources read the modulated values). */
void ec_mtx_apply(float *v, const float *live);
/* the value of every source for a track: live from the engine, M1..M4 from v (0..1) */
void ec_mtx_sources(const float *v, const float *live, float *src /* EC_MS_N */);

/* 0.4 -> 0.5: the old fixed routes (LFO 1 / LFO 2 / ENV / VEL dest, the 20 macro depths) into slots, in that order,
 * at the same strength; the old params back to their defaults. Returns the routes dropped (past 8 slots). */
int ec_mtx_migrate(float *v);
/* an old macro depth (id EC_P_MACRO_DEPTH + m * 5 + d, value 0..128): the slot of v (after ec_mtx_migrate) that
 * carries that route (-1 none) and *depth, the value as that slot's DEPTH (the scenes' values move with it) */
int ec_mtx_migrated_slot(const float *v, int old_id, float old_value, float *depth);

#endif
