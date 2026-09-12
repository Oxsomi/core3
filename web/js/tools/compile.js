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
    argv read back from the compiler: these are the exact dxc invocations this compile runs.
    <b>Run with DXC isn't wired up yet</b>; the lines are editable as a scratchpad.</div>`;
  for (const c of compiles) {
    const t = c.args.indexOf("-T");
    const profile = t >= 0 ? c.args[t + 1] : "";
    const label = `${c.binaryType === "spirv" ? "SPIR-V" : "DXIL"} · ${profile}`
      + (c.lib ? " library" : (c.entrypoint ? ` (${c.entrypoint})` : ""))
      + (c.requiresLink ? " + link" : "");
    html += card(label, c.binaryType === "spirv" ? "chip-spv" : "chip-dxil", "dxc " + c.args.map(quoteArg).join(" "));
    if (c.amendedSource) {
      /* Only the injected preamble is worth reading: everything after the second #line marker is the
       * file's own text restated verbatim, so the details cut there. */
      const marker = c.amendedSource.indexOf('#line 1 "', 1);
      const nl = marker >= 0 ? c.amendedSource.indexOf("\n", marker) : -1;
      const preamble = nl >= 0 ? c.amendedSource.slice(0, nl + 1) : c.amendedSource;
      html += `<details class="small mt-1"><summary>uniforms prepend this to the input source (the file's
        own text follows unchanged); the link steps under it then specialize each permutation</summary>
        <pre class="asm cmdcard mt-1">${esc(preamble)}</pre></details>`;
    }

    /* the compile alone is not the final binary when links follow: each link step below freezes one
     * permutation's uniforms (and pins entrypoint + stage for [shader] libs) into a finished blob */
    for (const l of (c.links || [])) {
      const who = (l.entrypoint ? `${l.entrypoint}` : "library") + ` (${l.profile})`;
      const uni = (l.uniforms || []).map(u => `${u.name} = ${u.value}`).join(", ");
      if (c.binaryType === "spirv")
        html += `<div class="small ms-3 mt-1"><span class="chip chip-muted">then</span>
            final ${esc(who)}${uni ? " · " + esc(uni) : ""}:
            <code>spirv-opt ${esc(l.spirvOpt.join(" "))} in.spv -o out.spv</code></div>`;
      else {
        html += `<div class="small ms-3 mt-1"><span class="chip chip-muted">then</span>
            final ${esc(who)}${uni ? " · " + esc(uni) : ""}:
            <code>IDxcLinker ${l.entrypoint ? "-E " + esc(l.entrypoint) + " " : ""}-T ${esc(l.profile)}</code>
            over libs ${esc(l.libs.join(", "))}${l.linkArgs.length ? " · " + esc(l.linkArgs.join(" ")) : ""}
            (programmatic; dxc has no CLI spelling for this link)</div>`;
        if (l.uniformsHlsl)
          html += `<details class="small ms-4"><summary>the "uniforms" library it links, compiled
              <code>dxc ${esc(l.uniformsArgs.join(" "))}</code></summary>
              <pre class="asm cmdcard mt-1">${esc(l.uniformsHlsl)}</pre></details>`;
      }
    }
  }
  return html;
}

let renderSeq = 0;
let lastRealFor = null;                              // file the current #cmdList cards belong to

async function renderCommands(activeName, project) {
  const o = opts();
  $("#cmdCli").textContent = cliLine(activeName, o);
  $("#cmdNote").textContent = o.reflectionOnly
    ? "Reflection only (OxC3 shader reflect): same compiles, then binaries are stripped and the oiSH is rewritten with ESHSettingsFlags_ReflectionOnly."
    : (o.split ? "--split: each backend is written to its own lean file (x.spv.oiSH, x.dxil.oiSH) instead of one bulky oiSH." : "");

  /* each derived line is editable: that's the "raw DXC" lens, starting from what OxC3 actually runs.
   * The run buttons stay DISABLED until the compiler grows a raw compile entry (api.js compileRaw):
   * the printed argv is real, running an edited line is not wired up yet. */
  let cardId = 0;
  const card = (label, css, cmd) =>
    `<div class="mb-1 mt-2 d-flex align-items-center gap-2"><span class="chip ${css}">${esc(label)}</span>
      <button class="btn btn-sm btn-outline-secondary dxc-run ms-auto" data-dxc="${cardId}" disabled
        title="Not wired up yet: the argv is real, but running an edited line needs a raw compile entry in the compiler."><i class="bi bi-play-fill"></i> Run with DXC</button></div>` +
    `<textarea class="asm cmdcard dxc-line form-control" data-dxc="${cardId++}" rows="3" spellcheck="false">${esc(cmd)}</textarea>`;

  const seq = ++renderSeq;
  const real = await window.OxAPI.getCompileArgs(activeName, project, o);
  if (seq !== renderSeq) return;                     // superseded by a newer render while awaiting

  if (!real) {

    /* Without a module this refuses the way every compiler answer does (api.js noModule): the CLI line
     * above is only flag spelling, so it stays; nothing fabricated renders under it. With the module a
     * null answer means the source doesn't parse right now: mid-edit of the SAME file the previous
     * listing stays (the parseEntrypoints convention), but another file's cards would be lies, so
     * switching to a file that doesn't parse says so instead. */
    if (window.OxAPI.backend !== "wasm")
      $("#cmdList").innerHTML = `<div class="text-body-secondary small">The derived dxc lines need the
        compiler module, which isn't running. The OxC3 CLI line above applies either way.</div>`;

    else if (lastRealFor !== activeName) {
      lastRealFor = null;
      $("#cmdList").innerHTML = `<div class="text-body-secondary small">This source doesn't parse right
        now, so there are no compiles to print. The diagnostics in the editor say why.</div>`;
    }

    return;
  }

  lastRealFor = activeName;

  let html = realCommandsHtml(real, card);

  /* and a free line for anything DXC accepts, pre-filled with the first derived one */
  const first = (html.match(/<textarea[^>]*>([\s\S]*?)<\/textarea>/) || [])[1] || `dxc -T cs_6_5 -E main -HV 2021 -O3 -spirv ${esc(activeName)}`;
  html += `<div class="mb-1 mt-3 d-flex align-items-center gap-2"><span class="chip chip-muted">raw DXC</span>
      <span class="small text-body-secondary">any flag line; running it isn't wired up yet, so this is a scratchpad for now</span>
      <button class="btn btn-sm btn-teal dxc-run ms-auto" data-dxc="raw" disabled
        title="Not wired up yet: running a flag line needs a raw compile entry in the compiler."><i class="bi bi-play-fill"></i> Run with DXC</button></div>
    <textarea class="asm cmdcard dxc-line form-control" data-dxc="raw" rows="3" spellcheck="false">${first}</textarea>
    <pre id="dxcLog" class="asm cmdcard mt-2 d-none"></pre>`;

  $("#cmdList").innerHTML = html || `<div class="text-body-secondary small">No entrypoints in this file: nothing to compile (see --ignore-empty-files).</div>`;

  /* what the disabled run buttons will trigger once the raw compile entry exists; disabled buttons
   * never fire, so this stays dormant rather than reaching the mock compileRaw */
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
