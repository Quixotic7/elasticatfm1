/* SPDX-License-Identifier: GPL-3.0-only */
/* test_v61ux: the owner's 0.6.0 play-test fixes (docs/UX-V2.md §10) through the core descriptor, the demo folder
 * (host/make_demo_folder.py) and the shipped layout, one gesture per check: no mode pop-up, a 0.6.0 APP SETTINGS
 * record loads, messages in the header (no box), every held key's header text (table-driven, docs/FM1-CONTROLS.md
 * "The header"), the source keys never change the mode, NOT MODDABLE, CAPTURE, KEYBOARD's stopped audition and its
 * pitch direction. The screen is read from the canvas (ec_gfx_canvas) and the header's line (ec_hdr_title).
 *   ec_test_v61ux <folder> */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ec_app.h"
#include "ec_fs_native.h"
#include "ec_gfx.h"
#include "ec_internal.h"
#include "ec_mtx.h"
#include "fm1core.h"

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

#define A (&ec_app)
static const fm1core_t *C;
static fm1panel_t *P;
static uint32_t ms;
static uint64_t frames;
static double sumsq;                    /* the output's energy since meter_reset() */
static uint64_t nsum;

static void audio_block(void)
{
    int16_t out[FM1P_BLOCK * 2];
    int i;
    C->audio(out, FM1P_BLOCK);
    for (i = 0; i < FM1P_BLOCK * 2; i++)
        sumsq += (double)out[i] * (double)out[i];
    nsum += FM1P_BLOCK * 2;
    frames += FM1P_BLOCK;
}
static void run(int n)
{
    int i;
    for (i = 0; i < n; i++) {
        ec_fs_native_pump();
        C->tick(ms);
        if (ms % 15 == 0)
            C->frame();
        else
            C->idle();
        ms++;
        while (frames + FM1P_BLOCK <= (uint64_t)ms * FM1P_FS / 1000u)
            audio_block();
    }
}
static void meter_reset(void) { sumsq = 0, nsum = 0; }
static double rms(void) { return nsum ? sqrt(sumsq / (double)nsum) : 0; }

static void btn(int b, int down)
{
    const uint32_t m = 1u << P->btn_id[b];
    if (down) {
        P->buttons |= m;
        P->buttons_tap |= m;
    } else
        P->buttons &= ~m;
}
static void press(int b, int dur)
{
    btn(b, 1);
    run(dur);
    btn(b, 0);
    run(20);
}
static int key_of(int upper, int i)
{
    int k, nl = 0, nu = 0;
    for (k = 0; k < FM1P_NKEY; k++) {
        const int s = k % 12;
        const int natural = s == 0 || s == 2 || s == 4 || s == 6 || s == 7 || s == 9 || s == 11;
        if (natural && !upper && nl++ == i)
            return k;
        if (!natural && upper && nu++ == i)
            return k;
    }
    return 0;
}
static void key(int upper, int i, int down)
{
    const int k = key_of(upper, i);
    if (down) {
        P->key_vel[k] = 100;
        P->keys |= 1u << k;
        P->keys_tap |= 1u << k;
    } else
        P->keys &= ~(1u << k);
}
static void tapkey(int upper, int i, int dur)
{
    key(upper, i, 1);
    run(dur);
    key(upper, i, 0);
    run(20);
}
static void turn(int role, int n)
{
    P->enc[P->enc_id[role]] += n * P->enc_dir[role];
    run(1);
}
static void bk(int b, int down) { key(1, b - 1, down); }   /* B1..B11 */
static void wk(int w, int down) { key(0, w - 1, down); }   /* W1..W16 */

static int mode(void) { return ec_app_mode(); }
static const int BTN_OF_MB[EC_NMB] = {-1, FM1P_B_FX, FM1P_B_SEL, FM1P_B_ENV, FM1P_B_LFO, FM1P_B_EDIT, FM1P_B_HOME,
                                      FM1P_B_SAVE, FM1P_B_ARP, FM1P_B_SEQ};
