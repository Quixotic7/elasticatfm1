// SPDX-License-Identifier: GPL-3.0-only
// Elasticat FM-1's layout designer (tools/layout/README.md). The preview is the firmware: Elasticat compiled to
// WebAssembly (ec_layout.js, tools/layout/ec_layout_wasm.c) draws every edit through its own ec_pages.c and screens,
// from the design packed as tools/layout/gen_layout.py packs it (layout_compile.js). The design is a JSON file the
// build compiles in: Export -> design/layout/elasticat.json.
import * as L from "./layout_compile.js";
import { Controls, DeviceView, panelLabels, KM, roleToMatrix } from "./fm1/panel.js";
import { attachPointer, Keyboard } from "./fm1/input.js";

const $ = (s, r = document) => r.querySelector(s);
function h(tag, attrs, ...kids) {
  const e = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs || {})) {
    if (v == null || v === false) continue;
    if (k === "class") e.className = v;
    else if (k.startsWith("on")) e.addEventListener(k.slice(2), v);
    else if (k === "text") e.textContent = v;
    else e.setAttribute(k, v === true ? "" : v);
  }
  for (const c of kids.flat()) if (c != null && c !== false) e.append(c.nodeType ? c : document.createTextNode(String(c)));
  return e;
}
const clone = (x) => JSON.parse(JSON.stringify(x));

// the FM-1 buttons by label (fm1core.h FM1P_B_* order)
const BTN = { FX: 0, SEL: 1, ENV: 2, LFO: 3, EDIT: 4, GLO: 5, HOME: 6, SAVE: 7, ARP: 8, SEQ: 9, PLAY: 10, REC: 11, OCTM: 12, OCTP: 13 };
// 0.6.0: the categories are modes on the buttons (L.modeOf); the modes the bus pages do not show on
const NO_BUS = ["MST", "FILE", "PAT", "XFD"];
const NTH = (o) => ({ 1: "1st", 2: "2nd", 3: "3rd" }[o] || o + "th");
const TARGETS = ["T1", "T2", "T3", "T4", "T5", "T6", "S1", "S2", "MST"];
const PIC_NAMES = { steps: "steps", wave: "waveform · sample", "wave-machine": "waveform · machine",
  "wave-warp": "waveform · warp", "wave-range": "waveform · range", filter: "filter curve", fenv: "filter env",
  aenv: "amp env", fx: "FX picture", sends: "sends", lfo1: "LFO 1 shape", lfo2: "LFO 2 shape", menv: "mod env",
  vel: "velocity", macros: "macros", macro: "one macro · its matrix rows", mix: "mixer", scene: "crossfader", file: "file list",
  razor: "razor editor", pattern: "pattern grid", matrix: "mod matrix", editor: "sample editor" };
const FREE_PICS = L.PICS.filter((p) => !L.FIXED[p]);
const STORE = "elasticat-layout-design-v1";

const S = {
  m: null, api: {}, catalog: [], byName: new Map(), byId: new Map(),
  shipped: null, shippedText: "", shippedPack: "",
  design: null, undo: [], redo: [],
  sel: null,             // the selected slot's ref
  selPage: null,         // {scope, cat, pi, bus}
  view: { cat: "SRC" },
  ctx: { mach: 1, warp: 1, filt: 1, fenv: 2, env: 2, fx: 1 }, busfx: [1, 1, 1], target: 0,
  st: null, compiled: null, lastGood: "", home: new Map(), msg: "",
  inserted: "",          // what opening a design added to it (a MATRIX page for a design made before 0.5.0)
  desc: { cats: {}, pages: {}, options: {}, items: {} },   // design/layout/descriptions.json
  panel: null,           // the drawn FM-1: {view, controls, ...}
};

// ------------------------------------------------------------------------------------------------ the module
async function boot() {
  const m = await window.ElasticatLayout();
  S.m = m;
  const f = (n, r, a) => m.cwrap(n, r, a);
  S.api = {
    boot: f("lw_boot", "number", ["string", "string"]), run: f("lw_run", null, ["number"]),
    rgba: f("lw_rgba", "number", []), load: f("lw_load", "number", ["string"]), error: f("lw_error", "string", []),
    pack: f("lw_pack", "string", []), context: f("lw_context", null, ["number", "number", "number", "number", "number", "number"]),
    busfx: f("lw_bus_fx", null, ["number", "number"]), target: f("lw_target", null, ["number"]),
    goto: f("lw_goto", null, ["number", "number", "number", "number"]), f3: f("lw_f3", null, ["number"]),
    turn: f("lw_turn", null, ["number", "number"]), press: f("lw_press", null, ["number", "number"]),
    state: f("lw_state", "string", []), value: f("lw_value", "string", ["number", "number"]),
    catalog: f("lw_catalog", "string", []),
    button: f("lw_button", null, ["number", "number"]), key: f("lw_key", null, ["number", "number", "number"]),
    master: f("lw_master", null, ["number"]), leds: f("lw_leds", "number", []), wiring: f("lw_wiring", "string", []),
    labels: f("lw_labels", "string", []), modes: f("lw_modes", "string", []),
  };
  S.api.boot("/demo", "/demo.flash");
  S.catalog = JSON.parse(S.api.catalog());
  for (const p of S.catalog) { S.byName.set(p.name, p); S.byId.set(p.i, p); }
  S.api.run(1200);
  S.api.press(BTN.PLAY, 60);
  S.api.run(400);
  S.shippedText = await (await fetch("shipped.json")).text();
  S.shipped = JSON.parse(S.shippedText);
  S.shippedPack = await (await fetch("shipped.pack")).text();
  try { S.desc = await (await fetch("descriptions.json")).json(); } catch (e) { /* no descriptions: no tooltips */ }
  computeHome();
  let start = S.shipped, restored = false;
  try {
    const t = localStorage.getItem(STORE);
    if (t) { const d = JSON.parse(t); const added = [ensureMatrix(d), ensureModes(d)].filter(Boolean).join(" · "); if (L.compile(d, S.catalog).pack) { start = d; restored = true; S.inserted = added; } }
  } catch (e) { /* no storage: start from the shipped layout */ }
  S.design = clone(start);
  readState();
  syncCtxFromUnit();
  await fillOpen();
  apply(false);
  if (restored) flash("restored the design from this browser");
  setupPanel();
  requestAnimationFrame(loop);
  bindUI();
  window.designer.ready = true;
}

let lastT = 0, lastVals = 0;
function loop(t) {
  const dt = lastT ? Math.min(50, Math.max(1, Math.round(t - lastT))) : 16;
  lastT = t;
  S.api.run(dt);
  draw();
  drawPanel();
  // the live values and the unit's page in the tree: text and classes updated in place, never a rebuild; and not at
  // all while a dropdown is open (an open <select> has the focus: nothing near it may move)
  if (t - lastVals > 250 && document.activeElement?.tagName !== "SELECT") { lastVals = t; readState(); refreshValues(); markActive(); }
  requestAnimationFrame(loop);
}
const ctx2d = () => $("#screen").getContext("2d");
function draw() {
  const p = S.api.rgba();
  const img = new ImageData(new Uint8ClampedArray(S.m.HEAPU8.buffer, p, 240 * 240 * 4).slice(), 240, 240);
  ctx2d().putImageData(img, 0, 0);
}
function readState() { S.st = JSON.parse(S.api.state()); }
function syncCtxFromUnit() {
  const c = S.st.ctx;
  S.ctx = { mach: c[0], warp: c[1], filt: c[2], fenv: c[3], env: c[4], fx: c[5] };
  S.busfx = S.st.busfx.slice();
  S.target = S.st.bus ? 5 + S.st.bus : S.st.track;
}

// ------------------------------------------------------------------------------------------------ design state
function computeHome() {
  const d = S.shipped;
  const add = (n, cat) => { if (n && !S.home.has(n)) S.home.set(n, cat); };
  for (const c of d.categories) {
    const all = [];
    for (const p of c.pages) { all.push(p.lanes || []); for (const v of p.variants || []) all.push(v.lanes || []); }
    if (c.setup) all.push(c.setup);
    for (const ls of all) for (const l of ls) for (const s of l) add(L.slotName(s), c.cat);
  }
  for (const s of (d.f3 || []).flat()) add(L.slotName(s), "PAT");
  for (const p of S.catalog) if (!S.home.has(p.name)) S.home.set(p.name, p.fx ? "FX" : "OTHER");
  for (const n of Object.keys(L.SPECIAL)) if (!S.home.has(n))
    S.home.set(n, n.startsWith("razor") || n.startsWith("slot") ? "FILE" : n.startsWith("global") ? "MST" : "SRC");
}

function compile() { return L.compile(S.design, S.catalog); }

// an edit: the undo point, the change, the preview, the page
function edit(fn) {
  S.undo.push(L.canonical(S.design));
  if (S.undo.length > 300) S.undo.shift();
  S.redo = [];
  fn(S.design);
  apply(true);
}
function apply(save) {
  S.compiled = compile();
  S.api.run(20);                           // (a frame: the live labels follow the new layout)
  if (S.compiled.pack) {
    if (S.compiled.pack !== S.lastGood) {
      if (S.api.load(S.compiled.pack) !== 0) S.compiled.errors.push("the preview refused it: " + S.api.error());
      else S.lastGood = S.compiled.pack;
    }
  }
  if (save) try { localStorage.setItem(STORE, L.canonical(S.design)); } catch (e) { /* no storage */ }
  readState();
  render();
}
function undo() { if (!S.undo.length) return; S.redo.push(L.canonical(S.design)); S.design = JSON.parse(S.undo.pop()); apply(true); }
function redo() { if (!S.redo.length) return; S.undo.push(L.canonical(S.design)); S.design = JSON.parse(S.redo.pop()); apply(true); }

