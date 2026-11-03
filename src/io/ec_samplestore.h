/* SPDX-License-Identifier: GPL-3.0-only */
/* The sample store: one fixed arena (EC_SS_ARENA_MIB, static) holding decoded samples as int16 stereo interleaved,
 * plus a fixed table of sample handles. Allocation is a bump pointer; ec_ss_free() of the newest sample gives its
 * space back, ec_ss_reset() empties the store. No malloc anywhere; the audio path only reads ready samples. */
#ifndef EC_SAMPLESTORE_H
#define EC_SAMPLESTORE_H
#include <stdint.h>

/* 0.6.0: the arena for MEMORY_MIB 256 (wasm32 linear memory; sizes measured on the native and the wasm32 build):
 *   engine (EC_DSP_MEM_MAX)                        18.0 MiB
 *   ec_app_t (holds the 64-pattern ec_project_t)    9.4
 *   the app's memorize record (src/app/ec_e5.c g_memo: ec_e5_memo_save into EC_PLAT_RECORD_MAX)   1.0
 *   the flash image (ec_core.c g_flash; 0.6.0 stages into it: no second copy)   1.0
 *   0.6.1: the project file's pack buffer (src/app/ec_projfile.c g_pack, EC_PF_PACK_MAX: the largest project)   11.0
 *   the loader's read buffer (ec_loader.c, one FS request)   1.0
 *   every other static + read-only data (screens, tables, lists; 0.45 measured)   <= 1.0
 *   the stack                                       1.0
 *   free above the stack top for the host shim's heap (its flash staging copy, audio blocks) and a margin   >= 4.0
 *   = 47.4 MiB, so the arena gets 256 - 47.4 = 208 MiB (~20.6 min of stereo at 44.1 kHz). (0.6.0, projects in the
 * flash: 219. Before memorize became a record, a whole 9.1 MiB ec_project_t copy: 44.5 MiB and 211. 0.5.0,
 * MEMORY_MIB 128: 96 MiB.) Hosts that build smaller (the designer) set their own. */
#ifndef EC_SS_ARENA_MIB
#define EC_SS_ARENA_MIB 208         /* ~20.6 minutes of stereo at 44.1 kHz */
#endif
#define EC_SS_MAX 136               /* sample handles: the 128 pool slots, a take, a preview and some slack */
#define EC_SS_NAME 64               /* a sample's display name (the file name) */
#define EC_SS_PATH 256              /* its virtual path (/audio/..) */
#define EC_SS_OVERVIEW 128          /* columns of the waveform overview */

typedef struct {
    int16_t *data;                  /* frames * 2 int16 (L R L R ..) */
    uint32_t frames;                /* frames allocated (the file's length) */
    uint32_t frames_ready;          /* frames decoded so far (== frames when ready) */
    uint32_t rate;                  /* the file's sample rate (the reader plays it at rate / EC_FS speed) */
    uint8_t channels;               /* the file's channel count (1 or 2; more are folded to 2) */
    uint8_t bits;                   /* the file's bits per sample */
    uint8_t is_float;
    uint8_t ready;                  /* 1: fully decoded, safe to play */
    float bpm;                      /* from the file name ("break_bpm136", "136bpm"), 0 none */
    char name[EC_SS_NAME];
    char path[EC_SS_PATH];
    uint8_t overview[EC_SS_OVERVIEW];   /* peak |sample| per column, 0..255 (filled while decoding) */
} ec_ss_sample_t;

void ec_ss_reset(void);                         /* forget every sample */
/* a new handle with room for frames (stereo int16); -1 if no handle or no room. Not ready until the loader says */
int ec_ss_alloc(uint32_t frames);
void ec_ss_free(int h);                          /* forget it; its space comes back only if it was the newest */
/* E6: keep only its first frames (a recording that reserved more than it took): the rest comes back if it is the
 * newest. Returns 0, -1 for a bad handle or more frames than it has */
int ec_ss_shrink(int h, uint32_t frames);
ec_ss_sample_t *ec_ss_get(int h);                   /* NULL for a bad / free handle */
const ec_ss_sample_t *ec_ss_ready(int h);           /* NULL unless ready: what the audio path uses */
uint32_t ec_ss_used_bytes(void);                 /* the arena's top: live samples + the holes freed below it */
uint32_t ec_ss_capacity_bytes(void);
uint32_t ec_ss_free_frames(void);                /* the largest sample that still fits (above the top) */
/* 0.6.0: the pool meter and freeing slots. A freed sample that is not the newest leaves a hole (its bytes come back
 * on ec_ss_compact or ec_ss_reset). The live bytes are what the meter shows as used */
uint32_t ec_ss_live_bytes(void);                 /* the samples held */
uint32_t ec_ss_hole_bytes(void);                 /* used - live: what ec_ss_compact gives back */
int ec_ss_count(void);                           /* handles in use */
/* slide every sample down over the holes (in arena order); handles keep their numbers, their data pointers move.
 * Returns the bytes given back. The caller first stops everything that reads or writes the arena (the engine's
 * tracks: ec_dsp_set_sample(t, 0); no load or FS write in flight), then points the tracks at ec_ss_ready(h) again.
 * A memmove of at most the arena (~0.1-0.3 s for 200 MiB in wasm): no reloading from files */
uint32_t ec_ss_compact(void);

/* "loop_bpm120.wav" -> 120, "136BPM break" -> 136, "bpm 92.5" -> 92.5; 0 when none (Elasticat's
 * bpm_from_filename: "bpm" then digits, else digits then "bpm"; separators _ - space allowed between) */
float ec_bpm_from_name(const char *name);

#endif
