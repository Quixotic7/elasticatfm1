/* SPDX-License-Identifier: GPL-3.0-only */
/* The CAT stage (ec_ui_kit.h, docs/UX-V2.md §10 "CAT stage" and "CAT fractals", §11 "CAT stage, 0.6.3"): the cat over
 * a greyscale fractal that never settles into a loop, a different algorithm each time the CAT opens.
 *
 * The stage: the theme's grey ramp only (BG, L3, L5, L8, L12, L15), 60 x 60 cells of 2x2 units, never on a cell that
 * touches the cat. A cell's grey is its count (escape or convergence) on a triangle over the ramp, BG L3 L5 L8 L12 L15
 * L12 L8 L5 L3: smooth contour bands; points that never escape are BG, so the cat stands in the dark.
 *
 * The algorithms (ec_scr_cat_algo_name): JULIA (z^2 + c), MANDEL (zooms into the Mandelbrot set's edge), SHIP
 * (Burning Ship zooms into its antenna), TRICORN (Mandelbar zooms into its tips), JULIA4 (z^4 + c), PHOENIX
 * (z^2 + p + q z', p and q wandering), NEWTON (z^3 - 1 with a wandering relaxation, by root and steps). Everything is
 * a function of t (the evolution clock: the app counts it from the CAT's redraws when stopped and from the transport's
 * beats when playing; never the host clock) and the entry's seed:
 *  - the wandering values (zoom, rotation, centre, constants) are sums of three slow sines at unrelated (irrational-
 *    ratio) rates, their phases from the seed, most with a slow steady drift on top: no visible loop;
 *  - the Julia kinds' c glides from one constant to the next (JULIA: a list of well-loved ones; JULIA4: anywhere on its
 *    Multibrot's edge), a new random one every JSEG;
 *  - the zooms: t falls into epochs of ZEPOCH, each cut into ZCYC cycles of random lengths; each cycle dives from the
 *    overview to a target (a search in doubles, once a cycle, cached: from one of the set's detailed regions, level by
 *    level into the third of the view with the most detail) to a random depth (up to 2^12.5, well inside Q28's
 *    precision), the target moving out beside the cat, then pulls back out (the overview is the same view whatever
 *    the target, so the cycles join seamlessly). Deeper, the cap rises and the bands widen.
 * A frame whose greys collapse (one class over CAT_FLAT of the cells, or BG over CAT_DARK: mostly inside) pulls the
 * view back out and eases in again once it has detail.
 * Fixed point (Q28 in 64 bits; NEWTON Q20), low iteration caps; the z -> -z symmetric kinds (JULIA, JULIA4: centred
 * on 0) compute half the cells (the mirror), the others half a frame (a checkerboard, the rest from the last frame). */
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "ec_spr_cat.h"
#include "ec_ui_kit.h"

#define W EC_GW
#define H EC_GH
#define R(c, x, y, w, h) ec_gfx_rect((x), (y), (w), (h), (c))

#define CAT_KEY EC_C_SHADE                         /* the sprite's ground (sprite_to_c.py): not drawn */
#define CAT_N 60                                   /* cells a side */
#define CAT_FLAT 0.86                              /* one class over this share of the cells: collapsed, */
#define CAT_DARK 0.6                               /*   or BG (the inside) over this */
#define ZEPOCH 420.0                               /* the zooms' epoch (t units: stopped seconds) */
#define ZCYC 6                                     /* cycles an epoch */
#define FQ 28                                      /* fixed point */
#define NQ 20                                      /* NEWTON's */
typedef int64_t fx;
#define F(v) ((fx)((v) * (double)((fx)1 << FQ)))
#define MUL(a, b) (((a) * (b)) >> FQ)
#define TWO ((fx)2 << FQ)
#define FOUR ((fx)4 << FQ)

