/* SPDX-License-Identifier: GPL-3.0-only */
/* WAV and AIFF / AIFC: header parsing (incremental, over bytes read through the async file API) and decoding of raw
 * frames into int16 stereo. WAV: PCM 8 (unsigned) / 16 / 24 / 32-bit int, 32 / 64-bit float, WAVE_FORMAT_EXTENSIBLE.
 * AIFF: PCM 8 / 16 / 24 / 32-bit big-endian; AIFC: NONE / twos, sowt (little-endian), fl32 / fl64. Any channel count
 * (mono is doubled, more than two: the first two), any sample rate (kept: the reader plays at rate / 44100). */
#ifndef EC_AUDIOFILE_H
#define EC_AUDIOFILE_H
#include <stdint.h>

enum { EC_AF_WAV = 1, EC_AF_AIFF = 2 };
enum { EC_AC_PCM_LE = 1, EC_AC_PCM_BE, EC_AC_PCM_U8, EC_AC_FLOAT_LE, EC_AC_FLOAT_BE };
enum { EC_AF_OK = 0, EC_AF_NEED = 1, EC_AF_BAD = -1, EC_AF_UNSUPPORTED = -2 };

typedef struct {
    uint8_t container;       /* EC_AF_WAV / EC_AF_AIFF */
    uint8_t codec;           /* EC_AC_* */
    uint16_t channels;
    uint16_t bits;           /* bits per sample (8, 16, 24, 32, 64) */
    uint16_t block_align;    /* bytes per frame */
    uint32_t rate;
    uint64_t data_off;       /* the first frame's byte offset in the file */
    uint32_t frames;
} ec_afinfo_t;

/* the parser's walk through the chunks (zero it before the first call) */
typedef struct {
    uint64_t next;           /* the next chunk's offset (0: the file header) */
    uint8_t have_fmt, have_data, started, aifc;
    uint64_t data_len;       /* the data chunk's byte length (clamped to the file) */
} ec_afparse_t;

/* Feed bytes buf[0..n) that start at file offset `base` (the file is file_size bytes). Returns EC_AF_OK (info
 * complete), EC_AF_NEED (read again from *need_off, at least the next ~64 KiB), EC_AF_BAD or EC_AF_UNSUPPORTED. */
int ec_af_parse(ec_afparse_t *st, const uint8_t *buf, uint32_t n, uint64_t base, uint64_t file_size,
                ec_afinfo_t *info, uint64_t *need_off);

/* decode `frames` frames of raw data (frames * block_align bytes) into dst (frames * 2 int16, L R) */
void ec_af_decode(const ec_afinfo_t *in, const uint8_t *src, uint32_t frames, int16_t *dst);

/* 1 if the name ends in .wav .wave .aif .aiff .aifc (any case) */
int ec_af_is_audio_name(const char *name);

#endif
