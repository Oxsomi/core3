/* wasmload.js: picks which OxC3 wasm module the page loads, so the hosted site can keep older module
 * builds around when a change breaks previously shared links (a dxc update, a format bump).
 *
 * The choice must exist before the module script loads, so this runs synchronously during parse and
 * document.writes the tag: a remembered pick from localStorage names a subfolder under wasm/, and the
 * default is wasm/ itself, exactly what a checkout without extra versions serves. Which versions exist
 * comes from wasm/versions.json, fetched async only to offer the picker; that file is a hosting
 * artifact (web/wasm/ is gitignored) and absent everywhere else, which just means no picker.
 * The hosting layout is documented in web/README.md. */
(function () {
"use strict";

/* A remembered pick names a subfolder under wasm/. It is a stored string rather than a chosen one, so
   it is filtered down to the characters a folder name may hold: that is what keeps a tampered value
   from walking out of wasm/ or ending up somewhere else entirely. */

function moduleBase(pick) {

  if (!pick || pick === "current")
    return "wasm/";

  const folder = String(pick).replace(/[^\w.-]/g, "");
  return folder && folder !== "." && folder !== ".." ? "wasm/" + folder + "/" : "wasm/";
}

let pick = null;
try { pick = localStorage.getItem("ox.wasmVersion"); } catch (e) { }

const base = moduleBase(pick);

/* Off the main thread whenever possible: js/wasm_rpc.js hosts the two file module (OxC3_wasm.js +
 * .wasm) in a Web Worker on any http(s) origin, and then the page must NOT also parse the 20+ MB
 * module script here. file:// cannot spawn workers or fetch the sidecar .wasm, so only there does a
 * module load in-page, and it is the embedded flavor (OxC3_wasm_sf.js) that carries its wasm inline. */
const worker = typeof Worker !== "undefined" && location.protocol !== "file:";

window.OxWasmVersion = { pick: pick || "current", base, worker, moduleBase };

/* A version whose files went away must not brick the page: onerror drops the pick so the next load is
 * the default again, and this load falls back to the mocks like any missing module does. */
if (!worker)
  document.write(
    '<script src="' + base + 'OxC3_wasm_sf.js"' +
    ' onerror="try{localStorage.removeItem(&quot;ox.wasmVersion&quot;)}catch(e){}"><\/script>');

window.OxWasmVersions = (async () => {
  try {
    const r = await fetch("wasm/versions.json");
    if (!r.ok) return null;
    const v = await r.json();
    return Array.isArray(v.versions) && v.versions.length ? v.versions : null;
  } catch (e) { return null; }
})();
})();