enum { A_JULIA, A_MANDEL, A_SHIP, A_TRICORN, A_JULIA4, A_PHOENIX, A_NEWTON, A_N };
static const char *const NAMES[A_N] = {"JULIA", "MANDEL", "SHIP", "TRICORN", "JULIA4", "PHOENIX", "NEWTON"};
static const uint8_t BAND[10] = {EC_C_BG, EC_C_L3, EC_C_L5, EC_C_L8, EC_C_L12,
                                 EC_C_L15, EC_C_L12, EC_C_L8, EC_C_L5, EC_C_L3};
static const double PI2 = 6.283185307179586;

int ec_scr_cat_frames(void) { return EC_SPR_CAT_FRAMES; }
int ec_scr_cat_algos(void) { return A_N; }
const char *ec_scr_cat_algo_name(int algo) { return algo >= 0 && algo < A_N ? NAMES[algo] : "?"; }

/* a small generator (xorshift32): the app keeps its state; never 0 */
static uint32_t xs(uint32_t *s)
{
    uint32_t x = *s ? *s : 0x9E3779B9u;
    x ^= x << 13, x ^= x >> 17, x ^= x << 5;
    return *s = x;
}
int ec_scr_cat_next(uint32_t *rng, int prev)
{
    const uint32_t r = xs(rng);
    if (prev < 0 || prev >= A_N)
        return (int)(r % A_N);
    return (prev + 1 + (int)(r % (A_N - 1))) % A_N;   /* never prev */
}

static uint32_t hash3(uint32_t a, uint32_t b, uint32_t c)   /* a mix (for the seed's phases and the cycles) */
{
    uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u ^ (c + 0x165667B1u) * 0xC2B2AE3Du;
    h ^= h >> 15, h *= 0x2C1B3C6Du, h ^= h >> 12, h *= 0x297A2D39u, h ^= h >> 15;
    return h;
}
static double u01(uint32_t h) { return (double)(h >> 8) / 16777216.0; }

/* a slow wandering value in [-1, 1]: three sines at unrelated rates (base x 1, sqrt 2, golden), the seed's phases */
static double wander(double t, double rate, uint32_t seed, int k)
{
    const double p0 = PI2 * u01(hash3(seed, (uint32_t)k, 1)), p1 = PI2 * u01(hash3(seed, (uint32_t)k, 2)),
                 p2 = PI2 * u01(hash3(seed, (uint32_t)k, 3));
    return 0.5 * sin(rate * t + p0) + 0.3 * sin(rate * 1.41421356 * t + p1) + 0.2 * sin(rate * 0.61803399 * t + p2);
}

/* ------------------------------------------------------------------------------------- the zoom targets --- */
/* the escape count in doubles (the target search): cap if still bounded */
static int esc_d(int algo, double cr, double ci, int cap)
{
    double x = 0.0, y = 0.0;
    int n;
    for (n = 0; n < cap; n++) {
        const double xx = x * x, yy = y * y;
        if (xx + yy > 4.0)
            return n;
        if (algo == A_SHIP)
            y = 2.0 * fabs(x * y) + ci;
        else if (algo == A_TRICORN)
            y = -2.0 * x * y + ci;
        else
            y = 2.0 * x * y + ci;
        x = xx - yy + cr;
    }
    return cap;
}

typedef struct {
    double ox, oy, hw;             /* the overview: centre, half width */
    double maxd;                   /* the deepest dive (log2) */
    int nstart;                    /* where the dives start (the set's detailed parts; MANDEL: the overview) */
    double start[6][3];            /*   centre, half width */
} zoomset_t;
static const zoomset_t ZS[A_N] = {
    [A_MANDEL] = {-0.6, 0.0, 1.45, 12.5, 6, {{-0.75, 0.12, 0.25}, {0.3, 0.03, 0.2}, {-0.12, 0.85, 0.25},   /* valleys, */
                                              {-1.55, 0.0, 0.3}, {-0.6, 0.0, 1.45}, {-0.15, -0.9, 0.25}}},  /* bulbs */
    [A_SHIP] = {-0.5, -0.45, 1.45, 11.0, 2, {{-1.72, -0.03, 0.2}, {-1.62, -0.01, 0.12}}},   /* the antenna's ships */
    [A_TRICORN] = {-0.25, 0.0, 1.5, 8.0, 3, {{-1.45, 0.0, 0.4}, {0.22, 0.85, 0.32}, {0.22, -0.85, 0.32}}},   /* tips */
};