// ------------------------------------------------------------------------------------------------ refs
// a slot's ref: {scope: "page"|"setup"|"bus"|"bsetup"|"msrc"|"f3", cat, pi, bus, vi (-1 the page's own), lane, k}
function catObj(name) { return S.design.categories.find((c) => c.cat === name); }
function pageOf(ref) {
  switch (ref.scope) {
    case "page": return catObj(ref.cat).pages[ref.pi];
    case "bus": { const p = S.design.bus[ref.bus].page; return L.sourcePage(S.design, p) || p; }
    case "msrc": return S.design.bus[2].src;
    default: return null;
  }
}
function lanesOf(ref, create) {
  switch (ref.scope) {
    case "setup": { const c = catObj(ref.cat); if (!c.setup && create) c.setup = []; return c.setup || []; }
    case "bsetup": { const b = S.design.bus[ref.bus]; if (!b.setup && create) b.setup = []; return b.setup || []; }
    case "f3": if (!S.design.f3 && create) S.design.f3 = []; return S.design.f3 || [];
    default: {
      const p = pageOf(ref);
      if (ref.vi >= 0) { const v = p.variants[ref.vi]; if (!v.lanes && create) v.lanes = []; return v.lanes || []; }
      if (!p.lanes && create) p.lanes = [];
      return p.lanes || [];
    }
  }
}
function getSlot(ref) { const l = lanesOf(ref)[ref.lane]; return l && l[ref.k] !== undefined ? l[ref.k] : null; }
function setSlot(ref, v) {
  const ls = lanesOf(ref, true);
  while (ls.length <= ref.lane) ls.push([]);
  const lane = ls[ref.lane];
  while (lane.length <= ref.k) lane.push(null);
  lane[ref.k] = v;
}
const sameRef = (a, b) => a && b && ["scope", "cat", "pi", "bus", "vi", "lane", "k"].every((k) => a[k] === b[k]);
// the MATRIX page's slots are fixed (L.MATRIX_LANES): nothing dropped onto or dragged off them, no lane edits
function fixedSlots(ref) { return !!ref && ref.scope === "page" && catObj(ref.cat)?.pages[ref.pi]?.picture === "matrix"; }
// a design without a MATRIX page (made before 0.5.0) gets it as MOD's first page: the text of what was done, or ""
function ensureMatrix(d) {
  const mod = (d.categories || []).find((c) => c && c.cat === "MOD");
  if (!mod || !Array.isArray(mod.pages) || mod.pages.some((p) => p && p.picture === "matrix")) return "";
  let t = "MATRIX", n = 2;
  while (mod.pages.some((p) => p && p.title === t)) t = "MATRIX " + n++;
  mod.pages.unshift({ ...L.MATRIX_PAGE(), title: t });
  return `MOD/${t} inserted as MOD's first page (the design had no mod matrix page)`;
}
// a design made before 0.6.0 gets the modes: CROSSFADER (XFD) with MST's SCENE page, every category's mode (name,
// tag, button, order) and its pages' black keys written out, FX's bus keys, the fixed screens. The text of what was
// done, or ""
function ensureModes(d) {
  if (!d || !Array.isArray(d.categories) || d.categories.some((c) => c && c.cat === "XFD")) return "";
  const out = [];
  const mst = d.categories.find((c) => c && c.cat === "MST");
  const xfd = { cat: "XFD", pages: [] };
  if (mst && Array.isArray(mst.pages)) {
    const i = mst.pages.findIndex((p) => p && p.picture === "scene");
    if (i >= 0 && mst.pages.length > 1) { xfd.pages.push(mst.pages.splice(i, 1)[0]); out.push(`MST/${xfd.pages[0].title} moved to CROSSFADER`); }
  }
  if (!xfd.pages.length) xfd.pages.push({ title: "SCENE", picture: "scene", lanes: [[{ item: "e5.xfd", label: "CROSSFADE" }]] });
  d.categories.push(xfd);
  d.categories = d.categories.map((c) => {
    if (!c || !L.CATS.includes(c.cat)) return c;
    const keys = L.pageKeys(c), m = L.modeOf(c);
    const n = { cat: c.cat, mode: m.name, tag: m.tag, button: m.button, order: m.order };
    for (const [k, v] of Object.entries(c)) if (!(k in n)) n[k] = v;
    n.pages = (n.pages || []).map((p, i) => withKey(p, keys[i]));
    return n;
  });
  (d.bus || []).forEach((b, bi) => { if (b && typeof b === "object" && !("key" in b)) { const n = { bus: b.bus, key: L.BUS_KEY_DEFAULT[bi] }; Object.assign(n, b); for (const k of Object.keys(b)) delete b[k]; Object.assign(b, n); } });
  if (!d.screens) d.screens = L.SCREENS.map((m) => ({ mode: m, button: L.SCREEN_DEFAULT[m][0], order: L.SCREEN_DEFAULT[m][1] }));
  out.push("the modes added (0.6.0): buttons, orders, black keys");
  return out.join("; ");
}
// a page with its black key k (after its title; 0: none)
function withKey(p, k) {
  if (!p || typeof p !== "object") return p;
  const n = {};
  for (const [kk, v] of Object.entries(p)) {
    if (kk === "key") continue;
    n[kk] = v;
    if (kk === "title" && k) n.key = k;
  }
  if (k && !("key" in n)) n.key = k;
  return n;
}
// the category's keys written out (so that an edit of one keeps the others), then fn
function setKeys(c, fn) {
  const ks = L.pageKeys(c);
  c.pages = c.pages.map((p, i) => withKey(p, ks[i]));
  if (fn) fn();
}
function setPageKey(cat, pi, k) {
  const c = catObj(cat);
  edit(() => setKeys(c, () => {
    const old = c.pages[pi].key || 0;
    if (k) {
      const other = c.pages.findIndex((p, i) => i !== pi && p.key === k);
      if (other >= 0) c.pages[other] = withKey(c.pages[other], old);     // (a key taken: the two swap)
      if (cat === "FX") S.design.bus.forEach((b) => { if (b.key === k) b.key = old || 0; });
    }
    c.pages[pi] = withKey(c.pages[pi], k);
  }));
}
function maxLanes(scope) { return scope === "setup" || scope === "bsetup" ? L.SETUP_LANES : scope === "f3" ? 1 : L.MAX_LANES; }

// the variant a page shows now (for a bus page: with that bus's FX machine)
function curVariant(scope, page, bus) {
  const ctx = scope === "bus" ? { ...S.ctx, mach: 1, warp: 1, filt: 1, fenv: 2, env: 2, fx: S.busfx[bus] } : S.ctx;
  return L.matchVariant(S.design, page, ctx);
}

// ------------------------------------------------------------------------------------------------ drag and drop
let drag = null;     // {ref} | {name}
function drop(src, dst) {
  if (!src || !dst) return false;
  if (src.ref && dst.ref && sameRef(src.ref, dst.ref)) return false;
  if (fixedSlots(src.ref) || fixedSlots(dst.ref)) { flash("the MATRIX page's knobs are fixed (SRC, DST, DEPTH)"); return false; }
  if (src.name && (L.isMtx(src.name) || L.SPECIAL[src.name]?.[0] === L.IT.MTX)) { flash(`${src.name}: the MATRIX page has it`); return false; }
  edit(() => {
    if (src.ref && dst.ref) {                 // swap (or move onto an empty slot)
      const a = getSlot(src.ref), b = getSlot(dst.ref);
      setSlot(dst.ref, a);
      setSlot(src.ref, b);
    } else if (src.name && dst.ref) setSlot(dst.ref, src.name);   // from the tray (the old one goes back there)
    else if (src.ref && dst.tray) setSlot(src.ref, null);       // back to the tray
  });
  if (dst.ref) select(dst.ref);
  return true;
}

// ------------------------------------------------------------------------------------------------ the tree
function itemInfo(slot) {
  const name = L.slotName(slot);
  if (!name) return null;
  const label = L.slotLabel(slot);
  if (L.SPECIAL[name]) { const [kind, id, dl] = L.SPECIAL[name]; return { name, kind, id, label: label || dl, own: dl, relabel: !!label && label !== dl, special: true }; }
  const p = S.byName.get(name);
  if (!p) return { name, kind: 0, id: 0, label: "?" + name, own: "?", relabel: false, bad: true };
  return { name, kind: 1, id: p.i, label: label || p.label, own: p.label, relabel: !!label };
}

// ------------------------------------------------------------------------------------------------ descriptions
// design/layout/descriptions.json (gen_layout.py checks it): item id -> {name, desc}; "CAT/TITLE", "CAT/SETUP",
// "bus:S1", "bus:MST/SRC", "F3" -> the page's; the option values a variant picks -> what that machine / mode is
function itemDesc(name) { return (name && S.desc.items[name]) || null; }
function itemTip(name, extra) {
  const d = itemDesc(name);
  return `${d ? d.name : name} · ${name}${extra ? ` (${extra})` : ""}\n${d ? d.desc : "(no description)"}`;
}
function pageKey(sp) {
  if (!sp) return null;
  if (sp.scope === "page") return `${sp.cat}/${catObj(sp.cat)?.pages[sp.pi]?.title}`;
  if (sp.scope === "setup") return `${sp.cat}/SETUP`;
  if (sp.scope === "bus") return `bus:${S.design.bus[sp.bus].bus}`;
  if (sp.scope === "bsetup") return `bus:${S.design.bus[sp.bus].bus}/SETUP`;
  if (sp.scope === "msrc") return "bus:MST/SRC";
  if (sp.scope === "f3") return "F3";
  return null;
}
// what a variant's conditions mean: "COMB: a comb filter ..."
function whenDesc(w) {
  const out = [];
  for (const [k, vals] of Object.entries(w || {})) for (const v of vals) {
    if (k === "machine") for (const m of L.MCLASS[v] || []) { const n = L.MACHINES[m - 1]; const t = S.desc.options.machine?.[n]; if (t) out.push(`${n}: ${t}`); }
    else { const t = S.desc.options[k]?.[v]; if (t) out.push(`${v}: ${t}`); }
  }
  return out;
}
function pageTip(sp, page, vi) {
  const key = pageKey(sp), d = S.desc.pages[key];
  let t = `${key}\n${d || "(no description)"}`;
  const src = page && (L.sourcePage(S.design, page) || page);
  if (src && vi >= 0 && src.variants?.[vi]) { const w = whenDesc(src.variants[vi].when); if (w.length) t += `\n\nThis variant (${L.whenText(src.variants[vi].when)}):\n` + w.join("\n"); }
  return t;
}

