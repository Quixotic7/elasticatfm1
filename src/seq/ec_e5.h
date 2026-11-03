/* SPDX-License-Identifier: GPL-3.0-only */
/* E5, the sequencer / model side: the project's E5 chunk, per-pattern BPM and names, page loops (PAGE > LOOP), the
 * slice programs (per-slice p-locks, choke group, mute: lib/sequencer/track_sequencer.lua), the copy / clear / paste
 * clipboards (lib/grid_sequencer.lua "Copy / Clear / Paste"), the namesizer, the quick-undo stack (lib/undo.lua) and
 * memorize / recall. Platform free, like the rest of src/seq. The scenes are ec_scene.h. */
#ifndef EC_E5_H
#define EC_E5_H
#include <stdint.h>

#include "ec_project.h"
#include "ec_seq.h"

#define EC_CHUNK_E5 0x35354345u    /* "EC55": the record's E5 chunk */

/* ---- the project (ec_project.c calls these) ---- */
void ec_e5_project_init(ec_project_t *p);
void ec_e5_pattern_prepared(ec_project_t *p, int idx);     /* a never-used slot became a pattern */
void ec_e5_load_begin(ec_project_t *p);                    /* the defaults an E2 record implies */
int32_t ec_e5_chunk_save(const ec_project_t *p, uint8_t *buf, uint32_t cap);   /* payload bytes, -1 full */
int ec_e5_chunk_load(ec_project_t *p, const uint8_t *buf, uint32_t n);         /* 0 ok, -1 bad (partial) */

/* ---- page loop: the 0-based step index the clock position plays (pages outside the loop are skipped) ---- */
int ec_e5_play_index(const ec_project_t *p, int t, int steps, int64_t pos, int *pass);
/* PAGE > LOOP tap: toggle page (0-based) in the loop; never leaves it empty (a full set is stored as 0) */
void ec_e5_toggle_page_loop(ec_project_t *p, int t, int page, int pages);
int ec_e5_page_in_loop(const ec_project_t *p, int t, int page, int pages);

/* ---- slice programs ---- */
static inline ec_slicemap_t *ec_e5_slices(ec_project_t *p, int t) { return &p->pe5[p->current].slices[t]; }
int ec_slice_lock_get(const ec_slicemap_t *m, int slice, int id, float *val);    /* slice 1..32 */
int ec_slice_lock_set(ec_slicemap_t *m, int slice, int id, float val);           /* 0 ok, -1 full */
void ec_slice_clear(ec_slicemap_t *m, int slice);                                /* locks, choke, mute */
int ec_slice_has_program(const ec_slicemap_t *m, int slice);

/* ---- the sequencer's hooks (ec_seq.c) ---- */
void ec_e5_pattern_loaded(ec_seq_t *s);               /* the pattern's BPM, the scenes re-applied */
void ec_e5_bpm_changed(ec_project_t *p, float bpm);        /* the current pattern keeps its BPM */

/* ---- clipboards: each scope its own buffer ---- */
typedef struct {
    int n;                         /* 0 = empty */
    int8_t off[16];                /* offset from the lowest copied step */
    ec_step_t st[16];
} ec_clip_steps_t;
typedef struct {
    int ok;
    ec_step_t st[16];
} ec_clip_page_t;
typedef struct {
    int ok;
    ec_pattern_t pat;
    ec_pat_e5_t pe5;
} ec_clip_pattern_t;
typedef struct {
    int ok;
    uint8_t choke, mute, nlock;
    uint8_t lock_id[EC_SLICE_LOCKS];
    float lock_val[EC_SLICE_LOCKS];
} ec_clip_slice_t;

