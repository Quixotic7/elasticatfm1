/* SPDX-License-Identifier: GPL-3.0-only */
/* 0.6.0 (docs/UX-V2.md §3 FILE, §5 the pool): FILE's three pages and the pool.
 *   BROWSE   SEL walks the folder's rows (its subfolders first, then its files, from the scan of /audio: ec_browse),
 *            B1 up a folder, B2 into the folder under the cursor; every move previews the file (APP SETTINGS'
 *            AUTO-PREVIEW), B11 previews (play / stop); B10 LOADs it into the next free pool slot (the selected track
 *            plays it; the cursor stays put; FULL when the 128 are used); F2 frees the slot of the file under the
 *            cursor. The pool meter: the arena used / free, the slots used of 128.
 *   EDITOR   the track's slot: B2..B10 the zoom (1X..256X), B1 / B11 scroll; the white keys are 16 places across the
 *            view: F1 / F2 pick the point they place (START / END), a held key + a second key place both (0.6.1).
 *   RAZOR    0.4's (ec_razor_ui.c); 0.6.1: ALGO picks the slice.
 *   POOL     0.6.1: the 128 slots: SEL the slot (auto-preview), B10 ASSIGN (the selected track plays it), B11
 *            preview, F1 MOVE (pick up, SEL, drop: a swap, every reference follows; one undo), F2 FREE.
 *   SEL turns the page on EDITOR and RAZOR (lanes, then pages); on BROWSE and POOL it is the cursor, and the page keys
 *   B3 BROWSE, B4 EDITOR, B5 RAZOR, B6 POOL leave (the EDITOR's black keys are its zoom: SEL leaves it).
 *   SHIFT + B11 (FILE, SAMPLER): purge: every slot no pattern uses is freed and the arena compacted.
 * The preview loads through the one loader (ec_loader.c) when no slot load runs, into a store handle of its own; LOAD
 * of the previewed file adopts that handle (no second load). It plays on the main output after the master bus (FILE
 * cannot play the sequencer), and stops / is freed when FILE is left. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ec_app.h"
#include "ec_browse.h"
#include "ec_loader.h"
#include "ec_palette.h"
#include "ec_projfile.h"
#include "ec_razor_ui.h"

#define A (&ec_app)
#define P (&ec_app.pn)

#define ROOT "/audio"
#define TOP "/"                /* 0.6.1: the top level: AUDIO/ and PROJECTS/ (B1 from /audio or /data/projects) */
#define PREVIEW_GAIN 0.8f

static int clampi(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }

/* ------------------------------------------------------------------------------------------- the pool --- */
static int slot_has(int s)
{
    const ec_slotrt_t *r = &A->slot[s];
    return A->proj.slot[s].path[0] || r->builtin || r->state == EC_SL_READY || r->state == EC_SL_QUEUED ||
           r->state == EC_SL_LOADING || (A->rec.state != EC_RS_IDLE && A->rec.slot == s);   /* (a take's) */
}

int ec_file_next_free(void)
{
    int s;
    for (s = 0; s < EC_NSLOT; s++)
        if (!slot_has(s))
            return s;
    return -1;
}

int ec_file_slots_used(void)
{
    int s, n = 0;
    for (s = 0; s < EC_NSLOT; s++)
        n += slot_has(s);
    return n;
}

int ec_file_pooled(const char *path)
{
    int s;
    if (!path || !path[0])
        return -1;
    for (s = 0; s < EC_NSLOT; s++)
        if (A->proj.slot[s].path[0] && !strcmp(A->proj.slot[s].path, path))
            return s;
    return -1;
}

static int handle_used(int h, int except)
{
    int s;
    for (s = 0; s < EC_NSLOT; s++)
        if (s != except && A->slot[s].handle == h)
            return 1;
    return 0;
}

