// SPDX-License-Identifier: GPL-3.0-only
// Elasticat FM-1's layout in the browser: the design's checks and its packed form, the same as tools/layout/gen_layout.py
// (the designer's preview loads the pack into the firmware's own ec_pages.c; tools/layout/test_designer.mjs checks
// that this pack of the shipped design is byte for byte the generator's), the canonical JSON text, variant matching.
// An ES module: the page imports it, node can too.

export const CATS = ["TRIG", "SRC", "FILT", "AMP", "FX", "MOD", "MST", "FILE", "PAT", "XFD"];
// 0.6.0: every category is a mode (docs/UX-V2.md §3): its name, the header's tag, its FM-1 button and its place among
// that button's modes; the fixed screens are modes without params. EC_MB_* = BUTTONS index + 1. A design may leave
// these out (a design before 0.6.0): it gets these (as gen_layout.py)
export const BUTTONS = ["FX", "SEL", "ENV", "LFO", "EDIT", "HOME", "FILE", "ARP", "SEQ"];
export const MODE_DEFAULT = { TRIG: ["TRIG", "TRIG", "HOME", 2], SRC: ["LOOP ENGINE", "LOOP", "HOME", 1],
  FILT: ["FILTER", "FILTER", "SEL", 1], AMP: ["AMP", "AMP", "ENV", 1], FX: ["FX", "FX", "FX", 1],
  MOD: ["MODULATION", "MOD", "LFO", 1], MST: ["MIX", "MIX", "EDIT", 1], FILE: ["FILE", "FILE", "FILE", 1],
  PAT: ["PATTERN LAUNCH", "PATTERNS", "SEQ", 1], XFD: ["CROSSFADER", "CROSSFADER", "ARP", 1] };
export const SCREENS = ["KEYBOARD", "PAGES", "SAMPLER"];
export const SCREEN_DEFAULT = { KEYBOARD: ["HOME", 3], PAGES: ["EDIT", 2], SAMPLER: ["FILE", 2] };
// the black keys B1..B11 (a page's "key", 0 = none); a category that names none: the source template's pages 1..5 =
// B1..B5, MOD: MATRIX B1, MACRO n B(n+1), MACROS B11; FX's buses S1 S2 MST: B3 B4 B5
export const NBKEY = 11, TEMPLATE = ["TRIG", "SRC", "FILT", "AMP", "FX"], BUS_KEY_DEFAULT = [3, 4, 5];
export const MODE_MAX = 16, TAG_MAX = 10, BUS_TAG = "FILTER";
// a category's mode as the design gives it, with the defaults: {name, tag, button, order}
export function modeOf(c) {
  const d = MODE_DEFAULT[c.cat] || ["?", "?", "FX", 1];
  return { name: c.mode ?? d[0], tag: c.tag ?? d[1], button: c.button ?? d[2], order: c.order ?? d[3] };
}
// every mode of a design, in the panel's button order: [{button, order, name, cat (null: a fixed screen)}]
export function modeList(design) {
  const out = [];
  for (const c of design.categories || []) if (c && CATS.includes(c.cat)) { const m = modeOf(c); out.push({ ...m, cat: c.cat }); }
  const scr = Array.isArray(design.screens) ? design.screens : SCREENS.map((m) => ({ mode: m, button: SCREEN_DEFAULT[m][0], order: SCREEN_DEFAULT[m][1] }));
  for (const x of scr) if (x && typeof x === "object") out.push({ name: x.mode, button: x.button, order: x.order, cat: null });
  return out.sort((a, b) => (BUTTONS.indexOf(a.button) - BUTTONS.indexOf(b.button)) || (a.order - b.order));
}
// the black key of each page of a category (as compile() gives them: the design's, or the defaults when it names none)
export function pageKeys(c) {
  const ps = c.pages || [];
  if (ps.some((p) => p && typeof p === "object" && "key" in p)) return ps.map((p) => (Number.isInteger(p?.key) ? p.key : 0));
  return ps.map((p, i) => {
    if (TEMPLATE.includes(c.cat)) return i < 5 ? i + 1 : 0;
    if (c.cat === "MOD") return p.picture === "matrix" ? 1 : p.picture === "macros" ? 11 : p.picture === "macro" ? (p.macro || 1) + 1 : 0;
    return 0;
  });
}
export const PICS = ["steps", "wave", "wave-machine", "wave-warp", "wave-range", "filter", "fenv", "aenv", "fx", "sends",
  "lfo1", "lfo2", "menv", "vel", "macros", "macro", "mix", "scene", "file", "razor", "pattern", "matrix"];
