/* SPDX-License-Identifier: GPL-3.0-only */
/* The recursive folder scan (ec_browse.h) */
#include <stdio.h>
#include <string.h>

#include "ec_audiofile.h"
#include "ec_browse.h"
#include "ec_fs.h"

#define POOL_BYTES (EC_BROWSE_MAX * 96)
#define LIST_BYTES 16384
#define PATH_MAX_EC 256

typedef struct {
    char path[PATH_MAX_EC];
    uint8_t depth;
} dir_t;

static char g_pool[POOL_BYTES];        /* the files' paths, NUL-terminated, one after another */
static uint32_t g_pool_top;
static uint32_t g_file[EC_BROWSE_MAX]; /* offsets into g_pool */
static uint16_t g_name[EC_BROWSE_MAX]; /* where the name starts in the path */
static int g_nfiles;
static dir_t g_dirs[EC_BROWSE_DIRS];
static int g_ndirs, g_cur;             /* folders queued, the one being listed */
static int64_t g_entry;                /* the next entry to list in it */
static uint8_t g_list[LIST_BYTES];
static ec_fsreq_t g_req;
static int g_state = EC_BR_IDLE;
static int32_t g_err;
static int g_trunc;

void ec_browse_start(const char *root)
{
    ec_fs_cancel(&g_req);
    g_pool_top = 0;
    g_nfiles = 0;
    g_ndirs = 1;
    g_cur = 0;
    g_entry = 0;
    g_err = 0;
    g_trunc = 0;
    snprintf(g_dirs[0].path, sizeof g_dirs[0].path, "%s", root);
    g_dirs[0].depth = 0;
    g_state = EC_BR_BUSY;
}

static void add_file(const char *dir, const char *name, int nl)
{
    const size_t dl = strlen(dir);
    const size_t need = dl + 1u + (size_t)nl + 1u;
    if (g_nfiles >= EC_BROWSE_MAX || g_pool_top + need > POOL_BYTES || need > PATH_MAX_EC) {
        g_trunc = 1;
        return;
    }
    memcpy(g_pool + g_pool_top, dir, dl);
    g_pool[g_pool_top + dl] = '/';
    memcpy(g_pool + g_pool_top + dl + 1, name, (size_t)nl);
    g_pool[g_pool_top + need - 1] = 0;
    g_file[g_nfiles] = g_pool_top;
    g_name[g_nfiles] = (uint16_t)(dl + 1);
    g_nfiles++;
    g_pool_top += (uint32_t)need;
}

static void add_dir(const char *dir, const char *name, int nl, int depth)
{
    const size_t dl = strlen(dir);
    if (g_ndirs >= EC_BROWSE_DIRS || depth > EC_BROWSE_DEPTH || dl + 1u + (size_t)nl + 1u > PATH_MAX_EC) {
        g_trunc = 1;
        return;
    }
    memcpy(g_dirs[g_ndirs].path, dir, dl);
    g_dirs[g_ndirs].path[dl] = '/';
    memcpy(g_dirs[g_ndirs].path + dl + 1, name, (size_t)nl);
    g_dirs[g_ndirs].path[dl + 1 + (size_t)nl] = 0;
    g_dirs[g_ndirs].depth = (uint8_t)depth;
    g_ndirs++;
}

int ec_browse_step(void)
{
    if (g_state != EC_BR_BUSY)
        return g_state;
    if (g_req.state == EC_FSR_IDLE) {
        if (g_cur >= g_ndirs) {
            g_state = EC_BR_DONE;
            return g_state;
        }
        ec_fs_start(&g_req, FS_LIST, g_dirs[g_cur].path, g_entry, g_list, LIST_BYTES);
    }
    if (ec_fs_update(&g_req) != EC_FSR_DONE)
        return g_state;
    {
        const int32_t r = g_req.result;
        g_req.state = EC_FSR_IDLE;
        if (r < 0) {
            if (g_cur == 0) {                  /* the root itself: no folder, not allowed, missing */
                g_err = r;
                g_state = EC_BR_ERROR;
                return g_state;
            }
            g_cur++;                           /* (a subfolder that went away: skip it) */
            g_entry = 0;
        } else if (r == 0) {
            g_cur++;
            g_entry = 0;
        } else {
            uint32_t at = 0;
            int32_t k;
            char name[256];
            for (k = 0; k < r && at + 2u <= LIST_BYTES; k++) {
                const int is_dir = g_list[at], nl = g_list[at + 1];
                if (at + 2u + (uint32_t)nl > LIST_BYTES)
                    break;
                memcpy(name, g_list + at + 2, (size_t)nl);
                name[nl] = 0;
                if (is_dir)
                    add_dir(g_dirs[g_cur].path, name, nl, g_dirs[g_cur].depth + 1);
                else if (ec_af_is_audio_name(name))
                    add_file(g_dirs[g_cur].path, name, nl);
                at += 2u + (uint32_t)nl;
            }
            g_entry += r;
        }
    }
    return g_state;
}

int ec_browse_state(void) { return g_state; }
int32_t ec_browse_error(void) { return g_err; }
int ec_browse_count(void) { return g_nfiles; }
const char *ec_browse_path(int i) { return i >= 0 && i < g_nfiles ? g_pool + g_file[i] : ""; }
const char *ec_browse_name(int i) { return i >= 0 && i < g_nfiles ? g_pool + g_file[i] + g_name[i] : ""; }
int ec_browse_find(const char *path)
{
    int i;
    for (i = 0; i < g_nfiles; i++)
        if (!strcmp(g_pool + g_file[i], path))
            return i;
    return -1;
}
int ec_browse_truncated(void) { return g_trunc; }
