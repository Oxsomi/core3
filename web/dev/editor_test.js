/* editor_test.js: the HLSL mode's indentation, against the real CodeMirror the page loads.
 *
 * Needs `npm i codemirror@5.65.16` next to jsdom; run with `node dev/editor_test.js` from web/.
 * The mode is asked directly rather than through an editor: CodeMirror measures character widths to lay
 * a document out, which jsdom has no way to answer, and indentation is the mode's decision anyway.
 */
const fs = require("fs"), path = require("path");
const ROOT = path.join(__dirname, "..");

global.window = global;
global.navigator = { userAgent: "node" };
global.document = {
  createElement: () => ({ style: {}, setAttribute() {}, appendChild() {} }),
  documentElement: { style: {} },
  createDocumentFragment: () => ({ appendChild() {} })
};

let CodeMirror;

try {
  CodeMirror = require(path.join(ROOT, "node_modules/codemirror/lib/codemirror.js"));
  global.CodeMirror = CodeMirror;
  require(path.join(ROOT, "node_modules/codemirror/mode/clike/clike.js"));
} catch (err) {
  console.log("SKIP: codemirror is not installed (npm i codemirror@5.65.16)");
  process.exit(0);
}

/* Only the defineMIME call: the rest of editor.js drives an editor this has no DOM for. */
const src = fs.readFileSync(path.join(ROOT, "js/editor.js"), "utf8");
new Function("CodeMirror", src.slice(src.indexOf("function mk("), src.indexOf("const oxcOverlay")))(CodeMirror);

const mode = CodeMirror.getMode({ indentUnit: 4, tabSize: 4 }, "x-shader/x-hlsl");

/* What the editor would indent the next line by, after feeding it these lines. */
function indentAfter(lines) {
  const state = CodeMirror.startState(mode);
  for (const line of lines) {
    const stream = new CodeMirror.StringStream(line, 4, {});
    while (!stream.eol()) { mode.token(stream, state); stream.start = stream.pos; }
  }
  return mode.indent(state, "", "");
}

let failures = 0;

function check(name, got, want) {
  const ok = got === want;
  console.log((ok ? "  ok " : "FAIL ") + name + (ok ? "" : `  got ${got}, want ${want}`));
  if (!ok) failures++;
}

/* A directive ends at the newline, but it carries no ';', so clike read the line after one as the
 * continuation of a statement and indented it. Every OxC3 shader opens with an include. */
check("a line after #include is not indented", indentAfter(['#include "@types.hlsli"']), 0);
check("a line after #define is not indented", indentAfter(["#define FOO 1"]), 0);
check("a line after #pragma is not indented", indentAfter(["#pragma once"]), 0);

/* A directive continued with a backslash does carry on, and the line after it still isn't a statement. */
check("a continued directive still ends", indentAfter(["#define FOO \\\\", "  (1 + 2)"]), 0);

check("a line after a statement is not indented", indentAfter(["RWStructuredBuffer<float> b;"]), 0);
check("a line inside a block is indented once", indentAfter(["void main() {"]), 4);
check("a line inside two blocks is indented twice", indentAfter(["void main() {", "    if (x) {"]), 8);
check("a closed block returns to the outer level", indentAfter(["void main() {", "    int x;", "}"]), 0);

/* The include is the common case, so check it doesn't poison what follows it either. */
check("a block after an include still indents",
  indentAfter(['#include "@types.hlsli"', "", "void main() {"]), 4);

console.log(failures ? `\n${failures} FAILURE(S)` : "\nALL PASS");
process.exit(failures ? 1 : 0);
