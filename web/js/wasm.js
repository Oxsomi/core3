/* wasm.js: the OxC3_wasm module behind js/api.js.
 *
 * This is the only file that knows the module exists. It owns three things api.js shouldn't
 * have to: the call frame every export answers with, wasm64 pointer marshalling, and the
 * project tree the compiler resolves #includes against.
 *
 * Call frame. Every export returns one allocation laid out as
 *     U64 jsonLength; U64 blobLength; C8 json[jsonLength]; U8 blob[blobLength]
 * so a call that produces a document, bytes, or both is decoded and freed the same way
 * (see src/tools/oxc3_wasm/wasm_bridge.h). A `json.error` field is the C side's own message.
 *
 * Pointers. The build is memory64, and emscripten does NOT wrap these exports, so a pointer
 * crosses as a BigInt in both directions. Everything below passes BigInt and converts to
 * Number only to index the heap; ccall is unused for the same reason (it can't convert one).
 * A heap view is taken from wasmMemory per use, because growing memory replaces the buffer.
 *
 * Project tree. The compiler resolves an #include through the ordinary file API against the
 * working directory, so the project is written into the module's filesystem under PROJECT_ROOT
 * and the working directory is set there before the platform is created. Nothing about include
 * resolution is bridged; it is the same code path the CLI takes.
 *
 * Diagnostics. Compile errors reach a caller of Compiler_compileShaders through the log, so a
 * call captures the module's output and parses it back. The severity is the ANSI colour the
 * log writes (red is an error, yellow a warning), which is what tells a diagnostic apart from
 * progress output; the location is the `file:line:col:` prefix the compiler logs.
 *
 * Calls block the thread they run on. Moving the module into a Worker changes nothing above
 * this file, since every method here is already async.
 */
