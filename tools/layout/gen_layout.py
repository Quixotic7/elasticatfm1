#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Elasticat FM-1's layout generator: design/layout/elasticat.json -> the C tables (src/app/ec_layout_gen.c,
ec_layout.h), checked first. Also writes the packed text form the designer's preview loads (--pack), the per-page knob
tables of docs/FM1-CONTROLS.md (--docs: the section between the layout markers) and the canonical JSON (--format).

    gen_layout.py [JSON] [--c OUT.c] [--symbol EC_LAYOUT_SHIPPED] [--pack OUT.txt] [--docs FM1-CONTROLS.md]
                  [--desc descriptions.json | --no-desc] [--format] [--check] [--quiet]

Default JSON: design/layout/elasticat.json. Errors stop it (exit 1, nothing written); warnings are printed. The
parameters (spec names -> EC_P_ ids, default labels) are read from src/seq/ec_params.c, so the generated file names
the ids by their enum symbols. The JSON shape is in tools/layout/README.md.

Descriptions (design/layout/descriptions.json, or --desc): every placeable item's and page's short text for the
designer's tooltips and the "Parameter reference" in docs/FM1-CONTROLS.md (--docs: between the params markers). They
are never compiled into the cart. Checked with the design: an error for a description of an unknown item, category or
option; a warning for a placeable item, page, category or option without one, or a page key no page has.
"""
import json
import os
import re
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))

CATS = ["TRIG", "SRC", "FILT", "AMP", "FX", "MOD", "MST", "FILE", "PAT", "XFD"]
# 0.6.0: every category is a mode (docs/UX-V2.md §3): its name, the header's short tag, the FM-1 button that opens it
# and its place among that button's modes (HOME: LOOP ENGINE -> TRIG -> KEYBOARD). EC_MB_* = BUTTONS index + 1. A
# design may leave these out (a design before 0.6.0): it gets these
BUTTONS = ["FX", "SEL", "ENV", "LFO", "EDIT", "HOME", "FILE", "ARP", "SEQ"]
MODE_DEFAULT = {"TRIG": ("TRIG", "TRIG", "HOME", 2), "SRC": ("LOOP ENGINE", "LOOP", "HOME", 1),
                "FILT": ("FILTER", "FILTER", "SEL", 1), "AMP": ("AMP", "AMP", "ENV", 1), "FX": ("FX", "FX", "FX", 1),
                "MOD": ("MODULATION", "MOD", "LFO", 1), "MST": ("MIX", "MIX", "EDIT", 1),
                "FILE": ("FILE", "FILE", "FILE", 1), "PAT": ("PATTERN LAUNCH", "PATTERNS", "SEQ", 1),
                "XFD": ("CROSSFADER", "CROSSFADER", "ARP", 1)}
# the fixed screens: modes without params of their own (EC_MODE_KEYBOARD ..), in this order
SCREENS = ["KEYBOARD", "PAGES", "SAMPLER"]
SCREEN_DEFAULT = {"KEYBOARD": ("HOME", 3), "PAGES": ("EDIT", 2), "SAMPLER": ("FILE", 2)}
# the black keys B1..B11 (a page's "key", 0 = none); the source template's modes: pages 1..5 = B1..B5 when the design
# names no key in that category; MOD: MATRIX B1, MACRO n B(n+1), MACROS B11; FX's buses S1 S2 MST: B3 B4 B5
NBKEY = 11
TEMPLATE = ["TRIG", "SRC", "FILT", "AMP", "FX"]
BUS_KEY_DEFAULT = [3, 4, 5]
MODE_MAX, TAG_MAX = 16, 10
BUS_TAG = "FILTER"   # the widest tag a bus page can show under (the bus pages' titles are checked with it)
PICS = ["steps", "wave", "wave-machine", "wave-warp", "wave-range", "filter", "fenv", "aenv", "fx", "sends", "lfo1",
        "lfo2", "menv", "vel", "macros", "macro", "mix", "scene", "file", "razor", "pattern", "matrix"]
# pictures with behaviour of their own: their category, and how many pages there may carry them
FIXED = {"mix": "MST", "scene": "XFD", "file": "FILE", "razor": "FILE", "pattern": "PAT", "matrix": "MOD"}
# the MATRIX page's knobs are fixed (0.5.0): SELECT picks the row, KNOB 1-3 its SRC, DST, DEPTH
MATRIX_LANES = [["mtx.src", "mtx.dst", "mtx.depth", None]]
MTX_RE = re.compile(r"^mtx[1-8]_(src|dst|depth)$")
RETIRED = {"lfo1_dest", "lfo2_dest", "menv_dest", "mvel_dest"}
RETIRED_RE = re.compile(r"^macro[1-4]_(pitch|cutoff|res|amp|pan)_depth$")
MACHINES = ["LOOP", "TRIG", "SLICE", "RAZOR", "S.POLY", "R.POLY", "NEIGH", "INPUT"]
MCLASS = {"loop": [1, 2], "slice": [3, 5], "razor": [4, 6], "neighbor": [7], "input": [8]}
OPTS = {
    "warp": ["TAPE", "VARI", "DOMINO", "GRAIN", "OLA", "PITCH", "HARM", "WTBL", "FREEZE", "FORMNT", "GSTR", "GSTR2",
             "CHOP"],
    "filter": ["CLASSC", "MORPH", "LADDER", "COMB", "FORMNT", "SVF"],
    "fenv": ["ADSR", "AHR"],
    "env": ["ADSR", "AHR"],
    "fx": ["NONE", "DELAY", "REVERB", "LOFI", "COMP", "DESTRY", "ECHO", "B.HOLE", "CHORUS", "FLANGR", "PHASER",
           "DJ EQ", "DUCK", "T.ECHO", "CASSET", "MOTION", "RINGS", "LIMIT"],
}
# the named items that are not track params: kind, C id, default label
SPECIAL = {
    "global.bpm": ("EC_IT_GLOBAL", "EC_G_BPM", "BPM"), "global.swing": ("EC_IT_GLOBAL", "EC_G_SWING", "SWNG"),
    "global.glen": ("EC_IT_GLOBAL", "EC_G_GLEN", "GLEN"), "global.quant": ("EC_IT_GLOBAL", "EC_G_QUANT", "QUANT"),
    "global.rtn": ("EC_IT_GLOBAL", "EC_G_RTN", "RTN"),
    "slot.bpm": ("EC_IT_SLOT", "EC_S_BPM", "BPM"), "slot.steps": ("EC_IT_SLOT", "EC_S_STEPS", "STEP"),
    "slot.gain": ("EC_IT_SLOT", "EC_S_GAIN", "GAIN"), "slot.trim_start": ("EC_IT_SLOT", "EC_S_TRIM_START", "T-ST"),
    "slot.trim_end": ("EC_IT_SLOT", "EC_S_TRIM_END", "T-EN"),
    "razor.start": ("EC_IT_RAZOR", "EC_RZ_START", "STRT"), "razor.end": ("EC_IT_RAZOR", "EC_RZ_END", "END"),
    "razor.play": ("EC_IT_RAZOR", "EC_RZ_PLAY", "PLAY"), "razor.zoom": ("EC_IT_RAZOR", "EC_RZ_ZOOM", "ZOOM"),
    "razor.snap": ("EC_IT_RAZOR", "EC_RZ_SNAP", "SNAP"), "razor.choke": ("EC_IT_RAZOR", "EC_RZ_CHOKE", "CHOK"),
    "razor.pitch": ("EC_IT_RAZOR", "EC_RZ_PITCH", "P/T"), "razor.sens": ("EC_IT_RAZOR", "EC_RZ_SENS", "SENS"),
    "e5.xfd": ("EC_IT_E5", "EC_X_XFD", "XFD"), "e5.choke": ("EC_IT_E5", "EC_X_CHOKE", "CHOK"),
    "e5.gbpm": ("EC_IT_E5", "EC_X_GBPM", "GBPM"), "e5.name": ("EC_IT_E5", "EC_X_NAME", "NAME"),
    "e5.mute": ("EC_IT_E5", "EC_X_MUTE", "SMUT"),
    "mtx.src": ("EC_IT_MTX", "EC_MX_SRC", "SRC"), "mtx.dst": ("EC_IT_MTX", "EC_MX_DST", "DST"),
    "mtx.depth": ("EC_IT_MTX", "EC_MX_DEPTH", "DEPTH"),
}
# the C values of those ids (the enums in src/app/ec_pages.h), for the packed form
SPECIAL_NUM = {"EC_IT_BLANK": 0, "EC_IT_PARAM": 1, "EC_IT_GLOBAL": 2, "EC_IT_SLOT": 3, "EC_IT_RAZOR": 4, "EC_IT_E5": 5,
               "EC_IT_MTX": 6, "EC_MX_SRC": 0, "EC_MX_DST": 1, "EC_MX_DEPTH": 2,
               "EC_X_XFD": 0, "EC_X_CHOKE": 1, "EC_X_GBPM": 2, "EC_X_NAME": 3, "EC_X_MUTE": 4,
               "EC_G_BPM": 0, "EC_G_SWING": 1, "EC_G_GLEN": 2, "EC_G_QUANT": 3, "EC_G_RTN": 4,
               "EC_RZ_START": 0, "EC_RZ_END": 1, "EC_RZ_PLAY": 2, "EC_RZ_ZOOM": 3, "EC_RZ_SNAP": 4, "EC_RZ_CHOKE": 5,
               "EC_RZ_PITCH": 6, "EC_RZ_SENS": 7,
               "EC_S_BPM": 0, "EC_S_STEPS": 1, "EC_S_GAIN": 2, "EC_S_TRIM_START": 3, "EC_S_TRIM_END": 4}
STOPPED = {"mode", "filter_machine", "filter_mix_mode"}     # change only while stopped (EC_IF_STOPPED)
MAX_LANES, SETUP_LANES, MAX_PAGES = 4, 2, 16
CELL_LABEL_WARN, LABEL_MAX = 5, 10
TEXT_W = lambda s: len(s) * 4 - 1 if s else 0               # the 3x5 font (src/ui/ec_gfx.c ec_gfx_text_w), units
TITLE_ROOM = 72      # the header: "TAG/TITLE" from x 17 up to the right-hand info (4 characters, "P01") at x 91
#                      (120-unit grid; 0.6.0: the mode's tag, 0.5.0 the category's id with 64)


class Fail(Exception):
    pass


def slot_name(s):
    return s.get("item") if isinstance(s, dict) else s


def read_params(root):
    """spec name -> (index, C id expression, label), from src/seq/ec_params.h (the enum) and ec_params.c (the table)"""
    h = open(os.path.join(root, "src/seq/ec_params.h")).read()
    body = re.search(r"enum\s*\{(.*?EC_NP\s*=\s*EC_P_\w+)\s*\};", h, re.S).group(1)
    body = re.sub(r"/\*.*?\*/", "", body, flags=re.S)
    index, i = {}, 0
    for tok in [t.strip() for t in body.split(",")]:
        if not tok:
            continue
        m = re.match(r"(EC_\w+)\s*(?:=\s*(.*))?$", tok)
        if not m:
            raise Fail("ec_params.h: cannot read the enum at %r" % tok)
        name, val = m.group(1), m.group(2)
        if name == "EC_NP":
            break
        if val:     # "EC_P_X" or "EC_P_X + n" (EC_P_E4_END = EC_P_MACRO_DEPTH + 20, EC_P_MTX1_SRC = EC_P_E4_END)
            v = re.match(r"(EC_P_\w+)\s*(?:\+\s*(\d+))?$", val.strip())
            if not v or v.group(1) not in index:
                raise Fail("ec_params.h: unexpected value for %s" % name)
            i = index[v.group(1)] + int(v.group(2) or 0)
        index[name] = i
        i += 1
    c = open(os.path.join(root, "src/seq/ec_params.c")).read()
    params = {}
    for m in re.finditer(r"\[(EC_P_\w+)\]\s*=\s*[A-Z]+\(\s*\"(\w+)\"\s*,\s*\"([^\"]*)\"", c):
        sym, nm, lb = m.groups()
        params[nm] = (index[sym], sym, lb)
    for m in re.finditer(r"MD\((\d),\s*(\d),\s*\"(\w+)\",\s*\"([^\"]*)\"\)", c):
        k = int(m.group(1)) * 5 + int(m.group(2))
        params[m.group(3)] = (index["EC_P_MACRO_DEPTH"] + k, "EC_P_MACRO_DEPTH + %d" % k, m.group(4))
    if len(params) != len(set(p[0] for p in params.values())) or len(params) < 200:
        raise Fail("ec_params.c: the parameter table could not be read (%d)" % len(params))
    return params


def is_mtx(name):
    """a mod matrix slot's param (mtx1_src .. mtx8_depth): reached through the MATRIX page, never placed"""
    return bool(MTX_RE.match(name))


