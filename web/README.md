# shader.oxsomi.com

An HLSL shader compiler and inspector that runs entirely in your browser.

You write HLSL, it compiles to SPIR-V and DXIL and shows you everything about the result: the
diagnostics, the resource bindings and layouts, the disassembly tinted by the source line that
produced it, the pipeline state a shader implies, and what changed between two versions. The
compiler is the real thing, not an approximation: the same one OxC3's command line uses, a fork of
Microsoft's DirectXShaderCompiler plus SPIRV-Tools, compiled to WebAssembly. It runs on your
machine, in a Web Worker, and nothing you type is sent anywhere.

It is a prototype. The status bar badge says at every moment whether you are looking at compiler
output (`OxC3 3.2.104`) or at a stand-in (`mock`), and its tooltip lists exactly what the stand-in
covers, so nothing has to be taken on trust.

## What you can do

- **Compile** with the same flags the CLI has, and get errors and warnings inline in the editor.
  Stage, shader model and extensions live in the source itself (`[shader]`, `[[oxc::model]]`,
  `[[oxc::extension]]`), so what you see is what the command line would compile.
- **Read the reflection**: registers, dual bindings, which resources are actually used, constant
  buffer layouts, inputs and outputs, wave sizes, payload sizes.
- **Navigate the symbols**: an outline of entrypoints, types, resources and locals, each a link to
  its declaration. Hover a name for its declaration in HLSL spelling, Ctrl+Space to complete
  symbols, members, intrinsics and `#include` targets, and go to definition across the project.
- **Read the disassembly** of the SPIR-V or DXIL, each line tinted by the source line it came from.
  Click a disassembly line to jump to the source; move the cursor to light up everything it
  produced.
- **See the pipeline** a shader implies, with every field marked derived, supplied or assumed and
  the reason; edit a field to see what a PSO change does, download the result as an `.oiSP`.
- **Diff** two compiles, two backends, or two binaries by entrypoint, and combine two oiSH files
  the way `file combine` does.
- **Load your own files**: compiled `.oiSH`, reflected `.oiSR`, pipeline `.oiSP`, or a bare `.spv`
  / `.dxil` to disassemble, validate and list entrypoints.
- **Share**: a link carries the whole project inside the URL itself, no server involved. Bigger
  projects go as an `.oiCA` snapshot you can download and drop back in. Named workspaces keep
  your work in the browser between visits.

The one tab that is not real in the browser is **ISA**: the AMD ISA route spawns a compiler the
browser cannot, so the tab explains the route and shows a stand-in. The native CLI runs it for
real; a device-free Mesa build that can run in the browser is in planning.

## Try it

Open the site and pick one of the sample shaders in the rail. Or open `web/index.html` straight
from a checkout: it works from `file://`, with no server and no build step beyond staging the
module (see below).

The compiler module is WebAssembly with 64-bit memory, which needs a current Chrome, Edge or
Firefox. Where the module cannot load, the page still works on the stand-in tier and the badge
says `mock`.

## What is real and what is not

With the module loaded, everything the page shows is what OxC3 produced, except these, each a call
the library does not have yet rather than a shortcut taken here:

