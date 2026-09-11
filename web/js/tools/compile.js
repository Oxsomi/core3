/* tools/compile.js: Compile-mode logic: gather toolbar options (they map 1:1 to
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
    noOpt: $("#optNoOpt").checked,
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
  if (o.noOpt) parts.push("--no-opt");
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
  // and `-threads 0|50%|4` / `-include-dir <dir>`: the web version uses the file tree as roots.
}

/* ---- derived DXC invocations --------------------------------------------------------- */

/* An argv element the shell would split or unquote gets quoted, so a printed line pastes back as run. */
function quoteArg(a) {
  return /[\s"']/.test(a) ? '"' + a.replace(/(["\\])/g, "\\$1") + '"' : a;
}

/* The real lens: one card per compile the driver spawns, each line the argv the compiler itself built
 * (Compiler_buildCompileArgs through oxc3_getCompileArgs), so nothing here can drift from the run. */
function realCommandsHtml(compiles, card) {
  let html = `<div class="small text-body-secondary mb-2"><i class="bi bi-check-circle"></i>
    argv read back from the compiler: these are the exact dxc invocations this compile runs.</div>`;
  for (const c of compiles) {
    const t = c.args.indexOf("-T");
    const profile = t >= 0 ? c.args[t + 1] : "";
    const label = `${c.binaryType === "spirv" ? "SPIR-V" : "DXIL"} · ${profile}`
      + (c.lib ? " library" : (c.entrypoint ? ` (${c.entrypoint})` : ""))
      + (c.requiresLink ? " + link" : "");
    html += card(label, c.binaryType === "spirv" ? "chip-spv" : "chip-dxil", "dxc " + c.args.map(quoteArg).join(" "));
    if (c.amendedSource)
      html += `<details class="small mt-1"><summary>uniforms amend the input source; this line compiles the
        amended text below, then the link step specializes it</summary>
        <pre class="asm cmdcard mt-1">${esc(c.amendedSource)}</pre></details>`;
  }
  return html;
}

let renderSeq = 0;

async function renderCommands(activeName, project) {
  const o = opts();
  $("#cmdCli").textContent = cliLine(activeName, o);
  $("#cmdNote").textContent = o.reflectionOnly
    ? "Reflection only (OxC3 shader reflect): same compiles, then binaries are stripped and the oiSH is rewritten with ESHSettingsFlags_ReflectionOnly."
    : (o.split ? "--split: each backend is written to its own lean file (x.spv.oiSH, x.dxil.oiSH) instead of one bulky oiSH." : "");

  /* each derived line is editable and runnable: that's the "raw DXC" lens, starting from what OxC3 would run */
  let cardId = 0;
  const card = (label, css, cmd) =>
    `<div class="mb-1 mt-2 d-flex align-items-center gap-2"><span class="chip ${css}">${esc(label)}</span>
      <button class="btn btn-sm btn-outline-secondary dxc-run ms-auto" data-dxc="${cardId}" title="run this line as is through DXC; the output is a standalone binary (no oiSH, no annotations processed)"><i class="bi bi-play-fill"></i> Run with DXC</button></div>` +
    `<textarea class="asm cmdcard dxc-line form-control" data-dxc="${cardId++}" rows="3" spellcheck="false">${esc(cmd)}</textarea>`;

  const seq = ++renderSeq;
  const real = await window.OxAPI.getCompileArgs(activeName, project, o);
  if (seq !== renderSeq) return;                     // superseded by a newer render while awaiting

  if (!real) {

    /* Without a module this refuses the way every compiler answer does (api.js noModule): the CLI line
     * above is only flag spelling, so it stays; nothing fabricated renders under it. With the module a
     * null answer means the source doesn't parse right now, and the previous listing stays rather than
     * flashing away mid-edit (the parseEntrypoints convention). */
    if (window.OxAPI.backend !== "wasm")
      $("#cmdList").innerHTML = `<div class="text-body-secondary small">The derived dxc lines need the
        compiler module, which isn't running. The OxC3 CLI line above applies either way.</div>`;

    return;
  }

  let html = realCommandsHtml(real, card);

  /* and a free line for anything DXC accepts, pre-filled with the first derived one */
  const first = (html.match(/<textarea[^>]*>([\s\S]*?)<\/textarea>/) || [])[1] || `dxc -T cs_6_5 -E main -HV 2021 -O3 -spirv ${esc(activeName)}`;
  html += `<div class="mb-1 mt-3 d-flex align-items-center gap-2"><span class="chip chip-muted">raw DXC</span>
      <span class="small text-body-secondary">any flag line, run as is; output lands in SPV / DXIL mode as a standalone binary</span>
      <button class="btn btn-sm btn-teal dxc-run ms-auto" data-dxc="raw"><i class="bi bi-play-fill"></i> Run with DXC</button></div>
    <textarea class="asm cmdcard dxc-line form-control" data-dxc="raw" rows="3" spellcheck="false">${first}</textarea>
    <pre id="dxcLog" class="asm cmdcard mt-2 d-none"></pre>`;

  $("#cmdList").innerHTML = html || `<div class="text-body-secondary small">No entrypoints in this file: nothing to compile (see --ignore-empty-files).</div>`;

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
  ["optRefl", "optDebug", "optNoOpt", "optSplit", "optKeepReg", "wUnusedReg", "wUnusedConst", "wPad", "wIgnoreEmpty"]
    .forEach(id => $("#" + id).addEventListener("change", onOptionChange));
}

window.OxCompile = { targets, opts, run, renderCommands, wire, cliLine };
})();
