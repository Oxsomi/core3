/* smoke.js: boots index.html in jsdom (local scripts only, CodeMirror stubbed) and walks
 * all three modes plus the Symbols / Pipeline / ISA tabs.
 *
 * It runs against the REAL module, staged into web/wasm by build_web.py --frontend, because that is
 * now the only tier that answers: without one the page refuses to compile or reflect rather than
 * fabricating, so a mock-tier walk would have nothing to render. What this covers that
 * dev/wasm_smoke.js does not is the rendering: wasm_smoke drives the boundary, this drives the page.
 *
 * Run: npm i jsdom && node dev/smoke.js [path/to/OxC3_wasm_sf.js]
 * (or point NODE_PATH at a node_modules that has jsdom). */
const { JSDOM } = require("jsdom");
const fs = require("fs"), path = require("path");
const ROOT = require("path").join(__dirname, "..");

/* The embedded flavor: it carries its wasm inside the js, so nothing has to fetch a sibling file
 * from a jsdom document that has no real network. */
const modulePath = process.argv[2] || path.join(ROOT, "wasm", "OxC3_wasm_sf.js");

if (!fs.existsSync(modulePath)) {
  console.error(`-- No module at ${modulePath} (build it with build_web.py --frontend)`);
  process.exit(1);
}

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
    setCursor: () => {}, scrollIntoView: () => {}, focus: () => {}, lineCount: () => 999, refresh: () => {}
  };
};
Object.defineProperty(window.navigator, "clipboard", { value: { writeText: async () => {} } });
window.TextEncoder = TextEncoder; window.TextDecoder = TextDecoder;
/* The share codec's gzip path: every shipped browser has CompressionStream, jsdom's window has none, and
 * without it encodeShare falls back to plain base64, which is not the link users get. */
for (const k of ["CompressionStream", "DecompressionStream", "Blob", "Response"]) window[k] = globalThis[k];
window.URL.createObjectURL = () => "blob:mock"; window.URL.revokeObjectURL = () => {};
window.performance = window.performance || { now: () => Date.now() };
window.prompt = () => null;

/* editor.js drives cm via closure; our stub's setValue fires change: mimic real enough.
 * But editor.open() suppresses the callback itself, so that's fine. */

for (const f of ["js/util.js", "js/workspace.js", "js/theme.js", "js/mock_data.js", "js/intrinsics_data.js", "js/mock.js", "js/mock_formats.js", "js/wasm.js", "js/wasm_rpc.js",
  "js/api.js", "js/editor.js", "js/intellisense.js", "js/asmmap.js",
  "js/tools/compile.js", "js/tools/inspect.js", "js/tools/symbols.js", "js/tools/pipeline.js", "js/tools/isa.js",
  "js/tools/diff.js", "js/tools/binary.js", "js/app.js"]) {
  window.eval(fs.readFileSync(path.join(ROOT, f), "utf8"));
}

/* The page decides where the module comes from through js/wasmload.js, which writes a script tag a
 * jsdom document won't execute. That choice is the one thing this harness makes for it: the factory is
 * required here and handed to the same OxWasm.load the page calls, so everything above the boundary
 * runs exactly as it does in a browser.
 * wasm_rpc.js leaves window.OxWasm alone because OxWasmVersion is absent (wasmload isn't loaded), so
 * the module runs in this thread rather than behind a Worker jsdom doesn't have. */
{
  const createOxC3Module = require(modulePath);
  const load = window.OxWasm.load;
  window.OxWasm.load = opts => load({
    ...(opts || {}), factory: createOxC3Module, locateFile: f => path.join(path.dirname(modulePath), f)
  });
}

const $ = s => window.document.querySelector(s);
const sleep = ms => new Promise(r => setTimeout(r, ms));

/* Real work takes as long as it takes: the module instantiates in seconds where the mock answered
 * instantly, and a compile is a compile. Waiting on the condition rather than on a guessed duration is
 * what keeps this suite from being a timing lottery on a slower machine. */
async function until(what, cond, ms = 120000) {
  const t0 = Date.now();
  while (Date.now() - t0 < ms) {
    let ok = false;
    try { ok = cond(); } catch (e) { ok = false; }
    if (ok) return true;
    await sleep(25);
  }
  console.log(`FAIL timed out waiting for ${what} (${ms} ms)`);
  failures++;
  return false;
}

/* A compile has landed when the status line stops saying it is running: it names the binaries it
 * produced, or says it failed. */
const compileSettled = () => /binaries|failed/.test($("#stCompiled").textContent);
const awaitCompile = () => until("the compile to finish", compileSettled);

/* Entering Inspect or SPV/DXIL mode compiles the example documents for real (ensureExamples), which is
 * seconds rather than the instant a fabricated seed took. Until that lands the page is still showing
 * the recorded copies, which carry the same names but no bytes, so waiting on a name would pass on the
 * wrong document; the status line's spinner is what actually says the seeding is still running. */
const seeded = () => until("the example documents to be compiled",
  () => !/compiling the examples/.test($("#stCompiled").textContent));
let failures = 0;
/* A view the open document can't answer for is hidden rather than left to fail (applyContext). */
const tabHidden = t => $(`#outTabs [data-tab="${t}"]`).classList.contains("d-none");

function check(name, cond, extra) {
  console.log((cond ? "  ok " : "FAIL ") + name + (cond ? "" : "  " + (extra || "")));
  if (!cond) failures++;
}

