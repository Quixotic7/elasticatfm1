/* SPDX-License-Identifier: GPL-3.0-only */
/* 0.6.1: projects as files in /data/projects/ (ec_projfile.h) */
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "ec_app.h"
#include "ec_fs.h"
#include "ec_loader.h"
#include "ec_projfile.h"

#define A (&ec_app)

#define NAME_MAGIC 0x504C4345u   /* "ECLP": the flash record holds the last project's name */
#define NAME_VERSION 1u
#define READ_CHUNK ((uint32_t)FS_MAX_LEN)

static uint8_t g_pack[EC_PF_PACK_MAX];  /* the packed project: a save's snapshot, a load's file */

/* the save: MKDIR, (LIST: the next free name), (CHECK: NAME.ecp is still what this instance last read or wrote),
 * WRITE the first copy, WRITE the second, DELETE the hidden one */
enum { SV_IDLE, SV_MKDIR, SV_LIST, SV_CHECK, SV_WRITE, SV_DELETE };
/* the load: READ NAME.ecp, else READ the hidden copy; WAIT: no folder yet (power-on before the folder is allowed) */
enum { LD_IDLE, LD_READ, LD_WAIT };

static struct {
    char name[EC_PF_NAME];       /* the current project's file ("" until its first save) */
    int tmp_good;                /* 1: NAME.ecp is broken, the hidden copy is the whole one */
    /* save */
    int sv, quiet, again, again_quiet;
    uint32_t len, at;
    int copy, ncopies;           /* the copy being written (0 / 1) of ncopies; which is hidden: hidden[] */
    int hidden[2];
    uint32_t list_gen;
    ec_fsreq_t req;
    /* load */
    int ld, boot, ld_tmp, from_tmp;
    uint32_t ld_at;
    char ld_name[EC_PF_NAME];
    int32_t ld_changed;
    int open_after;              /* LOAD waits for the save of the edits first */
    char open_name[EC_PF_NAME];
    /* the list */
    ec_fsreq_t lreq;
    int lwant, lrun;
    int64_t lentry;
    uint32_t lgen;
    int lerr;
    /* 0.6.36 (owner): what NAME.ecp holds as far as this instance knows (its last load or save), so a save never
     * overwrites a file another instance (another browser tab on the same folder) wrote meanwhile */
    int disk_known;
    uint32_t disk_len, disk_sum;
    uint32_t ck_at, ck_sum;
    int conflict;                /* a save found NAME.ecp changed: the next SHIFT + SAVE writes a new project */
    /* 0.6.42: NEW waiting for a save, the card being read */
    int new_after, save_new;
    int as;                      /* 0.6.45: the save is a save-as: its own message (SAVED +1, SAVED OVER IT) */
    char as_msg[20];
    ec_fsreq_t dreq;             /* 0.6.45: a delete: NAME.ecp, then its hidden copy */
    int dstep;
    char dname[EC_PF_NAME];
    ec_fsreq_t kreq;
    int kstate;                  /* 0 idle, 1 reading, 2 read */
    uint32_t kat;
    char kname[EC_PF_NAME], kwant[EC_PF_NAME];
    ec_project_card_t card;
} g;
static uint8_t g_kbuf[64 * 1024];

static uint8_t g_cbuf[16384];

static uint32_t sum_more(uint32_t h, const uint8_t *p, uint32_t n)   /* FNV-1a, streamed */
{
    uint32_t i;
    for (i = 0; i < n; i++)
        h = (h ^ p[i]) * 16777619u;
    return h;
}
#define SUM0 2166136261u

static void disk_set(const uint8_t *p, uint32_t n)
{
    g.disk_known = 1;
    g.disk_len = n;
    g.disk_sum = sum_more(SUM0, p, n);
}

static uint8_t g_lbuf[16384];
static char g_names[EC_PF_LIST_MAX][EC_PF_NAME];
static int g_nnames;
static char g_lnames[EC_PF_LIST_MAX][EC_PF_NAME];   /* the listing being built */
static int g_nl;

static void path_of(char *out, size_t cap, const char *name, int hidden)
{
    snprintf(out, cap, "%s/%s%s%s", EC_PF_DIR, hidden ? "." : "", name, EC_PF_EXT);
}

static void say_error(const char *what, int32_t err)
{
    char m[32];
    int i;
    snprintf(m, sizeof m, "%s: %s", what, ec_fs_strerror(err));
    for (i = 0; m[i]; i++)
        m[i] = (char)toupper((unsigned char)m[i]);
    ec_app_alert(m);
}