static int zcap(double zoom) { const int c = 16 + (int)(4.0 * zoom); return c > 64 ? 64 : c; }
static int bjump(int a, int b)                   /* 0..9 bands (circular), 10 inside */
{
    int d;
    if (a == 10 || b == 10)
        return a != b;
    d = a > b ? a - b : b - a;
    return d > 1 && d < 9;
}
static int zbw(int cap) { return 1 + (cap - 16) / 20; }   /* the counts a band spans: deep zooms, wider bands */

/* the dive: from the overview, level by level, into the third of the view with the most detail (a 12 x 12 probe grid;
 * windows of 4 x 4 probes at a stride of 2: the most distinct bands, not mostly inside, a random tie break), down to
 * maxd (log2) or until no window has detail. The target: the last window's centre; its depth: the levels reached */
static double find_target(int algo, uint32_t h, double maxd, double *tx, double *ty)
{
    const zoomset_t *z = &ZS[algo];
    const double *st = z->start[h % (uint32_t)z->nstart];
    double cx = st[0], cy = st[1], hw = st[2], zoom = log2(z->hw / st[2]);
    uint32_t s = h | 1u;
    while (zoom + 1.58 <= maxd) {
        const int cap = zcap(zoom + 1.58), bw = zbw(cap);
        int b[12][12], i, j, wi, wj, bi = -1, bj = -1;
        double best = 4.5;
        for (j = 0; j < 12; j++)
            for (i = 0; i < 12; i++) {
                const int n = esc_d(algo, cx + hw * ((2 * i - 11) / 12.0), cy + hw * ((2 * j - 11) / 12.0), cap);
                b[j][i] = n >= cap ? 10 : n / (bw + (algo == A_SHIP)) % 10;
            }
        for (wj = 0; wj <= 8; wj += 2)
            for (wi = 0; wi <= 8; wi += 2) {
                unsigned seen = 0;
                int in = 0, k = 0, jumps = 0, ii, jj;
                double sc;
                for (jj = 0; jj < 4; jj++)
                    for (ii = 0; ii < 4; ii++) {
                        const int c = b[wj + jj][wi + ii];
                        in += c == 10, seen |= 1u << c;
                        if (ii < 3)                /* a jump: neighbours more than one band apart (not a gradient) */
                            jumps += bjump(c, b[wj + jj][wi + ii + 1]);
                        if (jj < 3)
                            jumps += bjump(c, b[wj + jj + 1][wi + ii]);
                    }
                for (ii = 0; ii < 11; ii++)
                    k += seen >> ii & 1u;
                sc = k + 0.6 * (jumps < 8 ? jumps : 16 - jumps) + u01(xs(&s)) * 3.5 + (in >= 1 && in <= 8 ? 2.0 : 0.0) -
                     (in > 10 ? 99.0 : 0.0);
                if (sc > best)
                    best = sc, bi = wi, bj = wj;
            }
        if (bi < 0)
            break;
        cx += hw * ((2 * bi + 3 - 11) / 12.0), cy += hw * ((2 * bj + 3 - 11) / 12.0);
        hw /= 3.0, zoom += 1.585;
    }
    *tx = cx, *ty = cy;
    return zoom;
}

/* the zoom at t: the cycle's target, its depth (log2), where in the dive */
typedef struct {
    uint32_t seed;
    int algo, epoch, cyc;
    double tx, ty, depth, t0, len;
} zcache_t;
static zcache_t g_zc = {0, -1, -1, -1, 0, 0, 0, 0, 0};

