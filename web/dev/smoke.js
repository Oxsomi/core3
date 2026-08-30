/* smoke.js — boots index.html in jsdom (local scripts only, CodeMirror stubbed) and walks
 * all three modes plus the Symbols / Pipeline / ISA tabs. Run: npm i jsdom && node dev/smoke.js
 * (or point NODE_PATH at a node_modules that has jsdom). */
const { JSDOM } = require("jsdom");
const fs = require("fs"), path = require("path");
const ROOT = require("path").join(__dirname, "..");

const html = fs.readFileSync(path.join(ROOT, "index.html"), "utf8");
const dom = new JSDOM(html, { url: "https://shader.oxsomi.com/", pretendToBeVisual: true, runScripts: "outside-only" });
const { window } = dom;
global.window = window; global.document = window.document;

/* ---- stubs for CDN scripts ---- */
window.CodeMirror = function () {};
window.CodeMirror.defineMIME = () => {};
window.CodeMirror.fromTextArea = () => {
  let value = "", cbs = {};
  return {
    setValue: v => { value = v; (cbs.change || []).forEach(f => f()); },
    getValue: () => value,
    on: (ev, f) => { (cbs[ev] = cbs[ev] || []).push(f); },
    setOption: () => {}, addOverlay: () => {}, setSize: () => {},
    clearGutter: () => {}, eachLine: () => {}, addLineClass: () => {}, removeLineClass: () => {},
    markText: () => ({ clear: () => {} }), setGutterMarker: () => {},
    setCursor: () => {}, scrollIntoView: () => {}, focus: () => {}, lineCount: () => 999
  };
};
Object.defineProperty(window.navigator, "clipboard", { value: { writeText: async () => {} } });
window.TextEncoder = TextEncoder;
window.URL.createObjectURL = () => "blob:mock"; window.URL.revokeObjectURL = () => {};
window.performance = window.performance || { now: () => Date.now() };
window.prompt = () => null;

/* editor.js drives cm via closure; our stub's setValue fires change — mimic real enough.
 * But editor.open() suppresses the callback itself, so that's fine. */

for (const f of ["js/util.js", "js/mock.js", "js/mock_formats.js", "js/api.js", "js/editor.js",
  "js/tools/compile.js", "js/tools/inspect.js", "js/tools/symbols.js", "js/tools/pipeline.js", "js/tools/isa.js",
  "js/tools/diff.js", "js/tools/binary.js", "js/app.js"]) {
  window.eval(fs.readFileSync(path.join(ROOT, f), "utf8"));
}

const $ = s => window.document.querySelector(s);
const sleep = ms => new Promise(r => setTimeout(r, ms));
let failures = 0;
function check(name, cond, extra) {
  console.log((cond ? "  ok " : "FAIL ") + name + (cond ? "" : "  " + (extra || "")));
  if (!cond) failures++;
}

