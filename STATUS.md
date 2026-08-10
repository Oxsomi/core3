# OxC3 Status Matrix

Honest, per-feature maturity so nobody designs against something that doesn't exist yet.
Legend: ✅ implemented + tested · 🟡 implemented, caveats · 🚧 in progress · 📄 spec/design only · ❌ not planned near-term

Last updated: 2026-08-04 (branch `android`, v3.2.103). Update this table in the same PR as the feature.

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
| oiBC (Chimera) | 📄 | 📄 | – | Spec draft + stub only |
| BMP | 🟡 | 🟡 | – | BGRA8/BGR8 only, ≤2 GiB |
| DDS | 🟡 | 🟡 | – | Modern DXGI subset; no YUV/depth/legacy |
| WAV | ✅ | ✅ | – | |

## Platforms

| Area | Windows | Linux | OS X | Android |
| --- | --- | --- | --- | --- |
| Platform init / CPU topology | ✅ | ✅ | ✅ | ✅ |
| Tracked allocator + leak report | ✅ | ✅ | ✅ | ✅ |
| Sandboxed file IO + FileStream | ✅ | ✅ | ✅ | ✅ (assets read-only via AAsset) |
| Virtual FS (embedded oiCA) | ✅ | ✅ | ✅ | ✅ (apk `section_*` workaround) |
| Window + monitors | ✅ | 🟡 Wayland only (no X11) | ❌ yet | ✅ |
| Keyboard/mouse (multi-device) | ✅ | ✅ | ❌ yet | 🟡 touch story undocumented |
| Dynamic libraries | ✅ | ✅ | ✅ | ✅ |

## Graphics

| Feature | Vulkan | D3D12 | Notes |
| --- | --- | --- | --- |
| Instance/device/swapchain | ✅ | ✅ | Side-by-side APIs via dynamic linking (desktop) |
| Virtual command lists + auto transitions | ✅ | ✅ | API-independent recorder (unit tests wanted) |
| Buffers/textures + suballocation | ✅ | ✅ | |
| Descriptor heap/layout/table + bindless | ✅ | ✅ | Android: bindful path missing |
| Compute/graphics pipelines | ✅ | ✅ | Android: render passes missing |
| Raytracing (pipeline + query, BLAS/TLAS) | ✅ | ✅ | Micromap/motion-blur/reorder behind extension flags |
| Mesh shaders / VRS | ✅ | ✅ | |
| Workgraphs | 🚧 | 🚧 | Lib-link specialization actively being fixed (see recent commits) |
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
