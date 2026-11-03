/* SPDX-License-Identifier: GPL-3.0-only */
/* The native stand-in for the host's file API (rfm1_fs_*), for host/ec_run and the tests: serves a real RemoteFM1
 * folder (/audio -> <root>/audio, /code -> <root>/code/<id>, /data -> <root>/data/<id>) and completes each request
 * one tick late, as rfm1_run --folder does, so the polling is exercised. Compiled only with EC_FS_NATIVE (this
 * repo's native build); inside FM1VST the host provides rfm1_fs_* (imports in an .rfm1, stubs natively). */
#ifndef EC_FS_NATIVE_H
#define EC_FS_NATIVE_H
#include <stdint.h>

void ec_fs_native_root(const char *root, const char *cart_id);   /* NULL: no folder (FS_ENOFOLDER) */
void ec_fs_native_pump(void);          /* between two ticks: complete what was requested before */
void ec_fs_native_touch(void);         /* bump the folder-change counter */
uint32_t ec_fs_native_served(void);    /* requests completed so far */
/* 0.6.1, tests: the op's request number `after` from now (0 = the next one) fails with err (FS_ENOSPC, FS_EIO ..),
 * once, without touching the disk. op 0 turns it off */
void ec_fs_native_fail(int32_t op, int after, int32_t err);

#endif
