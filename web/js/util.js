/* util.js: small shared helpers. No app state in here. */
(function () {
"use strict";

const $  = s => document.querySelector(s);
const $$ = s => Array.from(document.querySelectorAll(s));

function esc(s) {
  return String(s).replace(/[&<>"]/g, c => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c]));
}

/* CRC32C (Castagnoli, reflected poly 0x82F63B78): same algorithm the CLI reports for
   source hashes and include hashes (Buffer_crc32c). Deterministic so the mock's hashes
   are stable across reloads and diffs line up. */
const CRC_T = (() => {
  const t = new Uint32Array(256);
  for (let n = 0; n < 256; n++) {
    let c = n;
    for (let k = 0; k < 8; k++) c = (c & 1) ? (0x82F63B78 ^ (c >>> 1)) : (c >>> 1);
    t[n] = c >>> 0;
  }
  return t;
})();
function crc32c(input) {
  const bytes = typeof input === "string" ? new TextEncoder().encode(input) : input;
  let c = 0xFFFFFFFF;
  for (let i = 0; i < bytes.length; i++) c = CRC_T[(c ^ bytes[i]) & 0xFF] ^ (c >>> 8);
  return (c ^ 0xFFFFFFFF) >>> 0;
}
const hex8 = v => (v >>> 0).toString(16).toUpperCase().padStart(8, "0");

function fmtBytes(n) {
  if (n == null) return "-";
  if (n < 1024) return n + " B";
  if (n < 1024 * 1024) return (n / 1024).toFixed(n < 10240 ? 2 : 1) + " KiB";
  return (n / 1048576).toFixed(2) + " MiB";
}

function download(name, data, mime) {
  const blob = new Blob([data], { type: mime || "application/octet-stream" });
  const a = document.createElement("a");
  a.href = URL.createObjectURL(blob); a.download = name;
  document.body.appendChild(a); a.click(); a.remove();
  setTimeout(() => URL.revokeObjectURL(a.href), 800);
}

/* Deterministic PRNG from a seed: used by the mock to fabricate stable "binary" content. */
function mulberry32(seed) {
  let a = seed >>> 0;
  return function () {
    a |= 0; a = (a + 0x6D2B79F5) | 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

/* Line diff (LCS). Returns ops: {t:'=', a}|{t:'-', a}|{t:'+', b}. Inputs are arrays of lines.
   O(n*m): fine for disassembly-sized inputs; swap for Myers if real disassemblies get huge. */
function lineDiff(A, B) {
  const n = A.length, m = B.length;
  const L = new Uint16Array((n + 1) * (m + 1));
  const at = (i, j) => i * (m + 1) + j;
  for (let i = n - 1; i >= 0; i--)
    for (let j = m - 1; j >= 0; j--)
      L[at(i, j)] = A[i] === B[j] ? L[at(i + 1, j + 1)] + 1 : Math.max(L[at(i + 1, j)], L[at(i, j + 1)]);
  const ops = [];
  let i = 0, j = 0;
  while (i < n && j < m) {
    if (A[i] === B[j]) { ops.push({ t: "=", a: A[i] }); i++; j++; }
    else if (L[at(i + 1, j)] >= L[at(i, j + 1)]) { ops.push({ t: "-", a: A[i] }); i++; }
    else { ops.push({ t: "+", b: B[j] }); j++; }
  }
  while (i < n) ops.push({ t: "-", a: A[i++] });
  while (j < m) ops.push({ t: "+", b: B[j++] });
  return ops;
}

/* ---- share codec -------------------------------------------------------------------------- */

/* State -> URL hash payload and back. gzip when the browser has CompressionStream: 3 to 5 times
 * smaller, and gzip's own CRC32 footer makes truncation loud (a clipped link fails to inflate instead
 * of quietly restoring half a project). Without it, plain base64 with an explicit crc32c suffix does
 * the same detection. decodeShare accepts every format ever emitted, so old links keep working. */

const b64 = bytes => btoa(String.fromCharCode(...bytes));
const unb64 = str => Uint8Array.from(atob(str), c => c.charCodeAt(0));

async function pipeThrough(bytes, stream) {
  const out = new Response(new Blob([bytes]).stream().pipeThrough(stream));
  return new Uint8Array(await out.arrayBuffer());
}

async function encodeShare(obj) {
  const json = JSON.stringify(obj);
  const raw = new TextEncoder().encode(json);
  if (typeof CompressionStream === "function")
    return "z=" + b64(await pipeThrough(raw, new CompressionStream("gzip")));
  return "s=" + btoa(unescape(encodeURIComponent(json))) + "&c=" + crc32c(raw).toString(16);
}

/* Returns the object, or throws with a reason a toast can show (truncated, corrupt, unknown format). */
async function decodeShare(hash) {

  let m;

  if ((m = /z=([A-Za-z0-9+/=]+)/.exec(hash))) {
    let raw;
    try { raw = await pipeThrough(unb64(m[1]), new DecompressionStream("gzip")); }
    catch (e) { throw new Error("the link is truncated or corrupt (its checksum doesn't match)"); }
    return JSON.parse(new TextDecoder().decode(raw));
  }

  if ((m = /s=([^&]+)(?:&c=([0-9a-f]+))?/.exec(hash))) {
    const json = decodeURIComponent(escape(atob(m[1])));
    if (m[2] && crc32c(new TextEncoder().encode(json)) !== parseInt(m[2], 16))
      throw new Error("the link is truncated or corrupt (its checksum doesn't match)");
    return JSON.parse(json);
  }

  throw new Error("no share payload in the URL");
}

function debounce(fn, ms) {
  let t = null;
  return function (...args) { clearTimeout(t); t = setTimeout(() => fn.apply(this, args), ms); };
}

/* ---- syntax highlighting for the text views ---------------------------------------------------
 *
 * One left-to-right scan per view, not a chain of replaces over the whole string. A chain looks
 * simpler but is wrong: once a comment is wrapped, a later pass still sees the text inside it, so a
 * number in a comment gets its own color and the comment stops reading as one. Scanning consumes a
 * token whole, so nothing inside one is ever looked at again.
 *
 * Each highlighter is a list of [class, pattern]. Patterns are tried in order at the cursor, all of
 * them sticky so they can only match where the cursor is; the first hit wins and the cursor jumps
 * past it. Anything nothing matches is copied across escaped. Adding a language means adding a rule
 * list, not touching the scanner. */

function scan(text, rules) {

  let out = "", i = 0;
  const src = text || "";

  outer: while (i < src.length) {

    for (const [cls, re] of rules) {
      re.lastIndex = i;
      const m = re.exec(src);
      if (!m || m.index !== i) continue;
      out += `<span class="${cls}">${esc(m[0])}</span>`;
      i += m[0].length;
      continue outer;
    }

    out += esc(src[i++]);
  }

  return out;
}

/* SPIR-V and DXIL LL share a shape: an optional result id, a mnemonic, then operands. One rule list
 * covers both, since neither vocabulary collides with the other.
 * Comments and strings come first so their contents are consumed rather than scanned. */
const ASM_RULES = [
  ["hl-cmt", /;[^\n]*/y],
  ["hl-str", /"(?:[^"\\\n]|\\.)*"/y],
  ["hl-id", /[%!][A-Za-z0-9_.]+/y],
  ["hl-op", /\bOp[A-Z]\w*/y],
  ["hl-kw", /\b(?:define|declare|call|ret|br|label|entry|void|float|double|half|i1|i8|i16|i32|i64|ptr|align|nounwind|readnone|readonly|target|datalayout|triple|attributes|metadata)\b/y],
  ["hl-num", /-?\b\d+(?:\.\d+)?\b/y]
];

/* SPFile_print output: every line is a comment, so the shape inside one is what carries meaning.
 * The provenance word gets the color its chip has in the table, since that is the thing being read. */
const PIPELINE_RULES = [
  ["hl-head", /;\s*Pipeline state[^\n]*/y],
  ["hl-note", /;\s+NOTE:[^\n]*/y],
  ["prov-derived", /\bderived\b/y],
  ["prov-supplied", /\bsupplied\b/y],
  ["prov-assumed", /\bassumed\b/y],
  ["hl-field", /\b[a-z][\w]*(?:\.[\w]+)+(?:\[\d+\])?/y],
  ["hl-num", /-?\b\d+(?:\.\d+)?\b/y],
  ["hl-cmt", /^;/my]
];

const highlightAsm = text => scan(text, ASM_RULES);
const highlightPipeline = text => scan(text, PIPELINE_RULES);

window.OxUtil = {
  highlightAsm, highlightPipeline, $, $$, esc, crc32c, hex8, fmtBytes, download, mulberry32, lineDiff, debounce,
  encodeShare, decodeShare };
})();
