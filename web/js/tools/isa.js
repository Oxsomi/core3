/* tools/isa.js — the ISA tab: device-specific assembly for the selected binary.
 * Two routes, mirroring `OxC3 isa disassemble -asic <x>`:
 *  - offline: the bundled amdllpc (SPIR-V -> ELF -> amdgpu-dis), gfx11xx/gfx12xx, raster/compute/mesh only,
 *    prepended with the register/resource usage line; reproducible, the corpus goldens pin it. It compiles the
 *    SPIR-V without pipeline context, so the pipeline state doesn't reach it.
 *  - live: a real device, picked by the binary: SPIR-V runs on Vulkan (VK_KHR_pipeline_executable_properties,
 *    cross-vendor but driver-dependent), DXIL on D3D12, which has no introspection API and so validates the
 *    pipeline without disassembling it. It needs a GPU, so it's greyed out unless the host (a VS Code webview
 *    bridging to the native OxC3) says it can answer it. What it compiles is the pipeline below: derived with
 *    provenance, every field overridable here (SPFile_supply = `-pso-set`), stored with `-pso-output`,
 *    replayed with `-pso-input`. */
(function () {
"use strict";
const { $, esc, fmtBytes } = window.OxUtil;

function statsLine(s) {
  return [["SGPRs", s.sgprs], ["VGPRs", s.vgprs], ["code", fmtBytes(s.codeBytes)], ["instrs", s.instrs], ["scratch", s.scratch], ["lds", s.lds]]
    .map(([k, v]) => `<span class="isa-stat"><span class="text-body-secondary">${k}</span> ${v}</span>`).join("");
}

function cliFor(ctx) {
  const { doc, row, asic } = ctx;
  const entry = doc.binaries.length > 1 ? ` -entry ${row.binIdx}` : "";
  if (asic !== "live") return `OxC3 isa disassemble -input ${doc.name} -asic ${asic}${entry}`;
  const set = ctx.sp ? window.OxPipeline.psoSetArg(ctx.sp, 0) : "";
  return `OxC3 isa disassemble -input ${doc.name} -asic live${entry}${set} -pso-output ${doc.name.replace(/\.(oiSH|hlsl)$/i, "")}.oiSP${ctx.exact ? "" : " --assume-defaults"}`;
}

/* ctx: {doc, bin, row, targets, asic, entrypoint, result, error, sp, refused, printText, exact, caps, busy, stateOpen} */
function html(ctx) {
  if (!ctx || !ctx.doc || !ctx.bin) return `<div class="empty-hint"><i class="bi bi-cpu fs-1"></i><h6 class="mt-2">ISA (device-specific assembly)</h6>
    <p class="small">Compile something or load an oiSH, pick a binary in the strip, then disassemble it to AMD ISA for a gfx target. RDNA ISA, VGPR/SGPR pressure, scratch and LDS; what DXIL/SPIR-V hide.</p></div>`;
  const { doc, bin, row, caps } = ctx;
  const liveOk = !!(caps && caps.liveIsa);
  const who = bin.lib ? `lib(${bin.entryNames.join(",")})` : `${bin.entrypoint} · ${bin.stage}`;
  const entryOpts = bin.lib ? `<select id="isaEntry" class="form-select form-select-sm w-auto" title="-entry picks a binary index; inside a lib the entrypoint to lower">
      ${bin.entryNames.map(n => `<option ${n === ctx.entrypoint ? "selected" : ""}>${esc(n)}</option>`).join("")}</select>` : "";
  const liveTitle = liveOk ? "a real device through the host's native OxC3" : "needs a real GPU (Vulkan for SPIR-V, D3D12 for DXIL): greyed out in the browser, available when this page is hosted in VS Code with the native OxC3";
  const head = `<div class="px-3 py-2 border-bottom bg-body-tertiary small d-flex align-items-center gap-2 flex-wrap">
      <span class="fw-semibold"><i class="bi bi-cpu"></i> ISA</span>
      <span class="text-body-secondary">#${row.binIdx} ${esc(who)} · ${row.backend === "spirv" ? "SPIR-V" : "DXIL"}</span>
      <select id="isaAsic" class="form-select form-select-sm w-auto" title="-asic: a gfx target the bundled offline compiler accepts (isa devices), or live">
        ${ctx.targets.map(t => `<option value="${t.asic}" ${t.asic === ctx.asic ? "selected" : ""}>${t.asic} (${t.arch})</option>`).join("")}
        <option value="live" ${ctx.asic === "live" ? "selected" : ""} ${liveOk ? "" : "disabled"} title="${esc(liveTitle)}">live${liveOk ? "" : " (native only)"}</option>
      </select>
      ${entryOpts}
      <button id="isaRun" class="btn btn-sm btn-teal" ${ctx.busy || (ctx.asic === "live" && !liveOk) ? "disabled" : ""}><i class="bi bi-braces-asterisk me-1"></i>Disassemble</button>
      <button id="isaState" class="btn btn-sm btn-outline-secondary" title="the pipeline state a live run compiles, with every field overridable (-pso-set)">
        <i class="bi bi-sliders"></i> Pipeline state${ctx.sp ? ` <span class="badge ${ctx.exact ? "text-bg-success" : "text-bg-warning"}">${ctx.exact ? "exact" : ctx.sp.pipelines[0].fields.filter(f => f.source === "assumed").length + " assumed"}</span>` : ""}</button>
      <span class="ms-auto text-body-secondary cli">${esc(cliFor(ctx))}</span>
    </div>`;

  /* the override panel: the same provenance table the Pipeline tab edits, because it's the same oiSP */
  let statePanel = "";
  if (ctx.stateOpen) {
    const inner = ctx.refused
      ? `<div class="small text-danger"><i class="bi bi-x-octagon"></i> ${esc(ctx.refused.refused)} (pick in the Pipeline tab)</div>`
      : ctx.sp
        ? window.OxPipeline.fieldsHTML(ctx.sp, 0, true)
        : `<div class="small text-body-secondary">No pipeline derived (reflection-only oiSH, or a compile error).</div>`;
    statePanel = `<div class="px-3 py-2 border-bottom isa-state">
      <div class="small text-body-secondary mb-2">The offline route compiles the SPIR-V alone, so these don't reach it. They are exactly what a <b>live</b> (or device-free Mesa) run compiles: every field can be overridden here, the CLI line above grows a <code>-pso-set</code> with each one you supply, and <b>Download → .oiSP</b> is the same state for <code>-pso-input</code>.</div>
      ${inner}
    </div>`;
  }

  let body = "";
  if (ctx.asic === "live" && !liveOk) {
    body = `<div class="p-3">
      <div class="card mb-3"><div class="card-body">
        <div class="d-flex align-items-center gap-2 mb-2"><span class="fw-semibold">Live ISA</span><span class="chip chip-muted">native only</span>
          <span class="ms-auto small text-body-secondary cli">${esc(cliFor(ctx))}</span></div>
        <div class="small text-body-secondary">
          <p class="mb-2"><code>-asic live[:index]</code> creates a real device, builds the pipeline and reads back whatever the driver exposes. <b>The binary picks the backend</b>: SPIR-V runs on Vulkan, DXIL on D3D12 (<code>-compile-output spv|dxil</code> forces one), which is what makes DXIL inspectable at all, since the offline path can't lower it. Vulkan implements <code>VK_KHR_pipeline_executable_properties</code>, so statistics come from any driver supporting it and disassembly <b>text</b> only from AMD's driver and Mesa (RADV/ANV/Turnip/NVK/PanVK); NVIDIA, Qualcomm and ARM return statistics only, and NVIDIA nothing for ray tracing. D3D12 has no such API at all, so a DXIL run validates the pipeline against the driver and prints the state, without ISA.</p>
          <p class="mb-2">A browser has no device to hand it, so the option stays greyed out here. Hosted in <b>VS Code</b>, the extension bridges to the native OxC3 on the machine and this same button runs the command above, with the state from the <i>Pipeline state</i> panel passed as <code>-pso-input</code> / <code>-pso-set</code>, so the result is the real driver's, not a mock.</p>
          <p class="mb-0">Every graphics stage the shader declares is bound and only the missing ends of the chain are generated (a pixel-only shader gets a vertex stand-in, a vertex-only one a pixel stand-in); every stage of a ray tracing lib is bound and hit shaders are grouped by order. Tessellation and mesh/task aren't inspectable live yet.</p>
        </div>
      </div></div>
      ${ctx.printText != null ? `<div class="mb-1 small text-body-secondary">Pipeline state that prints above the ISA</div><pre class="asm cmdcard">${esc(ctx.printText)}</pre>` : ""}
    </div>`;
  } else if (ctx.error) {
    body = `<div class="p-3"><div class="alert alert-danger small mb-2"><i class="bi bi-x-octagon"></i> ${esc(ctx.error)}</div>
      <div class="small text-body-secondary">Offline AMD ISA is derived from SPIR-V only and only for raster, compute and mesh stages (ray tracing has no offline path: amdllpc emits an empty code section without pipeline context). Use <code>-asic live</code> on a machine with the device for those.</div></div>`;
  } else if (ctx.result) {
    const r = ctx.result;
    body = `<div class="px-3 py-1 border-bottom small d-flex align-items-center gap-3 flex-wrap isa-stats">
        <span class="chip chip-isa">${esc(r.asic)} · ${esc(r.arch)}</span><span class="text-body-secondary">${esc(r.entrypoint)} · ${esc(r.stage)}</span>${statsLine(r.stats)}
      </div>${r.report ? `<pre class="asm cmdcard m-3 mb-0">${esc(r.report)}</pre>` : ""}<pre class="asm"><code id="isaText">${esc(r.text)}</code></pre>`;
  } else {
    body = `<div class="p-3 small text-body-secondary">Pick a target and hit Disassemble. The offline path compiles the stored SPIR-V with the bundled <code>amdllpc</code> for that gfx target and disassembles the ELF with <code>amdgpu-dis</code>, so it's reproducible (the corpus goldens pin it); it supports gfx11xx (RDNA3), gfx1150 (RDNA3.5) and gfx12xx (RDNA4). The same view is available inline as <code>file data -input x.oiSH -asic gfx1100</code>.</div>`;
  }
  const foot = `<div class="px-3 py-2 small text-body-secondary border-top">Other vendors: the device-free Mesa route (RADV/ACO for all AMD generations, Intel brw for Gen9 to Xe2) is prototyped and is the one that can run in the browser and take the pipeline state above, but it isn't in the CLI yet; Qualcomm Adreno and Mali offline compilers stay manual.</div>`;
  return head + statePanel + body + foot;
}

function mount(el, ctx, handlers) {
  handlers = handlers || {};
  el.innerHTML = html(ctx);
  const run = $("#isaRun"), asic = $("#isaAsic"), entry = $("#isaEntry"), stateBtn = $("#isaState");
  if (asic) asic.addEventListener("change", () => handlers.onChange && handlers.onChange({ asic: asic.value, entrypoint: entry ? entry.value : null }));
  if (entry) entry.addEventListener("change", () => handlers.onChange && handlers.onChange({ asic: asic.value, entrypoint: entry.value }));
  if (run) run.addEventListener("click", () => handlers.onRun && handlers.onRun({ asic: asic.value, entrypoint: entry ? entry.value : null }));
  if (stateBtn) stateBtn.addEventListener("click", () => handlers.onToggleState && handlers.onToggleState());
  if (ctx && ctx.sp) window.OxPipeline.wireSupply(el, ctx.sp, handlers);
}

window.OxIsa = { html, mount };
})();