/* ------------------------------------------------------------------------------------------- the flash --- */
/* the name record: magic, version (u16), length (u8), the name */
static int flash_name(char *out)
{
    uint32_t size, magic;
    const uint8_t *r = ec_plat_record(&size);
    memcpy(&magic, r, 4);
    if (magic != NAME_MAGIC || (r[4] | r[5] << 8) != NAME_VERSION || r[6] == 0 || r[6] >= EC_PF_NAME)
        return 0;
    memcpy(out, r + 7, r[6]);
    out[r[6]] = 0;
    return 1;
}

/* 0.6.31 (owner): the last project's name also lives in the folder (/data/last.txt), so a host whose flash image
 * is fresh (a new firmware version, another device) still comes back to the last project; the flash record is read
 * first, the file when it says nothing */
#define EC_PF_LAST "/data/last.txt"
static void load_start(const char *name, int boot);
static ec_fsreq_t g_nreq;
static char g_last[EC_PF_NAME + 2], g_last_written[EC_PF_NAME + 2];
static int g_last_write, g_last_read, g_last_wait;
static uint32_t g_last_changed;

static void last_tick(void)
{
    if (g_last_wait && (uint32_t)ec_fs_changed() != g_last_changed) {   /* no folder at power-on: read once one is there */
        g_last_wait = 0;
        g_last_read = 1;
    }
    if (!g_last_read && !g_last_write)
        return;
    if (g_nreq.state == EC_FSR_IDLE) {
        if (g_last_read) {
            memset(g_last, 0, sizeof g_last);
            ec_fs_start(&g_nreq, FS_READ, EC_PF_LAST, 0, (uint8_t *)g_last, (int32_t)sizeof g_last - 1);
        } else
            ec_fs_start(&g_nreq, FS_WRITE, EC_PF_LAST, -1, (uint8_t *)g_last_written, (int32_t)strlen(g_last_written));
    }
    if (ec_fs_update(&g_nreq) != EC_FSR_DONE)
        return;
    g_nreq.state = EC_FSR_IDLE;
    if (g_last_read) {
        const int32_t r = g_nreq.result;
        g_last_read = 0;
        if (r == FS_ENOFOLDER) {
            g_last_wait = 1;
            g_last_changed = (uint32_t)ec_fs_changed();
        } else if (r > 0) {
            size_t n = (size_t)r < sizeof g_last - 1 ? (size_t)r : sizeof g_last - 1;
            g_last[n] = 0;
            while (n && (g_last[n - 1] == '\n' || g_last[n - 1] == '\r' || g_last[n - 1] == ' '))
                g_last[--n] = 0;
            if (n && n < EC_PF_NAME && g.ld == LD_IDLE && !g.name[0])
                load_start(g_last, 1);
        }
    } else
        g_last_write = 0;
}

static void last_set(const char *name)
{
    if (!strcmp(g_last_written, name))
        return;
    snprintf(g_last_written, sizeof g_last_written, "%s", name);
    g_last_write = 1;
}

static void flash_set_name(const char *name)
{
    last_set(name);
    uint32_t size;
    uint8_t *r = ec_plat_record(&size);
    const uint32_t magic = NAME_MAGIC;
    const size_t l = strlen(name);
    char now[EC_PF_NAME];
    if (flash_name(now) && !strcmp(now, name))
        return;                                     /* (unchanged: the host has nothing to persist) */
    memset(r, 0xFF, size);                          /* the old project record goes: the flash holds the name only */
    memcpy(r, &magic, 4);
    r[4] = NAME_VERSION & 0xFF;
    r[5] = 0;
    r[6] = (uint8_t)l;
    memcpy(r + 7, name, l);
    ec_plat_record_changed();
}

/* no folder, no name: the project goes into the flash record as in 0.6.0 (<= 1 MiB) */
static int flash_fallback(void)
{
    uint32_t size;
    uint8_t *rec = ec_plat_record(&size);
    if (g.len > size)
        return -1;
    memcpy(rec, g_pack, g.len);
    ec_plat_record_changed();
    return 0;
}

/* ------------------------------------------------------------------------------------------- the list --- */
void ec_pf_list_refresh(void) { g.lwant = 1; }
int ec_pf_list_count(void) { return g_nnames; }
const char *ec_pf_list_name(int i) { return i >= 0 && i < g_nnames ? g_names[i] : ""; }
int ec_pf_list_busy(void) { return g.lwant || g.lrun; }