function slotEl(ref) {
  const slot = getSlot(ref);
  const info = itemInfo(slot);
  const fixed = fixedSlots(ref);
  const e = h("div", { class: "slot" + (info ? "" : " empty") + (info && info.relabel ? " relabel" : "") + (info && info.special ? " special" : "") +
      (fixed ? " fixedslot" : "") + (sameRef(ref, S.sel) ? " sel" : ""), draggable: info && !fixed ? "true" : "false", "data-fixed": fixed ? "1" : null,
    "data-tip": info ? itemTip(info.name, (info.relabel ? `label ${info.label}, its own ${info.own}` : "") + (fixed ? "fixed: the MATRIX page's own knob" : "")) : fixed ? "fixed: the MATRIX page's knob 4 does nothing" : "empty: drop a parameter here" },
    h("span", { class: "lb", text: info ? info.label : "—" }),
    h("span", { class: "vl", text: info && !info.bad ? S.api.value(info.kind, info.id) : "" }),
    h("span", { class: "k", text: "K" + (ref.k + 1) }));
  e._ref = ref;
  e.addEventListener("click", (ev) => { ev.stopPropagation(); select(ref, true); });
  e.addEventListener("dragstart", (ev) => { drag = { ref }; ev.dataTransfer.setData("text/plain", info ? info.name : ""); ev.dataTransfer.effectAllowed = "move"; });
  e.addEventListener("dragend", () => { drag = null; });
  e.addEventListener("dragover", (ev) => { if (!fixed && drag && (drag.ref || drag.name)) { ev.preventDefault(); e.classList.add("over"); } });
  e.addEventListener("dragleave", () => e.classList.remove("over"));
  e.addEventListener("drop", (ev) => { ev.preventDefault(); e.classList.remove("over"); const d = drag; drag = null; drop(d, { ref }); });
  e.addEventListener("wheel", (ev) => {           // a knob on the unit: this page and lane, the knob turned
    if (!info) return;
    ev.preventDefault();
    gotoRef(ref);
    S.api.turn(3 + ref.k, ev.deltaY < 0 ? 1 : -1);
  }, { passive: false });
  return e;
}

function lanesEl(ref0, page) {
  const ls = lanesOf(ref0);
  const max = maxLanes(ref0.scope);
  const fixed = fixedSlots(ref0);
  const wrap = h("div", { class: "lanes" });
  const n = Math.max(1, ls.length);
  for (let li = 0; li < n; li++) {
    const ref = (k) => ({ ...ref0, lane: li, k });
    const row = h("div", { class: "lane", "data-lane": li },
      h("span", { class: "ln", text: "L" + (li + 1) }),
      [0, 1, 2, 3].map((k) => slotEl(ref(k))),
      fixed ? h("span", { class: "lops" }) : h("span", { class: "lops" },
        h("button", { class: "small", title: "lane up", disabled: li === 0, onclick: () => edit(() => { const a = lanesOf(ref0, true); [a[li - 1], a[li]] = [a[li], a[li - 1]]; }) }, "↑"),
        h("button", { class: "small", title: "lane down", disabled: li >= ls.length - 1, onclick: () => edit(() => { const a = lanesOf(ref0, true); [a[li + 1], a[li]] = [a[li], a[li + 1]]; }) }, "↓"),
        h("button", { class: "small", title: "delete the lane (its parameters go back to the tray)", disabled: ls.length === 0, onclick: () => edit(() => { lanesOf(ref0, true).splice(li, 1); }) }, "✕")));
    row._lane = { ...ref0, lane: li };
    wrap.append(row);
  }
  if (ls.length < max && !fixed) wrap.append(h("div", { class: "addrow" }, h("button", { class: "small", onclick: () => edit(() => { lanesOf(ref0, true).push([null, null, null, null]); }) }, "+ LANE")));
  void page;
  return wrap;
}

function variantRow(scope, page, refBase, bus) {
  const src = L.sourcePage(S.design, page) || page;
  const vs = src.variants || [];
  const cur = curVariant(scope, page, bus);
  const row = h("div", { class: "vrow" });
  if (vs.length) {
    const sel = h("select", { title: "the variant shown (picks the preview's context to match)", "data-key": `var-${scope}-${refBase.cat}-${refBase.pi}-${bus}`, onchange: (ev) => pickVariant(scope, src, +ev.target.value, bus) },
      vs.map((v, i) => h("option", { value: i, selected: i === cur }, (v.title ? v.title + " · " : "") + L.whenText(v.when))),
      h("option", { value: -1, selected: cur === -1 }, "otherwise (the page's own)"));
    row.append("VARIANT", sel);
  }
  const dims = h("select", { title: "give the current context its own lanes", "data-key": `addvar-${scope}-${refBase.cat}-${refBase.pi}-${bus}`, onchange: (ev) => { addVariant(refBase, src, ev.target.value, bus); ev.target.value = ""; } },
    h("option", { value: "" }, "+ VARIANT FOR…"),
    ["machine", "warp", "filter", "fenv", "env", "fx"].map((d) => h("option", { value: d }, `this ${d}: ${ctxValueName(d, scope === "bus" ? bus : -1)}`)));
  row.append(dims);
  if (cur >= 0) row.append(h("button", { class: "small", title: "delete this variant (the page's own lanes show instead)", onclick: () => edit(() => { src.variants.splice(cur, 1); if (!src.variants.length) delete src.variants; }) }, "✕ VARIANT"));
  return row;
}
function ctxValueName(dim, bus) {
  const c = S.ctx;
  switch (dim) {
    case "machine": return machineClass(c.mach).toUpperCase();
    case "warp": return L.OPTS.warp[c.warp - 1];
    case "filter": return L.OPTS.filter[c.filt - 1];
    case "fenv": return L.OPTS.fenv[c.fenv - 1];
    case "env": return L.OPTS.env[c.env - 1];
    case "fx": return L.OPTS.fx[(bus >= 0 ? S.busfx[bus] : c.fx) - 1];
  }
  return "";
}
function machineClass(m) { return Object.entries(L.MCLASS).find(([, ms]) => ms.includes(m))?.[0] || "loop"; }
function addVariant(ref, src, dim, bus) {
  if (!dim) return;
  const val = dim === "machine" ? machineClass(S.ctx.mach) : ctxValueName(dim, ref.scope === "bus" ? bus : -1);
  edit(() => {
    const cur = L.matchVariant(S.design, src, ref.scope === "bus" ? { ...S.ctx, fx: S.busfx[bus] } : S.ctx);
    const lanes = clone(cur >= 0 ? src.variants[cur].lanes || [] : src.lanes || []);
    src.variants = src.variants || [];
    src.variants.unshift({ when: { [dim]: [val] }, lanes });
  });
}
function pickVariant(scope, src, vi, bus) {
  if (vi < 0) return render();
  const w = src.variants[vi].when || {};
  if (w.machine) S.ctx.mach = L.MCLASS[w.machine[0]][0];
  for (const [k, key] of [["warp", "warp"], ["filter", "filt"], ["fenv", "fenv"], ["env", "env"]]) if (w[k]) S.ctx[key] = L.OPTS[k].indexOf(w[k][0]) + 1;
  if (w.fx) { if (scope === "bus") S.busfx[bus] = L.OPTS.fx.indexOf(w.fx[0]) + 1; else S.ctx.fx = L.OPTS.fx.indexOf(w.fx[0]) + 1; }
  pushCtx();
  render();
}
function pushCtx() {
  const c = S.ctx;
  S.api.context(c.mach, c.warp, c.filt, c.fenv, c.env, c.fx);
  S.busfx.forEach((f, b) => S.api.busfx(b + 1, f));
  S.api.target(S.target);
  readState();
}