/* forget slot s and its sample (when nothing else holds it); the tracks on it fall silent */
static int free_slot(int s)
{
    ec_slotrt_t *r = &A->slot[s];
    const int h = r->handle, old = r->old_handle;
    if ((h >= 0 && ec_rec_holds(h)) || (old >= 0 && ec_rec_holds(old)))
        return -1;
    if (A->load_slot == s) {
        ec_load_cancel();
        A->load_slot = -1;
    }
    ec_slot_clear(&A->proj.slot[s]);
    r->handle = r->old_handle = -1;
    r->state = EC_SL_EMPTY;
    r->builtin = 0;
    r->newmeta = 0;
    r->err[0] = 0;
    ec_app_assign_samples();                       /* (before the space can be reused) */
    if (P->pv_handle == h && h >= 0) {             /* the preview's adopted sample: the preview owns it again */
        P->pv_owned = 1;
    } else {
        if (h >= 0 && !handle_used(h, -1))
            ec_ss_free(h);
        if (old >= 0 && old != h && !handle_used(old, -1))
            ec_ss_free(old);
    }
    ec_seq_touch_all(&A->seq);
    ec_app_mark_dirty();
    return 0;
}

int ec_file_free_slot(int s)
{
    if (s < 0 || s >= EC_NSLOT)
        return -1;
    return free_slot(s);
}

/* the slots any pattern plays: the tracks' own (used patterns and the live one) and the step locks */
static void slots_in_use(uint8_t *used)
{
    int p, t, i, k;
    memset(used, 0, EC_NSLOT);
    for (p = 0; p < EC_NPAT; p++) {
        const ec_pattern_t *pt = &A->proj.pat[p];
        if (!pt->used && p != A->proj.current)
            continue;
        for (t = 0; t < EC_NTRACK; t++) {
            const ec_ptrack_t *tr = &pt->tr[t];
            used[clampi((int)floorf(tr->v[EC_P_SLOT] + 0.5f), 1, EC_NSLOT) - 1] = 1;
            for (i = 0; i < EC_MAXSTEPS; i++)
                for (k = 0; k < tr->steps[i].nlock; k++)
                    if (tr->steps[i].lock_id[k] == EC_P_SLOT)
                        used[clampi((int)floorf(tr->steps[i].lock_val[k] + 0.5f), 1, EC_NSLOT) - 1] = 1;
        }
    }
}

static void preview_drop(void);

int ec_file_purge(void)
{
    uint8_t used[EC_NSLOT];
    int s, t, n = 0;
    uint32_t got;
    char m[32];
    if (A->load_slot >= 0 || ec_rec_busy() || A->repack) {
        ec_app_alert("BUSY: TRY AGAIN");
        return -1;
    }
    for (s = 0; s < EC_NSLOT; s++)
        if (A->slot[s].state == EC_SL_QUEUED) {
            ec_app_alert("BUSY: TRY AGAIN");
            return -1;
        }
    if (A->seq.playing) {
        ec_app_alert("STOP FIRST");
        return -1;
    }
    preview_drop();
    slots_in_use(used);
    for (s = 0; s < EC_NSLOT; s++)
        if (slot_has(s) && !used[s] && free_slot(s) == 0)
            n++;
    /* the arena slides down over the holes: every engine track lets go first, then gets its sample again */
    for (t = 0; t < EC_NTRACK; t++) {
        ec_dsp_set_sample(A->dsp, t, 0);
        A->assigned[t] = -1;
    }
    got = ec_ss_compact();
    ec_app_assign_samples();
    snprintf(m, sizeof m, "PURGED %d  %uM BACK", n, (unsigned)((got + (1u << 19)) >> 20));
    ec_app_message(m);
    A->redraw = 1;
    return n;
}

/* ------------------------------------------------------------------------------------------- the folders --- */
#define NENT (EC_BROWSE_MAX + EC_BROWSE_DIRS)
static int g_ent[NENT];                /* >= 0: a file (ec_browse index); < 0: a folder, -(the first file in it + 1) */
static int g_nent = -1;
static char g_key[sizeof P->fdir];
static int g_key_n = -1, g_key_scan = -1;

static const char *dir(void) { return P->fdir[0] ? P->fdir : ROOT; }
static int at_top(void) { return !strcmp(dir(), TOP); }
static int at_projects(void) { return !strcmp(dir(), EC_PF_DIR); }
static const char *const TOP_ROW[2] = {"AUDIO/", "PROJECTS/"};
static const char *const TOP_DIR[2] = {ROOT, EC_PF_DIR};