static void to(int m)
{
    int n;
    if (A->pn.as_open || A->ux.f3 || A->settings)
        press(FM1P_B_OCTDN, 40);
    if (A->pn.shift_sticky)
        press(FM1P_B_GLO, 40);
    if (A->pn.f_sticky)
        press(A->pn.f_sticky == 1 ? FM1P_B_OCTDN : FM1P_B_OCTUP, 40);
    if (A->rec.open)
        ec_rec_close();
    for (n = 0; n < 5 && mode() != m; n++)
        press(BTN_OF_MB[ec_mode_button(m)], 40);
    if (A->pn.cat)
        press(BTN_OF_MB[ec_mode_button(m)], 40);
    run(1300);                                                /* any message gone */
}

/* the screen now: the canvas after a forced draw */
static const uint8_t *draw(void)
{
    A->redraw = 1;
    C->frame();
    return ec_gfx_canvas();
}
static int px(int x, int y) { return draw()[y * EC_GW + x]; }
/* the header's title line and its bar */
static const char *hdr(int *bar)
{
    static char b[40];
    const int c = ec_hdr_title("", b, sizeof b);
    if (bar)
        *bar = c;
    return ec_hdr_custom() ? b : "";
}

/* ------------------------------------------------------------------------------------------- no mode pop-up --- */
static void test_no_popup(void)
{
    static uint8_t early[EC_GW * EC_GH];
    int m, y, same;
    const int MODES[] = {EC_CAT_FX, EC_CAT_FILT, EC_CAT_AMP, EC_CAT_MOD, EC_CAT_MASTER, EC_CAT_XFD, EC_CAT_PAT,
                         EC_CAT_SRC};
    for (m = 0; m < (int)(sizeof MODES / sizeof MODES[0]); m++) {
        to(MODES[m]);                                         /* (HOME: back to LOOP ENGINE, its last sub-mode) */
        to(MODES[m] == EC_CAT_SRC ? EC_CAT_FILT : EC_CAT_TRIG);
        press(BTN_OF_MB[ec_mode_button(MODES[m])], 40);
        run(100);                                             /* (the 0.6.0 pop-up was up 0.8 s) */
        memcpy(early, draw(), sizeof early);
        CHECK(mode() == MODES[m] && !ec_hdr_custom(), "mode %d entered: the header's own title (%s)", MODES[m],
              hdr(0));
        run(1200);
        same = 1;
        for (y = 10; y < EC_GH; y++)                          /* below the header: the same as a second later */
            if (memcmp(early + y * EC_GW, draw() + y * EC_GW, EC_GW))
                same = 0;
        CHECK(same || MODES[m] == EC_CAT_MASTER || MODES[m] == EC_CAT_SRC,
              "mode %d: nothing extra shows on entry (the screen 0.1 s in = 1.3 s in)", MODES[m]);
        CHECK(early[22 * EC_GW + 4] != EC_C_L12 || early[22 * EC_GW + 60] != EC_C_L12,
              "mode %d: no pop-up box (its frame at y 22)", MODES[m]);
    }
}

/* ------------------------------------------------------------------------------------------- APP SETTINGS --- */
static void test_appset_v1(void)
{
    /* a 0.6.0 record, byte by byte: "ECAS", version 1, size 20, theme 2, MODE POP-UP 1, knob 0, preview 0, at 0,
     * metro 2, cc 1, intro 0, advance 0, glide 3, pad */
    static const uint8_t V1[20] = {0x45, 0x43, 0x41, 0x53, 1, 0, 20, 0, 2, 1, 0, 0, 0, 2, 1, 0, 0, 3, 0, 0};
    uint32_t size = 0;
    uint8_t *rec = ec_plat_settings(&size);
    const ec_appset_t keep = A->pn.as;
    int r, popup_row = 0;
    CHECK(rec && size >= sizeof V1 && sizeof(ec_appset_t) == sizeof V1, "the record's layout is 0.6.0's (%u bytes)",
          (unsigned)sizeof(ec_appset_t));
    if (!rec)
        return;
    memcpy(rec, V1, sizeof V1);
    ec_as_load();
    CHECK(A->pn.as.theme == 2 && A->pn.as.knob == 0 && !A->pn.as.preview && !A->pn.as.at && A->pn.as.metro == 2 &&
              A->pn.as.cc == 1 && !A->pn.as.intro && !A->pn.as.advance && A->pn.as.glide == 3,
          "a version-1 record loads with every other value (theme %d knob %d preview %d at %d metro %d cc %d intro %d "
          "advance %d glide %d)",
          A->pn.as.theme, A->pn.as.knob, A->pn.as.preview, A->pn.as.at, A->pn.as.metro, A->pn.as.cc, A->pn.as.intro,
          A->pn.as.advance, A->pn.as.glide);
    for (r = 0; r < EC_AS_NROW; r++)
        if (strstr(ec_as_row_label(r), "POP"))
            popup_row = 1;
    CHECK(EC_AS_NROW == 7 && !popup_row && !strcmp(ec_as_row_label(1), "KNOB SPEED"), "no MODE POP-UP row (%d rows)",
          EC_AS_NROW);
    A->pn.as = keep;
    ec_as_store();
    ec_gfx_set_theme(A->pn.as.theme);
    CHECK(rec[9] == 0, "the old pop-up byte is written 0");
}

