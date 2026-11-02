/* SPDX-License-Identifier: GPL-3.0-only */
/* The E0 screen ("the pipe"): the ELASTICAT header in the mockup's style, the loaded file, its waveform with the
 * playhead, a 16-step ruler, VOL / STATE cells, the last input event and the file cursor. Pure drawing: the app fills
 * an ec_e0_view_t and calls ec_screen_e0_draw() inside frame. */
#ifndef EC_SCREEN_E0_H
#define EC_SCREEN_E0_H
#include <stdint.h>

typedef struct {
    const char *file;            /* the loaded sample's name ("" none) */
    const char *message;         /* instead of the waveform: "NO FOLDER", "ERROR: NOT FOUND" (NULL: none) */
    int load_permille;           /* 0..1000 while loading, -1 not loading */
    const uint8_t *overview;     /* EC_SS_OVERVIEW peak columns 0..255, NULL none */
    int overview_n;
    float pos;                   /* playhead 0..1, < 0 none */
    int playing;
    int step;                    /* 0..15, -1 none */
    int vol_pct;                 /* 0..100 */
    float bpm;                   /* 0: unknown */
    uint32_t rate;               /* the sample's rate (Hz), 0 none */
    int channels;
    int cursor, nfiles;          /* the browser's cursor (0-based) and file count */
    const char *cursor_name;     /* the file under the cursor */
    int cursor_loaded;           /* 1: the cursor is on the loaded file */
    const char *last_event;      /* "BTN PLAY", "KEY L05", "K1 +1", "MASTER 80%" */
    const char *files_note;      /* the FILE line when there are no files ("NO FOLDER"; NULL: "NONE") */
} ec_e0_view_t;

void ec_screen_e0_draw(const ec_e0_view_t *v);   /* the whole screen (ec_gfx_begin first) */

#endif
