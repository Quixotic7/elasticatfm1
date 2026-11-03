// SPDX-License-Identifier: GPL-3.0-only
// Elasticat in the real RemoteFM1 web player, end to end, headless (mirrors RemoteFM1's web/site/tools/fantasy-e2e.mjs:
// headless Chromium over the DevTools protocol, a fresh profile, muted, a real AudioContext + AudioWorklet; python's
// http.server; the browser and the server ended by their PIDs; showDirectoryPicker answered with a folder made in the
// site's own storage, since the picker can't be clicked headless).
// 0.6.0's panel (docs/UX-V2.md, docs/FM1-CONTROLS.md): modes; power-on is LOOP ENGINE after the intro (~5.5 s).
//   1. the cart opened as a file (Your firmware): it boots on the rfm1 engine as a Fantasy CFW, the badge shows
//   2. the folder prompt, answered with a folder holding audio/ (the E1 demo loops): the first file loads on T1 (FILE
//      > EDITOR (B4) shows the selected track's sample: read from the LCD's pixels with the cart's own 3x5 font,
//      src/ui/ec_gfx.c); the intro over, power-on is LOOP ENGINE
//   3. PLAY (on the press; the page's panel API, FM1Site.panel): sound (peak + RMS on an AnalyserNode after the worklet)
//   4. TRIG (HOME tapped again) + steps on the white keys; FILE while playing opens the SAMPLER (refused); stopped:
//      SHIFT + W2 = T2, FILE > BROWSE, SEL to the second file, B10 LOAD (the next free pool slot, T2 points at it:
//      the EDITOR shows it), steps on T2, a knob turned (LOOP ENGINE's K1), both tracks play
//   5. the worklet's load reports while it plays (render share, busiest 21 ms, late ms)
//   6. the live panel labels (the player's "labels" message): F1 / F2 COPY / CUT, SHIFT held: UNDO / REDO, the black
//      keys TAP LATCH MEMO RECALL .. FILL, FILE = SAVE (src/app/ec_panel_leds.c)
//   7. stop (PLAY), SHIFT + FILE saves, a reload: the project comes back (T1's file, T2's file) and plays
//   8. (E6, a site with ain1) live input from a fake microphone (Chromium's --use-file-for-fake-audio-capture: the
//      demo's chord loop): T3 on INPUT (hold B1 = the machine, K1) says NO INPUT while off and is silent (stopped);
//      input on (the page's hint): the INPUT track sounds; the SAMPLER (FILE, FILE) B2 = 1 BAR, B11 arms (stopped:
//      at once): the take runs, lands in the next free pool slot (POOL 3/128) and saves (SAVED REC-0001). A site
//      without ain1: these are skipped, said so.
// Screenshots + SUMMARY.txt to results/web/.
//   node host/web_e2e.mjs --site=<RemoteFM1 origin/main export (fs1 + ain1)> --rfm1=<elasticat .rfm1> [--out=results/web]
import { spawn, spawnSync } from "node:child_process";
import { mkdtempSync, mkdirSync, writeFileSync, readFileSync, existsSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, dirname, basename, relative } from "node:path";
import { fileURLToPath } from "node:url";

const REPO = fileURLToPath(new URL("../", import.meta.url));
const arg = (k, d) => { const a = process.argv.find((x) => x.startsWith(`--${k}=`)); return a ? a.slice(k.length + 3) : d; };
const SITE = arg("site");                         // the export's root (web/site/ inside)
const RFM1 = arg("rfm1");
const OUT = arg("out", join(REPO, "results/web"));
if (!SITE || !existsSync(join(SITE, "web/site/index.html")) || !RFM1 || !existsSync(RFM1)) {
  console.error("usage: node host/web_e2e.mjs --site=<RemoteFM1 export root> --rfm1=<elasticat .rfm1> [--out=dir]");
  process.exit(2);
}
mkdirSync(OUT, { recursive: true });
const CHROMIUM = process.env.CHROMIUM || "/Applications/Chromium.app/Contents/MacOS/Chromium";
const PORT = 8000 + Math.floor(Math.random() * 900), DEBUG = 9300 + Math.floor(Math.random() * 600);
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const lines = [];
const log = (s) => { console.log(s); lines.push(s); };
let fails = 0, checks = 0;
const ok = (c, what) => { checks++; if (!c) fails++; log(`${c ? "ok  " : "FAIL"} ${what}`); return c; };

