# OxC3 graphics usability review

Review of 2 Oct 2026, from reading core3's docs, headers and tests and an application built on the C++ wrapper.
Nothing was built or run. Line numbers refer to core3 at 3.2.106. The roadmap carries one line per item and
points here for the detail.

## 0. Corrections to earlier assumptions (no items)

- **Android**: graphics runs where dynamic rendering exists. Render passes are still wanted there.
- **macOS**: runs headless through MoltenVK. The gap is windowed presentation and a native Metal backend.
- **Ray payloads**: capped at the U8 ceiling, 255 bytes, checked for 2 byte alignment
  (`src/graphics/generic/raytracing_pipeline.c:203-225`).
- **Scope read/write exclusivity is by design**: one transition per subresource per scope is what lets scopes run
  in parallel implicitly. Document the reason (section 6).
- **Missed transitions can't be detected inside OxC3**: shaders reach resources through opaque bindless handles.
  Commands with explicit resources already transition themselves (clear and copy in `command_list_cmds.c:194/397`,
  render targets in `command_list_render.c:434-476`). Only bindless shader access relies on the validation layers,
  and GPU-assisted validation has gaps (Vulkan GPU-AV needs SPIR-V 1.4+ to instrument fully; D3D12 GPU-based
  validation fails on ray tracing state objects).

## 1. Build and integration

- **Dynamic graphics linking as the default**, with a Platform init per DLL. Static linking stays as an opt-in,
  for performance (no cross DLL calls, whole program optimisation) and for targets that prefer it, likely Android.
  Dynamic by default gives one link shape on desktop, both backends on Windows, and no profile dependent package
  ids leaking into consumers' build scripts. Document the static path: which backends it carries per platform
  (static Windows is D3D12 only today) and how a consumer picks it. CI keeps building both link modes.
- **Fixed: shader-only edits never reached an ELF binary.** The ELF branch now adds the .oiCA to LINK_DEPENDS
  like macOS, and the package targets declare it as a byproduct so a fresh Ninja tree has a rule for it.
  As found: in `cmake/oxc3.cmake:390-395` the .oiCA files are appended by a POST_BUILD objcopy, which only runs when the target relinks, and nothing makes the .oiCA a link input on the
  ELF branch. macOS adds LINK_DEPENDS (`:375`) and Windows uses OBJECT_DEPENDS on the generated .rc (`:411`). The
  comment at `:410` ("POST_BUILD objcopy that runs every build") is wrong. Fix: `set_property(TARGET ${target}
  APPEND PROPERTY LINK_DEPENDS "${file}")` in the `UNIX AND NOT ANDROID AND NOT EMSCRIPTEN` branch (`:380`).
  Consumers can then drop their delete-the-executable workarounds.
- **Fixed: the Windows .rc dependency broke Ninja Multi-Config.** OBJECT_DEPENDS on the generated .rc named the
  .oiCA paths with a literal `$<CONFIG>`, since OBJECT_DEPENDS doesn't evaluate generator expressions. The .rc is now
  the OUTPUT of a custom command that copies a `file(GENERATE)` template and DEPENDS on it and every .oiCA, both of
  which evaluate `$<CONFIG>`; a rebuilt package recopies the .rc, so the resource compiler reruns.
- **A missing `//OxC3_graphics` virtual dependency fails only at runtime.** Fail at configure time, or make it
  implicit when linking graphics.
- **Case sensitive `find_program(OxC3)`**: `oxc3` only works on Windows.
- **Recipe skew**: `conan create` clones GitHub, not the local tree, so local core3 edits are invisible to
  consumers until pushed. A local path or editable mode removes the need for working tree export scripts.

## 2. Silent trap: a defaulted Bool directly before Error* (C++ wrapper)

**Fixed.** Every Bool parameter of the C++ wrappers (graphics.hpp, the other hand-written headers and the
generated ones) takes `oxc::BoolArg` (`include/types/base/bool_arg.hpp`), which accepts a bool and deletes every
other argument type, so the call below no longer compiles. `check_style.py` rejects a Bool parameter directly
before an `Error*` in a C++ header. As found:

`c::Bool` is `bool` (`include/types/base/types.h:75`) and pointer to bool is a standard conversion that
`-Wall -Wextra -Werror` does not catch. Calling `f(..., e_rr)` and forgetting the Bool binds `e_rr` to the Bool,
drops the error channel and changes behaviour. A real incident: `createTlas(..., e_rr)` produced a TLAS without a
bindless descriptor, so every ray missed, and a furnace test still passed.

