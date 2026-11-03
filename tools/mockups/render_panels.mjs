// SPDX-License-Identifier: GPL-3.0-only
// The 0.6.0 mockups' FM-1 panels: tools/mockups/panel.html (RemoteFM1's panel.js) in headless Chromium over the
// DevTools protocol, as tools/layout/test_designer.mjs does it (a fresh profile, python's http.server on the repo, both
// ended by their PIDs, no window). Needs results/ux-v2-mockups/<Mode>_screen.png (build/ec_mock_ux2) first.
//   node tools/mockups/render_panels.mjs [--out=results/ux-v2-mockups]
// -> <Mode>_panel.png (2184 x 1418) and <Mode>_panel.lcd.json (the LCD's box, for check_png.py)
import { spawn } from "node:child_process";
import { mkdtempSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, relative } from "node:path";
import { fileURLToPath } from "node:url";

const REPO = fileURLToPath(new URL("../../", import.meta.url));
const arg = (k, d) => { const a = process.argv.find((x) => x.startsWith(`--${k}=`)); return a ? a.slice(k.length + 3) : d; };
const OUT = arg("out", join(REPO, "results/ux-v2-mockups"));
const CHROMIUM = process.env.CHROMIUM || "/Applications/Chromium.app/Contents/MacOS/Chromium";
const PORT = 8100 + Math.floor(Math.random() * 800), DEBUG = 9300 + Math.floor(Math.random() * 600);
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

const tmp = mkdtempSync(join(tmpdir(), "ec-mockups-"));
const server = spawn("python3", ["-m", "http.server", String(PORT), "--bind", "127.0.0.1", "--directory", REPO], { stdio: "ignore" });
const chrome = spawn(CHROMIUM, ["--headless=new", "--use-mock-keychain", "--password-store=basic", "--disable-gpu",
  "--no-sandbox", "--no-first-run", `--user-data-dir=${join(tmp, "profile")}`, "--hide-scrollbars",
  "--window-size=1200,800", "--mute-audio", `--remote-debugging-port=${DEBUG}`, "about:blank"], { stdio: "ignore" });
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

let code = 0;
try {
  let targets;
  for (let i = 0; i < 50 && !targets; i++) { await sleep(200); try { targets = await (await fetch(`http://127.0.0.1:${DEBUG}/json/list`)).json(); } catch {} }
  ws = new WebSocket(targets.find((t) => t.type === "page").webSocketDebuggerUrl);
  await new Promise((r, j) => { ws.onopen = r; ws.onerror = j; });
  ws.onmessage = (m) => {
    const d = JSON.parse(m.data);
    if (d.id && waiting.has(d.id)) { waiting.get(d.id)(d); waiting.delete(d.id); }
    if (d.method === "Runtime.exceptionThrown") console.error("exception:", JSON.stringify(d.params.exceptionDetails).slice(0, 400));
  };
  await cdp("Runtime.enable"); await cdp("Page.enable");
  await cdp("Emulation.setDeviceMetricsOverride", { width: 1092, height: 709, deviceScaleFactor: 2, mobile: false });
  for (let i = 0; i < 50; i++) { try { await fetch(`http://127.0.0.1:${PORT}/tools/mockups/panel.html`); break; } catch { await sleep(100); } }
  await cdp("Page.navigate", { url: `http://127.0.0.1:${PORT}/tools/mockups/panel.html` });
  let ready = false;
  for (let i = 0; i < 100 && !ready; i++) { await sleep(100); ready = await js("!!window.mockReady"); }
  if (!ready) throw new Error("panel.html did not start");
  for (const file of await js("mock.modes")) {
    const box = await js(`mock.render(${JSON.stringify(file)})`);
    await sleep(50);
    const r = await cdp("Page.captureScreenshot", { format: "png", clip: { x: 0, y: 0, width: 1092, height: 709, scale: 1 } });
    const png = join(OUT, `${file}_panel.png`);
    writeFileSync(png, Buffer.from(r.result.data, "base64"));
    writeFileSync(join(OUT, `${file}_panel.lcd.json`), JSON.stringify(box) + "\n");
    console.log(`${relative(REPO, png)} ${box.w}x${box.h} (${box.layout}, LCD ${box.s} px at ${box.x},${box.y})`);
  }
} catch (e) {
  console.error(e.message || e);
  code = 1;
}
stop();
process.exit(code);
