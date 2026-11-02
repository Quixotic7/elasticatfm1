/* SPDX-License-Identifier: GPL-3.0-only */
/* E3: the razor machines' slice table (razor_slice / razor_poly) and the per-slice settings of every slice machine.
 *
 * The original keeps 32 razor slices as global params razor_NN_start / razor_NN_end (one table shared by the tracks,
 * "per-track razor deferred"); here every track has its own table, kept outside the patterns like the sample pool
 * and saved in the project record's "RZR1" chunk. Points are TRACK SPACE 0..128 (the slot trim x the track Range),
 * the same space as loop_start / loop_end and the engine's slice ranges.
 *
 * Per slice (all slice machines; the original's TrackSequencer slice_choke_map / slice_mute_map / slice locks):
 * choke group, mute, play mode (0 = the track's slice_play_mode) and a pitch offset.
 *
 * Also the sample analysis of lib/wav_reader.lua: the onset envelope + transient picking (Auto-Chop, transient snap)
 * and the nearest zero crossing (zero-x snap). Platform free: it works on the int16 sample data. */
#ifndef EC_RAZOR_H
#define EC_RAZOR_H
#include <stdint.h>

#include "ec_dsp.h"

#define EC_RZ_SLICES 32
#define EC_RZ_ONSET_WINDOWS 512     /* read_wav_onsets' default */
#define EC_RAZOR_TAG 0x31525A52u    /* "RZR1" */

/* razor snap (params "slice_snap"): what a STRT / END turn snaps to */
enum { EC_SNAP_GRID = 1, EC_SNAP_ZOOM, EC_SNAP_ZEROX, EC_SNAP_TRANSIENT, EC_SNAP_FRIENDS, EC_NSNAP = 5 };

typedef struct {
    float start[EC_RZ_SLICES], end[EC_RZ_SLICES];   /* track space 0..128 */
    int8_t choke[EC_RZ_SLICES];                     /* 0 none, 1..8 */
    uint8_t mute[EC_RZ_SLICES];
    int8_t play[EC_RZ_SLICES];                      /* 0 the track's, else EC_SLICE_* */
    float pitch[EC_RZ_SLICES];                      /* st offset -24..24 */
    uint8_t snap;                                   /* EC_SNAP_* */
    uint8_t sens;                                   /* chop sensitivity 0..100 (slice_chop_sense) */
} ec_razor_t;

struct ec_project;   /* (ec_project.h includes this header) */

void ec_razor_init(ec_razor_t *rz);                 /* 16 even slices (17..32: the 32-grid of razor_reset) */
void ec_razor_redistribute(ec_razor_t *rz, int count);
int ec_razor_is_razor(int machine);
/* a slice's range in track space for any slice machine (grid: the slice_count division of 0..128) */
void ec_razor_range(const struct ec_project *p, int track, int slice, float *s, float *e);
/* move one point of a slice (which: 0 start, 1 end) to x, keeping end > start (+0.01, the owner's clamp);
 * friends: also drags the touching neighbour (previous end / next start, count = the slices in use) */
void ec_razor_set_point(ec_razor_t *rz, int slice, int which, float x, int friends, int count);
/* split slice `slice` at x (track space) into two: the slices after it move up one, the track's steps follow;
 * returns the new slice count (unchanged when 32 already) */
int ec_razor_add(struct ec_project *p, int track, int slice, float x);
/* delete slice `slice`: the ones after it move down, steps follow; returns the new count (>= 1) */
int ec_razor_delete(struct ec_project *p, int track, int slice);

/* the sequencer's hook for one slice event (EC_EV_TRIG / EC_EV_SLICE_ON of a slice machine): range (razor),
 * choke, pitch offset, play mode and its gate length. note = the step's note length in samples, or -1 for a held
 * pad (a long gate until the key comes up). Returns 0 when the slice is muted (send nothing). */
int ec_razor_slice_event(const struct ec_project *p, int track, ec_event_t *e, int64_t note);
int ec_razor_play_mode(const struct ec_project *p, int track, int slice);   /* the effective EC_SLICE_* */

/* analysis (lib/wav_reader.lua): data = interleaved int16 L,R */
typedef struct {
    float rise[EC_RZ_ONSET_WINDOWS];
    float max_rise, mean_rise;
    uint32_t frames;
    float rate;
    int valid;
} ec_onsets_t;
void ec_razor_onsets(const int16_t *data, uint32_t frames, float rate, ec_onsets_t *out);
/* transients (0..1 of the whole sample, ascending) at sensitivity 0..1; returns the count (<= cap) */
int ec_razor_transients(const ec_onsets_t *on, float sens, float *pos01, int cap);
/* the nearest zero crossing (channel 1) to pos01 within window_s seconds, else pos01 */
double ec_razor_zero_cross(const int16_t *data, uint32_t frames, float rate, double pos01, double window_s);
/* Auto-Chop: one slice per transient inside the track window [lo01, hi01] of the sample (the pre-first-onset head
 * left out, the last to the end), the track's slice_count set to match. Returns the count (0: none, unchanged). */
int ec_razor_autochop(struct ec_project *p, int track, const float *pos01, int n, double lo01, double hi01);

/* the record chunk (ec_project_save / _load) */
int32_t ec_razor_save_chunk(const struct ec_project *p, uint8_t *dst, uint32_t cap);
void ec_razor_load_chunk(struct ec_project *p, const uint8_t *src, uint32_t len);

#endif
