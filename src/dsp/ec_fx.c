/* ec_fx.c -- STUB (passthrough) until the FX machines land. */
#include <string.h>
#include "ec_fx.h"

void ec_fx_params_default(ec_fx_params_t *p) { memset(p, 0, sizeof *p); p->machine = EC_FX_NONE; p->mix = 64.f / 127.f; }
const char *ec_fx_name(int m) { (void)m; return "?"; }
void ec_fx_init(ec_fx_t *fx, float *mem, float fs, int is_send, uint32_t seed)
{
    memset(fx, 0, sizeof *fx);
    fx->mem = mem; fx->fs = fs; fx->is_send = is_send; fx->seed = seed; fx->running = EC_FX_NONE;
}
void ec_fx_process(ec_fx_t *fx, const ec_fx_params_t *p, float bpm, const float *kl, const float *kr, float *L, float *R, int n)
{
    (void)p; (void)bpm; (void)kl; (void)kr;
    if (fx->is_send) for (int i = 0; i < n; ++i) L[i] = R[i] = 0.f;
}
void ec_fx_clear(ec_fx_t *fx) { (void)fx; }
int ec_fx_running(const ec_fx_t *fx) { return fx->running; }
void ec_fx_meter(const ec_fx_t *fx, ec_fx_meter_t *m) { *m = fx->meter; }
