/* intellisense.js: hover, completion and ctrl+click, fed by the same oiSR reflection the Symbols tab
 * shows, plus a macro index scraped from the sources themselves (a #define is preprocessed away long
 * before the AST, so texture2DUniform and friends exist nowhere else).
 *
 * Hover speaks HLSL: `F32x4 color : TEXCOORD0`, `void shade(Light l)`, `struct Light { ... }`, the way
 * the declaration would be written, with the kind and its home as the small print. The cores are pure
 * (doc + text in, data out) so they test without a DOM; the CodeMirror glue at the bottom degrades to
 * nothing when the hint addon or a real editor aren't there. */
(function () {
"use strict";
const U = window.OxUtil;

const lex = () => (window.OxEditor && window.OxEditor.LEX) || {};
const words = s => (s || "").split(" ").filter(Boolean);

/* ---- lookup ------------------------------------------------------------------------------- */

/* Every node in the doc carrying this name, user code before builtin includes, definitions first. */
/* Several declarations can share a name (an interface's area() and every implementation of it), so
 * resolution follows the language: from the scope the cursor is in, outward. The innermost node whose
 * loc spans the line is the starting scope (a method body, its struct, a namespace), and each level of
 * the parent chain is searched before the one above it, so `area` inside Circle means Circle's.
 * A cursor on a declaration's own line resolves to that declaration. */
function resolveInScope(doc, name, line) {

  /* Line numbers only mean something within one file: the cursor is in the reflected main file, so an
   * include's node that happens to span the same line number must not count as a covering scope. */
  const home = (doc.sourceName || doc.name || "").replace(/^\.\//, "");
  const inHome = n => !home || (n.loc.file || "").replace(/^\.\//, "") === home;

  const covering = doc.nodes
    .filter(n =>
      !n.builtin && n.loc && n.loc.line && inHome(n) &&
      line >= n.loc.line && line < n.loc.line + (n.loc.lines || 1))
    .sort((a, b) => (a.loc.lines || 1) - (b.loc.lines || 1));

  let scope = covering[0] || null;

  while (scope) {
    if (scope.name === name) return scope;
    const hit = (scope.children || []).map(c => doc.nodes[c]).find(c => c && c.name === name);
    if (hit) return hit;
    scope = scope.parent >= 0 ? doc.nodes[scope.parent] : null;
  }
  return null;
}

function findByName(doc, name, at) {
  if (!doc || !doc.nodes) return [];

  const rank = { Struct: 0, Interface: 0, Enum: 0, Function: 1, Register: 2, Typedef: 2 };
  const untypedFn = n => n.kind !== "Function" ? 0 :
    (n.children || []).some(c => doc.nodes[c] && doc.nodes[c].name === "(return)" && doc.nodes[c].type) ? 0 : 1;

  const matches = doc.nodes
    .filter(n => n.name === name)
    .sort((a, b) =>
      (!!a.builtin - !!b.builtin) ||
      ((rank[a.kind] ?? 3) - (rank[b.kind] ?? 3)) || (untypedFn(a) - untypedFn(b)));

  const scoped = at && at.line ? resolveInScope(doc, name, at.line) : null;
  return scoped ? [scoped, ...matches.filter(n => n !== scoped)] : matches;
}

const typeText = n => n.type ? (n.type.display || n.type.name || "") : "";
const arrText = n => n.arrays && n.arrays.length ? "[" + n.arrays.join("][") + "]" : "";

/* An enumerator's value, spelled `= 2` after its name the way the enum body writes it; a value past 2^53
 * arrives as text and prints as it is. The enum's underlying integer type rides on each enumerator, named
 * the oiSR way (U32, I64), so the hover translates it to the HLSL spelling. */
/* The module spells the underlying type the way the CLI prints it ("uint"); hover speaks the page's
 * OxC3 notation like every other type it shows, so the spelling maps over. */
const ENUM_TYPE_OXC = { uint: "U32", int: "I32", uint64_t: "U64", int64_t: "I64", uint16_t: "U16", int16_t: "I16" };
const enumValueText = n => n.enumValue && n.enumValue.value != null ? ` = ${n.enumValue.value}` : "";
const enumTypeText = n => n.enumValue && n.enumValue.type ? (ENUM_TYPE_OXC[n.enumValue.type] || n.enumValue.type) : "";

/* ---- macros ------------------------------------------------------------------------------- */

/* #define table built from the project and the builtin include sources. Memoized on a cheap
 * fingerprint (names + total length) because hover asks often and the sources change rarely. */
let macroCache = { key: "", table: null };

function macroIndex(files, builtins) {

  let key = "";
  for (const [name, f] of Object.entries(files || {})) key += name + ":" + ((f.src || "").length) + ";";
  for (const [name, src] of Object.entries(builtins || {})) key += name + ":" + ((src || "").length) + ";";

  if (macroCache.key === key && macroCache.table) return macroCache.table;

  const table = {};
  const scan = (src, file) => {
    const rx = /^[ \t]*#define[ \t]+([A-Za-z_]\w*)(\([^)]*\))?[ \t]*(.*)$/gm;
    let m;
    while ((m = rx.exec(src)))
      if (!table[m[1]])
        table[m[1]] = {
          args: m[2] || "",
          body: m[3].trim(),
          file,
          line: (src.slice(0, m.index).match(/\n/g) || []).length + 1
        };
  };

  for (const [name, f] of Object.entries(files || {})) scan(f.src || "", name);
  for (const [name, src] of Object.entries(builtins || {})) scan(src || "", name);

  macroCache = { key, table };
  return table;
}

/* Type aliases scraped from the builtin headers' own typedefs, so completion offers exactly what the
 * compiler declares (the hand list in LEX stays as the fallback when no builtins are loaded). */
let aliasCache = { key: "", list: null };

function aliasIndex(builtins) {

  let key = "";
  for (const [name, src] of Object.entries(builtins || {})) key += name + ":" + ((src || "").length) + ";";

  if (aliasCache.key === key && aliasCache.list) return aliasCache.list;

  const seen = new Set();
  const rx = /^\s*typedef\s+\S+\s+([A-Za-z_]\w*);/gm;

  for (const src of Object.values(builtins || {})) {
    let m;
    while ((m = rx.exec(src || ""))) seen.add(m[1]);
  }

  aliasCache = { key, list: [...seen] };
  return aliasCache.list;
}

/* ---- hover -------------------------------------------------------------------------------- */

/* The declaration spelled the way HLSL would write it. */
function signature(doc, n) {

  const sem = n.semantic ? ` : ${n.semantic}` : "";

  if (n.kind === "Function") {
    const kids = (n.children || []).map(c => doc.nodes[c]).filter(c => c && c.kind === "Parameter");
    const ret = kids.find(c => c.name === "(return)");
    const params = kids.filter(c => c !== ret && c.name && c.name !== "(anonymous)");
    const dir = p => p.direction === "inout" ? "inout " : p.direction === "out" ? "out " : "";
    return `${ret ? typeText(ret) : "void"} ${n.name}(` +
      params.map(p => `${dir(p)}${typeText(p)} ${p.name}${p.semantic ? " : " + p.semantic : ""}`.trim()).join(", ") +
      `)${sem}${n.entry ? `  [shader("${n.entry.stage}")]` : ""}`;
  }

  if (n.kind === "Struct" || n.kind === "Interface" || n.kind === "Enum") {

    const kw = n.kind === "Struct" ? "struct" : n.kind === "Interface" ? "interface" : "enum";

    /* The hierarchy in HLSL's own spelling: `struct Circle : Shape, IArea`. On a struct the colon IS
     * inheritance syntax, so unlike a variable there is nothing for it to collide with. */
    const name = id => doc.nodes[id] && doc.nodes[id].name;
    const bases = [
      ...(n.type && n.type.base >= 0 && name(n.type.base) ? [name(n.type.base)] : []),
      ...((n.implements || []).map(name).filter(Boolean))
    ];

    const members = (n.children || []).map(c => doc.nodes[c])
      .filter(c => c && c.name && !c.name.startsWith("$") && (c.kind === "Variable" || c.kind === "EnumValue"));

    /* An enum body is its enumerators with their values, comma separated as HLSL writes them, and the
     * underlying integer type sits where a struct's base does: `enum Mode : uint { Off = 0, Linear = 1 }`. */
    const isEnum = n.kind === "Enum";
    const underlying = isEnum && members.length ? enumTypeText(members[0]) : "";
    const shown = members.slice(0, 6).map(c => isEnum ? `${c.name}${enumValueText(c)}` : `${typeText(c)} ${c.name}${arrText(c)}`.trim());
    if (members.length > 6) shown.push("...");
    const heritage = isEnum ? (underlying ? " : " + underlying : "") : (bases.length ? " : " + bases.join(", ") : "");
    return `${kw} ${n.name}${heritage}${shown.length ? ` { ${shown.join(isEnum ? ", " : "; ")} }` : ""}`;
  }

  /* An enumerator reads as its line in the enum body: `Linear = 1`. */
  if (n.kind === "EnumValue")
    return `${n.name}${enumValueText(n)}`;

  /* A register array's size lives in its bind count when it is one-dimensional. */
  if (n.kind === "Register") {
    const arr = arrText(n) || (n.register && n.register.count > 1 ? `[${n.register.count}]` : "");
    return `${n.register ? n.register.info + " " : ""}${n.name}${arr}`;
  }

  if (n.kind === "Typedef")
    return `typedef ${typeText(n)} ${n.name}`;

  const ty = typeText(n);
  return `${ty ? ty + " " : ""}${n.name}${arrText(n)}${sem}`;
}

/* What the hover card says for one name; ctx = {files, builtins} feeds the macro fallback. */
/* The contiguous `//` block sitting right above a declaration is its documentation, the way VS Code
 * reads it by default. oiSR carries no comment spans, so the lines come from the source itself. */
function docComment(ctx, loc) {
  if (!ctx || !loc || !loc.file || !loc.line) return null;
  const file = loc.file.replace(/^\.\//, "");
  const src = (ctx.files && ctx.files[file] && ctx.files[file].src) ||
    (ctx.builtins && (ctx.builtins[file] || ctx.builtins["@" + file]));
  if (typeof src !== "string") return null;
  const lines = src.split("\n");
  const out = [];
  for (let i = loc.line - 2; i >= 0; --i) {
    const m = /^\s*\/\/\/?\s?(.*)$/.exec(lines[i]);
    if (!m) break;
    out.unshift(m[1]);
  }
  return out.length ? out.join("\n").trim() : null;
}

function hoverInfo(doc, name, ctx, at) {

  const matches = findByName(doc, name, at);

  if (matches.length) {
    const n = matches[0];
    /* The alias the source wrote is the title; what it resolves to rides in the small print, since a
     * colon after a variable already means its semantic. */
    const aka = n.type && n.type.name && n.type.display && n.type.name !== n.type.display
      ? ` · aka ${n.type.name}` : "";

    /* An enumerator's small print names the enum it belongs to and that enum's underlying type. */
    const owner = n.kind === "EnumValue" && n.parent >= 0 && doc.nodes[n.parent] && doc.nodes[n.parent].kind === "Enum"
      ? doc.nodes[n.parent] : null;
    const home = owner ? ` · ${owner.name}${enumTypeText(n) ? " : " + enumTypeText(n) : ""}` : "";

    return {
      title: signature(doc, n),
      sub: n.kind + aka + home + (n.builtin && n.loc ? ` · from ${n.loc.file.replace(/^\.\//, "")}` : ""),
      doc: docComment(ctx, n.loc),
      loc: n.loc ? `${n.loc.file}:${n.loc.line}` : null,
      more: matches.length - 1,
      node: n
    };
  }

  const mac = ctx && macroIndex(ctx.files, ctx.builtins)[name];

  if (mac)
    return {
      title: `#define ${name}${mac.args} ${mac.body}`.slice(0, 120),
      sub: `macro · from ${mac.file}`,
      loc: `${mac.file}:${mac.line}`,
      more: 0,
      node: null
    };

  return null;
}

/* An #include line's target, when the position sits inside the quotes: a path is not a word, so the
 * word-based lookup below can never resolve one. */
function includeTarget(lineText, ch) {
  const m = /^\s*#include\s+"([^"]*)"/.exec(lineText);
  if (!m) return null;
  const a = lineText.indexOf('"') + 1, b = a + m[1].length;
  return ch >= a - 1 && ch <= b + 1 ? { file: m[1], line: 1, ch0: 0, ch1: 1 } : null;
}

/* Where ctrl+click goes: the declaration, whether it is a symbol or a #define. */
function definitionOf(doc, name, ctx, at) {
  const n = findByName(doc, name, at)[0];
  if (n && n.loc)
    return { file: n.loc.file, line: n.loc.line, ch0: Math.max(0, (n.loc.col || 1) - 1),
      ch1: (n.loc.col || 1) - 1 + (n.loc.len || 1) };
  const mac = ctx && macroIndex(ctx.files, ctx.builtins)[name];
  if (mac) return { file: mac.file, line: mac.line, ch0: 0, ch1: 1 };
  return null;
}

/* ---- completion --------------------------------------------------------------------------- */

const KIND_HINT = { Struct: "struct", Interface: "interface", Function: "fn", Register: "resource",
  Variable: "var", StaticVariable: "var", GroupsharedVariable: "var", Typedef: "type", Enum: "enum",
  EnumValue: "enum", Parameter: "param", Namespace: "namespace" };

/* The completion list for a cursor sitting at `ch` in `lineText`. Returns null when there is nothing
 * sensible to offer; {from, list:[{text, hint}]} otherwise, `from` being where the replacement starts. */
function completions(doc, files, lineText, ch, builtins) {

  const before = lineText.slice(0, ch);
  let m;

  /* #include "..." offers the built-ins and the project's own files. */
  if ((m = /#include\s+"([^"]*)$/.exec(before))) {
    const partial = m[1];
    const names = [
      ...Object.keys(builtins || (window.OxMock && window.OxMock.BUILTINS) || {}),
      ...Object.keys(files || {})
    ];
    const list = names.filter(n => n.startsWith(partial)).map(text => ({ text, hint: "include" }));
    return list.length ? { from: ch - partial.length, list } : null;
  }

  /* [[oxc:: offers the annotations. */
  if ((m = /\[\[\s*oxc::(\w*)$/.exec(before))) {
    const list = words(lex().annotations).filter(a => a.startsWith(m[1]))
      .map(text => ({ text: text + "(", hint: "annotation" }));
    return list.length ? { from: ch - m[1].length, list } : null;
  }

  /* obj.partial offers the members of obj's struct, read off the type graph. */
  if ((m = /([A-Za-z_]\w*)\.(\w*)$/.exec(before))) {

    const owner = findByName(doc, m[1])[0];
    const def = owner && owner.type && owner.type.def >= 0 && doc.nodes[owner.type.def];

    if (def && def.children) {
      const partial = m[2];
      const list = def.children.map(c => doc.nodes[c])
        .filter(c => c && c.name && !c.name.startsWith("$") && c.name.startsWith(partial) &&
          (c.kind === "Variable" || c.kind === "Function"))
        .map(c => ({ text: c.name, hint: `${KIND_HINT[c.kind] || ""} ${typeText(c)}`.trim() }));
      return list.length ? { from: ch - partial.length, list } : null;
    }
    return null;
  }

  /* A plain identifier offers everything declared plus what HLSL spells. */
  if ((m = /([A-Za-z_]\w*)$/.exec(before))) {

    const partial = m[1];
    if (!partial.length) return null;
    const seen = new Set();
    const list = [];
    const add = (text, hint) => { if (!seen.has(text) && text.startsWith(partial)) { seen.add(text); list.push({ text, hint }); } };

    if (doc && doc.nodes)
      for (const n of doc.nodes)
        if (n.name && !n.builtin && !n.name.startsWith("$") && n.name !== "(anonymous)")
          add(n.name, `${KIND_HINT[n.kind] || n.kind} ${typeText(n)}`.trim());

    for (const [name, mac] of Object.entries(macroIndex(files, builtins)))
      add(name, "macro" + mac.args);

    const derived = aliasIndex(builtins);
    for (const w of derived) add(w, "oxc type");

    const L = lex();
    if (!derived.length)
      for (const w of words(L.oxcTypes)) add(w, "oxc type");
    for (const w of words(L.types)) add(w, "type");
    for (const w of words(L.intrinsics)) add(w, "intrinsic");
    for (const w of words(L.keywords)) add(w, "");
    for (const w of words(L.semantics)) add(w, "semantic");

    return list.length ? { from: ch - partial.length, list } : null;
  }

  return null;
}

/* ---- CodeMirror glue ---------------------------------------------------------------------- */

let tip = null;

function showTip(html, x, y) {
  if (!tip) {
    tip = document.createElement("div");
    tip.className = "ox-hover";
    document.body.appendChild(tip);
  }
  tip.innerHTML = html;
  tip.style.display = "block";
  const w = tip.offsetWidth, vw = window.innerWidth;
  tip.style.left = Math.min(x + 12, vw - w - 8) + "px";
  tip.style.top = (y + 16) + "px";
}

const hideTip = () => { if (tip) tip.style.display = "none"; };

function wordAt(cm, e) {
  const pos = cm.coordsChar({ left: e.clientX, top: e.clientY }, "window");
  const line = cm.getLine(pos.line) || "";
  let a = pos.ch, b = pos.ch;
  while (a > 0 && /\w/.test(line[a - 1])) a--;
  while (b < line.length && /\w/.test(line[b])) b++;
  const w = line.slice(a, b);
  return !w || /^\d/.test(w) ? null : w;
}

function attach(editor, getDoc, getFiles, getBuiltins, onGoto) {

  const cm = editor.cmHandle && editor.cmHandle();
  if (!cm || typeof cm.getWrapperElement !== "function") return;

  const ctx = () => ({ files: getFiles(), builtins: getBuiltins ? getBuiltins() : null });
  const wrap = cm.getWrapperElement();

  /* hover: quick, and quiet while the word under the mouse hasn't changed. The class toggle and the
   * timer only fire when something actually changed: both ran per mousemove event before, and a style
   * recalc per mouse pixel is exactly what "the editor feels sluggish" is made of. */
  let hoverTimer = null, lastWord = null, ctrlShown = false;

  wrap.addEventListener("mousemove", e => {
    const ctrlNow = e.ctrlKey || e.metaKey;
    if (ctrlNow !== ctrlShown) { ctrlShown = ctrlNow; wrap.classList.toggle("ox-ctrl", ctrlNow); }
    clearTimeout(hoverTimer);
    hoverTimer = setTimeout(() => {
      const word = wordAt(cm, e);
      if (word === lastWord && tip && tip.style.display === "block") return;
      lastWord = word;
      const pos = cm.coordsChar ? cm.coordsChar({ left: e.clientX, top: e.clientY }, "window") : null;
      const info = word && hoverInfo(getDoc(), word, ctx(), pos ? { line: pos.line + 1 } : null);
      if (!info) return hideTip();
      showTip(
        `<div class="ox-hover-title">${U.esc(info.title)}</div>` +
        (info.doc ? `<div class="ox-hover-doc">${U.esc(info.doc)}</div>` : "") +
        `<div class="ox-hover-loc">${U.esc(info.sub)}${info.loc ? " · " + U.esc(info.loc) : ""}` +
        `${info.more ? ` · +${info.more} more` : ""} · ctrl+click to go</div>`,
        e.clientX, e.clientY);
    }, 50);
  });
  wrap.addEventListener("mouseleave", () => { clearTimeout(hoverTimer); lastWord = null; hideTip(); });
  cm.on("cursorActivity", () => { lastWord = null; hideTip(); });

  /* ctrl+click: go to the declaration, macros and builtin includes included */
  wrap.addEventListener("mousedown", e => {
    if (!(e.ctrlKey || e.metaKey) || e.button !== 0) return;
    const pos = cm.coordsChar({ left: e.clientX, top: e.clientY }, "window");
    const inc = includeTarget(cm.getLine(pos.line) || "", pos.ch);
    const word = inc ? null : wordAt(cm, e);
    const def = inc || (word && definitionOf(getDoc(), word, ctx(), { line: pos.line + 1 }));
    if (!def) return;
    e.preventDefault();
    hideTip();
    if (onGoto) onGoto(def);
  }, true);

  /* completion, through the standard hint addon when it is loaded.
   * The hint function is passed EXPLICITLY on every call: registerHelper keys on the mode's name, and
   * this mode is clike wearing an HLSL MIME, so a helper registered under the MIME is never found and
   * Ctrl+Space silently does nothing (it did). */
  if (!CodeMirror.showHint) return;

  const hintFn = () => {
    const cur = cm.getCursor();
    const r = completions(getDoc(), getFiles(), cm.getLine(cur.line), cur.ch, getBuiltins ? getBuiltins() : null);
    if (!r) return null;
    return {
      from: CodeMirror.Pos(cur.line, r.from),
      to: cur,
      list: r.list.map(c => ({ text: c.text, displayText: c.hint ? `${c.text}  ·  ${c.hint}` : c.text }))
    };
  };

  const popup = () => cm.showHint({ hint: hintFn, completeSingle: false });

  cm.setOption("extraKeys", { ...(cm.getOption("extraKeys") || {}), "Ctrl-Space": popup });

  /* Auto-popup only where narrowing is likely wanted: a member dot or an include quote. Popping on
   * every identifier character rebuilt a several-hundred-entry list per keystroke, which is what
   * "typing feels sluggish" was; Ctrl+Space asks for the big list deliberately. */
  cm.on("inputRead", (_, change) => {
    if (cm.getOption("readOnly")) return;
    const ch = change.text[change.text.length - 1];
    if (/[."]$/.test(ch)) popup();
  });
}

window.OxIntelliSense = { hoverInfo, completions, attach, findByName, definitionOf, signature, macroIndex, includeTarget, enumTypeText };
})();
