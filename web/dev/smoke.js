/* smoke.js: boots index.html in jsdom (local scripts only, CodeMirror stubbed) and walks
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
    setCursor: () => {}, scrollIntoView: () => {}, focus: () => {}, lineCount: () => 999, refresh: () => {}
  };
};
Object.defineProperty(window.navigator, "clipboard", { value: { writeText: async () => {} } });
window.TextEncoder = TextEncoder;
/* The share codec's gzip path: every shipped browser has CompressionStream, jsdom's window has none, and
 * without it encodeShare falls back to plain base64, which is not the link users get. */
for (const k of ["CompressionStream", "DecompressionStream", "Blob", "Response"]) window[k] = globalThis[k];
window.URL.createObjectURL = () => "blob:mock"; window.URL.revokeObjectURL = () => {};
window.performance = window.performance || { now: () => Date.now() };
window.prompt = () => null;

/* editor.js drives cm via closure; our stub's setValue fires change: mimic real enough.
 * But editor.open() suppresses the callback itself, so that's fine. */

/* js/wasm.js is loaded but never instantiates anything: no createOxC3Module is defined here, so
 * OxAPI.init() falls back to the mocks, which is the half this test covers. */
for (const f of ["js/util.js", "js/workspace.js", "js/theme.js", "js/mock_data.js", "js/mock.js", "js/mock_formats.js", "js/wasm.js", "js/wasm_rpc.js",
  "js/api.js", "js/editor.js", "js/intellisense.js", "js/asmmap.js",
  "js/tools/compile.js", "js/tools/inspect.js", "js/tools/symbols.js", "js/tools/pipeline.js", "js/tools/isa.js",
  "js/tools/diff.js", "js/tools/binary.js", "js/app.js"]) {
  window.eval(fs.readFileSync(path.join(ROOT, f), "utf8"));
}

const $ = s => window.document.querySelector(s);
const sleep = ms => new Promise(r => setTimeout(r, ms));
let failures = 0;
/* A view the open document can't answer for is hidden rather than left to fail (applyContext). */
const tabHidden = t => $(`#outTabs [data-tab="${t}"]`).classList.contains("d-none");

function check(name, cond, extra) {
  console.log((cond ? "  ok " : "FAIL ") + name + (cond ? "" : "  " + (extra || "")));
  if (!cond) failures++;
}

