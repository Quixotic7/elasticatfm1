/*
 * ec_fft.h -- a small fixed-size real FFT (E3, the spectral warp modes).
 * N = 2048 real points (SC's FFT(LocalBuf(2048)) of \elasticatSpectral), computed
 * as one N/2 complex radix-2 FFT plus the real split. No allocation, no libs:
 * the tables live in ec_rfft_t (about 14 KB), filled once by ec_rfft_init().
 *
 * Packed spectrum layout (in place, N floats):
 *   x[0] = DC (real), x[1] = Nyquist (real), x[2k], x[2k+1] = Re, Im of bin k (1 <= k < N/2).
 * ec_rfft_inv(ec_rfft_fwd(x)) = x * (N / 2): scale by 2/N to get x back.
 */
#ifndef EC_FFT_H
#define EC_FFT_H

#include <stdint.h>

#define EC_FFT_N    2048
#define EC_FFT_HALF (EC_FFT_N / 2)

typedef struct {
    float tw_re[EC_FFT_HALF / 2], tw_im[EC_FFT_HALF / 2];   /* complex FFT twiddles e^-2pi i k/(N/2) */
    float sp_re[EC_FFT_HALF], sp_im[EC_FFT_HALF];           /* real split e^-2pi i k/N */
    uint16_t rev[EC_FFT_HALF];                              /* bit reversal of the N/2 FFT */
    int ready;
} ec_rfft_t;

void ec_rfft_init(ec_rfft_t *f);
void ec_rfft_fwd(const ec_rfft_t *f, float *x);   /* N reals -> packed spectrum */
void ec_rfft_inv(const ec_rfft_t *f, float *x);   /* packed spectrum -> N reals x (N/2) */

#endif
