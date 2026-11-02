/* SPDX-License-Identifier: GPL-3.0-only */
/* The native stand-in for rfm1_host_1 (ec_fs_native.h). Only with EC_FS_NATIVE: a glob of src/io stays safe in
 * FM1VST's build, where the host provides rfm1_fs_*. */
#ifdef EC_FS_NATIVE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "ec_fs_native.h"
#include "rfm1_host.h"

typedef struct {
    int32_t id, op, result;
    int done, ready;          /* ready: requested before the last pump, so due now */
    int area;
    int64_t off;
    void *buf;
    int32_t len;
    char path[PATH_MAX];
} nreq_t;

static char g_root[PATH_MAX], g_id[64];
static nreq_t g_q[FS_MAX_INFLIGHT];
static int32_t g_next, g_changed = 1;
static uint32_t g_served;

void ec_fs_native_root(const char *root, const char *cart_id)
{
    g_root[0] = 0;
    if (root && !realpath(root, g_root))
        g_root[0] = 0;
    snprintf(g_id, sizeof g_id, "%s", cart_id ? cart_id : "elasticat");
    memset(g_q, 0, sizeof g_q);
    g_changed++;
}
void ec_fs_native_touch(void) { g_changed++; }
uint32_t ec_fs_native_served(void) { return g_served; }

static int32_t err_of(int e)
{
    return e == ENOENT || e == ENOTDIR ? FS_ENOENT : e == EACCES || e == EPERM ? FS_EACCES : e == ENOSPC ? FS_ENOSPC
                                                                                                          : FS_EIO;
}

/* a virtual path -> a host path; area 0 audio, 1 code, 2 data. "..", "." and empty segments refused */
static int32_t map(const char *v, int n, char *out, int *area)
{
    static const char *const AREA[3] = {"audio", "code", "data"};
    int a = -1, i, k;
    size_t at;
    char seg[256];
    for (i = 0; i < 3; i++) {
        const int l = (int)strlen(AREA[i]);
        if (n >= l + 1 && v[0] == '/' && !strncmp(v + 1, AREA[i], (size_t)l) && (n == l + 1 || v[l + 1] == '/')) {
            a = i;
            break;
        }
    }
    if (a < 0)
        return FS_EACCES;
    at = a == 0 ? (size_t)snprintf(out, PATH_MAX, "%s/audio", g_root)
                : (size_t)snprintf(out, PATH_MAX, "%s/%s/%s", g_root, AREA[a], g_id);
    i = (int)strlen(AREA[a]) + 1;
    while (i < n) {
        i++;
        if (i >= n)
            break;
        for (k = 0; i < n && v[i] != '/'; i++) {
            if (k >= (int)sizeof seg - 1)
                return FS_EACCES;
            seg[k++] = v[i];
        }
        seg[k] = 0;
        if (!k || !strcmp(seg, ".") || !strcmp(seg, ".."))
            return FS_EACCES;
        if (at + 1 + (size_t)k >= PATH_MAX)
            return FS_EIO;
        at += (size_t)snprintf(out + at, PATH_MAX - at, "/%s", seg);
    }
    *area = a;
    return 0;
}

int32_t rfm1_fs_request(int32_t op, const char *path, int32_t path_len, int64_t offset, void *buf, int32_t len)
{
    int i, slot = -1, area = 0;
    int32_t rc;
    if (!g_root[0])
        return FS_ENOFOLDER;
    for (i = 0; i < FS_MAX_INFLIGHT; i++)
        if (!g_q[i].id) {
            slot = i;
            break;
        }
    if (slot < 0)
        return FS_EBUSY;
    if (op < FS_STAT || op > FS_MKDIR || path_len <= 0 || len < 0 || len > FS_MAX_LEN || (op == FS_STAT && len < 16))
        return FS_EIO;
    rc = map(path, path_len, g_q[slot].path, &area);
    if (rc)
        return rc;
    if ((op == FS_WRITE || op == FS_DELETE || op == FS_MKDIR) && area != 2)
        return FS_EACCES;
    if (++g_next <= 0)
        g_next = 1;
    g_q[slot].id = g_next;
    g_q[slot].op = op;
    g_q[slot].done = g_q[slot].ready = 0;
    g_q[slot].area = area;
    g_q[slot].off = offset;
    g_q[slot].buf = buf;
    g_q[slot].len = len;
    return g_next;
}

int32_t rfm1_fs_poll(int32_t id)
{
    int i;
    for (i = 0; i < FS_MAX_INFLIGHT; i++)
        if (id > 0 && g_q[i].id == id) {
            if (!g_q[i].done)
                return FS_PENDING;
            g_q[i].id = 0;
            return g_q[i].result;
        }
    return g_root[0] ? FS_EIO : FS_ENOFOLDER;
}

