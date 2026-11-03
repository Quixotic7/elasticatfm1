// SPDX-License-Identifier: GPL-3.0-only
// The 0.6.0 modes' panel labels (docs/UX-V2.md; first drawn on the owner's hand-made boards): what each mode would put
// on RemoteFM1's live labels (ec_app_labels -> panelLabels(): 8 knobs by role, 14 buttons, W1..W16, B1..B11), which
// button is lit, and the black / white keys' roles (P page, S source, X special, A action, T track, O unused).
// host/mock_ux2.c draws the screens; keep its MODES[] roles in step with `black` here.

const steps = () => Array.from({ length: 16 }, (_, i) => [String(i + 1), "P"]);
const off = (n) => Array.from({ length: n }, () => ["", "O"]);
const SRC = [["LFO1", "S"], ["LFO2", "S"], ["MENV", "S"], ["VEL", "S"], ["AT", "S"]];
const TRACKS = ["T1", "T2", "T3", "T4", "T5", "T6", "S1", "S2", "MST"].map((t) => [t, "T"]);
// the trigs of the demo pattern: the step keys light where there is a trig, as today
export const TRIGS = "x.x.x..xx.x.xx..";

// the buttons by printed name (panel.js B order) -> the 0.6.0 mode they open
export const BUTTONS = { FX: "FX", SEL: "FILTER", ENV: "AMP", LFO: "MOD", EDIT: "MIX", GLO: "SHIFT", HOME: "LOOP",
  SAVE: "FILE", ARP: "XFADE", SEQ: "PATTERN", PLAY: "PLAY", REC: "REC" };
const DEF = { sel: "PAGE", algo: "STEP PG", presets: "SLOT", k: ["VALUE", "VALUE", "VALUE", "VALUE"], f1: "COPY", f2: "CUT" };

