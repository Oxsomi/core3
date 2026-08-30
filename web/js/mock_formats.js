/* mock_formats.js — the mock for everything past oiSH: oiSR (frontend symbol AST),
 * oiSP (pipelines with provenance) and the ISA paths (offline amdllpc / live driver).
 *
 * Same rules as mock.js: js/api.js is the only consumer, everything derived is fabricated
 * deterministically, and both mock files are deleted wholesale when the wasm port lands.
 * Constants (node kinds, feature bits, the oiSP field table with its reasons and legal
 * domains, the offline ISA target list, the CLI's wording) come from the repo:
 * docs/oiSR.md, src/formats/oiSP/sp_file.c (SPField_info), shader_compiler/spirv_isa.h. */
(function () {
"use strict";
const U = window.OxUtil;
const M = () => window.OxMock;

/* ================================================================ oiSR (shader reflect-symbols) */

/* ESRNodeType, in on-disk order (docs/oiSR.md). */
const SR_KINDS = ["Register", "Function", "Enum", "EnumValue", "Namespace", "Variable", "Typedef", "Struct", "Union",
  "Interface", "Parameter", "StaticVariable", "GroupsharedVariable", "Using", "Scope", "IfRoot", "IfFirst", "ElseIf",
  "Else", "For", "While", "Do", "Switch", "Case", "Default"];
/* ESRFeature tiers a file carries. */
const SR_FEATURES = ["Basics", "Functions", "Namespaces", "UserTypes", "Scopes", "SymbolInfo"];

/* Symbol counts the real built-in includes reflect to; `shader reflect-symbols` collapses them into one
 * summary line per file (the four measured ones are real, the rest follow the same rule as any mock size). */
const BUILTIN_SYMBOLS = { "@types.hlsli": 141, "@mat.hlsli": 90, "@fixed_point.hlsli": 27, "@indirect.hlsli": 17 };
const builtinSymbolCount = name => BUILTIN_SYMBOLS[name] || (8 + U.crc32c(name) % 40);

/* D3D12 resource dimension / return-type codes the Register record carries (D3D_SRV_DIMENSION, D3D_RESOURCE_RETURN_TYPE). */
const DIM = { Texture1D: 2, Texture1DArray: 3, Texture2D: 4, Texture2DArray: 5, Texture2DMS: 6, Texture3D: 8, TextureCube: 9, TextureCubeArray: 10 };
const RET = { float: 5, uint: 4, int: 3, double: 7 };

const HLSL_KEYWORDS = new Set(["return", "if", "else", "for", "while", "do", "switch", "case", "break", "continue", "struct", "cbuffer", "static", "const", "groupshared", "void", "typedef", "namespace", "using", "in", "out", "inout"]);

/* A type record: the frontend type (`uint`) plus the alias the source wrote (`U32`), class, rows/cols.
 * `Variable corner (U32)` in the plain print, `(uint : Scalar 1x1 elems=0)` in --verbose. */
function typeOf(src, structs) {
  const alias = src;
  const n = M().normTypeExport ? M().normTypeExport(src) : src;
  const m = n.match(/^([FIUB])(8|16|32|64|ool)?(?:x([2-4]))?(?:x([2-4]))?$/);
  if (structs[n] || structs[src]) return { name: src, display: src, cls: "Struct", rows: 0, cols: 0 };
  if (!m) return { name: src, display: src, cls: "Object", rows: 0, cols: 0 };
  const base = m[1] === "F" ? (m[2] === "64" ? "double" : m[2] === "16" ? "float16_t" : "float")
    : m[1] === "U" ? (m[2] === "16" ? "uint16_t" : m[2] === "64" ? "uint64_t" : "uint")
    : m[1] === "I" ? (m[2] === "16" ? "int16_t" : m[2] === "64" ? "int64_t" : "int") : "bool";
  const cols = m[3] ? +m[3] : 1, rows = m[4] ? +m[4] : 1;
  const hlsl = base + (rows > 1 ? `${rows}x${cols}` : cols > 1 ? cols : "");
  return { name: hlsl, display: alias === hlsl ? hlsl : alias, cls: rows > 1 ? "Matrix" : cols > 1 ? "Vector" : "Scalar", rows, cols };
}

function findLine(lines, re, from) {
  for (let i = from || 0; i < lines.length; i++) { const m = lines[i].match(re); if (m) return { i, m }; }
  return null;
}

/* Build an SRDocument from a source file, the way Compiler_reflect walks IHLSLReflectionData into SRFile:
 * one anonymous root Namespace, Register nodes (cbuffers carry their Variables, structured buffers a $Element),
 * Struct nodes with member Variables, Function nodes with Parameters (+ the return slot) and local Variables.
 * Every node has a source location (ESRFeature_SymbolInfo) so the tree can drive go-to-definition. */
function reflectSymbols(name, project) {
  const src = String(project[name]?.src ?? project[name] ?? "");
  const lines = src.split("\n");
  const structs = M().parseStructs(src);
  const nodes = [];
  const add = (kind, parent, props) => {
    const n = { id: nodes.length, kind, parent, children: [], annotations: [], loc: null, ...props };
    nodes.push(n);
    if (parent >= 0) nodes[parent].children.push(n.id);
    return n;
  };
  const loc = (line, col, len, span) => ({ file: name, line: line + 1, col: (col || 0) + 1, len: len || 1, lines: span || 1 });
  const root = add("Namespace", -1, { name: "(anonymous)" });

  /* structs first, in source order */
  for (const [sname, st] of Object.entries(structs)) {
    const at = findLine(lines, new RegExp("^\\s*struct\\s+" + sname + "\\b"));
    const s = add("Struct", root.id, { name: sname, loc: at ? loc(at.i, lines[at.i].search(/\S/), lines[at.i].trim().length) : null });
    let li = at ? at.i + 1 : 0;
    for (const mem of st.members) {
      const ml = findLine(lines, new RegExp("^\\s*" + mem.type.replace(/[.*+?^${}()|[\]\\]/g, "\\$&") + "\\s+" + mem.name + "\\b"), li);
      add("Variable", s.id, { name: mem.name, type: typeOf(mem.type, structs), semantic: mem.semantic ? mem.semantic.toUpperCase() : null,
        arrays: mem.arrays, loc: ml ? loc(ml.i, lines[ml.i].search(/\S/), lines[ml.i].trim().length) : null });
      if (ml) li = ml.i + 1;
    }
  }

  /* registers, reusing the oiSH analyzer's declaration scan (declLine + class + type) */
  const doc = M().analyze(name, project, {});
  for (const r of doc.registers) {
    const l = r.declLine - 1, text = lines[l] || "";
    const kind = r.typeStr.replace(/^RW/, ""), rw = /^RW/.test(r.typeStr);
    const tmpl = (text.match(/<([^>]+)>/) || [])[1];
    const info = r.typeStr === "ConstantBuffer" ? "cbuffer"
      : DIM[kind] ? `${rw ? "RW" : ""}Texture dim=${DIM[kind]} ret=${RET[(tmpl || "float").replace(/\d.*$/, "")] || 5}`
      : `${r.typeStr} dim=0 ret=0`;
    const span = r.typeStr === "ConstantBuffer" ? Math.max(1, (r.buffer ? r.buffer.vars.length : 0) + 2) : 1;
    const reg = add("Register", root.id, { name: r.name, register: { info, count: r.arrays.length ? r.arrays.reduce((a, b) => a * b, 1) : 1, cls: r.cls },
      type: null, loc: loc(l, text.search(/\S/), Math.max(1, text.trim().length), span) });
    if (r.buffer && !r.buffer.packed) {
      let li = l + 1;
      for (const v of r.buffer.vars) {
        const ml = findLine(lines, new RegExp("^\\s*\\S+\\s+" + v.name + "\\b"), li);
        add("Variable", reg.id, { name: v.name, type: typeOf(v.type, structs), loc: ml ? loc(ml.i, lines[ml.i].search(/\S/), lines[ml.i].trim().length) : null });
        if (ml) li = ml.i + 1;
      }
    } else if (r.buffer && r.buffer.packed)
      add("Variable", reg.id, { name: "$Element", type: typeOf(r.buffer.elem || "F32x4", structs), loc: loc(l, text.search(/\S/), text.trim().length) });
  }

  /* functions: every definition, entrypoints carry their annotations */
  const entryByName = Object.fromEntries(doc.entries.map(e => [e.name, e]));
  const fnRe = /^\s*([A-Za-z_]\w*(?:x\d(?:x\d)?)?)\s+([A-Za-z_]\w*)\s*\(([^)]*)\)\s*(?::\s*([A-Za-z_]\w*\d*))?\s*\{?\s*$/;
  for (let i = 0; i < lines.length; i++) {
    const m = lines[i].match(fnRe);
    if (!m || (m[1] !== "void" && HLSL_KEYWORDS.has(m[1])) || HLSL_KEYWORDS.has(m[2])) continue;
    if (/^\s*(if|for|while|switch)\b/.test(lines[i])) continue;
    /* find the body extent */
    let depth = 0, end = i, started = false;
    for (let j = i; j < lines.length; j++) {
      for (const ch of lines[j]) { if (ch === "{") { depth++; started = true; } else if (ch === "}") depth--; }
      if (started && depth === 0) { end = j; break; }
    }
    const e = entryByName[m[2]];
    const fn = add("Function", root.id, { name: m[2], semantic: m[4] ? m[4].toUpperCase() : null, returns: m[1],
      loc: loc(i, lines[i].search(/\S/), lines[i].trim().length, end - i + 1) });
    /* annotations above the definition, in source order */
    for (let a = i - 1; a >= 0 && /^\s*\[/.test(lines[a]); a--) fn.annotations.unshift(lines[a].trim());
    if (e) fn.entry = { stage: e.stage, lib: e.lib };
    /* parameters */
    for (const p of m[3].split(/,(?![^()]*\))/).map(s => s.trim()).filter(Boolean)) {
      const pm = p.match(/^((?:in|out|inout|_flat|nointerpolation|linear|centroid|sample|precise)\s+)*([A-Za-z_]\w*(?:x\d(?:x\d)?)?)\s+([A-Za-z_]\w*)\s*(?::\s*([A-Za-z_]\w*\d*))?/);
      if (!pm) continue;
      const col = lines[i].indexOf(pm[3], lines[i].indexOf("("));
      add("Parameter", fn.id, { name: pm[3], type: typeOf(pm[2], structs), semantic: pm[4] ? pm[4].toUpperCase() : null,
        direction: /inout/.test(pm[1] || "") ? "inout" : /\bout\b/.test(pm[1] || "") ? "out" : "in",
        loc: loc(i, col < 0 ? 0 : col, pm[3].length) });
    }
    if (m[1] !== "void") add("Parameter", fn.id, { name: "(return)", type: typeOf(m[1], structs), direction: "return", loc: null });
    /* local variables (ESRFeature_Scopes): `Type name =` / `Type name;` inside the body */
    for (let j = i + 1; j < end; j++) {
      const vm = lines[j].match(/^\s*(?:const\s+)?([A-Za-z_]\w*(?:x\d(?:x\d)?)?)\s+([A-Za-z_]\w*)\s*(?:=|;)/);
      if (!vm || HLSL_KEYWORDS.has(vm[1]) || HLSL_KEYWORDS.has(vm[2])) continue;
      add("Variable", fn.id, { name: vm[2], type: typeOf(vm[1], structs), loc: loc(j, lines[j].search(/\S/), lines[j].trim().length) });
    }
  }

  /* built-in includes: the symbols exist in the file, the CLI collapses them per include */
  const builtins = doc.includes.filter(i => i.path.startsWith("@")).map(i => ({ file: i.path, count: builtinSymbolCount(i.path) }));
  const counts = {
    nodes: nodes.length + builtins.reduce((s, b) => s + b.count, 0),
    annotations: nodes.reduce((s, n) => s + n.annotations.length, 0),
    registers: nodes.filter(n => n.kind === "Register").length,
    enumValues: 0, types: nodes.filter(n => n.type).length,
    arrayDims: nodes.filter(n => n.arrays && n.arrays.length > 1).length, interfaces: 0
  };
  return { name: name.replace(/\.hlsl$/i, ".oiSR"), sourceName: name, compilerVersion: { ...M().VERSION },
    header: { version: "1.1", flags: { hasSymbols: true }, features: SR_FEATURES.slice(), counts },
    nodes, builtinCollapsed: builtins, hash: U.crc32c(src + "|oiSR") };
}

/* ================================================================ oiSP (pipeline + provenance) */

/* ESPField: path · indexed · why reflection can't prove it · legal values. Verbatim from SPField_info. */
const SP_FIELDS = [
  ["rtv.format", true, "the signature proves the primitive written, never the target's storage format", "any color format"],
  ["rtv.count", false, "the signature proves how many targets are written, and more may be bound", "0..8"],
  ["blend.enable", false, "blending is pipeline state a shader can't declare", "0 = off, 1 = on"],
  ["blend.independent", false, "per target blend state is a pipeline choice", "0 = all targets share [0], 1 = per target"],
  ["blend.targetMask", false, "which targets blend is a pipeline choice", "bit per render target"],
  ["blend.logicOp", false, "a logic op replaces blending and is pipeline state", "ELogicOpExt"],
  ["blend.writeMask", true, "which channels a target keeps is pipeline state", "EWriteMask bits"],
  ["blend.src", true, "a blend factor is pipeline state", "EBlend"],
  ["blend.dst", true, "a blend factor is pipeline state", "EBlend"],
  ["blend.srcAlpha", true, "a blend factor is pipeline state", "EBlend"],
  ["blend.dstAlpha", true, "a blend factor is pipeline state", "EBlend"],
  ["blend.op", true, "a blend op is pipeline state", "EBlendOp"],
  ["blend.opAlpha", true, "a blend op is pipeline state", "EBlendOp"],
  ["depth.format", false, "the depth attachment is pipeline state a shader can't declare", "0 = none, else a depth format"],
  ["depth.flags", false, "depth test, write and stencil enable are pipeline state", "EDepthStencilFlags bits"],
  ["depth.compare", false, "the depth compare changes early vs late depth testing", "ECompareOp"],
  ["stencil.compare", false, "stencil compare is pipeline state", "ECompareOp"],
  ["stencil.fail", false, "a stencil op is pipeline state", "EStencilOp"],
  ["stencil.pass", false, "a stencil op is pipeline state", "EStencilOp"],
  ["stencil.depthFail", false, "a stencil op is pipeline state", "EStencilOp"],
  ["stencil.writeMask", false, "a stencil mask is pipeline state", "0..255"],
  ["stencil.readMask", false, "a stencil mask is pipeline state", "0..255"],
  ["raster.cullMode", false, "culling is pipeline state", "ECullMode"],
  ["raster.flags", false, "wireframe, clamp and similar are pipeline state", "ERasterizerFlags bits"],
  ["raster.depthBiasConstant", false, "depth bias is pipeline state", "any I32"],
  ["raster.depthBiasClamp", false, "depth bias is pipeline state", "any F32 (as bits)"],
  ["raster.depthBiasSlope", false, "depth bias is pipeline state", "any F32 (as bits)"],
  ["msaa", false, "sample count changes whether the pixel stage runs per sample", "0 = 1 sample, 1 = 2, 2 = 4, 3 = 8"],
  ["msaa.minSampleShading", false, "sample rate shading changes the pixel stage's whole schedule", "0 = off, else 0..1 (as bits)"],
  ["topology", false, "topology is pipeline state a shader can't declare", "ETopologyMode"],
  ["patchControlPoints", false, "the control point count is pipeline state the hull stage doesn't declare", "1..32, 0 without tessellation"],
  ["vertex.stride", true, "the signature proves the formats, never how they are packed into a buffer", "0..4095"],
  ["vertex.rate", true, "per vertex or per instance is a pipeline choice", "0 = per vertex, 1 = per instance"],
  ["rt.maxRecursionDepth", false, "recursion depth is a pipeline limit, not something a shader declares", "1 or higher"],
  ["rt.flags", false, "skip triangles, skip AABBs and null shader rules are pipeline state", "EPipelineRaytracingFlags bits (assumed as the engine default, skip AABBs)"]
].map(([field, indexed, reason, domain]) => ({ field, indexed, reason, domain }));
const SP_FIELD = Object.fromEntries(SP_FIELDS.map(f => [f.field, f]));

const GRAPHICS_STAGES = ["vertex", "hull", "domain", "geometry", "pixel"];
const RT_STAGES = ["raygeneration", "miss", "closesthit", "anyhit", "intersection", "callable"];
const HIT_STAGES = ["closesthit", "anyhit", "intersection"];
const RGBA8 = 3;                                     // ETextureFormatId_RGBA8, the assumed color target

function esbBytes(t) {
  const m = String(t).match(/^([FIU])(8|16|32|64)(?:x([2-4]))?(?:x([2-4]))?$/);
  if (!m) return 16;
  return (+m[2] / 8) * (m[3] ? +m[3] : 1) * (m[4] ? +m[4] : 1);
}

/* SPFile_derivePipeline over an SHDocument: pick the pipeline kind the way the CLI does (compute first, then
 * graphics, then ray tracing), bind every stage of that kind, and report every field reflection can't prove with
 * its provenance. `pick` = {stage: binaryIdx} resolves a stage kind that occurs more than once (the CLI refuses
 * to guess between two vertex shaders; -entry picks). */
function derivePipeline(doc, pick) {
  pick = pick || {};
  const bins = doc.binaries.map((b, i) => ({ ...b, idx: i }));
  /* a lib binary's stages are those of the entries it holds ([shader("compute")] is a lib too) */
  const stagesOf = b => b.lib ? doc.entries.filter(e => b.entryNames.includes(e.name)).map(e => e.stage) : [b.stage];
  const kindOf = b => {
    const s = stagesOf(b);
    return s.some(x => x === "compute" || x === "node") ? "compute" : s.some(x => GRAPHICS_STAGES.includes(x)) ? "graphics" : s.some(x => RT_STAGES.includes(x)) ? "raytracing" : null;
  };
  const kinds = [...new Set(bins.map(kindOf).filter(Boolean))];
  const kind = kinds.includes("compute") ? "compute" : kinds.includes("graphics") ? "graphics" : kinds.includes("raytracing") ? "raytracing" : null;
  if (!kind) return { refused: "no stage this file declares can form a pipeline" };
  const notes = [], flags = [];
  if (kinds.length > 1)
    notes.push(`this oiSH holds ${kinds.length} pipelines (${kinds.join(", ")}); ${kind} is compiled first, the others are separate pipelines`);

  const stageOf = (stage, bin, e) => ({ stage, shaderFile: doc.name, entrypoint: e ? e.name : (bin.entrypoint || bin.entryNames[0]),
    sourceHash: doc.sourceHash, binaryIdx: bin.idx, generated: false });
  let stages = [], fields = [];
  const F = (field, index, value, source) => fields.push({ ...SP_FIELD[field], index: index || 0, value, source });

  if (kind === "compute") {
    const cs = bins.filter(b => kindOf(b) === "compute");
    const chosen = cs.find(b => b.idx === pick.compute) || cs[0];
    if (cs.length > 1 && pick.compute == null)
      return { refused: `two compute entries (${cs.map(b => `#${b.idx} ${b.entrypoint || b.entryNames.join(",")}`).join(", ")}); pass -entry to pick one`,
        candidates: { compute: cs.map(b => b.idx) } };
    stages = [stageOf("compute", chosen, null)];
  }

  else if (kind === "graphics") {
    const candidates = {};
    for (const s of GRAPHICS_STAGES) {
      const c = bins.filter(b => stagesOf(b).includes(s));
      if (c.length > 1) candidates[s] = c.map(b => b.idx);
      if (c.length > 1 && pick[s] == null)
        return { refused: `two ${s} entries (${c.map(b => `#${b.idx} ${b.entrypoint || b.entryNames.join(",")}` + (b.defines.length ? ` [${b.defines.map(d => d.name).join(",")}]` : "")).join(", ")}); pass -entry to pick one`, candidates };
      const b = c.find(x => x.idx === pick[s]) || c[0];
      if (b) stages.push(stageOf(s, b, doc.entries.find(e => e.stage === s && (b.lib ? b.entryNames.includes(e.name) : e.name === b.entrypoint))));
    }
    const has = s => stages.some(x => x.stage === s);
    if ((has("hull") || has("domain")) && !(has("hull") && has("domain")))
      return { refused: "a lone hull or domain stage is refused: generating the other half of a tessellation pair isn't supported yet" };
    if (has("hull")) return { refused: "tessellation stages aren't inspectable yet: ETopologyMode has no patch-list topology, which Vulkan requires whenever they're present" };
    /* stand-ins for the missing ends of the chain */
    if (!has("vertex")) { stages.unshift({ stage: "vertex", shaderFile: "(generated)", entrypoint: "main", sourceHash: 0, generated: true }); flags.push("GeneratedVertexStage"); }
    if (!has("pixel")) { stages.push({ stage: "pixel", shaderFile: "(generated)", entrypoint: "main", sourceHash: 0, generated: true }); flags.push("GeneratedPixelStage"); }

    const vsE = doc.entries.find(e => e.name === (stages.find(s => s.stage === "vertex") || {}).entrypoint && e.stage === "vertex");
    const psE = doc.entries.find(e => e.name === (stages.find(s => s.stage === "pixel") || {}).entrypoint && e.stage === "pixel");
    const written = psE ? psE.outputs.filter(o => o.semantic === "SV_TARGET").length : 0;
    const rtvCount = Math.max(1, written);
    F("rtv.count", 0, rtvCount, "assumed");
    for (let i = 0; i < rtvCount; i++) F("rtv.format", i, RGBA8, "assumed");
    for (let i = 0; i < rtvCount; i++) {
      F("blend.writeMask", i, 15, "assumed");
      for (const f of ["blend.src", "blend.dst", "blend.srcAlpha", "blend.dstAlpha", "blend.op", "blend.opAlpha"]) F(f, i, 0, "assumed");
    }
    for (const f of ["blend.enable", "blend.independent", "blend.targetMask", "blend.logicOp", "depth.format", "depth.flags", "depth.compare",
      "stencil.compare", "stencil.fail", "stencil.pass", "stencil.depthFail", "stencil.writeMask", "stencil.readMask",
      "raster.cullMode", "raster.flags", "raster.depthBiasConstant", "raster.depthBiasClamp", "raster.depthBiasSlope",
      "msaa", "msaa.minSampleShading", "topology"]) F(f, 0, 0, "assumed");
    /* vertex layout: formats derive from the VS inputs, packing into a buffer doesn't */
    const inputs = vsE ? vsE.inputs.filter(io => !/^SV_/.test(io.semantic)) : [];
    if (inputs.length) {
      const stride = inputs.reduce((s, io) => s + esbBytes(io.type), 0);
      F("vertex.stride", 0, stride, "assumed");
      F("vertex.rate", 0, 0, "assumed");
    }
    fields.vertexAttributes = inputs.length;
  }

  else {
    const lib = bins.filter(b => b.lib);
    const chosen = lib.find(b => b.idx === pick.lib) || lib[0];
    if (lib.length > 1 && pick.lib == null)
      return { refused: `this file holds ${lib.length} library binaries (${lib.map(b => `#${b.idx} ${b.extensions.join("+") || "no extensions"}`).join(", ")}); pass -entry to pick one`, candidates: { lib: lib.map(b => b.idx) } };
    for (const e of doc.entries.filter(e => chosen.entryNames.includes(e.name) && RT_STAGES.includes(e.stage)))
      stages.push(stageOf(e.stage, chosen, e));
    const perKind = HIT_STAGES.map(s => stages.filter(x => x.stage === s).length);
    if (perKind.some(n => n > 1)) flags.push("AssumedHitGrouping");
    F("rt.maxRecursionDepth", 0, 1, "assumed");
    F("rt.flags", 0, 2, "assumed");                    // EPipelineRaytracingFlags_Default = SkipAABBs
  }

  const base = doc.name.replace(/\.(oiSH|hlsl)$/i, "");
  const pipeline = { name: base, type: kind, flags, stages, fields, notes,
    vertexAttributes: fields.vertexAttributes || 0, binaryIdx: stages.find(s => !s.generated)?.binaryIdx ?? 0 };
  delete fields.vertexAttributes;
  const counts = spCounts([pipeline]);
  return { name: base + ".oiSP", sourceName: doc.name, header: { version: "1.1", counts }, pipelines: [pipeline] };
}

function spCounts(pipelines) {
  const g = pipelines.filter(p => p.type === "graphics").length;
  return {
    pipelines: pipelines.length, stages: pipelines.reduce((s, p) => s + p.stages.length, 0),
    specializations: pipelines.reduce((s, p) => s + p.fields.length, 0),
    graphicsStates: g, raytracingStates: pipelines.filter(p => p.type === "raytracing").length,
    /* a blend state stores only the attachments it can reach; the assumed state blends nothing */
    blendAttachments: pipelines.reduce((s, p) => s + (p.type !== "graphics" ? 0 : (fieldVal(p, "blend.enable") ? (fieldVal(p, "blend.independent") ? popcount(fieldVal(p, "blend.targetMask")) : 1) : 0)), 0),
    vertexBuffers: pipelines.reduce((s, p) => s + p.fields.filter(f => f.field === "vertex.stride" && f.value).length, 0),
    vertexAttributes: pipelines.reduce((s, p) => s + (p.vertexAttributes || 0), 0)
  };
}
const popcount = v => { let n = 0; for (v >>>= 0; v; v &= v - 1) n++; return n; };
const fieldVal = (p, name, index) => (p.fields.find(f => f.field === name && f.index === (index || 0)) || {}).value || 0;

/* SPFile_supply: the caller chose a value, so the field stops being assumed. */
function supply(sp, pipelineIdx, field, index, value) {
  const p = sp.pipelines[pipelineIdx];
  const f = p.fields.find(x => x.field === field && x.index === index);
  if (!f) throw new Error(`${field}${SP_FIELD[field]?.indexed ? `[${index}]` : ""} isn't a field this pipeline reports`);
  f.value = value >>> 0; f.source = "supplied";
  sp.header.counts = spCounts(sp.pipelines);
  return sp;
}
const isExact = p => p.fields.every(f => f.source !== "assumed");

/* The `file data` print of an oiSP, line for line how SPFile_print writes it. */
function spPrint(sp, pipelineIdx) {
  const p = sp.pipelines[pipelineIdx];
  const L = [];
  const assumed = p.fields.filter(f => f.source === "assumed").length;
  L.push(`; Pipeline state (${p.type}), ${p.stages.filter(s => !s.generated).length} stage(s), ${assumed} assumed field(s)`);
  if (p.flags.includes("GeneratedVertexStage"))
    L.push(";   NOTE: the vertex stage was generated, since this shader declares none and a graphics pipeline needs one. It reads the values the pixel stage expects from the app data buffer so the driver can't fold them away. Its own disassembly means nothing here.");
  if (p.flags.includes("GeneratedPixelStage"))
    L.push(";   NOTE: the pixel stage was generated, since this shader declares none. It consumes every input it receives so the preceding stage's output writes aren't eliminated as dead. Its own disassembly means nothing here.");
  if (p.flags.includes("AssumedHitGrouping"))
    L.push(";   NOTE: this lib has more than one hit shader, so their pairing into hit groups was inferred by order. Pass explicit groups to be sure.");
  for (const f of p.fields) L.push(`;   ${f.field}${f.indexed ? `[${f.index}]` : ""} = ${f.value} (${f.source}; ${f.domain})`);
  return L.join("\n");
}

const SP_MAGIC = [0x6F, 0x69, 0x53, 0x50], SR_MAGIC = [0x6F, 0x69, 0x53, 0x52];
function mockBytes(magic, size, seed) {
  const a = new Uint8Array(Math.max(size, 64));
  magic.forEach((b, i) => a[i] = b);
  const rng = U.mulberry32(seed >>> 0);
  for (let i = magic.length; i < a.length; i++) a[i] = Math.floor(rng() * 256);
  return a;
}
/* stored size mirrors the format: SPHeader 36 + 20/pipeline + 16/stage + 8/specialization + 64/graphics state
 * + 4/rt state, then the names oiDL; an SRFile is dominated by its 12 B nodes. */
const spBytes = sp => mockBytes(SP_MAGIC, 4 + 36 + sp.pipelines.length * 20 + sp.header.counts.stages * 16 + sp.header.counts.specializations * 8
  + sp.header.counts.graphicsStates * 64 + sp.header.counts.raytracingStates * 4 + 96, U.crc32c(JSON.stringify(sp.pipelines)));
const srBytes = sr => mockBytes(SR_MAGIC, 4 + 48 + sr.header.counts.nodes * 12 + sr.header.counts.registers * 8 + sr.header.counts.types * 16 + 256, sr.hash);

/* Preloaded examples for Inspect mode, derived from the sample project: two symbol ASTs and one pipeline of each kind,
 * the graphics and ray tracing ones with a few fields supplied so every provenance shows. */
function seedOisrDocs() {
  const out = {};
  for (const src of ["lighting.hlsl", "post.hlsl"]) {
    const sr = reflectSymbols(src, M().SAMPLE_FILES);
    out[sr.name] = sr;
  }
  return out;
}
function seedOispDocs() {
  const out = {};
  const lighting = derivePipeline(M().analyze("lighting.hlsl", M().SAMPLE_FILES), {});
  out[lighting.name] = lighting;

  const postDoc = M().analyze("post.hlsl", M().SAMPLE_FILES);
  const post = derivePipeline(postDoc, { pixel: postDoc.binaries.findIndex(b => b.stage === "pixel") });
  if (!post.refused) {
    supply(post, 0, "rtv.format", 0, 28);             // rgba16f, the way -pso-set rtv.format[0]=rgba16f supplies it
    supply(post, 0, "blend.enable", 0, 1);
    supply(post, 0, "blend.targetMask", 0, 1);
    supply(post, 0, "blend.src", 0, 2);               // EBlend_SrcAlpha
    supply(post, 0, "blend.dst", 0, 3);               // EBlend_OneMinusSrcAlpha
    supply(post, 0, "topology", 0, 0);
    out[post.name] = post;
  }

  const traceDoc = M().analyze("trace.hlsl", M().SAMPLE_FILES);
  const trace = derivePipeline(traceDoc, { lib: traceDoc.binaries.length - 1 });
  if (!trace.refused) {
    supply(trace, 0, "rt.maxRecursionDepth", 0, 2);   // -pso-set rt.maxRecursionDepth=2
    supply(trace, 0, "rt.flags", 0, 2);               // EPipelineRaytracingFlags_Default (skip AABBs)
    out[trace.name] = trace;
  }
  return out;
}

/* Mock parse of uploaded bytes: derive a shape from the sample project, the way mock.js does for oiSH. */
function parseOiSRBytes(name, bytes) {
  const pick = U.crc32c(bytes) & 1 ? "post.hlsl" : "lighting.hlsl";
  const sr = reflectSymbols(pick, M().SAMPLE_FILES);
  sr.name = name; sr.mockParsed = true; sr.byteLength = bytes.length;
  return sr;
}
function parseOiSPBytes(name, bytes) {
  const pick = U.crc32c(bytes) % 3;
  const src = ["lighting.hlsl", "post.hlsl", "trace.hlsl"][pick];
  const doc = M().analyze(src, M().SAMPLE_FILES);
  const sp = derivePipeline(doc, { pixel: doc.binaries.findIndex(b => b.stage === "pixel"), lib: doc.binaries.findIndex(b => b.lib) });
  if (sp.refused) return { name, mockParsed: true, header: { version: "1.1", counts: spCounts([]) }, pipelines: [], refused: sp.refused };
  sp.name = name; sp.mockParsed = true; sp.byteLength = bytes.length;
  return sp;
}

/* ================================================================ ISA (isa devices / isa disassemble) */

/* What the bundled offline compiler accepts as -asic (SpvISA_listSupportedTargets): gfx11xx + gfx12xx. */
const ISA_TARGETS = [
  { asic: "gfx1100", arch: "RDNA3", names: ["Radeon RX 7900 XTX / XT", "Radeon PRO W7900 / W7800"] },
  { asic: "gfx1101", arch: "RDNA3", names: ["Radeon RX 7800 XT / 7700 XT"] },
  { asic: "gfx1102", arch: "RDNA3", names: ["Radeon RX 7600 / 7600 XT"] },
  { asic: "gfx1103", arch: "RDNA3", names: ["Radeon 780M / 760M (Phoenix)"] },
  { asic: "gfx1150", arch: "RDNA3.5", names: ["Radeon 890M / 880M (Strix Point)"] },
  { asic: "gfx1200", arch: "RDNA4", names: ["Radeon RX 9070 XT / 9070"] },
  { asic: "gfx1201", arch: "RDNA4", names: ["Radeon RX 9060 XT"] }
];
const isaTargets = () => ISA_TARGETS.map(t => ({ ...t, names: t.names.slice() }));

const SHADER_TYPE = { vertex: "vs", pixel: "ps", compute: "cs", geometry: "gs", hull: "hs", domain: "ds", mesh: "ms", task: "as", node: "cs" };

/* SpvISA_stageHasOfflinePath + SpvISA_disassemble: raster/compute/mesh only, SPIR-V only, one entrypoint. */
function isaDisassemble(doc, bin, asic, entrypoint) {
  const target = ISA_TARGETS.find(t => t.asic === asic || t.asic === "gfx" + String(asic).replace(/\./g, ""));
  if (!target) throw new Error(`'${asic}' isn't a supported -asic; pick one of ${ISA_TARGETS.map(t => t.asic).join(", ")} (see isa devices)`);
  if (!bin.sizes.spirv) throw new Error("DXIL has no offline path here: offline AMD ISA is derived from SPIR-V only. Disassembling DXIL needs a live AMD device (-asic live).");
  const stage = bin.lib ? (doc.entries.find(e => e.name === (entrypoint || bin.entryNames[0])) || {}).stage : bin.stage;
  if (RT_STAGES.includes(stage))
    throw new Error("ray tracing has no offline path: amdllpc emits an empty code section without pipeline context. Use -asic live for a real device's ISA.");
  const name = bin.lib ? (entrypoint || bin.entryNames[0]) : bin.entrypoint;
  const seed = U.crc32c(`${bin.identKey}|${asic}|${name}|${doc.sourceHash}`);
  const rng = U.mulberry32(seed);
  const regs = bin.registers.length;
  const body = [];
  const pick = arr => arr[Math.floor(rng() * arr.length)];
  const hex = n => n.toString(16).toUpperCase().padStart(8, "0");
  const V = () => "v" + Math.floor(rng() * 12), S = () => "s" + Math.floor(rng() * 10);
  let pc = 0;
  const emit = (text, words) => { body.push({ text, pc, enc: words }); pc += words.length * 4; };
  emit(`s_load_dwordx4 s[0:3], s[0:1], 0x0`, [0xF4080000 ^ seed & 0xFFFF, 0xF8000000]);
  emit(`s_waitcnt lgkmcnt(0)`, [0xBF89FC07]);
  const n = 14 + regs * 3 + Math.floor(rng() * 12);
  for (let i = 0; i < n; i++) {
    const k = rng();
    if (k < 0.18) emit(`${pick(["v_mul_f32_e32", "v_add_f32_e32", "v_fma_f32", "v_mad_u32_u24", "v_cvt_f32_i32_e32", "v_xor_b32_e32"])} ${V()}, ${V()}, ${V()}`, [0x08000000 | Math.floor(rng() * 0xFFFFFF)]);
    else if (k < 0.30) emit(`${pick(["s_mov_b32", "s_bfe_u32", "s_and_b32", "s_lshl_b32"])} ${S()}, ${S()}, ${pick(["0x10", "0x90016", "exec_lo", "0xFF"])}`, [0xBE800000 | Math.floor(rng() * 0xFFFF), Math.floor(rng() * 0xFFFFFFFF)]);
    else if (k < 0.40) emit(`buffer_load_dword ${V()}, ${V()}, s[4:7], 0 offen`, [0xE0301000, 0x80000000 | Math.floor(rng() * 0xFFFF)]);
    else if (k < 0.48) emit(`image_sample ${V()}, [${V()}, ${V()}], s[8:15], s[16:19] dmask:0xf dim:SQ_RSRC_IMG_2D`, [0xF06C0F00, Math.floor(rng() * 0xFFFFFFFF), 0x00000000]);
    else if (k < 0.56) emit(`s_waitcnt vmcnt(0)`, [0xBF8C0F70]);
    else if (k < 0.62) emit(`v_cmpx_gt_u32_e64 ${S()}, ${V()}`, [0xD4CC007E, 0x00020400]);
    else if (k < 0.68) emit(`s_cbranch_execz _L${Math.floor(rng() * 3)}`, [0xBFA50002]);
    else if (k < 0.76) emit(`s_delay_alu instid0(VALU_DEP_${1 + Math.floor(rng() * 3)}) | instskip(NEXT) | instid1(VALU_DEP_2)`, [0xBF870112]);
    else if (k < 0.84) emit(`v_mov_b32_e32 ${V()}, ${pick(["1.0", "0", "0x3f000000", "-1"])}`, [0x7E0002F2 ^ Math.floor(rng() * 0xFF)]);
    else if (k < 0.90) emit(`ds_${pick(["read_b32", "write_b32", "add_u32"])} ${V()}, ${V()}`, [0xD8EC0000, Math.floor(rng() * 0xFFFF)]);
    else emit(`s_or_b64 exec, exec, s[${2 * Math.floor(rng() * 4)}:${2 * Math.floor(rng() * 4) + 1}]`, [0x8CFE047E]);
  }
  if (stage === "vertex") emit(`exp pos0, v1, v3, v2, v0 done`, [0xF80008CF, 0x00020301]);
  else if (stage === "pixel") emit(`exp mrt0, v0, v1, v2, v3 done compr vm`, [0xF8001C0F, 0x03020100]);
  else if (stage !== "compute") emit(`exp prim, v0, off, off, off done`, [0xF8000941, 0x00000000]);
  else emit(`${pick(["buffer_store_dword", "global_store_dword", "image_store"])} ${V()}, ${V()}, s[20:23], 0 offen`, [0xE0701000, 0x80000000]);
  emit(`s_endpgm`, [0xBFB00000]);

  const lines = body.map(b => {
    const pad = Math.max(1, 60 - b.text.length);
    return `\t${b.text}${" ".repeat(pad)}// ${hex(b.pc).padStart(12, "0")}: ${b.enc.map(w => (w >>> 0).toString(16).toUpperCase().padStart(8, "0")).join(" ")}`;
  });
  const stats = { sgprs: 8 + Math.floor(rng() * 20), vgprs: 4 + regs * 2 + Math.floor(rng() * 12), codeBytes: pc, instrs: body.length,
    scratch: 0, lds: stage === "compute" ? Math.floor(rng() * 4) * 1024 : 0 };
  const text = [`; ${asic}  SGPRs ${stats.sgprs}  VGPRs ${stats.vgprs}  code ${stats.codeBytes} B  instrs ${stats.instrs}  scratch ${stats.scratch}  lds ${stats.lds}`,
    `_amdgpu_${SHADER_TYPE[stage] || "cs"}_main:`, ...lines].join("\n");
  return { asic: target.asic, arch: target.arch, entrypoint: name, stage, stats, text };
}

window.OxMockFormats = { SR_KINDS, SR_FEATURES, SP_FIELDS, ISA_TARGETS, GRAPHICS_STAGES, RT_STAGES,
  reflectSymbols, derivePipeline, supply, isExact, spPrint, spBytes, srBytes, parseOiSRBytes, parseOiSPBytes,
  seedOisrDocs, seedOispDocs, isaTargets, isaDisassemble };
})();