static void list_publish(void);
static void list_tick(void)
{
    if (!g.lrun) {
        if (!g.lwant)
            return;
        g.lwant = 0;
        g.lrun = 1;
        g.lentry = 0;
        g_nl = 0;
        g.lerr = 0;
    }
    if (g.lreq.state == EC_FSR_IDLE)
        ec_fs_start(&g.lreq, FS_LIST, EC_PF_DIR, g.lentry, g_lbuf, (int32_t)sizeof g_lbuf);
    if (ec_fs_update(&g.lreq) != EC_FSR_DONE)
        return;
    {
        const int32_t r = g.lreq.result;
        uint32_t at = 0;
        int32_t k;
        g.lreq.state = EC_FSR_IDLE;
        if (r > 0) {
            for (k = 0; k < r && at + 2u <= sizeof g_lbuf; k++) {
                const int is_dir = g_lbuf[at], nl = g_lbuf[at + 1];
                const int el = (int)strlen(EC_PF_EXT);
                if (at + 2u + (uint32_t)nl > sizeof g_lbuf)
                    break;
                if (!is_dir && nl > el && nl - el < EC_PF_NAME && g_lbuf[at + 2] != '.' &&
                    !memcmp(g_lbuf + at + 2 + nl - el, EC_PF_EXT, (size_t)el) && g_nl < EC_PF_LIST_MAX) {
                    memcpy(g_lnames[g_nl], g_lbuf + at + 2, (size_t)(nl - el));
                    g_lnames[g_nl][nl - el] = 0;
                    g_nl++;
                }
                at += 2u + (uint32_t)nl;
            }
            g.lentry += r;
            return;                                 /* (the next entries) */
        }
        g.lerr = r < 0 && r != FS_ENOENT ? r : 0;   /* (no folder /data/projects yet: no projects) */
        list_publish();                             /* 0.6.45 (owner): by name, naturally */
    }
}

/* 0.6.45 (owner): the list by name, naturally: the numbers by their value, a name before its numbered copies
 * ("SPICY WALRUS", "SPICY WALRUS 2" .. "SPICY WALRUS 10"; plain alphabetical puts 10 before 2), case aside */
static int natcmp(const char *a, const char *b)
{
    while (*a && *b) {
        if (*a >= '0' && *a <= '9' && *b >= '0' && *b <= '9') {
            const char *a0, *b0;
            size_t la, lb;
            while (*a == '0' && a[1] >= '0' && a[1] <= '9')
                a++;
            while (*b == '0' && b[1] >= '0' && b[1] <= '9')
                b++;
            for (a0 = a; *a >= '0' && *a <= '9'; a++)
                ;
            for (b0 = b; *b >= '0' && *b <= '9'; b++)
                ;
            la = (size_t)(a - a0), lb = (size_t)(b - b0);
            if (la != lb)
                return la < lb ? -1 : 1;
            {
                const int c = strncmp(a0, b0, la);
                if (c)
                    return c;
            }
            continue;
        }
        {
            const int ca = toupper((unsigned char)*a), cb = toupper((unsigned char)*b);
            if (ca != cb)
                return ca < cb ? -1 : 1;
        }
        a++, b++;
    }
    return *a ? 1 : *b ? -1 : 0;
}

static void list_publish(void)
{
    int i, j;
    for (i = 1; i < g_nl; i++)                      /* (insertion sort: a few hundred at most) */
        for (j = i; j > 0 && natcmp(g_lnames[j], g_lnames[j - 1]) < 0; j--) {
            char t[EC_PF_NAME];
            memcpy(t, g_lnames[j], sizeof t);
            memcpy(g_lnames[j], g_lnames[j - 1], sizeof t);
            memcpy(g_lnames[j - 1], t, sizeof t);
        }
    memcpy(g_names, g_lnames, sizeof g_lnames[0] * (size_t)g_nl);
    g_nnames = g_nl;
    g.lrun = 0;
    g.lgen++;
    A->redraw = 1;
}

int ec_pf_list_find(const char *name)
{
    int i;
    for (i = 0; name && i < g_nnames; i++)
        if (!strcmp(g_names[i], name))
            return i;
    return -1;
}