void rfm1_fs_cancel(int32_t id)
{
    int i;
    for (i = 0; i < FS_MAX_INFLIGHT; i++)
        if (id > 0 && g_q[i].id == id && !g_q[i].done) {
            g_q[i].done = 1;
            g_q[i].result = FS_ECANCELED;
        }
}

int32_t rfm1_fs_changed(void) { return g_changed; }

static int cmp(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

static int32_t serve(nreq_t *q)
{
    struct stat st;
    uint8_t *m = (uint8_t *)q->buf;
    switch (q->op) {
    case FS_STAT: {
        uint64_t size;
        uint32_t mt, dir;
        if (stat(q->path, &st))
            return err_of(errno);
        dir = S_ISDIR(st.st_mode) ? 1u : 0u;
        size = dir ? 0 : (uint64_t)st.st_size;
        mt = (uint32_t)st.st_mtime;
        memcpy(m, &size, 8);
        memcpy(m + 8, &mt, 4);
        memcpy(m + 12, &dir, 4);
        return 16;
    }
    case FS_LIST: {
        DIR *d = opendir(q->path);
        struct dirent *e;
        char **names = NULL;
        size_t nn = 0, cap = 0, i;
        int32_t written = 0;
        uint32_t at = 0;
        if (!d)
            return err_of(errno);
        while ((e = readdir(d))) {
            if (e->d_name[0] == '.' || strlen(e->d_name) > 255)
                continue;
            if (nn == cap) {
                cap = cap ? cap * 2 : 64;
                names = realloc(names, cap * sizeof *names);
            }
            names[nn++] = strdup(e->d_name);
        }
        closedir(d);
        if (nn)
            qsort(names, nn, sizeof *names, cmp);
        for (i = q->off < 0 ? nn : (size_t)q->off; i < nn; i++) {
            char full[PATH_MAX];
            const size_t nl = strlen(names[i]);
            if (at + 2u + nl > (uint32_t)q->len)
                break;
            snprintf(full, sizeof full, "%s/%s", q->path, names[i]);
            m[at] = (uint8_t)(!stat(full, &st) && S_ISDIR(st.st_mode));
            m[at + 1] = (uint8_t)nl;
            memcpy(m + at + 2, names[i], nl);
            at += 2u + (uint32_t)nl;
            written++;
        }
        for (i = 0; i < nn; i++)
            free(names[i]);
        free(names);
        return written;
    }
    case FS_READ: {
        ssize_t r;
        int e;
        const int fd = open(q->path, O_RDONLY);
        if (fd < 0)
            return err_of(errno);
        if (!fstat(fd, &st) && S_ISDIR(st.st_mode)) {
            close(fd);
            return FS_EIO;
        }
        r = pread(fd, m, (size_t)q->len, q->off < 0 ? 0 : (off_t)q->off);
        e = errno;
        close(fd);
        return r < 0 ? err_of(e) : (int32_t)r;
    }
    case FS_WRITE: {
        char base[PATH_MAX];
        ssize_t w;
        int fd, e;
        snprintf(base, sizeof base, "%s/data", g_root);
        mkdir(base, 0755);
        snprintf(base, sizeof base, "%s/data/%s", g_root, g_id);
        mkdir(base, 0755);
        fd = open(q->path, O_WRONLY | O_CREAT | (q->off < 0 ? O_TRUNC : 0), 0644);
        if (fd < 0)
            return err_of(errno);
        w = pwrite(fd, m, (size_t)q->len, q->off < 0 ? 0 : (off_t)q->off);
        e = errno;
        close(fd);
        return w < 0 ? err_of(e) : (int32_t)w;
    }
    case FS_DELETE:
        if (stat(q->path, &st))
            return err_of(errno);
        if (S_ISDIR(st.st_mode) ? rmdir(q->path) : unlink(q->path))
            return err_of(errno);
        return 0;
    case FS_MKDIR:
        if (mkdir(q->path, 0755) && !(errno == EEXIST && !stat(q->path, &st) && S_ISDIR(st.st_mode)))
            return err_of(errno);
        return 0;
    }
    return FS_EIO;
}

void ec_fs_native_pump(void)
{
    int i;
    for (i = 0; i < FS_MAX_INFLIGHT; i++)
        if (g_q[i].id && !g_q[i].done) {
            g_q[i].result = serve(&g_q[i]);
            g_q[i].done = 1;
            g_served++;
        }
}

#else
typedef int ec_fs_native_unused;   /* (FM1VST's build: the host provides rfm1_fs_*) */
#endif
