# src/dsp — the Elasticat FM-1 sound engine

C99 port of the DSP of Elasticat (`../Elasticat/lib/Engine_Elasticat.sc` and the compiled UGens in
`lib/ugens/src/ElasticatUGens.cpp`). Pure C, `<math.h>`/`<stdint.h>`/`<string.h>` only, no allocation:
the caller gives `ec_dsp_mem_size()` bytes (≈ 270 KB) to `ec_dsp_init()`.

The API, the parameter mapping table (params_spec.lua name → `ec_track_params_t` field + xform) and the
event list are documented at the top of **`ec_dsp.h`**.

| file | what |
|---|---|
| `ec_dsp.h` | public API |
| `ec_dsp.c` | init, params, events, region layers + live loop-key scrub, track chain, master |
| `ec_reader.c` | `ElasticatReader` (tape, tempo_varispeed): two heads, one queued equal-power crossfade |
| `ec_grains.c` | `ElasticatGrains` (granular warp) |
| `ec_chop.c` | warp mode `chopped` (`\elasticatChopSync`) |
| `ec_warp3.c/.h` | E3: domino (ElasticatSlicer), random_ola (TGrains), pitch_corrected / gstretch / gstretch2 (PitchShift), harmonizer, wavetable (ElasticatWavetable), spectral_freeze / formant (FFT + PV_*) |
| `ec_fft.c/.h` | E3: fixed 2048-point real FFT (N/2 complex radix-2 + split) |
| `ec_voices.c` | slice voices (raw + Warp1-like grain cloud), voice budget, stealing, choke |
| `ec_filter.c` | filter machines + AMP/PAN stage |
| `ec_env.c` | SC EnvGen segments, ADSR / AHR / gated voice envelopes |
| `ec_internal.h` | internal structs + inline helpers |

Tests, demo renders and the benchmark: `tests/dsp` (`make -C tests/dsp`, `render`, `bench`, `wasm`).

## Implemented (E1 + E2)

- 6 tracks: machine → filter → amp/pan → track mix (mute) → master volume → soft limiter.
- Machines: `loop`, `loop_trig`, `grid_slice` (mono), `slice_poly` (≤ 8 per track, ≤ 24 total, oldest
  stolen, same-slice retrigger steals, choke groups). `razor_slice` / `razor_poly` play like the grid
  machines when the event carries an explicit slice range.
- Warp modes: `tape`, `tempo_varispeed`, `granular`, `chopped`. Every mode applies `warp_rate`.
- Slice play modes: one-shot, hold, loop, continue, ping-pong, continue-loop; slice reverse; clock sync /
  slice rate; per-warp slice voices (raw read; granular → grain cloud).
- Region model: trim → Range → region (map_trim_point inside the engine), the three region layers
  (loop keys > step lock > track), and the region verbs of the Lua (set_region, set_region_with_phase,
  within_or_warp, preserve) with the release modes return / boomerang / reset and their anchors.
- Filters: classic (LP/HP/BP/notch), morphing, ladder, comb, formant, svf; drive, filter envelope
  (depth ±72 st), MSBL (stereo / mid-side + balance ±24 st spread), LeakDC.
- Amp: ADSR / AHR with the Elektron time curve (xform helpers), portamento legato, noteOff for held notes,
  amp ×2.63 makeup + Balance2 pan as in the SC filter stage.

## E3

- Every warp mode: reader family + post stage (pitch_corrected, harmonizer, random_ola, gstretch, gstretch2 on
  the click-free reader; one 64k delay line per track written by every reader mode so a stage switch has history;
  stages crossfade over mode_switch_fade), and the families domino, wavetable, spectral (`ec_warp3.c`).
- `neighbor`: the left track's `track_chunk` output (post filter / amp / pan / mute; E4's insert must run inside
  `track_chunk`, see the "E3 neighbor tap" comment in `ec_dsp_process`) is this track's source; the left track then
  skips the master.
- Slice events: `ev.mode` 1..6 = a per-slice play mode (razor editor), the seq fills razor ranges + choke + pitch.
- Memory: `ec_dsp_mem_size()` is now ~3.6 MB (6 x 512 KB delay lines + FFT state); the app's arena is 4 MiB.
- CPU (`make -C tests/dsp bench`, M-series, native): the 6-track scene in the heaviest E3 modes (harmonizer x3,
  random_ola density 16, spectral_freeze, formant, wavetable LFO 300 Hz, gstretch2) 1.18 % of a core; the E2 busy
  scene with four of its loop tracks in harmonizer / random_ola / freeze / formant 1.30 %; six spectral tracks
  0.75 %; worst-case random_ola on all six tracks (density 64, 0.6 s grains) 1.7 % with the grain budget. wasm3
  ~22x: 26-37 %, i.e. >= 2.7x real time for the DSP.
- Insert FX, sends S1/S2, master insert (E4): marked "E4 hook" in `track_chunk()` / `ec_dsp_process()`;
  `send_tap` / `send1_level` / `send2_level` are stored, unused.
- MOD (LFOs, mod env, macros, velocity mod): the cutoff / res / pitch / amp / pan mod buses are zero.

## Deliberate deviations from the original