/* the path's component right below the folder (its length), or 0 when the path is not below it; *is_file */
static int below(const char *path, const char *d, int *is_file)
{
    const size_t dl = strlen(d);
    const char *rest, *sl;
    if (strncmp(path, d, dl) || path[dl] != '/')
        return 0;
    rest = path + dl + 1;
    sl = strchr(rest, '/');
    *is_file = !sl;
    return sl ? (int)(sl - rest) : (int)strlen(rest);
}

static void build(void)
{
    const int n = ec_browse_count();
    const char *d = dir();
    const size_t dl = strlen(d);
    int i, j, nd = 0;
    if (g_nent >= 0 && g_key_n == n && g_key_scan == A->scanning && !strcmp(g_key, d))
        return;
    snprintf(g_key, sizeof g_key, "%s", d);
    g_key_n = n;
    g_key_scan = A->scanning;
    g_nent = 0;
    for (i = 0; i < n && g_nent < NENT; i++) {     /* the subfolders first, as the scan meets them */
        int f, l = below(ec_browse_path(i), d, &f);
        if (!l || f)
            continue;
        for (j = 0; j < nd; j++) {
            const char *q = ec_browse_path(-g_ent[j] - 1) + dl + 1;
            if (!strncmp(q, ec_browse_path(i) + dl + 1, (size_t)l) && q[l] == '/')
                break;
        }
        if (j == nd) {
            g_ent[g_nent++] = -(i + 1);
            nd++;
        }
    }
    for (i = 0; i < n && g_nent < NENT; i++) {     /* then its files */
        int f;
        if (below(ec_browse_path(i), d, &f) && f)
            g_ent[g_nent++] = i;
    }
}

int ec_file_count(void)
{
    if (at_top())
        return 2;
    if (at_projects())
        return ec_pf_list_count();
    build();
    return g_nent;
}

const char *ec_file_entry(int i, int *is_dir, int *file)
{
    static char b[EC_SS_NAME + 2];
    if (at_top() || at_projects()) {               /* 0.6.1: the top level's two folders, the project files */
        const int n = ec_file_count();
        if (is_dir)
            *is_dir = at_top() && i >= 0 && i < n;
        if (file)
            *file = -1;
        return i < 0 || i >= n ? "" : at_top() ? TOP_ROW[i] : ec_pf_list_name(i);
    }
    build();
    if (i < 0 || i >= g_nent) {
        if (is_dir)
            *is_dir = 0;
        if (file)
            *file = -1;
        return "";
    }
    if (g_ent[i] >= 0) {
        if (is_dir)
            *is_dir = 0;
        if (file)
            *file = g_ent[i];
        return ec_browse_name(g_ent[i]);
    }
    {
        const char *p = ec_browse_path(-g_ent[i] - 1) + strlen(dir()) + 1;
        const char *sl = strchr(p, '/');
        const int l = sl ? (int)(sl - p) : (int)strlen(p);
        snprintf(b, sizeof b, "%.*s/", l > EC_SS_NAME - 1 ? EC_SS_NAME - 1 : l, p);
        if (is_dir)
            *is_dir = 1;
        if (file)
            *file = -g_ent[i] - 1;
        return b;
    }
}

static const char *cursor_path(void)
{
    int d, f;
    ec_file_entry(A->cursor, &d, &f);
    return !d && f >= 0 ? ec_browse_path(f) : 0;
}

/* ------------------------------------------------------------------------------------------- the preview --- */
static void preview_drop(void)
{
    if (P->pv_state == EC_PV_LOADING)
        ec_load_cancel();                          /* (frees its partial sample) */
    else if (P->pv_handle >= 0 && P->pv_owned && !handle_used(P->pv_handle, -1))
        ec_ss_free(P->pv_handle);
    P->pv_handle = -1;
    P->pv_owned = 0;
    P->pv_play = 0;
    P->pv_state = EC_PV_IDLE;
    P->pv_path[0] = 0;
}

static void preview_begin(void)
{
    if (ec_load_start(P->pv_path) != 0) {
        P->pv_state = EC_PV_ERROR;
        return;
    }
    P->pv_state = EC_PV_LOADING;
}

