/* api.js: the backend boundary.
 *
 * Every backend capability the UI needs goes through OxAPI, and ONLY through OxAPI. Each method
 * routes to the real OxC3_wasm module (js/wasm.js) when it loaded, and to js/mock.js +
 * js/mock_formats.js when it didn't or when nothing in the library answers that call yet.
 * OxAPI.backend says which, and OxAPI.stubs lists what is still fabricated and why, so the page
 * can be honest about it rather than the reader having to guess.
 *
 * ---------------------------------------------------------------------------------------
 * SHDocument: the JSON contract the UI consumes, mirroring formats/oiSH (core3):
 * {
 *   name, sourceName,
 *   compilerVersion: {major, minor, patch},              // SHFile::compilerVersion
 *   sourceHash,                                          // SHFile::sourceHash (CRC32C)
 *   flags: {reflectionOnly},                             // ESHSettingsFlags
 *   header: {version:'1.2', sizeTypes:{spirv,dxil}},     // SHHeader (file header view)
 *   model: '6.8',                                        // highest model any binary asked for
 *   entries: [{                                          // SHEntry
 *     name, stage, lib, binaryIds:[i],
 *     inputs:  [{type /*ESBType name* /, semantic, idx}],// graphics stages
 *     outputs: [{type, semantic, idx}],
 *     group:[x,y,z]|null, waveSize:{req,min,max,rec}|null,
 *     payloadSize?, intersectionSize?                    // raytracing stages
 *   }],
 *   binaries: [{                                         // SHBinaryInfo (identifier + data)
 *     entrypoint|null, stage|'lib', lib, entryNames:[],
 *     model:'6.8', extensions:[], dormant:[],            // active = extensions - dormant
 *     defines:[{name,value}], uniforms:[{type,name,value}],
 *     vendors:[..]|null /*null = all* /, supported:['spv','dxil'],
 *     sizes:{spirv,dxil},                                // bytes; 0 = not present
 *     registers: [{                                      // SHRegisterRuntime
 *       name, arrays:[], typeStr, cls:'CBV|SRV|UAV|SMP',
 *       flags:{write,array,combined},
 *       bindings:{ spirv:{set,binding}|null /*push const* /, dxil:{letter,binding,space} },
 *       used:{spirv,dxil}, push, texture:{primitive,format}|null,
 *       buffer:{size,padding,packed,vars:[{offset,name,type,stride,arrays,used,children}]}|null  // SBFile (oiSB)
 *     }]
 *   }],
 *   includes: [{path, crc32c}],                          // SHInclude
 *   registers: [..],                                     // the file's register set (union), for the diff
 *   bytes: Uint8Array                                    // the oiSH itself, present on every real document
 * }
 *
 * SRDocument: formats/oiSR, the frontend symbol AST (`shader reflect-symbols`):
 * {
 *   name, sourceName, compilerVersion, hash /*16 hex digits: 64 bits don't survive a JSON number* /,
 *   header: {version:'1.1', flags:{hasSymbols}, features:['Basics',..,'SymbolInfo'],   // SRHeader + ESRFeature
 *            counts:{nodes, annotations, registers, enumValues, types, arrayDims, interfaces}},
 *   nodes: [{                                            // SRNode, pre-order; id = index
 *     id, kind /*ESRNodeType name* /, name, parent /*-1 at the root* /, children:[id],
 *     loc:{file,line,col,len,lines}|null,                // SRSymbol (only with HasSymbols)
 *     annotations:['[[oxc::stage("compute")]]',..],      // SRAnnotation, in the brackets it was written in
 *     type:{name,display,cls,rows,cols,def}|null,        // SRType; name/display are null when the reflection
 *                                                        // reports no spelling (cls still says which kind),
 *                                                        // def = the node that declares it, or -1
 *     semantic|null, arrays:[], direction:'in|out|inout|return',   // Parameter
 *     register:{info,count,cls}|null,                    // SRRegister (frontend bind info)
 *     enumValue:{value,type}|null,                       // SREnumValue: the enumerator's value (decimal text past
 *                                                        // 2^53) and the enum's underlying type (U32, I32, ...)
 *     entry:{stage,lib}|null, returns, builtin           // builtin: folded into the collapsed summary
 *   }],
 *   builtinCollapsed: [{file,count}],                    // the built-in includes' symbols, as the CLI collapses them
 *   bytes: Uint8Array
 * }
 *
 * SPDocument: formats/oiSP, a pipeline plus where each field's value came from (`-pso-output`):
 * {
 *   name, sourceName,
 *   header: {version:'1.1', counts:{pipelines, stages, specializations, graphicsStates, raytracingStates,
 *            blendAttachments, vertexBuffers, vertexAttributes}},                      // SPHeader
 *   pipelines: [{                                        // SPPipelineBase + its state
 *     name, type:'compute|graphics|raytracing', flags:['GeneratedVertexStage',..],      // ESPPipelineFlag
 *     layoutIndex,                                       // into layouts, -1 = the device's default layout
 *     stages:[{stage, shaderFile, entrypoint, sourceHash, generated}],                  // SPStage
 *     fields:[{field, index, value, source:'derived|supplied|assumed', reason, domain, indexed}], // SPSpecialization + ESPField
 *     notes:[]
 *   }],
 *   layouts: [{                                          // every embedded oiPL (PLFile)
 *     bindings:[{name, source, class, type, isWrite, isArray, count, visibility:[stage,..],
 *                bindings:{spirv:{space,binding}, dxil:{space,binding}},
 *                strideOrLength | samplerId | texture:{primitive, formatId}}],           // PLDescriptorBinding
 *     samplers:[{filter, addressU, addressV, addressW, aniso, borderColor, comparisonFunction,
 *                enableComparison, mipBias, minLod, maxLod}],                           // PLSamplerInfo
 *     pushConstant: row | null
 *   }],
 *   bytes: Uint8Array
 * }
 * A pipeline is "exact" when no field is assumed; compute derives completely, ray tracing leaves
 * only rt.* to supply, graphics reports everything no shader signature can carry.
 *
 * ISAResult: `isa disassemble`: {asic, arch, entrypoint, stage, stats:{sgprs,vgprs,codeBytes,instrs,scratch,lds}, text}
 * ---------------------------------------------------------------------------------------
 */
