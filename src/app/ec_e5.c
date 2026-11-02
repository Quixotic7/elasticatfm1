/* SPDX-License-Identifier: GPL-3.0-only */
/* E5 on the panel (ec_e5app.h). The gestures, as docs/FM1-CONTROLS.md "E5" lists them:
 *   scene row (OCT to SCENE, or the MST > SCENE page): lower keys 1 / 9 = scene A, 8 / 16 = scene B, 2-7 / 10-15 =
 *     the six glide positions A..B. Hold an anchor + turn a knob = that param into the scene; FN + anchor = capture
 *     everything; hold an anchor + PLAY = clear that scene.
 *   REC / PLAY / FN+PLAY (= STOP) = COPY / CLEAR / PASTE with a scope held: step(s), slice pad(s), PAGE (an OCT key
 *     held), PATN (held; clear asks for a second PLAY).
 *   in the loop layer, FN + key = loop keys / slice pads 17-32 (when LDIV > 16 / more than 16 slices).
 *   hold OCT- or OCT+ = PAGE: the lower keys are the pages (SELECT knob: page select / page loop / pattern rate).
 *   FN + OCT+ = memorize, FN + OCT- = recall. FN + SAVE = undo. PATN overlay + SAVE = name the pattern; MST setup
 *   NAME = name the project. Hold slice pad(s) + turn = slice lock; CHOK / SMUT cells = choke group / mute. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ec_app.h"
#include "ec_snap.h"

#define A (&ec_app)
#define E (&ec_app.e5)

static ec_project_t g_memo;            /* memorize: the whole project in RAM */

enum { R5_SWALLOW = 100, R5_ANCHOR_A, R5_ANCHOR_B, R5_GLIDE = 110 /* + 0..5 */ };
enum { OP_COPY, OP_CLEAR, OP_PASTE };

static void preview_stop(void);
static float global_get(int id);

/* PAGE > RATE: the 16 keys over the 21 pattern rates, 1x on key 9 */
static const uint8_t RATE_KEYS[16] = {1, 2, 4, 5, 7, 9, 10, 12, 13, 14, 15, 16, 17, 18, 19, 21};

static int clampi(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }
static int iv(const float *v, int id) { return (int)floorf(v[id] + 0.5f); }
static int is_slice_m(int m) { return m >= EC_MACH_GRID_SLICE && m <= EC_MACH_RAZOR_POLY; }
static float *tv(void) { return ec_project_track(&A->proj, A->track)->v; }
static ec_ptrack_t *tr(void) { return ec_project_track(&A->proj, A->track); }

static void msg(const char *s)
{
    snprintf(A->msg, sizeof A->msg, "%s", s);
    A->msg_ms = A->ms ? A->ms : 1;
    A->redraw = 1;
}

static void toast(const char *s)
{
    snprintf(E->toast, sizeof E->toast, "%s", s);
    E->toast_ms = A->ms ? A->ms : 1;
    E->toast_keep = 0;
    A->redraw = 1;
}

static void dirty(void)
{
    A->dirty = 1;
    A->dirty_ms = A->ms;
    A->redraw = 1;
}

static int pages_of(int t) { return (ec_seq_pattern_steps(&A->seq, t) + 15) / 16; }
static int loop_layer(void) { return A->sel_held || A->keymode == EC_KM_LOOP; }

/* the slice pads held now (slice machines, the loop layer, no step held): bit s-1 */
static uint32_t pads_held(void)
{
    if (A->bus || A->step_held || !loop_layer() || !is_slice_m(iv(tv(), EC_P_MACHINE)))
        return 0;
    return A->seq.tr[A->track].pads;
}
static int first_bit(uint32_t m)
{
    int k;
    for (k = 0; k < 32; k++)
        if (m >> k & 1u)
            return k;
    return -1;
}

static void xf_apply(float pos)
{
    const uint32_t ch = ec_scene_apply(&A->proj, pos);
    int t;
    for (t = 0; t < EC_NTRACK; t++)
        if (ch >> t & 1u)
            ec_seq_touch(&A->seq, t);
    A->redraw = 1;
}

int ec_e5_scene_layer(void)
{
    if (E->naming || A->patn_open || A->sel_held || A->ux.f3)
        return 0;
    return A->keymode == EC_KM_SCENE || (A->cat == EC_CAT_MASTER && !A->settings && A->page[EC_CAT_MASTER] == 2);
}

int ec_e5_page_visible(void) { return A->ux.f3; }   /* 0.3.0: the pages are F3's */

const char *ec_e5_pattern_name(int idx) { return idx >= 0 && idx < EC_NPAT ? A->proj.pe5[idx].name : ""; }
float ec_e5_rate_of_key(int i) { return (float)RATE_KEYS[clampi(i, 0, 15)]; }

/* ------------------------------------------------------------------------------------------- init / tick --- */
void ec_e5_init(void)
{
    E->rng = 0x51E5C0DEu;
    ec_undo_clear(&E->undo);
    E->undo.gget = global_get;                   /* 0.3.0: redo of a global needs its current value */
    xf_apply(A->proj.e5.sc.pos);                 /* a loaded project's scenes land at their saved position */
}

