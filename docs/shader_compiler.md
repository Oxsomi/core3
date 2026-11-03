# OxC3 shader compiler

OxC3 shader compiler turns HLSL into the binaries a graphics API takes. It wraps a statically linked DXC, compiles
each entrypoint of a source file to DXIL, SPIR-V or both, reflects what it built and packages the result as an
[oiSH](file/oiSH.md). Around the compile it keeps what a build system and an editor need: the files a shader was
built from, the source level symbols, and for AMD the ISA a SPIR-V binary lowers to.

This page is a map, not a reference: the API is the headers under `include/shader_compiler/` (`compiler.h` and
`spirv_isa.h`), and the byte layouts of what it writes are the specs under [file/](file/), [oiSH](file/oiSH.md),
[oiSB](file/oiSB.md) and [oiSR](file/oiSR.md). Which targets and platforms work is kept in one place, the
[Shader compiler and CLI table in STATUS.md](../STATUS.md#shader-compiler--cli). The command line that drives the
module is [OxC3_tool.md](OxC3_tool.md#compiling-shaders). What a shader author writes (annotations, defines,
includes, semantics) holds however the compiler is invoked, and is the reference at the end of this page, under
[Shader source](#shader-source).

## The module

A `Compiler` is the handle to DXC's interfaces: its utilities, compiler, reflector and include handler.
`Compiler_create` makes one (the first call in a process initializes DXC and concurrent callers wait for it),
`Compiler_free` releases it and `Compiler_shutdown` takes DXC down at exit. A Compiler is not shared between threads:
create one per thread, which is what the batch driver does.

The module links statically by default, or as a shared library (`DynamicLinkingShaderCompiler`, see
[README.md](../README.md)). A shared module has its own `Platform_instance`, so the host calls `Compiler_setPlatform`
once after `Platform_create`; in a static build the call does nothing.

The only source language is HLSL (`ECompilerFormat_HLSL`).

## Reading a source

One source can be read for four things, each with its own call:

- **Entries**: `Compiler_parse` finds the entrypoints and reads their annotations, without compiling anything. This
  is what the compile needs to know before it starts.
- **Symbols**: `Compiler_reflect` walks the source level symbol AST into an `SRFile`, for editor tooling (see
  [Reflection](#reflection)).
- **Binaries**: `Compiler_compile` runs DXC for one entry combination and one backend.
- **Includes**: setting `infoAboutIncludes` in `CompilerSettings` makes a compile also report every file it read (see
  [Includes and incremental builds](#includes-and-incremental-builds)).

`Compiler_disassemble` and `Compiler_assemble` turn a binary into text and back (SPIR-V through SPIRV-Tools, DXIL
through DXC), and `Compiler_getUniqueEntrypoints` lists the entrypoints inside a library binary. The one compile type
is `ECompileType_Compile`.

## The life of a compile

For one source file and one backend, the batch driver runs these steps, and each is also a public call:

1. **Parse.** `Compiler_parse` runs DXC's HLSL reflector over the source as a shader model 6.10 library, with
   `__OXC_PREPROCESS` defined and every extension switched on, so the code behind any extension is visible. A function
   with annotations and a stage becomes an entry. The checks that need no compile happen here: a wave size on an entry
   that is not compute, RayReorder outside a raygeneration entry, uniforms on an entry that is not a `[shader(...)]`
   library entry, and uniforms that differ in name, type or order between entries are all refused.
2. **Group.** `Compiler_getUniqueCompiles` lists which combinations need a compile of their own. A combination is a
   shader model, an extension line and a define line; a uniform row is not part of it, because a row is a link, so
   entries that differ only in uniform values share one compile.
3. **Compile.** `Compiler_compile` makes one DXC invocation for a combination, with its defines, extension macros and
   (for SPIR-V) the target environment. A combination that reaches `OXC_DISABLE_COMBINATION` fails with a type DXC does
   not know, which is read as the combination being disabled: the result is a success with `isDisabled` set and
   nothing compiled.
4. **Process.** `Compiler_process` reads the binary back (D3D reflection for DXIL, SPIRV-Reflect for SPIR-V) into
   registers, shader buffers and the extensions the binary uses. SPIR-V then goes through SPIRV-Tools: debug info is
   stripped unless a debug build was asked for, and the module is legalized, optimized and validated. An extension that
   was declared but not found comes back as a demotion, which the oiSH records as dormant.
5. **Link.** `Compiler_link` runs when a combination has uniforms or a library holds several entries. A uniform is a
   specialization constant in SPIR-V and a library export in DXIL, and its value is fixed here, so a hundred uniform
   rows cost links and not compiles. `Compiler_getLinkEntries` works out which entries and combinations each link
   serves.
6. **Finalize.** `Compiler_finalizeEntrypoint` pushes the entry level reflection (see [Reflection](#reflection)).
7. **Assemble.** The binaries, registers, buffers, include list and entries are registered into an `SHFile`. Files
   with the same output are merged (`SHFile_combine`), so a DXIL compile and a SPIR-V compile of one source become one
   oiSH.

## Batches and threads

`Compiler_getTargetsFromFile` turns an input, a file or a folder, and the binary types wanted into parallel lists:
files, their text (read once per file, carriage returns removed), output names and binary type. A folder yields every
`.hlsl` file under it, and `.hlsli` files are for including only. `Compiler_compileShaders` takes those lists and does
the rest; it is what the CLI calls.

The work runs on a `JobQueue` with one Compiler per execution context. A file is a tree of jobs: the file, then one
job per combination from the group step, then one per link. A combination's binary is freed as soon as its links
finish, which bounds memory to the combinations in flight. A thread count of 1 or less runs every job inline in push
order, which keeps a failing compile easy to step through.

An output is written to its name, or handed back in `allBuffers` when the caller passes one. If any file feeding an
output fails, that output is not written and the others still are, and the call returns false. A file without
entrypoints is an error unless `ignoreEmptyFiles` is set. An entry whose stage or extensions cannot be expressed on a
backend is skipped for it without an error; when that leaves nothing, no oiSH is written and a warning says so.

## Errors and warnings

A call that cannot run (a bad parameter, DXC failing to start) returns false with an `Error`. A shader that does not
compile is a different thing: the call returns a `CompileResult` with `isSuccess` false and the diagnostics in
`compileErrors`, a list of `CompileError` holding the file, line, column, message and whether it is a warning or an
error. `Compiler_parseErrors` builds that list from DXC's text. It drops the trailing validation summary and the
include chain lines, and filters the warnings OxC3's own syntax causes: `[[oxc::...]]` annotations DXC does not know,
and the `[[vk::...]]` and inline SPIR-V attributes the DXIL backend ignores. The batch driver logs every diagnostic as a
warning or an error, with its `file:line:column` when it has a location.

The extra warnings are a separate pass over the finished oiSH, `Compiler_handleExtraWarnings`, asked for with an
`ECompilerWarning` mask: unused registers, unused constants and buffer padding. They are off unless requested,
because techniques such as bindless leave many registers unused on purpose.

## Targets

A binary is DXIL or SPIR-V (`EGfxBinaryType`). A compile makes exactly one of them; a file that wants both is two
compiles merged into one oiSH.

**SPIR-V version.** `Compiler_requiredSpirvVersion` picks the version from the features a binary uses, and nothing
else drives it:

| Version | Chosen when |
| --- | --- |
| 1.6 (Vulkan 1.3) | cooperative vectors or matrices, or descriptor heaps |
| 1.4 | ray tracing (a ray tracing stage or ray queries) or mesh and task shaders |
| 1.3 (Vulkan 1.1) | everything else |

The same function sets DXC's target environment, the version of the SPIR-V optimizer and validator in `Compiler_process`
and the one in `Compiler_link`, so they cannot disagree. A link never goes below the version of the module it is given.

**Shader models.** An entry can name models from 6.5 to 6.10 (`Compiler_registerModel` refuses others); without one it
gets the minimum for what it uses. An extension needs a minimum model (`Compiler_minFeatureSetExtension`), and a
combination below it is raised to it rather than refused. A wave size needs 6.6, or 6.8 for a range
(`Compiler_minFeatureSetStage`).

**Extensions.** An extension (`ESHExtension`, and `ESHExtension2` for the ones past the first set) is a capability a
binary relies on:

- *Detected or declared.* Reflection finds some from the binary itself (`ESHExtension_DxilNative` and
  `ESHExtension_SpirvNative` say which, per backend). The rest are declared by an annotation and trusted. A declared
  extension the binary does not use is dormant.
- *Macros and options.* Each extension in a compile defines `__OXC_EXT_<NAME>` and switches on what DXC needs, such as
  16 bit types, payload qualifiers or the SPIR-V extensions it may use.
- *One backend only.* `ESHExtension_NoDxilCompile` holds the extensions that cannot compile to DXIL: AtomicF32 and
  AtomicF64, because DXIL has no float atomic intrinsic, and UnsafeExperimentExecution, because there is no unvalidated
  path into DXIL. `ESHExtension_NoSpirvCompile` holds the ones that cannot compile to SPIR-V: MeshTaskTexDeriv, PAQ and
  WriteMSTexture, because DXC's SPIR-V backend refuses them. A backend is only closed off where it has no path at all;
  an intrinsic DXC lowers to one backend can still reach the other as inline SPIR-V, so it stays on both.

## Annotations

An entrypoint carries `[[oxc::...]]` annotations in front of DXC's own. They decide which binaries the entry becomes:
`stage` makes a standalone compile of that stage, `model`, `extension` and `defines` each add a line that multiplies
the combinations, `uniforms` adds rows that are linked rather than compiled, `vendor` limits which GPUs may run it and
`binary` narrows the backends. `Compiler_parse` reads them out of the HLSL reflector, so DXC has checked the syntax
before OxC3 reads the meaning. The full list, with the rules for each, is in
[Entrypoint annotations](#entrypoint-annotations) below.

## Includes and incremental builds

A relative `#include` resolves from the including file and from the directories in `includeDirs` of `CompilerSettings`,
all under the working directory (see [Include limits](#include-limits)). A name with an `@` prefix is a built-in: the
`.hlsli` files under `include/shader_compiler/shaders/` are compiled into the module, so they resolve the same on every
platform and in a sandbox. `Compiler_builtInIncludeCount`, `Compiler_builtInIncludeAt` and `Compiler_findBuiltInInclude`
list and look them up, for tools that want to show them. An include inside a file of the virtual file system resolves in
the virtual file system.

With `infoAboutIncludes`, a compile reports each file it read as an `IncludeInfo`: its relative path, size, CRC32C
(computed with carriage returns removed, so Windows and Unix agree) and timestamp. `Compiler_mergeIncludeInfo` folds
the lists of several compiles of one file into one. The oiSH stores the names and hashes beside a `sourceHash` of the
source itself, and `OxC3 shader includes` prints them. That is what makes two things possible:

- **Rebuild only what changed.** A build compares the stored hashes with the files on disk and recompiles an oiSH only
  when its source or one of its includes differs.
- **Hot reload.** A Compiler keeps the contents of the includes it has read. On the next compile it revalidates each
  one by timestamp and size, and by CRC32C when only the timestamp moved, so a host that keeps its Compiler alive
  serves unchanged includes from memory.

## Reflection

Three kinds of reflection come out of a compile, at different levels:

- **Binding level, in the oiSH.** `Compiler_process` reflects each binary into registers (type, read or write, array
  shape and the binding for each backend) and into shader buffers. The buffers are [oiSB](file/oiSB.md) layouts:
  DXIL flattens arrays and SPIR-V keeps them, and merging the two keeps the SPIR-V shape. Registers are matched by name
  when two backends merge, see the quirks in
  [oiSH](file/oiSH.md#quirks-between-dxil-and-spirv).
- **Entry level, in the oiSH.** `Compiler_finalizeEntrypoint` records an entry's group size, payload and intersection
  sizes, the type of each graphics input and output (16 slots) and their semantics. **All permutations of one entry
  reflect identical inputs and outputs**: the first to finish records them and every later one is compared against
  them, and a mismatch fails the compile. A wave size is read from the source for every backend, because SPIR-V cannot
  carry one, and a DXIL binary that reflects another size fails.
- **Symbol level, in an oiSR.** `Compiler_reflect` returns the pre codegen symbol AST (functions, types, resources,
  scopes, with source locations) as an `SRFile`, see [oiSR](file/oiSR.md). It runs on the preprocessed source, so the
  defines in the settings decide which code paths exist. An oiSR is a file of its own for tooling, not part of an
  oiSH; the CLI writes one with `OxC3 shader reflect-symbols`.

A reflection only oiSH, with the registers and entries but no binaries, is made by compiling and then stripping the
code (`OxC3 shader reflect`, see [Header flags](file/oiSH.md#header-flags)).

## Debug info and unused registers

Two settings change what a compile emits. `debug` keeps debug info in the binary: DXC's `-Zi` for DXIL, and for SPIR-V
the debug categories that carry the source, with SPIRV-Tools leaving the debug instructions in place. Without it
SPIR-V is stripped of them.

`keepUnusedRegisters` keeps resources the shader declares but never uses bound and reflected, so the layout of
registers is stable across variants. Without it they are dropped, though their slots stay reserved. On SPIR-V it also
tells DXC and the optimizer to preserve the bindings.

## AMD ISA

`spirv_isa.h` turns a SPIR-V module into AMD ISA text, so the cost of a shader can be read without a device.
`SpvISA_disassemble` runs the bundled `amdllpc` for a gfx target, RDNA3 (gfx11) and RDNA4 (gfx12), which lowers the
module and emits the ISA as text itself, with each instruction's encoding. `SpvISA_stageHasOfflinePath` says whether a module qualifies: raster, compute,
mesh and task entries do, ray tracing does not. `SpvISA_listSupportedTargets` asks `amdllpc` which targets this build
accepts. All of it spawns a process, so it needs `SUPPORTS_PROCESS`, and it exists only where AMD ships the tools
(`SHADER_COMPILER_OFFLINE_ISA`). The CLI side is [ISA](OxC3_tool.md#isa-device-specific-assembly); the corpus test pins
the output as snapshot files.

## Shader source

This part is the reference for the HLSL a shader author writes: the built-in includes, the defines OxC3 sets, the
semantics it accepts, the annotations it reads and how combinations are dropped. It holds however the compiler is
invoked, through the library, the CLI or the CMake packaging.

### Built-in includes

`@myFile.hlsl` specifies builtin shaders, such as `@types.hlsl` and `@resources.hlsl` which are bindings to be compatible with OxC3. This can also access NV specific HLSL extensions when DXIL is used as a target and the `extension` annotation is used.

### Compile

Compile mode (default) will turn the text into shaders ready for consumption by a graphics API. This could be DXIL, SPIRV or even text representations (MSL, WGSL or even GLSL in the future). These are then stored in an oiSH file, which contains information about the defines, inputs/outputs, basic reflection info and entrypoint binary/name as well as other metadata. These oiSH files can be either bulky (works for every backend) or lean (works only for the target(s)).

#### Include limits

Includes use OxC3 file paths, as such, escaping out of the working directory is prohibited. Any resolved file path that doesn't start with the working directory is illegal. And any tool that may read these includes (such as a file watcher) should have a similar protection mechanism.
This gives a logical limit to how far back an include can go. Such a limit is also very useful to prevent any tooling from reading into files that weren't intentional or don't exist within the scope of the program (such as embedding a fictional include file which could read any file the program has access to). It will also enforce relative files, rather than allowing absolute files in the includes for some reason.

#### Built-in defines

The following defines are set by OxC3 during compilation or preprocess:

- `__OXC` to indicate that OxC3 is compiling or parsing the shader.
- `__OXC_PREPROCESS` to indicate that OxC3 is parsing the entrypoints (useful for ensuring OxC3 sees all entrypoints even if some get hidden in other compilations).
  - When this happens, all extensions are turned on but no uniforms or defines are set. This means that your shader should gracefully handle a uniform or define not being present. As well as too many extensions being available while `__OXC_PREPROCESS` is used; these extensions won't make it into the final binary. For example, defines should check `#ifdef $$UNIFORM` before using them, just like normal defines.

- `__OXC_MAJOR`, `__OXC_MINOR` and `__OXC_PATCH` to indicate OxC3 version. For 0.2.0 these would be 0, 2 and 0 respectively.
- `__OXC_VERSION` same layout as `OXC3_MAKE_VERSION` aka (major << 22) | (minor << 12) | patch.
- `__OXC_EXT_<X>` foreach extension that's enabled by the current compilation. For example: `__OXC_EXT_F16`, `__OXC_EXT_F64`, `__OXC_EXT_RAYQUERY`, etc.
  - `__OXC_EXT_RAYTRACING` is set when raytracing shaders are used.

#### Semantics

Semantics for input(s) and output(s) for shaders get parsed and have the following restrictions:

- SV_ is reserved for HLSL semantic values.
- Other semantic values should follow TEXCOORD[n], for optimal compatibility and storage. These can be bound through Vulkan as binding n or in D3D12 as TEXCOORD with id n. Semantics for input/output layouts are compatible with the OxC3 shader compiler, but are stored less efficiently and could experience less compatibility for non DirectX pipelines.

#### Entrypoint annotations

Each entrypoint can have annotations on top of the ones used by DXC (have to be before the DXC annotations). The ones introduced in OxC3's pre-processor are the following:

- `[[oxc::stage("vertex")]]` similar to DXC's `[shader("vertex")]` but instead of implying a library compilation (for use in StateObjects), it signals a standalone compilation for this entrypoint. Essentially allowing pixel and vertex shaders to be compiled and packaged as a single oiSH file.
  - Defining stages is only allowed for stages that have to be separately compiled. For raytracing shaders, the shader annotation should still be used.
  - Type must be one of vertex, pixel, compute, geometry, hull, domain, mesh, task.
  - Do keep in mind that each stage needs a full compile. It might be beneficial to bundle them as a single lib using the "shader" type.
  - If not defined, the compiler will ignore the functions if they don't have either a `stage` or `shader` annotation.
  - Note: Please prefer the use of `[shader("type")]` since stage is deprecated. It's there to support backwards compatibility, but introduces extra compile time compared to the 'shader' annotation. This is required for oxc::uniforms.
- `[[oxc::vendor("NV", "AMD", "QCOM")]]` which vendors are allowed to run this entrypoint. There could be a reason to restrict this, for example when NV specific instructions are used (specifically together with DXIL). Must be one of: NV, AMD, ARM, QCOM, INTC, IMGT, MSFT, APPL, SMSG, HWEI, GOGL, MESA. If not defined, will assume all vendors are applicable. Multiple annotations for vendor is illegal to clarify that it won't induce a new compile for each vendor.
- `[[oxc::extension("16BitTypes")]]` which extensions to enable. For example 16BitTypes will enable 16-bit types for that entrypoint.
  - Do keep in mind that extensions might introduce another recompile for entrypoints that don't have the same extensions. For example with raytracing shaders. In their case, it will introduce two compiles if one entrypoint doesn't support 16-bit ints and another does.
  - `__OXC_EXT_<X>` can be used to see which extension is enabled. For example `__OXC_EXT_ATOMICI64`.
  - Extension must be one of F64, I64, 16BitTypes, AtomicI64, AtomicF32, AtomicF64, SubgroupArithmetic, SubgroupShuffle, RayQuery, RayMicromapOpacity, RayTriPosition, Barycentrics, RayReorder, Multiview, ComputeDeriv, PAQ, MeshTaskTexDeriv, WriteMSTexture, SubgroupOperations, CoopVec, CoopMat, CoopFP8, CoopVecTraining, DescriptorHeap, SubgroupQuad, DynamicSamplers, UnsafeExperimentExecution, UnsafeExtensionExecution, ExperimentTier1, ExperimentTier2, IntegerDot4x8.
    - **Note**: RayReorder is currently only available for raygeneration shaders.
    - **Note**: UnsafeExperimentExecution marks a binary its author vouches for rather than one OxC3 checks: it compiles to SPIR-V only and unvalidated, and only runs on a device created with `EGraphicsDeviceFlags_AllowUnsafeExperimentExecution` (see "Unsafe experiment execution" in graphics_api.md).
    - **Note**: UnsafeExtensionExecution lets in extensions oiSH doesn't recognize, on both backends: SPIR-V capabilities and DXIL requirement flags it has no extension for, and any SPIR-V extension DXC knows. It only runs on a device created with `EGraphicsDeviceFlags_AllowUnsafeExtensionExecution` (see "Unsafe extension execution" in graphics_api.md).
    - **Note**: ExperimentTier1 and ExperimentTier2 (ESHExtension2) declare an experiment tier the device's driver has to claim (`EGraphicsFeatures2_ExperimentTier1`), which every binary declaring one is held to, an UnsafeExperimentExecution binary included. The compile sees `__OXC_EXT_EXPERIMENTTIER1` or `__OXC_EXT_EXPERIMENTTIER2`. OxC3 gives the tiers no meaning.
    - **Note**: IntegerDot4x8 (ESHExtension2) declares the packed 4 x 8 bit integer dot products (`dot4add_i8packed`, `dot4add_u8packed`), which need `EGraphicsFeatures2_IntegerDot4x8`; the compile sees `__OXC_EXT_INTEGERDOT4X8`. A SPIR-V compile refuses a shader that uses them without declaring it.
    - **Note**: Multiple extension annotations will indicate there will be a separate compile with each. For example: `[[extension()]]` and `[[extension("16BitTypes")]]` in front of the same function will indicate the function will be compiled with 16-bit types on and off. 16-bit off would for example run on <=Pascal (GTX 10xx). This will allow the same entrypoint to be ran with different functionality. This could aid for example in unpacking vertex/texture data with native support (rather than manual f16tof32).
      - Another good example could be RayReorder, which could give substantial boosts in path tracing workloads. Lovelace would need `[[extension("RayReorder")]]` while the rest such as non NV and Pascal, Turing, Ampere would need `[[extension()]]`. This will force a recompile.
      - If multiple extensions are available, it will pick from top to bottom if they're all available. So `[[extension("16BitTypes")]]` and `[[extension("RayReorder")]]` would for example only pick the second one if the first one isn't available.
- `[[oxc::uniforms(U32 x = 45, F32 y = 90.f)]]` Creates either 2 spec constants (spv) or 2 library exports (dxil) that contain these values. These are only accessible as functions and not as static const at compile time, but they will be fossilized in a subsequent linking step internally to reduce compile time (but keep runtime performance).
  - Please try to use uniforms if possible and avoid defines, since defines cause a full HLSL compile, while uniforms will only cause DXIL/SPV to be fossilized into the right shader. Each unique uniform combo will cause DXIL/SPV linking to happen.
  - The types are scalars: `(U/I)(8/16/32/64)`, `F(16/32/64)` and `B1`. Vectors and matrices are refused, since SPIR-V has no spec constant of either; make one out of several scalars. Each is stored at its width (a B1 as one bit).
    - In HLSL `$$X` has the type the binary's extensions give it, as with any declaration: `uint16_t` for a U16 with 16-bit types and `uint` without, a U8 widening to the same, an F16 being `float16_t` or `float` alike, and 64-bit types needing I64 or F64. On SPIR-V an 8 or 16-bit uniform is a 32-bit spec constant underneath (DXC has none narrower) that `$$X` casts back, so its value is the same on both backends: signed ones sign extended, a half the float of its value.
    - A value is exact: the binary holds the value's bits (`asfloat(0x3DFCD6EAu)` in DXIL's export for `F32 f = 0.123456789`), not a decimal rounded on the way.
  - Reserved syntax for defining vectors is `(1, 2, 3, 4)` for example and matrices are `((1, 2, 3), (4, 5, 6), (7, 8, 9))`, for when they're supported.
  - Since the uniform becomes a function or variable, it must not include symbols or spaces (excluding _). 
  - The application is in charge of picking the uniform & define combo for the entire lib or each entrypoint. So it is possible to switch between defines based on what the app wants, unlike models which is handled by the runtime; though extensions can be explicitly required or disabled at runtime.
  - Uniforms types, ordering and names must stay the same for the same compilation unit (source file). This ensures consistent behavior between compiles and clearly defines values of uniforms (rather than assuming they'd be defined as zero). For example: `[[oxc::uniforms(B1 value = true)]] [[oxc::uniforms()]]` this would be invalid, as the first one defines B1 value but the 2nd doesn't. Another invalid would be if the uniform is defined, but the type mismatches; only the value can change. Another would be if the order isn't consistent between others.
  - Uniforms require the use of the DXC `[shader("type")]` intrinsics (lib compilation), whereafter this will be turned into a specialized shader afterwards. Every stage takes them this way: ray tracing stages link one library a row, and a mesh or amplification entry is written `[shader("mesh")]` or `[shader("amplification")]`, DXC's name for the task stage (`[[oxc::stage("task")]]` still spells it task).
  - If a compilation unit has uniforms that are used by only one entrypoint, it still has to be available and they can't change types. This is because every compilation unit is only compiled once per combination (defines, shader models, extensions).
  - Each entrypoint has rows of its own: two entrypoints of one file may give the same uniform different values, and each gets a binary a row of its own (one compile of the file serves both).
  - The app finds a binary by its uniforms' values (`GraphicsDeviceRef_getFirstShaderEntry`'s `uniforms`, name and value pairs): each value is read as the uniform's type and compared as a value, so `3` and `+3`, or `1.5` and `1.50`, find the same binary, and a float written with more digits than it holds finds the binary its annotation rounded to the same value.
- `[[oxc::defines("X" = "F32x3(1, 0, 0)", "Y" = "F32x3(0, 1, 0)")]]` Introduces one subtype of the stage that has the defines `$X` and `$Y` set to the relevant values of `(F32x3(1, 0, 0))` and `(F32x3(0, 1, 0))`.
  - Try to avoid defines in favor of uniforms to reduce compilation time (each unique define combo will cause an additional full HLSL compile).
  - This attribute can be used multiple times to compile the same entrypoint with different defines. It will try to bundle compiles wherever possible (try to keep defines similar).
  - Only defining the define but no assign will just see it as a define that can be checked with #ifdef.
  - Define name must not indicate symbols or spaces (excluding _).
- The application is in charge of picking the define combo for the entire lib or each entrypoint. So it is possible to switch between defines based on what the app wants, unlike models and extensions which are handled by the runtime.
- `[[oxc::model("6.8")]]`
  - Which shader model to use. If this annotation is present multiple times, it indicates multiple compiles with different shader models.
    - Minimum must be 6.5+ since that's the minimum OxC3 supports (the maximum is 6.10).
    - AtomicI64, ComputeDeriv and PAQ (Payload Access Qualifiers) require SM6.6.
    - WaveSize requires SM6.6.
    - WaveSize with 2 or 3 arguments (min, max, preferred) requires SM6.8.
    - WaveSize is read off the source with the entry (through the HLSL reflector, which hands it over as a builtin annotation), so the entry carries it for every binary type: SPIR-V can't hold a wave size, and a Vulkan pipeline asks for it instead (graphics_api.md, Wave size). A DXIL compile's reflected size has to be the same one, and WaveSize is for compute entries only.
    - WriteMSTexture (writable multisampled textures) requires SM6.7.
    - RayReorder (Shader Execution Reordering / SER) and RayMicromapOpacity (Opacity Micromaps / OMM) require SM6.9.
    - DescriptorHeap requires SM6.6 (dynamic resources).
    - CoopVec, CoopMat, CoopFP8, CoopVecTraining and RayTriPosition require SM6.10.
    - Stage type always has to be compatible with specified models.
    - A declared model is a floor: when an extension line needs a newer model than the one declared, that combination is raised to the extension's minimum rather than refused. A model is never lowered, because plain HLSL can need a model no extension names (IsHelperLane, Barrier(), templates). Combinations that land on the same model share one binary.
      - Example: `[[model("6.5")]]` and `[[model("6.8")]]` with `[[extension()]]` and `[[extension("AtomicI64")]]` (which needs 6.6) make 4 binaries: 6.5 without the extension, 6.6 with it (raised from 6.5), and 6.8 with and without it. This keeps models and extension lines a cross product, so a baseline line doesn't have to be pulled up to what another line's extension needs.
    - If multiple models are available, the runtime will choose from highest shader model to lowest shader model available.
  - If not defined, will use minimum for the detected feature set.
  - `__SHADER_TARGET_MAJOR` and `__SHADER_TARGET_MINOR` can be used to distinguish which one is being compiled.
- `[[oxc::binary("spv", "dxil")]]`
  - Restricts which binary backends this entrypoint is emitted for. Accepts `spv` (or `spirv`) and `dxil` (case insensitive). Unlike models/defines/extensions this does **not** introduce extra compiles; it only narrows which of the already-requested backends this entrypoint ends up in.
  - If not defined, the entrypoint is emitted for every requested backend its stage + extensions can actually be expressed on.
  - The effective set is an AND of three things: the backends passed to `-compile-output`, this annotation (or all backends if it's absent), and the backends the entrypoint's stage + extensions support. Example: compiling with `all` a shader whose entrypoint can only be expressed in SPIRV will only emit SPIRV for it.
  - Stage / extension support is auto-restricted even without the annotation: AtomicF32/F64 are SPIRV-only, since DXIL has no float atomic intrinsic; MeshTaskTexDeriv, PAQ and WriteMSTexture are DXIL-only, since DXC's SPIR-V backend refuses them. Everything else compiles on both, including RayMicromapOpacity and RayReorder (SPIR-V reaches them through inline SPIR-V). Listing a backend the stage/extension can't target simply drops it (that's the AND) rather than erroring; when that leaves no entrypoint at all, no oiSH is written and a warning says so.
  - A single annotation lists the full set (e.g. `[[oxc::binary("spv", "dxil")]]`); duplicate binary types within one annotation are rejected.
  - **NOTE** (current limitation): this only filters the oiSH *reflection*, an excluded entrypoint is not reported for that backend, but when it shares a library compile with a kept entrypoint the emitted DXIL/SPIRV blob can still physically contain its code. Explicitly stripping the entrypoint and re-running DCE per backend is planned, after which reflection and the binary will fully agree.

**NOTE**: Since multiple models, defines and extensions can cause multiple compiles, be very careful that you don't use too many of these decorations (2 different sets of defines + 2 different extensions + 2 models = 2^3 or 8 compiles). Even though the compiler will do everything in parallel, it will still be slower to do multiple configurations that will never be used. So keep duplicate annotations in check wherever possible. The compiler will also try to combine compiles if it notices that they can be shared (for example with lib compiles where one compile might expose multiple entrypoints). Please prefer uniforms rather than defines to keep compile times low, this is because uniforms are specialization constants under the hood (or libraries in DXIL) and this in turn means that they get fossilized after HLSL has been compiled to DXIL/SPIRV, which is way cheaper than processing the entire file.

#### Disabling combinations

An entry's binaries are the full cross product of its models, extension lines, define lines and uniform rows. Where some of those combinations can't or shouldn't exist (a define set meaningless under one extension line, a uniform row one family of entries never uses), `@variants.hlsli` drops them while the annotations stay a cross product, in two ways that work alike on SPIR-V and DXIL:

- `OXC_DISABLE_COMBINATION`, written under an `#if` on what a combination is made of (its defines, `$NAME`, its extensions, `__OXC_EXT_NAME`, or `__spirv__`), drops the combination. The marker expands to a type DXC doesn't know, so a combination that reaches it fails its compile with that name in the error, which OxC3 takes as the combination being disabled instead of a failure: nothing is compiled for it, every uniform row of it included. It has to be written at global scope, and in the discovery pass, which only reads the entrypoints, it expands to nothing.
  ```hlsl
  #include "@variants.hlsli"

  #if defined(__OXC_EXT_EXPERIMENTTIER1) && $FAMILY == 0
  	OXC_DISABLE_COMBINATION        //The experiment line only means something to the other family
  #endif
  ```
- `oxcDisableBinary()`, called on a path only a uniform (or a define) decides, drops each binary whose specialized code still reaches it, decided after the link. It writes to a marker resource (`_oxcDisableBinary`, in register space and descriptor set 196, `OXC3_DISABLE_SPACE`), which survives in a binary exactly where the call does: OxC3 drops the binaries whose reflection uses it and removes the register from the rest, `--keep-registers` included, so no binary it writes declares it.
  ```hlsl
  #ifdef $$FORMAT
  	if($$FORMAT == 2 && $FAMILY == 1)
  		oxcDisableBinary();        //Format 2 has nothing to write in family 1
  #endif
  ```
  Only call it under a condition the specialization folds: under one it can't, such as a value read at run time, every binary keeps the call and every one is dropped.

Each dropped combination and binary is logged ("Disabled at compile", "Disabled after link"). An entry keeps the binaries left, and the oiSH format doesn't change: its entries list their binaries, so a lookup naming a disabled combination moves on to the next extension line, as it does where a binary isn't supported. An entry with none left isn't written, and a file with none writes nothing, with a warning.
