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
- **AMD's driver fail-fasts tearing a virtual GPU down.** Targeting another ASIC for DXIL ISA works
  (`-asic <name>` resolves the name, hands it to the driver and re-runs itself so the adapter initializes as
  the target), but the second process dies with STATUS_STACK_BUFFER_OVERRUN on the way out, after the
  disassembly is produced. A fail-fast bypasses the child's own reporting, so the parent can only say which
  half of the run the driver reached. Two other quirks are already handled and worth not rediscovering: the
  D3D12 debug layer faults in the same driver as soon as a virtual GPU is selected, and an emulated ASIC
  reports that ASIC's memory rather than the machine's, which the 512 MiB device requirement rejected it
  over. All three want a discrete AMD card to check whether they are specific to an integrated adapter.
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
- **Nothing reads the .mtl an obj names.** `Obj_read` numbers materials by first use of `usemtl` and puts
  the index in the triangle word, and that is the whole of it: `mtllib` is skipped and the names are dropped,
  so a consumer can tell the runs apart and cannot tell what any of them is. What a renderer would actually
  shade with lives in the .mtl: `Kd`, `Ks`, `Ns`, `Ni`, `d`, `illum` and the `map_*` texture references. The
  gap is a second parser plus a list of named material records the indices point into, which is also what
  makes keeping the names worth anything. Textures can lag behind the scalars. The scalars alone are the
  difference between a consumer inventing a material and the file stating one.
- **A reader does not sort triangles into material runs.** A mesh with several materials therefore cannot go
  straight into a BLAS. One geometry per run is what `GeometryIndex()` resolves against, and a BLAS now takes
  a list of geometries each carrying its own flags, so the consuming half is in place. The producing half is
  not: triangles come out in file order, a run is contiguous only where the file happened to be written that
  way, and the material stays hidden in the per-triangle word. What is missing is a stable sort by material
  index with the index stream permuted to match, and a first/count per material that a consumer hands
  straight to the BLAS geometry list. It belongs in the reader, since it is the same work for every consumer
  and the cache-order pass below has to run after it. The ranges have nowhere to live in the flat form today,
  so the form grows a submesh table before the sort has anywhere to put its result.
- **The mesh readers still pay per record where they could pay per block.** The binary PLY path now decodes
  in place out of the cursor's window against a plan resolved once per element, and the sink stages records
  into a block rather than handing each one to the cursor. What is left is measured rather than assumed:
  on Lucy's 28M triangles a read of positions and indices alone is 1.39 s, attributes add 0.35 s, and both of
  the expensive parts have moved OUT of the reader, since `MeshFlat_computeWords` and `MeshFlat_computeNormals`
  let a caller derive them afterwards (1.19 s and 1.23 s of reader time respectively, and 117 ms together on a
  3070). What remains inside is the parse itself.
  Ascii is untouched and cannot use any of this, since a text value has no width until it is parsed.
