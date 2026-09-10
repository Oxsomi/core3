/* intellisense.js: hover, completion and ctrl+click, fed by the same oiSR reflection the Symbols tab
 * shows, plus a macro index scraped from the sources themselves (a #define is preprocessed away long
 * before the AST, so texture2DUniform and friends exist nowhere else).
 *
 * Hover speaks HLSL: `F32x4 color : TEXCOORD0`, `void shade(Light l)`, `struct Light { ... }`, the way
 * the declaration would be written, with the kind and its home as the small print. The cores are pure
 * (doc + text in, data out) so they test without a DOM; the CodeMirror glue at the bottom degrades to
 * nothing when the hint addon or a real editor aren't there. */
(function () {
"use strict";
const U = window.OxUtil;

const lex = () => (window.OxEditor && window.OxEditor.LEX) || {};
const words = s => (s || "").split(" ").filter(Boolean);

/* ---- lookup ------------------------------------------------------------------------------- */

/* Every node in the doc carrying this name, user code before builtin includes, definitions first. */
/* Several declarations can share a name (an interface's area() and every implementation of it), so
 * resolution follows the language: from the scope the cursor is in, outward. The innermost node whose
 * loc spans the line is the starting scope (a method body, its struct, a namespace), and each level of
 * the parent chain is searched before the one above it, so `area` inside Circle means Circle's.
 * A cursor on a declaration's own line resolves to that declaration. */
function resolveInScope(doc, name, line) {

  /* Line numbers only mean something within one file: the cursor is in the reflected main file, so an
   * include's node that happens to span the same line number must not count as a covering scope. */
  const home = (doc.sourceName || doc.name || "").replace(/^\.\//, "");
  const inHome = n => !home || (n.loc.file || "").replace(/^\.\//, "") === home;

  const covering = doc.nodes
    .filter(n =>
      !n.builtin && n.loc && n.loc.line && inHome(n) &&
      line >= n.loc.line && line < n.loc.line + (n.loc.lines || 1))
    .sort((a, b) => (a.loc.lines || 1) - (b.loc.lines || 1));

  let scope = covering[0] || null;

  while (scope) {
    if (scope.name === name) return scope;
    const hit = (scope.children || []).map(c => doc.nodes[c]).find(c => c && c.name === name);
    if (hit) return hit;
    scope = scope.parent >= 0 ? doc.nodes[scope.parent] : null;
  }
  return null;
}

function findByName(doc, name, at) {
  if (!doc || !doc.nodes) return [];

  const rank = { Struct: 0, Interface: 0, Enum: 0, Function: 1, Register: 2, Typedef: 2 };
  const untypedFn = n => n.kind !== "Function" ? 0 :
    (n.children || []).some(c => doc.nodes[c] && doc.nodes[c].name === "(return)" && doc.nodes[c].type) ? 0 : 1;

  const matches = doc.nodes
    .filter(n => n.name === name)
    .sort((a, b) =>
      (!!a.builtin - !!b.builtin) ||
      ((rank[a.kind] ?? 3) - (rank[b.kind] ?? 3)) || (untypedFn(a) - untypedFn(b)));

  const scoped = at && at.line ? resolveInScope(doc, name, at.line) : null;
  return scoped ? [scoped, ...matches.filter(n => n !== scoped)] : matches;
}

const typeText = n => n.type ? (n.type.display || n.type.name || "") : "";
const arrText = n => n.arrays && n.arrays.length ? "[" + n.arrays.join("][") + "]" : "";

/* ---- member chains ------------------------------------------------------------------------ */

/* A member belongs to what it is written on, so `a.b` resolves through a's type rather than by the
 * name b across the document, where two structs with a `pos` answer for each other. oiSR already
 * carries the link this needs: a node's type names the node that declares it (type.def), so a chain
 * is that hop once per segment.
 * Only plain chains resolve. `arr[i].x`, a call's result and swizzles need the expression parsed,
 * which the page does not do and does not pretend to: those fall back to the by-name lookup. */

const typeNameOf = n => ((n && n.type && n.type.name) || "").replace(/<.*$/, "");

const DECL_KINDS = { Struct: 1, Interface: 1, Enum: 1 };

/* A value reaches its members through its type, while a type declaration is that scope already, so
 * Light.pos reads the same way sun.pos does. */
function memberOf(doc, node, name) {
  if (!node) return null;
  const def = DECL_KINDS[node.kind] ? node
    : (node.type && node.type.def >= 0 ? doc.nodes[node.type.def] : null);
  if (!def) return null;
  return (def.children || []).map(c => doc.nodes[c]).find(c => c && c.name === name) || null;
}

/* The same hop for a type HLSL declares rather than the user: RayDesc's fields are in the builtin
 * table, not in any oiSR the page could walk. */
function builtinMemberOf(typeName, name) {
  const t = BUILTIN_TYPES[typeName];
  const row = t && t.m ? t.m.find(([, n]) => n === name) : null;
  return row ? { type: row[0], name: row[1] } : null;
}

/* The scope a receiver names: the node its chain resolves to, or the builtin a bare type name spells,
 * which no node can stand for. */
function receiverType(doc, path, at) {
  const node = resolveChain(doc, path, at);
  if (node) return { node, typeName: typeNameOf(node) || node.name || "" };
  return path.length === 1 && BUILTIN_TYPES[path[0]] ? { node: null, typeName: path[0] } : null;
}

/* The node a receiver path names: ["sun"] -> sun, ["sun", "area"] -> Light's area. */
function resolveChain(doc, path, at) {
  let node = findByName(doc, path[0], at)[0] || null;
  for (let i = 1; i < path.length && node; ++i) node = memberOf(doc, node, path[i]);
  return node;
}

/* The receiver written immediately before the cursor's word, as its segments; null when there is no
 * dot, so a bare identifier keeps resolving the way it always has. */
function receiverPath(prefix) {
  const m = /((?:[A-Za-z_]\w*\s*\.\s*)*[A-Za-z_]\w*)\s*\.\s*$/.exec(prefix || "");
  return m ? m[1].split(".").map(s => s.trim()) : null;
}

/* An enumerator's value, spelled `= 2` after its name the way the enum body writes it; a value past 2^53
 * arrives as text and prints as it is. The enum's underlying integer type rides on each enumerator, named
 * the oiSR way (U32, I64), so the hover translates it to the HLSL spelling. */
/* The module spells the underlying type the way the CLI prints it ("uint"); hover speaks the page's
 * OxC3 notation like every other type it shows, so the spelling maps over. */
const ENUM_TYPE_OXC = { uint: "U32", int: "I32", uint64_t: "U64", int64_t: "I64", uint16_t: "U16", int16_t: "I16" };
const enumValueText = n => n.enumValue && n.enumValue.value != null ? ` = ${n.enumValue.value}` : "";
const enumTypeText = n => n.enumValue && n.enumValue.type ? (ENUM_TYPE_OXC[n.enumValue.type] || n.enumValue.type) : "";

/* ---- macros ------------------------------------------------------------------------------- */

/* #define table built from the project and the builtin include sources. Memoized on a cheap
 * fingerprint (names + total length) because hover asks often and the sources change rarely. */
let macroCache = { key: "", table: null };

function macroIndex(files, builtins) {

  let key = "";
  for (const [name, f] of Object.entries(files || {})) key += name + ":" + ((f.src || "").length) + ";";
  for (const [name, src] of Object.entries(builtins || {})) key += name + ":" + ((src || "").length) + ";";

  if (macroCache.key === key && macroCache.table) return macroCache.table;

  const table = {};
  const scan = (src, file) => {
    const rx = /^[ \t]*#define[ \t]+([A-Za-z_]\w*)(\([^)]*\))?[ \t]*(.*)$/gm;
    let m;
    while ((m = rx.exec(src)))
      if (!table[m[1]])
        table[m[1]] = {
          args: m[2] || "",
          body: m[3].trim(),
          file,
          line: (src.slice(0, m.index).match(/\n/g) || []).length + 1
        };
  };

  for (const [name, f] of Object.entries(files || {})) scan(f.src || "", name);
  for (const [name, src] of Object.entries(builtins || {})) scan(src || "", name);

  macroCache = { key, table };
  return table;
}

/* Type aliases scraped from the builtin headers' own typedefs, so completion offers exactly what the
 * compiler declares (the hand list in LEX stays as the fallback when no builtins are loaded). */
let aliasCache = { key: "", list: null };

function aliasIndex(builtins) {

  let key = "";
  for (const [name, src] of Object.entries(builtins || {})) key += name + ":" + ((src || "").length) + ";";

  if (aliasCache.key === key && aliasCache.list) return aliasCache.list;

  const seen = new Set();
  const rx = /^\s*typedef\s+\S+\s+([A-Za-z_]\w*);/gm;

  for (const src of Object.values(builtins || {})) {
    let m;
    while ((m = rx.exec(src || ""))) seen.add(m[1]);
  }

  aliasCache = { key, list: [...seen] };
  return aliasCache.list;
}

/* ---- builtin intrinsics ------------------------------------------------------------------- */

/* The compiler's own intrinsic table (js/intrinsics_data.js, generated from the DXC fork), so mul,
 * saturate and Sample hover and complete with their real signatures instead of staying silent. The
 * reflection doesn't carry them: they exist in Sema, not in any source the oiSR could point at. */
const intrinsics = () => window.OxIntrinsicsData || null;

/* vk:: and dx:: names are spelled with their namespace everywhere they show. */
const nsSig = (e, sig, name) => e.ns ? sig.replace(name + "(", e.ns + "::" + name + "(") : sig;

/* What T and the layout genericity mean, spelled once in the small print. */
const genericNote = (e, method) => [
  ...(e.t ? [method ? "T: the element type" : "T: the template/payload type"] : []),
  ...(e.g ? ["takes scalars, vectors and matrices"] : []),
  ...(e.sm ? ["SM " + e.sm + "+"] : [])
].join(" · ");

/* Hover info for one builtin name; free functions outrank methods, matching how a bare word reads. */
function intrinsicInfo(name) {

  const d = intrinsics();
  if (!d) return null;

  const fn = d.fns[name];
  if (fn) {
    const note = genericNote(fn, false);
    return {
      title: nsSig(fn, fn.sigs[0], name),
      sub: "intrinsic" + (note ? " · " + note : ""),
      doc: fn.doc || null,
      loc: null,
      more: fn.sigs.length - 1,
      node: null
    };
  }

  const me = d.methods[name];
  if (!me) return null;

  const on = [...new Set(me.sigs.flatMap(s => s.on))];
  const note = genericNote(me, true);
  return {
    title: me.sigs[0].s,
    sub: `method of ${on.slice(0, 3).join(", ")}${on.length > 3 ? ` +${on.length - 3}` : ""}` +
      (note ? " · " + note : ""),
    doc: me.doc || null,
    loc: null,
    more: me.sigs.length - 1 + (me.more || 0),
    node: null
  };
}

/* The completion hint is the signature itself, shortened to what a dropdown row can carry. */
const sigHint = s => s.length > 46 ? s.slice(0, 45) + "…" : s;

/* ---- builtin types and globals ------------------------------------------------------------ */

/* Docs for the types HLSL declares rather than the user; like the intrinsic descriptions, prose has
 * no source to parse it from. Vector and matrix spellings (float3, float4x4) derive from their base
 * instead of being listed. g marks the two SM 6.6 heap globals, which are values, not types. */
const BUILTIN_TYPES = {
  bool: { d: "Boolean scalar." },
  int: { d: "32-bit signed integer." },
  uint: { d: "32-bit unsigned integer." },
  dword: { d: "32-bit unsigned integer (alias of uint)." },
  float: { d: "32-bit float." },
  double: { d: "64-bit float; needs the F64 extension." },
  half: { d: "16-bit float under 16BitTypes, otherwise 32-bit." },
  float16_t: { d: "16-bit float; needs 16BitTypes." },
  int16_t: { d: "16-bit signed integer; needs 16BitTypes." },
  uint16_t: { d: "16-bit unsigned integer; needs 16BitTypes." },
  int64_t: { d: "64-bit signed integer; needs the I64 extension." },
  uint64_t: { d: "64-bit unsigned integer; needs the I64 extension." },
  min16float: { d: "At-least-16-bit float precision hint; 16BitTypes' float16_t is the exact form." },
  min16int: { d: "At-least-16-bit signed integer precision hint." },
  min16uint: { d: "At-least-16-bit unsigned integer precision hint." },
  Texture1D: { t: "Texture1D<T>", d: "1D sampled texture (SRV): Sample through a SamplerState, Load by integer coordinate." },
  Texture1DArray: { t: "Texture1DArray<T>", d: "Array of 1D sampled textures (SRV); the last coordinate picks the slice." },
  Texture2D: { t: "Texture2D<T>", d: "2D sampled texture (SRV): Sample through a SamplerState, Load by integer coordinate, Gather per channel." },
  Texture2DArray: { t: "Texture2DArray<T>", d: "Array of 2D sampled textures (SRV); the last coordinate picks the slice." },
  Texture2DMS: { t: "Texture2DMS<T>", d: "Multisampled 2D texture (SRV): Load takes the sample index, no filtering." },
  Texture2DMSArray: { t: "Texture2DMSArray<T>", d: "Array of multisampled 2D textures (SRV)." },
  Texture3D: { t: "Texture3D<T>", d: "3D sampled texture (SRV)." },
  TextureCube: { t: "TextureCube<T>", d: "Cube texture (SRV), sampled by direction vector." },
  TextureCubeArray: { t: "TextureCubeArray<T>", d: "Array of cube textures (SRV); the 4th coordinate picks the cube." },
  RWTexture1D: { t: "RWTexture1D<T>", d: "Writable 1D texture (UAV): load/store by integer coordinate, no sampler." },
  RWTexture1DArray: { t: "RWTexture1DArray<T>", d: "Writable 1D texture array (UAV)." },
  RWTexture2D: { t: "RWTexture2D<T>", d: "Writable 2D texture (UAV): load/store by integer coordinate, no sampler." },
  RWTexture2DArray: { t: "RWTexture2DArray<T>", d: "Writable 2D texture array (UAV)." },
  RWTexture3D: { t: "RWTexture3D<T>", d: "Writable 3D texture (UAV)." },
  Buffer: { t: "Buffer<T>", d: "Typed buffer (SRV): one format-converted element per index." },
  RWBuffer: { t: "RWBuffer<T>", d: "Writable typed buffer (UAV)." },
  StructuredBuffer: { t: "StructuredBuffer<T>", d: "Buffer of T (SRV), indexed per element." },
  RWStructuredBuffer: { t: "RWStructuredBuffer<T>", d: "Writable buffer of T (UAV); Increment/DecrementCounter drive its hidden counter." },
  AppendStructuredBuffer: { t: "AppendStructuredBuffer<T>", d: "Write-only buffer of T (UAV): Append pushes past the hidden counter." },
  ConsumeStructuredBuffer: { t: "ConsumeStructuredBuffer<T>", d: "Read-once buffer of T (UAV): Consume pops from the hidden counter." },
  ByteAddressBuffer: { d: "Raw buffer (SRV) addressed in bytes: Load/Load2/Load3/Load4." },
  RWByteAddressBuffer: { d: "Writable raw buffer (UAV): Store family plus the Interlocked* family at byte offsets." },
  ConstantBuffer: { t: "ConstantBuffer<T>", d: "Constant buffer (CBV) laid out as T." },
  SamplerState: { d: "Sampler: filtering and addressing state the Sample-style methods take." },
  SamplerComparisonState: { d: "Comparison sampler for SampleCmp/GatherCmp: fetched texels compare against a reference value." },
  RaytracingAccelerationStructure: { d: "The acceleration structure TraceRay and RayQuery traverse." },
  RayDesc: { d: "The ray TraceRay and TraceRayInline take.",
    m: [["float3", "Origin"], ["float", "TMin"], ["float3", "Direction"], ["float", "TMax"]] },
  RayQuery: { t: "RayQuery<flags>", d: "Inline ray tracing: TraceRayInline starts a traversal this shader steps itself with Proceed()." },
  BuiltInTriangleIntersectionAttributes: { d: "What an any-hit or closest-hit shader receives for a triangle hit.",
    m: [["float2", "barycentrics"]] },
  BuiltInTrianglePositions: { d: "Object-space vertex positions of a triangle, out of the TriangleObjectPositions family.",
    m: [["float3", "p0"], ["float3", "p1"], ["float3", "p2"]] },
  StateObjectConfig: { d: "Ray tracing subobject: state object flags.", m: [["uint", "Flags"]] },
  GlobalRootSignature: { d: "Ray tracing subobject: the global root signature, as its text.", m: [["string", "signature"]] },
  LocalRootSignature: { d: "Ray tracing subobject: a local root signature, as its text.", m: [["string", "signature"]] },
  SubobjectToExportsAssociation: { d: "Ray tracing subobject: associates another subobject with exports.",
    m: [["string", "Subobject"], ["string", "Exports"]] },
  RaytracingShaderConfig: { d: "Ray tracing subobject: payload and attribute sizes.",
    m: [["uint", "MaxPayloadSizeInBytes"], ["uint", "MaxAttributeSizeInBytes"]] },
  RaytracingPipelineConfig: { d: "Ray tracing subobject: recursion bound.", m: [["uint", "MaxTraceRecursionDepth"]] },
  RaytracingPipelineConfig1: { d: "Ray tracing subobject: recursion bound plus pipeline flags.",
    m: [["uint", "MaxTraceRecursionDepth"], ["uint", "Flags"]] },
  TriangleHitGroup: { d: "Ray tracing subobject: a triangle hit group's shaders.",
    m: [["string", "AnyHit"], ["string", "ClosestHit"]] },
  ProceduralPrimitiveHitGroup: { d: "Ray tracing subobject: a procedural hit group's shaders.",
    m: [["string", "AnyHit"], ["string", "ClosestHit"], ["string", "Intersection"]] },
  TriangleStream: { t: "TriangleStream<T>", d: "Geometry shader output stream: Append emits a vertex, RestartStrip cuts the strip." },
  LineStream: { t: "LineStream<T>", d: "Geometry shader line output stream." },
  PointStream: { t: "PointStream<T>", d: "Geometry shader point output stream." },
  SubpassInput: { d: "SPIR-V subpass attachment, read at the current fragment with SubpassLoad." },
  SubpassInputMS: { d: "Multisampled SPIR-V subpass attachment; SubpassLoad takes the sample index." },
  ResourceDescriptorHeap: { g: 1, d: "SM 6.6 dynamic resources: index it to reach any SRV/UAV/CBV in the heap without declaring a binding." },
  SamplerDescriptorHeap: { g: 1, d: "SM 6.6 dynamic samplers: index it to reach any sampler in the heap without declaring a binding." },
};

const VEC_MAT = /^(bool|int|uint|dword|float|double|half|float16_t|int16_t|uint16_t|int64_t|uint64_t|min16float|min16int|min16uint)([1-4])(?:x([1-4]))?$/;

function typeInfo(name) {

  const t = BUILTIN_TYPES[name];
  if (t) {

    /* A builtin struct previews its members the way a user struct does; an object type previews its
     * methods off the generated table instead. */
    const title = t.m
      ? `struct ${name} { ${t.m.map(([ty, n]) => `${ty} ${n}`).join("; ")} }`
      : t.t || name;

    let doc = t.d;
    const data = intrinsics();

    if (!t.m && data) {
      const ms = Object.keys(data.methods).filter(k => data.methods[k].sigs.some(s => s.on.includes(name)));
      if (ms.length)
        doc += " Methods: " + ms.slice(0, 8).join(", ") + (ms.length > 8 ? ", …" : "") + ".";
    }

    return { title, sub: t.g ? "builtin global" : "builtin type", doc, loc: null, more: 0, node: null };
  }

  const m = VEC_MAT.exec(name);
  if (!m) return null;
  return {
    title: name, sub: "builtin type",
    doc: m[3] ? `${m[2]}x${m[3]} matrix of ${m[1]}.` : `${m[2]}-component vector of ${m[1]}.`,
    loc: null, more: 0, node: null
  };
}

/* ---- semantics ---------------------------------------------------------------------------- */

/* System-value semantics, keyed lower case with the index stripped, since HLSL matches them that way
 * (sv_target3 is SV_Target[3]). Anything else after a `:` is a user semantic. */
const SEMANTICS = {
  sv_position: { t: "SV_Position", d: "Clip-space position out of the vertex path; in a pixel shader, the pixel's window coordinate." },
  sv_target: { t: "SV_Target[n]", d: "Pixel shader output to render target n." },
  sv_depth: { t: "SV_Depth", d: "Pixel shader depth output, replacing the rasterizer's depth." },
  sv_depthgreaterequal: { t: "SV_DepthGreaterEqual", d: "Depth output promised >= the rasterized depth, so early depth testing stays on." },
  sv_depthlessequal: { t: "SV_DepthLessEqual", d: "Depth output promised <= the rasterized depth, so early depth testing stays on." },
  sv_dispatchthreadid: { t: "SV_DispatchThreadID", d: "This thread's global 3D id across the whole dispatch." },
  sv_groupid: { t: "SV_GroupID", d: "This thread group's 3D id within the dispatch." },
  sv_groupthreadid: { t: "SV_GroupThreadID", d: "This thread's 3D id within its group." },
  sv_groupindex: { t: "SV_GroupIndex", d: "This thread's flattened index within its group." },
  sv_vertexid: { t: "SV_VertexID", d: "Index of the vertex being processed." },
  sv_instanceid: { t: "SV_InstanceID", d: "Index of the instance being processed." },
  sv_primitiveid: { t: "SV_PrimitiveID", d: "Index of the primitive." },
  sv_sampleindex: { t: "SV_SampleIndex", d: "The MSAA sample being shaded; reading it forces per-sample shading." },
  sv_coverage: { t: "SV_Coverage", d: "MSAA coverage mask: as input the covered samples, as output the samples to keep." },
  sv_isfrontface: { t: "SV_IsFrontFace", d: "True when the primitive is front facing." },
  sv_domainlocation: { t: "SV_DomainLocation", d: "Where on the patch this domain shader invocation evaluates." },
  sv_tessfactor: { t: "SV_TessFactor", d: "The patch's edge tessellation factors." },
  sv_insidetessfactor: { t: "SV_InsideTessFactor", d: "The patch's inside tessellation factor(s)." },
  sv_outputcontrolpointid: { t: "SV_OutputControlPointID", d: "The control point this hull shader invocation outputs." },
  sv_gsinstanceid: { t: "SV_GSInstanceID", d: "The geometry shader instance, under [instance(n)]." },
  sv_rendertargetarrayindex: { t: "SV_RenderTargetArrayIndex", d: "Which slice of the render target array to rasterize into." },
  sv_viewportarrayindex: { t: "SV_ViewportArrayIndex", d: "Which viewport to rasterize into." },
  sv_clipdistance: { t: "SV_ClipDistance[n]", d: "Signed distance to clip plane n; a pixel with any negative distance is clipped." },
  sv_culldistance: { t: "SV_CullDistance[n]", d: "Signed distance to cull plane n; a primitive entirely negative is culled." },
  sv_barycentrics: { t: "SV_Barycentrics", d: "The pixel's barycentric weights within its primitive; needs the Barycentrics extension." },
  sv_shadingrate: { t: "SV_ShadingRate", d: "The variable shading rate applied to the primitive or pixel." },
  sv_viewid: { t: "SV_ViewID", d: "Which view is being rendered under view instancing." },
  sv_stencilref: { t: "SV_StencilRef", d: "Pixel shader stencil reference output." },
  sv_cullprimitive: { t: "SV_CullPrimitive", d: "Mesh shader per-primitive flag: true discards the primitive." },
};

function semanticInfo(word) {

  const e = SEMANTICS[word.toLowerCase().replace(/\d+$/, "")];
  if (e)
    return { title: e.t, sub: "system-value semantic", doc: e.d, loc: null, more: 0, node: null };

  return {
    title: word, sub: "user semantic",
    doc: "Names an interstage value: one stage's output pairs with the next stage's input by semantic.",
    loc: null, more: 0, node: null
  };
}

/* The word at ch, when it sits in semantic position: after a single `:` (register and packoffset live
 * there too, and mean something else). Ternaries land here as well, which is why the caller tries the
 * declared symbols first and only falls back to this. */
function semanticAt(lineText, ch) {

  let a = ch, b = ch;
  while (a > 0 && /\w/.test(lineText[a - 1])) a--;
  while (b < lineText.length && /\w/.test(lineText[b])) b++;

  const word = lineText.slice(a, b);
  if (!word || /^(register|packoffset)$/i.test(word)) return null;

  let p = a - 1;
  while (p >= 0 && (lineText[p] === " " || lineText[p] === "\t")) p--;
  if (p < 0 || lineText[p] !== ":" || (p > 0 && lineText[p - 1] === ":")) return null;
  return word;
}

/* ---- annotations and attributes ----------------------------------------------------------- */

/* HLSL's own single-bracket attributes, keyed lower case (the language matches them that way).
 * Prose again: nothing declares these anywhere a parser could reach. */
const ATTRIBUTES = {
  numthreads: { t: "[numthreads(x, y, z)]", d: "Thread group size of a compute, mesh or task entrypoint." },
  outputtopology: { t: "[outputtopology(\"triangle\")]", d: "Mesh shader output topology: \"point\", \"line\" or \"triangle\"." },
  maxvertexcount: { t: "[maxvertexcount(n)]", d: "Most vertices one geometry shader invocation may Append." },
  instance: { t: "[instance(n)]", d: "Runs the geometry shader n times per primitive (SV_GSInstanceID tells them apart)." },
  domain: { t: "[domain(\"tri\")]", d: "Patch domain of the hull/domain pair: \"tri\", \"quad\" or \"isoline\"." },
  partitioning: { t: "[partitioning(\"integer\")]", d: "How the tessellator splits edges: integer, fractional_even, fractional_odd or pow2." },
  outputcontrolpoints: { t: "[outputcontrolpoints(n)]", d: "Control points the hull shader emits per patch." },
  patchconstantfunc: { t: "[patchconstantfunc(\"f\")]", d: "The function computing the per-patch constants, tess factors included." },
  maxtessfactor: { t: "[maxtessfactor(n)]", d: "Upper bound the hull shader promises for its tess factors." },
  earlydepthstencil: { t: "[earlydepthstencil]", d: "Forces depth/stencil testing before the pixel shader runs." },
  wavesize: { t: "[WaveSize(n)] / [WaveSize(min, max, preferred)]", d: "Required wave size (SM 6.6), or the allowed range (SM 6.8)." },
  unroll: { t: "[unroll] / [unroll(n)]", d: "Unrolls the loop, fully or n times." },
  loop: { t: "[loop]", d: "Keeps the loop rolled instead of unrolled." },
  branch: { t: "[branch]", d: "Selects real branching over evaluating both sides." },
  flatten: { t: "[flatten]", d: "Evaluates both sides and selects, instead of branching." },
  allow_uav_condition: { t: "[allow_uav_condition]", d: "Allows a loop condition to depend on a UAV read." },
  raypayload: { t: "[raypayload]", d: "Marks a struct as a ray payload, enabling its payload access qualifiers." },
};

/* The annotation or attribute name at ch: {kind: "oxc"|"shader"|"attr", name}, or null. The oxc and
 * [shader] docs come out of the page's syntax reference (annotationDocs in the glue): that panel is
 * their single home, so hover reads it rather than keeping a copy that can drift. A single-bracket
 * name only counts when the attribute table knows it, so array indexing never reads as one. */
function annotationAt(lineText, ch) {

  for (const m of lineText.matchAll(/\[\[\s*oxc::(\w+)/g)) {
    const a = m.index + m[0].length - m[1].length;
    if (ch >= a && ch <= a + m[1].length) return { kind: "oxc", name: m[1] };
  }

  const s = /\[\s*shader\s*\(/.exec(lineText);
  if (s) {
    const a = s.index + s[0].indexOf("shader");
    if (ch >= a && ch <= a + "shader".length) return { kind: "shader", name: "shader" };
  }

  for (const m of lineText.matchAll(/\[\s*([A-Za-z_]\w*)/g)) {
    const a = m.index + m[0].length - m[1].length;
    if (ch >= a && ch <= a + m[1].length && ATTRIBUTES[m[1].toLowerCase()])
      return { kind: "attr", name: m[1].toLowerCase() };
  }

  return null;
}

/* ---- hover -------------------------------------------------------------------------------- */

/* The declaration spelled the way HLSL would write it. */
function signature(doc, n) {

  const sem = n.semantic ? ` : ${n.semantic}` : "";

  if (n.kind === "Function") {
    const kids = (n.children || []).map(c => doc.nodes[c]).filter(c => c && c.kind === "Parameter");
    const ret = kids.find(c => c.name === "(return)");
    const params = kids.filter(c => c !== ret && c.name && c.name !== "(anonymous)");
    const dir = p => p.direction === "inout" ? "inout " : p.direction === "out" ? "out " : "";
    return `${ret ? typeText(ret) : "void"} ${n.name}(` +
      params.map(p => `${dir(p)}${typeText(p)} ${p.name}${p.semantic ? " : " + p.semantic : ""}`.trim()).join(", ") +
      `)${sem}${n.entry ? `  [shader("${n.entry.stage}")]` : ""}`;
  }

  if (n.kind === "Struct" || n.kind === "Interface" || n.kind === "Enum") {

    const kw = n.kind === "Struct" ? "struct" : n.kind === "Interface" ? "interface" : "enum";

    /* The hierarchy in HLSL's own spelling: `struct Circle : Shape, IArea`. On a struct the colon IS
     * inheritance syntax, so unlike a variable there is nothing for it to collide with. */
    const name = id => doc.nodes[id] && doc.nodes[id].name;
    const bases = [
      ...(n.type && n.type.base >= 0 && name(n.type.base) ? [name(n.type.base)] : []),
      ...((n.implements || []).map(name).filter(Boolean))
    ];

    const members = (n.children || []).map(c => doc.nodes[c])
      .filter(c => c && c.name && !c.name.startsWith("$") && (c.kind === "Variable" || c.kind === "EnumValue"));

    /* An enum body is its enumerators with their values, comma separated as HLSL writes them, and the
     * underlying integer type sits where a struct's base does: `enum Mode : uint { Off = 0, Linear = 1 }`. */
    const isEnum = n.kind === "Enum";
    const underlying = isEnum && members.length ? enumTypeText(members[0]) : "";
    const shown = members.slice(0, 6).map(c => isEnum ? `${c.name}${enumValueText(c)}` : `${typeText(c)} ${c.name}${arrText(c)}`.trim());
    if (members.length > 6) shown.push("...");
    const heritage = isEnum ? (underlying ? " : " + underlying : "") : (bases.length ? " : " + bases.join(", ") : "");
    return `${kw} ${n.name}${heritage}${shown.length ? ` { ${shown.join(isEnum ? ", " : "; ")} }` : ""}`;
  }

  /* An enumerator reads as its line in the enum body: `Linear = 1`. */
  if (n.kind === "EnumValue")
    return `${n.name}${enumValueText(n)}`;

  /* A register array's size lives in its bind count when it is one-dimensional. The declared type is
   * the title (Texture2D<F32x4>, StructuredBuffer<Material>); the bind-class name only stands in for
   * a document old enough to carry no register types. */
  if (n.kind === "Register") {
    const arr = arrText(n) || (n.register && n.register.count > 1 ? `[${n.register.count}]` : "");
    const ty = typeText(n) || (n.register ? n.register.info : "");
    return `${ty ? ty + " " : ""}${n.name}${arr}`;
  }

  if (n.kind === "Typedef")
    return `typedef ${typeText(n)} ${n.name}`;

  const ty = typeText(n);
  return `${ty ? ty + " " : ""}${n.name}${arrText(n)}${sem}`;
}

/* What the hover card says for one name; ctx = {files, builtins} feeds the macro fallback. */
/* The contiguous `//` block sitting right above a declaration is its documentation, the way VS Code
 * reads it by default. oiSR carries no comment spans, so the lines come from the source itself. */
function docComment(ctx, loc) {
  if (!ctx || !loc || !loc.file || !loc.line) return null;
  const file = loc.file.replace(/^\.\//, "");
  const src = (ctx.files && ctx.files[file] && ctx.files[file].src) ||
    (ctx.builtins && (ctx.builtins[file] || ctx.builtins["@" + file]));
  if (typeof src !== "string") return null;
  const lines = src.split("\n");
  const out = [];
  for (let i = loc.line - 2; i >= 0; --i) {
    const m = /^\s*\/\/\/?\s?(.*)$/.exec(lines[i]);
    if (!m) break;
    out.unshift(m[1]);
  }
  return out.length ? out.join("\n").trim() : null;
}

function hoverInfo(doc, name, ctx, at) {

  /* Written on a receiver, the name is that receiver's member: resolved through the chain, so the
   * card describes this struct's field and carries the doc comment above THAT declaration. A chain
   * the page can't follow (an index, a call) falls through to the by-name lookup below. */

  const path = at && at.prefix ? receiverPath(at.prefix) : null;

  if (path && doc && doc.nodes) {

    const recv = receiverType(doc, path, at);
    const owner = recv && recv.node;
    const member = owner ? memberOf(doc, owner, name) : null;

    if (member)
      return {
        title: signature(doc, member),
        sub: `${member.kind} of ${typeNameOf(owner) || (owner.name || "")}`,
        doc: docComment(ctx, member.loc),
        loc: member.loc ? `${member.loc.file}:${member.loc.line}` : null,
        more: 0,
        node: member
      };

    const builtin = recv ? builtinMemberOf(recv.typeName, name) : null;

    if (builtin)
      return {
        title: `${builtin.type} ${builtin.name}`,
        sub: `member of ${recv.typeName}`,
        doc: (BUILTIN_TYPES[recv.typeName] || {}).d || null,
        loc: null, more: 0, node: null
      };
  }

  const matches = findByName(doc, name, at);

  if (matches.length) {
    const n = matches[0];
    /* The alias the source wrote is the title; what it resolves to rides in the small print, since a
     * colon after a variable already means its semantic. */
    const aka = n.type && n.type.name && n.type.display && n.type.name !== n.type.display
      ? ` · aka ${n.type.name}` : "";

    /* An enumerator's small print names the enum it belongs to and that enum's underlying type. */
    const owner = n.kind === "EnumValue" && n.parent >= 0 && doc.nodes[n.parent] && doc.nodes[n.parent].kind === "Enum"
      ? doc.nodes[n.parent] : null;
    const home = owner ? ` · ${owner.name}${enumTypeText(n) ? " : " + enumTypeText(n) : ""}` : "";

    return {
      title: signature(doc, n),
      sub: n.kind + aka + home + (n.builtin && n.loc ? ` · from ${n.loc.file.replace(/^\.\//, "")}` : ""),
      doc: docComment(ctx, n.loc),
      loc: n.loc ? `${n.loc.file}:${n.loc.line}` : null,
      more: matches.length - 1,
      node: n
    };
  }

  const mac = ctx && macroIndex(ctx.files, ctx.builtins)[name];

  if (mac)
    return {
      title: `#define ${name}${mac.args} ${mac.body}`.slice(0, 120),
      sub: `macro · from ${mac.file}`,
      loc: `${mac.file}:${mac.line}`,
      more: 0,
      node: null
    };

  /* Nothing declared and nothing defined: a builtin intrinsic or builtin type. Last so a user's own
   * declaration or macro of the same name shadows it, the way it does in the language. */
  return intrinsicInfo(name) || typeInfo(name);
}

/* An #include line's target, when the position sits inside the quotes: a path is not a word, so the
 * word-based lookup below can never resolve one. */
function includeTarget(lineText, ch) {
  const m = /^\s*#include\s+"([^"]*)"/.exec(lineText);
  if (!m) return null;
  const a = lineText.indexOf('"') + 1, b = a + m[1].length;
  return ch >= a - 1 && ch <= b + 1 ? { file: m[1], line: 1, ch0: 0, ch1: 1 } : null;
}

/* Where ctrl+click goes: the declaration, whether it is a symbol or a #define. */
function definitionOf(doc, name, ctx, at) {
  const n = findByName(doc, name, at)[0];
  if (n && n.loc)
    return { file: n.loc.file, line: n.loc.line, ch0: Math.max(0, (n.loc.col || 1) - 1),
      ch1: (n.loc.col || 1) - 1 + (n.loc.len || 1) };
  const mac = ctx && macroIndex(ctx.files, ctx.builtins)[name];
  if (mac) return { file: mac.file, line: mac.line, ch0: 0, ch1: 1 };
  return null;
}

/* ---- completion --------------------------------------------------------------------------- */

const KIND_HINT = { Struct: "struct", Interface: "interface", Function: "fn", Register: "resource",
  Variable: "var", StaticVariable: "var", GroupsharedVariable: "var", Typedef: "type", Enum: "enum",
  EnumValue: "enum", Parameter: "param", Namespace: "namespace" };

/* The completion list for a cursor sitting at `ch` in `lineText`. Returns null when there is nothing
 * sensible to offer; {from, list:[{text, hint}]} otherwise, `from` being where the replacement starts. */
function completions(doc, files, lineText, ch, builtins) {

  const before = lineText.slice(0, ch);
  let m;

  /* #include "..." offers the built-ins and the project's own files. */
  if ((m = /#include\s+"([^"]*)$/.exec(before))) {
    const partial = m[1];
    const names = [
      ...Object.keys(builtins || (window.OxMock && window.OxMock.BUILTINS) || {}),
      ...Object.keys(files || {})
    ];
    const list = names.filter(n => n.startsWith(partial)).map(text => ({ text, hint: "include" }));
    return list.length ? { from: ch - partial.length, list } : null;
  }

  /* [[oxc:: offers the annotations. */
  if ((m = /\[\[\s*oxc::(\w*)$/.exec(before))) {
    const list = words(lex().annotations).filter(a => a.startsWith(m[1]))
      .map(text => ({ text: text + "(", hint: "annotation" }));
    return list.length ? { from: ch - m[1].length, list } : null;
  }

  /* vk:: and dx:: offer their namespace's intrinsics. */
  if ((m = /\b(vk|dx)::(\w*)$/.exec(before))) {
    const d = intrinsics();
    if (!d) return null;
    const partial = m[2];
    const list = Object.entries(d.fns)
      .filter(([n, e]) => e.ns === m[1] && n.startsWith(partial))
      .map(([n, e]) => ({ text: n, hint: sigHint(e.sigs[0]) }));
    return list.length ? { from: ch - partial.length, list } : null;
  }

  /* obj.partial offers the members of obj's struct, read off the type graph. A builtin struct
   * (RayDesc) offers its own members; a receiver whose builtin type is known (a RayQuery value, a
   * register whose info names its texture kind) gets that type's methods only; a register or a
   * receiver the graph can't resolve gets every builtin method, since that dot is almost always
   * about to spell Sample or Load. A resolved value of a vector/scalar type stays quiet, because a
   * dot there means a swizzle, not a member. */
  if ((m = /((?:[A-Za-z_]\w*\s*\.\s*)*[A-Za-z_]\w*)\s*\.\s*(\w*)$/.exec(before))) {

    /* The whole receiver, so a.b.c offers c's members rather than nothing past the first dot, and a
     * type written by name offers its own. */
    const path = m[1].split(".").map(x => x.trim());
    const recv = receiverType(doc, path);
    const owner = recv && recv.node;
    const def = owner && (DECL_KINDS[owner.kind] ? owner
      : (owner.type && owner.type.def >= 0 ? doc.nodes[owner.type.def] : null));
    const partial = m[2];

    /* A builtin struct's fields live in the table rather than in any oiSR. */
    const bm = recv && (BUILTIN_TYPES[recv.typeName] || {}).m;

    if (bm) {
      const list = bm.filter(([, n]) => n.startsWith(partial)).map(([ty, n]) => ({ text: n, hint: ty }));
      return list.length ? { from: ch - partial.length, list } : null;
    }

    if (def && def.children) {
      const list = def.children.map(c => doc.nodes[c])
        .filter(c => c && c.name && !c.name.startsWith("$") && c.name.startsWith(partial) &&
          (c.kind === "Variable" || c.kind === "Function"))
        .map(c => ({ text: c.name, hint: `${KIND_HINT[c.kind] || ""} ${typeText(c)}`.trim() }));
      return list.length ? { from: ch - partial.length, list } : null;
    }

    const d = intrinsics();
    if (!d) return null;

    /* The receiver's builtin type: a register spells it first in its info line, a value in its type
     * name, template arguments stripped. */
    const typeName = owner
      ? (owner.kind === "Register"
        ? ((owner.register && owner.register.info) || "").split(" ")[0].replace(/<.*$/, "")
        : ((owner.type && owner.type.name) || "").replace(/<.*$/, ""))
      : null;

    const byType = typeName
      ? Object.entries(d.methods).filter(([, e]) => e.sigs.some(s => s.on.includes(typeName)))
      : [];

    if (byType.length) {
      const list = byType.filter(([n]) => n.startsWith(partial)).map(([n, e]) => ({ text: n, hint: sigHint(e.sigs[0].s) }));
      return list.length ? { from: ch - partial.length, list } : null;
    }

    /* A chain whose receiver did not resolve is a chain into something that is not there, so it
     * offers nothing; a single unknown identifier still gets the method list, since that dot is
     * usually about to spell one and the page cannot see what the value is. */
    if ((owner && owner.kind !== "Register") || (!owner && m[1].includes("."))) return null;
    const list = Object.entries(d.methods)
      .filter(([n]) => n.startsWith(partial))
      .map(([n, e]) => ({ text: n, hint: sigHint(e.sigs[0].s) }));
    return list.length ? { from: ch - partial.length, list } : null;
  }

  /* A plain identifier offers everything declared plus what HLSL spells. */
  if ((m = /([A-Za-z_]\w*)$/.exec(before))) {

    const partial = m[1];
    if (!partial.length) return null;
    const seen = new Set();
    const list = [];
    const add = (text, hint) => { if (!seen.has(text) && text.startsWith(partial)) { seen.add(text); list.push({ text, hint }); } };

    if (doc && doc.nodes)
      for (const n of doc.nodes)
        if (n.name && !n.builtin && !n.name.startsWith("$") && n.name !== "(anonymous)")
          add(n.name, `${KIND_HINT[n.kind] || n.kind} ${typeText(n)}`.trim());

    for (const [name, mac] of Object.entries(macroIndex(files, builtins)))
      add(name, "macro" + mac.args);

    const derived = aliasIndex(builtins);
    for (const w of derived) add(w, "oxc type");

    const L = lex();
    if (!derived.length)
      for (const w of words(L.oxcTypes)) add(w, "oxc type");
    for (const w of words(L.types)) add(w, "type");

    /* Intrinsics complete with their real signature as the hint; the LEX word list stands in when
     * the generated table isn't loaded. The vk:: and dx:: ones only complete behind their namespace
     * (the branch above), since the bare name is not something the compiler accepts. */
    const d = intrinsics();
    if (d)
      for (const [n, e] of Object.entries(d.fns)) { if (!e.ns) add(n, sigHint(e.sigs[0])); }
    else
      for (const w of words(L.intrinsics)) add(w, "intrinsic");

    for (const w of words(L.keywords)) add(w, "");
    for (const w of words(L.semantics)) add(w, "semantic");

    return list.length ? { from: ch - partial.length, list } : null;
  }

  return null;
}

/* ---- CodeMirror glue ---------------------------------------------------------------------- */

let tip = null;

function showTip(html, x, y) {

  if (!tip) {
    tip = document.createElement("div");
    tip.className = "ox-hover";
    document.body.appendChild(tip);
  }

  tip.innerHTML = html;
  tip.style.display = "block";

  /* Measured after the content is in, then placed inside the viewport on both axes. The card flips
   * above the cursor rather than hanging off the bottom, which is where a hover on one of the last
   * lines would otherwise put it. */

  const w = tip.offsetWidth, h = tip.offsetHeight;
  const below = y + 18;

  tip.style.left = Math.max(8, Math.min(x + 12, window.innerWidth - w - 8)) + "px";
  tip.style.top = (below + h > window.innerHeight - 8 ? Math.max(8, y - h - 12) : below) + "px";
}

/* The card. The signature is highlighted the way the disassembly views are, since it is code and reads
 * as code; what follows it is prose about the code and stays plain.
 * The kind line is already written as ` · ` separated parts, so each becomes its own chip rather than
 * one long grey run, and the ctrl+click hint appears only where there is a declaration to jump to. */
function tipHtml(info) {

  const parts = String(info.sub || "").split(" · ").filter(Boolean);

  return `<div class="ox-hover-title">${U.highlightSignature(info.title)}</div>` +
    (info.doc ? `<div class="ox-hover-doc">${U.esc(info.doc)}</div>` : "") +
    `<div class="ox-hover-meta">` +
      parts.map((p, i) =>
        `<span class="${i ? "ox-hover-note" : "ox-hover-kind"}">${U.esc(p)}</span>`).join("") +
      (info.loc ? `<span class="ox-hover-loc">${U.esc(info.loc)}</span>` : "") +
      (info.more ? `<span class="ox-hover-note">+${info.more} more</span>` : "") +
      (info.loc ? `<span class="ox-hover-hint">ctrl+click to go</span>` : "") +
    `</div>`;
}

const hideTip = () => { if (tip) tip.style.display = "none"; };

function wordAt(cm, e) {
  const pos = cm.coordsChar({ left: e.clientX, top: e.clientY }, "window");
  const line = cm.getLine(pos.line) || "";
  let a = pos.ch, b = pos.ch;
  while (a > 0 && /\w/.test(line[a - 1])) a--;
  while (b < line.length && /\w/.test(line[b])) b++;
  const w = line.slice(a, b);
  return !w || /^\d/.test(w) ? null : w;
}

/* The [[oxc::]] and [shader] docs, scraped from the syntax reference offcanvas so it stays their one
 * home; the rows carry the exact signature and prose the panel shows. Read per hover rather than
 * cached, since the vocabularies inside some rows fill in after boot. */
function annotationDocs() {

  const out = {};
  if (typeof document === "undefined" || !document.querySelectorAll) return out;

  for (const row of document.querySelectorAll(".row-item")) {
    const sig = row.querySelector(".sig"), desc = row.querySelector(".desc");
    if (!sig || !desc) continue;
    const text = sig.textContent.trim();
    const m = /^\[\[oxc::(\w+)/.exec(text) || (/^\[shader/.test(text) ? [null, "shader"] : null);
    if (m && !out[m[1]]) out[m[1]] = { sig: text, doc: desc.textContent.trim() };
  }

  return out;
}

function annotationHover(lineText, ch) {

  const at = annotationAt(lineText, ch);
  if (!at) return null;

  if (at.kind === "attr") {
    const e = ATTRIBUTES[at.name];
    return { title: e.t, sub: "attribute", doc: e.d, loc: null, more: 0, node: null };
  }

  const e = annotationDocs()[at.name];
  if (!e) return null;
  return {
    title: e.sig, sub: at.kind === "shader" ? "entry annotation" : "oxc annotation",
    doc: e.doc, loc: null, more: 0, node: null
  };
}

function attach(editor, getDoc, getFiles, getBuiltins, onGoto) {

  const cm = editor.cmHandle && editor.cmHandle();
  if (!cm || typeof cm.getWrapperElement !== "function") return;

  const ctx = () => ({ files: getFiles(), builtins: getBuiltins ? getBuiltins() : null });
  const wrap = cm.getWrapperElement();

  /* hover: quick, and quiet while the word under the mouse hasn't changed. The class toggle and the
   * timer only fire when something actually changed: both ran per mousemove event before, and a style
   * recalc per mouse pixel is exactly what "the editor feels sluggish" is made of. */
  let hoverTimer = null, lastWord = null, ctrlShown = false;

  wrap.addEventListener("mousemove", e => {
    const ctrlNow = e.ctrlKey || e.metaKey;
    if (ctrlNow !== ctrlShown) { ctrlShown = ctrlNow; wrap.classList.toggle("ox-ctrl", ctrlNow); }
    clearTimeout(hoverTimer);
    hoverTimer = setTimeout(() => {
      const word = wordAt(cm, e);
      if (word === lastWord && tip && tip.style.display === "block") return;
      lastWord = word;
      const pos = cm.coordsChar ? cm.coordsChar({ left: e.clientX, top: e.clientY }, "window") : null;
      const lineText = pos && cm.getLine ? cm.getLine(pos.line) || "" : "";

      /* What is written in front of the word decides whether it is a member of something: hoverInfo
       * resolves `a.b` through a rather than by the name b. */
      let wordStart = pos ? pos.ch : 0;
      while (wordStart > 0 && /\w/.test(lineText[wordStart - 1])) wordStart--;

      /* Position outranks name where position IS the meaning (an annotation's word, a semantic after
       * the colon); declared symbols still shadow a semantic-looking spot, since a ternary's `: x`
       * lands there too and x is usually a declared local. */
      const info = word && (
        annotationHover(lineText, pos ? pos.ch : -1) ||
        hoverInfo(getDoc(), word, ctx(), pos ? { line: pos.line + 1, prefix: lineText.slice(0, wordStart) } : null) ||
        (semanticAt(lineText, pos ? pos.ch : -1) ? semanticInfo(word) : null)
      );
      if (!info) return hideTip();
      showTip(tipHtml(info), e.clientX, e.clientY);
    }, 50);
  });
  wrap.addEventListener("mouseleave", () => { clearTimeout(hoverTimer); lastWord = null; hideTip(); });
  cm.on("cursorActivity", () => { lastWord = null; hideTip(); });

  /* ctrl+click: go to the declaration, macros and builtin includes included */
  wrap.addEventListener("mousedown", e => {
    if (!(e.ctrlKey || e.metaKey) || e.button !== 0) return;
    const pos = cm.coordsChar({ left: e.clientX, top: e.clientY }, "window");
    const inc = includeTarget(cm.getLine(pos.line) || "", pos.ch);
    const word = inc ? null : wordAt(cm, e);
    const def = inc || (word && definitionOf(getDoc(), word, ctx(), { line: pos.line + 1 }));
    if (!def) return;
    e.preventDefault();
    hideTip();
    if (onGoto) onGoto(def);
  }, true);

  /* completion, through the standard hint addon when it is loaded.
   * The hint function is passed EXPLICITLY on every call: registerHelper keys on the mode's name, and
   * this mode is clike wearing an HLSL MIME, so a helper registered under the MIME is never found and
   * Ctrl+Space silently does nothing (it did). */
  if (!CodeMirror.showHint) return;

  const hintFn = () => {
    const cur = cm.getCursor();
    const r = completions(getDoc(), getFiles(), cm.getLine(cur.line), cur.ch, getBuiltins ? getBuiltins() : null);
    if (!r) return null;
    return {
      from: CodeMirror.Pos(cur.line, r.from),
      to: cur,
      list: r.list.map(c => ({ text: c.text, displayText: c.hint ? `${c.text}  ·  ${c.hint}` : c.text }))
    };
  };

  const popup = () => cm.showHint({ hint: hintFn, completeSingle: false });

  cm.setOption("extraKeys", { ...(cm.getOption("extraKeys") || {}), "Ctrl-Space": popup });

  /* Auto-popup only where narrowing is likely wanted: a member dot or an include quote. Popping on
   * every identifier character rebuilt a several-hundred-entry list per keystroke, which is what
   * "typing feels sluggish" was; Ctrl+Space asks for the big list deliberately. */
  cm.on("inputRead", (_, change) => {
    if (cm.getOption("readOnly")) return;
    const ch = change.text[change.text.length - 1];
    if (/[."]$/.test(ch)) popup();
  });
}

window.OxIntelliSense = {
  hoverInfo, completions, attach, findByName, definitionOf, signature, macroIndex, includeTarget,
  enumTypeText, intrinsicInfo, typeInfo, semanticInfo, semanticAt, annotationAt, annotationDocs, tipHtml
};
})();