/* preview path (play: start it once ready) */
static void preview(const char *path, int play)
{
    const int s = ec_file_pooled(path);
    if (P->pv_path[0] && !strcmp(P->pv_path, path) && P->pv_state != EC_PV_ERROR) {
        P->pv_play = play && P->pv_state == EC_PV_READY;
        P->pv_pos = 0;
        if (P->pv_state != EC_PV_READY)
            P->pv_play = play ? 2 : 0;             /* (2: start once ready) */
        return;
    }
    preview_drop();
    snprintf(P->pv_path, sizeof P->pv_path, "%s", path);
    P->pv_pos = 0;
    if (s >= 0 && A->slot[s].state == EC_SL_READY && A->slot[s].handle >= 0) {   /* in the pool: play that */
        P->pv_handle = A->slot[s].handle;
        P->pv_owned = 0;
        P->pv_state = EC_PV_READY;
        P->pv_play = play;
        return;
    }
    P->pv_play = play ? 2 : 0;
    if (A->load_slot >= 0)
        P->pv_state = EC_PV_WAIT;
    else
        preview_begin();
}

void ec_file_preview_yield(void)
{
    if (P->pv_state == EC_PV_LOADING) {
        ec_load_cancel();
        P->pv_state = EC_PV_WAIT;
    }
}

void ec_file_tick(void)
{
    int i;
    if (P->pv_state == EC_PV_WAIT && A->load_slot < 0) {
        for (i = 0; i < EC_NSLOT && A->slot[i].state != EC_SL_QUEUED; i++)
            ;
        if (i == EC_NSLOT)
            preview_begin();
    }
    if (P->pv_state == EC_PV_LOADING && A->load_slot < 0) {
        const int r = ec_load_step();
        if (r == EC_LD_DONE) {
            P->pv_handle = ec_load_handle();
            P->pv_owned = 1;
            P->pv_state = EC_PV_READY;
            P->pv_pos = 0;
            if (P->pv_play == 2)
                P->pv_play = 1;
            A->redraw = 1;
        } else if (r == EC_LD_ERROR) {
            P->pv_state = EC_PV_ERROR;
            P->pv_play = 0;
            ec_app_message(ec_load_error());
        }
    }
    {   /* (a slot that adopted the preview's sample keeps it: the preview no longer owns it) */
        if (P->pv_handle >= 0 && P->pv_owned && handle_used(P->pv_handle, -1))
            P->pv_owned = 0;
    }
}

void ec_file_leave(void)
{
    preview_drop();
    P->ed_hold = P->pool_carry = -1;               /* (a MOVE in hand is put down) */
}

void ec_file_preview_mix(int16_t *out, uint32_t n)
{
    const ec_ss_sample_t *x;
    double step;
    uint32_t k;
    if (P->pv_play != 1 || P->pv_state != EC_PV_READY || (x = ec_ss_ready(P->pv_handle)) == 0)
        return;
    step = x->rate ? (double)x->rate / (double)EC_FS : 1.0;
    for (k = 0; k < n; k++) {
        const uint32_t i = (uint32_t)P->pv_pos;
        int c;
        if (i >= x->frames) {
            P->pv_play = 0;
            A->redraw = 1;
            return;
        }
        for (c = 0; c < 2; c++) {
            const int v = out[2 * k + c] + (int)((float)x->data[2 * i + c] * PREVIEW_GAIN);
            out[2 * k + c] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
        }
        P->pv_pos += step;
    }
}

/* ------------------------------------------------------------------------------------------- the browser --- */
int ec_file_page(void) { return ec_page_pic(EC_CAT_FILE, A->page[EC_CAT_FILE], 0); }

/* the page a black key opens in FILE: the layout's keys; a layout without any (0.4.0's, the tests') B3 the browser,
 * B4 RAZOR */
int ec_file_key_page(int key)
{
    int p, any = 0;
    const int np = ec_cat_pages(EC_CAT_FILE, 0);
    for (p = 0; p < np; p++)
        any |= ec_cat_page_key(EC_CAT_FILE, p) > 0;
    if (any)
        return ec_cat_key_page(EC_CAT_FILE, key);
    for (p = 0; p < np && (key == 3 || key == 4); p++)
        if (ec_page_pic(EC_CAT_FILE, p, 0) == (key == 3 ? EC_PIC_FILE : EC_PIC_RAZOR))
            return p;
    return -1;
}