static double zoom_at(int algo, uint32_t seed, double t, double *tx, double *ty)
{
    const int e = (int)floor(t / ZEPOCH);
    const double te = t - e * ZEPOCH;
    double w[ZCYC], sum = 0.0, at = 0.0, p, s;
    int i;
    for (i = 0; i < ZCYC; i++)                     /* the cycles' lengths: random shares of the epoch */
        sum += w[i] = 0.6 + u01(hash3(seed, (uint32_t)e, 100u + (uint32_t)i));
    for (i = 0; i < ZCYC - 1; i++) {
        const double len = ZEPOCH * w[i] / sum;
        if (te < at + len)
            break;
        at += len;
    }
    if (g_zc.algo != algo || g_zc.seed != seed || g_zc.epoch != e || g_zc.cyc != i) {
        const uint32_t h = hash3(seed, (uint32_t)e, 200u + (uint32_t)i);
        g_zc.algo = algo, g_zc.seed = seed, g_zc.epoch = e, g_zc.cyc = i;
        g_zc.depth = find_target(algo, h, ZS[algo].maxd - 3.5 * u01(hash3(h, 1, 2)), &g_zc.tx, &g_zc.ty);
        g_zc.t0 = at, g_zc.len = ZEPOCH * w[i] / sum;
    }
    p = (te - g_zc.t0) / g_zc.len;
    p = p < 0.0 ? 0.0 : p > 1.0 ? 1.0 : p;
    s = p < 0.8 ? sin(1.5707963 * p / 0.8) : cos(1.5707963 * (p - 0.8) / 0.2);   /* in, then a quick pull back */
    *tx = g_zc.tx, *ty = g_zc.ty;
    return g_zc.depth * s;
}

/* -------------------------------------------------------------------------------------------- the view --- */
typedef struct {
    int algo, cap, bw, mirror;
    fx ox, oy;                     /* cell (0, 0)'s centre */
    fx ux, uy, vx, vy;             /* a cell step across / down */
    fx kr, ki, k2r, k2i;           /* the constants: c (Julia kinds), p and q (PHOENIX), the relaxation (NEWTON) */
} view_t;

static int count_cell(const view_t *v, fx x, fx y)   /* the band index's count, or -1: BG (still bounded) */
{
    int n;
    switch (v->algo) {
    case A_JULIA:
    case A_MANDEL:
    case A_SHIP:
    case A_TRICORN: {
        fx cr = v->kr, ci = v->ki, zx = x, zy = y;
        if (v->algo != A_JULIA) {
            cr = x, ci = y, zx = 0, zy = 0;
            if (v->algo == A_MANDEL) {             /* the main cardioid and the period-2 bulb: inside, skip */
                const fx a = x - F(0.25), y2 = MUL(y, y), q = MUL(a, a) + y2, b = x + ((fx)1 << FQ);
                if (MUL(q, q + a) < y2 / 4 || MUL(b, b) + y2 < F(0.0625))
                    return -1;
            }
        }
        for (n = 0; n < v->cap; n++) {
            fx xx, yy, xy;
            if (zx > TWO || zx < -TWO || zy > TWO || zy < -TWO)
                return n;
            xx = MUL(zx, zx), yy = MUL(zy, zy);
            if (xx + yy > FOUR)
                return n;
            xy = MUL(zx, zy);
            if (v->algo == A_SHIP)
                zy = 2 * (xy < 0 ? -xy : xy) + ci;
            else if (v->algo == A_TRICORN)
                zy = -2 * xy + ci;
            else
                zy = 2 * xy + ci;
            zx = xx - yy + cr;
        }
        return -1;
    }
    case A_JULIA4: {
        fx zx = x, zy = y;
        for (n = 0; n < v->cap; n++) {
            fx xx, yy, ax, ay;
            if (zx > TWO || zx < -TWO || zy > TWO || zy < -TWO)
                return n;
            xx = MUL(zx, zx), yy = MUL(zy, zy);
            if (xx + yy > FOUR)
                return n;
            ax = xx - yy, ay = 2 * MUL(zx, zy);    /* z^2, then its square */
            zx = MUL(ax, ax) - MUL(ay, ay) + v->kr;
            zy = 2 * MUL(ax, ay) + v->ki;
        }
        return -1;
    }
    case A_PHOENIX: {
        fx zx = x, zy = y, px = 0, py = 0;
        for (n = 0; n < v->cap; n++) {
            fx xx, yy, nx, ny;
            if (zx > TWO || zx < -TWO || zy > TWO || zy < -TWO)
                return n;
            xx = MUL(zx, zx), yy = MUL(zy, zy);
            if (xx + yy > FOUR)
                return n;
            nx = xx - yy + v->kr + MUL(v->k2r, px) - MUL(v->k2i, py);
            ny = 2 * MUL(zx, zy) + v->ki + MUL(v->k2r, py) + MUL(v->k2i, px);
            px = zx, py = zy, zx = nx, zy = ny;
        }
        return -1;
    }
    default: {                                     /* NEWTON, Q20: z -= a (z^3 - 1) / 3z^2; the root and the steps */
        const int sh = FQ - NQ;
        const int64_t one = (int64_t)1 << NQ, lim = (int64_t)3 << NQ, eps = one / 400;
        const int64_t ar = v->kr >> sh, ai = v->ki >> sh;
        int64_t zx = x >> sh, zy = y >> sh;
        for (n = 0; n < v->cap; n++) {
            int64_t wx, wy, fr, fi, dr, di, dd, qr, qi;
            if (zx > lim || zx < -lim || zy > lim || zy < -lim)
                return -1;
            wx = (zx * zx - zy * zy) >> NQ, wy = (2 * zx * zy) >> NQ;          /* z^2 */
            fr = ((wx * zx - wy * zy) >> NQ) - one, fi = (wx * zy + wy * zx) >> NQ;   /* z^3 - 1 */
            if (((fr * fr + fi * fi) >> NQ) < eps) {
                const int root = zx > 0 ? 0 : zy > 0 ? 1 : 2;
                return n + 3 * root;
            }
            dr = 3 * wx, di = 3 * wy;
            dd = (dr * dr + di * di) >> NQ;
            if (dd < one / 64)
                return -1;
            qr = ((fr * dr + fi * di) >> NQ) * one / dd, qi = ((fi * dr - fr * di) >> NQ) * one / dd;
            zx -= (ar * qr - ai * qi) >> NQ;
            zy -= (ar * qi + ai * qr) >> NQ;
        }
        return -1;
    }
    }
}

