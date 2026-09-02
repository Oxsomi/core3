# Roadmap

Living document: open problems, backlog and hard-won debugging knowledge that should survive any one
person's (or agent's) memory. Update it when an item lands or a new one appears; the per-feature details
belong in graphics_api.md / graphics_spec.md, not here.

Last full revision: 2026-09-02.

## Open issues

- **Linux mesa: Shaders/draw (entryFound) fails on arm64 and x64 static Vulkan.** Deterministic on both
  arches, so it is code or driver capability, not flake. Eliminated already: the DynamicSamplers refusal
  (none of that module's ten shaders declares a sampler, and checkShaderFeatures reads active-only
  extensions), stale entry names, and the union-aliasing class. Next step: the CI log contains a Debug line
  `GraphicsDeviceRef_getFirstShaderEntry(): skipped entry N binary M: <reason>` right above the failure
  (pipeline.c). If no such line exists the binary fell through a silent filter (no SPIRV blob,
  require/disallow, uniforms/defines matching), which points at packaging of the multi-entry files
  (test_draw_mrt_ps / test_logicop_ps / test_draw_dualsrc_ps).
- **Android emulator: push descriptor emulation.** Owned by a separate effort in another checkout; do not
  double-work it. The substance: gfxstream drops VK_KHR_push_descriptor from the guest even where the host
  driver has it, and OxC3 only emulates the device's OWN globals layout (one pre-written set per frame in
  flight, which works because that buffer is fixed for the whole frame); a caller-owned push descriptor
  layout is refused at creation without the extension. The general emulation needs a set allocated per push
  from a per-frame pool, since a caller's push descriptors change per work op.
- **NV returns 1:1 nondeterministically.** Measured on an RTX 3080: the same geometry compacts to ~35-70%
  on most runs and reports no saving on others. The compaction test's staleness asserts are gated on the
  structure actually having moved for exactly this reason; any assert downstream of "compaction shrank" is
  a coin flip without that gate.
- **-fvk-invert-y double flip (latent).** The compiler applies the y flip to vertex AND domain AND geometry
  AND lib compiles, so a VS->GS Vulkan pipeline flips twice. Only decidable at PSO create time (whichever
  stage feeds the rasterizer flips once). Latent until GS/tessellation content exists.
- **Compiler_linkSPIRV entrypoint strip (TODO in code).** Lifting it removes the one-entrypoint-per-file
  constraint on non-RT test/package shaders (multi-entry SPIR-V below 1.4 trips validation, multi-entry
  DXIL lib-linking renumbers implicit cbuffer registers).

## Feature backlog

Roughly ordered by how often the gap bites.

- **Mesh/task shaders have no runtime path** while the feature bit reads true: stages compile and reflect,
  the VK extension is enabled, but there is no dispatchMesh on either backend (explicit TODO in
  graphics_pipeline.c). Widely adopted, both APIs, deprecated by nobody: a gap to fill, not weight to cut.
- **Multi-geometry BLAS.** Both backends hardcode one geometry per BLAS, so EBLASFlag (DisableAnyHit etc.)
  is per-BLAS when both APIs allow per-geometry. Real use cases: car with glass, clothed character, plant
  pot with foliage; today the options are whole-BLAS non-opaque (opaque parts pay anyHit for nothing) or
  splitting into instances (traversal cost, instance slots). Expect the BLAS create surface to change:
  geometry becomes a list and EBLASFlag moves per-geometry; BLASCreateInfo is the place to grow. Overlaps
  with special-index OMM, which stops being the only way to vary opacity within one BLAS once this lands.
- **Frame capture/replay, v1.** A debugging/replay layer on top of OxC3's own abstractions: virtual command
  lists are already API-neutral opcodes plus a resource table, scopes declare their transitions, and oiSH
  ships DXIL+SPIRV so a capture on VK replays on D3D12 (something no below-API tool can do). v1 sequence:
  oiXX serialization (commands, resource descs + contents, pipeline descs), headless replay runner,
  scope-prefix stepping ("jump to scope N" = replay 0..N then pull), then captured golden frames as CI
  regression tests. Known fiddly parts: bindless handles baked into captured data (needs stable table
  allocation order or remapping), GPU-written inputs (indirect args, AS builds), swapchain images
  substituted with RenderTextures. Not a profiler; PIX keeps ISA/waves/timing.
