/* SPDX-License-Identifier: GPL-3.0-only */
/* The async file layer over the host's rfm1_host_1 API (FM1VST docs/VIRTUAL-CARTS.md section 4). A request is a
 * caller-owned ec_fsreq_t: start it, then call ec_fs_update() once per tick until it is done. Nothing ever blocks:
 * the host completes a request between two calls into the module. While a request is pending its buffer belongs to
 * the host. Paths are virtual (/audio/.., /data/..), NUL-terminated here. */
#ifndef EC_FS_H
#define EC_FS_H
#include <stdint.h>
#include "rfm1_host.h"     /* FS_* ops and errors, rfm1_fs_* */

enum { EC_FSR_IDLE, EC_FSR_PENDING, EC_FSR_DONE };

typedef struct {
    int32_t id;          /* the host's request id while pending */
    int32_t result;      /* when done: bytes / entries / 16, or an FS_E* error (< 0) */
    uint8_t state;       /* EC_FSR_* */
} ec_fsreq_t;

/* start a request (op FS_STAT / FS_LIST / FS_READ / FS_WRITE / FS_DELETE / FS_MKDIR). Returns 0 when the host took
 * it (state PENDING), or the error (state DONE, result = the error: FS_EBUSY when 32 are in flight) */
int ec_fs_start(ec_fsreq_t *r, int op, const char *path, int64_t offset, void *buf, int32_t len);
/* poll a pending request; returns its state (DONE: r->result holds the answer) */
int ec_fs_update(ec_fsreq_t *r);
/* give a pending request up (its buffer is ours again); state IDLE */
void ec_fs_cancel(ec_fsreq_t *r);
/* the host's folder-change counter (rescan when it moves) */
int32_t ec_fs_changed(void);
/* requests started and not yet answered (at most FS_MAX_INFLIGHT) */
int ec_fs_inflight(void);
const char *ec_fs_strerror(int32_t err);

/* the 16-byte FS_STAT record */
typedef struct {
    uint64_t size;
    uint32_t mtime;
    uint32_t is_dir;
} ec_fsstat_t;
void ec_fs_stat_parse(const uint8_t rec[16], ec_fsstat_t *out);

#endif