// the cart's font (src/ui/ec_gfx.c FONT[64], ASCII 32..95, 5 rows of 3 bits, the left pixel the row's high bit)
const FONT = [...readFileSync(join(REPO, "src/ui/ec_gfx.c"), "utf8").match(/FONT\[64\]\s*=\s*\{([\s\S]*?)\};/)[1].matchAll(/0x([0-9A-Fa-f]{4})/g)].map((m) => parseInt(m[1], 16));
if (FONT.length !== 64) throw new Error("the font table in src/ui/ec_gfx.c");

// a server root: the site's export and the demo folder side by side
const tmp = mkdtempSync(join(tmpdir(), "ec-web-e2e-")), profile = join(tmp, "profile"), root = join(tmp, "root");
mkdirSync(root);
spawnSync("ln", ["-s", SITE, join(root, "site")]);
const demo = join(root, "demo");
const mk = spawnSync("python3", [join(REPO, "host/make_demo_folder.py"), demo], { encoding: "utf8" });
if (mk.status) { console.error(mk.stderr); process.exit(2); }
const WAVS = ["a_drums_bpm120.wav", "b_bass_bpm120.wav", "c_chords_bpm120.wav", "d_perc16_bpm120.wav"];
const server = spawn("python3", ["-m", "http.server", String(PORT), "--bind", "127.0.0.1", "--directory", root], { stdio: "ignore" });
const chrome = spawn(CHROMIUM, ["--headless=new", "--use-mock-keychain", "--password-store=basic", "--disable-gpu",
  "--no-sandbox", "--no-first-run", `--user-data-dir=${profile}`, "--hide-scrollbars", "--window-size=1280,900",
  "--mute-audio", "--autoplay-policy=no-user-gesture-required", "--use-fake-ui-for-media-stream",
  "--use-fake-device-for-media-stream", `--use-file-for-fake-audio-capture=${join(demo, "audio", "c_chords_bpm120.wav")}`,
  `--remote-debugging-port=${DEBUG}`, "about:blank"], { stdio: "ignore" });
const stop = () => { try { process.kill(chrome.pid); } catch {} try { process.kill(server.pid); } catch {} };
process.on("exit", stop);
process.on("SIGINT", () => process.exit(130));

let ws, id = 0;
const waiting = new Map();
const cdp = (method, params = {}) => new Promise((r) => { const i = ++id; waiting.set(i, r); ws.send(JSON.stringify({ id: i, method, params })); });
async function js(expression, timeout = 60000) {
  const r = await cdp("Runtime.evaluate", { expression, awaitPromise: true, returnByValue: true, timeout });
  if (r.result?.exceptionDetails) throw new Error(`page: ${JSON.stringify(r.result.exceptionDetails).slice(0, 500)}`);
  return r.result?.result?.value;
}
async function until(expr, ms, what) {
  const t0 = Date.now();
  while (Date.now() - t0 < ms) { if (await js(`(async () => !!(await (${expr})))()`)) return true; await sleep(100); }
  console.log("  page:", JSON.stringify(await js(`({ cur: window.FM1Site ? (({ entry, engine, info }) => ({ id: entry && entry.id, engine, info: !!info }))(FM1Site.current()) : null,
    folder: window.FM1Folder && { mode: FM1Folder.mode, state: FM1Folder.state, name: FM1Folder.name },
    toast: document.getElementById("toast")?.textContent })`)));
  throw new Error(`timed out: ${what}`);
}
const shots = [];
async function shot(name) {
  const r = await cdp("Page.captureScreenshot", { format: "png" });
  const f = join(OUT, `${name}.png`);
  writeFileSync(f, Buffer.from(r.result.data, "base64"));
  shots.push(f);
  console.log(`  shot ${f}`);
}
const running = `window.FM1Site && FM1Site.current().info && FM1Site.current().engine === "rfm1" && FM1Site.current().info.id === "elasticat"`;
// the frames, the output's level (peak and RMS per 40 ms on an AnalyserNode after the worklet node), the load reports
const WATCH = `(() => { const p = FM1Rec.o.getPlayer(); if (window.__w) clearInterval(window.__w.timer); const W = window.__w = { frames: 0, lv: [], loads: [], errors: [], labels: null, nlabels: 0 };
  const fb = p.ev.__labels || p.ev.labels; p.ev.__labels = fb; p.ev.labels = (m) => { W.labels = m.list || null; W.nlabels++; if (fb) fb(m); };
  const ff = p.ev.__frame || p.ev.frame; p.ev.__frame = ff; p.ev.frame = (px, seq) => { W.frames++; if (ff) ff(px, seq); };
  const fe = p.ev.__error || p.ev.error; p.ev.__error = fe; p.ev.error = (m) => { W.errors.push(String(m)); if (fe) fe(m); };
  const fl = p.ev.__load || p.ev.load; p.ev.__load = fl; p.ev.load = (m) => { W.loads.push({ t: performance.now(), ...m }); if (fl) fl(m); };
  const an = p.ctx.createAnalyser(); an.fftSize = 2048; p.node.connect(an);
  const buf = new Float32Array(an.fftSize);
  W.timer = setInterval(() => { an.getFloatTimeDomainData(buf); let m = 0, s = 0; for (const v of buf) { m = Math.max(m, Math.abs(v)); s += v * v; }
    W.lv.push({ t: performance.now(), peak: m, ms: s / buf.length }); }, 40);
  return true; })()`;
