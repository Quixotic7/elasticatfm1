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
//   4. undo: the preview's pixels back to the first frame's
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
  ok(c0.errors.length === 0 && c0.warnings.length === 0, `   the shipped design: ${c0.errors.length} errors, ${c0.warnings.length} warnings`);

  // 3. the swap
  await js("designer.context({ filt: 1 }), designer.goto('FILT', 0, 0), true");
  await js("designer.run(200)");
  const px0 = region(await js("designer.pixels()"));
  const s1 = await js("designer.state()");
  ok(s1.title === "FILTER" && s1.items[0].label === "CUT" && s1.items[1].label === "RES",
    `3. on FILT/FILTER (classic): KNOB 1 ${s1.items[0].label}, KNOB 2 ${s1.items[1].label}`);
  const vi = await js("designer.variantOf('FILT', 0)");
  const moved = await js(`designer.drop({ ref: designer.ref("page", { cat: "FILT", pi: 0, vi: ${vi}, lane: 0, k: 0 }) },
                                       { ref: designer.ref("page", { cat: "FILT", pi: 0, vi: ${vi}, lane: 0, k: 1 }) })`);
  await js("designer.goto('FILT', 0, 0), designer.run(120), true");
  const s2 = await js("designer.state()");
  const px1 = region(await js("designer.pixels()"));
  ok(moved && s2.items[0].label === "RES" && s2.items[1].label === "CUT", `   dropped CUT onto KNOB 2: KNOB 1 ${s2.items[0].label}, KNOB 2 ${s2.items[1].label}`);
  ok(ndiff(px0, px1) > 20, `   the preview's pixels changed where the labels are (${ndiff(px0, px1)} values)`);
  ok(s2.labels[3] === "RES" && s2.labels[4] === "CUT", `   the live panel labels: KNOB 1 ${s2.labels[3]}, KNOB 2 ${s2.labels[4]}`);
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
  ok(s4.items[0].label === "CUT" && ndiff(px0, px2) === 0, `4. undo: KNOB 1 ${s4.items[0].label}, the label row as before (${ndiff(px0, px2)} values differ)`);
  ok((await js("designer.exportText()")) === readFileSync(join(WEB, "shipped.json"), "utf8"), "   Export = the shipped file again, byte for byte");

  // 5. pages: F.MIX moved from FILT to MOD, a lane added there with DRIV; a title in lower case is refused (the
  // preview keeps the last good layout)
  await js("designer.movePage('FILT', 2, 'MOD', 99), true");
  await js("designer.edit((d) => { const p = d.categories.find((c) => c.cat === 'MOD').pages.at(-1); p.lanes.push(['filter_drive']); }), true");
  const d5 = await js("designer.design()");
  const mod = d5.categories.find((c) => c.cat === "MOD").pages;
  ok(mod.at(-1).title === "F.MIX" && d5.categories.find((c) => c.cat === "FILT").pages.length === 2,
    `5. F.MIX moved to MOD (page ${mod.length}), FILT has 2 pages`);
  await js(`designer.goto('MOD', ${mod.length - 1}, 1), designer.run(120), true`);
  const s5 = await js("designer.state()");
  ok(s5.title === "F.MIX" && s5.lanes === 2 && s5.items[4] && s5.items[4].label === "DRIV",
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
  ok(exceptions.length === 0, `   no page exceptions (${exceptions.length})`);
} catch (e) {
  ok(false, `the run: ${e.stack || e}`);
} finally {
  writeFileSync(join(OUT, "SUMMARY.txt"), [`tools/layout/test_designer.mjs (${new Date().toISOString()}): ${checks} checks, ${fails} failed`, "", ...lines, ""].join("\n"));
  log(`test_designer: ${checks} checks, ${fails} failed`);
  stop();
  process.exit(fails ? 1 : 0);
}
