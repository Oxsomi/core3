# OxC3 Status Matrix

Honest, per-feature maturity so nobody designs against something that doesn't exist yet.
Legend: ✅ implemented + tested · 🟡 implemented, caveats · 🚧 in progress · 📄 spec/design only · ❌ not planned near-term

Last updated: 2026-09-06 (branch `web_frontend`, v3.2.105). Update this table in the same PR as the feature.

## Core (types_*)

| Feature | Status | Notes |
| --- | --- | --- |
| Base types / Error / Buffer / CharString | ✅ | Two error conventions exist; new code uses `Bool + e_rr` (see ARCHITECTURE.md) |
| Atomics / SpinLock / Thread / Time | ✅ | MSVC-ARM64 cycle counter via .s shim |
| SIMD vectors (SSE / NEON / scalar) | ✅ | `I32x8`/`I32x16` are SSE-only internals |
| Arbitrary float format casts (F16/BF16/TF19/…) | ✅ | Software tie-rounding is round-half-away-from-half (not IEEE RNE); hardware paths differ on exact ties |
| Checked numeric casts | ✅ | |
| TList / GenericList / strings / Unicode | ✅ | |
| SHA256 / CRC32C / MD5 / CSPRNG | ✅ | Hardware SHA on supporting CPUs |
| AES256/128-GCM | 🟡 | HW paths: AES-NI, VAES/AVX2, AVX512, ARM AESE. **No software fallback**, CPUs without crypto extensions (some budget ARMv8.0) are unsupported |
| BigInt / U128 | ✅ | |
| AllocationBuffer (GPU suballocator) | ✅ | Non-linear alignment supported |
| JobQueue | ✅ | Deterministic single-thread mode |
| Compression (Brotli) | 📄 | oiXX headers reserve flags; implementation is a disabled WIP. Readers must reject compressed files |
| Generic hash map | ❌→🚧 | Wanted by debug allocator, command-list dedup, compiler caches (TODOs in tree) |

## Formats

| Format | Read | Write | Encryption | Notes |
| --- | --- | --- | --- | --- |
| oiCA | ✅ (streaming) | ✅ | ✅ AES-GCM | 14-file test suite; forward-compat extension blocks |
| oiDL | ✅ | ✅ | ✅ | |
| oiSH | ✅ | ✅ | – | v1.2; golden corpus in shader_compiler tests |
| oiSB | ✅ | ✅ | – | |
| oiSR | ✅ | ✅ | – | Frontend symbol AST; reference validation lives in `SRFile_finalize` |
| oiSP | ✅ | ✅ | – | Pipelines with per-field provenance; embeds oiPL |
| oiPL | ✅ | ✅ | – | Descriptor layout; embedded in oiSP |
| oiBC (Chimera) | 📄 | 📄 | – | Spec draft + stub only |
| BMP | 🟡 | 🟡 | – | BGRA8/BGR8 only, ≤2 GiB |
| DDS | 🟡 | 🟡 | – | Modern DXGI subset; no YUV/depth/legacy |
| WAV | ✅ | ✅ | – | |
| JSON | ❌ | ✅ | – | Write only (`OxC3_formats_json`): every oi format above writes a view of itself, reachable as `file data --json`. A reader is wanted, since nothing reads a document back yet |

## Platforms

| Area | Windows | Linux | OS X | Android | Web (wasm64) |
| --- | --- | --- | --- | --- | --- |
| Platform init / CPU topology | ✅ | ✅ | ✅ | ✅ | ✅ |
| Tracked allocator + leak report | ✅ | ✅ | ✅ | ✅ | ✅ (aligned_alloc, wasm max_align_t is 8) |
| Sandboxed file IO + FileStream | ✅ | ✅ | ✅ | ✅ (assets read-only via AAsset) | 🟡 read-only, NODERAWFS |
| Virtual FS (embedded oiCA) | ✅ | ✅ | ✅ | ✅ (apk `section_*` workaround) | ✅ (packaged oiCA) |
| Window + monitors | ✅ | 🟡 Wayland only (no X11) | ❌ yet | ✅ | 🟡 canvas per window, CPU blit only |
| Keyboard/mouse (multi-device) | ✅ | ✅ | ❌ yet | 🟡 touch story undocumented | ❌ yet |
| Dynamic libraries | ✅ | ✅ | ✅ | ✅ | ❌ needs MAIN_MODULE |
| SIMD | ✅ SSE | ✅ SSE | ✅ SSE/NEON | ✅ NEON | 🟡 SIMD_WASM for vector math, crypto scalar |
| Hardware crypto | ✅ AES-NI/SHA | ✅ AES-NI/SHA | ✅ | ✅ | 🟡 routed to host, needs a Worker + COOP/COEP |

## Graphics

