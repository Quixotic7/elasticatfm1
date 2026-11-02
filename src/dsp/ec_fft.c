/*
 * ec_fft.c -- fixed-size real FFT (ec_fft.h): an N/2-point complex radix-2 FFT
 * on the interleaved real data (z[n] = x[2n] + i x[2n+1]) and the even/odd split.
 */
#include <math.h>
#include "ec_fft.h"

#define M EC_FFT_HALF   /* complex points */

void ec_rfft_init(ec_rfft_t *f)
{
    int bits = 0, i;
    while ((1 << bits) < M) ++bits;
    for (i = 0; i < M; ++i) {
        int r = 0, b;
        for (b = 0; b < bits; ++b)
            if (i & (1 << b)) r |= 1 << (bits - 1 - b);
        f->rev[i] = (uint16_t)r;
    }
    for (i = 0; i < M / 2; ++i) {
        double a = -2.0 * 3.14159265358979323846 * (double)i / (double)M;
        f->tw_re[i] = (float)cos(a);
        f->tw_im[i] = (float)sin(a);
    }
    for (i = 0; i < M; ++i) {
        double a = -2.0 * 3.14159265358979323846 * (double)i / (double)EC_FFT_N;
        f->sp_re[i] = (float)cos(a);
        f->sp_im[i] = (float)sin(a);
    }
    f->ready = 1;
}

/* in-place complex FFT of M points (interleaved re, im); inverse = conjugated */
static void cfft(const ec_rfft_t *f, float *x, int inverse)
{
    int i, s;
    for (i = 0; i < M; ++i) {
        int j = f->rev[i];
        if (j > i) {
            float tr = x[2 * i], ti = x[2 * i + 1];
            x[2 * i] = x[2 * j]; x[2 * i + 1] = x[2 * j + 1];
            x[2 * j] = tr; x[2 * j + 1] = ti;
        }
    }
    for (s = 2; s <= M; s <<= 1) {
        int half = s >> 1, step = M / s, j, k;
        for (j = 0; j < M; j += s) {
            for (k = 0; k < half; ++k) {
                float wr = f->tw_re[k * step], wi = inverse ? -f->tw_im[k * step] : f->tw_im[k * step];
                float *a = x + 2 * (j + k), *b = x + 2 * (j + k + half);
                float tr = wr * b[0] - wi * b[1];
                float ti = wr * b[1] + wi * b[0];
                b[0] = a[0] - tr; b[1] = a[1] - ti;
                a[0] += tr; a[1] += ti;
            }
        }
    }
}

void ec_rfft_fwd(const ec_rfft_t *f, float *x)
{
    int k;
    cfft(f, x, 0);
    {
        float z0r = x[0], z0i = x[1];
        x[0] = z0r + z0i;          /* DC */
        x[1] = z0r - z0i;          /* Nyquist */
    }
    for (k = 1; k <= M / 2; ++k) {
        int m = M - k;
        float ar = x[2 * k], ai = x[2 * k + 1];          /* Z[k] */
        float br = x[2 * m], bi = -x[2 * m + 1];         /* conj(Z[M-k]) */
        float er = 0.5f * (ar + br), ei = 0.5f * (ai + bi);
        /* O = -i (a - b) / 2 */
        float or_ = 0.5f * (ai - bi), oi = -0.5f * (ar - br);
        float wr = f->sp_re[k], wi = f->sp_im[k];
        float tr = wr * or_ - wi * oi, ti = wr * oi + wi * or_;
        x[2 * k] = er + tr;
        x[2 * k + 1] = ei + ti;
        if (m != k) {              /* X[M-k] = conj(E - wO) */
            x[2 * m] = er - tr;
            x[2 * m + 1] = -(ei - ti);
        }
    }
}

void ec_rfft_inv(const ec_rfft_t *f, float *x)
{
    int k;
    {
        float x0 = x[0], xm = x[1];
        float e = 0.5f * (x0 + xm), o = 0.5f * (x0 - xm);
        x[0] = e;                  /* Z0 = E0 + i O0 */
        x[1] = o;
    }
    for (k = 1; k <= M / 2; ++k) {
        int m = M - k;
        float ar = x[2 * k], ai = x[2 * k + 1];          /* X[k] */
        float br = x[2 * m], bi = -x[2 * m + 1];         /* conj(X[M-k]) */
        float er = 0.5f * (ar + br), ei = 0.5f * (ai + bi);
        float dr = 0.5f * (ar - br), di = 0.5f * (ai - bi);   /* w O */
        float wr = f->sp_re[k], wi = -f->sp_im[k];       /* conj(w) */
        float or_ = wr * dr - wi * di, oi = wr * di + wi * dr;
        /* Z[k] = E + i O ; Z[M-k] = conj(E) + i conj(O) */
        x[2 * k] = er - oi;
        x[2 * k + 1] = ei + or_;
        if (m != k) {
            x[2 * m] = er + oi;
            x[2 * m + 1] = -ei + or_;
        }
    }
    cfft(f, x, 1);
}