void ec_e5_tick(void)
{
    if (E->gliding && A->ms - E->glide_ms >= 1000 / EC_GLIDE_HZ) {
        const float cur = A->proj.e5.sc.pos;
        const float next = ec_scene_glide_step(cur, E->glide_target, A->fn);
        E->glide_ms = A->ms;
        if (next != cur) {
            xf_apply(next);
            dirty();
        }
    }
    if (E->preview && A->seq.playing)
        preview_stop();                          /* the transport owns playback now */
    if (E->toast_ms && !E->toast_keep && A->ms - E->toast_ms > 900) {
        E->toast_ms = 0;
        A->redraw = 1;
    }
}

/* ------------------------------------------------------------------------------------------- undo --- */
void ec_e5_edit_param(int track, int id, int plock)
{
    if (plock)
        ec_undo_steps(&E->undo, &A->proj, track, A->step_page, A->step_held, "LOCK", 0x100u + (uint32_t)id, A->ms);
    else {
        ec_undo_param(&E->undo, &A->proj, track, id, A->ms);
        ec_scene_knob(&A->proj, track, id);     /* a hand edit takes the param back from the crossfader */
    }
}

void ec_e5_edit_global(int id, float old)
{
    static const char *const L[] = {"BPM", "SWING", "GLEN", "QUANT", "RTN"};
    ec_undo_global(&E->undo, id, old, L[clampi(id, 0, 4)], A->ms);
}

void ec_e5_steps_edit(uint16_t mask, const char *label, uint32_t key)
{
    ec_undo_steps(&E->undo, &A->proj, A->track, A->step_page, mask, label, key, A->ms);
}

static float global_get(int id)
{
    const ec_project_t *p = &A->proj;
    switch (id) {
    case EC_G_BPM: return p->bpm;
    case EC_G_SWING: return p->swing;
    case EC_G_GLEN: return (float)p->glen;
    case EC_G_QUANT: return (float)p->quant;
    default: return (float)p->rtn;
    }
}

/* 0.3.0: SHIFT + OCT- undo, SHIFT + OCT+ redo */
void ec_e5_undo(int redo)
{
    int t = -1, gid = -1, kind;
    float gv = 0;
    char label[16], b[28];
    kind = redo ? ec_undo_redo(&E->undo, &A->proj, &t, &gid, &gv, label, sizeof label)
                : ec_undo_pop(&E->undo, &A->proj, &t, &gid, &gv, label, sizeof label);
    if (!kind) {
        toast(redo ? "NOTHING TO REDO" : "NOTHING TO UNDO");
        return;
    }
    switch (kind) {
    case EC_U_GLOBAL:
        switch (gid) {
        case EC_G_BPM: ec_seq_set_bpm(&A->seq, gv); break;
        case EC_G_SWING: A->proj.swing = gv; break;
        case EC_G_GLEN: A->proj.glen = (int)gv; break;
        case EC_G_QUANT: A->proj.quant = (int)gv; break;
        default: A->proj.rtn = (int)gv; break;
        }
        ec_seq_timing_changed(&A->seq);
        ec_seq_touch_all(&A->seq);
        break;
    case EC_U_XFADE: ec_seq_touch_all(&A->seq); break;
    default:
        ec_seq_timing_changed(&A->seq);        /* (a pattern rate may have come back) */
        if (t >= 0)
            ec_seq_touch(&A->seq, t);
        xf_apply(A->proj.e5.sc.pos);           /* one-sided morphs follow the restored base */
        break;
    }
    snprintf(b, sizeof b, "%s %s", redo ? "REDO" : "UNDO", label);
    toast(b);
    dirty();
}

/* ------------------------------------------------------------------------------------------- memorize --- */
void ec_e5_memorize(void)
{
    g_memo = A->proj;
    E->memo_ok = 1;
    toast("MEMORIZED");
}

void ec_e5_recall_memo(void)
{
    int s;
    if (!E->memo_ok) {
        toast("NOTHING MEMORIZED");
        return;
    }
    /* the pool: only a slot whose path differs reloads (ram_recalling: same paths, no buffer reloads) */
    for (s = 0; s < EC_NSLOT; s++)
        if (strcmp(A->proj.slot[s].path, g_memo.slot[s].path)) {
            ec_slotrt_t *r = &A->slot[s];
            r->old_handle = r->state == EC_SL_READY ? r->handle : -1;
            r->state = g_memo.slot[s].path[0] ? EC_SL_QUEUED : EC_SL_EMPTY;
            if (!g_memo.slot[s].path[0])
                r->handle = -1;
            r->newmeta = 0;
            r->builtin = 0;
        }
    ec_e5_recall(&A->proj, &g_memo);
    ec_seq_reload(&A->seq);                    /* playing: the pattern restarts at step 1 (a direct start) */
    ec_seq_touch_all(&A->seq);
    toast("RECALLED");
    dirty();
}