/* ------------------------------------------------------------------------------------------- messages --- */
static void test_messages(void)
{
    static uint8_t with[EC_GW * EC_GH];
    int bar, y, same = 1;
    to(EC_CAT_FILT);
    press(FM1P_B_OCTDN, 40);                                  /* F1 sticky + a step: COPY STEP 1 (0.6.0: a toast box) */
    run(200);
    wk(1, 1);
    run(30);
    wk(1, 0);
    run(30);
    CHECK(!strcmp(hdr(&bar), "COPY STEP 1") && bar == EC_C_L12 && !A->e5.toast_ms, "copy: COPY STEP 1 in the header (%s)",
          hdr(0));
    memcpy(with, draw(), sizeof with);
    CHECK(with[14] == EC_C_L12, "on the plain bar");
    A->msg_ms = 0;
    for (y = 10; y < EC_GH; y++)
        if (memcmp(with + y * EC_GW, draw() + y * EC_GW, EC_GW))
            same = 0;
    CHECK(same, "no box: below the header the screen is the same without the message");
    press(FM1P_B_OCTDN, 40);
    to(EC_CAT_SRC);                                           /* an alert: FILE while playing */
    press(FM1P_B_PLAY, 40);
    press(FM1P_B_SAVE, 40);                                   /* (playing: FILE opens the SAMPLER; again: refused) */
    run(100);
    press(FM1P_B_SAVE, 40);
    run(30);
    CHECK(!strcmp(hdr(&bar), "STOP FIRST") && bar == EC_C_ALERT && px(14, 0) == EC_C_ALERT,
          "an alert: STOP FIRST on the ALERT bar (%s %d)", hdr(0), bar);
    run(1300);
    CHECK(!ec_hdr_custom(), "gone after its time");
    press(FM1P_B_PLAY, 40);
    to(EC_CAT_FILT);                                          /* SHIFT + B4 RECALL (0.6.0: a toast box) */
    btn(FM1P_B_GLO, 1);
    run(20);
    bk(4, 1);
    run(20);
    bk(4, 0);
    btn(FM1P_B_GLO, 0);
    run(30);
    CHECK(strstr(hdr(0), "MEMORIZED") || strstr(hdr(0), "RECALL"), "SHIFT + B4: the header says it (%s)", hdr(0));
    CHECK(!A->e5.toast_ms, "no toast");
}

/* ------------------------------------------------------------------------------------------- held keys --- */
enum { K_BK, K_WK, K_BTN, K_SHIFTBK, K_WK2 };
typedef struct {
    int mode;
    int kind, idx;
    int hold;                     /* ms held in all (the check is at 250) */
    const char *want;
    int bar;
} held_t;

static void held_down(const held_t *h, int down)
{
    switch (h->kind) {
    case K_BK: bk(h->idx, down); break;
    case K_WK: wk(h->idx, down); break;
    case K_WK2:
        wk(h->idx, down);
        wk(h->idx + 1, down);
        break;
    case K_BTN: btn(h->idx, down); break;
    case K_SHIFTBK:
        if (down) {
            btn(FM1P_B_GLO, 1);
            run(10);
            bk(h->idx, 1);
        } else {
            bk(h->idx, 0);
            run(10);
            btn(FM1P_B_GLO, 0);
        }
        break;
    }
}

