/* app.js — state + orchestration. Owns the rail, the three modes, uploads/downloads,
 * the problems panel, share permalinks and the CLI↔web map. All backend work goes
 * through OxAPI (see js/api.js for the wasm porting notes). */
(function () {
"use strict";
const { $, $$, esc, fmtBytes, download, debounce } = window.OxUtil;
const M = window.OxMock;

/* ------------------------------------------------------------------ state */

const state = {
  mode: "compile",                       // compile | oish | bins
  files: JSON.parse(JSON.stringify(M.SAMPLE_FILES)),
  active: "lighting.hlsl",
  activeBuiltin: null,                   // name when a read-only @builtin is open
  compiled: null,                        // {doc, diags, ms, opts}
  symbols: null,                         // SRDocument of the active file (shader reflect-symbols)
  pipeline: { sp: null, refused: null, pick: {} },   // SPDocument derived from the compiled doc (or the loaded oiSP)
  isa: { asic: "gfx1100", entrypoint: null, result: null, error: null, targets: [], stateOpen: false },
  caps: { host: "browser", liveIsa: false, offlineIsa: true },   // OxAPI.capabilities(): what the host can really run
  oishDocs: M.seedOishDocs(),            // name -> SHDocument (Inspect mode)
  oisrDocs: window.OxMockFormats.seedOisrDocs(),   // name -> SRDocument (Inspect mode examples)
  oispDocs: window.OxMockFormats.seedOispDocs(),   // name -> SPDocument
  inspected: null,                       // name of the doc open in Inspect mode
  inspectedKind: "oish",                 // oish | oisr | oisp
  standalone: M.sampleStandaloneBins(),  // name -> {type, bytes, text?, origin?, doc?}; doc = reflected one-binary SHDocument
  binActive: null,                       // name of the standalone binary open in SPV / DXIL mode
  binRowsArr: []                         // rows currently in #binSel
};

/* ------------------------------------------------------------------ toast */

function toast(msg, kind) {
  const t = document.createElement("div");
  t.className = "position-fixed bottom-0 end-0 m-3 alert alert-" + (kind || "info") + " shadow py-2 px-3";
  t.style.zIndex = 3000; t.style.maxWidth = "440px"; t.style.fontSize = "13px";
  t.innerHTML = msg;
  document.body.appendChild(t);
  setTimeout(() => { t.classList.add("fade"); setTimeout(() => t.remove(), 400); }, 4200);
}

/* ------------------------------------------------------------------ file rail */

function fitem(cls, icon, name, tag, title) {
  return `<div class="fitem ${cls}" data-name="${esc(name)}" title="${esc(title || name)}">
    <i class="bi ${icon}"></i><span class="name">${esc(name.split("/").pop())}</span>
    ${tag ? `<span class="tag text-body-secondary">${esc(tag)}</span>` : ""}</div>`;
}
const railGroup = label => `<div class="rail-grp mt-2"><span>${esc(label)}</span></div>`;

function renderRail() {
  const tree = $("#railTree");
  if (state.mode === "compile") {
    $("#railTitle").textContent = "Project";
    $("#addFile").classList.remove("d-none"); $("#addFolder").classList.remove("d-none");
    $("#builtinGrp").classList.remove("d-none"); $("#builtinFiles").classList.remove("d-none");
    const names = Object.keys(state.files).sort();
    const roots = names.filter(n => !n.includes("/"));
    const dirs = {};
    names.filter(n => n.includes("/")).forEach(n => { const d = n.split("/")[0]; (dirs[d] = dirs[d] || []).push(n); });
    let h = roots.map(n => fitem(n === state.active && !state.activeBuiltin ? "active" : "", "bi-file-earmark-code", n)).join("");
    for (const d of Object.keys(dirs).sort()) {
      h += `<div class="fitem folder"><i class="bi bi-folder2-open"></i><span class="name">${esc(d)}/</span></div>`;
      h += dirs[d].map(n => `<div style="padding-left:14px">` +
        fitem(n === state.active && !state.activeBuiltin ? "active" : "", "bi-file-earmark-code", n) + `</div>`).join("");
    }
    tree.innerHTML = h;
    $("#builtinFiles").innerHTML = Object.keys(M.BUILTINS)
      .map(n => fitem("builtin" + (state.activeBuiltin === n ? " active" : ""), "bi-file-earmark-lock2", n, "", n + " (read-only)")).join("");
  } else if (state.mode === "oish") {
    $("#railTitle").textContent = "Loaded files";
    $("#addFile").classList.add("d-none"); $("#addFolder").classList.add("d-none");
    $("#builtinGrp").classList.add("d-none"); $("#builtinFiles").classList.add("d-none");
    const act = (n, k) => state.inspected === n && state.inspectedKind === k ? " active" : "";
    let h = "";
    const sh = Object.keys(state.oishDocs);
    if (sh.length) h += railGroup("oiSH · compiled shaders") + sh.map(n => {
      const d = state.oishDocs[n];
      return fitem("oish" + act(n, "oish"), "bi-box-seam", n, `${d.entries.length}·SM${d.model}`, `${n} — ${d.binaries.length} binaries`);
    }).join("");
    const sr = Object.keys(state.oisrDocs);
    if (sr.length) h += railGroup("oiSR · symbol ASTs") + sr.map(n => {
      const d = state.oisrDocs[n];
      return fitem("oisr" + act(n, "oisr"), "bi-diagram-2", n, `${d.header.counts.nodes} nodes`, `${n} — ${d.header.counts.registers} registers`);
    }).join("");
    const sp = Object.keys(state.oispDocs);
    if (sp.length) h += railGroup("oiSP · pipelines") + sp.map(n => {
      const d = state.oispDocs[n];
      return fitem("oisp" + act(n, "oisp"), "bi-diagram-3", n, d.pipelines.map(p => p.type).join(","), `${n} — ${d.header.counts.pipelines} pipeline(s)`);
    }).join("");
    tree.innerHTML = h || `<div class="p-3 small text-body-secondary">Load an .oiSH, .oiSR or .oiSP to inspect it.</div>`;
  } else {
    $("#railTitle").textContent = "Binaries";
    $("#addFile").classList.add("d-none"); $("#addFolder").classList.add("d-none");
    $("#builtinGrp").classList.add("d-none"); $("#builtinFiles").classList.add("d-none");
    tree.innerHTML = Object.keys(state.standalone).map(n =>
      fitem("sbin", n.endsWith(".spv") ? "bi-filetype-raw" : "bi-file-binary", n,
        fmtBytes(state.standalone[n].bytes.length))).join("")
      || `<div class="p-3 small text-body-secondary">Load a .spv / .dxil.</div>`;
  }
}

$("#railTree").addEventListener("click", e => {
  const it = e.target.closest(".fitem"); if (!it || it.classList.contains("folder")) return;
  const name = it.dataset.name;
  if (state.mode === "compile") openFile(name);
  else if (state.mode === "oish") openInspect(name, it.classList.contains("oisr") ? "oisr" : it.classList.contains("oisp") ? "oisp" : "oish");
  else openStandalone(name);
});
$("#builtinFiles").addEventListener("click", e => {
  const it = e.target.closest(".fitem"); if (it) openBuiltin(it.dataset.name);
});

function openFile(name) {
  state.active = name; state.activeBuiltin = null;
  $("#fileName").textContent = name; $("#roBadge").classList.add("d-none");
  window.OxEditor.open(state.files[name].src, false);
  if (state.compiled) window.OxEditor.markDiags(state.compiled.diags.filter(d => d.file === name));
  renderRail(); scheduleCommands(); scheduleSymbols();
}
function openBuiltin(name) {
  state.activeBuiltin = name;
  $("#fileName").textContent = name; $("#roBadge").classList.remove("d-none");
  window.OxEditor.open(M.BUILTINS[name], true);
  renderRail();
}

$("#addFile").addEventListener("click", () => {
  const name = prompt("New file (e.g. sky.hlsl or include/util.hlsli):"); if (!name) return;
  if (state.files[name]) return toast("File already exists.", "warning");
  state.files[name] = { src: `#include "@types.hlsli"\n`, diags: [] };
  openFile(name);
});
$("#addFolder").addEventListener("click", () => {
  const name = prompt("New folder name:"); if (!name) return;
  state.files[name.replace(/\/$/, "") + "/.keep.hlsli"] = { src: "#pragma once\n", diags: [] };
  renderRail();
});

/* ------------------------------------------------------------------ modes */

const tabItem = t => document.querySelector(`#outTabs [data-tab="${t}"]`);
const clickTab = t => document.querySelector(`[data-bs-target="#p-${t}"]`).click();

function setMode(mode) {
  state.mode = mode;
  $("#compileTools").classList.toggle("d-none", mode !== "compile");
  $("#oishTools").classList.toggle("d-none", mode !== "oish");
  $("#binsTools").classList.toggle("d-none", mode !== "bins");
  $("#editorPane").classList.toggle("d-none", mode !== "compile");
  $("#binPane").classList.toggle("d-none", mode !== "bins");
  $("#problems").classList.toggle("d-none", mode !== "compile");
  tabItem("cmd").classList.toggle("d-none", mode !== "compile");
  tabItem("sym").classList.toggle("d-none", mode === "bins");
  renderRail();

  if (mode === "compile") {
    $("#binStrip").classList.toggle("d-none", !state.compiled);
    renderCompiled();
    renderSymbols(state.symbols);
  } else if (mode === "oish") {
    fillDiffSelects();
    if (!state.inspected) { state.inspected = Object.keys(state.oishDocs)[0] || null; state.inspectedKind = "oish"; }
    openInspect(state.inspected, state.inspectedKind, true);
    clickTab("diff");
  } else {
    enterBinsMode();
  }
}
async function enterBinsMode() {
  /* every loaded binary is reflected up front so Diff A↔B can pair them like oiSH documents */
  for (const n of Object.keys(state.standalone)) await standaloneDoc(n);
  fillDiffSelects();
  if (!state.binActive || !state.standalone[state.binActive]) state.binActive = Object.keys(state.standalone)[0] || null;
  await openStandalone(state.binActive, true);
  clickTab("refl");
}
$("#mCompile").addEventListener("change", () => setMode("compile"));
$("#mOish").addEventListener("change", () => setMode("oish"));
$("#mBins").addEventListener("change", () => setMode("bins"));

/* ------------------------------------------------------------------ compile mode */

async function doCompile() {
  const res = await window.OxCompile.run(state);
  state.compiled = res;
  state.pipeline = { sp: null, refused: null, pick: {} };
  state.isa.result = null; state.isa.error = null;
  renderCompiled();
  await refreshSymbols();
  await refreshPipeline();
}
$("#compileBtn").addEventListener("click", doCompile);

function renderCompiled() {
  const c = state.compiled;
  if (!c) {
    $("#reflView").innerHTML = window.OxInspect.reflectionHTML(null);
    $("#oishView").innerHTML = window.OxInspect.oishViewHTML(null);
    window.OxDiff.renderCompileDiff(null);
    renderPipeline(); renderIsa();
    $("#stCompiled").innerHTML = '<i class="bi bi-circle"></i> not compiled';
    $("#stSizes").textContent = ""; $("#stProblems").textContent = "";
    $("#dlBtn").setAttribute("disabled", "");
    renderProblems([]);
    return;
  }
  const { doc, diags, ms, opts } = c;
  const errors = diags.filter(d => d.sev === "error").length;
  const warns = diags.filter(d => d.sev === "warn").length;
  const failed = errors > 0;

  $("#failOverlay").style.display = failed ? "flex" : "none";
  window.OxEditor.markDiags(diags.filter(d => d.file === state.active));
  renderProblems(diags);

  $("#reflView").innerHTML = window.OxInspect.reflectionHTML(failed ? null : doc);
  $("#oishView").innerHTML = window.OxInspect.oishViewHTML(failed ? null : doc);
  window.OxDiff.renderCompileDiff(failed ? null : doc);
  window.OxCompile.renderCommands(state.active, state.files);

  /* binary strip */
  state.binRowsArr = failed ? [] : window.OxInspect.binRows(doc, opts.targets);
  fillBinStrip();
  $("#binStrip").classList.toggle("d-none", !state.binRowsArr.length);
  if (state.binRowsArr.length) showSelectedBinary(true);
  renderPipeline(); renderIsa();

  /* status + downloads */
  const spvTotal = doc.binaries.reduce((s, b) => s + (opts.targets.includes("spv") ? b.sizes.spirv : 0), 0);
  const dxTotal = doc.binaries.reduce((s, b) => s + (opts.targets.includes("dxil") ? b.sizes.dxil : 0), 0);
  $("#stCompiled").innerHTML = failed
    ? `<i class="bi bi-x-circle text-danger"></i> ${esc(state.active)} failed in ${ms} ms`
    : `<i class="bi bi-check-circle text-success"></i> ${esc(state.active)} → ${doc.binaries.length} binaries in ${ms} ms${opts.reflectionOnly ? " (reflection only)" : ""}`;
  $("#stSizes").textContent = failed || opts.reflectionOnly ? "" :
    [opts.targets.includes("spv") ? "spv " + fmtBytes(spvTotal) : "", opts.targets.includes("dxil") ? "dxil " + fmtBytes(dxTotal) : ""].filter(Boolean).join(" · ");
  $("#stProblems").textContent = diags.length ? `${errors} error(s) · ${warns} warning(s)` : "";
  buildDlMenu();
  if (failed) $("#dlBtn").setAttribute("disabled", ""); else $("#dlBtn").removeAttribute("disabled");
}

/* live Command tab while typing */
const scheduleCommands = debounce(() => {
  if (state.mode === "compile" && !state.activeBuiltin)
    window.OxCompile.renderCommands(state.active, state.files);
}, 350);

/* ------------------------------------------------------------------ symbols (oiSR) */

/* `shader reflect-symbols` runs on the source, not the oiSH, so it follows the editor (debounced) rather than the compile */
const scheduleSymbols = debounce(() => { if (state.mode === "compile" && !state.activeBuiltin) refreshSymbols(); }, 500);

async function refreshSymbols() {
  if (!state.files[state.active]) return;
  state.symbols = await window.OxAPI.reflectSymbols(state.active, state.files);
  if (state.mode === "compile") renderSymbols(state.symbols);
}
function renderSymbols(sr) {
  window.OxSymbols.mount($("#symView"), sr, { onGoto: gotoSymbol });
}
function gotoSymbol(loc) {
  if (!state.files[loc.file]) return toast(`${esc(loc.file)} isn't in the project tree, so there's nothing to jump to (the oiSR keeps the location anyway).`, "info");
  if (state.mode !== "compile") { $("#mCompile").checked = true; setMode("compile"); }
  if (loc.file !== state.active || state.activeBuiltin) openFile(loc.file);
  window.OxEditor.gotoDiag(loc);
}

/* ------------------------------------------------------------------ pipeline (oiSP) */

async function refreshPipeline() {
  const doc = state.compiled && !state.compiled.diags.some(d => d.sev === "error") ? state.compiled.doc : null;
  if (!doc || doc.flags.reflectionOnly) { state.pipeline.sp = null; state.pipeline.refused = null; renderPipeline(); renderIsa(); return; }
  const r = await window.OxAPI.derivePipeline(doc, state.pipeline.pick);
  if (r.refused) { state.pipeline.sp = null; state.pipeline.refused = r; }
  else { state.pipeline.sp = r; state.pipeline.refused = null; }
  await renderPipeline(); renderIsa();
}
async function renderPipeline() {
  const el = $("#psoView");
  const inspectingSp = state.mode === "oish" && state.inspectedKind === "oisp";
  const sp = inspectingSp ? state.oispDocs[state.inspected] : state.pipeline.sp;
  const doc = inspectingSp ? null : currentDoc();
  const refused = inspectingSp ? null : state.pipeline.refused;
  const printText = sp && sp.pipelines.length ? await window.OxAPI.printPipeline(sp, 0) : null;
  state.pipeline.printText = printText;
  window.OxPipeline.mount(el, { sp, refused, doc, editable: !inspectingSp, printText }, {
    onSupply: (pi, field, index, value) => supplyField(sp, pi, field, index, value),
    onPick: async (stage, idx) => { state.pipeline.pick[stage] = idx; await refreshPipeline(); }
  });
}
async function supplyField(sp, pi, field, index, value) {
  try {
    await window.OxAPI.supplyPipeline(sp, pi, field, index, value);
    state.isa.result = null;                 // the state changed, so a live result would be stale
    await renderPipeline(); renderIsa(); buildDlMenu();
  } catch (err) { toast(`<b>SPFile_supply failed:</b> ${esc(err.message)}`, "danger"); }
}

/* ------------------------------------------------------------------ ISA */

function isaCtx() {
  const doc = currentDoc();
  const row = state.binRowsArr[+$("#binSel").value] || state.binRowsArr[0];
  if (!doc || !row) return null;
  const bin = doc.binaries[row.binIdx];
  const entrypoint = state.isa.entrypoint && bin.entryNames.includes(state.isa.entrypoint) ? state.isa.entrypoint : (bin.lib ? bin.entryNames[0] : bin.entrypoint);
  const sp = state.pipeline.sp;
  return { doc, bin, row, targets: state.isa.targets, asic: state.isa.asic, entrypoint, result: state.isa.result, error: state.isa.error,
    sp, refused: state.pipeline.refused, printText: state.pipeline.printText,
    exact: !!(sp && sp.pipelines[0] && sp.pipelines[0].fields.every(f => f.source !== "assumed")),
    caps: state.caps, busy: state.isa.busy, stateOpen: state.isa.stateOpen };
}
function renderIsa() {
  window.OxIsa.mount($("#isaView"), isaCtx(), {
    onChange: o => { state.isa.asic = o.asic; state.isa.entrypoint = o.entrypoint; state.isa.result = null; state.isa.error = null; renderIsa(); },
    onRun: async o => {
      state.isa.asic = o.asic; state.isa.entrypoint = o.entrypoint; state.isa.result = null; state.isa.error = null;
      const ctx = isaCtx(); if (!ctx) return;
      state.isa.busy = true; renderIsa();
      try {
        if (o.asic === "live") state.isa.result = await window.OxAPI.isaLive(ctx.doc, ctx.bin, ctx.sp, { entry: ctx.row.binIdx });
        else state.isa.result = await window.OxAPI.isaDisassemble(ctx.doc, ctx.bin, o.asic, ctx.entrypoint);
      } catch (err) { state.isa.error = err.message; }
      state.isa.busy = false; renderIsa(); buildDlMenu();
    },
    onToggleState: () => { state.isa.stateOpen = !state.isa.stateOpen; renderIsa(); },
    onSupply: (pi, field, index, value) => { const sp = state.pipeline.sp; if (sp) supplyField(sp, pi, field, index, value); }
  });
}

/* ------------------------------------------------------------------ standalone binaries (SPV / DXIL mode) */

/* a bare binary is reflected from its bytes once, then it's a document like any other */
async function standaloneDoc(name) {
  const entry = state.standalone[name];
  if (!entry) return null;
  if (!entry.doc) entry.doc = await window.OxAPI.reflectBinary(name, entry.type, entry.bytes, entry.origin);
  return entry.doc;
}

async function openStandalone(name, keepTab) {
  state.binActive = name && state.standalone[name] ? name : null;
  window.OxBinary.render(state, {
    onAssembled: (outName, entry) => { state.standalone[outName] = entry; fillDiffSelects(); openStandalone(outName); }
  });
  renderRail();
  const doc = state.binActive ? await standaloneDoc(state.binActive) : null;
  $("#reflView").innerHTML = window.OxInspect.reflectionHTML(doc);
  $("#oishView").innerHTML = window.OxInspect.oishViewHTML(doc);
  renderSymbols(null);
  state.binRowsArr = doc ? window.OxInspect.binRows(doc, null) : [];
  fillBinStrip();
  $("#binStrip").classList.toggle("d-none", !state.binRowsArr.length);
  if (state.binRowsArr.length) showSelectedBinary(true);
  state.pipeline = { sp: null, refused: null, pick: {} };
  state.isa.result = null; state.isa.error = null;
  if (doc) {
    const r = await window.OxAPI.derivePipeline(doc, {});
    if (r && r.refused) state.pipeline.refused = r; else state.pipeline.sp = r;
  }
  await renderPipeline(); renderIsa();
  renderInspectDiff();
  buildDlMenu();
  wireAssembleCard(doc);
  if (!keepTab) clickTab("refl");
}

/* the oiSH tab of a standalone binary: give it an identifier and it becomes a real oiSH in Inspect mode */
function wireAssembleCard(doc) {
  const go = $("#asmOishGo");
  if (!go || !doc) return;
  go.addEventListener("click", async () => {
    const picked = id => [...$(id).selectedOptions].map(o => o.value);
    const ident = { entry: $("#asmEntry").value.trim(), stage: $("#asmStage").value, model: $("#asmModel").value,
      extensions: picked("#asmExts"), vendors: picked("#asmVendors"), name: $("#asmName").value.trim() };
    const out = await window.OxAPI.assembleOiSH(doc, ident);
    state.oishDocs[out.name] = out;
    $("#mOish").checked = true; setMode("oish"); fillDiffSelects(); openInspect(out.name, "oish");
    toast(`<b>Assembled:</b> ${esc(out.name)} — a real oiSH now (${esc(ident.stage)} · SM ${esc(ident.model)}${ident.extensions.length ? " · " + esc(ident.extensions.join("+")) : ""}).`, "success");
  });
}

/* raw DXC output (Command tab → Run with DXC) is a standalone binary, never an oiSH */
function onRawDxc(r) {
  state.standalone[r.name] = { type: r.type, bytes: r.bytes, text: r.text, origin: r.origin, raw: true };
  toast(`<b>DXC:</b> ${esc(r.name)} — standalone ${r.type === "spirv" ? "SPIR-V" : "DXIL"}, ${fmtBytes(r.bytes.length)}. No oiSH and no annotations were processed; assemble one from its oiSH tab.`, "success");
  $("#mBins").checked = true; setMode("bins");
  state.binActive = r.name; openStandalone(r.name);
}

/* ------------------------------------------------------------------ inspect mode (oiSH / oiSR / oiSP) */

const diffSelects = () => state.mode === "bins" ? [$("#diffA2"), $("#diffB2")] : [$("#diffA"), $("#diffB")];
function diffDoc(name) {
  if (state.mode === "bins") return (state.standalone[name] && state.standalone[name].doc) || null;
  return state.oishDocs[name] || null;
}
function fillDiffSelects() {
  const names = state.mode === "bins" ? Object.keys(state.standalone) : Object.keys(state.oishDocs);
  const opt = names.map(n => `<option>${esc(n)}</option>`).join("");
  const [a, b] = diffSelects();
  const pa = a.value, pb = b.value;
  a.innerHTML = opt; b.innerHTML = opt;
  a.value = names.includes(pa) ? pa : names[0] || "";
  b.value = names.includes(pb) ? pb : names[Math.min(1, names.length - 1)] || "";
}
function currentAB() {
  const [a, b] = diffSelects();
  return [diffDoc(a.value), diffDoc(b.value)];
}
function renderInspectDiff() {
  const [a, b] = currentAB();
  window.OxDiff.renderReflectionDiff(a, b);
  window.OxDiff.refreshBinaryPairs(a, b);
}
["#diffA", "#diffB", "#diffA2", "#diffB2"].forEach(id => $(id).addEventListener("change", renderInspectDiff));

async function openInspect(name, kind, keepTab) {
  const store = { oish: state.oishDocs, oisr: state.oisrDocs, oisp: state.oispDocs }[kind || "oish"];
  if (!name || !store || !store[name]) { renderRail(); renderInspectDiff(); return; }
  state.inspected = name; state.inspectedKind = kind;
  const doc = store[name];
  if (kind === "oish") {
    $("#reflView").innerHTML = window.OxInspect.reflectionHTML(doc);
    $("#oishView").innerHTML = window.OxInspect.oishViewHTML(doc);
    state.binRowsArr = window.OxInspect.binRows(doc, null);
    fillBinStrip();
    $("#binStrip").classList.toggle("d-none", !state.binRowsArr.length);
    if (state.binRowsArr.length) showSelectedBinary(true);
    renderSymbols(null);
    state.pipeline = { sp: null, refused: null, pick: {} };
    const r = doc.flags.reflectionOnly ? null : await window.OxAPI.derivePipeline(doc, {});
    if (r && r.refused) state.pipeline.refused = r; else state.pipeline.sp = r;
    await renderPipeline(); renderIsa();
    if (!keepTab) clickTab("refl");
  } else if (kind === "oisr") {
    $("#binStrip").classList.add("d-none"); state.binRowsArr = [];
    $("#reflView").innerHTML = window.OxInspect.reflectionHTML(null);
    $("#oishView").innerHTML = window.OxInspect.oishViewHTML(null);
    renderSymbols(doc);
    await renderPipeline(); renderIsa();
    if (!keepTab) clickTab("sym");
  } else {
    $("#binStrip").classList.add("d-none"); state.binRowsArr = [];
    $("#reflView").innerHTML = window.OxInspect.reflectionHTML(null);
    $("#oishView").innerHTML = window.OxInspect.oishViewHTML(null);
    renderSymbols(null);
    await renderPipeline(); renderIsa();
    if (!keepTab) clickTab("pso");
  }
  renderInspectDiff();
  buildDlMenu();
  $("#dlBtn").removeAttribute("disabled");
  renderRail();
}

$("#combineBtn").addEventListener("click", async () => {
  const [a, b] = currentAB();
  if (!a || !b) return toast("Pick two oiSH files (A and B) first.", "warning");
  try {
    const doc = await window.OxAPI.combineOiSH(a, b);
    state.oishDocs[doc.name] = doc;
    fillDiffSelects(); openInspect(doc.name, "oish");
    toast(`<b>Combined:</b> ${esc(doc.name)} — union of both files' binaries.`, "success");
  } catch (err) {
    toast(`<b>file combine failed:</b> ${esc(err.message)}`, "danger");
  }
});

/* ------------------------------------------------------------------ binary strip */

function fillBinStrip() {
  const sel = $("#binSel");
  sel.innerHTML = state.binRowsArr.map((r, i) => `<option value="${i}">${esc(r.label)}</option>`).join("");
  $("#binMeta").textContent = "";
}
$("#binSel").addEventListener("change", () => { state.isa.result = null; state.isa.error = null; state.isa.entrypoint = null; showSelectedBinary(false); renderIsa(); });
async function showSelectedBinary(auto) {
  const row = state.binRowsArr[+$("#binSel").value];
  const doc = currentDoc();
  if (row && doc) await window.OxInspect.showBinary(doc, row, !auto);
}
function currentDoc() {
  if (state.mode === "bins") return (state.binActive && state.standalone[state.binActive] && state.standalone[state.binActive].doc) || null;
  return state.mode === "oish" ? (state.inspectedKind === "oish" ? state.oishDocs[state.inspected] : null) : (state.compiled && state.compiled.doc);
}

/* ------------------------------------------------------------------ uploads */

$("#loadOishBtn").addEventListener("click", () => $("#uploadOish").click());
$("#uploadOish").addEventListener("change", async e => {
  let last = null;
  for (const f of e.target.files) {
    const bytes = new Uint8Array(await f.arrayBuffer());
    /* the CLI sniffs the magic (file header); the extension is the fallback for mock bytes */
    const magic = String.fromCharCode(...bytes.slice(0, 4));
    const kind = magic === "oiSR" || /\.oiSR$/i.test(f.name) ? "oisr" : magic === "oiSP" || /\.oiSP$/i.test(f.name) ? "oisp" : "oish";
    if (kind === "oisr") state.oisrDocs[f.name] = await window.OxAPI.parseOiSR(f.name, bytes);
    else if (kind === "oisp") state.oispDocs[f.name] = await window.OxAPI.parseOiSP(f.name, bytes);
    else state.oishDocs[f.name] = await window.OxAPI.parseOiSH(f.name, bytes);
    last = { name: f.name, kind };
  }
  e.target.value = "";
  fillDiffSelects(); renderRail();
  if (last) openInspect(last.name, last.kind);
  toast("Loaded — note: parsing is mocked (shape derived from the sample); the real path is SHFile_read / SRFile_read / SPFile_read.", "info");
});

$("#loadBinBtn").addEventListener("click", () => $("#uploadBin").click());
$("#uploadBin").addEventListener("change", async e => {
  let last = null;
  for (const f of e.target.files) {
    const bytes = new Uint8Array(await f.arrayBuffer());
    state.standalone[f.name] = { type: /\.dxil$/i.test(f.name) ? "dxil" : "spirv", bytes };
    last = f.name;
  }
  e.target.value = "";
  for (const n of Object.keys(state.standalone)) await standaloneDoc(n);
  fillDiffSelects();
  if (last) openStandalone(last);
  toast("Loaded — note: reflection of uploaded bytes is mocked (shape derived from the sample); the real path is spirv-reflect / DXC container reflection.", "info");
});

/* ------------------------------------------------------------------ downloads */

function dlItem(act, label, sub) {
  return `<li><button class="dropdown-item small" data-act="${act}">${label}
    ${sub ? `<div class="text-body-secondary" style="font-size:11px">${sub}</div>` : ""}</button></li>`;
}
function buildDlMenu() {
  const menu = $("#dlMenu");
  const doc = currentDoc();
  const inspectKind = state.mode === "oish" ? state.inspectedKind : "oish";
  let h = "";
  if (doc && doc.standalone) {
    h += dlItem("bin", `${esc(doc.name)}`, "the loaded bytes as they are");
    h += dlItem("txt", `${esc(doc.name.replace(/\.(spv|dxil)$/i, ""))}.txt`, "shader disassemble");
    h += `<li><hr class="dropdown-divider"></li>`;
    h += `<li><span class="dropdown-item-text small text-body-secondary">no oiSH yet: assemble one from the oiSH tab</span></li>`;
  } else if (doc) {
    const base = doc.name.replace(/\.oiSH$/i, "").replace(/\.hlsl$/i, "");
    const opts = state.compiled && state.mode === "compile" ? state.compiled.opts : null;
    if (opts && opts.split) {
      if (opts.targets.includes("spv")) h += dlItem("oish-spv", `${esc(base)}.spv.oiSH`, "--split · lean SPIR-V-only file");
      if (opts.targets.includes("dxil")) h += dlItem("oish-dxil", `${esc(base)}.dxil.oiSH`, "--split · lean DXIL-only file");
    } else {
      h += dlItem("oish", `${esc(base)}.oiSH`, "SHFile_write — full file (all backends)");
      if (!doc.flags.reflectionOnly) {
        h += dlItem("oish-spv", `${esc(base)}.spv.oiSH`, "per-backend lean file (planned `file split`)");
        h += dlItem("oish-dxil", `${esc(base)}.dxil.oiSH`, "per-backend lean file (planned `file split`)");
      }
    }
    if (state.binRowsArr.length) {
      h += `<li><hr class="dropdown-divider"></li>`;
      h += dlItem("bin", `selected binary (.spv/.dxil)`, "file data --bin -entry N -compile-output …");
      h += dlItem("txt", `selected disassembly (.txt)`, "shader disassemble of the selected binary");
      if (state.isa.result) h += dlItem("isa", `selected ISA · ${esc(state.isa.result.asic)} (.txt)`, `isa disassemble -asic ${esc(state.isa.result.asic)} -output …`);
    }
  }
  if (state.mode === "compile" && state.symbols) {
    h += `<li><hr class="dropdown-divider"></li>`;
    h += dlItem("oisr", `${esc(state.symbols.name)}`, "shader reflect-symbols -output · the frontend symbol AST");
  } else if (inspectKind === "oisr" && state.oisrDocs[state.inspected]) {
    h += dlItem("oisr", `${esc(state.inspected)}`, "SRFile_write — re-serialize the loaded oiSR");
  }
  const sp = inspectKind === "oisp" ? state.oispDocs[state.inspected] : state.pipeline.sp;
  if (sp) {
    h += `<li><hr class="dropdown-divider"></li>`;
    h += dlItem("oisp", `${esc(sp.name)}`, inspectKind === "oisp" ? "SPFile_write — re-serialize the loaded oiSP" : "-pso-output · the pipeline a live ISA run compiles, with provenance");
  }
  menu.innerHTML = h;
  if (h) $("#dlBtn").removeAttribute("disabled"); else $("#dlBtn").setAttribute("disabled", "");
}
$("#dlMenu").addEventListener("click", async e => {
  const btn = e.target.closest("[data-act]"); if (!btn) return;
  const act = btn.dataset.act;
  if (act === "oisr") {
    const sr = state.mode === "compile" ? state.symbols : state.oisrDocs[state.inspected];
    if (sr) download(sr.name, await window.OxAPI.writeOiSR(sr), "application/octet-stream");
    return;
  }
  if (act === "oisp") {
    const sp = state.mode === "oish" && state.inspectedKind === "oisp" ? state.oispDocs[state.inspected] : state.pipeline.sp;
    if (sp) download(sp.name, await window.OxAPI.writeOiSP(sp), "application/octet-stream");
    return;
  }
  const doc = currentDoc(); if (!doc) return;
  const base = doc.name.replace(/\.oiSH$/i, "").replace(/\.hlsl$/i, "");
  if (doc.standalone) {
    const entry = state.standalone[state.binActive];
    if (act === "bin") download(doc.name, entry.bytes, "application/octet-stream");
    else download(doc.name.replace(/\.(spv|dxil)$/i, "") + ".txt", entry.text || await window.OxAPI.disassemble(entry.type, entry.bytes), "text/plain");
    return;
  }
  if (act === "oish") download(base + ".oiSH", await window.OxAPI.writeOiSH(doc), "application/octet-stream");
  else if (act === "oish-spv") download(base + ".spv.oiSH", await window.OxAPI.writeOiSH(doc, { backend: "spirv" }), "application/octet-stream");
  else if (act === "oish-dxil") download(base + ".dxil.oiSH", await window.OxAPI.writeOiSH(doc, { backend: "dxil" }), "application/octet-stream");
  else {
    const row = state.binRowsArr[+$("#binSel").value] || state.binRowsArr[0]; if (!row) return;
    const bin = doc.binaries[row.binIdx];
    const stem = `${base}.${bin.lib ? "lib" + row.binIdx : bin.entrypoint}`;
    if (act === "bin")
      download(`${stem}.${row.backend === "spirv" ? "spv" : "dxil"}`, await window.OxAPI.extractBinary(doc, bin, row.backend), "application/octet-stream");
    else if (act === "isa" && state.isa.result)
      download(`${stem}.${state.isa.result.asic}.isa.txt`, state.isa.result.text, "text/plain");
    else
      download(`${stem}.${row.backend}.txt`, await window.OxAPI.disassembleDocBinary(doc, bin, row.backend), "text/plain");
  }
});

/* ------------------------------------------------------------------ problems */

function renderProblems(diags) {
  const body = $("#probBody");
  $("#probCount").textContent = diags.length
    ? `${diags.filter(d => d.sev === "error").length} errors · ${diags.filter(d => d.sev === "warn").length} warnings`
    : "no problems";
  if (!diags.length) { body.innerHTML = `<div class="text-body-secondary small p-3">No problems — nice.</div>`; return; }
  body.innerHTML = diags.map((d, i) => `
    <div class="diag diag-${d.sev}" data-i="${i}">
      <i class="bi ${d.sev === "error" ? "bi-x-circle-fill" : d.sev === "warn" ? "bi-exclamation-triangle-fill" : "bi-info-circle-fill"}"></i>
      <span class="diag-loc">${esc(d.file || state.active)}:${d.line}:${d.ch0 + 1}</span>
      <span class="diag-msg">${esc(d.msg)}</span><span class="diag-code">${esc(d.code || "")}</span>
    </div>`).join("");
  body.dataset.count = diags.length;
  body._diags = diags;
}
$("#probBody").addEventListener("click", e => {
  const row = e.target.closest(".diag"); if (!row) return;
  const d = ($("#probBody")._diags || [])[+row.dataset.i]; if (!d) return;
  if (d.file && d.file !== state.active) openFile(d.file);
  window.OxEditor.gotoDiag(d);
});
$("#probToggle").addEventListener("click", () => document.body.classList.toggle("prob-collapsed"));
$("#failProblemsLink").addEventListener("click", e => { e.preventDefault(); document.body.classList.remove("prob-collapsed"); });
$("#stProblems").addEventListener("click", () => document.body.classList.remove("prob-collapsed"));

/* ------------------------------------------------------------------ tree collapse (reflection + symbols) */

$("#tabContent").addEventListener("click", e => {
  if (e.target.closest("[data-goto]")) return;
  const n = e.target.closest(".tnode.exp");
  if (n) n.parentElement.classList.toggle("collapsed");
});

/* ------------------------------------------------------------------ share / theme */

$("#shareBtn").addEventListener("click", () => {
  const o = window.OxCompile.opts();
  const payload = { m: state.mode, f: state.active, src: state.files[state.active] ? state.files[state.active].src : "", o };
  location.hash = "s=" + btoa(unescape(encodeURIComponent(JSON.stringify(payload))));
  navigator.clipboard && navigator.clipboard.writeText(location.href).catch(() => {});
  toast("Permalink copied to the clipboard (state is encoded in the URL hash).", "success");
});
function restoreFromHash() {
  const m = location.hash.match(/s=([^&]+)/); if (!m) return false;
  try {
    const p = JSON.parse(decodeURIComponent(escape(atob(m[1]))));
    if (p.f && p.src != null) { state.files[p.f] = state.files[p.f] || { diags: [] }; state.files[p.f].src = p.src; state.active = p.f; }
    if (p.o) {
      ({ spv: $("#tSpv"), dxil: $("#tDxil") }[p.o.targets && p.o.targets.length === 1 ? p.o.targets[0] : "x"] || $("#tBoth")).checked = true;
      $("#optRefl").checked = !!p.o.reflectionOnly; $("#optDebug").checked = !!p.o.debug; $("#optSplit").checked = !!p.o.split;
      $("#optKeepReg").checked = !!p.o.keepRegisters;
      $("#wUnusedReg").checked = !!p.o.warnUnusedRegisters; $("#wUnusedConst").checked = !!p.o.warnUnusedConstants;
      $("#wPad").checked = !!p.o.warnBufferPadding; $("#wIgnoreEmpty").checked = !!p.o.ignoreEmptyFiles;
    }
    return true;
  } catch { return false; }
}

$("#themeBtn").addEventListener("click", () => {
  const el = document.documentElement;
  const dark = el.getAttribute("data-bs-theme") !== "dark";
  el.setAttribute("data-bs-theme", dark ? "dark" : "light");
  window.OxEditor.setTheme(dark);
});

/* ------------------------------------------------------------------ CLI ↔ web map */

const OxCliMap = [
  { c: "OxC3 shader compile -input x.hlsl -output x.oiSH [-compile-output spv|dxil|all] [--debug] [--split] [--keep-registers] [--warn-*] [--ignore-empty-files] [-threads N] [-include-dir d]",
    w: "Compile mode: toolbar switches map 1:1; the Command tab shows the exact line for the current run. Include roots = the file tree.", s: "wired (mock)" },
  { c: "OxC3 compile shaders -format HLSL …", w: "Same as above — the pre-`shader`-category spelling.", s: "wired (mock)" },
  { c: "OxC3 shader reflect -input x.hlsl -output x.oiSH", w: "Compile mode → “Reflection only” switch (binaries stripped, ESHSettingsFlags_ReflectionOnly).", s: "wired (mock)" },
  { c: "OxC3 shader reflect-symbols -input x.hlsl [-output x.oiSR] [-include-dir d] [--verbose]",
    w: "Symbols tab: the frontend symbol AST (entrypoints, user types, resources, parameters, locals) with source locations → go-to-definition into the editor; built-in include symbols collapsed per include. Download menu → .oiSR.", s: "wired (mock)" },
  { c: "OxC3 shader entrypoints -input x.oiSH [--verbose]", w: "oiSH tab → Entrypoints card; full per-entry detail in the Reflection tree (IO, wave sizes, payload, binary refs).", s: "wired (mock)" },
  { c: "OxC3 shader includes -input x.oiSH", w: "oiSH tab → Includes card (path + CRC32C).", s: "wired (mock)" },
  { c: "OxC3 shader feature_set -input x.oiSH", w: "oiSH tab → Feature set table (extensions with dormant struck through, vendors, models, sizes).", s: "wired (mock)" },
  { c: "OxC3 file header -input x.oiSH|x.oiSR|x.oiSP", w: "oiSH tab → header card; Symbols tab → header strip (nodes, features); Pipeline tab → header card (pipeline/stage/specialization counts, stored blend/vertex entries).", s: "wired (mock)" },
  { c: "OxC3 file data -input x.oiSH [--bin] [-entry N] [-compile-output spv|dxil] [--includes] [-start/-length] [-output f]",
    w: "Reflection tree = the full print; Binary strip + SPIR-V/DXIL tabs = --bin -entry N; Download menu = -output.", s: "wired (mock)" },
  { c: "OxC3 file data -input x.oiSR [--includes]", w: "Symbols tab on a loaded .oiSR (Inspect mode); --includes expands the collapsed built-in symbols.", s: "wired (mock)" },
  { c: "OxC3 file data -input x.oiSP", w: "Pipeline tab: every pipeline, its stages (oiSH + entrypoint + source hash) and the full state with each field's provenance (derived / supplied / assumed).", s: "wired (mock)" },
  { c: "OxC3 file data -input x.oiSH -asic gfx1100 [-entry N]", w: "ISA tab on a binary of an oiSH: the same offline AMD ISA view inline (-asic implies --bin + SPIR-V).", s: "wired (mock)" },
  { c: "OxC3 isa devices", w: "ISA tab → the -asic dropdown: gfx11xx (RDNA3), gfx1150 (RDNA3.5), gfx12xx (RDNA4), what the bundled amdllpc accepts.", s: "wired (mock)" },
  { c: "OxC3 isa disassemble -input x.spv|x.oiSH -asic gfxNNNN [-entry N] [-output x.txt]",
    w: "ISA tab → Disassemble: SPIR-V → ELF (amdllpc) → ISA text (amdgpu-dis) with the SGPR/VGPR/code/scratch/LDS line; raster, compute and mesh stages only (ray tracing and DXIL have no offline path). Download menu → .isa.txt.", s: "wired (mock)" },
  { c: "OxC3 isa disassemble -input x.oiSH -asic live[:i] [-entry N] [-pso-set \"path=value,..\"] [-pso-input x.oiSP] [-pso-output x.oiSP] [--assume-defaults]",
    w: "ISA tab → “live”: a real Vulkan device via VK_KHR_pipeline_executable_properties (cross-vendor statistics; ISA text from AMD + Mesa only). The Pipeline tab and the ISA tab's <i>Pipeline state</i> panel ARE the state it compiles: derived with provenance, refused while anything is assumed; every field you change becomes a -pso-set entry on the CLI line (the only override there is: no per-field flags, so one vocabulary), Download → .oiSP is what -pso-input replays. Greyed out in a browser (no device); runs for real when the page is hosted in VS Code with the native OxC3.", s: "native only" },
  { c: "OxC3 shader disassemble -input x.spv|x.dxil [-output x.txt]", w: "SPV/DXIL mode → the SPIR-V / DXIL tab of a loaded binary (also powers the binary views + binary diff); Download → .txt.", s: "wired (mock)" },
  { c: "OxC3 shader assemble -input x.txt -output x.spv", w: "SPV/DXIL mode → the strip's Assemble card; the result is a loaded binary like any other. SPIR-V only — DXIL assembly is not supported yet (matches the CLI).", s: "wired (mock)" },
  { c: "reflect a bare .spv / .dxil (spirv-reflect / DXC container reflection; no CLI verb yet)", w: "SPV/DXIL mode: a loaded binary is reflected from its bytes into a one-binary document, so Reflection, ISA, Pipeline and Diff A↔B apply to it; it has no identifier until assembled.", s: "planned" },
  { c: "shader assemble → oiSH (planned: -stage, -model, -extensions, -vendors, a previous oiSH to merge into)", w: "SPV/DXIL mode → oiSH tab → “Assemble into oiSH”: gives a bare binary the identifier its source would have declared; the result lands in Inspect mode as a real oiSH (Combine with the other backend's file, pipelines, downloads).", s: "planned" },
  { c: "raw DXC: dxc <any flags> x.hlsl (no CLI verb yet; the wasm entry is getCompileArgs + a raw compile)", w: "Compile mode → Command tab: every derived DXC line is editable and runnable (“Run with DXC”), plus a free line. Output is a standalone binary in SPV/DXIL mode, never an oiSH: no annotations processed, nothing reflected into an identifier.", s: "planned" },
  { c: "OxC3 file combine -format oiSH -input a.oiSH -input2 b.oiSH -output c.oiSH",
    w: "Inspect mode → “Combine A+B” (requires same source hash / includes / settings; errors surface as a toast).", s: "wired (mock)" },
  { c: "OxC3 file split -format oiSH …", w: "Download menu → per-backend lean files (x.spv.oiSH / x.dxil.oiSH). The CLI verb itself is still marked TODO upstream.", s: "planned" },
  { c: "Compiler_getUniqueEntrypoints (no CLI verb yet)", w: "SPV/DXIL mode → “List entrypoints” on a binary.", s: "wired (mock)" },
  { c: "device-free ISA: Mesa RADV/ACO (all AMD, gfx6 to gfx12) + Intel brw (Gen9 to Xe2)", w: "The route that can run in the browser without a device or a spawned process. Prototyped (SPIR-V → ISA + stats for compute/graphics/RT), not in the CLI yet; amdllpc stays the shipped offline path. Adreno / Mali offline compilers stay manual.", s: "prototype" },
  { c: "OxC3 package -input dir -output dir [-aes key]", w: "Bakes a virtual dir (incl. compiling all shaders) into oiCA — out of scope for this page; belongs to tool.oxsomi.com.", s: "n/a here" }
];
function fillCliRef() {
  $("#cliRefBody").innerHTML = `<p class="text-body-secondary small">Every shader-related CLI capability in core3 and where it lives on this page.
    “wired (mock)” = the UI flow exists and calls <code>js/api.js</code>, which fakes the result until the WASM port lands; “native only” = needs a device or a process the browser can't give it.</p>` +
    OxCliMap.map(e => `<div class="row-item">
      <span class="cli">${esc(e.c)}</span>
      <div class="desc mt-1">${e.w}</div>
      <span class="badge ${e.s.startsWith("wired") ? "text-bg-success" : e.s === "planned" || e.s === "prototype" ? "text-bg-secondary" : e.s === "native only" ? "text-bg-warning" : "text-bg-dark"} mt-1">${esc(e.s)}</span>
    </div>`).join("");
}

/* ------------------------------------------------------------------ init */

document.addEventListener("DOMContentLoaded", async () => {
  await window.OxAPI.init();
  state.caps = await window.OxAPI.capabilities();
  state.isa.targets = await window.OxAPI.isaTargets();
  if (state.caps.host !== "browser") document.querySelector(".navbar-brand").insertAdjacentHTML("beforeend", ` <span class="badge text-bg-info">${esc(state.caps.host)} host</span>`);
  window.OxEditor.init();
  window.OxEditor.onChange(src => {
    if (state.activeBuiltin) return;
    state.files[state.active].src = src;
    scheduleCommands(); scheduleSymbols();
  });
  window.OxCompile.wire(() => { scheduleCommands(); if (state.compiled) buildDlMenu(); }, onRawDxc);
  window.OxDiff.initBinaryDiffControls();
  fillCliRef();
  document.body.classList.add("prob-collapsed");

  restoreFromHash();
  setMode("compile");
  openFile(state.files[state.active] ? state.active : Object.keys(state.files)[0]);
});
})();