export const FIXED = { mix: "MST", scene: "XFD", file: "FILE", razor: "FILE", pattern: "PAT", matrix: "MOD" };
// the MATRIX page (0.5.0): its knobs are fixed (SELECT picks the row; KNOB 1-3 its SRC, DST, DEPTH)
export const MATRIX_LANES = [["mtx.src", "mtx.dst", "mtx.depth", null]];
export const MATRIX_PAGE = () => ({ title: "MATRIX", picture: "matrix", lanes: JSON.parse(JSON.stringify(MATRIX_LANES)) });
// the mod matrix's slot params (reached through the MATRIX page, never placed) and the params retired in 0.5.0 (kept
// for old saves, read by nothing: never in the tray, never "not on any page"; placing one is a warning)
export const isMtx = (n) => /^mtx[1-8]_(src|dst|depth)$/.test(n);
export const isRetired = (n) => ["lfo1_dest", "lfo2_dest", "menv_dest", "mvel_dest"].includes(n) ||
  /^macro[1-4]_(pitch|cutoff|res|amp|pan)_depth$/.test(n);
export const MACHINES = ["LOOP", "TRIG", "SLICE", "RAZOR", "S.POLY", "R.POLY", "NEIGH", "INPUT"];
export const MCLASS = { loop: [1, 2], slice: [3, 5], razor: [4, 6], neighbor: [7], input: [8] };
export const OPTS = {
  warp: ["TAPE", "VARI", "DOMINO", "GRAIN", "OLA", "PITCH", "HARM", "WTBL", "FREEZE", "FORMNT", "GSTR", "GSTR2", "CHOP"],
  filter: ["CLASSC", "MORPH", "LADDER", "COMB", "FORMNT", "SVF"],
  fenv: ["ADSR", "AHR"],
  env: ["ADSR", "AHR"],
  fx: ["NONE", "DELAY", "REVERB", "LOFI", "COMP", "DESTRY", "ECHO", "B.HOLE", "CHORUS", "FLANGR", "PHASER", "DJ EQ",
    "DUCK", "T.ECHO", "CASSET", "MOTION", "RINGS", "LIMIT"],
};
// item kinds (src/app/ec_pages.h EC_IT_*) and the named items that are not track params: [kind, id, default label]
export const IT = { BLANK: 0, PARAM: 1, GLOBAL: 2, SLOT: 3, RAZOR: 4, E5: 5, MTX: 6 };
export const SPECIAL = {
  "global.bpm": [2, 0, "BPM"], "global.swing": [2, 1, "SWNG"], "global.glen": [2, 2, "GLEN"],
  "global.quant": [2, 3, "QUANT"], "global.rtn": [2, 4, "RTN"],
  "slot.bpm": [3, 0, "BPM"], "slot.steps": [3, 1, "STEP"], "slot.gain": [3, 2, "GAIN"], "slot.trim_start": [3, 3, "T-ST"],
  "slot.trim_end": [3, 4, "T-EN"],
  "razor.start": [4, 0, "STRT"], "razor.end": [4, 1, "END"], "razor.play": [4, 2, "PLAY"], "razor.zoom": [4, 3, "ZOOM"],
  "razor.snap": [4, 4, "SNAP"], "razor.choke": [4, 5, "CHOK"], "razor.pitch": [4, 6, "P/T"], "razor.sens": [4, 7, "SENS"],
  "e5.xfd": [5, 0, "XFD"], "e5.choke": [5, 1, "CHOK"], "e5.gbpm": [5, 2, "GBPM"], "e5.name": [5, 3, "NAME"],
  "e5.mute": [5, 4, "SMUT"],
  "mtx.src": [6, 0, "SRC"], "mtx.dst": [6, 1, "DST"], "mtx.depth": [6, 2, "DEPTH"],
};
export const STOPPED = new Set(["mode", "filter_machine", "filter_mix_mode"]);
export const MAX_LANES = 4, SETUP_LANES = 2, MAX_PAGES = 16, CELL_LABEL_WARN = 5, LABEL_MAX = 10, TITLE_ROOM = 72;
export const textW = (s) => (s ? s.length * 4 - 1 : 0);
const ZERO = () => ({ mach: 0, warp: 0, filt: 0, fenv: 0, env: 0, fx: 0 });

