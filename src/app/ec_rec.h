/* SPDX-License-Identifier: GPL-3.0-only */
/* E6: sampling (src/app/ec_rec.c): the REC pop-up's recorder. A take of the live input (the host's audio input, FM1VST
 * docs/VIRTUAL-CARTS.md 8) or of the master out (resample) goes straight into the sample store's arena, int16 stereo,
 * sample-accurate in the audio call; it starts on PLAY, on the next bar or at once / at a threshold, runs a length in
 * bars (or until stopped) and lands in a pool slot the moment it ends (the track plays it at once), then is written as
 * a WAV to /data/rec/ over the file API (FS_WRITE in pieces of at most 1 MiB, from the tick, never the audio) and its
 * path kept in the project, so it reloads like any sample. A synced take of the input is shifted by the host's
 * latency_frames (the round trip, as every host
 * reports it), so what was played on step 1 lands on its frame 0.
 * docs/FM1-CONTROLS.md "REC (sampling)" is the player's reference. ec_app.c calls the hooks below. */
#ifndef EC_REC_H
#define EC_REC_H
#include <stdint.h>

#include "ec_fs.h"

enum { EC_RS_IDLE, EC_RS_ARMED, EC_RS_RUN, EC_RS_STOPPED };   /* STOPPED: the audio ended it, the tick lands it */
enum { EC_RSRC_IN, EC_RSRC_MST, EC_NRSRC };                     /* the source: the live input, the master out */
enum { EC_RSYNC_PLAY, EC_RSYNC_BAR, EC_RSYNC_FREE, EC_NRSYNC };  /* when an armed take starts */
#define EC_REC_NLEN 6       /* LEN: FREE, 1, 2, 4, 8, 16 bars */
#define EC_REC_NTHR 11      /* THR: OFF, -60 .. -6 dB in 6 dB steps (FREE sync only) */
#define EC_REC_MIN_FRAMES 64

typedef struct {
    /* the pop-up and its settings (runtime: not in the project) */
    int open;
    int src, len, thr, sync;
    int slot;                    /* the target pool slot 0..15 */
    int slot_set;                /* SELECT moved it (else it follows the track's slot) */
    /* the take */
    int state;                   /* EC_RS_* */
    int handle;                  /* the store handle it fills (-1) */
    uint32_t cap, n, target;     /* frames reserved, taken, wanted (0: until stopped) */
    uint64_t start_at;           /* the sample time (ec_seq_t now) of its frame 0 */
    uint32_t shift;              /* the latency shift applied (frames) */
    int was_playing;
    int stop_req, discard_req, now_req;
    int full;                    /* it ended at the reserve's end */
    uint32_t host_block;         /* the host's audio block (frames of the last ec_app_audio; not in the shift) */
    /* the input meters (the raw input, before any track) */
    float in_pk[2], in_meter[2];
    /* the last take */
    int last_slot;
    uint32_t last_frames, takes;
    /* the writer: /data/rec/rec-NNNN.wav */
    int w;                       /* 0 idle, else its step */
    ec_fsreq_t req;
    uint8_t stat[16];
    uint8_t hdr[44];
    int whandle, wslot, wnum;
    uint32_t wdone, wtotal;
    char wpath[48];
    int saved, wfail, wmkdata;
    char werr[24];
} ec_rec_t;

void ec_rec_init(void);
void ec_rec_open(void);                     /* REC tapped with no scope: the pop-up */
void ec_rec_close(void);                    /* HOME (a take goes on) */
int ec_rec_button(int b);                   /* the pop-up's buttons (REC, OCT-, OCT+); 1 = used */
int ec_rec_knob(int role, int det);         /* the pop-up's knobs (SELECT, K1..K4); 1 = used */
void ec_rec_tick(void);                     /* lands an ended take, runs the writer */
void ec_rec_frame(void);                    /* the meters' decay */
/* the audio call (ec_app_audio): per rendered chunk, before (the sync points) and after (the capture). in: this
 * chunk's input (in_n frames of it, the rest silence; NULL none), out: the chunk as it goes out */
void ec_rec_pre(uint32_t n);
void ec_rec_post(const int16_t *in, uint32_t in_n, const int16_t *out, uint32_t n);

int ec_rec_busy(void);                      /* armed, taking or saving: the store must stay as it is */
int ec_rec_holds(int h);                    /* the recorder fills or saves this store handle */
uint32_t ec_rec_reserve(void);              /* the frames an armed take would reserve now (LEN, the free arena) */
uint32_t ec_rec_len_frames(int len, float bpm);   /* LEN's frames at bpm (0: FREE) */
int ec_rec_thr_db(int thr);                 /* THR's dB (0: OFF) */
const char *ec_rec_state_name(void);        /* idle armed rec stopped */
void ec_rec_labels(void);                   /* the pop-up's live panel labels (ec_ux.c, while open) */
void ec_rec_ui_info(char *buf, uint32_t n);

/* what the recorder needs from ec_app.c */
void ec_app_slot_take(int s, int h);        /* a ready store handle into pool slot s (the track plays it at once) */
int ec_app_handle_used(int h);              /* some slot plays it */
void ec_app_message(const char *s);
void ec_app_mark_dirty(void);

#endif
