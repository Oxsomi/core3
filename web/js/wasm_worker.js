/* wasm_worker.js: hosts the OxC3 module inside a Web Worker, so compiles and reflects never block
 * typing, hover, or paint on the main thread. The page side is js/wasm_rpc.js, which speaks this
 * tiny protocol: {id, fn, args} in, {id, result} or {id, error} out. "load" is special: it pulls in
 * js/wasm.js (the module boundary, already environment-agnostic) plus the module itself, and answers
 * with the boundary's method names so the proxy can mirror the surface without hardcoding it.
 * Everything else is OxWasm[fn](...args) verbatim; results cross by structured clone. */
"use strict";

self.onmessage = async e => {

  const { id, fn, args } = e.data;

  try {
    let result;

    if (fn === "load") {
      const opts = args[0] || {};
      importScripts(opts.boundaryUrl, opts.moduleUrl);
      result = await self.OxWasm.load({
        factory: self.createOxC3Module,
        locateFile: f => opts.baseUrl + f
      });
      result.methods = Object.keys(self.OxWasm).filter(k => typeof self.OxWasm[k] === "function");
    }

    else
      result = await self.OxWasm[fn](...args);

    postMessage({ id, result });
  }

  catch (err) {
    postMessage({ id, error: err && err.message ? err.message : String(err) });
  }
};
