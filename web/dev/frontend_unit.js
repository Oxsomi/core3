/* frontend_unit.js: the pure cores of asmmap.js, intellisense.js and the symbols tree against fixed inputs.
 *
 * No DOM and no module: the fixtures spell the exact syntaxes the real toolchain produces (classic
 * OpLine, NonSemantic DebugLine, DXIL !dbg metadata) so a parser drifting away from any of them fails
 * here before it fails in a browser. Run with `node dev/frontend_unit.js` from web/.
 */
"use strict";
const fs = require("fs"), path = require("path");
const ROOT = path.join(__dirname, "..");

global.window = global;
global.document = { createElement: () => ({ style: {}, appendChild() {} }), body: { appendChild() {} } };
global.CodeMirror = { defineMIME() {} };

/* A quota-limited stand-in for localStorage, so the workspace eviction policy is testable: setItem
 * throws QuotaExceededError once the store would exceed `quota` bytes, exactly like a full browser. */
let lsData = {}, lsQuota = Infinity;
global.localStorage = {
  getItem: k => (k in lsData ? lsData[k] : null),
  setItem(k, v) {
    const next = { ...lsData, [k]: String(v) };
    const size = Object.entries(next).reduce((a, [kk, vv]) => a + kk.length + vv.length, 0);
    if (size > lsQuota) { const e = new Error("quota"); e.name = "QuotaExceededError"; throw e; }
    lsData = next;
  },
  removeItem(k) { delete lsData[k]; }
};

/* wasmload.js runs at parse time in a page: it picks a module folder and writes the script tag for it.
   Only the picking is exercised here, so the tag is never written (a worker capable origin takes the
   other branch) and the versions fetch has nothing to reach. */

global.location = { protocol: "https:" };
global.Worker = function () {};

for (const f of ["js/util.js", "js/wasmload.js", "js/workspace.js", "js/editor.js", "js/intellisense.js", "js/asmmap.js", "js/tools/symbols.js"])
  new Function(fs.readFileSync(path.join(ROOT, f), "utf8")).call(global);

let failures = 0;
function check(name, cond, extra) {
  console.log((cond ? "  ok " : "FAIL ") + name + (cond ? "" : "  " + (extra || "")));
  if (!cond) failures++;
}

/* ---- asmmap: NonSemantic DebugLine (what -fspv-debug=vulkan-with-source emits) -------------- */

const SPV_RICH = [
  '%4 = OpString "/project/lighting.hlsl"',
  '%9 = OpString "./include/light.hlsli"',
  "%uint_7 = OpConstant %uint 7",
  "%62 = OpExtInst %void %1 DebugSource %4",
  "%88 = OpExtInst %void %1 DebugSource %9",
  "%main = OpFunction %void None %3",
  "%201 =   OpExtInst %void %1 DebugLine %62 %uint_19 %uint_19 %uint_33 %uint_36",
  "%202 = OpLoad %float %x",
  "%203 = OpFAdd %float %202 %202",
  "%204 = OpExtInst %void %1 DebugLine %88 %uint_7 %uint_7 %uint_1 %uint_5",
  "%205 = OpFMul %float %203 %203",
  "OpFunctionEnd",
  "%299 = OpIAdd %uint %a %b"
].join("\n");

{
  const r = window.OxAsmMap.parse(SPV_RICH, "spirv");
  check("spv rich: mapping found at all", r.any);
  check("spv rich: instruction after DebugLine maps to its line",
    r.toSrc[7] && r.toSrc[7].line === 19 && r.toSrc[7].file === "lighting.hlsl", JSON.stringify(r.toSrc[7]));
  check("spv rich: the scope holds until the next DebugLine",
    r.toSrc[8] && r.toSrc[8].line === 19);
  check("spv rich: a DebugLine into an include carries that file",
    r.toSrc[10] && r.toSrc[10].line === 7 && r.toSrc[10].file === "include/light.hlsli", JSON.stringify(r.toSrc[10]));
  check("spv rich: the DebugLine bookkeeping row itself is not mapped", r.toSrc[9] === null);
  check("spv rich: OpFunctionEnd closes the scope", r.toSrc[12] === null);
}

