// SPDX-License-Identifier: GPL-3.0-only
// Elasticat in the real RemoteFM1 web player, end to end, headless (mirrors RemoteFM1's web/site/tools/fantasy-e2e.mjs:
// headless Chromium over the DevTools protocol, a fresh profile, muted, a real AudioContext + AudioWorklet; python's
// http.server; the browser and the server ended by their PIDs; showDirectoryPicker answered with a folder made in the
// site's own storage, since the picker can't be clicked headless).
//   1. the cart opened as a file (Your firmware): it boots on the rfm1 engine as a Fantasy CFW, the badge shows
//   2. the folder prompt, answered with a folder holding audio/ (the E1 demo loops): the first file loads on T1 (the
//      screen shows its name: read from the LCD's pixels with the cart's own 3x5 font, src/ui/ec_gfx.c)
//   3. PLAY (the page's panel API, FM1Site.panel): sound (peak + RMS on an AnalyserNode after the worklet)
//   4. steps on the lower keys, T2 + FILE (SAVE) + SELECT + SAVE loads the second file on T2, a knob turned
//   5. the worklet's load reports while it plays (render share, busiest 21 ms, late ms)
//   6. the live panel labels (0.3.0: the player's "labels" message): OCT- says LOOP KEYS, SHIFT held: UNDO, MEMORIZE
//   7. stop (SHIFT + PLAY), the autosave, a reload: the project comes back (T1's file, T2's file) and plays
// Screenshots + SUMMARY.txt to results/web/.
//   node host/web_e2e.mjs --site=<RemoteFM1 origin/main export> --rfm1=<elasticat .rfm1> [--out=results/web]
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
  "--mute-audio", "--autoplay-policy=no-user-gesture-required", `--remote-debugging-port=${DEBUG}`, "about:blank"], { stdio: "ignore" });
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
async function untilText(str, ms, what) {
  const t0 = Date.now();
  while (Date.now() - t0 < ms) { const at = findText(await screen(), str); if (at) return at; await sleep(200); }
  return null;
}
// a whole Escape press (down + up), as a real key: a lone keyDown that reached the panel would leave OCT- held
// (Escape = OCT- in the page's key map; 0.3.0: an OCT key held is the PAGE scope, its tap the LOOP KEYS mode)
const esc = async () => {
  await cdp("Input.dispatchKeyEvent", { type: "keyDown", code: "Escape", key: "Escape", windowsVirtualKeyCode: 27 });
  await cdp("Input.dispatchKeyEvent", { type: "keyUp", code: "Escape", key: "Escape", windowsVirtualKeyCode: 27 });
};
// the FILE page's top line (y 26 at scale 2; the list starts at y 42): the name of the track's sample
const fileHeader = async (name) => { const at = await untilText(name, 15000); return at && at.y < 36 ? at : null; };
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
  const info = await js(`(({ info }) => ({ virtual: info.virtual, needs: info.needs, folders: info.folders, version: info.version, mem: info.memoryBytes }))(FM1Site.current())`);
  ok(info.virtual === true && (info.needs || []).includes("fs1"), `the cart boots on the rfm1 engine as a Fantasy CFW in ${nums.bootMs} ms (${basename(RFM1)}, needs ${info.needs}, folders ${JSON.stringify(info.folders)}, memory ${info.mem} bytes)`);
  await js(WATCH);
  await js(`(document.getElementById("fw-btn").click(), true)`);
  await sleep(600);
  ok(await js(`!!document.querySelector(".cards.mine .card[data-sha] .fantasy-badge")`), "Your firmware: the card has the Fantasy CFW badge");
  await shot("01_your_firmware_badge");
  // Escape only while the sheet is open (aside.sheet.open: the page's own sheet.isOpen); a closing sheet is still
  // visible but no longer takes the key, which then reaches the panel as OCT- (0.3.0: a tap = the LOOP KEYS mode)
  for (let i = 0; i < 3 && await js(`!!document.querySelector("aside.sheet.open")`); i++) { await esc(); await sleep(400); }

  // 2. the prompt, the folder, the first file on T1
  await until(`!document.querySelector(".folder-ask").hidden`, 8000, "the folder prompt");
  const ask = await js(`document.querySelector(".folder-ask").innerText.replace(/\\s+/g, " ")`);
  await sleep(1500);
  const noFolder = !!findText(await screen(), "NO FOLDER") || !!findText(await screen(), "SCANNING");
  ok(/Pick your RemoteFM1 folder/.test(ask), `no folder yet: the prompt "${ask}"${noFolder ? ", the screen says NO FOLDER" : ""}`);
  await shot("02_folder_prompt");
  await js(`(document.querySelector(".folder-ask .pill.primary").click(), true)`);
  await until(`FM1Folder.state === "ready" && FM1Folder.name === "picked"`, 8000, "the folder picked");
  const t1 = await untilText("A_DRUMS_BPM120.WAV", 15000);
  ok(!!t1 && await js(`document.querySelector(".folder-ask").hidden`), `the folder picked (${(put / 1e6).toFixed(1)} MB in audio/): the prompt gone, T1 shows A_DRUMS_BPM120.WAV${t1 ? ` (at ${t1.x},${t1.y})` : " (not found on the screen)"}`);
  await sleep(800);
  await shot("03_t1_loaded");

  // 3. PLAY
  const quiet = await level(800);
  await tap("PLAY");
  await sleep(3000);
  const l1 = await level(2500);
  nums.play = l1;
  ok(l1.rms > 0.005 && l1.peak > 0.02, `PLAY: sound (${fmtL(l1)}; before PLAY ${fmtL(quiet)})`);
  await shot("04_playing");

  // 4. steps on T1, T2 + FILE + the second file, a knob
  const leds0 = await js(`Array.from(FM1Site.leds().levels)`);
  await tap("SEQ");
  for (const i of [2, 6, 10, 14]) await keyTap(LOWER[i]);
  await sleep(400);
  const leds1 = await js(`Array.from(FM1Site.leds().levels)`);
  ok(diff(leds0, leds1) > 0 && !!findText(await screen(), "TRIG"), `steps 3, 7, 11, 15 on T1 (TRIG page; ${diff(leds0, leds1)} LED levels changed)`);
  await shot("05_t1_steps");
  await keyTap(UPPER[1]);                       // T2
  await tap("SAVE");                            // FILE
  const fp = await untilText("FILE/", 3000);
  await turn(E.SELECT, 1);                      // the cursor to the second file
  await sleep(300);
  await shot("06_file_browser_t2");
  await tap("SAVE");                            // load it into T2's slot
  const t2 = await fileHeader("B_BASS_BPM120.WAV");
  await sleep(500);
  ok(!!fp && !!t2 && !!findText(await screen(), "T2"), `T2: FILE (SAVE), SELECT +1, SAVE: B_BASS_BPM120.WAV loaded${fp ? "" : " (no FILE/ header)"}${t2 ? "" : " (not on the screen)"}`);
  await shot("07_t2_loaded");
  await tap("SEL");                             // SRC
  for (const i of [0, 4, 8, 12]) await keyTap(LOWER[i]);
  await sleep(300);
  const before = await screen();
  await turn(E.K1, 6);
  await sleep(300);
  const after = await screen();
  ok(diff(before, after) > 20, `steps 1, 5, 9, 13 on T2; KNOB1 +6 on SRC: the screen changed (${diff(before, after)} pixels)`);
  await shot("08_t2_steps_knob");
  await sleep(4000);
  const l2 = await level(3500);
  nums.play2 = l2;
  ok(l2.rms > 0.005, `T1 + T2 playing: sound (${fmtL(l2)})`);
  await shot("09_two_tracks_playing");

  // 5. load while playing
  const ld = await loads(8000);
  const mx = (k) => Math.max(0, ...ld.map((x) => x[k] || 0));
  nums.load = { n: ld.length, load: mx("load"), peak: mx("peak"), late: ld.reduce((s, x) => s + (x.lateMs || 0), 0), p99: mx("p99Ms"), max: mx("maxMs") };
  ok(ld.length >= 4 && nums.load.load < 0.5 && nums.load.peak < 0.95, `the worklet's load while playing (${ld.length} reports, 8 s): render share max ${(nums.load.load * 100).toFixed(1)} %, busiest 21 ms ${(nums.load.peak * 100).toFixed(1)} %, late ${nums.load.late.toFixed(0)} ms, quantum p99 ${nums.load.p99.toFixed(2)} ms / max ${nums.load.max.toFixed(2)} ms`);
  ok(!(await js(`window.__w.errors.length`)), `no worklet errors${(await js(`window.__w.errors.join(" | ")`)) ? ": " + (await js(`window.__w.errors.join(" | ")`)) : ""}`);

  // 6. the live labels: the slots are FIZZ's 49 (8 + the button's FM1P_B_* index; 38 + the black key)
  const LB = (i) => js(`(window.__w.labels || [])[${i}] ?? null`);
  await until(`window.__w.labels`, 5000, "a labels message");
  const offDn = await LB(8 + 12), offT1 = await LB(38), offGlo = await LB(8 + 5);
  await btnDown("GLO", true); await sleep(400);
  const onDn = await LB(8 + 12), onT1 = await LB(38), onUp = await LB(8 + 13);
  await shot("10_shift_map");
  await btnDown("GLO", false); await sleep(400);
  const backDn = await LB(8 + 12);
  ok(offDn === "LOOP KEYS" && offGlo === "SHIFT" && offT1 === "T1" && onDn === "UNDO" && onUp === "REDO" && onT1 === "MEMORIZE" && backDn === offDn,
    `live labels: OCT- "${offDn}", GLO "${offGlo}", T1 "${offT1}"; SHIFT held: OCT- "${onDn}", OCT+ "${onUp}", T1 "${onT1}"; released: OCT- "${backDn}" (${await js("window.__w.nlabels")} labels messages)`);

  // 7. stop (SHIFT + PLAY), the autosave (the cart: stopped + 2 s quiet; the page: the flash read 1.5 s later), a reload
  await btnDown("GLO", true); await sleep(80); await tap("PLAY"); await btnDown("GLO", false);
  await sleep(7000);
  await shot("11_stopped_saved");
  await js("(window.__before = true)"); await cdp("Page.reload", {});
  await until(`!window.__before && window.FM1Folder && window.FM1Rec && ${running}`, 60000, "the cart after the reload");
  await js("FM1Folder.ready");
  const fst = await js(`({ state: FM1Folder.state, name: FM1Folder.name })`);
  if (fst.state === "ask") { await js(`(document.querySelector(".folder-ask .pill.primary").click(), true)`); await sleep(500); }
  await js(WATCH);
  await keyTap(UPPER[1]);                       // T2
  await tap("SAVE");                            // FILE (from SRC, where a boot starts: one tap)
  const r2 = await fileHeader("B_BASS_BPM120.WAV");
  await shot("12_reloaded_t2_file");
  await keyTap(UPPER[0]);                       // T1, FILE stays
  const r1 = await fileHeader("A_DRUMS_BPM120.WAV");
  await shot("13_reloaded_t1_file");
  await tap("SEQ");
  ok(!!r1 && !!r2, `after a reload (folder ${fst.name}, ${fst.state}): the project is back, T2 B_BASS_BPM120.WAV ${r2 ? "ok" : "missing"}, T1 A_DRUMS_BPM120.WAV ${r1 ? "ok" : "missing"}`);
  await tap("PLAY");
  await sleep(3000);
  const l3 = await level(2500);
  nums.play3 = l3;
  ok(l3.rms > 0.005 && l3.peak > 0.02, `after the reload PLAY plays (${fmtL(l3)})`);
  await shot("14_reloaded_playing");

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