(async () => {
  window.document.dispatchEvent(new window.Event("DOMContentLoaded", { bubbles: true }));
  await sleep(50);

  check("boot: rail lists project files", $("#railTree").textContent.includes("lighting.hlsl"));
  check("boot: builtins listed", $("#builtinFiles").textContent.includes("@types.hlsli"));
  check("boot: CLI map filled", $("#cliRefBody").textContent.includes("shader disassemble"));

  /* ---- compile mode ---- */
  $("#wUnusedReg").checked = true;
  $("#compileBtn").click();
  await sleep(900);
  check("compile: status shows binaries", $("#stCompiled").textContent.includes("binaries"), $("#stCompiled").textContent);
  check("compile: reflection tree has entrypoint + binary + include CRC",
    $("#reflView").textContent.includes("main") && $("#reflView").textContent.includes("CRC32C"));
  check("compile: register table shows dual bindings",
    $("#reflView").innerHTML.includes("vk::binding") && $("#reflView").innerHTML.includes("register(b0"));
  check("compile: oiSB layout rendered", $("#reflView").innerHTML.includes("0x00000040"));
  check("compile: oiSH tab has feature set + header", $("#oishView").textContent.includes("Feature set") && $("#oishView").textContent.includes("Source hash"));
  check("compile: problems include unused-register warning", $("#probBody").textContent.includes("--warn-unused-registers"));
  check("compile: binary strip populated", $("#binSel").options.length >= 2, "options=" + $("#binSel").options.length);
  check("compile: SPIR-V view auto-filled", $("#spvAsm").textContent.includes("OpEntryPoint"));
  check("compile: command tab CLI line", $("#cmdCli").textContent.startsWith("OxC3 shader compile -input lighting.hlsl"));
  check("compile: dxc card present", $("#cmdList").textContent.includes("-T lib_6_5"), $("#cmdList").textContent.slice(0, 120));
  check("compile: download enabled", !$("#dlBtn").disabled);
  await (async () => { $("#dlMenu").innerHTML && null; })();
  check("compile: dl menu has oiSH + binary entries", $("#dlMenu").textContent.includes(".oiSH") && $("#dlMenu").textContent.includes("selected binary"));

  /* ---- symbols (oiSR) ---- */
  await sleep(300);
  check("symbols: tree rendered with the root namespace", $("#symView").textContent.includes("Namespace") && $("#symView").textContent.includes("(anonymous)"));
  check("symbols: entrypoint function with its annotation", $("#symView").textContent.includes("Function") && $("#symView").textContent.includes('[shader("compute")]'));
  check("symbols: register + cbuffer member", $("#symView").textContent.includes("Register") && $("#symView").textContent.includes("viewProj"));
  check("symbols: builtin includes collapsed", $("#symView").textContent.includes("builtin-include symbols collapsed"));
  check("symbols: location is a goto link", $("#symView").querySelector("[data-goto]") != null);
  check("symbols: CLI line", $("#symView").textContent.includes("OxC3 shader reflect-symbols -input lighting.hlsl"));

  /* ---- pipeline (oiSP): compute derives completely ---- */
  check("pipeline: compute pipeline is exact", $("#psoView").textContent.includes("compute") && $("#psoView").textContent.includes("exact"));
  check("pipeline: header card counts", $("#psoView").textContent.includes("Pipelines · stages · specializations"));
  check("pipeline: file data print", $("#psoView").textContent.includes("; Pipeline state (compute), 1 stage(s), 0 assumed field(s)"));

  /* ---- ISA: offline amdllpc on the compute binary ---- */
  check("isa: asic list from isa devices", $("#isaAsic") && [...$("#isaAsic").options].some(o => o.value === "gfx1201"));
  $("#isaRun").click(); await sleep(600);
  check("isa: stats line + ISA text", $("#isaView").textContent.includes("SGPRs") && $("#isaView").textContent.includes("s_endpgm"), $("#isaView").textContent.slice(0, 160));
  check("isa: CLI line names the asic", $("#isaView").textContent.includes("isa disassemble -input") && $("#isaView").textContent.includes("-asic gfx1100"));
  check("isa: live option greyed out in a browser", [...$("#isaAsic").options].find(o => o.value === "live").disabled);
  $("#isaAsic").value = "live"; $("#isaAsic").dispatchEvent(new window.Event("change")); await sleep(50);
  check("isa: live route explained as native only", $("#isaView").textContent.includes("native only") && $("#isaView").textContent.includes("VK_KHR_pipeline_executable_properties"));
  check("isa: disassemble disabled on the greyed route", $("#isaRun").disabled);
  $("#isaState").click(); await sleep(50);
  check("isa: pipeline state panel opens", $("#isaView").querySelector(".isa-state") != null);
  check("dl menu: .oiSR and .oiSP entries", $("#dlMenu").textContent.includes(".oiSR") && $("#dlMenu").textContent.includes(".oiSP"));

  /* ---- graphics pipeline: post.hlsl has two pixel variants -> refused with an -entry picker, then assumed fields ---- */
  [...$("#railTree").querySelectorAll(".fitem")].find(f => f.dataset.name === "post.hlsl").click();
  $("#compileBtn").click(); await sleep(1100);
  check("pipeline: duplicate pixel entries refused", $("#psoView").textContent.includes("Refused") && $("#psoView").querySelector("[data-pick]") != null, $("#psoView").textContent.slice(0, 200));
  const pick = $("#psoView").querySelector("[data-pick]");
  pick.value = pick.options[1].value; pick.dispatchEvent(new window.Event("change")); await sleep(200);
  check("pipeline: graphics fields reported with provenance", $("#psoView").textContent.includes("assumed") && $("#psoView").textContent.includes("rtv.format[0]"), $("#psoView").textContent.slice(0, 200));
  check("pipeline: reason + legal domain shown", $("#psoView").textContent.includes("never the target's storage format") && $("#psoView").textContent.includes("any color format"));
  const before = ($("#psoView").textContent.match(/(\d+) assumed/) || [])[1];
  const inp = $("#psoView").querySelector("[data-supply]");
  inp.value = "5"; inp.dispatchEvent(new window.Event("change")); await sleep(150);
  const after = ($("#psoView").textContent.match(/(\d+) assumed/) || [])[1];
  check("pipeline: supplying a field lowers the assumed count", +after === +before - 1, `${before} -> ${after}`);
  check("pipeline: supplied chip rendered", $("#psoView").textContent.includes("supplied"));
  check("pipeline: CLI line grows a -pso-set with the supplied field", $("#psoView").textContent.includes('-pso-set "rtv.count=5'), $("#psoView").textContent.match(/OxC3 isa[^\n]{0,160}/)?.[0]);

  /* the ISA tab's override panel edits the same oiSP */
  $("#isaAsic").value = "gfx1100"; $("#isaAsic").dispatchEvent(new window.Event("change")); await sleep(50);
  if (!$("#isaView").querySelector(".isa-state")) { $("#isaState").click(); await sleep(50); }
  check("isa: override panel lists the graphics fields", $("#isaView").querySelector(".isa-state [data-supply]") != null);
  const isaInp = [...$("#isaView").querySelectorAll(".isa-state [data-supply]")][1];
  isaInp.value = "28"; isaInp.dispatchEvent(new window.Event("change")); await sleep(150);
  check("isa: supplying there shows on the live CLI line", $("#isaView").textContent.includes("rtv.format[0]=28") || $("#psoView").textContent.includes("rtv.format[0]=28"), $("#psoView").textContent.match(/-pso-set[^\n]{0,120}/)?.[0]);

  /* ---- ISA refusal: ray tracing lib has no offline path ---- */
  [...$("#railTree").querySelectorAll(".fitem")].find(f => f.dataset.name === "trace.hlsl").click();
  $("#compileBtn").click(); await sleep(1100);
  /* rgen carries two extension sets -> two lib binaries -> refused until -entry picks one */
  check("pipeline: two libs refused with a picker", $("#psoView").textContent.includes("Refused") && $("#psoView").querySelector("[data-pick]") != null, $("#psoView").textContent.slice(0, 200));
  /* lib #0 is rgen's RayQuery set (rgen alone); the empty set is the one chit joins, so pick the last candidate */
  const pickLib = $("#psoView").querySelector("[data-pick]");
  pickLib.value = pickLib.options[pickLib.options.length - 1].value; pickLib.dispatchEvent(new window.Event("change")); await sleep(200);
  check("pipeline: ray tracing derives rt.* only", $("#psoView").textContent.includes("raytracing") && $("#psoView").textContent.includes("rt.maxRecursionDepth"), $("#psoView").textContent.slice(0, 200));
  check("pipeline: every RT stage of the lib bound", $("#psoView").textContent.includes("raygeneration") && $("#psoView").textContent.includes("closesthit"));
  $("#isaAsic").value = "gfx1100"; $("#isaAsic").dispatchEvent(new window.Event("change")); await sleep(50);
  $("#isaRun").click(); await sleep(600);
  check("isa: ray tracing refused offline", $("#isaView").textContent.includes("no offline path"), $("#isaView").textContent.slice(0, 160));

  /* failing compile */
  window.OxApp; // (not exported; drive through UI)
  const rail = $("#railTree");
  [...rail.querySelectorAll(".fitem")].find(f => f.dataset.name === "broken.hlsl").click();
  $("#compileBtn").click(); await sleep(900);
  check("compile-fail: overlay shown", $("#failOverlay").style.display === "flex");
  check("compile-fail: error in problems", $("#probBody").textContent.includes("undeclared identifier"));

  /* ---- inspect mode ---- */
  $("#mOish").checked = true;
  $("#mOish").dispatchEvent(new window.Event("change"));
  await sleep(400);
  check("oish: rail lists seeded docs", $("#railTree").textContent.includes("lighting.v1.oiSH") && $("#railTree").textContent.includes("lighting.v2.oiSH"));
  check("inspect: rail lists the oiSR and oiSP examples", $("#railTree").textContent.includes("lighting.oiSR") && $("#railTree").textContent.includes("post.oiSP") && $("#railTree").textContent.includes("trace.oiSP"));
  [...$("#railTree").querySelectorAll(".fitem")].find(f => f.dataset.name === "post.oiSP").click(); await sleep(150);
  check("inspect: example oiSP opens read-only with supplied + assumed fields", $("#psoView").textContent.includes("supplied") && $("#psoView").textContent.includes("assumed") && $("#psoView").querySelector("[data-supply]") == null);
  check("inspect: example oiSP has the file data print", $("#psoView").textContent.includes("; Pipeline state (graphics)"));
  [...$("#railTree").querySelectorAll(".fitem")].find(f => f.dataset.name === "lighting.oiSR").click(); await sleep(150);
  check("inspect: example oiSR opens in Symbols", $("#symView").textContent.includes("Register") && $("#symView").textContent.includes("lighting.oiSR"));
  [...$("#railTree").querySelectorAll(".fitem")].find(f => f.dataset.name === "lighting.v1.oiSH").click(); await sleep(150);
  check("oish: A/B selects filled", $("#diffA").options.length >= 2 && $("#diffB").value === "lighting.v2.oiSH");
  check("oish: reflection diff has sections", $("#diffBody").textContent.includes("File") && $("#diffBody").textContent.includes("Registers"));
  check("oish: diff shows divergence rows", $("#diffBody").querySelectorAll(".diff-diverge").length > 0);
  check("oish: binary-diff section visible", !$("#bdiffSection").classList.contains("d-none"));
  check("oish: pair select filled", $("#bdEntry").options.length >= 1, $("#bdEntry").textContent);
  await sleep(200);
  check("oish: binary diff verdict rendered", $("#bdSummary").textContent.includes("differs") || $("#bdSummary").textContent.includes("identical"), $("#bdSummary").textContent);
  check("oish: line diff has +/- lines", $("#bdBody").querySelectorAll(".ln.add,.ln.del").length > 0);
  check("oish: pair label shows model bump", $("#bdEntry").textContent.includes("SM 6.7 \u2192 6.8"), $("#bdEntry").textContent);

  /* combine: v1+v2 have different source hashes -> must fail with a toast */
  $("#combineBtn").click(); await sleep(150);
  check("combine: mismatch surfaces as toast", window.document.body.textContent.includes("file combine failed"), "");

  /* same doc on both sides -> identical verdict */
  $("#diffB").value = "lighting.v1.oiSH";
  $("#diffB").dispatchEvent(new window.Event("change")); await sleep(250);
  check("oish: identical verdict for same doc", $("#bdSummary").textContent.includes("identical"), $("#bdSummary").textContent);
  check("oish: changes-only collapses identical body to skip rows", $("#bdBody").querySelectorAll(".ln.skip").length > 0);

  /* combine now (same hash) -> success + new doc in rail */
  $("#combineBtn").click(); await sleep(200);
  check("combine: same-hash combine succeeds", $("#railTree").textContent.includes("lighting.v1+lighting.v1"), $("#railTree").textContent);

  /* inspect an oiSR and an oiSP (mock parse from bytes with the right magic) */
  const srBytes = new Uint8Array(64); [0x6F, 0x69, 0x53, 0x52].forEach((b, i) => srBytes[i] = b);
  window.OxAPI.parseOiSR("uploaded.oiSR", srBytes).then(d => { window.__sr = d; });
  await sleep(50);
  check("oisr: parse yields a symbol document", window.__sr && window.__sr.nodes.length > 3 && window.__sr.header.version === "1.1");
  const spBytes = new Uint8Array(64); [0x6F, 0x69, 0x53, 0x50].forEach((b, i) => spBytes[i] = b);
  window.OxAPI.parseOiSP("uploaded.oiSP", spBytes).then(d => { window.__sp = d; });
  await sleep(50);
  check("oisp: parse yields a pipeline document", window.__sp && window.__sp.header.counts.pipelines === 1);
  check("oish: derived pipeline + ISA for the inspected oiSH", $("#psoView").textContent.includes("Pipeline state (compute)") && $("#isaView").textContent.includes("Disassemble"));

  /* ---- SPV/DXIL mode: a bare binary is a document ---- */
  $("#mBins").checked = true;
  $("#mBins").dispatchEvent(new window.Event("change"));
  await sleep(400);
  check("bins: tabs stay, reflection of the bare binary", !$("#outTabs").classList.contains("d-none") && $("#reflView").textContent.includes("standalone binary") && $("#reflView").innerHTML.includes("register("), $("#reflView").textContent.slice(0, 120));
  check("bins: no identifier until assembled", $("#reflView").textContent.includes("no identifier"));
  check("bins: strip shows the binary + assemble toggle", $("#binPane").textContent.includes("post.ps.spv") && $("#asmToggle") != null, $("#binPane").textContent.slice(0, 80));
  check("bins: disassembly view filled", $("#spvAsm").textContent.includes("OpEntryPoint"), $("#spvAsm").textContent.slice(0, 60));
  check("bins: oiSH tab offers assemble into oiSH", $("#oishView").textContent.includes("Assemble into oiSH") && $("#asmOishGo") != null);
  check("bins: pipeline derived from the reflected stage", $("#psoView").textContent.includes("Pipeline state (graphics)"), $("#psoView").textContent.slice(0, 120));
  check("bins: diff selects list the binaries", $("#diffA2").options.length >= 2);
  $("#isaAsic").value = "gfx1100"; $("#isaAsic").dispatchEvent(new window.Event("change")); await sleep(50);
  $("#isaRun").click(); await sleep(600);
  check("bins: offline ISA on the bare .spv", $("#isaView").textContent.includes("s_endpgm"), $("#isaView").textContent.slice(0, 120));
  $("#sbEntries").click(); await sleep(80);
  check("bins: entrypoint list rendered", $("#sbMeta").textContent.includes("entrypoints"));
  $("#asmToggle").click(); $("#asmGo").click(); await sleep(300);
  check("bins: assembled text becomes a loaded binary", $("#railTree").textContent.includes("assembled."), $("#railTree").textContent);

  /* assemble into oiSH: the binary gets an identifier and lands in Inspect mode as a real oiSH */
  [...$("#railTree").querySelectorAll(".fitem")].find(f => f.dataset.name === "post.ps.spv").click(); await sleep(300);
  $("#asmModel").value = "6.8";
  $("#asmOishGo").click(); await sleep(600);
  check("assemble: lands in Inspect mode as an oiSH", $("#mOish").checked && $("#railTree").textContent.includes("post.ps.oiSH"),
    `mode=${$("#mOish").checked} rail=${$("#railTree").textContent.replace(/\s+/g, " ").slice(0, 200)} toast=${(window.document.body.textContent.match(/Assembled:[^.]*/) || ["none"])[0]}`);
  check("assemble: document says where it came from + has an identifier", $("#reflView").textContent.includes("assembled from post.ps.spv") && $("#reflView").textContent.includes("SM 6.8"), $("#reflView").textContent.slice(0, 200));

  /* raw DXC from the Command tab: the derived line runs as is, output is a standalone binary.
   * broken.hlsl was the last file compiled, so switch to one that compiles (a failing line stays in SPV/DXIL-less
   * Compile mode with the DXC error in the log, which is the right behaviour but not what's checked here) */
  $("#mCompile").checked = true; $("#mCompile").dispatchEvent(new window.Event("change")); await sleep(300);
  [...$("#railTree").querySelectorAll(".fitem")].find(f => f.dataset.name === "lighting.hlsl").click(); await sleep(500);
  const runBtn = $("#cmdList .dxc-run");
  check("raw dxc: derived DXC lines are editable + runnable", runBtn != null && $("#cmdList textarea.dxc-line") != null);
  runBtn.click(); await sleep(800);
  check("raw dxc: log reports the output", $("#dxcLog").textContent.includes("bytes"), $("#dxcLog").textContent.slice(0, 160));
  check("raw dxc: output opened in SPV/DXIL mode as a document", $("#mBins").checked && $("#railTree").textContent.includes(".sm6") && $("#reflView").textContent.includes("standalone binary"), $("#railTree").textContent);

  console.log(failures ? `\n${failures} FAILURE(S)` : "\nALL PASS");
  process.exit(failures ? 1 : 0);
})();
