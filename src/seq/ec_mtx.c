/* SPDX-License-Identifier: GPL-3.0-only */
/* 0.5.0: the mod matrix (ec_mtx.h, docs/MOD-MATRIX.md) */
#include <math.h>
#include <string.h>

#include "ec_mod.h"
#include "ec_mtx.h"

static int iv(const float *v, int id) { return (int)floorf(v[id] + 0.5f); }

int ec_mtx_slot_of(int id, int *col)
{
    if (id < EC_P_MTX1_SRC || id >= EC_P_MTX_END)
        return -1;
    if (col)
        *col = (id - EC_P_MTX1_SRC) % 3;
    return (id - EC_P_MTX1_SRC) / 3;
}

int ec_mtx_src(const float *v, int k)
{
    const int s = iv(v, ec_mtx_src_id(k)) - 1;
    return s < 0 || s >= EC_MS_N ? EC_MS_OFF : s;
}

int ec_mtx_dst(const float *v, int k)
{
    const int d = iv(v, ec_mtx_dst_id(k)) - 1;
    return d >= 0 && d < EC_NP && ec_param_mod_dest(d) ? d : -1;
}

float ec_mtx_depth(const float *v, int k)
{
    const float d = v[ec_mtx_depth_id(k)] / 100.0f;
    return d < -1 ? -1 : d > 1 ? 1 : d;
}

int ec_mtx_live(const float *v, int k)
{
    return ec_mtx_src(v, k) != EC_MS_OFF && ec_mtx_dst(v, k) >= 0 && v[ec_mtx_depth_id(k)] != 0.0f;
}

int ec_mtx_bus(int id)
{
    switch (id) {
    case EC_P_PITCH: return EC_MOD_PITCH;
    case EC_P_FILTER_CUTOFF: return EC_MOD_CUTOFF;
    case EC_P_FILTER_RES: return EC_MOD_RES;
    case EC_P_AMP: return EC_MOD_AMP;
    case EC_P_PAN: return EC_MOD_PAN;
    default: return -1;
    }
}

const char *ec_mtx_src_name(int s)
{
    static const char *const N[EC_MS_N] = {"", "LFO1", "LFO2", "ENV", "VEL", "AT", "MW", "EXPR",
                                           "M1", "M2", "M3", "M4"};
    return s >= 0 && s < EC_MS_N ? N[s] : "";
}

int ec_mtx_level_id(int s)
{
    switch (s) {
    case EC_MS_LFO1: return EC_P_LFO1_DEPTH;
    case EC_MS_LFO2: return EC_P_LFO2_DEPTH;
    case EC_MS_ENV: return EC_P_MENV_DEPTH;
    case EC_MS_VEL: return EC_P_MVEL_DEPTH;
    default: return -1;
    }
}

int ec_mtx_find(const float *v, int src, int dst)
{
    int k;
    for (k = 0; k < EC_MTX_SLOTS; k++)
        if (ec_mtx_src(v, k) == src && ec_mtx_dst(v, k) == dst)
            return k;
    return -1;
}

int ec_mtx_free(const float *v)
{
    int k;
    for (k = 0; k < EC_MTX_SLOTS; k++)
        if (ec_mtx_src(v, k) == EC_MS_OFF)
            return k;
    return -1;
}

int ec_mtx_generic(const float *v)
{
    int k;
    for (k = 0; k < EC_MTX_SLOTS; k++)
        if (ec_mtx_live(v, k) && ec_mtx_bus(ec_mtx_dst(v, k)) < 0)
            return 1;
    return 0;
}

void ec_mtx_sources(const float *v, const float *live, float *src)
{
    int i;
    src[EC_MS_OFF] = 0;
    for (i = 0; i < EC_MS_LIVE; i++)
        src[EC_MS_LFO1 + i] = live ? live[i] : 0.0f;
    for (i = 0; i < 4; i++)
        src[EC_MS_M1 + i] = v[EC_P_MACRO1 + i] / 127.0f;
}

static int is_macro(int id) { return id >= EC_P_MACRO1 && id <= EC_P_MACRO4; }

