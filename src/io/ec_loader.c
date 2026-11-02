/* SPDX-License-Identifier: GPL-3.0-only */
/* The sample loader (ec_loader.h) */
#include <stdio.h>
#include <string.h>

#include "ec_audiofile.h"
#include "ec_fs.h"
#include "ec_loader.h"
#include "ec_samplestore.h"

#define HEADER_READ 65536u
#define STAGE_BYTES FS_MAX_LEN               /* one request's worth */

static uint8_t g_stage[STAGE_BYTES];         /* the bytes of the read in flight / being decoded */
static uint8_t g_stat[16];
static char g_path[EC_SS_PATH];
static int g_state = EC_LD_IDLE, g_h = -1;
static const char *g_err = "";
static ec_fsreq_t g_req;
static uint64_t g_size;
static ec_afparse_t g_ps;
static ec_afinfo_t g_info;
static uint64_t g_read_off;                  /* the next data byte to read */
static uint32_t g_stage_frames, g_stage_done;   /* frames in g_stage, decoded so far */

static void fail(const char *why)
{
    ec_fs_cancel(&g_req);
    if (g_h >= 0)
        ec_ss_free(g_h);
    g_h = -1;
    g_err = why;
    g_state = EC_LD_ERROR;
}

int ec_load_start(const char *path)
{
    ec_load_cancel();
    snprintf(g_path, sizeof g_path, "%s", path);
    g_err = "";
    g_h = -1;
    memset(&g_ps, 0, sizeof g_ps);
    memset(&g_info, 0, sizeof g_info);
    g_stage_frames = g_stage_done = 0;
    if (ec_fs_start(&g_req, FS_STAT, g_path, 0, g_stat, 16) < 0) {
        fail(ec_fs_strerror(g_req.result));
        return -1;
    }
    g_state = EC_LD_STAT;
    return 0;
}

void ec_load_cancel(void)
{
    ec_fs_cancel(&g_req);
    if (g_state != EC_LD_DONE && g_state != EC_LD_IDLE && g_h >= 0)
        ec_ss_free(g_h);
    if (g_state != EC_LD_DONE)
        g_h = -1;
    g_state = EC_LD_IDLE;
}

static void start_header_read(uint64_t off)
{
    ec_fs_start(&g_req, FS_READ, g_path, (int64_t)off, g_stage, (int32_t)HEADER_READ);
}

/* the next data read: whole frames, at most one request */
static void start_data_read(void)
{
    ec_sample_t *s = ec_ss_get(g_h);
    const uint32_t ba = g_info.block_align;
    const uint64_t left_frames = (uint64_t)(s->frames - s->frames_ready);
    uint64_t frames = (STAGE_BYTES / ba);
    if (frames > left_frames)
        frames = left_frames;
    ec_fs_start(&g_req, FS_READ, g_path, (int64_t)g_read_off, g_stage, (int32_t)(frames * ba));
}

/* the sample's display name: the path's last segment */
static const char *base_name(const char *p)
{
    const char *s = strrchr(p, '/');
    return s ? s + 1 : p;
}

static void begin_data(void)
{
    ec_sample_t *s;
    if (!g_info.frames) {
        fail("empty");
        return;
    }
    g_h = ec_ss_alloc(g_info.frames);
    if (g_h < 0) {
        fail("too big");
        return;
    }
    s = ec_ss_get(g_h);
    s->rate = g_info.rate;
    s->channels = (uint8_t)(g_info.channels > 2 ? 2 : g_info.channels);
    s->bits = (uint8_t)g_info.bits;
    s->is_float = g_info.codec == EC_AC_FLOAT_LE || g_info.codec == EC_AC_FLOAT_BE;
    snprintf(s->path, sizeof s->path, "%s", g_path);
    snprintf(s->name, sizeof s->name, "%s", base_name(g_path));
    s->bpm = ec_bpm_from_name(s->name);
    g_read_off = g_info.data_off;
    g_state = EC_LD_DATA;
    start_data_read();
}