/* steps on step page `page` of a track; held: bit i = step page*16+i. Returns the steps copied / pasted */
int ec_clip_copy_steps(ec_clip_steps_t *c, const ec_ptrack_t *tr, int page, uint16_t held);
int ec_clip_paste_steps(const ec_clip_steps_t *c, ec_ptrack_t *tr, int page, uint16_t held);
uint16_t ec_clip_paste_mask(const ec_clip_steps_t *c, uint16_t held);   /* the steps a paste writes (undo) */
void ec_clip_clear_locks(ec_ptrack_t *tr, int page, uint16_t held);   /* keep the trig + slices */
void ec_clip_copy_page(ec_clip_page_t *c, const ec_ptrack_t *tr, int page);
void ec_clip_paste_page(const ec_clip_page_t *c, ec_ptrack_t *tr, int page);
void ec_clip_clear_page(ec_ptrack_t *tr, int page);
void ec_clip_copy_pattern(ec_clip_pattern_t *c, const ec_project_t *p, int idx);
void ec_clip_paste_pattern(const ec_clip_pattern_t *c, ec_project_t *p, int idx);   /* the name stays */
void ec_clip_clear_pattern(ec_project_t *p, int idx);     /* the trigs only (steps of every track) */
void ec_clip_copy_slice(ec_clip_slice_t *c, const ec_slicemap_t *m, int slice);
void ec_clip_paste_slice(const ec_clip_slice_t *c, ec_slicemap_t *m, int slice);

/* ---- names ---- */
#define EC_NAME_CHARS " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.!#+&"
void ec_e5_random_name(char *out, int cap, uint32_t *rng);   /* namesizer: "descriptor thing" (<= 16) */

/* ---- quick undo (lib/undo.lua): gesture-coalesced, 16 deep ---- */
enum { EC_U_PARAM = 1, EC_U_GLOBAL, EC_U_XFADE, EC_U_STEPS };
typedef struct {
    uint8_t kind;
    int8_t track, pat;
    uint8_t id;
    float val;
    uint16_t mask;                 /* EC_U_STEPS: the steps of page `id` saved in st[] */
    ec_step_t st[16];
    uint32_t key, ms;
    char label[12];
} ec_undo_entry_t;
#define EC_UNDO_DEPTH 16
#define EC_UNDO_COALESCE_MS 1000
typedef struct {
    ec_undo_entry_t e[EC_UNDO_DEPTH];
    int n;
    uint32_t last_key, last_ms;
    int last_ok;
    ec_undo_entry_t r[EC_UNDO_DEPTH];   /* 0.3.0: redo (what an undo replaced); a new edit empties it */
    int nr;
    float (*gget)(int id);              /* the app's current value of global id (redo of a global), or NULL */
} ec_undo_t;
void ec_undo_clear(ec_undo_t *u);
/* start (or extend: same key inside the window) a gesture; returns the new entry to fill, or 0 when coalesced */
ec_undo_entry_t *ec_undo_record(ec_undo_t *u, uint32_t key, uint32_t ms);
void ec_undo_param(ec_undo_t *u, const ec_project_t *p, int track, int id, uint32_t ms);
void ec_undo_global(ec_undo_t *u, int id, float old, const char *label, uint32_t ms);
void ec_undo_xfade(ec_undo_t *u, const ec_project_t *p, uint32_t ms);
void ec_undo_steps(ec_undo_t *u, const ec_project_t *p, int track, int page, uint16_t mask, const char *label,
                   uint32_t key, uint32_t ms);
/* pop the last entry into the project; returns its kind (0: nothing to undo). EC_U_GLOBAL: *gid / *gval are the
 * global's id and value for the caller to apply (the project globals belong to the app) */
int ec_undo_pop(ec_undo_t *u, ec_project_t *p, int *track, int *gid, float *gval, char *label, int cap);
/* 0.3.0: redo the last undone entry (the same out-params as ec_undo_pop); 0: nothing to redo */
int ec_undo_redo(ec_undo_t *u, ec_project_t *p, int *track, int *gid, float *gval, char *label, int cap);

/* ---- memorize / recall: the whole project to RAM and back (sample paths compared by the caller) ---- */
void ec_e5_recall(ec_project_t *dst, const ec_project_t *memo);
/* 0.6.0: memorize as a record instead of a second ec_project_t (9.1 MiB with 64 patterns): the project's record
 * (ec_project_save, at most EC_PLAT_RECORD_MAX = 1 MiB - 4 KiB, ~0.3 MiB for a dense project) in the caller's buffer.
 * Returns the bytes, -1 when it does not fit (nothing memorized). Recall loads it back and re-applies the scenes:
 * 0 ok, -1 a bad buffer. The caller keeps what it compares across a recall (the slot paths) */
int32_t ec_e5_memo_save(const ec_project_t *p, uint8_t *buf, uint32_t cap);
int ec_e5_memo_recall(ec_project_t *dst, const uint8_t *memo, uint32_t n);

#endif
