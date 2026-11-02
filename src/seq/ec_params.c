/* SPDX-License-Identifier: GPL-3.0-only */
/* The parameter table (lib/tracks/params_spec.lua, ec_params.h) and the SPEC -> engine xforms */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ec_params.h"

static const char *const O_MACH[] = {"LOOP", "TRIG", "SLICE", "RAZOR", "S.POLY", "R.POLY", "NEIGH", "INPUT"};
static const char *const O_WARP[] = {"TAPE", "VARI", "DOMINO", "GRAIN", "OLA", "PITCH", "HARM",
                                     "WTBL", "FREEZE", "FORMNT", "GSTR", "GSTR2", "CHOP"};
static const char *const WARP_LONG[] = {"TAPE", "TEMPO VARISPEED", "DOMINO", "GRANULAR", "RANDOM OLA",
                                        "PITCH CORRECT", "HARMONIZER", "WAVETABLE", "SPECTRAL FREEZE",
                                        "FORMANT", "GSTRETCH", "GSTRETCH 2", "CHOPPED"};
static const char *const O_CHOPLOOP[] = {"CHOP", "CHOP R", "LOOP", "LOOP R", "PONG", "PONG R", "RUN", "RUN R"};
static const char *const O_SLICEPLAY[] = {"1SHOT", "HOLD", "LOOP", "CONT", "PONG", "CLOOP"};
static const char *const O_RELEASE[] = {"RETURN", "BOOMR", "RESET"};
static const char *const O_COND[] = {"--", "1:2", "2:2", "1:3", "2:3", "3:3", "1:4", "2:4", "3:4", "4:4",
                                     "FILL", "!FILL", "PRE", "!PRE", "NEI", "!NEI", "1ST"};
static const char *const O_FILT[] = {"CLASSC", "MORPH", "LADDER", "COMB", "FORMNT", "SVF"};
static const char *const O_FTYPE[] = {"LP", "HP", "BP", "NOTCH"};
static const char *const O_MIXMODE[] = {"STEREO", "M/S"};
static const char *const O_ENVMODE[] = {"ADSR", "AHR"};
static const char *const O_ENVRANGE[] = {"1S", "4S", "8S", "16S", "32S", "64S", "128S", "256S", "512S", "1024S"};
/* E4: lib/fx_modes/registry.lua (DRIVE was removed there 2026-08-01: the 0.1.0 list is replaced) */
static const char *const O_FX[] = {"NONE",  "DELAY",  "REVERB", "LOFI",   "COMP",   "DESTRY",
                                   "ECHO",  "B.HOLE", "CHORUS", "FLANGR", "PHASER", "DJ EQ",
                                   "DUCK",  "T.ECHO", "CASSET", "MOTION", "RINGS",  "LIMIT"};
static const char *const O_DTIME[] = {"1/32", "1/16T", "1/16", "1/8T", "1/16D", "1/8", "1/4T", "1/8D",
                                      "1/4",  "1/2T",  "1/4D", "1/2",  "1/2D",  "1BAR", "2BAR"};
static const char *const O_MRATE[] = {"1/32", "1/16T", "1/16", "1/8T", "1/16D", "1/8",  "1/4T", "1/8D",
                                      "1/4",  "1/2T",  "1/4D", "1/2",  "1/2D",  "1BAR", "2BAR", "4BAR"};
static const char *const O_STAGES[] = {"1", "2", "3", "4", "5", "6", "7", "8"};
static const char *const O_ECHOMODE[] = {"STEREO", "PING"};
static const char *const O_MSHAPE[] = {"SINE", "TRI", "SQR"};
static const char *const O_RMODE[] = {"RING", "SHIFT"};
static const char *const O_TAP[] = {"PRE", "POST"};
static const char *const O_DEST[] = {"OFF", "PITCH", "CUT", "RES", "AMP", "PAN", "M1", "M2", "M3", "M4"};
static const char *const O_WAVE[] = {"SINE", "TRI", "SAW", "RSAW", "SQR", "RAND"};
static const char *const O_SPEED[] = {"8BAR", "4BAR", "2BAR", "1BAR", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64"};
static const char *const O_LFOMODE[] = {"FREE", "TRIG", "ONE", "HOLD"};
static const char *const O_POLY[] = {"MONO", "POLY"};

#define L EC_PF_LOCKABLE
#define NUM(n, lb, mn, mx, st, df, fm, fl) {n, lb, mn, mx, st, df, EC_PK_NUM, fm, fl, 0, 0}
#define OPT(n, lb, df, fl, o) {n, lb, 1, (float)(sizeof o / sizeof o[0]), 1, df, EC_PK_OPT, EC_PF_OPT, fl, \
                               (uint8_t)(sizeof o / sizeof o[0]), o}
#define BIN(n, lb, df, fl) {n, lb, 0, 1, 1, df, EC_PK_BIN, EC_PF_BIN, fl, 0, 0}
/* E4: an FX amount 0..127 / bipolar 0..128 / real-unit param (all p-lockable) */
#define FXA(n, lb, df) NUM(n, lb, 0, 127, 1, df, EC_PF_INT, L | EC_PF_FX)
#define FXB(n, lb, df) NUM(n, lb, 0, 128, 1, df, EC_PF_BIP, L | EC_PF_FX)
#define FXU(n, lb, df) NUM(n, lb, 0, 127, 1, df, EC_PF_UNIT, L | EC_PF_FX)
#define FXO(n, lb, df, o) OPT(n, lb, df, L | EC_PF_FX, o)

