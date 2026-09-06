/* wasm_rpc.js: swaps the in-page OxWasm for a proxy to js/wasm_worker.js when the page can use one.
 * js/wasmload.js decides that (any http(s) origin; file:// cannot spawn workers, so there the module
 * stays in-page via its script tag) and skips loading the module on the main thread when it says yes,
 * which also spares first paint a 20+ MB parse. Same method names, same promises; api.js can't tell
 * the difference, it reads window.OxWasm per call. */
(function () {
"use strict";

if (typeof window === "undefined" || !window.OxWasmVersion || !window.OxWasmVersion.worker) return;

let worker = null, nextId = 1;
const pending = new Map();

function rpc(fn, args) {
  return new Promise((resolve, reject) => {
    const id = nextId++;
    pending.set(id, { resolve, reject });
    worker.postMessage({ id, fn, args });
  });
}

function failAll(message) {
  for (const p of pending.values()) p.reject(new Error(message));
  pending.clear();
}

const remote = {
  async load() {

    if (worker) return { version: remote.version, capabilities: remote.capabilities };

    worker = new Worker("js/wasm_worker.js");
    worker.onmessage = e => {
      const p = pending.get(e.data.id);
      if (!p) return;
      pending.delete(e.data.id);
      if (e.data.error != null) p.reject(new Error(e.data.error));
      else p.resolve(e.data.result);
    };
    worker.onerror = ev => failAll("OxC3_wasm worker: " + (ev.message || "failed to start"));

    const base = new URL(window.OxWasmVersion.base, window.location.href).href;
    const info = await rpc("load", [{
      boundaryUrl: new URL("js/wasm.js", window.location.href).href,
      moduleUrl: base + "OxC3_wasm.js",
      baseUrl: base
    }]);

    remote.version = info.version;
    remote.capabilities = info.capabilities;
    for (const name of info.methods)
      if (!(name in remote)) remote[name] = (...args) => rpc(name, args);

    return { version: info.version, capabilities: info.capabilities };
  }
};

window.OxWasm = remote;
})();