static void moved(void)
{
    const char *p = cursor_path();
    A->redraw = 1;
    if (!p) {                                      /* a folder: the preview stops */
        P->pv_play = 0;
        return;
    }
    if (ec_as->preview)
        preview(p, 1);
}

void ec_file_select(int d)
{
    const int n = ec_file_count();
    if (n <= 0)
        return;
    A->cursor = clampi(A->cursor + d, 0, n - 1);
    moved();
}

static void folder_up(void)
{
    char *sl;
    char was[sizeof P->fdir];
    int i, n;
    if (at_top()) {
        ec_app_message("TOP FOLDER");
        return;
    }
    if (!strcmp(dir(), ROOT) || at_projects()) {   /* 0.6.1: up to the top level, the cursor on where we were */
        A->cursor = at_projects();
        snprintf(P->fdir, sizeof P->fdir, "%s", TOP);
        ec_app_message(TOP);
        moved();
        return;
    }
    snprintf(was, sizeof was, "%s", dir());
    sl = strrchr(P->fdir, '/');
    if (sl)
        *sl = 0;
    A->cursor = 0;
    n = ec_file_count();
    for (i = 0; i < n; i++) {                      /* the cursor on the folder we came from */
        int isd, f;
        ec_file_entry(i, &isd, &f);
        if (isd && f >= 0 && !strncmp(ec_browse_path(f), was, strlen(was)) && ec_browse_path(f)[strlen(was)] == '/') {
            A->cursor = i;
            break;
        }
    }
    ec_app_message(dir());
    moved();
}

static void folder_in(void)
{
    int isd, f;
    const char *name = ec_file_entry(A->cursor, &isd, &f);
    char d[sizeof P->fdir];
    if (!isd) {
        ec_app_message("NOT A FOLDER");
        return;
    }
    if (at_top()) {                                /* 0.6.1: AUDIO/ or PROJECTS/ */
        snprintf(P->fdir, sizeof P->fdir, "%s", TOP_DIR[A->cursor ? 1 : 0]);
        if (at_projects())
            ec_pf_list_refresh();
        A->cursor = 0;
        ec_app_message(name);
        moved();
        return;
    }
    snprintf(d, sizeof d, "%s/%.*s", dir(), (int)strlen(name) - 1, name);
    snprintf(P->fdir, sizeof P->fdir, "%s", d);
    A->cursor = 0;
    ec_app_message(name);
    moved();
}

static void load(void)
{
    const char *path = cursor_path();
    int s, k;
    char m[24];
    if (at_projects()) {                           /* 0.6.1: LOAD on a project file opens it */
        if (A->cursor >= 0 && A->cursor < ec_pf_list_count())
            ec_pf_open(ec_pf_list_name(A->cursor));
        else
            ec_app_message("NO PROJECTS");
        return;
    }
    if (!path) {
        ec_app_message(ec_browse_count() <= 0 ? (A->no_folder ? "NO FOLDER" : "NO FILES") : "A FOLDER");
        return;
    }
    k = ec_file_pooled(path);
    s = k >= 0 ? k : ec_file_next_free();
    if (s < 0) {
        ec_app_alert("FULL");
        return;
    }
    if (k < 0) {
        if (P->pv_state == EC_PV_READY && P->pv_owned && !strcmp(P->pv_path, path) && P->pv_handle >= 0) {
            P->pv_owned = 0;                       /* the preview's sample becomes the slot's: no second load */
            ec_app_slot_adopt(s, P->pv_handle, path);
        } else
            ec_app_load_file(s, path);
    }
    if (!A->bus) {                                 /* the selected track plays it */
        ec_e5_edit_param(A->track, EC_P_SLOT, 0);
        ec_app_params(A)[EC_P_SLOT] = (float)(s + 1);
        ec_seq_touch(&A->seq, A->track);
    }
    ec_app_mark_dirty();
    snprintf(m, sizeof m, k >= 0 ? "IN SLOT %d" : "LOADED > %d", s + 1);
    ec_app_message(m);
}

