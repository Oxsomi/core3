/* dev/gen_mock_data.js: records real compiler output into js/mock_data.js.
 *
 * The mock exists so the page works without a wasm build. What it shows there used to be invented:
 * hand-written excerpts of the built-in includes, and documents produced by a heuristic HLSL parse.
 * Everything it invented now has a format behind it (oiSH, oiSR, oiSP), so it is recorded from a real
 * run instead, and the fallback shows what the compiler actually produced rather than a plausible
 * imitation of it.
 *
 *   node web/dev/gen_mock_data.js [path/to/OxC3_wasm.js]
 *
 * Re-run it when the sample project, the built-in includes or any of the three document contracts
 * change; js/mock_data.js is generated and committed, so nobody needs a wasm build to read it.
 *
 * File bytes are deliberately not recorded. They would roughly double the file, and the fallback has
 * no reader for them: a download without a module still goes through the mock's own serializer.
 */
"use strict";

const path = require("path");
const fs = require("fs");

const root = path.join(__dirname, "..", "..");
const modulePath = process.argv[2] ||
  path.join(root, "build", "Release", "web", "wasm64", "bin", "OxC3_wasm.js");

if (!fs.existsSync(modulePath)) {
  console.error(`-- No module at ${modulePath} (build it with build_web.py --frontend)`);
  process.exit(1);
}

/* web/samples/ is the single source of truth for the sample project: this reads the files straight
 * off disk (mock.js gets them through the very mock_data.js this generates, so reading them via
 * OxMock here would chase this script's own previous output). */
function readSamples() {
  const dir = path.join(root, "web", "samples");
  const out = {};
  const walk = sub => {
    for (const entry of fs.readdirSync(path.join(dir, sub), { withFileTypes: true })) {
      const rel = sub ? sub + "/" + entry.name : entry.name;
      if (entry.isDirectory()) walk(rel);
      else if (/\.(hlsl|hlsli)$/i.test(entry.name)) out[rel] = fs.readFileSync(path.join(dir, rel), "utf8");
    }
  };
  walk("");
  return out;
}

const samples = readSamples();
const SAMPLE_FILES = Object.fromEntries(Object.entries(samples).map(([n, src]) => [n, { src, diags: [] }]));

/* mock.js still supplies the lighting-variant helper; feed it the samples it now expects from data. */
global.window = { OxMockData: { samples } };
new Function(fs.readFileSync(path.join(root, "web", "js", "util.js"), "utf8"))();
new Function(fs.readFileSync(path.join(root, "web", "js", "mock.js"), "utf8"))();

/* The second version of the sample the Diff A<->B demo pairs against the first. It's the sample
 * project's own content, so it comes from there rather than being spelled out twice. */
const lightingV2 = global.window.OxMock.lightingVariantProject;

const OxWasm = require(path.join(__dirname, "..", "js", "wasm.js"));

/* The pipeline fields the seeds supply, so every provenance shows in the UI: a derived one, a
 * supplied one and an assumed one. A field that the pipeline doesn't report is skipped rather than
 * failing the generation, since which fields a stage reports is the library's decision. */
const SUPPLIED = {
  "post.oiSP": [
    ["rtv.format", 0, 28],          // rgba16f, the way -pso-set rtv.format[0]=rgba16f supplies it
    ["blend.enable", 0, 1],
    ["blend.src", 0, 2],            // EBlend_SrcAlpha
    ["blend.dst", 0, 3],            // EBlend_OneMinusSrcAlpha
    ["topology", 0, 0]
  ],
  "trace.oiSP": [
    ["rt.maxRecursionDepth", 0, 2],
    ["rt.flags", 0, 2]
  ],
  "lighting.oiSP": []               // compute derives completely, so there is nothing to supply
};

/* One reflect, one module, one process. Returns the SRDocument or throws with what the module said. */
function reflectInOwnProcess(source) {

  const script = `
    const path = require("path");
    global.window = { addEventListener() {} };
    const OxWasm = require(${JSON.stringify(path.join(__dirname, "..", "js", "wasm.js"))});
    const project = ${JSON.stringify(SAMPLE_FILES)};
    (async () => {
      await OxWasm.load({
        factory: require(${JSON.stringify(modulePath)}),
        locateFile: f => path.join(${JSON.stringify(path.dirname(modulePath))}, f)
      });
      const r = await OxWasm.reflectSymbols(${JSON.stringify(source)}, project);
      if (!r.doc) { console.error("REFLECT_FAILED " + r.error); process.exit(1); }
      process.stdout.write("@@" + JSON.stringify(r.doc));
    })();
  `;

  const run = require("child_process").spawnSync(process.execPath, ["-e", script], {
    maxBuffer: 64 * 1024 * 1024, encoding: "utf8"
  });

  const marker = (run.stdout || "").indexOf("@@");

  if (run.status !== 0 || marker < 0)
    throw new Error(`${source} failed to reflect: ${(run.stderr || "").trim().split("\n").pop()}`);

  return JSON.parse(run.stdout.slice(marker + 2));
}

