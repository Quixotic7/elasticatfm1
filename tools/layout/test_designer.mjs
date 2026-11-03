// SPDX-License-Identifier: GPL-3.0-only
// The layout designer end to end, headless (headless Chromium over the DevTools protocol, as host/web_e2e.mjs; a
// fresh profile; python's http.server; both ended by their PIDs; no window):
//   1. the page loads, the preview (the firmware in WebAssembly) boots the demo project and plays
//   2. the designer's own pack of the shipped design = gen_layout.py --pack (JS = Python), and the firmware's pack of
//      what it loaded = the same (C = Python): the preview runs exactly what the build compiles
//   3. a param dragged onto another slot through the page's API (designer.drop, what a drag and drop calls): FILT /
//      FILTER (classic) KNOB 1 CUT <-> KNOB 2 RES. The preview's pixels change where the cells' labels are, the knob
//      turns the new param, the live panel labels follow, and Export is exactly the shipped JSON with that swap
//      (compared as data, and as text with gen_layout.py --format's canonical form)
//   4. undo: the preview's pixels back to the first frame's; 4b. the MATRIX page: MOD's first (tree and preview),
//      its slots fixed (drops refused), never in the tray; a design without one opened: inserted, in the changes
//   5. a page moved to another category with a new lane; a bad title refused
//  5b. the modes (0.6.0): the tree groups the modes by their FM-1 button in order (HOME: LOOP ENGINE -> TRIG ->
//      KEYBOARD), a mode's pages with their black keys = the firmware's (lw_modes); a key set on a page opens it on the
//      unit and a key taken swaps; a page moved between modes keeps its key where it is free (and the preview's
//      firmware maps it); a design before 0.6.0 opened gets CROSSFADER with MST's SCENE page, in the changes
//   6. every kind of <select> (context, a page's picture, MOVE TO, VARIANT, + VARIANT FOR, OPEN) opened with the
//      mouse stays: 2 s of the demo playing, the same element, focused, its value kept
//   7. descriptions: a slot's and a page header's tooltip, the inspector, the tray's search over descriptions
//   8. the drawn FM-1 by the mouse (0.6.0's gestures): SEQ -> PATTERN LAUNCH, HOME twice -> LOOP ENGINE -> TRIG, the
//      key LEDs light for trigs, Shift-click latches SHIFT (the live labels change), a white key latched + the wheel
//      on KN1 = a p-lock (the header's step box in LOCK, K1's filled cell)
//   9. the columns: the tree ~30% wide at 1280 and 2000 px with the screen and the FM-1 whole, lanes capped; the tree
//      splitter dragged, kept across a reload, a double-click = the default (09_columns_2000.png at 2000x1050)
// Screenshots + SUMMARY.txt into results/layout/.
//   node tools/layout/test_designer.mjs [--web=build/layout-web] [--out=results/layout]
import { spawn, spawnSync } from "node:child_process";
import { mkdtempSync, mkdirSync, writeFileSync, readFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { fileURLToPath } from "node:url";

const REPO = fileURLToPath(new URL("../../", import.meta.url));
const arg = (k, d) => { const a = process.argv.find((x) => x.startsWith(`--${k}=`)); return a ? a.slice(k.length + 3) : d; };
const WEB = arg("web", join(REPO, "build/layout-web"));
const OUT = arg("out", join(REPO, "results/layout"));
mkdirSync(OUT, { recursive: true });
const CHROMIUM = process.env.CHROMIUM || "/Applications/Chromium.app/Contents/MacOS/Chromium";
const PORT = 8100 + Math.floor(Math.random() * 800), DEBUG = 9300 + Math.floor(Math.random() * 600);
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const lines = [];
const log = (s) => { console.log(s); lines.push(s); };
let fails = 0, checks = 0;
const ok = (c, what) => { checks++; if (!c) fails++; log(`${c ? "ok  " : "FAIL"} ${what}`); return c; };

const tmp = mkdtempSync(join(tmpdir(), "ec-layout-e2e-"));
const server = spawn("python3", ["-m", "http.server", String(PORT), "--bind", "127.0.0.1", "--directory", WEB], { stdio: "ignore" });
const chrome = spawn(CHROMIUM, ["--headless=new", "--use-mock-keychain", "--password-store=basic", "--disable-gpu",
  "--no-sandbox", "--no-first-run", `--user-data-dir=${join(tmp, "profile")}`, "--hide-scrollbars",
  "--window-size=1600,1000", "--mute-audio", `--remote-debugging-port=${DEBUG}`, "about:blank"], { stdio: "ignore" });
const stop = () => { try { process.kill(chrome.pid); } catch {} try { process.kill(server.pid); } catch {} };
process.on("exit", stop);
process.on("SIGINT", () => process.exit(130));

let ws, id = 0;
const waiting = new Map();
const cdp = (method, params = {}) => new Promise((r) => { const i = ++id; waiting.set(i, r); ws.send(JSON.stringify({ id: i, method, params })); });
async function js(expression) {
  const r = await cdp("Runtime.evaluate", { expression, awaitPromise: true, returnByValue: true });
  if (r.result?.exceptionDetails) throw new Error(`page: ${JSON.stringify(r.result.exceptionDetails).slice(0, 500)}`);
  return r.result?.result?.value;
}
async function shot(name) {
  const r = await cdp("Page.captureScreenshot", { format: "png" });
  const f = join(OUT, `${name}.png`);
  writeFileSync(f, Buffer.from(r.result.data, "base64"));
  log(`  shot ${f.slice(REPO.length)}`);
}
// the cells' label row of KNOB 1 and KNOB 2 (the mockup's grid y 79..84 at scale 2: rows 156..170, x 0..240)
const region = (px) => { const out = []; for (let y = 156; y < 170; y++) for (let x = 0; x < 240; x++) for (let c = 0; c < 3; c++) out.push(px[(y * 240 + x) * 4 + c]); return out; };
const ndiff = (a, b) => a.reduce((n, v, i) => n + (v !== b[i]), 0);

try {
  let targets;
  for (let i = 0; i < 50 && !targets; i++) { await sleep(200); try { targets = await (await fetch(`http://127.0.0.1:${DEBUG}/json/list`)).json(); } catch {} }
  ws = new WebSocket(targets.find((t) => t.type === "page").webSocketDebuggerUrl);
  await new Promise((r, j) => { ws.onopen = r; ws.onerror = j; });
  const exceptions = [];
  ws.onmessage = (m) => {
    const d = JSON.parse(m.data);
    if (d.id && waiting.has(d.id)) { waiting.get(d.id)(d); waiting.delete(d.id); }
    if (d.method === "Runtime.exceptionThrown") { exceptions.push(JSON.stringify(d.params.exceptionDetails).slice(0, 400)); console.error("exception:", exceptions.at(-1)); }
  };
  await cdp("Runtime.enable"); await cdp("Page.enable");
  for (let i = 0; i < 50; i++) { try { await fetch(`http://127.0.0.1:${PORT}/index.html`); break; } catch { await sleep(100); } }
  await cdp("Page.navigate", { url: `http://127.0.0.1:${PORT}/index.html` });
  let ready = false;
  for (let i = 0; i < 300 && !ready; i++) { await sleep(100); ready = await js("!!(window.designer && (window.designer.ready || window.designer.error))"); }
  const err = await js("window.designer && window.designer.error");
  ok(ready && !err, `1. the page and its preview start${err ? ": " + err : ""}`);
  await sleep(1500);
  const st0 = await js("designer.state()");
  ok(st0.playing === 1, `   the demo project plays (${st0.title}, ${st0.ms} device ms)`);
  await shot("01_designer");

  const shippedPack = await js("designer.shippedPack()");
  ok(shippedPack.length > 1000 && (await js("designer.jsPackOfShipped()")) === shippedPack,
    `2. the designer's pack of the shipped design = gen_layout.py --pack (${shippedPack.length} bytes)`);
  ok((await js("designer.cPack()")) === shippedPack, "   the firmware's pack of the layout it loaded = the same");
  const c0 = await js("designer.compiled()");
  // (the owner's long cell labels are theirs: "longer than 5" warnings are allowed; nothing else)
  const w0 = c0.warnings.filter((w) => !/longer than 5/.test(w));
  ok(c0.errors.length === 0 && w0.length === 0, `   the shipped design: ${c0.errors.length} errors, ${w0.length} warnings besides long labels${w0.length ? ": " + w0[0] : ""}`);

  // 3. the swap
  await js("designer.context({ filt: 1 }), designer.goto('FILT', 0, 0), true");
  await js("designer.run(200)");
  const px0 = region(await js("designer.pixels()"));
  const s1 = await js("designer.state()");
  // (the labels the design gives cutoff and resonance there: CUT / RES, or the owner's CUTOFF / RESO)
  const [CUT, RES] = [s1.items[0].label, s1.items[1].label];
  ok(s1.title === "FILTER" && /^CUT(OFF)?$/.test(CUT) && /^RESO?$/.test(RES),
    `3. on FILT/FILTER (classic): KNOB 1 ${s1.items[0].label}, KNOB 2 ${s1.items[1].label}`);
  const vi = await js("designer.variantOf('FILT', 0)");
  const moved = await js(`designer.drop({ ref: designer.ref("page", { cat: "FILT", pi: 0, vi: ${vi}, lane: 0, k: 0 }) },
                                       { ref: designer.ref("page", { cat: "FILT", pi: 0, vi: ${vi}, lane: 0, k: 1 }) })`);
  await js("designer.goto('FILT', 0, 0), designer.run(120), true");
  const s2 = await js("designer.state()");
  const px1 = region(await js("designer.pixels()"));
  ok(moved && s2.items[0].label === RES && s2.items[1].label === CUT, `   dropped CUT onto KNOB 2: KNOB 1 ${s2.items[0].label}, KNOB 2 ${s2.items[1].label}`);
  ok(ndiff(px0, px1) > 20, `   the preview's pixels changed where the labels are (${ndiff(px0, px1)} values)`);
  ok(s2.labels[3] === RES && s2.labels[4] === CUT, `   the live panel labels: KNOB 1 ${s2.labels[3]}, KNOB 2 ${s2.labels[4]}`);
  const res0 = s2.items[0].value;
  await js("designer.turn(3, 5), designer.run(60), true");
  const s3 = await js("designer.state()");
  ok(s3.items[0].value !== res0 && s3.items[1].value === s2.items[1].value, `   KNOB 1 turns RES now (${res0} -> ${s3.items[0].value}), CUT stays ${s3.items[1].value}`);
  await js("designer.turn(3, -5), designer.run(60), true");
  await shot("02_after_swap");
  const exported = await js("designer.exportText()");
  const want = JSON.parse(readFileSync(join(WEB, "shipped.json"), "utf8"));
  const page = want.categories.find((c) => c.cat === "FILT").pages[0];
  const lanes = vi >= 0 ? page.variants[vi].lanes : page.lanes;
  [lanes[0][0], lanes[0][1]] = [lanes[0][1], lanes[0][0]];
  ok(JSON.stringify(JSON.parse(exported)) === JSON.stringify(want), "   Export = the shipped JSON with that swap (as data)");
  const f = join(tmp, "want.json");
  writeFileSync(f, JSON.stringify(want));
  const fmt = spawnSync("python3", [join(REPO, "tools/layout/gen_layout.py"), f, "--format", "--quiet"], { encoding: "utf8" });
  ok(fmt.status === 0 && readFileSync(f, "utf8") === exported, "   Export's text = gen_layout.py --format's canonical text");
  writeFileSync(join(OUT, "swap-cut-res.json"), exported);

  // 4. undo
  await js("designer.undo(), designer.goto('FILT', 0, 0), designer.run(120), true");
  const s4 = await js("designer.state()");
  const px2 = region(await js("designer.pixels()"));
  ok(s4.items[0].label === CUT && ndiff(px0, px2) === 0, `4. undo: KNOB 1 ${s4.items[0].label}, the label row as before (${ndiff(px0, px2)} values differ)`);
  ok((await js("designer.exportText()")) === readFileSync(join(WEB, "shipped.json"), "utf8"), "   Export = the shipped file again, byte for byte");

  // 4b. the MATRIX page (0.5.0): MOD's first page in the tree and on the unit, its slots fixed, its items and the
  // matrix / retired params never in the tray; a design without one gets it inserted on open (shown in the changes)
  const t4 = await js(`(() => { const e = document.querySelector('.page[data-cat="MOD"][data-pi="0"]'); return e && { title: e.querySelector(".title").value,
    fixed: [...e.querySelectorAll(".slot")].map((x) => x.dataset.fixed === "1" && x.getAttribute("draggable") === "false"),
    ops: e.querySelectorAll(".lops button, .addrow button, .vrow").length, pic: e.querySelector(".prow .fixed")?.textContent }; })()`);
  ok(t4 && t4.title === "MATRIX" && t4.fixed.length === 4 && t4.fixed.every(Boolean) && t4.ops === 0 && /mod matrix/.test(t4.pic),
    `4b. the tree: MOD's first page is ${t4 && t4.title} (${t4 && t4.pic}), its 4 slots fixed (${t4 && t4.fixed.join(" ")}), ${t4 && t4.ops} lane / variant controls`);
  await js("designer.goto('MOD', 0, 0), designer.run(120), true");
  const s4b = await js("designer.state()");
  ok(s4b.cat === 5 && s4b.page === 0 && s4b.title === "MATRIX", `   the preview: MOD page ${s4b.page + 1} is ${s4b.title}`);
  await shot("02b_matrix");
  const mref = (k) => `designer.ref("page", { cat: "MOD", pi: 0, lane: 0, k: ${k} })`;
  const d4a = await js("designer.exportText()");
  const r4 = await js(`[designer.drop({ ref: ${mref(0)} }, { ref: ${mref(3)} }), designer.drop({ name: "lfo1_wave" }, { ref: ${mref(3)} }),
    designer.drop({ ref: designer.ref("page", { cat: "MOD", pi: 1, lane: 0, k: 0 }) }, { ref: ${mref(1)} }), designer.drop({ ref: ${mref(2)} }, { tray: true }),
    designer.drop({ name: "mtx.src" }, { ref: designer.ref("page", { cat: "MOD", pi: 1, lane: 0, k: 3 }) })]`);
  ok(r4.every((x) => x === false) && (await js("designer.exportText()")) === d4a, `   drops onto / off its slots refused (${r4.join(" ")}), the design unchanged`);
  const tr = await js(`[designer.trayNames("mtx"), designer.trayNames("depth"), designer.trayNames("dest")]`);
  const leak = tr.flat().filter((n) => /^mtx/.test(n) || /_dest$/.test(n) || /^macro\d_.*_depth$/.test(n));
  ok(leak.length === 0 && tr[1].includes("lfo1_depth"), `   the tray never has mtx.* / mtx1_* / retired params (searches "mtx" ${tr[0].length}, "depth" ${tr[1].length}, "dest" ${tr[2].length}: ${leak.join(" ") || "none of them"})`);
  const old = JSON.parse(readFileSync(join(WEB, "shipped.json"), "utf8"));
  const om = old.categories.find((c) => c.cat === "MOD");
  om.pages = om.pages.filter((p) => p.picture !== "matrix");
  om.pages.find((p) => p.title === "MACRO 1").lanes.push(["macro1_pitch_depth"]);
  const opened = await js(`designer.open(${JSON.stringify(old)}, "a 0.4 design")`);
  const d4c = await js("designer.design()");
  const mp = d4c.categories.find((c) => c.cat === "MOD").pages;
  const c4 = await js("designer.compiled()");
  const diff4 = await js(`document.querySelector("#difflist").textContent`);
  ok(opened && mp[0].title === "MATRIX" && mp[0].picture === "matrix" && c4.errors.length === 0 && /MATRIX inserted/.test(diff4) &&
    c4.warnings.some((w) => /macro1_pitch_depth is retired/.test(w)),
    `   a design without one opened: MOD/${mp[0].title} inserted first (changes: "${(diff4.match(/OPENED added[^]*?page\)/) || [""])[0]}"), a retired item kept with a warning`);
  await js("designer.undo(), true");
  ok((await js("designer.exportText()")) === readFileSync(join(WEB, "shipped.json"), "utf8"), "   undo: the shipped file again");

  // 5. pages: FILT's third page (F.MIX, the owner's MIX) moved from FILT to MOD, a lane added there with DRIV; a title
  // in lower case is refused (the preview keeps the last good layout)
  const nfilt = (await js("designer.design()")).categories.find((c) => c.cat === "FILT").pages.length;
  const fmix = (await js("designer.design()")).categories.find((c) => c.cat === "FILT").pages[2].title;
  await js("designer.movePage('FILT', 2, 'MOD', 99), true");
  await js("designer.edit((d) => { const p = d.categories.find((c) => c.cat === 'MOD').pages.at(-1); p.lanes.push(['filter_drive']); }), true");
  const d5 = await js("designer.design()");
  const mod = d5.categories.find((c) => c.cat === "MOD").pages;
  ok(mod.at(-1).title === fmix && d5.categories.find((c) => c.cat === "FILT").pages.length === nfilt - 1,
    `5. FILT/${fmix} moved to MOD (page ${mod.length}), FILT has ${nfilt - 1} pages`);
  await js(`designer.goto('MOD', ${mod.length - 1}, 1), designer.run(120), true`);
  const s5 = await js("designer.state()");
  ok(s5.title === fmix && s5.lanes === 2 && s5.items[4] && s5.items[4].label === "DRIV",
    `   the preview: MOD/${s5.title}, ${s5.lanes} lanes, lane 2 KNOB 1 ${s5.items[4] && s5.items[4].label} (labels ${s5.labels.slice(3, 7).join(" ")})`);
  await shot("03_page_moved");
  const f5 = join(tmp, "moved.json");
  writeFileSync(f5, await js("designer.exportText()"));
  const chk = spawnSync("python3", [join(REPO, "tools/layout/gen_layout.py"), f5, "--check"], { encoding: "utf8" });
  ok(chk.status === 0, `   gen_layout.py --check of the export: ${(chk.stdout + chk.stderr).trim().split("\n").at(-1)}`);
  const good = await js("designer.cPack()");
  await js("designer.edit((d) => { d.categories[0].pages[0].title = 'trig'; }), true");
  const c6 = await js("designer.compiled()");
  ok(c6.errors.length === 1 && /lower case/.test(c6.errors[0]) && (await js("designer.cPack()")) === good,
    `   a lower-case title: ${c6.errors.length} error (${c6.errors[0]}), the preview keeps the last good layout`);
  await js("designer.undo(), designer.undo(), designer.undo(), true");
  ok((await js("designer.exportText()")) === readFileSync(join(WEB, "shipped.json"), "utf8"), "   three undos: the shipped file again");

  // 5b. the modes
  const BTNS = ["FX", "SEL", "ENV", "LFO", "EDIT", "HOME", "FILE", "ARP", "SEQ"];
  const shipped = JSON.parse(readFileSync(join(WEB, "shipped.json"), "utf8"));
  const tree = await js(`[...document.querySelectorAll("#tree .btngrp")].map((g) => ({ b: g.dataset.button,
    modes: [...g.querySelectorAll(":scope > .cat")].map((c) => c.dataset.mode || c.dataset.screen) }))`);
  const home = tree.find((g) => g.b === "HOME");
  ok(tree.length === 9 && tree.map((g) => g.b).join() === BTNS.join() && home && home.modes.join(" > ") === "LOOP ENGINE > TRIG > KEYBOARD" &&
    tree.find((g) => g.b === "EDIT").modes.join(" > ") === "MIX > PAGES" && tree.find((g) => g.b === "ARP").modes.join() === "CROSSFADER",
    `5b. the tree: the modes by button (${tree.map((g) => g.b + ": " + g.modes.join(" > ")).join(" | ")})`);
  const fw = await js("designer.modes()");
  const keysIn = (cat) => js(`[...document.querySelectorAll('.page[data-cat="${cat}"]:not(.setup) select.key')].map((e) => +e.value)`);
  let same = true;
  for (const c of shipped.categories) {
    const ci = ["TRIG", "SRC", "FILT", "AMP", "FX", "MOD", "MST", "FILE", "PAT", "XFD"].indexOf(c.cat);
    const tk = await keysIn(c.cat);
    const fk = c.pages.map((_, pi) => fw[ci].keys.indexOf(pi) + 1);
    if (tk.join() !== fk.join() || fw[ci].name !== c.mode || BTNS[fw[ci].button - 1] !== c.button || fw[ci].order !== c.order) { same = false; log(`  ${c.cat}: tree ${tk} firmware ${fk} ${fw[ci].name}`); }
  }
  const amp = fw[3], fx = fw[4], modm = fw[5];
  ok(same && amp.keys.slice(0, 3).join() === "0,1,2" && fx.keys[10] === 2 && fx.bus.slice(2, 5).join() === "1,2,0" && modm.keys[0] === 0 && modm.keys[10] === 4 && fw[10].name === "KEYBOARD",
    `   every mode's pages and black keys in the tree = the firmware's (AMP B1-B3 ${amp.keys.slice(0, 3)}, FX B11 page ${fx.keys[10]} and buses ${fx.bus.slice(2, 5)}, MOD B1 / B11 ${modm.keys[0]} / ${modm.keys[10]})`);
  await shot("03b_modes");
  // a key set on FILT's MIX page (B3 -> B5), then onto B1 (taken by FILTER: the two swap)
  await js("designer.setKey('FILT', 2, 5), true");
  const k1 = (await js("designer.modes()"))[2].keys;
  await js("designer.goto('FILT', 2, 0), designer.run(60), true");
  const sk = await js("designer.state()");
  await js("designer.setKey('FILT', 2, 1), true");
  const k2 = (await js("designer.modes()"))[2].keys;
  const fd = (await js("designer.design()")).categories.find((c) => c.cat === "FILT").pages;
  ok(k1[4] === 2 && k1[2] === -1 && sk.key === 5 && sk.mode === "FILTER" && k2[0] === 2 && k2[4] === 0 && fd[0].key === 5 && fd[2].key === 1,
    `   FILT/MIX set to B5: the firmware's B5 opens it (the unit on ${sk.mode}/${sk.title} says B${sk.key}); then B1: FILTER takes its B5 (B1 -> page ${k2[0] + 1}, B5 -> page ${k2[4] + 1})`);
  await js("designer.undo(), designer.undo(), true");
  // a page moved between modes: AMP's MOD ENV (B3) to MODULATION, where B3 is MACRO 2's: it arrives without a key,
  // AMP keeps its B1 B2; given a free key there (B8), the unit opens it by that key
  await js("designer.movePage('AMP', 2, 'MOD', 99), true");
  const d5b = await js("designer.design()");
  const modp = d5b.categories.find((c) => c.cat === "MOD").pages, ampp = d5b.categories.find((c) => c.cat === "AMP").pages;
  const fw2 = await js("designer.modes()");
  const lastPi = modp.length - 1;
  ok(modp[lastPi].title === "MOD ENV" && !modp[lastPi].key && ampp.length === 2 && ampp.map((p) => p.key).join() === "1,2" && fw2[5].keys.indexOf(lastPi) < 0 && fw2[3].keys.slice(0, 3).join() === "0,1,-1",
    `   AMP/MOD ENV (B3) moved to MODULATION (B3 is MACRO 2's there): it arrives keyless, AMP keeps B1 B2 (firmware: AMP ${fw2[3].keys.slice(0, 3)}, MOD keys ${fw2[5].keys})`);
  await js(`designer.setKey('MOD', ${lastPi}, 8), true`);
  await js(`designer.goto('MOD', ${lastPi}, 0), designer.run(60), true`);
  const sm = await js("designer.state()");
  ok(sm.mode === "MODULATION" && sm.title === "MOD ENV" && sm.key === 8 && (await js("designer.modes()"))[5].keys[7] === lastPi,
    `   given B8 there: the unit's MODULATION/${sm.title} is opened by B${sm.key}`);
  await js("designer.undo(), designer.undo(), true");
  ok((await js("designer.exportText()")) === readFileSync(join(WEB, "shipped.json"), "utf8"), "   undos: the shipped file again");
  // a design before 0.6.0 (no XFD, no modes, no keys): opened, it gets CROSSFADER with MST's SCENE page and the modes
  const legacy = JSON.parse(readFileSync(join(REPO, "host/fixtures/layout-0.4.0.json"), "utf8"));
  const op = await js(`designer.open(${JSON.stringify(legacy)}, "the 0.4.0 layout")`);
  const dl = await js("designer.design()");
  const xf = dl.categories.find((c) => c.cat === "XFD"), ms = dl.categories.find((c) => c.cat === "MST");
  const diffL = await js(`document.querySelector("#difflist").textContent`);
  const cl = await js("designer.compiled()");
  const fwl = await js("designer.modes()");
  ok(op && xf && xf.pages[0].picture === "scene" && !ms.pages.some((p) => p.picture === "scene") && xf.mode === "CROSSFADER" && xf.button === "ARP" &&
    dl.categories.find((c) => c.cat === "SRC").pages.map((p) => p.key).join() === "1,2,3,4" && dl.screens?.length === 3 && cl.errors.length === 0 &&
    /moved to CROSSFADER/.test(diffL) && fwl[9].keys.every((k) => k === -1) && fwl[1].name === "LOOP ENGINE",
    `   the 0.4.0 layout opened: XFD/${xf && xf.pages[0].title} from MST, the modes and keys written (LOOP ENGINE ${dl.categories.find((c) => c.cat === "SRC").pages.map((p) => "B" + p.key).join(" ")}), ${cl.errors.length} errors`);
  await js("designer.undo(), true");
  ok((await js("designer.exportText()")) === readFileSync(join(WEB, "shipped.json"), "utf8"), "   undo: the shipped file again");

  // 6. dropdowns stay open while the preview plays: each kind of <select> clicked with the mouse (CDP input), 2 s of
  // the demo playing, the same element still in the page and focused (nothing rebuilt it), its value kept
  const mouse = async (type, x, y, extra = {}) => cdp("Input.dispatchMouseEvent", { type, x, y, button: type === "mouseMoved" || type === "mouseWheel" ? "none" : "left", clickCount: 1, ...extra });
  const click = async (x, y, extra = {}) => { await mouse("mousePressed", x, y, extra); await mouse("mouseReleased", x, y, extra); };
  const key = async (k, code, vk) => { await cdp("Input.dispatchKeyEvent", { type: "keyDown", key: k, code, windowsVirtualKeyCode: vk }); await cdp("Input.dispatchKeyEvent", { type: "keyUp", key: k, code, windowsVirtualKeyCode: vk }); };
  const centre = (q) => js(`(() => { const e = ${q}; if (!e) return null; e.scrollIntoView({ block: "center" }); window.__sel = e; const r = e.getBoundingClientRect(); return { x: r.x + r.width / 2, y: r.y + Math.min(r.height / 2, 8), v: e.value }; })()`);
  const sels = [["the context's MACHINE", `document.querySelector('[data-key="ctx-MACHINE"]')`],
    ["a page's picture", `document.querySelector('.page[data-cat="FILT"][data-pi="1"] [data-key^="pic-"]')`],
    ["a page's MOVE TO", `document.querySelector('.page[data-cat="MOD"][data-pi="1"] [data-key^="move-"]')`],
    ["a page's VARIANT", `document.querySelector('.page[data-cat="FILT"][data-pi="0"] [data-key^="var-"]')`],
    ["+ VARIANT FOR", `document.querySelector('.page[data-cat="AMP"][data-pi="0"] [data-key^="addvar-"]')`],
    ["a page's black key", `document.querySelector('.page[data-cat="FILT"][data-pi="1"] [data-key^="key-"]')`],
    ["OPEN", `document.querySelector("#open")`]];
  for (const [what, q] of sels) {
    const c = await centre(q);
    if (!ok(!!c, `6. ${what}: found`)) continue;
    const ms0 = (await js("designer.state()")).ms;
    await click(c.x, c.y);
    await sleep(2000);
    const r = await js(`[document.contains(window.__sel), document.activeElement === window.__sel, window.__sel.value, designer.state().ms]`);
    ok(r[0] && r[1] && r[2] === c.v && r[3] > ms0 + 500, `   ${what}: opened, 2 s of the demo playing (${r[3] - ms0} device ms): the same element, focused, value kept (${r[0]}, ${r[1]}, ${JSON.stringify(r[2])})`);
    await key("Escape", "Escape", 27);
    await js("document.activeElement && document.activeElement.blur(), true");
  }

  // 7. descriptions: a slot's and a page header's tooltip, the inspector, the tray's search over descriptions
  const desc = await js("designer.desc()");
  ok(Object.keys(desc.items).length >= 225 && Object.keys(desc.pages).length >= 40, `7. descriptions loaded: ${Object.keys(desc.items).length} items, ${Object.keys(desc.pages).length} pages`);
  await js("designer.context({ filt: 1 }), true");
  const sc = await centre(`document.querySelector('.page[data-cat="FILT"][data-pi="0"] .lane .slot')`);
  await mouse("mouseMoved", sc.x - 5, sc.y + 30); await mouse("mouseMoved", sc.x, sc.y);
  await sleep(600);
  const tip1 = await js("(() => { const t = document.querySelector('#tip'); return t.hidden ? '' : t.textContent; })()");
  ok(tip1.includes(desc.items.filter_cutoff.desc) && tip1.includes(desc.items.filter_cutoff.name), `   hover FILT/FILTER KNOB 1: the tooltip is filter_cutoff's ("${tip1.split("\n")[1].slice(0, 60)}...")`);
  await shot("04_tooltip_slot");
  const pc = await centre(`document.querySelector('.page[data-cat="FILT"][data-pi="0"] .prow')`);
  await mouse("mouseMoved", pc.x + 120, pc.y + 1);
  await sleep(600);
  const tip2 = await js("(() => { const t = document.querySelector('#tip'); return t.hidden ? '' : t.textContent; })()");
  ok(tip2.includes(desc.pages["FILT/FILTER"]) && tip2.includes(desc.options.filter.CLASSC), `   hover the FILT/FILTER header: the page's description and its variant's (CLASSC)`);
  await shot("05_tooltip_page");
  const sc2 = await centre(`document.querySelector('.page[data-cat="FILT"][data-pi="0"] .lane .slot')`);
  await click(sc2.x, sc2.y);
  await sleep(200);
  const insp = await js("[...document.querySelectorAll('#inspector .desc')].map((e) => e.textContent)");
  ok(insp[0] === desc.items.filter_cutoff.desc && insp[1] === desc.pages["FILT/FILTER"], `   the inspector: the slot's description and the page's (${insp.length} blocks: ${insp.map((x) => x.slice(0, 40)).join(" / ")})`);
  await js(`(() => { const s = document.querySelector("#search"); s.value = "semitones"; s.dispatchEvent(new Event("input")); return true; })()`);
  const found = await js("[...document.querySelectorAll('#traylist .chip')].map((e) => e.querySelector('.c') && e.dataset.tip.split(' · ')[1].split(/[\\s(]/)[0])");
  ok(found.includes("pitch") && found.includes("harm_interval"), `   the tray's search finds descriptions: "semitones" -> ${found.length} (${found.slice(0, 6).join(" ")})`);
  await js(`(() => { const s = document.querySelector("#search"); s.value = ""; s.dispatchEvent(new Event("input")); return true; })()`);
  await mouse("mouseMoved", 5, 5);

  // 8. the FM-1 panel, by the mouse (CDP input on the drawn panel: the same path as a click)
  const at = async (kind, idx) => { const p = await js(`(() => { const c = document.querySelector("#panel"); c.scrollIntoView({ block: "end" }); return designer.panel.where(${JSON.stringify(kind)}, ${idx}); })()`); return p; };
  await js("designer.goto('SRC', 0, 0), designer.run(60), true");
  const seq = await at("btn", 9);
  await mouse("mousePressed", seq.x, seq.y); await sleep(150); await mouse("mouseReleased", seq.x, seq.y);
  await sleep(300);
  const s8 = await js("designer.state()");
  const lbl8 = await js("designer.panel.labels()");
  // (SEQ opens the layout's SEQ mode: PATTERN LAUNCH)
  const seqMode = (await js("designer.modes()")).find((m) => m.button === 9);
  ok(seqMode && s8.cat === seqMode.mode && s8.mode === "PATTERN LAUNCH", `8. the panel's SEQ button (drawn "${lbl8.buttons[9]}") pressed: the preview is on ${s8.mode}/${s8.title}`);
  // HOME, then HOME again: LOOP ENGINE, then TRIG (0.6.0: the white keys are the steps there; their LEDs the trigs)
  const homeB = await at("btn", 6);
  const modesSeen = [];
  for (let i = 0; i < 2; i++) {
    await mouse("mousePressed", homeB.x, homeB.y); await sleep(120); await mouse("mouseReleased", homeB.x, homeB.y);
    await sleep(300);
    modesSeen.push((await js("designer.state()")).mode);
  }
  ok(modesSeen.join(" > ") === "LOOP ENGINE > TRIG", `   the panel's HOME, tapped twice: ${modesSeen.join(" > ")}`);
  let maxLit = 0, litKeys = new Set();
  for (let i = 0; i < 20; i++) { const lv = await js("designer.panel.keyLevels()"); lv.forEach((v, k) => { if (k <= 26 && v >= 176) litKeys.add(k); maxLit = Math.max(maxLit, v); }); await sleep(60); }
  ok(litKeys.size > 0, `   the key LEDs light for the trigs (keys lit over 1.2 s: ${[...litKeys].join(" ")})`);
  await shot("06_panel_trig");
  // SHIFT (GLO) latched with a Shift-click: the live labels on the buttons change
  const glo = await at("btn", 5);
  const before = (await js("designer.panel.labels()")).buttons;
  await click(glo.x, glo.y, { modifiers: 8 });
  await sleep(300);
  const held = await js("designer.panel.held()");
  const after = (await js("designer.panel.labels()")).buttons;
  const changed = after.map((b, i) => (b !== before[i] ? `${before[i]}->${b}` : null)).filter(Boolean);
  ok(held.btn[5] && changed.length >= 3, `   Shift-click latches SHIFT: live labels on the buttons change (${changed.join(" ")})`);
  await shot("07_panel_shift_labels");
  await click(glo.x, glo.y);
  await sleep(200);
  ok(!(await js("designer.panel.held()")).btn[5], "   a click on it lets SHIFT go");
  // a lower key (step 1) latched + KNOB 1 turned on the panel: the step's lock (the screen's lock frame, K1's filled chip)
  await js("designer.context({ filt: 1 }), designer.goto('FILT', 0, 0), designer.run(60), true");
  const orange = (px, x0, y0, x1, y1) => { let n = 0; for (let y = y0; y < y1; y++) for (let x = x0; x < x1; x++) { const o = (y * 240 + x) * 4; if (px[o] > 225 && px[o + 1] > 80 && px[o + 1] < 130 && px[o + 2] < 70) n++; } return n; };
  const pxA = await js("designer.pixels()");
  const k0 = await at("key", 0);
  await click(k0.x, k0.y, { modifiers: 8 });
  await sleep(250);
  const pxB = await js("designer.pixels()");
  // (0.6.0: a held step lights the header's page box in the LOCK colour, its number dark on it)
  const frameA = orange(pxA, 210, 0, 240, 20), frameB = orange(pxB, 210, 0, 240, 20);
  ok((await js("designer.panel.held()")).key[0] && frameB > 200 && frameA < 50, `   Shift-click on the white key 1: held, the header's step box in the LOCK colour (orange px ${frameA} -> ${frameB})`);
  const chipA = orange(pxB, 4, 168, 120, 196);
  const kn1 = await at("knob", 3);
  for (let i = 0; i < 6; i++) { await mouse("mouseWheel", kn1.x, kn1.y, { deltaX: 0, deltaY: -60 }); await sleep(40); }
  await sleep(250);
  const pxC = await js("designer.pixels()");
  const chipB = orange(pxC, 4, 168, 120, 196);
  ok(chipB > chipA + 40, `   the wheel on the panel's KN1: KNOB 1 locked on that step (K1's orange chip ${chipA} -> ${chipB} px)`);
  await shot("08_panel_plock");
  await click(k0.x, k0.y);
  await sleep(150);
  ok(!(await js("designer.panel.held()")).key[0], "   a click on the key lets it go");

  // 9. the columns: the tree ~30% of the width by default, the preview whole; the splitter drags it, a reload keeps
  //    it, a double-click gives the default back
  const viewport = async (width, height) => { await cdp("Emulation.setDeviceMetricsOverride", { width, height, deviceScaleFactor: 1, mobile: false }); await sleep(300); };
  const fullyIn = (sel) => js(`(() => { const r = document.querySelector(${JSON.stringify(sel)}).getBoundingClientRect(), c = document.querySelector("#centre").getBoundingClientRect(); return r.left >= c.left - 1 && r.right <= c.right + 1; })()`);
  for (const w of [1280, 2000]) {
    await viewport(w, 1050);
    const c = await js("designer.columns()");
    ok(Math.abs(c.tree / c.main - 0.30) < 0.02 && c.centre >= 470 && (await fullyIn("#panel")) && (await fullyIn("#screenwrap")),
      `9. at ${w} px: the tree ${c.tree} px (${(100 * c.tree / c.main).toFixed(1)}%), side ${c.side}, preview ${c.centre} (screen + FM-1 whole)`);
  }
  const laneW = await js(`Math.max(...[...document.querySelectorAll("#tree .lane")].map((e) => e.offsetWidth))`);
  ok(laneW <= 560, `   at 2000 px a lane is at most 560 px wide (${laneW})`);
  await shot("09_columns_2000");
  const sp = await js(`(() => { const r = document.querySelector("#split0").getBoundingClientRect(); return { x: r.left + r.width / 2, y: r.top + 200 }; })()`);
  const tw0 = (await js("designer.columns()")).tree;
  await mouse("mousePressed", sp.x, sp.y);
  for (let i = 1; i <= 5; i++) { await mouse("mouseMoved", sp.x - 30 * i, sp.y, { button: "left", buttons: 1 }); await sleep(30); }
  await mouse("mouseReleased", sp.x - 150, sp.y);
  await sleep(150);
  const tw1 = (await js("designer.columns()")).tree;
  ok(Math.abs(tw1 - (tw0 - 150)) <= 3, `   dragging the splitter 150 px left: the tree ${tw0} -> ${tw1} px`);
  await cdp("Page.reload");
  ready = false;
  for (let i = 0; i < 300 && !ready; i++) { await sleep(100); ready = await js("!!(window.designer && window.designer.ready)"); }
  const tw2 = (await js("designer.columns()")).tree;
  ok(ready && Math.abs(tw2 - tw1) <= 2, `   after a reload the tree is still ${tw2} px`);
  await mouse("mousePressed", sp.x - 150, sp.y, { clickCount: 1 }); await mouse("mouseReleased", sp.x - 150, sp.y, { clickCount: 1 });
  await mouse("mousePressed", sp.x - 150, sp.y, { clickCount: 2 }); await mouse("mouseReleased", sp.x - 150, sp.y, { clickCount: 2 });
  await sleep(150);
  const tw3 = (await js("designer.columns()")).tree;
  ok(Math.abs(tw3 - tw0) <= 2, `   a double-click on it: back to the default ${tw3} px`);
  ok(exceptions.length === 0, `   no page exceptions (${exceptions.length})`);
} catch (e) {
  ok(false, `the run: ${e.stack || e}`);
} finally {
  writeFileSync(join(OUT, "SUMMARY.txt"), [`tools/layout/test_designer.mjs (${new Date().toISOString()}): ${checks} checks, ${fails} failed`, "", ...lines, ""].join("\n"));
  log(`test_designer: ${checks} checks, ${fails} failed`);
  stop();
  process.exit(fails ? 1 : 0);
}