Sites in `include/graphics/graphics.hpp`: the DescriptorTable set functions' `maintainRef` (519, 529, 541, 562,
586, 593, 614); `CommandList::begin(Bool doClear = true, Error*)` (1259); `createCommandList(..., Bool allowResize =
true, Error*)` (1708); `createTlas(..., Bool disallowBindlessDescriptor = false, Error*)` (1883). Near misses:
`keepSource` (1800), `disallowBindlessDescriptor` (2276).

Fix: a strong argument type that deletes pointer constructors (`struct BoolArg { constexpr BoolArg(bool v);
BoolArg(const void*) = delete; };`) or a flags enum, plus a style-check rule so it can't come back. The C API has no
defaults and is unaffected.

## 3. C++ wrapper (graphics.hpp) gaps

- **Stale header comment**: `graphics.hpp:49-52` and `117-152` say the bindful path, descriptor layout/heap/table,
  pipeline layout and MSAA aren't wrapped; they are (511, 635, 2154-2234, 977-1002, 1822, 2130). DescriptorLayout and
  PipelineLayout (508-509) are empty handle shells with no methods.
- **Missing wrappers applications hit**: static/immutable samplers and the default bindless layout
  (`GraphicsDevice_defaultBindlessLayout`, `DescriptorLayoutInfo_addStaticSampler` / `addImmutableSampler`);
  `pullRegionStream` on buffer, device texture and texture; `GraphicsDeviceInfo_supports*Format`; pipeline
  serialization (`Pipeline_toSPFile`, `createPipelineFromSPFile`); `getPipelineExecutables`; BLAS/TLAS from cache;
  `listShaderTargets` / `selectShaderTarget`; `findResourceByAddress`.
- **Weaker than the C version**: `getFirstShaderEntry` (2485) passes nullptr for defines and uniforms, forcing raw
  C calls; `createSwapchain` (1729) exposes only window and present modes (no format, other SwapchainInfo fields,
  bindless table); `createDepthStencil` (2130) has no bindless table parameter; `submit` / `submitJob`
  (2498/2530) are capped at 16 command lists and 16 swapchains; Device exposes no `submitId` / `completedSubmitId`;
  the Handle base exposes `data()` publicly, which invites reaching into fields.

## 4. What an application ends up doing with c:: (evidence for 3)

Most `c::` use is plain data (enums, Transition, BLASGeometry, TLASInstance, AttachmentInfo, PipelineLayoutInfo,
math), which is fine to share. About 15 sites are forced by the missing wrappers above (static sampler and default
layout, `getFirstShaderEntry` with defines, list frees, BLAS geometry lists, and a reimplementation of `scope()`
that skips null-resource transitions). About 12 reach into internals: `submitId` / `completedSubmitId` /
`framesInFlight` through `RefPtr_data(device)`, `cpuData` through `RefPtr_data(buffer)` in pull callbacks that
receive a `void*`, and acceleration structure sizes through `->data()->base.asBuffer` / `resource.size`.

Proposed accessors: `Device::submitId()` and `completedSubmitId()`; `Blas::bufferSize()` and
`Tlas::bufferSize()`; a typed DeviceBuffer passed to pull callbacks, with `cpuData()`; a defines parameter on
`getFirstShaderEntry`; a skip-null option on `scope()` / `scopeSpan`.

## 5. Hot reload

oiSH already records its includes and a sourceHash, but there is no watcher or reload API. Proposal: a device level
"reload pipelines from package" call that recompiles changed oiSH files in the background (CLI or compiler
library), swaps each Pipeline behind its handle atomically at a frame boundary, and defers destroying the old one
through the existing refcounting. Pairs with the wanted on-disk pipeline cache.

## 6. Documentation (docs/graphics_api.md)

