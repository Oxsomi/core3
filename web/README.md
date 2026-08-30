# shader.oxsomi.com — web frontend for OxC3's shader tooling

A static, dependency-free (CDN Bootstrap + CodeMirror only) frontend for `OxC3 shader …`,
`OxC3 file …` and `OxC3 isa …`, organised so the WASM port of the real compiler drops into
**one file**.

Open `index.html` directly (file:// works) or serve the folder. Everything runs against a
mock backend until the port lands, the yellow **mock** badge in the status bar marks that.

## Layout

```
index.html             shell: navbar, 3-mode toolbar, rail, editor, output tabs, problems, statusbar,
                       syntax offcanvas, CLI-map offcanvas
css/app.css            all styling (same visual identity as the original prototype)
js/util.js             $, esc, CRC32C (real Castagnoli, matches Buffer_crc32c), byte fmt, LCS line diff
js/mock.js             THE MOCK, part 1: sample project, heuristic HLSL analyzer producing SHDocument,
                       deterministic fake disassembly/bytes.
js/mock_formats.js     THE MOCK, part 2: oiSR symbol ASTs, oiSP pipelines + provenance, offline ISA.
                       Delete both when the wasm port lands.
js/api.js              THE WASM BOUNDARY. Only file the port touches; the SHDocument / SRDocument /
                       SPDocument contracts on top, every method carries a TODO(wasm) + the native call.
js/editor.js           CodeMirror wrapper (HLSL mode + oxc overlay, diagnostics, goto)
js/tools/compile.js    Compile mode: toolbar → `shader compile` flags, run, Command tab (CLI + DXC lines)
js/tools/inspect.js    SHDocument renderers: reflection tree, oiSH tab, binary strip + disassembly views
js/tools/symbols.js    SRDocument renderer: the `shader reflect-symbols` tree with go-to-definition
js/tools/pipeline.js   SPDocument renderer: derived pipeline, provenance table + supply, refusals, `file data`
js/tools/isa.js        ISA tab: offline AMD ISA per gfx target, the live route explained (native only)
js/tools/diff.js       reflection diff (A↔B), cross-backend diff, binary diff by entrypoint
js/tools/binary.js     standalone SPV/DXIL mode: disassemble / assemble / list entrypoints
js/app.js              state + orchestration: modes, rail, uploads/downloads, problems, share, CLI map
```

Plain scripts with IIFE namespaces (`OxUtil`, `OxMock`, `OxMockFormats`, `OxAPI`, `OxEditor`,
`OxCompile`, `OxInspect`, `OxSymbols`, `OxPipeline`, `OxIsa`, `OxDiff`, `OxBinary`), loaded in
that order, no bundler, works from file://.

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
  (debounced) rather than the compile, since it runs on source. `--verbose` shows the frontend
  type behind each alias (`uint : Scalar 1x1`) and node ids.
- **SPIR-V / DXIL**: `file data --bin -entry N`, the stored binary disassembled.
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
Pipeline and ISA), an oiSR in Symbols, an oiSP in Pipeline (read-only, `file data` style). The
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

## Porting to WASM

1. Build the module exposing thin C wrappers over: `Compiler_create/free/shutdown`,
   `Compiler_compileShaders` (all in-memory: `allFiles`/`allShaderText`/`allBuffers`),
   `Compiler_reflect` (→ `SRFile`), `Compiler_disassemble`, `Compiler_assemble`,
   `Compiler_getUniqueEntrypoints`, `SHFile_read/write/combine`, `SRFile_read/write`,
   `SPFile_derivePipeline/supply/validate/print/finalize/read/write`,
   `SpvISA_listSupportedTargets/disassemble` (or the Mesa port), plus serializers from
   `SHFile` / `SRFile` / `SPFile` to the **SHDocument / SRDocument / SPDocument** JSON contracts
   documented at the top of `js/api.js` (they mirror the C structs, so it's mostly a walk).
2. Replace the bodies in `js/api.js`, each method already contains the proposed
   `Module.ccall` line and names the native function it stands for. Keep everything async;
   moving the module into a worker later changes nothing upstream.
3. Bridge include resolution to the in-memory project tree (`{path: {src}}` passed to
   `compileFile` / `reflectSymbols`); the `@…` builtins ship inside the compiler already
   (`types`/`resources` are umbrellas over `mat`, `indirect`, `fixed_point`, `buffer`, `appdata`).
4. Delete `js/mock.js` + `js/mock_formats.js` and their `<script>` tags; drop the **mock** badge
   in `index.html`. `js/tools/compile.js` uses `OxMock.analyze` once for the *preview* in the
   Command tab, swap that for the port's parse step (or a `doc` from the last compile).

Faithful vs fabricated, so nobody trusts the wrong thing: constants (stages, the 26
extensions, backend restrictions, the 12 vendors, builtin names, DXC flags, CLI names/flags,
version 3.2.102, oiSH/oiSR/oiSP header fields, ESRNodeType/ESRFeature names, the oiSP field
table with its reasons and legal domains, the offline ISA target list, the CLI's refusal and
NOTE wording) come from the repo; everything *derived* (diagnostics, binding assignment, sizes,
hashes-of-mock-content, disassembly and ISA text, register statistics, symbol locations of the
heuristic parse, parsed uploads) is fabricated deterministically by the mocks.

## Tests

`dev/smoke.js` (needs `npm i jsdom`, run with `node dev/smoke.js`) boots the page headless,
compiles, walks all three modes and the Symbols / Pipeline / ISA tabs, and asserts 60
behaviours, useful as a regression net while wiring the wasm module.