void ec_pf_name_plus(const char *base, char *out)
{
    char b[EC_PF_NAME];
    int l, n;
    snprintf(b, sizeof b, "%s", base && base[0] ? base : "PROJ");
    l = (int)strlen(b);                             /* "SPICY WALRUS 2" -> "SPICY WALRUS" */
    while (l > 0 && b[l - 1] >= '0' && b[l - 1] <= '9')
        l--;
    if (l > 0 && l < (int)strlen(b) && b[l - 1] == ' ')
        b[l - 1] = 0;
    for (n = 2; n < 1000; n++) {
        char num[8];
        int keep;
        snprintf(num, sizeof num, " %d", n);
        keep = 16 - (int)strlen(num);              /* (16: the names' room, EC_NAME - 1) */
        snprintf(out, EC_PF_NAME, "%.*s%s", keep, b, num);
        if (ec_pf_list_find(out) < 0)
            return;
    }
}

/* the lowest PROJ-nnn not in the list */
static void next_free(char *out)
{
    int n, i;
    for (n = 1; n < 100000; n++) {
        snprintf(out, EC_PF_NAME, "PROJ-%03d", n);
        for (i = 0; i < g_nnames && strcmp(g_names[i], out); i++)
            ;
        if (i == g_nnames)
            return;
    }
}

/* ------------------------------------------------------------------------------------------- the save --- */
static void save_start(void)
{
    const int32_t n = ec_project_save(&A->proj, g_pack, sizeof g_pack);
    if (n <= 0) {                                   /* (cannot happen: EC_PF_PACK_MAX holds the largest project) */
        ec_app_alert("TOO BIG: NOT SAVED");
        return;
    }
    g.len = (uint32_t)n;
    A->dirty = 0;                                   /* (an edit while it is written: saved again later) */
    g.sv = SV_MKDIR;
    if (!g.quiet)
        ec_app_message("SAVING");
}

void ec_pf_save(int quiet)
{
    if (g.ld != LD_IDLE) {                          /* a project is being read (it replaces this one), or no folder */
        A->dirty = 0;                               /* (no autosave loop) */
        if (!quiet)
            ec_app_alert(g.ld == LD_WAIT ? "NOT SAVED: NO FOLDER" : "BUSY: TRY AGAIN");
        return;
    }
    if (g.sv != SV_IDLE) {
        g.again_quiet = g.again ? g.again_quiet && quiet : quiet;
        g.again = 1;
        if (!quiet)
            ec_app_message("SAVING");
        return;
    }
    g.quiet = quiet;
    if (g.conflict && !quiet && g.name[0]) {        /* 0.6.36: SHIFT + SAVE after FILE CHANGED: a new project */
        g.name[0] = 0;
        g.disk_known = 0;
        g.conflict = 0;
    }
    save_start();
}

static void save_done(int ok, const char *err_what, int32_t err)
{
    g.sv = SV_IDLE;
    if (ok) {
        A->saves++;
        disk_set(g_pack, g.len);                    /* 0.6.36: NAME.ecp is this save */
        g.conflict = 0;
        if (g.as)                                   /* 0.6.45: what kind of save it was (the name: NOW, the cursor) */
            ec_app_message(g.as_msg);
        else if (!g.quiet)
            ec_app_message("SAVED");
        g.as = 0;
        ec_pf_list_refresh();
    } else {
        say_error(err_what, err);
        g.again = 0;                                /* (no retry loop: the next edit or SHIFT + SAVE tries again) */
        g.open_after = 0;
    }
    if (g.again) {
        g.again = 0;
        g.quiet = g.again_quiet;
        save_start();
    }
}

/* the copies' order: the broken one (or the hidden one, normally) first, so a whole copy always stays */
static void save_writes(void)
{
    if (g.tmp_good) {                               /* NAME.ecp is broken: it is written; the hidden copy stays whole */
        g.ncopies = 1;
        g.hidden[0] = 0;
    } else {
        g.ncopies = 2;
        g.hidden[0] = 1;
        g.hidden[1] = 0;
    }
    g.copy = 0;
    g.at = 0;
    g.sv = SV_WRITE;
}