const level = (ms) => js(`(() => { const t = performance.now() - ${ms}; const a = window.__w.lv.filter((p) => p.t > t);
  return { peak: Math.max(0, ...a.map((p) => p.peak)), rms: a.length ? Math.sqrt(a.reduce((s, p) => s + p.ms, 0) / a.length) : 0 }; })()`);
const loads = (ms) => js(`(() => { const t = performance.now() - ${ms}; return window.__w.loads.filter((p) => p.t > t); })()`);
const fmtL = (l) => `peak ${l.peak.toFixed(3)}, RMS ${l.rms.toFixed(4)}`;

// the panel (FM1Site.panel: remote-hook.js). Buttons by label (panel.js PANEL_LABELS = the cart's EC_B_x order) ->
// matrix; encoders by matrix (0 SELECT 1 ALGO 2..5 KNOB1..4 6 PRESETS); keys 0..26 = F3..G5, the lower row the naturals
const B = { FX: 0, SEL: 1, ENV: 2, LFO: 3, EDIT: 4, GLO: 5, HOME: 6, SAVE: 7, ARP: 8, SEQ: 9, PLAY: 10, REC: 11, "OCT-": 12, "OCT+": 13 };
const toMatrix = (b) => (b < 12 ? b + 2 : b - 12);
const LOWER = [0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26], UPPER = [1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25];
const E = { SELECT: 0, ALGO: 1, K1: 2, K2: 3, K3: 4, K4: 5, PRESETS: 6 };
const btnDown = (l, d) => js(`(FM1Site.panel.button(${toMatrix(B[l])}, ${d}), true)`);
async function tap(l, ms = 120) { await btnDown(l, true); await sleep(ms); await btnDown(l, false); await sleep(200); }
async function keyTap(k, ms = 120) { await js(`(FM1Site.panel.key(${k}, true, 100), true)`); await sleep(ms); await js(`(FM1Site.panel.key(${k}, false, 0), true)`); await sleep(150); }
async function turn(e, d) { for (let i = 0; i < Math.abs(d); i++) { await js(`(FM1Site.panel.turn(${e}, ${Math.sign(d)}), true)`); await sleep(60); } await sleep(200); }
// 0.6.0's names (docs/FM1-CONTROLS.md): white keys W1..W16 (the naturals), black keys B1..B11 (the sharps); FILE is the
// button printed SAVE, F1 / F2 = OCT- / OCT+, GLO = SHIFT
const W = (n) => LOWER[n - 1], BK = (n) => UPPER[n - 1];
const keyDown = (k, d) => js(`(FM1Site.panel.key(${k}, ${d}, ${d ? 100 : 0}), true)`);
async function shifted(fn) { await btnDown("GLO", true); await sleep(150); await fn(); await btnDown("GLO", false); await sleep(250); }

