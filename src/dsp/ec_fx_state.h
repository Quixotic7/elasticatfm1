/*
 * ec_fx_state.h -- the FX slot state (private to ec_fx.c; included by ec_fx.h
 * only so the engine can embed ec_fx_t by value). Nothing here is API.
 */
#ifndef EC_FX_STATE_H
#define EC_FX_STATE_H

#define EC_FX_KR 16                      /* control period (frames) inside a call */

/* circular line in the slot memory: 3 guard floats after len mirror buf[0..2]
 * so a cubic read never wraps; filled = samples written since the (re)start
 * (older ages read as 0, like SC's "checked" delay helpers -> no clearing). */
typedef struct { float *buf; int32_t len, w, filled; } ec_fxline_t;
typedef struct { float a0, b1, b2; } ec_fxbq_t;            /* SC LPF / HPF / BPF */
typedef struct { float y1, y2; } ec_fxbqs_t;
typedef struct { float a, b, c, ph; } ec_fxn2_t;           /* LFNoise2 */
typedef struct { float a, b, ph; } ec_fxn1_t;              /* LFNoise1 */
typedef struct {                                           /* SC Limiter */
    float *in, *mid, *out;
    int32_t size, pos, flips;
    float level, slope, tgt, prevmax, curmax;
} ec_fxlim_t;
typedef struct { float *buf; int32_t len, idx; float r, f; } ec_fxfv_t;  /* FreeVerb line */
typedef struct { uint32_t dice[16]; uint32_t total, counter; } ec_fxpink_t;

typedef struct {                       /* DELAY */
    ec_fxline_t ln[2]; ec_fxbq_t lp; ec_fxbqs_t lps[2];
    float fb[2], fbg, t, ds; int init;
} ec_fxm_delay_t;

typedef struct {                       /* REVERB (FreeVerb2) */
    ec_fxfv_t v[24]; int32_t n;
    float fb, d1, d2;
} ec_fxm_reverb_t;

typedef struct {                       /* LOFI */
    float held[2], ph, inc, trig, step, inv;
} ec_fxm_lofi_t;

typedef struct {                       /* COMP */
    float thresh, slope, makeup, cc, rc, mc;
    float prevmax, gain, amp_in, amp_out;
} ec_fxm_comp_t;

typedef struct {                       /* DESTROY */
    ec_fxline_t ln[2]; ec_fxn2_t n2[2]; ec_fxbq_t lp; ec_fxbqs_t lps[2];
    float ds[2]; int init;
    float warb, nrate, driv, dk, dofs, dga, dgb, fk, fga, fgb, sk, sga, sgb;
    float cstep, cinv, cga, cgb, sinc, sga2, sgb2, lvl;
    float held[2], ph, trig;
} ec_fxm_destroy_t;

typedef struct {                       /* ECHO */
    ec_fxline_t ln[2]; ec_fxbq_t lp; ec_fxbqs_t lps[2]; ec_fxn2_t n2[2];
    float fb[2], fbg, lt, off, offt, ds[2], ph[2], wob; int mode, init;
} ec_fxm_echo_t;

typedef struct {                       /* BLACKHOLE */
    ec_fxline_t pre[2], ap[8]; ec_fxn2_t n2[8]; ec_fxlim_t lim[2];
    ec_fxbq_t lp, hp; ec_fxbqs_t lps[2], hps[2];
    float ds[8], loop[2], dcx[2], dcy[2];
    float size, fbg, mod, decay; int32_t pres; int init;
} ec_fxm_bh_t;

typedef struct {                       /* CHORUS / FLANGER */
    ec_fxline_t ln[2]; ec_fxbq_t lp; ec_fxbqs_t lps[2];
    float ph, rate, depth, wofs, fbamt, fb[2], ds[2]; int init;
} ec_fxm_mod_t;

