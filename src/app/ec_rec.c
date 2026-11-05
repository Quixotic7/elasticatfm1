/* SPDX-License-Identifier: GPL-3.0-only */
/* E6: sampling (ec_rec.h). The pop-up's gestures, the take in the audio call, its landing in a pool slot and the WAV
 * writer. Every call happens inside one of the core's entry points (ec_plat.h: never concurrently), so the tick's
 * buttons may change the take's state directly; the audio call only starts, fills and ends it. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ec_app.h"
#include "ec_rec.h"

#define A (&ec_app)
#define R (&ec_app.rec)

enum { W_IDLE, W_MKDIR, W_PROBE, W_HDR, W_DATA, W_MKDATA };
#define REC_DIR "/data/rec"

static int clampi(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }

static const int LEN_BARS[EC_REC_NLEN] = {0, 1, 2, 4, 8, 16};

uint32_t ec_rec_len_frames(int len, float bpm)
{
    const int bars = LEN_BARS[clampi(len, 0, EC_REC_NLEN - 1)];
    if (!bars || bpm <= 0)
        return 0;
    return (uint32_t)floor((double)bars * 4.0 * 60.0 * EC_FS / (double)bpm + 0.5);
}

int ec_rec_thr_db(int thr) { return thr <= 0 ? 0 : -66 + 6 * clampi(thr, 1, EC_REC_NTHR - 1); }

const char *ec_rec_state_name(void)
{
    static const char *const N[] = {"idle", "armed", "rec", "stopped"};
    return N[clampi(R->state, 0, 3)];
}

int ec_rec_busy(void) { return R->state != EC_RS_IDLE || R->w != W_IDLE; }
int ec_rec_holds(int h) { return h >= 0 && ((R->state != EC_RS_IDLE && R->handle == h) || (R->w && R->whandle == h)); }

/* an armed take reserves the arena: FREE all of what is free, a length its frames at the tempo + a quarter + 1 s
 * (a slower tempo by its start; the take ends at the reserve) */
uint32_t ec_rec_reserve(void)
{
    const uint32_t fr = ec_ss_free_frames();
    const uint32_t len = ec_rec_len_frames(R->len, A->proj.bpm);
    uint32_t want;
    if (!len)
        return fr;
    want = len + len / 4u + EC_FS;
    return want < fr ? want : fr;
}

void ec_rec_init(void)
{
    memset(R, 0, sizeof *R);
    R->handle = R->whandle = -1;
    R->len = 3;                                    /* 4 bars, synced on PLAY, the input (the mockup's) */
    R->sync = EC_RSYNC_PLAY;
    R->src = EC_RSRC_IN;
    R->last_slot = -1;
    R->req.state = EC_FSR_IDLE;
    R->host_block = EC_BLOCK;
}

/* ------------------------------------------------------------------------------------------- the take --- */
static int target_slot(void) { return R->slot_set ? R->slot : ec_app_track_slot(A, A->track); }   /* (E6: the track's slot: a resample) */

static void drop(void)
{
    if (R->handle >= 0)
        ec_ss_free(R->handle);
    R->handle = -1;
    R->state = EC_RS_IDLE;
    R->n = R->cap = R->target = 0;
    R->sampler = 0;
    A->redraw = 1;
}
void ec_rec_drop(void) { drop(); }

/* the take's sync / threshold: the SAMPLER's own (next bar, or at once) over the setup's */
static int sync_of(void) { return R->sampler ? R->ssync : R->sync; }
static int thr_of(void) { return R->sampler ? 0 : R->thr; }

static int arm(void)
{
    uint32_t cap;
    int h;
    if (R->w) {
        ec_app_message("SAVING THE LAST TAKE");
        return -1;
    }
    cap = ec_rec_reserve();
    if (cap < EC_FS / 2u || (h = ec_ss_alloc(cap)) < 0) {
        ec_app_alert("NO ROOM: POOL FULL");
        return -1;
    }
    R->handle = h;
    R->cap = cap;
    R->n = R->target = 0;
    R->full = 0;
    R->slot = target_slot();
    R->was_playing = A->seq.playing;
    R->state = EC_RS_ARMED;
    R->sampler = 0;
    A->redraw = 1;
    return 0;
}

