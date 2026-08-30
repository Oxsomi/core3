/* api.js — the WASM boundary.
 *
 * Every backend capability the UI needs goes through OxAPI, and ONLY through OxAPI.
 * Today each method delegates to js/mock.js + js/mock_formats.js; the port replaces the
 * bodies here (and deletes both mock files) without touching the tools. All methods are
 * async so swapping in wasm (worker or main thread) changes nothing upstream.
 *
 * ---------------------------------------------------------------------------------------
 * SHDocument — the JSON contract the UI consumes, mirroring formats/oiSH (core3):
 * {
 *   name, sourceName,
 *   compilerVersion: {major, minor, patch},              // SHFile::compilerVersion
 *   sourceHash,                                          // SHFile::sourceHash (CRC32C)
 *   flags: {reflectionOnly},                             // ESHSettingsFlags
 *   header: {version:'1.2', sizeTypes:{spirv,dxil}},     // SHHeader (file header view)
 *   entries: [{                                          // SHEntry
 *     name, stage, lib, binaryIds:[i],
 *     inputs:  [{type /*ESBType name* /, semantic, idx}],// graphics stages
 *     outputs: [{type, semantic, idx}],
 *     group:[x,y,z]|null, waveSize:{req,min,max,rec}|null,
 *     payloadSize?, intersectionSize?                    // raytracing stages
 *   }],
 *   binaries: [{                                         // SHBinaryInfo (identifier + data)
 *     entrypoint|null, stage|'lib', lib, entryNames:[],
 *     model:'6.8', extensions:[], dormant:[],            // active = extensions − dormant
 *     defines:[{name,value}], uniforms:[{type,name,value}],
 *     vendors:[..]|null /*null = all* /, supported:['spv','dxil'],
 *     sizes:{spirv,dxil},                                // bytes; 0 = not present
 *     registers: [{                                      // SHRegisterRuntime
 *       name, arrays:[], typeStr, cls:'CBV|SRV|UAV|SMP',
 *       flags:{write,array,combined},
 *       bindings:{ spirv:{set,binding}|null /*push const* /, dxil:{letter,binding,space} },
 *       used:{spirv,dxil}, texture:{primitive,format}|null,
 *       buffer:{size,padding,packed,vars:[{offset,name,type,stride,arrays,used,children}]}|null  // SBFile (oiSB)
 *     }]
 *   }],
 *   includes: [{path, crc32c}]                           // SHInclude
 * }
 *
 * SRDocument — formats/oiSR, the frontend symbol AST (`shader reflect-symbols`):
 * {
 *   name, sourceName, compilerVersion, hash,
 *   header: {version:'1.1', flags:{hasSymbols}, features:['Basics',..,'SymbolInfo'],   // SRHeader + ESRFeature
 *            counts:{nodes, annotations, registers, enumValues, types, arrayDims, interfaces}},
 *   nodes: [{                                            // SRNode, pre-order; id = index
 *     id, kind /*ESRNodeType name* /, name, parent, children:[id],
 *     loc:{file,line,col,len,lines}|null,                // SRSymbol (only with HasSymbols)
 *     annotations:['[[oxc::stage("compute")]]',..],      // SRAnnotation
 *     type:{name,display,cls,rows,cols}|null,            // SRType (name = frontend type, display = alias written)
 *     semantic|null, arrays:[], direction:'in|out|inout|return',   // Parameter
 *     register:{info,count,cls}|null,                    // SRRegister (frontend bind info)
 *     entry:{stage,lib}|null, returns
 *   }],
 *   builtinCollapsed: [{file,count}]                     // the built-in includes' symbols, as the CLI collapses them
 * }
 *
 * SPDocument — formats/oiSP, a pipeline plus where each field's value came from (`-pso-output`):
 * {
 *   name, sourceName,
 *   header: {version:'1.1', counts:{pipelines, stages, specializations, graphicsStates, raytracingStates,
 *            blendAttachments, vertexBuffers, vertexAttributes}},                      // SPHeader
 *   pipelines: [{                                        // SPPipelineBase + its state
 *     name, type:'compute|graphics|raytracing', flags:['GeneratedVertexStage',..],      // ESPPipelineFlag
 *     stages:[{stage, shaderFile, entrypoint, sourceHash, generated}],                  // SPStage
 *     fields:[{field, index, value, source:'derived|supplied|assumed', reason, domain, indexed}], // SPSpecialization + ESPField
 *     notes:[]
 *   }]
 * }
 * A pipeline is "exact" when no field is assumed; compute derives completely, ray tracing leaves
 * only rt.* to supply, graphics reports everything no shader signature can carry.
 *
 * ISAResult — `isa disassemble`: {asic, arch, entrypoint, stage, stats:{sgprs,vgprs,codeBytes,instrs,scratch,lds}, text}
 * ---------------------------------------------------------------------------------------
 */
