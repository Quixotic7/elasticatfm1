/* SPDX-License-Identifier: GPL-3.0-only */
/* The async file layer (ec_fs.h) */
#include <string.h>

#include "ec_fs.h"

static int g_inflight;

int ec_fs_start(ec_fsreq_t *r, int op, const char *path, int64_t offset, void *buf, int32_t len)
{
    int32_t id;
    if (r->state == EC_FSR_PENDING)
        ec_fs_cancel(r);
    if (len > FS_MAX_LEN)
        len = FS_MAX_LEN;
    if (g_inflight >= FS_MAX_INFLIGHT) {
        r->state = EC_FSR_DONE;
        r->result = FS_EBUSY;
        return FS_EBUSY;
    }
    id = rfm1_fs_request(op, path, (int32_t)strlen(path), offset, buf, len);
    if (id <= 0) {
        r->state = EC_FSR_DONE;
        r->result = id ? id : FS_EIO;
        return r->result;
    }
    r->id = id;
    r->result = FS_PENDING;
    r->state = EC_FSR_PENDING;
    g_inflight++;
    return 0;
}

int ec_fs_update(ec_fsreq_t *r)
{
    if (r->state == EC_FSR_PENDING) {
        const int32_t v = rfm1_fs_poll(r->id);
        if (v != FS_PENDING) {
            r->result = v;
            r->state = EC_FSR_DONE;
            r->id = 0;
            g_inflight--;
        }
    }
    return r->state;
}

void ec_fs_cancel(ec_fsreq_t *r)
{
    if (r->state == EC_FSR_PENDING) {
        rfm1_fs_cancel(r->id);
        (void)rfm1_fs_poll(r->id);   /* (the id is freed once its result, FS_ECANCELED, is read) */
        g_inflight--;
    }
    r->id = 0;
    r->state = EC_FSR_IDLE;
    r->result = 0;
}

int32_t ec_fs_changed(void) { return rfm1_fs_changed(); }
int ec_fs_inflight(void) { return g_inflight; }

const char *ec_fs_strerror(int32_t err)
{
    switch (err) {
    case FS_PENDING: return "pending";
    case FS_ENOENT: return "not found";
    case FS_EACCES: return "not allowed";
    case FS_EIO: return "io error";
    case FS_ENOSPC: return "disk full";
    case FS_EBUSY: return "busy";
    case FS_ECANCELED: return "canceled";
    case FS_ENOFOLDER: return "no folder";
    default: return err < 0 ? "error" : "ok";
    }
}

void ec_fs_stat_parse(const uint8_t rec[16], ec_fsstat_t *out)
{
    uint64_t s = 0;
    int i;
    for (i = 7; i >= 0; i--)
        s = s << 8 | rec[i];
    out->size = s;
    out->mtime = (uint32_t)rec[8] | (uint32_t)rec[9] << 8 | (uint32_t)rec[10] << 16 | (uint32_t)rec[11] << 24;
    out->is_dir = (uint32_t)rec[12] | (uint32_t)rec[13] << 8 | (uint32_t)rec[14] << 16 | (uint32_t)rec[15] << 24;
}