/* 0.6.0: the SAMPLER's B11: a take into slot s, starting on the next bar (EC_RSYNC_BAR) or at once (FREE) */
int ec_rec_arm_to(int s, int sync)
{
    if (R->state != EC_RS_IDLE || arm() != 0)
        return -1;
    R->slot = s;
    R->sampler = 1;
    R->ssync = sync;
    return 0;
}

/* the take starts at sample time `at` (its frame 0), shifted by `shift` frames */
static void start(uint64_t at, uint32_t shift)
{
    const uint32_t len = ec_rec_len_frames(R->len, (float)A->seq.bpm);
    R->start_at = at + shift;
    R->shift = shift;
    R->target = len && len < R->cap ? len : len ? R->cap : 0;
    R->n = 0;
    R->state = EC_RS_RUN;
    A->redraw = 1;
}

/* a synced take of the input: latency_frames is the round trip (FM1VST docs/VIRTUAL-CARTS.md 8, every host): output
 * frame j comes back in input frame j + latency_frames, so the take of a sync point at output frame P starts at input
 * frame P + latency_frames. Nothing of our own added (the host's number includes its buffering) */
static uint32_t in_shift(void)
{
    return R->src == EC_RSRC_IN ? ec_plat_ain_latency() : 0u;
}

void ec_rec_pre(uint32_t n)
{
    const ec_seq_t *s = &A->seq;
    const uint64_t t0 = s->now;
    if (R->state == EC_RS_ARMED) {
        switch (sync_of()) {
        case EC_RSYNC_PLAY:                        /* the transport's start: step 1 at its sample_base */
            if (s->playing && !R->was_playing)
                start(s->sample_base, in_shift());
            break;
        case EC_RSYNC_BAR:                         /* the next bar line (4 beats) while playing */
            if (s->playing) {
                const double spb = s->fs * 60.0 / (s->bpm > 1 ? s->bpm : 120.0);
                const double b0 = ec_seq_beats(s), bar = ceil(b0 / 4.0 - 1e-9) * 4.0;
                const uint64_t at = t0 + (uint64_t)ceil((bar - b0) * spb - 1e-6);
                if (at < t0 + n)
                    start(at, in_shift());
            }
            break;
        default:                                   /* FREE: at once (the threshold: in the capture) */
            if (!thr_of())
                start(t0, 0);
            break;
        }
    }
    R->was_playing = s->playing;
}

void ec_rec_post(const int16_t *in, uint32_t in_n, const int16_t *out, uint32_t n)
{
    const uint64_t t0 = A->seq.now - n;
    uint32_t k;
    float pl = 0, pr = 0;
    for (k = 0; in && k < in_n && k < n; k++) {    /* the input meters */
        const float l = fabsf((float)in[2 * k]), r = fabsf((float)in[2 * k + 1]);
        pl = l > pl ? l : pl;
        pr = r > pr ? r : pr;
    }
    pl /= 32768.0f;
    pr /= 32768.0f;
    R->in_pk[0] = pl > R->in_pk[0] ? pl : R->in_pk[0];
    R->in_pk[1] = pr > R->in_pk[1] ? pr : R->in_pk[1];

    k = 0;
    if (R->state == EC_RS_ARMED && sync_of() == EC_RSYNC_FREE && thr_of()) {   /* the threshold: the first frame over */
        const float th = 32768.0f * powf(10.0f, (float)ec_rec_thr_db(R->thr) / 20.0f);
        for (; k < n; k++) {
            int16_t l = 0, r = 0;
            if (R->src == EC_RSRC_MST) {
                l = out[2 * k];
                r = out[2 * k + 1];
            } else if (in && k < in_n) {
                l = in[2 * k];
                r = in[2 * k + 1];
            }
            if (fabsf((float)l) >= th || fabsf((float)r) >= th) {
                start(t0 + k, 0);
                break;
            }
        }
    }
    if (R->state == EC_RS_RUN) {
        ec_ss_sample_t *x = ec_ss_get(R->handle);
        if (!x) {
            R->state = EC_RS_IDLE;
            return;
        }
        for (; k < n; k++) {
            int16_t *d;
            if (t0 + k < R->start_at)
                continue;
            if (R->n >= R->cap) {
                R->full = 1;
                R->state = EC_RS_STOPPED;
                break;
            }
            d = x->data + 2u * R->n;
            if (R->src == EC_RSRC_MST) {
                d[0] = out[2 * k];
                d[1] = out[2 * k + 1];
            } else if (in && k < in_n) {
                d[0] = in[2 * k];
                d[1] = in[2 * k + 1];
            } else
                d[0] = d[1] = 0;
            R->n++;
            if (R->target && R->n >= R->target) {
                R->state = EC_RS_STOPPED;
                break;
            }
        }
        x->frames_ready = R->n;
    }
}

