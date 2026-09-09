/* editor.js: CodeMirror setup + diagnostics rendering. No app state; app.js drives it. */
(function () {
"use strict";
const { $ } = window.OxUtil;

function mk(s) { const o = {}; s.split(" ").forEach(w => o[w] = true); return o; }

/* One lexicon for the mode, the completions and anything else that needs to know what HLSL spells:
 * declared once here so the highlighter and IntelliSense can never disagree about what a keyword is.
 * The builtin intrinsics come off the compiler's own table (js/intrinsics_data.js, generated); the
 * short list here is the fallback for a page loaded without it. */
const LEX = {
  keywords: "if else for while do switch case default return break continue struct class interface cbuffer tbuffer register numthreads WaveSize void in out inout const static uniform typedef template groupshared precise sizeof namespace using nointerpolation linear centroid sample true false export",
  types: "void bool int uint float double int2 int3 int4 uint2 uint3 uint4 float2 float3 float4 float2x2 float3x3 float4x4 min16float half float16_t int16_t uint16_t int64_t uint64_t Texture1D Texture2D Texture3D TextureCube Texture2DMS Texture2DArray RWTexture1D RWTexture2D RWTexture3D Buffer RWBuffer ByteAddressBuffer RWByteAddressBuffer StructuredBuffer RWStructuredBuffer AppendStructuredBuffer ConsumeStructuredBuffer ConstantBuffer SamplerState SamplerComparisonState RaytracingAccelerationStructure BuiltInTriangleIntersectionAttributes RayDesc RayQuery",
  intrinsics: "mul dot cross normalize length lerp saturate clamp min max abs pow exp log sqrt sin cos tan floor ceil frac step smoothstep asfloat asuint asint reflect refract distance TraceRay TraceRayInline ReportHit CallShader DispatchRaysIndex DispatchRaysDimensions WorldRayOrigin WorldRayDirection RayTCurrent InterlockedAdd InterlockedCompareExchange GetDimensions SampleLevel Sample Load Store any all transpose ddx ddy",
  semantics: "SV_Position SV_Target SV_DispatchThreadID SV_GroupID SV_GroupThreadID SV_GroupIndex SV_VertexID SV_InstanceID SV_DomainLocation SV_TessFactor SV_InsideTessFactor SV_PrimitiveID COLOR NORMAL TEXCOORD0",
  oxcTypes: "F16 F32 F64 I8 I16 I32 I64 U8 U16 U32 U64 Bool F32x2 F32x3 F32x4 F32x4x4 U32x2 U32x3 U32x4 I32x2 I32x3 I32x4 F16x2 F16x4 U64x3",
  annotations: "stage extension model vendor uniforms defines binary"
};

/* A preprocessor line, tokenized whole so the mode sees it as meta rather than as an expression.
 * clike ends a statement on ';', and a directive has none, so without this every line after an #include
 * or a #define is indented as the continuation of one. A trailing backslash continues onto the next line,
 * which is why the hook can reinstall itself. */

function preprocessorLine(stream, state) {

  if (!state.startOfLine)
    return false;

  let next = null;

  for (let ch; (ch = stream.peek());) {

    if (ch === "\\" && stream.match(/^.$/)) { next = preprocessorLine; break; }
    if (ch === "/" && stream.match(/^\/[\/*]/, false)) break;     // a comment ends the directive

    stream.next();
  }

  state.tokenize = next;
  return "meta";
}

/* Every intrinsic and method name the compiler's table declares highlights as a builtin; without the
 * generated data the hand list above still covers the common ones. */
const intrinsicNames = () => {
  const d = window.OxIntrinsicsData;
  return d ? [...Object.keys(d.fns), ...Object.keys(d.methods)].join(" ") : LEX.intrinsics;
};

CodeMirror.defineMIME("x-shader/x-hlsl", {
  name: "clike",
  keywords: mk(LEX.keywords),
  types: mk(LEX.types),
  builtin: mk(LEX.semantics + " " + intrinsicNames()),
  atoms: mk("true false"), blockKeywords: mk("case do else for if switch while struct"),
  defKeywords: mk("struct"), typeFirstDefinitions: true, indentSwitch: false,
  hooks: { "#": preprocessorLine }
});

const oxcOverlay = { token(stream) {
  if (stream.match(/@?[A-Za-z0-9_./]+\.hlsli?/)) return "oxc-include";
  if (stream.match(/\[\[\s*oxc::[a-z_]+/)) return "oxc-anno";
  if (stream.match(/\[\s*shader\s*\(/)) return "oxc-shader";
  if (stream.match(/OXC_[A-Z0-9_]+/)) return "oxc-macro";
  if (stream.match(/\b(?:F16|F32|F64|I8|I16|I32|I64|U8|U16|U32|U64|Bool|B1)(?:x[2-4](?:x[2-4])?)?\b/)) return "oxc-type";
  if (stream.match(/\b(?:PUSH_CONSTANT|UNKNOWN_FORMAT|_flat|_bind)\b/)) return "oxc-macro";
  if (stream.match(/\$\$?[A-Za-z_]\w*/)) return "oxc-macro";
  while (stream.next() != null) { if (/[@\[$OFIUBP_]/.test(stream.peek() || "")) break; }
  return null;
} };

let cm = null, marks = [], flashLine = null, changeCb = null, lastDiagKey = null;
let docs = new Map();                    // open-tab key -> CodeMirror.Doc (its own undo history + cursor)

const OxEditor = {

  init() {
    cm = CodeMirror.fromTextArea($("#src"), {
      mode: "x-shader/x-hlsl",
      theme: document.documentElement.getAttribute("data-bs-theme") === "dark" ? "material-darker" : "default",
      lineNumbers: true, indentUnit: 4, tabSize: 4, matchBrackets: true, styleActiveLine: true,
      gutters: ["CodeMirror-linenumbers", "diag-gutter"]
    });
    cm.addOverlay(oxcOverlay);
    cm.setSize("100%", "100%");
    cm.on("change", () => changeCb && changeCb(cm.getValue()));
    return cm;
  },

  onChange(cb) { changeCb = cb; },
  onCursor(cb) { cm.on("cursorActivity", () => cm.getCursor && cb(cm.getCursor().line + 1)); },
  cmHandle() { return cm; },
  LEX,
  value() { return cm.getValue(); },

  /* key identifies the tab; with one, the file gets its own Doc so switching tabs keeps undo
   * history, cursor and scroll per file. Without one (or under a stub), plain setValue. */
  open(text, readOnly, key) {
    const prev = changeCb; changeCb = null;         // don't echo programmatic loads back into state
    if (key && CodeMirror.Doc && cm.swapDoc) {
      let doc = docs.get(key);
      if (!doc) { doc = new CodeMirror.Doc(text, "x-shader/x-hlsl"); docs.set(key, doc); }
      else if (doc.getValue() !== text) doc.setValue(text);   // replaced from outside (share, snapshot)
      if (cm.getDoc() !== doc) cm.swapDoc(doc);
    } else
      cm.setValue(text);
    cm.setOption("readOnly", readOnly ? "nocursor" : false);
    changeCb = prev;
    this.clearDiags();
  },

  closeDoc(key) { docs.delete(key); },
  resetDocs() { docs.clear(); },

  clearDiags() {
    lastDiagKey = null;
    marks.forEach(m => m.clear()); marks = [];
    cm.clearGutter("diag-gutter");
    cm.eachLine(l => { cm.removeLineClass(l, "background", "diag-line-error"); cm.removeLineClass(l, "background", "diag-line-warn"); });
  },

  markDiags(diags) {
    const key = JSON.stringify(diags);
    if (key === lastDiagKey) return;                //unchanged between reflects, the common case
    this.clearDiags();
    lastDiagKey = key;
    diags.forEach(d => {
      if (d.sev === "info") return;
      const ln = d.line - 1, cls = d.sev === "error" ? "error" : "warn";
      if (ln < 0 || ln >= cm.lineCount()) return;
      cm.addLineClass(ln, "background", "diag-line-" + cls);
      marks.push(cm.markText({ line: ln, ch: d.ch0 }, { line: ln, ch: d.ch1 }, { className: "cm-underline-" + cls, title: d.msg }));
      const g = document.createElement("span");
      g.className = "gutter-diag " + cls; g.innerHTML = d.sev === "error" ? "●" : "▲"; g.title = d.msg;
      cm.setGutterMarker(ln, "diag-gutter", g);
    });
  },

  gotoDiag(d) {
    cm.focus();
    cm.setCursor({ line: d.line - 1, ch: d.ch0 });
    cm.scrollIntoView({ line: d.line - 1, ch: d.ch0 }, 120);
    const ln = d.line - 1;
    if (flashLine != null) cm.removeLineClass(flashLine, "background", "diag-line-flash");
    cm.addLineClass(ln, "background", "diag-line-flash"); flashLine = ln;
    setTimeout(() => { cm.removeLineClass(ln, "background", "diag-line-flash"); if (flashLine === ln) flashLine = null; }, 700);
  },

  setTheme(theme) {
    if (!cm) return;                     //applied again right after init(), so a pre-init call is fine to drop
    cm.setOption("theme", theme || "default"); cm.refresh();
  }
};

window.OxEditor = OxEditor;
})();
