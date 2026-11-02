/*
 * bench_wasm.c -- the bench scene as a wasm module with no imports, for timing
 * in wasm3 (the interpreter the .rfm1 runs in). Build: make -C tests/dsp wasm-bench;
 * a host calls bench_init() once, then bench_run(blocks) and times it.
 */
#include "ec_scene.h"

static ec_dsp_t *g_d;
static tk_scene_t g_sc;
static float g_buf[2 * 128];

__attribute__((used)) int bench_init(void)
{
    g_d = tk_engine();
    if (!g_d) return -1;
    tk_scene_setup(g_d, &g_sc);
    return (int)ec_dsp_mem_size();
}

__attribute__((used)) float bench_run(int blocks)
{
    float acc = 0.f;
    for (int b = 0; b < blocks; ++b) {
        tk_scene_events(g_d, &g_sc, 128);
        ec_dsp_process(g_d, g_buf, 128);
        acc += g_buf[0];
    }
    return acc;
}
