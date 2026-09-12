/* api.js: the backend boundary.
 *
 * Every backend capability the UI needs goes through OxAPI, and ONLY through OxAPI. Each method
 * routes to the real OxC3_wasm module (js/wasm.js) when it loaded, and to js/mock.js +
 * js/mock_formats.js when it didn't or when nothing in the library answers that call yet.
 * OxAPI.backend says which, and OxAPI.stubs lists what is still fabricated and why, so the page
 * can be honest about it rather than the reader having to guess.
 *
 * ---------------------------------------------------------------------------------------
 * SHDocument: the JSON contract the UI consumes, written by SHFile_writeJson (formats/oiSH, core3) with the
 * module adding name and sourceName in front:
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
 * SRDocument: SRFile_writeJson (formats/oiSR), the frontend symbol AST (`shader reflect-symbols`):
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
 * SPDocument: SPFile_writeJson (formats/oiSP), a pipeline plus where each field's value came from (`-pso-output`):
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

/* Without the module there is nothing real to answer with, and a fabricated answer is impossible to
 * tell from a real one once it is on screen. So every call that would have to invent refuses, and the
 * page switches the features that need one off rather than showing something made up.
 * What survives is what js/mock_data.js recorded from a real run: the sample sources, the builtin
 * includes, the vocabularies and the example documents. That is real output, just not of anything the
 * visitor typed. */
function noModule(what) {
  throw new Error(what + " needs the compiler module, which isn't running");
}

/* The calls that still fabricate WITH the module, because the library has no answer for them yet.
 * These are worse than the no-module tier: the badge says the compiler is running, so nothing warns
 * the reader. Everything they produce carries `mock`, and every view that renders one says so. */
function markMock(doc, why) {
  if (doc && typeof doc === "object") doc.mock = why;
  return doc;
}

/* What the module can't answer, and why. The page shows this rather than letting a fabricated
 * result pass for a real one; each entry names the library call that would replace it. */
