/* SPDX-License-Identifier: GPL-3.0-only */
/* test_proj: 0.6.1's projects as files (docs/UX-V2.md §9, src/app/ec_projfile.h) through the panel, with the demo
 * folder (host/make_demo_folder.py; its data/elasticat/projects/ cleared first):
 *   SHIFT + SAVE writes /data/projects/PROJ-001.ecp (the packed record, byte for byte), SAVING in the header until
 *   it lands, the flash then holds the name; a reboot loads the last project by its name;
 *   a project bigger than the old 1 MiB flash record (64 patterns, locks on many steps) saves while playing over
 *   many ticks (SAVING all along, the audio running) and reloads bit-identically;
 *   a failed write (the native stand-in's hook: ENOSPC on the hidden copy, EIO on the main file) keeps a whole copy
 *   that loads, the error in ALERT; the next save repairs it;
 *   a 0.6.0 (and a 0.5) project record in the flash loads once, the next save makes the file and the flash keeps
 *   only the name;
 *   FILE's browser: the top level AUDIO/ PROJECTS/, LOAD (B10) on a project file opens it (unsaved edits saved
 *   first);
 *   a new project gets the lowest free name (PROJ-003 beside PROJ-001, 002, 004).
 *   the largest possible project fits the pack buffer (EC_PF_PACK_MAX).
 *   ec_test_proj <folder> */
#include <dirent.h>

#include "ec_harness.h"
#include "ec_projfile.h"

static const char *root;
static uint8_t img[EC_PLAT_FLASH_SIZE];