export const slotName = (s) => (s == null ? null : typeof s === "string" ? s : s.item);
export const slotLabel = (s) => (s && typeof s === "object" ? s.label || null : null);

// ------------------------------------------------------------------------------------------------ variants
// the masks of a "when" (bit k: option value k + 1; machine: the classes' machines)
export function whenMasks(w) {
  const m = ZERO();
  for (const [k, vals] of Object.entries(w || {})) {
    for (const v of vals || []) {
      if (k === "machine") for (const x of MCLASS[v] || []) m.mach |= 1 << (x - 1);
      else if (OPTS[k] && OPTS[k].includes(v)) m[k === "filter" ? "filt" : k] |= 2 ** OPTS[k].indexOf(v) >>> 0;
    }
  }
  return m;
}
// ctx: {mach 1..8, warp 1..13, filt 1..6, fenv 1..2, env 1..2, fx 1..18}
export function masksMatch(m, ctx) {
  const has = (mask, v) => !mask || (v >= 1 && v <= 32 && ((mask >>> (v - 1)) & 1));
  return has(m.mach, ctx.mach) && has(m.warp, ctx.warp) && has(m.filt, ctx.filt) && has(m.fenv, ctx.fenv) &&
    has(m.env, ctx.env) && has(m.fx, ctx.fx);
}
// the page whose variants a page uses (itself, or its same_as)
export function sourcePage(design, page) {
  if (!page || !page.same_as) return page;
  const [cat, title] = [page.same_as.split("/")[0], page.same_as.split("/").slice(1).join("/")];
  const c = (design.categories || []).find((x) => x.cat === cat);
  return (c && (c.pages || []).find((p) => p.title === title)) || null;
}
// the variant of page shown in ctx: its index in variants, -1 = the page's own lanes
export function matchVariant(design, page, ctx) {
  const src = sourcePage(design, page) || page;
  const vs = src.variants || [];
  for (let i = 0; i < vs.length; i++) if (masksMatch(whenMasks(vs[i].when), ctx)) return i;
  return -1;
}
export function whenText(w) {
  if (!w) return "otherwise";
  return Object.entries(w).map(([k, v]) => (k === "fenv" ? "F.ENV " : k === "env" ? "ENV " : "") + v.join("/").toUpperCase()).join(" · ");
}