export const MODES = [
  { file: "Main", lit: ["HOME"], k: ["RATE", "PITCH", "START", "LEN"], white: steps(),
    black: [["LOOP", "P"], ["MACH", "P"], ["WARP", "P"], ["RANGE", "P"], ["NEXT", "P"], ...SRC, ["ZOOM", "X"]] },
  { file: "Shift", lit: ["GLO"], sel: "BPM", algo: "SWING", presets: "TRACK", k: ["SNAP", "SNAP", "SNAP", "SNAP"],
    f1: "UNDO", f2: "REDO", btn: { SAVE: "SAVE" }, white: [...TRACKS, ...off(7)],
    black: [["TAP", "A"], ["LATCH", "A"], ["MEMO", "A"], ["RECALL", "A"], ...off(6), ["FILL", "A"]] },
  { file: "Fx", lit: ["FX"], k: ["BITS", "RATE", "MIX", "TONE"], white: steps(),
    black: [["INS1", "P"], ["INS2", "P"], ["SEND1", "P"], ["SEND2", "P"], ["MSTR", "P"], ...SRC, ["MIX", "X"]] },
  { file: "Filter", lit: ["SEL"], k: ["CUTOFF", "RESO", "TYPE", "DRIVE"], white: steps(),
    black: [["FILTER", "P"], ["F.ENV", "P"], ["MIX", "P"], ...off(2), ...SRC, ["ENV ON", "X"]] },
  { file: "Amp", lit: ["ENV"], k: ["ATTACK", "DECAY", "SUST", "RELEASE"], white: steps(),
    black: [["AMP", "P"], ["AMP ENV", "P"], ["MOD ENV", "P"], ...off(2), ...SRC, ["", "O"]] },
  { file: "Mod", lit: ["LFO"], sel: "ROW", k: ["SRC", "DST", "DEPTH", ""], white: steps(),
    black: [["MATRIX", "P"], ["M1", "S"], ["M2", "S"], ["M3", "S"], ["M4", "S"], ...SRC, ["MACROS", "X"]] },
  { file: "Mix", lit: ["EDIT"], k: ["LEVEL", "PAN", "SEND1", "SEND2"], white: [...TRACKS, ...off(7)],
    black: [...["1/8", "1/4", "1/2", "3/4", "1X", "3/2", "2X", "3X", "4X"].map((t) => [t, "A"]), ["SLOWER", "A"], ["FASTER", "A"]] },
  { file: "Pages", lit: ["EDIT"], btn: { EDIT: "PAGES" }, k: ["LENGTH", "RATE", "LOOP", ""], f1: "COPY PG", f2: "CUT PG",
    white: steps(), black: [["PAGE1", "P"], ["PAGE2", "P"], ["PAGE3", "P"], ["PAGE4", "P"], ...off(7)] },
  { file: "Loop", lit: ["HOME"], k: ["WARP", "RATE", "PITCH", "FADE"],
    white: Array.from({ length: 16 }, (_, i) => ["L" + (i + 1), "A"]),
    black: [["LOOP", "P"], ["MACH", "P"], ["WARP", "P"], ["RANGE", "P"], ["NEXT", "P"], ...SRC, ["ZOOM", "X"]] },
  { file: "Trig", lit: ["HOME"], btn: { HOME: "TRIG" }, k: ["CHANCE", "COND", "RATCHET", ""], white: steps(),
    black: [["TRIG", "P"], ["COND", "P"], ["PLAYHD", "P"], ...off(2), ...SRC, ["", "O"]] },
  { file: "Keyboard", lit: ["HOME"], btn: { HOME: "KEYS" }, algo: "TARGET", k: ["SLICE", "PITCH", "VEL", "STEP"],
    f1: "OCT-", f2: "OCT+", white: ["F", "G", "A", "B", "C", "D", "E", "F", "G", "A", "B", "C", "D", "E", "F", "G"].map((t) => [t, "A"]),
    black: [...Array.from({ length: 10 }, (_, i) => ["SL" + (i + 1), "A"]), ["BANK", "X"]] },
  { file: "FileBrowser", lit: ["SAVE"], sel: "SCROLL", k: ["DIR", "", "", ""], f1: "", f2: "FREE", white: off(16),
    black: [["UP", "A"], ["INTO", "A"], ["EDITOR", "P"], ["RAZOR", "P"], ["POOL", "P"], ...off(4), ["LOAD", "A"], ["PREVIEW", "X"]] },
  { file: "FileEditor", lit: ["SAVE"], k: ["START", "END", "BPM", "GAIN"], f1: "", f2: "",
    white: Array.from({ length: 16 }, (_, i) => ["TRIM" + (i + 1), "A"]),
    black: [["<", "A"], ["1X", "P"], ["2X", "P"], ["4X", "P"], ["8X", "P"], ["16X", "P"], ["32X", "P"], ["64X", "P"], ["128X", "P"], ["FIT", "P"], [">", "A"]] },
  { file: "Sampler", lit: ["SAVE"], btn: { SAVE: "SAMPLER" }, k: ["LENGTH", "GAIN", "MONITOR", ""], f1: "", f2: "",
    white: Array.from({ length: 16 }, (_, i) => ["", i < 10 ? "P" : i < 14 ? "X" : "O"]),
    black: [["FREE", "A"], ["1 BAR", "A"], ["2 BARS", "A"], ["4 BARS", "A"], ["8 BARS", "A"], ...off(5), ["ARM", "X"]] },
  { file: "Crossfader", lit: ["ARP"], k: ["VALUE", "VALUE", "VALUE", "VALUE"],
    white: [...[0, 12, 23, 35, 46, 58, 69, 81, 92, 104, 115, 127].map((v) => [String(v), "A"]),
      ["INST", "P"], ["FAST", "P"], ["MED", "P"], ["SLOW", "P"]],
    black: [["SCENE A", "X"], ["SCENE B", "X"], ...off(8), ["CAPTURE", "X"]] },
  { file: "Patterns", lit: ["SEQ"], k: ["VALUE", "VALUE", "VALUE", "VALUE"], f1: "COPY PT", f2: "CUT PT",
    white: Array.from({ length: 16 }, (_, i) => ["A" + (i + 1), "A"]),
    black: [["BANK A", "P"], ["BANK B", "P"], ["BANK C", "P"], ["BANK D", "P"], ...off(7)] },
  { file: "Record", lit: ["REC"], sel: "ROW", k: ["MODE", "COUNT", "CLICK", "AUTO"], f1: "BACK", f2: "OK",
    white: off(16), black: off(11) },
  { file: "AppSettings", lit: ["GLO", "HOME"], btn: { HOME: "SETTINGS" }, sel: "ROW", algo: "", presets: "",
    k: ["VALUE", "VALUE", "VALUE", "VALUE"], f1: "BACK", f2: "OK", white: off(16), black: off(11) },
];

const ORDER = ["FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC"];
// -> the 49 live labels (panel.js panelLabels order) and which keys / buttons are lit
export function liveOf(m) {
  const d = { ...DEF, ...m };
  const knobs = [d.sel, d.algo, d.presets, ...d.k, null];       // SELECT ALGO PRESETS K1..K4 MASTER
  const btns = ORDER.map((b) => (m.btn && m.btn[b]) || BUTTONS[b]).concat([d.f1, d.f2]);
  const white = m.white.map(([t]) => t), black = m.black.map(([t]) => t);
  const lit = (role, i, isStep) => role !== "O" && (!isStep || TRIGS[i] === "x");
  return {
    live: [...knobs, ...btns, ...white, ...black],
    whiteLit: m.white.map(([t, r], i) => lit(r, i, r === "P" && /^\d+$/.test(t))),
    blackLit: m.black.map(([, r]) => r !== "O"),
    buttonsLit: m.lit,
  };
}
