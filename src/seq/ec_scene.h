/* SPDX-License-Identifier: GPL-3.0-only */
/* The A/B scene crossfader (E5): a C port of Elasticat's lib/scene_store.lua plus the coordinator's crossfader
 * layer (elasticat.lua set_crossfader_override / knob_takes_over / reconcile_crossfader, BASE_VALUE_RESOLVER.md).
 *
 * Two scenes, each a sparse set of (track, param) values; the fader position 0..1 morphs every param locked in
 * either scene. A scene that does not hold a param falls back to the param's BASE, and the base is the track value
 * (the original keeps a stashed base that follows every edit, pattern load and undo, i.e. always the track value).
 * A param whose two ends are equal, or that neither scene holds, is left alone.
 *
 * Non-destructive: ec_scene_apply() publishes the morph into the runtime layer (on / xf), which the sequencer's
 * resolver reads as source 2 (step p-lock > crossfader > track value). The track value is never written. A normal
 * knob edit takes its param back from the crossfader (ec_scene_knob) until the fader next moves.
 *
 * Only continuous params morph (ec_scene_morphable): a half-morphed option is nonsense, and the structural ones
 * (lengths, slots, slice count, rates, the track volume) are excluded as the original excludes them. */
#ifndef EC_SCENE_H
#define EC_SCENE_H

#include "ec_project.h"

int ec_scene_morphable(int id);
int ec_scene_row_morphable(int row, int id);   /* rows 0..5 tracks (morphable ids), 6..8 S1 S2 MST (their FX ids) */
void ec_scene_reset(ec_scenes_t *sc);
int ec_scene_count(const ec_scenes_t *sc, int scene);           /* values the scene holds (0 = empty) */

/* FN + anchor: capture every morphable param of every track (the APPLIED value: the morph where it is published,
 * else the track value) into scene 0 (A) / 1 (B). Sparse: a param at its default is stored only when the other scene
 * holds it (and removed otherwise). */
void ec_scene_capture(ec_project_t *p, int scene);
/* hold anchor + turn: write one param of one track into the scene only (the live sound does not move) */
void ec_scene_set(ec_project_t *p, int scene, int track, int id, float value);
int ec_scene_get(const ec_project_t *p, int scene, int track, int id, float *value);   /* 1 if the scene holds it */
void ec_scene_clear(ec_project_t *p, int scene);                /* empty one scene */
void ec_scene_clear_key(ec_project_t *p, int track, int id);    /* drop (track, id) from both scenes */

/* morph to position t (0..1) and publish; returns a bitmask of the tracks whose resolved values changed */
uint32_t ec_scene_apply(ec_project_t *p, float t);
/* a hand edit of (track, id): the track value plays again until the fader next moves */
void ec_scene_knob(ec_project_t *p, int track, int id);
/* the resolver's source 2: 1 and *x when the crossfader drives (track, id) now */
static inline int ec_scene_value(const ec_project_t *p, int track, int id, float *x)
{
    if (!p->e5.sc.on[track][id])
        return 0;
    *x = p->e5.sc.xf[track][id];
    return 1;
}
/* how many (track, param) pairs the fader moves now (both ends known and different) */
int ec_scene_targets(const ec_project_t *p);

/* one eased glide step at 30 Hz toward target (lib/grid_sequencer.lua step_scene_glide): exponential at 5 /s
 * (lands within 0.004), or linear over 5 s for the whole travel with FN held. */
#define EC_GLIDE_HZ 30
float ec_scene_glide_step(float cur, float target, int slow);

#endif