/* decode up to EC_LOAD_FRAMES_PER_TICK frames of the staged read; the overview follows */
static void decode_some(void)
{
    ec_sample_t *s = ec_ss_get(g_h);
    uint32_t n = g_stage_frames - g_stage_done, i;
    int16_t *dst;
    if (n > EC_LOAD_FRAMES_PER_TICK)
        n = EC_LOAD_FRAMES_PER_TICK;
    dst = s->data + (size_t)s->frames_ready * 2u;
    ec_af_decode(&g_info, g_stage + (size_t)g_stage_done * g_info.block_align, n, dst);
    for (i = 0; i < n; i++) {
        const uint32_t f = s->frames_ready + i;
        const uint32_t col = (uint32_t)((uint64_t)f * EC_SS_OVERVIEW / s->frames);
        int a = dst[2 * i], b = dst[2 * i + 1];
        uint32_t pk;
        a = a < 0 ? -a : a;
        b = b < 0 ? -b : b;
        pk = (uint32_t)(a > b ? a : b) >> 7;
        if (pk > 255)
            pk = 255;
        if (pk > s->overview[col])
            s->overview[col] = (uint8_t)pk;
    }
    s->frames_ready += n;
    g_stage_done += n;
}

int ec_load_step(void)
{
    switch (g_state) {
    case EC_LD_STAT:
        if (ec_fs_update(&g_req) != EC_FSR_DONE)
            break;
        if (g_req.result < 16) {
            fail(g_req.result < 0 ? ec_fs_strerror(g_req.result) : "bad stat");
            break;
        }
        {
            ec_fsstat_t st;
            ec_fs_stat_parse(g_stat, &st);
            if (st.is_dir) {
                fail("a folder");
                break;
            }
            g_size = st.size;
        }
        g_state = EC_LD_HEADER;
        start_header_read(0);
        break;
    case EC_LD_HEADER: {
        uint64_t need = 0, base;
        int r;
        if (ec_fs_update(&g_req) != EC_FSR_DONE)
            break;
        if (g_req.result < 0) {
            fail(ec_fs_strerror(g_req.result));
            break;
        }
        /* (the read's offset: 0 for the first, else the parser's last need) */
        base = g_ps.started ? g_ps.next : 0;
        r = ec_af_parse(&g_ps, g_stage, (uint32_t)g_req.result, base, g_size, &g_info, &need);
        g_req.state = EC_FSR_IDLE;
        if (r == EC_AF_NEED) {
            if (need >= g_size) {
                fail("bad file");
                break;
            }
            start_header_read(need);
        } else if (r == EC_AF_OK)
            begin_data();
        else
            fail(r == EC_AF_UNSUPPORTED ? "unsupported" : "bad file");
        break;
    }
    case EC_LD_DATA: {
        ec_sample_t *s = ec_ss_get(g_h);
        if (g_stage_done < g_stage_frames)     /* decoding the staged read */
            decode_some();
        else {                                 /* waiting for the next read */
            if (ec_fs_update(&g_req) != EC_FSR_DONE)
                break;
            if (g_req.result < 0) {
                fail(ec_fs_strerror(g_req.result));
                break;
            }
            g_req.state = EC_FSR_IDLE;
            g_stage_frames = (uint32_t)g_req.result / g_info.block_align;
            g_stage_done = 0;
            g_read_off += (uint64_t)g_stage_frames * g_info.block_align;
            if (!g_stage_frames) {             /* the file ended early: keep what came */
                s->frames = s->frames_ready;
                if (!s->frames) {
                    fail("empty");
                    break;
                }
                s->ready = 1;
                g_state = EC_LD_DONE;
                break;
            }
            decode_some();
        }
        if (g_stage_done >= g_stage_frames) {  /* the staged read is in: done, or the next read */
            g_stage_frames = g_stage_done = 0;
            if (s->frames_ready >= s->frames) {
                s->ready = 1;
                g_state = EC_LD_DONE;
            } else
                start_data_read();
        }
        break;
    }
    default:
        break;
    }
    return g_state;
}

int ec_load_state(void) { return g_state; }
int ec_load_handle(void) { return g_h; }
uint32_t ec_load_permille(void)
{
    const ec_sample_t *s = ec_ss_get(g_h);
    if (g_state == EC_LD_DONE)
        return 1000;
    if (!s || !s->frames)
        return 0;
    return (uint32_t)((uint64_t)s->frames_ready * 1000u / s->frames);
}
const char *ec_load_error(void) { return g_err; }
const char *ec_load_path(void) { return g_path; }
