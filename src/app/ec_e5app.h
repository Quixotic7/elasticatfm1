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
enum { EC_NM_NONE, EC_NM_PATTERN, EC_NM_PROJECT, EC_NM_FILE };   /* what the naming screen names (0.6.47: a file) */

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
#define EC_NAME_KEYBED 14         /* 0.6.48 (owner): the keybed types / rolls 14 characters (one font size on screen) */
    char name[EC_NAME];          /* 0.6.47: the name as typed (no padding); 0.6.48: 14 typed (16 kept from before) */
    int name_cur;                /* the cursor: typing inserts there */
    uint32_t rng;
    int name_sel;                /* 0.6.47 (owner, FIZZ): the suggested / rolled name is selected: a key replaces it */
    int name_rolled;             /* the die shows (a rolled name) */
    char name_hist[10][EC_NAME]; /* the last ten names seen (PRESETS left walks back) */
    int name_nh, name_hp;
    int name_key;                /* the key typed last (0..10 B1..B11, 11..26 W1..W16), -1; its glow */
    uint32_t name_key_ms;
    char name_file[32];          /* EC_NM_FILE: the project file renamed (EC_PF_NAME) */
    uint32_t name_roll_ms;       /* 0.6.48 (owner): the die's tumble (from this roll) */
    int name_face;               /* the face it lands on (1..6) */
    /* step preview (stopped) */
    int preview;                 /* 0 none, 1 slices, 2 loop keys, 3 a note (0.6.59: ALGO's step, its pitch) */
    int preview_semis;
    uint32_t preview_until;      /* 0.6.59: a timed preview's end (ALGO), 0 held */
    uint32_t preview_bits;
    int preview_track;
    uint32_t key_hi;             /* lower keys (bit k) pressed as loop keys / pads 17-32 */
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
void ec_e5_name_close(int accept);                       /* 0.3.0: naming: OCT+ keeps, HOME cancels */
void ec_e5_name_file(const char *file);                  /* 0.6.47: the keybed on a project file's name (RENAME) */
void ec_e5_name_back(int clear);                         /* 0.6.47: OCT-: delete (empty: cancel); SHIFT: clear */
const char *ec_e5_name_key_char(int upper, int i, int shift);   /* 0.6.47: the keybed's character on a key */
void ec_e5_page_key(int i, int mode);                    /* 0.3.0: F3's lower keys (EC_PM_SELECT / EC_PM_LOOP) */
int ec_e5_lower_down(int k, int i, int vel);
int ec_e5_lower_up(int k, int i);
int ec_e5_upper(int i, int down);
int ec_e5_knob(int role, int det);                       /* SELECT / ALGO / PRESETS / K1..K4 before the page */
int ec_e5_knob_cell(const ec_item_t *it, int det, int mult);   /* a cell turn: scene / slice edits, E5 items */
void ec_e5_step_down(int i);                             /* a step key went down (preview while stopped) */
void ec_e5_step_up(void);
void ec_e5_step_preview(int idx);                       /* 0.6.59: ALGO, stopped: step idx heard once */
void ec_e5_edit_param(int track, int id, int plock);    /* before an edit: undo, the knob takes over */
void ec_e5_edit_params(int track, const uint8_t *ids, int n, const char *label);   /* 0.6.1: several as one undo */
void ec_e5_edit_global(int id, float old);
void ec_e5_steps_edit(uint16_t mask, const char *label, uint32_t key);   /* before a step edit on the step page */
int ec_e5_item_value(const ec_item_t *it, float *x, int *locked);       /* a cell's value under a held anchor / pad */
void ec_e5_item_format(const ec_item_t *it, float x, char *out, int cap);
float ec_e5_item_frac(const ec_item_t *it, float x);
int ec_e5_leds_lower(void);
void ec_e5_ui_info(char *buf, uint32_t n);
int ec_e5_loop_key(int k, int i, int down);             /* the loop key / pad 1..32 of white key i (the zoom) */
int ec_e5_scene_layer(void);                             /* (0.6.0: always 0, the scene row is gone) */
int ec_e5_page_visible(void);
const char *ec_e5_pattern_name(int idx);                 /* "" when unnamed */
float ec_e5_rate_of_key(int i);                          /* PAGE > RATE: key i -> the rate index */

#endif