(function () {
"use strict";
const M = () => window.OxMock;
const MF = () => window.OxMockFormats;

window.OxAPI = {

  ready: false,

  /* ---- host ---------------------------------------------------------------------------- */
  /* A plain browser runs pure wasm. Hosted in a VS Code webview (acquireVsCodeApi exists) the
   * extension can bridge to the native OxC3 on the machine, which is what makes `-asic live`
   * (a real Vulkan device) and the process-backed amdllpc route possible there; the UI asks for
   * capabilities once and greys out what the host can't do rather than faking it. */
  host: typeof window.acquireVsCodeApi === "function" ? "vscode" : "browser",

  async capabilities() {
    // TODO(vscode): ask the extension host what the native binary can do:
    //   liveIsa    = a Vulkan device with VK_KHR_pipeline_executable_properties is reachable
    //   offlineIsa = the bundled amdllpc can be spawned (SUPPORTS_PROCESS) or the Mesa port is built in
    // Proposed: vscode.postMessage({ cmd: "capabilities" }) -> { liveIsa, offlineIsa, devices: [...] }
    return { host: this.host, liveIsa: this.host === "vscode", offlineIsa: true };
  },

  async init() {
    // TODO(wasm): load + instantiate the module once, then flip the "mock" badge off.
    //
    // Proposed:
    //   const Module = await createOxC3Module({ locateFile: f => "wasm/" + f });
    //   Module._oxc3_init();                       // Platform_create + Compiler_create pool
    //   window.addEventListener("beforeunload", () => Module._oxc3_shutdown()); // Compiler_shutdown()
    this.ready = true;
  },

  /* ---- OxC3 shader compile (= OxC3 compile shaders) --------------------------------- */
  /* project: {path: {src}}, opts: {targets:['spv','dxil'], debug, split, keepRegisters, reflectionOnly,
   *          warnUnusedRegisters, warnUnusedConstants, warnBufferPadding, ignoreEmptyFiles} */
  async compileFile(name, project, opts) {
    // TODO(wasm): the CLI path is Compiler_getTargetsFromFile + Compiler_compileShaders
    // (shader_compiler/compiler.h) with allFiles/allShaderText/allBuffers set, so the compile is
    // fully in-memory and the oiSH comes back as bytes instead of being written to disk. The
    // include resolver must be bridged to the in-memory project tree (roots act as -include-dir;
    // @builtins ship in the module).
    //
    // Proposed:
    //   const r = Module.ccall("oxc3_compileShaders", "number",
    //     ["string","string","number","number","number"],
    //     [JSON.stringify(project), name, targetMask(opts.targets),
    //      packFlags(opts) /* --debug/--split/--keep-registers/warnings/--ignore-empty-files */, opts.reflectionOnly?1:0]);
    //   -> { oish: Uint8Array[SHFile_write], doc: SHDocument, errors: CompileError[] }
    //
    // opts.reflectionOnly mirrors `OxC3 shader reflect`: compile, then strip binaries and
    // rewrite with ESHSettingsFlags_ReflectionOnly (CLI_shaderStripToReflection).
    await fakeLatency(250, 650);
    return M().compileFile(name, project, opts);
  },

  /* ---- raw DXC (Compile mode → Command tab → Run with DXC) --------------------------- */
  async compileRaw(argv, name, project) {
    // TODO(wasm): run an (edited) DXC flag line through the same DXC instance the compile path uses. The line the
    // Command tab pre-fills is what OxC3 derives (browser_wasm_plan: expose the real argv via getCompileArgs rather than
    // re-deriving it in JS); this runs whatever the user made of it. The output is a standalone binary, never an oiSH:
    // no annotations were processed and nothing was reflected into an identifier. No CLI verb for this yet.
    //
    // Proposed: Module.ccall("oxc3_dxcRaw", "number", ["string","string","string"], [argv, name, JSON.stringify(project)])
    //   -> { ok, type, bytes, log, diags }
    await fakeLatency(200, 500);
    return M().compileRaw(argv, name, project);
  },

  /* ---- standalone binaries as documents ---------------------------------------------- */
  async reflectBinary(name, type, bytes, origin) {
    // TODO(wasm): the backend reflection the compiler already runs while building an oiSH (spirv-reflect over SPIR-V,
    // DXC's IDxcContainerReflection over DXIL) applied to a bare binary: registers, bindings, IO, group/wave size, one
    // entry per entrypoint found. The result is a one-binary SHDocument with no identifier. No CLI verb for this yet;
    // `file data` only reads oiSH.
    //
    // Proposed: Module.ccall("oxc3_reflectBinaryToJson", "string", ["number","number","number"], [typeEnum, ptr, len]);
    void origin;
    return M().reflectBinary(name, type, bytes, origin);
  },

  async assembleOiSH(doc, identifier) {
    // TODO(wasm): the planned `shader assemble -> oiSH`: wrap standalone binaries into an SHFile with the identifier the
    // source would have declared (stage, model, extensions, vendors, later defines/uniforms and a previous oiSH to
    // merge into), so a pipeline can be created from it. The CLI's `shader assemble` only produces a .spv today.
    //
    // Proposed: Module.ccall("oxc3_assembleOiSH", ..., [binariesPtr, JSON.stringify(identifier)]) -> Uint8Array + SHDocument
    return M().assembleOiSH(doc, identifier);
  },

  /* ---- OxC3 shader reflect-symbols (oiSR) ------------------------------------------- */
  async reflectSymbols(name, project) {
    // TODO(wasm): Compiler_reflect (shader_compiler/compiler.h) walks DXC's IHLSLReflectionData
    // into an SRFile, which serializes to the SRDocument above (it's a pre-order node walk plus the
    // symbols/annotations/registers/types pools). Same include bridging as compileFile; the
    // built-in includes' symbols come back too and are reported collapsed per include, as the
    // CLI prints them (`... N builtin-include symbols collapsed`).
    //
    // Proposed:
    //   Module.ccall("oxc3_reflectSymbolsToJson", "string", ["string","string"], [JSON.stringify(project), name]);
    await fakeLatency(60, 160);
    return MF().reflectSymbols(name, project);
  },

  async parseOiSR(name, bytes) {
    // TODO(wasm): SRFile_read (formats/oiSR/sr_read.c) over a MemoryStream, then serialize to SRDocument.
    // This powers `file header` / `file data` on an .oiSR (and `file data --includes`, which expands
    // the collapsed built-in include symbols).
    return MF().parseOiSRBytes(name, bytes);
  },

  async writeOiSR(sr) {
    // TODO(wasm): SRFile_write (formats/oiSR/sr_write.c) -> Uint8Array; this is `shader reflect-symbols -output x.oiSR`.
    return MF().srBytes(sr);
  },

  /* ---- OxC3 file data / shader entrypoints / includes / feature_set ------------------ */
  async parseOiSH(name, bytes) {
    // TODO(wasm): SHFile_read (formats/oiSH/sh_read.c) over a MemoryStream, then serialize
    // the SHFile into the SHDocument contract above. This single call powers the whole
    // inspector: `shader entrypoints --verbose`, `shader includes`, `shader feature_set`
    // and `file data` (incl. --bin) are all views over the same SHFile.
    //
    // Proposed:
    //   const ptr = Module._malloc(bytes.length); Module.HEAPU8.set(bytes, ptr);
    //   const json = Module.ccall("oxc3_shReadToJson", "string", ["number","number"], [ptr, bytes.length]);
    //   Module._free(ptr); return JSON.parse(json);
    return M().parseOiSHBytes(name, bytes);
  },

  async readHeader(bytes) {
    // TODO(wasm): `OxC3 file header` — sniff the magic (oiSH / oiSR / oiSP) and read the raw
    // header that sits right after it: SHHeader (formats/oiSH/sh_headers.h), SRHeader
    // (formats/oiSR/sr_file.h) or SPHeader (formats/oiSP/sp_file.h).
    //
    // Proposed: Module.ccall("oxc3_fileHeaderToJson", "string", ["number","number"], [ptr, len]);
    const magic = String.fromCharCode(...bytes.slice(0, 4));
    if (magic === "oiSR") { const d = MF().parseOiSRBytes("header", bytes); return { format: "oiSR", ...d.header }; }
    if (magic === "oiSP") { const d = MF().parseOiSPBytes("header", bytes); return { format: "oiSP", ...d.header }; }
    const doc = M().parseOiSHBytes("header", bytes);
    return { format: "oiSH", version: doc.header.version, compilerVersion: doc.compilerVersion,
      sourceHash: doc.sourceHash, sizeTypes: doc.header.sizeTypes,
      binaryCount: doc.binaries.length, stageCount: doc.entries.length,
      includeFileCount: doc.includes.length };
  },

  async writeOiSH(doc, { backend } = {}) {
    // TODO(wasm): SHFile_write (formats/oiSH/sh_write.c). With `backend` set this is the
    // planned `OxC3 file split -format oiSH -compile-output <backend>` (lean per-backend
    // file, --split naming: x.spv.oiSH / x.dxil.oiSH).
    //
    // Proposed: Module.ccall("oxc3_shWrite", ..., [docHandle, backendMask]) -> Uint8Array
    return M().oishBytes(doc);
  },

  async extractBinary(doc, bin, backend) {
    // TODO(wasm): `file data --bin -entry <i> -compile-output <spv|dxil> -output <file>` —
    // just SHBinaryInfo::binaries[type] copied out of the parsed SHFile.
    return M().binBytes(bin, backend);
  },

  /* ---- OxC3 shader disassemble / assemble -------------------------------------------- */
  async disassemble(type /* 'spirv'|'dxil' */, bytes) {
    // TODO(wasm): Compiler_disassemble (shader_compiler/compiler.h) — spirv-tools for SPIR-V,
    // DXC for DXIL. The CLI detects the type from the file extension (CLI_shaderBinaryType);
    // here the caller passes it (or sniff the magic: 0x07230203 vs 'DXBC').
    //
    // Proposed:
    //   Module.ccall("oxc3_disassemble", "string", ["number","number","number"], [typeEnum, ptr, len]);
    void bytes;
    return "; TODO(wasm): Compiler_disassemble over the uploaded bytes.\n" +
           "; The mock only knows how to disassemble binaries it fabricated itself.";
  },

  async disassembleDocBinary(doc, bin, backend) {
    // Same underlying call as disassemble(), over the binary stored in a parsed/compiled oiSH:
    // this is `file data -input x.oiSH --bin -entry <i> -compile-output <spv|dxil>` (which
    // prints the disassembly when the shader compiler is linked, hexdump otherwise).
    return M().disasm(doc, bin, backend);
  },

  async assemble(type, text) {
    // TODO(wasm): Compiler_assemble — SPIR-V text via spirv-as; DXIL LL text via DXC's
    // IDxcAssembler exists in the API but the CLI marks DXIL assembly as not supported yet,
    // so the UI only enables SPIR-V.
    //
    // Proposed: Module.ccall("oxc3_assemble", ..., [typeEnum, textPtr]) -> Uint8Array
    if (type !== "spirv") throw new Error("DXIL assembly isn't supported yet (mirrors the CLI).");
    const enc = new TextEncoder().encode(text);
    return M().binBytes({ identKey: "assembled|" + OxUtil.crc32c(enc), sizes: { spirv: 256 + enc.length % 512, dxil: 0 } }, "spirv");
  },

  async getUniqueEntrypoints(type, bytes) {
    // TODO(wasm): Compiler_getUniqueEntrypoints — list the entrypoints embedded in a lib
    // binary (name + ESHPipelineStage), e.g. to pick a link target. Not exposed by the CLI
    // yet, but it's exactly what the "List entrypoints" button in the SPV/DXIL pane wants.
    void type; void bytes;
    return [{ name: "main", stage: "compute", note: "mock — wire Compiler_getUniqueEntrypoints" }];
  },

  /* ---- OxC3 file combine -format oiSH ------------------------------------------------ */
  async combineOiSH(a, b) {
    // TODO(wasm): SHFile_combine (formats/oiSH/sh_combine.c) — merges two oiSH compiled from
    // the same source: source hash, include CRCs and compiler settings must match; binaries
    // for different backends/identifiers are unioned (two lean files -> one bulky file).
    //
    // Proposed: Module.ccall("oxc3_shCombine", ..., [aPtr,aLen,bPtr,bLen]) -> Uint8Array | error
    if (a.sourceHash !== b.sourceHash)
      throw new Error(`source hash mismatch (${OxUtil.hex8(a.sourceHash)} vs ${OxUtil.hex8(b.sourceHash)}) — combine requires the same source(s), includes and compiler settings.`);
    const doc = JSON.parse(JSON.stringify(a));
    doc.name = (a.name.replace(/\.oiSH$/i, "") + "+" + b.name.replace(/\.oiSH$/i, "")) + ".oiSH";
    doc.combinedFrom = [a.name, b.name];
    return doc;
  },

  /* ---- oiSP: pipeline state with provenance ----------------------------------------- */
  async derivePipeline(doc, pick) {
    // TODO(wasm): SPFile_derivePipeline (formats/oiSP/sp_file.c) over the parsed SHFile: picks the
    // pipeline kind the CLI compiles first (compute, then graphics, then ray tracing), binds every
    // stage of that kind, generates the missing end of a graphics chain, and records every field
    // reflection can't prove as a specialization with its provenance. `pick` resolves a stage kind
    // that occurs more than once (the CLI refuses to guess; `-entry` picks).
    //
    // Proposed: Module.ccall("oxc3_spDeriveToJson", "string", ["number","string"], [shHandle, JSON.stringify(pick)]);
    // -> SPDocument, or {refused, candidates} when the CLI would refuse.
    return MF().derivePipeline(doc, pick);
  },

  async supplyPipeline(sp, pipelineIdx, field, index, value) {
    // TODO(wasm): SPFile_supply — the caller chose a value, so the field stops being assumed. This is what
    // `-pso-set` does on the CLI side, taking any field by the path the report prints.
    return MF().supply(sp, pipelineIdx, field, index, value);
  },

  async printPipeline(sp, pipelineIdx) {
    // TODO(wasm): SPFile_print — the text `file data` prints for an oiSP and `isa disassemble` prints above the ISA.
    return MF().spPrint(sp, pipelineIdx);
  },

  async parseOiSP(name, bytes) {
    // TODO(wasm): SPFile_read (formats/oiSP/sp_read.c) over a MemoryStream, then serialize to SPDocument.
    // Every pool range, string id and the stored blend/vertex masks are validated on read.
    return MF().parseOiSPBytes(name, bytes);
  },

  async writeOiSP(sp) {
    // TODO(wasm): SPFile_finalize + SPFile_write (formats/oiSP/sp_write.c) -> Uint8Array; this is `-pso-output x.oiSP`.
    // A graphics state is stored as SPGraphicsStateStored (64 B) plus only the blend attachments and vertex entries
    // it can reach, so the file is much smaller than the runtime state.
    return MF().spBytes(sp);
  },

  /* ---- OxC3 isa devices / isa disassemble -------------------------------------------- */
  async isaTargets() {
    // TODO(wasm): SpvISA_listSupportedTargets (shader_compiler/spirv_isa.h) — the gfx targets the bundled
    // offline compiler (amdllpc) accepts as -asic: gfx11xx (RDNA3) and gfx12xx (RDNA4). Natively it probes the
    // amdllpc process; in the browser the list is static (or comes from a device-free Mesa ACO port, see README).
    return MF().isaTargets();
  },

  async isaDisassemble(doc, bin, asic, entrypoint) {
    // TODO(wasm): SpvISA_disassemble — SPIR-V -> ELF (amdllpc) -> ISA text (amdgpu-dis), prepended with the
    // register/resource usage line. Natively this spawns the bundled amdllpc (SUPPORTS_PROCESS), which the browser
    // can't do; the in-browser route is the device-free Mesa RADV/ACO (and Intel brw) compile, prototyped but not
    // in the CLI yet. Only raster/compute/mesh stages have an offline path; ray tracing and DXIL need `-asic live`.
    //
    // Proposed: Module.ccall("oxc3_spvIsa", "string", ["number","number","string","string"], [ptr, len, asic, entrypoint]);
    await fakeLatency(120, 400);
    return MF().isaDisassemble(doc, bin, asic, entrypoint);
  },

  async isaLive(doc, bin, sp, opts) {
    // `-asic live[:index]` creates a real device and reads back whatever the driver exposes: Vulkan through
    // VK_KHR_pipeline_executable_properties for SPIR-V, D3D12 for DXIL (which has no introspection API, so it
    // validates the pipeline without disassembling it). That needs a GPU and a driver, so a plain browser can't:
    // there is no route until a WebGPU backend exists, and even then WebGPU exposes no compiled-shader introspection.
    //
    // TODO(vscode): when capabilities().liveIsa is true, hand the oiSH bytes + the pipeline (as the oiSP `-pso-input`
    // takes, plus `-pso-set` for anything changed since) to the extension host, which runs
    //   OxC3 isa disassemble -input x.oiSH -asic live[:index] -pso-input x.oiSP [-pso-set ...] -pso-output x.oiSP
    // and returns { stats, text, report } per stage; the result shape is the same ISAResult the offline route gives.
    // Proposed: vscode.postMessage({ cmd: "isa.live", oish: bytes, oisp: await writeOiSP(sp), entry: opts.entry })
    void doc; void bin; void sp; void opts;
    throw new Error("`-asic live` needs a real device (VK_KHR_pipeline_executable_properties); run it natively: OxC3 isa disassemble -input x.oiSH -asic live, or host this page in VS Code where the native OxC3 answers it");
  }
};

function fakeLatency(min, max) { return new Promise(r => setTimeout(r, min + Math.random() * (max - min))); }
})();