1. **Loop seam keeps time.** ElasticatReader restarted the new head exactly at the loop start, dropping
   the overshoot (< 1 frame per pass, so a tempo-fitted loop drifted slowly against the clock). Here the
   overshoot is carried, so tempo_varispeed lands on the boundary sample-exactly forever (tested).
2. **Reverse phase 0** starts at the region end (phase 0 ≡ 1) instead of at the start followed by an
   immediate seam crossfade.
3. **Region jumps that land on the current position are skipped** (no crossfade between two identical
   heads). The Lua estimated the playhead from 1–15 Hz reports; the engine knows it exactly, so
   preserve-position / boomerang / within-or-warp / "return" are sample-exact and cost nothing.
4. **Chopped uses two heads**: a new grain starts on the free head while the previous one releases
   (the SynthDef retriggered one read, which clicked at GATE ≈ 1). The step clock is re-phased on
   transport start (the SynthDef's Impulse free-ran from synth creation).
5. **Ladder** (MoogFF) resonance is capped at k = 3.98 (SC goes to 4.0, marginally stable) so it rings
   hard but can never run away.
6. **Classic filter type changes crossfade** over 256 samples (SC ran LP, HP and BP in parallel and
   switched hard; we compute only the selected type).
7. **Comb**: no block-delay compensation (`dt = 1/f`, the SC LocalIn loop added one control block).
8. **Envelopes / smoothing** run per sample (envelopes) and per ≤ 16-sample control chunk (Lags, filter
   coefficients) instead of SC's 64-sample control blocks. ADSR retrigger restarts from the current level
   without the 1.5-block gate dip.
9. **LeakDC** once per track at the filter output (SC also had one on every reader and voice).
10. **Slice grain voices** (granular warp) are capped at 16 grains per voice (Warp1 allowed up to the
    overlap count, 64).
11. **tempo_varispeed ignores PITCH**, as the SC engine does (`pitch is a byproduct of the tempo stretch`);
    docs/MODE_CATALOG.md still describes an older pitch offset.
12. **Soft limiter** on the master (linear to 0.8, tanh knee to 1.0) — not in the original (it had an
    optional master insert); a safety stage for the s16 output.
13. **Sample swap** hard-stops the track's voices and fades the old reader out with a 128-sample ramp of
    its last value (the new sample cannot read the old memory). The reader restarts at the same relative
    position with a fade-in.
14. Per-track `slice_sync` / `slice_rate` (the SC engine had them engine-global).

E3:

15. **Spectral budget**: the original ran the FFT chain on track 1 only (tape elsewhere). The FFT here is cheap
    (one 2048 real FFT + IFFT per 1024 samples, magnitudes / phases kept as unit vectors, no trig per bin): measured
    0.06 % of a core per track, so `EC_W3_SPECTRAL_TRACKS` = 6 (every track). The hook stays (tracks past it fall
    back to tape and report `mode_implemented = 0`; the WARP page then marks TAPE).
16. **PitchShift window** is live (SC's windowSize is init-rate: the original's PWIN / PCWN only took effect at the
    next mode spawn); it is capped at 0.74 s (pitch_corrected's PCWN x (1 + SMEAR) could ask for 4 s). Both
    channels share one grain schedule (SC ran two independent PitchShift UGens; identical with dispersion 0).
17. **random_ola grain budget**: at most 80 TGrains grains sounding across all tracks (each track's cap stays 64,
    TGrains' own); six dense OLA tracks would otherwise cost ~7 % native (150 % in wasm3).
18. **Loop reverse** also reverses random_ola / pitch_corrected / harmonizer / spectral reads (their SynthDefs read
    forward only); domino's slicer phase follows the region verbs (phase 0 = the window edge, as the original).
19. **Mode switches inside the reader family** (e.g. tape -> harmonizer) crossfade the post stages; the SC engine
    respawned the synth with a modeAmp fade.
20. Slice voices in random_ola / pitch_corrected play the raw voice (the original had \\ola / \\pc voice defs:
    per-voice TGrains / PitchShift would need 24 delay lines).
21. Domino's playhead window follows the resolved region (loop keys / step locks included); the original sent the
    raw loop_start / loop_end.


## For the sequencer (src/seq)

- Write resolved params (p-locks applied) with `ec_dsp_track_params()` between blocks; option params are
  the spec's 1-based option index, region points stay 0..128 track space, trim and sample_steps come from
  the slot.
- Loop machine step: `EC_EV_TRIG` (length = note length in samples; start/end = the step's loop_start /
  loop_end locks or −1; `EC_TF_JUMP` = trig_jump; `mode` = the step's trig_release). When its lock window
  ends (note length expired, or a new mono trig clears holds) send `EC_EV_REGION_RELEASE`.
- Loop keys: forward presses/releases as `EC_EV_LOOP_KEY` (key 1..32, `loop_division` and
  `playhead_return` in the params); the engine does the region, anchor and release logic.
- Slice machines: one `EC_EV_TRIG` per slice (slice index, pitch, velocity, length = 0 for one-shot,
  choke). Ratchets = several events in the step. Live slice keys: `EC_EV_SLICE_ON` / `EC_EV_SLICE_OFF`.
- Transport: `EC_EV_TRANSPORT` (start with `EC_TF_RESET` = enter_step(reset)); stop twice → `EC_EV_PANIC`.