/* the c of a z^d + c Julia set on the edge of its Multibrot's main component (|f'| = 1 at the fixed point), angle a
 * (0..2 pi (d - 1) goes round), scaled by r about the component's centre: r < 1 inside (blobs), > 1 outside (dust) */
static void edge_c(int d, double a, double r, double *cr, double *ci)
{
    const double m = pow((double)d, -1.0 / (d - 1)), ph = a / (d - 1);
    const double zr = m * cos(ph), zi = m * sin(ph);
    double pr = zr, pi = zi, k;
    int i;
    for (i = 1; i < d; i++)                        /* z^d */
        k = pr * zr - pi * zi, pi = pr * zi + pi * zr, pr = k;
    *cr = r * (zr - pr), *ci = r * (zi - pi);
}

/* the Julia kinds' c: a glide from one constant to the next (random lengths ~JSEG, never the same twice running) with a
 * small wander on top. JULIA's from a list of well-loved ones (dendrites, rabbits, spirals, Siegel disks, dust);
 * JULIA4's anywhere on its Multibrot's edge, a little in or out */
#define JSEG 22.0
static const double JC[][2] = {{-0.8, 0.156},     {-0.4, 0.6},       {0.285, 0.01},    {-0.70176, -0.3842},
                               {-0.835, -0.2321}, {0.355, 0.355},    {-0.123, 0.745},  {-0.391, -0.587},
                               {-0.162, 1.04},    {0.0, 1.0},        {-0.54, 0.54},    {0.45, 0.1428},
                               {-0.75, 0.11},     {-1.476, 0.0},     {-1.25, 0.02},    {0.34, -0.05},
                               {-0.1, 0.651},     {0.27334, 0.00742}, {-0.7269, 0.1889}, {-1.7548, 0.0}};
