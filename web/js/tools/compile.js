/* tools/compile.js — Compile-mode logic: gather toolbar options (they map 1:1 to
 * `OxC3 shader compile` flags), run the compile through OxAPI, and render the Command tab. */
(function () {
"use strict";
const { $, $$, esc } = window.OxUtil;

function targets() {
  if ($("#tSpv").checked) return ["spv"];
  if ($("#tDxil").checked) return ["dxil"];
  return ["spv", "dxil"];
}
function opts() {
  return {
    targets: targets(),
    reflectionOnly: $("#optRefl").checked,
    debug: $("#optDebug").checked,
    split: $("#optSplit").checked,
    keepRegisters: $("#optKeepReg").checked,
    warnUnusedRegisters: $("#wUnusedReg").checked,
    warnUnusedConstants: $("#wUnusedConst").checked,
    warnBufferPadding: $("#wPad").checked,
    ignoreEmptyFiles: $("#wIgnoreEmpty").checked
  };
}

/* ---- equivalent CLI line ------------------------------------------------------------- */
function cliLine(fileName, o) {
  const parts = ["OxC3 shader compile", `-input ${fileName}`, `-output out/${fileName.replace(/\.hlsl$/, ".oiSH")}`];
  const t = o.targets.length === 2 ? "all" : o.targets[0];
  if (t !== "all") parts.push(`-compile-output ${t}`);           // default is all
  if (o.debug) parts.push("--debug");
  if (o.split) parts.push("--split");
  if (o.keepRegisters) parts.push("--keep-registers");
  if (o.warnUnusedRegisters) parts.push("--warn-unused-registers");
  if (o.warnUnusedConstants) parts.push("--warn-unused-constants");
  if (o.warnBufferPadding) parts.push("--warn-buffer-padding");
  if (o.ignoreEmptyFiles) parts.push("--ignore-empty-files");
  if (o.reflectionOnly) return parts.join(" ").replace("shader compile", "shader reflect")
    .replace(/ -compile-output \w+/, "");                        // reflect has no -compile-output
  return parts.join(" ");
  // Also valid: `OxC3 compile shaders -format HLSL …` (the pre-`shader`-category spelling),
  // and `-threads 0|50%|4` / `-include-dir <dir>` — the web version uses the file tree as roots.
}

/* ---- derived DXC invocations --------------------------------------------------------- */
/* Illustrative but built from the flags the real compiler passes (src/shader_compiler/compiler.cpp):
 * -T/-E/-HV, __OXC* defines, -Zi -Qembed_debug vs -O3, -Wconversion -Wdouble-promotion,
 * -enable-16bit-types, -enable-payload-qualifiers, and on SPIR-V: -spirv -fvk-use-dx-layout,
 * -fspv-target-env=…, -fspv-extension=…, -fhlsl-unused-resource-bindings=reserve-all,
 * -fvk-invert-y -fvk-use-dx-position-w (gfx), -fspv-entrypoint-name=main (standalone stages). */
const SPV_EXT_FLAG = { RayQuery: "SPV_KHR_ray_query", Multiview: "SPV_KHR_multiview", "16BitTypes": "SPV_KHR_16bit_storage",
  ComputeDeriv: "SPV_NV_compute_shader_derivatives", RayMotionBlur: "SPV_NV_ray_tracing_motion_blur",
  DescriptorHeap: "SPV_EXT_descriptor_heap" };

function renderCommands(activeName, project) {
  const o = opts();
  $("#cmdCli").textContent = cliLine(activeName, o);

  const doc = window.OxMock.analyze(activeName, project);        // preview only; wasm port can reuse its parse step
  const groups = new Map();                                       // profile -> entries
  for (const e of doc.entries) {
    const key = e.lib ? "lib" : (window.OxMock.STAGE_PROFILE[e.stage] || "cs");
    if (!groups.has(key)) groups.set(key, []);
    groups.get(key).push(e);
  }
  const V = window.OxMock.VERSION;
  /* each derived line is editable and runnable: that's the "raw DXC" lens, starting from what OxC3 would run */
  let cardId = 0;
  const card = (label, css, cmd) =>
    `<div class="mb-1 mt-2 d-flex align-items-center gap-2"><span class="chip ${css}">${esc(label)}</span>
      <button class="btn btn-sm btn-outline-secondary dxc-run ms-auto" data-dxc="${cardId}" title="run this line as is through DXC; the output is a standalone binary (no oiSH, no annotations processed)"><i class="bi bi-play-fill"></i> Run with DXC</button></div>` +
    `<textarea class="asm cmdcard dxc-line form-control" data-dxc="${cardId++}" rows="3" spellcheck="false">${esc(cmd)}</textarea>`;
  let html = "";

  for (const [prof, ents] of groups) {
    const lib = prof === "lib";
    /* one representative binary of this group carries model/extensions for the flags */
    const rep = doc.binaries.find(b => lib ? b.lib : (!b.lib && ents.some(e => e.name === b.entrypoint))) || doc.binaries[0];
    const model = (rep ? rep.model : "6.5").replace(".", "_");
    const exts = rep ? rep.extensions : [];
    const is16 = exts.includes("16BitTypes");
    const defs = ["-D__OXC", `-D__OXC_MAJOR=${V.major}`, `-D__OXC_MINOR=${V.minor}`, `-D__OXC_PATCH=${V.patch}`,
      ...exts.map(e => `-D__OXC_EXT_${e.toUpperCase()}`),
      ...(ents.some(e => window.OxMock.LIB_STAGES.has(e.stage) && e.stage !== "node") ? ["-D__OXC_EXT_RAYTRACING"] : [])].join(" ");
    const dbg = o.debug ? "-Zi -Qembed_debug " : "-O3 ";
    const common = `-T ${lib ? "lib" : prof}_${model} -HV 2021 ${is16 ? "-enable-16bit-types " : ""}${exts.includes("PAQ") ? "-enable-payload-qualifiers " : ""}${dbg}-Wconversion -Wdouble-promotion ${defs} -I .`;
    const label = lib
      ? `library lib_${model} (${ents.map(e => e.name + " [" + e.stage + "]").join(", ")})`
      : `${prof}_${model} (${ents.map(e => e.name).join(", ")})`;
    const perEntry = lib ? [""] : ents.map(e => `-E ${e.name} `);

    if (o.targets.includes("spv")) {
      const env = window.OxMock.spvEnvFor(rep || { extensions: [], lib, entryNames: [] }, doc.entries).env;
      const spvExt = exts.map(e => SPV_EXT_FLAG[e]).filter(Boolean).map(x => `-fspv-extension=${x}`).join(" ");
      //--keep-registers upgrades the unused-binding mode from reserve-all to keep-all and preserves SPIRV bindings
      const spvKeep = o.keepRegisters
        ? "-fhlsl-unused-resource-bindings=keep-all -fspv-preserve-bindings "
        : "-fhlsl-unused-resource-bindings=reserve-all ";
      for (const ee of perEntry)
        html += card("SPIR-V · " + label, "chip-spv",
          `dxc ${common} ${ee}${lib ? "" : "-fspv-entrypoint-name=main "}-spirv -fvk-use-dx-layout -fvk-invert-y -fvk-use-dx-position-w -fspv-target-env=${env} ${spvKeep}${spvExt ? spvExt + " " : ""}${activeName}`);
    }
    if (o.targets.includes("dxil"))
      for (const ee of perEntry)
        html += card("DXIL · " + label, "chip-dxil",
          `dxc ${common} ${ee}${o.keepRegisters ? "-fhlsl-unused-resource-bindings=keep-all " : ""}${lib ? "-Qkeep_reflect_in_dxil -reflect-functions " : ""}${activeName}`);
  }

  /* and a free line for anything DXC accepts, pre-filled with the first derived one */
  const first = (html.match(/<textarea[^>]*>([\s\S]*?)<\/textarea>/) || [])[1] || `dxc -T cs_6_5 -E main -HV 2021 -O3 -spirv ${esc(activeName)}`;
  html += `<div class="mb-1 mt-3 d-flex align-items-center gap-2"><span class="chip chip-muted">raw DXC</span>
      <span class="small text-body-secondary">any flag line, run as is; output lands in SPV / DXIL mode as a standalone binary</span>
      <button class="btn btn-sm btn-teal dxc-run ms-auto" data-dxc="raw"><i class="bi bi-play-fill"></i> Run with DXC</button></div>
    <textarea class="asm cmdcard dxc-line form-control" data-dxc="raw" rows="3" spellcheck="false">${first}</textarea>
    <pre id="dxcLog" class="asm cmdcard mt-2 d-none"></pre>`;

  $("#cmdList").innerHTML = html || `<div class="text-body-secondary small">No entrypoints in this file — nothing to compile (see --ignore-empty-files).</div>`;
  $("#cmdNote").textContent = o.reflectionOnly
    ? "Reflection only (OxC3 shader reflect): same compiles, then binaries are stripped and the oiSH is rewritten with ESHSettingsFlags_ReflectionOnly."
    : (o.split ? "--split: each backend is written to its own lean file (x.spv.oiSH, x.dxil.oiSH) instead of one bulky oiSH." : "");

  $("#cmdList").querySelectorAll(".dxc-run").forEach(btn => btn.addEventListener("click", async () => {
    const line = $("#cmdList").querySelector(`textarea[data-dxc="${btn.dataset.dxc}"]`).value.replace(/^\s*dxc\s+/, "");
    btn.setAttribute("disabled", "");
    const r = await window.OxAPI.compileRaw(line, activeName, project);
    btn.removeAttribute("disabled");
    const log = $("#dxcLog");
    log.classList.remove("d-none"); log.textContent = r.log;
    if (r.ok && rawHandler) rawHandler(r);
  }));
}

let rawHandler = null;

/* ---- run ------------------------------------------------------------------------------ */
async function run(state) {
  const o = opts();
  const name = state.active;
  const btn = $("#compileBtn"), spin = $("#spin");
  spin.classList.remove("d-none"); btn.setAttribute("disabled", "");
  $("#stCompiled").innerHTML = '<span class="spinner-border spinner-border-sm"></span> compiling…';
  const t0 = performance.now();
  const { doc, diags } = await window.OxAPI.compileFile(name, state.files, o);
  const ms = Math.round(performance.now() - t0);
  spin.classList.add("d-none"); btn.removeAttribute("disabled");
  return { doc, diags, ms, opts: o };
}

function wire(onOptionChange, onRawResult) {
  rawHandler = onRawResult || null;
  $$('input[name="target"]').forEach(r => r.addEventListener("change", onOptionChange));
  ["optRefl", "optDebug", "optSplit", "optKeepReg", "wUnusedReg", "wUnusedConst", "wPad", "wIgnoreEmpty"]
    .forEach(id => $("#" + id).addEventListener("change", onOptionChange));
}

window.OxCompile = { targets, opts, run, renderCommands, wire, cliLine };
})();
