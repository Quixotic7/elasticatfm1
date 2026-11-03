/* SPDX-License-Identifier: GPL-3.0-only */
/* 0.6.1: the hold menu (docs/UX-V2.md §10, FX "type menu" and LOOP ENGINE "quick settings"): a list of one setting's
 * values that shows only while its black key is held. The key goes down: the menu is armed; held EC_HOLD_MS, or SEL /
 * K1 turned while it is held, it shows (the cursor on the value now); SEL or K1 moves the cursor; letting the key go
 * picks the value under the cursor (nothing changes when the cursor never moved). Let go before it showed: a tap (the
 * caller's page turn). While it shows it is the only thing on the screen below the header, and the header says what
 * the held key does (ec_hm_head). One menu at a time; a mode change or another menu drops it.
 *
 * Users: FX's insert key B1 (the selected slot's effect type, INS 1 / INS 2) and its bus keys B3..B5 (that bus's
 * effect); LOOP ENGINE's B1 LOOP (loop mode), B2 MACHINE, B3 WARP, B4 RANGE (voice mode), B5 ADVANCE. */
#ifndef EC_HOLDMENU_H
#define EC_HOLDMENU_H

enum { EC_HM_PARAM,      /* a track param (an option list): id; FX's INS 1 type on a bus targets the bus's record */
       EC_HM_INS2,       /* INS 2's type (EC_P_INS2_MACHINE: picking resets its MIX and P1..P8, one undo) */
       EC_HM_ADVANCE,    /* KEYBOARD's ADVANCE (APP SETTINGS) */
       EC_HM_SWAP };     /* FX SENDS (0.6.4): no list; held EC_HOLD_MS the key's box is raised, K1 moves it left / right
                          * (each step swaps the pair: id 0 the inserts, 1 the sends); letting go = one undo */

/* arm the menu on black key `key` (1..11): kind, the param (EC_HM_PARAM), the header while it shows */
void ec_hm_arm(int key, int kind, int id, const char *head);
void ec_hm_tick(void);                    /* shows it once the key has been held EC_HOLD_MS */
int ec_hm_knob(int role, int det);        /* 1: the menu took the knob (SEL / K1 move; others swallowed while up) */
int ec_hm_up(int key);                    /* the key let go: 1 = it showed (a value was picked), 0 = a tap / not its key */
void ec_hm_cancel(void);                  /* drop it, nothing picked */
int ec_hm_armed(void);                    /* armed or showing: its key (1..11), 0 none */
int ec_hm_shown(void);                    /* showing */
int ec_hm_raised(void);                   /* FX SENDS: the raised box's place (0..3), -1 none */
const char *ec_hm_head(void);             /* the header text while it shows, NULL otherwise */
int ec_hm_count(void);                    /* its values */
int ec_hm_cursor(void);                   /* 0..count-1 */
int ec_hm_current(void);                  /* the value set now (0..count-1) */
const char *ec_hm_label(int i);           /* value i's name */

#endif