- **A device resource's upload source is a BUFFER, so every upload materializes on the host first.**
  `DeviceBuffer` and `DeviceTexture` both hold a `cpuData` Buffer, and a flush copies out of it into mapped
  memory. So the bytes exist twice on the host before they exist on the device, and a resource cannot be
  larger than what the host is willing to hold. Measured on a 533 MB mesh: the flush runs at 7.2 GB/s on a
  link that does 24, because what it is timing is a single threaded `Buffer_memcpy`, not a transfer.

  The fix is to make the SOURCE a stream rather than a buffer, and it needs no change to `OxStream` at all:
  `StreamFunc` is already `(stream, offset, length, Buffer dst)`, so a flush can read a dirty range straight
  out of the source into mapped memory, and `EStreamType_DisableSeek` already marks the sources that cannot
  serve a range. Dirty marking does not change either: `markDirty(offset, count)` keeps its meaning, its
  interval merge and its 256 byte rounding, and only what it pulls from changes.

  What that buys, in order:

  - **Nothing materializes.** A file backed source reads the range it needs into GPU visible memory and the
    resource never exists on the host. The same holds for an archive entry or an encryption stream, which
    then decompress or decrypt into mapped memory rather than into a buffer that is then copied.
  - **An update stops being a recreate.** Mark the range that changed and only those bytes are read and
    transferred. Today a not CPU backed resource only accepts a first frame whole resource mark, which is
    what forces a recreate.
  - **`CPUBacked` becomes one case of the general one**, a source that happens to be a MemoryStream, rather
    than a separate path through the same function.

  Three things it has to be honest about:

  - **The read moves onto the submit path.** Today the I/O has already happened by the time a flush runs; with
    a stream source it happens during it, so a slow source stalls where it used to stall at create. That makes
    an async or prefetching variant the follow up rather than an optional extra, and it is the reason to do
    the buffer case before the texture one.
  - **Textures keep their staging copy, until host image copy lands.** An optimally tiled image has a layout
    the driver owns, so a source cannot be read into it directly today; the stream fills the staging buffer
    and the copy does the tiling. `VK_EXT_host_image_copy`, core in Vulkan 1.4, is the feature that removes
    even that: `vkCopyMemoryToImageEXT` writes host memory into an optimally tiled image with the driver
    swizzling on the CPU, it reports whether doing so is actually a good idea on the device
    (`optimalDeviceAccess`) and which layouts it will take, and D3D12's `WriteToSubresource` is the UMA twin.
    Neither is wired in core3 yet. Wiring it makes the texture path symmetric with the buffer one and turns
    the tiling into CPU work, which is the next entry's problem rather than this one's.
  - **A compressed or encrypted source has its own block size**, so a dirty range has to round out to it the
    way it already rounds out to 256 bytes. That is a property the stream should report rather than something
    the graphics layer should guess, and `encryption_stream` already chunks.

  Readback is unaffected and still needs `CPUBacked`: a stream source describes the upload direction only.

- **A submit that reads its sources on the host has no way to use more than one core.** Once the source is a
  stream, a flush stops being a memcpy and becomes whatever the source costs: an encryption stream is a block
  cipher per chunk, a compressed entry is a decode, and a host image copy is a swizzle the driver runs on the
  CPU. All of it is per block and independent, and all of it currently happens on one thread inside submit.

  So submit should TAKE a `JobQueue *`, optional, and fan the pending ranges across it when the work is worth
  it. Passing it to the submit rather than holding it on the device is the right end for two reasons: the work
  begins and ends inside that call, so a borrowed queue needs no ownership and cannot outlive anything, and an
  engine that already runs a job system hands over the SAME one, so core3's threads interleave with its rather
  than competing with a second pool for the same cores. `JobQueue_push` and `JobQueue_wait` in
  types/container are already the whole API this needs.

  Four things it has to get right:

  - **The threshold is not bytes.** A MemoryStream flush is a memcpy and wants a high one; an encryption
    stream costs tens of times more per byte and wants a much lower one. So the STREAM reports a cost hint
    beside the block size the entry above already wants from it, and the rule is bytes times cost, which
    stops the graphics layer guessing about sources it does not understand.
  - **Splitting across pending ranges is not enough.** The common case is ONE range, a whole resource on its
    first frame, so a range has to split into block aligned pieces or the parallelism never applies to the
    upload that actually hurts.
  - **Concurrent reads have to be DECLARED, not assumed.** `StreamFunc` takes an explicit offset, so the
    interface is positional and an implementation can be safe; whether one is depends on whether it seeks
    before it reads. A stream without the flag is serialized against itself and still runs in parallel with
    other streams. Getting this wrong tears a read, and a torn read looks like data rather than like a crash.
  - **The join is before the command lists go.** Fan out, wait, then submit. That is a sync point where one
    already exists, so it costs nothing structurally.

- **The derived passes have no GPU twin in core3, by design, and no test proves the two agree.**
  `MeshFlat_computeNormals` and `MeshFlat_computeWords` are the reference: a device
  runs its own compute version of each, because a big mesh wants the parallel one and OxC3 gfx ships a shader
  only when the shader implements an OxC3 command. What is missing is the bridge that makes the pair
  trustworthy: a test fixture that takes a mesh, both results and reports where they differ, so an engine can
  check its shader against this without writing the comparison itself. Without one, a caller can only compare
  what the two passes RENDER to, which averages a per vertex disagreement away and is the weaker statement.