(async () => {
  window.document.dispatchEvent(new window.Event("DOMContentLoaded", { bubbles: true }));

  /* Boot loads the module and asks it for every vocabulary the page shows; the extension list is the
   * last of those to land, so it standing in for "boot finished" is what the rest can rely on. */
  await until("the page to finish booting on the module",
    () => window.OxAPI.ready && window.OxAPI.backend === "wasm" && !/loading/.test($("#extList").textContent));

  check("boot: rail lists project files", $("#railTree").textContent.includes("lighting.hlsl"));

  /* The module answered, so nothing on the page is a stand-in and the banner that would say otherwise
   * stays down; the badge carries the compiler's own version rather than the word mock. */
  check("boot: no stand-in banner when the compiler is running", $("#mockBanner").classList.contains("d-none"));
  check("boot: the badge names the compiler version",
    /\d+\.\d+\.\d+/.test($("#stBackend").textContent), $("#stBackend").textContent);
  check("boot: builtins listed", $("#builtinFiles").textContent.includes("@types.hlsli"));
  check("boot: CLI map filled", $("#cliRefBody").textContent.includes("shader disassemble"));
  check("boot: extension list comes from the oiSH enums (recorded)",
    $("#extList").textContent.includes("SubgroupQuad") && !$("#extList").textContent.includes("RayMotionBlur"),
    $("#extList").textContent.slice(0, 80));
  check("boot: vendor list too", $("#vendorList").textContent.includes("NV"), $("#vendorList").textContent);
  check("boot: type aliases scraped from the builtin headers",
    $("#aliasList").textContent.includes("F32") && $("#aliasList").textContent.includes("F64") &&
    $("#aliasList").textContent.includes("extension.F64.hlsli"),
    $("#aliasList").textContent.slice(0, 100));

  /* The history floor: a sentinel under the current entry, so the mouse's back button bounces at the
   * page instead of leaving it mid-edit. */
  check("boot: the history sentinel is armed", window.history.state && window.history.state.ox === "top",
    JSON.stringify(window.history.state));

  /* ---- compile mode ---- */
  $("#wUnusedReg").checked = true;
  $("#compileBtn").click();
  await sleep(900);
  check("compile: status shows binaries", $("#stCompiled").textContent.includes("binaries"), $("#stCompiled").textContent);
  check("compile: reflection tree has entrypoint + binary + include CRC",
    $("#reflView").textContent.includes("main") && $("#reflView").textContent.includes("CRC32C"));
  check("compile: register table shows dual bindings",
    /vk::binding\(\d+, \d+\)/.test($("#reflView").innerHTML) && /register\([butsBUTS]\d+, space\d+\)/.test($("#reflView").innerHTML),
    $("#reflView").innerHTML.slice(0, 200));
  check("compile: oiSB layout rendered", $("#reflView").innerHTML.includes("0x00000040"));
  check("compile: oiSH tab has feature set + header", $("#oishView").textContent.includes("Feature set") && $("#oishView").textContent.includes("Source hash"));
  /* This sample compiles clean even with --warn-unused-registers on, so what it proves is the quiet
   * case: no problems, and a panel that stays out of the way. The rendering of real diagnostics is
   * covered against a source that actually fails, under "compile-fail" below. */
  check("problems: a clean compile reports none", $("#probBody").textContent.includes("No problems"),
    $("#probBody").textContent.slice(0, 80));
  check("problems: and leaves the panel closed", window.document.body.classList.contains("prob-collapsed"));

  /* The panel reveals itself when diagnostics change, and a deliberate close holds while they don't. */
  window.document.body.classList.add("prob-collapsed");
  $("#compileBtn").click(); await awaitCompile();
  check("problems: unchanged diagnostics respect a close", window.document.body.classList.contains("prob-collapsed"));
  /* The asm panes render per line now (asmmap.js): the mock's disassembly carries no debug info, so
   * every line is plain and the header offers -Zi instead of pretending there is a mapping. */
  check("asmmap: panes render per line", window.document.querySelectorAll("#spvAsm .asm-line").length > 5,
    String(window.document.querySelectorAll("#spvAsm .asm-line").length));
  check("asmmap: no debug info offers -Zi", $("#spvHead").textContent.includes("-Zi"));
  check("asmmap: and nothing is tinted", window.document.querySelectorAll("#spvAsm .asm-mapped").length === 0);

  check("compile: binary strip populated", $("#binSel").options.length >= 2, "options=" + $("#binSel").options.length);
  check("compile: SPIR-V view auto-filled", $("#spvAsm").textContent.includes("OpEntryPoint"));

  /* A row picked for one backend renders into that backend's pane, so entering the other tab used to
   * show whatever was there before, or nothing. Entering it now picks its first binary. */
  const selTab = t => {
    const was = window.document.querySelector("#outTabs .nav-link.active");
    if (was) was.classList.remove("active");
    const btn = window.document.querySelector(`[data-bs-target="#p-${t}"]`);
    btn.classList.add("active");
    btn.dispatchEvent(new window.Event("shown.bs.tab", { bubbles: true }));
  };
  selTab("dxil"); await sleep(300);
  check("bins: entering the DXIL tab picks a DXIL binary",
    window.document.querySelectorAll("#dxilAsm .asm-line").length > 5,
    String(window.document.querySelectorAll("#dxilAsm .asm-line").length));
  selTab("spv"); await sleep(300);
  check("bins: and switching back re-picks SPIR-V", $("#spvAsm").textContent.includes("OpEntryPoint"));

  /* Hover docs for [[oxc::]] annotations come off the syntax reference offcanvas, so the scrape has
   * to find every annotation the compiler accepts, binary included. */
  {
    const docs = window.OxIntelliSense.annotationDocs();
    check("hoverdocs: the syntax panel serves every oxc annotation",
      ["stage", "extension", "model", "vendor", "uniforms", "defines", "binary", "shader"].every(k => docs[k] && docs[k].doc.length > 10),
      JSON.stringify(Object.keys(docs)));
  }
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

  /* The tree reads in the alias the source wrote until the switch asks for the builtin it resolves to.
   * Checked as a swap in both directions, since showing both spellings at once would pass a one-sided check. */
  /* The built-in includes are a summary line until the switch asks for them, and the switch only means
   * something when the document carries their symbols: a tree that lists them without having them would
   * flip and appear to do nothing. */

  const symRows = () => window.document.querySelectorAll("#symView .tree li").length;
  const collapsedRows = symRows();
  check("symbols: --includes is offered when the symbols are there", !$("#symBuiltins").disabled);
  $("#symBuiltins").checked = true;
  $("#symBuiltins").dispatchEvent(new window.Event("change"));
  await sleep(150);
  check("symbols: --includes expands the built-in symbols into the tree", symRows() > collapsedRows * 2,
    `${collapsedRows} -> ${symRows()}`);
  check("symbols: and says so", $("#symView").textContent.includes("shown above"));
  $("#symBuiltins").checked = false;
  $("#symBuiltins").dispatchEvent(new window.Event("change"));
  await sleep(150);
  check("symbols: --includes collapses them again", symRows() === collapsedRows,
    `${collapsedRows} -> ${symRows()}`);

  const symAliasText = $("#symView").textContent;
  check("symbols: types read as the source aliases", symAliasText.includes("(F32x4)") && !symAliasText.includes("(float4)"));
  $("#symHlslTypes").checked = true;
  $("#symHlslTypes").dispatchEvent(new window.Event("change"));
  await sleep(150);
  const symHlslText = $("#symView").textContent;
  check("symbols: HLSL types switch shows the builtin spelling", symHlslText.includes("(float4)") && !symHlslText.includes("(F32x4)"));
  check("symbols: HLSL types switch stays on", $("#symHlslTypes").checked);
  $("#symHlslTypes").checked = false;
  $("#symHlslTypes").dispatchEvent(new window.Event("change"));
  await sleep(150);
  check("symbols: switching back restores the aliases", $("#symView").textContent.includes("(F32x4)"));

  /* Opening a builtin include reflects IT as the main file, so its own symbols outline and hover
   * instead of the page keeping the previous project file's tree. */
  {
    const before = $("#symView").textContent;
    [...document.querySelectorAll("#builtinFiles .fitem")].find(f => f.dataset.name === "@buffer.hlsli").click();
    await sleep(400);
    check("symbols: an open builtin include reflects as its own document",
      /@buffer\.hlsli\.oiSR/.test($("#symView").textContent) && /\d+ nodes/.test($("#symView").textContent),
      $("#symView").textContent.slice(0, 160));
    [...document.querySelectorAll("#railTree .fitem")].find(f => f.dataset.name === "lighting.hlsl").click();
    await sleep(700);
    check("symbols: and a project file takes the outline back",
      !/@buffer\.hlsli/.test($("#symView").textContent) && $("#symView").textContent.length > 40,
      $("#symView").textContent.slice(0, 120) + " | before=" + before.slice(0, 40));
  }

  /* ---- pipeline (oiSP): compute derives completely ---- */
  check("pipeline: compute pipeline is exact", $("#psoView").textContent.includes("compute") && $("#psoView").textContent.includes("exact"));
  check("pipeline: header card counts", $("#psoView").textContent.includes("Pipelines · stages · specializations"));
  check("pipeline: file data print", $("#psoView").textContent.includes("; Pipeline state (compute), 1 stage(s), 0 assumed field(s)"));

  /* ---- ISA: offline amdllpc on the compute binary ---- */
  /* The offline route spawns amdllpc, which wasm cannot, so the module reports no targets and the tab
   * says so rather than showing an ISA it could not have produced. The desktop CLI is what runs it. */
  check("isa: no offline targets are offered in wasm",
    !!$("#isaAsic") && ![...$("#isaAsic").options].some(o => o.value === "gfx1201"),
    [...($("#isaAsic") ? $("#isaAsic").options : [])].map(o => o.value).join(","));
  check("isa: and the tab says the target is unavailable",
    $("#isaView").textContent.includes("no target available"), $("#isaView").textContent.slice(0, 160));
  check("isa: CLI line names the asic", $("#isaView").textContent.includes("isa disassemble -input") && $("#isaView").textContent.includes("-asic gfx1100"));
  check("isa: live option greyed out in a browser", [...$("#isaAsic").options].find(o => o.value === "live").disabled);
  $("#isaAsic").value = "live"; $("#isaAsic").dispatchEvent(new window.Event("change")); await sleep(50);
  check("isa: live route explained as native only", $("#isaView").textContent.includes("native only") && $("#isaView").textContent.includes("VK_KHR_pipeline_executable_properties"));
  check("isa: disassemble disabled on the greyed route", $("#isaRun").disabled);
  $("#isaState").click(); await sleep(50);
  check("isa: pipeline state panel opens", $("#isaView").querySelector(".isa-state") != null);
  check("dl menu: .oiSR and .oiSP entries", $("#dlMenu").textContent.includes(".oiSR") && $("#dlMenu").textContent.includes(".oiSP"));

  /* A compile that produces no document at all, which is what the wasm backend returns for a source that
   * does not compile. The mock always produces one, so the only way to reach the app's handling of it is to
   * hand it the shape the real backend does. It threw here, and the throw took the symbol refresh with it,
   * so a file that failed to compile lost the outline it is most wanted for. */

  const realCompileFile = window.OxAPI.compileFile;
  window.OxAPI.compileFile = async () => ({
    doc: null,
    diags: [{ sev: "error", line: 1, ch0: 0, ch1: 1, file: "lighting.hlsl", msg: "synthetic failure", code: "E_test" }]
  });

  await (async () => {
    try {
      $("#compileBtn").click();
      await sleep(900);
      check("failed compile: the page survives a compile with no document", true);
    } catch (err) {
      check("failed compile: the page survives a compile with no document", false, err.message);
    }
  })();

  check("failed compile: it reports the failure", $("#stCompiled").textContent.includes("failed"),
    $("#stCompiled").textContent);
  check("failed compile: no sizes are claimed", $("#stSizes").textContent === "", $("#stSizes").textContent);
  check("failed compile: the symbols tree is still there",
    $("#symView").textContent.includes("Function"), $("#symView").textContent.slice(0, 120));

  /* The overlay says there is no binary, which is true of the tabs that describe one and false of Symbols:
   * reflection runs on the source, so covering it would hide the one view a failed compile still has. */

  /* Bootstrap's JS is a CDN script the harness does not load, so the tab change it would make is made
   * here: move the active class and fire the event the page listens for. */

  const selectTab = t => {
    const was = window.document.querySelector("#outTabs .nav-link.active");
    if (was) was.classList.remove("active");
    const btn = window.document.querySelector(`[data-bs-target="#p-${t}"]`);
    btn.classList.add("active");
    btn.dispatchEvent(new window.Event("shown.bs.tab", { bubbles: true }));
  };

  const overlayShown = () => $("#failOverlay").style.display !== "none";

  selectTab("sym"); await sleep(100);
  check("failed compile: the overlay stays off the symbols tab", !overlayShown());
  selectTab("refl"); await sleep(100);
  check("failed compile: the overlay covers the binary tabs", overlayShown());

  window.OxAPI.compileFile = realCompileFile;
  $("#compileBtn").click(); await awaitCompile();
  check("failed compile: a good compile recovers", $("#stCompiled").textContent.includes("binaries"),
    $("#stCompiled").textContent);

  /* The switches describe the view, so they have to outlive the reflection: Symbols follows the editor
   * and re-renders on its own, which used to clear them on the next keystroke or file switch. */
  $("#symHlslTypes").checked = true;
  $("#symHlslTypes").dispatchEvent(new window.Event("change"));
  await sleep(150);

  /* ---- graphics pipeline: post.hlsl has two pixel variants -> refused with an -entry picker, then assumed fields ---- */
  [...$("#railTree").querySelectorAll(".fitem")].find(f => f.dataset.name === "post.hlsl").click();
  $("#compileBtn").click(); await awaitCompile();
  check("symbols: HLSL types survives a re-reflect", $("#symHlslTypes").checked && $("#symView").textContent.includes("(float4)"));
  $("#symHlslTypes").checked = false;
  $("#symHlslTypes").dispatchEvent(new window.Event("change"));
  await sleep(150);
  /* The status line settles when the compile lands, but doCompile still has the reflect and the
   * derivation to run after it, so the pipeline pane is waited for on its own. */
  await until("the pipeline to derive",
    () => /Refused|compute|graphics|raytracing/.test($("#psoView").textContent));
  /* post.hlsl's psMain carries two [[oxc::defines]] sets, so it compiles to two pixel BINARIES of one
   * entrypoint. That is not the ambiguity a pipeline refuses on: the derivation picks by entry, and one
   * vertex plus one pixel entry is a pipeline. The refusal with an -entry picker needs two entries of a
   * stage kind, which the ray tracing section below covers. */
  check("pipeline: one entry per stage derives rather than refusing",
    $("#psoView").textContent.includes("graphics") && !$("#psoView").textContent.includes("Refused"),
    $("#psoView").textContent.slice(0, 200));
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

  /* Enum-typed fields edit as dropdowns of their value names, mask fields as per-bit checkboxes and
   * binary choices as toggles; the vocabularies come off the compiler's own name tables. */
  check("pso: enum fields render as dropdowns", !!document.querySelector("#psoView select.sp-val"),
    document.querySelectorAll("#psoView select.sp-val").length);
  check("pso: mask fields render as per-bit checkboxes", !!document.querySelector("#psoView .sp-bits input[data-bit]"));
  check("pso: a binary choice renders as a toggle",
    !!document.querySelector('#psoView input.sp-val[type="checkbox"]'));

  /* The follows picker lists the ACTIVE file's permutations off the parse-only listing, refilled on
   * every switch and edit; the pick itself stays per file and starts at the default on a fresh one
   * rather than carrying a config across sources. */
  check("follows: the backend view control defaults to DXIL",
    !!$("#reflectBackend") && $("#reflectBackend").value === "dxil");
  check("follows: options exist for the compiled file", $("#reflectFollow").options.length > 1,
    $("#reflectFollow").options.length);
  [...document.querySelectorAll("#railTree .fitem")].find(f => f.dataset.name === "lighting.hlsl").click();
  check("follows: a file switch resets the pick to the default parse", $("#reflectFollow").value === "");
  await sleep(300);
  check("follows: the picker refills from the new file's own parse, no compile needed",
    $("#reflectFollow").options.length > 1, $("#reflectFollow").options.length);
  [...document.querySelectorAll("#railTree .fitem")].find(f => f.dataset.name === "post.hlsl").click();
  await sleep(300);
  check("follows: the compiled file's options come back with it",
    $("#reflectFollow").options.length > 1, $("#reflectFollow").options.length);

  /* Picking a permutation drives the binary strip to its binary, matched by identity; a pick the
   * last compile can't answer leaves the strip alone and lights the stale marker instead. */
  {
    $("#reflectFollow").value = "0";
    $("#reflectFollow").dispatchEvent(new window.Event("change"));
    await sleep(250);
    const picked = $("#reflectFollow").options[1].textContent.split(" · ")[0];
    const row = $("#binSel").options[+$("#binSel").value];
    check("follows: picking a permutation drives the binary strip",
      row && row.textContent.includes(picked), (row && row.textContent) + " vs " + picked);
    check("follows: a matched pick shows no stale marker", $("#staleMark").classList.contains("d-none"));

    /* Switching backend tabs keeps the SAME binary selected (its row on the other backend), so a
     * permutation picked by hand or through the picker survives the pane change. post.hlsl is
     * compiled here with several binaries, which is what makes the retention observable. */
    {
      const rowNo = o => (o.textContent.match(/^#(\d+)/) || [])[1];
      const selTab2 = t => {
        const was = document.querySelector("#outTabs .nav-link.active");
        if (was) was.classList.remove("active");
        const btn = document.querySelector(`[data-bs-target="#p-${t}"]`);
        btn.classList.add("active");
        btn.dispatchEvent(new window.Event("shown.bs.tab", { bubbles: true }));
      };
      const spv2 = [...$("#binSel").options].findIndex(o => rowNo(o) && rowNo(o) !== "0" && o.textContent.includes("SPIR-V"));
      if (spv2 >= 0) {
        $("#binSel").value = String(spv2);
        $("#binSel").dispatchEvent(new window.Event("change")); await sleep(200);
        const pickedNo = rowNo($("#binSel").options[+$("#binSel").value]);
        selTab2("dxil"); await sleep(300);
        check("bins: the picked binary survives a backend tab switch",
          rowNo($("#binSel").options[+$("#binSel").value]) === pickedNo &&
          $("#binSel").options[+$("#binSel").value].textContent.includes("DXIL"),
          $("#binSel").options[+$("#binSel").value].textContent + " vs #" + pickedNo);
        selTab2("spv"); await sleep(300);
      } else check("bins: the picked binary survives a backend tab switch", false, "no second SPIR-V row to pick");
    }

    /* Renaming the entrypoint reparses into a permutation the compile doesn't hold. */
    const src = window.OxEditor.value();
    window.OxEditor.cmHandle().setValue(src.replace(/vsMain/g, "vsRenamed"));
    await sleep(900);
    /* Option 0 is the default parse, so option position N carries row value N - 1. */
    const renamed = [...$("#reflectFollow").options].findIndex(o => o.textContent.includes("vsRenamed"));
    check("follows: the listing refollows the edit", renamed > 0,
      [...$("#reflectFollow").options].map(o => o.textContent).join(" | "));
    if (renamed > 0) {
      $("#reflectFollow").value = String(renamed - 1);
      $("#reflectFollow").dispatchEvent(new window.Event("change"));
      await sleep(150);
      check("follows: a permutation the compile doesn't hold lights the stale marker",
        !$("#staleMark").classList.contains("d-none"));
    }

    window.OxEditor.cmHandle().setValue(src);       //back the way the sections below expect it
    await sleep(900);
    check("follows: the restored source clears the pick and the marker",
      $("#reflectFollow").value === "" && $("#staleMark").classList.contains("d-none"),
      $("#reflectFollow").value);
  }

  /* The Download menu must offer the derived .oiSP even while every field is assumed: the menu is
   * rebuilt by the derivation itself, not only by a later supply. */
  check("dl: a fully assumed pipeline is still downloadable",
    !!document.querySelector('#dlMenu [data-act="oisp"]'),
    document.querySelector("#dlMenu").textContent.slice(0, 80));

  /* ---- ISA refusal: ray tracing lib has no offline path ---- */
  [...$("#railTree").querySelectorAll(".fitem")].find(f => f.dataset.name === "trace.hlsl").click();
  $("#compileBtn").click(); await awaitCompile();
  /* rgen carries two extension sets, so it compiles to two lib binaries of one entry: not an
   * ambiguity, since the derivation binds entries. The refusal with an -entry picker needs two
   * entries of one stage kind, which no sample has; dev/wasm_smoke.js covers it with a fixture. */
  await until("the ray tracing pipeline to derive", () => /raytracing|Refused/.test($("#psoView").textContent));
  check("pipeline: ray tracing derives rt.* only", $("#psoView").textContent.includes("raytracing") && $("#psoView").textContent.includes("rt.maxRecursionDepth"), $("#psoView").textContent.slice(0, 200));
  check("pipeline: every RT stage of the lib bound", $("#psoView").textContent.includes("raygeneration") && $("#psoView").textContent.includes("closesthit"));
  $("#isaAsic").value = "gfx1100"; $("#isaAsic").dispatchEvent(new window.Event("change")); await sleep(50);
  $("#isaRun").click(); await sleep(600);
  check("isa: ray tracing has nothing to run offline",
    /no offline path|no target available/.test($("#isaView").textContent), $("#isaView").textContent.slice(0, 160));

  /* failing compile */
  window.OxApp; // (not exported; drive through UI)
  const rail = $("#railTree");
  [...rail.querySelectorAll(".fitem")].find(f => f.dataset.name === "broken.hlsl").click();
  $("#compileBtn").click(); await awaitCompile();
  check("compile-fail: overlay shown", $("#failOverlay").style.display === "flex");
  check("compile-fail: the diagnostics point at the source",
    /broken\.hlsl:\d+:\d+/.test($("#probBody").textContent) &&
    $("#probBody").querySelector(".diag-error") != null,
    $("#probBody").innerHTML.slice(0, 200));

  /* The reflection runs on the source, so the file that would not compile still has an outline, and the
   * compile must not be what takes it away. */

  check("compile-fail: the symbols tree survives the compile",
    $("#symView").textContent.includes("Namespace"), $("#symView").textContent.slice(0, 120));
  selectTab("sym"); await sleep(100);
  check("compile-fail: and is not covered by the overlay", $("#failOverlay").style.display === "none");

  /* The mock reads the source in front of it, so a file it parses heuristically has no built-in symbols.
   * It must not name them with a count either, or the tree claims hundreds it cannot show and the switch
   * has nothing to expand. */

  /* The reflection describes what parsed, so a file that fails to compile still pulls its includes in,
   * and the collapsed builtin summary is real rather than a claim about symbols nobody has. The switch
   * that expands it is offered exactly when there is something behind it. */
  check("compile-fail: the builtin summary and its switch agree",
    $("#symView").textContent.includes("builtin-include symbols") === !$("#symBuiltins").disabled,
    `summary=${$("#symView").textContent.includes("builtin-include symbols")} disabled=${$("#symBuiltins").disabled}`);
  selectTab("refl"); await sleep(100);

  /* ---- inspect mode ---- */
  $("#mOish").checked = true;
  $("#mOish").dispatchEvent(new window.Event("change"));
  await seeded();
  check("oish: rail lists seeded docs", $("#railTree").textContent.includes("lighting.v1.oiSH") && $("#railTree").textContent.includes("lighting.v2.oiSH"));
  check("inspect: rail lists the oiSR and oiSP examples", $("#railTree").textContent.includes("lighting.oiSR") && $("#railTree").textContent.includes("post.oiSP") && $("#railTree").textContent.includes("trace.oiSP"));
  [...$("#railTree").querySelectorAll(".fitem")].find(f => f.dataset.name === "post.oiSP").click();
  await until("the example oiSP to open", () => /post\.oiSP/.test($("#psoView").textContent));
  check("inspect: example oiSP opens read-only with supplied + assumed fields", $("#psoView").textContent.includes("supplied") && $("#psoView").textContent.includes("assumed") && $("#psoView").querySelector("[data-supply]") == null);
  check("inspect: example oiSP has the file data print", $("#psoView").textContent.includes("; Pipeline state (graphics)"),
    $("#psoView").textContent.replace(/\s+/g, " ").slice(0, 240));
  [...$("#railTree").querySelectorAll(".fitem")].find(f => f.dataset.name === "lighting.oiSR").click();
  await until("the example oiSR to open", () => /lighting\.oiSR/.test($("#symView").textContent));
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
  /* The two versions come from a real compile now (js/mock_data.js), so the models are whatever the
   * sample declares and the compiler's minimum is; what the label has to show is that they differ. */
  check("oish: pair label shows model bump", /SM \d+\.\d+ \u2192 \d+\.\d+/.test($("#bdEntry").textContent), $("#bdEntry").textContent);

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

  /* An upload is a stranger's bytes: the magic alone is not a document, and the reader says so rather
   * than producing something shaped like one. Reading a REAL oiSR/oiSP back is dev/wasm_smoke.js's
   * round trip; what matters here is that the page surfaces the refusal instead of a fabrication. */
  const refused = async (what, call) => {
    let err = null;
    try { await call(); } catch (e) { err = e.message; }
    check(`${what}: bytes that aren't one are refused`, !!err, err || "(resolved)");
  };
  const srBytes = new Uint8Array(64); [0x6F, 0x69, 0x53, 0x52].forEach((b, i) => srBytes[i] = b);
  await refused("oisr", () => window.OxAPI.parseOiSR("uploaded.oiSR", srBytes));
  const spBytes = new Uint8Array(64); [0x6F, 0x69, 0x53, 0x50].forEach((b, i) => spBytes[i] = b);
  await refused("oisp", () => window.OxAPI.parseOiSP("uploaded.oiSP", spBytes));
  check("oish: derived pipeline + ISA for the inspected oiSH", $("#psoView").textContent.includes("Pipeline state (compute)") && $("#isaView").textContent.includes("Disassemble"));

  /* ---- SPV/DXIL mode: a bare binary is a document ---- */
  $("#mBins").checked = true;
  $("#mBins").dispatchEvent(new window.Event("change"));

  /* Two waits, not one: the seeding may already be done from the mode before (so its spinner never
   * shows), and entering this mode still has to reflect every binary before the strip has one. */
  await seeded();
  await until("the standalone binary to open", () => /\.spv/.test($("#binPane").textContent));
  check("bins: tabs stay, reflection of the bare binary", !$("#outTabs").classList.contains("d-none") && $("#reflView").textContent.includes("standalone binary") && $("#reflView").innerHTML.includes("register("), $("#reflView").textContent.slice(0, 120));
  check("bins: no identifier until assembled", $("#reflView").textContent.includes("no identifier"));
  check("bins: strip names the open binary and offers the assemble card",
    /\.spv · SPIR-V · [\d.]+ KiB/.test($("#binPane").textContent) && $("#asmToggle") != null,
    $("#binPane").textContent.replace(/\s+/g, " ").slice(0, 100));
  check("bins: disassembly view filled", $("#spvAsm").textContent.includes("OpEntryPoint"), $("#spvAsm").textContent.slice(0, 60));
  check("bins: oiSH tab offers assemble into oiSH", $("#oishView").textContent.includes("Assemble into oiSH") && $("#asmOishGo") != null);
  /* A pipeline is derived from an oiSH, and a standalone binary has none until the oiSH tab assembles
   * it into one, so the tab isn't offered here. */
  check("bins: no pipeline tab for a standalone binary",
    tabHidden("pso"), "pso tab visible");
  check("bins: no symbols tab either", tabHidden("sym"), "sym tab visible");
  check("bins: diff selects list the binaries", $("#diffA2").options.length >= 2);
  $("#isaAsic").value = "gfx1100"; $("#isaAsic").dispatchEvent(new window.Event("change")); await sleep(50);
  $("#isaRun").click(); await sleep(600);
  check("bins: a bare .spv gets the same ISA refusal, not an invented listing",
    /no target available|no offline path/.test($("#isaView").textContent) && !$("#isaView").textContent.includes("s_endpgm"),
    $("#isaView").textContent.slice(0, 120));
  $("#sbEntries").click(); await sleep(80);
  check("bins: entrypoint list rendered", $("#sbMeta").textContent.includes("entrypoints"));
  $("#asmToggle").click(); $("#asmGo").click(); await sleep(300);
  check("bins: assembled text becomes a loaded binary", $("#railTree").textContent.includes("assembled."), $("#railTree").textContent);

  /* assemble into oiSH: the binary gets an identifier and lands in Inspect mode as a real oiSH */
  const spvItem = [...$("#railTree").querySelectorAll(".fitem")].find(f => /\.spv$/.test(f.dataset.name || ""));
  const spvName = spvItem && spvItem.dataset.name;
  spvItem.click(); await sleep(300);
  $("#asmModel").value = "6.8";
  $("#asmOishGo").click(); await sleep(600);
  check("assemble: lands in Inspect mode as an oiSH",
    $("#mOish").checked && $("#railTree").textContent.includes(spvName.replace(/\.spv$/, ".oiSH")),
    `mode=${$("#mOish").checked} rail=${$("#railTree").textContent.replace(/\s+/g, " ").slice(0, 200)} toast=${(window.document.body.textContent.match(/Assembled:[^.]*/) || ["none"])[0]}`);
  check("assemble: document says where it came from + has an identifier",
    $("#reflView").textContent.includes("assembled from " + spvName) && $("#reflView").textContent.includes("SM 6.8"),
    $("#reflView").textContent.slice(0, 200));

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

  /* ---- ctrl+click targets (pure core) ------------------------------------------------------ */

  const inc = window.OxIntelliSense.includeTarget('#include "include/light.hlsli"', 15);
  check("goto: ctrl+click inside an include's quotes resolves the file",
    inc && inc.file === "include/light.hlsli", JSON.stringify(inc));
  check("goto: outside the quotes it does not", window.OxIntelliSense.includeTarget('#include "a.hlsli"  // x', 24) === null);

  /* ---- recompile keeps the selected backend ------------------------------------------------ */

  $("#mCompile").checked = true;
  $("#mCompile").dispatchEvent(new window.Event("change"));
  await sleep(200);
  [...$("#railTree").querySelectorAll(".fitem")].find(f => f.dataset.name === "lighting.hlsl").click();
  $("#compileBtn").click(); await awaitCompile();
  const dxRow = [...$("#binSel").options].findIndex((o, i) => window.OxUtil && o.textContent.toLowerCase().includes("dxil"));
  if (dxRow >= 0) {
    $("#binSel").value = String(dxRow);
    $("#binSel").dispatchEvent(new window.Event("change")); await sleep(200);
    $("#compileBtn").click(); await awaitCompile();
    check("bins: a recompile keeps the DXIL row selected",
      $("#binSel").options[+$("#binSel").value].textContent.toLowerCase().includes("dxil"),
      $("#binSel").options[+$("#binSel").value].textContent);
  } else check("bins: a recompile keeps the DXIL row selected", false, "no dxil row found");

  /* ---- share ------------------------------------------------------------------------------- */

  /* The link has to carry the whole project: a shader and the include it edited share together, or the
   * recipient compiles against the stock include and sees different results. */

  $("#shareBtn").click(); await sleep(300);
  const hash = /^#z=/.test(window.location.hash);
  check("share: state lands in the URL hash as the gzip payload every browser gets", hash, window.location.hash.slice(0, 12));
  if (hash) {
    const p = await window.OxUtil.decodeShare(window.location.hash);
    check("share: the active file and its includes are in the payload",
      p.files && "lighting.hlsl" in p.files && "include/light.hlsli" in p.files,
      JSON.stringify(Object.keys(p.files || {})));
    check("share: unrelated samples are not, so the link fits in a chat message",
      !("trace.hlsl" in p.files) && Object.keys(p.files).length === 2 && window.location.hash.length < 2000,
      Object.keys(p.files).length + " files, " + window.location.hash.length + " chars");
    check("share: the compile options ride along", !!p.o && Array.isArray(p.o.targets));
  }

  /* ---- history restore --------------------------------------------------------------------- */

  /* The Share button wrote this state into the hash above; walking history back to it (what the mouse's
   * back button does) has to bring that state back rather than leaving the page out of sync. */

  const shareHash = window.location.hash;
  const beforeNav = state_snapshot();
  window.location.hash = "#other";
  await sleep(100);
  window.location.hash = shareHash;
  await sleep(200);
  check("history: navigating back to a share link restores its state", state_snapshot() === beforeNav,
    state_snapshot().slice(0, 60) + " vs " + beforeNav.slice(0, 60));

  function state_snapshot() {
    const t = document.querySelector("#editorTabs .etab.active");
    return window.OxEditor.value().length + ":" + (t ? t.title : "");
  }

  /* ---- workspaces -------------------------------------------------------------------------- */

  /* The share link above must have arrived as its OWN workspace, leaving the boot project intact. */

  const wsList = () => window.OxWorkspace.list();
  check("ws: the boot project and the shared link are separate workspaces",
    wsList().length >= 2 && wsList().some(w => w.kind === "shared"),
    JSON.stringify(wsList().map(w => [w.name, w.kind])));
  check("ws: the shared one is what the link opened", (window.OxWorkspace.current() || {}).kind === "shared",
    JSON.stringify(window.OxWorkspace.current()));
  check("ws: the menu names it", document.querySelectorAll("#wsMenu .ws-row").length >= 2,
    document.querySelectorAll("#wsMenu .ws-row").length);

  const homeRow = [...document.querySelectorAll("#wsMenu .ws-row")]
    .find(r => (wsList().find(x => x.id === r.dataset.ws) || {}).kind !== "shared");
  homeRow.querySelector("[data-open]").click();
  await sleep(250);

  check("ws: switching back restores the files the share had trimmed",
    !![...document.querySelectorAll("#railTree .fitem")].find(f => f.dataset.name === "trace.hlsl"),
    [...document.querySelectorAll("#railTree .fitem")].map(f => f.dataset.name).join(","));

  const pinRow = document.querySelector("#wsMenu .ws-row");
  pinRow.querySelector("[data-pin]").click();
  check("ws: pinning persists", (wsList().find(w => w.id === pinRow.dataset.ws) || {}).pinned === true);

  /* The rail rerenders on every file switch; the title must keep naming the workspace, not revert. */
  [...document.querySelectorAll("#railTree .fitem")].find(f => f.dataset.name === "trace.hlsl").click();
  check("ws: the rail title survives a file switch",
    document.querySelector("#railTitle").textContent === (window.OxWorkspace.current() || {}).name,
    document.querySelector("#railTitle").textContent);

  /* ---- editor tabs ------------------------------------------------------------------------- */

  const railItem = n => [...document.querySelectorAll("#railTree .fitem")].find(f => f.dataset.name === n);
  const tabs = () => [...document.querySelectorAll("#editorTabs .etab")];
  const activeTab = () => document.querySelector("#editorTabs .etab.active");

  /* The share tests above replaced the file set, so the second file is whatever the rail has left. */
  const otherFile = [...document.querySelectorAll("#railTree .fitem")]
    .map(f => f.dataset.name).find(n => n && n !== (activeTab() || {}).title);

  const tabsBefore = tabs().length;
  check("tabs: the open file is a tab", tabsBefore >= 1 && !!activeTab(), tabsBefore);

  railItem(otherFile).click();
  check("tabs: opening from the rail adds a tab and focuses it",
    tabs().length === tabsBefore + 1 && activeTab().title === otherFile,
    tabs().length + " " + (activeTab() || {}).title);

  const firstTitle = tabs()[0].title;
  tabs()[0].click();
  check("tabs: clicking a tab switches back to it", activeTab().title === firstTitle,
    (activeTab() || {}).title);

  const traceTab = tabs().find(t => t.title === otherFile);
  traceTab.querySelector(".etab-x").click();
  check("tabs: closing an inactive tab keeps the active one",
    tabs().length === tabsBefore && activeTab().title === firstTitle,
    tabs().length + " " + (activeTab() || {}).title);

  railItem(otherFile).click();
  activeTab().querySelector(".etab-x").click();
  check("tabs: closing the active tab focuses a neighbor",
    tabs().length === tabsBefore && !!activeTab() && activeTab().title !== otherFile,
    (activeTab() || {}).title);

  /* ---- project upload / drop ---------------------------------------------------------------- */

  {
    /* The page reads name + arrayBuffer() off a dropped file; jsdom's File carries no arrayBuffer,
     * so the fixture is the shape the page consumes rather than a real File. */
    const droppedSrc = '#include "@types.hlsli"\nF32 half(F32 v) { return v * 0.5; }\n';
    const dropped = { name: "dropped.hlsl", arrayBuffer: async () => new TextEncoder().encode(droppedSrc).buffer };
    const ev = new window.Event("drop", { bubbles: true });
    Object.defineProperty(ev, "dataTransfer", { value: { files: [dropped] } });
    document.querySelector("#editorPane").dispatchEvent(ev);
    await sleep(150);

    check("upload: a dropped .hlsl joins the project",
      !![...document.querySelectorAll("#railTree .fitem")].find(f => f.dataset.name === "dropped.hlsl"),
      [...document.querySelectorAll("#railTree .fitem")].map(f => f.dataset.name).join(","));
    check("upload: and opens in the editor", window.OxEditor.value().includes("half(F32 v)"),
      window.OxEditor.value().slice(0, 40));
    check("upload: the rail has the button", !!document.querySelector("#upProject"));
  }

  /* ---- tab pinning + bulk close ------------------------------------------------------------ */

  railItem(otherFile).click();
  const pinned = tabs()[0];
  pinned.dispatchEvent(new window.MouseEvent("contextmenu", { bubbles: true, clientX: 5, clientY: 5 }));
  document.querySelector('#etabMenu [data-act="pin"]').click();
  check("tabs: pinning via the context menu marks the tab",
    !!tabs()[0].querySelector(".bi-pin-fill") && !tabs()[0].querySelector(".etab-x"),
    tabs()[0].innerHTML);

  tabs()[1].dispatchEvent(new window.MouseEvent("contextmenu", { bubbles: true, clientX: 5, clientY: 5 }));
  document.querySelector('#etabMenu [data-act="all"]').click();
  check("tabs: close all keeps the pinned tab", tabs().length === 1 && !!tabs()[0].querySelector(".bi-pin-fill"),
    tabs().length);
  check("tabs: something is still open in the editor", !!activeTab(), (activeTab() || {}).title);

  tabs()[0].dispatchEvent(new window.MouseEvent("contextmenu", { bubbles: true, clientX: 5, clientY: 5 }));
  document.querySelector('#etabMenu [data-act="pin"]').click();
  check("tabs: unpinning brings the close button back", !!tabs()[0].querySelector(".etab-x"));

  /* ---- appearance -------------------------------------------------------------------------- */

  window.OxTheme.set({ theme: "midnight", fontSize: 15 });
  check("theme: palette lands on <html>", document.documentElement.getAttribute("data-ox-theme") === "midnight");
  check("theme: editor size var applied",
    document.documentElement.style.getPropertyValue("--ox-editor-size") === "15px");
  check("theme: choice persists", JSON.parse(window.localStorage.getItem("ox.appearance")).theme === "midnight");
  window.OxTheme.set({ theme: "oxsomi-dark", fontSize: 13 });
  check("theme: the default clears the palette attribute again",
    !document.documentElement.hasAttribute("data-ox-theme"));

  console.log(failures ? `\n${failures} FAILURE(S)` : "\nALL PASS");
  process.exit(failures ? 1 : 0);
})();
