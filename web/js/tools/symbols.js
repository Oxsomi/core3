/* tools/symbols.js: renders an SRDocument (oiSR, the frontend symbol AST) the way
 * `OxC3 shader reflect-symbols` / `file data x.oiSR` print it: one line per node, kind first,
 * then name, type, semantic and source location, children indented; the built-in includes'
 * symbols collapsed into one summary line per include.
 * In Compile mode a location is a go-to-definition link into the editor, which is what the
 * format exists for (outline, hover, go-to-def on the web frontend). */
(function () {
"use strict";
const { $, esc } = window.OxUtil;

const KIND_CSS = { Register: "sym-reg", Function: "sym-fn", Struct: "sym-type", Union: "sym-type", Enum: "sym-type", Interface: "sym-type",
  Typedef: "sym-type", Namespace: "sym-ns", Parameter: "sym-param", Variable: "sym-var", StaticVariable: "sym-var", GroupsharedVariable: "sym-var",
  EnumValue: "sym-var" };

/* A type with no name reported is shown as its category in lower case, so it reads as "some struct"
 * rather than as a type called Struct. Named record types reach the tree through the reflector's own
 * type record, so what lands here is the rest: types the reflection describes by kind alone. */
function typeText(n, verbose, hlslTypes) {
  if (!n.type) return "";
  const t = n.type;
  const unnamed = `<span class="ttype-unnamed" title="the reflection carries no name for this type, only its kind">${esc((t.cls || "").toLowerCase())}</span>`;
  if (!verbose) {
    /* Two spellings are stored: the alias the source wrote and the HLSL builtin it resolves to
     * (F64x4 vs double4, float64_t vs double). The switch picks which one the tree reads in, and
     * falls back to the other when a type only has one of them. */
    const shown = hlslTypes ? (t.name == null ? t.display : t.name) : (t.display == null ? t.name : t.display);
    return shown == null ? `(${unnamed})` : `(${esc(shown)})`;
  }
  const shape = t.cls === "Struct" || t.cls === "Object" ? t.cls : `${t.cls} ${t.rows}x${t.cols} elems=${n.arrays && n.arrays.length ? n.arrays[0] : 0}`;
  return t.name == null ? `(${unnamed} : ${esc(shape)})` : `(${esc(t.name)} : ${esc(shape)})`;
}

function locText(n, verbose) {
  if (!n.loc) return "";
  const l = n.loc;
  const text = verbose ? `${l.file}:${l.line}+${l.lines}:${l.col}-${l.col + l.len}` : `${l.file}:${l.line}:${l.col}`;
  return `<a class="sym-loc" href="#" data-goto="${l.line}:${l.col - 1}:${l.len}" data-file="${esc(l.file)}" title="go to definition">(${esc(text)})</a>`;
}

function nodeLine(sr, n, verbose, hlslTypes) {
  let head = `<span class="sym-kind ${KIND_CSS[n.kind] || ""}">${esc(n.kind)}</span> <span class="tname">${esc(n.name)}</span>`;
  if (n.kind === "Register" && n.register) head += ` <span class="sym-reg-info">[${esc(n.register.info)} count=${n.register.count}]</span>`;
  if (n.kind === "Parameter") head += n.direction === "return" ? "" : "";
  if (n.type) head += ` <span class="ttype">${typeText(n, verbose, hlslTypes)}</span>`;
  /* An enumerator's value, after the name the way the CLI prints it; verbose adds the enum's underlying type. */
  if (n.kind === "EnumValue" && n.enumValue)
    head += ` <span class="text-body-secondary">= ${esc(String(n.enumValue.value))}${verbose ? ` (${esc(window.OxIntelliSense.enumTypeText(n))})` : ""}</span>`;
  if (n.kind === "Function" && n.semantic) head += ` <span class="text-body-secondary">: ${esc(n.semantic)} -> returns</span>`;
  else if (n.semantic) head += ` <span class="text-body-secondary">: ${esc(n.semantic)}</span>`;
  if (n.kind === "Function" && n.entry) head += ` <span class="badge ${n.entry.lib ? "text-bg-warning" : "text-bg-secondary"}">${esc(n.entry.stage)}${n.entry.lib ? " · lib" : ""}</span>`;
  head += ` ${locText(n, verbose)}`;
  if (verbose) head += ` <span class="sym-meta">{#${n.id} parent=${n.parent} children=${n.children.length}}</span>`;
  return head;
}

/* An annotation has no source span of its own, so it jumps to the node it is written on: clicking
 * [[oxc::stage("compute")]] lands on the entrypoint that carries it. */
function annoLine(n, a) {
  const inner = `<span class="sym-anno">${esc(a)}</span>`;
  if (!n.loc) return inner;
  return `<a class="sym-anno-link" href="#" data-goto="${n.loc.line}:${n.loc.col - 1}:${n.loc.len}"
    data-file="${esc(n.loc.file)}" title="go to the symbol this is written on">${inner}</a>`;
}

function renderNode(sr, id, verbose, showBuiltins, hlslTypes) {
  const n = sr.nodes[id];
  const kids = n.children
    .filter(c => showBuiltins || !sr.nodes[c].builtin)
    .map(c => renderNode(sr, c, verbose, showBuiltins, hlslTypes)).join("");
  const annos = n.annotations.map(a => `<li><div class="tnode"><span class="tw"></span>${annoLine(n, a)}</div></li>`).join("");
  const exp = !!(kids || annos);
  return `<li><div class="tnode${exp ? " exp" : ""}">${exp ? '<i class="bi bi-chevron-down tw"></i>' : '<span class="tw"></span>'}${nodeLine(sr, n, verbose, hlslTypes)}</div>${exp ? `<ul>${annos}${kids}</ul>` : ""}</li>`;
}

function html(sr, opts) {
  opts = opts || {};
  if (!sr) return `<div class="empty-hint"><i class="bi bi-diagram-2 fs-1"></i><h6 class="mt-2">Symbols (oiSR)</h6>
    <p class="small">Compile (or open a file) to reflect its frontend symbol AST: entrypoints, user types, resources, parameters and locals with source locations.
    This is <code>OxC3 shader reflect-symbols</code>; the result can be written as an .oiSR.</p></div>`;
  const c = sr.header.counts;
  const verbose = !!opts.verbose;
  const showBuiltins = !!opts.builtins;
  const hlslTypes = !!opts.hlslTypes;

  /* A document can name its built-in includes in the summary without carrying their symbols, which is what
   * the mock parse produces: it reads the source in front of it, and the built-ins are not in it. The switch
   * has nothing to expand then, so it says so rather than flipping and appearing to do nothing. */

  const hasBuiltinNodes = sr.nodes.some(n => n.builtin);
  const expanded = showBuiltins && hasBuiltinNodes;
  const cli = `OxC3 shader reflect-symbols -input ${sr.sourceName || sr.name}${verbose ? " --verbose" : ""}${showBuiltins ? " --includes" : ""}${opts.output ? " -output " + sr.name : ""}`;
  const head = `<div class="px-3 py-1 border-bottom bg-body-tertiary small d-flex align-items-center gap-3 flex-wrap">
      <span><span class="chip chip-sr">oiSR ${esc(sr.header.version)}</span>
        <span class="ms-2 text-body-secondary">${esc(sr.name)}${sr.mockParsed ? " · mock parse" : ""}</span></span>
      <span class="text-body-secondary">${c.nodes} nodes · ${c.annotations} annotations · ${c.registers} registers · ${c.types} types</span>
      <span class="text-body-secondary">features: ${sr.header.features.join(" · ")}</span>
      <div class="form-check form-switch mb-0 ms-auto"><input class="form-check-input" type="checkbox" id="symBuiltins" ${showBuiltins ? "checked" : ""} ${hasBuiltinNodes ? "" : "disabled"}>
        <label class="form-check-label" for="symBuiltins" title="${hasBuiltinNodes
          ? "file data --includes: expand the built-in includes' symbols into the tree instead of collapsing them"
          : "this document lists its built-in includes but doesn't carry their symbols, so there is nothing to expand"}">--includes</label></div>
      <div class="form-check form-switch mb-0"><input class="form-check-input" type="checkbox" id="symHlslTypes" ${hlslTypes ? "checked" : ""}>
        <label class="form-check-label" for="symHlslTypes" title="show the HLSL builtin each type resolves to (double4) instead of the alias the source wrote (F64x4)">HLSL types</label></div>
      <div class="form-check form-switch mb-0"><input class="form-check-input" type="checkbox" id="symVerbose" ${verbose ? "checked" : ""}>
        <label class="form-check-label" for="symVerbose">--verbose</label></div>
      <span class="text-body-secondary cli">${esc(cli)}</span>
    </div>`;
  const roots = sr.nodes
    .filter(n => n.parent < 0 && (expanded || !n.builtin))
    .map(n => renderNode(sr, n.id, verbose, expanded, hlslTypes)).join("");
  const total = sr.builtinCollapsed.reduce((s, b) => s + b.count, 0);

  /* Folded, the built-in includes are one summary line per include, the way the CLI prints them.
   * Expanded, their symbols sit in the tree where they belong; the summary stays as the count. */
  const builtins = sr.builtinCollapsed.length
    ? `<li><div class="tnode exp"><i class="bi bi-chevron-down tw"></i><span class="text-body-secondary">${expanded ? `${total} builtin-include symbols shown above:` : `... ${total} builtin-include symbols collapsed:`}</span></div>
        <ul>${sr.builtinCollapsed.map(b => `<li><div class="tnode"><span class="tw"></span><a class="tname inc-link" href="#" data-openfile="${esc(b.file.replace(/^\.\//, ""))}" title="open ${esc(b.file)}">${esc(b.file)}</a><span class="text-body-secondary">: ${b.count} symbols</span></div></li>`).join("")}</ul></li>`
    : "";
  return head + `<ul class="tree sym-tree">${roots}${builtins}</ul>` +
    `<div class="px-3 py-2 small text-body-secondary border-top">Every node carries its source location (ESRFeature_SymbolInfo), so the tree doubles as an outline with go-to-definition. Types are stored as the frontend type plus the alias the source wrote (<code>Variable corner (U32)</code>, <code>--verbose</code> shows <code>uint : Scalar 1x1</code>). <b>HLSL types</b> swaps the tree over to the builtin spelling on its own, so <code>F64x4</code> reads as <code>double4</code> and <code>float64_t</code> as <code>double</code>; it is a display choice, so the command line beside it does not change. <code>file data --includes</code> expands the collapsed built-in symbols.</div>`;
}

/* mount into an element and wire go-to-definition + the display switches */
function mount(el, sr, opts) {
  opts = opts || {};
  el.innerHTML = html(sr, opts);

  /* The switches describe the view, not the reflection, so a flip is reported back before the
   * re-render: the caller re-mounts on every new reflection and would otherwise clear them. */
  const flip = (id, key) => {
    const box = $("#" + id);
    if (!box) return;
    box.addEventListener("change", () => {
      const next = { ...opts, [key]: box.checked };
      if (opts.onOpts) opts.onOpts({ verbose: !!next.verbose, builtins: !!next.builtins, hlslTypes: !!next.hlslTypes });
      mount(el, sr, next);
    });
  };

  flip("symVerbose", "verbose");
  flip("symBuiltins", "builtins");
  flip("symHlslTypes", "hlslTypes");
  el.querySelectorAll("[data-goto]").forEach(a => a.addEventListener("click", e => {
    e.preventDefault(); e.stopPropagation();
    const [line, ch0, len] = a.dataset.goto.split(":").map(Number);
    if (opts.onGoto) opts.onGoto({ file: a.dataset.file, line, ch0, ch1: ch0 + len, msg: "" });
  }));
}

window.OxSymbols = { html, mount };
})();
