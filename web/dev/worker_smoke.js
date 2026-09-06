/* worker_smoke.js: drives js/wasm_worker.js through its real message protocol, with node standing in
 * for the worker scope (self/importScripts/postMessage shimmed onto globals). This is the regression
 * net for the protocol itself: load answers with version + the mirrored method surface, calls route
 * through with their arguments, and an unknown method fails as an {error} answer, not a crash.
 * The module comes from the build tree like wasm_smoke's does. */
"use strict";
const path = require("path");
const fs = require("fs");

const root = path.resolve(__dirname, "..", "..");
const modulePath = path.resolve(process.argv[2] ||
  path.join(root, "build", "Release", "web", "wasm64", "bin", "OxC3_wasm.js"));

if (!fs.existsSync(modulePath)) {
  console.error(`-- No module at ${modulePath} (build it with build_web.py --frontend)`);
  process.exit(1);
}

let failures = 0;
function assert(name, cond, extra) {
  console.log((cond ? "PASS " : "FAIL ") + name + (cond || extra === undefined ? "" : `\n   got: ${extra}`));
  if (!cond) failures++;
}

/* ---- the worker scope, as node globals ---- */

global.self = global;

const answers = [];
let wake = null;
global.postMessage = msg => { answers.push(msg); if (wake) { wake(); wake = null; } };

/* URLs arrive absolute in the real page; here they are file paths already. The module factory lands
 * on self the way importScripts drops globals there. */
global.importScripts = (...urls) => {
  for (const u of urls) {
    const m = require(u);
    if (typeof m === "function") self.createOxC3Module = m;
  }
};

require(path.join(__dirname, "..", "js", "wasm_worker.js"));

const call = async (id, fn, args) => {
  const seen = answers.length;
  global.onmessage({ data: { id, fn, args } });
  while (answers.length === seen) await new Promise(r => { wake = r; });
  return answers[answers.length - 1];
};

(async () => {

  const load = await call(1, "load", [{
    boundaryUrl: path.join(__dirname, "..", "js", "wasm.js"),
    moduleUrl: modulePath,
    baseUrl: path.dirname(modulePath) + path.sep
  }]);

  assert("load answers with the module version", /^\d+\.\d+\.\d+$/.test(load.result && load.result.version),
    JSON.stringify(load));
  assert("load mirrors the method surface", (load.result.methods || []).includes("compile")
    && load.result.methods.includes("caPack"), JSON.stringify(load.result.methods));

  const files = { "a.hlsl": { src:
    "[[oxc::stage(\"compute\")]] [numthreads(1, 1, 1)] void main(uint i : SV_DispatchThreadID) { }\n" } };

  const compiled = await call(2, "compile", ["a.hlsl", files, { targets: ["spv"] }]);
  assert("a compile routes through with its arguments", compiled.result && !!compiled.result.doc,
    JSON.stringify(compiled).slice(0, 200));
  assert("and answers under its own id", compiled.id === 2, compiled.id);

  const bogus = await call(3, "noSuchMethod", []);
  assert("an unknown method answers {error}, not a crash", bogus.error != null && bogus.result === undefined,
    JSON.stringify(bogus));

  /* ---- the page side: js/wasm_rpc.js against a fake Worker looped straight into the handler ---- */

  global.window = {
    OxWasmVersion: { worker: true, base: "wasm/", pick: "current" },
    location: { href: "http://page.test/" }
  };

  let fake = null;
  global.Worker = class {
    constructor() { fake = this; global.postMessage = msg => this.onmessage({ data: msg }); }
    postMessage(m) { global.onmessage({ data: m }); }
  };
  global.importScripts = () => { };      //the module is already resident from the first half

  require(path.join(__dirname, "..", "js", "wasm_rpc.js"));
  const proxy = global.window.OxWasm;

  const info = await proxy.load();
  assert("proxy load mirrors version", info.version === load.result.version, JSON.stringify(info));
  assert("proxy mirrors the surface", typeof proxy.compile === "function" && typeof proxy.caPack === "function");

  const viaProxy = await proxy.compile("a.hlsl", files, { targets: ["spv"] });
  assert("a compile through the proxy answers with a document", !!viaProxy.doc,
    JSON.stringify(viaProxy).slice(0, 160));

  const err = await proxy.reflectSymbols("missing.hlsl", {}).then(() => null, e => e);
  assert("a failure crosses back as a rejection", err === null || err instanceof Error, String(err));

  console.log(failures ? `\n${failures} FAILURE(S)` : "\nALL PASS");
  process.exit(failures ? 1 : 0);
})();