// ------------------------------------------------------------------------------------------------ the checks + pack
export function compile(design, catalog) {
  const params = new Map(catalog.map((p) => [p.name, p]));
  const errors = [], warnings = [];
  const items = [], vars = [], pages = [], pkey = [];
  const tags = {};
  let legacy = false;
  const placed = new Map();
  const err = (w, m) => errors.push(`${w}: ${m}`);
  const warn = (w, m) => warnings.push(`${w}: ${m}`);
  const BLANK = [0, 0, 0, null, null];

  function item(where, slot, ctx) {
    if (slot == null) return BLANK;
    let name, label = null;
    if (typeof slot === "string") name = slot;
    else if (slot && typeof slot === "object" && typeof slot.item === "string") {
      name = slot.item;
      label = slot.label ?? null;
      const extra = Object.keys(slot).filter((k) => k !== "item" && k !== "label");
      if (extra.length) err(where, `unknown field(s) ${extra.sort().join(", ")}`);
      if (label !== null && (typeof label !== "string" || !label)) { err(where, `${name}: the label must be a text`); label = null; }
    } else { err(where, `a slot is null, an item name or {"item", "label"}: ${JSON.stringify(slot)}`); return BLANK; }
    if (label !== null) {
      if (label !== label.toUpperCase()) err(where, `${name}: the label '${label}' has lower case (the font has capitals only)`);
      if (textW(label) > textW("X".repeat(LABEL_MAX))) err(where, `${name}: the label '${label}' is longer than ${LABEL_MAX} characters`);
      if ([...label].some((ch) => ch.charCodeAt(0) < 32 || ch.charCodeAt(0) > 95)) err(where, `${name}: the label '${label}' has characters the font lacks`);
    }
    if (SPECIAL[name]) {
      const [kind, id, dl] = SPECIAL[name];
      if (kind === IT.RAZOR && ctx.pic !== "razor") err(where, `${name} works on the RAZOR page only`);
      if (kind === IT.MTX && ctx.pic !== "matrix") err(where, `${name} works on the MATRIX page only`);
      const lb = label || dl;
      if (ctx.cells && lb.length > CELL_LABEL_WARN) warn(where, `${name}: the label '${lb}' is longer than ${CELL_LABEL_WARN} (the panel's knob label is cut)`);
      return [kind, id, 0, lb, name];
    }
    const p = params.get(name);
    if (!p) { err(where, `unknown item '${name}'`); return BLANK; }
    if (isMtx(name)) err(where, `${name} is a mod matrix slot's: it is reached through the MATRIX page (MOD), not placed`);
    else if (isRetired(name)) warn(where, `${name} is retired in 0.5.0: reads nothing (the mod matrix replaced it)`);
    if (ctx.cells && label && label.length > CELL_LABEL_WARN) warn(where, `${name}: the label '${label}' is longer than ${CELL_LABEL_WARN} (the panel's knob label is cut)`);
    if (!placed.has(name)) placed.set(name, []);
    placed.get(name).push(where);
    return [IT.PARAM, p.i, STOPPED.has(name) ? 1 : 0, label, name];
  }

  function lanes(where, ls, ctx, maxL) {
    if (ls == null) ls = [];
    if (!Array.isArray(ls) || ls.some((l) => !Array.isArray(l))) { err(where, "lanes are a list of lists"); return []; }
    if (ls.length > maxL) err(where, `${ls.length} lanes (at most ${maxL})`);
    const out = [], seen = new Map();
    ls.forEach((lane, li) => {
      if (lane.length > 4) err(where, `lane ${li + 1} has ${lane.length} slots (four knobs)`);
      lane.slice(0, 4).forEach((slot, k) => {
        const it = item(`${where} lane ${li + 1} KNOB ${k + 1}`, slot, ctx);
        if (it[4]) {
          const key = it[0] + ":" + it[1];
          if (seen.has(key)) err(where, `${it[4]} twice (${seen.get(key)} and lane ${li + 1} KNOB ${k + 1})`);
          seen.set(key, `lane ${li + 1} KNOB ${k + 1}`);
        }
        out.push(it);
      });
      if (li < ls.length - 1) for (let k = lane.slice(0, 4).length; k < 4; k++) out.push(BLANK);
    });
    while (out.length && out[out.length - 1][0] === 0) out.pop();
    return out;
  }

  function when(where, w) {
    const m = ZERO();
    if (!w || typeof w !== "object" || Array.isArray(w) || !Object.keys(w).length) {
      err(where, 'a variant needs "when" (machine / warp / filter / fenv / env / fx)');
      return m;
    }
    for (const [k, vals] of Object.entries(w)) {
      if (!Array.isArray(vals) || !vals.length) { err(where, `when.${k} is a non-empty list`); continue; }
      if (k === "machine") {
        for (const v of vals) {
          if (!MCLASS[v]) { err(where, `machine '${v}' (one of ${Object.keys(MCLASS).join(", ")})`); continue; }
          for (const x of MCLASS[v]) m.mach |= 1 << (x - 1);
        }
      } else if (OPTS[k]) {
        for (const v of vals) {
          if (!OPTS[k].includes(v)) { err(where, `${k} '${v}' (one of ${OPTS[k].join(", ")})`); continue; }
          m[k === "filter" ? "filt" : k] = (m[k === "filter" ? "filt" : k] | (2 ** OPTS[k].indexOf(v))) >>> 0;
        }
      } else err(where, `unknown condition '${k}'`);
    }
    return m;
  }

  function titleFits(where, cat, title) {
    const tag = tags[cat] ?? cat;      // (the header shows the mode's tag: "LOOP/WARP")
    if (title !== title.toUpperCase()) err(where, `the title '${title}' has lower case (the font has capitals only)`);
    if (textW(tag + "/" + title) > TITLE_ROOM) err(where, `the title '${title}' is too wide for the header (${tag}/ + ${Math.floor((TITLE_ROOM + 1) / 4) - tag.length - 1} characters at most)`);
  }
  const isInt = (x) => Number.isInteger(x);
  function textOk(where, what, t, most) {
    if (typeof t !== "string" || !t) { err(where, `${what}: a text`); return false; }
    if (t !== t.toUpperCase() || [...t].some((ch) => ch.charCodeAt(0) < 32 || ch.charCodeAt(0) > 95)) err(where, `${what} '${t}': capitals, digits and the font's signs only`);
    if (t.length > most) err(where, `${what} '${t}': ${most} characters at most`);
    return true;
  }
  const mode = {}, screens = [];
  function modes(byname) {
    for (const n of CATS) {
      const c = byname.get(n) || {}, d = MODE_DEFAULT[n];
      let name = c.mode ?? d[0], tag = c.tag ?? d[1], btn = c.button ?? d[2], order = c.order ?? d[3];
      if (!textOk(n, "mode", name, MODE_MAX)) name = d[0];
      if (!textOk(n, "tag", tag, TAG_MAX)) tag = d[1];
      if (!BUTTONS.includes(btn)) { err(n, `button '${btn}' (one of ${BUTTONS.join(" ")})`); btn = d[2]; }
      if (!isInt(order) || order < 1) { err(n, `order ${JSON.stringify(order)}: 1 or more (the place among the button's modes)`); order = 1; }
      mode[n] = [name, tag, btn, order];
      tags[n] = tag;
    }
    const dflt = () => SCREENS.map((m) => ({ mode: m, button: SCREEN_DEFAULT[m][0], order: SCREEN_DEFAULT[m][1] }));
    let scr = design.screens ?? dflt();
    if (!Array.isArray(scr) || JSON.stringify(scr.map((x) => (x && typeof x === "object" ? x.mode : null))) !== JSON.stringify(SCREENS)) {
      err("screens", `"screens" lists ${SCREENS.join(", ")} in that order`);
      scr = dflt();
    }
    for (const x of scr) {
      const w = `screens/${x.mode}`;
      const ex = Object.keys(x).filter((k) => !["mode", "button", "order", "note"].includes(k));
      if (ex.length) err(w, `unknown field(s) ${ex.sort().join(", ")}`);
      let btn = x.button, order = x.order;
      if (!BUTTONS.includes(btn)) { err(w, `button '${btn}' (one of ${BUTTONS.join(" ")})`); btn = SCREEN_DEFAULT[x.mode][0]; }
      if (!isInt(order) || order < 1) { err(w, `order ${JSON.stringify(order)}: 1 or more`); order = 1; }
      screens.push([x.mode, btn, order]);
    }
    const on = new Map();
    const put = (b, o, m) => { if (!on.has(b)) on.set(b, []); on.get(b).push([o, m]); };
    for (const n of CATS) put(mode[n][2], mode[n][3], mode[n][0]);
    for (const [m, b, o] of screens) put(b, o, m);
    for (const [b, ms] of on) {
      const os = ms.map((x) => x[0]).sort((a, c) => a - c);
      if (os.some((o, i) => o !== i + 1)) {
        const srt = ms.slice().sort((a, c) => a[0] - c[0] || (a[1] < c[1] ? -1 : 1));
        err("modes", `the ${b} button's modes are numbered ${os.join(" ")} (1..${ms.length}, each once): ${srt.map(([o, m]) => `${m} ${o}`).join(", ")}`);
      }
    }
  }
  function keys(n, c, p0, np) {
    const named = (c.pages || []).some((p) => p && typeof p === "object" && "key" in p);
    if (!named) for (let i = 0; i < np; i++) {
      const pic = PICS[pages[p0 + i][1]];
      let k = 0;
      if (TEMPLATE.includes(n) && i < 5) k = i + 1;
      else if (n === "MOD") k = pic === "matrix" ? 1 : pic === "macros" ? 11 : pic === "macro" ? pages[p0 + i][2] + 2 : 0;
      pkey[p0 + i] = k;
    }
    const seen = new Map();
    if (n === "FX") ["S1", "S2", "MST"].forEach((b, bi) => { if (busKey[bi]) seen.set(busKey[bi], "bus " + b); });
    for (let i = 0; i < np; i++) {
      const k = pkey[p0 + i];
      if (!k) continue;
      if (seen.has(k)) err(`${n}/${pages[p0 + i][0]}`, `B${k} opens ${seen.get(k)} already`);
      seen.set(k, pages[p0 + i][0]);
    }
  }
  let busKey = BUS_KEY_DEFAULT.slice();

  function findPage(ref) {
    const i = ref.indexOf("/");
    const cat = i < 0 ? ref : ref.slice(0, i), title = i < 0 ? "" : ref.slice(i + 1);
    for (const c of design.categories || []) if (c.cat === cat) for (const p of c.pages || []) if (p.title === title) return p;
    return null;
  }

  function page(where, p, cat, kind = "page") {
    if (!p || typeof p !== "object" || Array.isArray(p)) { err(where, "a page is an object"); p = {}; }
    const extra = Object.keys(p).filter((k) => !["title", "picture", "lanes", "variants", "note", "same_as", "macro", "key"].includes(k) || (k === "key" && kind !== "page"));
    if (extra.length) err(where, `unknown field(s) ${extra.sort().join(", ")}`);
    let bkey = p.key ?? 0;
    if (!isInt(bkey) || bkey < 0 || bkey > NBKEY) { err(where, `key ${JSON.stringify(bkey)}: a black key 1..${NBKEY} (0 or none: no key)`); bkey = 0; }
    let src = p;
    if ("same_as" in p) {
      src = typeof p.same_as === "string" ? findPage(p.same_as) : null;
      if (src == null) { err(where, `same_as '${p.same_as}': no such page (CAT/TITLE)`); src = {}; }
      else if ("same_as" in src) err(where, `same_as '${p.same_as}': that page is itself a copy`);
      if ("lanes" in p || "variants" in p) err(where, "a page with same_as has no lanes or variants of its own");
    }
    let title = p.title;
    if (typeof title !== "string" || !title) { err(where, "a page needs a title"); title = "?"; }
    let pic = p.picture ?? src.picture ?? "steps";
    if (!PICS.includes(pic)) { err(where, `picture '${pic}' (one of ${PICS.join(", ")})`); pic = "steps"; }
    if (kind === "page" || kind === "bus") titleFits(where, cat, title);
    const home = pic === "scene" && legacy ? "MST" : FIXED[pic];
    if (FIXED[pic] && kind === "page" && home !== cat) err(where, `the ${pic} picture stays in ${home} (its page has behaviour of its own)`);
    let arg = 0;
    if (pic === "macro") {
      const m = p.macro ?? src.macro;
      if (![1, 2, 3, 4].includes(m)) err(where, 'a macro page needs "macro": 1..4');
      else arg = m - 1;
    } else if ("macro" in p) err(where, '"macro" is for the macro picture');
    if ("note" in p && typeof p.note !== "string") err(where, "a note is a text");
    if (pic === "matrix" && ((src.variants && src.variants.length) ||
        JSON.stringify((Array.isArray(src.lanes) ? src.lanes : [[]]).filter(Array.isArray).flat().map(slotName)) !== JSON.stringify(MATRIX_LANES[0])))
      err(where, `the MATRIX page's knobs are fixed: lanes ${pyDumps(MATRIX_LANES)}, no variants`);
    const ctx = { pic, cells: ["page", "bus", "f3"].includes(kind) };
    const maxL = { page: MAX_LANES, bus: MAX_LANES, setup: SETUP_LANES, f3: 1 }[kind];
    const var0 = vars.length;
    let variants = src.variants ?? [];
    if (!Array.isArray(variants)) { err(where, "variants are a list"); variants = []; }
    const seenWhen = [];
    variants.forEach((v, vi) => {
      const w = `${where} variant ${vi + 1}`;
      if (!v || typeof v !== "object") { err(w, "a variant is an object"); return; }
      const ex = Object.keys(v).filter((k) => !["when", "title", "lanes", "note"].includes(k));
      if (ex.length) err(w, `unknown field(s) ${ex.sort().join(", ")}`);
      const masks = when(w, v.when);
      const key = JSON.stringify(Object.entries(masks).sort());
      if (seenWhen.includes(key)) warn(w, "the same conditions as an earlier variant: never shown");
      seenWhen.push(key);
      let vt = v.title ?? null;
      if (vt !== null) {
        if (typeof vt !== "string" || !vt) { err(w, "a variant's title is a text"); vt = null; }
        else if (kind === "page" || kind === "bus") titleFits(w, cat, vt);
      }
      const its = lanes(w, v.lanes ?? [], ctx, maxL);
      vars.push([masks, vt, items.length, its.length, w]);
      items.push(...its);
    });
    const its = lanes(where, src.lanes ?? [], ctx, maxL);
    vars.push([ZERO(), null, items.length, its.length, where]);
    items.push(...its);
    pages.push([title, PICS.indexOf(pic), arg, var0, vars.length - var0, where]);
    pkey.push(bkey);
    return pages.length - 1;
  }

  if (!design || design.format !== "elasticat-layout" || design.version !== 1) {
    return { errors: ["not an elasticat-layout version 1 file"], warnings, pack: null };
  }
  const extra = Object.keys(design).filter((k) => !["format", "version", "name", "note", "categories", "bus", "f3", "screens"].includes(k));
  if (extra.length) err("design", `unknown field(s) ${extra.sort().join(", ")}`);
  if (!Array.isArray(design.categories)) return { errors: ['"categories" is a list'], warnings, pack: null };
  const byname = new Map();
  for (const c of design.categories) {
    const n = c && typeof c === "object" ? c.cat : undefined;
    if (!CATS.includes(n)) err("categories", `unknown category '${n}' (${CATS.join(" ")})`);
    else if (byname.has(n)) err("categories", `${n} twice`);
    else byname.set(n, c);
  }
  // a design before 0.6.0 has no XFD: CROSSFADER then shows its SCENE page where it is (MST)
  legacy = !byname.has("XFD");
  for (const n of CATS) if (!byname.has(n) && n !== "XFD") err("categories", `${n} is missing`);
  if (legacy) warn("XFD", "no XFD category (a design before 0.6.0): CROSSFADER shows MST's SCENE page");
  if (errors.length) return { errors, warnings, pack: null };
  modes(byname);
  if (Array.isArray(design.bus)) design.bus.slice(0, 3).forEach((b, bi) => {
    if (b && typeof b === "object" && "key" in b) {
      let k = b.key;
      if (!isInt(k) || k < 0 || k > NBKEY) { err(`bus ${b.bus ?? "?"}`, `key ${JSON.stringify(k)}: a black key 1..${NBKEY} in FX (0: none)`); k = 0; }
      busKey[bi] = k;
    }
  });
  const nz = busKey.filter((k) => k);
  if (new Set(nz).size !== nz.length) err("bus", `two buses on one key (${busKey.map((k) => "B" + k).join(" ")})`);
  const cats = [];
  const fixedCount = Object.fromEntries(Object.keys(FIXED).map((k) => [k, 0]));
  for (const n of CATS) {
    if (n === "XFD" && legacy) { cats.push(null); continue; }
    const c = byname.get(n);
    const ex = Object.keys(c).filter((k) => !["cat", "pages", "setup", "mode", "tag", "button", "order"].includes(k));
    if (ex.length) err(n, `unknown field(s) ${ex.sort().join(", ")}`);
    let ps = c.pages;
    if (!Array.isArray(ps) || !ps.length) { err(n, "a category needs one page at least"); ps = []; }
    if (ps.length > MAX_PAGES) err(n, `${ps.length} pages (at most ${MAX_PAGES})`);
    const titles = new Set();
    const p0 = pages.length;
    ps.forEach((p, pi) => {
      const t = p && typeof p === "object" ? p.title : undefined;
      const where = `${n}/${t ? t : `page ${pi + 1}`}`;
      if (titles.has(t)) err(where, `two pages of ${n} are called ${t}`);
      titles.add(t);
      page(where, p, n);
      const pic = PICS[pages[pages.length - 1][1]];
      if (pic in fixedCount) fixedCount[pic]++;
    });
    let setup = -1;
    if (c.setup != null) setup = page(n + "/SETUP", { title: "SETUP", picture: "steps", lanes: c.setup }, n, "setup");
    cats.push([p0, pages.length - p0 - (setup >= 0 ? 1 : 0), setup]);
    keys(n, c, p0, cats[cats.length - 1][1]);
  }
  if (legacy) {
    const sc = pages.findIndex((pg) => PICS[pg[1]] === "scene");
    cats[CATS.indexOf("XFD")] = [sc >= 0 ? sc : cats[CATS.indexOf("MST")][0], 1, -1];
  }
  for (let [pic, cat] of Object.entries(FIXED)) {
    if (legacy && pic === "scene") cat = "MST";
    if (pic === "matrix" && fixedCount[pic] === 0) warn(cat, "no MATRIX page (0.5.0): the mod matrix is reached by MOD KEYS only; add it as MOD's first page");
    else if (pic !== "mix" && fixedCount[pic] !== 1) err(cat, `the ${pic} page must be there once (${fixedCount[pic]})`);
    if (pic === "mix" && fixedCount[pic] < 1) err(cat, "the mix page is missing");
  }
  let bus = design.bus;
  if (!Array.isArray(bus) || JSON.stringify(bus.filter((b) => b && typeof b === "object").map((b) => b.bus)) !== '["S1","S2","MST"]') {
    err("bus", '"bus" lists S1, S2, MST in that order');
    bus = [{}, {}, {}];
  }
  const busPage = [], busSetup = [];
  let mstSrc = -1;
  for (const b of bus) {
    const name = b.bus ?? "?";
    const ex = Object.keys(b).filter((k) => !["bus", "page", "setup", "src", "key"].includes(k));
    if (ex.length) err("bus " + name, `unknown field(s) ${ex.sort().join(", ")}`);
    busPage.push(page(`bus ${name}`, b.page ?? {}, BUS_TAG, "bus"));
    busSetup.push(b.setup != null ? page(`bus ${name}/SETUP`, { title: "SETUP", lanes: b.setup ?? [] }, "FX", "setup") : -1);
    if (name === "MST") {
      if (!("src" in b)) err("bus MST", '"src": the master bus\'s SRC page (the mix) is missing');
      else {
        mstSrc = page("bus MST/SRC", b.src, "SRC", "bus");
        if (PICS[pages[mstSrc][1]] !== "mix") err("bus MST/SRC", "the master bus's SRC page is the mix (picture mix)");
      }
    } else if ("src" in b) err("bus " + name, 'only the master bus has "src"');
  }
  if (!("f3" in design)) err("f3", "F3's lane is missing");
  const f3 = page("F3", { title: "F3", lanes: design.f3 ?? [] }, "PAT", "f3");
  const unplaced = catalog.map((p) => p.name).filter((n) => !placed.has(n) && !isMtx(n) && !isRetired(n)).sort();
  if (unplaced.length) warn("design", `not on any page: ${unplaced.join(", ")}`);
  if (errors.length) return { errors, warnings, pack: null, placed };
  const s = (x) => (x == null ? "" : x);
  const out = [`ECL2\t${design.name ?? ""}`];
  CATS.forEach((n, ci) => { const c = cats[ci], [name, tag, btn, order] = mode[n]; out.push(`C\t${c[0]}\t${c[1]}\t${c[2]}\t${BUTTONS.indexOf(btn) + 1}\t${order}\t${tag}\t${name}`); });
  for (const [m, b, o] of screens) out.push(`S\t${BUTTONS.indexOf(b) + 1}\t${o}\t${m}`);
  out.push(`B\t${[...busPage, ...busSetup, mstSrc, f3, ...busKey].join("\t")}`);
  pages.forEach(([t, pic, arg, v0, nv], i) => out.push(`P\t${pic}\t${arg}\t${pkey[i]}\t${v0}\t${nv}\t${t}`));
  for (const [m, vt, i0, n] of vars) out.push(`V\t${m.mach}\t${m.warp}\t${m.filt}\t${m.fenv}\t${m.env}\t${m.fx}\t${i0}\t${n}\t${s(vt)}`);
  for (const [kind, id, fl, lb] of items) out.push(`I\t${kind}\t${id}\t${fl}\t${s(lb)}`);
  return { errors, warnings, pack: out.join("\n") + "\n", placed, npages: pages.length, nvars: vars.length, nitems: items.length, legacy };
}

