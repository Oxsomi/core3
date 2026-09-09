/* dev/wasm_smoke.js: drives the real OxC3_wasm module through js/wasm.js, headless.
 *
 * This is the regression net for the boundary itself: the call frame, wasm64 pointer
 * marshalling, the project tree the compiler resolves #includes against, the diagnostics
 * parsed back out of the log, and every document serializer. dev/smoke.js is the other half
 * and covers the page's rendering against the mock; nothing here touches the DOM.
 *
 *   node web/dev/wasm_smoke.js [path/to/OxC3_wasm.js]
 *
 * Default module path is the Release web build (build_web.py --frontend puts a copy in
 * web/wasm/ too, which works just as well). Needs the emsdk's node, since that is the one
 * with a matching wasm64 runtime; build_web.py --run_frontend_tests picks it for you.
 */
"use strict";

const path = require("path");
const fs = require("fs");

const root = path.join(__dirname, "..", "..");
/* Resolved against the working directory, since require() only takes an absolute path or one that
 * starts with ./ and a path typed on the command line is usually neither. */
const modulePath = path.resolve(process.argv[2] ||
  path.join(root, "build", "Release", "web", "wasm64", "bin", "OxC3_wasm.js"));

if (!fs.existsSync(modulePath)) {
  console.error(`-- No module at ${modulePath} (build it with build_web.py --frontend)`);
  process.exit(1);
}

/* js/api.js is a browser script; it and the fallback it routes around are evaluated against a stub
 * window so the last section can check the routing layer, not just the boundary under it. */
global.window = global.window || { addEventListener() {} };

const OxWasm = require(path.join(__dirname, "..", "js", "wasm.js"));

for (const f of ["js/util.js", "js/mock_data.js", "js/mock.js", "js/mock_formats.js", "js/api.js", "js/tools/inspect.js"])
  new Function(fs.readFileSync(path.join(__dirname, "..", f), "utf8"))();

const OxAPI = global.window.OxAPI;

let passed = 0, failed = 0;

function assert(what, condition, detail) {
  if (condition) { ++passed; console.log(`PASS ${what}`); return true; }
  ++failed;
  console.log(`FAIL ${what}${detail == null ? "" : ` (${detail})`}`);
  return false;
}

/* A self-contained project: one compute shader, one graphics pair, and an include, so the
 * include path, the pipeline kinds and the refusal all have something real to run against. */
const PROJECT = {
  "include/shared.hlsli": { src:
`#pragma once
static const uint groupSize = 8;
struct Light { float3 color; float intensity; };
` },
  "compute.hlsl": { src:
`#include "include/shared.hlsli"
RWStructuredBuffer<float> _out;
cbuffer Settings { float scale; uint count; };
[[oxc::stage("compute")]]
[numthreads(8, 1, 1)]
void main(uint i : SV_DispatchThreadID) { _out[i] = scale * (float) (i % count); }
` },
  "graphics.hlsl": { src:
`struct Varying { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };
[[oxc::stage("vertex")]]
Varying mainVS(float3 pos : POSITION, float2 uv : TEXCOORD0) {
	Varying v;
	v.pos = float4(pos, 1);
	v.uv = uv;
	return v;
}
[[oxc::stage("pixel")]]
float4 mainPS(Varying v) : SV_TARGET { return float4(v.uv, 0, 1); }
` },
  "broken.hlsl": { src:
`[[oxc::stage("compute")]]
[numthreads(1, 1, 1)]
void main() { undeclaredThing(); }
` },
  "warns.hlsl": { src:
`#include "@resources.hlsli"
RWStructuredBuffer<F32> _out;
cbuffer Settings { F32 used; F32 neverRead; F32x3 pad0; F32 after; };
[[oxc::stage("compute")]]
[numthreads(1, 1, 1)]
void main() { F32 truncated = F32x4(1, 2, 3, 4); _out[0] = truncated + used; }
` }
};

