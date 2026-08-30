/* tools/inspect.js — everything that renders an SHDocument.
 * Shared by Compile mode (the doc it just produced) and Inspect-oiSH mode (loaded docs).
 * The level of detail intentionally matches `OxC3 file data` / SHFile_print --verbose:
 * per-binary identifiers, dual-backend bindings + used flags, oiSB buffer layouts,
 * IO semantics, group/wave sizes, payload sizes, include CRCs. */
(function () {
"use strict";
const { $, esc, hex8, fmtBytes } = window.OxUtil;

/* ---------------------------------------------------------------- small bits */

const clsCss = { CBV: "reg-cbv", SRV: "reg-srv", UAV: "reg-uav", SMP: "reg-smp" };
const stageBadge = e =>
  `<span class="badge ${e.lib ? "text-bg-warning" : "text-bg-secondary"}">${esc(e.stage)}</span>`;
const usedTag = (u, label) =>
  `<span class="${u ? "used-y" : "used-n"}" title="${label}: ${u ? "Used" : "Unused"}">${label} ${u ? "✓" : "✕"}</span>`;
const node = (inner, expandable, children) =>
  `<li${children ? "" : ""}><div class="tnode${expandable ? " exp" : ""}">${expandable ? '<i class="bi bi-chevron-down tw"></i>' : '<span class="tw"></span>'}${inner}</div>${children ? `<ul>${children}</ul>` : ""}</li>`;
const leaf = inner => node(inner, false, null);
const dims = arrays => arrays && arrays.length ? `<span class="tdim">${arrays.map(a => `[${a}]`).join("")}</span>` : "";

function annoLines(bin) {
  const L = [];
  const active = bin.extensions.filter(e => !bin.dormant.includes(e));
  if (!bin.model) { L.push("// standalone binary: no identifier (nothing declared a model, extensions or defines)"); L.push("// assemble it into an oiSH to give it one"); return L; }
  L.push(`[[oxc::model("${bin.model}")]]`);
  if (bin.extensions.length) L.push(`[[oxc::extension(${bin.extensions.map(e => `"${e}"`).join(", ")})]]`);
  if (active.length && bin.dormant.length) L.push(`//[[oxc::active_extension(${active.map(e => `"${e}"`).join(", ")})]]  // dormant: ${bin.dormant.join(", ")}`);
  if (bin.defines.length) L.push(`[[oxc::defines(${bin.defines.map(d => d.value ? `"${d.name}" = "${d.value}"` : `"${d.name}"`).join(", ")})]]`);
  if (bin.uniforms.length) L.push(`[[oxc::uniforms(${bin.uniforms.map(u => `${u.type} ${u.name} = ${u.value}`).join(", ")})]]`);
  if (bin.vendors) L.push(...bin.vendors.map(v => `[[oxc::vendor("${v}")]]`));
  if (bin.binAnno) L.push(`[[oxc::binary(${bin.binAnno.map(b => `"${b}"`).join(", ")})]]`);
  return L;
}

/* ---------------------------------------------------------------- oiSB layout */

function sbVarsHTML(vars, depth) {
  return vars.map(v => {
    const used = v.used || { spirv: true, dxil: true };
    const usedStr = used.spirv && used.dxil ? "SPIRV & DXIL: Used"
      : used.spirv ? "SPIRV: Used" : used.dxil ? "DXIL: Used" : "SPIRV & DXIL: Unused";
    return `<div class="sbline${used.spirv || used.dxil ? "" : " opacity-50"}">
      <span class="off">0x${(v.offset >>> 0).toString(16).padStart(8, "0")}</span>:
      <span class="tname">${esc(v.name)}</span>${dims(v.arrays)}
      <span class="text-body-secondary">(${usedStr})</span>:
      <span class="ttype">${esc(v.type)}</span>${v.stride != null ? ` <span class="stride">(Stride: ${v.stride})</span>` : ""}
    </div>${v.children ? `<div class="nest">${sbVarsHTML(v.children, depth + 1)}</div>` : ""}`;
  }).join("");
}
function sbHTML(reg) {
  const b = reg.buffer;
  if (!b) return "";
  const head = `<span class="text-body-secondary">${b.packed ? "tightly packed" : "cbuffer layout"} · ${b.size} B${b.padding ? ` · <span class="text-warning">${b.padding} B padding</span>` : ""}${b.elem ? ` · element <span class="ttype">${esc(b.elem)}</span>` : ""}</span>`;
  return node(`<span class="tname">Layout</span> ${head} <span class="text-body-secondary small">(oiSB)</span>`,
    true, `<li><div class="sb px-2">${sbVarsHTML(b.vars, 0)}</div></li>`);
}

/* ---------------------------------------------------------------- registers */

function bindingsHTML(r) {
  const d = r.bindings.dxil, s = r.bindings.spirv;
  const dxil = d ? `<code>register(${d.letter}${d.binding}, space${d.space})</code>` : "—";
  const spv = s ? `<code>[[vk::binding(${s.binding}, ${s.set})]]</code>`
    : (r.push ? `<span class="text-body-secondary">push_constant</span>` : "—");
  return { dxil, spv };
}
function registerRow(r) {
  const b = bindingsHTML(r);
  const tex = r.texture ? `<div class="text-body-secondary">${esc(r.texture.primitive)}${r.texture.format ? " · " + esc(r.texture.format) : (r.flags.write ? " · unknown format" : "")}</div>` : "";
  const att = r.attachment != null ? `<div class="text-body-secondary">input_attachment_index = ${r.attachment}</div>` : "";
  return `<tr class="${!r.used.spirv && !r.used.dxil ? "unused" : ""}">
    <td><span class="reg ${clsCss[r.cls]}">${r.cls}</span></td>
    <td><span class="tname">${esc(r.name)}</span>${dims(r.arrays)}</td>
    <td><span class="ttype">${esc(r.typeStr)}</span>${tex}${att}</td>
    <td>${b.dxil}</td><td>${b.spv}</td>
    <td>${usedTag(r.used.spirv, "SPIRV")}<br>${usedTag(r.used.dxil, "DXIL")}</td>
  </tr>${r.buffer ? `<tr class="sbrow"><td></td><td colspan="5"><ul class="tree m-0 p-0">${sbHTML(r)}</ul></td></tr>` : ""}`;
}
function registersHTML(regs) {
  if (!regs.length) return `<li class="px-3 py-1 text-body-secondary">no registers</li>`;
  return `<li><table class="ox">
    <thead><tr><th></th><th>Register</th><th>Type</th><th>DXIL</th><th>SPIR-V</th><th>Used</th></tr></thead>
    <tbody>${regs.map(registerRow).join("")}</tbody></table></li>`;
}

/* ---------------------------------------------------------------- entries */

function ioTable(rows, kind) {
  if (!rows.length) return "";
  return node(`<span class="tname">${kind}</span> <span class="text-body-secondary">${rows.length}</span>`, true,
    `<li><table class="ox"><thead><tr><th>#</th><th>Type</th><th>Semantic</th></tr></thead><tbody>` +
    rows.map((io, i) => `<tr><td>${i}</td><td><span class="ttype">${esc(io.type)}</span></td><td>${esc(io.semantic)}${(io.idx > 0 || io.semantic === "TEXCOORD") ? io.idx : ""}</td></tr>`).join("") +
    `</tbody></table></li>`);
}
function waveHTML(w) {
  if (!w) return "";
  const parts = [];
  if (w.req) parts.push(`required ${w.req}`);
  if (w.min) parts.push(`min ${w.min}`); if (w.max) parts.push(`max ${w.max}`); if (w.rec) parts.push(`recommended ${w.rec}`);
  return parts.length ? leaf(`<span class="tname">WaveSize</span> <span class="text-body-secondary">${parts.join(" · ")}</span>`) : "";
}
function entryNode(e, doc) {
  const meta = [];
  if (e.group) meta.push(`numthreads ${e.group.join("×")}`);
  if (e.lib) meta.push("library");
  let kids = "";
  kids += leaf(`<span class="tname">Binaries</span> <span class="text-body-secondary">[ ${e.binaryIds.join(", ")} ]</span>`);
  kids += ioTable(e.inputs, "Inputs") + ioTable(e.outputs, "Outputs");
  if (e.group) kids += leaf(`<span class="tname">Thread count</span> <span class="text-body-secondary">${e.group.join(", ")}</span>`);
  kids += waveHTML(e.waveSize);
  if (e.intersectionSize != null) kids += leaf(`<span class="tname">Intersection size</span> <span class="text-body-secondary">${e.intersectionSize} B</span>`);
  if (e.payloadSize != null) kids += leaf(`<span class="tname">Payload size</span> <span class="text-body-secondary">${e.payloadSize} B</span>`);
  void doc;
  return node(`<i class="bi bi-box-arrow-in-right ${e.lib ? "text-warning" : "text-info"}"></i>
    <span class="tname">${esc(e.name)}</span>${stageBadge(e)}<span class="text-body-secondary">${meta.join(" · ")}</span>`, true, kids);
}

/* ---------------------------------------------------------------- binaries */

function binTitle(bin, i) {
  const chips = ["spirv", "dxil"].filter(b => bin.sizes[b])
    .map(b => `<span class="chip ${b === "spirv" ? "chip-spv" : "chip-dxil"}">${b === "spirv" ? "SPV" : "DXIL"} ${fmtBytes(bin.sizes[b])}</span>`).join(" ");
  const who = bin.lib ? `<span class="chip chip-lib">lib</span> <span class="tname">${esc(bin.entryNames.join(", "))}</span>`
    : `<span class="tname">${esc(bin.entrypoint)}</span>${stageBadge(bin)}`;
  return `<span class="text-body-secondary">#${i}</span> ${who} <span class="text-body-secondary">${bin.model ? "SM " + esc(bin.model) : "no identifier"}</span> ${chips}`;
}
function binNode(bin, i) {
  let kids = "";
  kids += node(`<span class="tname">Identifier</span>`, true,
    `<li><pre class="asm p-2 m-0">${esc(annoLines(bin).join("\n"))}</pre></li>`);
  kids += node(`<span class="tname">Registers</span> <span class="text-body-secondary">${bin.registers.length}</span>`, true, registersHTML(bin.registers));
  return node(binTitle(bin, i), true, kids);
}

/* ---------------------------------------------------------------- top-level views */

function reflectionHTML(doc) {
  if (!doc) return `<ul class="tree"><li class="p-3 text-body-secondary">Nothing to reflect yet.</li></ul>`;
  let out = "";
  out += node(`<i class="bi bi-file-earmark-binary text-info"></i><span class="tname">${esc(doc.name)}</span>
      <span class="text-body-secondary">${doc.standalone ? "standalone binary · reflected from the bytes · content" : `OxC3 ${doc.compilerVersion.major}.${doc.compilerVersion.minor}.${doc.compilerVersion.patch} · source`} ${hex8(doc.sourceHash)}${doc.flags.reflectionOnly ? " · reflection-only" : ""}${doc.mockParsed ? " · mock parse" : ""}${doc.assembledFrom ? " · assembled from " + esc(doc.assembledFrom) : ""}</span>`,
    true,
    leaf(`<span class="tname">Serialized binary types</span> <span class="text-body-secondary">${serializedTypes(doc).join(" ") || "(none)"}</span>`));
  out += node(`<span class="tname">Entrypoints</span> <span class="text-body-secondary">${doc.entries.length}</span>`, true,
    doc.entries.map(e => entryNode(e, doc)).join("") || `<li class="px-3 text-body-secondary">none</li>`);
  out += node(`<span class="tname">Binaries</span> <span class="text-body-secondary">${doc.binaries.length}</span>`, true,
    doc.binaries.map(binNode).join("") || `<li class="px-3 text-body-secondary">none${doc.flags.reflectionOnly ? " (reflection-only)" : ""}</li>`);
  out += node(`<span class="tname">Includes</span> <span class="text-body-secondary">${doc.includes.length}</span>`, true,
    doc.includes.map((inc, i) => leaf(`<span class="text-body-secondary">${i}</span> <span class="tname">${esc(inc.path)}</span> <span class="toff">CRC32C ${hex8(inc.crc32c)}</span>`)).join("") || `<li class="px-3 text-body-secondary">none</li>`);
  return `<ul class="tree">${out}</ul>`;
}
function serializedTypes(doc) {
  const t = [];
  if (doc.binaries.some(b => b.sizes.spirv)) t.push('<span class="chip chip-spv">SPIRV</span>');
  if (doc.binaries.some(b => b.sizes.dxil)) t.push('<span class="chip chip-dxil">DXIL</span>');
  return t;
}

/* A standalone binary has no oiSH yet: the tab offers to assemble one (the planned `shader assemble -> oiSH`). */
function assembleCardHTML(doc) {
  const e = doc.entries[0], b = doc.binaries[0];
  const stages = window.OxMock.STAGES;
  const models = ["6.5", "6.6", "6.7", "6.8", "6.9", "6.10"];
  return `<div class="card mb-3"><div class="card-body">
    <div class="d-flex align-items-center gap-2 mb-2"><span class="fw-semibold">Assemble into oiSH</span>
      <span class="chip chip-muted" title="no CLI verb yet: shader assemble only produces a .spv">planned</span>
      <span class="ms-auto small text-body-secondary cli">OxC3 shader assemble -input ${esc(doc.name)} -output ${esc(doc.name.replace(/\.(spv|dxil)$/i, ""))}.oiSH -stage … -model …</span></div>
    <div class="small text-body-secondary mb-2">A bare ${b.sizes.spirv ? "SPIR-V" : "DXIL"} binary carries no identifier, so a pipeline can't pick it. Give it the one its source would have declared and it becomes a real oiSH: Reflection, Diff, Combine and the pipeline all work on it, and it can be combined with the other backend's file later.</div>
    <div class="d-flex flex-wrap align-items-end gap-3">
      <div><label class="small text-body-secondary">Entrypoint</label><input id="asmEntry" class="form-control form-control-sm" value="${esc(e.name)}"></div>
      <div><label class="small text-body-secondary">Stage</label><select id="asmStage" class="form-select form-select-sm">${stages.map(s => `<option ${s === e.stage ? "selected" : ""}>${s}</option>`).join("")}</select></div>
      <div><label class="small text-body-secondary">[[oxc::model]]</label><select id="asmModel" class="form-select form-select-sm">${models.map(m => `<option ${m === (b.model || "6.5") ? "selected" : ""}>${m}</option>`).join("")}</select></div>
      <div style="min-width:220px"><label class="small text-body-secondary">[[oxc::extension]]</label><select id="asmExts" class="form-select form-select-sm" multiple size="4">${window.OxMock.EXTENSIONS.map(x => `<option>${x}</option>`).join("")}</select></div>
      <div style="min-width:140px"><label class="small text-body-secondary">[[oxc::vendor]] (none = all)</label><select id="asmVendors" class="form-select form-select-sm" multiple size="4">${window.OxMock.VENDORS.map(v => `<option>${v}</option>`).join("")}</select></div>
      <div><label class="small text-body-secondary">Output</label><input id="asmName" class="form-control form-control-sm" value="${esc(doc.name.replace(/\.(spv|dxil)$/i, ""))}.oiSH"></div>
      <button id="asmOishGo" class="btn btn-sm btn-teal"><i class="bi bi-box-arrow-in-down me-1"></i>Assemble</button>
    </div>
  </div></div>`;
}

/* The oiSH tab: `file header` + `file data` summary + `shader feature_set` + `shader includes`. */
function oishViewHTML(doc) {
  if (!doc) return `<div class="empty-hint"><i class="bi bi-box fs-1"></i><p class="small mt-2">Compile something or load an .oiSH.</p></div>`;
  if (doc.standalone) return assembleCardHTML(doc);
  const v = doc.compilerVersion;
  const head = `
  <div class="card mb-3"><div class="card-body">
    <div class="d-flex align-items-center gap-2 mb-2">
      <span class="fw-semibold">${esc(doc.name)}</span>
      ${serializedTypes(doc).join(" ")}
      ${doc.flags.reflectionOnly ? '<span class="chip chip-muted">reflection-only</span>' : ""}
      ${doc.combinedFrom ? `<span class="chip chip-ok" title="${esc(doc.combinedFrom.join(" + "))}">combined</span>` : ""}
      <span class="ms-auto small text-body-secondary cli">OxC3 file header -input ${esc(doc.name)}</span>
    </div>
    <div class="kv">
      <div class="k">oiSH format version</div><div>${esc(doc.header.version)}</div>
      <div class="k">Compiler version</div><div>${v.major}.${v.minor}.${v.patch}</div>
      <div class="k">Source hash (CRC32C)</div><div>${hex8(doc.sourceHash)}</div>
      <div class="k">SPIRV / DXIL size type</div><div>${esc(doc.header.sizeTypes.spirv)} / ${esc(doc.header.sizeTypes.dxil)}</div>
      <div class="k">Counts</div><div>${doc.binaries.length} binaries · ${doc.entries.length} stages · ${doc.includes.length} includes</div>
    </div>
  </div></div>`;

  const feat = `
  <div class="card mb-3"><div class="card-body">
    <div class="d-flex align-items-center mb-2"><span class="fw-semibold">Feature set</span>
      <span class="ms-auto small text-body-secondary cli">OxC3 shader feature_set -input ${esc(doc.name)}</span></div>
    <table class="ox"><thead><tr><th>Binary</th><th>SM</th><th>Sizes</th><th>Extensions (dormant struck)</th><th>Vendors</th></tr></thead><tbody>
    ${doc.binaries.map((b, i) => `<tr>
      <td>#${i} ${b.lib ? `<span class="chip chip-lib">lib</span> ${esc(b.entryNames.join(", "))}` : esc(b.entrypoint) + " · " + esc(b.stage)}</td>
      <td>${b.model ? esc(b.model) : "—"}</td>
      <td>${b.sizes.spirv ? "spv " + fmtBytes(b.sizes.spirv) : ""}${b.sizes.spirv && b.sizes.dxil ? " · " : ""}${b.sizes.dxil ? "dxil " + fmtBytes(b.sizes.dxil) : ""}${!b.sizes.spirv && !b.sizes.dxil ? "—" : ""}</td>
      <td>${b.extensions.map(e => b.dormant.includes(e) ? `<s class="text-body-secondary" title="dormant: declared but not detected in the executable">${esc(e)}</s>` : esc(e)).join(", ") || "—"}</td>
      <td>${b.vendors ? esc(b.vendors.join(", ")) : "all"}</td></tr>`).join("")}
    </tbody></table>
  </div></div>`;

  const entries = `
  <div class="card mb-3"><div class="card-body">
    <div class="d-flex align-items-center mb-2"><span class="fw-semibold">Entrypoints</span>
      <span class="ms-auto small text-body-secondary cli">OxC3 shader entrypoints -input ${esc(doc.name)} --verbose</span></div>
    <div class="small">${doc.entries.map(e => `${esc(e.name)} <span class="text-body-secondary">(${esc(e.stage)}${e.group ? " · " + e.group.join("×") : ""}${e.payloadSize != null ? " · payload " + e.payloadSize + " B" : ""})</span>`).join(" · ") || "—"}</div>
    <div class="small text-body-secondary mt-1">Full per-entry reflection (IO semantics, wave sizes, binary refs) lives in the Reflection tab.</div>
  </div></div>`;

  const incs = `
  <div class="card mb-3"><div class="card-body">
    <div class="d-flex align-items-center mb-2"><span class="fw-semibold">Includes</span>
      <span class="ms-auto small text-body-secondary cli">OxC3 shader includes -input ${esc(doc.name)}</span></div>
    ${doc.includes.map(i => `<span class="badge text-bg-secondary me-1 mb-1">${esc(i.path)} <span class="text-body-secondary">CRC ${hex8(i.crc32c)}</span></span>`).join("") || '<span class="small text-body-secondary">none</span>'}
    <div class="small text-body-secondary mt-2">Include CRCs are what <code>file combine</code> checks, and what a hot-reload file watcher would compare.</div>
  </div></div>`;

  return head + feat + entries + incs;
}

/* ---------------------------------------------------------------- binary strip + disassembly */

function binRows(doc, backends) {
  const rows = [];
  if (!doc || doc.flags.reflectionOnly) return rows;
  doc.binaries.forEach((bin, i) => {
    for (const bk of ["spirv", "dxil"]) {
      const short = bk === "spirv" ? "spv" : "dxil";
      if (!bin.sizes[bk] || (backends && !backends.includes(short))) continue;
      const who = bin.lib ? `lib(${bin.entryNames.join(",")})` : `${bin.entrypoint} · ${bin.stage}`;
      const extra = [bin.model || "raw", ...(bin.defines.length ? ["[" + bin.defines.map(d => d.name).join(",") + "]"] : []),
        ...(bin.extensions.length ? [bin.extensions.join("+")] : [])].join(" · ");
      rows.push({ binIdx: i, backend: bk, label: `#${i} ${who} · ${bk === "spirv" ? "SPIR-V" : "DXIL"} · ${extra}` });
    }
  });
  return rows;
}

async function showBinary(doc, row, switchTab = true) {
  const bin = doc.binaries[row.binIdx];
  const isSpv = row.backend === "spirv";
  const size = fmtBytes(bin.sizes[row.backend]);
  const env = window.OxMock.spvEnvFor(bin, doc.entries);
  const sm = bin.model ? (bin.lib ? "lib_" + bin.model.replace(".", "_") : "SM " + bin.model) : "no identifier";
  $("#binMeta").textContent = isSpv
    ? `SPIR-V ${env.ver} · ${env.env} · ${size}`
    : `DXIL ${sm} · signed · ${size}`;
  const text = await window.OxAPI.disassembleDocBinary(doc, bin, row.backend);
  const cli = doc.standalone
    ? `shader disassemble -input ${doc.name}`
    : `file data -input ${doc.name} --bin -entry ${row.binIdx} -compile-output ${isSpv ? "spv" : "dxil"}`;
  if (isSpv) {
    $("#spvHead").innerHTML = `<span class="chip chip-spv">SPIR-V ${env.ver}</span>
      <span class="ms-2 text-body-secondary">${esc(bin.lib ? bin.entryNames.join(", ") : bin.entrypoint)} · ${env.env} · ${size}</span>
      <span class="float-end text-body-secondary cli">OxC3 ${esc(cli)}</span>`;
    $("#spvAsm").textContent = text;
    if (switchTab) document.querySelector('[data-bs-target="#p-spv"]').click();
  } else {
    $("#dxilHead").innerHTML = `<span class="chip chip-dxil">DXIL ${sm}</span>
      <span class="ms-2 text-body-secondary">${esc(bin.lib ? bin.entryNames.join(", ") : bin.entrypoint)} · ${size}</span>
      <span class="float-end text-body-secondary cli">OxC3 ${esc(cli)}</span>`;
    $("#dxilAsm").textContent = text;
    if (switchTab) document.querySelector('[data-bs-target="#p-dxil"]').click();
  }
}

window.OxInspect = { reflectionHTML, oishViewHTML, assembleCardHTML, binRows, showBinary, annoLines };
})();
