/* tools/binary.js — the standalone "SPV / DXIL" mode's own tools. A loaded binary is a document like any
 * other (reflected from its bytes, see OxAPI.reflectBinary), so Reflection / SPIR-V / DXIL / ISA / Pipeline / Diff
 * and the oiSH tab (assemble into oiSH) are the regular tabs. What's left here is the strip above them:
 * `OxC3 shader assemble` (SPIR-V text -> .spv) and Compiler_getUniqueEntrypoints (no CLI verb yet). */
(function () {
"use strict";
const { $, esc, fmtBytes, download } = window.OxUtil;

let open = false;

function render(state, handlers) {
  handlers = handlers || {};
  const name = state.binActive;
  const entry = name ? state.standalone[name] : null;
  $("#binPane").innerHTML = `
    <div class="d-flex align-items-center gap-2 flex-wrap small">
      <span class="text-body-secondary"><i class="bi bi-file-binary"></i> ${entry ? `${esc(name)} · ${entry.type === "spirv" ? "SPIR-V" : "DXIL"} · ${fmtBytes(entry.bytes.length)}` : "no binary selected"}</span>
      <button id="sbEntries" class="btn btn-sm btn-outline-secondary" ${entry ? "" : "disabled"}
        title="Compiler_getUniqueEntrypoints — list entrypoints embedded in a (lib) binary; no CLI verb yet">List entrypoints</button>
      <button id="sbDl" class="btn btn-sm btn-outline-secondary" ${entry ? "" : "disabled"}><i class="bi bi-download me-1"></i>disassembly .txt</button>
      <button id="asmToggle" class="btn btn-sm btn-outline-secondary ms-auto"><i class="bi bi-hammer me-1"></i>Assemble SPIR-V text → .spv</button>
    </div>
    <div id="sbMeta" class="small text-body-secondary mt-1"></div>
    <div id="asmCard" class="mt-2 ${open ? "" : "d-none"}">
      <div class="d-flex align-items-center gap-2 mb-1 small"><span class="fw-semibold">Assemble</span>
        <span class="text-body-secondary cli">OxC3 shader assemble -input x.txt -output x.spv</span>
        <select id="asmType" class="form-select form-select-sm w-auto ms-auto">
          <option value="spirv">SPIR-V (spirv-as)</option>
          <option value="dxil" disabled title="mirrors the CLI: DXIL assembly not supported yet">DXIL — not supported yet</option>
        </select>
        <button id="asmGo" class="btn btn-sm btn-teal"><i class="bi bi-hammer me-1"></i>Assemble</button>
      </div>
      <textarea id="asmIn" class="form-control font-monospace" rows="7" spellcheck="false">; SPIR-V assembly text (spirv-as syntax)
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main"
               OpExecutionMode %main LocalSize 8 8 1
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd</textarea>
      <div id="asmMsg" class="small text-body-secondary mt-1"></div>
    </div>`;

  $("#asmToggle").addEventListener("click", () => { open = !open; $("#asmCard").classList.toggle("d-none", !open); });

  $("#sbEntries").addEventListener("click", async () => {
    if (!entry) return;
    const eps = await window.OxAPI.getUniqueEntrypoints(entry.type, entry.bytes);
    $("#sbMeta").innerHTML = `${esc(name)} — entrypoints: ` +
      eps.map(e => `<code>${esc(e.name)}</code> <span class="text-body-secondary">(${esc(e.stage)})</span>`).join(", ") +
      (eps[0] && eps[0].note ? ` <span class="text-warning">· ${esc(eps[0].note)}</span>` : "");
  });

  $("#sbDl").addEventListener("click", async () => {
    if (!entry) return;
    const text = entry.text || await window.OxAPI.disassemble(entry.type, entry.bytes);
    download(name.replace(/\.(spv|dxil)$/i, "") + ".txt", text, "text/plain");
  });

  $("#asmGo").addEventListener("click", async () => {
    try {
      const bytes = await window.OxAPI.assemble($("#asmType").value, $("#asmIn").value);
      const outName = `assembled.${Object.keys(state.standalone).length}.spv`;
      $("#asmMsg").innerHTML = `<span class="text-success">assembled ${fmtBytes(bytes.length)} → ${esc(outName)}</span> <span class="text-body-secondary">(mock bytes until Compiler_assemble is wired; the result is a document like any loaded binary)</span>`;
      if (handlers.onAssembled) handlers.onAssembled(outName, { type: "spirv", bytes });
    } catch (err) {
      $("#asmMsg").innerHTML = `<span class="text-danger">${esc(err.message)}</span>`;
    }
  });
}

window.OxBinary = { render };
})();