Signature drift (examples don't compile as written):

- `GraphicsDeviceRef_create` example (197-205) misses `reservedDescriptors` (`device.h:408`).
- The functions listing (242-515) drops `GraphicsDeviceRef *` and the `GraphicsDeviceRef_` prefix throughout.
- `startScope` (622-624, 2188) shows 5 by-value arguments; the real call (`commands.h:90-100`) takes list
  pointers, flags, name, predicate and predicateOffset, and 2203 wrongly says dependencies are the last parameter.
- TLAS example (1653-1663): a spurious NULL; `instanceList` and the name passed by value where `tlas.h:175` wants
  pointers.
- Names passed by value instead of `const CharString*` at 869, 910, 949, 984, 1560-1573.
- `getFirstShaderEntry` (1218-1225) shows 6 arguments; the header takes 7 (uniforms missing).
- `startRenderExt` (2000-2006) passes colors and depth-stencil info by value.
- `setViewportAndScissor` (1727) misses `e_rr`.
- `PipelineRaytracingInfo` (1175-1194) lists fields that no longer exist; 1086 names a nonexistent
  `PipelineRaytracingInfoExt`.
- `PipelineGraphicsInfo`: `.stageCount` doesn't exist (1171, 1262); `attachmentFormatsExt` is given
  `ETextureFormat_BGRA8` where it needs `ETextureFormatId`; `depthFormatExt` is `EDepthStencilFormat`.
- `PipelineStage` is described as a Buffer plus a stage (1076-1080, 1171); the real struct
  (`pipeline_structs.h:99`) is binaryId, shFileId, stageType, localShaderId, groupId.
- `EGraphicsResourceFlag_ShaderRW` (981) doesn't exist; `ExposeBindless*` is never documented.
- `ReadASExt` should be `ASReadExt` (1390-1398); Uniform and Predicate are missing from that list.
- `EWindowFlags_IsVirtual` (803) doesn't exist; the real check is `EWindowType_Virtual`.
- `ESamplerFilterMode_*` lives in the oiPL headers, which nothing tells the reader.

Stale or contradicting STATUS.md:

- MSL and WGSL listed as compile types (1352-1360); STATUS says design only.
- `compile.bat` / `compile_debug.bat` referenced (1364) but gone; the CLI does this now.
- Shader include path given as `inc/.../resources.hlsl` and `types.hlsl` (1357-1358); the real files are
  `include/shader_compiler/shaders/resources.hlsli` and `types.hlsli`.
- "Resources require bindless" (1357, 2163) contradicts DisableBindless and caller-owned layouts.
- "Buffering not exposed" (832) is wrong: `EGraphicsBufferingMode` is exposed.
- Mesh shaders and VRS listed as capabilities (139-141, README:81) with no warning that `dispatchMesh` has no
  runtime path.
- The scope dependency docs (2201-2222) present Unconditional as working; roadmap says it emits no barrier.
- The Sampler section (864) says the sampler array is accessible; it is opt-in (221, 2070).

Incomplete examples: `...` placeholders (1214, 1241, 1245, 1275, 1285-1291, 1342); `//TODO: Bind compute shader(s)
and dispatch` (2189); a `**TODO**` on renderPass/subPass (1204) although both fields exist; undeclared variables
(902, 943, 975, 1228, 1272, 1338, 1372, 1493, 1632); shader ownership inconsistent (freeing a "list" at 1233/1278
versus a single `SHFile_free`).

Missing topics: the C++ layer (graphics.hpp is never mentioned, yet it is what applications use); the HLSL side
(resources.hlsli arrays, the handle bit layout, `[[oxc::...]]` annotations); scope rules in one place (why reads
and writes split into scopes, that bindless access is unchecked except by the layers); the readback lifecycle (when
a pull completes, which thread runs the callback, `reserveReadback`); a complete frame loop (create, record,
submit, resize, re-record); virtual/headless window creation; the `//OxC3_graphics` virtual dependency and
CMake/Conan integration including dynamic versus static linking (only README:318-322 today); a short "submit
returned an error / device lost" guide.

Structure: maintainer internals sit in the user reference (the staging and flush formula 233-238,
CreatePlacedResource2 282-284, internal backend functions at 523, the ExecuteIndirect bridging at 2159, internal
flags 695/700, the compaction rules repeated three times at 444-474); concepts are used before they are introduced
(scopes used at 622, explained at 2108; bindless handles before the shader side); RT content is split between
1480-1704 and 2088-2160. Proposal: a user guide (C++ first, tutorial order) and a backend/maintainer reference,
with function listings generated from the headers so they can't drift.

## 7. Samples

There are no standalone samples; the compute, rays and draw tests are the de facto tutorials, buried in harness
helpers. Proposal: a `samples/` folder of small CI-built C++ programs, each under about 150 lines: clear the
swapchain; triangle; compute plus readback; BLAS/TLAS plus dispatchRays; headless (virtual window) render to file;
bindful plus static sampler. Each has its own CMakeLists and conanfile showing the minimal integration, which
doubles as the integration test for section 1; at least one builds in each link mode. Doc snippets are pulled from
the samples so they compile by construction.