static void save_tick(void)
{
    char path[96];
    if (g.sv == SV_IDLE)
        return;
    if (!g.quiet && (!A->msg_ms || !strcmp(A->msg, "SAVING")))
        ec_app_message("SAVING");                   /* (the header says so until it lands) */
    switch (g.sv) {
    case SV_MKDIR:
        if (g.req.state == EC_FSR_IDLE)
            ec_fs_start(&g.req, FS_MKDIR, EC_PF_DIR, 0, 0, 0);
        if (ec_fs_update(&g.req) != EC_FSR_DONE)
            return;
        g.req.state = EC_FSR_IDLE;
        if (g.req.result == FS_ENOFOLDER && !g.name[0] && g.ld == LD_IDLE) {
            if (flash_fallback() == 0)              /* no folder: the flash keeps it, as 0.6.0 did */
                save_done(1, 0, 0);
            else
                save_done(0, "NOT SAVED", FS_ENOFOLDER);   /* (bigger than the flash record) */
            return;
        }
        if (g.req.result < 0) {
            save_done(0, "NOT SAVED", g.req.result);
            return;
        }
        if (g.name[0] && g.disk_known && !g.tmp_good) {   /* 0.6.36: first check NAME.ecp is still ours */
            g.sv = SV_CHECK;
            g.ck_at = 0;
            g.ck_sum = SUM0;
        } else if (g.name[0])
            save_writes();
        else {
            g.sv = SV_LIST;
            g.list_gen = g.lgen;
            ec_pf_list_refresh();
        }
        return;
    case SV_LIST:
        if (g.lgen == g.list_gen || g.lrun || g.lwant)
            return;
        if (g.lerr) {
            save_done(0, "NOT SAVED", g.lerr);
            return;
        }
        next_free(g.name);
        g.tmp_good = 0;
        save_writes();
        return;
    case SV_CHECK:                                  /* 0.6.36: NAME.ecp read back (streamed) and compared */
        if (g.req.state == EC_FSR_IDLE) {
            path_of(path, sizeof path, g.name, 0);
            ec_fs_start(&g.req, FS_READ, path, (int64_t)g.ck_at, g_cbuf, (int32_t)sizeof g_cbuf);
        }
        if (ec_fs_update(&g.req) != EC_FSR_DONE)
            return;
        g.req.state = EC_FSR_IDLE;
        if (g.req.result > 0) {
            g.ck_sum = sum_more(g.ck_sum, g_cbuf, (uint32_t)g.req.result);
            g.ck_at += (uint32_t)g.req.result;
            return;                                 /* (the rest; 0 = the end) */
        }
        if (g.req.result == 0 && (g.ck_at != g.disk_len || g.ck_sum != g.disk_sum)) {
            g.conflict = 1;                         /* another instance wrote it: keep its file */
            g.again = 0;
            g.open_after = 0;
            g.sv = SV_IDLE;
            ec_app_alert("NOT SAVED: FILE CHANGED");
            return;
        }
        save_writes();                              /* the same file, or gone, or unreadable (the write will say) */
        return;
    case SV_WRITE:
        if (g.req.state == EC_FSR_IDLE) {
            const uint32_t n = g.len - g.at < EC_PF_CHUNK ? g.len - g.at : EC_PF_CHUNK;
            path_of(path, sizeof path, g.name, g.hidden[g.copy]);
            ec_fs_start(&g.req, FS_WRITE, path, g.at ? (int64_t)g.at : -1, g_pack + g.at, (int32_t)n);
        }
        if (ec_fs_update(&g.req) != EC_FSR_DONE)
            return;
        g.req.state = EC_FSR_IDLE;
        if (g.req.result <= 0) {                    /* (0 bytes written: the disk took nothing) */
            if (!g.hidden[g.copy] && g.ncopies == 2)
                g.tmp_good = 1;                     /* NAME.ecp broken now; the hidden copy (just written) is whole */
            save_done(0, "NOT SAVED", g.req.result ? g.req.result : FS_ENOSPC);
            return;
        }
        g.at += (uint32_t)g.req.result;
        if (g.at < g.len)
            return;
        g.at = 0;
        if (++g.copy < g.ncopies)
            return;
        g.sv = SV_DELETE;                           /* NAME.ecp is whole: the hidden copy goes */
        g.tmp_good = 0;
        flash_set_name(g.name);                     /* power-on loads it (an old project record is replaced) */
        return;
    case SV_DELETE:
        if (g.req.state == EC_FSR_IDLE) {
            path_of(path, sizeof path, g.name, 1);
            ec_fs_start(&g.req, FS_DELETE, path, 0, 0, 0);
        }
        if (ec_fs_update(&g.req) != EC_FSR_DONE)
            return;
        g.req.state = EC_FSR_IDLE;                  /* (a hidden copy that stays is harmless: NAME.ecp is read first) */
        save_done(1, 0, 0);
        return;
    default:
        return;
    }
}