(async () => {
  window.document.dispatchEvent(new window.Event("DOMContentLoaded", { bubbles: true }));
  await sleep(50);

  check("boot: rail lists project files", $("#railTree").textContent.includes("lighting.hlsl"));

  /* This harness never loads a module, so it is the mock, and the page has to say so where it cannot be
   * missed: every compile and every tree below is invented, and nothing else on the page shows that. */
  check("boot: the mock backend announces itself", !$("#mockBanner").classList.contains("d-none"));
  check("boot: and says what to do about it", $("#mockBannerText").textContent.includes("--single_file"));
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
    $("#reflView").innerHTML.includes("vk::binding") && $("#reflView").innerHTML.includes("register(b0"));
  check("compile: oiSB layout rendered", $("#reflView").innerHTML.includes("0x00000040"));
  check("compile: oiSH tab has feature set + header", $("#oishView").textContent.includes("Feature set") && $("#oishView").textContent.includes("Source hash"));
  check("compile: problems include unused-register warning", $("#probBody").textContent.includes("--warn-unused-registers"));

  /* New problems open the panel on their own; the same problems again do not, so a deliberate close
   * holds until the diagnostics actually change. */

  check("problems: the panel revealed itself", !window.document.body.classList.contains("prob-collapsed"));
  window.document.body.classList.add("prob-collapsed");
  $("#compileBtn").click(); await sleep(900);
  check("problems: unchanged diagnostics respect a close", window.document.body.classList.contains("prob-collapsed"));

  /* One click hands every diagnostic over as text, for pasting into an issue or a chat. */
  let copied = "";
  window.navigator.clipboard.writeText = async t => { copied = t; };
  $("#probCopy").click(); await sleep(50);
  check("problems: copy puts file:line:col text on the clipboard",
    /:\d+:\d+: warn: /.test(copied), JSON.stringify(copied.slice(0, 80)));
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
  $("#compileBtn").click(); await sleep(900);
  check("failed compile: a good compile recovers", $("#stCompiled").textContent.includes("binaries"),
    $("#stCompiled").textContent);

  /* The switches describe the view, so they have to outlive the reflection: Symbols follows the editor
   * and re-renders on its own, which used to clear them on the next keystroke or file switch. */
  $("#symHlslTypes").checked = true;
  $("#symHlslTypes").dispatchEvent(new window.Event("change"));
  await sleep(150);

  /* ---- graphics pipeline: post.hlsl has two pixel variants -> refused with an -entry picker, then assumed fields ---- */
  [...$("#railTree").querySelectorAll(".fitem")].find(f => f.dataset.name === "post.hlsl").click();
  $("#compileBtn").click(); await sleep(1100);
  check("symbols: HLSL types survives a re-reflect", $("#symHlslTypes").checked && $("#symView").textContent.includes("(float4)"));
  $("#symHlslTypes").checked = false;
  $("#symHlslTypes").dispatchEvent(new window.Event("change"));
  await sleep(150);
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

  /* Enum-typed fields edit as dropdowns of their value names, mask fields as per-bit checkboxes and
   * binary choices as toggles; the vocabularies come off the compiler's own name tables. */
  check("pso: enum fields render as dropdowns", !!document.querySelector("#psoView select.sp-val"),
    document.querySelectorAll("#psoView select.sp-val").length);
  check("pso: mask fields render as per-bit checkboxes", !!document.querySelector("#psoView .sp-bits input[data-bit]"));
  check("pso: a binary choice renders as a toggle",
    !!document.querySelector('#psoView input.sp-val[type="checkbox"]'));

  /* The follows picker describes the COMPILED file's binaries: another active file falls back to the
   * default parse instead of applying a config that belongs to a different source. */
  check("follows: options exist for the compiled file", $("#reflectFollow").options.length > 1,
    $("#reflectFollow").options.length);
  [...document.querySelectorAll("#railTree .fitem")].find(f => f.dataset.name === "lighting.hlsl").click();
  check("follows: switching files resets the picker to the default parse",
    $("#reflectFollow").options.length === 1 && $("#reflectFollow").value === "",
    $("#reflectFollow").options.length);
  [...document.querySelectorAll("#railTree .fitem")].find(f => f.dataset.name === "post.hlsl").click();
  check("follows: the compiled file's options come back with it",
    $("#reflectFollow").options.length > 1, $("#reflectFollow").options.length);

  /* The Download menu must offer the derived .oiSP even while every field is assumed: the menu is
   * rebuilt by the derivation itself, not only by a later supply. */
  check("dl: a fully assumed pipeline is still downloadable",
    !!document.querySelector('#dlMenu [data-act="oisp"]'),
    document.querySelector("#dlMenu").textContent.slice(0, 80));

  /* ---- ISA refusal: ray tracing lib has no offline path ---- */
  [...$("#railTree").querySelectorAll(".fitem")].find(f => f.dataset.name === "trace.hlsl").click();
  $("#compileBtn").click(); await sleep(1100);
  /* rgen carries two extension sets -> two lib binaries -> refused until -entry picks one */
  check("pipeline: two libs refused with a picker", $("#psoView").textContent.includes("Refused") && $("#psoView").querySelector("[data-pick]") != null, $("#psoView").textContent.slice(0, 200));
  /* Two libs: rgen alone under its RayQuery set, and the empty set every stage joins. The whole
   * pipeline lives in the second kind, so pick by content rather than by position in the list. */
  const pickLib = $("#psoView").querySelector("[data-pick]");
  const fullLib = [...pickLib.options].find(o => o.value !== "" && !/RayQuery/i.test(o.textContent));
  pickLib.value = fullLib.value; pickLib.dispatchEvent(new window.Event("change")); await sleep(200);
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

  /* The reflection runs on the source, so the file that would not compile still has an outline, and the
   * compile must not be what takes it away. */

  check("compile-fail: the symbols tree survives the compile",
    $("#symView").textContent.includes("Namespace"), $("#symView").textContent.slice(0, 120));
  selectTab("sym"); await sleep(100);
  check("compile-fail: and is not covered by the overlay", $("#failOverlay").style.display === "none");

  /* The mock reads the source in front of it, so a file it parses heuristically has no built-in symbols.
   * It must not name them with a count either, or the tree claims hundreds it cannot show and the switch
   * has nothing to expand. */

  check("compile-fail: no built-in symbols are claimed that aren't there",
    !$("#symView").textContent.includes("builtin-include symbols"), $("#symView").textContent.slice(0, 200));
  check("compile-fail: so --includes is offered as unavailable", $("#symBuiltins").disabled);
  selectTab("refl"); await sleep(100);

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
  /* A pipeline is derived from an oiSH, and a standalone binary has none until the oiSH tab assembles
   * it into one, so the tab isn't offered here. */
  check("bins: no pipeline tab for a standalone binary",
    tabHidden("pso"), "pso tab visible");
  check("bins: no symbols tab either", tabHidden("sym"), "sym tab visible");
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
  $("#compileBtn").click(); await sleep(900);
  const dxRow = [...$("#binSel").options].findIndex((o, i) => window.OxUtil && o.textContent.toLowerCase().includes("dxil"));
  if (dxRow >= 0) {
    $("#binSel").value = String(dxRow);
    $("#binSel").dispatchEvent(new window.Event("change")); await sleep(200);
    $("#compileBtn").click(); await sleep(900);
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
    const dropped = new window.File(['#include "@types.hlsli"\nF32 half(F32 v) { return v * 0.5; }\n'],
      "dropped.hlsl", { type: "text/plain" });
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