/* ------------------------------------------------------------------------------------------- naming --- */
static void name_open(int kind, int pat)
{
    const char *src = kind == EC_NM_PROJECT ? A->proj.e5.name : A->proj.pe5[pat].name;
    int n;
    E->naming = kind;
    E->name_pat = pat;
    memset(E->name, ' ', EC_NAME - 1);
    E->name[EC_NAME - 1] = 0;
    n = (int)strlen(src);
    memcpy(E->name, src, (size_t)(n > EC_NAME - 1 ? EC_NAME - 1 : n));
    E->name_cur = clampi(n, 0, EC_NAME - 2);
    A->patn_open = A->patn_latched = 0;
    A->patn_ms = 0;
    E->rng ^= A->ms * 2654435761u;
    A->redraw = 1;
}

void ec_e5_name_pattern(void) { name_open(EC_NM_PATTERN, A->proj.current); }

void ec_e5_name_close(int accept)
{
    if (accept) {
        char b[EC_NAME];
        int n = EC_NAME - 1;
        memcpy(b, E->name, sizeof b);
        while (n > 0 && b[n - 1] == ' ')
            n--;
        b[n] = 0;
        if (E->naming == EC_NM_PROJECT)
            memcpy(A->proj.e5.name, b, sizeof b);
        else
            memcpy(A->proj.pe5[E->name_pat].name, b, sizeof b);
        toast(n ? "NAMED" : "NAME CLEARED");
        dirty();
    } else
        msg("CANCELLED");
    E->naming = EC_NM_NONE;
    A->redraw = 1;
}

static int char_index(char c)
{
    const char *set = EC_NAME_CHARS;
    const char *p;
    if (c >= 'a' && c <= 'z')
        c = (char)(c - 32);
    p = strchr(set, c);
    return p && c ? (int)(p - set) : 0;
}

static void name_knob(int role, int det)
{
    const int n = (int)strlen(EC_NAME_CHARS), last = EC_NAME - 2;
    int k;
    switch (role) {
    case EC_E_SELECT:
    case EC_E_K1: E->name_cur = clampi(E->name_cur + det, 0, last); break;
    case EC_E_K2: {
        const int c = char_index(E->name[E->name_cur]);
        E->name[E->name_cur] = EC_NAME_CHARS[((c + det) % n + n) % n];
    } break;
    case EC_E_K3: {
        char b[EC_NAME];
        int len;
        ec_e5_random_name(b, sizeof b, &E->rng);
        memset(E->name, ' ', EC_NAME - 1);
        len = (int)strlen(b);
        memcpy(E->name, b, (size_t)len);
        E->name_cur = clampi(len, 0, last);
    } break;
    case EC_E_K4:
        if (det < 0) {                           /* backspace */
            for (; det < 0; det++) {
                const int at = E->name_cur > 0 ? E->name_cur - 1 : 0;
                for (k = at; k < EC_NAME - 2; k++)
                    E->name[k] = E->name[k + 1];
                E->name[EC_NAME - 2] = ' ';
                if (E->name_cur > 0)
                    E->name_cur--;
            }
        } else {                                 /* insert a space */
            for (; det > 0; det--) {
                for (k = EC_NAME - 2; k > E->name_cur; k--)
                    E->name[k] = E->name[k - 1];
                E->name[E->name_cur] = ' ';
            }
        }
        break;
    default: break;
    }
    A->redraw = 1;
}

/* ------------------------------------------------------------------------------------------- copy / clear / paste --- */
static void steps_scope(int op)
{
    ec_ptrack_t *t = tr();
    char b[28];
    const uint16_t held = A->step_held;
    int n;
    switch (op) {
    case OP_COPY:
        n = ec_clip_copy_steps(&E->clip_steps, t, A->step_page, held);
        if (n == 1)
            snprintf(b, sizeof b, "COPY STEP %02d", A->step_page * 16 + first_bit(held) + 1);
        else
            snprintf(b, sizeof b, "COPY %d STEPS", n);
        toast(b);
        break;
    case OP_CLEAR:
        ec_e5_steps_edit(held, "CLEAR", 0x200u);
        ec_clip_clear_locks(t, A->step_page, held);
        toast("LOCKS CLEARED");
        break;
    default:
        if (!E->clip_steps.n) {
            toast("NOTHING COPIED");
            break;
        }
        ec_e5_steps_edit(ec_clip_paste_mask(&E->clip_steps, held), "PASTE", 0x300u);
        ec_clip_paste_steps(&E->clip_steps, t, A->step_page, held);
        toast("PASTED");
        break;
    }
    A->step_edited |= held;
    ec_seq_touch(&A->seq, A->track);
    dirty();
}