/* ------------------------------------------------------------------------------------------- the load --- */
static void load_start(const char *name, int boot)
{
    snprintf(g.ld_name, sizeof g.ld_name, "%s", name);
    g.boot = boot;
    g.ld = LD_READ;
    g.ld_tmp = 0;
    g.ld_at = 0;
    g.ld_changed = ec_fs_changed();
}

/* the project in g_pack replaces the live one: every sample let go (FILE is stopped, nothing is recording), the
 * slots queued again, the sequencer reloaded, the undo history cleared; LOOP ENGINE (§3: project load) */
static void replace(uint32_t n)
{
    int s, t;
    ec_file_leave();                                /* (the preview's sample) */
    if (A->load_slot >= 0) {
        ec_load_cancel();
        A->load_slot = -1;
    }
    for (t = 0; t < EC_NTRACK; t++) {
        ec_dsp_set_sample(A->dsp, t, 0);
        A->assigned[t] = -1;
    }
    ec_ss_reset();
    for (s = 0; s < EC_NSLOT; s++) {
        memset(&A->slot[s], 0, sizeof A->slot[s]);
        A->slot[s].handle = A->slot[s].old_handle = -1;
    }
    A->repack = 0;
    if (ec_project_load(&A->proj, g_pack, n) != 0)
        ec_project_init(&A->proj);                  /* (cannot happen: the checksum was checked) */
    for (s = 0; s < EC_NSLOT; s++)
        if (A->proj.slot[s].path[0])
            A->slot[s].state = EC_SL_QUEUED;
    A->fresh = 0;
    A->dirty = 0;
    ec_e5_init();
    ec_seq_reload(&A->seq);
    ec_seq_touch_all(&A->seq);
    ec_dsp_set_tempo(A->dsp, A->proj.bpm);
    ec_app_assign_samples();
    A->redraw = 1;
}

static void load_done(uint32_t n)
{
    char m[48];
    const int boot = g.boot;
    g.ld = LD_IDLE;
    g.from_tmp = g.ld_tmp;
    replace(n);
    snprintf(g.name, sizeof g.name, "%s", g.ld_name);
    g.tmp_good = g.ld_tmp;
    g.conflict = 0;
    if (g.ld_tmp)
        g.disk_known = 0;                           /* (NAME.ecp is broken: the save rewrites it unchecked) */
    else
        disk_set(g_pack, n);                        /* 0.6.36: NAME.ecp as read */
    flash_set_name(g.name);
    snprintf(m, sizeof m, g.ld_tmp ? "%s (COPY)" : "%s LOADED", g.name);
    ec_app_message(m);
    if (!boot)
        ec_app_goto(EC_CAT_SRC, ec_src_loop_page());   /* 0.6.41: the LOOP page */
}

static void load_fail(int32_t err)
{
    const int boot = g.boot;
    g.ld = LD_IDLE;
    if (boot) {                                     /* power-on: a new project instead (a new name at its first save) */
        g.name[0] = 0;
        ec_app_alert(err == FS_ENOENT ? "PROJECT MISSING" : "PROJECT DAMAGED");
        ec_app_fresh_project();
    } else
        ec_app_alert(err == FS_ENOENT ? "NOT FOUND" : "DAMAGED FILE");
}

static void load_tick(void)
{
    char path[96];
    if (g.ld == LD_WAIT) {                          /* no folder at power-on: read once one is there */
        if (ec_fs_changed() != g.ld_changed)
            load_start(g.ld_name, g.boot);
        return;
    }
    if (g.ld != LD_READ)
        return;
    if (g.req.state == EC_FSR_IDLE) {
        const uint32_t room = (uint32_t)sizeof g_pack - g.ld_at;
        path_of(path, sizeof path, g.ld_name, g.ld_tmp);
        if (!room) {                                /* (bigger than any project can be) */
            g.req.state = EC_FSR_DONE;
            g.req.result = FS_EIO;
        } else
            ec_fs_start(&g.req, FS_READ, path, (int64_t)g.ld_at, g_pack + g.ld_at,
                        (int32_t)(room < READ_CHUNK ? room : READ_CHUNK));
    }
    if (ec_fs_update(&g.req) != EC_FSR_DONE)
        return;
    g.req.state = EC_FSR_IDLE;
    {
        const int32_t r = g.req.result;
        if (r == FS_ENOFOLDER && g.boot) {
            g.ld = LD_WAIT;
            g.ld_changed = ec_fs_changed();
            return;
        }
        if (r > 0) {
            g.ld_at += (uint32_t)r;
            return;                                 /* (the rest; 0 = the end) */
        }
        if (r == 0 && ec_project_check(g_pack, g.ld_at) > 0) {
            load_done(g.ld_at);
            return;
        }
        if (!g.ld_tmp) {                            /* NAME.ecp missing or broken: the hidden copy */
            g.ld_tmp = 1;
            g.ld_at = 0;
            return;
        }
        load_fail(r < 0 ? r : FS_EIO);
    }
}