function pageEl(c, pi) {
  const p = c.pages[pi];
  const fixed = !!L.FIXED[p.picture];
  const cur = curVariant("page", p);
  const vref = { scope: "page", cat: c.cat, pi, vi: cur };
  const isSel = S.selPage && S.selPage.scope === "page" && S.selPage.cat === c.cat && S.selPage.pi === pi;
  const e = h("div", { class: "page" + (isSel ? " sel" : ""), "data-cat": c.cat, "data-pi": pi });
  const title = h("input", { class: "title", value: p.title, "data-key": `title-${c.cat}-${pi}`, maxlength: Math.floor((L.TITLE_ROOM + 1) / 4) - L.modeOf(c).tag.length - 1, spellcheck: "false", title: `the page's name (the header shows ${L.modeOf(c).tag}/NAME)`,
    onchange: (ev) => { const t = ev.target.value.toUpperCase().trim(); if (t && t !== p.title) edit(() => { renameRefs(c.cat, p.title, t); p.title = t; }); },
    oninput: (ev) => { ev.target.value = ev.target.value.toUpperCase(); } });
  const grip = h("span", { class: "grip", draggable: fixed ? "false" : "true", title: fixed ? "a fixed page stays in its category" : "drag onto another category or page to move it" }, "⠿");
  grip.addEventListener("dragstart", (ev) => { drag = { page: { cat: c.cat, pi } }; ev.dataTransfer.setData("text/plain", p.title); });
  grip.addEventListener("dragend", () => { drag = null; });
  e.addEventListener("dragover", (ev) => { if (drag && drag.page) { ev.preventDefault(); e.classList.add("drop"); } });
  e.addEventListener("dragleave", () => e.classList.remove("drop"));
  e.addEventListener("drop", (ev) => { if (!drag || !drag.page) return; ev.preventDefault(); e.classList.remove("drop"); const d = drag.page; drag = null; movePage(d.cat, d.pi, c.cat, pi); });
  const picSel = fixed ? h("span", { class: "fixed", title: p.picture === "matrix" ? "the mod matrix: its picture, category and knobs are fixed (SELECT picks the row; KNOB 1-3 SRC, DST, DEPTH)" : "this page has behaviour of its own: its picture and category are fixed; its knobs are yours" }, "FIXED · " + PIC_NAMES[p.picture])
    : h("select", { title: "the picture above the cells", "data-key": `pic-${c.cat}-${pi}`, onchange: (ev) => edit(() => { p.picture = ev.target.value; if (p.picture === "macro") p.macro = p.macro || 1; else delete p.macro; }) },
      FREE_PICS.map((x) => h("option", { value: x, selected: x === p.picture }, PIC_NAMES[x])));
  const macroSel = p.picture === "macro" ? h("select", { title: "which macro", onchange: (ev) => edit(() => { p.macro = +ev.target.value; }) },
    [1, 2, 3, 4].map((m) => h("option", { value: m, selected: m === p.macro }, "M" + m))) : null;
  const moveSel = fixed ? null : h("select", { title: "move the page to another mode", "data-key": `move-${c.cat}-${pi}`, onchange: (ev) => { const to = ev.target.value; if (to) movePage(c.cat, pi, to, catObj(to).pages.length); } },
    h("option", { value: "" }, "MOVE TO…"), S.design.categories.filter((x) => x.cat !== c.cat).map((x) => h("option", { value: x.cat }, L.modeOf(x).name)));
  const k = L.pageKeys(c)[pi];
  const keySel = h("select", { class: "key" + (k ? " on" : ""), title: `the black key that opens this page in ${L.modeOf(c).name}`, "data-key": `key-${c.cat}-${pi}`, onchange: (ev) => setPageKey(c.cat, pi, +ev.target.value) },
    h("option", { value: 0, selected: !k }, "—"), Array.from({ length: L.NBKEY }, (_, i) => h("option", { value: i + 1, selected: k === i + 1 }, "B" + (i + 1))));
  e.append(h("div", { class: "prow", "data-tip": pageTip({ scope: "page", cat: c.cat, pi }, p, cur), onclick: (ev) => { if (!ev.target.closest("select, input, button")) selectPage({ scope: "page", cat: c.cat, pi }); } },
    grip, keySel, title, picSel, macroSel, p.note ? h("span", { class: "notemark", title: p.note }, "✎") : null, h("span", { class: "sp" }),
    h("button", { class: "small", title: "earlier", disabled: pi === 0, onclick: (ev) => { ev.stopPropagation(); movePage(c.cat, pi, c.cat, pi - 1); } }, "↑"),
    h("button", { class: "small", title: "later", disabled: pi >= c.pages.length - 1, onclick: (ev) => { ev.stopPropagation(); movePage(c.cat, pi, c.cat, pi + 2); } }, "↓"),
    moveSel,
    h("button", { class: "small", title: fixed ? "a fixed page cannot be deleted" : "delete the page", disabled: fixed || c.pages.length < 2,
      onclick: (ev) => { ev.stopPropagation(); edit(() => { c.pages.splice(pi, 1); }); S.selPage = null; } }, "✕")));
  if (p.picture !== "matrix") e.append(variantRow("page", p, vref));
  e.append(lanesEl(vref, p));
  return e;
}
function renameRefs(cat, from, to) {
  for (const b of S.design.bus) if (b.page && b.page.same_as === `${cat}/${from}`) b.page.same_as = `${cat}/${to}`;
}
function movePage(fromCat, pi, toCat, toIndex) {
  const p = catObj(fromCat).pages[pi];
  if (!p || (L.FIXED[p.picture] && toCat !== fromCat)) return;
  if (fromCat !== toCat && catObj(fromCat).pages.length < 2) { flash("a category keeps one page at least"); return; }
  edit(() => {
    setKeys(catObj(fromCat));                 // (the keys written out first: the pages keep theirs; a page moved to
    if (fromCat !== toCat) setKeys(catObj(toCat));   // another mode keeps its key if that mode has it free, else none)
    const src = catObj(fromCat).pages, dst = catObj(toCat).pages;
    let moved = src[pi];
    if (fromCat !== toCat && moved.key && (dst.some((x) => x.key === moved.key) || (toCat === "FX" && S.design.bus.some((b) => b.key === moved.key)))) moved = withKey(moved, 0);
    src.splice(pi, 1);
    let at = toIndex;
    if (fromCat === toCat && toIndex > pi) at--;
    let t = moved.title, n = 2;
    while (fromCat !== toCat && dst.some((x) => x.title === t)) t = (moved.title.slice(0, 8) + " " + n++).toUpperCase();
    if (t !== moved.title) { renameRefs(fromCat, moved.title, t); moved.title = t; }
    for (const b of S.design.bus) if (b.page && b.page.same_as === `${fromCat}/${moved.title}`) b.page.same_as = `${toCat}/${moved.title}`;
    dst.splice(Math.max(0, Math.min(at, dst.length)), 0, moved);
  });
}

function renderTree() {
  const t = $("#tree");
  const top = t.scrollTop;
  t.innerHTML = "";
  // 0.6.0: the modes by their FM-1 button (in the panel's order), a button's modes in their order (HOME: LOOP ENGINE
  // -> TRIG -> KEYBOARD); a mode with params is its category (its pages with their black keys, its setup), a fixed
  // screen a line
  const modes = L.modeList(S.design);
  let grp = null, lastBtn = null;
  for (const m of modes) {
    if (m.button !== lastBtn) {
      lastBtn = m.button;
      const n = modes.filter((x) => x.button === m.button).length;
      grp = h("div", { class: "btngrp", "data-button": m.button }, h("div", { class: "btnhead", "data-tip": `the ${m.button} button${n > 1 ? `: ${modes.filter((x) => x.button === m.button).map((x) => x.name).join(" → ")} (tap it again: the next)` : ""}` },
        h("span", { class: "b" }, m.button === "FILE" ? "FILE (SAVE)" : m.button), n > 1 ? h("span", { class: "dim" }, `${n} modes, tap again: the next`) : null));
      t.append(grp);
    }
    if (!m.cat) {
      grp.append(h("div", { class: "cat screen", "data-screen": m.name }, h("div", { class: "cathead" }, h("span", { class: "ord" }, NTH(m.order)), h("span", { class: "n" }, m.name), h("span", { class: "dim" }, "a fixed screen: no params of its own"))));
      continue;
    }
    const c = catObj(m.cat);
    const nOn = modes.filter((x) => x.button === m.button).length;
    const on = S.st && L.CATS[S.st.cat] === c.cat;
    const box = h("div", { class: "cat" + (on ? " on" : ""), "data-cat": c.cat, "data-mode": m.name });
    const head = h("div", { class: "cathead", "data-tip": `${m.name} (${c.cat}): the ${m.button} button${nOn > 1 ? `, ${NTH(m.order)}` : ""}\n${S.desc.cats[c.cat] || ""}`, onclick: (ev) => { if (ev.target.closest("button")) return; S.view.cat = c.cat; tapCategory(c.cat); } },
      nOn > 1 ? h("span", { class: "ord" }, NTH(m.order)) : null, h("span", { class: "n" }, m.name), h("span", { class: "dim" }, `${c.cat} · ${m.tag}/ · ${c.pages.length} page${c.pages.length > 1 ? "s" : ""}`), h("span", { class: "sp" }),
      h("button", { class: "small", title: "a new page at the end", disabled: c.pages.length >= L.MAX_PAGES, onclick: (ev) => { ev.stopPropagation(); addPage(c); } }, "+ PAGE"));
    head.addEventListener("dragover", (ev) => { if (drag && drag.page) { ev.preventDefault(); head.classList.add("drop"); } });
    head.addEventListener("dragleave", () => head.classList.remove("drop"));
    head.addEventListener("drop", (ev) => { if (!drag || !drag.page) return; ev.preventDefault(); head.classList.remove("drop"); const d = drag.page; drag = null; movePage(d.cat, d.pi, c.cat, catObj(c.cat).pages.length); });
    box.append(head);
    c.pages.forEach((_, pi) => box.append(pageEl(c, pi)));
    const sref = { scope: "setup", cat: c.cat, vi: -1 };
    const setup = h("div", { class: "page setup", "data-cat": c.cat, "data-setup": 1 },
      h("div", { class: "prow", "data-tip": pageTip({ scope: "setup", cat: c.cat }), onclick: (ev) => { if (!ev.target.closest("select, input, button")) selectPage({ scope: "setup", cat: c.cat }); } }, h("b", {}, "SETUP"), h("span", { class: "dim" }, " (hold or SHIFT + the button)"), h("span", { class: "sp" }),
        c.setup ? h("button", { class: "small", title: "no setup list for this category", onclick: () => edit(() => { delete c.setup; }) }, "✕") : null));
    if (c.setup) setup.append(lanesEl(sref));
    else setup.append(h("button", { class: "small", onclick: () => edit(() => { c.setup = [[null, null, null, null]]; }) }, "+ SETUP LIST"));
    box.append(setup);
    grp.append(box);
  }
  // the buses
  const bx = h("div", { class: "cat bus" }, h("div", { class: "cathead" }, h("span", { class: "n" }, "BUSES"), h("span", { class: "dim" }, "S1 S2 MST: FX's B3 B4 B5; every mode but MIX, FILE, PATTERN LAUNCH, CROSSFADER shows the bus's page")));
  S.design.bus.forEach((b, bi) => {
    const p = b.page;
    const vref = { scope: "bus", bus: bi, vi: curVariant("bus", p, bi) };
    const e = h("div", { class: "page", "data-bus": bi });
    e.append(h("div", { class: "prow", "data-tip": pageTip({ scope: "bus", bus: bi }, p, vref.vi), onclick: (ev) => { if (!ev.target.closest("select, input, button")) selectPage({ scope: "bus", bus: bi }); } }, h("b", {}, b.bus), b.key ? h("span", { class: "keytag", title: "the black key that targets this bus in FX" }, "FX B" + b.key) : null, " ",
      h("input", { class: "title", value: p.title, spellcheck: "false", onchange: (ev) => { const v = ev.target.value.toUpperCase().trim(); if (v) edit(() => { p.title = v; }); } }),
      h("span", { class: "dim" }, PIC_NAMES[p.picture || "fx"])));
    if (p.same_as) {
      e.append(h("div", { class: "shared" }, `the lanes of ${p.same_as} (its FX machine's), for this bus's own machine `,
        h("button", { class: "small", title: "give this bus its own lanes (a copy)", onclick: () => edit(() => { const s = L.sourcePage(S.design, p); delete p.same_as; p.picture = p.picture || s.picture; p.lanes = clone(s.lanes || []); if (s.variants) p.variants = clone(s.variants); }) }, "UNLINK")));
    } else {
      e.append(variantRow("bus", p, vref, bi));
      e.append(lanesEl(vref, p));
      const fxp = catObj("FX").pages.find((x) => x.picture === "fx");
      if (fxp) e.append(h("button", { class: "small", title: `use FX/${fxp.title}'s lanes again`, onclick: () => edit(() => { delete p.lanes; delete p.variants; p.same_as = `FX/${fxp.title}`; }) }, `LINK TO FX/${fxp.title}`));
    }
    if (b.setup) e.append(h("div", { class: "h" }, b.bus + " SETUP"), lanesEl({ scope: "bsetup", bus: bi, vi: -1 }));
    if (b.src) {
      const sp = b.src;
      e.append(h("div", { class: "h", "data-tip": pageTip({ scope: "msrc" }) }, "MST: ITS SRC PAGE (FIXED · mixer)"), lanesEl({ scope: "msrc", bus: 2, vi: curVariant("msrc", sp) }, sp));
    }
    bx.append(e);
  });
  t.append(bx);
  const f3 = h("div", { class: "cat" }, h("div", { class: "cathead", "data-tip": pageTip({ scope: "f3" }), onclick: () => { S.api.f3(1); readState(); selectPage({ scope: "f3" }); } }, h("span", { class: "n" }, "F3"), h("span", { class: "dim" }, "OCT- + OCT+, or PLAY held: one lane")));
  f3.append(h("div", { class: "page" }, lanesEl({ scope: "f3", vi: -1 })));
  t.append(f3);
  t.scrollTop = top;
}
function addPage(c) {
  edit(() => {
    let n = c.pages.length + 1, t = "PAGE " + n;
    while (c.pages.some((p) => p.title === t)) t = "PAGE " + ++n;
    c.pages.push({ title: t, picture: "steps", lanes: [[null, null, null, null]] });
  });
}