static void slice_scope(int op, uint32_t pads)
{
    ec_slicemap_t *m = ec_e5_slices(&A->proj, A->track);
    char b[28];
    int s;
    switch (op) {
    case OP_COPY:
        s = first_bit(pads) + 1;
        ec_clip_copy_slice(&E->clip_slice, m, s);
        snprintf(b, sizeof b, "COPY SLICE %02d", s);
        toast(b);
        break;
    case OP_CLEAR:
        for (s = 1; s <= 32; s++)
            if (pads >> (s - 1) & 1u)
                ec_slice_clear(m, s);
        toast("SLICE CLEARED");
        break;
    default:
        if (!E->clip_slice.ok) {
            toast("NOTHING COPIED");
            break;
        }
        for (s = 1; s <= 32; s++)
            if (pads >> (s - 1) & 1u)
                ec_clip_paste_slice(&E->clip_slice, m, s);
        toast("SLICE PASTED");
        break;
    }
    dirty();
}

static void page_scope(int op)
{
    char b[28];
    const int pg = A->step_page;
    switch (op) {
    case OP_COPY:
        ec_clip_copy_page(&E->clip_page, tr(), pg);
        snprintf(b, sizeof b, "PAGE %d COPIED", pg + 1);
        break;
    case OP_CLEAR:
        ec_e5_steps_edit(0xFFFFu, "CLR PAGE", 0x400u);
        ec_clip_clear_page(tr(), pg);
        snprintf(b, sizeof b, "PAGE %d CLEARED", pg + 1);
        break;
    default:
        if (!E->clip_page.ok) {
            snprintf(b, sizeof b, "NOTHING COPIED");
            break;
        }
        ec_e5_steps_edit(0xFFFFu, "PST PAGE", 0x500u);
        ec_clip_paste_page(&E->clip_page, tr(), pg);
        snprintf(b, sizeof b, "PASTED TO PAGE %d", pg + 1);
        break;
    }
    toast(b);
    ec_seq_touch(&A->seq, A->track);
    dirty();
}

static void pattern_scope(int op)
{
    char b[28];
    const int cur = A->proj.current;
    A->patn_used = 1;
    switch (op) {
    case OP_COPY:
        ec_clip_copy_pattern(&E->clip_pat, &A->proj, cur);
        snprintf(b, sizeof b, "P%02d COPIED", cur + 1);
        toast(b);
        break;
    case OP_CLEAR:
        if (!E->confirm) {                     /* 0.3.0: a question, it stays: OCT+ yes, OCT- / HOME no */
            E->confirm = 1;
            E->confirm_ms = A->ms;
            snprintf(b, sizeof b, "CLEAR P%02d?", cur + 1);
            toast(b);
            E->toast_keep = 1;
            return;
        }
        E->confirm = 0;
        ec_clip_clear_pattern(&A->proj, cur);
        ec_seq_touch_all(&A->seq);
        snprintf(b, sizeof b, "P%02d CLEARED", cur + 1);
        toast(b);
        dirty();
        break;
    default:
        if (!E->clip_pat.ok) {
            toast("NOTHING COPIED");
            return;
        }
        ec_clip_paste_pattern(&E->clip_pat, &A->proj, cur);
        ec_e5_pattern_loaded(&A->seq);          /* its BPM, the scenes against its values */
        ec_seq_timing_changed(&A->seq);
        ec_seq_touch_all(&A->seq);
        snprintf(b, sizeof b, "PASTED TO P%02d", cur + 1);
        toast(b);
        dirty();
        break;
    }
}

/* 0.3.0: the question's answer (OCT+ yes; OCT-, HOME, any other button no) */
void ec_e5_answer(int yes)
{
    if (!E->confirm)
        return;
    if (yes)
        pattern_scope(OP_CLEAR);
    else {
        E->confirm = 0;
        E->toast_ms = 0;
        E->toast_keep = 0;
        msg("CANCELLED");
    }
    A->redraw = 1;
}

/* 0.3.0: SHIFT + S1 / S2: copy / paste the pattern */
void ec_e5_pattern_copy(int paste) { pattern_scope(paste ? OP_PASTE : OP_COPY); }