/* ---- asmmap: classic OpLine ----------------------------------------------------------------- */

const SPV_CLASSIC = [
  '%1 = OpString "shader.hlsl"',
  "OpLine %1 12 3",
  "%2 = OpFAdd %float %a %b",
  "OpNoLine",
  "%3 = OpFMul %float %a %b"
].join("\n");

{
  const r = window.OxAsmMap.parse(SPV_CLASSIC, "spirv");
  check("spv classic: OpLine maps the following instruction",
    r.toSrc[2] && r.toSrc[2].line === 12 && r.toSrc[2].file === "shader.hlsl", JSON.stringify(r.toSrc[2]));
  check("spv classic: OpNoLine ends it", r.toSrc[4] === null);
}

/* ---- asmmap: DXIL --------------------------------------------------------------------------- */

const DXIL = [
  "define void @main() {",
  "  %1 = fadd float %a, %b, !dbg !230",
  "  %2 = fmul float %1, %1, !dbg !231",
  "  ret void",
  "}",
  '!168 = distinct !DISubprogram(name: "main", file: !5, line: 20)',
  '!5 = !DIFile(filename: "./lighting.hlsl", directory: "")',
  "!230 = !DILocation(line: 22, column: 15, scope: !168)",
  "!231 = !DILocation(line: 23, column: 5, scope: !168)"
].join("\n");

{
  const r = window.OxAsmMap.parse(DXIL, "dxil");
  check("dxil: !dbg resolves through DILocation",
    r.toSrc[1] && r.toSrc[1].line === 22 && r.toSrc[1].file === "lighting.hlsl", JSON.stringify(r.toSrc[1]));
  check("dxil: second location too", r.toSrc[2] && r.toSrc[2].line === 23);
  check("dxil: an undecorated instruction is unmapped", r.toSrc[3] === null);
}

check("no debug info: any is false and nothing throws",
  !window.OxAsmMap.parse("%1 = OpFAdd %float %a %b", "spirv").any);

/* ---- intellisense --------------------------------------------------------------------------- */

