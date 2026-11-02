/* SPDX-License-Identifier: GPL-3.0-only */
/* Loading one WAV / AIFF file into the sample store, asynchronously: FS_STAT, the header read in 64 KiB pieces until
 * the format and data chunks are found, then the data in reads of at most 1 MiB, each decoded over several ticks
 * (EC_LOAD_FRAMES_PER_TICK at most per tick, so the core's tick stays short and audio never waits). One load at a
 * time; the sample turns ready (ec_ss_ready) only when every frame is in. */
#ifndef EC_LOADER_H
#define EC_LOADER_H
#include <stdint.h>

#ifndef EC_LOAD_FRAMES_PER_TICK
#define EC_LOAD_FRAMES_PER_TICK 4096
#endif

enum { EC_LD_IDLE, EC_LD_STAT, EC_LD_HEADER, EC_LD_DATA, EC_LD_DONE, EC_LD_ERROR };

int ec_load_start(const char *path);     /* begins (cancelling a load in progress); 0 ok */
int ec_load_step(void);                  /* once per tick; returns EC_LD_* */
void ec_load_cancel(void);               /* stops; a partly loaded sample is freed */
int ec_load_state(void);
int ec_load_handle(void);                /* the store handle being / last loaded (-1 none) */
uint32_t ec_load_permille(void);         /* progress 0..1000 */
const char *ec_load_error(void);         /* why EC_LD_ERROR ("not found", "unsupported", "too big", ..) */
const char *ec_load_path(void);

#endif
