/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2026 Oxsomi / Nielsbishere (Niels Brunekreef)
*
*  This program is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, either version 3 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program. If not, see https://github.com/Oxsomi/core3/blob/main/LICENSE.
*  Be aware that GPL3 requires closed source products to be GPL3 too if released to the public.
*  To prevent this a separate license will have to be requested at contact@osomi.net for a premium;
*  This is called dual licensing.
*/

//shader_compiler/test/test_shader_compiler_corpus.c

#include "test_shader_compiler_shared.h"
#include "shader_compiler/compiler.h"
#include "shader_compiler/spirv_isa.h"
#include "formats/oiSH/sh_binaries.h"
#include "formats/oiSH/sh_file.h"
#include "formats/oiSR/sr_file.h"
#include "platforms/platform.h"
#include "platforms/file.h"
#include "types/container/string.h"
#include "types/container/buffer.h"
#include "types/container/list_basic_types.h"
#include "types/container/memory_stream.h"
#include "types/container/ref_ptr.h"
#include "types/container/log.h"
#include "types/base/time.h"
#include "types/base/error.h"
#include "types/base/string_read_helper.h"

//Parse an in-memory oiSH and dump its reflection, so a snapshot mismatch shows *what* changed.
static void printOiSH(const Allocator *alloc, Buffer buf, const C8 *label) {

	Error err = Error_none();
	const RefPtrType msType = MemoryStream_makeType(alloc);
	MemoryStreamRef *ms = NULL;
	SHFile file = (SHFile) { 0 };
	U64 off = 0;

	Log_debugLn(alloc, "--- oiSH (%s) ---", label);

	if (
		MemoryStream_createFromBufferRegion(
			Buffer_createRefFromBuffer(buf, true), 0, Buffer_length(buf), EMemoryStreamFlags_None, &msType, &ms, &err
		) &&
		SHFile_read((StreamRef*) ms, &off, false, alloc, &file, &err)
	)
		SHFile_print(&file, true, alloc);

	else Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);

	SHFile_free(&file, alloc);
	RefPtr_dec(&ms);
}

//Parse an in-memory oiSR and dump its verbose reflection tree, so a snapshot mismatch shows *what* changed.
static void printOiSR(const Allocator *alloc, Buffer buf, const C8 *label) {

	Error err = Error_none();
	const RefPtrType msType = MemoryStream_makeType(alloc);
	MemoryStreamRef *ms = NULL;
	SRFile file = (SRFile) { 0 };
	U64 off = 0;

	Log_debugLn(alloc, "--- oiSR (%s) ---", label);

	if (
		MemoryStream_createFromBufferRegion(
			Buffer_createRefFromBuffer(buf, true), 0, Buffer_length(buf), EMemoryStreamFlags_None, &msType, &ms, &err
		) &&
		SRFile_read((StreamRef*) ms, &off, false, alloc, &file, &err)
	)
		SRFile_print(&file, 0, true, true, alloc);

	else Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);

	SRFile_free(&file, alloc);
	RefPtr_dec(&ms);
}

//Read an in-memory oiSH into an SHFile (returns false + prints on failure).
static Bool shReadFile(const Allocator *alloc, Buffer buf, SHFile *out) {

	Error err = Error_none();
	const RefPtrType msType = MemoryStream_makeType(alloc);
	MemoryStreamRef *ms = NULL;
	U64 off = 0;

	Bool ok =
		MemoryStream_createFromBufferRegion(
			Buffer_createRefFromBuffer(buf, true), 0, Buffer_length(buf), EMemoryStreamFlags_None, &msType, &ms, &err
		) &&
		SHFile_read((StreamRef*) ms, &off, false, alloc, out, &err);

	RefPtr_dec(&ms);

	if(!ok)
		Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);

	return ok;
}

//Re-serialize an oiSH with the fields that churn without the OUTPUT changing blanked out.
//
//Two of them are metadata about the build rather than about what was compiled: the OxC3 version stamped into
// every header, which moves on every release, and the CRC32C recorded per include, which moves when any
// embedded header is touched even for whitespace.
//Neither can change the bytecode or the reflection on its own, so letting them fail the snapshot meant all 32
// references churned for a version bump or a stray space in types.hlsli.
//
//sourceHash is deliberately NOT blanked: that one moves when the corpus shader itself changes, which is
// exactly when the reference should be looked at again.
//
//The header's own hash covers the include CRCs, so it is recomputed rather than patched; writing the file out
// again does that.