typedef struct {                       /* PHASER */
    float x1[2][8], y1[2][8], fb[2];
    float ph, rate, lo, hi, fbamt, c; int sel, nst;
} ec_fxm_phaser_t;

typedef struct {                       /* DJ EQ */
    ec_fxbq_t lp, hp; ec_fxbqs_t lps[2], hps[2];
    float gl, gm, gh;
} ec_fxm_eq_t;

typedef struct {                       /* DUCK */
    float amt, sens, cc, rc, amp, gain; int init;
} ec_fxm_duck_t;

typedef struct {                       /* TAPE ECHO */
    ec_fxline_t ln[2]; ec_fxbq_t lp, hp; ec_fxbqs_t lps[2], hps[2]; ec_fxn2_t n2;
    float fb[2], fbg, t, ds, ph, wow, flut, k1, k2, ga, gb; int init;
} ec_fxm_tape_t;

typedef struct {                       /* CASSETTE */
    ec_fxline_t ln[2]; ec_fxn2_t n2; ec_fxn1_t n1;
    ec_fxbq_t lp, hp60, hp800, bp; ec_fxbqs_t lps[2], h60s[2], h800s[2], bps[2];
    ec_fxpink_t pink[2];
    float ds, dm, drop, ph, wow, flut, noise, dens, dec[2]; int init;
} ec_fxm_cass_t;

typedef struct {                       /* MOTION */
    float ph, u, sg, trem, pan; int shape, init;
} ec_fxm_motion_t;

typedef struct {                       /* RINGS */
    float hy[2][12], fb[2], oc, os, cr, sr, fbamt; int mode;
    ec_fxbq_t lp; ec_fxbqs_t lps[2];
} ec_fxm_rings_t;

typedef struct {                       /* LIMIT */
    ec_fxlim_t lim[2]; float g, ceil, amp_in, amp_out, mc;
} ec_fxm_limit_t;

typedef union {
    ec_fxm_delay_t dl; ec_fxm_reverb_t rv; ec_fxm_lofi_t lf; ec_fxm_comp_t cp;
    ec_fxm_destroy_t ds; ec_fxm_echo_t ec; ec_fxm_bh_t bh; ec_fxm_mod_t md;
    ec_fxm_phaser_t ph; ec_fxm_eq_t eq; ec_fxm_duck_t dk; ec_fxm_tape_t tp;
    ec_fxm_cass_t cs; ec_fxm_motion_t mo; ec_fxm_rings_t rg; ec_fxm_limit_t lm;
} ec_fxm_t;

typedef struct ec_fx {
    float   *mem;
    float    fs;
    int      is_send;
    uint32_t seed, rng;
    int      running;          /* machine audible now */
    int      want;             /* latest requested machine */
    int      fdir;             /* 0 steady, -1 fading out, +1 fading in */
    float    g, gstep;         /* swap fade gain, per-sample step (10 ms) */
    int32_t  prime;            /* samples before the fade-in starts (lookahead) */
    int      restart_req;      /* respawn the running machine (LIMIT REL) */
    int      lim_size;         /* LIMIT lookahead the next start uses */
    int      asleep;
    int32_t  quiet;            /* silent samples in a row (sleep detector) */
    float    gd, gw, gd_t, gw_t;   /* fxMixBlend gains (current, target) */
    int      dirty;
    ec_fx_params_t pc;         /* params the derived values were computed from */
    /* constants (init) */
    int32_t  big_len, ap_len, pre_len, mod_len;
    float    lag15[EC_FX_KR + 1];  /* Lag 0.15 s coefficient per chunk length */
    float    hcoef[12];            /* SC Hilbert / FreqShift allpass coefficients */
    ec_fxbq_t bp2500;              /* cassette crackle BPF */
    float    dec002, duck_lag;     /* Decay 2 ms, Lag 5 ms (per sample) */
    ec_fx_meter_t meter;
    ec_fxm_t u;                    /* the running machine's state */
} ec_fx_t;

#endif
