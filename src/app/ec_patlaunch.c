/* SPDX-License-Identifier: GPL-3.0-only */
/* 0.6.0 (docs/UX-V2.md §3 PATTERN LAUNCH, SEQ): 4 banks x 16 = 64 patterns. B1..B4 pick the bank, the white keys
 * launch the bank's patterns with the launch quantize from F3 (the project's LNCH MODE), F1 / F2 copy / cut and paste
 * patterns (§2). Hold SEQ or SHIFT + SEQ = F3 (which also names the pattern: PRESETS). */
#include <stdio.h>

#include "ec_app.h"

#define A (&ec_app)
#define P (&ec_app.pn)

void ec_pat_white(int i)
{
    const int p = ec_pat_index(P->bank, i);
    char b[16];
    if (ec_pn_f(1) || ec_pn_f(2)) {
        P->f_used = 3;
        ec_clip_tap(EC_CK_PATTERN, p);
        return;
    }
    ec_seq_request_pattern(&A->seq, p);
    snprintf(b, sizeof b, "%c%d", 'A' + P->bank, i + 1);
    ec_app_message(b);
    ec_app_mark_dirty();
}

void ec_pat_black(int key)
{
    if (key >= 1 && key <= EC_NBANK) {
        P->bank = key - 1;
        A->redraw = 1;
    }
}
