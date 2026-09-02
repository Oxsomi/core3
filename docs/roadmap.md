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
  (test_draw_mrt_ps / test_logicop_ps / test_draw_dualsrc_ps). Confirmed adjacent on isa_and_sr_file: several
  graphics stages in one file DO compile to a single SPIR-V module carrying several OpEntryPoints, and the
  Vulkan backend was binding whichever came first instead of the stage's own (fixed in vk_graphics_pipeline.c
  by preferring the name the oiSH recorded, then the stage's execution model). That is a later layer than
  getFirstShaderEntry so it is not this bug, but the multi-entry premise itself is no longer a guess.
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
- **A DXC object that misbehaves only on its SECOND use in a process is an allocator mismatch, and an ASan
  build cannot see it.** DXC overrides global operator new/delete to route through a thread-local IMalloc, so
  every entry point that allocates must open with `DxcThreadMalloc TM(m_pMalloc)`. The reflector's `FromBlob`
  did not, so the reflection object and all its tables were allocated against whichever allocator the calling
  thread happened to carry and freed against another; symptoms were `DLFile_addEntryString: not a valid string`
  and a fault reading a `std::string` through an index `Deserialize` had provably validated. The trap:
  packages/dxc sets `DXC_DISABLE_ALLOCATOR_OVERRIDES` when enableASAN, so ASan turns the broken mechanism OFF
  and comes back clean. A clean ASan run next to an MSVC-Release crash is evidence FOR this bug, not against
  it. Fixed in the fork @044649868 (dxc/2026.08.23). Third instance of this class after 5fed79f4 and the
  DxcCreateInstance2 custom-IMalloc attempt.
- **Wiping conan packages leaves .dep_hashes.json lying, and the error names the wrong culprit.**
  build_common's conanCreateIfChanged skips a package whose hash is unchanged, so after a manual cache clean
  you get `ERROR: Package 'X/version' not resolved` together with `-- Skipping packages/X, unchanged` in the
  same log. Delete that package's entries from .dep_hashes.json (or the whole file after a full clean) rather
  than reaching for --force_deps, which rebuilds everything including DXC.
- **oiSH extension bits are append-only and Count-sized tables live in sh_binaries.c**: adding a bit means
  bumping ESHExtension_Count and BOTH the names and defines tables, in bit order; a merge that lands two
  new bits needs them renumbered sequentially (this collided once already: SubgroupQuad and DynamicSamplers
  both claimed bit 26, and both sides wrote Count = 27, which auto-merged silently wrong).
- **Deleting an ESHExtension bit silently re-points anything that mirrored the bit VALUE.** Addition
  collides loudly (previous point); removal does not. RayMotionBlur was deleted and Barycentrics took bit
  11, so spirv2isa, which mirrored ESHExtension values into its own gate, began rejecting every
  barycentrics shader as "ray motion blur is not supported"; nothing failed to build and every test stayed
  green, because a mirrored constant cannot notice that it went stale. Consumers outside this repo now own
  their vocabulary (mesa/src/spirv2isa_features.h) and we translate into it, naming each ESHExtension_* in
  the translation so a deletion is a compile error there, with a Count static_assert beside it so an
  addition is one too.
- **Do not gate a feature you have not observed to fail.** Marking a spirv2isa feature unsupported off a
  single crashing shader was wrong three times running: bindless, ray-tri-position and descriptor-heap all
  compiled fine once the gate was bypassed, and 6 of the 7 bindless shaders had been passing the whole
  time. A gate is a claim about the backend, so check it with the gate off before adding it, and re-check
  the existing ones whenever a stage or lowering lands, because wiring the Intel RT stages made two
  verdicts stale the moment they started working.
- **The same read appearing in three places will diverge, and the odd one out fails quietly.** Reading an
  oiCA entry has to handle both shapes an archive holds data in, loaded buffer or still stream-backed.
  file_util.c and convert_oiCA.c each grew their own correct copy; inspect_data.c grew neither and just
  called CAFile_getDataConst, ignoring the isValid it wrote into, so `file data -entry N -output` wrote a
  0-byte file and exited 0 for any stream-backed entry (found on a 1.25 MB pathtrace.oiSH; every smaller
  entry in the same archive extracted fine). Now one CLI_openArchiveEntry in cli.h, used by all of them,
  handing back a stream for both shapes rather than a materialized copy.

