/* tools/pipeline.js: the Pipeline (oiSP) tab: a pipeline derived from the compiled/loaded
 * oiSH the way `isa disassemble` derives it (SPFile_derivePipeline), every field reflection
 * can't prove listed with its provenance (derived / supplied / assumed), supply controls so
 * the assumed ones can be chosen (-pso-set on the CLI, which takes any field by its path), the
 * `file header` / `file data` views of the resulting oiSP, and the refusal the CLI gives when
 * it would otherwise have to guess between two stages of the same kind. */
(function () {
"use strict";
const { $, esc, hex8, highlightPipeline } = window.OxUtil;

const PROV = { derived: ["prov-derived", "proven by the shader's reflection"], supplied: ["prov-supplied", "you chose it"], assumed: ["prov-assumed", "nobody chose it, a value was picked"] };
const provChip = s => `<span class="chip ${PROV[s][0]}" title="${PROV[s][1]}">${s}</span>`;

function headerCard(sp) {
  const c = sp.header.counts;
  return `<div class="card mb-3"><div class="card-body">
    <div class="d-flex align-items-center gap-2 mb-2">
      <span class="chip chip-sp">oiSP ${esc(sp.header.version)}</span><span class="fw-semibold">${esc(sp.name)}</span>
      ${sp.mockParsed ? '<span class="chip chip-muted">mock parse</span>' : ""}
      <span class="ms-auto small text-body-secondary cli">OxC3 file header -input ${esc(sp.name)}</span>
    </div>
    <div class="kv">
      <div class="k">Pipelines · stages · specializations</div><div>${c.pipelines} · ${c.stages} · ${c.specializations}</div>
      <div class="k">Graphics / ray tracing states</div><div>${c.graphicsStates} / ${c.raytracingStates}</div>
      <div class="k">Stored blend attachments</div><div>${c.blendAttachments} <span class="text-body-secondary">(only the ones blending can reach: enable ? (independent ? popcount(mask) : 1) : 0)</span></div>
      <div class="k">Stored vertex buffers / attributes</div><div>${c.vertexBuffers} / ${c.vertexAttributes} <span class="text-body-secondary">(selected by mask, a graphics state is 64 B on disk against 204 B bound)</span></div>
    </div>
  </div></div>`;
}

function stagesTable(p) {
  return `<table class="ox mb-2"><thead><tr><th>Stage</th><th>Shader</th><th>Entrypoint</th><th>Source hash</th></tr></thead><tbody>
    ${p.stages.map(s => `<tr class="${s.generated ? "opacity-75" : ""}"><td><span class="badge ${s.generated ? "text-bg-warning" : "text-bg-secondary"}">${esc(s.stage)}</span></td>
      <td>${esc(s.shaderFile)}${s.generated ? ' <span class="text-body-secondary">stand-in</span>' : ""}</td><td><span class="tname">${esc(s.entrypoint)}</span></td>
      <td class="text-body-secondary">${s.generated ? "-" : hex8(s.sourceHash)}</td></tr>`).join("")}
  </tbody></table>`;
}

/* The descriptor layout the pipeline compiles against, read out of the oiPL the file embeds for it.
 * -1 means the device's default layout, which has no rows of its own to show. A row's source says who
 * decided it, the same vocabulary the fields table uses. */
function layoutTable(sp, p) {
  const layout = p.layoutIndex >= 0 && sp.layouts ? sp.layouts[p.layoutIndex] : null;
  if (!layout)
    return `<div class="small text-body-secondary mb-2"><i class="bi bi-layout-text-sidebar"></i> Descriptor layout: the device's default</div>`;
  const pair = (b, k) => b.bindings && b.bindings[k] ? `${b.bindings[k].space}:${b.bindings[k].binding}` : "-";
  const extra = b => "samplerId" in b ? (b.samplerId ? `static sampler #${b.samplerId - 1}` : "dynamic sampler")
    : "texture" in b ? `format ${b.texture.formatId}` : b.strideOrLength ? `${b.strideOrLength} B` : "";
  const row = (b, label) => `<tr><td>${label ? `<span class="badge text-bg-secondary">${esc(label)}</span> ` : ""}${esc(b.name || "(unnamed)")}</td>
      <td><span class="tname">${esc(b.type)}</span>${b.count !== 1 ? ` <span class="text-body-secondary">[${b.count || "unbounded"}]</span>` : ""}</td>
      <td class="text-body-secondary">${esc(b.visibility.join(", ") || "-")}</td>
      <td>${esc(pair(b, "spirv"))}</td><td>${esc(pair(b, "dxil"))}</td>
      <td class="text-body-secondary">${esc(extra(b))}</td>
      <td><span class="chip prov-${esc(b.source)}">${esc(b.source)}</span></td></tr>`;
  return `<div class="small fw-semibold mb-1">Descriptor layout <span class="text-body-secondary fw-normal">(oiPL #${p.layoutIndex}, ${layout.bindings.length} rows, ${layout.samplers.length} static samplers)</span></div>
  <table class="ox mb-2"><thead><tr><th>Name</th><th>Type</th><th>Visible to</th><th>SPIR-V set:binding</th><th>DXIL space:register</th><th></th><th>Source</th></tr></thead><tbody>
    ${layout.bindings.map(b => row(b)).join("")}
    ${layout.pushConstant ? row(layout.pushConstant, "push") : ""}
  </tbody></table>`;
}

/* The -pso-set argument that reproduces every supplied field, by the exact path the report prints. */
function psoSetArg(sp, pipelineIdx) {
  const p = sp && sp.pipelines[pipelineIdx];
  if (!p) return "";
  const parts = p.fields.filter(f => f.source === "supplied").map(f => `${f.field}${f.indexed ? `[${f.index}]` : ""}=${f.value}`);
  return parts.length ? ` -pso-set "${parts.join(",")}"` : "";
}

/* Value vocabularies for the enum-typed fields (oxc3_spFieldVocab: the name tables beside the enums in
 * sp_state.h), set once at boot; empty means every editor falls back to the raw number input. */
let VOCAB = {};
function setVocab(v) { VOCAB = v || {}; }

/* The editor a field's domain earns: enum domains get their value names, mask domains get a checkbox
 * per bit, a binary choice gets a toggle, and anything free stays the number input. Every editor
 * supplies the same numeric value the CLI's -pso-set takes. */
function fieldEditor(f, pipelineIdx, i) {

  const ds = `data-supply="${pipelineIdx}:${i}"`;
  const title = 'title="SPFile_supply: choosing a value marks the field supplied"';
  const num = () =>
    `<input class="form-control form-control-sm sp-val" type="number" min="0" step="1" value="${f.value}" ${ds} ${title}>`;
  const enumSel = names =>
    `<select class="form-select form-select-sm sp-val" ${ds} ${title}>` +
    names.map((n, v) => n == null ? "" :
      `<option value="${v}" ${v === f.value ? "selected" : ""}>${esc(n)}</option>`).join("") + "</select>";
  const bits = (names, count) =>
    `<div class="sp-bits" ${ds} ${title}>` + Array.from({ length: count }, (_, b) =>
      names && names[b] === null ? "" :
      `<label class="form-check-label small me-2 text-nowrap"><input type="checkbox" class="form-check-input" data-bit="${b}"` +
      ` ${(f.value >> b) & 1 ? "checked" : ""}> ${esc(names ? names[b] : "target " + b)}</label>`).join("") + "</div>";

  const d = f.domain || "";
  if (VOCAB[d] && VOCAB[d].length) return enumSel(VOCAB[d]);
  if (d === "EWriteMask bits" && VOCAB.EWriteMask) return bits(VOCAB.EWriteMask, 4);
  if (d === "EDepthStencilFlags bits" && VOCAB.EDepthStencilFlags) return bits(VOCAB.EDepthStencilFlags, 3);
  if (d === "ERasterizerFlags bits" && VOCAB.ERasterizerFlags) return bits(VOCAB.ERasterizerFlags, 4);
  if (d.startsWith("EPipelineRaytracingFlags bits") && VOCAB.EPipelineRaytracingFlags)
    return bits(VOCAB.EPipelineRaytracingFlags, 8);
  if (d === "bit per render target") return bits(null, 8);
  if (d === "any color format" && (VOCAB.ETextureFormatIdColor || VOCAB.ETextureFormatId))
    return enumSel(VOCAB.ETextureFormatIdColor || VOCAB.ETextureFormatId);
  if (d === "0 = none, else a depth format" && VOCAB.EDepthStencilFormat)
    return enumSel(VOCAB.EDepthStencilFormat);
  if (d === "0 = 1 sample, 1 = 2, 2 = 4, 3 = 8" && VOCAB.EMSAASamples) return enumSel(VOCAB.EMSAASamples);
  if (/^0 = [^,=]+, 1 = [^,=]+$/.test(d))
    return `<input type="checkbox" class="form-check-input sp-val" ${ds} ${title} ${f.value ? "checked" : ""}>`;
  return num();
}

function fieldsTable(p, pipelineIdx, editable) {
  if (!p.fields.length) return `<div class="small text-body-secondary">No field to specialize: a ${p.type} pipeline derives completely from its shader, so it's exact.</div>`;
  return `<table class="ox"><thead><tr><th>Field</th><th>Value</th><th>Source</th><th>Why reflection can't prove it</th><th>Legal</th></tr></thead><tbody>
    ${p.fields.map((f, i) => `<tr>
      <td><span class="tname">${esc(f.field)}${f.indexed ? `[${f.index}]` : ""}</span></td>
      <td>${editable ? fieldEditor(f, pipelineIdx, i) : f.value}</td>
      <td>${provChip(f.source)}</td>
      <td class="text-body-secondary">${esc(f.reason)}</td><td class="text-body-secondary">${esc(f.domain)}</td></tr>`).join("")}
  </tbody></table>`;
}

function pipelineCard(sp, idx, opts) {
  const p = sp.pipelines[idx];
  const assumed = p.fields.filter(f => f.source === "assumed").length;
  const exact = assumed === 0;
  const cli = opts.doc
    ? `OxC3 isa disassemble -input ${opts.doc.name} -asic live${psoSetArg(sp, idx)} -pso-output ${sp.name}${exact ? "" : " --assume-defaults"}`
    : `OxC3 file data -input ${sp.name}`;
  return `<div class="card mb-3"><div class="card-body">
    <div class="d-flex align-items-center gap-2 mb-2 flex-wrap">
      <span class="fw-semibold">${esc(p.name || "(unnamed)")}</span>
      <span class="badge ${p.type === "compute" ? "text-bg-info" : p.type === "graphics" ? "text-bg-primary" : "text-bg-warning"}">${esc(p.type)}</span>
      ${exact ? '<span class="chip chip-ok" title="every reported field is derived or supplied">exact</span>' : `<span class="chip prov-assumed">${assumed} assumed</span>`}
      ${p.flags.map(f => `<span class="chip chip-muted" title="ESPPipelineFlag">${esc(f)}</span>`).join("")}
      <span class="ms-auto small text-body-secondary cli">${esc(cli)}</span>
    </div>
    ${p.notes.map(n => `<div class="small text-warning mb-2"><i class="bi bi-info-circle"></i> ${esc(n)}</div>`).join("")}
    ${stagesTable(p)}
    ${layoutTable(sp, p)}
    ${fieldsTable(p, idx, !!opts.editable)}
    ${opts.editable ? `<div class="small text-body-secondary mt-2">${exact ? "Every field is derived or supplied, so the CLI compiles this without <code>--assume-defaults</code>." : "The CLI <b>refuses</b> to compile while any field is assumed and lists exactly these, with why and what's legal; <code>--assume-defaults</code> proceeds and prints every assumption above the ISA."} Changing a value here is <code>SPFile_supply</code>: the CLI line above grows a <code>-pso-set</code> with every supplied field (the CLI's only override, taking any field by the path printed), and <code>-pso-input</code> replays a downloaded .oiSP. That's how you see what one PSO change does to the ISA.</div>` : ""}
  </div></div>`;
}

function refusedCard(r, doc) {
  const cands = r.candidates || {};
  const picks = Object.entries(cands).map(([stage, idxs]) => `<div class="d-flex align-items-center gap-2 mb-1">
      <span class="badge text-bg-secondary">${esc(stage)}</span><span class="small text-body-secondary">-entry</span>
      <select class="form-select form-select-sm w-auto" data-pick="${esc(stage)}"><option value="">(pick one)</option>
        ${idxs.map(i => { const b = doc.binaries[i]; return `<option value="${i}">#${i} ${esc(b.entrypoint || b.entryNames.join(","))}${b.defines.length ? " [" + esc(b.defines.map(d => d.name).join(",")) + "]" : ""}${b.extensions.length ? " " + esc(b.extensions.join("+")) : ""}</option>`; }).join("")}
      </select></div>`).join("");
  return `<div class="card mb-3 border-danger"><div class="card-body">
    <div class="d-flex align-items-center gap-2 mb-2"><i class="bi bi-x-octagon text-danger"></i><span class="fw-semibold">Refused</span>
      <span class="ms-auto small text-body-secondary cli">OxC3 isa disassemble -input ${esc(doc ? doc.name : "x.oiSH")} -asic live</span></div>
    <div class="small mb-2">${esc(r.refused)}</div>
    ${picks}
    <div class="small text-body-secondary">Rather than guess between two entries of one stage kind (or invent state), the CLI names what to pick or specialize. Choosing here is the web equivalent of <code>-entry</code>.</div>
  </div></div>`;
}

function printCard(sp, idx, text) {
  return `<div class="card mb-3"><div class="card-body">
    <div class="d-flex align-items-center mb-2"><span class="fw-semibold">file data</span>
      <span class="ms-auto small text-body-secondary cli">OxC3 file data -input ${esc(sp.name)}</span></div>
    <pre class="asm cmdcard mb-0">${highlightPipeline(text)}</pre>
    <div class="small text-body-secondary mt-2">This is what prints above the disassembly of a live ISA run, so an assumed value is never mistaken for something the shader declared.</div>
  </div></div>`;
}

/* ctx: {sp|null, refused|null, doc, editable, printText} */
function html(ctx) {
  if (!ctx || (!ctx.sp && !ctx.refused)) return `<div class="empty-hint"><i class="bi bi-diagram-3 fs-1"></i><h6 class="mt-2">Pipeline (oiSP)</h6>
    <p class="small">Compile something or load an .oiSP. A pipeline is derived from the oiSH the way <code>isa disassemble</code> derives it: compute is exact, ray tracing leaves only <code>rt.*</code> to supply, graphics reports every field no shader signature can carry.</p></div>`;
  if (ctx.refused) return `<div class="container-fluid py-3" style="max-width:1100px">${refusedCard(ctx.refused, ctx.doc)}</div>`;
  const sp = ctx.sp;
  return `<div class="container-fluid py-3" style="max-width:1100px">${headerCard(sp)}${sp.pipelines.map((p, i) => pipelineCard(sp, i, ctx)).join("")}${ctx.printText != null ? printCard(sp, 0, ctx.printText) : ""}</div>`;
}

/* mount + wire supply inputs and -entry picks */
/* One numeric value out of whatever editor the field earned: a bit group sums its checked bits, a
 * toggle is 0/1, everything else is its value. change bubbles, so the group's container listens. */
function supplyValue(ctrl) {
  if (ctrl.classList.contains("sp-bits")) {
    let v = 0;
    ctrl.querySelectorAll("input[data-bit]:checked").forEach(c => { v |= 1 << +c.dataset.bit; });
    return v;
  }
  if (ctrl.type === "checkbox") return ctrl.checked ? 1 : 0;
  return Number(ctrl.value);
}

function mount(el, ctx, handlers) {
  handlers = handlers || {};
  el.innerHTML = html(ctx);
  el.querySelectorAll("[data-supply]").forEach(inp => inp.addEventListener("change", () => {
    const [pi, fi] = inp.dataset.supply.split(":").map(Number);
    const f = ctx.sp.pipelines[pi].fields[fi];
    if (handlers.onSupply) handlers.onSupply(pi, f.field, f.index, supplyValue(inp));
  }));
  el.querySelectorAll("[data-pick]").forEach(sel => sel.addEventListener("change", () => {
    if (sel.value !== "" && handlers.onPick) handlers.onPick(sel.dataset.pick, Number(sel.value));
  }));
}

/* The provenance table on its own, for the ISA tab's override panel; wired through the same onSupply. */
function fieldsHTML(sp, pipelineIdx, editable) {
  return sp && sp.pipelines[pipelineIdx] ? fieldsTable(sp.pipelines[pipelineIdx], pipelineIdx, editable) : "";
}
function wireSupply(el, sp, handlers) {
  el.querySelectorAll("[data-supply]").forEach(inp => inp.addEventListener("change", () => {
    const [pi, fi] = inp.dataset.supply.split(":").map(Number);
    const f = sp.pipelines[pi].fields[fi];
    if (handlers.onSupply) handlers.onSupply(pi, f.field, f.index, supplyValue(inp));
  }));
}

window.OxPipeline = { html, mount, fieldsHTML, wireSupply, psoSetArg, setVocab };
})();
