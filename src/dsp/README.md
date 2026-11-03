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
| `ec_fx.h` / `ec_fx.c` / `ec_fx_state.h` | E4: the 18 FX machines (`\elasticatFx*`), one slot = one insert / send return / master insert |
| `ec_mod.h` / `ec_mod.c` | E4: `\elasticatMod` -- 2 LFOs, mod envelope, velocity, 4 macros x 5 destinations |
| `ec_click.h` / `ec_click.c` | 0.6.0: the metronome click (WOOD / BEEP / CLICK / COWBELL, accent, level), mixed into int16 after the master |
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
- `neighbor`: the left track's `track_chunk` output (post filter, send taps, insert FX, track mix = mute + pan; SC
  `applyNeighborRouting` re-points the left track's \elasticatTrackMix output) is this track's source; the left
  track's sends are fed as usual; a NEIGH track is never paused by the idle saver (SC `idleEligible`); the left track then
  skips the master.
- Slice events: `ev.mode` 1..6 = a per-slice play mode (razor editor), the seq fills razor ranges + choke + pitch.
- Memory: E3's state is ~3.6 MB (6 x 512 KB delay lines + FFT state); see E4 for the merged total.
- MOD (E4) reaches every E3 reader as the SC's `pitchRatio` does: the PITCH bus (+-12 st) is added to the pitch of
  domino, random_ola, pitch_corrected, gstretch(2), wavetable (oscillator freq), spectral (PV_BinShift) and to each
  harmonizer interval (the harmonizer's dry read stays unpitched).
- CPU (`make -C tests/dsp bench`, M-series, native): the 6-track scene in the heaviest E3 modes (harmonizer x3,
  random_ola density 16, spectral_freeze, formant, wavetable LFO 300 Hz, gstretch2) 1.18 % of a core; the E2 busy
  scene with four of its loop tracks in harmonizer / random_ola / freeze / formant 1.30 %; six spectral tracks
  0.75 %; worst-case random_ola on all six tracks (density 64, 0.6 s grains) 1.7 % with the grain budget. wasm3
  ~22x: 26-37 %, i.e. >= 2.7x real time for the DSP.

## E4: FX, sends, master insert, MOD

- **Chain** (`track_chunk`): source -> filter (+ the PITCH / CUTOFF / RES / AMP / PAN mod sums) -> send taps
  (`send_tap` 1 pre / 2 post insert, levels Lag 0.02) -> the insert (`p.fx`) -> the track mix (mute x centre
  Balance2, moved here from the filter stage as \elasticatTrackMix) -> master. Then S1 / S2 returns
  (`ec_dsp_bus_params`), the master insert, the master volume and the soft limiter.
- **Duck key** as the SC `keyIn = masterBus`: a track's insert hears the tracks mixed before it (tracks 1..N-1,
  the node order), S1 / S2 the sum of the six tracks, the master insert its own input.
- **Memory**: the 9 FX slots' delay lines follow the state in the caller's block. Merged 0.2.0 (E3 + E4):
  `ec_dsp_mem_size()` = 17 523 904 bytes (16.7 MiB); `EC_DSP_MEM_MAX` = 18 MiB for static buffers, checked at
  compile time in ec_dsp.c.