(function () {
"use strict";
const M = () => window.OxMock;
const MF = () => window.OxMockFormats;
const W = () => window.OxWasm;

const wasm = () => window.OxAPI.backend === "wasm";

/* What the module can't answer, and why. The page shows this rather than letting a fabricated
 * result pass for a real one; each entry names the library call that would replace it. */
const STUBS = [
  { what: "Raw DXC (Command tab)",
    why: "the compiler doesn't expose its own argv or a raw compile entry (getCompileArgs), so there's nothing to call" },
  { what: "Reflecting a standalone SPV / DXIL binary",
    why: "backend reflection only runs as part of building an oiSH (Compiler_process); no call takes a bare binary" },
  { what: "Assembling a binary into an oiSH",
    why: "`shader assemble` produces a .spv; wrapping one into an SHFile with an identifier isn't a CLI verb yet" },
  { what: "Per-backend lean oiSH (Download -> .spv.oiSH / .dxil.oiSH)",
    why: "`file split -format oiSH` is planned, not implemented; compile with one -compile-output instead" },
  { what: "ISA disassembly",
    why: "the offline route spawns the bundled amdllpc, which a browser can't (SUPPORTS_PROCESS is off in wasm)" },
  { what: "`-asic live`",
    why: "it needs a real device through VK_KHR_pipeline_executable_properties" }
];

window.OxAPI = {

  ready: false,
  backend: "mock",                     // wasm once the module loaded
  version: null,                       // the compiler's own version, once it has answered
  loadError: null,                     // why the module isn't there, when it isn't
  stubs: STUBS,

  /* A plain browser runs the wasm module. Hosted in a VS Code webview (acquireVsCodeApi exists) the
   * extension can bridge to the native OxC3 on the machine, which is what makes `-asic live` (a real
   * Vulkan device) and the process-backed amdllpc route possible there; the UI asks for capabilities
   * once and greys out what the host can't do rather than faking it. */
  host: typeof window.acquireVsCodeApi === "function" ? "vscode" : "browser",

  async capabilities() {
    // TODO(vscode): ask the extension host what the native binary can do:
    //   liveIsa    = a Vulkan device with VK_KHR_pipeline_executable_properties is reachable
    //   offlineIsa = the bundled amdllpc can be spawned (SUPPORTS_PROCESS) or the Mesa port is built in
    // Proposed: vscode.postMessage({ cmd: "capabilities" }) -> { liveIsa, offlineIsa, devices: [...] }
    if (this.host === "vscode") return { host: this.host, liveIsa: true, offlineIsa: true };
    /* The page knows which host it is on; the module only knows what it can run there. */
    if (wasm()) {
      const caps = W().capabilities;
      return { host: this.host, liveIsa: caps.liveIsa, offlineIsa: caps.offlineIsa, threads: caps.threads };
    }
    return { host: this.host, liveIsa: false, offlineIsa: true };
  },

  /* Loads the module and falls back to the mock when it isn't there, so the page works from
   * file:// and from a checkout with no wasm build. */
  async init(options) {
    try {
      await W().load(options);
      this.backend = "wasm";
      this.version = W().version;
    } catch (e) {
      this.backend = "mock";
      this.loadError = e.message;
    }
    this.ready = true;
    return { backend: this.backend, version: this.version, error: this.loadError };
  },

  /* The @-prefixed includes, compiled into the module rather than read from disk. Without one the
   * page reads the copies js/mock_data.js recorded from exactly this call. */
  async builtinIncludes() {
    if (!wasm()) return null;
    try { return await W().builtinIncludes(); } catch (e) { return null; }
  },

  /* The documents Inspect mode and SPV / DXIL mode open on.
   *
   * With a module these are compiled for real, so every one of them has a file behind it and the
   * operations that need one (derive a pipeline, extract a binary, combine, download) work on them
   * exactly as they work on something you compiled yourself. Without one, the page keeps what
   * js/mock_data.js recorded, which is the same output with no bytes to hand back.
   *
   * It costs a handful of compiles, so the caller does this when a mode that needs them is opened
   * rather than at load.
   */
  async seedExamples(project) {

    if (!wasm()) return null;

    const seeded = { oish: {}, oisr: {}, oisp: {}, standalone: {} };

    /* Two versions of one shader, so Diff A<->B opens on something to compare. */
    const versions = { "lighting.v1.oiSH": project, "lighting.v2.oiSH": M().lightingVariantProject() };

    for (const [name, files] of Object.entries(versions)) {
      const doc = await this.compileFile("lighting.hlsl", files, { targets: ["spv", "dxil"] });
      if (!doc.doc) continue;
      doc.doc.name = name;
      seeded.oish[name] = doc.doc;
    }

    for (const source of ["lighting.hlsl", "post.hlsl"]) {
      const sr = (await this.reflectSymbols(source, project)).doc;
      if (sr) seeded.oisr[sr.name] = sr;
    }

    /* One pipeline of each kind: compute is exact, graphics reports everything no signature carries,
     * ray tracing leaves only its own limits. */
    for (const source of ["lighting.hlsl", "post.hlsl", "trace.hlsl"]) {
      const compiled = await this.compileFile(source, project, { targets: ["spv", "dxil"] });
      if (!compiled.doc) continue;
      compiled.doc.name = source.replace(/\.hlsl$/i, "") + ".oiSH";
      const sp = await this.derivePipeline(compiled.doc, {});
      if (!sp.refused) seeded.oisp[sp.name] = sp;

    }

    /* Standalone binaries are two of these lifted back out, which is what a `.spv` on disk is.
     * They come from the two versions of one shader rather than from two different shaders, because
     * the binary diff pairs on entrypoint and stage: two unrelated shaders have nothing to pair. */
    for (const [version, doc] of Object.entries(seeded.oish)) {

      const bin = doc.binaries.find(b => b.sizes.spirv);
      if (!bin) continue;

      const stem = version.replace(/\.oiSH$/i, "");
      seeded.standalone[`${stem}.${bin.lib ? "lib" : bin.entrypoint}.spv`] = {
        type: "spirv",
        bytes: await this.extractBinary(doc, bin, "spirv"),
        origin: { file: doc.sourceName, entry: bin.entrypoint }
      };
    }

    return seeded;
  },

  /* ---- OxC3 shader compile (= OxC3 compile shaders) --------------------------------- */
  /* project: {path: {src}}, opts: {targets:['spv','dxil'], debug, split, keepRegisters, reflectionOnly,
   *          warnUnusedRegisters, warnUnusedConstants, warnBufferPadding, ignoreEmptyFiles} */
  async compileFile(name, project, opts) {
    if (wasm()) {
      const r = await W().compile(name, project, opts);
      if (r.doc) r.doc.bytes = r.bytes;
      return { doc: r.doc, diags: r.diags };
    }
    await fakeLatency(250, 650);
    return M().compileFile(name, project, opts);
  },

  /* ---- raw DXC (Compile mode -> Command tab -> Run with DXC) --------------------------- */
  async compileRaw(argv, name, project) {
    // TODO(wasm): run an (edited) DXC flag line through the same DXC instance the compile path uses. The line the
    // Command tab pre-fills is what OxC3 derives; exposing the real argv (getCompileArgs) plus a raw compile entry
    // is what this needs, and neither exists. The output would be a standalone binary, never an oiSH: no annotations
    // were processed and nothing was reflected into an identifier.
    await fakeLatency(200, 500);
    return M().compileRaw(argv, name, project);
  },

  /* ---- standalone binaries as documents ---------------------------------------------- */
  async reflectBinary(name, type, bytes, origin) {
    // TODO(wasm): the backend reflection the compiler already runs while building an oiSH (spirv-reflect over SPIR-V,
    // DXC's IDxcContainerReflection over DXIL) applied to a bare binary. Compiler_process does it, but only as part of
    // a compile: it wants the entry's runtime reflection to check against, so there's no call that takes a bare binary.
    void origin;
    return M().reflectBinary(name, type, bytes, origin);
  },

  async assembleOiSH(doc, identifier) {
    // TODO(wasm): the planned `shader assemble -> oiSH`: wrap standalone binaries into an SHFile with the identifier the
    // source would have declared, so a pipeline can be created from it. The CLI's `shader assemble` only produces a .spv.
    return M().assembleOiSH(doc, identifier);
  },

  /* ---- OxC3 shader reflect-symbols (oiSR) ------------------------------------------- */
  /* Returns { doc, diags }: reflection runs between keystrokes where a compile never does, so its
   * diagnostics are the only ones a file being typed gets, and the doc alone would drop them. The doc
   * is null only when nothing at all could be reflected; the diagnostics still say why. */
  async reflectSymbols(name, project, cfg) {
    if (wasm()) {
      const r = await W().reflectSymbols(name, project, undefined, cfg);
      if (r.doc) {
        r.doc.bytes = r.bytes;
        r.doc.name = name.replace(/\.hlsl$/i, "") + ".oiSR";
      }
      return { doc: r.doc, diags: r.diags || [] };
    }
    await fakeLatency(60, 160);
    return { doc: MF().reflectSymbols(name, project), diags: [] };
  },

  async parseOiSR(name, bytes) {
    if (wasm()) {
      const doc = await W().srRead(name, bytes);
      doc.bytes = bytes;
      return doc;
    }
    return MF().parseOiSRBytes(name, bytes);
  },

  async writeOiSR(sr) {
    if (wasm() && sr.bytes) return sr.bytes;       // SRFile_write already produced these
    return MF().srBytes(sr);
  },

  /* ---- OxC3 file data / shader entrypoints / includes / feature_set ------------------ */
  async parseOiSH(name, bytes) {
    if (wasm()) {
      const doc = await W().shRead(name, bytes);
      doc.bytes = bytes;
      return doc;
    }
    return M().parseOiSHBytes(name, bytes);
  },

  /* `OxC3 file header`: sniff the magic (oiSH / oiSR / oiSP) and read what follows it. */
  async readHeader(bytes) {
    if (wasm()) {
      const r = await W().fileHeader(bytes);
      const d = r.document;
      if (r.format === "oiSR") return { format: "oiSR", ...d.header };
      if (r.format === "oiSP") return { format: "oiSP", ...d.header };
      return { format: "oiSH", version: d.header.version, compilerVersion: d.compilerVersion,
        sourceHash: d.sourceHash, sizeTypes: d.header.sizeTypes,
        binaryCount: d.binaries.length, stageCount: d.entries.length,
        includeFileCount: d.includes.length };
    }
    const magic = String.fromCharCode(...bytes.slice(0, 4));
    if (magic === "oiSR") { const d = MF().parseOiSRBytes("header", bytes); return { format: "oiSR", ...d.header }; }
    if (magic === "oiSP") { const d = MF().parseOiSPBytes("header", bytes); return { format: "oiSP", ...d.header }; }
    const doc = M().parseOiSHBytes("header", bytes);
    return { format: "oiSH", version: doc.header.version, compilerVersion: doc.compilerVersion,
      sourceHash: doc.sourceHash, sizeTypes: doc.header.sizeTypes,
      binaryCount: doc.binaries.length, stageCount: doc.entries.length,
      includeFileCount: doc.includes.length };
  },

  /* A real document already carries the bytes SHFile_write produced, so the full file is those.
   * The per-backend lean file is `file split -format oiSH`, which doesn't exist; compiling with a
   * single -compile-output produces one, which is what the CLI line in the Command tab shows. */
  async writeOiSH(doc, { backend } = {}) {
    if (wasm()) {
      if (!backend) {
        if (!doc.bytes) throw new Error("this document has no oiSH behind it (it wasn't compiled or loaded here)");
        return doc.bytes;
      }
      throw new Error(
        "a per-backend lean oiSH is `file split -format oiSH`, which isn't implemented; " +
        "compile with -compile-output " + (backend === "spirv" ? "spv" : "dxil") + " to get one"
      );
    }
    return M().oishBytes(doc);
  },

  async extractBinary(doc, bin, backend) {
    /* A standalone binary IS its bytes: no oiSH to extract from, and none needed. */
    if (doc.rawBytes && backend === doc.rawType) return doc.rawBytes;
    if (wasm()) {
      if (!doc.bytes) throw new Error("this document has no oiSH behind it (it wasn't compiled or loaded here)");
      return W().shExtractBinary(doc.bytes, doc.binaries.indexOf(bin), backend);
    }
    return M().binBytes(bin, backend);
  },

  /* ---- OxC3 shader disassemble / assemble -------------------------------------------- */
  async disassemble(type /* 'spirv'|'dxil' */, bytes) {
    if (wasm()) return W().disassemble(type, bytes);
    return "; The mock only knows how to disassemble binaries it fabricated itself.\n" +
           "; Build the wasm module (build_web.py --frontend) for Compiler_disassemble over these bytes.";
  },

  async disassembleDocBinary(doc, bin, backend) {
    if (wasm()) return this.disassemble(backend, await this.extractBinary(doc, bin, backend));
    return M().disasm(doc, bin, backend);
  },

  async assemble(type, text) {
    if (wasm()) return W().assemble(type, text);
    if (type !== "spirv") throw new Error("the mock can't assemble DXIL; the module does.");
    const enc = new TextEncoder().encode(text);
    return M().binBytes({ identKey: "assembled|" + OxUtil.crc32c(enc), sizes: { spirv: 256 + enc.length % 512, dxil: 0 } }, "spirv");
  },

  /* The validator's verdict on arbitrary bytes (spirv-val, or DXC's validator for a DXIL container); the
   * mock can't judge, so it answers valid and the real refusals still come from the module-side gates. */
  async validate(backend, bytes) {
    if (wasm()) {
      try { return await W().validate(backend, bytes); } catch (e) { return { valid: false, message: e.message }; }
    }
    return { valid: true, message: "" };
  },

  async getUniqueEntrypoints(type, bytes) {
    if (wasm()) return W().uniqueEntrypoints(type, bytes, true);
    return [{ name: "main", stage: "compute", note: "mock - build the wasm module for Compiler_getUniqueEntrypoints" }];
  },

  /* ---- OxC3 file combine -format oiSH ------------------------------------------------ */
  async combineOiSH(a, b) {
    if (wasm()) {
      if (!a.bytes || !b.bytes) throw new Error("both documents need an oiSH behind them to combine");
      const name = (a.name.replace(/\.oiSH$/i, "") + "+" + b.name.replace(/\.oiSH$/i, "")) + ".oiSH";
      const r = await W().shCombine(name, a.bytes, b.bytes);
      r.doc.bytes = r.bytes;
      r.doc.combinedFrom = [a.name, b.name];
      return r.doc;
    }
    if (a.sourceHash !== b.sourceHash)
      throw new Error(`source hash mismatch (${OxUtil.hex8(a.sourceHash)} vs ${OxUtil.hex8(b.sourceHash)}) - combine requires the same source(s), includes and compiler settings.`);
    const doc = JSON.parse(JSON.stringify(a));
    doc.name = (a.name.replace(/\.oiSH$/i, "") + "+" + b.name.replace(/\.oiSH$/i, "")) + ".oiSH";
    doc.combinedFrom = [a.name, b.name];
    return doc;
  },

  /* ---- oiSP: pipeline state with provenance ----------------------------------------- */
  /* `pick` resolves a stage kind that occurs more than once, as {stage: binaryIndex}; the module
   * takes entry indices, which is what -entry names, so the pick is resolved to one here. */
  async derivePipeline(doc, pick) {
    if (wasm()) {
      if (!doc.bytes) throw new Error("this document has no oiSH behind it");
      const picks = Object.entries(pick || {}).map(([stage, binIdx]) =>
        doc.entries.findIndex(e => e.stage === stage && e.binaryIds.includes(binIdx))
      ).filter(i => i >= 0);
      const r = await W().spDerive(doc.bytes, doc.name, picks);
      if (r.refused) return r;
      r.doc.bytes = r.bytes;
      r.doc.name = doc.name.replace(/\.oiSH$/i, "") + ".oiSP";
      return r.doc;
    }
    return MF().derivePipeline(doc, pick);
  },

  async supplyPipeline(sp, pipelineIdx, field, index, value) {
    if (wasm()) {
      const spec = sp.pipelines[pipelineIdx].fields.find(f => f.field === field && f.index === index);
      const path = field + (spec && spec.indexed ? `[${index}]` : "");
      const r = await W().spSupply(sp.bytes, pipelineIdx, path, value, sp.name);
      r.doc.bytes = r.bytes;
      r.doc.name = sp.name;
      return r.doc;
    }
    return MF().supply(sp, pipelineIdx, field, index, value);
  },

  async printPipeline(sp, pipelineIdx) {
    if (wasm()) return W().spPrint(sp.bytes, pipelineIdx);
    return MF().spPrint(sp, pipelineIdx);
  },

  async parseOiSP(name, bytes) {
    if (wasm()) {
      const doc = await W().spRead(name, bytes);
      doc.bytes = bytes;
      return doc;
    }
    return MF().parseOiSPBytes(name, bytes);
  },

  async writeOiSP(sp) {
    if (wasm() && sp.bytes) return sp.bytes;       // SPFile_finalize + SPFile_write already produced these
    return MF().spBytes(sp);
  },

  /* ---- OxC3 isa devices / isa disassemble -------------------------------------------- */
  /* Both of these drive the bundled amdllpc as a child process, so in a browser they refuse. The
   * refusal is the library's own, not a guess made here: the device-free Mesa route (RADV/ACO, and
   * Intel brw) is the one that could run as wasm, and it isn't in the CLI yet. */
  /* The project as a real .oiCA and back; wasm only, since the mock has no archive writer. */
  async projectSnapshot(files) {
    if (!wasm()) throw new Error("project snapshots need the wasm module (the mock has no oiCA writer)");
    return W().caPack(files);
  },

  async importSnapshot(bytes) {
    if (!wasm()) throw new Error("importing a snapshot needs the wasm module (the mock has no oiCA reader)");
    return W().caUnpack(bytes);
  },

  /* Extension + vendor names for the syntax reference; the recording keeps the mock tier honest. */
  async annotationEnums() {
    if (wasm()) {
      try { return await W().annotationEnums(); } catch (e) { /* fall through to the recording */ }
    }
    return (window.OxMockData && window.OxMockData.enums) || { extensions: [], vendors: [] };
  },

  /* Value names for every enum-typed oiSP field (dropdowns in the pipeline editor); the recording
   * keeps the mock tier honest the same way the annotation enums are kept. */
  async spFieldVocab() {
    if (wasm()) {
      try { return await W().spFieldVocab(); } catch (e) { /* fall through to the recording */ }
    }
    return (window.OxMockData && window.OxMockData.spVocab) || {};
  },

  async isaTargets() {
    if (wasm()) {
      try { return await W().isaTargets(); } catch (e) { return []; }
    }
    return MF().isaTargets();
  },

  async isaDisassemble(doc, bin, asic, entrypoint) {
    if (wasm()) {
      const spirv = await this.extractBinary(doc, bin, "spirv");
      if (!await W().isaHasOfflinePath(spirv))
        throw new Error("this stage has no offline ISA path (ray tracing needs pipeline context); `-asic live` does it natively");
      return { asic, entrypoint, text: await W().isaDisassemble(spirv, asic, entrypoint) };
    }
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