#define JCN ((int)(sizeof JC / sizeof JC[0]))
static void julia_key(int d, uint32_t seed, long k, double *cr, double *ci)
{
    uint32_t h = hash3(seed, (uint32_t)k, 300u), hp = hash3(seed, (uint32_t)(k - 1), 300u);
    if (d == 2) {
        int i = (int)(h % (uint32_t)JCN);
        if (i == (int)(hp % (uint32_t)JCN))       /* never the same constant twice running */
            i = (i + 1) % JCN;
        *cr = JC[i][0], *ci = JC[i][1];
    } else
        edge_c(d, PI2 * (d - 1) * u01(h), 1.0 + 0.12 * u01(hash3(h, 1, 1)), cr, ci);
}
static void julia_c(int d, uint32_t seed, double t, double *cr, double *ci)
{
    const long k = (long)floor(t / JSEG);
    const double f0 = t / JSEG - (double)k, f = f0 * f0 * (3.0 - 2.0 * f0);
    double ar, ai, br, bi;
    julia_key(d, seed, k, &ar, &ai);
    julia_key(d, seed, k + 1, &br, &bi);
    *cr = ar + (br - ar) * f + 0.03 * wander(t, 0.047, seed, 8);
    *ci = ai + (bi - ai) * f + 0.03 * wander(t, 0.053, seed, 9);
}

typedef struct {
    int algo;
    uint32_t seed;
    double pull;                                   /* the pull back after collapsed frames (log2 zoom units) */
} cstate_t;
static cstate_t g_cs = {-1, 0, 0.0};

static void make_view(view_t *v, int algo, uint32_t seed, double t, double pulse)
{
    double cx = 0.0, cy = 0.0, hw = 1.5, zoom, rot, cell;
    double pull = g_cs.pull;
    memset(v, 0, sizeof *v);
    v->algo = algo;
    v->bw = 1;
    rot = 0.9 * wander(t, 0.021, seed, 0) + 0.025 * t * (seed & 1u ? 1.0 : -1.0);   /* (monotonic: never back) */
    switch (algo) {
    case A_JULIA:
    case A_JULIA4: {
        double cr, ci;
        julia_c(algo == A_JULIA ? 2 : 4, seed, t, &cr, &ci);
        v->kr = F(cr), v->ki = F(ci);
        zoom = (algo == A_JULIA ? 0.45 : 0.15) + 0.45 * wander(t, 0.029, seed, 3) - pull;
        hw = algo == A_JULIA ? 1.55 : 1.3;
        v->mirror = 1;
        v->cap = 20;
        break;
    }
    case A_PHOENIX: {
        v->kr = F(0.5667 + 0.05 * wander(t, 0.019, seed, 1));
        v->ki = F(0.06 * wander(t, 0.023, seed, 2));
        v->k2r = F(-0.5 + 0.07 * wander(t, 0.031, seed, 4));
        v->k2i = F(0.08 * wander(t, 0.017, seed, 5));
        zoom = 0.35 + 0.45 * wander(t, 0.027, seed, 3) - pull;
        cx = 0.12 * wander(t, 0.013, seed, 6), cy = 0.12 * wander(t, 0.011, seed, 7);
        hw = 1.6;
        v->cap = 16;
        break;
    }
    case A_NEWTON: {
        const double m = 0.35 + 0.35 * wander(t, 0.024, seed, 1), ph = PI2 * wander(t, 0.009, seed, 2) + 0.013 * t;
        v->kr = F(1.0 + m * cos(ph)), v->ki = F(m * sin(ph));
        zoom = 0.8 + 0.8 * wander(t, 0.026, seed, 3) - pull;
        cx = 0.45 * wander(t, 0.015, seed, 4), cy = 0.45 * wander(t, 0.012, seed, 5);
        hw = 1.6;
        v->cap = 18;
        break;
    }
    default: {                                     /* the zooms */
        double tx, ty, f;
        const zoomset_t *z = &ZS[algo];
        zoom = zoom_at(algo, seed, t, &tx, &ty);
        if (pull > zoom)                           /* (no pull past the overview: it would only linger there) */
            pull = g_cs.pull = zoom;
        zoom -= pull;
        zoom = zoom < 0.0 ? 0.0 : zoom;
        f = pow(2.0, -zoom);
        {   /* zooming in on the target, which moves on screen from where it is in the overview to beside the cat
             * (0.62 of the half width out, its angle wandering), not under it */
            const double a = PI2 * wander(t, 0.017, seed, 10) + 0.009 * t, hz = z->hw * f;
            const double sx = (tx - z->ox) / z->hw * f + 0.62 * cos(a) * (1.0 - f);
            const double sy = (ty - z->oy) / z->hw * f + 0.62 * sin(a) * (1.0 - f);
            cx = tx - sx * hz, cy = ty - sy * hz;
        }
        hw = z->hw;
        v->cap = zcap(zoom);
        v->bw = zbw(v->cap) + (algo == A_SHIP);   /* the ship's antenna is glittery: wider bands */
        break;
    }
    }
    cell = 2.0 * hw * pow(2.0, -zoom) * (1.0 - pulse) / CAT_N;
    v->ux = F(cell * cos(rot)), v->uy = F(cell * sin(rot));
    v->vx = F(-cell * sin(rot)), v->vy = F(cell * cos(rot));
    v->ox = F(cx) - (v->ux + v->vx) * (CAT_N - 1) / 2, v->oy = F(cy) - (v->uy + v->vy) * (CAT_N - 1) / 2;
    if (v->mirror)                                 /* exactly centred on 0: the mirror cell is the point's negative */
        v->ox = -(v->ux + v->vx) * (CAT_N - 1) / 2, v->oy = -(v->uy + v->vy) * (CAT_N - 1) / 2;
}