static uint8_t *read_file(const char *name, int hidden, uint32_t *n)
{
    char p[1024];
    FILE *f;
    long sz;
    uint8_t *d;
    snprintf(p, sizeof p, "%s/data/elasticat/projects/%s%s.ecp", root, hidden ? "." : "", name);
    *n = 0;
    f = fopen(p, "rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    d = malloc(sz > 0 ? (size_t)sz : 1u);
    if (d && fread(d, 1, (size_t)sz, f) != (size_t)sz) {
        free(d);
        d = NULL;
    }
    fclose(f);
    *n = d ? (uint32_t)sz : 0;
    return d;
}
static int file_exists(const char *name, int hidden)
{
    uint32_t n;
    uint8_t *d = read_file(name, hidden, &n);
    free(d);
    return d != NULL;
}
static void write_file(const char *name, const uint8_t *d, uint32_t n)
{
    char p[1024];
    FILE *f;
    snprintf(p, sizeof p, "%s/data/elasticat/projects/%s.ecp", root, name);
    f = fopen(p, "wb");
    if (f) {
        fwrite(d, 1, n, f);
        fclose(f);
    }
}

static void shift_save(void)
{
    shift_down(1);
    press(FM1P_B_SAVE, 40);
    shift_down(0);
}
/* SHIFT + SAVE held: the ticks from the press until it lands, *saving_all: SAVING in the header on each */
static int timed_save(int *saving_all)
{
    int ticks = 0, i;
    *saving_all = 1;
    shift_down(1);
    btn(FM1P_B_SAVE, 1);
    for (i = 0; i < 20000 && !ec_pf_saving(); i++)
        run(1);
    while (ticks < 20000 && ec_pf_saving()) {
        *saving_all &= !strcmp(A->msg, "SAVING") && A->msg_ms != 0;
        run(1);
        ticks++;
    }
    btn(FM1P_B_SAVE, 0);
    run(20);
    shift_down(0);
    return ticks;
}
/* run until the save in flight lands (or fails); the ticks it took */
static int until_saved(void)
{
    int i;
    for (i = 0; i < 20000 && ec_pf_saving(); i++)
        run(1);
    run(5);
    return i;
}
static void reboot(void)
{
    memcpy(img, C->flash(), sizeof img);
    C->shutdown();
    boot(img, sizeof img);
    run(200);
}
static void fresh_boot(void)
{
    char p[1024];
    C->shutdown();
    snprintf(p, sizeof p, "%s/data/elasticat/last.txt", root);   /* 0.6.31: a fresh flash alone would come back to the
                                                                   * folder's last project: a fresh boot forgets it too */
    remove(p);
    boot(NULL, 0);
}
static int flash_magic(uint32_t m)
{
    uint32_t size, v;
    memcpy(&v, ec_plat_record(&size), 4);
    return v == m;
}
#define ECLP 0x504C4345u
static uint8_t *repack(uint32_t *n)
{
    static uint8_t b[EC_PF_PACK_MAX];
    const int32_t r = ec_project_save(&A->proj, b, sizeof b);
    *n = r > 0 ? (uint32_t)r : 0;
    return b;
}
/* the pattern under test: a few trigs and a lock on T1, a BPM */
static void edit(float bpm, int stepi)
{
    ec_step_t *s = &A->proj.pat[A->proj.current].tr[0].steps[stepi];
    A->proj.bpm = bpm;
    A->proj.pe5[A->proj.current].bpm = bpm;        /* (the pattern's BPM is the one played) */
    s->trig = 1;
    ec_step_lock_set(s, EC_P_FILTER_CUTOFF, 1234.5f);
    ec_seq_touch_all(&A->seq);
    ec_app_mark_dirty();
}

/* ------------------------------------------------------------------------------------------- save, reboot --- */
static void test_save_reboot(void)
{
    uint32_t n, pn;
    uint8_t *d;
    const uint8_t *pk;
    int ticks, saving_all = 1, i;
    const int saves = A->saves;
    edit(133, 4);
    ticks = timed_save(&saving_all);
    CHECK(saving_all && ticks >= 3, "SHIFT + SAVE: SAVING in the header until it landed (%d ticks)", ticks);
    CHECK(A->saves == saves + 1 && !strcmp(A->msg, "SAVED") && !strcmp(ec_pf_name(), "PROJ-001"),
          "it landed: SAVED, the name PROJ-001 (%s, %s)", A->msg, ec_pf_name());
    d = read_file("PROJ-001", 0, &n);
    pk = ec_pf_pack(&pn);
    CHECK(d && n == pn && !memcmp(d, pk, n) && ec_project_check(d, n) == (int32_t)n,
          "/data/projects/PROJ-001.ecp = the packed record (%u bytes)", n);
    CHECK(!file_exists("PROJ-001", 1), "the hidden copy deleted");
    CHECK(flash_magic(ECLP), "the flash holds the name record");
    free(d);
    reboot();
    CHECK(!strcmp(ec_pf_name(), "PROJ-001") && (int)A->proj.bpm == 133 &&
              A->proj.pat[A->proj.current].tr[0].steps[4].trig,
          "reboot: PROJ-001 loaded by its name (bpm %g)", A->proj.bpm);
    for (i = 0; i < 100 && A->slot[0].state != EC_SL_READY; i++)
        run(10);
    CHECK(A->slot[0].state == EC_SL_READY, "its sample loads");
    {
        char b[4096];
        C->ui_info(b, sizeof b);
        CHECK(strstr(b, "pfile:PROJ-001 psave:0 pload:0") != NULL, "ui_info pfile:PROJ-001");
    }
}

/* ------------------------------------------------------------------------------------------- big --- */
static void fill_big(void)
{
    int p, t, i, k;
    for (p = 0; p < EC_NPAT; p++) {
        ec_pattern_prepare(&A->proj, p);
        for (t = 0; t < EC_NTRACK; t++)
            for (i = 0; i < 128; i++) {
                ec_step_t *s = &A->proj.pat[p].tr[t].steps[i];
                if (p == A->proj.current && t == 0 && i % 4)
                    continue;                      /* (the live pattern: T1 on the beat, heard) */
                s->trig = 1;
                for (k = 0; k < 8; k++)
                    ec_step_lock_set(s, EC_P_FILTER_CUTOFF + k,
                                     ec_param_clamp(EC_P_FILTER_CUTOFF + k, 0.37f + (float)(p + t + i + k) * 0.01f));
            }
    }
    ec_seq_touch_all(&A->seq);
    ec_app_mark_dirty();
}

static void test_big(void)
{
    uint32_t n, pn, rn;
    uint8_t *d, *keep;
    const uint8_t *pk;
    int ticks, saving_all = 1, saves = A->saves;
    uint64_t f0;
    fill_big();
    press(FM1P_B_PLAY, 40);
    run(500);
    f0 = frames;
    ticks = timed_save(&saving_all);
    pk = ec_pf_pack(&pn);
    CHECK(pn > EC_PLAT_RECORD_MAX, "the project is bigger than the old 1 MiB flash record (%u bytes)", pn);
    CHECK(ticks >= 8, "the save spans many ticks (%d): nothing blocks", ticks);
    printf("big: %u bytes saved over %d ticks\n", pn, ticks);
    CHECK(saving_all && !strcmp(A->msg, "SAVED") && A->saves == saves + 1, "SAVING all along, then SAVED (%s)",
          A->msg);
    CHECK(A->seq.playing && out_peak(f0, frames) > 1000 && out_peak(frames - 44100 / 50, frames) > 100,
          "the audio kept running while it saved (peak %d over %d ms)", out_peak(f0, frames), ticks);
    d = read_file("PROJ-001", 0, &n);
    CHECK(d && n == pn && !memcmp(d, pk, n), "PROJ-001.ecp = the pack, byte for byte (%u)", n);
    free(d);
    keep = malloc(pn);
    memcpy(keep, pk, pn);
    press(FM1P_B_PLAY, 40);
    reboot();
    d = repack(&rn);
    CHECK(rn == pn && !memcmp(d, keep, pn), "reboot: the big project reloads bit-identically (%u / %u bytes)", rn, pn);
    free(keep);
}

/* ------------------------------------------------------------------------------------------- a failed write --- */
static void test_failed_write(void)
{
    uint32_t n, n1, rn;
    uint8_t *old = read_file("PROJ-001", 0, &n1), *d;
    const int chunks = (int)((n1 + EC_PF_CHUNK - 1) / EC_PF_CHUNK);   /* (the hidden copy's write requests) */
    /* 1. ENOSPC in the middle of the hidden copy: PROJ-001.ecp untouched */
    const int saves = A->saves;
    edit(97, 6);
    ec_fs_native_fail(FS_WRITE, 1, FS_ENOSPC);
    shift_save();
    until_saved();
    CHECK(A->msg_alert && !strcmp(A->msg, "NOT SAVED: DISK FULL") && A->saves == saves,
          "ENOSPC mid-write: the error in ALERT (%s)", A->msg);
    d = read_file("PROJ-001", 0, &n);
    CHECK(d && old && n == n1 && !memcmp(d, old, n), "PROJ-001.ecp is the old file, intact");
    free(d);
    reboot();
    CHECK(!strcmp(ec_pf_name(), "PROJ-001") && (int)A->proj.bpm != 97 && !ec_pf_from_tmp(),
          "reboot: the old project loads (bpm %g)", A->proj.bpm);
    /* 2. EIO in the middle of PROJ-001.ecp itself (after the whole hidden copy): the hidden copy loads */
    edit(97, 6);
    ec_fs_native_fail(FS_WRITE, chunks + 1, FS_EIO);
    shift_save();
    until_saved();
    CHECK(A->msg_alert && !strcmp(A->msg, "NOT SAVED: IO ERROR"), "EIO mid-write: the error in ALERT (%s)", A->msg);
    d = read_file("PROJ-001", 0, &n);
    CHECK(d && ec_project_check(d, n) < 0, "PROJ-001.ecp is broken (%u bytes)", n);
    free(d);
    CHECK(file_exists("PROJ-001", 1), "the hidden copy is there");
    reboot();
    CHECK(!strcmp(ec_pf_name(), "PROJ-001") && (int)A->proj.bpm == 97 && ec_pf_from_tmp(),
          "reboot: the hidden copy loads (bpm %g)", A->proj.bpm);
    /* 3. the next save writes PROJ-001.ecp first (the hidden copy stays whole until it lands) */
    ec_app_mark_dirty();
    shift_save();
    until_saved();
    d = read_file("PROJ-001", 0, &n);
    CHECK(!A->msg_alert && d && ec_project_check(d, n) == (int32_t)n && !file_exists("PROJ-001", 1),
          "the next save repairs PROJ-001.ecp, the hidden copy deleted (%s)", A->msg);
    free(d);
    reboot();
    (void)repack(&rn);
    CHECK((int)A->proj.bpm == 97 && !ec_pf_from_tmp(), "reboot: PROJ-001.ecp loads");
    free(old);
}

/* ------------------------------------------------------------------------------------------- old flash --- */
static void test_old_flash(int v1)
{
    uint32_t size;
    uint8_t *rec;
    char want[EC_PF_NAME];
    fresh_boot();
    edit(v1 ? 88 : 111, 9);
    rec = ec_plat_record(&size);
    CHECK((v1 ? ec_project_save_v1(&A->proj, rec, size) : ec_project_save(&A->proj, rec, size)) > 0,
          "a %s project record in the flash", v1 ? "0.5" : "0.6.0");
    A->dirty = 0;
    reboot();
    CHECK(!ec_pf_name()[0] && (int)A->proj.bpm == (v1 ? 88 : 111) && A->proj.pat[A->proj.current].tr[0].steps[9].trig &&
              flash_magic(EC_PROJECT_MAGIC),
          "power-on: the flash's project loads (bpm %g)", A->proj.bpm);
    shift_save();
    until_saved();
    snprintf(want, sizeof want, "%s", ec_pf_name());
    CHECK(!strncmp(want, "PROJ-", 5) && file_exists(want, 0) && flash_magic(ECLP),
          "the next save makes the file %s; the flash keeps only the name", want);
    reboot();
    CHECK(!strcmp(ec_pf_name(), want) && (int)A->proj.bpm == (v1 ? 88 : 111), "reboot: %s loads from its file",
          ec_pf_name());
}

/* ------------------------------------------------------------------------------------------- FILE: LOAD --- */
static int find_row(const char *name)
{
    int i, d, f;
    for (i = 0; i < ec_file_count(); i++)
        if (!strcmp(ec_file_entry(i, &d, &f), name))
            return i;
    return -1;
}
static void cursor_to(const char *name)
{
    const int i = find_row(name);
    if (i >= 0) {
        turn(FM1P_E_SELECT, i - A->cursor);
        run(10);
    }
}

static void test_browser_load(void)
{
    char other[EC_PF_NAME];
    int i;
    /* the current project: the last one test_old_flash saved (bpm 88); PROJ-001 has bpm 97 */
    snprintf(other, sizeof other, "%s", ec_pf_name());
    edit(150, 11);                                 /* unsaved edits: LOAD saves them first */
    to(EC_CAT_FILE);
    ec_app_set_page(EC_CAT_FILE, 0);
    A->pn.fdir[0] = 0;
    run(20);
    B(1, 40);                                      /* up from /audio: the top level */
    CHECK(!strcmp(A->pn.fdir, "/") && ec_file_count() == 2, "B1: the top level (%s)", A->pn.fdir);
    cursor_to("PROJECTS/");
    B(2, 40);                                      /* into PROJECTS/ */
    for (i = 0; i < 50 && ec_pf_list_busy(); i++)
        run(5);
    CHECK(!strcmp(A->pn.fdir, EC_PF_DIR) && find_row("PROJ-001") >= 0 && find_row(other) >= 0,
          "B2: /data/projects lists the project files (%d: %s ..)", ec_file_count(), ec_file_entry(0, 0, 0));
    cursor_to("PROJ-001");
    CHECK(!A->pn.pv_play, "no preview on a project file");
    B(10, 40);                                     /* LOAD */
    for (i = 0; i < 500 && (ec_pf_saving() || ec_pf_loading() || strcmp(ec_pf_name(), "PROJ-001")); i++)
        run(5);
    run(50);
    CHECK(!strcmp(ec_pf_name(), "PROJ-001") && (int)A->proj.bpm == 97 && mode() == EC_CAT_SRC,
          "B10 LOAD: PROJ-001 open (bpm %g), LOOP ENGINE (mode %d)", A->proj.bpm, mode());
    reboot();
    CHECK(!strcmp(ec_pf_name(), "PROJ-001"), "the flash now names PROJ-001 (%s)", ec_pf_name());
    {
        uint32_t n;
        uint8_t *d = read_file(other, 0, &n);
        ec_project_t *p = malloc(sizeof *p);
        CHECK(d && p && ec_project_load(p, d, n) == 0 && (int)p->bpm == 150,
              "the edits made before LOAD were saved into %s first", other);
        free(p);
        free(d);
    }
}

/* ------------------------------------------------------------------------------------------- names --- */
static void test_next_free(void)
{
    uint32_t n;
    uint8_t *d = read_file("PROJ-001", 0, &n);
    char m[EC_PF_NAME];
    write_file("PROJ-004", d, n);                  /* PROJ-001, 002, 004: 003 is the gap */
    free(d);
    if (file_exists("PROJ-003", 0)) {
        char p[1024];
        snprintf(p, sizeof p, "%s/data/elasticat/projects/PROJ-003.ecp", root);
        remove(p);
    }
    CHECK(file_exists("PROJ-002", 0), "PROJ-002 exists");
    fresh_boot();
    edit(120, 2);
    shift_save();
    until_saved();
    snprintf(m, sizeof m, "%s", ec_pf_name());
    CHECK(!strcmp(m, "PROJ-003") && file_exists("PROJ-003", 0), "a new project: the lowest free name (%s)", m);
}

/* ------------------------------------------------------------------------------------------- the largest --- */
static void test_largest(void)
{
    ec_project_t *p = malloc(sizeof *p);
    static uint8_t b[EC_PF_PACK_MAX];
    int i, t, s, k, r;
    int32_t n;
    ec_project_init(p);
    for (k = 0; k < EC_NP; k++)
        for (i = 0; i < EC_NPAT; i++)
            for (t = 0; t < EC_NTRACK; t++)
                p->pat[i].tr[t].v[k] = 0.123f;
    for (i = 0; i < EC_NPAT; i++) {
        p->pat[i].used = 1;
        for (t = 0; t < EC_NTRACK; t++) {
            for (s = 0; s < EC_MAXSTEPS; s++) {
                ec_step_t *st = &p->pat[i].tr[t].steps[s];
                st->trig = 1;
                st->slices = 0xFFFFFFFFu;
                st->nlock = EC_STEP_LOCKS;
                for (k = 0; k < EC_STEP_LOCKS; k++) {
                    st->lock_id[k] = (uint8_t)k;
                    st->lock_val[k] = 0.37f;
                }
            }
            for (r = 0; r < 32; r++) {
                ec_slicemap_t *m = &p->pe5[i].slices[t];
                m->choke[r] = 1;
                m->nlock[r] = EC_SLICE_LOCKS;
                for (k = 0; k < EC_SLICE_LOCKS; k++) {
                    m->lock_id[r][k] = (uint8_t)k;
                    m->lock_val[r][k] = 0.3f;
                }
            }
        }
        snprintf(p->pe5[i].name, EC_NAME, "ABCDEFGHIJKLMNOP");
    }
    for (s = 0; s < 2; s++)
        for (t = 0; t < EC_SCN_ROWS; t++)
            for (k = 0; k < EC_NP; k++)
                p->e5.sc.has[s][t][k] = 1;
    for (i = 0; i < EC_NSLOT; i++) {
        memset(p->slot[i].path, 'a', EC_PATH - 1);
        p->slot[i].path[EC_PATH - 1] = 0;
    }
    n = ec_project_save(p, b, sizeof b);
    CHECK(n > 0 && (uint32_t)n <= EC_PF_PACK_MAX, "the largest project (%d bytes) fits the pack buffer (%u)", n,
          EC_PF_PACK_MAX);
    free(p);
}

/* no projects to start with (a run of its own: PROJ-001 first) */
static void clear_projects(void)
{
    char d[1024], p[1300];
    DIR *dp;
    struct dirent *e;
    snprintf(d, sizeof d, "%s/data/elasticat/projects", root);
    dp = opendir(d);
    if (!dp)
        return;
    while ((e = readdir(dp)))
        if (strstr(e->d_name, ".ecp")) {
            snprintf(p, sizeof p, "%s/%s", d, e->d_name);
            remove(p);
        }
    closedir(dp);
}

/* 0.6.35: SLOT's default was 1 until 0.6.32 (OFF since); a version 2 record left a slot-1 track's SLOT out, so a
 * missing SLOT there is slot 1; a version 3 record keeps OFF */
static void test_slot_default(void)
{
    static ec_project_t pr, back;
    static uint8_t b[1u << 20];
    int32_t n;
    ec_project_init(&pr);
    pr.pat[0].tr[2].v[EC_P_SLOT] = 5.0f;           /* T1 on 1 (written now), T2 OFF (left out), T3 on 5 */
    n = ec_project_save(&pr, b, sizeof b);
    CHECK(n > 16 && b[4] == EC_PROJECT_VERSION && b[5] == 0, "a save is version %d", EC_PROJECT_VERSION);
    CHECK(ec_project_load(&back, b, (uint32_t)n) == 0 && back.pat[0].tr[0].v[EC_P_SLOT] == 1.0f &&
              back.pat[0].tr[1].v[EC_P_SLOT] == 0.0f && back.pat[0].tr[2].v[EC_P_SLOT] == 5.0f,
          "version 3: T1 slot 1, T2 OFF, T3 slot 5 (%g %g %g)", back.pat[0].tr[0].v[EC_P_SLOT],
          back.pat[0].tr[1].v[EC_P_SLOT], back.pat[0].tr[2].v[EC_P_SLOT]);
    b[4] = 2;                                      /* the same payload as a version 2 record (the sum covers the payload) */
    CHECK(ec_project_load(&back, b, (uint32_t)n) == 0 && back.pat[0].tr[1].v[EC_P_SLOT] == 1.0f &&
              back.pat[0].tr[2].v[EC_P_SLOT] == 5.0f,
          "version 2: a missing SLOT is slot 1 (T2 %g, T3 %g)", back.pat[0].tr[1].v[EC_P_SLOT],
          back.pat[0].tr[2].v[EC_P_SLOT]);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("usage: ec_test_proj <folder>\n");
        return 2;
    }
    root = argv[1];
    clear_projects();
    ec_fs_native_root(root, "elasticat");
    boot(NULL, 0);
    test_save_reboot();
    test_big();
    test_failed_write();
    test_old_flash(0);
    test_old_flash(1);
    test_browser_load();
    test_next_free();
    test_largest();
    test_slot_default();
    C->shutdown();
    printf("test_proj: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