static void test_held(void)
{
    static const held_t H[] = {
        {EC_CAT_FILT, K_BK, 6, 450, "LFO 1", EC_C_SRC},
        {EC_CAT_FILT, K_BK, 7, 450, "LFO 2", EC_C_SRC},
        {EC_CAT_FILT, K_BK, 8, 450, "MOD ENV", EC_C_SRC},
        {EC_CAT_FILT, K_BK, 9, 450, "VEL", EC_C_SRC},
        {EC_CAT_FILT, K_BK, 10, 450, "AT", EC_C_SRC},
        {EC_CAT_MOD, K_BK, 2, 450, "M1", EC_C_SRC},
        {EC_CAT_MOD, K_BK, 3, 450, "M2", EC_C_SRC},
        {EC_CAT_MOD, K_BK, 4, 450, "M3", EC_C_SRC},
        {EC_CAT_MOD, K_BK, 5, 450, "M4", EC_C_SRC},
        {EC_CAT_FX, K_BK, 6, 450, "LFO 1", EC_C_SRC},
        {EC_CAT_TRIG, K_BK, 10, 450, "AT", EC_C_SRC},
        {EC_CAT_SRC, K_BK, 8, 450, "MOD ENV", EC_C_SRC},
        {EC_CAT_FILT, K_WK, 5, 450, "STEP 5", EC_C_LOCK},
        {EC_CAT_AMP, K_WK2, 3, 450, "2 STEPS", EC_C_LOCK},
        {EC_CAT_FILT, K_BTN, FM1P_B_OCTDN, 450, "COPY / CLR LOCK", EC_C_L12},
        {EC_CAT_FILT, K_BTN, FM1P_B_OCTUP, 450, "CUT / CLR MOD", EC_C_L12},
        {EC_MODE_PAGES, K_BTN, FM1P_B_OCTDN, 450, "COPY PAGE", EC_C_L12},
        {EC_CAT_PAT, K_BTN, FM1P_B_OCTUP, 450, "CUT PATTERN", EC_C_L12},
        {EC_CAT_FILT, K_BTN, FM1P_B_GLO, 450, "SHIFT", EC_C_L12},
        {EC_CAT_FILT, K_SHIFTBK, 11, 450, "FILL", EC_C_L12},
        {EC_CAT_XFD, K_BK, 1, 450, "SCENE A", EC_C_L12},
        {EC_CAT_XFD, K_BK, 2, 450, "SCENE B", EC_C_L12},
        {EC_CAT_XFD, K_BK, 11, 450, "CAPTURE", EC_C_L12},
        {EC_CAT_XFD, K_WK, 12, 450, "GLIDE TO 127", EC_C_L12},
        {EC_CAT_FILT, K_BK, 1, 300, "FILTER TYPE", EC_C_L12},
        {EC_CAT_AMP, K_BK, 1, 300, "ENV MODE", EC_C_L12},
        {EC_CAT_SRC, K_BK, 1, 300, "MACHINE", EC_C_L12},
        {EC_CAT_FX, K_BK, 1, 300, "FX TYPE", EC_C_L12},
        {EC_CAT_FX, K_BK, 3, 300, "S1 FX TYPE", EC_C_L12},
        {EC_CAT_FILT, K_BTN, FM1P_B_FX, 450, "FX SETUP", EC_C_L12},
        {EC_CAT_FX, K_BTN, FM1P_B_SEL, 450, "FILTER SETUP", EC_C_L12},
        {EC_CAT_FILT, K_BTN, FM1P_B_ENV, 450, "AMP SETUP", EC_C_L12},
        {EC_CAT_FILT, K_BTN, FM1P_B_EDIT, 450, "PROJECT SETUP", EC_C_L12},
        {EC_CAT_FILT, K_BTN, FM1P_B_HOME, 450, "LOOP SETUP", EC_C_L12},
        {EC_CAT_FILE, K_BTN, FM1P_B_SAVE, 450, "RECORDER SETUP", EC_C_L12},
        {EC_CAT_FILT, K_BTN, FM1P_B_SEQ, 450, "F3", EC_C_L12},
        {EC_CAT_FILT, K_BTN, FM1P_B_REC, 450, "REC SETUP", EC_C_L12},
        {EC_CAT_MASTER, K_WK, 2, 300, "MUTE T2", EC_C_L12},
        {EC_MODE_PAGES, K_BK, 1, 300, "LOOP PAGE 1", EC_C_L12},
        {EC_MODE_KEYBOARD, K_WK, 5, 450, "NOTE C4", EC_C_L12},
        {EC_MODE_KEYBOARD, K_WK, 1, 450, "NOTE F3", EC_C_L12},
        {EC_CAT_SRC, K_WK, 3, 450, "LOOP KEY 3", EC_C_L12},
    };
    int i, bar;
    A->proj.pat[A->proj.current].tr[0].v[EC_P_PATTERN_STEPS] = 32;   /* (PAGES: two pages) */
    for (i = 0; i < (int)(sizeof H / sizeof H[0]); i++) {
        const held_t *h = &H[i];
        to(h->mode);
        held_down(h, 1);
        run(60);
        CHECK(!ec_hdr_custom() || h->kind == K_SHIFTBK, "[%s] a tap (60 ms): no held text yet (%s)", h->want, hdr(0));
        run(190);
        CHECK(!strcmp(hdr(&bar), h->want) && bar == h->bar && px(14, 0) == h->bar,
              "[%s] held 250 ms: the header says it (\"%s\", bar %d)", h->want, hdr(0), bar);
        CHECK(mode() == h->mode || h->kind == K_BTN, "[%s] held: the mode stays", h->want);
        run(h->hold - 250);
        held_down(h, 0);
        run(1300);
        if (A->settings || A->ux.f3 || A->rec.open || A->pn.rs_open)
            press(FM1P_B_OCTDN, 40);                          /* (a hold that opened its settings) */
        if (A->rec.open)
            ec_rec_close();
        if (A->bus)
            A->bus = 0;
        run(1300);
        CHECK(!ec_hdr_custom(), "[%s] let go: the header's own title again (%s)", h->want, hdr(0));
    }
    to(EC_CAT_FILT);                                          /* sticky F1 / SHIFT: at once */
    press(FM1P_B_OCTDN, 40);
    CHECK(!strcmp(hdr(0), "COPY / CLR LOCK"), "F1 sticky: COPY / CLR LOCK at once (%s)", hdr(0));
    press(FM1P_B_OCTDN, 40);
    CHECK(!ec_hdr_custom(), "F1 off: gone");
    press(FM1P_B_GLO, 40);
    CHECK(!strcmp(hdr(0), "SHIFT"), "SHIFT sticky: SHIFT (%s)", hdr(0));
    press(FM1P_B_GLO, 40);
    CHECK(!ec_hdr_custom(), "SHIFT off: gone");
}