static Bool shNormalize(const Allocator *alloc, Buffer in, Buffer *out) {

	SHFile file = (SHFile) { 0 };
	const RefPtrType msType = MemoryStream_makeType(alloc);
	MemoryStreamRef *ms = NULL;
	Error err = Error_none();
	U64 off = 0;
	Bool ok = false;

	if(!shReadFile(alloc, in, &file))
		goto clean;

	file.compilerVersion = 0;

	for(U64 i = 0; i < file.includes.length; ++i)
		file.includes.ptrNonConst[i].crc32c = 0;

	//A rebuilt DXC restamps the generator word of every SPIRV header (word 2, holding the tool's id in the
	// high half and the tool's own version in the low half) while emitting byte identical instructions,
	// so keeping that low half turns the entire corpus red whenever the toolchain package is rebuilt.
	//Only the version half is dropped, so a binary that came out of a different tool is still caught.

	for(U64 i = 0; i < file.binaries.length; ++i) {

		const Buffer spirv = file.binaries.ptr[i].binaries[ESHBinaryType_SPIRV];
		Bool readMagic = false;

		if(Buffer_length(spirv) < sizeof(U32) * 5 || Buffer_isConstRef(spirv))
			continue;

		if(Buffer_readU32(spirv, 0, &readMagic, NULL) != 0x07230203 || !readMagic)
			continue;

		const U32 generator = Buffer_readU32(spirv, sizeof(U32) * 2, NULL, NULL);
		Buffer_writeU32(spirv, sizeof(U32) * 2, generator & 0xFFFF0000, NULL);
	}

	if(!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &msType, &ms, &err))
		goto clean;

	if(!SHFile_write((StreamRef*) ms, &off, &file, alloc, &err))
		goto clean;

	ok = MemoryStream_move(&ms, out, &err);

clean:

	if(!ok && err.genericError)
		Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);

	RefPtr_dec(&ms);
	SHFile_free(&file, alloc);
	return ok;
}

//True when two oiSH carry the same compiled output, ignoring only the churn shNormalize blanks.

static Bool shContentMatches(const Allocator *alloc, Buffer a, Buffer b) {

	Buffer na = Buffer_createNull(), nb = Buffer_createNull();

	const Bool match =
		shNormalize(alloc, a, &na) &&
		shNormalize(alloc, b, &nb) &&
		Buffer_eq(na, nb);

	Buffer_free(&na, alloc);
	Buffer_free(&nb, alloc);
	return match;
}

//Resolve the semantic name for one I/O slot.
//A zero name index is the default (TEXCOORD for an input, SV_TARGET for an output).
//Otherwise it indexes semanticNames (inputs first, outputs after uniqueInputSemantics).
static CharString shSemanticName(const SHEntry *e, Bool isOutput, U8 slot) {

	const U8 v = isOutput ? e->outputSemanticNames[slot] : e->inputSemanticNames[slot];
	const U8 nameIdx = (U8) (v >> 4);

	if(!nameIdx)
		return CharString_createRefCStrConst(isOutput ? "SV_TARGET" : "TEXCOORD");

	const U64 li = isOutput ? ((U64) e->uniqueInputSemantics + nameIdx - 1) : ((U64) nameIdx - 1);
	return e->semanticNames.ptr[li];
}

static const SHEntry *shFindEntry(const SHFile *f, CharString name) {

	for(U64 i = 0; i < f->entries.length; ++i)
		if(CharString_equalsStringSensitive(&f->entries.ptr[i].name, &name))
			return &f->entries.ptr[i];

	return NULL;
}

