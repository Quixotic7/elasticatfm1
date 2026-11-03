/* SPDX-License-Identifier: GPL-3.0-only */
/* The sample store (ec_samplestore.h) */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "ec_samplestore.h"

#define ARENA_SAMPLES ((uint32_t)EC_SS_ARENA_MIB * 1024u * 512u)   /* int16 values */

static int16_t g_arena[ARENA_SAMPLES];
static uint32_t g_top;                 /* int16 values in use */
static ec_ss_sample_t g_s[EC_SS_MAX];
static uint8_t g_used[EC_SS_MAX];

void ec_ss_reset(void)
{
    g_top = 0;
    memset(g_used, 0, sizeof g_used);
    memset(g_s, 0, sizeof g_s);
}

int ec_ss_alloc(uint32_t frames)
{
    int h;
    if (!frames || frames > (ARENA_SAMPLES - g_top) / 2u)
        return -1;
    for (h = 0; h < EC_SS_MAX; h++)
        if (!g_used[h])
            break;
    if (h == EC_SS_MAX)
        return -1;
    memset(&g_s[h], 0, sizeof g_s[h]);
    g_s[h].data = g_arena + g_top;
    g_s[h].frames = frames;
    g_top += frames * 2u;
    g_used[h] = 1;
    return h;
}

void ec_ss_free(int h)
{
    if (h < 0 || h >= EC_SS_MAX || !g_used[h])
        return;
    if (g_s[h].data + (size_t)g_s[h].frames * 2u == g_arena + g_top)   /* the newest: its space comes back */
        g_top = (uint32_t)(g_s[h].data - g_arena);
    g_used[h] = 0;
    memset(&g_s[h], 0, sizeof g_s[h]);
}

int ec_ss_shrink(int h, uint32_t frames)
{
    ec_ss_sample_t *s;
    if (h < 0 || h >= EC_SS_MAX || !g_used[h] || !frames || frames > g_s[h].frames)
        return -1;
    s = &g_s[h];
    if (s->data + (size_t)s->frames * 2u == g_arena + g_top)
        g_top = (uint32_t)(s->data - g_arena) + frames * 2u;
    s->frames = frames;
    if (s->frames_ready > frames)
        s->frames_ready = frames;
    return 0;
}

ec_ss_sample_t *ec_ss_get(int h) { return h >= 0 && h < EC_SS_MAX && g_used[h] ? &g_s[h] : NULL; }
const ec_ss_sample_t *ec_ss_ready(int h)
{
    return h >= 0 && h < EC_SS_MAX && g_used[h] && g_s[h].ready ? &g_s[h] : NULL;
}
uint32_t ec_ss_used_bytes(void) { return g_top * 2u; }
uint32_t ec_ss_capacity_bytes(void) { return ARENA_SAMPLES * 2u; }
uint32_t ec_ss_free_frames(void) { return (ARENA_SAMPLES - g_top) / 2u; }

uint32_t ec_ss_live_bytes(void)
{
    uint32_t n = 0;
    int h;
    for (h = 0; h < EC_SS_MAX; h++)
        if (g_used[h])
            n += g_s[h].frames * 4u;
    return n;
}
uint32_t ec_ss_hole_bytes(void) { return ec_ss_used_bytes() - ec_ss_live_bytes(); }
int ec_ss_count(void)
{
    int h, n = 0;
    for (h = 0; h < EC_SS_MAX; h++)
        n += g_used[h];
    return n;
}

uint32_t ec_ss_compact(void)
{
    const uint32_t before = g_top;
    int16_t *dst = g_arena;
    for (;;) {                             /* the live sample lowest in the arena at or above dst, next */
        int h, low = -1;
        for (h = 0; h < EC_SS_MAX; h++)
            if (g_used[h] && g_s[h].data >= dst && (low < 0 || g_s[h].data < g_s[low].data))
                low = h;
        if (low < 0)
            break;
        if (g_s[low].data != dst)
            memmove(dst, g_s[low].data, (size_t)g_s[low].frames * 4u);
        g_s[low].data = dst;
        dst += (size_t)g_s[low].frames * 2u;
    }
    g_top = (uint32_t)(dst - g_arena);
    return (before - g_top) * 2u;
}

/* digits [. digits] at s; returns the length (0: not a number) */
static int number_at(const char *s, float *v)
{
    int i = 0;
    float x = 0, scale = 1;
    while (isdigit((unsigned char)s[i]))
        x = x * 10 + (float)(s[i++] - '0');
    if (!i)
        return 0;
    if (s[i] == '.') {
        i++;
        while (isdigit((unsigned char)s[i])) {
            scale *= 0.1f;
            x += (float)(s[i++] - '0') * scale;
        }
    }
    *v = x;
    return i;
}
static int is_sep(char c) { return c == '_' || c == '-' || c == ' ' || c == '\t'; }
static int is_bpm(const char *s)
{
    return tolower((unsigned char)s[0]) == 'b' && tolower((unsigned char)s[1]) == 'p' &&
           tolower((unsigned char)s[2]) == 'm';
}

float ec_bpm_from_name(const char *name)
{
    size_t i;
    float v;
    /* "bpm" [sep]* number */
    for (i = 0; name[i]; i++)
        if (is_bpm(name + i)) {
            size_t k = i + 3;
            while (is_sep(name[k]))
                k++;
            if (number_at(name + k, &v))
                return v;
        }
    /* number [sep]* "bpm" */
    for (i = 0; name[i]; i++)
        if (isdigit((unsigned char)name[i])) {
            const int n = number_at(name + i, &v);
            size_t k = i + (size_t)n;
            while (is_sep(name[k]))
                k++;
            if (is_bpm(name + k))
                return v;
        }
    return 0;
}