// ------------------------------------------------------------------------------------------------ canonical JSON
// the same text as gen_layout.py canonical(): two-space indent, a lane / a when / an item object on one line
export function canonical(d) {
  const inl = (x) => pyDumps(x);
  function emit(x, ind, key) {
    const pad = "  ".repeat(ind);
    if (["lanes", "setup", "f3"].includes(key) && Array.isArray(x)) {
      if (!x.length) return "[]";
      return "[\n" + x.map((l) => pad + "  " + inl(l)).join(",\n") + "\n" + pad + "]";
    }
    if (key === "when" || (x && typeof x === "object" && !Array.isArray(x) && "item" in x)) return inl(x);
    if (x && typeof x === "object" && !Array.isArray(x)) {
      const ks = Object.keys(x);
      if (!ks.length) return "{}";
      return "{\n" + ks.map((k) => `${pad}  ${JSON.stringify(k)}: ${emit(x[k], ind + 1, k)}`).join(",\n") + "\n" + pad + "}";
    }
    if (Array.isArray(x)) {
      if (!x.length) return "[]";
      return "[\n" + x.map((v) => pad + "  " + emit(v, ind + 1)).join(",\n") + "\n" + pad + "]";
    }
    return inl(x);
  }
  return emit(d, 0) + "\n";
}
// json.dumps(x, ensure_ascii=False, separators=(", ", ": "))
function pyDumps(x) {
  if (x === null || x === undefined) return "null";
  if (Array.isArray(x)) return "[" + x.map(pyDumps).join(", ") + "]";
  if (typeof x === "object") return "{" + Object.keys(x).map((k) => JSON.stringify(k) + ": " + pyDumps(x[k])).join(", ") + "}";
  return JSON.stringify(x);
}
