/* SPDX-License-Identifier: GPL-3.0-only */
/* E5 on the panel (src/app/ec_e5.c): the scene row and the glide, copy / clear / paste with their scopes, the PAGE
 * overlay (page select / page loop / pattern rate), memorize / recall, quick undo, pattern and project naming, the
 * slice programs' gestures, step preview. docs/FM1-CONTROLS.md "E5" is the player's reference. ec_app.c calls the
 * hooks below; each returns 1 when it consumed the input. */
#ifndef EC_E5APP_H
#define EC_E5APP_H
#include <stdint.h>

#include "ec_e5.h"
#include "ec_pages.h"
#include "ec_scene.h"

enum { EC_PM_SELECT, EC_PM_LOOP, EC_PM_RATE, EC_NPM };       /* the PAGE overlay's modes */
enum { EC_NM_NONE, EC_NM_PATTERN, EC_NM_PROJECT };           /* what the naming screen names */

typedef struct {
    /* scenes */
    int anchor;                  /* the scene anchor held: 0 none, 1 A, 2 B (0.6.0: CROSSFADER's B1 / B2) */
    uint8_t glide_held;          /* (0.6.0: CROSSFADER's W key held: bit 0) */
    int gliding;                 /* 0.6.0: CROSSFADER's pressure glide (src/app/ec_xfd.c) */
    float glide_target;
    uint32_t glide_ms;
    int confirm;                 /* the pattern clear's question is open (0.3.0: OCT+ yes, OCT- no) */
    uint32_t confirm_ms;
    /* clipboards: each scope its own */
    ec_clip_steps_t clip_steps;
    ec_clip_page_t clip_page;
    ec_clip_pattern_t clip_pat;
    ec_clip_slice_t clip_slice;
    /* undo, memorize */
    ec_undo_t undo;
    int memo_ok;
    /* naming */
    int naming;                  /* EC_NM_* */
    int name_pat;
    char name[EC_NAME];          /* 16 characters, space padded while editing */
    int name_cur;
    uint32_t rng;
    /* step preview (stopped) */
    int preview;                 /* 0 none, 1 slices, 2 loop keys */
    uint32_t preview_bits;
    int preview_track;
    uint32_t key_hi;             /* lower keys (bit k) pressed as loop keys / pads 17-32 */
    /* toast */
    char toast[28];
    uint32_t toast_ms;
    int toast_keep;              /* stays until cleared (the clear prompt) */
} ec_e5app_t;

/* the hooks (ec_app.c) */
void ec_e5_init(void);
void ec_e5_tick(void);
void ec_e5_answer(int yes);                              /* 0.3.0: the open question */
void ec_e5_undo(int redo);                               /* 0.6.0: SHIFT + F1 / F2 */
void ec_e5_memorize(void);                               /* 0.6.0: SHIFT + B3 */
void ec_e5_recall_memo(void);                            /* 0.6.0: SHIFT + B4 */
void ec_e5_name_pattern(void);                           /* 0.6.0: F3 + PRESETS */
void ec_e5_xf_apply(float pos);                          /* 0.6.0: the crossfader to pos (CROSSFADER's glide) */
void ec_e5_name_close(int accept);                       /* 0.3.0: naming: OCT+ keeps, OCT- / HOME cancel */
void ec_e5_page_key(int i, int mode);                    /* 0.3.0: F3's lower keys (EC_PM_SELECT / EC_PM_LOOP) */
int ec_e5_lower_down(int k, int i, int vel);
int ec_e5_lower_up(int k, int i);
int ec_e5_upper(int i, int down);
int ec_e5_knob(int role, int det);                       /* SELECT / ALGO / PRESETS / K1..K4 before the page */
int ec_e5_knob_cell(const ec_item_t *it, int det, int mult);   /* a cell turn: scene / slice edits, E5 items */
void ec_e5_step_down(int i);                             /* a step key went down (preview while stopped) */
void ec_e5_step_up(void);
void ec_e5_edit_param(int track, int id, int plock);    /* before an edit: undo, the knob takes over */
void ec_e5_edit_global(int id, float old);
void ec_e5_steps_edit(uint16_t mask, const char *label, uint32_t key);   /* before a step edit on the step page */
int ec_e5_item_value(const ec_item_t *it, float *x, int *locked);       /* a cell's value under a held anchor / pad */
void ec_e5_item_format(const ec_item_t *it, float x, char *out, int cap);
float ec_e5_item_frac(const ec_item_t *it, float x);
int ec_e5_leds_lower(void);
void ec_e5_leds(void);
void ec_e5_ui_info(char *buf, uint32_t n);
int ec_e5_loop_key(int k, int i, int down);             /* the loop key / pad 1..32 of white key i (the zoom) */
int ec_e5_scene_layer(void);                             /* (0.6.0: always 0, the scene row is gone) */
int ec_e5_page_visible(void);
const char *ec_e5_pattern_name(int idx);                 /* "" when unnamed */
float ec_e5_rate_of_key(int i);                          /* PAGE > RATE: key i -> the rate index */

#endif