//Cross-backend reflection check: the same shader compiled to SPIRV and DXIL must expose the same entrypoints
// and, per entrypoint, the same input/output signature (type + semantic).
//Resources are intentionally NOT compared here: bindings differ by backend, but so does the very representation
// of some resources -- e.g. a push constant is a distinct PushConstants register in SPIRV but an ordinary cbuffer
// ($Globals / ConstantBuffer) in DXIL, which has no push-constant concept -- so a resource comparison needs a
// dedicated reconciliation pass rather than a naive name/type match.
static void shCrossCheckReflection(Test *t, const Allocator *alloc, Buffer spvBuf, Buffer dxBuf) {

	SHFile spv = (SHFile) { 0 }, dx = (SHFile) { 0 };

	if(!shReadFile(alloc, spvBuf, &spv) || !shReadFile(alloc, dxBuf, &dx)) {
		Test_assert(t, "cross-check: both oiSH read", false);
		goto clean;
	}

	Test_assert(t, "cross-check: same entrypoint count", spv.entries.length == dx.entries.length);

	for(U64 i = 0; i < spv.entries.length; ++i) {

		const SHEntry *se = &spv.entries.ptr[i];
		const SHEntry *de = shFindEntry(&dx, se->name);

		if(!de) {
			Test_assert(t, "cross-check: DXIL has matching entrypoint", false);
			continue;
		}

		Test_assert(t, "cross-check: same stage", se->stage == de->stage);

		//inputs[16]/outputs[16] are backend-agnostic ESBType arrays indexed by register, so a missing or extra
		// input (the reflection bug this guards against) shows up directly as a mismatch here.
		Test_assert(t, "cross-check: same input types", Buffer_eq(
			Buffer_createRefConst(se->inputs, sizeof(se->inputs)), Buffer_createRefConst(de->inputs, sizeof(de->inputs))
		));
		Test_assert(t, "cross-check: same output types", Buffer_eq(
			Buffer_createRefConst(se->outputs, sizeof(se->outputs)), Buffer_createRefConst(de->outputs, sizeof(de->outputs))
		));

		//Semantic name + index per used slot, resolved so it's independent of each backend's semanticNames ordering.
		for(U8 s = 0; s < 16; ++s) {

			if(se->inputs[s] || de->inputs[s]) {
				const CharString sn = shSemanticName(se, false, s), dn = shSemanticName(de, false, s);
				Test_assert(t, "cross-check: same input semantic", CharString_equalsStringInsensitive(&sn, &dn) &&
					(se->inputSemanticNames[s] & 0xF) == (de->inputSemanticNames[s] & 0xF));
			}

			if(se->outputs[s] || de->outputs[s]) {
				const CharString sn = shSemanticName(se, true, s), dn = shSemanticName(de, true, s);
				Test_assert(t, "cross-check: same output semantic", CharString_equalsStringInsensitive(&sn, &dn) &&
					(se->outputSemanticNames[s] & 0xF) == (de->outputSemanticNames[s] & 0xF));
			}
		}
	}

clean:
	SHFile_free(&spv, alloc);
	SHFile_free(&dx, alloc);
}

//True if two oiSH buffers carry the same reflection (entry signatures + extensions),
// i.e. only the compiled SPIRV/DXIL bytecode differs.
//That's the benign case where a DXC update churns bytecode without changing meaning.
static Bool shReflectionMatches(const Allocator *alloc, Buffer a, Buffer b) {

	SHFile fa = (SHFile) { 0 }, fb = (SHFile) { 0 };
	Bool match = false;

	if(!shReadFile(alloc, a, &fa) || !shReadFile(alloc, b, &fb))
		goto clean;

	if(fa.entries.length != fb.entries.length || fa.binaries.length != fb.binaries.length)
		goto clean;

	match = true;

	for(U64 i = 0; i < fa.binaries.length && match; ++i)
		if(fa.binaries.ptr[i].identifier.extensions != fb.binaries.ptr[i].identifier.extensions)
			match = false;

	for(U64 i = 0; i < fa.entries.length && match; ++i) {

		const SHEntry *ea = &fa.entries.ptr[i];
		const SHEntry *eb = shFindEntry(&fb, ea->name);

		if(
			!eb || ea->stage != eb->stage ||
			!Buffer_eq(
				Buffer_createRefConst(ea->inputs, sizeof(ea->inputs)),
				Buffer_createRefConst(eb->inputs, sizeof(eb->inputs))
			) ||
			!Buffer_eq(
				Buffer_createRefConst(ea->outputs, sizeof(ea->outputs)),
				Buffer_createRefConst(eb->outputs, sizeof(eb->outputs))
			)
		)
			match = false;
	}

clean:
	SHFile_free(&fa, alloc);
	SHFile_free(&fb, alloc);
	return match;
}