int ec_file_f2(void)
{
    const char *path = cursor_path();
    const int s = path ? ec_file_pooled(path) : -1;
    char m[24];
    if (s < 0) {
        ec_app_message("NOT IN THE POOL");
        return 0;
    }
    if (free_slot(s) < 0) {
        ec_app_alert("BUSY: TRY AGAIN");
        return 0;
    }
    snprintf(m, sizeof m, "SLOT %d FREED", s + 1);
    ec_app_message(m);
    return 1;
}

/* ------------------------------------------------------------------------------------------- the editor --- */
float ec_file_zoom(void) { return (float)(1 << clampi(P->ed_zoom, 0, EC_ED_ZOOMS - 1)); }

static void view_clamp(void)
{
    const float w = 1.0f / ec_file_zoom();
    P->ed_view = P->ed_view < 0 ? 0 : P->ed_view > 1.0f - w ? 1.0f - w : P->ed_view;
}

static void zoom(int z)
{
    const float c = P->ed_view + 0.5f / ec_file_zoom();
    char b[16];
    P->ed_zoom = clampi(z, 0, EC_ED_ZOOMS - 1);
    P->ed_view = c - 0.5f / ec_file_zoom();
    view_clamp();
    snprintf(b, sizeof b, "ZOOM %dX", 1 << P->ed_zoom);
    ec_app_message(b);
}

static void scroll(int d)
{
    if (!P->ed_zoom)
        return;
    P->ed_view += (float)d * 0.5f / ec_file_zoom();
    view_clamp();
    A->redraw = 1;
}

static void steps_recalc(ec_slot_t *sl, int s)
{
    const ec_ss_sample_t *x = A->slot[s].handle >= 0 ? ec_ss_ready(A->slot[s].handle) : 0;
    const float dur = x && x->rate ? (float)x->frames / (float)x->rate : 0;
    if (dur > 0) {
        const float st = floorf(dur * (sl->trim_end - sl->trim_start) * sl->bpm / 60.0f * 4.0f + 0.5f);
        sl->steps = st < 1 ? 1 : st > 512 ? 512 : st;
    }
}

/* 0.6.1 (docs/UX-V2.md 10 "FILE / EDIT trim"): the white keys are 16 places across the view; F1 / F2 pick the point a
 * key places (START / END). A tap places it; holding a key and pressing a second places the other point at the second
 * (START: held = start, second = end; END: the reverse). START never passes END: they swap. Each placement by a first
 * key is an undo of its own; the second key joins the held key's (one gesture, one undo). */
void ec_file_ed_point(int end)
{
    P->ed_point = end ? 1 : 0;
    ec_app_message(end ? "PLACE END" : "PLACE START");
}

static void place(ec_slot_t *sl, int end, float pos)
{
    if (end)
        sl->trim_end = pos;
    else
        sl->trim_start = pos;
    if (sl->trim_start > sl->trim_end) {           /* (START after END: they swap) */
        const float t = sl->trim_start;
        sl->trim_start = sl->trim_end;
        sl->trim_end = t;
    }
    if (sl->trim_end < sl->trim_start + 0.005f) {   /* (the shortest trim) */
        if (sl->trim_start + 0.005f <= 1.0f)
            sl->trim_end = sl->trim_start + 0.005f;
        else
            sl->trim_start = sl->trim_end - 0.005f;
    }
}

void ec_file_white(int i, int down)
{
    const int s = ec_app_track_slot(A, A->track);
    ec_slot_t *sl = &A->proj.slot[s];
    int end;
    float pos;
    if (!down) {
        if (i == P->ed_hold)
            P->ed_hold = -1;
        return;
    }
    if (A->bus)
        return;
    pos = P->ed_view + (float)clampi(i, 0, 15) / 15.0f / ec_file_zoom();
    pos = pos < 0 ? 0 : pos > 1 ? 1 : pos;
    if (P->ed_hold >= 0 && P->ed_hold != i)        /* a second key: the other point */
        end = !P->ed_point;
    else {
        ec_undo_trim(&A->e5.undo, &A->proj, s, 1, A->ms);
        end = P->ed_point;
        P->ed_hold = i;
    }
    place(sl, end, pos);
    ec_app_message(end ? "TRIM END" : "TRIM START");
    steps_recalc(sl, s);
    ec_seq_touch_all(&A->seq);
    ec_app_mark_dirty();
}