async function main() {

  const createOxC3Module = require(modulePath);
  const info = await OxWasm.load({
    factory: createOxC3Module,
    locateFile: f => path.join(path.dirname(modulePath), f)
  });

  assert("module reports a version", /^\d+\.\d+\.\d+$/.test(info.version), info.version);
  assert("live ISA is off in wasm", info.capabilities.liveIsa === false);

  /* The offline ISA route spawns bundled tools, which a sandbox cannot, so it must report
   * false here rather than failing later at the run. */
  assert("offline ISA is off in wasm", info.capabilities.offlineIsa === false);
  assert("threads are reported", info.capabilities.threads >= 1, info.capabilities.threads);

  const builtins = await OxWasm.builtinIncludes();
  assert("builtin includes are served", builtins.length > 0 && builtins.some(b => b.name === "types.hlsli"),
    builtins.length);
  assert("a builtin carries its source", builtins.every(b => typeof b.src === "string" && b.src.length));

  /* ---- reflect-symbols (oiSR) ----------------------------------------------------------------
   *
   * Deliberately before the rest: reflection runs on source and needs nothing the sections below
   * produce, and a run of compiles before a reflect used to bring on a fault (see
   * docs/web_browser_flavor.md). Ordering it first keeps this suite measuring the boundary. */


  const symbols = await OxWasm.reflectSymbols("compute.hlsl", PROJECT);
  assert("reflect-symbols produces a document", !!symbols.doc, symbols.error);
  if (symbols.doc) {
    assert("it produces oiSR bytes carrying the magic",
      symbols.bytes && String.fromCharCode(...symbols.bytes.slice(0, 4)) === "oiSR");
    assert("nodes came back", symbols.doc.nodes.length > 0, symbols.doc.nodes.length);
    assert("the header counts match the nodes",
      symbols.doc.header.counts.nodes === symbols.doc.nodes.length);
    assert("the entrypoint is a Function node with its stage",
      symbols.doc.nodes.some(n => n.kind === "Function" && n.name === "main" && n.entry &&
        n.entry.stage === "compute"),
      JSON.stringify(symbols.doc.nodes.filter(n => n.kind === "Function").map(n => [n.name, n.entry])));
    assert("nodes carry source locations",
      symbols.doc.nodes.some(n => n.loc && n.loc.line > 0 && typeof n.loc.file === "string"));
    assert("builtin include symbols are collapsed, not inlined",
      symbols.doc.builtinCollapsed.every(b => b.count > 0));

    const srReread = await OxWasm.srRead("compute.oiSR", symbols.bytes);
    assert("an oiSR reads back with the same node count",
      srReread.nodes.length === symbols.doc.nodes.length);
  }

  /* A file being typed is a file that does not compile, so the outline has to survive one. The struct that
   * contains itself is here because walking it used to recurse until the stack died, which takes the whole
   * module with it rather than failing one call. */

  const brokenCases = [
    ["an error inside a function body", "Light",
      "RWStructuredBuffer<float> b;\nstruct Light { float3 pos; };\n[[oxc::stage(\"compute\")]]\n" +
      "[numthreads(1,1,1)]\nvoid main(uint i : SV_DispatchThreadID) { b[i] = missingValue; }\n"],
    ["a half-typed member access", "Light",
      "struct Light { float3 pos; };\nStructuredBuffer<Light> lights;\nRWStructuredBuffer<float> b;\n" +
      "[[oxc::stage(\"compute\")]]\n[numthreads(1,1,1)]\nvoid main() { b[0] = lights[0]. }\n"],
    ["a struct that contains itself", "Node",
      "struct Node { Node next; float v; };\nRWStructuredBuffer<float> b;\n[[oxc::stage(\"compute\")]]\n" +
      "[numthreads(1,1,1)]\nvoid main() { b[0] = 1; }\n"]
  ];

  for (const [label, expect, src] of brokenCases) {
    const broken = await OxWasm.reflectSymbols("broken.hlsl", { "broken.hlsl": { src } });
    assert(`a broken source still outlines: ${label}`,
      !!broken.doc && broken.doc.nodes.some(n => n.name === expect),
      broken.error || JSON.stringify((broken.doc ? broken.doc.nodes : []).map(n => n.name)));
  }

  /* ---- parse-only entrypoints (Compiler_parse) -----------------------------------------------
   *
   * The listing behind the "IntelliSense follows" picker: entrypoints and their permutations off one
   * reflection pass, no compile. Its combinations must pair with what a compile of the same source
   * stores, through the page's own matcher, or the picker could never drive the binary strip. */

  {
    const listed = await OxWasm.parseEntrypoints("compute.hlsl", PROJECT);
    assert("parse lists the compute entry",
      !!listed && listed.length === 1 && listed[0].name === "main" &&
      listed[0].stage === "compute" && listed[0].lib === false, JSON.stringify(listed));
    assert("an unannotated entry is one combination",
      listed && listed[0].combinations.length === 1, listed && listed[0].combinations.length);

    /* Uniforms are link-time values the compiler only accepts on [shader] library entries. A compute
     * [shader] entry links into binaries SPECIALIZED to its entrypoint and stage, so the listing shows
     * concrete combinations rather than lib ones, matching what the compile below stores. */

    const permProject = { "perm.hlsl": { src:
`[[oxc::extension("F64")]]
[[oxc::extension()]]
[[oxc::defines("FANCY"="1")]]
[[oxc::defines()]]
[[oxc::uniforms(F32 gain = 2.0)]]
[[oxc::uniforms(F32 gain = 3.0)]]
[shader("compute")]
[numthreads(1, 1, 1)]
void main() {}
` } };

    const perm = await OxWasm.parseEntrypoints("perm.hlsl", permProject);
    const combos = perm && perm[0] ? perm[0].combinations : [];
    assert("the entry lists as specialized compute", !!perm && perm[0].name === "main" &&
      perm[0].stage === "compute" && perm[0].lib === false, JSON.stringify(perm));
    assert("the annotation axes multiply into combinations", combos.length === 8, combos.length);
    assert("a combination spells its axes the way a compiled binary does",
      combos.some(c =>
        c.entrypoint === "main" && c.stage === "compute" &&
        c.extensions.join() === "F64" &&
        c.defines.length === 1 && c.defines[0].name === "FANCY" && c.defines[0].value === "1" &&
        c.uniforms.length === 1 && c.uniforms[0].name === "gain"),
      JSON.stringify(combos.slice(0, 2)));
    assert("the uniform rows keep distinct values",
      new Set(combos.map(c => c.uniforms[0] && c.uniforms[0].value)).size === 2,
      JSON.stringify(combos.map(c => c.uniforms)));

    const compiled = await OxWasm.compile("perm.hlsl", permProject, { targets: ["spv"] });
    if (assert("the permutation source compiles", !!compiled.doc, JSON.stringify((compiled.diags || []).slice(0, 2)))) {
      const I = global.window.OxInspect;
      const unmatched = combos.filter(c => I.matchBinary(compiled.doc, c, perm[0].name) < 0);
      assert("every combination pairs with a compiled binary", unmatched.length === 0,
        JSON.stringify(unmatched.slice(0, 2)));
    }

    /* The illegal pairing refuses through the compiler's own validation; the page's api layer turns
     * any refusal into null and keeps its previous listing, so the boundary may throw, not lie. */

    let illegal = null;
    try {
      await OxWasm.parseEntrypoints("mix.hlsl", { "mix.hlsl": { src:
`[[oxc::uniforms(F32 gain = 1)]]
[[oxc::stage("compute")]]
[numthreads(1, 1, 1)]
void main() {}
` } });
    } catch (e) { illegal = e.message; }
    assert("uniforms on a non-lib entry refuse with the compiler's reason",
      !!illegal && /uniforms/.test(illegal), illegal);

    const rt = await OxWasm.parseEntrypoints("rt.hlsl", { "rt.hlsl": { src:
`struct P { float4 c; };
[shader("miss")]
void miss(inout P p) { p.c = (float4) 0; }
` } });
    assert("a [shader] entry lists as a lib combination",
      !!rt && rt[0] && rt[0].lib === true && rt[0].combinations.length === 1 &&
      rt[0].combinations[0].lib === true && rt[0].combinations[0].entrypoint === null &&
      rt[0].combinations[0].stage === "lib",
      JSON.stringify(rt));

    const bad = await OxWasm.parseEntrypoints("bad.hlsl", { "bad.hlsl": { src: "void oops( {" } });
    assert("a source that doesn't parse answers null", bad === null, JSON.stringify(bad));
  }

  /* ---- reflection corner cases and the backend view ------------------------------------------ */

  {
    /* Every builtin include has to reflect when driven as the main file, the way the page opens
     * them read-only; @resources.hlsli is the regression, its include chain reaches back to itself. */
    const builtins = await OxWasm.builtinIncludes();
    const broken = [];
    for (const b of builtins) {
      const name = "@" + b.name;
      const r = await OxWasm.reflectSymbols(name, { [name]: { src: b.src } });
      if (!r.doc) broken.push(name + ": " + (r.error || "no doc"));
    }
    assert("every builtin include reflects as a main file", broken.length === 0, JSON.stringify(broken));

    /* Legal-but-empty buffers, and a cbuffer redefinition whose duplicate serializes childless: both
     * used to be refused by the reflector's own deserializer. */
    const odd = await OxWasm.reflectSymbols("e.hlsl", { "e.hlsl": { src:
      "cbuffer g {};\ncbuffer h { float x; };\ncbuffer h { float x; };\n" } });
    assert("an empty cbuffer and a redefined one still reflect", !!odd.doc, odd.error);

    /* The backend view: the SPIR-V leg defines __spirv__ (and vk::) the way its compile would, the
     * DXIL leg leaves them out; the page's dropdown rides on cfg.backend, defaulting to DXIL. */
    const SPV_SRC = "#ifdef __spirv__\nfloat spvOnly;\n#endif\nfloat always;\n";
    const dx = await OxWasm.reflectSymbols("b.hlsl", { "b.hlsl": { src: SPV_SRC } });
    const sv = await OxWasm.reflectSymbols("b.hlsl", { "b.hlsl": { src: SPV_SRC } }, undefined, { backend: "spirv" });
    assert("the DXIL view hides __spirv__ code",
      !!dx.doc && !dx.doc.nodes.some(n => n.name === "spvOnly") && dx.doc.nodes.some(n => n.name === "always"),
      dx.doc ? JSON.stringify(dx.doc.nodes.map(n => n.name).slice(0, 8)) : dx.error);
    assert("the SPIR-V view reflects it",
      !!sv.doc && sv.doc.nodes.some(n => n.name === "spvOnly"),
      sv.doc ? JSON.stringify(sv.doc.nodes.map(n => n.name).slice(0, 8)) : sv.error);
  }

  /* Source to disassembly mapping: with -Zi on, both backends' debug info names the page's own file,
   * uniforms preamble and all (a bare #line 1 used to leave SPIRV attributed to "Spec constants"). */

  {
    new Function(require("fs").readFileSync(require("path").join(__dirname, "..", "js", "util.js"), "utf8")).call(global);
    new Function(require("fs").readFileSync(require("path").join(__dirname, "..", "js", "asmmap.js"), "utf8")).call(global);
    /* The uniforms annotation matters here: it injects the spec-constants preamble whose bare #line 1
     * used to leave every SPIRV line attributed to "Spec constants (SPIRV)" instead of the file. */
    const mapped = {
      "mapped.hlsl": { src:
        '#include "include/shared.hlsli"\n' +
        "RWStructuredBuffer<float> _out;\n" +
        '[[oxc::uniforms(F32 gain = 2.0)]]\n' +
        '[shader("compute")]\n' +
        "[numthreads(8, 1, 1)]\n" +
        "void main(uint i : SV_DispatchThreadID) {\n" +
        "    float a = (float) i * 3;\n" +
        "    float b = a * a + 1;\n" +
        "    _out[i] = b;\n" +
        "}\n" },
      "include/shared.hlsli": PROJECT["include/shared.hlsli"]
    };
    const dbg = await OxWasm.compile("mapped.hlsl", mapped, { targets: ["spv", "dxil"], debug: true });
    assert("a -Zi compile still succeeds", !!dbg.doc, JSON.stringify((dbg.diags || []).slice(0, 2)));
    for (const backend of ["spirv", "dxil"]) {
      const idx = dbg.doc.binaries.findIndex(b => b.sizes[backend] > 0);
      const text = await OxWasm.disassemble(backend, await OxWasm.shExtractBinary(dbg.bytes, idx, backend));
      const parsed = global.window.OxAsmMap.parse(text, backend);
      const own = parsed.toSrc.filter(m => m && m.file === "mapped.hlsl").length;
      assert(`${backend} disassembly maps back to the source file`, own > 3,
        backend + ": own=" + own + " total=" + parsed.toSrc.filter(Boolean).length +
        " files=" + [...new Set(parsed.toSrc.filter(Boolean).map(m => m.file))].join("|"));
    }

    /* --no-opt (-Od) exists so the optimizer stops folding what the line info describes: the same
     * debug compile with it on has to map at least as many distinct source lines, usually far more. */

    const noopt = await OxWasm.compile("mapped.hlsl", mapped, { targets: ["spv"], debug: true, noOpt: true });
    assert("--no-opt still compiles", !!noopt.doc, JSON.stringify((noopt.diags || []).slice(0, 2)));

    const density = async r => {
      const idx = r.doc.binaries.findIndex(b => b.sizes.spirv > 0);
      const text = await OxWasm.disassemble("spirv", await OxWasm.shExtractBinary(r.bytes, idx, "spirv"));
      const parsed = global.window.OxAsmMap.parse(text, "spirv");
      return new Set(parsed.toSrc.filter(m => m && m.file === "mapped.hlsl").map(m => m.line)).size;
    };
    const dOpt = await density(dbg), dNoOpt = await density(noopt);
    assert("--no-opt maps at least as many source lines", dNoOpt >= dOpt && dNoOpt > 0,
      `opt=${dOpt} noOpt=${dNoOpt}`);
    console.log(`   (line coverage: ${dOpt} distinct lines optimized, ${dNoOpt} with --no-opt)`);
  }

  /* The syntax reference's vocabularies come off the oiSH enums, so a rename there shows up here
   * rather than as a silently stale list on the page (it drifted twice while hand-written). */

  const enums = await OxWasm.annotationEnums();
  assert("annotation enums list every extension", enums.extensions.length >= 27, enums.extensions.length);
  assert("including the ones the old hand list missed",
    enums.extensions.includes("Barycentrics") && enums.extensions.includes("SubgroupQuad"),
    JSON.stringify(enums.extensions));
  assert("and not the sunset ones", !enums.extensions.includes("RayMotionBlur"));
  assert("vendors ride along", enums.vendors.includes("NV") && enums.vendors.includes("AMD"),
    JSON.stringify(enums.vendors));

  /* dormantExtensions can carry detected bits outside the declared set; the page must only ever see
   * declared ones or it prints phantom dormant lists (it did). */

  {
    const dhSrc = '#include "@types.hlsli"\n' +
      '[[oxc::extension("DescriptorHeap")]]\n[[oxc::model("6.6")]]\n[[oxc::stage("compute")]]\n' +
      "[numthreads(1,1,1)]\nvoid main(uint i : SV_DispatchThreadID) {\n" +
      "    Texture2D<float4> t = ResourceDescriptorHeap[0];\n" +
      "    RWStructuredBuffer<float> o = ResourceDescriptorHeap[1];\n" +
      "    o[i] = t.Load(int3(0, 0, 0)).x;\n}\n";
    const dh = await OxWasm.compile("dh.hlsl", { "dh.hlsl": { src: dhSrc } }, { targets: ["dxil"] });
    assert("a descriptor-heap shader compiles", !!dh.doc, JSON.stringify((dh.diags || []).slice(0, 2)));
    const bad = dh.doc.binaries.flatMap(b => (b.dormant || []).filter(d => !b.extensions.includes(d)));
    assert("dormant never lists an undeclared extension", bad.length === 0, JSON.stringify(bad));
  }

  /* An interface's body-less method is merged out of the reflector's function list; the fork's
   * GetFunctionParameter must resolve indices through the same merged list as GetFunctionDesc, or
   * every parameter reflected after an interface loses its type (it did: hover showed
   * `id : SV_DispatchThreadID` with no U32). */

  {
    const src = '#include "@types.hlsli"\n' +
      "interface IArea { F32 area(); };\n" +
      "struct Rect : IArea { F32 s; F32 area() { return s; } };\n" +
      '[[oxc::stage("compute")]]\n[numthreads(1, 1, 1)]\nvoid main(U32 id : SV_DispatchThreadID) { }\n';
    const r = await OxWasm.reflectSymbols("iface.hlsl", { "iface.hlsl": { src } });
    const id = r.doc.nodes.find(n => n.name === "id" && !n.builtin);
    assert("a parameter after an interface keeps its type",
      !!(id && id.type) && (id.type.display || id.type.name) === "U32",
      JSON.stringify(id && id.type));
    const ret = r.doc.nodes.find((n, i, a) => n.name === "(return)" && !n.builtin);
    assert("a struct method's return after an interface keeps its type",
      !!(ret && ret.type) && (ret.type.display || ret.type.name) === "F32",
      JSON.stringify(ret && ret.type));

    /* The interface's own body-less method is a real function to reflection: never defined, so never
     * merged away, and its return slot carries the declared type. */
    /* Enumerator values ride on the node ({value, type}), what hover prints as `= 2 (U32)`. */
    const esrc = '#include "@types.hlsli"\nenum class EMode : U32 { Off = 0, Fast = 5 };\n' +
      '[[oxc::stage("compute")]]\n[numthreads(1, 1, 1)]\nvoid main() { }\n';
    const er = await OxWasm.reflectSymbols("e.hlsl", { "e.hlsl": { src: esrc } });
    const fast = er.doc.nodes.find(n => !n.builtin && n.kind === "EnumValue" && n.name === "Fast");
    assert("an enumerator carries its value and underlying type",
      !!(fast && fast.enumValue) && fast.enumValue.value === 5 && !!fast.enumValue.type,
      JSON.stringify(fast && (fast.enumValue || "no enumValue field")));

    const iarea = r.doc.nodes.find(n => !n.builtin && n.kind === "Interface");
    const decl = r.doc.nodes.find(n => !n.builtin && n.kind === "Function" && n.parent === iarea.id);
    const declRet = r.doc.nodes.find(n => n.parent === decl.id && n.name === "(return)");
    assert("the interface method's own return type is reflected",
      !!(declRet && declRet.type) && (declRet.type.display || declRet.type.name) === "F32",
      JSON.stringify({ children: r.doc.nodes.filter(n => n.parent === decl.id).map(n => [n.name, n.type && n.type.display]) }));
  }

  /* An enumerator's value travels with its node along with the enum's underlying type, which is what the
   * hover and the tree print after the name; every other node says null. */

  {
    const src = '#include "@types.hlsli"\n' +
      "enum class Mode : U32 { Off = 0, Linear = 1, Cubic = 5 };\n" +
      '[[oxc::stage("compute")]]\n[numthreads(1, 1, 1)]\nvoid main(U32 id : SV_DispatchThreadID) { }\n';
    const r = await OxWasm.reflectSymbols("enums.hlsl", { "enums.hlsl": { src } });
    const mode = r.doc && r.doc.nodes.find(n => !n.builtin && n.kind === "Enum" && n.name === "Mode");
    const values = mode ? r.doc.nodes.filter(n => n.parent === mode.id && n.kind === "EnumValue") : [];
    assert("an enum's enumerators carry their values",
      JSON.stringify(values.map(n => [n.name, n.enumValue && n.enumValue.value])) ===
        JSON.stringify([["Off", 0], ["Linear", 1], ["Cubic", 5]]),
      JSON.stringify(values.map(n => [n.name, n.enumValue])));
    assert("with the enum's underlying type",
      values.length > 0 && values.every(n => n.enumValue && n.enumValue.type === "uint"),
      JSON.stringify(values.map(n => n.enumValue)));
    assert("and nothing else carries one",
      !!r.doc && r.doc.nodes.every(n => n.kind === "EnumValue" || n.enumValue === null),
      r.doc && JSON.stringify(r.doc.nodes.filter(n => n.kind !== "EnumValue" && n.enumValue).map(n => n.name)));
  }

  /* The reflect parse follows a chosen permutation: masking an extension out makes its types
   * diagnose (16bit off = F16 unavailable), and a $define flips preprocessor branches. */

  {
    const src = '#include "@types.hlsli"\nF16 pack(F16 v) { return v; }\n' +
      '[[oxc::stage("compute")]]\n[numthreads(1, 1, 1)]\nvoid main() {\n' +
      "#ifdef $FANCY\n    F32 x = 2;\n#else\n    F32 y = 3;\n#endif\n}\n";
    const files = { "cfg.hlsl": { src } };

    const no16 = await OxWasm.reflectSymbols("cfg.hlsl", files, undefined, { disabledExt: 1 << 2 });
    assert("masking 16BitTypes out makes F16 diagnose", (no16.diags || []).length >= 1,
      JSON.stringify(no16.diags));

    const fancy = await OxWasm.reflectSymbols("cfg.hlsl", files, undefined, { defines: { FANCY: "" } });
    const names = fancy.doc.nodes.filter(n => !n.builtin).map(n => n.name);
    assert("a followed define flips the preprocessor branch", names.includes("x") && !names.includes("y"),
      names.join(","));

    /* A uniform is a $$NAME macro in the compile, so following a binary's uniform value has to define
     * that spelling, not the single-$ define one, or the guarded body never outlines. */
    const uniFiles = { "uni.hlsl": { src: "RWStructuredBuffer<float> b;\n[shader(\"compute\")]\n[numthreads(1,1,1)]\n" +
      "void main() {\n#ifdef $$FLAG\n    if ($$FLAG) { float inner = 1; b[0] = inner; }\n#endif\n    b[1] = 2;\n}\n" } };
    const uni = await OxWasm.reflectSymbols("uni.hlsl", uniFiles, undefined, { defines: { "$$FLAG": "true" } });
    const uniNames = (uni.doc ? uni.doc.nodes : []).map(n => n.name);
    assert("a followed uniform defines its $$ spelling, so the guarded body outlines", uniNames.includes("inner"),
      uni.error || JSON.stringify(uniNames));
  }

  /* --keep-registers must survive the lib+link path a [shader] entry takes: the DXIL linker's own
   * codegen pass drops unused resources unless told what the per-library compile was told (it did:
   * this listed [] before the flag was threaded into IDxcLinker::Link). */

  {
    const src = '#include "@types.hlsli"\nTexture2D<F32x4> _unusedTex;\nSamplerState _unusedSmp;\n' +
      '[shader("compute")]\n[numthreads(1, 1, 1)]\nvoid main() { }\n';
    const r = await OxWasm.compile("keep.hlsl", { "keep.hlsl": { src } }, { targets: ["dxil"], keepRegisters: true });
    const regs = r.doc ? (r.doc.binaries[0].registers || []).map(x => x.name) : [];
    assert("--keep-registers holds through the [shader] link path",
      regs.includes("_unusedTex") && regs.includes("_unusedSmp"), JSON.stringify(regs));
  }

  /* A misspelled [[oxc::...]] annotation must refuse, not silently parse as nothing: `extensions`
   * (plural) used to no-op, and the permutation the author asked for quietly never existed. */

  for (const typo of ['[[oxc::extensions("16BitTypes")]]', '[[oxc::define("X")]]']) {
    const src = '#include "@types.hlsli"\n' + typo + '\n' +
      '[[oxc::stage("compute")]]\n[numthreads(1, 1, 1)]\nvoid main() { }\n';
    const r = await OxWasm.compile("typo.hlsl", { "typo.hlsl": { src } }, { targets: ["dxil"] });
    assert(`a misspelled oxc annotation refuses the compile: ${typo.slice(2, 18)}`,
      !r.doc && (r.diags || []).some(d => /unrecognized.*oxc/.test(d.msg)),
      JSON.stringify({ doc: !!r.doc, diags: (r.diags || []).slice(0, 1) }));
  }

  /* A multi-entry lib reports each register once per FUNCTION that binds it, and keep-all reports
   * every register under every function; the collector merges identical declarations instead of
   * refusing the file (it refused: SHFile_detectDuplicate on the user's trace.hlsl + keep). The
   * shared-resource case guards the pre-existing latent form of the same bug. */

  {
    const src = '#include "@types.hlsli"\n' +
      "struct P { F32x3 c; };\nRaytracingAccelerationStructure _tlas;\nRWTexture2D<F32x4> _shared;\nTexture2D _unused;\n" +
      '[shader("miss")]\nvoid miss(inout P p) { _shared[U32x2(0, 0)] = 0; p.c = 0; }\n' +
      '[shader("raygeneration")]\nvoid rgen() {\n' +
      "    RayDesc r; r.Origin = 0; r.Direction = F32x3(0, 0, 1); r.TMin = 0; r.TMax = 1;\n" +
      "    P p = (P)0; TraceRay(_tlas, RAY_FLAG_NONE, 0xFF, 0, 0, 0, r, p);\n" +
      "    _shared[DispatchRaysIndex().xy] = F32x4(p.c, 1);\n}\n";
    const files = { "rt.hlsl": { src } };

    const kept = await OxWasm.compile("rt.hlsl", files, { targets: ["dxil"], keepRegisters: true });
    const keptRegs = kept.doc ? (kept.doc.binaries[0].registers || []).map(x => x.name) : [];
    assert("a multi-entry lib with keep-registers compiles and keeps the unused register",
      !!kept.doc && keptRegs.includes("_unused") && keptRegs.includes("_shared"),
      kept.doc ? JSON.stringify(keptRegs) : JSON.stringify((kept.diags || []).slice(0, 1)));

    const plain = await OxWasm.compile("rt.hlsl", files, { targets: ["dxil"] });
    assert("two entries sharing a register still compile without keep",
      !!plain.doc, JSON.stringify((plain.diags || []).slice(0, 1)));
  }

  /* The reflection of half follows the 16bit language option: with 16BitTypes masked off, half is the
   * min-precision float the codegen would actually widen it to, not float16_t. */

  {
    const src = '#include "@types.hlsli"\nhalf g;\n[[oxc::stage("compute")]]\n[numthreads(1, 1, 1)]\nvoid main() { }\n';
    const files = { "half.hlsl": { src } };
    const on = await OxWasm.reflectSymbols("half.hlsl", files);
    const off = await OxWasm.reflectSymbols("half.hlsl", files, undefined, { disabledExt: 1 << 2 });
    const ty = r => { const g = r.doc.nodes.find(n => !n.builtin && n.name === "g"); return g && g.type && g.type.name; };
    assert("half is float16_t while 16BitTypes is on", ty(on) === "float16_t", ty(on));
    assert("and plain float once 16BitTypes is masked off", ty(off) === "float", ty(off));
  }

  /* Field vocabularies straight off the enum name tables, for the pipeline editors. */

  {
    const v = await OxWasm.spFieldVocab();
    assert("field vocab lists the compare ops in enum order", v.ECompareOp && v.ECompareOp[0] === "Gt"
      && v.ECompareOp.length === 8, JSON.stringify(v.ECompareOp));
    assert("blend factors are all present", v.EBlend && v.EBlend.length === 19, v.EBlend && v.EBlend.length);
    assert("texture formats ride along for rtv.format", v.ETextureFormatId && v.ETextureFormatId.length > 20,
      v.ETextureFormatId && v.ETextureFormatId.length);
    assert("a reserved mask bit travels as null", v.EPipelineRaytracingFlags && v.EPipelineRaytracingFlags[2] === null,
      JSON.stringify(v.EPipelineRaytracingFlags));
    assert("depth formats have their own list", v.EDepthStencilFormat && v.EDepthStencilFormat[1] === "D16"
      && v.EDepthStencilFormat.length === 6, JSON.stringify(v.EDepthStencilFormat));
    const color = v.ETextureFormatIdColor || [];
    assert("the color-target subset drops Undefined and every compressed format",
      color.length === v.ETextureFormatId.length && color[0] === null && color.includes("RGBA8")
      && color.filter(x => x != null).length < v.ETextureFormatId.length - 1,
      color.filter(x => x != null).length + " of " + v.ETextureFormatId.length);
  }

  /* Mesh pipelines derive like any graphics pipeline: the mesh stage replaces the vertex chain, so
   * there is no vertex layout and no input-assembly topology to specialize. */

  {
    const src = fs.readFileSync(path.join(root, "web", "samples", "mesh.hlsl"), "utf8");
    const r = await OxWasm.compile("mesh.hlsl", { "mesh.hlsl": { src } }, { targets: ["spv", "dxil"] });
    assert("the mesh sample compiles", !!r.doc, JSON.stringify((r.diags || []).slice(0, 1)));

    const sp = await OxWasm.spDerive(r.bytes, "mesh.oiSH", []);
    assert("a mesh+pixel pipeline derives instead of refusing", !sp.refused && sp.doc,
      sp.refused || "no doc");
    const fields = sp.doc.pipelines[0].fields.map(f => f.field);
    assert("with no vertex layout or topology to specialize",
      !fields.includes("topology") && !fields.includes("vertex.stride"),
      JSON.stringify(fields));
    assert("and the mesh stage bound", sp.doc.pipelines[0].stages.some(s => s.stage === "mesh"),
      JSON.stringify(sp.doc.pipelines[0].stages.map(s => s.stage)));
  }

  /* Arbitrary SPIR-V is judged by spirv-val before anything trusts it; the verdict carries the
   * validator's own message. */

  {
    const asmText = "OpCapability Shader\nOpMemoryModel Logical GLSL450\nOpEntryPoint GLCompute %m \"main\"\n" +
      "OpExecutionMode %m LocalSize 1 1 1\n%v = OpTypeVoid\n%f = OpTypeFunction %v\n" +
      "%m = OpFunction %v None %f\n%l = OpLabel\nOpReturn\nOpFunctionEnd\n";
    const good = await OxWasm.assemble("spirv", asmText);
    const ok = await OxWasm.validate("spirv", good);
    assert("a valid module validates", ok.valid === true, JSON.stringify(ok));

    const bad = new Uint8Array(good);
    bad[20] = 0xFF;                     //corrupt an instruction word past the header
    const verdict = await OxWasm.validate("spirv", bad);
    assert("a corrupted module is refused with the validator's reason",
      verdict.valid === false && (verdict.message || "").length > 0, JSON.stringify(verdict).slice(0, 160));
  }

  /* Project snapshot: the working tree as a real .oiCA and back, byte-honest both ways. This is the
   * share vehicle for projects too big for a URL, so the roundtrip has to preserve every file. */

  {
    const packed = await OxWasm.caPack(PROJECT);
    assert("a project packs into an oiCA", packed && packed.length > 64, packed && packed.length);
    assert("with the oiCA magic", String.fromCharCode(...packed.slice(0, 4)) === "oiCA",
      String.fromCharCode(...(packed || []).slice(0, 4)));

    const files = await OxWasm.caUnpack(packed);
    const want = Object.keys(PROJECT).sort();
    assert("and unpacks to the same file set", JSON.stringify(Object.keys(files).sort()) === JSON.stringify(want),
      JSON.stringify(Object.keys(files)));
    assert("with identical content", want.every(n => files[n] === PROJECT[n].src),
      want.find(n => files[n] !== PROJECT[n].src));
  }

  /* The squiggle contract: reflection is what runs between keystrokes, so its diagnostics are the only
   * ones a file being typed gets. The tree AND the located diagnostic have to come back from one call. */

  const sqSrc = "RWStructuredBuffer<float> b;\n[[oxc::stage(\"compute\")]]\n[numthreads(1,1,1)]\n" +
    "void main(uint i : SV_DispatchThreadID) { b[i] = missingValue; }\n";
  const sq = await OxWasm.reflectSymbols("typing.hlsl", { "typing.hlsl": { src: sqSrc } });
  assert("a mid-edit reflect still returns the tree", !!sq.doc && sq.doc.nodes.length > 0, sq.error);
  assert("and the diagnostic that says what is wrong",
    sq.diags.some(d => d.sev === "error" && d.file === "typing.hlsl" && d.line === 4 &&
      d.msg.includes("missingValue")),
    JSON.stringify(sq.diags));

  /* An include is reflected as the main file because that is the only way to parse one alone, and DXC
   * warns about its #pragma once every time. That warning describes the mechanism, not the code, so it
   * is dropped for a .hlsli; in a real main file it stays, because there it is a real mistake. */

  const onceSrc = "#pragma once\nstruct Light { float3 pos; };\n";
  const asInclude = await OxWasm.reflectSymbols("a.hlsli", { "a.hlsli": { src: onceSrc } });
  assert("an include's #pragma once is not warned about",
    !!asInclude.doc && asInclude.diags.length === 0, JSON.stringify(asInclude.diags));
  const asMain = await OxWasm.reflectSymbols("a.hlsl", { "a.hlsl": { src: onceSrc } });
  assert("a main file's #pragma once still is",
    asMain.diags.some(d => /#pragma once in main file/.test(d.msg)), JSON.stringify(asMain.diags));

  /* The module has to stay usable after all of that, since the page reflects on every pause in typing. */

  const afterBroken = await OxWasm.reflectSymbols("compute.hlsl", PROJECT);
  assert("the module still reflects a good source afterwards",
    !!afterBroken.doc && afterBroken.doc.nodes.length > 0, afterBroken.error);

  /* ---- compile ------------------------------------------------------------------------- */

  const compute = await OxWasm.compile("compute.hlsl", PROJECT, { targets: ["spv", "dxil"] });
  assert("compute compiles", !!compute.doc, compute.diags.map(d => d.msg).join(" | "));
  if (!compute.doc) { report(); return; }

  assert("compile produces oiSH bytes", compute.bytes && compute.bytes.length > 0);
  assert("oiSH bytes carry the magic",
    String.fromCharCode(...compute.bytes.slice(0, 4)) === "oiSH");
  assert("one entry, named main", compute.doc.entries.length === 1 && compute.doc.entries[0].name === "main");
  assert("entry is compute with its group size",
    compute.doc.entries[0].stage === "compute" &&
    JSON.stringify(compute.doc.entries[0].group) === JSON.stringify([8, 1, 1]));
  assert("both backends are present",
    compute.doc.binaries.some(b => b.sizes.spirv > 0) && compute.doc.binaries.some(b => b.sizes.dxil > 0));
  assert("the include is recorded with its crc",
    compute.doc.includes.some(i => /shared\.hlsli$/.test(i.path) && i.crc32c !== 0),
    JSON.stringify(compute.doc.includes));
  assert("registers are reflected", compute.doc.registers.length >= 2, compute.doc.registers.length);

  const cbuffer = compute.doc.registers.find(r => r.cls === "CBV");
  assert("the cbuffer reports its layout", !!cbuffer && !!cbuffer.buffer && cbuffer.buffer.vars.length === 2,
    cbuffer && cbuffer.buffer ? cbuffer.buffer.vars.length : "no buffer");
  assert("a layout variable carries offset, type and use",
    !!cbuffer && cbuffer.buffer.vars.every(v => typeof v.offset === "number" && typeof v.type === "string" &&
      typeof v.used.spirv === "boolean"));

  const uav = compute.doc.registers.find(r => r.cls === "UAV");
  assert("the UAV binds on both backends",
    !!uav && !!uav.bindings.spirv && !!uav.bindings.dxil && uav.bindings.dxil.letter === "u",
    uav ? JSON.stringify(uav.bindings) : "no UAV");

  /* ---- a compile that fails ------------------------------------------------------------- */

  const broken = await OxWasm.compile("broken.hlsl", PROJECT, { targets: ["spv"] });
  assert("a broken shader fails rather than throwing", broken.doc === null);
  assert("its diagnostics come back", broken.diags.some(d => d.sev === "error"),
    JSON.stringify(broken.diags));
  assert("a diagnostic points at the source",
    broken.diags.some(d => d.file === "broken.hlsl" && d.line > 0),
    JSON.stringify(broken.diags));

  /* The log around a failure narrates: a stacktrace, the error record's fields, "Compile failed" per
   * stage. Each used to surface as one more error on line 1, so a single mistake read as six. */

  const plumbing = /Stacktrace|wasm:\/\/|^\[\d|sub id:|Platform\/std|failed for file|^One of the previous/;
  assert("the plumbing around a failure is not presented as diagnostics",
    broken.diags.every(d => !plumbing.test(d.msg)),
    JSON.stringify(broken.diags.filter(d => plumbing.test(d.msg)).map(d => d.msg)));

  /* ---- warnings ---------------------------------------------------------------------------- */

  /* A warning is not a failure: the document comes back and the diagnostic comes with it.
   * DXC's own carry a location, which is what makes them clickable in the editor. */

  const warned = await OxWasm.compile("warns.hlsl", PROJECT, { targets: ["spv"] });
  assert("a shader that only warns still compiles", !!warned.doc,
    warned.diags.map(d => d.msg).join(" | "));
  assert("its warning comes back as a warning, not an error",
    warned.diags.length > 0 && warned.diags.every(d => d.sev === "warn"),
    JSON.stringify(warned.diags));
  assert("a warning points at the line it is about",
    warned.diags.some(d => d.file === "warns.hlsl" && d.line > 1),
    JSON.stringify(warned.diags));

  /* The compiler's own extra warnings (Compiler_handleExtraWarnings) name a binary and a register
   * rather than a source location, and only exist when the --warn-* flags ask for them.
   * --keep-registers is what keeps a declared-but-unused register bound long enough to warn about. */

  const quiet = await OxWasm.compile("warns.hlsl", PROJECT, { targets: ["spv"], keepRegisters: true });
  const extra = await OxWasm.compile("warns.hlsl", PROJECT, {
    targets: ["spv"], keepRegisters: true,
    warnUnusedRegisters: true, warnUnusedConstants: true, warnBufferPadding: true
  });

  assert("the --warn-* flags reach the compiler", extra.diags.length > quiet.diags.length,
    `${extra.diags.length} with them, ${quiet.diags.length} without`);
  assert("an unused constant is reported", extra.diags.some(d => /unused constant/.test(d.msg)));
  assert("buffer padding is reported", extra.diags.some(d => /padding/.test(d.msg)));
  assert("a warning with no location is kept rather than dropped",
    extra.diags.some(d => /unused constant/.test(d.msg) && d.sev === "warn" && d.line === 1));

  /* ---- read back, write out, combine ----------------------------------------------------- */

  const reread = await OxWasm.shRead("compute.oiSH", compute.bytes);
  assert("a written oiSH reads back identically",
    JSON.stringify(reread.entries) === JSON.stringify(compute.doc.entries));

  const header = await OxWasm.fileHeader(compute.bytes);
  assert("file header sniffs oiSH", header.format === "oiSH" && !!header.document);

  const spvOnly = await OxWasm.compile("compute.hlsl", PROJECT, { targets: ["spv"] });
  const dxilOnly = await OxWasm.compile("compute.hlsl", PROJECT, { targets: ["dxil"] });
  assert("single backend compiles produce lean files",
    !!spvOnly.doc && !!dxilOnly.doc &&
    spvOnly.doc.binaries.every(b => b.sizes.dxil === 0) &&
    dxilOnly.doc.binaries.every(b => b.sizes.spirv === 0));

  const combined = await OxWasm.shCombine("combined.oiSH", spvOnly.bytes, dxilOnly.bytes);
  assert("combine unions the two backends",
    combined.doc.binaries.some(b => b.sizes.spirv > 0) && combined.doc.binaries.some(b => b.sizes.dxil > 0));

  let combineRefused = false;
  try { await OxWasm.shCombine("bad.oiSH", spvOnly.bytes, (await OxWasm.compile("graphics.hlsl", PROJECT, { targets: ["spv"] })).bytes); }
  catch (e) { combineRefused = true; }
  assert("combine refuses two different sources", combineRefused);

  /* ---- binaries: extract, disassemble, assemble ------------------------------------------- */

  const spvIndex = compute.doc.binaries.findIndex(b => b.sizes.spirv > 0);
  const spirv = await OxWasm.shExtractBinary(compute.bytes, spvIndex, "spirv");
  assert("a stored SPIR-V binary extracts",
    spirv && spirv.length === compute.doc.binaries[spvIndex].sizes.spirv, spirv && spirv.length);
  assert("it carries the SPIR-V magic",
    spirv[0] === 0x03 && spirv[1] === 0x02 && spirv[2] === 0x23 && spirv[3] === 0x07);

  const disassembly = await OxWasm.disassemble("spirv", spirv);
  assert("SPIR-V disassembles", /OpEntryPoint/.test(disassembly), disassembly.slice(0, 80));

  const reassembled = await OxWasm.assemble("spirv", disassembly);
  assert("the disassembly assembles back", reassembled && reassembled.length > 0);
  assert("and keeps the module's SPIR-V version (validation rules differ per version)",
    reassembled[5] === spirv[5] && reassembled[6] === spirv[6],
    "was " + spirv[6] + "." + spirv[5] + ", got " + reassembled[6] + "." + reassembled[5]);

  const dxilIndex = compute.doc.binaries.findIndex(b => b.sizes.dxil > 0);
  const dxil = await OxWasm.shExtractBinary(compute.bytes, dxilIndex, "dxil");
  assert("the DXIL validator accepts a stored container", (await OxWasm.validate("dxil", dxil)).valid);

  const dxilBack = await OxWasm.assemble("dxil", await OxWasm.disassemble("dxil", dxil));
  assert("DXIL LL text assembles back into a container the validator accepts",
    dxilBack && dxilBack.length > 0x14 && (await OxWasm.validate("dxil", dxilBack)).valid);

  /* The first part offset pointed past the end: a container the validator has to refuse. */
  const brokenDxil = dxil.slice(); brokenDxil.fill(0xff, 32, 36);
  const dxilVerdict = await OxWasm.validate("dxil", brokenDxil);
  assert("and a broken container is refused with the validator's own reason",
    !dxilVerdict.valid && /\S/.test(dxilVerdict.message || ""), dxilVerdict.message);

  /* A sound part table with the DXIL part zeroed: this verdict is DXC's validator's own. */
  const zeroedDxil = dxil.slice();
  const dv = new DataView(zeroedDxil.buffer);
  for (let i = 0, n = dv.getUint32(28, true); i < n; i++) {
    const off = dv.getUint32(32 + 4 * i, true);
    if (String.fromCharCode(...zeroedDxil.subarray(off, off + 4)) === "DXIL")
      zeroedDxil.fill(0, off + 8, off + 8 + dv.getUint32(off + 4, true));
  }
  const zeroedVerdict = await OxWasm.validate("dxil", zeroedDxil);
  assert("and DXC's validator itself refuses a zeroed DXIL part",
    !zeroedVerdict.valid && /\S/.test(zeroedVerdict.message || ""), zeroedVerdict.message);

  let dxilAssemblyRefused = false;
  try { await OxWasm.assemble("dxil", "; nothing"); } catch (e) { dxilAssemblyRefused = true; }
  assert("DXIL assembly of garbage is refused", dxilAssemblyRefused);

  /* ---- vocabularies: what the page and the mock tier reason with, straight off the compiler ------- */

  {
    const v = await OxWasm.annotationEnums();
    const byName = Object.fromEntries((v.stages || []).map(s => [s.name, s]));
    assert("the vocabularies carry every stage with its library flag and profile",
      byName.compute && byName.compute.lib === false && byName.compute.profile === "cs" &&
      byName.raygeneration && byName.raygeneration.lib === true && byName.raygeneration.profile === "lib",
      JSON.stringify(v.stages));
    assert("and the backend masks", Array.isArray(v.extensionsNoDxil) && v.extensionsNoDxil.includes("AtomicF32") &&
      Array.isArray(v.extensionsNoSpirv) && v.extensionsNoSpirv.includes("MeshTaskTexDeriv"),
      JSON.stringify([v.extensionsNoDxil, v.extensionsNoSpirv]));
    assert("and the compiler's version", v.version && Number.isInteger(v.version.major) && v.version.major >= 3,
      JSON.stringify(v.version));
    assert("and the shader model range with each extension's floor",
      v.shaderModels && v.shaderModels.min === "6.5" && /^6\.\d+$/.test(v.shaderModels.max) &&
      v.extensionMinModel && v.extensionMinModel.CoopVec === "6.10",
      JSON.stringify([v.shaderModels, v.extensionMinModel]));
  }

  /* ---- pipelines (oiSP) ---------------------------------------------------------------------- */

  const computePipeline = await OxWasm.spDerive(compute.bytes, "compute.oiSH", []);
  assert("a compute pipeline derives", !!computePipeline.doc, JSON.stringify(computePipeline.refused));
  if (computePipeline.doc) {
    const p = computePipeline.doc.pipelines[0];
    assert("it is a compute pipeline with one stage", p.type === "compute" && p.stages.length === 1);

    /* A derived pipeline carries its descriptor layout as an embedded oiPL: the compute shader binds
     * registers, so the layout has rows, each proven by reflection and naming both backends' pair. */
    const layout = computePipeline.doc.layouts && computePipeline.doc.layouts[p.layoutIndex];
    assert("its descriptor layout is embedded", p.layoutIndex >= 0 && !!layout, JSON.stringify(p.layoutIndex));
    assert("with reflected rows that name both bindings",
      !!layout && layout.bindings.length > 0 && layout.bindings.every(b =>
        b.source === "derived" && typeof b.bindings.spirv.binding === "number" && typeof b.bindings.dxil.binding === "number"),
      layout && JSON.stringify(layout.bindings.slice(0, 2)));
    assert("compute is exact: nothing is assumed", p.fields.every(f => f.source !== "assumed"),
      JSON.stringify(p.fields));
    assert("the stage names the shader it came from", p.stages[0].shaderFile === "compute.oiSH");
    assert("oiSP bytes carry the magic",
      String.fromCharCode(...computePipeline.bytes.slice(0, 4)) === "oiSP");

    const print = await OxWasm.spPrint(computePipeline.bytes, 0);
    assert("the pipeline prints the way file data prints it", /Pipeline state \(compute\)/.test(print), print);

    const spReread = await OxWasm.spRead("compute.oiSP", computePipeline.bytes);
    assert("an oiSP reads back with the same pipeline", spReread.pipelines.length === 1);
  }

  const graphics = await OxWasm.compile("graphics.hlsl", PROJECT, { targets: ["spv"] });
  assert("the graphics pair compiles", !!graphics.doc, graphics.diags.map(d => d.msg).join(" | "));

  if (graphics.doc) {
    const gfxPipeline = await OxWasm.spDerive(graphics.bytes, "graphics.oiSH", []);
    assert("a graphics pipeline derives", !!gfxPipeline.doc, JSON.stringify(gfxPipeline.refused));
    if (gfxPipeline.doc) {
      const p = gfxPipeline.doc.pipelines[0];
      assert("it binds both stages", p.type === "graphics" && p.stages.length === 2);
      assert("it reports assumed fields, since no signature carries them",
        p.fields.some(f => f.source === "assumed"));
      assert("every field states why and what is legal",
        p.fields.every(f => f.reason && f.reason.length && f.domain && f.domain.length));

      /* Supplying a field is what -pso-set does, and the field stops being assumed. */
      const target = p.fields.find(f => f.source === "assumed");
      const pathName = target.field + (target.indexed ? `[${target.index}]` : "");
      const supplied = await OxWasm.spSupply(gfxPipeline.bytes, 0, pathName, target.value, "graphics.oiSP");
      const after = supplied.doc.pipelines[0].fields.find(
        f => f.field === target.field && f.index === target.index
      );
      assert("supplying a field marks it supplied", after && after.source === "supplied",
        after && after.source);
    }
  }

  /* ---- ISA ------------------------------------------------------------------------------------ */

  const offline = await OxWasm.isaHasOfflinePath(spirv);
  assert("a compute module reports an offline ISA path", offline === true);

  let isaRefused = false;
  try { await OxWasm.isaTargets(); } catch (e) { isaRefused = /process/i.test(e.message); }
  assert("the ISA target list refuses without process spawning, naming why", isaRefused);

  /* ---- the routing layer above it (js/api.js) ---------------------------------------------------- */

  const routed = await OxAPI.init({ factory: () => Promise.resolve(null) });
  assert("api routes to the module once it's loaded", routed.backend === "wasm", routed.error);
  assert("api reports the compiler's version", routed.version === info.version);
  assert("api names what is still fabricated", OxAPI.stubs.length > 0 &&
    OxAPI.stubs.every(s => s.what && s.why));

  /* The examples Inspect mode opens on are compiled rather than recorded when a module is there, so
   * each carries the file that operations like these need. */
  const seeded = await OxAPI.seedExamples(global.window.OxMock.SAMPLE_FILES);
  assert("two oiSH examples are compiled", Object.keys(seeded.oish).length === 2, Object.keys(seeded.oish));
  assert("symbol examples are reflected", Object.keys(seeded.oisr).length >= 1, Object.keys(seeded.oisr));
  assert("a pipeline of each kind is derived", Object.keys(seeded.oisp).length === 3, Object.keys(seeded.oisp));
  assert("standalone binaries are lifted out of them", Object.keys(seeded.standalone).length >= 1);

  const v1 = seeded.oish["lighting.v1.oiSH"], v2 = seeded.oish["lighting.v2.oiSH"];
  assert("the two versions differ in what they declare",
    !!v1 && !!v2 && v1.model !== v2.model && v1.registers.length !== v2.registers.length,
    v1 && v2 ? `${v1.model}/${v1.registers.length} vs ${v2.model}/${v2.registers.length}` : "missing");

  const seededPipeline = await OxAPI.derivePipeline(v1, {});
  assert("an example has the file its pipeline is derived from",
    !seededPipeline.refused && seededPipeline.pipelines[0].type === "compute", seededPipeline.refused);
  assert("and the binary its disassembly comes from",
    (await OxAPI.extractBinary(v1, v1.binaries[0], "spirv")).length > 0);

  let seededCombineRefused = false;
  try { await OxAPI.combineOiSH(v1, v2); } catch (e) { seededCombineRefused = /source/i.test(e.message); }
  assert("combining two different sources is refused with the library's reason", seededCombineRefused);

  OxWasm.shutdown();
  report();
}

function report() {
  console.log(`\n${passed} passed, ${failed} failed`);
  process.exit(failed ? 1 : 0);
}

main().catch(e => { console.error(e); process.exit(1); });
