/* ec_fx_state.h -- the FX slot state (private to ec_fx.c; included by ec_fx.h). STUB: replaced by the real one. */
#ifndef EC_FX_STATE_H
#define EC_FX_STATE_H
typedef struct ec_fx {
    float *mem;
    float fs;
    int is_send;
    int running;
    uint32_t seed;
    ec_fx_meter_t meter;
} ec_fx_t;
#endif
