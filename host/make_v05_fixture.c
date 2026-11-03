/* SPDX-License-Identifier: GPL-3.0-only */
/* make_v05_fixture: writes host/fixtures/v05_flash.bin, a flash image as 0.5.0 saves it (format 1, 1 MiB, project
 * record version 1), trimmed to the header sector + the record (the core pads the rest with 0xFF). NOT part of the
 * build: it is compiled against 0.5.0's sources (main de80447), so the record is the old firmware's own:
 *   git archive de80447 | tar -x -C /tmp/m050 && cmake -S /tmp/m050 -B /tmp/m050/build && \
 *   cmake --build /tmp/m050/build --target elasticat ec_testlayout && \
 *   cc -O1 -I/tmp/m050/src/{seq,dsp,plat} host/make_v05_fixture.c /tmp/m050/build/libelasticat.a -lm -o mk && \
 *   ./mk host/fixtures/v05_flash.bin
 * host/test_rec.c loads it and checks what is set here: every one of the 16 patterns, 16 slots, the scenes, the
 * matrix, the buses, names, page loops, razor. */
#include <stdio.h>
#include <string.h>

#include "ec_e5.h"
#include "ec_mtx.h"
#include "ec_project.h"
#include "ec_scene.h"

static ec_project_t P;
static uint8_t img[1u << 20];

int main(int argc, char **argv)
{
    int i, t, k;
    int32_t n;
    FILE *f;
    if (argc < 2)
        return 2;
    ec_project_init(&P);
    P.bpm = 128;
    P.swing = 55;
    P.glen = 64;
    P.quant = EC_Q_DIRECT_START;
    P.current = 5;
    for (i = 0; i < EC_NSLOT; i++) {
        snprintf(P.slot[i].path, EC_PATH, "/audio/slot%02d.wav", i + 1);
        P.slot[i].bpm = 90.0f + (float)i;
        P.slot[i].trim_start = 0.01f * (float)i;
        P.slot[i].gain = 1.5f;
    }
    for (i = 0; i < EC_NPAT; i++) {
        ec_pattern_prepare(&P, i);
        snprintf(P.pe5[i].name, EC_NAME, "PAT %d", i + 1);
        P.pe5[i].bpm = 100.0f + (float)i;
        for (t = 0; t < EC_NTRACK; t++) {
            ec_ptrack_t *tr = &P.pat[i].tr[t];
            tr->v[EC_P_MACHINE] = (float)(t < 3 ? 1 : 3);
            tr->v[EC_P_SLOT] = (float)(1 + (i + t) % EC_NSLOT);
            tr->v[EC_P_FILTER_CUTOFF] = (float)(40 + i + t);
            tr->v[EC_P_PATTERN_STEPS] = 32;
            tr->v[ec_mtx_src_id(0)] = (float)(EC_MS_LFO1 + 1);
            tr->v[ec_mtx_dst_id(0)] = (float)(EC_P_FILTER_CUTOFF + 1);
            tr->v[ec_mtx_depth_id(0)] = (float)(10 + t);
            for (k = 0; k < 32; k += 4) {
                ec_step_t *s = &tr->steps[k + (i % 4)];
                s->trig = 1;
                if (t >= 3)
                    s->slices = 1u << (k / 4);
                ec_step_lock_set(s, EC_P_PITCH, (float)(k / 4) - 3.0f);
                ec_step_lock_set(s, EC_P_VELOCITY, 0.75f);
                ec_step_lock_set(s, EC_P_FILTER_CUTOFF, (float)(i * 4 + k));
            }
        }
        P.pe5[i].page_loop[0] = 1;
    }
    P.razor[3].start[1] = 0.25f;
    P.bus[0][EC_P_FX_MACHINE] = 2;
    P.bus[0][EC_P_DELAY_FEEDBACK] = 77;
    ec_scene_set(&P, 0, 0, EC_P_FILTER_CUTOFF, 11);
    ec_scene_set(&P, 1, 2, EC_P_PAN, 99);
    snprintf(P.e5.name, EC_NAME, "OLD SONG");
    memset(img, 0xFF, sizeof img);
    {   /* ec_core.c's header (format 1) */
        const uint32_t magic = 0x54414345u, isz = sizeof img;
        const uint16_t fmt = 1, hs = 4096;
        uint32_t rs;
        n = ec_project_save(&P, img + 4096, sizeof img - 4096);
        if (n < 0)
            return 1;
        rs = (uint32_t)n;
        memset(img, 0, 32);                 /* (sizeof ec_flash_hdr_t; the rest stays erased) */
        memcpy(img, &magic, 4), memcpy(img + 4, &fmt, 2), memcpy(img + 6, &hs, 2), memcpy(img + 8, &isz, 4);
        memcpy(img + 12, &rs, 4), memcpy(img + 16, "0.5.0", 6);
    }
    f = fopen(argv[1], "wb");
    if (!f)
        return 1;
    fwrite(img, 1, 4096u + (uint32_t)n, f);
    fclose(f);
    printf("%s: %u bytes (record v1, %d bytes)\n", argv[1], 4096u + (unsigned)n, (int)n);
    return 0;
}