| Feature | Vulkan | D3D12 | Notes |
| --- | --- | --- | --- |
| Instance/device/swapchain | ✅ | ✅ | Side-by-side APIs via dynamic linking (desktop) |
| Virtual command lists + auto transitions | ✅ | ✅ | API-independent recorder (unit tests wanted) |
| Buffers/textures + suballocation | ✅ | ✅ | |
| Descriptor heap/layout/table + bindless | ✅ | ✅ | Android: bindful path missing |
| Compute/graphics pipelines | ✅ | ✅ | Android: render passes missing |
| Raytracing (pipeline + query, BLAS/TLAS) | ✅ | ✅ | Micromap/reorder behind extension flags |
| Mesh shaders / VRS | ✅ | ✅ | |
| Metal | 📄 | — | Enum reserved; needs SPIRV-Cross MSL path |
| WebGPU | 📄 | — | |
| Android swapchain pre-rotation | ✅ | — |  |

## Shader compiler & CLI

| Feature | Status | Notes |
| --- | --- | --- |
| HLSL → DXIL | ✅ | DXC statically linked |
| HLSL → SPIR-V | ✅ | + SPIRV-Tools strip/optimize/disasm, SPIRV-Reflect |
| `[[oxc::...]]` annotations | ✅ | stage/model/vendor/extension/uniforms/binary masks; parsed via DXC reflection |
| Multithreaded batch compile | ✅ | JobQueue, per-thread Compiler |
| Include tracking / sourceHash | ✅ | Hot-reload & incremental-build ready |
| Lib specialization + link (RT/workgraphs) | ✅ | Under active review |
| GLSL / Slang input | ❌ | HLSL-only by design (state in docs) |
| MSL output | 📄 | Blocked on SPIRV-Cross/Shader translator integration |
| CLI: convert/encrypt/hash/inspect/rand/profile/devices/package | ✅ | `-aes` key via argv only, stdin/env/file input wanted |
| CI | ✅ | Passing on all platforms and compilers |

## Web frontend (web/, shader.oxsomi.com)

| Feature | Status | Notes |
| --- | --- | --- |
| Real compiler as wasm64 (worker-hosted on http, in-page on file://) | ✅ | Both flavors staged side by side (two file for hosting, embedded for file://); brotli precompress + serving in build_web.py |
| Mock tier (page works with no module) | ✅ | Recordings regenerated from the real module (gen_mock_data) |
| Compile / Inspect / SPV-DXIL modes, diffs, pipeline + ISA views | ✅ | Offline ISA needs process spawning: browser shows the refusal, native hosts will run it |
| Editor: squiggles, hover, completion, go-to-def, source↔disasm mapping | ✅ | Symbol-driven; builtins hover too: intrinsics off the DXC table (gen_intrinsics), types/semantics/attributes, oxc annotations off the syntax panel; builtin includes reflect when opened |
| "IntelliSense follows" a permutation (defines/uniforms/extensions) | ✅ | Fills from the parse-only listing while editing (`oxc3_parseEntrypoints`); a pick drives the SPIR-V/DXIL views, stale picks flag instead of mismatching |
| Workspaces (share links open as their own; pin/evict) | ✅ | localStorage; the durable form is an .oiCA download |
| Share links (gzip hash, view + line carried) + .oiCA snapshots | ✅ | Truncated links fail loudly; links and snapshots are size-capped against decompression floods |
| Pipeline field editors (enum dropdowns, bit checkboxes) | ✅ | Vocabularies from the compiler's own name tables |
| Mesh/task pipeline derivation | ✅ | Derives like vertex+pixel; live ISA route still refuses mesh |
| Descriptor layouts (oiPL) in the pipeline view | ✅ | Read only: the embedded layout's rows, samplers and push constant, with each row's source; overrides are roadmap |
| Validation gate on arbitrary binaries (upload, assemble, reflect) | ✅ | spirv-val at the module's own version, DXC's validator for DXIL, `shader validate` in the CLI |
| Themes / tabs / uploads / drag-drop | ✅ | |
| Live AMD ISA in browser | ❌→📄 | Needs the spirv2isa Mesa fork (in planning) |

### Roadmap

Near-term, in no particular order; an item leaves this list by landing WITH its tests and docs.

**Compiler & CLI**
- Extension × model validation table (e.g. PAQ below its minimum SM), shared by the CLI and the web
  assemble card so illegal combinations grey out instead of failing late.
- `shader assemble` into a real oiSH (SHFile from binary + identifier); the web round-trip check then
  refuses instead of warning.
- Descriptor layout overrides in the pipeline editor, with assumed fields filling from the shader's
  own root signature when it is the only one.
- Parse-only permutation listing as a CLI verb; the page already gets it from the module's
  `oxc3_parseEntrypoints` (Compiler_parse), so this is the CLI/`--json` half.
- `SBFile_combine` mismatch errors should name the buffer and both sizes.

**Editor intelligence**
- Longer term: signature help while typing, outline, rename/find-references, gated on a DXC
  IntelliSense seam.

**Maintenance**
- Split web/js/app.js into feature modules (share+restore, binary strip, tabs first); new features
  already land as their own modules.
- UBSan (clang, -fsanitize=function) reports calls through mismatched function pointer types in the
  graphics C/C++ interop: the oxc:: C++ twins of C structs make the callback signatures distinct types
  (graphics.hpp free callbacks, device.c pull callbacks). Harmless on current ABIs, but it is 25 reports
  of noise in every sanitizer run.
- Parked: mouse back/forward navigating page state; side-by-side against upstream DXC (an upstream
  binary is too large to fetch casually).