/* A hand-built SRDocument with the exact field shapes the wasm boundary emits. */
const DOC = { nodes: [
  { id: 0, kind: "Namespace", name: "(anonymous)", parent: -1, children: [1, 4, 5], annotations: [] },
  { id: 1, kind: "Struct", name: "Light", parent: 0, children: [2, 3], annotations: [],
    loc: { file: "lighting.hlsl", line: 4, col: 1, len: 6 } },
  { id: 2, kind: "Variable", name: "pos", parent: 1, children: [], annotations: [],
    type: { name: "float3", display: "F32x3", cls: "Vector", def: -1 } },
  { id: 3, kind: "Variable", name: "color", parent: 1, children: [], annotations: [],
    type: { name: "float3", display: "F32x3", cls: "Vector", def: -1 } },
  { id: 4, kind: "Variable", name: "sun", parent: 0, children: [], annotations: [],
    type: { name: "Light", display: "Light", cls: "Struct", def: 1 },
    loc: { file: "lighting.hlsl", line: 9, col: 1, len: 3 } },
  { id: 5, kind: "Function", name: "shade", parent: 0, children: [6], annotations: [],
    loc: { file: "lighting.hlsl", line: 12, col: 1, len: 5 } },
  { id: 6, kind: "Parameter", name: "l", parent: 5, children: [], annotations: [],
    type: { name: "Light", display: "Light", cls: "Struct", def: 1 } },
  { id: 7, kind: "Register", name: "_layers", parent: 0, children: [], annotations: [],
    register: { info: "Texture2D dim=4 ret=5", count: 4, cls: "SRV" }, arrays: [4],
    loc: { file: "arrays.hlsl", line: 3, col: 1, len: 7 } },
  { id: 8, kind: "Parameter", name: "id", parent: 5, children: [], annotations: [],
    semantic: "SV_DispatchThreadID", type: { name: "uint3", display: "U32x3", cls: "Vector", def: -1 } },
  { id: 9, kind: "Function", name: "getAtUniform", parent: 0, children: [], annotations: [], builtin: true,
    loc: { file: "./@buffer.hlsli", line: 53, col: 1, len: 12 } },
  { id: 10, kind: "Interface", name: "IArea", parent: 0, children: [], annotations: [] },
  { id: 11, kind: "Struct", name: "Shape", parent: 0, children: [], annotations: [] },
  { id: 12, kind: "Struct", name: "Circle", parent: 0, children: [13], annotations: [], implements: [10],
    type: { name: "Circle", display: "Circle", cls: "Struct", def: -1, base: 11 } },
  { id: 13, kind: "Variable", name: "size", parent: 12, children: [], annotations: [],
    type: { name: "float2", display: "F32x2", cls: "Vector", def: -1 } },
  { id: 14, kind: "Register", name: "_bare", parent: 0, children: [], annotations: [],
    register: { info: "Texture", count: 4, cls: "SRV" } },
  /* Enumerators carry their value and the enum's underlying type; a value past 2^53 arrives as text. */
  { id: 15, kind: "Enum", name: "Mode", parent: 0, children: [16, 17], annotations: [],
    loc: { file: "enums.hlsl", line: 3, col: 1, len: 4 } },
  { id: 16, kind: "EnumValue", name: "Off", parent: 15, children: [], annotations: [], enumValue: { value: 0, type: "uint" } },
  { id: 17, kind: "EnumValue", name: "Linear", parent: 15, children: [], annotations: [], enumValue: { value: 1, type: "uint" } },
  { id: 18, kind: "Enum", name: "Wide", parent: 0, children: [19], annotations: [] },
  { id: 19, kind: "EnumValue", name: "Huge", parent: 18, children: [], annotations: [],
    enumValue: { value: "9007199254740993", type: "uint64_t" } }
] };
DOC.nodes[5].children = [6, 8];

/* The macro table the hover falls back to when the AST has nothing: a #define never reaches the AST. */
const CTX = { files: { "a.hlsl": { src: "// nothing\n" } },
  builtins: { "@resources.hlsli": "#pragma once\n#define texture2DUniform(i) _textures2D[i & ResourceId_mask]\n" } };

const IS = window.OxIntelliSense;