- **`MeshFlat` is resident where three of its four streams do not need to be.** A reader's output is streams
  throughout, and then the derive layer takes `Buffer`s, so a caller that wants `computeNormals` or
  `computeWords` has to materialize the whole mesh. That is not inherent. Of the four, only POSITIONS are
  random access, and only because a triangle names three arbitrary vertices; indices are read in order and the
  attribute and triangle outputs are written in order.

  Even the positions are seekable where the body is FIXED STRIDE, which a binary PLY's is and a text PLY's or
  an OBJ's is not, so the split is by source rather than by pass. `StreamFunc` already takes an explicit
  offset and `StreamCursor` already holds a window, so a seeking read of a position is a cursor read and not a
  new mechanism; what is missing is the header that reports the stride, which is the same prerequisite the
  range read below wants.

  So: the two sequential outputs become `StreamRef` like every other reader output, the indices become a
  cursor read, and the positions take a cursor whose window is large enough that a coherent index list hits it
  (which is what the cache-order pass below is for). A source that cannot seek materializes positions alone
  and says so, rather than the caller materializing all four. The win is that a 28M triangle mesh can have its
  normals and words derived without 670 MB of host residency, on the path where the device is not doing it.

- **Cache-order the flat mesh.** A pass over a reader's flat output, in types/mesh beside the readers, that
  reorders triangles for the post-transform reuse cache and then renumbers vertices in first-use order. The
  cache is a small FIFO on both vendors, AMD's the narrower, so the order is found by simulating one: Forsyth's
  linear-time scoring (a vertex scores high near the front of the simulated cache and high when few of its
  triangles remain, the three newest slots score flat so the greedy pick does not degenerate into a strip) or
  Tipsify; meshoptimizer's optimizeVertexCache followed by optimizeVertexFetch is the reference. A random order
  transforms two to three vertices per triangle, an ordered one about 0.65, and a vertex-bound pass
  follows that ratio. The order also decides how coherent per-triangle hit records are for coherent rays and
  how well AMD's BLAS builder pairs edge-sharing triangles. Sort into material runs first, stably, so a raster
  twin's one draw per run keeps its ranges. Needs the whole index list, so it is a pass after the read and not
  a read flag. Ships with a test that reports the simulated miss ratio before and after on a known mesh and has
  been seen to fail on an unordered input.

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
- **clang-cl ASan can fail to start at all on a new Windows build, and it surfaces as a build error.** On
  Windows 11 26200 every ASan instrumented binary segfaults before main, printing
  `interception_win: unhandled instruction at <addr>: 44 0f b6 1a 4c 8b d2 48` first: the runtime cannot
  decode the prologue of a system function it wants to hook, gives up, and faults. What is seen first is
  not that, it is `MSB8066 ... exited with code -1073741819` on the packaging rules, because those run a
  freshly built host tool as part of the build. Confirm in one command rather than reading the build log:
  run any instrumented test binary with no arguments, and if it faults too, nothing in the tree is
  implicated. The `windows_hook_rtl_allocators=0` and `windows_hook_legacy_allocators=0` ASAN_OPTIONS do
  not help. A newer LLVM is the fix; meanwhile clang-cl without `-asan` still gives the compiler coverage
  that is the reason to run it on Windows, and the sanitizer legs are Linux's anyway.
- **A lost device makes GPU-AV report nonsense, and the nonsense looks like the bug.** An unaligned shader
  binding table (the SBT base was not aligned to shaderGroupBaseAlignment) lost the device mid-suite on
  NVIDIA. What the log showed was not that: it was GPU-AV claiming `(set = 1, binding = 4) Descriptor index
  0 is uninitialized` for `_rwBuffer`, bindless dispatches reading back wrong values, and thirty-odd
  unrelated modules failing on submit afterwards. Turning the layer off made the suite pass, which reads as
  "the layer is at fault" and is exactly the wrong conclusion: it only removed the instrumentation that made
  the misalignment fatal. Suspect a real fault before the layer whenever a device is lost, and confirm a
  layer verdict by fixing the fault rather than by silencing the layer.
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
