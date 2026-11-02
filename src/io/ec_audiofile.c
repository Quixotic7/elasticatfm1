/* SPDX-License-Identifier: GPL-3.0-only */
/* WAV / AIFF parsing and decoding (ec_audiofile.h) */
#include <ctype.h>
#include <math.h>
#include <string.h>

#include "ec_audiofile.h"

static uint32_t le16(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }
static uint32_t le32(const uint8_t *p) { return le16(p) | le16(p + 2) << 16; }
static uint32_t be16(const uint8_t *p) { return (uint32_t)p[0] << 8 | (uint32_t)p[1]; }
static uint32_t be32(const uint8_t *p) { return be16(p) << 16 | be16(p + 2); }

/* the 80-bit IEEE extended float of an AIFF COMM chunk -> Hz */
static uint32_t ext80(const uint8_t *p)
{
    const int e = (int)((be16(p) & 0x7FFFu)) - 16383 - 63;
    const uint64_t m = (uint64_t)be32(p + 2) << 32 | be32(p + 6);
    double v;
    if (!m || (be16(p) & 0x8000u))
        return 0;
    v = ldexp((double)m, e);
    return v < 1 || v > 1e7 ? 0u : (uint32_t)(v + 0.5);
}

static int valid_pcm_bits(unsigned b) { return b == 8 || b == 16 || b == 24 || b == 32; }

/* the format chunk's body (fmt / COMM): fills info; 0 ok, else an EC_AF_* error */
static int wav_fmt(const uint8_t *b, uint32_t size, ec_afinfo_t *in)
{
    unsigned tag;
    if (size < 16)
        return EC_AF_BAD;
    tag = le16(b);
    in->channels = (uint16_t)le16(b + 2);
    in->rate = le32(b + 4);
    in->block_align = (uint16_t)le16(b + 12);
    in->bits = (uint16_t)le16(b + 14);
    if (tag == 0xFFFEu) {                      /* WAVE_FORMAT_EXTENSIBLE: the sub-format GUID's first two bytes */
        if (size < 40)
            return EC_AF_BAD;
        tag = le16(b + 24);
    }
    if (tag == 1 && valid_pcm_bits(in->bits))
        in->codec = in->bits == 8 ? EC_AC_PCM_U8 : EC_AC_PCM_LE;
    else if (tag == 3 && (in->bits == 32 || in->bits == 64))
        in->codec = EC_AC_FLOAT_LE;
    else
        return EC_AF_UNSUPPORTED;
    return 0;
}
static int aiff_comm(const uint8_t *b, uint32_t size, int aifc, ec_afinfo_t *in, uint32_t *nframes)
{
    if (size < 18)
        return EC_AF_BAD;
    in->channels = (uint16_t)be16(b);
    *nframes = be32(b + 2);
    in->bits = (uint16_t)be16(b + 6);
    in->rate = ext80(b + 8);
    in->codec = EC_AC_PCM_BE;
    if (aifc) {
        char c[5];
        if (size < 22)
            return EC_AF_BAD;
        memcpy(c, b + 18, 4);
        c[4] = 0;
        if (!strcmp(c, "NONE") || !strcmp(c, "twos"))
            in->codec = EC_AC_PCM_BE;
        else if (!strcmp(c, "sowt"))
            in->codec = EC_AC_PCM_LE;
        else if (!strcmp(c, "fl32") || !strcmp(c, "FL32")) {
            in->codec = EC_AC_FLOAT_BE;
            in->bits = 32;
        } else if (!strcmp(c, "fl64") || !strcmp(c, "FL64")) {
            in->codec = EC_AC_FLOAT_BE;
            in->bits = 64;
        } else
            return EC_AF_UNSUPPORTED;
    }
    if (in->codec != EC_AC_FLOAT_BE && !valid_pcm_bits(in->bits))
        return EC_AF_UNSUPPORTED;
    return 0;
}