// text on the LCD: the string drawn with the cart's font at a scale (2: the mockup's grid), one colour, the glyphs'
// unset pixels not that colour; returns { x, y } or null
const screen = async () => Uint16Array.from(await js(`Array.from(FM1Site.screen().pixels)`));
function findText(px, str, sc = 2) {
  const W = 240, cw = 4 * sc, s = str.toUpperCase();
  const g = [...s].map((c) => { const u = c.charCodeAt(0); return u >= 32 && u < 96 ? FONT[u - 32] : 0; });
  const on = [], off = [];
  g.forEach((bits, i) => { for (let r = 0; r < 5; r++) for (let c = 0; c < 3; c++) for (let dy = 0; dy < sc; dy++) for (let dx = 0; dx < sc; dx++)
    ((bits >> (14 - (r * 3 + c))) & 1 ? on : off).push([i * cw + c * sc + dx, r * sc + dy]); });
  const w = s.length * cw, h = 5 * sc;
  for (let y = 0; y + h <= W; y++) for (let x = 0; x + w <= W; x++) {
    const col = px[(y + on[0][1]) * W + x + on[0][0]];
    let hit = true;
    for (const [a, b] of on) if (px[(y + b) * W + x + a] !== col) { hit = false; break; }
    if (!hit) continue;
    for (const [a, b] of off) if (px[(y + b) * W + x + a] === col) { hit = false; break; }
    if (hit) return { x, y };
  }
  return null;
}
async function untilText(str, ms, sc = 2) {
  const t0 = Date.now();
  while (Date.now() - t0 < ms) { const at = findText(await screen(), str, sc); if (at) return at; await sleep(150); }
  return null;
}
// a whole Escape press (down + up), as a real key: a lone keyDown that reached the panel would leave OCT- held
// (Escape = OCT- in the page's key map; 0.6.0: OCT- is F1, a tap of it = sticky copy / paste)
const esc = async () => {
  await cdp("Input.dispatchKeyEvent", { type: "keyDown", code: "Escape", key: "Escape", windowsVirtualKeyCode: 27 });
  await cdp("Input.dispatchKeyEvent", { type: "keyUp", code: "Escape", key: "Escape", windowsVirtualKeyCode: 27 });
};
// FILE > EDITOR's top line (y 13 on the 120 grid = 26 on the LCD; the waveform below): the selected track's sample
const fileHeader = async (name, ms = 15000) => { const at = await untilText(name, ms); return at && at.y < 36 ? at : null; };
// the intro (the logo, ~5.5 s from power-on): any key skips it but still acts, so the test waits it out
let bootAt = 0;
const introOver = async () => { const left = bootAt + 6500 - Date.now(); if (left > 0) await sleep(left); };
const diff = (a, b) => { let n = 0; for (let i = 0; i < a.length; i++) n += a[i] !== b[i]; return n; };