{
  const h = IS.hoverInfo(DOC, "Light");
  check("hover: a struct previews its members",
    h && h.title === "struct Light { F32x3 pos; F32x3 color }", h && h.title);
  check("hover: and says where it lives", h && h.loc === "lighting.hlsl:4", h && h.loc);

  const f = IS.hoverInfo(DOC, "shade");
  check("hover: a function reads as its HLSL signature",
    f && f.title === "void shade(Light l, U32x3 id : SV_DispatchThreadID)", f && f.title);

  const v = IS.hoverInfo(DOC, "sun");
  check("hover: a value reads as type name", v && v.title === "Light sun", v && v.title);

  const r = IS.hoverInfo(DOC, "_layers");
  check("hover: a register carries its resource info and array size",
    r && r.title === "Texture2D dim=4 ret=5 _layers[4]", r && r.title);

  /* The real emitter keeps a one-dimensional register array's size in its bind count, with no arrays
   * field at all; the hover has to read it from there or `_layers[4]` shows bare. */
  const rc = IS.hoverInfo(DOC, "_bare");
  check("hover: a register array size comes from the bind count too",
    rc && rc.title === "Texture _bare[4]", rc && rc.title);

  const st = IS.hoverInfo(DOC, "Circle");
  check("hover: a struct spells its base and interfaces the HLSL way",
    st && st.title === "struct Circle : Shape, IArea { F32x2 size }", st && st.title);

  const ak = IS.hoverInfo(DOC, "sun");
  check("hover: same alias and underlying adds no aka", ak && !ak.sub.includes("aka"), ak && ak.sub);
  const ak2 = IS.hoverInfo(DOC, "size");
  check("hover: a differing underlying type rides as aka",
    ak2 && ak2.sub.includes("aka float2"), ak2 && ak2.sub);

  const p = IS.hoverInfo(DOC, "id");
  check("hover: a parameter keeps its semantic",
    p && p.title === "U32x3 id : SV_DispatchThreadID", p && p.title);

  const b = IS.hoverInfo(DOC, "getAtUniform");
  check("hover: a builtin says which include it came from",
    b && b.sub.includes("from @buffer.hlsli"), b && b.sub);

  const mac = IS.hoverInfo(DOC, "texture2DUniform", CTX);
  check("hover: a macro falls back to its #define",
    mac && mac.title.startsWith("#define texture2DUniform(i)"), mac && (mac.title || "null"));

  check("hover: an unknown name is nothing", IS.hoverInfo(DOC, "nope", CTX) === null);

  const ev = IS.hoverInfo(DOC, "Linear");
  check("hover: an enumerator carries its value", ev && ev.title === "Linear = 1", ev && ev.title);
  check("hover: and names its enum with the underlying type in OxC3 spelling",
    ev && ev.sub === "EnumValue · Mode : U32", ev && ev.sub);

  const en = IS.hoverInfo(DOC, "Mode");
  check("hover: an enum lists its enumerators with their values",
    en && en.title === "enum Mode : U32 { Off = 0, Linear = 1 }", en && en.title);

  const big = IS.hoverInfo(DOC, "Huge");
  check("hover: a value past 2^53 prints as the text it travelled as",
    big && big.title === "Huge = 9007199254740993" && big.sub === "EnumValue · Wide : U64",
    big && (big.title + " / " + big.sub));
}

/* ---- symbols tree ------------------------------------------------------------------------- */

/* The tree prints an enumerator the way the CLI does, `Linear = 1`, with the underlying type under --verbose. */
{
  const SYM = window.OxSymbols;
  const tree = { name: "enums.oiSR", sourceName: "enums.hlsl", builtinCollapsed: [],
    header: { version: "1.1", counts: { nodes: 3, annotations: 0, registers: 0, types: 0 }, features: ["Basics", "UserTypes"] },
    nodes: [
      { id: 0, kind: "Namespace", name: "(anonymous)", parent: -1, children: [1], annotations: [] },
      { id: 1, kind: "Enum", name: "Mode", parent: 0, children: [2], annotations: [] },
      { id: 2, kind: "EnumValue", name: "Linear", parent: 1, children: [], annotations: [], enumValue: { value: 1, type: "uint" } }
    ] };
  const text = html => html.replace(/<[^>]+>/g, "");
  const plain = text(SYM.html(tree));
  check("symbols: an enumerator shows its value after the name", plain.includes("EnumValue Linear = 1"), plain);
  const verbose = text(SYM.html(tree, { verbose: true }));
  check("symbols: --verbose adds the enum's underlying type", verbose.includes("EnumValue Linear = 1 (U32)"), verbose);
}

{
  const d = IS.definitionOf(DOC, "Light");
  check("goto: a symbol resolves to its declaration", d && d.file === "lighting.hlsl" && d.line === 4,
    JSON.stringify(d));
  const dm = IS.definitionOf(DOC, "texture2DUniform", CTX);
  check("goto: a macro resolves into its include", dm && dm.file === "@resources.hlsli" && dm.line === 2,
    JSON.stringify(dm));
  check("goto: an unknown name goes nowhere", IS.definitionOf(DOC, "nope", CTX) === null);
}