// ------------------------------------------------------------------------------------------------ selection + nav
function select(ref, nav) {
  S.sel = ref;
  if (ref.scope === "page") S.selPage = { scope: "page", cat: ref.cat, pi: ref.pi };
  else if (ref.scope === "bus") S.selPage = { scope: "bus", bus: ref.bus };
  if (ref.cat) S.view.cat = ref.cat;
  if (nav) gotoRef(ref);
  renderSelection();
}
// a page picked: the preview goes there; the tree is not rebuilt (only the marks, the inspector and the tray)
function selectPage(sp) {
  S.selPage = sp;
  if (sp.cat) S.view.cat = sp.cat;
  if (sp.scope === "page") { if (S.target >= 6) { S.target = 0; S.api.target(0); } S.api.f3(0); S.api.goto(L.CATS.indexOf(sp.cat), sp.pi, 0, 0); }
  else if (sp.scope === "setup") { if (S.target >= 6) { S.target = 0; S.api.target(0); } S.api.f3(0); S.api.goto(L.CATS.indexOf(sp.cat), 0, 0, 1); }
  else if (sp.scope === "bus") { S.target = 6 + sp.bus; pushCtx(); S.api.f3(0); S.api.goto(L.CATS.includes(S.view.cat) && !NO_BUS.includes(S.view.cat) ? L.CATS.indexOf(S.view.cat) : 4, 0, 0, 0); }
  readState();
  renderSelection();
}
// the selection's marks in the tree, the inspector, the tray (no rebuild of the tree: its controls stay as they are)
function renderSelection() {
  document.querySelectorAll("#tree .slot").forEach((e) => e.classList.toggle("sel", sameRef(e._ref, S.sel)));
  const sp = S.selPage;
  document.querySelectorAll("#tree .page").forEach((e) => {
    const on = !!sp && ((sp.scope === "page" && e.dataset.cat === sp.cat && e.dataset.pi === String(sp.pi)) ||
      (sp.scope === "setup" && e.dataset.setup && e.dataset.cat === sp.cat) || (sp.scope === "bus" && e.dataset.bus === String(sp.bus)));
    e.classList.toggle("sel", on);
  });
  if (S.st) S.lastCat = L.CATS[S.st.cat];
  renderInspector();
  renderTray();
  markActive();
}
function gotoRef(ref) {
  S.api.f3(0);
  switch (ref.scope) {
    case "page": if (S.target >= 6) { S.target = 0; S.api.target(0); } S.api.goto(L.CATS.indexOf(ref.cat), ref.pi, ref.lane, 0); break;
    case "setup": if (S.target >= 6) { S.target = 0; S.api.target(0); } S.api.goto(L.CATS.indexOf(ref.cat), 0, ref.lane, 1); break;
    case "bus": S.target = 6 + ref.bus; S.api.target(S.target); S.api.goto(4, 0, ref.lane, 0); break;
    case "bsetup": S.target = 6 + ref.bus; S.api.target(S.target); S.api.goto(4, 0, ref.lane, 1); break;
    case "msrc": S.target = 8; S.api.target(8); S.api.goto(1, 0, ref.lane, 0); break;
    case "f3": S.api.f3(1); break;
  }
  readState();
}
// a mode picked (its header or its button above the panel): its page; again: its next page (the gestures that reach
// it on the unit are the panel's own, below)
function tapCategory(cat) {
  S.api.f3(0);
  const ci = L.CATS.indexOf(cat), np = catObj(cat).pages.length, st = S.st;
  S.api.goto(ci, st && st.cat === ci && !st.settings ? (st.page + 1) % np : 0, 0, 0);
  S.api.run(80);
  readState();
  markActive();
}

// the page / lane on the unit now, marked in the tree
function markActive() {
  if (!S.st) return;
  const st = S.st, cat = L.CATS[st.cat];
  const want = new Set();
  const mark = () => {        // (classes changed only where they differ: no style churn while the demo plays)
    for (const q of [".lane.on", ".page.on", ".cat.on"]) document.querySelectorAll(q).forEach((x) => { if (!want.has(x)) x.classList.remove("on"); });
    for (const x of want) if (!x.classList.contains("on")) x.classList.add("on");
  };
  let pe = null;
  document.querySelectorAll("#buttons .grp:first-child button").forEach((b) => { const on = b.dataset.cat === cat && !st.f3; if (b.classList.contains("on") !== on) b.classList.toggle("on", on); });
  if (S.lastCat !== cat) {                 // the unit went to another category: the tray follows
    S.lastCat = cat;
    if (S.view.cat !== cat) { S.view.cat = cat; renderTray(); }
  }
  if (st.f3) return mark();
  if (st.bus && !NO_BUS.includes(cat)) {
    pe = st.bus === 3 && cat === "SRC" ? null : document.querySelector(`.page[data-bus="${st.bus - 1}"]`);
  } else if (st.settings) pe = document.querySelector(`.page.setup[data-cat="${cat}"]`);
  else pe = document.querySelector(`.page[data-cat="${cat}"][data-pi="${st.page}"]`);
  const ce = document.querySelector(`.cat[data-cat="${cat}"]`);
  if (ce) want.add(ce);
  if (pe) { want.add(pe); const le = pe.querySelectorAll(".lane")[st.lane]; if (le) want.add(le); }
  mark();
}
function refreshValues() {
  document.querySelectorAll("#tree .slot:not(.empty)").forEach((e) => {
    const info = itemInfo(getSlot(e._ref));
    if (!info || info.bad) return;
    const v = S.api.value(info.kind, info.id), t = e.querySelector(".vl");
    if (t.textContent !== v) t.textContent = v;
  });
}

