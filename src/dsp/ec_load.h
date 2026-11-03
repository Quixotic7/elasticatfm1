/*
 * ec_load.h -- 0.6.1: the counted load (ec_load.c): what the engine runs, counted after each ec_dsp_process, and
 * the cost table that weighs it. ec_dsp_load() (ec_dsp.h) is the smoothed reading the FX page's meter shows.
 */
#ifndef EC_LOAD_H
#define EC_LOAD_H

#include "ec_dsp.h"

typedef struct {
    int mode[EC_WARP_COUNT + 1];   /* awake tracks reading, by warp mode */
    int chain;                     /* awake tracks not reading */
    int voices;                    /* sounding slice voices */
    int grains;                    /* sounding grains (GRAIN warp, OLA, the voices' grain clouds) */
    int harm;                      /* harmonizer voices past the first */
    int mods;                      /* awake tracks with a live mod engine */
    int modwarps;                  /* .. of them reading a warp other than TAPE / VARI / CHOP */
    int buses;                     /* send / master slots running */
    int playing;                   /* the transport runs */
    int fx[EC_FX_COUNT + 1];       /* FX slots running, by machine (inserts, sends, master) */
} ec_load_counts_t;

void  ec_dsp_load_counts(const ec_dsp_t *d, ec_load_counts_t *c);   /* now (the calibration tool, tests) */
float ec_load_weigh(const ec_load_counts_t *c);                     /* their load: 1.0 = the 2x budget */

#endif /* EC_LOAD_H */