/* ------------------------------------------------------------------------------------------- landing --- */
static void name_paths(int num, ec_ss_sample_t *x)
{
    snprintf(R->wpath, sizeof R->wpath, REC_DIR "/rec-%04d.wav", num);
    if (x) {
        snprintf(x->name, sizeof x->name, "rec-%04d.wav", num);
        snprintf(x->path, sizeof x->path, "%s", R->wpath);
    }
    if (R->wslot >= 0 && A->slot[R->wslot].handle == R->whandle)
        snprintf(A->proj.slot[R->wslot].path, EC_PATH, "%s", R->wpath);
}

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8), p[2] = (uint8_t)(v >> 16), p[3] = (uint8_t)(v >> 24);
}

static void land(void)
{
    ec_ss_sample_t *x = ec_ss_get(R->handle);
    ec_slot_t *sl;
    const int s = R->slot;
    const uint32_t n = R->n;
    char m[32];
    uint32_t c;
    if (!x || n < EC_REC_MIN_FRAMES) {
        drop();
        ec_app_message("REC: NOTHING TAKEN");
        return;
    }
    ec_ss_shrink(R->handle, n);
    x->frames = x->frames_ready = n;
    x->rate = EC_FS;
    x->channels = 2;
    x->bits = 16;
    x->is_float = 0;
    x->bpm = A->proj.bpm;
    for (c = 0; c < EC_SS_OVERVIEW; c++) {         /* the waveform: each column's peak, at most 2048 frames read */
        const uint32_t a = (uint32_t)((uint64_t)c * n / EC_SS_OVERVIEW), b = (uint32_t)((uint64_t)(c + 1) * n /
                                                                                    EC_SS_OVERVIEW);
        const uint32_t step = (b - a) / 2048u + 1u;
        uint32_t i;
        int pk = 0;
        for (i = a; i < b; i += step) {
            const int l = abs(x->data[2 * i]), r = abs(x->data[2 * i + 1]);
            pk = l > pk ? l : pk;
            pk = r > pk ? r : pk;
        }
        x->overview[c] = (uint8_t)((pk >> 7) > 255 ? 255 : pk >> 7);
    }
    x->ready = 1;
    R->whandle = R->handle;
    R->wslot = s;
    R->wnum++;
    name_paths(R->wnum, x);
    sl = &A->proj.slot[s];
    snprintf(sl->path, EC_PATH, "%s", R->wpath);
    sl->bpm = A->proj.bpm;
    sl->trim_start = 0;
    sl->trim_end = 1;
    sl->gain = 1;
    {
        const float st = floorf((float)n * A->proj.bpm * 4.0f / (60.0f * EC_FS) + 0.5f);
        sl->steps = st < 1 ? 1 : st > 512 ? 512 : st;
    }
    ec_app_slot_take(s, R->handle);
    R->last_slot = s;
    R->last_frames = n;
    R->takes++;
    R->handle = -1;
    R->state = EC_RS_IDLE;
    R->sampler = 0;
    /* the writer: the header (PCM 16-bit stereo 44.1 kHz), then the frames straight from the arena */
    memcpy(R->hdr, "RIFF", 4);
    put32(R->hdr + 4, 36u + n * 4u);
    memcpy(R->hdr + 8, "WAVEfmt ", 8);
    put32(R->hdr + 16, 16);
    R->hdr[20] = 1, R->hdr[21] = 0, R->hdr[22] = 2, R->hdr[23] = 0;
    put32(R->hdr + 24, EC_FS);
    put32(R->hdr + 28, EC_FS * 4u);
    R->hdr[32] = 4, R->hdr[33] = 0, R->hdr[34] = 16, R->hdr[35] = 0;
    memcpy(R->hdr + 36, "data", 4);
    put32(R->hdr + 40, n * 4u);
    R->wdone = 0;
    R->wtotal = n * 4u;
    R->w = W_MKDIR;
    R->wmkdata = 0;
    R->req.state = EC_FSR_IDLE;
    snprintf(m, sizeof m, "REC %u:%02u > POOL %02d%s", n / EC_FS / 60u, n / EC_FS % 60u, s + 1, R->full ? " FULL" : "");
    ec_app_message(m);
    ec_app_mark_dirty();
}