const STUBS = [
  { what: "Raw DXC (Command tab, the Run button)",
    why: "the argv is real (getCompileArgs), but a raw compile entry running an edited flag line doesn't exist yet" },
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

/* The pipeline fields the seeded examples supply, so every provenance shows in the UI: a derived one,
 * a supplied one and an assumed one. dev/gen_mock_data.js carries the same list for the recorded
 * copies of these documents; they are a pair and are meant to agree. */
const EXAMPLE_SUPPLIED = {
  "post.oiSP": [
    ["rtv.format", 0, 28],            // rgba16f, the way -pso-set rtv.format[0]=rgba16f supplies it
    ["blend.enable", 0, 1],
    ["blend.src", 0, 2],              // EBlend_SrcAlpha
    ["blend.dst", 0, 3],              // EBlend_OneMinusSrcAlpha
    ["topology", 0, 0]
  ],
  "trace.oiSP": [
    ["rt.maxRecursionDepth", 0, 2],
    ["rt.flags", 0, 2]
  ],
  "lighting.oiSP": []                 // compute derives completely, so there is nothing to supply
};

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
      let sp = await this.derivePipeline(compiled.doc, {});
      if (sp.refused) continue;

      /* A few fields supplied so the examples show every provenance rather than only derived and
       * assumed. dev/gen_mock_data.js keeps the same list for the recorded copies; the two exist for
       * the same reason and are meant to match. A field this pipeline doesn't report is skipped, since
       * which fields a stage reports is the library's decision. */

      for (const [field, index, value] of (EXAMPLE_SUPPLIED[sp.name] || [])) {
        if (!sp.pipelines.some(pl => pl.fields.some(f => f.field === field)))
          continue;
        try { sp = await this.supplyPipeline(sp, 0, field, index, value); }
        catch (e) { /* the pipeline refused this one; the example is still worth having */ }
      }

      seeded.oisp[sp.name] = sp;

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
    noModule("compiling");
  },

  /* ---- Compiler_buildCompileArgs: the dxc argv per compile, from the compiler itself ----- */
  /* Returns [{entryId, combination, binaryType, entrypoint|null, stage, lib, requiresLink,
   *   args:[], amendedSource|null, links:[{entrypoint|null, stage, combination, profile,
   *   uniforms:[{type,name,value}], dxil: uniformsHlsl|null + uniformsArgs + libs + linkArgs,
   *   spirv: spirvOpt}]}], one element per compile the driver would spawn with the link steps that
   *   finish each permutation, or null when it can't answer (no module, a module predating the
   *   export, or a source that doesn't parse); the Command tab then refuses (no module), keeps its
   *   listing (mid-edit of the same file) or says the source doesn't parse (file switch). */
  async getCompileArgs(name, project, opts) {
    if (wasm()) {
      try { return await W().getCompileArgs(name, project, opts); }
      catch (err) { return null; }
    }
    return null;
  },

  /* ---- raw DXC (Compile mode -> Command tab -> Run with DXC) --------------------------- */
  async compileRaw(argv, name, project) {
    //TODO: MOCK! NOT REAL DATA YET! This does not run DXC. The binary it hands back is fabricated,
    // and it is fabricated even with the module loaded, so nothing else on the page warns about it.
    // The argv half is real now (getCompileArgs above); what remains is a raw compile entry, so an
    // edited flag line runs through the same DXC instance the compile path uses. The output would
    // be a standalone binary, never an oiSH: no annotations processed, nothing reflected into an
    // identifier. Until then every result carries `mock` and the views render the warning.
    await fakeLatency(200, 500);
    return markMock(M().compileRaw(argv, name, project), "this binary was not produced by DXC");
  },

  /* ---- standalone binaries as documents ---------------------------------------------- */
  async reflectBinary(name, type, bytes, origin) {
    //TODO: MOCK! NOT REAL DATA YET! Nothing here reads the binary: the registers, bindings and IO are
    // invented from its bytes, with the module loaded as much as without it.
    // What it needs: the backend reflection the compiler already runs while building an oiSH
    // (spirv-reflect over SPIR-V, IDxcContainerReflection over DXIL) applied to a BARE binary.
    // Compiler_process does exactly this, but only inside a compile, since it checks the result
    // against the entry's runtime reflection; no call takes a binary on its own.
    void origin;
    return markMock(M().reflectBinary(name, type, bytes, origin), "this reflection was not read from the binary");
  },

  async assembleOiSH(doc, identifier) {
    //TODO: MOCK! NOT REAL DATA YET! No oiSH is built here; the document is fabricated around the
    // identifier you typed, with the module loaded as much as without it.
    // What it needs: `shader assemble` producing a real SHFile (a binary plus the identifier its
    // source would have declared) rather than a bare .spv, so a pipeline can be derived from it.
    return markMock(M().assembleOiSH(doc, identifier), "this oiSH was not assembled by the compiler");
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
    noModule("editor intelligence");
  },

  /* ---- Compiler_parse: entrypoints and permutations without a compile ----------------- */
  /* Returns [{name, stage, lib, combinations:[{entrypoint|null, stage|'lib', lib, model,
   *   extensions:[], defines:[{name,value}], uniforms:[{type,name,value}]}]}] with the same
   * spellings SHDocument.binaries uses, so a combination can be matched against a compiled
   * binary. Null when nothing could be listed: the source doesn't parse, or the module predates
   * the export; the caller keeps what it had (a mid-edit listing) or falls back to the compile. */
  async parseEntrypoints(name, project) {
    if (wasm()) {
      try { return await W().parseEntrypoints(name, project); }
      catch (err) { return null; }
    }
    return null;                        // no module, no listing; the picker keeps its default row
  },

  async parseOiSR(name, bytes) {
    if (wasm()) {
      const doc = await W().srRead(name, bytes);
      doc.bytes = bytes;
      return doc;
    }
    noModule("reading an oiSR");
  },

  async writeOiSR(sr) {
    if (wasm() && sr.bytes) return sr.bytes;       // SRFile_write already produced these
    noModule("writing an oiSR");
  },

  /* ---- OxC3 file data / shader entrypoints / includes / feature_set ------------------ */
  async parseOiSH(name, bytes) {
    if (wasm()) {
      const doc = await W().shRead(name, bytes);
      doc.bytes = bytes;
      return doc;
    }
    noModule("reading an oiSH");
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
    noModule("reading a file header");
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
    noModule("writing an oiSH");
  },

  async extractBinary(doc, bin, backend) {
    /* A standalone binary IS its bytes: no oiSH to extract from, and none needed. */
    if (doc.rawBytes && backend === doc.rawType) return doc.rawBytes;
    if (wasm()) {
      if (!doc.bytes) throw new Error("this document has no oiSH behind it (it wasn't compiled or loaded here)");
      return W().shExtractBinary(doc.bytes, doc.binaries.indexOf(bin), backend);
    }
    noModule("extracting a binary");
  },

  /* ---- OxC3 shader disassemble / assemble -------------------------------------------- */
  async disassemble(type /* 'spirv'|'dxil' */, bytes) {
    if (wasm()) return W().disassemble(type, bytes);
    noModule("disassembling");
  },

  async disassembleDocBinary(doc, bin, backend) {
    if (wasm()) return this.disassemble(backend, await this.extractBinary(doc, bin, backend));
    noModule("disassembling");
  },

  async assemble(type, text) {
    if (wasm()) return W().assemble(type, text);
    noModule("assembling");
  },

  /* The validator's verdict on arbitrary bytes (spirv-val, or DXC's validator for a DXIL container); the
   * mock can't judge, so it answers valid and the real refusals still come from the module-side gates. */
  async validate(backend, bytes) {
    if (wasm()) {
      try { return await W().validate(backend, bytes); } catch (e) { return { valid: false, message: e.message }; }
    }
    noModule("validating");
  },

  async getUniqueEntrypoints(type, bytes) {
    if (wasm()) return W().uniqueEntrypoints(type, bytes, true);
    noModule("listing entrypoints");
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
    noModule("combining");
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
    noModule("deriving a pipeline");
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
    noModule("supplying a pipeline field");
  },

  async printPipeline(sp, pipelineIdx) {
    if (wasm()) return W().spPrint(sp.bytes, pipelineIdx);
    noModule("printing a pipeline");
  },

  async parseOiSP(name, bytes) {
    if (wasm()) {
      const doc = await W().spRead(name, bytes);
      doc.bytes = bytes;
      return doc;
    }
    noModule("reading an oiSP");
  },

  async writeOiSP(sp) {
    if (wasm() && sp.bytes) return sp.bytes;       // SPFile_finalize + SPFile_write already produced these
    noModule("writing an oiSP");
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

  /* The compiler's own vocabularies: {extensions, vendors, extensionsNoDxil, extensionsNoSpirv,
   * stages:[{name, lib, profile}], shaderModels:{min, max}, extensionMinModel:{name: model},
   * version:{major, minor, patch}}. The syntax reference, the Assemble card
   * and the mock tier all read these, off the module or off the recording taken from it. */
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
    return [];                         // no module, no target list to offer
  },

  async isaDisassemble(doc, bin, asic, entrypoint) {
    if (wasm()) {
      const spirv = await this.extractBinary(doc, bin, "spirv");
      if (!await W().isaHasOfflinePath(spirv))
        throw new Error("this stage has no offline ISA path (ray tracing needs pipeline context); `-asic live` does it natively");
      return { asic, entrypoint, text: await W().isaDisassemble(spirv, asic, entrypoint) };
    }
    await fakeLatency(120, 400);
    noModule("ISA disassembly");
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
