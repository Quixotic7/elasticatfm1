/* SPDX-License-Identifier: GPL-3.0-only */
/* 0.6.1 (docs/UX-V2.md §9 "Projects are files"): the project is a file in the cart's own folder, /data/projects/,
 * written and read through the async file API (ec_fs.h), never blocking:
 *   - a save packs the project (the version 2 record, ec_project_save: magic, length, FNV-1a checksum) into one static
 *     buffer, then writes it in EC_PF_CHUNK pieces, one request a tick: first the hidden copy /data/projects/.NAME.ecp,
 *     then NAME.ecp, then deletes the hidden copy. fs1 has no rename, so at every moment one copy is whole: a failed
 *     write leaves the other (a load takes NAME.ecp when its checksum holds, else the hidden copy, and the next save
 *     writes the broken one first). The header says SAVING until it lands (an explicit save), the error in ALERT.
 *   - the flash image keeps the APP SETTINGS record (ec_appset.c) and, in the record area, the last project's name
 *     ("ECLP"), which loads at power-on. A 0.6.0 (or older) project record ("ECPJ") there still loads once; the next
 *     save writes it as a file and the name record replaces it. With no folder at all (FS_ENOFOLDER) and no name yet,
 *     a save falls back to the flash record as in 0.6.0.
 *   - a new project gets the lowest free name PROJ-001, PROJ-002 .. at its first save.
 *   - FILE's browser lists /data/projects/ (ec_file.c: the top level holds AUDIO/ and PROJECTS/); LOAD opens one
 *     (the current project saved first when it has unsaved edits). */
#ifndef EC_PROJFILE_H
#define EC_PROJFILE_H
#include <stdint.h>

#include "ec_project.h"

#define EC_PF_DIR "/data/projects"
#define EC_PF_EXT ".ecp"
#define EC_PF_NAME 32                 /* a project's name (the file's, without EC_PF_EXT) + NUL */
#define EC_PF_CHUNK (256u * 1024u)    /* bytes a write request carries (fs1 allows 1 MiB) */
#define EC_PF_LIST_MAX 512            /* project files the browser lists */
/* the pack buffer: the largest version 2 record (every step of 64 patterns x 6 tracks x 256 steps with 16 f32 locks
 * and all 32 slices, every param, slice program and scene value set: 11 250 336 bytes, host/test_proj.c builds it) */
#define EC_PF_PACK_MAX (11u * 1024u * 1024u)

enum { EC_PF_BOOT_NONE, EC_PF_BOOT_FLASH, EC_PF_BOOT_FILE };

/* power-on (ec_app_init): the flash record's kind. EC_PF_BOOT_FLASH: an old project record (the app loads it);
 * EC_PF_BOOT_FILE: a name: the file's read has started (the project lands from ec_pf_tick) */
int ec_pf_boot(void);
void ec_pf_tick(void);
void ec_pf_shutdown(void);
/* save the project (quiet: no SAVING / SAVED, errors still shown). A save in flight: it runs again after */
void ec_pf_save(int quiet);
/* LOAD from the browser: open /data/projects/<name>.ecp (FILE is stopped) */
void ec_pf_open(const char *name);
int ec_pf_saving(void);
int ec_pf_loading(void);
const char *ec_pf_name(void);         /* the current project's name, "" until its first save */

/* the browser's list of /data/projects (names without EC_PF_EXT, sorted) */
void ec_pf_list_refresh(void);
int ec_pf_list_count(void);
const char *ec_pf_list_name(int i);
int ec_pf_list_busy(void);
/* 0.6.42 (owner): PROJECTS' list (0.6.45: by name, the numbers naturally); the actions; the card */
int ec_pf_list_find(const char *name);           /* its row, -1 */
void ec_pf_save_as(const char *name, const char *done);   /* SAVE +1 / over another: the project written as that file,
                                                             * now its own; done: the header's message */
void ec_pf_name_plus(const char *base, char *out);   /* "NAME 2", "NAME 3" ..: the first one not in the list */
void ec_pf_rename(const char *old, const char *nw);   /* 0.6.47: old NULL / "": the open project */
void ec_pf_unique(const char *base, char *out);      /* 0.6.47: base, or base + the next free number */
void ec_pf_delete(const char *name);           /* 0.6.45: the file and its hidden copy */
void ec_pf_new(void);                           /* a new project (saved first: the current one's edits) */
int ec_pf_busy_new(void);                       /* 0.6.46: a NEW still landing (its sample, its file) */
int ec_pf_dirty(void);                          /* edits not written yet (or a save in flight) */
void ec_pf_card_want(const char *name);         /* read that file's card (ec_project_peek) */
const ec_project_card_t *ec_pf_card(const char *name);   /* NULL until it is read */

/* tests */
const uint8_t *ec_pf_pack(uint32_t *n);   /* the last save's packed record */
int ec_pf_from_tmp(void);                 /* 1: the last load took the hidden copy (NAME.ecp was broken) */

#endif