/* a provider (v61-fx's hold menus) goes before the built-in ones */
static int g_prov_on;
static int prov(char *out, int cap)
{
    if (!g_prov_on)
        return -1;
    snprintf(out, (size_t)cap, "MENU");
    return EC_C_SPECIAL;
}
static void test_provider(void)
{
    int bar;
    to(EC_CAT_FILT);
    ec_hdr_provider(prov);
    g_prov_on = 1;
    run(20);
    CHECK(!strcmp(hdr(&bar), "MENU") && bar == EC_C_SPECIAL, "a provider's text (%s)", hdr(0));
    bk(6, 1);
    run(250);
    CHECK(!strcmp(hdr(0), "MENU"), "before the built-in held keys");
    bk(6, 0);
    g_prov_on = 0;
    run(30);
    CHECK(!ec_hdr_custom(), "its key let go: nothing");
}

/* ------------------------------------------------------------------------------------------- source keys --- */
static void test_srckeys_mode(void)
{
    ec_page_t pg;
    static const int MODES[] = {EC_CAT_FX, EC_CAT_FILT, EC_CAT_AMP, EC_CAT_SRC, EC_CAT_TRIG};
    int m, b;
    for (m = 0; m < 5; m++)
        for (b = 6; b <= 10; b++) {
            to(MODES[m]);
            tapkey(1, b - 1, 40);
            CHECK(mode() == MODES[m] && !A->bus, "mode %d: B%d tapped: the mode stays (%d)", MODES[m], b, mode());
            bk(b, 1);
            run(700);
            CHECK(mode() == MODES[m], "mode %d: B%d held: the mode stays", MODES[m], b);
            bk(b, 0);
            run(30);
            CHECK(mode() == MODES[m], "mode %d: B%d let go: the mode stays", MODES[m], b);
        }
    to(EC_CAT_MOD);                                           /* MODULATION: M1..M4 tapped = the macro's page there */
    for (b = 2; b <= 5; b++) {
        tapkey(1, b - 1, 40);
        ec_app_page(A, &pg);
        CHECK(mode() == EC_CAT_MOD && pg.pic == EC_PIC_MACRO && pg.arg == b - 2, "MODULATION: M%d tapped: MACRO %d",
              b - 1, b - 1);
    }
    tapkey(1, 7 - 1, 40);
    ec_app_page(A, &pg);
    CHECK(mode() == EC_CAT_MOD && pg.pic == EC_PIC_LFO2, "MODULATION: B7 tapped: its LFO 2 page");
}