// ------------------------------------------------------------------------------------------------ the unit's controls
function optTip(label, dim, v) {
  const t = dim && S.desc.options[dim]?.[v];
  return `${label}: ${v}${t ? "\n" + t : ""}\n(the preview's context: pages that depend on it show the variant for it)`;
}
function renderCtx() {
  const bar = $("#ctxbar");
  bar.innerHTML = "";
  const dim = { MACHINE: "machine", WARP: "warp", FILTER: "filter", "F.ENV": "fenv", "AMP ENV": "env" };
  const sel = (label, opts, val, on) => h("label", { "data-tip": optTip(label, dim[label] || (/FX$/.test(label) ? "fx" : null), opts[val - 1]) }, label,
    h("select", { "data-key": "ctx-" + label, onchange: (ev) => on(+ev.target.value) }, opts.map((o, i) => h("option", { value: i + 1, selected: i + 1 === val }, o))));
  bar.append(
    sel("TARGET", TARGETS, S.target + 1, (v) => { S.target = v - 1; pushCtx(); render(); }),
    sel("MACHINE", L.MACHINES, S.ctx.mach, (v) => { S.ctx.mach = v; pushCtx(); render(); }),
    sel("WARP", L.OPTS.warp, S.ctx.warp, (v) => { S.ctx.warp = v; pushCtx(); render(); }),
    sel("FILTER", L.OPTS.filter, S.ctx.filt, (v) => { S.ctx.filt = v; pushCtx(); render(); }),
    sel("F.ENV", L.OPTS.fenv, S.ctx.fenv, (v) => { S.ctx.fenv = v; pushCtx(); render(); }),
    sel("AMP ENV", L.OPTS.env, S.ctx.env, (v) => { S.ctx.env = v; pushCtx(); render(); }),
    sel("INSERT FX", L.OPTS.fx, S.ctx.fx, (v) => { S.ctx.fx = v; pushCtx(); render(); }),
    ...[0, 1, 2].map((b) => sel(["S1", "S2", "MST"][b] + " FX", L.OPTS.fx, S.busfx[b], (v) => { S.busfx[b] = v; pushCtx(); render(); })));
}
// the category buttons (as on the unit: again = the next page); the rest of the panel is the drawn FM-1 below
function renderButtons() {
  const b = $("#buttons");
  b.innerHTML = "";
  const st = S.st || {};
  b.append(h("div", { class: "grp" }, L.modeList(S.design).filter((m) => m.cat).map((m) => h("button", { class: L.CATS[st.cat] === m.cat && !st.f3 ? "on" : "", "data-cat": m.cat,
    "data-tip": `${m.name} (the ${m.button} button${L.modeList(S.design).filter((x) => x.button === m.button).length > 1 ? `, ${NTH(m.order)}` : ""}): its page; again: the next page\n${S.desc.cats[m.cat] || ""}`,
    onclick: () => { S.view.cat = m.cat; tapCategory(m.cat); renderTray(); } }, m.tag))));
}

// ------------------------------------------------------------------------------------------------ the side
function renderInspector() {
  const box = $("#inspector");
  box.innerHTML = "";
  if (S.sel) {
    const slot = getSlot(S.sel), info = itemInfo(slot);
    const where = S.sel.scope === "page" ? `${S.sel.cat}/${catObj(S.sel.cat).pages[S.sel.pi]?.title}` : S.sel.scope === "setup" ? `${S.sel.cat}/SETUP` : S.sel.scope.toUpperCase();
    box.append(h("div", { class: "h" }, "SLOT ", h("span", { class: "dim" }, `${where} · lane ${S.sel.lane + 1} · KNOB ${S.sel.k + 1}`)));
    if (info && fixedSlots(S.sel)) {
      const d = itemDesc(info.name);
      box.append(h("div", { class: "row" }, h("span", { class: "big" }, info.label), h("span", { class: "dim" }, info.name), d ? h("span", { class: "dname" }, d.name) : null),
        h("div", { class: "desc" }, d ? d.desc : "(no description in design/layout/descriptions.json)"),
        h("div", { class: "row dim" }, "fixed: the MATRIX page's knobs are always SRC, DST, DEPTH"));
    } else if (info) {
      const lab = h("input", { class: "lab", value: L.slotLabel(slot) || "", placeholder: info.own, maxlength: L.LABEL_MAX, spellcheck: "false",
        oninput: (ev) => { ev.target.value = ev.target.value.toUpperCase(); },
        onchange: (ev) => { const v = ev.target.value.toUpperCase().trim(); edit(() => setSlot(S.sel, v && v !== info.own ? { item: info.name, label: v } : info.name)); } });
      const d = itemDesc(info.name);
      box.append(h("div", { class: "row" }, h("span", { class: "big" }, info.label), h("span", { class: "dim" }, info.name), d ? h("span", { class: "dname" }, d.name) : null),
        h("div", { class: "desc" }, d ? d.desc : "(no description in design/layout/descriptions.json)"),
        h("div", { class: "row" }, "LABEL", lab, h("span", { class: "dim" }, `its own: ${info.own}`)),
        h("div", { class: "row" }, h("button", { onclick: () => edit(() => setSlot(S.sel, null)) }, "CLEAR SLOT"), h("span", { class: "dim" }, "or drag it to the tray")));
    } else box.append(h("div", { class: "row dim" }, fixedSlots(S.sel) ? "fixed: the MATRIX page's knob 4 does nothing" : "empty: drag a parameter here, or click one in the tray"));
  }
  const sp = S.selPage;
  const page = sp && (sp.scope === "page" ? catObj(sp.cat)?.pages[sp.pi] : sp.scope === "bus" ? S.design.bus[sp.bus].page : null);
  if (sp && pageKey(sp)) {
    const vi = page ? curVariant(sp.scope === "bus" ? "bus" : "page", page, sp.bus) : -1;
    const src = page && (L.sourcePage(S.design, page) || page);
    const w = src && vi >= 0 && src.variants?.[vi] ? whenDesc(src.variants[vi].when) : [];
    box.append(h("div", { class: "h" }, "PAGE ", h("span", { class: "dim" }, pageKey(sp))),
      h("div", { class: "desc pdesc" }, S.desc.pages[pageKey(sp)] || "(no description in design/layout/descriptions.json)"),
      w.length ? h("div", { class: "desc vdesc" }, h("b", {}, `VARIANT ${L.whenText(src.variants[vi].when)}: `), w.join(" · ")) : null);
  }
  if (page) {
    box.append(h("div", { class: "h" }, "PAGE NOTE ", h("span", { class: "dim" }, sp.scope === "page" ? `${sp.cat}/${page.title}` : `bus ${S.design.bus[sp.bus].bus}`)));
    box.append(h("textarea", { placeholder: "feedback that isn't a slot move: saved in the design", onchange: (ev) => edit(() => { const v = ev.target.value; if (v.trim()) page.note = v; else delete page.note; }) }, page.note || ""));
  }
  if (!S.sel && !page) box.append(h("div", { class: "dim" }, "click a slot or a page; drag parameters between slots, lanes, pages and categories; wheel over a slot turns its knob on the unit"));
}

function placedNow(catName) {
  // what the category's pages (their variants now) and setup show, and everywhere in the design (for the search)
  const inCat = new Set(), all = new Set();
  for (const c of S.design.categories) {
    const add = (ls, mine) => { for (const l of ls || []) for (const s of l) { const n = L.slotName(s); if (n) { all.add(n); if (mine) inCat.add(n); } } };
    for (const p of c.pages) { const vi = curVariant("page", p); add(vi >= 0 ? p.variants[vi].lanes : p.lanes, c.cat === catName); }
    add(c.setup, c.cat === catName);
  }
  return { inCat, all };
}
function renderTray() {
  const list = $("#traylist");
  list.innerHTML = "";
  const q = $("#search").value.trim().toLowerCase();
  const cat = S.view.cat;
  const { inCat, all } = placedNow(cat);
  // (never the matrix's own items or slot params, nor the params retired in 0.5.0)
  const names = [...S.catalog.map((p) => p.name), ...Object.keys(L.SPECIAL)].filter((n) => !L.isMtx(n) && !L.isRetired(n) && L.SPECIAL[n]?.[0] !== L.IT.MTX);
  const label = (n) => (L.SPECIAL[n] ? L.SPECIAL[n][2] : S.byName.get(n).label);
  const dtext = (n) => { const d = itemDesc(n); return d ? (d.name + " " + d.desc).toLowerCase() : ""; };
  let shown = names.filter((n) => (q ? n.toLowerCase().includes(q) || label(n).toLowerCase().includes(q) || dtext(n).includes(q) : S.home.get(n) === cat && !inCat.has(n)));
  if (!q && cat === "FILE") shown = shown.filter((n) => !n.startsWith("razor.") || true);
  $("#tray .h").firstChild.textContent = q ? "SEARCH " : `UNPLACED IN ${cat} `;
  if (!shown.length) list.append(h("span", { class: "empty-note" }, q ? "nothing matches" : `every ${cat} parameter is on a page in this context`));
  for (const n of shown.slice(0, 160)) {
    const placed = all.has(n);
    const chip = h("span", { class: "chip" + (q && placed ? " placed" : ""), draggable: "true", "data-name": n, "data-tip": itemTip(n, placed ? "on a page already" : "") }, label(n), h("span", { class: "c" }, q ? S.home.get(n) : n));
    chip.addEventListener("dragstart", (ev) => { drag = { name: n }; ev.dataTransfer.setData("text/plain", n); });
    chip.addEventListener("dragend", () => { drag = null; });
    chip.addEventListener("click", () => { if (S.sel) drop({ name: n }, { ref: S.sel }); });
    list.append(chip);
  }
}

