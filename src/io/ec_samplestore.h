/* SPDX-License-Identifier: GPL-3.0-only */
/* The sample store: one fixed arena (EC_SS_ARENA_MIB, static) holding decoded samples as int16 stereo interleaved,
 * plus a fixed table of sample handles. Allocation is a bump pointer; ec_ss_free() of the newest sample gives its
 * space back, ec_ss_reset() empties the store. No malloc anywhere; the audio path only reads ready samples. */
#ifndef EC_SAMPLESTORE_H
#define EC_SAMPLESTORE_H
#include <stdint.h>

#ifndef EC_SS_ARENA_MIB
#define EC_SS_ARENA_MIB 96          /* ~9 minutes of stereo at 44.1 kHz */
#endif
#define EC_SS_MAX 128               /* sample handles */
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
ec_ss_sample_t *ec_ss_get(int h);                   /* NULL for a bad / free handle */
const ec_ss_sample_t *ec_ss_ready(int h);           /* NULL unless ready: what the audio path uses */
uint32_t ec_ss_used_bytes(void);
uint32_t ec_ss_capacity_bytes(void);
uint32_t ec_ss_free_frames(void);                /* the largest sample that still fits */

/* "loop_bpm120.wav" -> 120, "136BPM break" -> 136, "bpm 92.5" -> 92.5; 0 when none (Elasticat's
 * bpm_from_filename: "bpm" then digits, else digits then "bpm"; separators _ - space allowed between) */
float ec_bpm_from_name(const char *name);

#endif