static void test_not_moddable(void)
{
    int bar;
    to(EC_CAT_FILT);
    ec_app_set_page(EC_CAT_FILT, 0);
    run(20);
    bk(6, 1);
    run(250);
    turn(FM1P_E_K3, 1);                                       /* the filter TYPE */
    run(20);
    CHECK(!strcmp(hdr(&bar), "NOT MODDABLE") && bar == EC_C_L12 && !A->ux.to_ms,
          "LFO 1 held + TYPE: NOT MODDABLE in the header, no takeover (%s)", hdr(0));
    run(1300);
    CHECK(!strcmp(hdr(&bar), "LFO 1") && bar == EC_C_SRC, "the message's time over: LFO 1 again (%s)", hdr(0));
    bk(6, 0);
    run(30);
    btn(FM1P_B_OCTUP, 1);                                     /* F2 + a knob on it: NOT MODDABLE too */
    run(20);
    turn(FM1P_E_K3, 1);
    run(20);
    btn(FM1P_B_OCTUP, 0);
    run(20);
    CHECK(!strcmp(hdr(0), "NOT MODDABLE"), "F2 + TYPE: NOT MODDABLE (%s)", hdr(0));
    run(1300);
    btn(FM1P_B_OCTUP, 1);                                     /* F2 + CUTOFF, nothing on it: NO MOD */
    run(20);
    turn(FM1P_E_K1, 1);
    run(20);
    btn(FM1P_B_OCTUP, 0);
    run(20);
    CHECK(!strcmp(hdr(0), "NO MOD"), "F2 + CUTOFF without modulation: NO MOD (%s)", hdr(0));
    run(1300);
}

static void test_capture(void)
{
    to(EC_CAT_XFD);
    bk(11, 1);
    run(250);
    CHECK(!strcmp(hdr(0), "CAPTURE"), "CROSSFADER: B11 held: CAPTURE (%s)", hdr(0));
    bk(1, 1);
    run(20);
    CHECK(!strcmp(hdr(0), "SCENE A CAPTURED"), "+ A: captured, said in the header (%s)", hdr(0));
    bk(1, 0);
    run(20);
    bk(11, 0);
    run(1300);
    CHECK(!ec_hdr_custom(), "let go: gone");
}