// what moved: every item's places in the shipped layout and in the design
function places(d) {
  const m = new Map(), pagesBy = {};
  const put = (n, where) => { if (!n) return; if (!m.has(n)) m.set(n, []); m.get(n).push(where); };
  const walk = (ls, where) => (ls || []).forEach((l, li) => l.forEach((s, k) => put(L.slotName(s) && (L.slotName(s) + (L.slotLabel(s) ? `"${L.slotLabel(s)}"` : "")), `${where} L${li + 1}K${k + 1}`)));
  for (const c of d.categories) {
    const ks = L.pageKeys(c);
    pagesBy[c.cat] = c.pages.map((p, i) => p.title + (ks[i] ? ` B${ks[i]}` : ""));
    for (const p of c.pages) {
      walk(p.lanes, `${c.cat}/${p.title}${p.variants ? "·otherwise" : ""}`);
      for (const v of p.variants || []) walk(v.lanes, `${c.cat}/${p.title}·${L.whenText(v.when)}`);
    }
    walk(c.setup, `${c.cat}/SETUP`);
  }
  for (const b of d.bus || []) {
    if (!b.page.same_as) { walk(b.page.lanes, `${b.bus}`); for (const v of b.page.variants || []) walk(v.lanes, `${b.bus}·${L.whenText(v.when)}`); }
    walk(b.setup, `${b.bus}/SETUP`);
    if (b.src) walk(b.src.lanes, `${b.bus}/SRC`);
  }
  walk(d.f3, "F3");
  return { m, pagesBy };
}
function renderDiff() {
  const out = $("#difflist");
  out.innerHTML = "";
  const a = places(S.shipped), b = places(S.design);
  const rows = [];
  for (const cat of L.CATS) {
    const x = (a.pagesBy[cat] || []).join(", "), y = (b.pagesBy[cat] || []).join(", ");
    if (x !== y) rows.push(h("div", {}, h("b", {}, cat), " pages: ", x, h("span", { class: "mv" }, " → "), y));
  }
  const names = new Set([...a.m.keys(), ...b.m.keys()]);
  for (const n of [...names].sort()) {
    const x = (a.m.get(n) || []).sort(), y = (b.m.get(n) || []).sort();
    if (x.join() === y.join()) continue;
    const gone = x.filter((w) => !y.includes(w)), came = y.filter((w) => !x.includes(w));
    if (!gone.length) rows.push(h("div", {}, h("b", {}, n), h("span", { class: "mv" }, " now also on "), came.join(", ")));
    else if (!came.length) rows.push(h("div", {}, h("b", {}, n), h("span", { class: "mv" }, " no longer on "), gone.join(", ")));
    else rows.push(h("div", {}, h("b", {}, n), " ", gone.join(", "), h("span", { class: "mv" }, " → "), came.join(", ")));
  }
  if (S.inserted) rows.unshift(h("div", {}, h("b", {}, "OPENED"), h("span", { class: "mv" }, " added: "), S.inserted));
  for (const c of S.design.categories) for (const p of c.pages) if (p.note) rows.push(h("div", {}, h("b", {}, `${c.cat}/${p.title}`), " note: ", p.note));
  for (const bb of S.design.bus) if (bb.page.note) rows.push(h("div", {}, h("b", {}, bb.bus), " note: ", bb.page.note));
  if (!rows.length) out.append(h("div", { class: "empty-note" }, "the design is the shipped layout"));
  rows.slice(0, 300).forEach((r) => out.append(r));
}

function renderLint() {
  const box = $("#lint");
  box.innerHTML = "";
  const c = S.compiled;
  for (const e of c.errors) box.append(h("div", { class: "e" }, "ERROR " + e));
  for (const w of c.warnings) box.append(h("div", { class: "w" }, "WARNING " + w));
  if (!c.errors.length && c.pack) box.append(h("div", { class: "i" }, `buildable: ${c.npages} pages, ${c.nvars} variants, ${c.nitems} items · the generator compiles this file as it is`));
  const s = $("#status");
  s.innerHTML = "";
  s.append(c.errors.length ? h("span", { class: "e" }, `${c.errors.length} ERROR${c.errors.length > 1 ? "S" : ""} (the preview shows the last good one)`)
    : h("span", { class: c.warnings.length ? "w" : "ok" }, c.warnings.length ? `${c.warnings.length} WARNING${c.warnings.length > 1 ? "S" : ""}` : "OK"), S.msg ? " · " + S.msg : "");
}

function render() {
  const fk = document.activeElement?.dataset?.key;
  $("#dname").value = S.design.name || "";
  renderCtx();
  renderTree();
  renderButtons();
  renderInspector();
  renderTray();
  renderDiff();
  renderLint();
  markActive();
  $("#undo").disabled = !S.undo.length;
  $("#redo").disabled = !S.redo.length;
  if (fk) document.querySelector(`[data-key="${CSS.escape(fk)}"]`)?.focus({ preventScroll: true });
}
function flash(m) { S.msg = m; renderLint(); setTimeout(() => { S.msg = ""; renderLint(); }, 4000); }

// ------------------------------------------------------------------------------------------------ files
function exportText() { return L.canonical(S.design); }
function doExport() {
  const blob = new Blob([exportText()], { type: "application/json" });
  const a = h("a", { href: URL.createObjectURL(blob), download: "elasticat.json" });
  document.body.append(a);
  a.click();
  setTimeout(() => { URL.revokeObjectURL(a.href); a.remove(); }, 1000);
}
function openDesign(d, what) {
  d = clone(d);
  const added = [ensureMatrix(d), ensureModes(d)].filter(Boolean).join(" · ");
  const r = L.compile(d, S.catalog);
  if (!r.pack) { flash(`${what}: ${r.errors.length} error(s), not opened (${r.errors[0]})`); return false; }
  S.inserted = added;
  edit((cur) => { for (const k of Object.keys(cur)) delete cur[k]; Object.assign(cur, clone(d)); });
  flash(`opened ${what}${added ? " · " + added : ""}`);
  return true;
}
async function fillOpen() {
  const sel = $("#open");
  sel.innerHTML = "";
  sel.append(h("option", { value: "" }, "OPEN…"), h("option", { value: "shipped" }, "the shipped layout"));
  try {
    const list = await (await fetch("designs/index.json")).json();
    for (const f of list) sel.append(h("option", { value: "designs/" + f }, f.replace(/\.json$/, "")));
  } catch (e) { /* no designs */ }
}

function bindUI() {
  $("#undo").onclick = undo;
  $("#redo").onclick = redo;
  $("#export").onclick = doExport;
  $("#import").onclick = () => $("#file").click();
  $("#file").onchange = async (ev) => {
    const f = ev.target.files[0];
    if (!f) return;
    try { openDesign(JSON.parse(await f.text()), f.name); } catch (e) { flash(`${f.name}: not JSON (${e.message})`); }
    ev.target.value = "";
  };
  $("#open").onchange = async (ev) => {
    const v = ev.target.value;
    ev.target.value = "";
    if (v === "shipped") openDesign(S.shipped, "the shipped layout");
    else if (v) try { openDesign(await (await fetch(v)).json(), v); } catch (e) { flash(`${v}: ${e.message}`); }
  };
  $("#dname").onchange = (ev) => edit((d) => { d.name = ev.target.value.trim() || "untitled"; });
  $("#search").oninput = renderTray;
  bindTips();
  const tray = $("#traylist");
  tray.addEventListener("dragover", (ev) => { if (drag && drag.ref) { ev.preventDefault(); tray.classList.add("over"); } });
  tray.addEventListener("dragleave", () => tray.classList.remove("over"));
  tray.addEventListener("drop", (ev) => { ev.preventDefault(); tray.classList.remove("over"); const d = drag; drag = null; drop(d, { tray: true }); });
  document.addEventListener("keydown", (ev) => {
    const typing = ["INPUT", "TEXTAREA", "SELECT"].includes(document.activeElement?.tagName);
    if ((ev.ctrlKey || ev.metaKey) && ev.key.toLowerCase() === "z" && !typing) { ev.preventDefault(); ev.shiftKey ? redo() : undo(); }
    else if ((ev.key === "Delete" || ev.key === "Backspace") && S.sel && !typing) { ev.preventDefault(); if (fixedSlots(S.sel)) flash("the MATRIX page's knobs are fixed"); else edit(() => setSlot(S.sel, null)); }
  });
}

// ------------------------------------------------------------------------------------------------ tooltips
// any element with data-tip: its text in a box by the pointer after a short rest (the descriptions: slots, tray chips,
// page headers, categories, the context)
function bindTips() {
  const tip = $("#tip");
  let cur = null, timer = 0, x = 0, y = 0;
  const place = () => {
    const r = tip.getBoundingClientRect();
    tip.style.left = Math.max(4, Math.min(innerWidth - r.width - 8, x + 14)) + "px";
    tip.style.top = (y + 18 + r.height > innerHeight ? Math.max(4, y - r.height - 10) : y + 18) + "px";
  };
  const hide = () => { clearTimeout(timer); cur = null; tip.hidden = true; };
  document.addEventListener("mouseover", (ev) => {
    const e = ev.target.closest?.("[data-tip]");
    if (e === cur) return;
    hide();
    if (!e || drag) return;
    cur = e;
    timer = setTimeout(() => { if (cur !== e || !e.isConnected) return; tip.textContent = e.dataset.tip; tip.hidden = false; place(); }, 280);
  });
  document.addEventListener("mousemove", (ev) => { x = ev.clientX; y = ev.clientY; if (!tip.hidden) place(); });
  document.addEventListener("mousedown", hide, true);
  document.addEventListener("dragstart", hide, true);
  document.addEventListener("wheel", () => { if (!tip.hidden) hide(); }, { passive: true });
}

