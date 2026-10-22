/* lazy_smoke.js: the page with no module and no preloaded recording, which is the load the other
 * suites structurally cannot see: smoke.js stages a module and the unit tests preload js/mock_data.js,
 * so a consumer that reads the recording at script load passes everywhere except in a real browser.
 *
 * This boots index.html the way that browser does on the no-module path: js/mock_samples.js is a
 * script tag, js/mock_data.js is not, and the one fetch the page performs for it is served off disk
 * here. What has to come out the other side is the recorded tier: the badge says mock, the vocabulary
 * lists fill, the builtin includes rail fills, and Inspect mode lists the recorded documents.
 *
 * Run: npm i jsdom && node dev/lazy_smoke.js (or point NODE_PATH at a node_modules that has jsdom). */
const { JSDOM } = require("jsdom");
const fs = require("fs"), path = require("path");
const ROOT = path.join(__dirname, "..");

const html = fs.readFileSync(path.join(ROOT, "index.html"), "utf8");
const dom = new JSDOM(html, { url: "https://shader.oxsomi.com/", pretendToBeVisual: true, runScripts: "outside-only" });
const { window } = dom;
global.window = window; global.document = window.document;

/* ---- the same stubs smoke.js carries, minus the module ---- */

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
for (const k of ["CompressionStream", "DecompressionStream", "Blob", "Response"]) window[k] = globalThis[k];
window.URL.createObjectURL = () => "blob:mock"; window.URL.revokeObjectURL = () => {};
window.performance = window.performance || { now: () => Date.now() };
window.prompt = () => null;

/* The one fetch a browser would do and jsdom will not: loadRecording appends a script tag for
 * js/mock_data.js, which this serves off disk the way a host would. Counted, because fetching it more
 * than once or before anything asked would each be its own bug. */

let recordingLoads = 0;
const head = window.document.head;
const realAppend = head.appendChild.bind(head);

head.appendChild = el => {
  if (el.tagName === "SCRIPT" && /js\/mock_data\.js$/.test(el.src || "")) {
    ++recordingLoads;
    window.eval(fs.readFileSync(path.join(ROOT, "js", "mock_data.js"), "utf8"));
    setTimeout(() => el.onload && el.onload(), 0);
    return el;
  }
  return realAppend(el);
};

/* Every script the page itself loads, except the recording: that absence is the point */

for (const f of ["js/util.js", "js/workspace.js", "js/theme.js", "js/mock_samples.js", "js/intrinsics_data.js",
  "js/mock.js", "js/mock_formats.js", "js/wasm.js", "js/wasm_rpc.js",
  "js/api.js", "js/editor.js", "js/intellisense.js", "js/asmmap.js",
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

async function until(what, cond, ms = 30000) {
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

(async () => {

  check("nothing fetched the recording before the page ran", recordingLoads === 0 && !window.OxMockData);

  window.document.dispatchEvent(new window.Event("DOMContentLoaded", { bubbles: true }));

  await until("the backend badge to say mock", () => $("#stBackend").textContent === "mock");

  check("the page fetched the recording itself, once", recordingLoads === 1, recordingLoads);

  /* The vocabularies come out of the late recording through the api fallbacks */

  await until("the extension list to fill", () => $("#extList").querySelectorAll(".tok").length > 0);
  check("the vendor list filled too", $("#vendorList").textContent.trim().length > 0);

  /* The builtin includes rail renders the recorded sources */

  await until("the builtin includes rail to fill", () => $("#builtinFiles").children.length > 0);
  check("the builtin rail names the umbrella", $("#builtinFiles").textContent.includes("@types.hlsli"));

  /* Inspect mode lists the recorded documents, which is what the no-module page is for */

  $("#mOish").checked = true;
  $("#mOish").dispatchEvent(new window.Event("change", { bubbles: true }));

  await until("the recorded documents to list", () => $("#railTree").textContent.includes("lighting.v1.oiSH"));
  check("both recorded versions are there", $("#railTree").textContent.includes("lighting.v2.oiSH"));

  check("and the recording still only loaded once", recordingLoads === 1, recordingLoads);

  console.log(failures ? `\n${failures} FAILURE(S)` : "\nALL PASS");
  process.exit(failures ? 1 : 0);
})();