const nums = {};
try {
  let served = false;
  for (let i = 0; i < 50 && !served; i++) { await sleep(200); try { served = !!(await (await fetch(`http://127.0.0.1:${PORT}/site/web/site/catalog.json`)).json()).paths.engines.rfm1; } catch {} }
  if (!served) throw new Error(`no catalog.json with paths.engines.rfm1 at http://127.0.0.1:${PORT}/site/web/site/`);
  let targets;
  for (let i = 0; i < 50 && !targets; i++) { await sleep(200); try { targets = await (await fetch(`http://127.0.0.1:${DEBUG}/json/list`)).json(); } catch {} }
  ws = new WebSocket(targets.find((t) => t.type === "page").webSocketDebuggerUrl);
  await new Promise((r, j) => { ws.onopen = r; ws.onerror = j; });
  const exceptions = [], warnings = [];
  ws.onmessage = (m) => {
    const d = JSON.parse(m.data);
    if (d.id && waiting.has(d.id)) { waiting.get(d.id)(d); waiting.delete(d.id); }
    if (d.method === "Runtime.exceptionThrown") { exceptions.push(JSON.stringify(d.params.exceptionDetails).slice(0, 400)); console.error("exception:", exceptions.at(-1)); }
    if (d.method === "Runtime.consoleAPICalled" && /warn|error/.test(d.params.type)) warnings.push(d.params.args.map((a) => a.value ?? a.description).join(" ").slice(0, 300));
  };
  await cdp("Runtime.enable"); await cdp("Page.enable"); await cdp("DOM.enable");
  const base = `http://127.0.0.1:${PORT}/site/web/site/`;
  await cdp("Page.navigate", { url: base });
  await until(`window.FM1Rec && window.FM1Folder && window.FM1Site && document.getElementById("fw-btn")`, 60000, "the page");
  await sleep(1500);

  // the folder the picker will answer with: picked/audio/<the demo loops>, in the site's storage
  const put = await js(`(async () => { const d = await (await (await navigator.storage.getDirectory()).getDirectoryHandle("picked", { create: true })).getDirectoryHandle("audio", { create: true });
    let n = 0; for (const f of ${JSON.stringify(WAVS)}) { const b = await (await fetch("/demo/audio/" + f)).arrayBuffer();
      const w = await (await d.getFileHandle(f, { create: true })).createWritable(); await w.write(b); await w.close(); n += b.byteLength; } return n; })()`);
  await js(`(window.showDirectoryPicker = async () => (await navigator.storage.getDirectory()).getDirectoryHandle("picked"), true)`);

  // 1. the cart from a file (Your firmware)
  await js(`(document.getElementById("fw-btn").click(), true)`);
  await until(`document.querySelector('input[type=file][accept=".rfm1,.fwsc"]')`, 5000, "the Open a file... input");
  const { nodeId } = (await cdp("DOM.querySelector", { nodeId: (await cdp("DOM.getDocument", { depth: 1 })).result.root.nodeId, selector: 'input[type=file][accept=".rfm1,.fwsc"]' })).result;
  await cdp("DOM.setFileInputFiles", { nodeId, files: [RFM1] });
  const tBoot = Date.now();
  await until(`${running} && FM1Site.current().entry.local`, 60000, "the cart running");
  nums.bootMs = Date.now() - tBoot;
  bootAt = Date.now();
  const info = await js(`(({ info }) => ({ virtual: info.virtual, needs: info.needs, folders: info.folders, version: info.version, mem: info.memoryBytes }))(FM1Site.current())`);
  ok(info.virtual === true && (info.needs || []).includes("fs1"), `the cart boots on the rfm1 engine as a Fantasy CFW in ${nums.bootMs} ms (${basename(RFM1)}, needs ${info.needs}, folders ${JSON.stringify(info.folders)}, memory ${info.mem} bytes)`);
  await js(WATCH);
  await js(`(document.getElementById("fw-btn").click(), true)`);
  await sleep(600);
  ok(await js(`!!document.querySelector(".cards.mine .card[data-sha] .fantasy-badge")`), "Your firmware: the card has the Fantasy CFW badge");
  await shot("01_your_firmware_badge");
  // Escape only while the sheet is open (aside.sheet.open: the page's own sheet.isOpen); a closing sheet is still
  // visible but no longer takes the key, which then reaches the panel as OCT- (0.6.0: F1, a tap = sticky copy)
  for (let i = 0; i < 3 && await js(`!!document.querySelector("aside.sheet.open")`); i++) { await esc(); await sleep(400); }

  // 2. the prompt, the folder, the first file on T1; the intro over: LOOP ENGINE
  await until(`!document.querySelector(".folder-ask").hidden`, 8000, "the folder prompt");
  const ask = await js(`document.querySelector(".folder-ask").innerText.replace(/\\s+/g, " ")`);
  ok(/Pick your RemoteFM1 folder/.test(ask), `no folder yet: the prompt "${ask}"`);
  await shot("02_folder_prompt");
  await js(`(document.querySelector(".folder-ask .pill.primary").click(), true)`);
  await until(`FM1Folder.state === "ready" && FM1Folder.name === "picked"`, 8000, "the folder picked");
  await introOver();
  const home = await untilText("LOOP/LOOP", 8000), homeFile = !!findText(await screen(), "A_DRUMS_BPM120.WAV");
  await shot("03_power_on_loop_engine");
  await tap("SAVE");                            // FILE (stopped: it opens)
  await keyTap(BK(4));                          // EDITOR: the selected track's sample
  const t1 = await fileHeader("A_DRUMS_BPM120.WAV");
  ok(!!home && !!t1 && await js(`document.querySelector(".folder-ask").hidden`), `the folder picked (${(put / 1e6).toFixed(1)} MB in audio/): the prompt gone; after the intro LOOP ENGINE${home ? "" : " (no LOOP/LOOP header)"}${homeFile ? " with A_DRUMS_BPM120.WAV" : ""}; FILE > EDITOR (B4): T1 plays A_DRUMS_BPM120.WAV${t1 ? ` (at ${t1.x},${t1.y})` : " (not found on the screen)"}`);
  await shot("04_t1_file_editor");
  await turn(E.SELECT, -1);                     // back to BROWSE (in the EDITOR the black keys are zoom keys; FILE
  await tap("HOME");                            // remembers its page); HOME: LOOP ENGINE (the sub-mode it was on)
  await sleep(1000);                            // (the mode pop-up, 0.8 s)

  // 3. PLAY (on the press)
  const quiet = await level(800);
  await tap("PLAY");
  await sleep(3000);
  const l1 = await level(2500);
  nums.play = l1;
  ok(l1.rms > 0.005 && l1.peak > 0.02, `PLAY in LOOP ENGINE: sound (${fmtL(l1)}; before PLAY ${fmtL(quiet)})`);
  await shot("05_playing");

  // 4. steps on T1 (TRIG: HOME tapped again; LOOP ENGINE's white keys play the loop), FILE refused while playing,
  //    T2 + FILE + LOAD, steps on T2, a knob
  await tap("HOME");                            // LOOP ENGINE -> TRIG
  const trig = await untilText("TRIG/", 3000);
  await sleep(1000);
  const leds0 = await js(`Array.from(FM1Site.leds().levels)`);
  for (const n of [3, 7, 11, 15]) await keyTap(W(n));
  await sleep(400);
  const leds1 = await js(`Array.from(FM1Site.leds().levels)`);
  ok(!!trig && diff(leds0, leds1) > 0, `HOME again = TRIG${trig ? "" : " (not on the screen)"}; steps W3, W7, W11, W15 on T1 (${diff(leds0, leds1)} LED levels changed)`);
  await shot("06_t1_steps");
  await tap("SAVE");                            // FILE while playing: the SAMPLER instead
  const smp = await untilText("SAMPLER", 3000);
  const noBrowse = !findText(await screen(), "FILE/BROWSE");
  ok(!!smp && noBrowse, `FILE while playing: refused, the SAMPLER opens instead${smp ? "" : " (no SAMPLER on the screen)"}${noBrowse ? "" : " (the browser opened)"}`);
  await shot("07_file_while_playing_sampler");
  await tap("PLAY");                            // stop (on the press)
  await sleep(500);
  await shifted(() => keyTap(W(2)));            // SHIFT + W2 = T2
  await tap("SAVE");                            // SAMPLER -> FILE (stopped: it opens)
  const fp = await untilText("FILE/BROWSE", 3000);
  await turn(E.SELECT, -8);                     // the cursor to the top (it clamps) ..
  await turn(E.SELECT, 1);                      // .. then the second file
  await sleep(300);
  await shot("08_file_browser_t2");
  await keyTap(BK(10));                         // LOAD: the next free pool slot, T2 points at it
  await sleep(1500);
  const pool = !!(await untilText("POOL 2/128", 8000));
  await keyTap(BK(4));                          // EDITOR: T2's sample
  const t2 = await fileHeader("B_BASS_BPM120.WAV");
  await sleep(300);
  ok(!!fp && !!t2 && pool, `stopped; SHIFT + W2 = T2, FILE > BROWSE, SEL to the second file, B10 LOAD: POOL 2/128${pool ? "" : " (not on the screen)"}, the EDITOR shows B_BASS_BPM120.WAV on T2${fp ? "" : " (no FILE/BROWSE header)"}${t2 ? "" : " (not on the screen)"}`);
  await shot("09_t2_loaded");
  await turn(E.SELECT, -1);                     // FILE back on BROWSE
  await tap("HOME");                            // TRIG (HOME's sub-mode it was on)
  await sleep(1000);
  for (const n of [1, 5, 9, 13]) await keyTap(W(n));
  await tap("HOME"); await tap("HOME");         // TRIG -> KEYBOARD -> LOOP ENGINE
  const le = await untilText("LOOP/", 3000);
  await sleep(1000);                            // (the pop-up)
  const before = await screen();
  await turn(E.K1, 6);                          // PITCH +6
  await sleep(300);
  const after = await screen();
  ok(!!le && diff(before, after) > 20, `steps W1, W5, W9, W13 on T2 (TRIG); HOME x2 = LOOP ENGINE, K1 +6: the screen changed (${diff(before, after)} pixels)`);
  await shot("10_t2_steps_knob");
  await tap("PLAY");
  await sleep(4000);
  const l2 = await level(3500);
  nums.play2 = l2;
  ok(l2.rms > 0.005, `T1 + T2 playing: sound (${fmtL(l2)})`);
  await shot("11_two_tracks_playing");

  // 5. load while playing
  await sleep(4500);
  const ld = await loads(8000);
  const mx = (k) => Math.max(0, ...ld.map((x) => x[k] || 0));
  nums.load = { n: ld.length, load: mx("load"), peak: mx("peak"), late: ld.reduce((s, x) => s + (x.lateMs || 0), 0), p99: mx("p99Ms"), max: mx("maxMs") };
  ok(ld.length >= 4 && nums.load.load < 0.5 && nums.load.peak < 0.95, `the worklet's load while playing (${ld.length} reports, 8 s): render share max ${(nums.load.load * 100).toFixed(1)} %, busiest 21 ms ${(nums.load.peak * 100).toFixed(1)} %, late ${nums.load.late.toFixed(0)} ms, quantum p99 ${nums.load.p99.toFixed(2)} ms / max ${nums.load.max.toFixed(2)} ms`);
  ok(!(await js(`window.__w.errors.length`)), `no worklet errors${(await js(`window.__w.errors.join(" | ")`)) ? ": " + (await js(`window.__w.errors.join(" | ")`)) : ""}`);

  // 6. the live labels: the slots are FIZZ's 49 (8 + the button's FM1P_B_* index; 38 + the black key)
  const LB = (i) => js(`(window.__w.labels || [])[${i}] ?? null`);
  const LBS = async () => ({ f1: await LB(8 + B["OCT-"]), f2: await LB(8 + B["OCT+"]), glo: await LB(8 + B.GLO), home: await LB(8 + B.HOME),
    file: await LB(8 + B.SAVE), play: await LB(8 + B.PLAY), b: await Promise.all([0, 1, 2, 3, 4, 10].map((i) => LB(38 + i))) });
  await until(`window.__w.labels`, 5000, "a labels message");
  const off = await LBS();
  await btnDown("GLO", true); await sleep(600);  // held past a tap (a quick tap would be sticky SHIFT)
  const on = await LBS();
  await shot("12_shift_map");
  await btnDown("GLO", false); await sleep(400);
  const back = await LBS();
  const q = (x) => JSON.stringify(x);
  ok(off.f1 === "COPY" && off.f2 === "CUT" && off.glo === "SHIFT" && off.home === "LOOP" && off.file === "FILE" && off.play === "STOP"
    && on.f1 === "UNDO" && on.f2 === "REDO" && on.file === "SAVE" && on.home === "SETTINGS"
    && q(on.b) === q(["TAP", "LATCH", "MEMO", "RECALL", "", "FILL"]) && back.f1 === off.f1 && back.f2 === off.f2 && q(back.b) === q(off.b),
    `live labels: F1 "${off.f1}", F2 "${off.f2}", GLO "${off.glo}", HOME "${off.home}", FILE "${off.file}", PLAY "${off.play}", B1-B5/B11 ${q(off.b)}; SHIFT held: F1 "${on.f1}", F2 "${on.f2}", FILE "${on.file}", HOME "${on.home}", B1-B5/B11 ${q(on.b)}; released: F1 "${back.f1}", B ${q(back.b)} (${await js("window.__w.nlabels")} labels messages)`);

  // 7. stop (PLAY), SHIFT + FILE saves, a reload
  await tap("PLAY");
  await sleep(800);
  await shifted(() => tap("SAVE"));
  const saved0 = await untilText("SAVED", 4000);
  await sleep(4000);
  ok(!!saved0, `stopped; SHIFT + FILE: the project saved${saved0 ? " (SAVED)" : " (no SAVED on the screen)"}`);
  await shot("13_stopped_saved");
  await js("(window.__before = true)"); await cdp("Page.reload", {});
  await until(`!window.__before && window.FM1Folder && window.FM1Rec && ${running}`, 60000, "the cart after the reload");
  bootAt = Date.now();
  await js("FM1Folder.ready");
  const fst = await js(`({ state: FM1Folder.state, name: FM1Folder.name })`);
  if (fst.state === "ask") { await js(`(document.querySelector(".folder-ask .pill.primary").click(), true)`); await sleep(500); }
  await js(WATCH);
  await introOver();
  const home2 = await untilText("LOOP/LOOP", 8000);
  await shifted(() => keyTap(W(2)));            // T2
  await tap("SAVE");                            // FILE
  await keyTap(BK(4));                          // EDITOR
  const r2 = await fileHeader("B_BASS_BPM120.WAV");
  await shot("14_reloaded_t2_file");
  await shifted(() => keyTap(W(1)));            // T1, the EDITOR stays
  const r1 = await fileHeader("A_DRUMS_BPM120.WAV");
  await shot("15_reloaded_t1_file");
  await turn(E.SELECT, -1);                     // FILE back on BROWSE
  await tap("HOME");                            // LOOP ENGINE
  await sleep(1000);
  ok(!!home2 && !!r1 && !!r2, `after a reload (folder ${fst.name}, ${fst.state}): LOOP ENGINE${home2 ? "" : " (not on the screen)"}; the project is back, T2 B_BASS_BPM120.WAV ${r2 ? "ok" : "missing"}, T1 A_DRUMS_BPM120.WAV ${r1 ? "ok" : "missing"}`);
  await tap("PLAY");
  await sleep(3000);
  const l3 = await level(2500);
  nums.play3 = l3;
  ok(l3.rms > 0.005 && l3.peak > 0.02, `after the reload PLAY plays (${fmtL(l3)})`);
  await shot("16_reloaded_playing");

  // 8. E6: live input (only on a site with ain1)
  if (await js(`!!(window.FM1Ain && FM1Ain.supported) && FM1Site.current().info.ain === "wants"`)) {
    await tap("PLAY"); await sleep(500);        // stop
    await shifted(() => keyTap(W(3)));          // T3 (LOOP ENGINE stays)
    await keyDown(BK(1), true); await sleep(600);   // hold B1 = the machine (the quick setting) ..
    await turn(E.K1, 7);                        // .. K1: LOOP + 7 = INPUT
    await keyDown(BK(1), false); await sleep(1200);
    const off = await level(600), noIn = !!findText(await screen(), "NO INPUT", 4);
    ok(noIn && off.peak < 0.002, `T3: hold B1 + K1 = the INPUT machine; input off: NO INPUT, silent (${fmtL(off)})`);
    await shot("17_input_off");
    const hint = await js(`(() => { const b = document.querySelector(".ain-ask:not([hidden]) .pill.primary"); if (b) b.click(); return !!b; })()`);
    if (!hint) await js(`FM1Ain.start().then(() => true)`);
    await until(`!!FM1Ain.live`, 10000, "the input live");
    await sleep(2000);
    const lon = await level(1500), told = await js(`FM1Ain.live && FM1Ain.live.told`);
    nums.input = { ...lon, latency: told, hint };
    ok(lon.peak > 0.02 && !findText(await screen(), "NO INPUT", 4), `input on (${hint ? "the hint's button" : "FM1Ain.start"}; latency ${told} frames): the INPUT track sounds (${fmtL(lon)})`);
    await shot("18_input_live");
    await tap("SAVE");                          // FILE (stopped)
    await tap("SAVE");                          // .. tapped again: the SAMPLER
    const sOpen = await untilText("SAMPLER", 3000);
    await keyTap(BK(2));                        // 1 BAR
    await sleep(400);
    await shot("19_sampler");
    await keyTap(BK(11));                       // arm: stopped, it records at once
    const taking = await untilText("BAR 1", 3000, 4);     // "REC  BAR 1": the take running
    await sleep(300);
    await shot("20_sampler_taking");
    const saved = await untilText("SAVED", 10000);
    await sleep(300);
    await shot("21_sampler_saved");
    await tap("SAVE");                          // the SAMPLER -> FILE: the pool meter
    const pool3 = await untilText("POOL 3/128", 5000);
    await shot("22_pool_with_take");
    ok(!!sOpen && !!taking && !!saved && !!pool3, `the SAMPLER (FILE, FILE): B2 1 BAR, B11 arms: the take ran${taking ? " (REC BAR 1)" : " (no REC BAR 1 on the screen)"}, saved${saved ? " (SAVED REC-0001..)" : " (no SAVED)"}, landed in the next free pool slot${pool3 ? " (POOL 3/128)" : " (no POOL 3/128)"}`);
    await tap("HOME");
    await sleep(500);
  } else
    log("  (this site has no ain1: the live input checks skipped)");

  ok(!exceptions.length, `no exceptions in the page${exceptions.length ? ": " + exceptions.join(" | ") : ""}`);
  if (warnings.length) log(`  page warnings: ${[...new Set(warnings)].slice(0, 8).join(" | ")}`);
} catch (e) {
  console.error(e.stack || e);
  log(`FAIL ${String(e.message || e)}`);
  fails++;
} finally {
  stop();
}
log(fails ? `elasticat web e2e: ${fails} of ${checks} FAILED` : `elasticat web e2e passed (${checks})`);
writeFileSync(join(OUT, "SUMMARY.txt"), [`Elasticat FM-1 in the RemoteFM1 web player (headless Chromium), ${new Date().toISOString()}`,
  `cart: ${basename(RFM1)}; site: ${SITE}`, "", ...lines, "", `numbers: ${JSON.stringify(nums)}`, "",
  "screenshots:", ...shots.map((f) => `  ${relative(OUT, f)}`), ""].join("\n"));
process.exit(fails ? 1 : 0);
