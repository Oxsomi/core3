/* editor.js — CodeMirror setup + diagnostics rendering. No app state; app.js drives it. */
(function () {
"use strict";
const { $ } = window.OxUtil;

function mk(s) { const o = {}; s.split(" ").forEach(w => o[w] = true); return o; }

CodeMirror.defineMIME("x-shader/x-hlsl", {
  name: "clike",
  keywords: mk("if else for while do switch case default return break continue struct class interface cbuffer tbuffer register numthreads WaveSize void in out inout const static uniform typedef template groupshared precise sizeof namespace using nointerpolation linear centroid sample true false export"),
  types: mk("void bool int uint float double int2 int3 int4 uint2 uint3 uint4 float2 float3 float4 float2x2 float3x3 float4x4 min16float half float16_t int16_t uint16_t int64_t uint64_t Texture1D Texture2D Texture3D TextureCube Texture2DMS Texture2DArray RWTexture1D RWTexture2D RWTexture3D Buffer RWBuffer ByteAddressBuffer RWByteAddressBuffer StructuredBuffer RWStructuredBuffer AppendStructuredBuffer ConsumeStructuredBuffer ConstantBuffer SamplerState SamplerComparisonState RaytracingAccelerationStructure BuiltInTriangleIntersectionAttributes"),
  builtin: mk("SV_Position SV_Target SV_DispatchThreadID SV_GroupID SV_GroupThreadID SV_GroupIndex SV_VertexID SV_InstanceID mul dot cross normalize length lerp saturate clamp min max abs pow exp log sqrt sin cos tan floor ceil frac step smoothstep asfloat asuint asint reflect refract distance TraceRay TraceRayInline ReportHit CallShader DispatchRaysIndex DispatchRaysDimensions InterlockedAdd"),
  atoms: mk("true false"), blockKeywords: mk("case do else for if switch while struct"),
  defKeywords: mk("struct"), typeFirstDefinitions: true, indentSwitch: false
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

let cm = null, marks = [], flashLine = null, changeCb = null;

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
  value() { return cm.getValue(); },

  open(text, readOnly) {
    const prev = changeCb; changeCb = null;         // don't echo programmatic loads back into state
    cm.setValue(text);
    cm.setOption("readOnly", readOnly ? "nocursor" : false);
    changeCb = prev;
    this.clearDiags();
  },

  clearDiags() {
    marks.forEach(m => m.clear()); marks = [];
    cm.clearGutter("diag-gutter");
    cm.eachLine(l => { cm.removeLineClass(l, "background", "diag-line-error"); cm.removeLineClass(l, "background", "diag-line-warn"); });
  },

  markDiags(diags) {
    this.clearDiags();
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

  setTheme(dark) { cm.setOption("theme", dark ? "material-darker" : "default"); }
};

window.OxEditor = OxEditor;
})();