/* ------------------------------------------------------------------------------------------- KEYBOARD --- */
static void test_keyboard(void)
{
    float *v;
    double held, after;
    int k, lit;
    to(EC_MODE_KEYBOARD);
    ec_pn_select_target(0);                                   /* T1 (the demo folder's first loop) */
    v = ec_app_params(A);
    v[EC_P_MACHINE] = EC_MACH_LOOP;
    v[EC_P_PITCH] = 0;
    ec_seq_touch(&A->seq, A->track);
    run(1000);
    CHECK(!A->seq.playing, "stopped");
    meter_reset();
    run(300);
    CHECK(rms() < 5, "silence before (rms %.1f)", rms());
    wk(5, 1);                                                 /* C4 */
    run(100);
    meter_reset();
    run(400);
    held = rms();
    CHECK(held > 200 && A->dsp->tr[A->track].audition, "stopped: a key held plays the loop (rms %.1f)", held);
    wk(5, 0);
    run(1500);
    meter_reset();
    run(300);
    after = rms();
    CHECK(after < 5 && !A->dsp->tr[A->track].audition, "let go: silence (rms %.1f)", after);
    /* slice tracks: the selected slice */
    v[EC_P_MACHINE] = EC_MACH_GRID_SLICE;
    v[EC_P_SLICE_COUNT] = 8;
    v[EC_P_SLICE_INDEX] = 2;
    ec_seq_touch(&A->seq, A->track);
    run(300);
    wk(6, 1);
    run(50);
    meter_reset();
    run(150);
    held = rms();
    CHECK(held > 200, "a slice track: a key plays the slice (rms %.1f)", held);
    wk(6, 0);
    run(1500);
    meter_reset();
    run(300);
    CHECK(rms() < 5, "let go: silence (rms %.1f)", rms());
    v[EC_P_MACHINE] = EC_MACH_LOOP;
    ec_seq_touch(&A->seq, A->track);
    run(300);
    /* the pitch direction: the key lit = the key + PITCH */
    k = key_of(0, 4);
    wk(5, 1);
    run(50);
    lit = ec_kb_lit_key();
    CHECK(lit == k, "PITCH 0: the key held is lit (%d, %d)", lit, k);
    v[EC_P_PITCH] = 2;
    ec_seq_touch(&A->seq, A->track);
    run(50);
    lit = ec_kb_lit_key();
    CHECK(lit == k + 2 && fabsf(ec_seq_value(&A->seq, A->track, EC_P_PITCH) - 2.0f) < 0.01f,
          "PITCH +2: the lit key moves up the keyboard (%d)", lit);
    v[EC_P_PITCH] = -3;
    ec_seq_touch(&A->seq, A->track);
    run(50);
    lit = ec_kb_lit_key();
    CHECK(lit == k - 3, "PITCH -3: down (%d)", lit);
    wk(5, 0);
    run(30);
    v[EC_P_PITCH] = 0;
    ec_seq_touch(&A->seq, A->track);
    /* the target step (ALGO): no takeover, no message */
    turn(FM1P_E_ALGO, 3);
    run(30);
    CHECK(A->pn.target == 3 && !A->ux.to_ms && !ec_hdr_custom(), "ALGO: target step 3, nothing pops up (%s)", hdr(0));
    turn(FM1P_E_ALGO, -3);
    run(30);
    /* while playing: as 0.6.0 (the note's pitch, no audition) */
    press(FM1P_B_PLAY, 40);
    run(200);
    wk(5, 1);
    run(50);
    CHECK(!A->dsp->tr[A->track].audition && A->seq.tr[A->track].live_pitch_on &&
              fabsf(ec_seq_value(&A->seq, A->track, EC_P_PITCH)) < 0.01f,
          "playing: the key sets the pitch, no audition");
    wk(5, 0);
    run(30);
    CHECK(!A->seq.tr[A->track].live_pitch_on, "let go: the pitch back");
    press(FM1P_B_PLAY, 40);
    run(500);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("usage: ec_test_v61ux <folder>\n");
        return 2;
    }
    ec_fs_native_root(argv[1], "elasticat");
    C = fm1core_get(FM1CORE_ABI);
    if (!C)
        return 1;
    P = C->panel;
    C->flash_stage(NULL, 0);
    C->options(FM1CORE_FLASH_STAGED, 0, 0, 1);
    P->ready = 2;
    C->init(0);
    run(1500);
    turn(FM1P_E_K1, 1);                                       /* the intro skipped */
    run(6000);
    test_no_popup();
    test_appset_v1();
    test_messages();
    test_held();
    test_provider();
    test_srckeys_mode();
    test_not_moddable();
    test_capture();
    test_keyboard();
    C->shutdown();
    printf("test_v61ux: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