| Not wired | Why |
| --- | --- |
| Raw DXC (Command tab) | the compiler exposes neither its own argv (`getCompileArgs`) nor a raw compile entry |
| Reflecting a standalone SPV / DXIL | backend reflection only runs inside a compile (`Compiler_process` wants the entry's runtime reflection to check against), so no call takes a bare binary |
| Assemble a binary into an oiSH | `shader assemble` produces a `.spv`; wrapping one into an `SHFile` with an identifier isn't a CLI verb |
| Per-backend lean oiSH download | `file split -format oiSH` is planned; compiling with one `-compile-output` produces one today |
| ISA disassembly | the offline route spawns the bundled amdllpc, and `SUPPORTS_PROCESS` is off in wasm. The device-free Mesa route is the one that could run here and isn't in the CLI yet |
| `-asic live` | needs a real device through `VK_KHR_pipeline_executable_properties` |

Without a module at all, the sample project's oiSH, oiSR and oiSP documents and the built-in
include sources are still real (recorded from a real run by `dev/gen_mock_data.js`), while
anything derived from an edit you made in the browser (diagnostics, binding assignment, sizes,
disassembly, symbol locations, parsed uploads) is fabricated deterministically by the mocks.

## License

The frontend in this directory (everything except `web/wasm/`) is MIT licensed, see [LICENSE](LICENSE):
take it, rework it, ship it, it exists partly as a demo of what the framework can do.

The compiled module in `web/wasm/` (`OxC3_wasm.js` / `OxC3_wasm.wasm`) is a build of the surrounding
[OxC3 repository](https://github.com/Oxsomi/core3) and keeps its licensing: GPLv3, with a commercial
license available (contact@osomi.net) for closed-source use, exactly as the repository's LICENSE
describes. On top of that, as an additional permission in the sense of GPLv3 section 7: serving or
bundling the UNMODIFIED module together with this frontend, or with a frontend derived from it or built
against its `OxAPI` boundary, for the purpose of compiling and inspecting shaders, does not extend the
GPL to that frontend. Any other use of the module, and any modified build, is plain GPLv3 or the
commercial license; the sources it is built from are public either way, so nothing is taken off the
table that GPLv3 grants.

The module also embeds third-party work that carries its own notices wherever the module is
redistributed: the [DirectXShaderCompiler fork](https://github.com/Oxsomi/DirectXShaderCompiler)
(Apache License 2.0 with LLVM exceptions) and [SPIRV-Tools](https://github.com/KhronosGroup/SPIRV-Tools)
(Apache License 2.0).

---

# For contributors

Everything below is about building, hosting and changing the page. A static frontend for
`OxC3 shader …`, `OxC3 file …` and `OxC3 isa …`, running the real compiler as wasm64. Its own code
has no dependencies; Bootstrap and CodeMirror are vendored into `web/vendor/` by the build and
served from the same host as the page.

## Building and serving

```
python build_web.py -mode Release --frontend --serve
```

That fetches the page's third party into `web/vendor/` (see below), stages both module flavors into
`web/wasm/`, `OxC3_wasm.js` + `.wasm` (two file) and `OxC3_wasm_sf.js` (the same build with the wasm
embedded in its js), and serves the page on
http://localhost:8000. Nothing is compiled on that server: it hands over the module and the page
does the rest in the browser, so it is a file server, not a backend. It exists because a browser
refuses to fetch a sibling `.wasm` from `file://`; opened straight from disk, `web/index.html`
loads the embedded flavor instead and needs no server at all.

The embedded flavor costs the streaming compile and about a tenth more bytes, so it is the local
flavor; a hosted page wants the two file one, where the browser streams and caches the wasm as its
own resource, and its worker only ever asks for that one. To iterate quickly without relinking
both, `--single_file` builds and stages only the embedded flavor.

## Third party

`index.html` loads Bootstrap, its icon font and CodeMirror by a local path, so a visitor's browser
talks to one host and only a build ever reaches a CDN. `--vendor` fetches them:

```
python build_web.py --vendor --skip_build
```

The set is pinned in build_web.py's `VENDOR` table, upstream's own minified builds at the exact
versions `index.html` asks for, each verified against a recorded SHA-256. A file already on disk with
the right hash is left alone and one that doesn't match is fetched again, so a second run costs one
read per file and an edited file heals itself; a download that doesn't match the pin fails the build,
since that is the CDN handing over something nobody reviewed. `.br` and `.gz` siblings are written next to each one (the
two font files are already compressed, so they get neither). `--frontend` runs this step too, and the
directory is gitignored: it is fetched, not checked in.

This is also what the VS Code webview needs, since its CSP blocks remote resources outright.

## Hosting for real

The module dominates first load: ~21 MB raw, ~5.5 MB under brotli. So the deployed site wants the
two file build, precompressed:

```
python build_web.py -mode Release --frontend --precompress --serve
```

A tagged release ships this exact folder as `OxC3-<tag>-Web-wasm64.zip`, `.br` siblings included, so a
host can unpack it next to the page and skip the local step entirely.

`--zip` packages the whole deployable site rather than just the module:

```
python build_web.py -mode Release --frontend --vendor --precompress --zip
```

That writes `build/<mode>/web/OxC3-web.zip` (or the path you pass) holding the page, its styles and
scripts, the vendored third party, the module in both flavors and every `.br` / `.gz` sibling, and
nothing else: the dev scripts, node_modules, the sample sources (they travel inside the recording) and
this README stay behind. It refuses rather than shipping half a site if the module was never staged or
the third party was never fetched, and warns when there are no siblings to serve. `dev/hosting_test.py`
checks that every local path `index.html` loads is in that set, so a new asset can't quietly miss it.

`--precompress` writes `.br` and `.gz` siblings for the staged module and for the page's own markup,
styles and scripts, the way `--vendor` does for the third party. That matters more than it looks: the
page's own files are ~840 KB raw and ~155 KB brotli, so serving them uncompressed costs more than the
compressed module does. `--serve` prefers the siblings exactly like a real host should: brotli first,
then gzip, and a sibling older than the file it came from is ignored, so an edit is never hidden
behind a stale one. `Content-Encoding` is set with the original content type, and
`application/wasm` on the `.wasm` so `instantiateStreaming` kicks in. Any static host reproduces
this with its precompressed-asset setting (nginx `brotli_static on;`); a host that re-compresses on
the fly at a low quality wastes most of the win, which is why the siblings are written at build
time. The server also sends the COOP/COEP pair a threaded module would need, so a page that works
under `--serve` works hosted.

### Keeping old module versions around

A breaking module change (a dxc update, a format bump) can invalidate links people shared earlier.
The page can offer a version picker for that: host older builds under `wasm/<id>/OxC3_wasm.js(+.wasm)`
and describe them in `wasm/versions.json`:

```json
{ "versions": [
  { "id": "current", "label": "0.2.104 (current)" },
  { "id": "v0.2.90", "label": "0.2.90 (pre dxc update)" }
] }
```

`id` is the subfolder name (`current` means `wasm/` itself, always first); `label` is what the
picker shows. `js/wasmload.js` reads a remembered pick from localStorage before the module loads
and the picker appears in the status bar only when the file lists more than one version, so a
checkout without any of this (the folder is gitignored) just loads `wasm/OxC3_wasm.js` as before.

## Layout

```
index.html             shell: navbar, 3-mode toolbar, rail, editor, output tabs, problems, statusbar,
                       syntax offcanvas, CLI-map offcanvas
css/app.css            all styling (same visual identity as the original prototype)
wasm/                  OxC3_wasm.js + .wasm and OxC3_wasm_sf.js (embedded), staged by --frontend (not committed)
js/util.js             $, esc, CRC32C (real Castagnoli, matches Buffer_crc32c), byte fmt, LCS line diff,
                       share codec (gzip via CompressionStream)
js/workspace.js        named projects in localStorage: the working one auto-saves, a share link or
                       snapshot import opens as its OWN workspace, pinned ones are never auto-evicted
js/theme.js            appearance: theme palettes (data-ox-theme + Bootstrap color mode + CodeMirror
                       theme), accent/font knobs, persisted in localStorage
js/wasmload.js         decides where the module loads (worker vs in-page for file://), honoring a
                       version picked from wasm/versions.json
js/wasm_rpc.js         proxy that hosts the module in a Web Worker on http(s), so compiles and
                       reflects never block typing; js/wasm_worker.js is the worker side
js/wasm.js             THE MODULE BOUNDARY: the call frame every export answers with, wasm64 pointer
                       marshalling, the project tree #includes resolve against, diagnostics out of the log
js/mock_data.js        GENERATED (dev/gen_mock_data.js): real builtin sources and real oiSH/oiSR/oiSP
                       documents, recorded so the no-module fallback shows something faithful
samples/               THE SAMPLE PROJECT: real .hlsl files, the single source of truth. Edit them here,
                       then `build_web.py --frontend` (or `node dev/gen_mock_data.js` by hand) embeds
                       them, plus fresh recordings, into js/mock_data.js so the page has them at first paint. The desktop suite compiles
                       every one of them for both backends (OxC3_web_samples_test), so a sample that
                       breaks fails CI rather than the demo.
js/mock.js             THE FALLBACK, part 1: heuristic HLSL analyzer producing SHDocument,
                       deterministic fake disassembly/bytes.
js/mock_formats.js     THE FALLBACK, part 2: oiSR symbol ASTs, oiSP pipelines + provenance, offline ISA.
                       Both prefer js/mock_data.js where it has an answer.
js/api.js              THE BACKEND BOUNDARY. Routes every call to js/wasm.js or to the fallback, and
                       carries the SHDocument / SRDocument / SPDocument contracts on top.
js/editor.js           CodeMirror wrapper (HLSL mode + oxc overlay, diagnostics, goto)
js/intellisense.js     PURE CORE: hover, completion, signatures, go-to-definition over SRDocument
js/asmmap.js           PURE CORE: source ↔ disassembly line mapping out of SPIR-V/DXIL debug info
js/tools/compile.js    Compile mode: toolbar → `shader compile` flags, run, Command tab (CLI + DXC lines)
js/tools/inspect.js    SHDocument renderers: reflection tree, oiSH tab, binary strip + disassembly views
js/tools/symbols.js    SRDocument renderer: the `shader reflect-symbols` tree with go-to-definition
js/tools/pipeline.js   SPDocument renderer: derived pipeline, provenance table + supply, refusals, `file data`
js/tools/isa.js        ISA tab: offline AMD ISA per gfx target, the live route explained (native only)
js/tools/diff.js       reflection diff (A↔B), cross-backend diff, binary diff by entrypoint
js/tools/binary.js     standalone SPV/DXIL mode: disassemble / assemble / list entrypoints
js/app.js              state + orchestration: modes, rail, uploads/downloads, problems, share, CLI map
```

Plain scripts with IIFE namespaces (`OxUtil`, `OxTheme`, `OxMock`, `OxMockFormats`, `OxAPI`,
`OxEditor`, `OxIntelliSense`, `OxAsmMap`, `OxCompile`, `OxInspect`, `OxSymbols`, `OxPipeline`, `OxIsa`,
`OxDiff`, `OxBinary`), loaded in that order, no bundler, works from file://.

## The three modes

**Compile**: the in-browser equivalent of `OxC3 shader compile`. Stage/model/extensions live in
the *source* (`[shader]`, `[[oxc::model]]`, `[[oxc::extension]]`, …) exactly like the CLI; the
toolbar only carries the flags the CLI has (`-compile-output`, `--debug`, `--split`,
`--keep-registers`, the three `--warn-*`, `--ignore-empty-files`, and `shader reflect` as the
"Reflection only" switch). The Command tab prints the equivalent CLI line plus the DXC
invocation per compile group, built from the real flag set in `src/shader_compiler/compiler.cpp`.

The output tabs, each labelled with the command it mirrors:

- **Reflection**: `file data` on the oiSH, the backend reflection (registers, dual bindings,
  used flags, oiSB layouts, IO, wave sizes, payload sizes, include CRCs).
- **Symbols**: `shader reflect-symbols`, the *frontend* symbol AST ([oiSR](../docs/oiSR.md)):
  entrypoints with their annotations, user types, resources, parameters (+ the return slot) and
  locals, every node with a source location. Locations are go-to-definition links into the
  editor, which is what the format exists for (outline, hover, go-to-def). Built-in include
  symbols are collapsed per include the way the CLI prints them. It follows the editor
  (debounced) rather than the compile, since it runs on source. The same reflection feeds the editor:
  hovering a name shows its declaration in HLSL spelling (semantics and array sizes included, a struct
  with its base and interfaces, an enum with its enumerators and their values, an enumerator as `Linear = 1`
  with its enum and underlying type as the small print, the underlying type as `aka` when an alias
  differs) and where it lives, and Ctrl+Space (or just typing) completes symbols, struct members (via
  the type graph), HLSL keywords and intrinsics, `[[oxc::` annotations and `#include` targets.
  `--verbose` shows the frontend type behind each alias (`uint : Scalar 1x1`) and node ids.
  **HLSL types** swaps the whole tree over to the builtin spelling instead, so `F64x4` reads as
  `double4` and `float64_t` as `double`; it changes what is displayed, not what is asked of the
  compiler, so the command line does not move.
- **SPIR-V / DXIL**: `file data --bin -entry N`, the stored binary disassembled. Compiled with `-Zi`,
  each disassembly line is tinted by the source line it was born on (read out of OpLine / NonSemantic
  DebugLine on SPIR-V, `!dbg` DILocation metadata on DXIL): clicking one goes to that line in the
  editor, and moving the editor's cursor lights up every line it produced. Without debug info the pane
  says to compile with `-Zi` rather than pretending there is a mapping.
- **ISA**: `isa disassemble -asic <gfx>`. The offline route: the stored SPIR-V compiled with the
  bundled amdllpc for the chosen gfx target (gfx11xx RDNA3, gfx1150 RDNA3.5, gfx12xx RDNA4, the
  `isa devices` list) and disassembled with amdgpu-dis, prepended with the SGPR/VGPR/code/
  scratch/LDS line. Raster, compute and mesh stages only; ray tracing and DXIL have no offline
  path and the tab says so, the way the CLI refuses. The "live" target is explained, not run:
  `-asic live` needs a real Vulkan device (`VK_KHR_pipeline_executable_properties`).
- **Pipeline**: the [oiSP](../docs/oiSP.md) a live ISA run compiles, derived from the oiSH the
  way `SPFile_derivePipeline` does it: compute is exact, ray tracing leaves only `rt.*` to
  supply, graphics reports every field no shader signature can carry (render target formats,
  blend, depth/stencil, rasterizer, MSAA, topology, vertex packing) with *why* and the legal
  domain. Every field shows its provenance (derived / supplied / assumed); editing a value is
  `SPFile_supply` (what `-pso-set` does, the CLI's only override), so you can see what a PSO change does
  before a live run. Two entries of one stage kind are refused with a `-entry` picker, a missing
  vertex or pixel stage gets a stand-in, all as the CLI does. `file header` / `file data` cards
  and the `-pso-output` download (.oiSP) round it off.
- **Command**, **Diff**, **oiSH** as before.

**Inspect oiSH / oiSR / oiSP**: load any of the three ([oiSH](../docs/oiSH.md) compiled shaders,
oiSR symbol ASTs, oiSP pipelines; the CLI sniffs the magic, here the extension is the fallback
for mock bytes). The rail groups them by format; an oiSH opens in Reflection (+ its derived
Pipeline and ISA), an oiSR in Symbols, an oiSP in Pipeline (read-only, `file data` style, with the
descriptor layout the file embeds for each pipeline, its [oiPL](../docs/oiPL.md), beneath the stages). The
Reflection/oiSH/Diff/Combine flows are unchanged: Diff A↔B covers the whole reflection and the
**binary diff by entrypoint** (pairs on entrypoint/lib × stage × extensions × define names,
model excluded so a bumped `[[oxc::model]]` still pairs), "Combine A+B" mirrors
`file combine -format oiSH` including the same-source-hash requirement.

**SPV / DXIL**: standalone binaries outside any oiSH. A loaded binary is **reflected from its
bytes** (the spirv-reflect / DXC container reflection the compiler already runs while building an
oiSH) into a one-binary document, so the regular tabs apply: Reflection (registers, IO, no
identifier), the SPIR-V / DXIL view (`shader disassemble`), ISA (`isa disassemble -input x.spv`
works on a bare `.spv` natively), Pipeline (derived from the reflected stage), Diff A↔B between two
binaries. The **oiSH** tab shows "Assemble into oiSH": give the binary the identifier its source
would have declared (stage, model, extensions, vendors) and it becomes a real oiSH in Inspect
mode, with Combine, pipelines and downloads; that's the planned `shader assemble → oiSH`. The
strip above the tabs keeps `shader assemble` (SPIR-V text → .spv, result loaded as a binary) and
`Compiler_getUniqueEntrypoints` ("List entrypoints", no CLI verb yet).

**Raw DXC** lives in Compile mode's Command tab: every derived DXC line is an editable textarea
with "Run with DXC", plus a free line for anything DXC accepts. It starts from exactly what OxC3
would run, so it's a lens on the gap between OxC3's derivation and DXC's behaviour (`-O` levels,
an extra `-fspv-extension`, `-Zi`), not a second compiler: the output is always a standalone
binary in SPV/DXIL mode, never an oiSH (no annotations processed, nothing reflected into an
identifier). The native counterpart is exposing the real argv (`getCompileArgs`) plus a raw
compile entry; neither is a CLI verb yet, so the CLI map marks it planned, like the reflect and
assemble-to-oiSH entries.

The full command↔UI map ships in-app: navbar → **CLI map**.

## Pipeline overrides

The ISA tab has a **Pipeline state** panel (and the Pipeline tab the same table): every field
the report lists can be supplied, not just render target formats and the recursion depth. Each
supplied field shows up on the CLI line as a `-pso-set "path=value,..."` entry, by the exact
path the report prints, and **Download → .oiSP** is the state `-pso-input` replays; the two
CLI parameters exist for the same reason the panel does, so a single PSO change can be tried
against the ISA. The offline amdllpc route compiles the SPIR-V without pipeline context, so the
panel says which routes the state reaches (live, and the device-free Mesa one).

## Hosts: browser vs VS Code

A VS Code extension doesn't need WASM at all: the Webview is just a browser surface for this page,
the extension host is a Node process that spawns the **native OxC3** (`child_process`), and the two
talk over `postMessage` (binary payloads as `Uint8Array`). Full speed, the real driver, the real
amdllpc. Every `OxAPI` method becomes an RPC to the host instead of a mock/WASM call; `OxAPI.host`
(`acquireVsCodeApi` present) and `capabilities()` are the seam. What the native side needs for
that is a machine-readable output, since the page consumes the SHDocument / SRDocument /
SPDocument JSON contracts: a `--json` (or `-output x.json`) on `file data`, `reflect-symbols` and
`isa disassemble`, which is the same serializer the WASM port needs anyway. Two webview details:
Bootstrap/CodeMirror have to ship as extension-local resources (the webview CSP blocks the CDN),
and files are the natural transport (the host writes the project to a temp dir, or the page
edits real workspace files and passes paths). The public site stays full web: the same page with
`OxAPI` backed by the WASM build instead, at reduced capability (no live device, no spawned
amdllpc; the device-free Mesa route is what closes most of that gap). Both hosts are first-class
behind the one boundary, and nothing in the UI is allowed to depend on which it's on.
The order to build the host in, and what is already in place for it, is under "The VS Code host" in
web/TODO.

Everything on this page is pure compute over bytes except two things, and the page greys them
out instead of pretending. `OxAPI.capabilities()` is the one place that says what the host can
really run:

- `-asic live` (driver ISA + statistics through `VK_KHR_pipeline_executable_properties`) needs a
  GPU. In a plain browser the option is disabled with the reason; hosted in a **VS Code
  webview** (`acquireVsCodeApi` present) the extension can bridge to the native OxC3 on the
  machine, `capabilities().liveIsa` turns true, and the same Disassemble button runs
  `OxC3 isa disassemble -asic live -pso-input … -pso-set …` for real (`OxAPI.isaLive` carries
  the TODO with the message shape). Nothing else in the UI changes between the two hosts.
- The shipped offline ISA path spawns the bundled amdllpc (`SUPPORTS_PROCESS`), which a browser
  can't either. The device-free Mesa route (RADV/ACO for every AMD generation, Intel brw for
  Gen9 to Xe2) is the one that can run as WASM and take the pipeline state; it's prototyped
  (SPIR-V → ISA + stats for compute, graphics and RT) but not in the CLI yet, so in the port
  `isaDisassemble` is either a Mesa build or, in VS Code, the native helper.

Inspect mode ships examples of all three formats (`lighting.v1/v2.oiSH`, `lighting.oiSR`,
`post.oiSR`, `lighting.oiSP`, `post.oiSP`, `trace.oiSP`), derived from the sample project, the
graphics and ray tracing pipelines with a few fields supplied so every provenance shows.

What the newer layers add to that picture: the language features and the line mapping were built as
pure cores exactly so both surfaces can consume them. `intellisense.js` (hover, completion, member
lookup, macro index, go-to-definition, include targets) maps one to one onto
`registerHoverProvider` / `CompletionItemProvider` / `DefinitionProvider`, with only the CodeMirror
glue replaced; `asmmap.js`'s parse feeds `setDecorations` on a virtual disassembly document instead
of tinted divs; the parsed diagnostics (wasm.js's `diagnostics()`, noise filter included) pour into a
`DiagnosticCollection`, and VS Code's own Problems panel, squiggles, history and file tree replace the
page's. The rule that keeps this cheap: page-only orchestration stays in app.js, reusable logic stays
in js/tools, intellisense.js, asmmap.js, wasm.js and api.js, and the dev suites (`frontend_unit`,
`editor_test`, `wasm_smoke`) keep guarding the shared halves for both surfaces. `--no-opt`/`--debug`
become a compile task setting; live ISA lights up on its own because the UI already keys off
`capabilities()`.

## The wasm backend

The module is `src/tools/oxc3_wasm`, built by `build_web.py --frontend` and staged into
`web/wasm/`. It is the shader tooling as a library rather than as a program: no `main()`, the
runtime stays up past its entry point, and `-sENVIRONMENT` is `web,worker,node` with no
`NODERAWFS` (a browser has no host filesystem to bind it to). Every call it exports is the same
library call the CLI makes for the same command, so the page is a second front end onto OxC3
rather than a reimplementation of it.

Three things live between the module and `js/api.js`, all of them in `js/wasm.js`:

- **The call frame.** Every export answers with one allocation, `U64 jsonLength; U64 blobLength;`
  then the JSON and then the bytes, so a call that produces a document, a file, or both is
  decoded and freed the same way. Binaries stay out of the JSON; base64 would cost a third more
  bytes and an encode plus a decode of every SPIR-V module the page looks at.
- **Pointer marshalling.** The build is memory64 and emscripten does not wrap these exports, so a
  pointer crosses as a `BigInt` in both directions. `ccall` is unused for the same reason: it
  can't convert one. Heap views come from `wasmMemory` per use, since growing memory replaces the
  buffer.
- **The project tree.** The page mirrors `{path: {src}}` into the module's filesystem and the
  working directory is set there before the platform is created, so `#include` resolution runs
  through the ordinary `File_read` path. Nothing about it is bridged; it is the code path the CLI
  takes. The `@…` builtins ship inside the compiler and are served through
  `Compiler_builtInIncludeAt`, which is what fills the read-only files in the rail.

Diagnostics come back out of the log: `Compiler_compileShaders` reports them by logging, so a
call captures the module's output and parses `file:line:col: message` back, with the severity
taken from the colour the log writes.

`js/mock.js` and `js/mock_formats.js` stay for the rows in the "not wired" table above and for the
page opened without a module at all; what they serve for the sample project is recorded real output
(`js/mock_data.js`), not a heuristic parse.

## What is still missing

[TODO](TODO) is the working list for this frontend: the next big item (AMD ISA in the browser through
a spirv2isa Mesa fork), the editor intelligence gaps (builtin intrinsics in hover, doc comments), and
the compiler and CLI work the page is waiting on. STATUS.md at the repository root holds the wider
capability table.

## Tests

`dev/wasm_smoke.js` drives the real module through `js/wasm.js`, headless, and is the regression
net for the boundary: the call frame, pointer marshalling, the project tree `#include`s resolve
against, the diagnostics parsed back out of the log, every document serializer, and the routing
`js/api.js` does on top of them. It needs no dependencies and takes the module path as an argument,
so it runs against either staged flavor:

```
python build_web.py -mode Release --frontend --run_frontend_tests
node web/dev/wasm_smoke.js [path/to/OxC3_wasm.js]        # or run it directly
```

`dev/frontend_unit.js` runs the pure cores of `js/asmmap.js` (source to disassembly line mapping, read
out of SPIR-V OpLine / NonSemantic DebugLine and DXIL !dbg metadata) and `js/intellisense.js` (hover and
completion off the oiSR reflection) against fixed inputs, no DOM and no module.

`dev/hosting_test.py` covers what a host serves rather than what the page does: which precompressed
sibling is chosen for an `Accept-Encoding`, that one older than the file it came from is refused, and
that every `vendor/` path `index.html` loads is one the `VENDOR` table produces with a pinned hash.

`dev/editor_test.js` (dev deps via `npm i` in web/, see package.json) asks the HLSL mode what it would
indent the next line by, which is the decision behind every Enter in the editor. It runs the mode
directly: CodeMirror measures character widths to lay a document out and jsdom cannot answer that.

`dev/smoke.js` (same dev deps, run with `node dev/smoke.js`) boots the page headless against the
mocks, compiles, walks all three modes and the Symbols / Pipeline / ISA tabs. It covers the
rendering, which the other suites deliberately don't touch.

`build_web.py --frontend` regenerates `js/mock_data.js` from the module it just built, so the recording
follows the sample project, the built-in includes and the three document contracts on its own;
`node dev/gen_mock_data.js` does the same by hand. The recording stays committed, since the page's first
paint and the module free tests read it without a build; the release job regenerates it and fails when
the committed copy differs, and then runs every test here through `build_web.py --run_frontend_tests`.