- **Gaps behind green feature bits** (detected and reported but not implemented): WBI on D3D12 (VK exists),
  PerfQuery (Vulkan-only, should generalize cross-API), VRS, BatchedAsyncCommandList, BDA not exposed in
  oiSH.
- **Readback follow-ups:** pulls for mips/arrays/stencil; decide whether CPUReadBit becomes publicly
  creatable.
- **Copy shader multi-region batching** (mainMultiple in image_copy.hlsl is a stubbed TODO): deliberately
  deferred until a real workload passes more than one region; the single-region path is strictly faster at
  one region and dispatches already overlap (no barrier between them).
- **Owned<T> generalization:** the OwnedList free-trait in graphics.hpp could move to the types layer for
  reuse; needs a decision on where a hand-written .hpp lives relative to generate_hpp.py ownership.
- **Per-type descriptor accounting on table create:** Vulkan's pool enforces per descriptor type at
  allocation while D3D12's CBV/SRV/UAV heap is flat, so a type-mismatched reservedDescriptors "works" on
  D3D12 and fails on Vulkan at table create. Generic per-type tracking would make both refuse identically
  at record time.

## Test and CI state

- **Coverage groups A-D are done** (shader execution, formats, capability execution, config variants) and
  the suite is validation-clean on VK + D3D12; per-device teardown asserts zero validation errors AND
  warnings, so regressions fail the suite itself.
- **VK validation layers on CI: installed** (linux lavapipe and osx legs), pinned near the header version
  the conanfile pins; the workflow comment says to bump both together, and that policy is what makes a
  specVersion gate for the newest pNext feature structs unnecessary (a layer can only warn "validation is
  undefined" for structs it predates when it is allowed to lag the headers).
- **Hardware rig (planned):** 7 NVIDIA (one per generation, Maxwell through Blackwell), 3 AMD, 1 Intel Pro,
  dual boot Linux/Windows. The all-adapter loop already runs every enumerated device, so the suite is
  rig-ready; per-format and per-capability modules benefit most.
- **Adding a test file needs a CMake reconfigure**: test/interface globs *.c(pp), so a new file without
  reconfiguring fails the link on missing symbols.

## Debugging lore (expensive lessons, do not relearn)

- **Nondeterministic-across-identical-runs graphics bugs: suspect indeterminate padding or uninitialized
  overlap before drivers or layers.** The 2026-08 flake/hang was tail padding after `UnifiedTexture base`
  (alignas(64) SpinLock) being scribbled by compound-literal copies; the fix pattern is an explicit pad
  member plus `static_assert(sizeof(T) == offsetof(T, base) + sizeof(UnifiedTexture))`, already present in
  device_texture.h and swapchain.h. Any future wrapper embedding UnifiedTexture needs the same assert.
- **Build skew after graphics header changes is real:** the generic lib is statically linked into the exe
  AND each backend DLL, so an incremental build after changing a shared graphics struct can mix layouts and
  produce a deterministic bogus crash. Clean-build OxC3_graphics + both backends + the test before trusting
  such a crash.
- **The D3D12 info-queue drain prints stored messages late** (at the next failure), so message position in
  a log can lie about when the message occurred. Win11's live callback does not have this problem.
- **A fail-fast (stack cookie, heap corruption, debug-layer break with no debugger) kills the process
  before sigFunc, the test summary or the sanitizers see anything.** cdb catches these:
  `cdb -g -G -o -c ".lines; sxd av; g; k 40; q" <exe>` (skip ASan's benign first-chance AV during init).
- **Silent CPU/GPU readback races look like driver behavior.** Distinguish them structurally: make
  impossible values hard errors instead of folding them into a legitimate neighboring case. Worked example:
  a compacted size readback of 0 is never a conformant driver answer (declining to compact returns the
  original size 1:1), so both backends hard-error on it; if that ever fires, the command buffer ordering
  and both completion gate legs are already audited clean.
- **oiSH extension bits are append-only and Count-sized tables live in sh_binaries.c**: adding a bit means
  bumping ESHExtension_Count and BOTH the names and defines tables, in bit order; a merge that lands two
  new bits needs them renumbered sequentially (this collided once already: SubgroupQuad and DynamicSamplers
  both claimed bit 26, and both sides wrote Count = 27, which auto-merged silently wrong).

## Upstream / external

- The D3D12 debug layer OMM input-alignment false positive was confirmed fixed in Agility 1.619.5 but not in
  the 1.721 preview line; the engine floors ASRead alignment based on the accepted SDK version until the
  preview line picks the fix up.
