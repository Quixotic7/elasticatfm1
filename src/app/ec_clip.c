/* SPDX-License-Identifier: GPL-3.0-only */
/* 0.6.0, docs/UX-V2.md §2: copy / cut / paste with a one-step buffer, while F1 (copy) or F2 (cut) is on.
 *   The first tap takes the thing into the buffer (cut: and empties it). The next tap pastes it there, onto anything.
 *   After that a tap on something that isn't empty takes it (a new buffer), a tap on an empty one pastes.
 *   The buffer empties when F1 / F2 turns off. Steps (the white keys), PAGES' pages (the black keys) and PATTERN
 *   LAUNCH's patterns (the white keys, the bank's) work alike; each paste and cut of steps / pages is one undo. */
#include <stdio.h>
#include <string.h>

#include "ec_app.h"

#define A (&ec_app)
#define P (&ec_app.pn)
#define E (&ec_app.e5)

static void toast(const char *s) { ec_app_message(s); }   /* 0.6.1: a message in the header (no box) */

void ec_clip_reset(void)
{
    P->clip_state = EC_CLIP_EMPTY;
    P->clip_from = -1;
}

static ec_ptrack_t *tr(void) { return ec_project_track(&A->proj, A->track); }

static int page_empty(int page)
{
    int i;
    for (i = 0; i < 16; i++)
        if (ec_step_has_content(&tr()->steps[page * 16 + i]))
            return 0;
    return 1;
}

static int is_empty(int kind, int i)
{
    switch (kind) {
    case EC_CK_STEP: return !ec_step_has_content(&tr()->steps[i]);
    case EC_CK_PAGE: return page_empty(i);
    default: return !A->proj.pat[i].used;
    }
}

static void undo_steps(int index, uint16_t mask, const char *label)
{
    ec_undo_steps(&E->undo, &A->proj, A->track, index, mask, label, 0xC00u + (uint32_t)index, A->ms);
    E->undo.last_key = 0;                          /* (every paste its own undo) */
}

static void take(int kind, int i, int cut)
{
    char b[28];
    switch (kind) {
    case EC_CK_STEP:
        P->clip_step = tr()->steps[i];
        if (cut) {
            undo_steps(i / 16, (uint16_t)(1u << (i % 16)), "CUT");
            ec_step_clear(&tr()->steps[i]);
        }
        snprintf(b, sizeof b, "%s STEP %d", cut ? "CUT" : "COPY", i + 1);
        break;
    case EC_CK_PAGE:
        ec_clip_copy_page(&E->clip_page, tr(), i);
        if (cut) {
            undo_steps(i, 0xFFFFu, "CUT PAGE");
            ec_clip_clear_page(tr(), i);
        }
        snprintf(b, sizeof b, "%s PAGE %d", cut ? "CUT" : "COPY", i + 1);
        break;
    default:
        ec_clip_copy_pattern(&E->clip_pat, &A->proj, i);
        if (cut) {
            ec_undo_blob(&E->undo, &A->proj, EC_UB_PATTERN, i, -1, "CUT PAT", 0, A->ms);   /* 0.6.55 (QA B6) */
            ec_clip_clear_pattern(&A->proj, i);
        }
        snprintf(b, sizeof b, "%s %c%d", cut ? "CUT" : "COPY", 'A' + i / EC_BANK_PATS, i % EC_BANK_PATS + 1);
        break;
    }
    toast(b);
}

static void put(int kind, int i)
{
    char b[28];
    switch (kind) {
    case EC_CK_STEP:
        undo_steps(i / 16, (uint16_t)(1u << (i % 16)), "PASTE");
        tr()->steps[i] = P->clip_step;
        snprintf(b, sizeof b, "PASTE STEP %d", i + 1);
        break;
    case EC_CK_PAGE:
        undo_steps(i, 0xFFFFu, "PST PAGE");
        ec_clip_paste_page(&E->clip_page, tr(), i);
        snprintf(b, sizeof b, "PASTE PAGE %d", i + 1);
        break;
    default:
        ec_undo_blob(&E->undo, &A->proj, EC_UB_PATTERN, i, -1, "PST PAT", 0, A->ms);   /* 0.6.55 (QA B6) */
        ec_clip_paste_pattern(&E->clip_pat, &A->proj, i);
        if (i == A->proj.current) {
            ec_e5_pattern_loaded(&A->seq);       /* its BPM, the scenes against its values */
            ec_seq_timing_changed(&A->seq);
        }
        snprintf(b, sizeof b, "PASTE %c%d", 'A' + i / EC_BANK_PATS, i % EC_BANK_PATS + 1);
        break;
    }
    toast(b);
}

void ec_clip_tap(int kind, int i)
{
    const int cut = !ec_pn_f(1) && ec_pn_f(2);
    if (P->clip_state != EC_CLIP_EMPTY && P->clip_kind != kind)
        ec_clip_reset();
    switch (P->clip_state) {
    case EC_CLIP_EMPTY:
        take(kind, i, cut);
        P->clip_kind = kind;
        P->clip_cut = cut;
        P->clip_from = i;
        P->clip_state = EC_CLIP_FRESH;
        break;
    case EC_CLIP_FRESH:                            /* the next tap pastes, onto anything */
        put(kind, i);
        P->clip_state = EC_CLIP_SMART;
        break;
    default:                                       /* then: empty = paste, else take (a new buffer) */
        if (is_empty(kind, i))
            put(kind, i);
        else {
            take(kind, i, cut);
            P->clip_cut = cut;
            P->clip_from = i;
        }
        break;
    }
    ec_seq_touch_all(&A->seq);
    ec_app_mark_dirty();
}