- **A RefPtr stores a POINTER to its RefPtrType, so a type made as a local dies before the object does.**
  `MemoryStream_makeType`/`FileStream_makeType`/`FileHandle_makeType` return a RefPtrType BY VALUE and
  RefPtr_create stores `&` it, so the type has to outlive every RefPtr built from it. A helper that builds
  the type on its stack and RETURNS the stream leaves `ptr->refPtrType` dangling, and the fault lands in
  RefPtr_dec on a later, innocent looking line rather than anywhere near the helper. This is why
  File_openStream takes fileHandleType and streamType as parameters instead of making them itself: any
  function that hands a RefPtr back to its caller has to take the type from the caller too. Making the type
  locally is only correct when the RefPtr is also released before that function returns.
  Release tolerated the dangling read and passed the whole CLI suite; the Debug build faulted immediately,
  which is the difference between the two worth remembering before calling a lifetime change verified.

- **Wrapping a long string inside an array initializer is a clang -Werror, and MSVC never says a word.**
  `-Wstring-concatenation` reads two adjacent string literals in a brace initializer as a probably missing
  comma. The rule is indentation, confirmed against clang 16: it fires when the continuation line is
  indented DEEPER than the element it belongs to, and stays quiet when the continuation sits at the same
  indent as the element (which is why the multi-line HLSL sources in test_shader_compiler_driver.c are
  fine). Wrap the element in parentheses to say the concatenation is deliberate, which is also what the
  compiler's own note suggests. Splitting a string to satisfy the 128 column limit is what walks into this,
  so it only shows up on the clang leg of CI, long after the MSVC build looked clean.

## Upstream / external

- The D3D12 debug layer OMM input-alignment false positive was confirmed fixed in Agility 1.619.5 but not in
  the 1.721 preview line; the engine floors ASRead alignment based on the accepted SDK version until the
  preview line picks the fix up.
- **amdgpu-dis is redundant: amdllpc disassembles itself.** `--filetype=asm --show-encoding` emits the ISA as
  text with per instruction encodings, from which the addresses and code size are derivable, so the ELF plus
  second tool are unnecessary for what OxC3 needs. Both are LLVM with the AMDGPU backend, vendored in the RGA
  tree from separate directories (external/vulkan_offline and external/lc).
- **The LLPC repository is archived (2025-09-15), so building amdllpc from source is not a practical route
  for us.** Its own docs build it as part of AMDVLK (`repo init -m build_with_tools.xml`, then llpc + xgl +
  pal + an LLVM fork + glslang + SPIRV-Tools) and say it "pretty much has the same requirements as an LLVM
  build"; macOS is never mentioned. The shipped binary is 114 MB and accepts the gfx11xx/gfx12xx targets,
  which is narrower than the range the offline corpus wants to cover. RGA is where we obtain it, which is why
  packages/radeon_gpu_analyzer exists at all even though none of RGA itself is built or shipped.
- **A portable (macOS/Android/wasm) AMD ISA path needs two halves, and the disassembler half is the one that
  cannot be avoided.** Compile is SPIR-V -> machine code, print is machine code -> text. ACO (Mesa, MIT,
  actively maintained, GFX6-GFX12) is the only viable portable compiler half, but it ships no disassembler:
  aco_print_asm.cpp either calls LLVM's `LLVMDisasmInstruction` under AMD_LLVM_AVAILABLE or shells out to
  clrxdisasm, and CLRX
  stops at GCN/Vega so it cannot do gfx11/gfx12. So ISA text always needs LLVM's AMDGPU MC disassembler
  (~20 MB, builds fine on macOS/Android) whichever compiler produced the code. The two halves are one project:
  the disassembler alone disassembles nothing, and today's src/amd/spirv2isa folds into RADV and pulls
  libdrm_amdgpu, LLVM and Vulkan WSI, so carving ACO free is real work rather than packaging.