const ec_param_def_t EC_PARAMS[EC_NP] = {
    [EC_P_MUTE] = BIN("mute", "MUTE", 0, 0),
    [EC_P_MACHINE] = OPT("machine", "MACH", 1, 0, O_MACH),
    [EC_P_PITCH] = NUM("pitch", "P/T", -24, 24, 1, 0, EC_PF_PITCH, L),
    [EC_P_SLOT] = NUM("sample_slot", "SLOT", 1, 16, 1, 1, EC_PF_INT, 0),
    [EC_P_LOOP_START] = NUM("loop_start", "STRT", 0, 128, 1, 0, EC_PF_INT, L | EC_PF_REGION),
    [EC_P_LOOP_END] = NUM("loop_end", "END", 0, 128, 1, 128, EC_PF_INT, L | EC_PF_REGION),
    [EC_P_RANGE_START] = NUM("range_start", "R-ST", 0, 128, 1, 0, EC_PF_INT, L | EC_PF_REGION),
    [EC_P_RANGE_END] = NUM("range_end", "R-EN", 0, 128, 1, 128, EC_PF_INT, L | EC_PF_REGION),
    [EC_P_LOOP_REVERSE] = BIN("loop_reverse", "REV", 0, L),
    [EC_P_XFADE] = NUM("xfade", "XFAD", 0, 0.25f, 0.001f, 0.010f, EC_PF_MS, L),
    [EC_P_MODE] = OPT("mode", "WARP", 1, 0, O_WARP),
    [EC_P_MODE_MACRO] = NUM("mode_macro", "MACR", 0, 1, 0.01f, 0, EC_PF_PCT01, L),
    [EC_P_CHOP_STEPS] = NUM("chop_steps", "CHOP", 1, 64, 1, 16, EC_PF_INT, L),
    [EC_P_CHOP_SLICE_LEN] = NUM("chop_slice_len", "SLEN", 0.05f, 32, 0.05f, 1, EC_PF_DEC2, L),
    [EC_P_CHOP_LOOP_MODE] = OPT("chop_loop_mode", "LOOP", 3, L, O_CHOPLOOP),
    [EC_P_CHOP_ATTACK] = NUM("chop_attack", "ATK", 0.0001f, 0.2f, 0.0005f, 0.002f, EC_PF_MS, L),
    [EC_P_CHOP_HOLD] = NUM("chop_hold", "GATE", 0, 1, 0.01f, 0.9f, EC_PF_PCT01, L),
    [EC_P_CHOP_RELEASE] = NUM("chop_release", "REL", 0.0001f, 0.2f, 0.0005f, 0.01f, EC_PF_MS, L),
    [EC_P_GRAIN_SIZE] = NUM("grain_size", "GSIZ", 0.002f, 0.5f, 0.002f, 0.08f, EC_PF_MS, L),
    [EC_P_GRAIN_DENSITY] = NUM("grain_density", "GDEN", 1, 64, 1, 8, EC_PF_INT, L),
    [EC_P_GRAIN_JITTER] = NUM("grain_jitter", "SPRD", 0, 0.25f, 0.001f, 0.01f, EC_PF_MS, L),
    [EC_P_GRAIN_SPEED] = NUM("grain_speed", "GSPD", 0, 4, 0.05f, 1, EC_PF_X, L),
    [EC_P_GRAIN_SPEED_RAND] = NUM("grain_speed_rand", "RAND", 0, 1, 0.01f, 0, EC_PF_PCT01, L),
    [EC_P_GRAIN_DIRECTION] = NUM("grain_direction", "DIR", 0, 1, 0.01f, 1, EC_PF_PCT01, L),
    [EC_P_WARP_RATE] = NUM("warp_rate", "RATE", 0.0625f, 8, 0.01f, 1, EC_PF_X, L),
    [EC_P_SLICE_COUNT] = NUM("slice_count", "CNT", 1, 32, 1, 16, EC_PF_INT, L),
    [EC_P_SLICE_INDEX] = NUM("slice_index", "SLIC", 1, 32, 1, 1, EC_PF_INT, L),
    [EC_P_SLICE_PLAY_MODE] = OPT("slice_play_mode", "PLAY", 1, L, O_SLICEPLAY),
    [EC_P_SLICE_REVERSE] = BIN("slice_reverse", "SREV", 0, L),
    [EC_P_SLICE_RATCHET] = NUM("slice_ratchet", "RTCH", 1, 4, 1, 1, EC_PF_INT, L | EC_PF_SEQ),
    [EC_P_SLICE_SYNC] = BIN("slice_sync", "SYNC", 1, 0),
    [EC_P_SLICE_RATE] = NUM("slice_rate", "RATE", 0.125f, 8, 0.01f, 1, EC_PF_X, L),
    [EC_P_SLICE_HOLD_TO_STEP] = BIN("slice_hold_to_step", "SHLD", 1, 0),
    [EC_P_SLICE_HOLD] = NUM("slice_hold", "HOLD", 0, 4, 0.01f, 0.25f, EC_PF_SEC, 0),
    [EC_P_PATTERN_STEPS] = NUM("pattern_steps", "LEN", 1, 256, 1, 16, EC_PF_STEPS, 0),
    [EC_P_TRIG_JUMP] = BIN("trig_jump", "JUMP", 1, L | EC_PF_SEQ),
    [EC_P_TRIG_RELEASE] = OPT("trig_release", "RLSE", 1, L | EC_PF_SEQ, O_RELEASE),
    [EC_P_TRIG_CHANCE] = NUM("trig_chance", "CHNC", 0, 100, 1, 100, EC_PF_PCT100, L | EC_PF_SEQ),
    [EC_P_TRIG_CONDITION] = OPT("trig_condition", "COND", 1, L | EC_PF_SEQ, O_COND),
    [EC_P_TRIG_RATCHET] = NUM("trig_ratchet", "RTCH", 1, 8, 1, 1, EC_PF_INT, L | EC_PF_SEQ),
    [EC_P_ENV_RESET] = BIN("env_reset", "ERST", 1, L | EC_PF_SEQ),
    [EC_P_LFO_RESET] = BIN("lfo_reset", "LRST", 1, L | EC_PF_SEQ),
    [EC_P_FILTER_RESET] = BIN("filter_reset", "FRST", 1, L | EC_PF_SEQ),
    [EC_P_FILTER_MACHINE] = OPT("filter_machine", "MACH", 1, 0, O_FILT),
    [EC_P_FILTER_TYPE] = OPT("filter_type", "TYPE", 1, L, O_FTYPE),
    [EC_P_FILTER_CUTOFF] = NUM("filter_cutoff", "CUT", 0, 127, 1, 127, EC_PF_HZ, L),
    [EC_P_FILTER_RES] = NUM("filter_res", "RES", 0, 127, 1, 0, EC_PF_INT, L),
    [EC_P_FILTER_DRIVE] = NUM("filter_drive", "DRIV", 0, 127, 1, 0, EC_PF_INT, L),
    [EC_P_FILTER_MORPH] = NUM("filter_morph", "MRPH", 0, 128, 1, 64, EC_PF_INT, L),
    [EC_P_FILTER_BALANCE] = NUM("filter_balance", "BAL", 0, 128, 1, 64, EC_PF_BIP, L),
    [EC_P_FILTER_MIX] = NUM("filter_mix", "MIX", 0, 127, 1, 127, EC_PF_INT, L),
    [EC_P_FILTER_MIX_MODE] = OPT("filter_mix_mode", "MODE", 1, 0, O_MIXMODE),
    [EC_P_FENV_MODE] = OPT("filter_env_mode", "FENV", 2, 0, O_ENVMODE),
    [EC_P_FENV_ATTACK] = NUM("filter_env_attack", "ATK", 0, 127, 1, 0, EC_PF_ENV, L),
    [EC_P_FENV_DECAY] = NUM("filter_env_decay", "DEC", 0, 127, 1, 64, EC_PF_ENV, L),
    [EC_P_FENV_SUSTAIN] = NUM("filter_env_sustain", "SUS", 0, 127, 1, 100, EC_PF_INT, L),
    [EC_P_FENV_RELEASE] = NUM("filter_env_release", "REL", 0, 128, 1, 0, EC_PF_ENV, L),
    [EC_P_FENV_HOLD] = NUM("filter_env_hold", "HOLD", 0, 128, 1, 128, EC_PF_ENV, L),
    [EC_P_FENV_DEPTH] = NUM("filter_env_depth", "DPTH", 0, 128, 1, 64, EC_PF_BIP, L),
    [EC_P_ENV_MODE] = OPT("env_mode", "ENV", 2, 0, O_ENVMODE),
    [EC_P_ENV_ATTACK] = NUM("env_attack", "ATCK", 0, 127, 1, 0, EC_PF_ENV, L),
    [EC_P_ENV_DECAY] = NUM("env_decay", "DECAY", 0, 127, 1, 64, EC_PF_ENV, L),
    [EC_P_ENV_SUSTAIN] = NUM("env_sustain", "SUST", 0, 127, 1, 100, EC_PF_INT, L),
    [EC_P_ENV_RELEASE] = NUM("env_release", "REL", 0, 128, 1, 128, EC_PF_ENV, L),
    [EC_P_ENV_HOLD] = NUM("env_hold", "HOLD", 0, 128, 1, 128, EC_PF_ENV, L),
    [EC_P_ENV_RANGE] = OPT("env_range", "RANGE", 3, 0, O_ENVRANGE),
    [EC_P_FX_MACHINE] = OPT("fx_insert1_machine", "MACH", 1, EC_PF_FX, O_FX),
    [EC_P_FX_DRIVE] = NUM("fx_drive", "DRIV", 0, 127, 1, 0, EC_PF_INT, L | EC_PF_FX),
    [EC_P_FX_MIX] = NUM("fx_mix", "MIX", 0, 127, 1, 64, EC_PF_INT, L | EC_PF_FX),
    [EC_P_SEND_TAP] = OPT("send_tap", "TAP", 1, 0, O_TAP),
    [EC_P_SEND1] = NUM("send1_level", "SND1", 0, 127, 1, 0, EC_PF_INT, L),
    [EC_P_SEND2] = NUM("send2_level", "SND2", 0, 127, 1, 0, EC_PF_INT, L),
    [EC_P_AMP] = NUM("amp", "VOL", 0, 127, 1, 100, EC_PF_INT, L),
    [EC_P_PAN] = NUM("pan", "PAN", 0, 128, 1, 64, EC_PF_PAN, L),
    [EC_P_LFO1_DEST] = OPT("lfo1_dest", "DEST", 1, 0, O_DEST),
    [EC_P_LFO1_WAVE] = OPT("lfo1_wave", "WAVE", 1, 0, O_WAVE),
    [EC_P_LFO1_SPEED] = OPT("lfo1_speed", "SPD", 4, L, O_SPEED),
    [EC_P_LFO1_DEPTH] = NUM("lfo1_depth", "DEP", 0, 128, 1, 64, EC_PF_BIP, L),
    [EC_P_LFO1_MODE] = OPT("lfo1_mode", "MODE", 1, 0, O_LFOMODE),
    [EC_P_LFO2_DEST] = OPT("lfo2_dest", "DEST", 1, 0, O_DEST),
    [EC_P_LFO2_WAVE] = OPT("lfo2_wave", "WAVE", 1, 0, O_WAVE),
    [EC_P_LFO2_SPEED] = OPT("lfo2_speed", "SPD", 4, L, O_SPEED),
    [EC_P_LFO2_DEPTH] = NUM("lfo2_depth", "DEP", 0, 128, 1, 64, EC_PF_BIP, L),
    [EC_P_LFO2_MODE] = OPT("lfo2_mode", "MODE", 1, 0, O_LFOMODE),
    [EC_P_PORTAMENTO] = BIN("portamento", "PORT", 0, 0),
    [EC_P_MODE_SWITCH_FADE] = NUM("mode_switch_fade", "MFAD", 0.001f, 0.25f, 0.001f, 0.05f, EC_PF_MS, 0),
    [EC_P_LOOP_DIVISION] = NUM("loop_division", "LDIV", 2, 32, 2, 16, EC_PF_INT, 0),
    [EC_P_TRIG_POLY] = OPT("trig_polyphony", "POLY", 1, 0, O_POLY),
    [EC_P_LENGTH] = NUM("default_length", "LEN", 0.25f, 16, 0.25f, 1, EC_PF_DEC2, L | EC_PF_MOMENT),
    [EC_P_VELOCITY] = NUM("default_velocity", "VEL", 0, 1, 0.01f, 1, EC_PF_PCT01, L | EC_PF_MOMENT),
    [EC_P_RATE] = NUM("pattern_rate", "RATE", 1, EC_RATES, 1, EC_RATE_UNITY, EC_PF_RATE, 0),
    /* E4: FX */
    [EC_P_DELAY_TIME] = FXO("delay_time", "TIME", 9, O_DTIME),
    [EC_P_DELAY_FEEDBACK] = FXA("delay_feedback", "FBK", 38),
    [EC_P_DELAY_TONE] = FXA("delay_tone", "TONE", 127),
    [EC_P_REVERB_SIZE] = FXA("reverb_size", "SIZE", 64),
    [EC_P_REVERB_DAMP] = FXA("reverb_damp", "DAMP", 64),
    [EC_P_LOFI_BITS] = FXU("lofi_bits", "BITS", 127),
    [EC_P_LOFI_RATE] = FXU("lofi_rate", "RATE", 127),
    [EC_P_COMP_THRESH] = FXU("comp_thresh", "THRS", 38),
    [EC_P_COMP_RATIO] = FXU("comp_ratio", "RATO", 25),
    [EC_P_COMP_ATTACK] = FXU("comp_attack", "ATK", 13),
    [EC_P_COMP_RELEASE] = FXU("comp_release", "REL", 38),
    [EC_P_COMP_MAKEUP] = FXU("comp_makeup", "MKUP", 0),
    [EC_P_DESTROY_WARBLE] = FXA("destroy_warble", "WARB", 0),
    [EC_P_DESTROY_FOLD] = FXA("destroy_fold", "FOLD", 0),
    [EC_P_DESTROY_SAT] = FXA("destroy_sat", "SAT", 0),
    [EC_P_DESTROY_CRUSH] = FXA("destroy_crush", "CRSH", 0),
    [EC_P_DESTROY_SRR] = FXA("destroy_srr", "SRR", 0),
    [EC_P_DESTROY_TONE] = FXA("destroy_tone", "TONE", 127),
    [EC_P_DESTROY_LEVEL] = FXA("destroy_level", "LEVL", 70),
    [EC_P_ECHO_TIME] = FXO("echo_time", "TIME", 9, O_DTIME),
    [EC_P_ECHO_OFFSET] = FXB("echo_offset", "OFST", 64),
    [EC_P_ECHO_MODE] = FXO("echo_mode", "MODE", 1, O_ECHOMODE),
    [EC_P_ECHO_TONE] = FXA("echo_tone", "TONE", 127),
    [EC_P_ECHO_WOBBLE] = FXA("echo_wobble", "WOBL", 0),
    [EC_P_BH_GRAVITY] = FXA("bh_gravity", "GRAV", 64),
    [EC_P_BH_MOD] = FXA("bh_mod", "MOD", 25),
    [EC_P_BH_LOW] = FXA("bh_low", "LOW", 13),
    [EC_P_BH_PREDELAY] = FXA("bh_predelay", "PRE", 13),
    [EC_P_MODFX_RATE] = FXA("modfx_rate", "RATE", 38),
    [EC_P_MODFX_DEPTH] = FXA("modfx_depth", "DPTH", 38),
    [EC_P_MODFX_TONE] = FXA("modfx_tone", "TONE", 102),
    [EC_P_CHORUS_WIDTH] = FXA("chorus_width", "WDTH", 64),
    [EC_P_FLANGER_FEEDBACK] = FXB("flanger_feedback", "FBK", 64),
    [EC_P_PHASER_CENTER] = FXA("phaser_center", "CNTR", 64),
    [EC_P_PHASER_STAGES] = FXO("phaser_stages", "STGS", 4, O_STAGES),
    [EC_P_PHASER_FEEDBACK] = FXA("phaser_feedback", "FBK", 25),
    [EC_P_EQ_LOW] = FXB("eq_low", "LOW", 64),
    [EC_P_EQ_MID] = FXB("eq_mid", "MID", 64),
    [EC_P_EQ_HIGH] = FXB("eq_high", "HIGH", 64),
    [EC_P_EQ_XLOW] = FXU("eq_xlow", "XLOW", 38),
    [EC_P_EQ_XHI] = FXU("eq_xhi", "XHI", 89),
    [EC_P_DUCK_AMOUNT] = FXU("duck_amount", "AMNT", 76),
    [EC_P_DUCK_SENS] = FXA("duck_sens", "SENS", 64),
    [EC_P_DUCK_ATTACK] = FXU("duck_attack", "ATK", 25),
    [EC_P_DUCK_RELEASE] = FXU("duck_release", "REL", 51),
    [EC_P_TAPE_WOW] = FXA("tape_wow", "WOW", 25),
    [EC_P_TAPE_FLUTTER] = FXA("tape_flutter", "FLUT", 15),
    [EC_P_TAPE_SAT] = FXA("tape_sat", "SAT", 30),
    [EC_P_CASS_NOISE] = FXA("cass_noise", "NOIS", 10),
    [EC_P_CASS_CRACKLE] = FXA("cass_crackle", "CRKL", 0),
    [EC_P_CASS_DROP] = FXA("cass_drop", "DROP", 0),
    [EC_P_CASS_TONE] = FXA("cass_tone", "TONE", 80),
    [EC_P_MOTION_WIDTH] = FXB("motion_width", "WDTH", 64),
    [EC_P_MOTION_RATE] = FXO("motion_rate", "RATE", 9, O_MRATE),
    [EC_P_MOTION_TREM] = FXA("motion_trem", "TREM", 0),
    [EC_P_MOTION_PAN] = FXA("motion_pan", "PAN", 0),
    [EC_P_MOTION_SHAPE] = FXO("motion_shape", "SHPE", 1, O_MSHAPE),
    [EC_P_RINGS_MODE] = FXO("rings_mode", "MODE", 1, O_RMODE),
    [EC_P_RINGS_FREQ] = FXB("rings_freq", "FREQ", 96),
    [EC_P_RINGS_FINE] = FXB("rings_fine", "FINE", 64),
    [EC_P_RINGS_FEEDBACK] = FXA("rings_feedback", "FBK", 0),
    [EC_P_LIMIT_GAIN] = FXU("limit_gain", "GAIN", 0),
    [EC_P_LIMIT_CEIL] = FXU("limit_ceil", "CEIL", 124),
    [EC_P_LIMIT_RELEASE] = FXU("limit_release", "REL", 38),
    /* E4: MOD (lib/pages/model.lua: DEST not lockable; times / depths / macro values are) */
    [EC_P_MENV_DEST] = OPT("menv_dest", "DEST", 1, 0, O_DEST),
    [EC_P_MENV_ATTACK] = NUM("menv_attack", "ATCK", 0, 127, 1, 0, EC_PF_ENV, L),
    [EC_P_MENV_DECAY] = NUM("menv_decay", "DECAY", 0, 127, 1, 64, EC_PF_ENV, L),
    [EC_P_MENV_SUSTAIN] = NUM("menv_sustain", "SUST", 0, 127, 1, 100, EC_PF_INT, L),
    [EC_P_MENV_RELEASE] = NUM("menv_release", "REL", 0, 127, 1, 48, EC_PF_ENV, L),
    [EC_P_MENV_DEPTH] = NUM("menv_depth", "DEPTH", 0, 128, 1, 64, EC_PF_BIP, L),
    [EC_P_MVEL_DEST] = OPT("mvel_dest", "DEST", 1, 0, O_DEST),
    [EC_P_MVEL_DEPTH] = NUM("mvel_depth", "DEPTH", 0, 128, 1, 64, EC_PF_BIP, L),
    [EC_P_MACRO1] = NUM("macro1_value", "M1", 0, 127, 1, 0, EC_PF_INT, L),
    [EC_P_MACRO2] = NUM("macro2_value", "M2", 0, 127, 1, 0, EC_PF_INT, L),
    [EC_P_MACRO3] = NUM("macro3_value", "M3", 0, 127, 1, 0, EC_PF_INT, L),
    [EC_P_MACRO4] = NUM("macro4_value", "M4", 0, 127, 1, 0, EC_PF_INT, L),
#define MD(m, d, nm, lb) [EC_P_MACRO_DEPTH + (m)*5 + (d)] = NUM(nm, lb, 0, 128, 1, 64, EC_PF_BIP, 0)
    MD(0, 0, "macro1_pitch_depth", "PTCH"), MD(0, 1, "macro1_cutoff_depth", "CUT"), MD(0, 2, "macro1_res_depth", "RES"),
    MD(0, 3, "macro1_amp_depth", "AMP"), MD(0, 4, "macro1_pan_depth", "PAN"),
    MD(1, 0, "macro2_pitch_depth", "PTCH"), MD(1, 1, "macro2_cutoff_depth", "CUT"), MD(1, 2, "macro2_res_depth", "RES"),
    MD(1, 3, "macro2_amp_depth", "AMP"), MD(1, 4, "macro2_pan_depth", "PAN"),
    MD(2, 0, "macro3_pitch_depth", "PTCH"), MD(2, 1, "macro3_cutoff_depth", "CUT"), MD(2, 2, "macro3_res_depth", "RES"),
    MD(2, 3, "macro3_amp_depth", "AMP"), MD(2, 4, "macro3_pan_depth", "PAN"),
    MD(3, 0, "macro4_pitch_depth", "PTCH"), MD(3, 1, "macro4_cutoff_depth", "CUT"), MD(3, 2, "macro4_res_depth", "RES"),
    MD(3, 3, "macro4_amp_depth", "AMP"), MD(3, 4, "macro4_pan_depth", "PAN"),
#undef MD
};