void ec_pf_open(const char *name)
{
    if (!name || !name[0])
        return;
    if (g.ld != LD_IDLE || ec_rec_busy()) {
        ec_app_alert("BUSY: TRY AGAIN");
        return;
    }
    snprintf(g.open_name, sizeof g.open_name, "%s", name);
    if (A->dirty || g.sv != SV_IDLE) {              /* the edits are saved first, then it opens */
        if (A->dirty)
            ec_pf_save(1);
        g.open_after = 1;
        ec_app_message("SAVING");
        return;
    }
    ec_app_message("LOADING");
    load_start(name, 0);
}

/* ------------------------------------------------------------------------------------------- the app --- */
/* ------------------------------------------------------------------------------------------- PROJECTS --- */
/* 0.6.42 (owner): SAVE +1: the current project written under a new name and from now on that file's */
void ec_pf_save_as(const char *name, const char *done)
{
    if (!name || !name[0])
        return;
    if (g.sv != SV_IDLE || g.ld != LD_IDLE) {
        ec_app_alert("BUSY: TRY AGAIN");
        return;
    }
    snprintf(g.name, sizeof g.name, "%s", name);
    snprintf(g.as_msg, sizeof g.as_msg, "%s", done ? done : "SAVED");
    g.as = 1;
    g.disk_known = 0;                               /* (a new file, or one the user chose to write over) */
    g.conflict = 0;
    g.tmp_good = 0;
    ec_pf_save(0);
}

/* 0.6.45 (owner): PROJECTS' DELETE: NAME.ecp and its hidden copy; the open project deleted stays open, unsaved (its
 * next save makes a new file) */
void ec_pf_delete(const char *name)
{
    if (!name || !name[0])
        return;
    if (g.dstep || g.sv != SV_IDLE || g.ld != LD_IDLE) {
        ec_app_alert("BUSY: TRY AGAIN");
        return;
    }
    snprintf(g.dname, sizeof g.dname, "%s", name);
    g.dstep = 1;
    if (!strcmp(name, g.name)) {
        g.name[0] = 0;
        g.disk_known = 0;
        g.conflict = 0;
        g.tmp_good = 0;
        A->dirty = 1;                               /* (EDITED: not in any file now) */
    }
}

static void delete_tick(void)
{
    char path[96];
    if (!g.dstep)
        return;
    if (g.dreq.state == EC_FSR_IDLE) {
        path_of(path, sizeof path, g.dname, g.dstep == 2);
        ec_fs_start(&g.dreq, FS_DELETE, path, 0, 0, 0);
    }
    if (ec_fs_update(&g.dreq) != EC_FSR_DONE)
        return;
    g.dreq.state = EC_FSR_IDLE;
    if (g.dstep == 1 && g.dreq.result < 0 && g.dreq.result != FS_ENOENT) {
        say_error("NOT DELETED", g.dreq.result);
        g.dstep = 0;
        return;
    }
    if (g.dstep == 1) {
        g.dstep = 2;                                /* (the hidden copy next: gone or never there, either is fine) */
        return;
    }
    g.dstep = 0;
    {
        char m[48];
        snprintf(m, sizeof m, "DELETED %s", g.dname);
        ec_app_message(m);
    }
    ec_pf_list_refresh();
}

int ec_pf_dirty(void) { return A->dirty || g.sv != SV_IDLE; }

/* a new project: the current one's edits saved first, then an empty project (the folder's first file on T1) under the
 * next free name, written as soon as that sample has landed */
static void new_now(void)
{
    replace(0);                                     /* (n 0: the load fails, so an empty project) */
    g.name[0] = 0;
    g.disk_known = 0;
    g.conflict = 0;
    g.tmp_good = 0;
    g.from_tmp = 0;
    ec_app_fresh_project();
    g.save_new = 1;
    ec_app_message("NEW PROJECT");
    ec_app_goto(EC_CAT_SRC, ec_src_loop_page());
}