// ------------------------------------------------------------------------------------------------ the FM-1 panel
// The FM-1 as RemoteFM1's web player draws it (tools/layout/fm1/, vendored), wired to the preview's core: buttons and
// keys held while the mouse holds them (Shift-, right- or ctrl-click latches one: SHIFT + a button, a held step + a
// knob with one mouse; a click lets it go, Escape or Shift-click on an empty spot lets all go), keys with a velocity
// from where they are struck (top soft, bottom hard), knobs by a vertical drag or the wheel (detents as they come:
// the firmware accelerates as on the unit), the MASTER pot. The LEDs and the live labels (ec_app_labels) every frame;
// its display mirrors the preview. The computer keyboard plays it (FM1VST's map) after a click on the panel, until a
// click elsewhere, never while a text field has the focus.
function setupPanel() {
  const canvas = $("#panel");
  const lcd = { canvas: $("#screen") };
  const sent = { btn: new Uint8Array(14), key: new Uint8Array(27), master: -1 };
  const vel = new Uint8Array(27);
  const controls = new Controls((snap) => {
    for (let b = 0; b < 14; b++) { const on = controls.btnHeld(b) ? 1 : 0; if (on !== sent.btn[b]) { sent.btn[b] = on; S.api.button(b, on); } }
    for (let k = 0; k < 27; k++) {
      const on = controls.keyHeld(k) ? 1 : 0;
      if (on !== sent.key[k]) { sent.key[k] = on; S.api.key(k, on, on ? vel[k] || controls.keyVel[k] || 100 : 0); if (!on) vel[k] = 0; }
    }
    for (let r = 0; r < 7; r++) { const d = snap.enc[roleToMatrix(r)]; if (d) S.api.turn(r, d); }
    if (snap.master !== sent.master) { sent.master = snap.master; S.api.master(snap.master); }
  });
  const view = new DeviceView(canvas, controls, lcd);
  const w = JSON.parse(S.api.wiring());
  view.setKeymap(Array.from({ length: 6 }, (_, r) => w.keymap.slice(r * 11, r * 11 + 11)));
  view.setRunning(true);
  S.api.master(controls.master);
  sent.master = controls.master;
  // the velocity: where the key is struck (registered before the panel's own pointer handler, so it is known first)
  canvas.addEventListener("pointerdown", (ev) => {
    const r = canvas.getBoundingClientRect(), x = ev.clientX - r.left, y = ev.clientY - r.top, hit = view.hit(x, y);
    S.panel.active = true;
    if (!hit || hit.kind !== KM.KEY || !view.L) return;
    const kb = view.L.keybed, k = view.L.kg.keys.find((q) => q.k === hit.idx);
    const f = k ? Math.max(0, Math.min(1, (y - kb.y - k.y) / k.h)) : 0.7;
    vel[hit.idx] = Math.round(24 + 103 * f);
  });
  document.addEventListener("pointerdown", (ev) => { if (ev.target !== canvas) S.panel.active = false; }, true);
  attachPointer(canvas, view, controls, {});
  new Keyboard({ controls, enabled: () => S.panel.active });
  const fit = () => {
    const r = canvas.parentElement.getBoundingClientRect();
    const wd = Math.max(200, Math.floor(r.width)), ht = Math.round(wd * 744 / 1146);
    canvas.style.width = wd + "px"; canvas.style.height = ht + "px";
    view.layout(wd, ht, window.devicePixelRatio || 1);
  };
  new ResizeObserver(fit).observe(canvas.parentElement);
  fit();
  S.panel = { view, controls, canvas, epoch: -1, active: false, leds: new Uint8Array(44) };
}
function drawPanel() {
  const P = S.panel;
  if (!P) return;
  const p = S.api.leds(), m = S.m.HEAPU8;
  P.leds = m.slice(p, p + 44);
  P.view.setLeds(P.leds, m[p + 44]);
  const lb = JSON.parse(S.api.labels());
  if (lb.epoch !== P.epoch) { P.epoch = lb.epoch; P.live = lb.list; P.view.setLabels(panelLabels(null, lb.list)); }
  P.view.draw();
}

// ------------------------------------------------------------------------------------------------ the column widths
// tree | side | preview, two splitters: the tree a share of the width (30% by default), the side in px, the preview
// the rest (never under its minimum, so the screen and the FM-1 stay whole). Kept in this browser; a double-click on
// a splitter gives its column back its default.
const COLS_STORE = "elasticat-layout-cols-v1";
const COLS = { tree: 0.30, side: 300, treeMin: 300, sideMin: 220, centreMin: 470, split: 6 };
const cols = { tree: COLS.tree, side: COLS.side };
try { Object.assign(cols, JSON.parse(localStorage.getItem(COLS_STORE)) || {}); } catch (e) { /* defaults */ }
if (!(cols.tree > 0 && cols.tree < 1)) cols.tree = COLS.tree;
if (!(cols.side > 0 && cols.side < 4000)) cols.side = COLS.side;
function sizeColumns(save) {
  const main = $("main"), W = main.clientWidth, room = W - 2 * COLS.split - COLS.centreMin;
  const side = Math.round(Math.max(COLS.sideMin, Math.min(cols.side, room - COLS.treeMin)));
  const tree = Math.round(Math.max(COLS.treeMin, Math.min(cols.tree * W, room - side)));
  main.style.setProperty("--tree-w", tree + "px");
  main.style.setProperty("--side-w", side + "px");
  if (save) try { localStorage.setItem(COLS_STORE, JSON.stringify(cols)); } catch (e) { /* not kept */ }
}
function setupSplitters() {
  const main = $("main");
  [["#split0", "tree"], ["#split1", "side"]].forEach(([sel, which]) => {
    const sp = $(sel);
    sp.addEventListener("pointerdown", (ev) => {
      ev.preventDefault();
      sp.setPointerCapture(ev.pointerId);
      sp.classList.add("drag"); document.body.classList.add("coldrag");
      const x0 = ev.clientX, t0 = $("#tree").offsetWidth, s0 = $("#side").offsetWidth, W = main.clientWidth;
      const move = (e) => {
        const dx = e.clientX - x0;
        if (which === "tree") cols.tree = (t0 + dx) / W; else cols.side = s0 + dx;
        sizeColumns(false);
      };
      const up = () => {
        sp.removeEventListener("pointermove", move); sp.removeEventListener("pointerup", up); sp.removeEventListener("pointercancel", up);
        sp.classList.remove("drag"); document.body.classList.remove("coldrag");
        // keep what the clamps gave, not what the pointer asked
        cols.tree = $("#tree").offsetWidth / W; cols.side = $("#side").offsetWidth;
        sizeColumns(true);
      };
      sp.addEventListener("pointermove", move); sp.addEventListener("pointerup", up); sp.addEventListener("pointercancel", up);
    });
    sp.addEventListener("dblclick", () => { cols[which] = COLS[which]; sizeColumns(true); });
  });
  window.addEventListener("resize", () => sizeColumns(false));
  sizeColumns(false);
}
setupSplitters();

// ------------------------------------------------------------------------------------------------ the test API
function ref(scope, a = {}) { return { scope, cat: a.cat, pi: a.pi, bus: a.bus, vi: a.vi ?? -1, lane: a.lane ?? 0, k: a.k ?? 0 }; }
window.designer = {
  ready: false,
  ref, drop, undo, redo, exportText, select,
  design: () => clone(S.design),
  compiled: () => ({ errors: S.compiled.errors, warnings: S.compiled.warnings, pack: S.compiled.pack }),
  shippedPack: () => S.shippedPack,
  jsPackOfShipped: () => L.compile(S.shipped, S.catalog).pack,
  cPack: () => S.api.pack(),
  modes: () => JSON.parse(S.api.modes()),           // the firmware's modes and black keys (the loaded layout)
  modeList: () => L.modeList(S.design),
  setKey: (cat, pi, k) => setPageKey(cat, pi, k),
  state: () => { readState(); return S.st; },
  goto: (cat, page, lane, settings = 0) => { S.api.f3(0); S.api.goto(L.CATS.indexOf(cat), page, lane, settings); S.api.run(40); readState(); markActive(); },
  context: (c) => { Object.assign(S.ctx, c); pushCtx(); render(); },
  run: (ms) => S.api.run(ms),
  turn: (role, det) => S.api.turn(role, det),
  pixels: () => { draw(); const p = S.api.rgba(); return Array.from(S.m.HEAPU8.subarray(p, p + 240 * 240 * 4)); },
  variantOf: (cat, pi) => curVariant("page", catObj(cat).pages[pi]),
  edit: (fn) => edit(fn),                 // (tests: any change to the design, as one undo step)
  movePage: (from, pi, to, at) => movePage(from, pi, to, at),
  open: (d, what = "a test design") => openDesign(d, what),
  fixedSlot: (r) => fixedSlots(r),
  trayNames: (q = "") => { $("#search").value = q; renderTray(); const r = [...document.querySelectorAll("#traylist .chip")].map((c) => c.dataset.name); $("#search").value = ""; renderTray(); return r; },
  // the drawn FM-1: a control's centre in page coordinates (kind "btn" | "key" | "knob", the label / key / role)
  panel: {
    where: (kind, idx) => {
      const v = S.panel.view, r = S.panel.canvas.getBoundingClientRect();
      const c = v.where(kind === "btn" ? KM.BTN : kind === "key" ? KM.KEY : KM.SELECT, idx);
      return c && { x: r.left + c.x, y: r.top + c.y };
    },
    leds: () => Array.from(S.panel.leds),
    keyLevels: () => Array.from({ length: 27 }, (_, k) => S.panel.view.keyLevel(k)),
    live: () => S.panel.live,
    labels: () => clone(S.panel.view.lbl),
    held: () => ({ btn: Array.from({ length: 14 }, (_, b) => S.panel.controls.btnHeld(b)), key: Array.from({ length: 27 }, (_, k) => S.panel.controls.keyHeld(k)) }),
    releaseLatches: () => S.panel.controls.releaseLatches(),
  },
  desc: () => S.desc,
  // the three columns' widths (px) and the page's
  columns: () => ({ main: $("main").clientWidth, tree: $("#tree").offsetWidth, side: $("#side").offsetWidth, centre: $("#centre").offsetWidth }),
};
boot().catch((e) => { document.body.prepend(h("pre", { style: "color:#ff4a3d" }, "the preview did not start: " + (e && e.stack || e))); window.designer.error = String(e); });