/* the header while a trim key is held: what a second key does */
static int hdr_held(char *out, int cap)
{
    if (P->ed_hold < 0 || !ec_hdr_holding() || ec_app_mode() != EC_CAT_FILE || A->fn || ec_file_page() != EC_PIC_EDITOR)
        return -1;
    snprintf(out, (size_t)cap, "%s", P->ed_point ? "END + KEY = START" : "START + KEY = END");
    return EC_C_L12;
}
void ec_file_hdr_init(void) { ec_hdr_provider(hdr_held); }

/* ------------------------------------------------------------------------------------------- the pool page --- */
void ec_file_pool_scan(uint8_t *tracks, uint8_t *locks)
{
    int p, t, i, k;
    memset(tracks, 0, EC_NSLOT);
    memset(locks, 0, EC_NSLOT);
    for (t = 0; t < EC_NTRACK; t++)                /* the live pattern's tracks */
        tracks[clampi((int)floorf(ec_project_track(&A->proj, t)->v[EC_P_SLOT] + 0.5f), 1, EC_NSLOT) - 1] |=
            (uint8_t)(1u << t);
    for (p = 0; p < EC_NPAT; p++) {                /* the step locks of every pattern */
        const ec_pattern_t *pt = &A->proj.pat[p];
        if (!pt->used && p != A->proj.current)
            continue;
        for (t = 0; t < EC_NTRACK; t++)
            for (i = 0; i < EC_MAXSTEPS; i++)
                for (k = 0; k < pt->tr[t].steps[i].nlock; k++)
                    if (pt->tr[t].steps[i].lock_id[k] == EC_P_SLOT)
                        locks[clampi((int)floorf(pt->tr[t].steps[i].lock_val[k] + 0.5f), 1, EC_NSLOT) - 1] = 1;
    }
}

/* preview slot s (play: sounding at once) */
static void pool_preview(int s, int play)
{
    const ec_slotrt_t *r = &A->slot[s];
    if (r->state != EC_SL_READY || r->handle < 0) {
        P->pv_play = 0;
        return;
    }
    if (P->pv_state == EC_PV_READY && P->pv_handle == r->handle) {
        P->pv_play = play;
        P->pv_pos = 0;
        return;
    }
    preview_drop();
    snprintf(P->pv_path, sizeof P->pv_path, "%s", A->proj.slot[s].path[0] ? A->proj.slot[s].path : "#slot");
    P->pv_handle = r->handle;
    P->pv_owned = 0;
    P->pv_state = EC_PV_READY;
    P->pv_play = play;
    P->pv_pos = 0;
}

void ec_file_pool_select(int d)
{
    const int was = P->pool_cur;
    P->pool_cur = clampi(P->pool_cur + d, 0, EC_NSLOT - 1);
    A->redraw = 1;
    if (P->pool_cur == was)
        return;
    if (slot_has(P->pool_cur)) {
        if (ec_as->preview && P->pool_carry < 0)   /* (carrying: no previews on the way) */
            pool_preview(P->pool_cur, 1);
    } else
        P->pv_play = 0;
}

static void pool_assign(void)
{
    const int s = P->pool_cur;
    char m[24];
    if (A->bus) {
        ec_app_message("PICK A TRACK");
        return;
    }
    if (!slot_has(s)) {
        ec_app_message("EMPTY SLOT");
        return;
    }
    ec_e5_edit_param(A->track, EC_P_SLOT, 0);
    ec_app_params(A)[EC_P_SLOT] = (float)(s + 1);
    ec_seq_touch(&A->seq, A->track);
    ec_app_mark_dirty();
    snprintf(m, sizeof m, "T%d > SLOT %d", A->track + 1, s + 1);
    ec_app_message(m);
}

