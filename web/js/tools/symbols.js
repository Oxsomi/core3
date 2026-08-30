/* tools/symbols.js — renders an SRDocument (oiSR, the frontend symbol AST) the way
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

function typeText(n, verbose) {
  if (!n.type) return "";
  const t = n.type;
  if (!verbose) return `(${esc(t.display)})`;
  const shape = t.cls === "Struct" || t.cls === "Object" ? t.cls : `${t.cls} ${t.rows}x${t.cols} elems=${n.arrays && n.arrays.length ? n.arrays[0] : 0}`;
  return `(${esc(t.name)} : ${esc(shape)})`;
}

function locText(n, verbose) {
  if (!n.loc) return "";
  const l = n.loc;
  const text = verbose ? `${l.file}:${l.line}+${l.lines}:${l.col}-${l.col + l.len}` : `${l.file}:${l.line}:${l.col}`;
  return `<a class="sym-loc" href="#" data-goto="${l.line}:${l.col - 1}:${l.len}" data-file="${esc(l.file)}" title="go to definition">(${esc(text)})</a>`;
}

function nodeLine(sr, n, verbose) {
  let head = `<span class="sym-kind ${KIND_CSS[n.kind] || ""}">${esc(n.kind)}</span> <span class="tname">${esc(n.name)}</span>`;
  if (n.kind === "Register" && n.register) head += ` <span class="sym-reg-info">[${esc(n.register.info)} count=${n.register.count}]</span>`;
  if (n.kind === "Parameter") head += n.direction === "return" ? "" : "";
  if (n.type) head += ` <span class="ttype">${typeText(n, verbose)}</span>`;
  if (n.kind === "Function" && n.semantic) head += ` <span class="text-body-secondary">: ${esc(n.semantic)} -> returns</span>`;
  else if (n.semantic) head += ` <span class="text-body-secondary">: ${esc(n.semantic)}</span>`;
  if (n.kind === "Function" && n.entry) head += ` <span class="badge ${n.entry.lib ? "text-bg-warning" : "text-bg-secondary"}">${esc(n.entry.stage)}${n.entry.lib ? " · lib" : ""}</span>`;
  head += ` ${locText(n, verbose)}`;
  if (verbose) head += ` <span class="sym-meta">{#${n.id} parent=${n.parent} children=${n.children.length}}</span>`;
  return head;
}

function renderNode(sr, id, verbose) {
  const n = sr.nodes[id];
  const kids = n.children.map(c => renderNode(sr, c, verbose)).join("");
  const annos = n.annotations.map(a => `<li><div class="tnode"><span class="tw"></span><span class="sym-anno">${esc(a)}</span></div></li>`).join("");
  const exp = !!(kids || annos);
  return `<li><div class="tnode${exp ? " exp" : ""}">${exp ? '<i class="bi bi-chevron-down tw"></i>' : '<span class="tw"></span>'}${nodeLine(sr, n, verbose)}</div>${exp ? `<ul>${annos}${kids}</ul>` : ""}</li>`;
}

function html(sr, opts) {
  opts = opts || {};
  if (!sr) return `<div class="empty-hint"><i class="bi bi-diagram-2 fs-1"></i><h6 class="mt-2">Symbols (oiSR)</h6>
    <p class="small">Compile (or open a file) to reflect its frontend symbol AST: entrypoints, user types, resources, parameters and locals with source locations.
    This is <code>OxC3 shader reflect-symbols</code>; the result can be written as an .oiSR.</p></div>`;
  const c = sr.header.counts;
  const verbose = !!opts.verbose;
  const cli = `OxC3 shader reflect-symbols -input ${sr.sourceName || sr.name}${verbose ? " --verbose" : ""}${opts.output ? " -output " + sr.name : ""}`;
  const head = `<div class="px-3 py-1 border-bottom bg-body-tertiary small d-flex align-items-center gap-3 flex-wrap">
      <span><span class="chip chip-sr">oiSR ${esc(sr.header.version)}</span>
        <span class="ms-2 text-body-secondary">${esc(sr.name)}${sr.mockParsed ? " · mock parse" : ""}</span></span>
      <span class="text-body-secondary">${c.nodes} nodes · ${c.annotations} annotations · ${c.registers} registers · ${c.types} types</span>
      <span class="text-body-secondary">features: ${sr.header.features.join(" · ")}</span>
      <div class="form-check form-switch mb-0 ms-auto"><input class="form-check-input" type="checkbox" id="symVerbose" ${verbose ? "checked" : ""}>
        <label class="form-check-label" for="symVerbose">--verbose</label></div>
      <span class="text-body-secondary cli">${esc(cli)}</span>
    </div>`;
  const roots = sr.nodes.filter(n => n.parent < 0).map(n => renderNode(sr, n.id, verbose)).join("");
  const total = sr.builtinCollapsed.reduce((s, b) => s + b.count, 0);
  const builtins = sr.builtinCollapsed.length
    ? `<li><div class="tnode exp"><i class="bi bi-chevron-down tw"></i><span class="text-body-secondary">... ${total} builtin-include symbols collapsed:</span></div>
        <ul>${sr.builtinCollapsed.map(b => `<li><div class="tnode"><span class="tw"></span><span class="tname">${esc(b.file)}</span><span class="text-body-secondary">: ${b.count} symbols</span></div></li>`).join("")}</ul></li>`
    : "";
  return head + `<ul class="tree sym-tree">${roots}${builtins}</ul>` +
    `<div class="px-3 py-2 small text-body-secondary border-top">Every node carries its source location (ESRFeature_SymbolInfo), so the tree doubles as an outline with go-to-definition. Types are stored as the frontend type plus the alias the source wrote (<code>Variable corner (U32)</code>, <code>--verbose</code> shows <code>uint : Scalar 1x1</code>). <code>file data --includes</code> expands the collapsed built-in symbols.</div>`;
}

/* mount into an element and wire go-to-definition + the verbose switch */
function mount(el, sr, opts) {
  opts = opts || {};
  el.innerHTML = html(sr, opts);
  const v = $("#symVerbose");
  if (v) v.addEventListener("change", () => mount(el, sr, { ...opts, verbose: v.checked }));
  el.querySelectorAll("[data-goto]").forEach(a => a.addEventListener("click", e => {
    e.preventDefault(); e.stopPropagation();
    const [line, ch0, len] = a.dataset.goto.split(":").map(Number);
    if (opts.onGoto) opts.onGoto({ file: a.dataset.file, line, ch0, ch1: ch0 + len, msg: "" });
  }));
}

window.OxSymbols = { html, mount };
})();