const uint8_t EC_RATE_FRAC[EC_RATES][2] = {
    {1, 16}, {1, 8}, {1, 6}, {1, 4}, {1, 3}, {3, 8}, {1, 2}, {5, 8}, {2, 3}, {3, 4}, {5, 6},
    {7, 8},  {1, 1}, {7, 6}, {5, 4}, {4, 3}, {3, 2}, {5, 3}, {7, 4}, {11, 6}, {2, 1}};

const ec_trigcond_t EC_TRIGCOND[EC_TRIGCONDS] = {
    {"--", EC_TC_NONE, 0, 0},  {"1:2", EC_TC_AB, 1, 2}, {"2:2", EC_TC_AB, 2, 2}, {"1:3", EC_TC_AB, 1, 3},
    {"2:3", EC_TC_AB, 2, 3},   {"3:3", EC_TC_AB, 3, 3}, {"1:4", EC_TC_AB, 1, 4}, {"2:4", EC_TC_AB, 2, 4},
    {"3:4", EC_TC_AB, 3, 4},   {"4:4", EC_TC_AB, 4, 4}, {"FILL", EC_TC_FILL, 0, 0}, {"!FILL", EC_TC_NFILL, 0, 0},
    {"PRE", EC_TC_PRE, 0, 0},  {"!PRE", EC_TC_NPRE, 0, 0}, {"NEI", EC_TC_NEI, 0, 0}, {"!NEI", EC_TC_NNEI, 0, 0},
    {"1ST", EC_TC_FIRST, 0, 0}};