(function () {
"use strict";

const FRAME_HEADER = 16;              // two U64 lengths
const PROJECT_ROOT = "/project";

const encoder = new TextEncoder();
const decoder = new TextDecoder();

/* Compile flags, matching EWasmCompileFlag in src/tools/oxc3_wasm/wasm_main.c. */
const FLAG = {
  debug: 1 << 0, keepRegisters: 1 << 1, noOpt: 1 << 7,
  warnUnusedRegisters: 1 << 2, warnUnusedConstants: 1 << 3, warnBufferPadding: 1 << 4,
  ignoreEmptyFiles: 1 << 5, reflectionOnly: 1 << 6
};

const BACKEND = { spv: 0, spirv: 0, dxil: 1 };

let M = null;                          // the instantiated module
let written = new Set();               // files currently in the module's filesystem
let captured = null;                   // log lines of the call in flight

const api = {
  ready: false,
  version: null,
  capabilities: null
};

/* ---- heap + frames -------------------------------------------------------------------- */

function heap() { return new Uint8Array(M.wasmMemory.buffer); }

/* The module heap's current size; growth that keeps climbing across identical calls is a leak. */
api.heapBytes = () => M ? M.wasmMemory.buffer.byteLength : 0;

function alloc(size) {
  const p = M._oxc3_alloc(size);
  if (!p)
    throw new Error(`OxC3_wasm: allocation of ${size} bytes failed (heap ${api.heapBytes()} bytes, ` +
      `${api.ready ? "module initialized" : "module NOT initialized"}); the captured log may name the cause`);
  return p;                            // BigInt
}

function free(p) { if (p) M._oxc3_free(p); }

/* Bytes into the module's heap, NUL terminated so the same call serves C strings. */
function put(bytes) {
  const p = alloc(bytes.length + 1);
  const h = heap();
  h.set(bytes, Number(p));
  h[Number(p) + bytes.length] = 0;
  return p;
}

const putString = s => put(encoder.encode(s == null ? "" : String(s)));

/* Decodes a returned frame and frees it. Throws on the module's own error field. */
function frame(ptr, what) {
  if (!ptr) throw new Error(`${what}: the module returned nothing (out of memory?)`);
  const h = heap();
  const base = Number(ptr);
  const view = new DataView(h.buffer, h.byteOffset + base, FRAME_HEADER);
  const jsonLength = Number(view.getBigUint64(0, true));
  const blobLength = Number(view.getBigUint64(8, true));
  const body = base + FRAME_HEADER;
  const json = jsonLength ? decoder.decode(h.subarray(body, body + jsonLength)) : "{}";
  const blob = blobLength ? h.slice(body + jsonLength, body + jsonLength + blobLength) : null;
  free(ptr);
  const doc = JSON.parse(json);
  if (doc.error) throw new Error(`${what}: ${doc.error}`);
  return { doc, blob };
}

/* Frees every pointer the call put on the heap, whether it threw or not. */
function withPointers(fn) {
  const owned = [];
  const hold = p => { owned.push(p); return p; };
  try { return fn(hold); }
  finally { owned.forEach(free); }
}

/* ---- log capture + diagnostics ---------------------------------------------------------- */

const ANSI = /\x1b\[[0-9;]*m/g;

/* The log closes every line with a reset, and the line break comes after it, so a line arrives here
 * carrying the previous line's reset in front of its own colour. Anchoring would read that reset as
 * the severity and drop every diagnostic, so this takes the first colour that isn't one. */
const COLOUR = /\x1b\[1;(3\d)m/;
const LOCATED = /^(.*?):(\d+):(\d+):\s*([\s\S]*)$/;

const projectRelative = p => p.replace(/^\/*project\/*/, "").replace(/^\.\//, "");

function capture(line) { if (captured) captured.push(line); }

function beginCapture() { captured = []; }

function endCapture() {
  const lines = captured || [];
  captured = null;
  return lines;
}

/* The log's own machinery around a failure: the stacktrace and its frames, the error record's field
 * dump, and the per-file narration that restates what the diagnostic beside it already said. None of
 * it is a diagnostic, and each line would otherwise surface as one more "error" on line 1. */
const LOG_NOISE = [
  /^Stacktrace:/,
  /^at (wasm|0x)/, /^0x[0-9a-f]+:/i, /^:\?$/,
  /^sub id: /, /^Platform\/std error/,
  /^(Precompile|Compile|Link) failed/,
  /^One of the previous oiSH compilations failed/
];

/* Log lines carry a `[threadId timestamp]: ` prefix; the message is what matters to a reader. */
const LOG_PREFIX = /^\[\d+ [0-9TZ:.+\-]+\]:\s*/;

/* An internal reason reads as `Function() what went wrong`, the shape retError writes. */
const INTERNAL = /^[A-Za-z_][\w:]*\(\)/;

/* The compiler logs a diagnostic coloured by severity: red is an error, yellow a warning, and every
 * other colour is progress output.
 * DXC's own diagnostics carry a `file:line:col:` prefix; the compiler's extra warnings
 * (Compiler_handleExtraWarnings) name a binary and a register instead and have no location at all,
 * so those are kept against line 1 of the file the caller asked about rather than dropped.
 * The library's `Function() reason` lines are kept only while no located diagnostic exists: they are
 * the whole answer when the failure never reached DXC (a combine refusing mismatched layouts), and
 * plumbing when a source diagnostic already names the line. */
function diagnostics(lines, fallbackFile) {
  const out = [];
  for (const raw of lines) {
    const colour = COLOUR.exec(raw);
    if (!colour) continue;                                  // not a log line
    const sev = colour[1] === "31" ? "error" : colour[1] === "33" ? "warn" : null;
    if (!sev) continue;                                     // debug / performance output
    const text = raw.replace(ANSI, "").replace(/\r?\n$/, "").trim().replace(LOG_PREFIX, "");
    if (!text || LOG_NOISE.some(rx => rx.test(text))) continue;
    const located = LOCATED.exec(text);
    if (located && /\.(hlsl|hlsli)$/i.test(located[1])) {
      const col = Math.max(0, +located[3] - 1);
      out.push({ sev, file: projectRelative(located[1]), line: +located[2], ch0: col, ch1: col + 1,
        msg: located[4], code: "", located: true });
    } else out.push({ sev, file: fallbackFile, line: 1, ch0: 0, ch1: 1, msg: text, code: "" });
  }
  const hasLocatedError = out.some(d => d.located && d.sev === "error");
  return out.filter(d => !(hasLocatedError && !d.located && d.sev === "error" && INTERNAL.test(d.msg)));
}

/* ---- project tree ----------------------------------------------------------------------- */

function mkdirp(dir) {
  const parts = dir.split("/").filter(Boolean);
  let path = "";
  for (const part of parts) {
    path += "/" + part;
    try { M.FS.mkdir(path); } catch (e) { /* already there */ }
  }
}

/* Mirrors the page's project into the module's filesystem, dropping what the page removed so a
 * deleted include stops resolving instead of lingering as a stale file. */
function syncProject(files) {
  const names = Object.keys(files || {});
  const wanted = new Set(names);
  for (const name of written)
    if (!wanted.has(name)) {
      try { M.FS.unlink(PROJECT_ROOT + "/" + name); } catch (e) { /* already gone */ }
    }
  for (const name of names) {
    const dir = name.lastIndexOf("/");
    if (dir > 0) mkdirp(PROJECT_ROOT + "/" + name.slice(0, dir));
    M.FS.writeFile(PROJECT_ROOT + "/" + name, files[name].src == null ? "" : files[name].src);
  }
  written = wanted;
}

/* ---- lifecycle ---------------------------------------------------------------------------- */

/* factory: the createOxC3Module the build emits. base/locateFile: where the .wasm sits beside it. */
api.load = async function load(options) {
  if (api.ready) return { version: api.version, capabilities: api.capabilities };
  const opts = options || {};
  const factory = opts.factory || (typeof window !== "undefined" ? window.createOxC3Module : null);
  if (typeof factory !== "function")
    throw new Error("OxC3_wasm: no createOxC3Module factory (load wasm/OxC3_wasm.js first)");

  M = await factory({
    locateFile: opts.locateFile || (f => (opts.base || "wasm/") + f),
    print: capture,
    printErr: capture
  });

  /* The working directory is what the platform records and what every relative include
   * resolves against, so it is set before the platform exists rather than after. */
  mkdirp(PROJECT_ROOT);
  M.FS.chdir(PROJECT_ROOT);

  const { doc } = frame(M._oxc3_init(), "init");
  api.version = doc.version;
  api.capabilities = doc.capabilities;
  api.ready = true;
  if (typeof window !== "undefined" && typeof window.addEventListener === "function")
    window.addEventListener("beforeunload", () => { try { M._oxc3_shutdown(); } catch (e) { /* leaving */ } });
  return { version: api.version, capabilities: api.capabilities };
};

api.shutdown = function shutdown() {
  if (!api.ready) return;
  M._oxc3_shutdown();
  api.ready = false;
};

/* ---- calls ---------------------------------------------------------------------------------- */

api.builtinIncludes = async function builtinIncludes() {
  return frame(M._oxc3_builtinIncludes(), "builtinIncludes").doc.includes;
};

/* OxC3 shader compile. opts mirrors the toolbar: targets, debug, keepRegisters, reflectionOnly,
 * warnUnusedRegisters, warnUnusedConstants, warnBufferPadding, ignoreEmptyFiles. */
api.compile = async function compile(name, files, opts) {
  const o = opts || {};
  syncProject(files);
  let mask = 0;
  for (const t of (o.targets && o.targets.length ? o.targets : ["spv", "dxil"]))
    mask |= 1 << BACKEND[t];
  let flags = 0;
  for (const key of Object.keys(FLAG)) if (o[key]) flags |= FLAG[key];

  beginCapture();
  try {
    return withPointers(hold => {
      const result = M._oxc3_compileShaders(
        hold(putString(name)), hold(putString(files[name] ? files[name].src : "")), mask, flags
      );
      const { doc, blob } = frame(result, "compile");
      return { doc, bytes: blob, diags: diagnostics(endCapture(), name) };
    });
  } catch (e) {
    /* A failed compile is a result the page renders, not an exception: the diagnostics are the
     * answer, and the module's own message is kept as one of them when nothing else parsed. */
    const diags = diagnostics(endCapture(), name);
    if (!diags.some(d => d.sev === "error"))
      diags.push({ sev: "error", file: name, line: 1, ch0: 0, ch1: 1, msg: e.message, code: "" });
    return { doc: null, bytes: null, diags };
  }
};

/* Reflecting an include drives it as the main file, which is the only way a lone file can be parsed,
 * and DXC rightly warns that #pragma once means nothing there. The warning describes how the outline
 * is produced rather than the user's code, so for a .hlsli it is dropped. */
function reflectDiags(name) {
  const diags = diagnostics(endCapture(), name);
  return /\.hlsli$/i.test(name) ? diags.filter(d => !/#pragma once in main file/.test(d.msg)) : diags;
}

/* OxC3 shader reflect-symbols: runs on source, so it takes the project rather than a compile. */
/* allowErrors: describe what parsed instead of refusing outright, which is what an editor wants while
 * a file is being typed. The diagnostics still come back either way. */
/* cfg (optional): { disabledExt, defines }, the "IntelliSense follows this binary" permutation.
 * disabledExt masks extensions OUT of the parse; defines is {NAME: value} appended the way a
 * binary's uniforms are. Absent cfg = the everything-enabled default parse. */
api.reflectSymbols = async function reflectSymbols(name, files, allowErrors, cfg) {
  syncProject(files);
  beginCapture();
  try {
    return withPointers(hold => {
      const defines = cfg && cfg.defines
        ? Object.entries(cfg.defines).map(([k, v]) => v == null || v === "" ? k : `${k}=${v}`).join("\n")
        : "";
      const result = M._oxc3_reflectSymbols(
        hold(putString(name)), hold(putString(files[name] ? files[name].src : "")),
        allowErrors === false ? 0 : 1,
        (cfg && cfg.disabledExt) >>> 0, hold(putString(defines))
      );
      const { doc, blob } = frame(result, "reflectSymbols");
      return { doc, bytes: blob, diags: reflectDiags(name) };
    });
  } catch (e) {
    return { doc: null, bytes: null, diags: reflectDiags(name), error: e.message };
  }
};

api.shRead = async function shRead(name, bytes) {
  return withPointers(hold =>
    frame(M._oxc3_shRead(hold(put(bytes)), bytes.length, hold(putString(name))), "shRead").doc);
};

api.shCombine = async function shCombine(name, a, b) {
  return withPointers(hold => {
    const { doc, blob } = frame(M._oxc3_shCombine(
      hold(put(a)), a.length, hold(put(b)), b.length, hold(putString(name))
    ), "shCombine");
    return { doc, bytes: blob };
  });
};

api.shExtractBinary = async function shExtractBinary(bytes, binaryId, backend) {
  return withPointers(hold =>
    frame(M._oxc3_shExtractBinary(hold(put(bytes)), bytes.length, binaryId, BACKEND[backend]),
      "shExtractBinary").blob);
};

api.fileHeader = async function fileHeader(bytes) {
  return withPointers(hold => frame(M._oxc3_fileHeader(hold(put(bytes)), bytes.length), "fileHeader").doc);
};

api.disassemble = async function disassemble(backend, bytes) {
  return withPointers(hold =>
    frame(M._oxc3_disassemble(BACKEND[backend], hold(put(bytes)), bytes.length), "disassemble").doc.text);
};

api.assemble = async function assemble(backend, text) {
  return withPointers(hold =>
    frame(M._oxc3_assemble(BACKEND[backend], hold(putString(text))), "assemble").blob);
};

api.uniqueEntrypoints = async function uniqueEntrypoints(backend, bytes, showAll) {
  return withPointers(hold =>
    frame(M._oxc3_uniqueEntrypoints(BACKEND[backend], hold(put(bytes)), bytes.length, showAll ? 1 : 0),
      "uniqueEntrypoints").doc.entrypoints);
};

api.srRead = async function srRead(name, bytes) {
  return withPointers(hold =>
    frame(M._oxc3_srRead(hold(put(bytes)), bytes.length, hold(putString(name))), "srRead").doc);
};

/* picks: entry indices to bind, as the CLI's -entry does; empty selects the way the CLI selects
 * and answers with {refused, candidates} when it would otherwise have to guess. */
api.spDerive = async function spDerive(bytes, shaderName, picks) {
  return withPointers(hold => {
    const { doc, blob } = frame(M._oxc3_spDerive(
      hold(put(bytes)), bytes.length, hold(putString(shaderName)),
      hold(putString((picks || []).join(",")))
    ), "spDerive");
    return doc.refused ? doc : { doc, bytes: blob };
  });
};

api.spSupply = async function spSupply(bytes, pipelineId, fieldPath, value, name) {
  return withPointers(hold => {
    const { doc, blob } = frame(M._oxc3_spSupply(
      hold(put(bytes)), bytes.length, pipelineId, hold(putString(fieldPath)), value >>> 0,
      hold(putString(name))
    ), "spSupply");
    return { doc, bytes: blob };
  });
};

api.spPrint = async function spPrint(bytes, pipelineId) {
  return withPointers(hold =>
    frame(M._oxc3_spPrint(hold(put(bytes)), bytes.length, pipelineId), "spPrint").doc.text);
};

api.spRead = async function spRead(name, bytes) {
  return withPointers(hold =>
    frame(M._oxc3_spRead(hold(put(bytes)), bytes.length, hold(putString(name))), "spRead").doc);
};

api.isaHasOfflinePath = async function isaHasOfflinePath(spirv) {
  return withPointers(hold =>
    frame(M._oxc3_isaHasOfflinePath(hold(put(spirv)), spirv.length), "isaHasOfflinePath").doc.offline);
};

/* Project snapshot: the synced working tree packed as a real .oiCA, and the reverse. Sync first so
 * the archive holds what the page shows, not what the last compile happened to mirror. */
api.caPack = async function caPack(files) {
  syncProject(files);
  return frame(M._oxc3_caPack(), "caPack").blob;
};

api.caUnpack = async function caUnpack(bytes) {
  return withPointers(hold =>
    frame(M._oxc3_caUnpack(hold(put(bytes)), bytes.length), "caUnpack").doc.files);
};

/* The annotation vocabularies (extension + vendor names) straight off the oiSH enums. */
api.annotationEnums = async function annotationEnums() {
  return frame(M._oxc3_annotationEnums(), "annotationEnums").doc;
};

api.spFieldVocab = async function spFieldVocab() {
  return frame(M._oxc3_spFieldVocab(), "spFieldVocab").doc;
};

api.validate = async function validate(backend, bytes) {
  return withPointers(hold => frame(M._oxc3_validate(BACKEND[backend], hold(put(bytes)), bytes.length), "validate").doc);
};

api.isaTargets = async function isaTargets() {
  return frame(M._oxc3_isaTargets(), "isaTargets").doc.targets;
};

api.isaDisassemble = async function isaDisassemble(spirv, asic, entrypoint) {
  return withPointers(hold =>
    frame(M._oxc3_isaDisassemble(
      hold(put(spirv)), spirv.length, hold(putString(asic)), hold(putString(entrypoint))
    ), "isaDisassemble").doc.text);
};

api.syncProject = files => syncProject(files);

if (typeof window !== "undefined") window.OxWasm = api;
else if (typeof self !== "undefined") self.OxWasm = api;   //worker scope (js/wasm_worker.js)
if (typeof module !== "undefined" && module.exports) module.exports = api;
})();
