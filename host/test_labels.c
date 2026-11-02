/* SPDX-License-Identifier: GPL-3.0-only */
/* test_labels: the live panel labels (fm1core_labels_epoch / fm1core_label, FIZZ's 49 slots) through the
 * descriptor: none at boot, the FN layer while GLO is held on the panel, none again on release; the epoch moves
 * only when the table changes. */
#include <stdio.h>
#include <string.h>

#include "fm1core.h"
#include "ec_fs_native.h"

uint32_t fm1core_labels_epoch(void);
const char *fm1core_label(int32_t which);

static int fails, checks;
#define CHECK(c, ...)                                                                                              \
    do {                                                                                                           \
        checks++;                                                                                                  \
        if (!(c)) {                                                                                                \
            fails++;                                                                                               \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                                                            \
            printf(__VA_ARGS__);                                                                                   \
            printf("\n");                                                                                          \
        }                                                                                                          \
    } while (0)

static const fm1core_t *C;
static uint32_t ms;
static void run(int n)
{
    int i;
    for (i = 0; i < n; i++) {
        C->tick(ms++);
        if (ms % 15 == 0)
            C->frame();
    }
}
static int is(int which, const char *want)
{
    const char *s = fm1core_label(which);
    if (!want)
        return s == NULL;
    return s && !strcmp(s, want);
}
static int all_null(void)
{
    int i;
    for (i = 0; i < FM1CORE_LBL_N; i++)
        if (fm1core_label(i))
            return 0;
    return 1;
}

int main(void)
{
    static const char *const FN[FM1P_NB] = {"SET", "SET", "SET", "", "SET", NULL, "SET",
                                            "UNDO", "SET", "SET", "STOP", "SAVE", "RCL", "MEM"};
    fm1panel_t *p;
    uint32_t e0, e1, e2;
    int b, i;

    ec_fs_native_root(NULL, "elasticat");
    C = fm1core_get(FM1CORE_ABI);
    CHECK(C != NULL, "fm1core_get");
    if (!C)
        return 1;
    p = C->panel;
    C->flash_stage(NULL, 0);
    C->options(FM1CORE_FLASH_STAGED, 0, 0, 1);
    p->ready = 2;
    C->init(0);
    run(100);
    e0 = fm1core_labels_epoch();
    CHECK(e0 == 0, "no live labels at boot: epoch %u", e0);
    CHECK(all_null(), "no labels given at boot");

    p->buttons |= 1u << p->btn_id[FM1P_B_GLO];          /* hold FN */
    run(40);
    e1 = fm1core_labels_epoch();
    CHECK(e1 != e0, "FN held: the epoch moves");
    for (b = 0; b < FM1P_NB; b++)
        CHECK(is(FM1CORE_LBL_BTN0 + b, FN[b]), "FN button %d: '%s' want '%s'", b,
              fm1core_label(FM1CORE_LBL_BTN0 + b) ? fm1core_label(FM1CORE_LBL_BTN0 + b) : "(null)",
              FN[b] ? FN[b] : "(null)");
    for (i = 0; i < FM1CORE_LBL_BTN0; i++)
        CHECK(fm1core_label(i) == NULL, "knob %d not given", i);
    for (i = FM1CORE_LBL_WHITE0; i < FM1CORE_LBL_N; i++)
        CHECK(fm1core_label(i) == NULL, "key slot %d not given", i);
    CHECK(fm1core_label(-1) == NULL && fm1core_label(FM1CORE_LBL_N) == NULL, "out of range: NULL");
    run(60);
    CHECK(fm1core_labels_epoch() == e1, "still held, nothing changed: the epoch stays");

    p->buttons &= ~(1u << p->btn_id[FM1P_B_GLO]);       /* release FN */
    run(40);
    e2 = fm1core_labels_epoch();
    CHECK(e2 != e1, "FN released: the epoch moves");
    CHECK(all_null(), "FN released: no labels given");
    run(60);
    CHECK(fm1core_labels_epoch() == e2, "released, nothing changed: the epoch stays");

    C->shutdown();
    printf("test_labels: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
