/* SPDX-License-Identifier: GPL-3.0-only */
/* The sample browser's scan: a folder (/audio) listed recursively through the async file API, one request at a time,
 * driven by ec_browse_step() once per tick. Keeps the WAV / AIFF files it finds (paths in a fixed pool): the root's
 * files first, then each subfolder's, breadth first, every folder sorted by name as the host lists it. */
#ifndef EC_BROWSE_H
#define EC_BROWSE_H
#include <stdint.h>

#define EC_BROWSE_MAX 1024          /* files kept */
#define EC_BROWSE_DIRS 128          /* folders visited at most */
#define EC_BROWSE_DEPTH 8           /* nesting below the root */

enum { EC_BR_IDLE, EC_BR_BUSY, EC_BR_DONE, EC_BR_ERROR };

void ec_browse_start(const char *root);   /* forget the list, scan again from root ("/audio") */
int ec_browse_step(void);                 /* advance the scan (one request at a time); returns EC_BR_* */
int ec_browse_state(void);
int32_t ec_browse_error(void);            /* the FS_E* error that stopped the scan (EC_BR_ERROR) */
int ec_browse_count(void);
const char *ec_browse_path(int i);        /* "/audio/breaks/amen_bpm136.wav" */
const char *ec_browse_name(int i);        /* "amen_bpm136.wav" */
int ec_browse_find(const char *path);     /* index of a path, -1 */
int ec_browse_truncated(void);            /* 1: more files than EC_BROWSE_MAX, or folders not visited */

#endif