int ec_af_parse(ec_afparse_t *st, const uint8_t *buf, uint32_t n, uint64_t base, uint64_t file_size,
                ec_afinfo_t *info, uint64_t *need_off)
{
    uint32_t aiff_frames = 0xFFFFFFFFu;
    const uint64_t end = base + n;
    if (!st->started) {
        if (base != 0) {
            *need_off = 0;
            return EC_AF_NEED;
        }
        if (n < 12)
            return EC_AF_BAD;
        memset(info, 0, sizeof *info);
        if (!memcmp(buf, "RIFF", 4) && !memcmp(buf + 8, "WAVE", 4))
            info->container = EC_AF_WAV;
        else if (!memcmp(buf, "FORM", 4) && (!memcmp(buf + 8, "AIFF", 4) || !memcmp(buf + 8, "AIFC", 4))) {
            info->container = EC_AF_AIFF;
            st->aifc = !memcmp(buf + 8, "AIFC", 4);
        } else
            return EC_AF_UNSUPPORTED;
        st->next = 12;
        st->started = 1;
    }
    while (!(st->have_fmt && st->have_data)) {
        const uint8_t *p;
        uint32_t size, want = 0;
        uint64_t body;
        int is_fmt, is_data;
        if (st->next + 8 > file_size)
            return EC_AF_BAD;                  /* (no format or no data chunk) */
        if (st->next < base || st->next + 8 > end) {
            *need_off = st->next;
            return EC_AF_NEED;
        }
        p = buf + (st->next - base);
        size = info->container == EC_AF_WAV ? le32(p + 4) : be32(p + 4);
        body = st->next + 8;
        if (info->container == EC_AF_WAV) {
            is_fmt = !memcmp(p, "fmt ", 4);
            is_data = !memcmp(p, "data", 4);
            want = is_fmt ? (size < 64 ? size : 64) : 0;
        } else {
            is_fmt = !memcmp(p, "COMM", 4);
            is_data = !memcmp(p, "SSND", 4);
            want = is_fmt ? (size < 64 ? size : 64) : is_data ? 8 : 0;
        }
        if (want && body + want > end) {
            if (st->next == base && end < file_size)
                return EC_AF_BAD;              /* (a read from the chunk's start came back short) */
            if (body + want > file_size)
                return EC_AF_BAD;
            *need_off = st->next;
            return EC_AF_NEED;
        }
        if (is_fmt) {
            const int r = info->container == EC_AF_WAV ? wav_fmt(p + 8, size, info)
                                                       : aiff_comm(p + 8, size, st->aifc, info, &aiff_frames);
            if (r)
                return r;
            st->have_fmt = 1;
        } else if (is_data) {
            uint64_t len = size;
            if (info->container == EC_AF_AIFF) {
                const uint32_t off = be32(p + 8);
                if (size < 8 + off)
                    return EC_AF_BAD;
                info->data_off = body + 8 + off;
                len = size - 8 - off;
            } else
                info->data_off = body;
            if (info->data_off > file_size)
                return EC_AF_BAD;
            if (info->data_off + len > file_size)   /* (a truncated file, or a streaming writer's size) */
                len = file_size - info->data_off;
            st->data_len = len;
            st->have_data = 1;
        }
        st->next = body + size + (size & 1u);
    }
    if (!info->channels || !info->rate)
        return EC_AF_BAD;
    info->block_align = (uint16_t)(info->channels * (info->bits / 8u));
    {
        uint64_t f = st->data_len / info->block_align;
        if (info->container == EC_AF_AIFF && aiff_frames != 0xFFFFFFFFu && aiff_frames < f)
            f = aiff_frames;
        info->frames = f > 0xFFFFFFFFu ? 0xFFFFFFFFu : (uint32_t)f;
    }
    return EC_AF_OK;
}

/* one sample at p -> int16 */
static int16_t sample(const ec_afinfo_t *in, const uint8_t *p)
{
    int32_t v;
    switch (in->codec) {
    case EC_AC_PCM_U8:
        return (int16_t)(((int)p[0] - 128) * 256);
    case EC_AC_PCM_LE:
        switch (in->bits) {
        case 16: return (int16_t)le16(p);
        case 24: return (int16_t)le16(p + 1);
        case 32: return (int16_t)le16(p + 2);
        default: return 0;
        }
    case EC_AC_PCM_BE:
        switch (in->bits) {
        case 8: return (int16_t)((int8_t)p[0] * 256);
        case 16: return (int16_t)be16(p);
        case 24:
        case 32: return (int16_t)be16(p);
        default: return 0;
        }
    case EC_AC_FLOAT_LE:
    case EC_AC_FLOAT_BE: {
        double f;
        if (in->bits == 32) {
            const uint32_t u = in->codec == EC_AC_FLOAT_LE ? le32(p) : be32(p);
            float x;
            memcpy(&x, &u, 4);
            f = x;
        } else {
            uint64_t u = in->codec == EC_AC_FLOAT_LE ? (uint64_t)le32(p + 4) << 32 | le32(p)
                                                     : (uint64_t)be32(p) << 32 | be32(p + 4);
            memcpy(&f, &u, 8);
        }
        if (!(f == f))
            return 0;                          /* (NaN) */
        v = (int32_t)lrint(f * 32767.0);
        return (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
    }
    }
    return 0;
}

void ec_af_decode(const ec_afinfo_t *in, const uint8_t *src, uint32_t frames, int16_t *dst)
{
    const uint32_t bps = in->bits / 8u, ba = in->block_align;
    const uint32_t r_off = in->channels > 1 ? bps : 0;
    uint32_t i;
    if (in->codec == EC_AC_PCM_LE && in->bits == 16 && in->channels == 2) {   /* the common case, fast */
        for (i = 0; i < frames; i++, src += 4) {
            dst[2 * i] = (int16_t)le16(src);
            dst[2 * i + 1] = (int16_t)le16(src + 2);
        }
        return;
    }
    for (i = 0; i < frames; i++, src += ba) {
        dst[2 * i] = sample(in, src);
        dst[2 * i + 1] = r_off ? sample(in, src + r_off) : dst[2 * i];
    }
}

int ec_af_is_audio_name(const char *name)
{
    static const char *const EXT[] = {".wav", ".wave", ".aif", ".aiff", ".aifc"};
    const size_t n = strlen(name);
    unsigned k;
    for (k = 0; k < sizeof EXT / sizeof EXT[0]; k++) {
        const size_t l = strlen(EXT[k]);
        size_t i;
        int ok = n > l;
        for (i = 0; ok && i < l; i++)
            ok = tolower((unsigned char)name[n - l + i]) == EXT[k][i];
        if (ok)
            return 1;
    }
    return 0;
}