int ec_e5_scope_op(int b)
{
    const int op = b == EC_B_REC ? OP_COPY : A->fn ? OP_PASTE : OP_CLEAR;
    const uint32_t pads = pads_held();
    if (A->step_held && !A->bus) {
        steps_scope(op);
        return 1;
    }
    if (pads) {
        slice_scope(op, pads);
        return 1;
    }
    if (E->page_down) {
        page_scope(op);
        return 1;
    }
    if (E->patn_down) {
        pattern_scope(op);
        return 1;
    }
    if (E->anchor && op == OP_CLEAR) {
        ec_scene_clear(&A->proj, E->anchor - 1);
        xf_apply(A->proj.e5.sc.pos);
        toast(E->anchor == 1 ? "SCENE A CLEARED" : "SCENE B CLEARED");
        dirty();
        return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------------------------------- buttons --- */
/* 0.3.0: the OCT keys, naming's OK / cancel, undo and memorize moved to ec_app.c (pop-ups, SHIFT's keys); a held OCT
 * key is still the PAGE scope (E->page_down, set by ec_app.c) */
int ec_e5_scope_held(void) { return (A->step_held && !A->bus) || pads_held() || E->page_down || E->patn_down || E->anchor; }

int ec_e5_upper(int i, int down)
{
    if (i == 10)
        E->patn_down = down;
    if (E->naming)
        return 1;
    return 0;
}

/* ------------------------------------------------------------------------------------------- lower keys --- */
/* 0.3.0: F3's lower keys: a tap edits the page (EC_PM_SELECT), a hold puts it in / out of the loop (EC_PM_LOOP) */
void ec_e5_page_key(int i, int mode)
{
    const int pages = pages_of(A->track);
    char b[28];
    switch (mode) {
    case EC_PM_SELECT:
        if (i < pages) {
            A->step_page = i;
            snprintf(b, sizeof b, "PAGE %d", i + 1);
            msg(b);
        }
        break;
    case EC_PM_LOOP:
        if (i < pages) {
            ec_e5_toggle_page_loop(&A->proj, A->track, i, pages);
            ec_seq_timing_changed(&A->seq);
            dirty();
        }
        break;
    default:
        ec_e5_edit_param(A->track, EC_P_RATE, 0);
        tv()[EC_P_RATE] = ec_e5_rate_of_key(i);
        ec_seq_timing_changed(&A->seq);
        ec_seq_touch(&A->seq, A->track);
        snprintf(b, sizeof b, "RATE %s", ec_rate_label(RATE_KEYS[i]));
        msg(b);
        dirty();
        break;
    }
}

static void scene_down(int k, int i)
{
    const int j = i % 8;
    if (j == 0 || j == 7) {
        const int sc = j == 0 ? 0 : 1;
        if (A->fn) {
            ec_scene_capture(&A->proj, sc);
            toast(sc ? "SCENE B STORED" : "SCENE A STORED");
            A->key_role[k] = R5_SWALLOW;
            dirty();
        } else {
            E->anchor = sc + 1;
            A->key_role[k] = (uint8_t)(sc ? R5_ANCHOR_B : R5_ANCHOR_A);
            msg(sc ? "SCENE B" : "SCENE A");
        }
        return;
    }
    {
        const int g = j - 1;
        E->glide_held |= (uint8_t)(1u << g);
        A->key_role[k] = (uint8_t)(R5_GLIDE + g);
        ec_undo_xfade(&E->undo, &A->proj, A->ms);
        xf_apply(A->proj.e5.sc.pos);          /* a param just locked into a scene snaps in at once */
        E->glide_target = (float)g / 5.0f;
        E->gliding = 1;
        E->glide_ms = A->ms;
        {
            const float next = ec_scene_glide_step(A->proj.e5.sc.pos, E->glide_target, A->fn);
            xf_apply(next);                   /* move now: even a fast tap registers */
        }
        dirty();
    }
}

int ec_e5_lower_down(int k, int i, int vel)
{
    (void)vel;
    if (E->naming) {
        A->key_role[k] = R5_SWALLOW;
        return 1;
    }
    if (A->patn_open)
        return 0;
    if (ec_e5_scene_layer()) {
        scene_down(k, i);
        A->redraw = 1;
        return 1;
    }
    return 0;
}

/* loop keys / slice pads 17-32: FN + key, when the track has them (LDIV > 16, more than 16 slices) */
static int bank2(void)
{
    const float *v = tv();
    if (is_slice_m(iv(v, EC_P_MACHINE)))
        return iv(v, EC_P_SLICE_COUNT) > 16;
    return iv(v, EC_P_LOOP_DIVISION) > 16;
}

int ec_e5_loop_key(int k, int i, int down)
{
    if (down) {
        if (A->fn && bank2())
            E->key_hi |= 1u << k;
        else
            E->key_hi &= ~(1u << k);
    }
    return i + 1 + (E->key_hi >> k & 1u ? 16 : 0);
}

int ec_e5_lower_up(int k, int i)
{
    const int r = A->key_role[k];
    (void)i;
    if (r < R5_SWALLOW)
        return 0;
    if (r == R5_ANCHOR_A || r == R5_ANCHOR_B) {
        if (E->anchor == r - R5_ANCHOR_A + 1)
            E->anchor = 0;
    } else if (r >= R5_GLIDE && r < R5_GLIDE + 6) {
        E->glide_held &= (uint8_t)~(1u << (r - R5_GLIDE));
        if (E->glide_held) {                    /* another glide key still held: retarget to it */
            int g;
            for (g = 5; g >= 0; g--)
                if (E->glide_held >> g & 1u)
                    break;
            E->glide_target = (float)g / 5.0f;
        } else
            E->gliding = 0;                      /* release freezes wherever it got to */
    }
    A->key_role[k] = 0;
    A->redraw = 1;
    return 1;
}

/* ------------------------------------------------------------------------------------------- step preview --- */
static void preview_stop(void)
{
    int k;
    for (k = 1; k <= 32; k++)
        if (E->preview_bits >> (k - 1) & 1u) {
            if (E->preview == 1)
                ec_seq_slice_pad(&A->seq, E->preview_track, k, 0, 0);
            else
                ec_seq_loop_key(&A->seq, E->preview_track, k, 0);
        }
    E->preview = 0;
    E->preview_bits = 0;
}

void ec_e5_step_down(int i)
{
    const float *v = tv();
    const ec_step_t *s = &tr()->steps[A->step_page * 16 + i];
    const int m = iv(v, EC_P_MACHINE);
    float x;
    if (A->seq.playing || E->preview || A->bus || !s->trig || ec_step_is_ghost(s, v))
        return;
    E->preview_track = A->track;
    if (is_slice_m(m)) {
        const int cnt = clampi(iv(v, EC_P_SLICE_COUNT), 1, 32);
        const uint32_t bits = s->slices ? s->slices : 1u << (clampi(iv(v, EC_P_SLICE_INDEX), 1, cnt) - 1);
        const float vel = ec_step_lock_get(s, EC_P_VELOCITY, &x) ? x : v[EC_P_VELOCITY];
        int k;
        for (k = 1; k <= cnt; k++)
            if (bits >> (k - 1) & 1u) {
                ec_seq_slice_pad(&A->seq, A->track, k, 1, vel);
                E->preview_bits |= 1u << (k - 1);
            }
        E->preview = 1;
    } else {
        /* loop machines: audition the step's region (its lock, else the track's) through the loop keys' span */
        const int div = clampi((iv(v, EC_P_LOOP_DIVISION) + 1) / 2 * 2, 2, 32);
        const float units = 128.0f / (float)div;
        const float st = ec_step_lock_get(s, EC_P_LOOP_START, &x) ? x : v[EC_P_LOOP_START];
        const float en = ec_step_lock_get(s, EC_P_LOOP_END, &x) ? x : v[EC_P_LOOP_END];
        const int k0 = clampi((int)(st / units) + 1, 1, div);
        const int k1 = clampi((int)ceilf(en / units), k0, div);
        ec_seq_loop_key(&A->seq, A->track, k0, 1);
        E->preview_bits = 1u << (k0 - 1);
        if (k1 != k0) {
            ec_seq_loop_key(&A->seq, A->track, k1, 1);
            E->preview_bits |= 1u << (k1 - 1);
        }
        E->preview = 2;
    }
}

void ec_e5_step_up(void)
{
    if (!A->step_held && E->preview)
        preview_stop();
}

/* ------------------------------------------------------------------------------------------- knobs --- */
int ec_e5_knob(int role, int det)
{
    if (E->naming) {
        name_knob(role, det);
        return 1;
    }
    return 0;
}

static int choke_target(uint32_t *mask)
{
    const uint32_t pads = pads_held();
    if (pads) {
        *mask = pads;
        return first_bit(pads) + 1;
    }
    {
        const int s = clampi(iv(tv(), EC_P_SLICE_INDEX), 1, 32);
        *mask = 1u << (s - 1);
        return s;
    }
}

int ec_e5_knob_cell(const ec_item_t *it, int det, int mult)
{
    if (it->kind == EC_IT_E5) {
        switch (it->id) {
        case EC_X_XFD: {
            const float pos = A->proj.e5.sc.pos;
            float q = A->fn ? ec_global_snap(EC_SNAP_XFD, floorf(pos * 128.0f + 0.5f), det)
                            : floorf(pos * 128.0f + 0.5f) + (float)(det * mult);
            ec_undo_xfade(&E->undo, &A->proj, A->ms);
            q = q < 0 ? 0 : q > 128 ? 128 : q;
            E->gliding = 0;
            xf_apply(q / 128.0f);
            dirty();
        } break;
        case EC_X_CHOKE: {
            ec_slicemap_t *m = ec_e5_slices(&A->proj, A->track);
            uint32_t mask;
            const int s = choke_target(&mask);
            const int g = clampi(m->choke[s - 1] + det, 0, 8);
            int k;
            for (k = 0; k < 32; k++)
                if (mask >> k & 1u)
                    m->choke[k] = (uint8_t)g;
            dirty();
        } break;
        case EC_X_MUTE: {
            ec_slicemap_t *m = ec_e5_slices(&A->proj, A->track);
            uint32_t mask;
            choke_target(&mask);
            m->mute = det > 0 ? m->mute | mask : m->mute & ~mask;
            dirty();
        } break;
        case EC_X_GBPM:
            A->proj.e5.gbpm = det > 0;
            dirty();
            break;
        case EC_X_NAME: name_open(EC_NM_PROJECT, 0); break;
        default: break;
        }
        A->redraw = 1;
        return 1;
    }
    if (E->anchor && !A->settings && it->kind != EC_IT_PARAM) {
        msg("NO MORPH");                        /* (an anchor held: nothing else moves mid-gesture) */
        return 1;
    }
    if (it->kind != EC_IT_PARAM || A->settings || A->bus)
        return 0;
    if (E->anchor) {                            /* hold an anchor + turn: the scene's value only */
        const int id = it->id, sc = E->anchor - 1;
        float x;
        char b[28], val[12];
        if (!ec_scene_morphable(id)) {
            msg("NO MORPH");
            return 1;                           /* swallowed: the live value never moves mid-gesture */
        }
        if (!ec_scene_get(&A->proj, sc, A->track, id, &x))
            x = tv()[id];
        x = ec_app_move_param(id, x, det, mult);
        ec_scene_set(&A->proj, sc, A->track, id, x);
        ec_param_format(id, x, iv(tv(), EC_P_ENV_RANGE), val, sizeof val);
        snprintf(b, sizeof b, "%s %s %s", sc ? "B" : "A", EC_PARAMS[id].label, val);
        msg(b);
        dirty();
        return 1;
    }
    {
        const uint32_t pads = pads_held();
        if (pads && (EC_PARAMS[it->id].flags & EC_PF_LOCKABLE) && !(EC_PARAMS[it->id].flags & EC_PF_REGION)) {
            ec_slicemap_t *m = ec_e5_slices(&A->proj, A->track);
            const int id = it->id;
            int s;
            for (s = 1; s <= 32; s++)
                if (pads >> (s - 1) & 1u) {
                    float x;
                    if (!ec_slice_lock_get(m, s, id, &x))
                        x = tv()[id];
                    if (ec_slice_lock_set(m, s, id, ec_app_move_param(id, x, det, mult)) < 0)
                        msg("LOCKS FULL");
                }
            if (A->sel_held)
                A->sel_used = 1;
            dirty();
            return 1;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------------------------------- cells --- */
int ec_e5_item_value(const ec_item_t *it, float *x, int *locked)
{
    if (it->kind == EC_IT_E5) {
        switch (it->id) {
        case EC_X_XFD: *x = floorf(A->proj.e5.sc.pos * 128.0f + 0.5f); break;
        case EC_X_CHOKE: {
            uint32_t mask;
            *x = ec_e5_slices(&A->proj, A->track)->choke[choke_target(&mask) - 1];
        } break;
        case EC_X_MUTE: {
            uint32_t mask;
            const int sl = choke_target(&mask);
            *x = (float)(ec_e5_slices(&A->proj, A->track)->mute >> (sl - 1) & 1u);
        } break;
        case EC_X_GBPM: *x = (float)A->proj.e5.gbpm; break;
        default: *x = 0; break;
        }
        return 1;
    }
    if (it->kind != EC_IT_PARAM || A->settings || A->bus)
        return 0;
    if (E->anchor && ec_scene_morphable(it->id)) {
        if (ec_scene_get(&A->proj, E->anchor - 1, A->track, it->id, x)) {
            if (locked)
                *locked = 1;
        } else
            *x = tv()[it->id];
        return 1;
    }
    {
        const uint32_t pads = pads_held();
        if (pads && ec_slice_lock_get(ec_e5_slices(&A->proj, A->track), first_bit(pads) + 1, it->id, x)) {
            if (locked)
                *locked = 1;
            return 1;
        }
    }
    return 0;
}

void ec_e5_item_format(const ec_item_t *it, float x, char *out, int cap)
{
    switch (it->id) {
    case EC_X_XFD: snprintf(out, (size_t)cap, "%d", (int)x); break;
    case EC_X_CHOKE:
        if (x < 0.5f)
            snprintf(out, (size_t)cap, "NONE");
        else
            snprintf(out, (size_t)cap, "%d", (int)(x + 0.5f));
        break;
    case EC_X_GBPM: snprintf(out, (size_t)cap, "%s", x > 0.5f ? "ON" : "OFF"); break;
    case EC_X_MUTE: snprintf(out, (size_t)cap, "%s", x > 0.5f ? "MUTE" : "OFF"); break;
    default: snprintf(out, (size_t)cap, "%.8s", A->proj.e5.name[0] ? A->proj.e5.name : "--"); break;
    }
}

float ec_e5_item_frac(const ec_item_t *it, float x)
{
    switch (it->id) {
    case EC_X_XFD: return x / 128.0f;
    case EC_X_CHOKE: return x / 8.0f;
    case EC_X_GBPM:
    case EC_X_MUTE: return x;
    default: return 0;
    }
}

/* ------------------------------------------------------------------------------------------- LEDs --- */
static int blink(uint32_t period) { return (A->ms / period) & 1u; }

int ec_e5_leds_lower(void)
{
    int i;
    if (E->naming) {
        for (i = 0; i < EC_NLOWER; i++)
            ec_plat_led_lower(i, i == E->name_cur ? EC_LED_MID : EC_LED_OFF);
        return 1;
    }
    if (ec_e5_page_visible()) {
        const int pages = pages_of(A->track);
        const int play = ec_seq_step(&A->seq, A->track);
        const int ppage = play >= 0 ? play / 16 : -1;
        for (i = 0; i < EC_NLOWER; i++) {
            uint8_t l = EC_LED_OFF;
            if (i < pages) {   /* F3: the page you edit lit, the loop's pages mid, the others faint */
                l = i == A->step_page ? EC_LED_LIT
                    : ec_e5_page_in_loop(&A->proj, A->track, i, pages) ? EC_LED_MID : EC_LED_FAINT;
                if (i == ppage && A->seq.playing && blink(125))
                    l = EC_LED_LIT;
            }
            ec_plat_led_lower(i, l);
        }
        return 1;
    }
    if (loop_layer() && !A->patn_open && !A->bus && A->fn && bank2()) {   /* FN: keys / pads 17-32 */
        const float *v = tv();
        if (is_slice_m(iv(v, EC_P_MACHINE))) {
            const int cnt = clampi(iv(v, EC_P_SLICE_COUNT), 1, 32);
            const ec_slicemap_t *m = ec_e5_slices(&A->proj, A->track);
            for (i = 0; i < EC_NLOWER; i++) {
                const int s = i + 16;
                ec_plat_led_lower(i, s >= cnt ? EC_LED_OFF : A->seq.tr[A->track].pads >> s & 1u ? EC_LED_LIT
                                     : m->mute >> s & 1u                                         ? EC_LED_FAINT
                                                                                                 : EC_LED_DIM);
            }
        } else {
            const int div = clampi((iv(v, EC_P_LOOP_DIVISION) + 1) / 2 * 2, 2, 32);
            for (i = 0; i < EC_NLOWER; i++)
                ec_plat_led_lower(i, i + 16 >= div ? EC_LED_OFF
                                     : A->seq.tr[A->track].live_keys >> (i + 16) & 1u ? EC_LED_LIT : EC_LED_DIM);
        }
        return 1;
    }
    if (ec_e5_scene_layer()) {
        const ec_scenes_t *sc = &A->proj.e5.sc;
        for (i = 0; i < EC_NLOWER; i++) {
            const int j = i % 8;
            uint8_t l;
            if (j == 0 || j == 7) {
                const int s = j == 0 ? 0 : 1;
                l = E->anchor == s + 1 ? EC_LED_LIT : ec_scene_count(sc, s) ? EC_LED_MID : EC_LED_DIM;
            } else {
                const float d = fabsf(sc->pos - (float)(j - 1) / 5.0f);
                l = E->glide_held >> (j - 1) & 1u ? EC_LED_LIT : d < 0.1f ? EC_LED_MID : d < 0.2f ? EC_LED_DIM
                                                                                               : EC_LED_FAINT;
            }
            ec_plat_led_lower(i, l);
        }
        return 1;
    }
    return 0;
}

void ec_e5_leds(void)
{
    if (loop_layer() && !A->patn_open && !A->bus && !A->fn && !ec_e5_page_visible() && !E->naming &&
        is_slice_m(iv(tv(), EC_P_MACHINE))) {     /* a muted pad shows faint */
        const uint32_t mute = ec_e5_slices(&A->proj, A->track)->mute;
        const uint32_t held = A->seq.tr[A->track].pads;
        int i;
        for (i = 0; i < EC_NLOWER; i++)
            if ((mute >> i & 1u) && !(held >> i & 1u) && i < iv(tv(), EC_P_SLICE_COUNT))
                ec_plat_led_lower(i, EC_LED_FAINT);
    }
    if (!A->fn && (A->step_held || pads_held() || E->page_down || E->patn_down))
        ec_plat_led_button(EC_B_REC, EC_LED_MID);      /* a scope is held: REC copies */
}

/* ------------------------------------------------------------------------------------------- ui_info --- */
void ec_e5_ui_info(char *buf, uint32_t n)
{
    const int pages = pages_of(A->track);
    char pn[EC_NAME], jn[EC_NAME];
    int i;
    uint32_t loop = 0;
    snprintf(pn, sizeof pn, "%s", A->proj.pe5[A->proj.current].name[0] ? A->proj.pe5[A->proj.current].name : "-");
    snprintf(jn, sizeof jn, "%s", A->proj.e5.name[0] ? A->proj.e5.name : "-");
    for (i = 0; pn[i]; i++)                      /* (keep ui_info one word per field) */
        pn[i] = pn[i] == ' ' ? '_' : pn[i];
    for (i = 0; jn[i]; i++)
        jn[i] = jn[i] == ' ' ? '_' : jn[i];
    for (i = 0; i < pages && i < 16; i++)
        loop |= (uint32_t)ec_e5_page_in_loop(&A->proj, A->track, i, pages) << i;
    snprintf(buf, n,
             " xf:%.3f sa:%d sb:%d targets:%d anchor:%d memo:%d undo:%d redo:%d spage:%d ploop:%04x rate:%s "
             "gbpm:%d pname:%s proj:%s naming:%d clip:%d/%d/%d/%d",
             (double)A->proj.e5.sc.pos, ec_scene_count(&A->proj.e5.sc, 0), ec_scene_count(&A->proj.e5.sc, 1),
             ec_scene_targets(&A->proj), E->anchor, E->memo_ok, E->undo.n, E->undo.nr, A->step_page + 1,
             (unsigned)loop, ec_rate_label(iv(tv(), EC_P_RATE)), A->proj.e5.gbpm,
             pn, jn, E->naming, E->clip_steps.n, E->clip_page.ok,
             E->clip_pat.ok, E->clip_slice.ok);
}
