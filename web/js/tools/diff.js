/* tools/diff.js — three diffs share the Diff tab:
 *  1) Compile mode: cross-backend view of the just-compiled doc (DXIL vs SPIR-V bindings).
 *  2) Inspect mode: sectioned reflection diff of oiSH A vs B (file / entrypoints / binaries /
 *     registers / includes) — a superset of the old flat field map.
 *  3) Inspect mode: binary diff by entrypoint — pick a binary identifier present in A and/or B,
 *     disassemble the stored SPIRV/DXIL on both sides and line-diff the text.
 */
(function () {
"use strict";
const { $, esc, hex8, fmtBytes, lineDiff, crc32c } = window.OxUtil;

/* ---------------------------------------------------------------- field maps (reflection diff) */

function fileSection(doc) {
  const v = doc.compilerVersion;
  return {
    "compiler version": `${v.major}.${v.minor}.${v.patch}`,
    "oiSH format": doc.header.version,
    "source hash": hex8(doc.sourceHash),
    "flags": doc.flags.reflectionOnly ? "reflection-only" : "—",
    "serialized types": ["spirv", "dxil"].filter(b => doc.binaries.some(x => x.sizes[b])).join(", ") || "(none)"
  };
}
function ioSig(list) { return list.map(io => `${io.type}:${io.semantic}${io.idx || ""}`).join(" "); }
function entrySection(doc) {
  const m = {};
  for (const e of doc.entries) {
    const p = `${e.name} `;
    m[p + "stage"] = e.stage + (e.lib ? " (lib)" : "");
    if (e.group) m[p + "numthreads"] = e.group.join("×");
    if (e.waveSize) m[p + "waveSize"] = ["req", "min", "max", "rec"].filter(k => e.waveSize[k]).map(k => `${k} ${e.waveSize[k]}`).join(" ");
    if (e.inputs.length) m[p + "inputs"] = ioSig(e.inputs);
    if (e.outputs.length) m[p + "outputs"] = ioSig(e.outputs);
    if (e.payloadSize != null) m[p + "payload"] = e.payloadSize + " B";
    if (e.intersectionSize != null) m[p + "intersection"] = e.intersectionSize + " B";
    m[p + "binaries"] = String(e.binaryIds.length);
  }
  return m;
}
function binShortKey(b) { return b.lib ? `lib(${b.entryNames.join(",")})` : `${b.entrypoint}·${b.stage}`; }
function binSection(doc) {
  const m = {};
  for (const b of doc.binaries) {
    const p = binShortKey(b) + (b.defines.length ? `[${b.defines.map(d => d.name).join(",")}]` : "") + " ";
    m[p + "model"] = b.model || "—";
    m[p + "extensions"] = b.extensions.join(", ") || "—";
    if (b.dormant.length) m[p + "dormant"] = b.dormant.join(", ");
    if (b.uniforms.length) m[p + "uniforms"] = b.uniforms.map(u => `${u.type} ${u.name}=${u.value}`).join(", ");
    m[p + "vendors"] = b.vendors ? b.vendors.join(", ") : "all";
    m[p + "spirv"] = b.sizes.spirv ? fmtBytes(b.sizes.spirv) : "—";
    m[p + "dxil"] = b.sizes.dxil ? fmtBytes(b.sizes.dxil) : "—";
  }
  return m;
}
function regSection(doc) {
  const m = {};
  for (const r of (doc.registers || [])) {
    const d = r.bindings.dxil, s = r.bindings.spirv;
    m[r.name + " type"] = r.typeStr + (r.arrays.length ? r.arrays.map(a => `[${a}]`).join("") : "");
    m[r.name + " dxil"] = d ? `${d.letter}${d.binding} space${d.space}` : "—";
    m[r.name + " spirv"] = s ? `set${s.set} binding${s.binding}` : (r.push ? "push_constant" : "—");
    m[r.name + " used"] = `spv ${r.used.spirv ? "✓" : "✕"} · dxil ${r.used.dxil ? "✓" : "✕"}`;
    if (r.buffer) m[r.name + " layout"] = `${r.buffer.size} B${r.buffer.padding ? ` (+${r.buffer.padding} pad)` : ""}`;
  }
  return m;
}
function incSection(doc) {
  const m = {};
  for (const i of doc.includes) m[i.path] = "CRC " + hex8(i.crc32c);
  return m;
}

function rowsFor(title, ma, mb) {
  const keys = [...new Set([...Object.keys(ma), ...Object.keys(mb)])];
  if (!keys.length) return "";
  let h = `<div class="diff-row diff-sect"><div>${esc(title)}</div><div></div><div></div></div>`;
  for (const k of keys) {
    const va = ma[k], vb = mb[k], diff = va !== vb;
    const ca = va === undefined ? '<span class="diff-del">—</span>' : esc(va);
    const cb = vb === undefined ? '<span class="diff-add">—</span>' : esc(vb);
    h += `<div class="diff-row${diff ? " diff-diverge" : ""}"><div class="k">${esc(k)}</div><div class="${diff ? "val-x" : ""}">${ca}</div><div class="${diff ? "val-x" : ""}">${cb}</div></div>`;
  }
  return h;
}

function renderReflectionDiff(a, b) {
  $("#diffHead").innerHTML = `<span class="text-body-secondary">oiSH reflection diff</span>
    <b>${esc(a ? a.name : "—")}</b> <i class="bi bi-arrow-left-right"></i> <b>${esc(b ? b.name : "—")}</b>
    <div class="form-check form-switch mb-0 ms-auto"><input class="form-check-input" type="checkbox" id="divOnly">
      <label class="form-check-label" for="divOnly">Divergences only</label></div>`;
  $("#divOnly").checked = document.body.classList.contains("divonly");
  $("#divOnly").addEventListener("change", e => document.body.classList.toggle("divonly", e.target.checked));
  if (!a || !b) { $("#diffBody").innerHTML = `<div class="empty-hint">Load and pick two oiSH files to diff.</div>`; return; }
  let h = `<div class="diff-row diff-head"><div>Field</div><div>${esc(a.name)}</div><div>${esc(b.name)}</div></div>`;
  h += rowsFor("File", fileSection(a), fileSection(b));
  h += rowsFor("Entrypoints", entrySection(a), entrySection(b));
  h += rowsFor("Binaries", binSection(a), binSection(b));
  h += rowsFor("Registers", regSection(a), regSection(b));
  h += rowsFor("Includes", incSection(a), incSection(b));
  $("#diffBody").innerHTML = h;
}

/* ---------------------------------------------------------------- compile-mode cross-backend diff */

function renderCompileDiff(doc) {
  $("#bdiffSection").classList.add("d-none");
  $("#diffHead").innerHTML = doc
    ? `<span class="text-body-secondary">Cross-backend reflection of</span> <b>${esc(doc.sourceName)}</b>
       <span class="chip chip-dxil">DXIL</span><span class="chip chip-spv">SPIR-V</span>
       <div class="form-check form-switch mb-0 ms-auto"><input class="form-check-input" type="checkbox" id="divOnlyC">
         <label class="form-check-label" for="divOnlyC">Divergences only</label></div>`
    : `<span class="text-body-secondary">Compile to see the cross-backend reflection diff.</span>`;
  if (!doc) { $("#diffBody").innerHTML = ""; return; }
  $("#divOnlyC").checked = document.body.classList.contains("divonly");
  $("#divOnlyC").addEventListener("change", e => document.body.classList.toggle("divonly", e.target.checked));
  let h = `<div class="diff-row diff-head"><div>Register</div><div>DXIL</div><div>SPIR-V</div></div>`;
  for (const r of doc.registers) {
    const d = r.bindings.dxil, s = r.bindings.spirv;
    const dv = d ? `${r.typeStr} · register(${d.letter}${d.binding}, space${d.space})` : "—";
    const sv = s ? `set ${s.set} · binding ${s.binding}` : (r.push ? "push_constant block" : "—");
    const diverge = !s || !d || d.binding !== (s ? s.binding : -1);
    h += `<div class="diff-row${diverge ? " diff-diverge" : ""}"><div class="k">${esc(r.name)}</div><div>${esc(dv)}</div><div class="${diverge ? "val-x" : ""}">${esc(sv)}</div></div>`;
  }
  h += `<div class="px-3 py-2 small text-body-secondary">DXIL keeps the per-class <code>register()</code> numbering; the SPIR-V side is a flat per-set binding order — the usual thing to verify before assuming layouts line up.</div>`;
  $("#diffBody").innerHTML = h;
}

/* ---------------------------------------------------------------- binary diff by entrypoint */

/* Pairs are matched on entrypoint/lib × stage × extensions × define names — i.e. the
 * SHBinaryIdentifier tuple minus the shader model and uniform values, so v1 vs v2 of the
 * same shader still pair up when a version bumped its [[oxc::model]]; the model change then
 * shows up inside the diff instead of splitting the pair. */
function pairKey(b) {
  return [b.lib ? "lib:" + b.entryNames.slice().sort().join(",") : b.entrypoint, b.stage,
    b.extensions.join("+"), b.defines.map(d => d.name).join("+")].join("|");
}
function collectPairs(a, b) {
  const map = new Map();
  const put = (doc, side) => doc && doc.binaries.forEach((bin, i) => {
    const k = pairKey(bin);
    if (!map.has(k)) map.set(k, { key: k, a: null, b: null });
    map.get(k)[side] = { bin, idx: i };
  });
  put(a, "a"); put(b, "b");
  return [...map.values()];
}
function pairLabel(p) {
  const bin = (p.a || p.b).bin;
  const who = bin.lib ? `lib(${bin.entryNames.join(",")})` : `${bin.entrypoint} · ${bin.stage}`;
  const sm = p.a && p.b && p.a.bin.model !== p.b.bin.model
    ? `SM ${p.a.bin.model || "?"} → ${p.b.bin.model || "?"}` : (bin.model ? `SM ${bin.model}` : "no identifier");
  const extra = [sm,
    ...(bin.defines.length ? ["[" + bin.defines.map(d => d.name).join(",") + "]"] : []),
    ...(bin.extensions.length ? [bin.extensions.join("+")] : [])].join(" · ");
  const presence = p.a && p.b ? "" : p.a ? "  (only in A)" : "  (only in B)";
  return `${who} · ${extra}${presence}`;
}

let pairs = [], docA = null, docB = null;

function refreshBinaryPairs(a, b) {
  docA = a; docB = b;
  pairs = collectPairs(a, b);
  const sel = $("#bdEntry");
  const noBins = (a && a.flags.reflectionOnly) || (b && b.flags.reflectionOnly);
  $("#bdiffSection").classList.toggle("d-none", !a || !b);
  if (!a || !b) return;
  sel.innerHTML = pairs.map((p, i) => `<option value="${i}">${esc(pairLabel(p))}</option>`).join("")
    || `<option>no binaries</option>`;
  if (noBins) $("#bdSummary").innerHTML = `<span class="text-warning">One side is reflection-only — no stored binaries to diff.</span>`;
  renderBinaryDiff();
}

async function renderBinaryDiff() {
  const p = pairs[+($("#bdEntry").value || 0)];
  const body = $("#bdBody"), sum = $("#bdSummary");
  if (!p) { body.innerHTML = ""; return; }
  const backend = $("#bdDxil").checked ? "dxil" : "spirv";
  const short = backend === "spirv" ? "SPIR-V" : "DXIL";
  const szA = p.a ? p.a.bin.sizes[backend] : 0, szB = p.b ? p.b.bin.sizes[backend] : 0;

  /* summary — sizes + text hash verdict per side */
  const half = async side => {
    if (!side || !side.bin.sizes[backend]) return null;
    const doc = side === p.a ? docA : docB;
    const text = await window.OxAPI.disassembleDocBinary(doc, side.bin, backend);
    return { text, hash: crc32c(text), size: side.bin.sizes[backend] };
  };
  const A = await half(p.a), B = await half(p.b);
  const verdict = !A || !B ? `<span class="chip chip-bad">missing on one side</span>`
    : A.hash === B.hash ? `<span class="chip chip-ok">identical</span>` : `<span class="chip chip-bad">differs</span>`;
  sum.innerHTML = `<span class="text-body-secondary">${short}:</span>
    A ${A ? `${fmtBytes(szA)} · ${hex8(A.hash)}` : "—"} <i class="bi bi-arrow-left-right"></i>
    B ${B ? `${fmtBytes(szB)} · ${hex8(B.hash)}` : "—"} &nbsp; ${verdict}
    <span class="float-end text-body-secondary cli" title="Real path: disassemble the stored binary on both sides, then diff — Compiler_disassemble over SHBinaryInfo::binaries[type]">
      file data --bin -entry ${p.a ? p.a.idx : "—"}/${p.b ? p.b.idx : "—"} -compile-output ${backend === "spirv" ? "spv" : "dxil"} · diffed</span>`;

  if (!A && !B) { body.innerHTML = ""; return; }
  const la = A ? A.text.split("\n") : [], lb = B ? B.text.split("\n") : [];
  const ops = lineDiff(la, lb);
  const changesOnly = $("#bdChangesOnly").checked, CTX = 2;
  let html = "", eqRun = [];
  const flushEq = (before) => {
    if (!eqRun.length) return;
    if (!changesOnly || eqRun.length <= CTX * 2 + 1) {
      html += eqRun.map(l => `<span class="ln ctx"> ${esc(l)}</span>`).join("");
    } else {
      const head = before ? [] : eqRun.slice(0, CTX);
      const tail = eqRun.slice(-CTX);
      html += head.map(l => `<span class="ln ctx"> ${esc(l)}</span>`).join("");
      html += `<span class="ln skip">··· ${eqRun.length - head.length - tail.length} unchanged line(s) ···</span>`;
      html += tail.map(l => `<span class="ln ctx"> ${esc(l)}</span>`).join("");
    }
    eqRun = [];
  };
  let first = true;
  for (const op of ops) {
    if (op.t === "=") { eqRun.push(op.a); continue; }
    flushEq(first); first = false;
    if (op.t === "-") html += `<span class="ln del">-${esc(op.a)}</span>`;
    else html += `<span class="ln add">+${esc(op.b)}</span>`;
  }
  /* trailing context */
  if (eqRun.length) {
    if (!changesOnly || eqRun.length <= CTX) html += eqRun.map(l => `<span class="ln ctx"> ${esc(l)}</span>`).join("");
    else {
      html += eqRun.slice(0, CTX).map(l => `<span class="ln ctx"> ${esc(l)}</span>`).join("");
      html += `<span class="ln skip">··· ${eqRun.length - CTX} unchanged line(s) ···</span>`;
    }
  }
  body.innerHTML = html || `<span class="ln ctx"> (empty)</span>`;
}

function initBinaryDiffControls() {
  $("#bdEntry").addEventListener("change", renderBinaryDiff);
  $("#bdSpv").addEventListener("change", renderBinaryDiff);
  $("#bdDxil").addEventListener("change", renderBinaryDiff);
  $("#bdChangesOnly").addEventListener("change", renderBinaryDiff);
}

window.OxDiff = { renderReflectionDiff, renderCompileDiff, refreshBinaryPairs, renderBinaryDiff, initBinaryDiffControls };
})();