/* ------------------------------------------------------------------------------------------- the writer --- */
static void writer_end(int ok, const char *why)
{
    char m[32];
    if (ok) {
        R->saved++;
        snprintf(m, sizeof m, "SAVED %s", strrchr(R->wpath, '/') + 1);
    } else {
        R->wfail++;
        snprintf(R->werr, sizeof R->werr, "%s", why);
        snprintf(m, sizeof m, "NOT SAVED: %s", why);
    }
    ec_app_message(m);
    if (!ec_app_handle_used(R->whandle))           /* (replaced while it was being written) */
        ec_ss_free(R->whandle);
    R->whandle = -1;
    R->w = W_IDLE;
    ec_app_mark_dirty();
}

static void writer(void)
{
    ec_ss_sample_t *x;
    int32_t res;
    if (!R->w)
        return;
    x = ec_ss_get(R->whandle);
    if (!x) {
        writer_end(0, "GONE");
        return;
    }
    if (R->req.state == EC_FSR_PENDING && ec_fs_update(&R->req) != EC_FSR_DONE)
        return;
    if (R->req.state != EC_FSR_DONE) {            /* the step's request */
        switch (R->w) {
        case W_MKDIR: ec_fs_start(&R->req, FS_MKDIR, REC_DIR, 0, 0, 0); break;
        case W_MKDATA: ec_fs_start(&R->req, FS_MKDIR, "/data", 0, 0, 0); break;
        case W_PROBE: ec_fs_start(&R->req, FS_STAT, R->wpath, 0, R->stat, 16); break;
        case W_HDR: ec_fs_start(&R->req, FS_WRITE, R->wpath, -1, R->hdr, 44); break;
        default: {
            const uint32_t left = R->wtotal - R->wdone;
            const int32_t len = (int32_t)(left < FS_MAX_LEN ? left : FS_MAX_LEN);
            ec_fs_start(&R->req, FS_WRITE, R->wpath, 44 + (int64_t)R->wdone, (uint8_t *)x->data + R->wdone, len);
        } break;
        }
        return;                                    /* (answered between two calls: the next tick) */
    }
    res = R->req.result;
    R->req.state = EC_FSR_IDLE;
    if (res == FS_EBUSY)                           /* (32 in flight: again next tick) */
        return;
    switch (R->w) {
    case W_MKDIR:
        if (res == FS_ENOFOLDER || res == FS_EACCES)
            writer_end(0, res == FS_ENOFOLDER ? "NO FOLDER" : "NO ACCESS");
        else if (res == FS_ENOENT && !R->wmkdata)
            R->w = W_MKDATA;                       /* (a host that has not made the cart's /data yet) */
        else
            R->w = W_PROBE;                        /* (made, or there already) */
        break;
    case W_MKDATA:
        R->wmkdata = 1;
        R->w = W_MKDIR;
        break;
    case W_PROBE:
        if (res >= 0 && R->wnum < 9999) {          /* taken: the next number */
            R->wnum++;
            name_paths(R->wnum, x);
        } else if (res == FS_ENOENT)
            R->w = W_HDR;
        else
            writer_end(0, ec_fs_strerror(res));
        break;
    case W_HDR:
        if (res == 44)
            R->w = W_DATA;
        else
            writer_end(0, res < 0 ? ec_fs_strerror(res) : "SHORT WRITE");
        break;
    default:
        if (res <= 0) {
            writer_end(0, res < 0 ? ec_fs_strerror(res) : "SHORT WRITE");
            break;
        }
        R->wdone += (uint32_t)res;
        if (R->wdone >= R->wtotal)
            writer_end(1, "");
        break;
    }
}