/* pass 0: the slots aimed at a macro VAL; pass 1: the others (the sources' M1..M4 from the modulated VALs) */
void ec_mtx_apply(float *v, const float *live)
{
    float src[EC_MS_N], add[EC_MTX_SLOTS];
    int pass, k, dst[EC_MTX_SLOTS];
    for (k = 0; k < EC_MTX_SLOTS; k++) {
        dst[k] = ec_mtx_live(v, k) ? ec_mtx_dst(v, k) : -1;
        if (dst[k] >= 0 && ec_mtx_bus(dst[k]) >= 0)
            dst[k] = -1;                          /* (the engine's fast buses) */
        add[k] = 0;
    }
    for (pass = 0; pass < 2; pass++) {
        ec_mtx_sources(v, live, src);
        for (k = 0; k < EC_MTX_SLOTS; k++)        /* (every offset from the same values, then added) */
            if (dst[k] >= 0 && is_macro(dst[k]) == !pass) {
                const ec_param_def_t *d = &EC_PARAMS[dst[k]];
                add[k] = src[ec_mtx_src(v, k)] * ec_mtx_depth(v, k) * (d->max - d->min);
            }
        for (k = 0; k < EC_MTX_SLOTS; k++)
            if (dst[k] >= 0 && is_macro(dst[k]) == !pass && add[k] != 0.0f) {
                const ec_param_def_t *d = &EC_PARAMS[dst[k]];
                const float x = v[dst[k]] + add[k];
                v[dst[k]] = x < d->min ? d->min : x > d->max ? d->max : x;
            }
    }
}

/* ------------------------------------------------------------------------------------------- migration --- */
/* the old dest option (1 OFF, 2 PITCH, 3 CUT, 4 RES, 5 AMP, 6 PAN, 7..10 M1..M4) -> its param id, -1 OFF */
static int old_dest(int o)
{
    static const int D[10] = {-1, EC_P_PITCH, EC_P_FILTER_CUTOFF, EC_P_FILTER_RES, EC_P_AMP, EC_P_PAN,
                              EC_P_MACRO1, EC_P_MACRO2, EC_P_MACRO3, EC_P_MACRO4};
    return o >= 1 && o <= 10 ? D[o - 1] : -1;
}
static const int BUS_ID[EC_MOD_NDEST] = {EC_P_PITCH, EC_P_FILTER_CUTOFF, EC_P_FILTER_RES, EC_P_AMP, EC_P_PAN};

/* the 0.4 engine scaled the LFO / ENV / macro terms on the cutoff bus by 0.3 (velocity at full weight) */
static float old_scale(int src, int dst)
{
    return dst == EC_P_FILTER_CUTOFF && src != EC_MS_VEL ? 30.0f : 100.0f;
}

int ec_mtx_migrate(float *v)
{
    static const int SRC_DEST[4][2] = {{EC_MS_LFO1, EC_P_LFO1_DEST}, {EC_MS_LFO2, EC_P_LFO2_DEST},
                                       {EC_MS_ENV, EC_P_MENV_DEST}, {EC_MS_VEL, EC_P_MVEL_DEST}};
    int n = 0, dropped = 0, i, m, d;
    for (i = 0; i < EC_MTX_SLOTS; i++) {          /* (a clean matrix) */
        v[ec_mtx_src_id(i)] = EC_PARAMS[ec_mtx_src_id(i)].def;
        v[ec_mtx_dst_id(i)] = EC_PARAMS[ec_mtx_dst_id(i)].def;
        v[ec_mtx_depth_id(i)] = EC_PARAMS[ec_mtx_depth_id(i)].def;
    }
    for (i = 0; i < 4; i++) {
        const int dst = old_dest(iv(v, SRC_DEST[i][1]));
        v[SRC_DEST[i][1]] = EC_PARAMS[SRC_DEST[i][1]].def;
        if (dst < 0)
            continue;
        if (n >= EC_MTX_SLOTS) {
            dropped++;
            continue;
        }
        v[ec_mtx_src_id(n)] = (float)(SRC_DEST[i][0] + 1);
        v[ec_mtx_dst_id(n)] = (float)(dst + 1);
        v[ec_mtx_depth_id(n)] = old_scale(SRC_DEST[i][0], dst);
        n++;
    }
    for (m = 0; m < 4; m++)
        for (d = 0; d < EC_MOD_NDEST; d++) {
            const int id = EC_P_MACRO_DEPTH + m * 5 + d;
            const float b = (v[id] - 64.0f) / 64.0f;
            v[id] = EC_PARAMS[id].def;
            if (b == 0.0f)
                continue;
            if (n >= EC_MTX_SLOTS) {
                dropped++;
                continue;
            }
            v[ec_mtx_src_id(n)] = (float)(EC_MS_M1 + m + 1);
            v[ec_mtx_dst_id(n)] = (float)(BUS_ID[d] + 1);
            v[ec_mtx_depth_id(n)] = b * old_scale(EC_MS_M1 + m, BUS_ID[d]);
            n++;
        }
    return dropped;
}

int ec_mtx_migrated_slot(const float *v, int old_id, float old_value, float *depth)
{
    const int k = old_id - EC_P_MACRO_DEPTH, m = k / 5, d = k % 5;
    int s;
    if (k < 0 || k >= 20)
        return -1;
    s = ec_mtx_find(v, EC_MS_M1 + m, BUS_ID[d]);
    if (s >= 0 && depth)
        *depth = (old_value - 64.0f) / 64.0f * old_scale(EC_MS_M1 + m, BUS_ID[d]);
    return s;
}