{
  const m = IS.completions(DOC, {}, "    sun.", 8);
  check("completion: members come off the type graph",
    m && m.list.map(c => c.text).sort().join(",") === "color,pos", m && JSON.stringify(m.list));
  const mp = IS.completions(DOC, {}, "    sun.po", 10);
  check("completion: a member prefix narrows", mp && mp.list.length === 1 && mp.list[0].text === "pos");
  check("completion: an unknown object offers nothing", IS.completions(DOC, {}, "nope.", 5) === null);
}

{
  const inc = IS.completions(DOC, { "post.hlsl": {}, "include/light.hlsli": {} }, '#include "in', 12);
  check("completion: includes offer project files",
    inc && inc.list.some(c => c.text === "include/light.hlsli"), inc && JSON.stringify(inc.list));
  const anno = IS.completions(DOC, {}, "[[oxc::st", 9);
  check("completion: oxc annotations complete", anno && anno.list.some(c => c.text === "stage("), anno && JSON.stringify(anno.list));
}

{
  const id = IS.completions(DOC, {}, "  Li", 4);
  check("completion: identifiers include declared symbols", id && id.list.some(c => c.text === "Light"));
  const ty = IS.completions(DOC, {}, "  F32x", 6);
  check("completion: and the oxc type aliases", ty && ty.list.some(c => c.text === "F32x3"));
  const kw = IS.completions(DOC, {}, "  ret", 5);
  check("completion: and plain keywords", kw && kw.list.some(c => c.text === "return"));
  const mc = IS.completions(DOC, CTX.files, "  texture2DU", 12, CTX.builtins);

  /* Aliases come from the builtin headers' typedefs when builtins are present, not the hand list. */
  const al = IS.completions(DOC, {}, "  NewAl", 7, { "@types.hlsli": "typedef float NewAlias;\ntypedef uint NewAliasx4;\n" });
  check("completion: aliases derive from the builtin typedefs",
    al && al.list.some(c => c.text === "NewAlias") && al.list.some(c => c.text === "NewAliasx4"),
    al && JSON.stringify(al.list));
  check("completion: and macros from the includes", mc && mc.list.some(c => c.text === "texture2DUniform"),
    mc && JSON.stringify(mc.list));
}

/* ---- share codec ---------------------------------------------------------------------------- */