//For each binary type present in BOTH oiSH (SPIRV and/or DXIL), disassemble the reference and produced blobs
// and, only when the disassembly actually differs, write <ref>.ref.<ext> / <ref>.new.<ext> so it can be diffed
// (`git diff --no-index`).
//Identical disassembly (e.g. only the binary header's generator-version word churned, as a DXC update does)
// writes nothing and is just logged - so this stays quiet on benign version bumps.
static void dumpDisasmDiff(
	const Allocator *alloc, Compiler *comp, Buffer produced, Buffer golden, CharString ref, Bool allowWrite
) {

	SHFile fp = (SHFile) { 0 }, fg = (SHFile) { 0 };
	const RefPtrType fileHandleType = FileHandle_makeType(alloc);

	if(!shReadFile(alloc, produced, &fp) || !shReadFile(alloc, golden, &fg))
		goto clean;

	if(!fp.binaries.length || !fg.binaries.length)
		goto clean;

	const ESHBinaryType types[2] = { ESHBinaryType_SPIRV, ESHBinaryType_DXIL };
	const C8 *exts[2] = { "spvasm", "dxil" };

	for(U64 k = 0; k < 2; ++k) {

		const Buffer gBin = fg.binaries.ptr[0].binaries[types[k]];
		const Buffer pBin = fp.binaries.ptr[0].binaries[types[k]];

		if(!Buffer_length(gBin) || !Buffer_length(pBin))        //The corpus snapshots SPIRV only, so DXIL is absent
			continue;

		Error err = Error_none();
		CharString gDis = CharString_createNull(), pDis = CharString_createNull();

		if(
			Compiler_disassemble(comp, types[k], gBin, alloc, &gDis, &err) &&
			Compiler_disassemble(comp, types[k], pBin, alloc, &pDis, &err)
		) {
			if(CharString_equalsStringSensitive(&gDis, &pDis))
				Log_debugLn(alloc, "\t%s disassembly identical - only the binary header (generator version) churned", exts[k]);

			else if (!allowWrite)
				Log_warnLn(alloc, "\t%s disassembly differs (dump skipped, the bundled corpus is read only)", exts[k]);

			else {

				CharString refPath = CharString_createNull(), newPath = CharString_createNull();

				if(
					CharString_format(alloc, &refPath, &err, "%.*s.ref.%s", (int) CharString_length(ref), ref.ptr, exts[k]) &&
					CharString_format(alloc, &newPath, &err, "%.*s.new.%s", (int) CharString_length(ref), ref.ptr, exts[k])
				) {
					Buffer gb = CharString_bufferConst(gDis), pb = CharString_bufferConst(pDis);
					File_write(&gb, &refPath, 0, 0, 1 * SECOND, true, &fileHandleType, &err);
					File_write(&pb, &newPath, 0, 0, 1 * SECOND, true, &fileHandleType, &err);
					Log_warnLn(alloc, "\t%s disassembly differs -> wrote .ref.%s / .new.%s", exts[k], exts[k], exts[k]);
				}

				CharString_free(&refPath, alloc);
				CharString_free(&newPath, alloc);
			}
		}

		else Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);

		CharString_free(&gDis, alloc);
		CharString_free(&pDis, alloc);
	}

clean:
	SHFile_free(&fp, alloc);
	SHFile_free(&fg, alloc);
}

//End-to-end snapshot test: enumerate the whole test/hlsl corpus (relative to the working directory,
// which CMake points at the corpus folder) and run it through the real pipeline
// (preprocess -> DXC compile -> reflect -> link -> oiSH assembly) targeting SPIRV, entirely in memory.
//Each produced oiSH must match its committed reference (allOutputs[i], e.g. dummy.oiSH) byte-for-byte,
// so any change in the compiler output is caught.
//A missing reference is generated once and the assertion fails,
// so new references are noticed, reviewed and committed rather than silently accepted.