/* --------------------------------------------------------------------------------------------- the stage --- */
static int cat_px(const uint8_t *spr, int x, int y)
{
    const uint8_t b = spr[(y * EC_SPR_CAT_W + x) >> 1];
    return x & 1 ? b & 15 : b >> 4;
}

static int cat_covers(const uint8_t *spr, int sx, int sy, int px, int py)   /* any pixel of the 2x2 cell at px, py */
{
    int i, j;
    for (j = 0; j < 2; j++)
        for (i = 0; i < 2; i++) {
            const int x = px + i - sx, y = py + j - sy;
            if (x >= 0 && x < EC_SPR_CAT_W && y >= 0 && y < EC_SPR_CAT_H && cat_px(spr, x, y) != CAT_KEY)
                return 1;
        }
    return 0;
}

static ec_cat_stats_t g_st;
const ec_cat_stats_t *ec_scr_cat_stats(void) { return &g_st; }

static void cat_fractal(const uint8_t *spr, int sx, int sy, double beats, int playing, double t, int algo,
                        uint32_t seed)
{
    /* the kinds without the mirror compute half their cells a frame (a checkerboard, alternating), the other half
     * from the last frame: the counts cached as bands (-1 inside); a new algorithm / seed or a jump in t: all */
    static int16_t band[CAT_N * CAT_N];
    static int parity, full = 1;
    static double last_t;
    double pulse = 0.0;
    int lift = 0, rows, cx, cy, hist[10], cells = 0, top = 0, i;
    view_t v;
    if (algo < 0 || algo >= A_N)
        algo = 0;
    if (g_cs.algo != algo || g_cs.seed != seed)
        g_cs.algo = algo, g_cs.seed = seed, g_cs.pull = 0.0, full = 1;
    if (fabs(t - last_t) > 1.0)
        full = 1;
    last_t = t;
    parity ^= 1;
    if (playing) {                                 /* the beat's pulse, stronger on the bar */
        const double ph = beats - floor(beats), env = (1.0 - ph) * (1.0 - ph) * (1.0 - ph);
        const int bar = ((long)floor(beats) & 3) == 0;
        pulse = (bar ? 0.2 : 0.1) * env;
        lift = env > 0.4 ? (bar ? 2 : 1) : 0;
    }
    make_view(&v, algo, seed, t, pulse);
    memset(hist, 0, sizeof hist);
    rows = v.mirror ? CAT_N / 2 : CAT_N;
    for (cy = 0; cy < rows; cy++) {
        fx x = v.ox + v.vx * cy, y = v.oy + v.vy * cy;
        for (cx = 0; cx < CAT_N; cx++, x += v.ux, y += v.uy) {
            const int px = 2 * cx, py = 2 * cy, mx = 2 * (CAT_N - 1 - cx), my = 2 * (CAT_N - 1 - cy);
            const int here = !cat_covers(spr, sx, sy, px, py), there = v.mirror && !cat_covers(spr, sx, sy, mx, my);
            int n, k, c;
            if (!here && !there)
                continue;
            if (v.mirror || full || ((cx + cy) & 1) == parity)
                band[cy * CAT_N + cx] = (int16_t)((n = count_cell(&v, x, y)) < 0 ? -1 : n / v.bw);
            n = band[cy * CAT_N + cx];
            k = n < 0 ? 0 : (n + lift) % 10;
            c = BAND[k];
            hist[k] += here + there, cells += here + there;
            if (c == EC_C_BG)
                continue;
            if (here)
                R(c, px, py, 2, 2);
            if (there)
                R(c, mx, my, 2, 2);
        }
    }
    full = 0;
    hist[1] += hist[9], hist[2] += hist[8], hist[3] += hist[7], hist[4] += hist[6];   /* by grey */
    for (i = 0; i < 6; i++)
        top = hist[i] > top ? hist[i] : top;
    g_st.algo = algo, g_st.cap = v.cap, g_st.cells = cells, g_st.top = top, g_st.pull = g_cs.pull;
    if (cells && (top > CAT_FLAT * cells || hist[0] > CAT_DARK * cells))   /* collapsed: pull back; detail: ease in */
        g_cs.pull = g_cs.pull + (top >= cells - 8 ? 0.5 : 0.06) > 6.0 ? 6.0 : g_cs.pull + (top >= cells - 8 ? 0.5 : 0.06);
    else
        g_cs.pull = g_cs.pull > 0.01 ? g_cs.pull - 0.01 : 0.0;
    if (algo != A_MANDEL && algo != A_SHIP && algo != A_TRICORN && g_cs.pull > 1.5)
        g_cs.pull = 1.5;                           /* (the Julia kinds: out to the whole set, no further) */
}

