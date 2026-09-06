/* asmmap.js: source to disassembly line mapping, read out of the debug info the binary already carries.
 *
 * SPIR-V spells it two ways: classic OpLine/OpNoLine directives, and the NonSemantic
 * Shader.DebugInfo.100 DebugLine/DebugNoLine instructions that -fspv-debug=vulkan-with-source emits
 * (operands are ids of OpConstant/OpString, so those tables are read first). DXIL is LLVM IR: an
 * instruction carries `!dbg !N` and `!N = !DILocation(line, column, scope)` names the source line, with
 * the file reached through the scope's DISubprogram/DILexicalBlock.
 *
 * Everything here is pure text -> data -> html; the page wires clicks and cursor sync around it. */
(function () {
"use strict";
const U = window.OxUtil;

/* Compiler-side paths arrive as "./file.hlsl" or "/project/file.hlsl"; the page's names carry neither. */
const norm = p => (p || "").replace(/^\/*project\/*/, "").replace(/^\.\//, "");

/* ---- SPIR-V ------------------------------------------------------------------------------- */

function parseSpirv(text) {

  const lines = text.split("\n");
  const strings = {};                    // %id -> OpString value
  const consts = {};                     // %id -> OpConstant integer value
  const sources = {};                    // %id -> file path (DebugSource)

  for (const l of lines) {
    let m;
    if ((m = /^\s*(%[\w.]+)\s*=\s*OpString\s+"([^"]*)"/.exec(l))) strings[m[1]] = m[2];
    else if ((m = /^\s*(%[\w.]+)\s*=\s*OpConstant\s+%\w+\s+(\d+)\s*$/.exec(l))) consts[m[1]] = +m[2];
    else if ((m = /^\s*(%[\w.]+)\s*=\s*OpExtInst\s+%\w+\s+%[\w.]+\s+DebugSource\s+(%[\w.]+)/.exec(l)))
      sources[m[1]] = strings[m[2]];
  }

  /* A friendly-named constant id spells its value (%uint_12); anything else goes through the table. */
  const intOf = id => {
    const m = /^%uint_(\d+)$/.exec(id);
    return m ? +m[1] : consts[id];
  };

  const toSrc = new Array(lines.length).fill(null);
  let current = null;

  lines.forEach((l, i) => {
    let m;
    if ((m = /OpLine\s+(%[\w.]+)\s+(\d+)\s+(\d+)/.exec(l)))
      current = { file: norm(strings[m[1]]), line: +m[2] };
    else if (/OpNoLine/.test(l) || /OpFunctionEnd/.test(l))
      current = null;
    else if ((m = /OpExtInst\s+%\w+\s+%[\w.]+\s+DebugLine\s+(%[\w.]+)\s+(%[\w.]+)/.exec(l))) {
      const line = intOf(m[2]);
      current = line == null ? current : { file: norm(sources[m[1]]), line };
      return;                            // the DebugLine instruction itself is bookkeeping, not code
    }
    else if (/DebugNoLine/.test(l))
      current = null;
    else if (current && l.trim())
      toSrc[i] = current;
  });

  return toSrc;
}

/* ---- DXIL --------------------------------------------------------------------------------- */

function parseDxil(text) {

  const lines = text.split("\n");
  const dilocs = {};                     // !N -> {line, scope}
  const scopeFile = {};                  // !N -> file metadata id (subprogram / lexical block)
  const difiles = {};                    // !N -> filename

  for (const l of lines) {
    let m;
    if ((m = /^\s*!(\d+)\s*=\s*!DILocation\(line:\s*(\d+)(?:,\s*column:\s*\d+)?(?:,\s*scope:\s*!(\d+))?/.exec(l)))
      dilocs[m[1]] = { line: +m[2], scope: m[3] };
    else if ((m = /^\s*!(\d+)\s*=\s*(?:distinct\s+)?!DI(?:Subprogram|LexicalBlock\w*)\([^\n]*?file:\s*!(\d+)/.exec(l)))
      scopeFile[m[1]] = m[2];
    else if ((m = /^\s*!(\d+)\s*=\s*!DIFile\(filename:\s*"([^"]*)"/.exec(l)))
      difiles[m[1]] = norm(m[2]);
  }

  const fileIds = Object.keys(difiles);
  const only = fileIds.length === 1 ? difiles[fileIds[0]] : null;

  const toSrc = new Array(lines.length).fill(null);

  lines.forEach((l, i) => {
    const m = /!dbg\s+!(\d+)/.exec(l);
    if (!m) return;
    const loc = dilocs[m[1]];
    if (!loc) return;
    const file = loc.scope != null && scopeFile[loc.scope] != null ? difiles[scopeFile[loc.scope]] : only;
    toSrc[i] = { file: file == null ? only : file, line: loc.line };
  });

  return toSrc;
}

/* ---- shared ------------------------------------------------------------------------------- */

function parse(text, backend) {
  const toSrc = backend === "dxil" ? parseDxil(text) : parseSpirv(text);
  return { toSrc, any: toSrc.some(Boolean) };
}

/* One div per line so a line can be tinted, clicked and found again. The tint hue comes from the
 * source line the way Compiler Explorer does it, so everything born on one line shares a color. */
function render(text, parsed, activeFile) {
  return text.split("\n").map((l, i) => {
    const m = parsed.toSrc[i];
    if (!m || !m.line || (m.file && activeFile && m.file !== activeFile && !/\.hlsli$/i.test(m.file)))
      return `<div class="asm-line">${U.highlightAsm(l) || "&nbsp;"}</div>`;
    const hue = (m.line * 47) % 360;
    return `<div class="asm-line asm-mapped" data-src="${m.line}"${m.file ? ` data-file="${U.esc(m.file)}"` : ""}` +
      ` style="--asm-h:${hue}" title="${U.esc(m.file || "")}:${m.line} (click to go there)">${U.highlightAsm(l) || "&nbsp;"}</div>`;
  }).join("");
}

/* Renders into el and remembers the map, so the page can sync the editor's cursor into the pane. */
function mount(el, text, backend, activeFile) {
  const parsed = parse(text, backend);
  el.innerHTML = render(text, parsed, activeFile);
  el.classList.toggle("asm-has-map", parsed.any);
  el._asmAny = parsed.any;
  return parsed;
}

/* Highlights every line born on the given source line; returns how many there were. */
function syncCursor(el, srcLine) {
  if (!el || !el._asmAny) return 0;
  el.querySelectorAll(".asm-cur").forEach(n => n.classList.remove("asm-cur"));
  const hits = el.querySelectorAll(`.asm-line[data-src="${srcLine}"]`);
  hits.forEach(n => n.classList.add("asm-cur"));
  if (hits.length) hits[0].scrollIntoView({ block: "nearest" });
  return hits.length;
}

window.OxAsmMap = { parse, render, mount, syncCursor };
})();