(async () => {

  const U2 = window.OxUtil;
  const state = { m: "compile", f: "a.hlsl", files: { "a.hlsl": "void main() {}\n".repeat(40) }, o: { targets: ["spv"] } };

  const enc = await U2.encodeShare(state);
  check("share codec: compresses when the browser can",
    enc.startsWith("z=") && enc.length < JSON.stringify(state).length, enc.slice(0, 12) + " len=" + enc.length);

  const back = await U2.decodeShare("#" + enc);
  check("share codec: roundtrips", JSON.stringify(back) === JSON.stringify(state));

  let threw = null;
  try { await U2.decodeShare("#" + enc.slice(0, enc.length - 8)); } catch (e) { threw = e.message; }
  check("share codec: a truncated link fails loudly", threw && /truncat|corrupt/.test(threw), threw);

  const legacyJson = JSON.stringify(state);
  const legacy = "s=" + Buffer.from(legacyJson, "utf8").toString("base64");
  const backLegacy = await U2.decodeShare("#" + legacy);
  check("share codec: old s= links still open", JSON.stringify(backLegacy) === JSON.stringify(state));

  const crc = U2.crc32c(new TextEncoder().encode(legacyJson)).toString(16);
  let threw2 = null;
  try { await U2.decodeShare("#s=" + legacy.slice(2, legacy.length - 6) + "&c=" + crc); } catch (e) { threw2 = e.message; }
  check("share codec: a truncated legacy link with a checksum fails loudly", !!threw2, threw2);

  /* ---- same-name resolution (interface method vs implementations) --------------------------- */

  {
    const L = (line, lines) => ({ file: "a.hlsl", line, lines });
    const doc2 = { sourceName: "a.hlsl", nodes: [
      { id: 0, kind: "Namespace", name: "(anonymous)", parent: -1, loc: null, children: [1, 3, 6] },
      { id: 1, kind: "Interface", name: "IArea", parent: 0, loc: L(2, 3), children: [2] },
      { id: 2, kind: "Function", name: "area", parent: 1, loc: L(3, 1), children: [] },
      { id: 3, kind: "Struct", name: "Rect", parent: 0, loc: L(6, 4), children: [4] },
      { id: 4, kind: "Function", name: "area", parent: 3, loc: L(7, 2), children: [5] },
      { id: 5, kind: "Parameter", name: "(return)", parent: 4, type: { display: "F32", name: "float" } },
      { id: 6, kind: "Struct", name: "Circle", parent: 0, loc: L(11, 3), children: [7] },
      { id: 7, kind: "Function", name: "area", parent: 6, loc: L(12, 1), children: [] }
    ] };

    check("scopes: inside Rect's body, area means Rect's area",
      IS.hoverInfo(doc2, "area", null, { line: 8 }).node.id === 4);
    check("scopes: inside Circle's body, area means Circle's area",
      IS.hoverInfo(doc2, "area", null, { line: 13 }).node.id === 7);
    check("scopes: on the interface's own line, area means the interface's declaration",
      IS.hoverInfo(doc2, "area", null, { line: 3 }).node.id === 2);
    check("scopes: outside every scope, the most informative declaration wins",
      IS.hoverInfo(doc2, "area", null, { line: 20 }).title.startsWith("F32 area("),
      IS.hoverInfo(doc2, "area", null, { line: 20 }).title);
  }

  /* ---- doc comments ------------------------------------------------------------------------ */

  {
    const files = { "a.hlsl": { src: "F32 g;\n// Computes the half.\n// Second line.\nF32 halve(F32 v) { return v; }\n" } };
    const d3 = { sourceName: "a.hlsl", nodes: [
      { id: 0, kind: "Namespace", name: "(anonymous)", parent: -1, loc: null, children: [1] },
      { id: 1, kind: "Function", name: "halve", parent: 0, loc: { file: "a.hlsl", line: 4, lines: 1 }, children: [] }
    ] };
    const h = IS.hoverInfo(d3, "halve", { files, builtins: null });
    check("hover: the // block above a declaration rides along", h.doc === "Computes the half.\nSecond line.", h.doc);
    const g = { ...d3, nodes: [d3.nodes[0], { ...d3.nodes[1], name: "g", loc: { file: "a.hlsl", line: 1, lines: 1 } }] };
    check("hover: no comment block means no doc", IS.hoverInfo(g, "g", { files, builtins: null }).doc === null);
  }

  /* ---- workspace eviction ------------------------------------------------------------------ */

  const WS = window.OxWorkspace;

  /* gzip would flatten repeated characters, so incompressible pseudo-random text keeps the sizes the
   * quota math below assumes. */
  const noise = n => Array.from({ length: n }, (_, i) => String.fromCharCode(33 + (i * 7919 + i * i * 31) % 90)).join("");
  const bigProject = { files: { "a.hlsl": noise(4000) }, f: "a.hlsl" };
  const midProject = { files: { "a.hlsl": noise(600) }, f: "a.hlsl" };
  const smallProject = { files: { "a.hlsl": "y" }, f: "a.hlsl" };

  const wsA = WS.create("keep-me", "local");
  WS.switchTo(wsA.id);
  check("ws: a save lands", await WS.save(smallProject) === true);
  WS.pin(wsA.id, true);

  const wsB = WS.create("victim", "local");
  WS.switchTo(wsB.id);
  await WS.save(bigProject);

  /* Quota that fits the mid project only once the big unpinned one is evicted: the save must evict
   * the victim, never the pinned one, never itself, and then succeed. */

  const wsC = WS.create("newcomer", "shared", "hash123");
  WS.switchTo(wsC.id);
  const encMid = await U2.encodeShare(midProject);
  lsQuota = Object.entries(lsData).reduce((a, [k, v]) => a + k.length + v.length, 0) + encMid.length - 1;

  const okC = await WS.save(midProject);
  const names = WS.list().map(w => w.name);

  check("ws: a full store evicts the oldest unpinned workspace", okC === true && !names.includes("victim"),
    okC + " " + JSON.stringify(names));
  check("ws: the pinned one survives", names.includes("keep-me"), JSON.stringify(names));
  check("ws: the evicted one's payload is gone too", localStorage.getItem("ox.ws." + wsB.id) === null);
  check("ws: the saved one loads back intact", (await WS.load(wsC.id)).files["a.hlsl"].length === 600);
  check("ws: srcHash lookup finds it", (WS.findBySrcHash("hash123") || {}).id === wsC.id);

  /* Nothing left to evict: the save reports failure instead of looping or throwing. */
  lsQuota = 10;
  check("ws: an unsatisfiable save fails loudly", await WS.save(bigProject) === false && WS.isDirty());

  /* ---- lineDiff: the ops, and the guards that keep the table from being the size of the product ---- */

  const D = U2.lineDiff;
  const ops = (a, b) => D(a, b).map(o => o.t + (o.t === "+" ? o.b : o.a)).join(" ");

  check("diff: identical input is all equal", ops(["a", "b"], ["a", "b"]) === "=a =b");
  check("diff: a replaced line removes then adds", ops(["a", "x", "c"], ["a", "y", "c"]) === "=a -x +y =c");
  check("diff: an insertion in the middle", ops(["a", "c"], ["a", "b", "c"]) === "=a +b =c");
  check("diff: an insertion at the head", ops(["b"], ["a", "b"]) === "+a =b");
  check("diff: an insertion at the tail", ops(["a"], ["a", "b"]) === "=a +b");
  check("diff: an empty side is all additions", ops([], ["a", "b"]) === "+a +b");
  check("diff: the other empty side is all removals", ops(["a", "b"], []) === "-a -b");
  check("diff: a common head and tail survive a changed middle",
    ops(["h", "1", "t"], ["h", "2", "t"]) === "=h -1 +2 =t");

  /* A shared head and tail is what two disassemblies of one shader look like: the table is never built
     for them, so a large pair stays instant. */

  {
    const A = [], B = [];
    for (let i = 0; i < 20000; i++) { A.push("l" + i); B.push("l" + i); }
    B[10000] = "changed";
    const t0 = Date.now();
    const out = D(A, B);
    const ms = Date.now() - t0;
    check("diff: 20k lines with one change stays instant", ms < 500, ms + "ms");
    check("diff: and reports exactly that one change",
      out.filter(o => o.t !== "=").map(o => o.t).join("") === "-+");
  }

  /* Past the cap the middle is one block replaced by another, rather than a table of n*m cells. */

  {
    const A = [], B = [];
    for (let i = 0; i < 3000; i++) { A.push("a" + i); B.push("b" + i); }
    const t0 = Date.now();
    const out = D(A, B);
    const ms = Date.now() - t0;
    check("diff: a large all-different pair falls back instead of allocating", ms < 500, ms + "ms");
    check("diff: and still accounts for every line",
      out.filter(o => o.t === "-").length === 3000 && out.filter(o => o.t === "+").length === 3000);
  }

  /* ---- share payloads: a link that arrives whole can still hold anything ---------------------- */

  const refused = p => {
    try { U2.checkSharePayload(p); return false; }
    catch (e) { return /doesn't hold a project/.test(e.message); }
  };

  check("share: what encodeShare produces is accepted",
    !refused({ m: "compile", f: "a.hlsl", files: { "a.hlsl": "x", "dir/b.hlsli": "y" }, o: {}, v: {} }));
  check("share: the older single file form is accepted", !refused({ f: "a.hlsl", src: "x" }));
  check("share: an array is refused", refused([1, 2]));
  check("share: a bare string is refused", refused("nope"));
  check("share: null is refused", refused(null));
  check("share: a file set that isn't an object is refused", refused({ files: "a.hlsl" }));
  check("share: a file set as an array is refused", refused({ files: ["a.hlsl"] }));
  check("share: contents that aren't text are refused", refused({ files: { "a.hlsl": { toString: 1 } } }));
  check("share: an absolute file name is refused", refused({ files: { "/etc/passwd": "x" } }));
  check("share: a name climbing out of the project is refused", refused({ files: { "../../x": "x" } }));
  check("share: a name climbing in the middle is refused", refused({ files: { "a/../../x": "x" } }));
  check("share: a windows style climb is refused", refused({ files: { "a\\..\\..\\x": "x" } }));
  check("share: a dotted name that does not climb is accepted", !refused({ files: { "a..b.hlsl": "x" } }));
  check("share: an empty name is refused", refused({ files: { "": "x" } }));
  check("share: an active file that isn't text is refused", refused({ f: 3 }));
  check("share: options that aren't an object are refused", refused({ o: "all" }));

  {
    const many = {};
    for (let i = 0; i < 600; i++) many["f" + i + ".hlsl"] = "x";
    check("share: an unreasonable file count is refused", refused({ files: many }));
  }

  /* ---- the module path a remembered version names ------------------------------------------- */

  const mb = window.OxWasmVersion.moduleBase;

  check("wasmload: no pick is the default folder", mb(null) === "wasm/" && mb("") === "wasm/");
  check("wasmload: 'current' is the default folder", mb("current") === "wasm/");
  check("wasmload: a version names a subfolder", mb("3.2.104") === "wasm/3.2.104/");
  check("wasmload: a slash is filtered out", mb("a/b") === "wasm/ab/");
  check("wasmload: a name that filters down to nothing falls back", mb("///") === "wasm/");
  check("wasmload: dots alone fall back", mb("..") === "wasm/");

  /* Whatever the stored value is, the path it names stays one folder under wasm/. */

  for (const hostile of ["../../etc", "a/../../b", "//evil.example/x", "..\\..\\x", "<script>", "%2e%2e%2f", "\u0000"]) {
    const out = mb(hostile);
    check("wasmload: " + JSON.stringify(hostile) + " stays under wasm/",
      /^wasm\/([\w.-]+\/)?$/.test(out) && !/(^|\/)\.\.(\/|$)/.test(out.slice(5)), out);
  }

  /* ---- highlighting: the classes and, more importantly, the escaping ------------------------- */

  check("highlight: an opcode and an id are classed",
    U2.highlightAsm("%1 = OpLoad") ===
    '<span class="hl-id">%1</span> = <span class="hl-op">OpLoad</span>');
  check("highlight: a comment swallows what follows",
    U2.highlightAsm("; OpLoad") === '<span class="hl-cmt">; OpLoad</span>');
  check("highlight: markup in the text is escaped, not emitted",
    U2.highlightAsm("<img src=x>").indexOf("<img") === -1 &&
    U2.highlightAsm("<img src=x>").indexOf("&lt;img") !== -1);
  check("highlight: a pipeline field keeps its provenance colour",
    U2.highlightPipeline("blend.enable = derived").indexOf('class="prov-derived"') !== -1);
  check("highlight: pipeline text escapes markup too",
    U2.highlightPipeline('"<b>"').indexOf("<b>") === -1);

  console.log(failures ? `\n${failures} FAILURE(S)` : "\nALL PASS");
  process.exit(failures ? 1 : 0);
})();