void ec_rec_tick(void)
{
    if (R->state == EC_RS_STOPPED)
        land();
    writer();
}

void ec_rec_frame(void)
{
    int c;
    for (c = 0; c < 2; c++) {
        const float p = R->in_pk[c] > 1 ? 1 : R->in_pk[c];
        R->in_meter[c] = p > R->in_meter[c] ? p : R->in_meter[c] * 0.85f;
        R->in_pk[c] = 0;
    }
}

/* ------------------------------------------------------------------------------------------- the pop-up --- */
void ec_rec_open(void)
{
    R->open = 1;
    R->row = 0;
    if (!R->slot_set && R->state == EC_RS_IDLE)
        R->slot = ec_app_track_slot(A, A->track);
    A->redraw = 1;
}

void ec_rec_close(void)
{
    R->open = 0;
    A->redraw = 1;
}

/* REC / OCT+: arm, then (armed) start now, then (taking) stop and keep. OCT-: close, disarm, discard */
int ec_rec_button(int b)
{
    if (!R->open)
        return 0;
    if (b == EC_B_REC || b == EC_B_OCTUP) {
        switch (R->state) {
        case EC_RS_IDLE: (void)arm(); break;
        case EC_RS_ARMED: start(A->seq.now, 0); break;   /* punch in: from the next block */
        case EC_RS_RUN:
            if (R->n)
                R->state = EC_RS_STOPPED;
            else
                drop();                            /* (still inside the latency shift: nothing yet) */
            break;
        default: break;
        }
        A->redraw = 1;
        return 1;
    }
    if (b == EC_B_OCTDN) {
        if (R->state == EC_RS_IDLE)
            ec_rec_close();
        else if (R->state != EC_RS_STOPPED) {
            const int was = R->state;
            drop();
            ec_app_message(was == EC_RS_ARMED ? "DISARMED" : "TAKE DISCARDED");
        }
        return 1;
    }
    return 0;
}

const char *ec_rec_row_label(int row)
{
    static const char *const L[EC_REC_NROW] = {"SOURCE", "LENGTH", "THRESHOLD", "SYNC", "SLOT"};
    return row >= 0 && row < EC_REC_NROW ? L[row] : "";
}

void ec_rec_row_value(int row, char *out, int cap)
{
    static const char *const SRC[] = {"IN", "MASTER"}, *const SYNC[] = {"PLAY", "BAR", "FREE"};
    static const char *const LEN[] = {"FREE", "1 BAR", "2 BARS", "4 BARS", "8 BARS", "16 BARS"};
    switch (row) {
    case EC_REC_ROW_SLOT:
        snprintf(out, (size_t)cap, "POOL %02d", (R->state == EC_RS_IDLE ? target_slot() : R->slot) + 1);
        break;
    case EC_REC_ROW_SRC: snprintf(out, (size_t)cap, "%s", SRC[clampi(R->src, 0, EC_NRSRC - 1)]); break;
    case EC_REC_ROW_LEN: snprintf(out, (size_t)cap, "%s", LEN[clampi(R->len, 0, EC_REC_NLEN - 1)]); break;
    case EC_REC_ROW_THR:
        if (R->thr)
            snprintf(out, (size_t)cap, "%dDB", ec_rec_thr_db(R->thr));
        else
            snprintf(out, (size_t)cap, "OFF");
        break;
    case EC_REC_ROW_SYNC: snprintf(out, (size_t)cap, "%s", SYNC[clampi(R->sync, 0, EC_NRSYNC - 1)]); break;
    default: out[0] = 0; break;
    }
}