void Test_shaderCompilerCorpus(Test *t) {

	Test_setModule(t, "Compiler corpus");

	const Allocator *alloc = Platform_instance->alloc;
	Error err = Error_none(), *e_rr = &err;
	Bool s_uccess = true;

	ListCharString allFiles = (ListCharString) { 0 };
	ListCharString allShaderText = (ListCharString) { 0 };
	ListCharString allOutputs = (ListCharString) { 0 };
	ListU8 allCompileModes = (ListU8) { 0 };
	ListBuffer allBuffers = (ListBuffer) { 0 };
	ListCharString includeDirs = (ListCharString) { 0 };
	Buffer golden = Buffer_createNull();
	Bool isFolder = false;

	const RefPtrType fileHandleType = FileHandle_makeType(alloc);

	//Rooted via TEST_SHADER_ROOT so the same corpus runs from the working directory (desktop ctest) and
	// from the virtual file system when bundled; see test_shader_compiler_shared.h.

	const CharString here = CharString_createRefCStrConst(TEST_SHADER_ROOT "hlsl");

	//Bundled, the corpus lives in the read only virtual file system: compare, never regenerate.
	//References are only written on desktop, where they sit in the repo to be reviewed and committed.

	#ifdef TEST_SHADER_SECTION
		const Bool corpusWritable = false;
	#else
		const Bool corpusWritable = true;
	#endif

	CharString refPath = CharString_createNull();

	//A disassembler used only to emit before/after SPIRV text on a byte-snapshot mismatch (see below).
	Compiler disasmComp = (Compiler) { 0 };
	const Bool disasmCompCreated = Compiler_create(alloc, &disasmComp, &err);
	err = Error_none();

	//Enumerate + resolve every .hlsl entrypoint in the corpus folder, targeting SPIRV for the byte-snapshot.
	//A separate DXIL compile+reflect coverage pass follows below; SPIRV and DXIL are snapshotted separately, see that
	// pass for the reason.

	gotoIfError3(clean, Compiler_getTargetsFromFile(
		here,
		ECompileType_Compile,
		(U64)1 << ESHBinaryType_SPIRV,      //Single SPIRV target (byte-snapshot)
		false,                              //multipleModes
		true,                               //combineFlag
		true,                               //enableLogging
		alloc,
		&isFolder,
		NULL,
		&allFiles,
		&allShaderText,
		&allOutputs,
		&allCompileModes
	));

	Test_assert(t, "corpus folder enumerated .hlsl shaders", isFolder && allFiles.length >= 1);

	//Shaders that can't be compiled here (e.g. inheritance, which provokes an uncatchable DXC assert on a
	// multi-base-class StructuredBuffer element) are renamed to *.hlsl.disabled on disk
	// so the .hlsl-only enumerator skips them, while keeping the source around as a repro.
	//No in-test filtering needed.

	//Sibling .hlsli includes resolve relative to the corpus folder

	gotoIfError3(clean, ListCharString_createRefConst(&here, 1, &includeDirs, e_rr));

	//Compile the whole corpus into in-memory oiSH buffers.
	//ignoreEmptyFiles tolerates include-only files.

	//Don't abort on a single failing shader: assert the whole corpus compiled, but still snapshot every
	//oiSH that was produced (failed entries come back as empty buffers and are skipped below).

	Bool compiledAll = Compiler_compileShaders(
		&allFiles, &allShaderText, &allOutputs, &allCompileModes,
		1,                                  //threadCount (single-threaded, deterministic)
		false,                              //isDebug
		false,                              //keepRegisters
		(ECompilerWarning) 0,               //no extra warnings
		true,                               //ignoreEmptyFiles
		ECompileType_Compile,
		&includeDirs,
		true,                               //enableLogging
		alloc,
		&allBuffers,
		&err
	);

	Test_assert(t, "entire corpus compiled", compiledAll && !err.genericError);
	err = Error_none();

	//Snapshot each produced oiSH against its committed reference

	for (U64 i = 0; i < allBuffers.length; ++i) {

		const Buffer produced = allBuffers.ptr[i];

		if (!Buffer_length(produced))       //Include-only / ignored empty files produce nothing
			continue;

		//allOutputs entries are bare names ("dummy.oiSH") relative to the corpus folder's parent,
		// which is the working directory on desktop.
		//Bundled, a bare name would resolve into the app's writable storage instead of the virtual file system,
		// so the reference is re-rooted explicitly.

		CharString_free(&refPath, alloc);

		gotoIfError3(clean, CharString_format(
			alloc, &refPath, e_rr, "%s%.*s",
			TEST_SHADER_ROOT, (int) CharString_length(allOutputs.ptr[i]), allOutputs.ptr[i].ptr
		));

		const CharString ref = CharString_createRefStrConst(refPath);

		if (File_has(&ref, alloc)) {

			Buffer_free(&golden, alloc);
			gotoIfError3(clean, File_read(&ref, 1 * SECOND, 0, 0, &fileHandleType, &golden, e_rr));

			//Exact first, so an untouched reference is still compared byte for byte.
			//Only when that fails does the version/include-CRC tolerance get a say; see shContentMatches.

			Bool matches = Buffer_eq(produced, golden);

			if(!matches && shContentMatches(alloc, produced, golden)) {
				Log_warnLn(
					alloc, "\toiSH differs from %.*s only in version/include metadata, content is identical",
					(int) CharString_length(ref), ref.ptr
				);
				matches = true;
			}

			if (!matches) {

				Log_errorLn(alloc, "oiSH mismatch vs reference %.*s", (int) CharString_length(ref), ref.ptr);
				printOiSH(alloc, produced, "produced");
				printOiSH(alloc, golden, "reference");

				//When only the bytecode changed (reflection identical),
				// emit before/after SPIRV disassembly next to the reference (<ref>.ref.spvasm vs <ref>.new.spvasm)
				// so `git diff --no-index` shows the delta a DXC update introduced.
				//If reflection *also* differs, that's a real change - the dumps above show it.
				if(disasmCompCreated && shReflectionMatches(alloc, produced, golden)) {
					Log_warnLn(alloc, "\treflection unchanged - only bytecode differs; comparing disassembly");
					dumpDisasmDiff(alloc, &disasmComp, produced, golden, ref, corpusWritable);
				}

				else Log_warnLn(alloc, "\treflection ALSO differs (not just bytecode) - inspect the reflection dumps above");
			}

			Test_assert(t, ref.ptr, matches);
		}

		else if (!corpusWritable) {
			Log_errorLn(alloc, "Reference %.*s is missing from the bundled corpus", (int) CharString_length(ref), ref.ptr);
			Test_assert(t, ref.ptr, false);         //Can't regenerate from a read only bundle; fix on desktop
		}

		else {
			gotoIfError3(clean, File_write(&produced, &ref, 0, 0, 1 * SECOND, true, &fileHandleType, e_rr));
			Log_warnLn(alloc, "Generated missing reference %.*s (review & commit)", (int) CharString_length(ref), ref.ptr);
			Test_assert(t, ref.ptr, false);         //Red until the new reference is reviewed & committed
		}
	}

	//--- oiSR reflection snapshot: reflect every corpus source into an oiSR (frontend symbol AST) and byte-snapshot
	//--- it against a committed <name>.oiSR reference.
	//--- This runs the reflection walker (Compiler_reflect) over the WHOLE corpus - so any shader it can't handle
	//--- surfaces here - and pins its output, using the same "missing reference -> write once + fail" convention as
	//--- the oiSH snapshots above.

	if (disasmCompCreated) {

		const RefPtrType msType = MemoryStream_makeType(alloc);

		for (U64 i = 0; i < allFiles.length; ++i) {

			SRFile reflection = (SRFile) { 0 };
			StreamRef *ws = NULL;
			Buffer produced = Buffer_createNull();
			CharString ref = CharString_createNull();
			CharString relPath = CharString_createNull();

			//Reflect with a path relative to the corpus (forward slashes), not the absolute enumerator path: the
			// source filename is baked into the oiSR (symbol locations), so an absolute path would make the committed
			// reference machine-specific.
			//The output ref is <name>.oiSH -> <name>.oiSR next to the oiSH references.

			CharString out = allOutputs.ptr[i];
			U64 baseLen = CharString_length(out) >= 5 ? CharString_length(out) - 5 : CharString_length(out);

			if (
				!CharString_format(alloc, &relPath, &err, "hlsl/%.*s.hlsl", (int) baseLen, out.ptr) ||
				!CharString_format(alloc, &ref, &err, "%.*s.oiSR", (int) baseLen, out.ptr)
			) {
				err = Error_none();
				Test_assert(t, "oiSR reference path", false);
				goto cleanRefl;
			}

			CompilerSettings rs = (CompilerSettings) {
				.string = allShaderText.ptr[i],
				.path = relPath,
				.format = ECompilerFormat_HLSL,
				.outputType = ESHBinaryType_SPIRV,
				.includeDirs = includeDirs
			};

			if (!Compiler_reflect(&disasmComp, &rs, alloc, &reflection, &err)) {
				Log_errorLn(alloc, "reflect failed for %.*s", (int) CharString_length(relPath), relPath.ptr);
				Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);
				err = Error_none();
				Test_assert(t, ref.ptr, false);
				goto cleanRefl;
			}

			U64 wo = 0;

			if (
				!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &msType, &ws, &err) ||
				!SRFile_write(&reflection, alloc, ws, &wo, &err) ||
				!MemoryStream_move(&ws, &produced, &err)
			) {
				Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);
				err = Error_none();
				Test_assert(t, ref.ptr, false);
				goto cleanRefl;
			}

			if (File_has(&ref, alloc)) {

				Buffer_free(&golden, alloc);

				if (!File_read(&ref, 1 * SECOND, 0, 0, &fileHandleType, &golden, &err)) {
					err = Error_none();
					Test_assert(t, ref.ptr, false);
					goto cleanRefl;
				}

				Bool matches = Buffer_eq(produced, golden);

				if (!matches) {
					Log_errorLn(alloc, "oiSR mismatch vs reference %.*s", (int) CharString_length(ref), ref.ptr);
					printOiSR(alloc, produced, "produced");
					printOiSR(alloc, golden, "reference");
				}

				Test_assert(t, ref.ptr, matches);
			}

			else {
				File_write(&produced, &ref, 0, 0, 1 * SECOND, true, &fileHandleType, &err);
				err = Error_none();
				Log_warnLn(
					alloc, "Generated missing oiSR reference %.*s (review & commit)", (int) CharString_length(ref), ref.ptr
				);
				Test_assert(t, ref.ptr, false);
			}

		cleanRefl:
			CharString_free(&relPath, alloc);
			CharString_free(&ref, alloc);
			Buffer_free(&produced, alloc);
			RefPtr_dec(&ws);
			SRFile_free(&reflection, alloc);
		}
	}

	//--- ISA snapshot: for each corpus shader whose stage has an offline AMD ISA path, disassemble its SPIR-V to AMD
	//--- ISA text (via the bundled amdllpc) for two architectures and pin it byte-for-byte, like the
	//--- oiSH/oiSR snapshots.
	//--- amdllpc's ISA is deterministic and path/timestamp-free, so it's a stable reference.
	//--- amdllpc drives closer-to-final ISA than a device-independent path.
	//--- The tools are bundled next to the exe (rga/utils, copied by the CLI build); if they aren't present the whole
	//--- phase is skipped rather than failed.
	//--- The phase only exists where AMD prebuilds amdllpc (Windows/Linux x64). On every other target there is no
	//--- compiler to spawn, and an x64 binary that can't exec returns empty output rather than a not-found, which
	//--- the runtime probe below would read as a real disassembly failure.

	#ifdef SHADER_COMPILER_OFFLINE_ISA

	{
		const RefPtrType msTypeIsa = MemoryStream_makeType(alloc);

		//Two architectures the bundled amdllpc supports: RDNA3 (gfx1100) and RDNA4 (gfx1201).
		//The golden suffix is the family (gfx11 / gfx12) so a later minor bump doesn't rename every reference.

		const C8 *isaTargets[2] = { "gfx1100", "gfx1201" };
		const C8 *isaSuffix[2] = { "gfx11", "gfx12" };

		Bool isaProbed = false, isaAvailable = false;

		for (U64 i = 0; i < allBuffers.length && (!isaProbed || isaAvailable); ++i) {

			const Buffer oiSH = allBuffers.ptr[i];

			if (!Buffer_length(oiSH))
				continue;

			SHFile sh = (SHFile) { 0 };
			MemoryStreamRef *ms = NULL;
			U64 shOff = 0;

			if (
				!MemoryStream_createFromBufferRegion(
					Buffer_createRefFromBuffer(oiSH, true), 0, Buffer_length(oiSH),
					EMemoryStreamFlags_None, &msTypeIsa, &ms, &err
				) ||
				!SHFile_read((StreamRef*) ms, &shOff, false, alloc, &sh, &err)
			) {
				err = Error_none();
				RefPtr_dec(&ms);
				SHFile_free(&sh, alloc);
				continue;
			}

			//base = <output> minus the ".oiSH" suffix

			const CharString out = allOutputs.ptr[i];
			const U64 baseLen = CharString_length(out) >= 5 ? CharString_length(out) - 5 : CharString_length(out);
			const Bool multi = sh.binaries.length > 1;

			//A multi-entry / lib oiSH carries several binaries; disassemble each offline-capable (raster/compute/mesh)
			// SPIR-V one.
			//RT / no-SPIRV binaries are skipped.
			//The golden is keyed by the binary index when there's more than one, so each entrypoint's ISA is pinned
			// separately.

			for (U64 b = 0; b < sh.binaries.length && (!isaProbed || isaAvailable); ++b) {

				const Buffer spv = sh.binaries.ptr[b].binaries[ESHBinaryType_SPIRV];

				if (!Buffer_length(spv) || !SpvISA_stageHasOfflinePath(spv, alloc))
					continue;

				for (U64 tI = 0; tI < 2; ++tI) {

					Buffer isa = Buffer_createNull();
					CharString ref = CharString_createNull();
					const CharString target = CharString_createRefCStrConst(isaTargets[tI]);

					//Pass the binary's entrypoint so amdllpc lowers the RIGHT one out of a multi-entry (library) module,
					//not just the module's first entrypoint (which would make every non-vertex lib stage wrong).

					const Bool ok =
						SpvISA_disassemble(spv, target, sh.binaries.ptr[b].identifier.entrypoint, &isa, alloc, &err);

					//The first attempt doubles as the availability probe: a launch failure (tools absent) skips the
					//whole phase, while any other failure is a real regression to surface.

					if (!isaProbed) {
						isaProbed = true;
						isaAvailable = ok || err.genericError != EGenericError_NotFound;
						if (!isaAvailable)
							Log_warnLn(
								alloc,
								"ISA snapshot skipped: amdllpc not found next to the test (rga/utils not bundled)"
							);
					}

					if (!isaAvailable) {
						err = Error_none();
						Buffer_free(&isa, alloc);
						break;
					}

					if (!ok) {
						Log_errorLn(
							alloc, "ISA disassembly failed for %.*s binary %"PRIu64" @ %s",
							(int) baseLen, out.ptr, b, isaTargets[tI]
						);
						Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);
						err = Error_none();
						Test_assert(t, "ISA disassembly", false);
						Buffer_free(&isa, alloc);
						continue;
					}

					//Golden = <base>.gfxNN.isa for a single-binary oiSH, <base>.<binaryIndex>.gfxNN.isa when it has several

					const Bool made = multi ?
						CharString_format(
							alloc, &ref, &err, "%.*s.%"PRIu64".%s.isa", (int) baseLen, out.ptr, b, isaSuffix[tI]
						) :
						CharString_format(alloc, &ref, &err, "%.*s.%s.isa", (int) baseLen, out.ptr, isaSuffix[tI]);

					if (!made) {
						err = Error_none();
						Buffer_free(&isa, alloc);
						continue;
					}

					if (File_has(&ref, alloc)) {

						Buffer_free(&golden, alloc);

						if (File_read(&ref, 1 * SECOND, 0, 0, &fileHandleType, &golden, &err)) {

							const Bool matches = Buffer_eq(isa, golden);

							if (!matches)
								Log_errorLn(alloc, "ISA mismatch vs reference %.*s", (int) CharString_length(ref), ref.ptr);

							Test_assert(t, ref.ptr, matches);
						}

						else {
							err = Error_none();
							Test_assert(t, ref.ptr, false);
						}
					}

					else {
						File_write(&isa, &ref, 0, 0, 1 * SECOND, true, &fileHandleType, &err);
						err = Error_none();
						Log_warnLn(
							alloc, "Generated missing ISA reference %.*s (review & commit)",
							(int) CharString_length(ref), ref.ptr
						);
						Test_assert(t, ref.ptr, false);
					}

					Buffer_free(&isa, alloc);
					CharString_free(&ref, alloc);
				}
			}

			RefPtr_dec(&ms);
			SHFile_free(&sh, alloc);
		}
	}

	#endif

	//--- DXIL coverage: compile the same on-disk corpus for DXIL too, so it isn't SPIRV-only.
	//--- There's no byte-snapshot here (the SPIRV pass above is the byte reference; DXIL is exercised for compile +
	//--- reflection coverage) - this stays robust to benign DXIL output churn while still catching any DXIL-specific
	//--- compile or reflection regression across the whole corpus.
	//--- Every corpus shader that produced a SPIRV binary must also produce a DXIL one (none here are backend-restricted).
	//--- A single combined SPIRV+DXIL snapshot (one oiSH per shader carrying both) would be stronger, but enumerating a
	//--- folder for both modes with combineFlag fails inside Compiler_compileShaders (the SHFile_combine step) before
	//--- any per-shader compile runs, so the two backends are snapshotted separately.

	{
		ListCharString dxFiles = (ListCharString) { 0 };
		ListCharString dxText = (ListCharString) { 0 };
		ListCharString dxOutputs = (ListCharString) { 0 };
		ListU8 dxModes = (ListU8) { 0 };
		ListBuffer dxBuffers = (ListBuffer) { 0 };
		Bool dxFolder = false;
		Error dxErr = Error_none();

		U64 spvProduced = 0;
		for (U64 i = 0; i < allBuffers.length; ++i)
			if (Buffer_length(allBuffers.ptr[i]))
				++spvProduced;

		Bool dxCompiled =
			Compiler_getTargetsFromFile(
				here, ECompileType_Compile, (U64)1 << ESHBinaryType_DXIL, false, true, true,
				alloc, &dxFolder, NULL, &dxFiles, &dxText, &dxOutputs, &dxModes
			) &&
			Compiler_compileShaders(
				&dxFiles, &dxText, &dxOutputs, &dxModes, 1, false, false, (ECompilerWarning) 0, true,
				ECompileType_Compile, &includeDirs, true, alloc, &dxBuffers, &dxErr
			);

		U64 dxProduced = 0;
		for (U64 i = 0; dxCompiled && i < dxBuffers.length; ++i)
			if (Buffer_length(dxBuffers.ptr[i]))
				++dxProduced;

		if (!dxCompiled || dxErr.genericError)
			Error_print(alloc, &dxErr, ELogLevel_Error, ELogOptions_Default);

		Test_assert(t, "entire corpus compiled for DXIL", dxCompiled && !dxErr.genericError);
		Test_assert(t, "DXIL produced a binary for every SPIRV corpus shader", dxProduced == spvProduced && dxProduced >= 1);

		//Every shader that produced both a SPIRV and a DXIL oiSH must expose the same reflection interface.
		//SPIRV is byte-snapshotted above, so this ties the (unsnapshotted) DXIL reflection to it and catches any
		// backend disagreement in entrypoints, input/output signature or resources (bindings excluded).
		if (dxCompiled && allBuffers.length == dxBuffers.length)
			for (U64 i = 0; i < allBuffers.length; ++i)
				if (Buffer_length(allBuffers.ptr[i]) && Buffer_length(dxBuffers.ptr[i]))
					shCrossCheckReflection(t, alloc, allBuffers.ptr[i], dxBuffers.ptr[i]);

		ListBuffer_freeUnderlying(&dxBuffers, alloc);
		ListCharString_freeUnderlying(&dxFiles, alloc);
		ListCharString_freeUnderlying(&dxText, alloc);
		ListCharString_freeUnderlying(&dxOutputs, alloc);
		ListU8_free(&dxModes, alloc);
	}

clean:

	Test_assert(t, "corpus module produced no error", s_uccess);

	if(disasmCompCreated)
		Compiler_free(&disasmComp, alloc);

	Buffer_free(&golden, alloc);
	CharString_free(&refPath, alloc);
	ListBuffer_freeUnderlying(&allBuffers, alloc);
	ListCharString_freeUnderlying(&allFiles, alloc);
	ListCharString_freeUnderlying(&allShaderText, alloc);
	ListCharString_freeUnderlying(&allOutputs, alloc);
	ListU8_free(&allCompileModes, alloc);
	ListCharString_free(&includeDirs, alloc);

	Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);
}