- **Machines** (`ec_fx.c`): DELAY / ECHO / TAPE ECHO (cubic DelayC, time Lag 0.15 s), REVERB (SC FreeVerb2,
  line-for-line), BLACKHOLE (predelay + 4 modulated AllpassC + LPF/HPF/LeakDC + SC Limiter in the loop), COMP (SC
  Compander), LIMIT (SC Limiter, 3 buffers), DUCK (Amplitude follower), LOFI / DESTROY (Latch + round, fold2,
  tanh, softclip), CHORUS / FLANGER / PHASER (DelayC / FOS), DJ EQ (LPF + HPF + remainder), CASSETTE (SC PinkNoise,
  Dust2 -> Decay -> BPF), MOTION, RINGS (SC FreqShift's 12-coefficient Hilbert). Swaps fade 10 ms to dry
  (silence on a send), restart, fade in; a send set to NONE returns nothing (spawnSend1).
- **MOD** (`ec_mod.c`): evaluated per engine chunk (<= 16 samples); a track with every depth at 0 computes nothing
  (needsMod). LFO modes FREE / TRIG / ONE / HOLD, waves sine tri saw ramp square S&H; the sequencer sends
  `EC_EV_MOD_TRIG` (lfo_reset / env_reset of the firing step, the mod env gate = the note length); velocity is
  latched from slice trigs (Lag 0.01). Macros: base + LFO/env/velocity routed at them (dest M1..M4), then the 4 x 5
  depth matrix; CUTOFF's LFO/env/macro terms x0.3, velocity at full weight.
- **Idle CPU saver** (`\elasticatTrackMix`'s Amplitude + `idleScan`): a track that is not playing, has no voices
  / reader / audition and whose post-mix level stayed <= 0.001 for 4 s is skipped whole; any event for it, a
  param change, a sample change or the transport start wakes it. FX tails keep it awake until they decay.
- **Panic** (stop twice) also clears every FX tail (panicClearFx).

### E4 deviations

15. **Feedback without the block delay**: LocalIn/LocalOut loops are per sample (SC added 64 samples), so flanger /
    phaser / rings resonances sit at the true pitch.
16. **Control rate 16 samples** (LFOs, Lags, Compander gain, the mod sources) instead of 64; Amplitude-based
    meters / the duck key follow per sample. LFNoise2 is a bounded midpoint Bezier.
17. **Swaps are click-free** (fade out, restart, fade in; delay-based machines wait until their line has output
    before fading in) where SC freed and respawned the synth. LIMIT's REL change respawns (SC fixed it at spawn).
18. **Tails end below -120 dBFS** (each slot sleeps once input and output stay that quiet; COMP / DUCK / LIMIT never
    sleep). Inputs are sanitised (NaN / Inf / > 1e4 -> 0).
19. **The mute is applied after the send taps** (faithful: \elasticatSendTap reads before \elasticatTrackMix), so
    a muted track still feeds S1 / S2.
20. **The idle saver runs inside the engine** with a 4 s grace on a post-mix peak follower (SC: a 2 s scan of a 2 Hz
    Amplitude feed, never the viewed track). The engine has no "viewed track".
21. **Velocity as a mod source** latches on slice trigs only (as `trackTriggerSlice`); loop trigs leave it.

## 0.6.0

- **Live grains** (`ec_dsp_grains`, in `ec_grains.c`): the track's sounding grains as the last block left them, at
  most `EC_GRAIN_VIEW_MAX` (16): pos (read position now) and len (|rate| x duration) as 0..1 of the sample, level =
  the Hann window now, `newest` on the youngest grain (phase / inc = its age: inc is fixed at spawn, so exact).
  Sources: the GRAIN warp's cloud, the grain clouds of the track's slice voices, random_ola's TGrains grains; 0 for
  the other warps, a track that is not reading, an idle track. Over 16: the newest + the 15 loudest. Computed at the
  call (between blocks) from the engine state: nothing in the audio path (`make -C tests/dsp bench` unchanged).
- **Metronome click** (`ec_click.[ch]`): fixed-size struct, no heap; up to two damped resonators started at their
  peak + a high-passed noise burst + an optional soft-square drive, attack / release ramps, <= 120 ms. Accent = the
  same sound higher (x1.33..2) and louder (-4 dBFS vs -8.4 dBFS peaks at level 1, normalised per sound at init).
  `ec_click_mix` adds into int16 stereo with saturation; the onset of a pending trigger lands on the given frame,
  a ringing click plays on up to it. Tests: `tests/dsp/test_v6.c`.

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
20. Slice voices in random_ola / pitch_corrected play the raw voice (the original had `elasticatSliceVoice_ola` / `_pc` defs:
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