const char *ec_rate_label(int index)
{
    static const char *const R[EC_RATES] = {"1/16", "1/8", "1/6", "1/4", "1/3", "3/8", "1/2", "5/8", "2/3", "3/4",
                                            "5/6",  "7/8", "1",   "7/6", "5/4", "4/3", "3/2", "5/3", "7/4", "11/6",
                                            "2"};
    return index >= 1 && index <= EC_RATES ? R[index - 1] : "1";
}

const char *ec_machine_name(int m) { return m >= 1 && m <= 8 ? O_MACH[m - 1] : "?"; }
const char *ec_warp_name(int m) { return m >= 1 && m <= 13 ? WARP_LONG[m - 1] : "?"; }
const char *ec_filter_name(int m) { return m >= 1 && m <= 6 ? O_FILT[m - 1] : "?"; }

void ec_params_default(float *v)
{
    int i;
    for (i = 0; i < EC_NP; i++)
        v[i] = EC_PARAMS[i].def;
}

float ec_param_clamp(int id, float x)
{
    const ec_param_def_t *d = &EC_PARAMS[id];
    if (x != x)
        x = d->def;
    if (x < d->min)
        x = d->min;
    if (x > d->max)
        x = d->max;
    if (d->kind != EC_PK_NUM || d->fmt == EC_PF_INT || d->fmt == EC_PF_STEPS || d->fmt == EC_PF_PCT100 ||
        d->fmt == EC_PF_RATE)
        x = floorf(x + 0.5f);
    return x;
}