async function main() {

  const createOxC3Module = require(modulePath);
  await OxWasm.load({
    factory: createOxC3Module,
    locateFile: f => path.join(path.dirname(modulePath), f)
  });

  const data = { samples, builtins: {}, oish: {}, oisr: {}, oisp: {} };

  /* The annotation vocabularies, exactly as the oiSH enums spell them. */
  data.enums = await OxWasm.annotationEnums();
  data.spVocab = await OxWasm.spFieldVocab();

  /* The @-prefixed includes, exactly as the compiler embeds them. */
  for (const include of await OxWasm.builtinIncludes())
    data.builtins["@" + include.name] = include.src;

  /* Symbols follow the source, so these are reflected rather than taken from a compile.
   * Each one runs in a module of its own, in its own process: the reflector faults partway into a
   * sequence of calls (see docs/web_browser_flavor.md), and one call per module is the only shape that
   * reliably avoids it. A dev script can afford the process; the page cannot, which is why it is a
   * documented fault rather than a workaround shared with js/api.js. */
  for (const source of ["lighting.hlsl", "post.hlsl"]) {
    const doc = reflectInOwnProcess(source);
    doc.name = source.replace(/\.hlsl$/i, "") + ".oiSR";
    data.oisr[doc.name] = doc;
  }

  /* Two versions of the same shader, so Inspect mode opens on something to diff. */
  const versions = { "lighting.v1.oiSH": SAMPLE_FILES, "lighting.v2.oiSH": lightingV2() };

  for (const [name, project] of Object.entries(versions)) {
    const compiled = await OxWasm.compile("lighting.hlsl", project, { targets: ["spv", "dxil"] });
    if (!compiled.doc) throw new Error(`${name} failed to compile: ${compiled.diags.map(d => d.msg).join(" | ")}`);
    compiled.doc.name = name;
    data.oish[name] = compiled.doc;
  }

  /* One pipeline of each kind: compute derives completely, graphics reports everything no signature
   * carries, ray tracing leaves only its own limits. */
  for (const source of ["lighting.hlsl", "post.hlsl", "trace.hlsl"]) {

    const name = source.replace(/\.hlsl$/i, "") + ".oiSP";
    const compiled = await OxWasm.compile(source, SAMPLE_FILES, { targets: ["spv", "dxil"] });
    if (!compiled.doc) throw new Error(`${source} failed to compile: ${compiled.diags.map(d => d.msg).join(" | ")}`);

    let derived = await OxWasm.spDerive(compiled.bytes, source.replace(/\.hlsl$/i, "") + ".oiSH", []);
    if (derived.refused) { console.log(`-- ${name}: ${derived.refused}`); continue; }

    let bytes = derived.bytes;
    let doc = derived.doc;

    for (const [field, index, value] of SUPPLIED[name] || []) {
      /* Only an indexed field takes a subscript, so the path is composed from what the report says
       * about the field rather than from the shape of the entry above. */
      const reported = doc.pipelines[0].fields.find(f => f.field === field && f.index === index);
      if (!reported) continue;
      const supplied = await OxWasm.spSupply(bytes, 0, field + (reported.indexed ? `[${index}]` : ""), value, name);
      bytes = supplied.bytes;
      doc = supplied.doc;
    }

    doc.name = name;
    data.oisp[name] = doc;
  }

  const header =
`/* mock_data.js: GENERATED by dev/gen_mock_data.js, do not edit.
 *
 * Real compiler output, recorded so the page shows something faithful when no wasm module is there:
 * the built-in include sources exactly as the compiler embeds them, and the sample project's oiSH,
 * oiSR and oiSP documents exactly as it produced them.
 *
 * The file bytes behind these documents are not recorded, so a download in the fallback still goes
 * through the mock's own serializer; everything the page reads is real.
 *
 * Regenerate with: node web/dev/gen_mock_data.js
 */
`;

  const out = path.join(root, "web", "js", "mock_data.js");
  fs.writeFileSync(out,
    header + "window.OxMockData = " + JSON.stringify(data) + ";\n"
  );

  const size = fs.statSync(out).size;
  console.log(
    `-- ${path.relative(root, out)}: ${Object.keys(data.samples).length} samples, ${Object.keys(data.builtins).length} builtins, ` +
    `${Object.keys(data.oish).length} oiSH, ${Object.keys(data.oisr).length} oiSR, ` +
    `${Object.keys(data.oisp).length} oiSP (${Math.round(size / 1024)} KiB)`
  );

  OxWasm.shutdown();
}

main().catch(e => { console.error(e); process.exit(1); });