void ec_file_slots_swapped(int a, int b)
{
    ec_slotrt_t tmp;
    int t;
    if (a < 0 || b < 0 || a >= EC_NSLOT || b >= EC_NSLOT || a == b)
        return;
    tmp = A->slot[a];
    A->slot[a] = A->slot[b];
    A->slot[b] = tmp;
#define SWAP0(x) ((x) == a ? b : (x) == b ? a : (x))
    A->load_slot = A->load_slot >= 0 ? SWAP0(A->load_slot) : A->load_slot;
    A->rec.slot = A->rec.slot >= 0 ? SWAP0(A->rec.slot) : A->rec.slot;
#undef SWAP0
    for (t = 0; t < EC_NTRACK; t++) {              /* (a slot lock still sounding) */
        int *sl = &A->seq.tr[t].slot_lock;
        *sl = *sl == a + 1 ? b + 1 : *sl == b + 1 ? a + 1 : *sl;
    }
    ec_app_assign_samples();
    ec_seq_touch_all(&A->seq);
    ec_app_mark_dirty();
    A->redraw = 1;
}

void ec_file_pool_f1(void)
{
    const int s = P->pool_cur, c = P->pool_carry;
    char m[24];
    if (A->seq.playing) {
        ec_app_alert("STOP FIRST");
        return;
    }
    if (c < 0) {                                   /* pick it up */
        if (!slot_has(s)) {
            ec_app_message("EMPTY SLOT");
            return;
        }
        P->pool_carry = s;
        P->pv_play = 0;
        A->redraw = 1;
        return;
    }
    P->pool_carry = -1;
    if (c == s) {                                  /* dropped where it was */
        ec_app_message("NOT MOVED");
        return;
    }
    ec_undo_slots(&A->e5.undo, c, s, A->ms);       /* one undo: the swap again */
    ec_project_swap_slots(&A->proj, c, s);
    ec_file_slots_swapped(c, s);
    snprintf(m, sizeof m, "MOVED %d > %d", c + 1, s + 1);
    ec_app_message(m);
}

void ec_file_pool_f2(void)
{
    const int s = P->pool_cur;
    char m[24];
    if (!slot_has(s)) {
        ec_app_message("EMPTY SLOT");
        return;
    }
    if (P->pool_carry == s)
        P->pool_carry = -1;
    if (free_slot(s) < 0) {
        ec_app_alert("BUSY: TRY AGAIN");
        return;
    }
    snprintf(m, sizeof m, "SLOT %d FREED", s + 1);
    ec_app_message(m);
}

/* ------------------------------------------------------------------------------------------- the keys --- */
void ec_file_settings(void)
{
    if (A->rec.open)
        ec_rec_close();
    else
        ec_rec_open();                             /* the recorder's setup (0.4's REC pop-up) */
}

void ec_file_black(int key, int down)
{
    const int mode = ec_app_mode();
    int pic;
    if (A->fn) {                                   /* SHIFT + B11: purge */
        if (down && key == 11)
            ec_file_purge();
        return;
    }
    if (mode == EC_MODE_SAMPLER) {
        ec_sampler_black(key, down);
        return;
    }
    if (!down || mode != EC_CAT_FILE)
        return;
    pic = ec_file_page();
    if (pic == EC_PIC_EDITOR) {
        if (key == 1 || key == 11)
            scroll(key == 1 ? -1 : 1);
        else
            zoom(key - 2);
        return;
    }
    if (ec_file_key_page(key) >= 0) {             /* B3 BROWSE, B4 EDITOR, B5 RAZOR, B6 POOL (the layout's keys) */
        ec_nav_key(EC_CAT_FILE, key);              /* 0.6.2: its page; tapped again there, its next lane */
        P->pool_carry = -1;
        A->redraw = 1;
        return;
    }
    if (pic == EC_PIC_POOL) {                      /* B10 ASSIGN, B11 preview */
        if (key == 10)
            pool_assign();
        else if (key == 11) {
            const ec_slotrt_t *r = &A->slot[P->pool_cur];
            if (P->pv_play && r->handle >= 0 && P->pv_handle == r->handle)
                P->pv_play = 0;
            else if (slot_has(P->pool_cur))
                pool_preview(P->pool_cur, 1);
        }
        A->redraw = 1;
        return;
    }
    if (pic != EC_PIC_FILE)
        return;
    switch (key) {
    case 1: folder_up(); break;
    case 2: folder_in(); break;
    case 10: load(); break;
    case 11: {
        const char *p = cursor_path();
        if (!p)
            break;
        if (P->pv_play && !strcmp(P->pv_path, p))
            P->pv_play = 0;
        else
            preview(p, 1);
    } break;
    default: break;
    }
    A->redraw = 1;
}