def is_retired(name):
    """0.5.0: kept in the enum for old saves, read by nothing (EC_PF_RETIRED): the old mod dests, the macro depths"""
    return name in RETIRED or bool(RETIRED_RE.match(name))


class Gen:
    def __init__(self, design, params):
        self.d = design
        self.params = params
        self.errors, self.warnings = [], []
        self.items, self.vars, self.pages = [], [], []
        self.pkey = []           # each page's black key (0 none)
        self.tags = {}
        self.placed = {}

    def err(self, where, msg):
        self.errors.append("%s: %s" % (where, msg))

    def warn(self, where, msg):
        self.warnings.append("%s: %s" % (where, msg))

    # ------------------------------------------------------------------------------------------ items
    def item(self, where, slot, ctx):
        if slot is None:
            return ("EC_IT_BLANK", "0", 0, None, None)
        if isinstance(slot, str):
            name, label = slot, None
        elif isinstance(slot, dict) and isinstance(slot.get("item"), str):
            name, label = slot["item"], slot.get("label")
            extra = set(slot) - {"item", "label"}
            if extra:
                self.err(where, "unknown field(s) %s" % ", ".join(sorted(extra)))
            if label is not None and (not isinstance(label, str) or not label):
                self.err(where, "%s: the label must be a text" % name)
                label = None
        else:
            self.err(where, "a slot is null, an item name or {\"item\", \"label\"}: %r" % (slot,))
            return ("EC_IT_BLANK", "0", 0, None, None)
        if label is not None:
            if label != label.upper():
                self.err(where, "%s: the label %r has lower case (the font has capitals only)" % (name, label))
            if TEXT_W(label) > TEXT_W("X" * LABEL_MAX):
                self.err(where, "%s: the label %r is longer than %d characters" % (name, label, LABEL_MAX))
            bad = [ch for ch in label if not (32 <= ord(ch) <= 95)]
            if bad:
                self.err(where, "%s: the label %r has characters the font lacks" % (name, label))
        if name in SPECIAL:
            kind, cid, dl = SPECIAL[name]
            if kind == "EC_IT_RAZOR" and ctx.get("pic") != "razor":
                self.err(where, "%s works on the RAZOR page only" % name)
            if kind == "EC_IT_MTX" and ctx.get("pic") != "matrix":
                self.err(where, "%s works on the MATRIX page only" % name)
            lb = label or dl
            if ctx.get("cells") and len(lb) > CELL_LABEL_WARN:
                self.warn(where, "%s: the label %r is longer than %d (the panel's knob label is cut)" %
                          (name, lb, CELL_LABEL_WARN))
            return (kind, cid, 0, lb, name)
        if name not in self.params:
            self.err(where, "unknown item %r" % name)
            return ("EC_IT_BLANK", "0", 0, None, None)
        idx, sym, dl = self.params[name]
        if is_mtx(name):
            self.err(where, "%s is a mod matrix slot's: it is reached through the MATRIX page (MOD), not placed" % name)
        elif is_retired(name):
            self.warn(where, "%s is retired in 0.5.0: reads nothing (the mod matrix replaced it)" % name)
        if ctx.get("cells") and label and len(label) > CELL_LABEL_WARN:
            self.warn(where, "%s: the label %r is longer than %d (the panel's knob label is cut)" %
                      (name, label, CELL_LABEL_WARN))
        self.placed.setdefault(name, []).append(where)
        return ("EC_IT_PARAM", sym, 1 if name in STOPPED else 0, label, name)

    def lanes(self, where, lanes, ctx, max_lanes):
        """the lanes -> items (short lanes padded with blanks, trailing blanks dropped)"""
        if lanes is None:
            lanes = []
        if not isinstance(lanes, list) or any(not isinstance(l, list) for l in lanes):
            self.err(where, "lanes are a list of lists")
            return []
        if len(lanes) > max_lanes:
            self.err(where, "%d lanes (at most %d)" % (len(lanes), max_lanes))
        out, seen = [], {}
        for li, lane in enumerate(lanes):
            if len(lane) > 4:
                self.err(where, "lane %d has %d slots (four knobs)" % (li + 1, len(lane)))
            for k, slot in enumerate(lane[:4]):
                w = "%s lane %d KNOB %d" % (where, li + 1, k + 1)
                it = self.item(w, slot, ctx)
                if it[4]:
                    key = (it[0], it[1])
                    if key in seen:
                        self.err(where, "%s twice (%s and lane %d KNOB %d)" % (it[4], seen[key], li + 1, k + 1))
                    seen[key] = "lane %d KNOB %d" % (li + 1, k + 1)
                out.append(it)
            if li < len(lanes) - 1:
                out += [("EC_IT_BLANK", "0", 0, None, None)] * (4 - len(lane[:4]))
        while out and out[-1][0] == "EC_IT_BLANK":
            out.pop()
        return out

    def when(self, where, w):
        masks = {"mach": 0, "warp": 0, "filt": 0, "fenv": 0, "env": 0, "fx": 0}
        if not isinstance(w, dict) or not w:
            self.err(where, "a variant needs \"when\" (machine / warp / filter / fenv / env / fx)")
            return masks
        for key, vals in w.items():
            if not isinstance(vals, list) or not vals:
                self.err(where, "when.%s is a non-empty list" % key)
                continue
            if key == "machine":
                for v in vals:
                    if v not in MCLASS:
                        self.err(where, "machine %r (one of %s)" % (v, ", ".join(MCLASS)))
                        continue
                    for m in MCLASS[v]:
                        masks["mach"] |= 1 << (m - 1)
            elif key in OPTS:
                for v in vals:
                    if v not in OPTS[key]:
                        self.err(where, "%s %r (one of %s)" % (key, v, ", ".join(OPTS[key])))
                        continue
                    masks[{"filter": "filt"}.get(key, key)] |= 1 << OPTS[key].index(v)
            else:
                self.err(where, "unknown condition %r" % key)
        return masks

    # ------------------------------------------------------------------------------------------ pages
    def find_page(self, ref):
        cat, _, title = ref.partition("/")
        for c in self.d.get("categories", []):
            if c.get("cat") == cat:
                for p in c.get("pages", []):
                    if p.get("title") == title:
                        return p
        return None

    def page(self, where, p, cat, kind="page"):
        """a page -> its index in self.pages"""
        if not isinstance(p, dict):
            self.err(where, "a page is an object")
            p = {}
        extra = set(p) - {"title", "picture", "lanes", "variants", "note", "same_as", "macro", "key"}
        if kind != "page":
            extra |= {"key"} & set(p)
        if extra:
            self.err(where, "unknown field(s) %s" % ", ".join(sorted(extra)))
        bkey = p.get("key", 0)
        if not isinstance(bkey, int) or isinstance(bkey, bool) or not 0 <= bkey <= NBKEY:
            self.err(where, "key %r: a black key 1..%d (0 or none: no key)" % (bkey, NBKEY))
            bkey = 0
        src = p
        if "same_as" in p:
            src = self.find_page(p["same_as"]) if isinstance(p["same_as"], str) else None
            if src is None:
                self.err(where, "same_as %r: no such page (CAT/TITLE)" % p.get("same_as"))
                src = {}
            elif "same_as" in src:
                self.err(where, "same_as %r: that page is itself a copy" % p["same_as"])
            if "lanes" in p or "variants" in p:
                self.err(where, "a page with same_as has no lanes or variants of its own")
        title = p.get("title")
        if not isinstance(title, str) or not title:
            self.err(where, "a page needs a title")
            title = "?"
        pic = p.get("picture", src.get("picture", "steps"))
        if pic not in PICS:
            self.err(where, "picture %r (one of %s)" % (pic, ", ".join(PICS)))
            pic = "steps"
        if kind in ("page", "bus"):
            self.title_fits(where, cat, title)
        home = "MST" if pic == "scene" and getattr(self, "legacy", False) else FIXED.get(pic)
        if pic in FIXED and kind == "page" and home != cat:
            self.err(where, "the %s picture stays in %s (its page has behaviour of its own)" % (pic, home))
        arg = 0
        if pic == "macro":
            m = p.get("macro", src.get("macro"))
            if m not in (1, 2, 3, 4):
                self.err(where, "a macro page needs \"macro\": 1..4")
            else:
                arg = m - 1
        elif "macro" in p:
            self.err(where, "\"macro\" is for the macro picture")
        if "note" in p and not isinstance(p["note"], str):
            self.err(where, "a note is a text")
        if pic == "matrix" and (src.get("variants") or [slot_name(x) for l in (src.get("lanes") or [[]]) if
                                                      isinstance(l, list) for x in l] != MATRIX_LANES[0]):
            self.err(where, "the MATRIX page's knobs are fixed: lanes %s, no variants" % json.dumps(MATRIX_LANES))
        ctx = {"pic": pic, "cells": kind in ("page", "bus", "f3")}
        max_l = {"page": MAX_LANES, "bus": MAX_LANES, "setup": SETUP_LANES, "f3": 1}[kind]
        var0 = len(self.vars)
        variants = src.get("variants", [])
        if not isinstance(variants, list):
            self.err(where, "variants are a list")
            variants = []
        seen_when = []
        for vi, v in enumerate(variants):
            w = "%s variant %d" % (where, vi + 1)
            if not isinstance(v, dict):
                self.err(w, "a variant is an object")
                continue
            extra = set(v) - {"when", "title", "lanes", "note"}
            if extra:
                self.err(w, "unknown field(s) %s" % ", ".join(sorted(extra)))
            masks = self.when(w, v.get("when"))
            key = tuple(sorted(masks.items()))
            if key in seen_when:
                self.warn(w, "the same conditions as an earlier variant: never shown")
            seen_when.append(key)
            vt = v.get("title")
            if vt is not None:
                if not isinstance(vt, str) or not vt:
                    self.err(w, "a variant's title is a text")
                    vt = None
                elif kind in ("page", "bus"):
                    self.title_fits(w, cat, vt)
            items = self.lanes(w, v.get("lanes", []), ctx, max_l)
            self.vars.append((masks, vt, len(self.items), len(items), w))
            self.items += items
        items = self.lanes(where, src.get("lanes", []), ctx, max_l)
        self.vars.append(({"mach": 0, "warp": 0, "filt": 0, "fenv": 0, "env": 0, "fx": 0}, None, len(self.items),
                          len(items), where))
        self.items += items
        self.pages.append((title, PICS.index(pic), arg, var0, len(self.vars) - var0, where))
        self.pkey.append(bkey)
        return len(self.pages) - 1

    def title_fits(self, where, cat, title):
        tag = self.tags.get(cat, cat)      # (the header shows the mode's tag: "LOOP/WARP")
        if title != title.upper():
            self.err(where, "the title %r has lower case (the font has capitals only)" % title)
        if TEXT_W(tag + "/" + title) > TITLE_ROOM:
            self.err(where, "the title %r is too wide for the header (%s/ + %d characters at most)" %
                     (title, tag, (TITLE_ROOM + 1) // 4 - len(tag) - 1))

    def text_ok(self, where, what, t, most):
        if not isinstance(t, str) or not t:
            self.err(where, "%s: a text" % what)
            return False
        if t != t.upper() or any(not (32 <= ord(ch) <= 95) for ch in t):
            self.err(where, "%s %r: capitals, digits and the font's signs only" % (what, t))
        if len(t) > most:
            self.err(where, "%s %r: %d characters at most" % (what, t, most))
        return True

    def modes(self, byname):
        """0.6.0: each category's mode (name, tag, button, order) and the fixed screens; checked: a button's modes
        are numbered 1..n"""
        self.mode = {}
        for n in CATS:
            c = byname.get(n, {})
            dn, dt, db, do = MODE_DEFAULT[n]
            name, tag, btn, order = c.get("mode", dn), c.get("tag", dt), c.get("button", db), c.get("order", do)
            if not self.text_ok(n, "mode", name, MODE_MAX):
                name = dn
            if not self.text_ok(n, "tag", tag, TAG_MAX):
                tag = dt
            if btn not in BUTTONS:
                self.err(n, "button %r (one of %s)" % (btn, " ".join(BUTTONS)))
                btn = db
            if not isinstance(order, int) or isinstance(order, bool) or order < 1:
                self.err(n, "order %r: 1 or more (the place among the button's modes)" % (order,))
                order = 1
            self.mode[n] = (name, tag, btn, order)
            self.tags[n] = tag
        scr = self.d.get("screens")
        self.screens = []
        if scr is None:
            scr = [{"mode": m, "button": SCREEN_DEFAULT[m][0], "order": SCREEN_DEFAULT[m][1]} for m in SCREENS]
        if not isinstance(scr, list) or [x.get("mode") if isinstance(x, dict) else None for x in scr] != SCREENS:
            self.err("screens", "\"screens\" lists %s in that order" % ", ".join(SCREENS))
            scr = [{"mode": m, "button": SCREEN_DEFAULT[m][0], "order": SCREEN_DEFAULT[m][1]} for m in SCREENS]
        for x in scr:
            w = "screens/%s" % x["mode"]
            extra = set(x) - {"mode", "button", "order", "note"}
            if extra:
                self.err(w, "unknown field(s) %s" % ", ".join(sorted(extra)))
            btn, order = x.get("button"), x.get("order")
            if btn not in BUTTONS:
                self.err(w, "button %r (one of %s)" % (btn, " ".join(BUTTONS)))
                btn = SCREEN_DEFAULT[x["mode"]][0]
            if not isinstance(order, int) or isinstance(order, bool) or order < 1:
                self.err(w, "order %r: 1 or more" % (order,))
                order = 1
            self.screens.append((x["mode"], btn, order))
        on = {}
        for n in CATS:
            on.setdefault(self.mode[n][2], []).append((self.mode[n][3], self.mode[n][0]))
        for m, b, o in self.screens:
            on.setdefault(b, []).append((o, m))
        for b, ms in on.items():
            if sorted(o for o, _ in ms) != list(range(1, len(ms) + 1)):
                self.err("modes", "the %s button's modes are numbered %s (1..%d, each once): %s" %
                         (b, " ".join(str(o) for o, _ in sorted(ms)), len(ms),
                          ", ".join("%s %d" % (m, o) for o, m in sorted(ms))))

    def keys(self, n, c, p0, np_):
        """the black keys of a category's pages: as the design names them, else (none named) the default"""
        named = any(isinstance(p, dict) and "key" in p for p in c.get("pages", []))
        if not named:
            for i in range(np_):
                pic = PICS[self.pages[p0 + i][1]]
                k = 0
                if n in TEMPLATE and i < 5:
                    k = i + 1
                elif n == "MOD":
                    k = 1 if pic == "matrix" else 11 if pic == "macros" else self.pages[p0 + i][2] + 2 if \
                        pic == "macro" else 0
                self.pkey[p0 + i] = k
        seen = {}
        if n == "FX":
            for bi, b in enumerate(["S1", "S2", "MST"]):
                if self.bus_key[bi]:
                    seen[self.bus_key[bi]] = "bus " + b
        for i in range(np_):
            k = self.pkey[p0 + i]
            if not k:
                continue
            if k in seen:
                self.err("%s/%s" % (n, self.pages[p0 + i][0]), "B%d opens %s already" % (k, seen[k]))
            seen[k] = self.pages[p0 + i][0]

    # ------------------------------------------------------------------------------------------ the design
    def run(self):
        d = self.d
        if d.get("format") != "elasticat-layout" or d.get("version") != 1:
            raise Fail("not an elasticat-layout version 1 file")
        extra = set(d) - {"format", "version", "name", "note", "categories", "bus", "f3", "screens"}
        if extra:
            self.err("design", "unknown field(s) %s" % ", ".join(sorted(extra)))
        cats = d.get("categories")
        if not isinstance(cats, list):
            raise Fail("\"categories\" is a list")
        byname = {}
        for c in cats:
            n = c.get("cat") if isinstance(c, dict) else None
            if n not in CATS:
                self.err("categories", "unknown category %r (%s)" % (n, " ".join(CATS)))
            elif n in byname:
                self.err("categories", "%s twice" % n)
            else:
                byname[n] = c
        # a design before 0.6.0 has no XFD: CROSSFADER then shows its SCENE page where it is (MST)
        self.legacy = "XFD" not in byname
        for n in CATS:
            if n not in byname and n != "XFD":
                self.err("categories", "%s is missing" % n)
        if self.legacy:
            self.warn("XFD", "no XFD category (a design before 0.6.0): CROSSFADER shows MST's SCENE page")
        if self.errors:
            return
        self.modes(byname)
        bus = d.get("bus")
        self.bus_key = list(BUS_KEY_DEFAULT)
        if isinstance(bus, list):
            for bi, b in enumerate(bus[:3]):
                if isinstance(b, dict) and "key" in b:
                    k = b["key"]
                    if not isinstance(k, int) or isinstance(k, bool) or not 0 <= k <= NBKEY:
                        self.err("bus %s" % b.get("bus", "?"), "key %r: a black key 1..%d in FX (0: none)" % (k, NBKEY))
                        k = 0
                    self.bus_key[bi] = k
        nz = [k for k in self.bus_key if k]
        if len(nz) != len(set(nz)):
            self.err("bus", "two buses on one key (%s)" % " ".join("B%d" % k for k in self.bus_key))
        self.cats = []
        fixed_count = {k: 0 for k in FIXED}
        for n in CATS:
            if n == "XFD" and self.legacy:
                self.cats.append(None)            # (below: the SCENE page's)
                continue
            c = byname[n]
            extra = set(c) - {"cat", "pages", "setup", "mode", "tag", "button", "order"}
            if extra:
                self.err(n, "unknown field(s) %s" % ", ".join(sorted(extra)))
            pages = c.get("pages")
            if not isinstance(pages, list) or not pages:
                self.err(n, "a category needs one page at least")
                pages = []
            if len(pages) > MAX_PAGES:
                self.err(n, "%d pages (at most %d)" % (len(pages), MAX_PAGES))
            titles = set()
            p0 = len(self.pages)
            for pi, p in enumerate(pages):
                t = p.get("title") if isinstance(p, dict) else None
                where = "%s/%s" % (n, t if t else "page %d" % (pi + 1))
                if t in titles:
                    self.err(where, "two pages of %s are called %s" % (n, t))
                titles.add(t)
                self.page(where, p, n)
                pic = PICS[self.pages[-1][1]]
                if pic in fixed_count:
                    fixed_count[pic] += 1
            setup = -1
            if c.get("setup") is not None:
                setup = self.page(n + "/SETUP", {"title": "SETUP", "picture": "steps", "lanes": c["setup"]}, n,
                                  "setup")
            self.cats.append((p0, len(self.pages) - p0 - (1 if setup >= 0 else 0), setup))
            self.keys(n, c, p0, self.cats[-1][1])
        if self.legacy:
            sc = [i for i, pg in enumerate(self.pages) if PICS[pg[1]] == "scene"]
            self.cats[CATS.index("XFD")] = (sc[0] if sc else self.cats[CATS.index("MST")][0], 1, -1)
        for pic, cat in FIXED.items():
            if self.legacy and pic == "scene":
                cat = "MST"
            if pic == "matrix" and fixed_count[pic] == 0:
                self.warn(cat, "no MATRIX page (0.5.0): the mod matrix is reached by MOD KEYS only; add it as MOD's "
                               "first page")
            elif pic != "mix" and fixed_count[pic] != 1:
                self.err(cat, "the %s page must be there once (%d)" % (pic, fixed_count[pic]))
            if pic == "mix" and fixed_count[pic] < 1:
                self.err(cat, "the mix page is missing")
        bus = d.get("bus")
        if not isinstance(bus, list) or [b.get("bus") for b in bus if isinstance(b, dict)] != ["S1", "S2", "MST"]:
            self.err("bus", "\"bus\" lists S1, S2, MST in that order")
            bus = [{}, {}, {}]
        self.bus_page, self.bus_setup, self.mst_src = [], [], -1
        for b in bus:
            name = b.get("bus", "?")
            extra = set(b) - {"bus", "page", "setup", "src", "key"}
            if extra:
                self.err("bus " + name, "unknown field(s) %s" % ", ".join(sorted(extra)))
            self.bus_page.append(self.page("bus %s" % name, b.get("page", {}), BUS_TAG, "bus"))   # (the widest tag)
            self.bus_setup.append(self.page("bus %s/SETUP" % name, {"title": "SETUP", "lanes": b.get("setup", [])},
                                            "FX", "setup") if b.get("setup") is not None else -1)
            if name == "MST":
                if "src" not in b:
                    self.err("bus MST", "\"src\": the master bus's SRC page (the mix) is missing")
                else:
                    self.mst_src = self.page("bus MST/SRC", b["src"], "SRC", "bus")
                    if PICS[self.pages[self.mst_src][1]] != "mix":
                        self.err("bus MST/SRC", "the master bus's SRC page is the mix (picture mix)")
            elif "src" in b:
                self.err("bus " + name, "only the master bus has \"src\"")
        if "f3" not in d:
            self.err("f3", "F3's lane is missing")
        self.f3 = self.page("F3", {"title": "F3", "lanes": d.get("f3", [])}, "PAT", "f3")
        unplaced = sorted(n for n in self.params if n not in self.placed and not is_mtx(n) and not is_retired(n))
        if unplaced:
            self.warn("design", "not on any page: %s" % ", ".join(unplaced))

    # ------------------------------------------------------------------------------------------ output
    def c_source(self, symbol, src_name):
        q = lambda s: "NULL" if s is None else json.dumps(s)
        lo = symbol.lower()
        out = ["/* SPDX-License-Identifier: GPL-3.0-only */",
               "/* GENERATED by tools/layout/gen_layout.py from %s: do not edit (edit the JSON, or use the designer:"
               % src_name,
               " * tools/layout/README.md). The layout \"%s\": %d pages, %d variants, %d items. */" %
               (self.d.get("name", ""), len(self.pages), len(self.vars), len(self.items)),
               "#include \"ec_layout.h\"", "#include \"ec_params.h\"", "",
               "static const ec_item_t %s_item[%d] = {" % (lo, max(1, len(self.items)))]
        for it in self.items:
            kind, cid, fl, lb, _ = it
            out.append("    {%s, %s, %s, %s}," % (kind, cid, "EC_IF_STOPPED" if fl else "0", q(lb)))
        if not self.items:
            out.append("    {EC_IT_BLANK, 0, 0, NULL},")
        out.append("};")
        out.append("static const ec_lay_var_t %s_var[%d] = {" % (lo, len(self.vars)))
        for masks, vt, i0, n, w in self.vars:
            out.append("    {0x%xu, 0x%xu, 0x%x, 0x%x, 0x%x, 0x%x, %s, %d, %d},   /* %s */" %
                       (masks["warp"], masks["fx"], masks["mach"], masks["filt"], masks["fenv"], masks["env"], q(vt),
                        i0, n, w.replace("*/", "* /")))
        out.append("};")
        out.append("static const ec_lay_page_t %s_page[%d] = {" % (lo, len(self.pages)))
        for i, (t, pic, arg, v0, nv, w) in enumerate(self.pages):
            out.append("    {%s, %d, %d, %d, %d, %d},   /* %s%s */" % (q(t), pic, arg, self.pkey[i], v0, nv,
                                                                     w.replace("*/", "* /"),
                                                                     " B%d" % self.pkey[i] if self.pkey[i] else ""))
        out.append("};")
        out.append("const ec_layout_t %s = {" % symbol)
        out.append("    %s," % q(self.d.get("name", "")))
        out.append("    {")
        for n, c in zip(CATS, self.cats):
            name, tag, btn, order = self.mode[n]
            out.append("        {%d, %d, %d, %s, %s, %d, %d},   /* %s */" % (c + (q(name), q(tag), BUTTONS.index(btn) + 1,
                                                                            order, n)))
        out.append("    },")
        out.append("    {%d, %d, %d}, {%d, %d, %d}, %d, %d, {%d, %d, %d}," %
                   (tuple(self.bus_page) + tuple(self.bus_setup) + (self.mst_src, self.f3) + tuple(self.bus_key)))
        out.append("    {" + ", ".join("{%s, %d, %d}" % (q(m), BUTTONS.index(b) + 1, o) for m, b, o in self.screens) +
                   "},")
        out.append("    %s_page, %s_var, %s_item, %d, %d, %d," % (lo, lo, lo, len(self.pages), len(self.vars),
                                                                  len(self.items)))
        out.append("};")
        return "\n".join(out) + "\n"

    def pack(self):
        """the packed text form (the designer's preview loads it: tools/layout/ec_layout_pack.c)"""
        def num(cid):
            if cid in SPECIAL_NUM:
                return SPECIAL_NUM[cid]
            if cid == "0":
                return 0
            for idx, sym, _ in self.params.values():
                if sym == cid:
                    return idx
            raise Fail("no number for %s" % cid)
        s = lambda x: "" if x is None else x
        out = ["ECL2\t%s" % self.d.get("name", "")]
        for n, c in zip(CATS, self.cats):
            name, tag, btn, order = self.mode[n]
            out.append("C\t%d\t%d\t%d\t%d\t%d\t%s\t%s" % (c + (BUTTONS.index(btn) + 1, order, tag, name)))
        for m, b, o in self.screens:
            out.append("S\t%d\t%d\t%s" % (BUTTONS.index(b) + 1, o, m))
        out.append("B\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d" %
                   (tuple(self.bus_page) + tuple(self.bus_setup) + (self.mst_src, self.f3) + tuple(self.bus_key)))
        for i, (t, pic, arg, v0, nv, _) in enumerate(self.pages):
            out.append("P\t%d\t%d\t%d\t%d\t%d\t%s" % (pic, arg, self.pkey[i], v0, nv, t))
        for masks, vt, i0, n, _ in self.vars:
            out.append("V\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%s" % (masks["mach"], masks["warp"], masks["filt"],
                                                                masks["fenv"], masks["env"], masks["fx"], i0, n, s(vt)))
        for kind, cid, fl, lb, _ in self.items:
            out.append("I\t%d\t%d\t%d\t%s" % (SPECIAL_NUM[kind], num(cid), fl, s(lb)))
        return "\n".join(out) + "\n"

    def label_of(self, it):
        kind, cid, fl, lb, name = it
        if kind == "EC_IT_BLANK":
            return "-"
        if lb:
            return lb
        return self.params[name][2]

    def mode_list(self):
        """every mode: (button, order, name, cat or None for a fixed screen), in the panel's button order"""
        ms = [(self.mode[n][2], self.mode[n][3], self.mode[n][0], n) for n in CATS]
        ms += [(b, o, m, None) for m, b, o in self.screens]
        return sorted(ms, key=lambda x: (BUTTONS.index(x[0]), x[1]))

    def docs(self):
        """the markdown tables: the modes by button, then per mode its pages (with their black keys) and lanes
        (variants as their own rows)"""
        fmtw = lambda masks: ", ".join(self.when_text(masks))
        head = ["| Page | Key | Lane | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |", "|---|---|---|---|---|---|---|"]
        nth = lambda o: {1: "1st", 2: "2nd", 3: "3rd"}.get(o, "%dth" % o)
        out = ["<!-- layout:begin (generated by tools/layout/gen_layout.py from design/layout/elasticat.json: edit the "
               "design, not this) -->", "",
               "## Modes, pages and knobs (the layout \"%s\")" % self.d.get("name", ""), "",
               "Each mode button opens its mode; tapping the lit button again goes to the mode's next page (HOME, EDIT "
               "and FILE: to the next mode on that button). In a mode, the black key a page names (Key) opens that "
               "page. A page has one or more lanes of four values on KNOB 1-4. Where a page depends on the track's "
               "machine, warp mode, filter machine, envelope mode or FX machine, each variant has its own rows. "
               "Settings: hold the mode button, or SHIFT + it.", "",
               "| Button | Modes (in order) |", "|---|---|"]
        ms = self.mode_list()
        for b in BUTTONS:
            names = [("%s" % m) + ("" if c else " (a fixed screen)") for bb, o, m, c in ms if bb == b]
            if names:
                out.append("| %s | %s |" % (b, " → ".join(names)))
        out.append("")
        for b, o, m, n in ms:
            if not n:
                continue
            ci = CATS.index(n)
            p0, np, setup = self.cats[ci]
            many = sum(1 for x in ms if x[0] == b) > 1
            out += ["### %s (%s%s · %s)" % (m, b, ", " + nth(o) if many else "", n), ""] + head
            for pi in range(p0, p0 + np):
                out += self.page_rows(pi, fmtw, key=self.pkey[pi])
            if n == "FX":
                for bi, bn in enumerate(["S1", "S2", "MST"]):
                    if self.bus_key[bi]:
                        out.append("| %s bus | B%d | | the bus's page (below) | | | |" % (bn, self.bus_key[bi]))
            if setup >= 0:
                out += self.page_rows(setup, fmtw, "SETUP", key="hold")
            out.append("")
        out += ["### Buses (S1, S2, MST: every mode but MIX, FILE, PATTERN LAUNCH and CROSSFADER shows the bus's page)",
                ""] + head
        for bi, b in enumerate(["S1", "S2", "MST"]):
            if self.bus_setup[bi] >= 0:
                out += self.page_rows(self.bus_setup[bi], fmtw, "%s SETUP" % b, key="hold")
        for bi, b in enumerate(["S1", "S2", "MST"]):
            p = self.bus_page[bi]
            twin = None
            for ci, n in enumerate(CATS):
                for q in range(self.cats[ci][0], self.cats[ci][0] + self.cats[ci][1]):
                    if twin is None and self.same_key(q) == self.same_key(p):
                        twin = "%s/%s" % (self.mode[n][1], self.pages[q][0])
            k = "B%d" % self.bus_key[bi] if self.bus_key[bi] else ""
            if twin:
                out.append("| %s: %s | %s | | the lanes of %s, for the bus's own FX machine | | | |" %
                           (b, self.pages[p][0], k, twin))
            else:
                out += self.page_rows(p, fmtw, "%s: %s" % (b, self.pages[p][0]), key=k)
        if self.mst_src >= 0:
            out += self.page_rows(self.mst_src, fmtw, "MST: LOOP ENGINE = %s" % self.pages[self.mst_src][0])
        out += ["", "### F3", ""] + head
        out += self.page_rows(self.f3, fmtw, "F3")
        out += ["", "<!-- layout:end -->"]
        return "\n".join(out)

    def same_key(self, p):
        t, pic, arg, v0, nv, _ = self.pages[p]
        return tuple((tuple(sorted(self.vars[v][0].items())), self.vars[v][1],
                      tuple(self.items[self.vars[v][2]:self.vars[v][2] + self.vars[v][3]])) for v in range(v0, v0 + nv))

    def when_text(self, masks):
        out = []
        if masks["mach"]:
            cls = [c for c, ms in MCLASS.items() if all(masks["mach"] >> (m - 1) & 1 for m in ms)]
            out.append("/".join(c.upper() for c in cls))
        for key, mk in (("warp", "warp"), ("filter", "filt"), ("fenv", "fenv"), ("env", "env"), ("fx", "fx")):
            if masks[mk]:
                names = [o for i, o in enumerate(OPTS[key]) if masks[mk] >> i & 1]
                out.append(("F.ENV " if key == "fenv" else "ENV " if key == "env" else "") + "/".join(names))
        return out

    def covered(self, v0, nv):
        """the variants before the last one cover every value of one condition: the last (the fallback) is never
        shown"""
        keys = {"warp": 13, "fx": 18, "mach": 8, "filt": 6, "fenv": 2, "env": 2}
        for k, n in keys.items():
            union = 0
            for v in range(v0, v0 + nv - 1):
                m = self.vars[v][0]
                if any(m[o] for o in keys if o != k):
                    continue
                union |= m[k]
            if union == (1 << n) - 1:
                return True
        return False

    def page_items(self, p):
        """the item names on page p, every variant, in order"""
        _, _, _, v0, nv, _ = self.pages[p]
        out = []
        for v in range(v0, v0 + nv):
            _, _, i0, n, _ = self.vars[v]
            out += [it[4] for it in self.items[i0:i0 + n] if it[4]]
        return out

    def page_rows(self, p, fmtw, name=None, key=None):
        t, pic, arg, v0, nv, _ = self.pages[p]
        rows = []
        kt = ("B%d" % key if key else "") if isinstance(key, int) or key is None else key
        if nv > 1 and self.covered(v0, nv):
            nv -= 1
        for v in range(v0, v0 + nv):
            masks, vt, i0, n, _ = self.vars[v]
            items = self.items[i0:i0 + n]
            label = name or t
            if vt:
                label = "%s (as %s)" % (label, vt) if name is None else label
            cond = fmtw(masks)
            if nv > 1:
                label += " · " + (cond if cond else "otherwise")
            first = v == v0
            if not items:
                rows.append("| %s | %s | - | | | | |" % (label.replace("|", "\\|"), kt if first else ""))
                continue
            for li in range(0, len(items), 4):
                lane = [self.label_of(x) for x in items[li:li + 4]] + [""] * 4
                rows.append("| %s | %s | %d | %s |" % (label.replace("|", "\\|") if li == 0 else "",
                                                         kt if first and li == 0 else "", li // 4 + 1,
                                                         " | ".join(x.replace("|", "\\|") for x in lane[:4])))
        return rows


# ---------------------------------------------------------------------------------------------- canonical JSON
# ------------------------------------------------------------------------------------------------ descriptions
DESC_DEFAULT = os.path.join(ROOT, "design/layout/descriptions.json")
DESC_OPTS = {"machine": MACHINES, **OPTS}


def page_keys(d):
    """the description keys of a design's pages: CAT/TITLE, CAT/SETUP, bus:S1 (bus:S1/SETUP), bus:MST/SRC, F3"""
    keys = []
    for c in d.get("categories", []):
        for p in c.get("pages", []):
            keys.append("%s/%s" % (c.get("cat"), p.get("title")))
        if c.get("setup") is not None:
            keys.append("%s/SETUP" % c.get("cat"))
    for b in d.get("bus", []):
        keys.append("bus:%s" % b.get("bus"))
        if b.get("setup") is not None:
            keys.append("bus:%s/SETUP" % b.get("bus"))
        if b.get("src") is not None:
            keys.append("bus:%s/SRC" % b.get("bus"))
    keys.append("F3")
    return keys


def check_desc(g, desc, design):
    """the descriptions against the parameter table and the design (errors and warnings into g)"""
    where = "descriptions"
    if not isinstance(desc, dict) or desc.get("format") != "elasticat-descriptions" or desc.get("version") != 1:
        g.err(where, "not an elasticat-descriptions version 1 file")
        return
    extra = set(desc) - {"format", "version", "cats", "pages", "options", "items"}
    if extra:
        g.err(where, "unknown field(s) %s" % ", ".join(sorted(extra)))
    items = desc.get("items", {})
    known = list(g.params) + list(SPECIAL)
    for k, v in items.items():
        if k not in g.params and k not in SPECIAL:
            g.err(where, "a description of an unknown item '%s'" % k)
        elif not isinstance(v, dict) or not str(v.get("name", "")).strip() or not str(v.get("desc", "")).strip():
            g.err(where, "item '%s': needs a name and a desc" % k)
    missing = [k for k in known if k not in items and not is_mtx(k) and not is_retired(k)]
    if missing:
        g.warn(where, "no description for item(s) %s" % ", ".join(missing))
    cats = desc.get("cats", {})
    for k in cats:
        if k not in CATS:
            g.err(where, "a description of an unknown category '%s'" % k)
    if [c for c in CATS if c not in cats]:
        g.warn(where, "no description for category(ies) %s" % ", ".join(c for c in CATS if c not in cats))
    opts = desc.get("options", {})
    for dim, vals in opts.items():
        if dim not in DESC_OPTS:
            g.err(where, "a description of an unknown option kind '%s'" % dim)
            continue
        for v in vals:
            if v not in DESC_OPTS[dim]:
                g.err(where, "a description of an unknown %s '%s'" % (dim, v))
    for dim, vals in DESC_OPTS.items():
        miss = [v for v in vals if v not in opts.get(dim, {})]
        if miss:
            g.warn(where, "no description for %s %s" % (dim, ", ".join(miss)))
    pages = desc.get("pages", {})
    keys = page_keys(design)
    miss = [k for k in keys if k not in pages]
    if miss:
        g.warn(where, "no description for page(s) %s" % ", ".join(miss))
    stale = [k for k in pages if k not in keys]
    if stale:
        g.warn(where, "description(s) for page(s) this design does not have: %s" % ", ".join(stale))


def desc_docs(g, desc):
    """the docs' "Parameter reference": per category (where the shipped layout puts an item first) a table"""
    items = desc.get("items", {})
    home = {}
    for ci, n in enumerate(CATS):
        p0, np_, setup = g.cats[ci]
        rng = list(range(p0, p0 + np_)) + ([setup] if setup >= 0 else [])
        for pi in rng:
            for it in g.page_items(pi):
                home.setdefault(it, n)
    rows = {n: [] for n in CATS + ["OTHER"]}
    for k in list(g.params) + list(SPECIAL):
        if k in items and not is_mtx(k) and not is_retired(k):
            rows[home.get(k, "OTHER")].append(k)
    esc = lambda t: str(t).replace("|", "\\|")
    out = ["<!-- params:begin (generated by tools/layout/gen_layout.py from design/layout/descriptions.json: edit that, "
           "not this) -->", "", "## Parameter reference", "",
           "What every value does, grouped by the mode whose pages show it first (the designer shows the same "
           "text as tooltips). Label: the screen's own short name (a page may give it another).", ""]
    for n in CATS + ["OTHER"]:
        if not rows[n]:
            continue
        out += ["### %s" % (n if n == "OTHER" else "%s (%s)" % (g.mode[n][0], n)), ""]
        if n != "OTHER" and desc.get("cats", {}).get(n):
            out += [desc["cats"][n], ""]
        out += ["| Item | Label | Name | What it does |", "|---|---|---|---|"]
        for k in rows[n]:
            lb = SPECIAL[k][2] if k in SPECIAL else g.params[k][2]
            out.append("| `%s` | %s | %s | %s |" % (k, esc(lb), esc(items[k]["name"]), esc(items[k]["desc"])))
        out.append("")
    opts = desc.get("options", {})
    names = {"machine": "Machines", "warp": "Warp modes", "filter": "Filter machines", "fenv": "Filter envelope modes",
             "env": "Amp envelope modes", "fx": "FX machines"}
    out += ["### Machines and modes (what a page variant is for)", ""]
    for dim, vals in DESC_OPTS.items():
        if not opts.get(dim):
            continue
        out += ["**%s**" % names[dim], ""]
        out += ["- **%s**: %s" % (v, opts[dim][v]) for v in vals if v in opts[dim]]
        out.append("")
    pages = desc.get("pages", {})
    out += ["### Pages", "", "| Page | What it is for |", "|---|---|"]
    out += ["| %s | %s |" % (esc(k), esc(pages[k])) for k in page_keys(g.d) if k in pages]
    out += ["", "<!-- params:end -->"]
    return "\n".join(out)


def canonical(d):
    """the design's canonical text: two-space indent, a lane / a when / an item on one line (the designer writes the
    same: tools/layout/designer.js canonical())"""
    def inline(x):
        return json.dumps(x, ensure_ascii=False, separators=(", ", ": "))

    def emit(x, ind, key=None):
        pad = "  " * ind
        if key in ("lanes", "setup", "f3") and isinstance(x, list):
            if not x:
                return "[]"
            return "[\n" + ",\n".join(pad + "  " + inline(l) for l in x) + "\n" + pad + "]"
        if key == "when" or (isinstance(x, dict) and "item" in x):
            return inline(x)
        if isinstance(x, dict):
            if not x:
                return "{}"
            return "{\n" + ",\n".join("%s  %s: %s" % (pad, json.dumps(k, ensure_ascii=False), emit(v, ind + 1, k))
                                       for k, v in x.items()) + "\n" + pad + "}"
        if isinstance(x, list):
            if not x:
                return "[]"
            return "[\n" + ",\n".join(pad + "  " + emit(v, ind + 1) for v in x) + "\n" + pad + "]"
        return inline(x)
    return emit(d, 0) + "\n"


def write_if_changed(path, text):
    old = open(path).read() if os.path.exists(path) else None
    if old != text:
        with open(path, "w") as f:
            f.write(text)
        return True
    return False


def main(argv):
    args = argv[1:]
    opt = {}
    pos = []
    i = 0
    while i < len(args):
        a = args[i]
        if a in ("--c", "--symbol", "--pack", "--docs", "--root", "--desc"):
            opt[a] = args[i + 1]
            i += 2
        elif a in ("--format", "--check", "--quiet", "--no-desc"):
            opt[a] = True
            i += 1
        elif a.startswith("--"):
            print("gen_layout: unknown option %s" % a, file=sys.stderr)
            return 2
        else:
            pos.append(a)
            i += 1
    path = pos[0] if pos else os.path.join(ROOT, "design/layout/elasticat.json")
    root = opt.get("--root", ROOT)
    try:
        design = json.load(open(path))
    except (OSError, ValueError) as e:
        print("gen_layout: %s: %s" % (path, e), file=sys.stderr)
        return 1
    try:
        g = Gen(design, read_params(root))
        g.run()
        desc = None
        dpath = opt.get("--desc", DESC_DEFAULT)
        if not opt.get("--no-desc") and ("--desc" in opt or os.path.exists(dpath)):
            try:
                desc = json.load(open(dpath))
            except (OSError, ValueError) as e:
                raise Fail("%s: %s" % (dpath, e))
            if not g.errors:
                check_desc(g, desc, design)
    except Fail as e:
        print("gen_layout: %s: %s" % (path, e), file=sys.stderr)
        return 1
    if not opt.get("--quiet"):
        for w in g.warnings:
            print("gen_layout: warning: %s" % w, file=sys.stderr)
    if g.errors:
        for e in g.errors:
            print("gen_layout: error: %s" % e, file=sys.stderr)
        print("gen_layout: %s: %d error(s), nothing written" % (path, len(g.errors)), file=sys.stderr)
        return 1
    if opt.get("--check"):
        print("gen_layout: %s: ok (%d pages, %d variants, %d items, %d warning(s))" %
              (path, len(g.pages), len(g.vars), len(g.items), len(g.warnings)))
        return 0
    if opt.get("--format"):
        write_if_changed(path, canonical(design))
    if "--c" in opt:
        rel = os.path.relpath(os.path.abspath(path), root)
        if write_if_changed(opt["--c"], g.c_source(opt.get("--symbol", "EC_LAYOUT_SHIPPED"), rel)) and \
                not opt.get("--quiet"):
            print("gen_layout: wrote %s" % opt["--c"])
    if "--pack" in opt:
        write_if_changed(opt["--pack"], g.pack())
    if "--docs" in opt:
        doc = open(opt["--docs"]).read()
        m = re.search(r"<!-- layout:begin.*?<!-- layout:end -->", doc, re.S)
        if not m:
            print("gen_layout: %s has no layout markers" % opt["--docs"], file=sys.stderr)
            return 1
        doc = doc[:m.start()] + g.docs() + doc[m.end():]
        if desc is not None:
            m = re.search(r"<!-- params:begin.*?<!-- params:end -->", doc, re.S)
            if not m:
                print("gen_layout: %s has no params markers" % opt["--docs"], file=sys.stderr)
                return 1
            doc = doc[:m.start()] + desc_docs(g, desc) + doc[m.end():]
        write_if_changed(opt["--docs"], doc)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