float ec_param_nudge(int id, float x, int det)
{
    const ec_param_def_t *d = &EC_PARAMS[id];
    float y = x + (float)det * d->step;
    if (d->kind == EC_PK_NUM && d->step < 1.0f) {   /* (snap to the step grid: no float crumbs) */
        const float n = floorf((y - d->min) / d->step + 0.5f);
        y = d->min + n * d->step;
    }
    return ec_param_clamp(id, y);
}

float ec_param_frac(int id, float x)
{
    const ec_param_def_t *d = &EC_PARAMS[id];
    const float r = d->max - d->min;
    float f = r > 0 ? (x - d->min) / r : 0;
    return f < 0 ? 0 : f > 1 ? 1 : f;
}

/* "1.50" -> "1.5", "2.00" -> "2" */
static void trim0(char *s)
{
    char *dot = strchr(s, '.');
    size_t n;
    if (!dot)
        return;
    n = strlen(s);
    while (n > 0 && s[n - 1] == '0')
        s[--n] = 0;
    if (n > 0 && s[n - 1] == '.')
        s[n - 1] = 0;
}

static void fmt_seconds(float s, char *out, int cap)
{
    if (s >= EC_ENV_INF_SECONDS / 10)
        snprintf(out, (size_t)cap, "INF");
    else if (s < 0.9995f) {
        const float ms = s * 1000.0f;
        if (ms < 10.0f && ms > 0.0f && fabsf(ms - floorf(ms + 0.5f)) > 0.05f)
            snprintf(out, (size_t)cap, "%.1fMS", ms);
        else
            snprintf(out, (size_t)cap, "%dMS", (int)(ms + 0.5f));
    } else if (s < 10.0f) {
        char b[16];
        snprintf(b, sizeof b, "%.2f", s);
        trim0(b);
        snprintf(out, (size_t)cap, "%sS", b);
    } else
        snprintf(out, (size_t)cap, "%dS", (int)(s + 0.5f));
}