void ec_pf_new(void)
{
    if (g.ld != LD_IDLE || ec_rec_busy()) {
        ec_app_alert("BUSY: TRY AGAIN");
        return;
    }
    if (A->dirty)
        ec_pf_save(1);
    if (g.sv != SV_IDLE)
        g.new_after = 1;                            /* (after the save, ec_pf_tick) */
    else
        new_now();
}

/* the card: the file's first 64 KB read (a project is ~7 KB), ec_project_peek */
void ec_pf_card_want(const char *name)
{
    if (!name || !strcmp(name, g.kwant))
        return;
    snprintf(g.kwant, sizeof g.kwant, "%s", name);
}

const ec_project_card_t *ec_pf_card(const char *name)
{
    return g.kstate == 2 && name && !strcmp(name, g.kname) ? &g.card : 0;
}

static void card_tick(void)
{
    char path[96];
    if (g.kstate != 1) {
        if (!g.kwant[0] || (g.kstate == 2 && !strcmp(g.kwant, g.kname)))
            return;
        snprintf(g.kname, sizeof g.kname, "%s", g.kwant);
        g.kstate = 1;
        g.kat = 0;
    }
    if (g.kreq.state == EC_FSR_IDLE) {
        snprintf(path, sizeof path, "%s/%s%s", EC_PF_DIR, g.kname, EC_PF_EXT);
        ec_fs_start(&g.kreq, FS_READ, path, (int64_t)g.kat, g_kbuf + g.kat, (int32_t)(sizeof g_kbuf - g.kat));
    }
    if (ec_fs_update(&g.kreq) != EC_FSR_DONE)
        return;
    g.kreq.state = EC_FSR_IDLE;
    if (g.kreq.result > 0 && g.kat + (uint32_t)g.kreq.result < sizeof g_kbuf) {
        g.kat += (uint32_t)g.kreq.result;
        return;                                     /* (the rest; 0 = the end) */
    }
    if (g.kreq.result > 0)
        g.kat += (uint32_t)g.kreq.result;
    if (ec_project_peek(g_kbuf, g.kat, &g.card) != 0)
        memset(&g.card, 0, sizeof g.card);
    g.kstate = 2;
    A->redraw = 1;
}

int ec_pf_boot(void)
{
    uint32_t size, magic;
    const uint8_t *rec = ec_plat_record(&size);
    char name[EC_PF_NAME];
    ec_pf_shutdown();
    memset(&g, 0, sizeof g);
    g_nnames = 0;
    memcpy(&magic, rec, 4);
    if (magic == EC_PROJECT_MAGIC)
        return EC_PF_BOOT_FLASH;                    /* 0.6.0 or older: loads once, the next save makes the file */
    if (flash_name(name)) {
        load_start(name, 1);
        return EC_PF_BOOT_FILE;
    }
    g_last_read = 1;                                /* 0.6.31: the folder may know (it lands from ec_pf_tick) */
    return EC_PF_BOOT_NONE;
}

void ec_pf_tick(void)
{
    list_tick();
    save_tick();
    load_tick();
    last_tick();
    card_tick();
    delete_tick();
    if (g.new_after && g.sv == SV_IDLE && g.ld == LD_IDLE) {   /* 0.6.42: NEW, after the edits' save */
        g.new_after = 0;
        new_now();
    }
    if (g.save_new && !A->fresh && !A->scanning && A->load_slot < 0 && g.sv == SV_IDLE) {   /* the new project's file */
        g.save_new = 0;
        ec_pf_save(1);
    }
    if (g.open_after && g.sv == SV_IDLE && g.ld == LD_IDLE) {
        g.open_after = 0;
        if (A->dirty)                               /* (edited again meanwhile) */
            ec_pf_save(1), g.open_after = 1;
        else {
            ec_app_message("LOADING");
            load_start(g.open_name, 0);
        }
    }
}

void ec_pf_shutdown(void)
{
    ec_fs_cancel(&g.req);
    ec_fs_cancel(&g.lreq);
    ec_fs_cancel(&g_nreq);
    ec_fs_cancel(&g.kreq);
    ec_fs_cancel(&g.dreq);
    g_last_read = g_last_write = g_last_wait = 0;
    g_last_written[0] = 0;
}

int ec_pf_saving(void) { return g.sv != SV_IDLE; }
int ec_pf_loading(void) { return g.ld != LD_IDLE; }
const char *ec_pf_name(void) { return g.name; }
const uint8_t *ec_pf_pack(uint32_t *n)
{
    if (n)
        *n = g.len;
    return g_pack;
}
int ec_pf_from_tmp(void) { return g.from_tmp; }
