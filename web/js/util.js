/* util.js — small shared helpers. No app state in here. */
(function () {
"use strict";

const $  = s => document.querySelector(s);
const $$ = s => Array.from(document.querySelectorAll(s));

function esc(s) {
  return String(s).replace(/[&<>"]/g, c => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c]));
}

/* CRC32C (Castagnoli, reflected poly 0x82F63B78) — same algorithm the CLI reports for
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
  if (n == null) return "—";
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

/* Deterministic PRNG from a seed — used by the mock to fabricate stable "binary" content. */
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
   O(n*m) — fine for disassembly-sized inputs; swap for Myers if real disassemblies get huge. */
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

function debounce(fn, ms) {
  let t = null;
  return function (...args) { clearTimeout(t); t = setTimeout(() => fn.apply(this, args), ms); };
}

window.OxUtil = { $, $$, esc, crc32c, hex8, fmtBytes, download, mulberry32, lineDiff, debounce };
})();