void ec_param_format(int id, float x, int env_range, char *out, int cap)
{
    const ec_param_def_t *d = &EC_PARAMS[id];
    char b[24];
    switch (d->fmt) {
    case EC_PF_OPT: {
        const int i = (int)(x + 0.5f) - 1;
        snprintf(out, (size_t)cap, "%s", i >= 0 && i < d->nopt ? d->opts[i] : "?");
    } break;
    case EC_PF_BIN: snprintf(out, (size_t)cap, "%s", x > 0.5f ? "ON" : "OFF"); break;
    case EC_PF_PITCH:
        snprintf(b, sizeof b, "%+.1f", (double)x);
        trim0(b);
        if (!strcmp(b, "+0") || !strcmp(b, "-0"))
            snprintf(b, sizeof b, "0");
        snprintf(out, (size_t)cap, "%s", b);
        break;
    case EC_PF_MS: fmt_seconds(x, out, cap); break;
    case EC_PF_SEC: fmt_seconds(x, out, cap); break;
    case EC_PF_ENV: fmt_seconds(ec_xf_env_seconds(x, env_range), out, cap); break;
    case EC_PF_HZ: {
        const float hz = ec_xf_cutoff_hz(x);
        if (hz < 1000.0f)
            snprintf(out, (size_t)cap, "%d", (int)(hz + 0.5f));
        else if (hz < 9950.0f)
            snprintf(out, (size_t)cap, "%.1fK", (double)(hz / 1000.0f));
        else
            snprintf(out, (size_t)cap, "%dK", (int)(hz / 1000.0f + 0.5f));
    } break;
    case EC_PF_BIP: {
        const int v = (int)floorf(x + 0.5f) - 64;
        if (v)
            snprintf(out, (size_t)cap, "%+d", v);
        else
            snprintf(out, (size_t)cap, "0");
    } break;
    case EC_PF_PAN: {
        const int v = (int)floorf(x + 0.5f) - 64;
        if (v < 0)
            snprintf(out, (size_t)cap, "L%d", -v);
        else if (v > 0)
            snprintf(out, (size_t)cap, "R%d", v);
        else
            snprintf(out, (size_t)cap, "C");
    } break;
    case EC_PF_PCT01: snprintf(out, (size_t)cap, "%d%%", (int)floorf(x * 100.0f + 0.5f)); break;
    case EC_PF_PCT100: snprintf(out, (size_t)cap, "%d%%", (int)floorf(x + 0.5f)); break;
    case EC_PF_X:
        snprintf(b, sizeof b, "%.2f", (double)x);
        trim0(b);
        snprintf(out, (size_t)cap, "%sX", b);
        break;
    case EC_PF_DEC1:
        snprintf(b, sizeof b, "%.1f", (double)x);
        trim0(b);
        snprintf(out, (size_t)cap, "%s", b);
        break;
    case EC_PF_DEC2:
        snprintf(b, sizeof b, "%.2f", (double)x);
        trim0(b);
        snprintf(out, (size_t)cap, "%s", b);
        break;
    case EC_PF_RATE: snprintf(out, (size_t)cap, "%s", ec_rate_label((int)(x + 0.5f))); break;
    case EC_PF_UNIT: ec_param_format_unit(id, x, out, cap); break;   /* E4 */
    default: snprintf(out, (size_t)cap, "%d", (int)floorf(x + 0.5f)); break;
    }
}

