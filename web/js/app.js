/* app.js: state + orchestration. Owns the rail, the three modes, uploads/downloads,
 * the problems panel, share permalinks and the CLI↔web map. All backend work goes
 * through OxAPI (see js/api.js for the wasm porting notes). */
(function () {
"use strict";
const { $, $$, esc, fmtBytes, download, debounce, checkSnapshotBytes, checkSnapshotFiles } = window.OxUtil;
const M = window.OxMock;

/* ------------------------------------------------------------------ state */

const state = {
  mode: "compile",                       // compile | oish | bins
  files: JSON.parse(JSON.stringify(M.SAMPLE_FILES)),
  active: "lighting.hlsl",
  activeBuiltin: null,                   // name when a read-only @builtin is open
  tabs: [],                              // open editor tabs: {name, builtin}; active is state.active/activeBuiltin
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
  binRowsArr: [],                        // rows currently in #binSel
  builtins: { ...M.BUILTINS },           // "@name" -> source; the module serves the real ones once it loads
  symOpts: { verbose: false, builtins: false, hlslTypes: false },  // Symbols display switches, kept across re-reflects
  diagEpoch: 0,                          // bumped whenever a compile draws its diagnostics, see refreshSymbols
  parsed: null,                          // {name, entries}: the active file's parse-only entrypoint listing
  reflectRows: [],                       // rows currently in #reflectFollow (see reflectFollowOptions)
  reflectPickByFile: {},                 // file -> picked row key, so a pick survives a file switch and a reparse
  reflectBackend: "dxil"                 // which leg's view IntelliSense reflects (#reflectBackend)
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
    $("#railTitle").textContent = (window.OxWorkspace.current() || {}).name || "Project";
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
    $("#builtinFiles").innerHTML = Object.keys(state.builtins)
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
      return fitem("oish" + act(n, "oish"), "bi-box-seam", n, `${d.entries.length}·SM${d.model}`, `${n}: ${d.binaries.length} binaries`);
    }).join("");
    const sr = Object.keys(state.oisrDocs);
    if (sr.length) h += railGroup("oiSR · symbol ASTs") + sr.map(n => {
      const d = state.oisrDocs[n];
      return fitem("oisr" + act(n, "oisr"), "bi-diagram-2", n, `${d.header.counts.nodes} nodes`, `${n}: ${d.header.counts.registers} registers`);
    }).join("");
    const sp = Object.keys(state.oispDocs);
    if (sp.length) h += railGroup("oiSP · pipelines") + sp.map(n => {
      const d = state.oispDocs[n];
      return fitem("oisp" + act(n, "oisp"), "bi-diagram-3", n, d.pipelines.map(p => p.type).join(","), `${n}: ${d.header.counts.pipelines} pipeline(s)`);
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
  ensureTab(name, false);
  $("#roBadge").classList.add("d-none");
  window.OxEditor.open(state.files[name].src, false, "f:" + name);
  reflectFollowOptions();
  refreshParsedEntries();                //the picker refills from this file's own parse listing
  if (state.compiled) window.OxEditor.markDiags(state.compiled.diags.filter(d => d.file === name));
  renderRail(); renderTabs(); scheduleCommands();
  refreshSymbols(true);                  //a switch reflects now; the debounce is for typing, not this
}
function openBuiltin(name) {
  state.activeBuiltin = name;
  ensureTab(name, true);
  $("#roBadge").classList.remove("d-none");
  window.OxEditor.open(state.builtins[name], true, "b:" + name);
  refreshSymbols();                      //the outline and hover follow the builtin, read-only or not
  renderRail(); renderTabs();
}

/* Editor tabs. Opening from the rail (or a ctrl+click include jump) adds a tab; the strip switches
 * between them. state.active/activeBuiltin stay the one source of truth for what the editor shows,
 * so squiggles, symbols and share, all keyed on them, need no notion of tabs. */

function ensureTab(name, builtin) {
  if (!state.tabs.some(t => t.name === name && t.builtin === !!builtin))
    state.tabs.push({ name, builtin: !!builtin, pinned: false });
}

function findTab(name, builtin) {
  return state.tabs.find(t => t.name === name && t.builtin === !!builtin);
}

/* Pinned tabs live at the front, survive close-all/close-others, and lose their inline close
 * button; the context menu (right-click) is where pinning and bulk closing happen. */
function pinTab(name, builtin, pinned) {
  const t = findTab(name, builtin);
  if (!t) return;
  t.pinned = pinned;
  state.tabs = [...state.tabs.filter(x => x !== t)];
  const at = state.tabs.filter(x => x.pinned).length;
  state.tabs.splice(at, 0, t);
  renderTabs();
}

function closeTabs(keep) {

  for (const t of state.tabs)
    if (!t.pinned && !keep(t)) window.OxEditor.closeDoc((t.builtin ? "b:" : "f:") + t.name);
  state.tabs = state.tabs.filter(t => t.pinned || keep(t));

  const activeOpen = state.tabs.some(t =>
    t.builtin ? state.activeBuiltin === t.name : !state.activeBuiltin && state.active === t.name);
  if (activeOpen) return renderTabs();

  const next = state.tabs[0];
  if (next) { if (next.builtin) openBuiltin(next.name); else openFile(next.name); }
  else {
    const first = Object.keys(state.files)[0];
    if (first) openFile(first); else renderTabs();
  }
}

/* A dot marks a file edited since the last compile: its output tabs describe older text. */
function tabDirty(t) {
  const f = !t.builtin && state.files[t.name];
  return !!(f && f.editedAt && (!state.compiledAt || f.editedAt > state.compiledAt));
}

function renderTabs() {
  $("#editorTabs").innerHTML = state.tabs.map(t => {
    const active = t.builtin ? state.activeBuiltin === t.name : !state.activeBuiltin && state.active === t.name;
    return `<span class="etab${active ? " active" : ""}" draggable="true" data-name="${esc(t.name)}"` +
      ` data-builtin="${t.builtin ? 1 : 0}" title="${esc(t.name)}${tabDirty(t) ? " · edited since the last compile" : ""}">` +
      (t.pinned ? '<i class="bi bi-pin-fill"></i>' : "") +
      (t.builtin ? '<i class="bi bi-lock"></i>' : "") + esc(t.name.replace(/^.*\//, "")) +
      (tabDirty(t) ? '<span class="etab-dot">●</span>' : "") +
      (t.pinned ? "" : `<button class="etab-x" title="Close (middle-click also closes)">×</button>`) + "</span>";
  }).join("");
}

function closeTab(name, builtin) {

  const i = state.tabs.findIndex(t => t.name === name && t.builtin === builtin);
  if (i < 0) return;

  const wasActive = builtin ? state.activeBuiltin === name : !state.activeBuiltin && state.active === name;
  state.tabs.splice(i, 1);
  window.OxEditor.closeDoc((builtin ? "b:" : "f:") + name);

  if (!wasActive) return renderTabs();

  const next = state.tabs[Math.min(i, state.tabs.length - 1)];
  if (next) { if (next.builtin) openBuiltin(next.name); else openFile(next.name); }
  else {
    const first = Object.keys(state.files)[0];
    if (first) openFile(first); else renderTabs();
  }
}

$("#editorTabs").addEventListener("click", e => {
  const tab = e.target.closest(".etab"); if (!tab) return;
  const name = tab.dataset.name, builtin = tab.dataset.builtin === "1";
  if (e.target.closest(".etab-x")) return closeTab(name, builtin);
  if (builtin) openBuiltin(name); else openFile(name);
});
$("#editorTabs").addEventListener("auxclick", e => {
  const tab = e.target.closest(".etab");
  if (tab && e.button === 1) closeTab(tab.dataset.name, tab.dataset.builtin === "1");
});

/* Drag to reorder; a drop re-sorts pinned-first so the pinned group stays at the front. */
let dragTab = null;
$("#editorTabs").addEventListener("dragstart", e => {
  const tab = e.target.closest(".etab");
  if (tab) dragTab = { name: tab.dataset.name, builtin: tab.dataset.builtin === "1" };
});
$("#editorTabs").addEventListener("dragover", e => { if (dragTab) e.preventDefault(); });
$("#editorTabs").addEventListener("drop", e => {
  const over = e.target.closest(".etab");
  if (!dragTab || !over) { dragTab = null; return; }
  e.preventDefault();
  const from = state.tabs.findIndex(t => t.name === dragTab.name && t.builtin === dragTab.builtin);
  const to = state.tabs.findIndex(t => t.name === over.dataset.name && t.builtin === (over.dataset.builtin === "1"));
  dragTab = null;
  if (from < 0 || to < 0 || from === to) return;
  const [moved] = state.tabs.splice(from, 1);
  state.tabs.splice(to, 0, moved);
  state.tabs = [...state.tabs.filter(t => t.pinned), ...state.tabs.filter(t => !t.pinned)];
  renderTabs();
});

/* A mouse's vertical wheel is the only wheel most have; over the strip it scrolls the tabs. */
$("#editorTabs").addEventListener("wheel", e => {
  if (!e.deltaY) return;
  e.preventDefault();
  $("#editorTabs").scrollLeft += e.deltaY;
}, { passive: false });

function closeTabMenu() { const m = $("#etabMenu"); if (m) m.remove(); }

$("#editorTabs").addEventListener("contextmenu", e => {

  const tab = e.target.closest(".etab"); if (!tab) return;
  e.preventDefault();
  closeTabMenu();

  const name = tab.dataset.name, builtin = tab.dataset.builtin === "1";
  const t = findTab(name, builtin); if (!t) return;

  const menu = document.createElement("ul");
  menu.id = "etabMenu";
  menu.className = "dropdown-menu show";
  menu.style.cssText = `position:fixed; left:${e.clientX}px; top:${e.clientY}px; z-index:2000`;
  menu.innerHTML =
    `<li><button class="dropdown-item small" data-act="pin"><i class="bi ${t.pinned ? "bi-pin-angle" : "bi-pin-fill"}"></i> ${t.pinned ? "Unpin" : "Pin"}</button></li>
     <li><hr class="dropdown-divider"></li>
     <li><button class="dropdown-item small" data-act="close">Close</button></li>
     <li><button class="dropdown-item small" data-act="others">Close others</button></li>
     <li><button class="dropdown-item small" data-act="all">Close all</button></li>`;

  menu.addEventListener("click", ev => {
    const act = (ev.target.closest("[data-act]") || {}).dataset;
    closeTabMenu();
    if (!act) return;
    if (act.act === "pin") pinTab(name, builtin, !t.pinned);
    else if (act.act === "close") closeTab(name, builtin);
    else if (act.act === "others") {
      if (t.builtin) openBuiltin(name); else openFile(name);
      closeTabs(x => x === t);
    }
    else if (act.act === "all") closeTabs(() => false);
  });

  document.body.appendChild(menu);
  setTimeout(() => document.addEventListener("click", closeTabMenu, { once: true }), 0);
});

/* ------------------------------------------------------------------ workspaces */

/* Every project on this page is a workspace (js/workspace.js): the working one auto-saves debounced,
 * a share link or snapshot import arrives as its OWN workspace, and the menu on the rail header
 * switches between them. The payload shape is the share payload's ({files, f, o}), so one decoder
 * serves both. */

function applyOptions(o) {
  ({ spv: $("#tSpv"), dxil: $("#tDxil") }[o.targets && o.targets.length === 1 ? o.targets[0] : "x"] || $("#tBoth")).checked = true;
  $("#optRefl").checked = !!o.reflectionOnly; $("#optDebug").checked = !!o.debug; $("#optSplit").checked = !!o.split;
  $("#optNoOpt").checked = !!o.noOpt;
  $("#optKeepReg").checked = !!o.keepRegisters;
  $("#wUnusedReg").checked = !!o.warnUnusedRegisters; $("#wUnusedConst").checked = !!o.warnUnusedConstants;
  $("#wPad").checked = !!o.warnBufferPadding; $("#wIgnoreEmpty").checked = !!o.ignoreEmptyFiles;
}

/* Hands a plain {name: src} map to the page as the working project; tabs, docs and stale compile
 * output all reset, since they belonged to the previous one. */
function applyProject(files, active, o) {

  state.files = {};
  for (const [name, src] of Object.entries(files || {}))
    if (typeof src === "string") state.files[name] = { src, diags: [] };

  state.tabs = []; window.OxEditor.resetDocs();
  state.compiled = null; state.symbols = null;
  state.pipeline = { sp: null, refused: null, pick: {} };

  if (o) applyOptions(o);

  state.active = active && state.files[active] ? active :
    Object.keys(state.files).find(n => /\.hlsl$/i.test(n)) || Object.keys(state.files)[0];

  renderCompiled(); renderProblems([]);
}

let wsQuotaWarned = false;

async function saveWorkspaceNow() {

  if (!window.OxWorkspace.currentId()) return;

  const files = Object.fromEntries(Object.entries(state.files).map(([n, f]) => [n, f.src == null ? "" : f.src]));
  const ok = await window.OxWorkspace.save({ files, f: state.active, o: window.OxCompile.opts() });

  if (!ok && !wsQuotaWarned) {
    wsQuotaWarned = true;
    toast("Storage is full, so this project can't auto-save. Delete or unpin old workspaces, or keep it as an .oiCA download.", "warning");
  }
  renderWsMenu();
}

const scheduleWsSave = debounce(saveWorkspaceNow, 1500);

function touchWorkspace() { window.OxWorkspace.markDirty(); scheduleWsSave(); }

async function bootWorkspace() {

  const cur = window.OxWorkspace.currentId();

  if (cur) {
    const payload = await window.OxWorkspace.load(cur);
    if (payload) { applyProject(payload.files, payload.f, payload.o); return; }
    window.OxWorkspace.remove(cur);      //its data was evicted or cleared; the meta alone is useless
  }

  //First visit: the samples become the first workspace, so there is always something to come back to.
  window.OxWorkspace.switchTo(window.OxWorkspace.create("samples", "local").id);
  saveWorkspaceNow();
}

async function switchWorkspace(id) {

  if (id === window.OxWorkspace.currentId()) return;

  await saveWorkspaceNow();
  const payload = await window.OxWorkspace.load(id);
  if (!payload) return toast("That workspace's data is gone (evicted, or browser storage was cleared).", "warning");

  window.OxWorkspace.switchTo(id);
  applyProject(payload.files, payload.f, payload.o);
  renderRail(); renderWsMenu();
  openFile(state.active);
}

function renderWsMenu() {

  const cur = window.OxWorkspace.currentId();
  const list = window.OxWorkspace.list().sort((a, b) => b.updated - a.updated);
  const curMeta = list.find(w => w.id === cur);

  $("#railTitle").textContent = curMeta ? curMeta.name : "Project";

  $("#wsMenu").innerHTML = list.map(w => `
    <li class="d-flex align-items-center pe-2 ws-row" data-ws="${esc(w.id)}">
      <button class="dropdown-item small text-truncate d-flex align-items-center gap-2" data-open style="min-width:0">
        <i class="bi ${w.kind === "shared" ? "bi-share" : w.kind === "import" ? "bi-box-arrow-in-down" : "bi-folder2"}"></i>
        <span class="text-truncate">${esc(w.name)}</span>
        ${w.id === cur ? '<i class="bi bi-check ms-auto"></i>' : ""}
      </button>
      <button class="btn btn-sm btn-link p-0 px-1 ${w.pinned ? "" : "text-body-secondary"}" data-pin
        title="${w.pinned ? "Pinned: never auto-evicted. Click to unpin." : "Pin: protect from auto-eviction when storage fills."}">
        <i class="bi ${w.pinned ? "bi-pin-fill" : "bi-pin-angle"}"></i></button>
      <button class="btn btn-sm btn-link p-0 px-1 text-body-secondary" data-del title="Delete workspace">
        <i class="bi bi-x-lg"></i></button>
    </li>`).join("") +
    `<li><hr class="dropdown-divider"></li>
     <li><button class="dropdown-item small" id="wsNew"><i class="bi bi-plus-lg"></i> New workspace (from the samples)</button></li>
     <li><button class="dropdown-item small" id="wsRename"><i class="bi bi-pencil"></i> Rename current</button></li>
     <li class="px-3 pt-1 small text-body-secondary" style="max-width:260px">Workspaces live in this browser's storage;
       the durable form of a project is an .oiCA download.</li>`;
}

/* The rail scrolls (overflow:auto), which would clip an absolutely positioned dropdown; the fixed
 * strategy positions the menu against the viewport instead. auto-close=outside keeps it open for
 * pin/rename/delete housekeeping, so the actions that change the open project close it themselves. */
if (window.bootstrap && window.bootstrap.Dropdown)
  new bootstrap.Dropdown($("#wsBtn"), { popperConfig: c => ({ ...c, strategy: "fixed" }) });

function hideWsMenu() {
  if (!(window.bootstrap && window.bootstrap.Dropdown)) return;
  const d = bootstrap.Dropdown.getInstance($("#wsBtn"));
  if (d) d.hide();
}

$("#wsMenu").addEventListener("click", async e => {

  if (e.target.closest("#wsNew")) {
    hideWsMenu();
    await saveWorkspaceNow();
    window.OxWorkspace.switchTo(window.OxWorkspace.create("workspace " + (window.OxWorkspace.list().length), "local").id);
    applyProject(Object.fromEntries(Object.entries(M.SAMPLE_FILES).map(([n, f]) => [n, f.src])));
    renderRail(); renderWsMenu(); openFile(state.active);
    saveWorkspaceNow();
    return;
  }

  if (e.target.closest("#wsRename")) {
    const cur = window.OxWorkspace.current();
    const name = cur && prompt("Workspace name:", cur.name);
    if (name) { window.OxWorkspace.rename(cur.id, name); renderWsMenu(); }
    return;
  }

  const row = e.target.closest(".ws-row"); if (!row) return;
  const id = row.dataset.ws;

  if (e.target.closest("[data-pin]")) {
    const w = window.OxWorkspace.list().find(x => x.id === id);
    window.OxWorkspace.pin(id, !w.pinned); renderWsMenu();
    return;
  }

  if (e.target.closest("[data-del]")) {
    const w = window.OxWorkspace.list().find(x => x.id === id);
    if (!confirm(`Delete workspace "${w ? w.name : id}"? This cannot be undone.`)) return;
    const wasCurrent = id === window.OxWorkspace.currentId();
    window.OxWorkspace.remove(id);
    if (wasCurrent) {
      hideWsMenu();
      const next = window.OxWorkspace.list().sort((a, b) => b.updated - a.updated)[0];
      if (next) { await switchWorkspace(next.id); return; }
      window.OxWorkspace.switchTo(window.OxWorkspace.create("samples", "local").id);
      applyProject(Object.fromEntries(Object.entries(M.SAMPLE_FILES).map(([n, f]) => [n, f.src])));
      renderRail(); openFile(state.active);
      saveWorkspaceNow();
    }
    renderWsMenu();
    return;
  }

  if (e.target.closest("[data-open]")) { hideWsMenu(); switchWorkspace(id); }
});

$("#addFile").addEventListener("click", () => {
  const name = prompt("New file (e.g. sky.hlsl or include/util.hlsli):"); if (!name) return;
  if (state.files[name]) return toast("File already exists.", "warning");
  state.files[name] = { src: `#include "@types.hlsli"\n`, diags: [] };
  touchWorkspace();
  openFile(name);
});
$("#addFolder").addEventListener("click", () => {
  const name = prompt("New folder name:"); if (!name) return;
  state.files[name.replace(/\/$/, "") + "/.keep.hlsli"] = { src: "#pragma once\n", diags: [] };
  touchWorkspace();
  renderRail();
});

/* ------------------------------------------------------------------ modes */

const tabItem = t => document.querySelector(`#outTabs [data-tab="${t}"]`);
const clickTab = t => document.querySelector(`[data-bs-target="#p-${t}"]`).click();
const activeTab = () => {
  const on = document.querySelector("#outTabs .nav-link.active");
  return on ? on.closest("[data-tab]").dataset.tab : null;
};

/* Which views can answer for what is open.
 *
 * A tab that can't is hidden rather than left to fail: an oiSR carries no binaries, so there is no
 * disassembly, pipeline or reflection to show for it, and a standalone binary has no oiSH behind it
 * for a pipeline to be derived from. The binary tabs appear per backend, since a lean file holds one.
 * The order is the order they sit in the bar, so the first visible one is the natural landing tab. */
const ALL_TABS = ["refl", "sym", "spv", "dxil", "isa", "pso", "cmd", "diff", "oish"];

/* The tabs the binary picker applies to: it chooses which binary the disassembly and the ISA show. */
const BINARY_TABS = ["spv", "dxil", "isa"];

function visibleTabs() {

  const doc = currentDoc();
  const has = backend => !!(doc && doc.binaries && doc.binaries.some(b => b.sizes && b.sizes[backend]));
  const binaries = [...(has("spirv") ? ["spv"] : []), ...(has("dxil") ? ["dxil"] : [])];

  if (state.mode === "compile")
    return ["refl", "sym", ...binaries, "isa", "pso", "cmd", "diff", "oish"];

  /* A standalone binary has no oiSH, so no pipeline can be derived from it; the oiSH tab is where it
   * offers to become one. */
  if (state.mode === "bins")
    return ["refl", ...binaries, "isa", "diff", "oish"];

  if (state.inspectedKind === "oisr") return ["sym"];
  if (state.inspectedKind === "oisp") return ["pso"];
  return ["refl", ...binaries, "isa", "pso", "diff", "oish"];
}

/* Hides what the open document can't answer, and moves off a tab that just became one of them.
 * Also drives the two strips, which are tied to a tab rather than to a mode: the binary picker only
 * means something where a binary is shown, and the A/B pair only where the differ can compare. */
function applyContext() {

  const visible = visibleTabs();
  const shown = new Set(visible);
  const active = activeTab();

  for (const t of ALL_TABS) {
    const item = tabItem(t);
    if (item) item.classList.toggle("d-none", !shown.has(t));
  }

  if (visible.length && (!active || !shown.has(active)))
    clickTab(visible[0]);

  /* The A/B pair compares two documents of one kind, so it only belongs where there are two to pick:
   * oiSH against oiSH, or binary against binary. */
  $("#oishDiffBar").classList.toggle("d-none", !(state.mode === "oish" && state.inspectedKind === "oish"));
  $("#binsDiffBar").classList.toggle("d-none", state.mode !== "bins");

  applyBinStrip();
  applyFailOverlay();
}

function applyBinStrip() {
  const on = state.binRowsArr.length > 0 && BINARY_TABS.includes(activeTab());
  $("#binStrip").classList.toggle("d-none", !on);
}

function compileFailed() {
  const c = state.compiled;
  return !!c && (!c.doc || c.diags.some(d => d.sev === "error"));
}

/* The overlay says there is no binary to look at, which is true of every tab that describes one and false
 * of Symbols: reflection runs on the source, so a file that does not compile still has an outline, and that
 * is the view most worth having when it doesn't. Covering it would hide the one tab that still works. */

function applyFailOverlay() {
  $("#failOverlay").style.display = compileFailed() && activeTab() !== "sym" ? "flex" : "none";
}

/* Entering a binary tab keeps the selector honest: a row picked for the other backend renders into the
 * other pane, so this one would sit stale or empty. The SAME binary on this tab's backend takes over,
 * so a permutation picked by hand or by the IntelliSense picker survives switching panes; only when
 * that binary has no build for this backend does the first row of the backend stand in. */
function syncBinTab(tab) {
  const backend = tab === "spv" ? "spirv" : tab === "dxil" ? "dxil" : null;
  if (!backend || !state.binRowsArr.length) return;
  const cur = state.binRowsArr[+$("#binSel").value];
  if (cur && cur.backend === backend) return;
  let i = cur ? state.binRowsArr.findIndex(r => r.backend === backend && r.binIdx === cur.binIdx) : -1;
  if (i < 0) i = state.binRowsArr.findIndex(r => r.backend === backend);
  if (i < 0) return;
  $("#binSel").value = String(i);
  showSelectedBinary(true);
}

document.querySelectorAll("#outTabs [data-bs-toggle=\"tab\"]").forEach(
  b => b.addEventListener("shown.bs.tab", e => {
    applyBinStrip(); applyFailOverlay();
    const item = e.target.closest("[data-tab]");
    if (item) syncBinTab(item.dataset.tab);
  })
);

/* The documents Inspect mode and SPV / DXIL mode open on.
 *
 * With a module they are compiled for real the first time one of those modes is opened, so each one
 * has a file behind it and everything that needs one works on them. That costs a handful of compiles,
 * hence once and on demand rather than at load. Without a module the page keeps what mock_data.js
 * recorded, which is the same output with no bytes behind it. */
let examplesSeeded = false;

async function ensureExamples() {
  if (examplesSeeded) return;
  examplesSeeded = true;
  if (window.OxAPI.backend !== "wasm") return;

  /* Real compiles block the thread they run on, so say what is happening rather than looking hung. */
  const status = $("#stCompiled").innerHTML;
  $("#stCompiled").innerHTML = '<span class="spinner-border spinner-border-sm"></span> compiling the examples…';

  let seeded = null;
  try { seeded = await window.OxAPI.seedExamples(M.SAMPLE_FILES); }
  catch (err) { toast(`<b>Couldn't build the examples:</b> ${esc(err.message)}`, "warning"); }
  $("#stCompiled").innerHTML = status;
  if (!seeded) return;
  if (Object.keys(seeded.oish).length) { state.oishDocs = seeded.oish; state.inspected = null; }
  if (Object.keys(seeded.oisr).length) state.oisrDocs = seeded.oisr;
  if (Object.keys(seeded.oisp).length) state.oispDocs = seeded.oisp;
  if (Object.keys(seeded.standalone).length) { state.standalone = seeded.standalone; state.binActive = null; }
}

async function setMode(mode) {
  state.mode = mode;
  $("#compileTools").classList.toggle("d-none", mode !== "compile");
  $("#oishTools").classList.toggle("d-none", mode !== "oish");
  $("#binsTools").classList.toggle("d-none", mode !== "bins");
  $("#editorPane").classList.toggle("d-none", mode !== "compile");
  $("#binPane").classList.toggle("d-none", mode !== "bins");
  $("#problems").classList.toggle("d-none", mode !== "compile");
  renderRail();

  if (mode === "compile") {
    renderCompiled();
    renderSymbols(state.symbols);
    applyContext();
  } else if (mode === "oish") {
    await ensureExamples();
    renderRail();
    fillDiffSelects();
    if (!state.inspected) { state.inspected = Object.keys(state.oishDocs)[0] || null; state.inspectedKind = "oish"; }
    await openInspect(state.inspected, state.inspectedKind);
  } else {
    await enterBinsMode();
  }
}
async function enterBinsMode() {
  await ensureExamples();
  renderRail();
  /* every loaded binary is reflected up front so Diff A↔B can pair them like oiSH documents */
  for (const n of Object.keys(state.standalone)) await standaloneDoc(n);
  fillDiffSelects();
  if (!state.binActive || !state.standalone[state.binActive]) state.binActive = Object.keys(state.standalone)[0] || null;
  await openStandalone(state.binActive);
}
$("#mCompile").addEventListener("change", () => setMode("compile"));
$("#mOish").addEventListener("change", () => setMode("oish"));
$("#mBins").addEventListener("change", () => setMode("bins"));

/* ------------------------------------------------------------------ compile mode */

async function doCompile() {
  const res = await window.OxCompile.run(state);
  state.compiled = res;
  state.compiledAt = Date.now();
  renderTabs();                          //compiling clears the edited-since-compile dots
  state.pipeline = { sp: null, refused: null, pick: {} };
  state.isa.result = null; state.isa.error = null;
  renderCompiled();
  reflectFollowOptions();                //after the strip exists, so a picked permutation can drive it
  await refreshSymbols();
  await refreshPipeline();
}
$("#compileBtn").addEventListener("click", doCompile);

/* Where the sender of a share link was looking, applied once the shared project is open: cursor
 * first, then the output tab (compiling when the view reads compile output), then the binary row. */
async function applyPendingView() {

  const v = state.pendingView;
  if (!v) return;
  state.pendingView = null;

  if (v.line) window.OxEditor.gotoDiag({ line: v.line, ch0: 0, ch1: 0 });
  if (!v.tab) return;

  if (!state.compiled) await doCompile();

  if (v.bin != null && state.binRowsArr[v.bin]) {
    $("#binSel").value = String(v.bin);
    await showSelectedBinary(false);
  }

  clickTab(v.tab);
  if (v.line) syncAsmCursor(v.line);

  /* The exact disassembly line the sender selected, by index in the same pane. */
  if (v.aline != null && (v.tab === "spv" || v.tab === "dxil")) {
    const pane = document.querySelector(v.tab === "spv" ? "#spvAsm" : "#dxilAsm");
    const row = pane && pane.querySelectorAll(".asm-line")[v.aline];
    if (row) {
      row.classList.add("asm-sel");
      state.asmSel = { pane: v.tab, line: v.aline };
      row.scrollIntoView({ block: "center" });
    }
  }
}

function renderCompiled() {

  //A compile's diagnostics describe exactly the text it compiled, so they outrank whatever a refresh
  // already in flight read; the bump makes that refresh drop its marks instead of overwriting these.
  state.diagEpoch++;

  const c = state.compiled;
  if (!c) {
    $("#reflView").innerHTML = window.OxInspect.reflectionHTML(null);
    $("#oishView").innerHTML = window.OxInspect.oishViewHTML(null);
    window.OxDiff.renderCompileDiff(null);
    renderPipeline(); renderIsa();
    $("#stCompiled").innerHTML = '<i class="bi bi-circle"></i> not compiled';
    $("#stSizes").textContent = ""; $("#stProblems").textContent = "";
    buildDlMenu();                     //the menu decides for itself: a snapshot needs no compile
    renderProblems([]);
    applyFailOverlay();
    return;
  }
  const { doc, diags, ms, opts } = c;
  const errors = diags.filter(d => d.sev === "error").length;
  const warns = diags.filter(d => d.sev === "warn").length;

  /* No document is a failure whether or not a diagnostic was parsed out of it, and every branch below reads
   * the document, so folding it in here is what keeps them from reaching into one that was never produced. */

  const failed = errors > 0 || !doc;

  applyFailOverlay();
  window.OxEditor.markDiags(diags.filter(d => d.file === state.active));
  renderProblems(diags);

  $("#reflView").innerHTML = window.OxInspect.reflectionHTML(failed ? null : doc);
  $("#oishView").innerHTML = window.OxInspect.oishViewHTML(failed ? null : doc);
  window.OxDiff.renderCompileDiff(failed ? null : doc);
  window.OxCompile.renderCommands(state.active, state.files);

  /* binary strip: a recompile keeps the backend that was being looked at, so pressing Compile while
   * reading DXIL doesn't snap the pane back to the first (SPIR-V) row. */
  const prevRow = state.binRowsArr[+$("#binSel").value];
  state.binRowsArr = failed ? [] : window.OxInspect.binRows(doc, opts.targets);
  fillBinStrip();
  if (prevRow) {
    const same = state.binRowsArr.findIndex(r => r.label === prevRow.label);
    const backend = same < 0 ? state.binRowsArr.findIndex(r => r.backend === prevRow.backend) : same;
    if (backend >= 0) $("#binSel").value = String(backend);
  }
  if (state.binRowsArr.length) showSelectedBinary(true);
  applyContext();
  renderPipeline(); renderIsa();

  /* status + downloads */

  /* A failed compile reports no sizes because it has no binaries to size. Reading them off the document
   * anyway threw here, and the throw took the rest of doCompile with it, including the symbol refresh that
   * is exactly what a file that doesn't compile still has to show. */

  const binaries = doc ? doc.binaries : [];
  const spvTotal = binaries.reduce((s, b) => s + (opts.targets.includes("spv") ? b.sizes.spirv : 0), 0);
  const dxTotal = binaries.reduce((s, b) => s + (opts.targets.includes("dxil") ? b.sizes.dxil : 0), 0);
  $("#stCompiled").innerHTML = failed
    ? `<i class="bi bi-x-circle text-danger"></i> ${esc(state.active)} failed in ${ms} ms`
    : `<i class="bi bi-check-circle text-success"></i> ${esc(state.active)} → ${binaries.length} binaries in ${ms} ms${opts.reflectionOnly ? " (reflection only)" : ""}`;
  $("#stSizes").textContent = failed || opts.reflectionOnly ? "" :
    [opts.targets.includes("spv") ? "spv " + fmtBytes(spvTotal) : "", opts.targets.includes("dxil") ? "dxil " + fmtBytes(dxTotal) : ""].filter(Boolean).join(" · ");
  $("#stProblems").textContent = diags.length ? `${errors} error(s) · ${warns} warning(s)` : "";
  buildDlMenu();
}

/* live Command tab while typing */
const scheduleCommands = debounce(() => {
  if (state.mode === "compile" && !state.activeBuiltin)
    window.OxCompile.renderCommands(state.active, state.files);
}, 350);

/* ------------------------------------------------------------------ symbols (oiSR) */

/* `shader reflect-symbols` runs on the source, not the oiSH, so it follows the editor (debounced) rather than the compile */
/* Edits and file switches pass applyDiags, so the squiggles follow both; the epoch check inside
 * refreshSymbols is what keeps a slow refresh from overwriting a compile that landed meanwhile. */
/* Both followers need the module: without it reflection and the parse listing refuse, so they are
 * never scheduled rather than throwing once per keystroke. */
const hasModule = () => window.OxAPI.backend === "wasm";
const scheduleSymbols = debounce(applyDiags => {
  if (hasModule() && state.mode === "compile" && !state.activeBuiltin) refreshSymbols(applyDiags);
}, 500);

/* "IntelliSense follows": which permutation the reflection parses as. Default = every extension on
 * and no defines (nothing errors); following a permutation uses ITS extension set and uniforms, so
 * `half`, PAQ and friends mean what they mean in that permutation. The extension list from the
 * compiler is in enum-bit order, which is what makes the mask. */

let extEnumNames = null;

/* The compiled doc, only while it describes the ACTIVE file: a picker or a binary drive built on
 * another file's compile would apply a config that belongs to a different source. */
function freshCompiledDoc() {
  const doc = state.compiled && state.compiled.doc;
  return doc && (doc.sourceName || "").replace(/^\.\//, "") === state.active ? doc : null;
}

/* The permutations come off Compiler_parse (refreshParsedEntries), so the picker fills and refollows
 * while the file is edited, before anything compiles. Each row keeps the combination's identity, and
 * a remembered pick is restored by that identity rather than by option index: after an edit that
 * removes the picked permutation, the pick falls back to the default instead of landing on whatever
 * permutation owns that slot now. Without a listing (an old module and nothing parsed yet), the last
 * compile's binaries fill the picker the way they always did. */
function reflectFollowOptions() {

  const sel = $("#reflectFollow");
  if (!sel) return;

  const I = window.OxInspect;
  const entries = state.parsed && state.parsed.name === state.active ? state.parsed.entries : null;
  const rows = [];
  const seen = new Set();

  if (entries) {
    for (const e of entries)
      for (const c of e.combinations || []) {
        /* Model promotion can collapse two combinations onto one identifier; one row is the truth. */
        const key = I.comboKey(e.name, c);
        if (seen.has(key)) continue;
        seen.add(key);
        rows.push({ key, label: I.comboLabel(e.name, c), combo: c, entryName: e.name });
      }
  } else {
    const doc = freshCompiledDoc();
    (doc ? doc.binaries : []).forEach((b, i) => {
      const entryName = b.lib ? (b.entryNames || []).join(",") : b.entrypoint;
      rows.push({ key: I.comboKey(entryName, b), label: `#${i} ` + I.comboLabel(entryName, b), binIdx: i, combo: b, entryName });
    });
  }

  state.reflectRows = rows;
  sel.innerHTML = '<option value="">IntelliSense: all extensions, no defines</option>' +
    rows.map((r, i) => `<option value="${i}">${esc(r.label)}</option>`).join("");

  const keep = rows.findIndex(r => r.key === (state.reflectPickByFile[state.active] || ""));
  sel.value = keep >= 0 ? String(keep) : "";
  applyReflectFollow();
}

async function applyReflectFollow() {

  const sel = $("#reflectFollow");
  const row = sel && sel.value !== "" ? state.reflectRows[+sel.value] : null;
  state.reflectPickByFile[state.active] = row ? row.key : "";

  if (!row)
    state.reflectCfg = null;

  else {
    const c = row.combo;
    if (!extEnumNames) extEnumNames = (await window.OxAPI.annotationEnums()).extensions;

    let disabled = 0;
    extEnumNames.forEach((n, i) => { if (!(c.extensions || []).includes(n)) disabled |= 1 << i; });

    const defines = {};
    for (const d of (c.defines || [])) defines[d.name] = d.value == null ? "" : d.value;
    for (const u of (c.uniforms || [])) defines["$$" + u.name] = u.value;   // the macro the compile defines for it

    state.reflectCfg = { disabledExt: disabled >>> 0, defines };
  }

  driveSelectedBinary(row);
  scheduleSymbols(true);
}

$("#reflectFollow").addEventListener("change", applyReflectFollow);

$("#reflectBackend").addEventListener("change", () => {

  state.reflectBackend = $("#reflectBackend").value;

  /* The pane follows the view: reading SPIR-V and switching to the DXIL view means the DXIL of the
   * same permutation is what you asked to see. Only from an asm tab, so the switch never yanks the
   * view away from the pipeline or the ISA. */

  const tab = activeTab();

  if (tab === "spv" || tab === "dxil") {
    const want = state.reflectBackend === "spirv" ? "spv" : "dxil";
    if (want !== tab && visibleTabs().includes(want)) clickTab(want);
  }

  refreshSymbols(true);                  //a deliberate toggle reflects now, like a file switch does
});

/* The picker drives the binary strip, so the SPIR-V and DXIL views follow whichever permutation the
 * editor is on. The pick lands by identity, never by index: a pick the last compile can't answer (the
 * permutation is new, renamed or gone since) leaves the strip alone and lights the stale marker next
 * to the compile button instead of silently showing some other permutation's binary. */
function driveSelectedBinary(row) {

  const doc = freshCompiledDoc();
  let missing = false;

  if (row && doc && !compileFailed()) {

    const binIdx = row.binIdx != null ? row.binIdx : window.OxInspect.matchBinary(doc, row.combo, row.entryName);

    if (binIdx < 0)
      missing = true;

    else if (state.binRowsArr.length) {
      /* Of the matched binary's rows, the one on the backend already being looked at. */
      const cur = state.binRowsArr[+$("#binSel").value];
      let i = state.binRowsArr.findIndex(r => r.binIdx === binIdx && cur && r.backend === cur.backend);
      if (i < 0) i = state.binRowsArr.findIndex(r => r.binIdx === binIdx);
      if (i >= 0 && +$("#binSel").value !== i) {
        $("#binSel").value = String(i);
        binSelectionChanged();
      }
    }
  }

  const mark = $("#staleMark");
  if (mark) mark.classList.toggle("d-none", !missing);
}

/* Compiler_parse follows the editor: the entrypoints of the active file and the permutations each one
 * expands into, listed without compiling anything. One parse in flight at a time, the way
 * refreshSymbols coalesces; a source that doesn't parse keeps the previous listing, so the picker
 * doesn't blank on a half-typed line. */
let parseBusy = false, parseRerun = false;

async function refreshParsedEntries() {

  if (!hasModule()) return;

  if (!state.files[state.active]) return;

  if (parseBusy) { parseRerun = true; return; }
  parseBusy = true;

  try {
    const name = state.active;
    const entries = await window.OxAPI.parseEntrypoints(name, state.files);
    if (entries && name === state.active) {
      state.parsed = { name, entries };
      reflectFollowOptions();
    }
  } catch (err) { /* the previous listing stands */ }
  finally {
    parseBusy = false;
    if (parseRerun) { parseRerun = false; refreshParsedEntries(); }
  }
}

const scheduleParse = debounce(() => { if (hasModule() && state.mode === "compile" && !state.activeBuiltin) refreshParsedEntries(); }, 500);

/* One reflect in flight at a time: a change arriving mid-reflect coalesces into a single rerun,
 * so fast typing can't stack a queue of stale reflects behind the current one. */
let symbolsBusy = false, symbolsRerun = false, symbolsRerunApply = false;

async function refreshSymbols(applyDiags) {

  if (!hasModule()) return;

  /* A builtin include open in the editor reflects like any other file, driven as the main file with
   * its own source, so hover and the outline work inside it too; the follows config stays off there,
   * since it describes the project file's permutations. */
  const name = state.activeBuiltin || state.active;
  const files = state.activeBuiltin ? { ...state.files, [name]: { src: state.builtins[name] } } : state.files;

  if (!files[name]) return;

  if (symbolsBusy) {
    symbolsRerun = true;
    symbolsRerunApply = symbolsRerunApply || !!applyDiags;
    return;
  }
  symbolsBusy = true;
  try {

  const epoch = state.diagEpoch;
  let diags = [];

  try {
    /* The backend view rides along whatever the follows picker built; the picker's config never
     * carries one itself, so the two compose. */
    const cfg = { ...(state.activeBuiltin ? null : state.reflectCfg), backend: state.reflectBackend };
    const r = await window.OxAPI.reflectSymbols(name, files, cfg);
    state.symbols = r.doc;
    diags = r.diags || [];
  } catch (err) { state.symbols = null; }

  if (state.mode === "compile") renderSymbols(state.symbols);

  /* Reflection is what runs between keystrokes and on a file switch, so its diagnostics are the live
   * squiggles: a broken file shows its marks the moment it is opened, not once it is compiled.
   * Two things outrank them. A compile that rendered while this was in flight drew marks for exactly
   * the text it compiled (the epoch moved), and only the wasm backend reflects for real, so the mock
   * keeps its compile-driven marks instead of having them blanked by a reflect that knows nothing. */

  if (
    applyDiags && state.mode === "compile" && !state.activeBuiltin &&
    epoch === state.diagEpoch && state.active === name && window.OxAPI.backend === "wasm"
  ) {

    /* The permissive parse (all extensions, or a followed permutation) can see FEWER errors than the
     * real compile did (per-permutation gating, cross-backend SBFile combining). While the text
     * hasn't changed since that compile, its diagnostics stay part of the story instead of being
     * blanked by a reflect that parsed clean; an edit makes the compile stale and reflect takes over. */
    const compileStale = !state.compiledAt ||
      Object.values(state.files).some(f => f.editedAt && f.editedAt > state.compiledAt);
    const kept = !compileStale && state.compiled ? state.compiled.diags : [];
    const sig = d => `${d.sev}|${d.file}|${d.line}|${d.msg}`;
    const merged = [...kept, ...diags.filter(d => !kept.some(k => sig(k) === sig(d)))];

    window.OxEditor.markDiags(merged.filter(d => d.file === state.active));
    renderProblems(merged);
  }

  } finally {
    symbolsBusy = false;
    if (symbolsRerun) {
      symbolsRerun = false;
      const apply = symbolsRerunApply; symbolsRerunApply = false;
      refreshSymbols(apply);
    }
  }
}
function renderSymbols(sr) {
  window.OxSymbols.mount($("#symView"), sr, {
    ...state.symOpts, onGoto: gotoSymbol, onOpts: o => Object.assign(state.symOpts, o)
  });
}
/* A location names the file the compiler resolved, which is the project path with a "./" in front of
 * it and a builtin as "./@types.hlsli". Both are openable: a project file in the editor, a builtin as
 * the read-only file the rail already lists, which is what makes a jump into an include work. */
function gotoSymbol(loc) {

  const file = loc.file.replace(/^\.\//, "");
  const target = { ...loc, file };

  if (state.mode !== "compile") { $("#mCompile").checked = true; setMode("compile"); }

  if (state.builtins[file]) {
    openBuiltin(file);
    window.OxEditor.gotoDiag(target);
    return;
  }

  if (!state.files[file])
    return toast(`${esc(file)} isn't in the project tree, so there's nothing to jump to (the oiSR keeps the location anyway).`, "info");

  if (file !== state.active || state.activeBuiltin) openFile(file);
  window.OxEditor.gotoDiag(target);
}

/* ------------------------------------------------------------------ pipeline (oiSP) */

/* A derivation reads the oiSH's bytes, so a document without any (one the assemble card fabricated,
 * which api.js marks `mock`) cannot have a pipeline. That is a missing feature rather than a failure,
 * so the pane says nothing is derivable instead of the refusal reaching the page as an error. */
async function derived(doc, pick) {
  try { return await window.OxAPI.derivePipeline(doc, pick); }
  catch (err) { return null; }
}

async function refreshPipeline() {
  const doc = state.compiled && !state.compiled.diags.some(d => d.sev === "error") ? state.compiled.doc : null;
  if (!doc || doc.flags.reflectionOnly) { state.pipeline.sp = null; state.pipeline.refused = null; renderPipeline(); renderIsa(); return; }
  const r = await derived(doc, state.pipeline.pick);
  if (!r) { state.pipeline.sp = null; state.pipeline.refused = null; }
  else if (r.refused) { state.pipeline.sp = null; state.pipeline.refused = r; }
  else { state.pipeline.sp = r; state.pipeline.refused = null; }
  await renderPipeline(); renderIsa();
  buildDlMenu();                         //the derivation decides whether the .oiSP download exists
}
async function renderPipeline() {
  const el = $("#psoView");
  const inspectingSp = state.mode === "oish" && state.inspectedKind === "oisp";
  const sp = inspectingSp ? state.oispDocs[state.inspected] : state.pipeline.sp;
  const doc = inspectingSp ? null : currentDoc();
  const refused = inspectingSp ? null : state.pipeline.refused;
  /* The print reads the oiSP's bytes, which a document recorded without them (js/mock_data.js keeps
   * the shape, not the file) doesn't have. That costs the `file data` card, not the pipeline. */
  const printText = sp && sp.pipelines.length && sp.bytes ? await window.OxAPI.printPipeline(sp, 0) : null;
  state.pipeline.printText = printText;
  window.OxPipeline.mount(el, { sp, refused, doc, editable: !inspectingSp, printText }, {
    onSupply: (pi, field, index, value) => supplyField(sp, pi, field, index, value),
    onPick: async (stage, idx) => { state.pipeline.pick[stage] = idx; await refreshPipeline(); }
  });
}
async function supplyField(sp, pi, field, index, value) {
  try {
    /* The wasm path answers with a NEW document over new bytes (SPFile_supply re-serializes); the
     * mock mutates in place. The answer replaces the document in state, or the print and the .oiSP
     * download would keep the pre-edit bytes. */
    const updated = await window.OxAPI.supplyPipeline(sp, pi, field, index, value);
    if (updated) state.pipeline.sp = updated;
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
  if (!entry.doc) {
    entry.doc = await window.OxAPI.reflectBinary(name, entry.type, entry.bytes, entry.origin);
    /* The reflection is heuristic (no oiSH behind a bare binary), but the BYTES are real: they ride
     * on the doc so disassembly and extraction read them instead of failing on the missing oiSH. */
    entry.doc.rawBytes = entry.bytes;
    entry.doc.rawType = entry.type;
  }
  return entry.doc;
}

async function openStandalone(name) {
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
  if (state.binRowsArr.length) showSelectedBinary(true);

  /* No pipeline here: one is derived from an oiSH, and a standalone binary has none until the oiSH tab
   * assembles it into one. */
  state.pipeline = { sp: null, refused: null, pick: {} };
  state.isa.result = null; state.isa.error = null;
  await renderPipeline(); renderIsa();
  renderInspectDiff();
  buildDlMenu();
  wireAssembleCard(doc);
  applyContext();
}

/* the oiSH tab of a standalone binary: give it an identifier and it becomes a real oiSH in Inspect mode */
function wireAssembleCard(doc) {
  const go = $("#asmOishGo");
  if (!go || !doc) return;
  go.addEventListener("click", async () => {
    const picked = id => [...$(id).querySelectorAll("input:checked")].map(o => o.value);
    const ident = { entry: $("#asmEntry").value.trim(), stage: $("#asmStage").value, model: $("#asmModel").value,
      extensions: picked("#asmExts"), vendors: picked("#asmVendors"), name: $("#asmName").value.trim() };
    const out = await window.OxAPI.assembleOiSH(doc, ident);

    /* An assembled oiSH is hand-built input; nothing may hand it over unvalidated. The round trip
     * through the real writer + reader refuses it with the reader's own reason before it can reach
     * the page's other tabs, a download, or a consumer. assembleOiSH itself is still the mock
     * (TODO(wasm) in api.js), and a mock document has no bytes the writer can serialize: that case
     * carries a warning instead, since such a document can't leave the page anyway. */
    try {
      if (window.OxAPI.backend === "wasm")
        await window.OxAPI.parseOiSH(out.name, await window.OxAPI.writeOiSH(out));
    } catch (err) {
      if (!/no oiSH behind it/.test(err.message))
        return toast(`<b>Refused:</b> the assembled oiSH doesn't survive SHFile_read: ${esc(err.message)}`, "danger");
      toast("Assembled without validation: `shader assemble → oiSH` isn't wired to the compiler yet, so this document is page-local and mock-shaped.", "warning");
    }

    state.oishDocs[out.name] = out;
    $("#mOish").checked = true; setMode("oish"); fillDiffSelects(); openInspect(out.name, "oish");
    toast(`<b>Assembled:</b> ${esc(out.name)}: a real oiSH now (${esc(ident.stage)} · SM ${esc(ident.model)}${ident.extensions.length ? " · " + esc(ident.extensions.join("+")) : ""}).`, "success");
  });
}

/* raw DXC output (Command tab → Run with DXC) is a standalone binary, never an oiSH */
function onRawDxc(r) {
  state.standalone[r.name] = { type: r.type, bytes: r.bytes, text: r.text, origin: r.origin, raw: true };
  toast(`<b>MOCK! NOT REAL DATA YET!</b> ${esc(r.name)} was NOT produced by DXC: nothing ran, the bytes are fabricated. ` +
    "The argv is real now, but running an edited flag line still needs a raw compile entry (see api.js compileRaw).", "danger");
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

async function openInspect(name, kind) {
  const store = { oish: state.oishDocs, oisr: state.oisrDocs, oisp: state.oispDocs }[kind || "oish"];
  if (!name || !store || !store[name]) { renderRail(); renderInspectDiff(); return; }
  state.inspected = name; state.inspectedKind = kind;
  const doc = store[name];
  if (kind === "oish") {
    $("#reflView").innerHTML = window.OxInspect.reflectionHTML(doc);
    $("#oishView").innerHTML = window.OxInspect.oishViewHTML(doc);
    state.binRowsArr = window.OxInspect.binRows(doc, null);
    fillBinStrip();
    if (state.binRowsArr.length) showSelectedBinary(true);
    renderSymbols(null);
    state.pipeline = { sp: null, refused: null, pick: {} };
    const r = doc.flags.reflectionOnly ? null : await derived(doc, {});
    if (r && r.refused) state.pipeline.refused = r; else state.pipeline.sp = r;
    await renderPipeline(); renderIsa();
  } else if (kind === "oisr") {
    state.binRowsArr = [];
    $("#reflView").innerHTML = window.OxInspect.reflectionHTML(null);
    $("#oishView").innerHTML = window.OxInspect.oishViewHTML(null);
    renderSymbols(doc);
    await renderPipeline(); renderIsa();
  } else {
    state.binRowsArr = [];
    $("#reflView").innerHTML = window.OxInspect.reflectionHTML(null);
    $("#oishView").innerHTML = window.OxInspect.oishViewHTML(null);
    renderSymbols(null);
    await renderPipeline(); renderIsa();
  }
  renderInspectDiff();
  buildDlMenu();
  $("#dlBtn").removeAttribute("disabled");
  renderRail();
  applyContext();
}

$("#combineBtn").addEventListener("click", async () => {
  const [a, b] = currentAB();
  if (!a || !b) return toast("Pick two oiSH files (A and B) first.", "warning");
  try {
    const doc = await window.OxAPI.combineOiSH(a, b);
    state.oishDocs[doc.name] = doc;
    fillDiffSelects(); openInspect(doc.name, "oish");
    toast(`<b>Combined:</b> ${esc(doc.name)}: union of both files' binaries.`, "success");
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
/* Shared by the strip's own dropdown and the IntelliSense picker driving it (driveSelectedBinary). */
function binSelectionChanged() {
  state.isa.result = null; state.isa.error = null; state.isa.entrypoint = null;
  /* Picking a binary refreshes every view, but only changes TAB when an asm tab is already open
   * (there the pick decides SPIR-V vs DXIL); on ISA or Pipeline it must not yank the view away. */
  const cur = document.querySelector("#outTabs .nav-link.active");
  const tab = cur && cur.closest("[data-tab]") ? cur.closest("[data-tab]").dataset.tab : null;
  showSelectedBinary(!(tab === "spv" || tab === "dxil"));
  renderIsa();
}
$("#binSel").addEventListener("change", binSelectionChanged);
async function showSelectedBinary(auto) {
  const row = state.binRowsArr[+$("#binSel").value];
  const doc = currentDoc();
  if (!row || !doc) return;
  /* A failed render must say so where the binary strip is; a swallowed rejection here leaves the
   * PREVIOUS binary's disassembly on screen, which reads as the new one with stale content. */
  try {
    await window.OxInspect.showBinary(doc, row, !auto, state.mode === "compile" ? state.active : null);
    syncAsmCursor();
  } catch (err) {
    $("#binMeta").innerHTML = `<span class="text-danger">couldn't show this binary: ${esc(err.message)}</span>`;
  }
}
/* The editor's cursor lights up every disassembly line born on that source line, the way Compiler
 * Explorer does it; clicking a tinted line goes the other way (the delegation below). */

let lastCursorLine = 1, lastSyncedLine = -1;

function syncAsmCursor(line) {

  if (line != null) lastCursorLine = line;
  if (state.mode !== "compile") return;

  /* cursorActivity fires per keystroke, and scanning thousands of asm rows each press is typing lag:
   * only the visible pane is synced, and only when the cursor actually changed line. */
  const tab = activeTab();
  const pane = tab === "spv" ? "#spvAsm" : tab === "dxil" ? "#dxilAsm" : null;

  if (line != null && lastCursorLine === lastSyncedLine && pane) return;

  if (pane) {
    const el = $(pane);
    if (el && el._asmAny) { window.OxAsmMap.syncCursor(el, lastCursorLine); lastSyncedLine = lastCursorLine; }
  }
}

/* Clicking a disassembly line selects it (the share link carries that selection so "look at this
 * SPIR-V line" lands there); a mapped line additionally jumps to the source it was born on. */
for (const id of ["#spvAsm", "#dxilAsm"])
  document.addEventListener("click", e => {
    const row = e.target.closest && e.target.closest(`${id} .asm-line`);
    if (!row) return;
    const pane = document.querySelector(id);
    pane.querySelectorAll(".asm-sel").forEach(n => n.classList.remove("asm-sel"));
    row.classList.add("asm-sel");
    state.asmSel = { pane: id === "#spvAsm" ? "spv" : "dxil", line: [...pane.querySelectorAll(".asm-line")].indexOf(row) };
    if (row.dataset.src)
      gotoSymbol({ file: row.dataset.file || state.active, line: +row.dataset.src, ch0: 0, ch1: 0 });
  });

function currentDoc() {
  if (state.mode === "bins") return (state.binActive && state.standalone[state.binActive] && state.standalone[state.binActive].doc) || null;
  return state.mode === "oish" ? (state.inspectedKind === "oish" ? state.oishDocs[state.inspected] : null) : (state.compiled && state.compiled.doc);
}

/* ------------------------------------------------------------------ uploads */

async function importSnapshotFile(f, bytes) {
  try {
    /* A snapshot is a stranger's bytes the way a share link is: the archive is bounded before the
       module allocates for it, and what it unpacked to passes the same file-set rules a link does. */
    const files = checkSnapshotFiles(await window.OxAPI.importSnapshot(checkSnapshotBytes(bytes)));
    if (!Object.keys(files).length) throw new Error("the archive holds no files");
    await saveWorkspaceNow();
    window.OxWorkspace.switchTo(window.OxWorkspace.create(f.name.replace(/\.oiCA$/i, ""), "import").id);
    applyProject(files);
    if (state.mode !== "compile") { $("#mCompile").checked = true; setMode("compile"); }
    renderRail(); renderWsMenu(); openFile(state.active);
    saveWorkspaceNow();
    toast(`Project restored from ${esc(f.name)} (${Object.keys(state.files).length} files).`, "success");
  } catch (err) {
    toast(`Couldn't import ${esc(f.name)}: ${esc(err.message)}`, "warning");
  }
}

/* Files arriving from the rail's Upload button or a drop onto the project: an .oiCA restores a whole
 * workspace, text becomes project files, and compiled documents are pointed at the mode that opens
 * them rather than silently swallowed. */
async function ingestProjectFiles(list) {

  let opened = null, added = 0;

  for (const f of list) {

    const bytes = new Uint8Array(await f.arrayBuffer());
    const magic = String.fromCharCode(...bytes.slice(0, 4));

    if (magic === "oiCA" || /\.oiCA$/i.test(f.name)) { await importSnapshotFile(f, bytes); continue; }

    if (/^oiS[HPR]$/.test(magic) || /\.(oiSH|oiSP|oiSR)$/i.test(f.name)) {
      toast(`${esc(f.name)} is a compiled document: open it in Inspect mode (top left).`, "info");
      continue;
    }

    if (/\.(spv|dxil)$/i.test(f.name)) {
      toast(`${esc(f.name)} is a binary: open it in SPV / DXIL mode (top left).`, "info");
      continue;
    }

    state.files[f.name] = { src: new TextDecoder().decode(bytes), diags: [] };
    opened = f.name; ++added;
  }

  if (added) {
    touchWorkspace(); renderRail(); openFile(opened);
    toast(`Added ${added} file${added > 1 ? "s" : ""} to the workspace.`, "success");
  }
}

$("#upProject").addEventListener("click", () => $("#upProjectFile").click());
$("#upProjectFile").addEventListener("change", async e => {
  await ingestProjectFiles(e.target.files);
  e.target.value = "";
});

/* Dropping files onto the rail or the editor works like the Upload button, compile mode only. */
for (const zone of [document.querySelector(".rail"), $("#editorPane")]) {
  zone.addEventListener("dragover", e => {
    if (state.mode !== "compile") return;
    e.preventDefault(); zone.classList.add("drop-hint");
  });
  zone.addEventListener("dragleave", () => zone.classList.remove("drop-hint"));
  zone.addEventListener("drop", e => {
    zone.classList.remove("drop-hint");
    if (state.mode !== "compile" || !e.dataTransfer || !e.dataTransfer.files.length) return;
    e.preventDefault();
    ingestProjectFiles(e.dataTransfer.files);
  });
}

$("#loadOishBtn").addEventListener("click", () => $("#uploadOish").click());
$("#uploadOish").addEventListener("change", async e => {
  let last = null;
  for (const f of e.target.files) {
    const bytes = new Uint8Array(await f.arrayBuffer());
    /* the CLI sniffs the magic (file header); the extension is the fallback for mock bytes */
    const magic = String.fromCharCode(...bytes.slice(0, 4));
      /* A project snapshot restores the working tree rather than opening a document. */
    if (magic === "oiCA" || /\.oiCA$/i.test(f.name)) { await importSnapshotFile(f, bytes); continue; }

  const kind = magic === "oiSR" || /\.oiSR$/i.test(f.name) ? "oisr" : magic === "oiSP" || /\.oiSP$/i.test(f.name) ? "oisp" : "oish";
    if (kind === "oisr") state.oisrDocs[f.name] = await window.OxAPI.parseOiSR(f.name, bytes);
    else if (kind === "oisp") state.oispDocs[f.name] = await window.OxAPI.parseOiSP(f.name, bytes);
    else state.oishDocs[f.name] = await window.OxAPI.parseOiSH(f.name, bytes);
    last = { name: f.name, kind };
  }
  e.target.value = "";
  fillDiffSelects(); renderRail();
  if (last) openInspect(last.name, last.kind);
  toast("Loaded: note: parsing is mocked (shape derived from the sample); the real path is SHFile_read / SRFile_read / SPFile_read.", "info");
});

$("#loadBinBtn").addEventListener("click", () => $("#uploadBin").click());
$("#uploadBin").addEventListener("change", async e => {
  let last = null;
  for (const f of e.target.files) {

    const bytes = new Uint8Array(await f.arrayBuffer());
    const type = /\.dxil$/i.test(f.name) ? "dxil" : "spirv";

    /* An uploaded binary is judged before anything reflects or disassembles it (spirv-val, or DXC's
     * validator for a container); a rejected one is refused with the validator's own reason. */
    const v = await window.OxAPI.validate(type, bytes);
    if (!v.valid) {
      const judge = type === "dxil" ? "the DXIL validator" : "spirv-val";
      toast(`<b>Refused ${esc(f.name)}:</b> ${judge} says:<br>${esc((v.message || "invalid binary").slice(0, 400))}`, "danger");
      continue;
    }

    state.standalone[f.name] = { type, bytes };
    last = f.name;
  }
  e.target.value = "";
  for (const n of Object.keys(state.standalone)) await standaloneDoc(n);
  fillDiffSelects();
  if (last) openStandalone(last);
  toast("Loaded: note: reflection of uploaded bytes is mocked (shape derived from the sample); the real path is spirv-reflect / DXC container reflection.", "info");
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
      h += dlItem("oish", `${esc(base)}.oiSH`, "SHFile_write: full file (all backends)");
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
    h += dlItem("oisr", `${esc(state.inspected)}`, "SRFile_write: re-serialize the loaded oiSR");
  }

  /* The snapshot packs state.files, nothing compiled, so it never waits for a compile. */
  if (state.mode === "compile" && window.OxAPI.backend === "wasm") {
    if (h && !(state.mode === "compile" && state.symbols)) h += `<li><hr class="dropdown-divider"></li>`;
    h += dlItem("snapshot", "project snapshot (.oiCA)",
      "the whole project as a real archive; drop it back on the page (or feed it to the CLI) to restore");
  }
  const sp = inspectKind === "oisp" ? state.oispDocs[state.inspected] : state.pipeline.sp;
  if (sp) {
    h += `<li><hr class="dropdown-divider"></li>`;
    h += dlItem("oisp", `${esc(sp.name)}`, inspectKind === "oisp" ? "SPFile_write: re-serialize the loaded oiSP" : "-pso-output · the pipeline a live ISA run compiles, with provenance");
  }
  menu.innerHTML = h;
  if (h) $("#dlBtn").removeAttribute("disabled"); else $("#dlBtn").setAttribute("disabled", "");
}
$("#dlMenu").addEventListener("click", e => {
  download_(e).catch(err => toast(`<b>Download failed:</b> ${esc(err.message)}`, "danger"));
});

/* Ctrl+S means "save my work": the project as an .oiCA, the durable form, instead of the browser's
 * save-page dialog. The mock page has no archive writer, so there the toast says what to use. */
document.addEventListener("keydown", async e => {
  if (!(e.ctrlKey || e.metaKey) || e.key.toLowerCase() !== "s" || e.shiftKey || e.altKey) return;
  e.preventDefault();
  if (window.OxAPI.backend !== "wasm")
    return toast("Saving as .oiCA needs the compiler module; on this mock page, Share gives you a link instead.", "info");
  try {
    download("project.oiCA", await window.OxAPI.projectSnapshot(state.files), "application/octet-stream");
    toast("Project saved as project.oiCA — drop it back on the page (or another machine) to restore.", "success");
  } catch (err) {
    toast(`Couldn't save: ${esc(err.message)}`, "warning");
  }
});

async function download_(e) {
  const btn = e.target.closest("[data-act]"); if (!btn) return;
  const act = btn.dataset.act;
  if (act === "oisr") {
    const sr = state.mode === "compile" ? state.symbols : state.oisrDocs[state.inspected];
    if (sr) download(sr.name, await window.OxAPI.writeOiSR(sr), "application/octet-stream");
    return;
  }
  if (act === "snapshot") {
    const bytes = await window.OxAPI.projectSnapshot(state.files);
    download("project.oiCA", bytes, "application/octet-stream");
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
}

/* An include named anywhere in the reflection opens the file it resolved to: a project file in the
 * editor, an @builtin as the read-only copy the compiler embeds. Delegated, since the reflection is
 * re-rendered on every compile and on every document opened. */
document.addEventListener("click", e => {
  const link = e.target.closest("[data-openfile]");
  if (!link) return;
  e.preventDefault();
  gotoSymbol({ file: link.dataset.openfile, line: 1, ch0: 0, ch1: 0 });
});

/* ------------------------------------------------------------------ problems */

let lastProblemsSig = "";

function renderProblems(diags) {

  /* A new problem opens the panel; the same problems again do not, so closing it while an error sits
   * there is respected until the diagnostics actually change. */

  const sig = diags.map(d => `${d.sev} ${d.file}:${d.line}:${d.ch0} ${d.msg}`).join("\n");

  if (diags.length && sig !== lastProblemsSig)
    document.body.classList.remove("prob-collapsed");

  lastProblemsSig = sig;

  const body = $("#probBody");
  $("#probCount").textContent = diags.length
    ? `${diags.filter(d => d.sev === "error").length} errors · ${diags.filter(d => d.sev === "warn").length} warnings`
    : "no problems";
  if (!diags.length) { body.innerHTML = `<div class="text-body-secondary small p-3">No problems: nice.</div>`; return; }
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
$("#probCopy").addEventListener("click", e => {
  e.stopPropagation();
  const diags = $("#probBody")._diags || [];
  if (!diags.length) return toast("No problems to copy.", "info");
  const text = diags.map(d => `${d.file || state.active}:${d.line}:${d.ch0 + 1}: ${d.sev}: ${d.msg}`).join("\n");
  (navigator.clipboard ? navigator.clipboard.writeText(text) : Promise.reject())
    .then(() => toast(`Copied ${diags.length} problem(s) to the clipboard.`, "success"))
    .catch(() => toast("Clipboard unavailable; select the rows instead.", "warning"));
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

  /* The active file plus everything it transitively includes, not the whole project: the include a
   * shader edited travels with it (or the recipient compiles against the stock one and sees different
   * results), but bundling every sample too made the link an order of magnitude bigger than chat
   * message limits. @builtins ship in the compiler and need no bundling. */

  const files = {};
  const queue = [state.active];

  while (queue.length) {

    const name = queue.pop();
    const f = state.files[name];
    if (!f || name in files) continue;

    files[name] = f.src == null ? "" : f.src;

    for (const m of files[name].matchAll(/^\s*#include\s+"([^@"][^"]*)"/gm)) {
      const inc = m[1].replace(/^\.\//, "");
      if (state.files[inc]) queue.push(inc);
      else {
        const dir = name.includes("/") ? name.slice(0, name.lastIndexOf("/") + 1) : "";
        if (state.files[dir + inc]) queue.push(dir + inc);
      }
    }
  }

  /* The link also carries where the sender was LOOKING: the open output tab, the selected binary row,
   * and the editor's cursor line. "look at this SPIR-V" then lands on the SPIR-V view with the mapped
   * lines lit, not on the default tab; the receiving side compiles first when the view needs one. */
  const activeTab = document.querySelector("#outTabs .nav-item.active, #outTabs [data-tab] .nav-link.active");
  const v = {
    tab: activeTab ? activeTab.closest("[data-tab]").dataset.tab : null,
    bin: $("#binSel") && $("#binSel").value !== "" ? +$("#binSel").value : null,
    line: lastCursorLine
  };
  if (state.asmSel && state.asmSel.pane === v.tab) v.aline = state.asmSel.line;

  const payload = { m: state.mode, f: state.active, files, o, v };

  window.OxUtil.encodeShare(payload).then(encoded => {

    location.hash = encoded;
    navigator.clipboard && navigator.clipboard.writeText(location.href).catch(() => {});

    /* A URL past chat-message size is a link that breaks in transit; from there the snapshot is the
     * right vehicle, and the server never has to know either exists. */
    if (location.href.length > 1900)
      toast(`Copied, but this link is ${location.href.length} characters and chat apps will truncate it; ` +
        "use Download → project snapshot instead.", "warning");
    else
      toast("Permalink copied to the clipboard (compressed into the URL hash, nothing leaves the browser).", "success");
  });
});
async function restoreFromHash() {
  if (!/[sz]=/.test(location.hash)) return false;
  try {
    const p = await window.OxUtil.decodeShare(location.hash);
    const srcHash = location.hash.replace(/^#/, "");
    if (p.v) state.pendingView = p.v;

    /* A link opens as its own workspace, so it can never overwrite the one that was open; that one is
     * saved and stays on the list. The same link opened again switches back to its workspace instead,
     * keeping whatever was edited in it since. */

    await saveWorkspaceNow();

    const existing = window.OxWorkspace.findBySrcHash(srcHash);
    if (existing) {
      const payload = await window.OxWorkspace.load(existing.id);
      if (payload) {
        window.OxWorkspace.switchTo(existing.id);
        applyProject(payload.files, payload.f, payload.o);
        renderRail(); renderWsMenu();
        return true;
      }
      window.OxWorkspace.remove(existing.id);       //its data was evicted; reseed from the link
    }

    /* Project links carry the whole file set; the single-file form is what older links carry,
     * overlaying the one file onto the samples. */

    let files;
    if (p.files && typeof p.files === "object") files = { ...p.files };
    else {
      files = Object.fromEntries(Object.entries(M.SAMPLE_FILES).map(([n, f]) => [n, f.src]));
      if (p.f && p.src != null) files[p.f] = p.src;
    }

    const label = (p.f || Object.keys(files).find(n => /\.hlsl$/i.test(n)) || "project").replace(/^.*\//, "");
    window.OxWorkspace.switchTo(window.OxWorkspace.create(label + " (shared)", "shared", srcHash).id);
    applyProject(files, p.f, p.o);
    renderRail(); renderWsMenu();
    saveWorkspaceNow();
    return true;
  } catch (e) {
    toast(`Couldn't restore from this link: ${esc(e.message)}.`, "warning");
    return false;
  }
}

/* Back/forward (keyboard or the mouse's side buttons) walk the hash history the Share button writes.
 * Without this the hash moved and the page didn't, which read as everything silently breaking; with it
 * they restore the encoded state, so history navigation becomes a feature instead of a trap. */
/* The floor of the app's history: back past the last in-page state would leave or reload the page,
 * which is what the mouse's back button kept doing mid-edit. The sentinel bounces it; forward/back
 * BETWEEN shared states still walks normally and restores below. */
try {
  if (!(history.state && history.state.ox)) {
    history.replaceState({ ox: "floor" }, "");
    history.pushState({ ox: "top" }, "");
  }
} catch (e) { /* file:// in some browsers refuses pushState; back then behaves as before */ }

/* Any promise nobody awaited that fails must still SAY so: a swallowed rejection leaves whatever
 * pane it was filling showing stale content, which reads as "feature silently broken" (it did,
 * three times). Local handlers still catch and place errors properly; this is the net under them. */
window.addEventListener("unhandledrejection", e => {
  const msg = e.reason && e.reason.message ? e.reason.message : String(e.reason);
  toast(`<b>Something failed in the background:</b> ${esc(msg)}`, "danger");
});

/* Leaving is only worth interrupting while an edit hasn't reached storage yet (the debounce window,
 * or a store too full to save); a saved workspace comes back by itself on the next visit. */
window.addEventListener("beforeunload", e => {
  if (window.OxWorkspace.isDirty()) { e.preventDefault(); e.returnValue = ""; }
});

window.addEventListener("popstate", e => {
  if (e.state && e.state.ox === "floor") {
    try { history.pushState({ ox: "top" }, ""); } catch (err) { /* see above */ }
    toast("Reached the start of this page's history; use Share links to move between states.", "info");
  }
});

window.addEventListener("hashchange", async () => {

  if (!await restoreFromHash()) return;

  renderRail();
  if (state.mode === "compile")
    openFile(state.files[state.active] ? state.active : Object.keys(state.files)[0]);
  scheduleCommands(); scheduleSymbols(true);
  toast("Restored from the link in the URL.", "info");
  applyPendingView().catch(() => { });
});


/* ------------------------------------------------------------------ CLI ↔ web map */

const OxCliMap = [
  { c: "OxC3 shader compile -input x.hlsl -output x.oiSH [-compile-output spv|dxil|all] [--debug] [--split] [--keep-registers] [--warn-*] [--ignore-empty-files] [-threads N] [-include-dir d]",
    w: "Compile mode: toolbar switches map 1:1; the Command tab shows the exact line for the current run. Include roots = the file tree.", s: "live" },
  { c: "OxC3 compile shaders -format HLSL …", w: "Same as above: the pre-`shader`-category spelling.", s: "live" },
  { c: "OxC3 shader reflect -input x.hlsl -output x.oiSH", w: "Compile mode → “Reflection only” switch (binaries stripped, ESHSettingsFlags_ReflectionOnly).", s: "live" },
  { c: "OxC3 shader reflect-symbols -input x.hlsl [-output x.oiSR] [-include-dir d] [--verbose]",
    w: "Symbols tab: the frontend symbol AST (entrypoints, user types, resources, parameters, locals) with source locations → go-to-definition into the editor; built-in include symbols collapsed per include. Download menu → .oiSR.", s: "live" },
  { c: "OxC3 shader entrypoints -input x.oiSH [--verbose]", w: "oiSH tab → Entrypoints card; full per-entry detail in the Reflection tree (IO, wave sizes, payload, binary refs).", s: "live" },
  { c: "OxC3 shader includes -input x.oiSH", w: "oiSH tab → Includes card (path + CRC32C).", s: "live" },
  { c: "OxC3 shader feature_set -input x.oiSH", w: "oiSH tab → Feature set table (extensions with dormant struck through, vendors, models, sizes).", s: "live" },
  { c: "OxC3 file header -input x.oiSH|x.oiSR|x.oiSP", w: "oiSH tab → header card; Symbols tab → header strip (nodes, features); Pipeline tab → header card (pipeline/stage/specialization counts, stored blend/vertex entries).", s: "live" },
  { c: "OxC3 file data -input x.oiSH [--bin] [-entry N] [-compile-output spv|dxil] [--includes] [-start/-length] [-output f]",
    w: "Reflection tree = the full print; Binary strip + SPIR-V/DXIL tabs = --bin -entry N; Download menu = -output.", s: "live" },
  { c: "OxC3 file data -input x.oiSR [--includes]", w: "Symbols tab on a loaded .oiSR (Inspect mode); --includes expands the collapsed built-in symbols.", s: "live" },
  { c: "OxC3 file data -input x.oiSP", w: "Pipeline tab: every pipeline, its stages (oiSH + entrypoint + source hash) and the full state with each field's provenance (derived / supplied / assumed).", s: "live" },
  { c: "OxC3 file data -input x.oiSH -asic gfx1100 [-entry N]", w: "ISA tab on a binary of an oiSH: the same offline AMD ISA view inline (-asic implies --bin + SPIR-V).", s: "native only" },
  { c: "OxC3 isa devices", w: "ISA tab → the -asic dropdown: gfx11xx (RDNA3), gfx1150 (RDNA3.5), gfx12xx (RDNA4), what the bundled amdllpc accepts.", s: "live" },
  { c: "OxC3 isa disassemble -input x.spv|x.oiSH -asic gfxNNNN [-entry N] [-output x.txt]",
    w: "ISA tab → Disassemble: SPIR-V → ELF (amdllpc) → ISA text (amdgpu-dis) with the SGPR/VGPR/code/scratch/LDS line; raster, compute and mesh stages only (ray tracing and DXIL have no offline path). Download menu → .isa.txt.", s: "native only" },
  { c: "OxC3 isa disassemble -input x.oiSH -asic live[:i] [-entry N] [-pso-set \"path=value,..\"] [-pso-input x.oiSP] [-pso-output x.oiSP] [--assume-defaults]",
    w: "ISA tab → “live”: a real Vulkan device via VK_KHR_pipeline_executable_properties (cross-vendor statistics; ISA text from AMD + Mesa only). The Pipeline tab and the ISA tab's <i>Pipeline state</i> panel ARE the state it compiles: derived with provenance, refused while anything is assumed; every field you change becomes a -pso-set entry on the CLI line (the only override there is: no per-field flags, so one vocabulary), Download → .oiSP is what -pso-input replays. Greyed out in a browser (no device); runs for real when the page is hosted in VS Code with the native OxC3.", s: "native only" },
  { c: "OxC3 shader validate -input x.spv|x.dxil", w: "SPV/DXIL mode → an uploaded binary is judged before anything reads it; a refusal shows the validator's reason.", s: "live" },
  { c: "OxC3 shader disassemble -input x.spv|x.dxil [-output x.txt]", w: "SPV/DXIL mode → the SPIR-V / DXIL tab of a loaded binary (also powers the binary views + binary diff); Download → .txt.", s: "live" },
  { c: "OxC3 shader assemble -input x.txt -output x.spv|x.dxil", w: "SPV/DXIL mode → the strip's Assemble card; the result is a loaded binary like any other. SPIR-V text or DXIL LL text; either result is judged by the validator before it loads.", s: "live" },
  { c: "reflect a bare .spv / .dxil (spirv-reflect / DXC container reflection; no CLI verb yet)", w: "SPV/DXIL mode: a loaded binary is reflected from its bytes into a one-binary document, so Reflection, ISA, Pipeline and Diff A↔B apply to it; it has no identifier until assembled.", s: "planned" },
  { c: "shader assemble → oiSH (planned: -stage, -model, -extensions, -vendors, a previous oiSH to merge into)", w: "SPV/DXIL mode → oiSH tab → “Assemble into oiSH”: gives a bare binary the identifier its source would have declared; the result lands in Inspect mode as a real oiSH (Combine with the other backend's file, pipelines, downloads).", s: "planned" },
  { c: "raw DXC: dxc <any flags> x.hlsl (no CLI verb yet; oxc3_getCompileArgs is live, the raw compile entry isn't)", w: "Compile mode → Command tab: each derived DXC line is the compiler's own argv (Compiler_buildCompileArgs), editable, plus a free line. Running one stays planned and the “Run with DXC” buttons sit disabled until then; the output would be a standalone binary in SPV/DXIL mode, never an oiSH: no annotations processed, nothing reflected into an identifier.", s: "planned" },
  { c: "OxC3 file combine -format oiSH -input a.oiSH -input2 b.oiSH -output c.oiSH",
    w: "Inspect mode → “Combine A+B” (requires same source hash / includes / settings; errors surface as a toast).", s: "live" },
  { c: "OxC3 file split -format oiSH …", w: "Download menu → per-backend lean files (x.spv.oiSH / x.dxil.oiSH). The CLI verb itself is still marked TODO upstream.", s: "planned" },
  { c: "Compiler_getUniqueEntrypoints (no CLI verb yet)", w: "SPV/DXIL mode → “List entrypoints” on a binary.", s: "live" },
  { c: "device-free ISA: Mesa RADV/ACO (all AMD, gfx6 to gfx12) + Intel brw (Gen9 to Xe2)", w: "The route that can run in the browser without a device or a spawned process. Prototyped (SPIR-V → ISA + stats for compute/graphics/RT), not in the CLI yet; amdllpc stays the shipped offline path. Adreno / Mali offline compilers stay manual.", s: "prototype" },
  { c: "OxC3 package -input dir -output dir [-aes key]", w: "Bakes a virtual dir (incl. compiling all shaders) into oiCA: out of scope for this page; belongs to tool.oxsomi.com.", s: "n/a here" }
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

/* Says which backend answered and, when it's the real one, what still isn't wired to a library call.
 * A page that can't say which half of what it shows is fabricated is worse than one that only mocks. */
/* The badge alone is a chip in a status bar, and the state it reports is one where every result on the page
 * is invented: a compile that should fail succeeds, a symbol tree is a guess at the source, and none of it
 * looks wrong until you check it against the real compiler. That is worth a line nobody can miss. */

function renderMockBanner() {

  const api = window.OxAPI;
  const el = $("#mockBanner");

  if (api.backend === "wasm") {
    el.classList.add("d-none");
    return;
  }

  /* Two audiences, and the wrong advice is useless to either. A visitor's module almost always fails
   * for one reason (their browser has no wasm64, which is every Safari at the time of writing), and
   * telling them to run a build script is noise; a contributor opening a checkout has no module staged
   * and needs exactly that command. The two cases are distinguishable: a staged module that refused to
   * load reports why, an absent one doesn't. */

  const memory64 = (() => {
    try { return !!new WebAssembly.Memory({ initial: 1, maximum: 1, index: "u64" }); }
    catch (e) { return false; }
  })();

  const why = !memory64
    ? "This browser can't run the compiler: it has no WebAssembly 64-bit memory. " +
      "Chrome, Edge and Firefox have it; Safari does not yet. "
    : api.loadError
      ? `The compiler module didn't load (${esc(api.loadError)}). `
      : "No compiler module is staged in <code>web/wasm/</code>. Build one with " +
        "<code>build_web.py --frontend</code> (add <code>--single_file</code> to open this page from file://). ";

  $("#mockBannerText").innerHTML =
    `<i class="bi bi-exclamation-triangle-fill"></i> <b>The compiler isn't running.</b> ${why}` +
    "Compiling, editor intelligence and every view derived from them are switched off rather than faked; " +
    "the sample project and the documents recorded from a real run are still here to read.";

  el.classList.remove("d-none");
}

/* Scalar/vector heads per declaring header, scraped from the typedefs in the builtin sources the page
 * already holds; the full xNxM matrix spread is summarized rather than listed. */
function renderTypeAliases() {

  const el = $("#aliasList");
  if (!el) return;

  const groups = [];

  for (const [file, src] of Object.entries(state.builtins)) {

    const seen = new Set();
    let m;
    const rx = /^\s*typedef\s+\S+\s+([A-Za-z_]\w*);/gm;

    while ((m = rx.exec(src))) {
      const head = m[1].replace(/x\d(x\d)?$/, "");
      seen.add(head);
    }

    if (seen.size)
      groups.push({ file, names: [...seen].sort() });
  }

  if (!groups.length) return;

  el.innerHTML = groups.map(g =>
    `<div class="mb-1"><span class="text-body-secondary">${esc(g.file)}:</span> ` +
    g.names.map(n => `<span class="tok">${esc(n)}</span>`).join("") + "</div>").join("");
}

/* Only on a hosted site that keeps older module builds around (wasm/versions.json); switching picks
 * where wasmload.js loads the module from on the next visit, so this just remembers and reloads. */
async function renderVersionPicker() {

  const versions = window.OxWasmVersions ? await window.OxWasmVersions : null;
  if (!versions || versions.length < 2) return;

  const cur = window.OxWasmVersion.pick;
  const sel = document.createElement("select");
  sel.id = "stVersion";
  sel.className = "form-select form-select-sm w-auto";
  sel.title = "OxC3 module version. Older ones stay hosted for links that predate a breaking change.";
  sel.innerHTML = versions.map(v =>
    `<option value="${esc(v.id)}" ${v.id === cur ? "selected" : ""}>${esc(v.label || v.id)}</option>`).join("");

  sel.addEventListener("change", () => {
    try {
      if (sel.value === "current") localStorage.removeItem("ox.wasmVersion");
      else localStorage.setItem("ox.wasmVersion", sel.value);
    } catch (e) { }
    location.reload();
  });

  $("#stBackend").after(sel);
}

function renderBackendBadge() {
  const el = $("#stBackend");
  const api = window.OxAPI;
  if (api.backend === "wasm") {
    el.className = "badge text-bg-success";
    el.textContent = `OxC3 ${api.version}`;
    el.title = "the real compiler, as wasm64. Still fabricated:\n" +
      api.stubs.map(s => `\u2022 ${s.what}: ${s.why}`).join("\n");
  } else {
    el.className = "badge text-bg-warning";
    el.textContent = "mock";
    el.title = (api.loadError ? `No wasm module (${api.loadError}). ` : "") +
      "Everything below js/api.js is fabricated (js/mock.js + js/mock_formats.js). " +
      "Build one with build_web.py --frontend.";
  }
}

document.addEventListener("DOMContentLoaded", async () => {
  await window.OxAPI.init(window.OxWasmVersion ? { base: window.OxWasmVersion.base } : undefined);
  renderBackendBadge();
  renderVersionPicker();
  renderMockBanner();

  /* The syntax reference's vocabularies come off the compiler itself (live module, or the recording
   * gen_mock_data took from it), so they can't silently drift again: extensions and vendors from the
   * oiSH enums, the type aliases scraped from the builtin headers' own typedefs. */
  window.OxPipeline.setVocab(await window.OxAPI.spFieldVocab());

  {
    const enums = await window.OxAPI.annotationEnums();
    window.OxInspect.setVocab(enums);
    if (enums.extensions.length)
      $("#extList").innerHTML = enums.extensions.map(e => `<span class="tok">${esc(e)}</span>`).join("");
    if (enums.vendors.length)
      $("#vendorList").textContent = enums.vendors.join(" · ");
    renderTypeAliases();
  }
  state.caps = await window.OxAPI.capabilities();
  state.isa.targets = await window.OxAPI.isaTargets();

  /* The @-prefixed includes are compiled into the module, so the real sources replace the mock's
   * excerpts as soon as it is there (Compiler_builtInIncludeAt). */
  const builtins = await window.OxAPI.builtinIncludes();
  if (builtins && builtins.length)
    state.builtins = Object.fromEntries(builtins.map(b => ["@" + b.name, b.src]));

  if (state.caps.host !== "browser") document.querySelector(".navbar-brand").insertAdjacentHTML("beforeend", ` <span class="badge text-bg-info">${esc(state.caps.host)} host</span>`);
  window.OxEditor.init();
  window.OxTheme.init();
  window.OxEditor.onCursor(line => syncAsmCursor(line));

  /* Without the module nothing here can answer for real, so the features that would have to invent are
   * switched off rather than faked: no compile, no editor intelligence, no derived views. The editor
   * still opens the sample project and the recorded documents stay readable. */

  if (window.OxAPI.backend === "wasm")
    window.OxIntelliSense.attach(window.OxEditor, () => state.symbols, () => state.files,
      () => state.builtins, gotoSymbol);

  else {
    const btn = $("#compileBtn");
    btn.disabled = true;
    btn.title = "The compiler module isn't running, so there is nothing to compile with";
  }
  window.OxEditor.onChange(src => {
    if (state.activeBuiltin) return;
    const f = state.files[state.active];
    f.src = src;
    const wasClean = !f.editedAt || (state.compiledAt && f.editedAt <= state.compiledAt);
    f.editedAt = Date.now();
    if (wasClean) renderTabs();          //the dot appears on the first edit only, not per keystroke
    touchWorkspace();
    scheduleCommands(); scheduleSymbols(true); scheduleParse();
  });
  window.OxCompile.wire(() => { touchWorkspace(); scheduleCommands(); if (state.compiled) buildDlMenu(); }, onRawDxc);
  window.OxDiff.initBinaryDiffControls();
  fillCliRef();
  document.body.classList.add("prob-collapsed");

  await bootWorkspace();
  renderWsMenu();
  await restoreFromHash();
  setMode("compile");
  openFile(state.files[state.active] ? state.active : Object.keys(state.files)[0]);
  applyPendingView().catch(() => { });
});
})();
