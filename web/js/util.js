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
   The table is one cell per pair of lines, so two things keep it from being the size of the product:
   equal lines at both ends are trimmed off first, which is most of two disassemblies of the same
   shader, and what is left is capped. Past the cap the differing middle is reported as one block
   replaced by another rather than allocating hundreds of megabytes for a pair of large listings.
   The cap also keeps every count inside the Uint16 cells, since a subsequence can be no longer than
   the shorter side. */

const DIFF_MAX_CELLS = 4e6;   //8 MB of table, which is a 2000 by 2000 diff done exactly

function lineDiff(A, B) {

  const ops = [];

  /* The shared head and tail, which never need a table */

  let head = 0, endA = A.length, endB = B.length;

  while (head < endA && head < endB && A[head] === B[head])
    head++;

  while (endA > head && endB > head && A[endA - 1] === B[endB - 1]) {
    endA--;
    endB--;
  }

  for (let i = 0; i < head; i++)
    ops.push({ t: "=", a: A[i] });

  const a = A.slice(head, endA), b = B.slice(head, endB);
  const n = a.length, m = b.length;

  const middle = n && m && n * m <= DIFF_MAX_CELLS ? lcsOps(a, b) : null;

  if (middle)
    ops.push(...middle);

  else {

    /* One side is empty, or the pair is too large to align line by line */

    for (let i = 0; i < n; i++) ops.push({ t: "-", a: a[i] });
    for (let j = 0; j < m; j++) ops.push({ t: "+", b: b[j] });
  }

  for (let i = endA; i < A.length; i++)
    ops.push({ t: "=", a: A[i] });

  return ops;
}

function lcsOps(A, B) {

  const n = A.length, m = B.length;

  /* L[i][j] = how many lines the longest common subsequence of A[i..] and B[j..] holds. Filled from the
     end backwards so each cell only reads ones already written, and kept in one flat array because a
     row of arrays costs an allocation per line. The row past the end stays 0, which is what makes the
     first real row correct. */

  const L = new Uint16Array((n + 1) * (m + 1));
  const at = (i, j) => i * (m + 1) + j;

  for (let i = n - 1; i >= 0; i--)
    for (let j = m - 1; j >= 0; j--)
      L[at(i, j)] =
        A[i] === B[j]
          ? L[at(i + 1, j + 1)] + 1
          : Math.max(L[at(i + 1, j)], L[at(i, j + 1)]);

  /* Walk both sides forwards, taking the step the table says loses nothing: equal lines are kept, and
     otherwise whichever of dropping A[i] or taking B[j] leaves the longer subsequence. The tie goes to
     the deletion, so a replaced line reads as its removal followed by its addition. */

  const ops = [];
  let i = 0, j = 0;

  while (i < n && j < m) {

    if (A[i] === B[j]) {
      ops.push({ t: "=", a: A[i] });
      i++;
      j++;
    }

    else if (L[at(i + 1, j)] >= L[at(i, j + 1)]) {
      ops.push({ t: "-", a: A[i] });
      i++;
    }

    else {
      ops.push({ t: "+", b: B[j] });
      j++;
    }
  }

  /* Whatever is left on one side once the other ran out is a pure removal or addition. */

  while (i < n)
    ops.push({ t: "-", a: A[i++] });

  while (j < m)
    ops.push({ t: "+", b: B[j++] });

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
/* A link is a stranger's bytes. The checksum only says it arrived whole, so the payload is checked for
   shape before anything downstream believes it, and every refusal throws the way a corrupt link does:
   the caller already turns that into one message.
   The size cap is against the compressed form lying about what it holds, since a small link can
   decompress into gigabytes. */

const SHARE_MAX_BYTES = 16 * 1024 * 1024;
const SHARE_MAX_FILES = 512;
const SHARE_MAX_NAME = 512;

function shareRefuse(why) {
  throw new Error("the link doesn't hold a project (" + why + ")");
}

function checkSharePayload(p) {

  if (!p || typeof p !== "object" || Array.isArray(p))
    shareRefuse("it isn't an object");

  /* The optional scalars: the active file, the mode, and the single file form older links carry */

  for (const k of ["f", "m", "src"])
    if (p[k] != null && typeof p[k] !== "string")
      shareRefuse(k + " isn't text");

  /* The optional objects: compile options and the view to restore */

  for (const k of ["o", "v"])
    if (p[k] != null && (typeof p[k] !== "object" || Array.isArray(p[k])))
      shareRefuse(k + " isn't an object");

  if (p.files == null)
    return p;

  if (typeof p.files !== "object" || Array.isArray(p.files))
    shareRefuse("its file set isn't an object");

  const names = Object.keys(p.files);

  if (names.length > SHARE_MAX_FILES)
    shareRefuse(names.length + " files");

  for (const name of names) {

    /* A name reaches the file tree and the editor's tabs, so it stays a plain relative path: no
       absolute root, and no segment that climbs out of the project. */

    if (!name || name.length > SHARE_MAX_NAME)
      shareRefuse("a file name is empty or too long");

    if (name.startsWith("/") || name.startsWith("\\") || /(^|[\\/])\.\.([\\/]|$)/.test(name))
      shareRefuse("a file name climbs out of the project");

    if (typeof p.files[name] !== "string")
      shareRefuse("the contents of " + name + " aren't text");
  }

  return p;
}

async function decodeShare(hash) {

  let m;

  if ((m = /z=([A-Za-z0-9+/=]+)/.exec(hash))) {
    let raw;
    try { raw = await pipeThrough(unb64(m[1]), new DecompressionStream("gzip")); }
    catch (e) { throw new Error("the link is truncated or corrupt (its checksum doesn't match)"); }
    if (raw.length > SHARE_MAX_BYTES) shareRefuse("it unpacks to more than the page will hold");
    return checkSharePayload(JSON.parse(new TextDecoder().decode(raw)));
  }

  if ((m = /s=([^&]+)(?:&c=([0-9a-f]+))?/.exec(hash))) {
    const json = decodeURIComponent(escape(atob(m[1])));
    if (m[2] && crc32c(new TextEncoder().encode(json)) !== parseInt(m[2], 16))
      throw new Error("the link is truncated or corrupt (its checksum doesn't match)");
    if (json.length > SHARE_MAX_BYTES) shareRefuse("it unpacks to more than the page will hold");
    return checkSharePayload(JSON.parse(json));
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
  encodeShare, decodeShare, checkSharePayload };
})();
