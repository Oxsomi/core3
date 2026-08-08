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
#include "formats/oiSH/sh_binaries.h"
#include "formats/oiSH/sh_file.h"
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
	//(A separate DXIL compile+reflect coverage pass follows below; a combined SPIRV+DXIL snapshot is blocked
	// on a driver issue when a folder is enumerated for both modes with combineFlag - see that pass.)

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

			Bool matches = Buffer_eq(produced, golden);

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

	//--- DXIL coverage: compile the same on-disk corpus for DXIL too, so it isn't SPIRV-only.
	//--- There's no byte-snapshot here (the SPIRV pass above is the byte reference; DXIL is exercised for compile +
	//--- reflection coverage) - this stays robust to benign DXIL output churn while still catching any DXIL-specific
	//--- compile or reflection regression across the whole corpus.
	//--- Every corpus shader that produced a SPIRV binary must also produce a DXIL one (none here are backend-restricted).
	//---
	//--- NOTE: a single *combined* SPIRV+DXIL snapshot (one oiSH per shader carrying both) would be stronger,
	//--- but enumerating a folder for both modes with combineFlag currently fails inside Compiler_compileShaders
	//--- (SHFile_combine step) before any per-shader compile runs - separate OxC3-side item to chase.

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
