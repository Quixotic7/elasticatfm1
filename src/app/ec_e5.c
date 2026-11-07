/* SPDX-License-Identifier: GPL-3.0-only */
/* E5 on the panel (ec_e5app.h), as 0.6.0 keeps it (docs/UX-V2.md): the scene anchors' knob path (CROSSFADER's
 * B1 / B2 held + a knob, src/app/ec_xfd.c), undo / redo (SHIFT + F1 / F2), memorize / recall (SHIFT + B3 / B4, the
 * project as a record), pattern and project naming, F3's page keys, the slice pads' locks in LOOP ENGINE (hold pad(s)
 * + turn = slice lock; CHOK / SMUT cells = choke group / mute), step preview, header messages. 0.6.0 took out the scene row,
 * the copy / clear / paste scopes (now §2's one-step buffer, src/app/ec_clip.c) and the PAGE overlay. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ec_app.h"
#include "ec_snap.h"

#define A (&ec_app)
#define E (&ec_app.e5)

/* memorize: the project as a record (0.6.0: a second ec_project_t would be 9 MiB with 64 patterns) */
static uint8_t g_memo[EC_PLAT_RECORD_MAX];
static uint32_t g_memo_n;

enum { R5_SWALLOW = 100 };

static void preview_stop(void);
static float global_get(int id);

/* PAGE > RATE: the 16 keys over the 21 pattern rates, 1x on key 9 */
static const uint8_t RATE_KEYS[16] = {1, 2, 4, 5, 7, 9, 10, 12, 13, 14, 15, 16, 17, 18, 19, 21};

static int clampi(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }
static int iv(const float *v, int id) { return (int)floorf(v[id] + 0.5f); }
static int is_slice_m(int m) { return m >= EC_MACH_GRID_SLICE && m <= EC_MACH_RAZOR_POLY; }
static float *tv(void) { return ec_project_track(&A->proj, A->track)->v; }
static ec_ptrack_t *tr(void) { return ec_project_track(&A->proj, A->track); }

static void msg(const char *s) { ec_app_message(s); }
static void toast(const char *s) { ec_app_message(s); }   /* 0.6.1: a message in the header (no box) */

static void dirty(void)
{
    A->dirty = 1;
    A->dirty_ms = A->ms;
    A->redraw = 1;
}

static int pages_of(int t) { return (ec_seq_pattern_steps(&A->seq, t) + 15) / 16; }
static int loop_layer(void) { return ec_app_mode() == EC_CAT_SRC && !A->settings; }   /* 0.6.0: LOOP ENGINE */