static int opt(const float *v, int id) { return (int)(v[id] + 0.5f); }

void ec_params_engine(const float *v, const ec_slotmeta_t *slot, int playhead_return, ec_track_params_t *p)
{
    const int er = opt(v, EC_P_ENV_RANGE);
    ec_track_params_default(p);
    p->play = 1;
    p->mute = opt(v, EC_P_MUTE);
    p->machine = opt(v, EC_P_MACHINE);
    p->pitch = v[EC_P_PITCH];
    p->loop_start = v[EC_P_LOOP_START];
    p->loop_end = v[EC_P_LOOP_END];
    p->range_start = v[EC_P_RANGE_START];
    p->range_end = v[EC_P_RANGE_END];
    if (slot) {
        p->trim_start = slot->trim_start;
        p->trim_end = slot->trim_end;
        p->sample_steps = slot->steps > 0 ? slot->steps : 16.0f;
    }
    p->loop_reverse = opt(v, EC_P_LOOP_REVERSE);
    p->xfade = v[EC_P_XFADE];
    p->mode = opt(v, EC_P_MODE);
    p->mode_macro = v[EC_P_MODE_MACRO];
    p->chop_steps = opt(v, EC_P_CHOP_STEPS);
    p->chop_slice_len = v[EC_P_CHOP_SLICE_LEN];
    p->chop_loop_mode = opt(v, EC_P_CHOP_LOOP_MODE);
    p->chop_attack = v[EC_P_CHOP_ATTACK];
    p->chop_hold = v[EC_P_CHOP_HOLD];
    p->chop_release = v[EC_P_CHOP_RELEASE];
    p->grain_size = v[EC_P_GRAIN_SIZE];
    p->grain_density = v[EC_P_GRAIN_DENSITY];
    p->grain_jitter = v[EC_P_GRAIN_JITTER];
    p->grain_speed = v[EC_P_GRAIN_SPEED];
    p->grain_speed_rand = v[EC_P_GRAIN_SPEED_RAND];
    p->grain_direction = v[EC_P_GRAIN_DIRECTION];
    p->warp_rate = v[EC_P_WARP_RATE];
    p->slice_count = opt(v, EC_P_SLICE_COUNT);
    p->slice_play_mode = opt(v, EC_P_SLICE_PLAY_MODE);
    p->slice_reverse = opt(v, EC_P_SLICE_REVERSE);
    p->slice_sync = opt(v, EC_P_SLICE_SYNC);
    p->slice_rate = v[EC_P_SLICE_RATE];
    p->filter_machine = opt(v, EC_P_FILTER_MACHINE);
    p->filter_type = opt(v, EC_P_FILTER_TYPE);
    p->filter_cutoff = ec_xf_cutoff_hz(v[EC_P_FILTER_CUTOFF]);
    p->filter_res = ec_xf_amount(v[EC_P_FILTER_RES]);
    p->filter_drive = ec_xf_amount(v[EC_P_FILTER_DRIVE]);
    p->filter_morph = ec_xf_morph(v[EC_P_FILTER_MORPH]);
    p->filter_balance = ec_xf_bipolar(v[EC_P_FILTER_BALANCE]);
    p->filter_mix = ec_xf_amount(v[EC_P_FILTER_MIX]);
    p->filter_mix_mode = opt(v, EC_P_FILTER_MIX_MODE);
    p->filter_env_mode = opt(v, EC_P_FENV_MODE);
    p->filter_env_attack = ec_xf_env_seconds(v[EC_P_FENV_ATTACK], er);
    p->filter_env_decay = ec_xf_env_seconds(v[EC_P_FENV_DECAY], er);
    p->filter_env_sustain = ec_xf_amount(v[EC_P_FENV_SUSTAIN]);
    p->filter_env_release = ec_xf_env_seconds(v[EC_P_FENV_RELEASE], er);
    p->filter_env_hold = ec_xf_env_seconds(v[EC_P_FENV_HOLD], er);
    p->filter_env_depth = ec_xf_bipolar(v[EC_P_FENV_DEPTH]);
    p->filter_slew = 0.09f;
    p->env_mode = opt(v, EC_P_ENV_MODE);
    p->env_attack = ec_xf_env_seconds(v[EC_P_ENV_ATTACK], er);
    p->env_decay = ec_xf_env_seconds(v[EC_P_ENV_DECAY], er);
    p->env_sustain = ec_xf_amount(v[EC_P_ENV_SUSTAIN]);
    p->env_release = ec_xf_env_seconds(v[EC_P_ENV_RELEASE], er);
    p->env_hold = ec_xf_env_seconds(v[EC_P_ENV_HOLD], er);
    p->amp = ec_xf_amount(v[EC_P_AMP]) * (slot ? slot->gain : 1.0f);
    p->pan = ec_xf_bipolar(v[EC_P_PAN]);
    p->portamento = opt(v, EC_P_PORTAMENTO);
    p->mode_switch_fade = v[EC_P_MODE_SWITCH_FADE];
    p->loop_division = opt(v, EC_P_LOOP_DIVISION);
    p->playhead_return = playhead_return >= 1 && playhead_return <= 3 ? playhead_return : EC_RETURN_BOOMERANG;
    p->send_tap = opt(v, EC_P_SEND_TAP);
    p->send1_level = ec_xf_amount(v[EC_P_SEND1]);
    p->send2_level = ec_xf_amount(v[EC_P_SEND2]);
    ec_params_fx(v, &p->fx);     /* E4 (ec_params_e4.c) */
    ec_params_mod(v, &p->mod);
}
