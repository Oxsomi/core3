/* mock.js — the mock backend.
 *
 * Everything in this file fabricates plausible data so the UI is fully explorable before the
 * WASM port exists. js/api.js is the only consumer; once the real compiler is wired up there,
 * this file can be deleted wholesale.
 *
 * The document shape produced here ("SHDocument") mirrors formats/oiSH (SHFile / SHEntry /
 * SHBinaryInfo / SHRegisterRuntime / SBFile) — see the contract comment at the top of js/api.js.
 */
(function () {
"use strict";
const U = window.OxUtil;

/* ------------------------------------------------------------------ constants (match core3) */

const VERSION = { major: 3, minor: 2, patch: 102 };                  // OXC3_MAJOR/MINOR/PATCH

const STAGES = [                                                     // SHEntry_stageNames
  "vertex", "pixel", "compute", "geometry", "hull", "domain",
  "raygeneration", "callable", "miss", "closesthit", "anyhit", "intersection",
  "mesh", "task", "node"
];
const LIB_STAGES = new Set(["raygeneration", "callable", "miss", "closesthit", "anyhit", "intersection", "node"]);
const STAGE_PROFILE = { vertex: "vs", pixel: "ps", geometry: "gs", hull: "hs", domain: "ds", compute: "cs", mesh: "ms", task: "as" };

const EXTENSIONS = [                                                 // ESHExtension_names
  "F64", "I64", "16BitTypes", "AtomicI64", "AtomicF32", "AtomicF64",
  "SubgroupArithmetic", "SubgroupShuffle", "RayQuery", "RayMicromapOpacity", "RayTriPosition",
  "RayMotionBlur", "RayReorder", "Multiview", "ComputeDeriv", "PAQ", "MeshTaskTexDeriv",
  "WriteMSTexture", "Bindless", "UnboundArraySize", "SubgroupOperations",
  "CoopVec", "CoopMat", "CoopFP8", "CoopVecTraining", "DescriptorHeap"
];
const EXT_SPV_ONLY  = new Set(["AtomicF32", "AtomicF64", "SubgroupArithmetic", "SubgroupShuffle", "RayMotionBlur"]); // ESHExtension_NoDxilCompile
const EXT_DXIL_ONLY = new Set(["MeshTaskTexDeriv"]);                                                                // ESHExtension_NoSpirvCompile
const VENDORS = ["NV", "AMD", "ARM", "QCOM", "INTC", "IMGT", "MSFT", "APPL", "SMSG", "HWEI", "GOGL", "MESA"];   // ESHVendor

/* ------------------------------------------------------------------ builtin includes (@…) */
/* Real builtin list from src/shader_compiler/compiler.cpp — excerpts only; the full sources
 * live in include/shader_compiler/shaders/. types/resources are umbrellas: mat, indirect and
 * fixed_point were split out of types.hlsli, buffer and appdata out of resources.hlsli, and the
 * umbrellas include them so a shader that only says @types.hlsli still sees everything. */

const BUILTINS = {
  "@types.hlsli":
`#pragma once
// @types.hlsli (excerpt) — portable type aliases + macros (umbrella: pulls mat, indirect, fixed_point).
#include "@mat.hlsli"
#include "@indirect.hlsli"
#include "@fixed_point.hlsli"
typedef uint  U32;   typedef float  F32;   typedef int  I32;   typedef bool Bool;
typedef uint2 U32x2; typedef float2 F32x2; typedef int2 I32x2;
typedef uint3 U32x3; typedef float3 F32x3; typedef int3 I32x3;
typedef uint4 U32x4; typedef float4 F32x4; typedef int4 I32x4;
typedef float4x4 F32x4x4; typedef float3x3 F32x3x3; /* … all xNxM (HLSL float4x3 = F32x3x4) … */
#ifdef __OXC_EXT_16BITTYPES
  typedef float16_t F16;  typedef float16_t4 F16x4; /* … I16/U16 + matrices … */
#endif
#ifdef __OXC_EXT_F64
  typedef double F64; typedef double4 F64x4; /* … */
#endif
#define _bind(x) TEXCOORD##x
#define _flat nointerpolation
#ifdef __spirv__
  #define UNKNOWN_FORMAT [[vk::image_format("unknown")]]
  #define PUSH_CONSTANT [[vk::push_constant]]
#else
  #define UNKNOWN_FORMAT
  #define PUSH_CONSTANT
#endif`,

  "@mat.hlsli":
`#pragma once
// @mat.hlsli (excerpt) — matrix helpers split out of types.hlsli: F32x4x4 transforms,
// quaternion <-> matrix, orthonormal bases, compose/decompose.`,

  "@indirect.hlsli":
`#pragma once
// @indirect.hlsli (excerpt) — indirect command structs (draw, draw indexed, dispatch,
// dispatch rays) laid out the way OxC3's indirect buffers expect them.`,

  "@fixed_point.hlsli":
`#pragma once
// @fixed_point.hlsli (excerpt) — fixed point packing helpers (Qm.n <-> float, saturating ops).`,

  "@resources.hlsli":
`#pragma once
#include "@types.hlsli"
#include "@buffer.hlsli"
#include "@appdata.hlsli"
// @resources.hlsli (excerpt) — OxC3's bindless resource access (umbrella: pulls buffer, appdata).
static const U32 ResourceId_mask = (1 << 17) - 1;
// getReadTexture2DF32x4(id) / getRWBuffer(id) / … descriptor-heap style accessors,
// so most shaders don't declare registers directly.`,

  "@buffer.hlsli":
`#pragma once
// @buffer.hlsli (excerpt) — typed loads/stores over OxC3's bindless buffers by ResourceId.`,

  "@appdata.hlsli":
`#pragma once
// @appdata.hlsli (excerpt) — the per-frame app data buffer (the generated vertex stand-in a live
// ISA run binds reads its values from here, so the driver can't fold them into the pixel stage).`,

  "@extensions.hlsli":
`#pragma once
// @extensions.hlsli — umbrella pulling each enabled extension's helpers
// (one #include "@extension.<X>.hlsli" per __OXC_EXT_<X> define).`,

  "@extension.CoopVec.hlsli":
`#pragma once
// @extension.CoopVec.hlsli (excerpt) — per-thread matrix×vector (SM6.10):
// DXIL dx::linalg, SPIR-V SPV_NV_cooperative_vector, hidden behind macros.
#define OXC_COOPVEC_MATRIX_BUFFER(name) /* BAB (DXIL) / RWStructuredBuffer (SPIRV) */
#define OXC_COOPVEC_VECTOR_BUFFER(name)
// OXC_COOPVEC_MATVEC_4X4_F16(mat, off, v)  (+ _BIAS, _FP8W, _I8 variants)
// OXC_COOPVEC_OUTER_PRODUCT_ACCUMULATE_4X4_F16(mat, off, a, b)  (CoopVecTraining)`,

  "@extension.CoopMat.hlsli":
`#pragma once
// @extension.CoopMat.hlsli (excerpt) — subgroup GEMM (SM6.10):
// DXIL dx::linalg, SPIR-V SPV_KHR_cooperative_matrix.
// OXC_COOPMAT_GEMM_16_F16(a, b)`,

  "@extension.RayReorder.hlsli":
`#pragma once
// @extension.RayReorder.hlsli (excerpt) — Shader Execution Reordering
// (dx::HitObject wrappers; raygeneration only).`,

  "@extension.RayTriPosition.hlsli":
`#pragma once
// @extension.RayTriPosition.hlsli (excerpt) — SM6.10 triangle vertex position fetch
// (RayQuery + ray pipeline).`,

  "@extension.RayMicromapOpacity.hlsli":
`#pragma once
// @extension.RayMicromapOpacity.hlsli (excerpt) — opacity micromaps.`,

  "@extension.AtomicF32.hlsli":
`#pragma once
// @extension.AtomicF32.hlsli (excerpt) — float32 atomics (SPIR-V only to compile).`,

  "@extension.AtomicF64.hlsli":
`#pragma once
// @extension.AtomicF64.hlsli (excerpt) — float64 atomics (SPIR-V only to compile).`
};

/* ------------------------------------------------------------------ sample project */

const SAMPLE_FILES = {
  "lighting.hlsl": { diags: [
      { sev: "warn", line: 21, ch0: 8, ch1: 13, msg: "implicit truncation of vector type", code: "W_conversion" }],
    src:
`#include "@types.hlsli"
#include "include/light.hlsli"

[[oxc::uniforms(U32 MAX_LIGHTS = 64)]]
cbuffer Globals : register(b0) {
    F32x4x4 viewProj;
    F32x3   cameraPos;
    U32     lightCount;
};

StructuredBuffer<Light> _lights : register(t0);
Texture2D<F32x4>        _albedo : register(t1);
SamplerState            _sampler: register(s0);
RWTexture2D<F32x4>      _output : register(u0);

[shader("compute")]
[numthreads(8, 8, 1)]
void main(U32x3 id : SV_DispatchThreadID) {
    F32x4 c = _albedo.Load(int3(id.xy, 0));
    for (U32 i = 0; i < lightCount; ++i)
        c.rgb += shadeLight(_lights[i], cameraPos);
    _output[id.xy] = c;
}` },

  "include/light.hlsli": { diags: [], src:
`#pragma once
#include "@types.hlsli"

struct Light {
    F32x3 position;
    F32   radius;
    F32x3 color;
    F32   intensity;
};

F32x3 shadeLight(Light l, F32x3 cam) {
    F32x3 d = l.position - cam;
    return l.color * l.intensity / (1.0 + dot(d, d));
}` },

  "post.hlsl": { diags: [], src:
`#include "@types.hlsli"

struct PostCmd {
    F32x2 invRes;
    F32   exposure;
    U32   flags;
    F32x3 tint;
};

PUSH_CONSTANT PostCmd cmd;

Texture2D<F32x4> _color   : register(t0);
SamplerState     _sampler : register(s0);

struct VSOut {
    F32x4 pos : SV_Position;
    F32x2 uv  : TEXCOORD0;
};

[[oxc::stage("vertex")]]
VSOut vsMain(U32 vid : SV_VertexID) {
    VSOut o;
    o.uv  = F32x2((vid << 1) & 2, vid & 2);
    o.pos = F32x4(o.uv * 2 - 1, 0, 1);
    return o;
}

[[oxc::defines("TONEMAP_ACES")]]
[[oxc::defines()]]
[[oxc::stage("pixel")]]
F32x4 psMain(VSOut i) : SV_Target {
    F32x4 c = _color.SampleLevel(_sampler, i.uv, 0) * cmd.exposure;
#ifdef $TONEMAP_ACES
    c.rgb = saturate((c.rgb * (2.51 * c.rgb + 0.03)) / (c.rgb * (2.43 * c.rgb + 0.59) + 0.14));
#endif
    return c * F32x4(cmd.tint, 1);
}` },

  "trace.hlsl": { diags: [], src:
`#include "@types.hlsli"

struct Payload { F32x4 color; U32 depth; };

RaytracingAccelerationStructure _tlas : register(t0);
RWTexture2D<F32x4>              _out  : register(u0);

// Two extension sets on rgen -> two lib compiles; chit joins the empty-set one.
[[oxc::model("6.5")]]
[[oxc::extension("RayQuery")]]
[[oxc::extension()]]
[shader("raygeneration")]
void rgen() {
    Payload p = (Payload)0;
    // TraceRay(_tlas, ...);
    _out[DispatchRaysIndex().xy] = p.color;
}

[shader("closesthit")]
void chit(inout Payload p, BuiltInTriangleIntersectionAttributes attr) {
    p.color = F32x4(attr.barycentrics, 0, 1);
}` },

  "coop_vec.hlsl": { diags: [], src:
`#include "@types.hlsli"
#include "@extension.CoopVec.hlsli"

OXC_COOPVEC_MATRIX_BUFFER(_weights)     : register(u0);
OXC_COOPVEC_VECTOR_BUFFER(_activations) : register(u1);

[[oxc::model("6.10")]]
[[oxc::extension("CoopVec", "16BitTypes")]]
[[oxc::vendor("NV")]]
[shader("compute")]
[WaveSize(32)]
[numthreads(64, 1, 1)]
void main(U32x3 id : SV_DispatchThreadID) {
    // per-thread matrix × vector (FP16):
    // OXC_COOPVEC_MATVEC_4X4_F16(_weights, 0, _activations[id.x])
}` },

  "broken.hlsl": { diags: [
      { sev: "error", line: 8, ch0: 22, ch1: 30, msg: "use of undeclared identifier 'position'", code: "E_undeclared" }],
    src:
`#include "@types.hlsli"

RWTexture2D<F32x4> _output : register(u0);

[shader("compute")]
[numthreads(8, 8, 1)]
void main(U32x3 id : SV_DispatchThreadID) {
    _output[id.xy] = position;
}` }
};

/* ------------------------------------------------------------------ type helpers */

const HLSL_TO_ESB = { float: "F32", int: "I32", uint: "U32", bool: "Bool", double: "F64",
  half: "F16", float16_t: "F16", int16_t: "I16", uint16_t: "U16", int64_t: "I64", uint64_t: "U64" };

function normType(t) {                              // "float4" -> "F32x4", "float4x3" -> "F32x3x4", passthrough aliases
  let m = t.match(/^([a-z_0-9]+?)(\d)x(\d)$/);
  if (m && HLSL_TO_ESB[m[1]]) return `${HLSL_TO_ESB[m[1]]}x${m[3]}x${m[2]}`;   // HLSL RxC -> F32xCxR (docs/oiSB.md)
  m = t.match(/^([a-z_0-9]+?)([2-4])$/);
  if (m && HLSL_TO_ESB[m[1]]) return `${HLSL_TO_ESB[m[1]]}x${m[2]}`;
  if (HLSL_TO_ESB[t]) return HLSL_TO_ESB[t];
  return t;                                          // already F32x4 / U32x3 / struct name
}
function esbInfo(t) {                                // {base bytes, comps, vecs(matrix columns)} or null when a struct
  const m = normType(t).match(/^([FIU])(8|16|32|64)(?:x([2-4]))?(?:x([2-4]))?$/);
  if (normType(t) === "Bool") return { base: 4, comps: 1, vecs: 1 };
  if (!m) return null;
  return { base: (+m[2]) / 8, comps: m[3] ? +m[3] : 1, vecs: m[4] ? +m[4] : 1 };
}

/* HLSL constant-buffer packing (16-byte rows, vectors don't straddle, arrays/matrices/structs
 * row-aligned with 16-byte element stride). Good enough for the mock oiSB view. */
function layoutCbuffer(members, structs) {
  const vars = []; let off = 0, padding = 0;
  const a16 = v => (v + 15) & ~15;
  for (const mem of members) {
    const st = structs[normType(mem.type)];
    const info = st ? null : esbInfo(mem.type);
    let size, stride = null, children = null;
    if (st) {
      const sub = layoutCbuffer(st.members, structs);
      size = a16(sub.size); stride = size; children = sub.vars; padding += sub.padding;
      const na = a16(off); padding += na - off; off = na;
    } else if (!info) { size = 16; const na = a16(off); padding += na - off; off = na; }
    else if (info.vecs > 1) {                        // matrix: one 16B row per column vector
      size = info.vecs * 16; stride = 16;
      const na = a16(off); padding += na - off; off = na;
    } else {
      size = info.comps * info.base;
      if (mem.arrays.length) { stride = a16(size); size = stride * mem.arrays.reduce((x, y) => x * y, 1); const na = a16(off); padding += na - off; off = na; }
      else if ((off & 15) + size > 16) { const na = a16(off); padding += na - off; off = na; }
    }
    vars.push({ offset: off, name: mem.name, type: normType(mem.type), stride, arrays: mem.arrays, children });
    off += size;
  }
  const size = a16(off); padding += size - off;
  return { size, padding, vars };
}
function layoutTight(members, structs) {             // StructuredBuffer<T> element layout
  const vars = []; let off = 0, maxA = 4;
  for (const mem of members) {
    const st = structs[normType(mem.type)];
    let size, stride = null, children = null;
    if (st) { const sub = layoutTight(st.members, structs); size = sub.size; stride = size; children = sub.vars; }
    else { const i = esbInfo(mem.type) || { base: 4, comps: 4, vecs: 1 }; size = i.base * i.comps * i.vecs; maxA = Math.max(maxA, i.base); off = (off + i.base - 1) & ~(i.base - 1); }
    if (mem.arrays.length) { stride = stride || size; size = stride * mem.arrays.reduce((x, y) => x * y, 1); }
    vars.push({ offset: off, name: mem.name, type: normType(mem.type), stride, arrays: mem.arrays, children });
    off += size;
  }
  const size = (off + maxA - 1) & ~(maxA - 1);
  return { size, padding: size - off, vars };
}

function parseStructs(src) {
  const out = {};
  const re = /struct\s+([A-Za-z_]\w*)\s*\{([^}]*)\}/g; let m;
  while ((m = re.exec(src))) {
    const members = [];
    for (const line of m[2].split(";")) {
      const mm = line.match(/^\s*([A-Za-z_]\w*)\s+([A-Za-z_]\w*)\s*((?:\[\d+\])*)\s*(?::\s*([A-Za-z_]\w*\d*)\s*)?$/);
      if (!mm) continue;
      const arrays = [...(mm[3] || "").matchAll(/\[(\d+)\]/g)].map(x => +x[1]);
      members.push({ type: mm[1], name: mm[2], arrays, semantic: mm[4] || null });
    }
    out[m[1]] = { members };
  }
  return out;
}

/* ------------------------------------------------------------------ include resolution */

function dirOf(p) { const i = p.lastIndexOf("/"); return i < 0 ? "" : p.slice(0, i + 1); }
function normPath(p) {
  const parts = [];
  for (const s of p.split("/")) { if (!s || s === ".") continue; if (s === "..") parts.pop(); else parts.push(s); }
  return parts.join("/");
}

/* Recursively resolve #include "…" through the project tree + @builtins.
 * Mirrors the CLI: every resolved include lands in the oiSH with its content CRC32C
 * (with '\r' stripped first). Escaping the tree is not modelled here. */
function resolveIncludes(rootName, project) {
  const includes = [], seen = new Set(); let all = "";
  const visit = (name, text) => {
    all += text.replace(/\r/g, "") + "\n";
    for (const m of text.matchAll(/#\s*include\s+"([^"]+)"/g)) {
      const inc = m[1];
      const path = inc.startsWith("@") ? inc : normPath(dirOf(name) + inc);
      if (seen.has(path)) continue;
      seen.add(path);
      const body = inc.startsWith("@") ? (BUILTINS[path] || "") : (project[path]?.src ?? project[path] ?? "");
      includes.push({ path, crc32c: U.crc32c(String(body).replace(/\r/g, "")) });
      if (body) visit(path, String(body));
    }
  };
  const rootSrc = project[rootName]?.src ?? project[rootName] ?? "";
  visit(rootName, String(rootSrc));
  return { includes, sourceHash: U.crc32c(all) };
}

/* ------------------------------------------------------------------ the analyzer */

const MIN_MODEL = "6.5";                             // OISH_SHADER_MODEL_MIN … 6.10 max
function modelNum(m) { const [a, b] = m.split("."); return (+a) * 100 + (+b); }
function minModelFor(stage, exts, wave) {
  let min = MIN_MODEL;
  const bump = v => { if (modelNum(v) > modelNum(min)) min = v; };
  if (stage === "node") bump("6.8");
  if (wave && (wave.min || wave.max || wave.rec)) bump("6.8"); else if (wave && wave.req) bump("6.6");
  for (const e of exts) {
    if (e === "CoopVec" || e === "CoopMat" || e === "CoopFP8" || e === "CoopVecTraining" || e === "RayTriPosition") bump("6.10");
    if (e === "AtomicI64" || e === "ComputeDeriv" || e === "PAQ" || e === "MeshTaskTexDeriv" || e === "DescriptorHeap") bump("6.6");
  }
  return min;
}

function parseStrings(argStr) { return [...argStr.matchAll(/"([^"]*)"/g)].map(m => m[1]); }
function parseDefines(argStr) {                       // "A", "B" = "4"  ->  [{name,value}]
  const out = [];
  for (const m of argStr.matchAll(/"([^"]+)"(?:\s*=\s*"([^"]*)")?/g)) out.push({ name: m[1], value: m[2] ?? "" });
  return out;
}
function parseUniforms(argStr) {                      // U32 x = 45, F32 y = 90.f
  const out = [];
  for (const part of argStr.split(",")) {
    const m = part.match(/\s*([A-Za-z_]\w*)\s+([A-Za-z_]\w*)\s*=\s*([^,]+)/);
    if (m) out.push({ type: m[1], name: m[2], value: m[3].trim() });
  }
  return out;
}

function sliceBalanced(src, openIdx) {                // src[openIdx] === '(' -> inner text
  let d = 0;
  for (let i = openIdx; i < src.length; i++) {
    if (src[i] === "(") d++;
    else if (src[i] === ")") { d--; if (!d) return { inner: src.slice(openIdx + 1, i), end: i }; }
  }
  return { inner: src.slice(openIdx + 1), end: src.length };
}

function ioFromParam(type, name, semantic, structs) { // -> [{type, semantic, idx}]
  const st = structs[normType(type)];
  if (st) return st.members.filter(m => m.semantic)
    .map(m => ({ type: normType(m.type), ...splitSem(m.semantic) }));
  if (!semantic) return [];
  return [{ type: normType(type), ...splitSem(semantic) }];
}
function splitSem(s) {
  const m = s.match(/^([A-Za-z_]+?)(\d+)?$/);
  return { semantic: (m ? m[1] : s).toUpperCase(), idx: m && m[2] ? +m[2] : 0 };
}
const SYSTEM_IN = new Set(["SV_DISPATCHTHREADID", "SV_GROUPID", "SV_GROUPTHREADID", "SV_GROUPINDEX", "SV_VERTEXID", "SV_INSTANCEID"]);

function analyze(name, project, opts) {
  opts = opts || {};
  const src = String(project[name]?.src ?? project[name] ?? "");
  const structs = parseStructs(src);
  const { includes, sourceHash } = resolveIncludes(name, project);
  const lines = src.split("\n");
  const lineOff = []; { let o = 0; for (const l of lines) { lineOff.push(o); o += l.length + 1; } }

  /* -- entrypoints: accumulate annotations until a function definition is hit -- */
  const freshPend = () => ({ stage: null, lib: false, models: [], extLists: [], defLists: [],
    uniforms: null, vendors: null, binAnno: null, nt: null, wave: null });
  let pend = freshPend();
  const entries = [];

  for (let li = 0; li < lines.length; li++) {
    const ln = lines[li]; let m;
    if ((m = ln.match(/\[\[\s*oxc::stage\("([a-z]+)"\)/)))            { pend.stage = m[1]; pend.lib = false; }
    else if ((m = ln.match(/\[\s*shader\s*\(\s*"([a-z]+)"/)))         { pend.stage = m[1]; pend.lib = true; }
    else if ((m = ln.match(/\[\[\s*oxc::model\("([\d.]+)"\)/)))       pend.models.push(m[1]);
    else if ((m = ln.match(/\[\[\s*oxc::extension\(([^)]*)\)/)))      pend.extLists.push(parseStrings(m[1]));
    else if ((m = ln.match(/\[\[\s*oxc::defines\(([^)]*)\)/)))        pend.defLists.push(parseDefines(m[1]));
    else if ((m = ln.match(/\[\[\s*oxc::uniforms\(([^)]*)\)/)))       pend.uniforms = parseUniforms(m[1]);
    else if ((m = ln.match(/\[\[\s*oxc::vendor\(([^)]*)\)/)))         pend.vendors = parseStrings(m[1]);
    else if ((m = ln.match(/\[\[\s*oxc::binary\(([^)]*)\)/)))         pend.binAnno = parseStrings(m[1]).map(s => s.toLowerCase() === "spirv" ? "spv" : s.toLowerCase());
    else if ((m = ln.match(/\[numthreads\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)/))) pend.nt = [+m[1], +m[2], +m[3]];
    else if ((m = ln.match(/\[WaveSize\(\s*(\d+)(?:\s*,\s*(\d+)\s*,\s*(\d+))?\s*\)/)))
      pend.wave = m[2] ? { req: 0, min: +m[1], max: +m[2], rec: +m[3] } : { req: +m[1], min: 0, max: 0, rec: 0 };
    else if (pend.stage && (m = ln.match(/^\s*([A-Za-z_]\w*(?:x\d(?:x\d)?)?)\s+([A-Za-z_]\w*)\s*\(/))) {
      const retType = m[1], fname = m[2];
      const { inner, end } = sliceBalanced(src, lineOff[li] + ln.indexOf("("));
      const e = { name: fname, stage: pend.stage, lib: pend.lib, declLine: li + 1,
        binaryIds: [], inputs: [], outputs: [], group: null, waveSize: null,
        payloadSize: undefined, intersectionSize: undefined,
        _models: pend.models.slice(), _extLists: pend.extLists.length ? pend.extLists : [[]],
        _defLists: pend.defLists.length ? pend.defLists : [[]],
        _uniforms: pend.uniforms || [], _vendors: pend.vendors, _binAnno: pend.binAnno };

      /* stage-specific reflection (SHEntry union) */
      if (["compute", "node", "mesh", "task"].includes(e.stage)) { e.group = pend.nt || [1, 1, 1]; e.waveSize = pend.wave; }
      const params = inner.split(/,(?![^()]*\))/).map(s => s.trim()).filter(Boolean).map(p => {
        const pm = p.match(/^((?:in|out|inout|_flat|nointerpolation|linear|centroid|sample|precise)\s+)*([A-Za-z_]\w*(?:x\d(?:x\d)?)?)\s+([A-Za-z_]\w*)\s*(?::\s*([A-Za-z_]\w*\d*))?/);
        return pm ? { mods: pm[1] || "", type: pm[2], name: pm[3], semantic: pm[4] || null } : null;
      }).filter(Boolean);

      if (LIB_STAGES.has(e.stage)) {
        if (["closesthit", "anyhit", "miss", "callable"].includes(e.stage)) {
          const pl = params.find(p => structs[normType(p.type)]);
          e.payloadSize = pl ? layoutTight(structs[normType(pl.type)].members, structs).size : 0;
        }
        if (["closesthit", "anyhit", "intersection"].includes(e.stage)) e.intersectionSize = 8; // BuiltInTriangleIntersectionAttributes
      } else if (e.stage !== "compute") {
        for (const p of params) {
          if (/\bout\b/.test(p.mods)) e.outputs.push(...ioFromParam(p.type, p.name, p.semantic, structs));
          else e.inputs.push(...ioFromParam(p.type, p.name, p.semantic, structs).filter(io => !SYSTEM_IN.has(io.semantic)));
        }
        const after = src.slice(end + 1, src.indexOf("{", end + 1));
        const rs = after.match(/:\s*([A-Za-z_]\w*\d*)/);
        if (retType !== "void") e.outputs.push(...ioFromParam(retType, "", rs ? rs[1] : (e.stage === "pixel" ? "SV_Target" : null), structs));
      }
      entries.push(e);
      pend = freshPend();
    }
  }

  /* -- registers (declaration order drives the mock SPIR-V auto-binding) -- */
  const registers = [];
  const declRe = /(?:^|\n)[ \t]*((?:PUSH_CONSTANT|UNKNOWN_FORMAT)[ \t]+)?(RW)?(ConstantBuffer|ByteAddressBuffer|StructuredBuffer|AppendStructuredBuffer|ConsumeStructuredBuffer|Buffer|Texture1D|Texture2D|Texture3D|TextureCube|Texture2DMS|Texture1DArray|Texture2DArray|TextureCubeArray|SamplerState|SamplerComparisonState|RaytracingAccelerationStructure)(?:<([^>]+)>)?[ \t]+([A-Za-z_]\w*)[ \t]*((?:\[\d+\])*)[ \t]*(?::[ \t]*register\(([buts])(\d+)(?:,[ \t]*space(\d+))?\))?/g;
  const cbRe = /cbuffer\s+([A-Za-z_]\w*)\s*(?::\s*register\(b(\d+)(?:,\s*space(\d+))?\))?\s*\{([^}]*)\}/g;
  const coopRe = /OXC_COOPVEC_(RW)?(MATRIX|VECTOR)_BUFFER(?:_I32)?\(\s*([A-Za-z_]\w*)\s*\)\s*:\s*register\(([buts])(\d+)(?:,\s*space(\d+))?\)/g;
  const pushStructRe = /PUSH_CONSTANT\s+([A-Za-z_]\w*)\s+([A-Za-z_]\w*)\s*;/g;
  const bodyText = src;
  const usedCount = n => (bodyText.match(new RegExp("\\b" + n + "\\b", "g")) || []).length;
  const lineOfIndex = idx => src.slice(0, idx).split("\n").length;
  const declLineOf = dm => lineOfIndex(dm.index + (dm[0].startsWith("\n") ? 1 : 0));
  const auto = { b: 0, t: 0, u: 0, s: 0 };

  let dm;
  while ((dm = cbRe.exec(src))) {
    const members = [];
    for (const l of dm[4].split(";")) {
      const mm = l.match(/^\s*([A-Za-z_]\w*(?:x\d(?:x\d)?)?)\s+([A-Za-z_]\w*)\s*((?:\[\d+\])*)\s*$/);
      if (mm) members.push({ type: mm[1], name: mm[2], arrays: [...(mm[3] || "").matchAll(/\[(\d+)\]/g)].map(x => +x[1]) });
    }
    const lay = layoutCbuffer(members, structs);
    registers.push(mkReg({ name: dm[1], typeStr: "ConstantBuffer", cls: "CBV", letter: "b",
      num: dm[2] != null ? +dm[2] : auto.b++, space: +(dm[3] || 0), declLine: lineOfIndex(dm.index),
      used: members.some(mem => usedCount(mem.name) > 1),
      buffer: { size: lay.size, padding: lay.padding, packed: false, vars: markUsed(lay.vars, usedCount) } }));
  }
  while ((dm = pushStructRe.exec(src))) {
    const st = structs[dm[1]];
    const lay = st ? layoutCbuffer(st.members, structs) : { size: 0, padding: 0, vars: [] };
    registers.push(mkReg({ name: dm[2], typeStr: "PushConstants", cls: "CBV", letter: "b",
      num: auto.b++, space: 0, declLine: lineOfIndex(dm.index), push: true,
      used: usedCount(dm[2]) > 1,
      buffer: { size: lay.size, padding: lay.padding, packed: false, vars: markUsed(lay.vars, n => usedCount(dm[2] + "." + n) + usedCount(n) - 1) } }));
  }
  while ((dm = coopRe.exec(src))) {
    registers.push(mkReg({ name: dm[3], typeStr: (dm[1] ? "RW" : "RW") + "StructuredBuffer", cls: "UAV", letter: dm[4],
      num: +dm[5], space: +(dm[6] || 0), declLine: declLineOf(dm), write: true,
      used: usedCount(dm[3]) > 1, buffer: { size: 8, padding: 0, packed: true,
        vars: [{ offset: 0, name: "v", type: "F16x4", stride: null, arrays: [], children: null, used: { spirv: true, dxil: true } }] } }));
  }
  while ((dm = declRe.exec(src))) {
    const [, prefix, rw, kind, tmpl, rname, arr, letter, num, space] = dm;
    const arrays = [...(arr || "").matchAll(/\[(\d+)\]/g)].map(x => +x[1]);
    const isTex = /^Texture/.test(kind), isSmp = /^Sampler/.test(kind);
    const cls = isSmp ? "SMP" : kind === "RaytracingAccelerationStructure" ? "SRV"
      : rw || /^(Append|Consume)/.test(kind) ? "UAV" : (kind === "ConstantBuffer" ? "CBV" : "SRV");
    const eLetter = letter || (cls === "CBV" ? "b" : cls === "SMP" ? "s" : cls === "UAV" ? "u" : "t");
    let buffer = null, texture = null, typeStr = (rw || "") + kind;
    if (kind === "StructuredBuffer" && tmpl && structs[normType(tmpl)]) {
      const lay = layoutTight(structs[normType(tmpl)].members, structs);
      buffer = { size: lay.size, padding: lay.padding, packed: true, vars: markUsed(lay.vars, usedCount), elem: normType(tmpl) };
    }
    if (/^(Append|Consume)/.test(kind)) typeStr = "Append/ConsumeBuffer";
    if (isTex) {
      const prim = tmpl ? esbPrimName(tmpl) : "float4";
      texture = { primitive: prim, format: rw ? (prefix && /UNKNOWN_FORMAT/.test(prefix) ? null : fmtIdFor(tmpl)) : null };
      typeStr = (rw || "") + kind;
    }
    registers.push(mkReg({ name: rname, typeStr, cls, letter: eLetter,
      num: num != null ? +num : auto[eLetter]++, space: +(space || 0), declLine: declLineOf(dm),
      write: !!rw || /^(Append|Consume)/.test(kind), arrays, texture, buffer,
      used: usedCount(rname) > 1 }));
  }
  registers.sort((a, b) => a.declLine - b.declLine);
  registers.forEach((r, i) => {                       // SPIR-V side: auto binding by declaration order per set
    r.bindings.spirv = r.push ? null : { set: r.bindings.dxil.space, binding: i };
  });

  /* -- binaries: identifier = entrypoint(lib) × model × extensions × defines (+ uniforms) -- */
  const binaries = [];
  const libGroups = new Map();
  for (const e of entries) {
    const extLists = e._extLists, defLists = e._defLists;
    for (const exts of extLists) for (const defs of defLists) {
      const models = e._models.length ? e._models : [opts.forceModel || minModelFor(e.stage, exts, e.waveSize)];
      for (const model of models) {
        if (e.lib) {
          const key = [model, exts.join("+"), defs.map(d => d.name + "=" + d.value).join("+"),
            (e._uniforms || []).map(u => u.type + " " + u.name).join("+")].join("|");
          if (!libGroups.has(key)) {
            libGroups.set(key, binaries.length);
            binaries.push(mkBin(null, "lib", true, model, exts, defs, e._uniforms, e._vendors, e._binAnno, registers, src));
          }
          const bi = libGroups.get(key);
          binaries[bi].entryNames.push(e.name);
          mergeVendors(binaries[bi], e._vendors);
          e.binaryIds.push(bi);
        } else {
          e.binaryIds.push(binaries.length);
          binaries.push(mkBin(e.name, e.stage, false, model, exts, defs, e._uniforms, e._vendors, e._binAnno, registers, src));
        }
      }
    }
  }
  for (const e of entries) for (const k of ["_models", "_extLists", "_defLists", "_uniforms", "_vendors", "_binAnno"]) delete e[k];

  const reqModel = binaries.length ? binaries.map(b => b.model).sort((a, b) => modelNum(b) - modelNum(a))[0] : MIN_MODEL;

  return {
    name: opts.rename || name, sourceName: name,
    compilerVersion: { ...VERSION },
    sourceHash, flags: { reflectionOnly: !!opts.reflectionOnly },
    header: { version: "1.2", sizeTypes: { spirv: "U16", dxil: "U32" } },   // SHHeader.sizeTypes (mocked)
    entries, binaries, includes, registers,
    model: reqModel, mockParsed: !!opts.mockParsed
  };
}

function markUsed(vars, usedCount) {
  for (const v of vars) {
    const u = usedCount(v.name) > 1;
    v.used = { spirv: u, dxil: u };
    if (v.children) markUsed(v.children, usedCount);
  }
  return vars;
}
function esbPrimName(tmpl) {
  const i = esbInfo(tmpl); if (!i) return "float4";
  const t = normType(tmpl)[0];
  const base = t === "U" ? "uint" : t === "I" ? "int" : normType(tmpl).startsWith("F64") ? "double" : "float";
  return base + (i.comps > 1 ? i.comps : "");
}
function fmtIdFor(tmpl) {
  const n = normType(tmpl || "F32x4");
  return { F32x4: "RGBA32f", F32x2: "RG32f", F32: "R32f", U32: "R32u", U32x4: "RGBA32u", I32: "R32i", F16x4: "RGBA16f" }[n] || "RGBA32f";
}
function mkReg(o) {
  return { name: o.name, arrays: o.arrays || [], typeStr: o.typeStr, cls: o.cls,
    flags: { write: !!o.write, array: (o.arrays || []).length > 0, combined: false },
    bindings: { spirv: null /* filled after sort */, dxil: { letter: o.letter, binding: o.num, space: o.space } },
    used: { spirv: o.push ? !!o.used : !!o.used, dxil: !!o.used },
    texture: o.texture || null, buffer: o.buffer || null, push: !!o.push, declLine: o.declLine };
}
function mergeVendors(bin, vendors) {
  if (!vendors) return;                               // one entry without the annotation keeps the group at "all"
  bin.vendors = bin.vendors ? [...new Set([...bin.vendors, ...vendors])] : vendors.slice();
}
function backendsFor(stage, exts, binAnno) {
  let out = ["spv", "dxil"];
  if (stage === "node") out = ["dxil"];
  for (const e of exts) {
    if (EXT_SPV_ONLY.has(e)) out = out.filter(b => b === "spv");
    if (EXT_DXIL_ONLY.has(e)) out = out.filter(b => b === "dxil");
  }
  if (binAnno) out = out.filter(b => binAnno.includes(b));
  return out;
}
function mkBin(entrypoint, stage, lib, model, exts, defs, uniforms, vendors, binAnno, registers, src) {
  /* dormant = declared but not detected in the final executable (ESHExtension dormantExt):
   * mocked as "no obvious usage token in the source". */
  const usage = { RayQuery: /RayQuery|TraceRayInline/, "16BitTypes": /\b(F16|float16_t|U16|I16)\b/,
    CoopVec: /OXC_COOPVEC/, CoopMat: /OXC_COOPMAT/, F64: /\b(F64|double)\b/, I64: /\b(I64|int64_t|uint64_t)\b/,
    DescriptorHeap: /ResourceDescriptorHeap|SamplerDescriptorHeap/ };
  const dormant = exts.filter(e => usage[e] && !usage[e].test(src.replace(/\[\[[^\]]*\]\]/g, "")));
  const supported = backendsFor(stage, exts, binAnno);
  const identKey = [entrypoint || "(lib)", stage, model, exts.join("+"), defs.map(d => d.name + "=" + d.value).join("+")].join("|");
  const sizes = {};
  for (const b of ["spv", "dxil"]) {
    if (!supported.includes(b)) { sizes[b] = 0; continue; }
    const seed = U.crc32c(identKey + "|" + b + "|" + src);
    sizes[b] = (b === "spv" ? 900 : 1500) + registers.length * 120 + (seed % 700);
  }
  return { entrypoint, stage, lib, entryNames: entrypoint ? [entrypoint] : [], model,
    extensions: exts.slice(), dormant, defines: defs, uniforms: uniforms || [],
    vendors: vendors ? vendors.slice() : null, binAnno: binAnno || null, supported,
    sizes: { spirv: sizes.spv, dxil: sizes.dxil }, registers, identKey };
}

/* ------------------------------------------------------------------ mock disassembly */
/* Deterministic on (identifier + backend + source text) so equal content diffs clean and
 * changed content shows a plausible, stable delta. */

const SPV_CAP = { RayQuery: "RayQueryKHR", "16BitTypes": "Float16", F64: "Float64", I64: "Int64",
  CoopVec: "CooperativeVectorNV", CoopMat: "CooperativeMatrixKHR", Multiview: "MultiView",
  AtomicF32: "AtomicFloat32AddEXT", AtomicF64: "AtomicFloat64AddEXT", ComputeDeriv: "ComputeDerivativeGroupQuadsNV",
  DescriptorHeap: "DescriptorHeapEXT" };
const SPV_EXT = { RayQuery: "SPV_KHR_ray_query", CoopVec: "SPV_NV_cooperative_vector", CoopMat: "SPV_KHR_cooperative_matrix",
  Multiview: "SPV_KHR_multiview", ComputeDeriv: "SPV_NV_compute_shader_derivatives", RayMotionBlur: "SPV_NV_ray_tracing_motion_blur",
  DescriptorHeap: "SPV_EXT_descriptor_heap" };

function spvEnvFor(bin, entries) {
  if (bin.extensions.some(e => /^Coop/.test(e))) return { env: "vulkan1.3", ver: "1.6" };
  const meshy = entries.some(e => bin.entryNames.includes(e.name) && (e.stage === "mesh" || e.stage === "task"));
  if (bin.lib || meshy) return { env: "vulkan1.1spirv1.4", ver: "1.4" };
  return { env: "vulkan1.1", ver: "1.3" };
}

function disasm(doc, bin, backend) {
  const seed = U.crc32c(bin.identKey + "|" + backend + "|" + doc.sourceHash);
  const rng = U.mulberry32(seed);
  const ents = doc.entries.filter(e => bin.lib ? bin.entryNames.includes(e.name) : e.name === bin.entrypoint);
  const L = [];
  if (backend === "spirv") {
    const { env, ver } = spvEnvFor(bin, doc.entries);
    L.push("; SPIR-V", `; Version: ${ver}`, `; Generator: OxC3 shader compiler ${VERSION.major}.${VERSION.minor}.${VERSION.patch} (mock)`,
      `; Bound: ${40 + (seed % 60)}`, "; Schema: 0", `; Target env: ${env}`);
    L.push("               OpCapability Shader");
    for (const e of bin.extensions) if (SPV_CAP[e] && !bin.dormant.includes(e)) L.push(`               OpCapability ${SPV_CAP[e]}`);
    for (const e of bin.extensions) if (SPV_EXT[e] && !bin.dormant.includes(e)) L.push(`               OpExtension "${SPV_EXT[e]}"`);
    L.push('               OpMemoryModel Logical GLSL450');
    for (const e of ents) {
      const em = { vertex: "Vertex", pixel: "Fragment", compute: "GLCompute", mesh: "MeshEXT", task: "TaskEXT",
        raygeneration: "RayGenerationKHR", closesthit: "ClosestHitKHR", anyhit: "AnyHitKHR", miss: "MissKHR",
        intersection: "IntersectionKHR", callable: "CallableKHR" }[e.stage] || "GLCompute";
      L.push(`               OpEntryPoint ${em} %${e.name} "${e.name}"`);
      if (e.group) L.push(`               OpExecutionMode %${e.name} LocalSize ${e.group.join(" ")}`);
      if (e.waveSize?.req) L.push(`               OpExecutionMode %${e.name} SubgroupSize ${e.waveSize.req}`);
    }
    for (const u of bin.uniforms) L.push(`               OpDecorate %specConst_${u.name} SpecId ${bin.uniforms.indexOf(u)}`);
    for (const r of bin.registers) {
      if (!r.bindings.spirv) { L.push(`               ; %${r.name} = push_constant block (${r.buffer ? r.buffer.size + " B" : ""})`); continue; }
      L.push(`               OpDecorate %${r.name} DescriptorSet ${r.bindings.spirv.set}`);
      L.push(`               OpDecorate %${r.name} Binding ${r.bindings.spirv.binding}`);
    }
    for (const d of bin.defines) L.push(`               ; define $${d.name}${d.value ? " = " + d.value : ""}`);
    L.push("       %void = OpTypeVoid", "          %3 = OpTypeFunction %void");
    for (const u of bin.uniforms) L.push(`%specConst_${u.name} = OpSpecConstant %uint ${u.value}`);
    for (const e of ents) {
      L.push(`      %${e.name} = OpFunction %void None %3`, "          %e = OpLabel");
      const n = 4 + (Math.floor(rng() * 5));
      for (let i = 0; i < n; i++) {
        const ops = ["OpLoad %v4float", "OpImageRead %v4float", "OpFMul %v4float", "OpFAdd %v4float",
          "OpAccessChain %ptr", "OpImageWrite", "OpIAdd %uint", "OpBitcast %uint"];
        L.push(`         %${10 + i} = ${ops[Math.floor(rng() * ops.length)]} %${Math.floor(rng() * 30) + 4}`);
      }
      L.push("               OpReturn", "               OpFunctionEnd");
    }
  } else {
    const modelTag = (bin.model || "6.x").replace(".", "_");              // a standalone binary carries no identifier
    const prof = bin.lib ? `lib_${modelTag}` : `${STAGE_PROFILE[bin.stage] || "cs"}_${modelTag}`;
    L.push(`; DXIL — mock of Compiler_disassemble (${prof})`,
      'target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"',
      'target triple = "dxil-ms-dx"', "");
    L.push("%dx.types.Handle = type { i8* }");
    for (const r of bin.registers)
      L.push(`; resource ${r.name}: ${r.typeStr} register(${r.bindings.dxil.letter}${r.bindings.dxil.binding}, space${r.bindings.dxil.space})${r.used.dxil ? "" : "  ; unused"}`);
    for (const u of bin.uniforms) L.push(`@${u.name} = external constant ${u.type.toLowerCase().replace(/\d+/, m => "i" + m)} ; uniform export = ${u.value}`);
    for (const d of bin.defines) L.push(`; define $${d.name}${d.value ? " = " + d.value : ""}`);
    L.push("");
    for (const e of ents) {
      L.push(`define void @${bin.lib ? "\\01?" + e.name + "@@YAXXZ" : e.name}() {`);
      const n = 4 + (Math.floor(rng() * 5));
      for (let i = 0; i < n; i++) {
        const ops = ["call %dx.types.Handle @dx.op.createHandleFromBinding", "call float @dx.op.dot4.f32",
          "call %dx.types.ResRet.f32 @dx.op.textureLoad.f32", "fmul fast float", "fadd fast float",
          "call void @dx.op.textureStore.f32", "add i32", "call i32 @dx.op.flattenedThreadIdInGroup.i32"];
        L.push(`  %${i + 1} = ${ops[Math.floor(rng() * ops.length)]}(i32 ${Math.floor(rng() * 200)})`);
      }
      L.push("  ret void", "}");
    }
    L.push("", `!dx.valver = !{!${1 + (seed % 3)}}`, `!dx.entryPoints = !{${ents.map((e, i) => "!" + (10 + i)).join(", ")}}`);
  }
  return L.join("\n");
}

/* ------------------------------------------------------------------ mock bytes */

const SPV_MAGIC = [0x03, 0x02, 0x23, 0x07], DXBC_MAGIC = [0x44, 0x58, 0x42, 0x43], OISH_MAGIC = [0x6F, 0x69, 0x53, 0x48];
function mockBytes(magic, size, seed) {
  const a = new Uint8Array(Math.max(size, 32));
  magic.forEach((b, i) => a[i] = b);
  const rng = U.mulberry32(seed >>> 0);
  for (let i = magic.length; i < a.length; i++) a[i] = Math.floor(rng() * 256);
  return a;
}
const oishBytes = doc => mockBytes(OISH_MAGIC, 256 + doc.binaries.length * 64, doc.sourceHash);
const binBytes = (bin, backend) => mockBytes(backend === "spirv" ? SPV_MAGIC : DXBC_MAGIC,
  backend === "spirv" ? bin.sizes.spirv : bin.sizes.dxil, U.crc32c(bin.identKey + backend));

/* ------------------------------------------------------------------ compile (mock of Compiler_compileShaders) */

function compileFile(name, project, opts) {
  const doc = analyze(name, project, { reflectionOnly: opts.reflectionOnly });
  const diags = (project[name]?.diags || []).map(d => ({ ...d, file: name }));

  if (opts.warnUnusedRegisters)
    for (const r of doc.registers) if (!r.used.dxil && !r.used.spirv)
      diags.push({ sev: "warn", line: r.declLine, ch0: 0, ch1: 1, file: name,
        msg: `register '${r.name}' is never used (--warn-unused-registers)`, code: "W_unused_register" });
  if (opts.warnUnusedConstants)
    for (const r of doc.registers) if (r.buffer && !r.buffer.packed)
      for (const v of r.buffer.vars) if (!v.used.dxil && !v.used.spirv)
        diags.push({ sev: "warn", line: r.declLine, ch0: 0, ch1: 1, file: name,
          msg: `constant '${r.name}.${v.name}' is never used (--warn-unused-constants)`, code: "W_unused_constant" });
  if (opts.warnBufferPadding)
    for (const r of doc.registers) if (r.buffer && r.buffer.padding > 0)
      diags.push({ sev: "warn", line: r.declLine, ch0: 0, ch1: 1, file: name,
        msg: `'${r.name}' layout introduces ${r.buffer.padding} byte(s) of padding (--warn-buffer-padding)`, code: "W_buffer_padding" });
  if (!doc.entries.length && !opts.ignoreEmptyFiles)
    diags.push({ sev: "error", line: 1, ch0: 0, ch1: 1, file: name,
      msg: "no entrypoints found — name include-only files .hlsli, or pass --ignore-empty-files", code: "E_no_entrypoints" });

  return { doc, diags };
}

/* ------------------------------------------------------------------ preloaded oiSH docs + uploads */

function seedOishDocs() {
  const p1 = { "lighting.hlsl": SAMPLE_FILES["lighting.hlsl"], "include/light.hlsli": SAMPLE_FILES["include/light.hlsli"] };
  const v2src = SAMPLE_FILES["lighting.hlsl"].src
    .replace('RWTexture2D<F32x4>      _output : register(u0);',
             'RWTexture2D<F32x4>      _output : register(u0);\nRWStructuredBuffer<U32>  _histogram : register(u1);')
    .replace('[shader("compute")]', '[[oxc::model("6.8")]]\n[shader("compute")]')
    .replace('_output[id.xy] = c;', '_output[id.xy] = c;\n    // InterlockedAdd(_histogram[luma(c)], 1);');
  const l2 = SAMPLE_FILES["include/light.hlsli"].src.replace("(1.0 + dot(d, d))", "max(dot(d, d), 1e-4)");
  const p2 = { "lighting.hlsl": { src: v2src, diags: [] }, "include/light.hlsli": { src: l2, diags: [] } };
  return {
    "lighting.v1.oiSH": analyze("lighting.hlsl", p1, { rename: "lighting.v1.oiSH", forceModel: "6.7" }),
    "lighting.v2.oiSH": analyze("lighting.hlsl", p2, { rename: "lighting.v2.oiSH" })
  };
}

/* Mock parse of an uploaded .oiSH: derive one of the two seeded shapes from the byte CRC so
 * two different uploads still diff interestingly. Real path: SHFile_read (see js/api.js). */
function parseOiSHBytes(name, bytes) {
  const seeds = seedOishDocs();
  const pick = U.crc32c(bytes) & 1 ? "lighting.v2.oiSH" : "lighting.v1.oiSH";
  const doc = seeds[pick];
  doc.name = name; doc.mockParsed = true; doc.byteLength = bytes.length;
  return doc;
}

function sampleStandaloneBins() {
  const post = analyze("post.hlsl", SAMPLE_FILES);
  const light = analyze("lighting.hlsl", SAMPLE_FILES);
  const psBin = post.binaries.find(b => b.stage === "pixel") || post.binaries[0];
  const csBin = light.binaries[0];
  return {
    "post.ps.spv":     { type: "spirv", bytes: binBytes(psBin, "spirv"), text: disasm(post, psBin, "spirv"),
      origin: { file: "post.hlsl", entry: psBin.entrypoint } },
    "lighting.cs.dxil": { type: "dxil", bytes: binBytes(csBin, "dxil"), text: disasm(light, csBin, "dxil"),
      origin: { file: "lighting.hlsl", entry: "main" } }
  };
}

/* ------------------------------------------------------------------ raw DXC + standalone binaries */

/* The argv the Command tab prints, parsed back: -T profile, -E entry, -spirv, the input file. */
function parseDxcArgv(argv) {
  const toks = (argv.match(/"[^"]*"|\S+/g) || []).map(t => t.replace(/^"|"$/g, ""));
  const out = { profile: null, entry: null, spirv: false, input: null, defines: [] };
  for (let i = 0; i < toks.length; i++) {
    const t = toks[i];
    if (t === "-T") out.profile = toks[++i];
    else if (t === "-E") out.entry = toks[++i];
    else if (t === "-spirv") out.spirv = true;
    else if (t.startsWith("-D")) out.defines.push(t.slice(2));
    else if (/\.hlsli?$/i.test(t) && !t.startsWith("-")) out.input = t;
  }
  const pm = out.profile && out.profile.match(/^([a-z]+)_(\d)_(\d+)$/);
  out.stageProfile = pm ? pm[1] : null;
  out.model = pm ? `${pm[2]}.${pm[3]}` : null;
  out.lib = out.stageProfile === "lib";
  return out;
}

/* Mock of running DXC with an arbitrary flag line: the same analysis, one binary out, no oiSH. */
function compileRaw(argv, name, project) {
  const a = parseDxcArgv(argv);
  const src = a.input && project[a.input] ? a.input : name;
  const type = a.spirv ? "spirv" : "dxil";
  const diags = (project[src]?.diags || []).map(d => ({ ...d, file: src }));
  const log = [`dxc ${argv}`];
  if (!a.profile) { log.push("error: no target profile (-T) given"); return { ok: false, diags, log: log.join("\n") }; }
  if (diags.some(d => d.sev === "error")) {
    for (const d of diags) log.push(`${src}:${d.line}:${d.ch0 + 1}: ${d.sev === "error" ? "error" : "warning"}: ${d.msg}`);
    return { ok: false, diags, log: log.join("\n") };
  }
  const doc = analyze(src, project, {});
  const entry = a.entry ? doc.entries.find(e => e.name === a.entry) : null;
  if (a.entry && !entry && !a.lib) { log.push(`error: entrypoint '${a.entry}' not found in ${src}`); return { ok: false, diags, log: log.join("\n") }; }
  const bin = entry ? doc.binaries[entry.binaryIds[0]] : (doc.binaries.find(b => b.lib) || doc.binaries[0]);
  if (!bin) { log.push(`error: ${src} declares no entrypoint`); return { ok: false, diags, log: log.join("\n") }; }
  /* the raw compile has no identifier beyond what the flags said, and its content depends on the exact flags */
  const raw = { ...bin, model: a.model || bin.model, identKey: bin.identKey + "|raw|" + U.crc32c(argv) };
  raw.sizes = { spirv: type === "spirv" ? bin.sizes.spirv + (U.crc32c(argv) % 97) : 0, dxil: type === "dxil" ? bin.sizes.dxil + (U.crc32c(argv) % 97) : 0 };
  const bytes = binBytes(raw, type);
  const stage = entry ? entry.stage : (doc.entries.find(e => bin.entryNames.includes(e.name)) || {}).stage;
  for (const d of diags) log.push(`${src}:${d.line}:${d.ch0 + 1}: warning: ${d.msg}`);
  log.push(`${type === "spirv" ? "SPIR-V" : "DXIL"}: ${bytes.length} bytes, ${a.profile}${entry ? " -E " + entry.name : ""} (mock: fabricated from the same analysis the compile path uses)`);
  const outName = `${src.replace(/\.hlsl$/i, "")}.${entry ? entry.name : "lib"}.${a.model ? "sm" + a.model.replace(".", "") + "." : ""}${type === "spirv" ? "spv" : "dxil"}`;
  return { ok: true, type, bytes, text: disasm(doc, raw, type), diags, log: log.join("\n"), name: outName,
    origin: { file: src, entry: entry ? entry.name : bin.entryNames[0], model: a.model, stage } };
}

/* A standalone binary as a one-binary SHDocument: the backend reflection (registers, IO, group size) with no
 * identifier, since nothing declared a model, extensions or defines. `origin` is the mock's way of knowing which sample
 * entry the bytes came from; the real path reflects the bytes themselves. */
function reflectBinary(name, type, bytes, origin) {
  if (!origin) {
    const pick = U.crc32c(bytes) & 1;
    origin = pick ? { file: "post.hlsl", entry: "psMain" } : { file: "lighting.hlsl", entry: "main" };
  }
  const doc = analyze(origin.file, SAMPLE_FILES, {});
  const e = doc.entries.find(x => x.name === origin.entry) || doc.entries[0];
  const from = doc.binaries[e.binaryIds[0]];
  const bin = { entrypoint: e.name, stage: e.stage, lib: false, entryNames: [e.name], model: origin.model || null,
    extensions: [], dormant: [], defines: [], uniforms: [], vendors: null, binAnno: null,
    supported: [type === "spirv" ? "spv" : "dxil"],
    sizes: { spirv: type === "spirv" ? bytes.length : 0, dxil: type === "dxil" ? bytes.length : 0 },
    registers: from.registers, identKey: "standalone|" + U.crc32c(bytes) };
  return { name, sourceName: name, standalone: true, compilerVersion: { ...VERSION }, sourceHash: U.crc32c(bytes),
    flags: { reflectionOnly: false }, header: { version: null, sizeTypes: { spirv: "U32", dxil: "U32" } },
    entries: [{ ...e, lib: false, binaryIds: [0] }], binaries: [bin], includes: [], registers: from.registers, model: null };
}

/* The planned `shader assemble -> oiSH`: a standalone binary wrapped into an oiSH with the identifier the source would
 * have declared, so a pipeline can be created from it. */
function assembleOiSH(doc, ident) {
  const out = JSON.parse(JSON.stringify(doc));
  const b = out.binaries[0], e = out.entries[0];
  const lib = LIB_STAGES.has(ident.stage);
  out.standalone = false; out.assembledFrom = doc.name;
  out.name = ident.name || doc.name.replace(/\.(spv|dxil)$/i, "") + ".oiSH";
  out.header = { version: "1.2", sizeTypes: { spirv: "U32", dxil: "U32" } };
  e.name = ident.entry || e.name; e.stage = ident.stage || e.stage; e.lib = lib;
  b.entrypoint = lib ? null : e.name; b.stage = lib ? "lib" : e.stage; b.lib = lib; b.entryNames = [e.name];
  b.model = ident.model; b.extensions = (ident.extensions || []).slice(); b.dormant = [];
  b.vendors = ident.vendors && ident.vendors.length ? ident.vendors.slice() : null;
  b.identKey = [e.name, b.stage, b.model, b.extensions.join("+"), ""].join("|");
  out.model = ident.model;
  return out;
}

window.OxMock = { VERSION, STAGES, LIB_STAGES, STAGE_PROFILE, EXTENSIONS, VENDORS, BUILTINS,
  SAMPLE_FILES, analyze, compileFile, seedOishDocs, parseOiSHBytes, disasm, oishBytes, binBytes,
  sampleStandaloneBins, spvEnvFor, backendsFor, parseStructs, normTypeExport: normType,
  parseDxcArgv, compileRaw, reflectBinary, assembleOiSH };
})();