int ec_rec_row_dim(int row)
{
    if (row == EC_REC_ROW_THR)
        return R->sync != EC_RSYNC_FREE;
    return R->state != EC_RS_IDLE;
}

int ec_rec_knob(int role, int det)
{
    const int idle = R->state == EC_RS_IDLE;
    if (!R->open)
        return 0;
    if (role != EC_E_SELECT && role < EC_E_K1)
        return 0;
    if (ec_app_list_knob(role, det, &R->row, EC_REC_NROW))   /* 0.6.1: a list menu: SEL the row, K1 its value */
        switch (R->row) {
        case EC_REC_ROW_SLOT:                      /* the target slot (not while a take is armed or running) */
            if (idle) {
                R->slot = clampi(target_slot() + det, 0, EC_NSLOT - 1);
                R->slot_set = 1;
            }
            break;
        case EC_REC_ROW_SRC:
            if (idle)
                R->src = clampi(R->src + det, 0, EC_NRSRC - 1);
            break;
        case EC_REC_ROW_LEN:
            if (idle)
                R->len = clampi(R->len + det, 0, EC_REC_NLEN - 1);
            break;
        case EC_REC_ROW_THR: R->thr = clampi(R->thr + det, 0, EC_REC_NTHR - 1); break;
        case EC_REC_ROW_SYNC:
            if (idle)
                R->sync = clampi(R->sync + det, 0, EC_NRSYNC - 1);
            break;
        default: break;
        }
    A->redraw = 1;
    return 1;
}

void ec_rec_labels(void)
{
    static const char *const REC[] = {"ARM", "REC NOW", "STOP", ""};
    static const char *const BACK[] = {"CLOSE", "DISARM", "DISCARD", ""};
    const int st = clampi(R->state, 0, 3);
    ec_plat_label(EC_LBL_KNOB0 + EC_E_SELECT, "ROW");   /* 0.6.1: a list menu: K1 the selected row */
    ec_plat_label(EC_LBL_KNOB0 + EC_E_K1, ec_rec_row_label(clampi(R->row, 0, EC_REC_NROW - 1)));
    ec_plat_label(EC_LBL_KNOB0 + EC_E_K2, "");
    ec_plat_label(EC_LBL_KNOB0 + EC_E_K3, "");
    ec_plat_label(EC_LBL_KNOB0 + EC_E_K4, "");
    ec_plat_label(EC_LBL_BTN0 + EC_B_REC, REC[st]);
    ec_plat_label(EC_LBL_BTN0 + EC_B_OCTUP, REC[st]);
    ec_plat_label(EC_LBL_BTN0 + EC_B_OCTDN, BACK[st]);
    ec_plat_label(EC_LBL_BTN0 + EC_B_HOME, "HOME");
}

void ec_rec_ui_info(char *buf, uint32_t n)
{
    static const char *const SRC[] = {"in", "mst"}, *const SY[] = {"play", "bar", "free"};
    snprintf(buf, n,
             " ain:%d lat:%u rec:%s recpop:%d rsrc:%s rlen:%d rthr:%d rsync:%s rslot:%d rframes:%u rstart:%llu "
             "rshift:%u takes:%u rsaved:%d rfail:%d rpath:%s",
             ec_plat_ain_state(), ec_plat_ain_latency(), ec_rec_state_name(), R->open, SRC[clampi(R->src, 0, 1)],
             LEN_BARS[clampi(R->len, 0, EC_REC_NLEN - 1)], ec_rec_thr_db(R->thr), SY[clampi(R->sync, 0, 2)],
             (R->state == EC_RS_IDLE ? target_slot() : R->slot) + 1, R->state == EC_RS_IDLE ? R->last_frames : R->n,
             (unsigned long long)R->start_at, R->shift, R->takes, R->saved, R->wfail, R->wpath[0] ? R->wpath : "-");
}