/* the slice pads held now (slice machines, the loop layer, no step held): bit s-1 */
static uint32_t pads_held(void)
{
    if (A->bus || !loop_layer() || !is_slice_m(iv(tv(), EC_P_MACHINE)))
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

int ec_e5_scene_layer(void) { return 0; }
void ec_e5_xf_apply(float pos) { xf_apply(pos); }   /* 0.6.0: the scene row is gone (CROSSFADER) */

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
    if (E->preview && A->seq.playing)
        preview_stop();                          /* the transport owns playback now */
    if (E->naming && ((E->name_key >= 0 && A->ms - E->name_key_ms < 400u) ||
                      (E->name_rolled && A->ms - E->name_roll_ms < 700u)))
        A->redraw = 1;                           /* 0.6.47: the typed key's glow fades; 0.6.48: the die tumbles */
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

void ec_e5_edit_params(int track, const uint8_t *ids, int n, const char *label)
{
    int i;
    ec_undo_params(&E->undo, &A->proj, track, ids, n, label, A->ms);
    for (i = 0; i < n; i++)
        ec_scene_knob(&A->proj, track, ids[i]);
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
    case EC_U_BLOB:                                /* 0.6.55: a pattern, the scenes or a track came back */
        if (gid == EC_UB_PATTERN && (int)gv == A->proj.current)
            ec_e5_pattern_loaded(&A->seq);         /* (its BPM, the scenes against its values) */
        ec_seq_timing_changed(&A->seq);
        ec_seq_touch_all(&A->seq);
        xf_apply(A->proj.e5.sc.pos);
        break;
    case EC_U_TRIM: ec_seq_touch_all(&A->seq); break;      /* 0.6.1: a slot's trim */
    case EC_U_SLOTS: ec_file_slots_swapped(gid, (int)gv); break;   /* 0.6.1: POOL's MOVE */
    case EC_U_SLOT: ec_file_slot_undone(gid); break;           /* 0.6.55: FREE undone (or redone) */
    case EC_U_FXSWAP:                              /* 0.6.3: the inserts / sends swapped back */
        ec_seq_touch_all(&A->seq);
        xf_apply(A->proj.e5.sc.pos);
        break;
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
    const int32_t n = ec_e5_memo_save(&A->proj, g_memo, sizeof g_memo);
    if (n <= 0) {
        toast("MEMO FULL");
        return;
    }
    g_memo_n = (uint32_t)n;
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
    {
        /* the pool: only a slot whose path differs reloads (ram_recalling: same paths, no buffer reloads) */
        static char before[EC_NSLOT][EC_PATH];
        for (s = 0; s < EC_NSLOT; s++)
            memcpy(before[s], A->proj.slot[s].path, EC_PATH);
        if (ec_e5_memo_recall(&A->proj, g_memo, g_memo_n) != 0) {
            toast("MEMO BAD");
            return;
        }
        for (s = 0; s < EC_NSLOT; s++)
            if (strcmp(before[s], A->proj.slot[s].path)) {
                ec_slotrt_t *r = &A->slot[s];
                r->old_handle = r->state == EC_SL_READY ? r->handle : -1;
                r->state = A->proj.slot[s].path[0] ? EC_SL_QUEUED : EC_SL_EMPTY;
                if (!A->proj.slot[s].path[0])
                    r->handle = -1;
                r->newmeta = 0;
                r->builtin = 0;
            }
    }
    ec_seq_reload(&A->seq);                    /* playing: the pattern restarts at step 1 (a direct start) */
    ec_seq_touch_all(&A->seq);
    toast("RECALLED");
    dirty();
}

/* ------------------------------------------------------------------------------------------- naming --- */
/* 0.6.47 (owner): FIZZ's typewriter keybed for every name (a pattern's, the project's, a project file's): B1..B11 =
 * A..K, W1..W16 = L..Z and SPACE (SHIFT: 0..9 and -, the signs); the suggested name starts selected, the first key
 * replaces it; PRESETS / K4 right rolls a new name every detent (FIZZ's roller), left walks back through the last ten;
 * ALGO the cursor, SEL / K1 the character under it; OCT- deletes (a selected name: all of it; an empty one: cancel),
 * SHIFT + OCT- clears; OCT+ (or FILE) OK, HOME cancels. Only file-safe characters (no / : * ? " < > | \) */
static const char *const NK_BLACK = "ABCDEFGHIJK", *const NK_BLACK_SH = "0123456789-";
static const char *const NK_WHITE = "LMNOPQRSTUVWXYZ ", *const NK_WHITE_SH = ".'&!#+()_~,=@$% ";

const char *ec_e5_name_key_char(int upper, int i, int shift)
{
    static char b[2];
    const char *set = upper ? (shift ? NK_BLACK_SH : NK_BLACK) : (shift ? NK_WHITE_SH : NK_WHITE);
    b[0] = i >= 0 && i < (int)strlen(set) ? set[i] : 0;
    b[1] = 0;
    return b;
}

static void hist_push(const char *n)
{
    int k;
    if (!n[0] || (E->name_nh && !strcmp(E->name_hist[E->name_nh - 1], n)))
        return;
    if (E->name_nh == 10) {
        for (k = 1; k < 10; k++)
            memcpy(E->name_hist[k - 1], E->name_hist[k], EC_NAME);
        E->name_nh--;
    }
    snprintf(E->name_hist[E->name_nh++], EC_NAME, "%s", n);
    E->name_hp = E->name_nh - 1;
}

static void name_set(const char *n, int sel)
{
    snprintf(E->name, sizeof E->name, "%s", n);
    E->name_cur = (int)strlen(E->name);
    E->name_sel = sel;
    A->redraw = 1;
}

static void name_open_with(int kind, int pat, const char *src)
{
    E->naming = kind;
    E->name_pat = pat;
    E->name_nh = E->name_hp = 0;
    E->name_rolled = 0;
    E->name_key = -1;
    E->rng ^= A->ms * 2654435761u + 0x5bd1e995u;
    A->ux.f3 = 0;                                /* (F3 + PRESETS: the name screen replaces F3) */
    if (src && src[0])
        name_set(src, 1);
    else {                                       /* no name yet: a suggestion */
        char b[EC_NAME];
        ec_name_roll(b, EC_NAME_KEYBED + 1, &E->rng, 0);
        name_set(b, 1);
        E->name_rolled = 1;
        E->name_roll_ms = A->ms ? A->ms : 1;
        E->name_face = 1 + (int)(E->rng % 6u);
    }
    hist_push(E->name);
}

static void name_open(int kind, int pat)
{
    name_open_with(kind, pat, kind == EC_NM_PROJECT ? A->proj.e5.name : A->proj.pe5[pat].name);
}

void ec_e5_name_pattern(void) { name_open(EC_NM_PATTERN, A->proj.current); }

void ec_e5_name_file(const char *file)
{
    snprintf(E->name_file, sizeof E->name_file, "%s", file ? file : "");
    name_open_with(EC_NM_FILE, 0, file);
}

void ec_e5_name_close(int accept)
{
    const int kind = E->naming;
    if (accept) {
        char b[EC_NAME];
        int n, st = 0;
        snprintf(b, sizeof b, "%s", E->name);
        while (b[st] == ' ' || b[st] == '.')     /* 0.6.53 (QA A3): no leading dot (a hidden file, another's copy) */
            st++;
        memmove(b, b + st, strlen(b + st) + 1);
        n = (int)strlen(b);
        while (n > 0 && b[n - 1] == ' ')
            b[--n] = 0;
        E->naming = EC_NM_NONE;
        if (kind == EC_NM_PATTERN) {
            memcpy(A->proj.pe5[E->name_pat].name, b, sizeof b);
            toast(n ? "NAMED" : "NAME CLEARED");
            dirty();
        } else if (!n)
            msg("NO NAME: KEPT");                /* (a project or file needs one) */
        else if (kind == EC_NM_PROJECT)
            ec_prj_named(0, b);                  /* the open project (its file follows) */
        else
            ec_prj_named(E->name_file, b);
    } else {
        E->naming = EC_NM_NONE;
        msg("CANCELLED");
    }
    A->redraw = 1;
}

static void name_type(const char *c)
{
    int n;
    if (!c[0])
        return;
    if (E->name_sel) {                           /* the first key replaces the selected name */
        E->name[0] = 0;
        E->name_cur = 0;
        E->name_sel = 0;
    }
    n = (int)strlen(E->name);
    if (n >= EC_NAME_KEYBED) {
        msg("14 CHARACTERS");
        return;
    }
    memmove(E->name + E->name_cur + 1, E->name + E->name_cur, (size_t)(n - E->name_cur + 1));
    E->name[E->name_cur++] = c[0];
    E->name_rolled = 0;
    A->redraw = 1;
}

void ec_e5_name_back(int clear)
{
    const int n = (int)strlen(E->name);
    if (clear || E->name_sel) {
        if (!n && !clear) {
            ec_e5_name_close(0);
            return;
        }
        E->name[0] = 0;
        E->name_cur = 0;
        E->name_sel = 0;
        E->name_rolled = 0;
        A->redraw = 1;
        return;
    }
    if (!n) {                                    /* an empty name: OCT- is no */
        ec_e5_name_close(0);
        return;
    }
    if (E->name_cur > 0) {
        memmove(E->name + E->name_cur - 1, E->name + E->name_cur, (size_t)(n - E->name_cur + 1));
        E->name_cur--;
    }
    E->name_rolled = 0;
    A->redraw = 1;
}

static void name_roll(int det)
{
    if (det > 0) {
        if (!E->name_sel)                        /* a name typed or edited is kept among them */
            hist_push(E->name);
        for (; det > 0; det--) {
            if (E->name_hp < E->name_nh - 1)
                E->name_hp++;                    /* forward through the ones seen first */
            else {
                char b[EC_NAME];
                ec_name_roll(b, EC_NAME_KEYBED + 1, &E->rng, E->name);
                hist_push(b);
                E->name_rolled = 1;
                E->name_roll_ms = A->ms ? A->ms : 1;   /* 0.6.48: the die tumbles, then lands */
                E->name_face = 1 + (int)(E->rng % 6u);
            }
        }
    } else {
        if (!E->name_sel)
            hist_push(E->name);
        E->name_hp = E->name_hp + det < 0 ? 0 : E->name_hp + det;
        E->name_rolled = 0;
    }
    name_set(E->name_hist[E->name_hp], 1);
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
    const int n = (int)strlen(E->name), ns = (int)strlen(EC_NAME_CHARS);
    switch (role) {
    case EC_E_ALGO:                              /* the cursor */
        E->name_sel = 0;
        E->name_cur = clampi(E->name_cur + det, 0, n);
        break;
    case EC_E_SELECT:
    case EC_E_K1: {                              /* the character under the cursor (at the end: one more) */
        int c;
        E->name_sel = 0;
        if (E->name_cur >= n) {
            if (n >= EC_NAME_KEYBED)
                break;
            E->name[n] = ' ';
            E->name[n + 1] = 0;
        }
        c = char_index(E->name[E->name_cur]);
        E->name[E->name_cur] = EC_NAME_CHARS[((c + det) % ns + ns) % ns];
        E->name_rolled = 0;
    } break;
    case EC_E_PRESETS:
    case EC_E_K2:                                /* 0.6.48 (owner): K2 or K4 roll */
    case EC_E_K4: name_roll(det); break;
    default: break;
    }
    A->redraw = 1;
}

static void name_key(int upper, int i)
{
    name_type(ec_e5_name_key_char(upper, i, A->fn));
    E->name_key = upper ? i : 11 + i;
    E->name_key_ms = A->ms ? A->ms : 1;
    if (A->fn)
        A->ux.shift_used = 1;                    /* (SHIFT is only the second set while typing) */
}

/* 0.3.0: the open question's answer (F2 yes; F1, HOME, any other button no). 0.6.0 asks none: kept for the pop-up */
void ec_e5_answer(int yes)
{
    (void)yes;
    if (!E->confirm)
        return;
    E->confirm = 0;
    msg("CANCELLED");
    A->redraw = 1;
}

int ec_e5_upper(int i, int down)
{
    if (E->naming && down)
        name_key(1, i);                          /* 0.6.47: B1..B11 type */
    return E->naming != 0;
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
            ec_app_undo_track("PAGE LOOP", 0);     /* 0.6.55 (QA B8) */
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

int ec_e5_lower_down(int k, int i, int vel)
{
    (void)vel;
    if (E->naming) {
        A->key_role[k] = R5_SWALLOW;
        name_key(0, i);                          /* 0.6.47: W1..W16 type */
        return 1;
    }
    return 0;
}

/* 0.6.0: LOOP ENGINE's zoom: on 32 loop keys the white keys are 1-16 or 17-32 (B11) */
int ec_e5_loop_key(int k, int i, int down)
{
    if (down) {
        if (A->pn.zoom_half)
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
    if (r != R5_SWALLOW)
        return 0;
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
        ec_undo_blob(&E->undo, &A->proj, EC_UB_SCENES, A->proj.current, -1, sc ? "SCENE B" : "SCENE A",
                     0x5CE00u + ((uint32_t)sc << 8) + (uint32_t)id, A->ms);   /* 0.6.55 (QA B7): one entry a gesture */
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
        const int whole = it->id == EC_P_SLICE_COUNT || it->id == EC_P_SLICE_INDEX || it->id == EC_P_SLOT ||
                          it->id == EC_P_PATTERN_STEPS || it->id == EC_P_LOOP_DIVISION || it->id == EC_P_RATE;
        /* 0.6.39 (owner): the track's structure (COUNT, the slice, the slot, length, LDIV, rate) is never a slice's
         * lock: with a pad held the knob edits the track as usual */
        if (pads && !whole && (EC_PARAMS[it->id].flags & EC_PF_LOCKABLE) && !(EC_PARAMS[it->id].flags & EC_PF_REGION)) {
            ec_slicemap_t *m = ec_e5_slices(&A->proj, A->track);
            const int id = it->id;
            int s;
            ec_app_undo_track("SLICE LOCK", 0x51C00u + (uint32_t)id + (pads << 8));   /* 0.6.55 (QA B4) */
            for (s = 1; s <= 32; s++)
                if (pads >> (s - 1) & 1u) {
                    float x;
                    if (!ec_slice_lock_get(m, s, id, &x))
                        x = tv()[id];
                    if (ec_slice_lock_set(m, s, id, ec_app_move_param(id, x, det, mult)) < 0)
                        msg("LOCKS FULL");
                }
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
        for (i = 0; i < EC_NLOWER; i++)          /* 0.6.47: all sixteen lit, the key typed bright a moment */
            ec_plat_led_lower(i, E->name_key == 11 + i && A->ms - E->name_key_ms < 250u ? EC_LED_LIT : EC_LED_40);
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
    return 0;
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