void ec_scr_cat(int frame, double beats, int playing, double t, int algo, uint32_t seed)
{
    const int sx = (W - EC_SPR_CAT_W) / 2, sy = EC_KIT_CAT_Y;
    const uint8_t *spr;
    int x, y;
    frame %= EC_SPR_CAT_FRAMES;
    frame = frame < 0 ? frame + EC_SPR_CAT_FRAMES : frame;
    spr = ec_spr_cat[frame];
    R(EC_C_BG, 0, 0, W, H);
    cat_fractal(spr, sx, sy, beats, playing, t, algo, seed);
    ec_gfx_art(1);
    for (y = 0; y < EC_SPR_CAT_H; y++)            /* the cat over it: its ground keyed out, its outline BLACK */
        for (x = 0; x < EC_SPR_CAT_W; x++) {
            const int c = cat_px(spr, x, y);
            if (c != CAT_KEY)
                ec_gfx_pixel(sx + x, sy + y, c == EC_C_BG ? EC_C_BLACK : c);
        }
    ec_gfx_art(0);
}

/* 0.6.46 (owner): the cat alone (no fractal), its ground keyed out, at (x, y): PROJECTS' loading pop-up */
void ec_kit_cat_frame(int frame, int x0, int y0)
{
    const uint8_t *spr;
    int x, y;
    frame %= EC_SPR_CAT_FRAMES;
    frame = frame < 0 ? frame + EC_SPR_CAT_FRAMES : frame;
    spr = ec_spr_cat[frame];
    ec_gfx_art(1);
    for (y = 0; y < EC_SPR_CAT_H; y++)
        for (x = 0; x < EC_SPR_CAT_W; x++) {
            const int c = cat_px(spr, x, y);
            if (c != CAT_KEY)
                ec_gfx_pixel(x0 + x, y0 + y, c == EC_C_BG ? EC_C_BLACK : c);
        }
    ec_gfx_art(0);
}
