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

//tools/oxc3_wasm/wasm_main.c
//
//The boundary the web frontend calls (web/js/wasm.js on the other side).
//Everything here is the same call the CLI makes for the same command, so the page is a second front end onto
// the library rather than a reimplementation of it; what has no library call behind it is absent from this file
// and the page keeps its stub, see web/README.md for which.
//
//Project files live in the module's own filesystem, written by the page before a call, so #include resolution
// runs through the ordinary File_read path against the working directory and nothing here has to bridge it.

#include "tools/oxc3_wasm/wasm_bridge.h"
#include "shader_compiler/compiler.h"
#include "shader_compiler/spirv_isa.h"
#include "platforms/file.h"
#include "platforms/platform.h"
#include "formats/oiSH/sh_file.h"
#include "formats/oiCA/ca_file.h"
#include "formats/oiCA/ca_edit.h"
#include "formats/oiCA/ca_lookup.h"
#include "formats/oiCA/ca_props.h"
#include "formats/oiSH/sh_binaries.h"
#include "formats/oiSH/sh_headers.h"
#include "formats/oiSR/sr_file.h"
#include "formats/oiSP/sp_file.h"
#include "types/container/texture_format.h"
#include "types/container/list_basic_types.h"
#include "types/container/memory_stream.h"
#include "types/container/string_helper.h"
#include "types/container/ref_ptr.h"
#include "types/container/log.h"
#include "types/base/string_read.h"
#include "types/base/string_read_helper.h"
#include <emscripten/emscripten.h>
#include <inttypes.h>

//Compile switches, matching the CLI flags the toolbar offers one for one.

typedef enum EWasmCompileFlag {
	EWasmCompileFlag_Debug                = 1 << 0,        //--debug
	EWasmCompileFlag_KeepRegisters        = 1 << 1,        //--keep-registers
	EWasmCompileFlag_WarnUnusedRegisters  = 1 << 2,        //--warn-unused-registers
	EWasmCompileFlag_WarnUnusedConstants  = 1 << 3,        //--warn-unused-constants
	EWasmCompileFlag_WarnBufferPadding    = 1 << 4,        //--warn-buffer-padding
	EWasmCompileFlag_IgnoreEmptyFiles     = 1 << 5,        //--ignore-empty-files
	EWasmCompileFlag_ReflectionOnly       = 1 << 6,        //shader reflect: compile, then strip the binaries
	EWasmCompileFlag_NoOpt                = 1 << 7         //--no-opt: -Od, so debug line info survives per statement
} EWasmCompileFlag;

//One Compiler for the module.
//A Compiler is per thread and this build has one, so the page's calls all run against this one rather than
// paying DXC's setup on every disassemble.
//Compiler_compileShaders is the exception: it builds its own pool, since it decides the fan out itself.

static Compiler wasmCompiler = (Compiler) { 0 };
static Bool wasmHasCompiler = false;

static const Allocator *Wasm_allocator() {
	return Platform_instance ? Platform_instance->alloc : NULL;
}

//A string the page passed as NUL terminated C, as a non owning CharString.

static CharString Wasm_string(const C8 *str) {
	return str ? CharString_createRefCStrConst(str) : CharString_createNull();
}

static Buffer Wasm_input(const U8 *ptr, U32 length) {
	return ptr && length ? Buffer_createRefConst(ptr, length) : Buffer_createNull();
}

//Reads a whole file format out of a buffer the page owns.
//A ref keeps the stream non owning, so the page's buffer outlives the read and is freed by the page.

#define WASM_READ_FILE(name, type, readFn)                                                              \
	static Bool name(Buffer buf, type *out, const Allocator *alloc, Error *e_rr) {                      \
		Bool s_uccess = true;                                                                           \
		const RefPtrType streamType = MemoryStream_makeType(alloc);                                     \
		MemoryStreamRef *stream = NULL;                                                                 \
		U64 offset = 0;                                                                                 \
		Buffer ref = Buffer_createRefFromBuffer(buf, true);                                             \
		gotoIfError3(clean, MemoryStream_createFromBufferRegion(                                        \
			ref, 0, Buffer_length(ref), EMemoryStreamFlags_None, &streamType, &stream, e_rr             \
		));                                                                                             \
		gotoIfError3(clean, readFn((StreamRef*) stream, &offset, false, alloc, out, e_rr));             \
	clean:                                                                                              \
		RefPtr_dec(&stream);                                                                            \
		return s_uccess;                                                                                \
	}

WASM_READ_FILE(Wasm_readSH, SHFile, SHFile_read);
WASM_READ_FILE(Wasm_readSR, SRFile, SRFile_read);
WASM_READ_FILE(Wasm_readSP, SPFile, SPFile_read);

//Serializes through a resizable memory stream and moves its buffer out, which the caller then owns.

static Bool Wasm_writeSH(const SHFile *file, Buffer *out, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;
	const RefPtrType streamType = MemoryStream_makeType(alloc);
	MemoryStreamRef *stream = NULL;
	U64 offset = 0;

	gotoIfError3(clean, MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &streamType, &stream, e_rr));
	gotoIfError3(clean, SHFile_write((StreamRef*) stream, &offset, file, alloc, e_rr));
	gotoIfError3(clean, MemoryStream_move(&stream, out, e_rr));

clean:
	RefPtr_dec(&stream);
	return s_uccess;
}

static Bool Wasm_writeSR(const SRFile *file, Buffer *out, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;
	const RefPtrType streamType = MemoryStream_makeType(alloc);
	MemoryStreamRef *stream = NULL;
	U64 offset = 0;

	gotoIfError3(clean, MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &streamType, &stream, e_rr));
	gotoIfError3(clean, SRFile_write(file, alloc, (StreamRef*) stream, &offset, e_rr));
	gotoIfError3(clean, MemoryStream_move(&stream, out, e_rr));

clean:
	RefPtr_dec(&stream);
	return s_uccess;
}

static Bool Wasm_writeSP(SPFile *file, Buffer *out, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;
	const RefPtrType streamType = MemoryStream_makeType(alloc);
	MemoryStreamRef *stream = NULL;
	U64 offset = 0;

	//The hash covers the pipelines, so it is refreshed before the write rather than by the reader.

	gotoIfError3(clean, SPFile_finalize(file, alloc, e_rr));
	gotoIfError3(clean, MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &streamType, &stream, e_rr));
	gotoIfError3(clean, SPFile_write(file, alloc, (StreamRef*) stream, &offset, e_rr));
	gotoIfError3(clean, MemoryStream_move(&stream, out, e_rr));

clean:
	RefPtr_dec(&stream);
	return s_uccess;
}

EMSCRIPTEN_KEEPALIVE void *oxc3_alloc(U32 size) {
	return Wasm_alloc(size);
}

EMSCRIPTEN_KEEPALIVE void oxc3_free(void *ptr) {
	Wasm_free(ptr);
}

//Brings the platform and the compiler up and reports what this build can actually do.
//Called once, before anything else; calling it again is reported rather than creating a second platform.

EMSCRIPTEN_KEEPALIVE void *oxc3_init() {

	Error err = Error_none(), *e_rr = &err;
	Bool s_uccess = true;
	CharString json = CharString_createNull();
	void *frame = NULL;

	if(Platform_instance)
		return Wasm_errorFrame("oxc3_init() the module is already initialized");

	if(!Platform_create(0, NULL, NULL, NULL, true, e_rr))
		return Wasm_errorFrameFromError(&err, "oxc3_init() couldn't create the platform");

	Compiler_setPlatform(Platform_instance);

	const Allocator *alloc = Wasm_allocator();

	gotoIfError3(clean, Compiler_create(alloc, &wasmCompiler, e_rr));
	wasmHasCompiler = true;

	//What this build can run, which the page would otherwise have to guess.
	//Which host it is on is the page's own business, so it isn't reported here; what is reported is what
	//works there.
	//The offline ISA route drives bundled tools as child processes, which a sandbox has no way to do, so the
	// module reports that rather than letting the page discover it as a failed run.

	Bool offlineIsa = false;

	#ifdef SUPPORTS_PROCESS
		offlineIsa = true;
	#endif

	gotoIfError3(clean, Json_fmt(
		&json, alloc, e_rr,
		"{\"version\":\"%u.%u.%u\",\"capabilities\":{\"liveIsa\":false,\"offlineIsa\":%s,\"threads\":%"PRIu64"}}",
		OXC3_MAJOR, OXC3_MINOR, OXC3_PATCH, offlineIsa ? "true" : "false", Platform_getThreads()
	));

	frame = Wasm_frame(&json, NULL);

clean:

	CharString_free(&json, alloc);

	if(!s_uccess) {

		if(wasmHasCompiler) {
			Compiler_free(&wasmCompiler, alloc);
			wasmHasCompiler = false;
		}

		Compiler_shutdown();
		Platform_cleanup();
		return Wasm_errorFrameFromError(&err, "oxc3_init() couldn't create the compiler");
	}

	return frame;
}

EMSCRIPTEN_KEEPALIVE void oxc3_shutdown() {

	if(!Platform_instance)
		return;

	if (wasmHasCompiler) {
		Compiler_free(&wasmCompiler, Platform_instance->alloc);
		wasmHasCompiler = false;
	}

	Compiler_shutdown();
	Platform_cleanup();
}

//The includes a shader reaches with an @ prefix, compiled into the module rather than read from disk.
//Serving them is what lets the page show them as read only files without a second copy of their text.

EMSCRIPTEN_KEEPALIVE void *oxc3_builtinIncludes() {

	const Allocator *alloc = Wasm_allocator();

	if(!alloc)
		return NULL;

	Error err = Error_none(), *e_rr = &err;
	Bool s_uccess = true;
	CharString json = CharString_createNull();
	void *frame = NULL;

	gotoIfError3(clean, Json_raw(&json, "{\"includes\":[", alloc, e_rr));

	U64 count = Compiler_builtInIncludeCount();

	for (U64 i = 0; i < count; ++i) {

		const CompilerBuiltInInclude *include = Compiler_builtInIncludeAt(i);

		if(!include)
			continue;

		gotoIfError3(clean, Json_raw(&json, i ? ",{\"name\":" : "{\"name\":", alloc, e_rr));
		gotoIfError3(clean, Json_cstr(&json, include->name, alloc, e_rr));
		gotoIfError3(clean, Json_raw(&json, ",\"src\":", alloc, e_rr));
		gotoIfError3(clean, Json_cstr(&json, include->source, alloc, e_rr));
		gotoIfError3(clean, Json_raw(&json, "}", alloc, e_rr));
	}

	gotoIfError3(clean, Json_raw(&json, "]}", alloc, e_rr));
	frame = Wasm_frame(&json, NULL);

clean:
	CharString_free(&json, alloc);
	return s_uccess ? frame : Wasm_errorFrameFromError(&err, "oxc3_builtinIncludes() failed");
}

//OxC3 shader compile, in memory.
//`name` is the shader's path inside the module's filesystem, which is what its #includes resolve relative to,
// and `source` its text; the page has already written the project tree there.
//One compile is queued per requested backend under a single output name, which is what makes the results
// combine into one oiSH the way the CLI does without --split.

EMSCRIPTEN_KEEPALIVE void *oxc3_compileShaders(const C8 *name, const C8 *source, U32 targetMask, U32 flags) {

	const Allocator *alloc = Wasm_allocator();

	if(!alloc)
		return NULL;

	Error err = Error_none(), *e_rr = &err;
	Bool s_uccess = true;

	ListCharString allFiles = (ListCharString) { 0 };
	ListCharString allShaderText = (ListCharString) { 0 };
	ListCharString allOutputs = (ListCharString) { 0 };
	ListU8 allCompileModes = (ListU8) { 0 };
	ListCharString includeDirs = (ListCharString) { 0 };
	ListBuffer outputs = (ListBuffer) { 0 };

	SHFile file = (SHFile) { 0 };
	Buffer blob = Buffer_createNull();
	CharString json = CharString_createNull();
	void *frame = NULL;

	CharString nameStr = Wasm_string(name);
	CharString sourceStr = Wasm_string(source);

	if(!CharString_length(nameStr))
		retError(clean, Error_invalidParameter(0, 0, "oxc3_compileShaders()::name is required"));

	if(!(targetMask & ((1 << ESHBinaryType_Count) - 1)))
		retError(clean, Error_invalidParameter(2, 0, "oxc3_compileShaders()::targetMask names no backend"));

	//The output name is shared by every backend on purpose: Compiler_compileShaders combines the results that
	// share one, which is the single bulky oiSH the page shows as one document.

	CharString output = CharString_createRefCStrConst("out.oiSH");

	for (U8 i = 0; i < ESHBinaryType_Count; ++i) {

		if(!((targetMask >> i) & 1))
			continue;

		gotoIfError3(clean, ListCharString_pushBack(&allFiles, nameStr, alloc, e_rr));
		gotoIfError3(clean, ListCharString_pushBack(&allShaderText, sourceStr, alloc, e_rr));
		gotoIfError3(clean, ListCharString_pushBack(&allOutputs, output, alloc, e_rr));
		gotoIfError3(clean, ListU8_pushBack(&allCompileModes, i, alloc, e_rr));
	}

	ECompilerWarning extraWarnings = (ECompilerWarning) (
		((flags & EWasmCompileFlag_WarnUnusedRegisters) ? ECompilerWarning_UnusedRegisters : 0) |
		((flags & EWasmCompileFlag_WarnUnusedConstants) ? ECompilerWarning_UnusedConstants : 0) |
		((flags & EWasmCompileFlag_WarnBufferPadding) ? ECompilerWarning_BufferPadding : 0)
	);

	//Single threaded on purpose rather than by omission: JobQueue runs inline without -pthread, and the page
	// runs the module on one worker either way.

	gotoIfError3(clean, Compiler_compileShaders(
		&allFiles, &allShaderText, &allOutputs, &allCompileModes,
		1,
		(flags & EWasmCompileFlag_Debug) != 0,
		(flags & EWasmCompileFlag_NoOpt) != 0,
		(flags & EWasmCompileFlag_KeepRegisters) != 0,
		extraWarnings,
		(flags & EWasmCompileFlag_IgnoreEmptyFiles) != 0,
		ECompileType_Compile,
		&includeDirs,
		true,
		alloc,
		&outputs,
		e_rr
	));

	//Every queued backend writes into the same output, so the combined file is whichever slot got it.

	for(U64 i = 0; i < outputs.length && !Buffer_length(blob); ++i)
		if (Buffer_length(outputs.ptr[i])) {
			blob = outputs.ptrNonConst[i];
			outputs.ptrNonConst[i] = Buffer_createNull();
		}

	if(!Buffer_length(blob))
		retError(clean, Error_invalidState(0, "oxc3_compileShaders() produced no oiSH"));

	gotoIfError3(clean, Wasm_readSH(blob, &file, alloc, e_rr));

	//shader reflect: the same compile, with the binaries dropped and the file marked reflection only, which is
	// what the CLI rewrites in place after compiling.

	if (flags & EWasmCompileFlag_ReflectionOnly) {

		file.flags |= ESHSettingsFlags_ReflectionOnly;

		for(U64 i = 0; i < file.binaries.length; ++i)
			for(U8 j = 0; j < ESHBinaryType_Count; ++j)
				Buffer_free(&file.binaries.ptrNonConst[i].binaries[j], alloc);

		Buffer_free(&blob, alloc);
		gotoIfError3(clean, Wasm_writeSH(&file, &blob, alloc, e_rr));
	}

	gotoIfError3(clean, WasmJson_shFile(&file, nameStr, nameStr, &json, alloc, e_rr));
	frame = Wasm_frame(&json, &blob);

clean:

	SHFile_free(&file, alloc);
	CharString_free(&json, alloc);
	Buffer_free(&blob, alloc);
	ListBuffer_freeUnderlying(&outputs, alloc);

	//The three input lists hold refs into what the page passed, so only the lists themselves are freed.

	ListCharString_free(&allFiles, alloc);
	ListCharString_free(&allShaderText, alloc);
	ListCharString_free(&allOutputs, alloc);
	ListCharString_free(&includeDirs, alloc);
	ListU8_free(&allCompileModes, alloc);

	return s_uccess ? frame : Wasm_errorFrameFromError(&err, "oxc3_compileShaders() failed");
}

EMSCRIPTEN_KEEPALIVE void *oxc3_shRead(const U8 *ptr, U32 length, const C8 *name) {

	const Allocator *alloc = Wasm_allocator();

	if(!alloc)
		return NULL;

	Error err = Error_none(), *e_rr = &err;
	Bool s_uccess = true;

	SHFile file = (SHFile) { 0 };
	CharString json = CharString_createNull();
	void *frame = NULL;

	CharString nameStr = Wasm_string(name);

	gotoIfError3(clean, Wasm_readSH(Wasm_input(ptr, length), &file, alloc, e_rr));
	gotoIfError3(clean, WasmJson_shFile(&file, nameStr, nameStr, &json, alloc, e_rr));
	frame = Wasm_frame(&json, NULL);

clean:
	SHFile_free(&file, alloc);
	CharString_free(&json, alloc);
	return s_uccess ? frame : Wasm_errorFrameFromError(&err, "oxc3_shRead() failed");
}

//OxC3 file combine -format oiSH.
//Two files compiled from the same source, include set and compiler settings become one; the requirement is
// SHFile_combine's, so a mismatch is reported by it rather than pre-checked here.

EMSCRIPTEN_KEEPALIVE void *oxc3_shCombine(
	const U8 *aPtr, U32 aLength, const U8 *bPtr, U32 bLength, const C8 *name
) {

	const Allocator *alloc = Wasm_allocator();

	if(!alloc)
		return NULL;

	Error err = Error_none(), *e_rr = &err;
	Bool s_uccess = true;

	SHFile a = (SHFile) { 0 }, b = (SHFile) { 0 }, combined = (SHFile) { 0 };
	Buffer blob = Buffer_createNull();
	CharString json = CharString_createNull();
	void *frame = NULL;

	CharString nameStr = Wasm_string(name);

	gotoIfError3(clean, Wasm_readSH(Wasm_input(aPtr, aLength), &a, alloc, e_rr));
	gotoIfError3(clean, Wasm_readSH(Wasm_input(bPtr, bLength), &b, alloc, e_rr));
	gotoIfError3(clean, SHFile_combine(&a, &b, alloc, &combined, e_rr));
	gotoIfError3(clean, Wasm_writeSH(&combined, &blob, alloc, e_rr));
	gotoIfError3(clean, WasmJson_shFile(&combined, nameStr, nameStr, &json, alloc, e_rr));
	frame = Wasm_frame(&json, &blob);

clean:
	SHFile_free(&combined, alloc);
	SHFile_free(&b, alloc);
	SHFile_free(&a, alloc);
	CharString_free(&json, alloc);
	Buffer_free(&blob, alloc);
	return s_uccess ? frame : Wasm_errorFrameFromError(&err, "oxc3_shCombine() failed");
}

//file data --bin -entry N -compile-output <spv|dxil>: the stored binary, copied out of the file.

EMSCRIPTEN_KEEPALIVE void *oxc3_shExtractBinary(const U8 *ptr, U32 length, U32 binaryId, U32 binaryType) {

	const Allocator *alloc = Wasm_allocator();

	if(!alloc)
		return NULL;

	Error err = Error_none(), *e_rr = &err;
	Bool s_uccess = true;

	SHFile file = (SHFile) { 0 };
	Buffer blob = Buffer_createNull();
	CharString json = CharString_createNull();
	void *frame = NULL;

	gotoIfError3(clean, Wasm_readSH(Wasm_input(ptr, length), &file, alloc, e_rr));

	if(binaryId >= file.binaries.length)
		retError(clean, Error_outOfBounds(
			2, binaryId, file.binaries.length, "oxc3_shExtractBinary()::binaryId out of bounds"
		));

	if(binaryType >= ESHBinaryType_Count)
		retError(clean, Error_invalidParameter(3, 0, "oxc3_shExtractBinary()::binaryType is spirv or dxil"));

	Buffer stored = file.binaries.ptr[binaryId].binaries[binaryType];

	if(!Buffer_length(stored))
		retError(clean, Error_notFound(0, 0, "oxc3_shExtractBinary() this binary holds no code for that backend"));

	gotoIfError3(clean, Buffer_createCopy(stored, alloc, &blob, e_rr));
	gotoIfError3(clean, Json_fmt(&json, alloc, e_rr, "{\"length\":%"PRIu64"}", Buffer_length(blob)));
	frame = Wasm_frame(&json, &blob);

clean:
	SHFile_free(&file, alloc);
	CharString_free(&json, alloc);
	Buffer_free(&blob, alloc);
	return s_uccess ? frame : Wasm_errorFrameFromError(&err, "oxc3_shExtractBinary() failed");
}

//OxC3 file header: sniff the magic and read the header that sits right after it.
//The three formats are told apart the way the CLI tells them apart, by their magic rather than by a name.

EMSCRIPTEN_KEEPALIVE void *oxc3_fileHeader(const U8 *ptr, U32 length) {

	const Allocator *alloc = Wasm_allocator();

	if(!alloc)
		return NULL;

	Error err = Error_none(), *e_rr = &err;
	Bool s_uccess = true;

	SHFile shFile = (SHFile) { 0 };
	SRFile srFile = (SRFile) { 0 };
	SPFile spFile = (SPFile) { 0 };
	CharString json = CharString_createNull();
	void *frame = NULL;

	Buffer input = Wasm_input(ptr, length);

	if(Buffer_length(input) < 4)
		retError(clean, Error_outOfBounds(1, 4, Buffer_length(input), "oxc3_fileHeader() needs at least a magic"));

	U32 magic = 0;
	Buffer_memcpy(Buffer_createRef(&magic, sizeof(magic)), Buffer_createRefConst(input.ptr, sizeof(magic)));

	if (magic == SRHeader_MAGIC) {
		gotoIfError3(clean, Wasm_readSR(input, &srFile, alloc, e_rr));
		gotoIfError3(clean, Json_raw(&json, "{\"format\":\"oiSR\",\"document\":", alloc, e_rr));
		gotoIfError3(clean, WasmJson_srFile(&srFile, CharString_createNull(), CharString_createNull(), &json, alloc, e_rr));
		gotoIfError3(clean, Json_raw(&json, "}", alloc, e_rr));
	}

	else if (magic == SPHeader_MAGIC) {
		gotoIfError3(clean, Wasm_readSP(input, &spFile, alloc, e_rr));
		gotoIfError3(clean, Json_raw(&json, "{\"format\":\"oiSP\",\"document\":", alloc, e_rr));
		gotoIfError3(clean, WasmJson_spFile(&spFile, CharString_createNull(), CharString_createNull(), &json, alloc, e_rr));
		gotoIfError3(clean, Json_raw(&json, "}", alloc, e_rr));
	}

	else if (magic == SHHeader_MAGIC) {
		gotoIfError3(clean, Wasm_readSH(input, &shFile, alloc, e_rr));
		gotoIfError3(clean, Json_raw(&json, "{\"format\":\"oiSH\",\"document\":", alloc, e_rr));
		gotoIfError3(clean, WasmJson_shFile(&shFile, CharString_createNull(), CharString_createNull(), &json, alloc, e_rr));
		gotoIfError3(clean, Json_raw(&json, "}", alloc, e_rr));
	}

	else retError(clean, Error_invalidParameter(0, 0, "oxc3_fileHeader() the magic is not oiSH, oiSR or oiSP"));

	frame = Wasm_frame(&json, NULL);

clean:
	SPFile_free(&spFile, alloc);
	SRFile_free(&srFile, alloc);
	SHFile_free(&shFile, alloc);
	CharString_free(&json, alloc);
	return s_uccess ? frame : Wasm_errorFrameFromError(&err, "oxc3_fileHeader() failed");
}

//OxC3 shader disassemble: spirv-tools for SPIR-V, DXC for DXIL.

EMSCRIPTEN_KEEPALIVE void *oxc3_disassemble(U32 binaryType, const U8 *ptr, U32 length) {

	const Allocator *alloc = Wasm_allocator();

	if(!alloc || !wasmHasCompiler)
		return Wasm_errorFrame("oxc3_disassemble() the module isn't initialized");

	Error err = Error_none(), *e_rr = &err;
	Bool s_uccess = true;

	CharString text = CharString_createNull();
	CharString json = CharString_createNull();
	void *frame = NULL;

	if(binaryType >= ESHBinaryType_Count)
		retError(clean, Error_invalidParameter(0, 0, "oxc3_disassemble()::binaryType is spirv or dxil"));

	gotoIfError3(clean, Compiler_disassemble(
		&wasmCompiler, (ESHBinaryType) binaryType, Wasm_input(ptr, length), alloc, &text, e_rr
	));

	gotoIfError3(clean, Json_raw(&json, "{\"text\":", alloc, e_rr));
	gotoIfError3(clean, Json_str(&json, text, alloc, e_rr));
	gotoIfError3(clean, Json_raw(&json, "}", alloc, e_rr));
	frame = Wasm_frame(&json, NULL);

clean:
	CharString_free(&text, alloc);
	CharString_free(&json, alloc);
	return s_uccess ? frame : Wasm_errorFrameFromError(&err, "oxc3_disassemble() failed");
}

//OxC3 shader assemble: spirv-as for SPIR-V text, DXC's assembler for DXIL LL text; either result is judged by the
//validator before it is handed back.

EMSCRIPTEN_KEEPALIVE void *oxc3_assemble(U32 binaryType, const C8 *text) {

	const Allocator *alloc = Wasm_allocator();

	if(!alloc || !wasmHasCompiler)
		return Wasm_errorFrame("oxc3_assemble() the module isn't initialized");

	Error err = Error_none(), *e_rr = &err;
	Bool s_uccess = true;

	Buffer blob = Buffer_createNull();
	CharString json = CharString_createNull();
	void *frame = NULL;

	if(binaryType >= ESHBinaryType_Count)
		retError(clean, Error_invalidParameter(0, 0, "oxc3_assemble()::binaryType is spirv or dxil"));

	gotoIfError3(clean, Compiler_assemble(
		&wasmCompiler, (ESHBinaryType) binaryType, Wasm_string(text), alloc, &blob, e_rr
	));

	gotoIfError3(clean, Json_fmt(&json, alloc, e_rr, "{\"length\":%"PRIu64"}", Buffer_length(blob)));
	frame = Wasm_frame(&json, &blob);

clean:
	CharString_free(&json, alloc);
	Buffer_free(&blob, alloc);
	return s_uccess ? frame : Wasm_errorFrameFromError(&err, "oxc3_assemble() failed");
}

//Compiler_getUniqueEntrypoints: the entrypoints a library binary holds, name and stage each.

EMSCRIPTEN_KEEPALIVE void *oxc3_uniqueEntrypoints(U32 binaryType, const U8 *ptr, U32 length, U32 showAll) {

	const Allocator *alloc = Wasm_allocator();

	if(!alloc || !wasmHasCompiler)
		return Wasm_errorFrame("oxc3_uniqueEntrypoints() the module isn't initialized");

	Error err = Error_none(), *e_rr = &err;
	Bool s_uccess = true;

	ListCompilerEntrypoint entrypoints = (ListCompilerEntrypoint) { 0 };
	CharString json = CharString_createNull();
	void *frame = NULL;

	if(binaryType >= ESHBinaryType_Count)
		retError(clean, Error_invalidParameter(0, 0, "oxc3_uniqueEntrypoints()::binaryType is spirv or dxil"));

	gotoIfError3(clean, Compiler_getUniqueEntrypoints(
		&wasmCompiler, (ESHBinaryType) binaryType, Wasm_input(ptr, length), showAll != 0, &entrypoints, alloc, e_rr
	));

	gotoIfError3(clean, Json_raw(&json, "{\"entrypoints\":[", alloc, e_rr));

	for (U64 i = 0; i < entrypoints.length; ++i) {

		gotoIfError3(clean, Json_raw(&json, i ? ",{\"name\":" : "{\"name\":", alloc, e_rr));
		gotoIfError3(clean, Json_str(&json, entrypoints.ptr[i].name, alloc, e_rr));
		gotoIfError3(clean, Json_raw(&json, ",\"stage\":", alloc, e_rr));
		gotoIfError3(clean, Json_cstr(&json, entrypoints.ptr[i].stage < ESHPipelineStage_Count ?
			SHEntry_stageNames[entrypoints.ptr[i].stage] : "unknown", alloc, e_rr
		));
		gotoIfError3(clean, Json_raw(&json, "}", alloc, e_rr));
	}

	gotoIfError3(clean, Json_raw(&json, "]}", alloc, e_rr));
	frame = Wasm_frame(&json, NULL);

clean:

	for(U64 i = 0; i < entrypoints.length; ++i)
		CharString_free(&entrypoints.ptrNonConst[i].name, alloc);

	ListCompilerEntrypoint_free(&entrypoints, alloc);
	CharString_free(&json, alloc);
	return s_uccess ? frame : Wasm_errorFrameFromError(&err, "oxc3_uniqueEntrypoints() failed");
}

//OxC3 shader reflect-symbols: the frontend symbol AST of the source, as an oiSR.
//This runs on source rather than on a compile, so it follows the editor instead of the compile button.

//disabledExt masks extensions out of the parse (0 = the everything-enabled default) and defines carries
//the followed binary's uniforms as newline separated NAME=VALUE lines; together they are the page's
//"IntelliSense follows this binary" picker.

EMSCRIPTEN_KEEPALIVE void *oxc3_reflectSymbols(
	const C8 *name, const C8 *source, U32 allowErrors, U32 disabledExt, const C8 *defines
) {

	const Allocator *alloc = Wasm_allocator();

	if(!alloc || !wasmHasCompiler)
		return Wasm_errorFrame("oxc3_reflectSymbols() the module isn't initialized");

	Error err = Error_none(), *e_rr = &err;
	Bool s_uccess = true;

	SRFile reflection = (SRFile) { 0 };
	Buffer blob = Buffer_createNull();
	CharString json = CharString_createNull();
	ListCharString definePairs = (ListCharString) { 0 };
	void *frame = NULL;

	//Name,value pairs out of "NAME=VALUE" lines; refs into the caller's buffer, valid for this call,
	//so the list frees shallow.

	const CharString definesStr = Wasm_string(defines);
	U64 off = 0;

	while (off < CharString_length(definesStr)) {

		U64 end = CharString_findFirstSensitive(&definesStr, '\n', off, 0);
		if (end == U64_MAX) end = CharString_length(definesStr);

		if (end > off) {

			const CharString line = CharString_createRefSizedConst(definesStr.ptr + off, end - off, false);
			CharString nameRef = CharString_createNull(), valueRef = CharString_createNull();

			if (!CharString_cutAfterFirstSensitive(&line, '=', &nameRef)) nameRef = line;
			else CharString_cutBeforeFirstSensitive(&line, '=', &valueRef);

			gotoIfError3(clean, ListCharString_pushBack(&definePairs, nameRef, alloc, e_rr));
			gotoIfError3(clean, ListCharString_pushBack(&definePairs, valueRef, alloc, e_rr));
		}

		off = end + 1;
	}

	//The module's own Compiler, not one per call: a second Compiler alongside it faults inside the
	//reflector on its first use.

	CharString nameStr = Wasm_string(name);

	//The page reflects on every edit, so it asks for what parsed rather than for nothing: an outline that
	//vanishes on a half-typed line is worse than one that is briefly incomplete.

	CompilerSettings settings = (CompilerSettings) {
		.string = Wasm_string(source),
		.path = nameStr,
		.format = ECompilerFormat_HLSL,
		.outputType = ESHBinaryType_SPIRV,
		.reflectAllowErrors = allowErrors != 0,
		.reflectDisabledExt = (ESHExtension) disabledExt,
		.reflectDefines = definePairs
	};

	gotoIfError3(clean, Compiler_reflect(&wasmCompiler, &settings, alloc, &reflection, e_rr));
	gotoIfError3(clean, Wasm_writeSR(&reflection, &blob, alloc, e_rr));
	gotoIfError3(clean, WasmJson_srFile(&reflection, nameStr, nameStr, &json, alloc, e_rr));
	frame = Wasm_frame(&json, &blob);

clean:
	SRFile_free(&reflection, alloc);
	ListCharString_free(&definePairs, alloc);      //shallow: the strings are refs into the caller's buffer
	CharString_free(&json, alloc);
	Buffer_free(&blob, alloc);
	return s_uccess ? frame : Wasm_errorFrameFromError(&err, "oxc3_reflectSymbols() failed");
}

EMSCRIPTEN_KEEPALIVE void *oxc3_srRead(const U8 *ptr, U32 length, const C8 *name) {

	const Allocator *alloc = Wasm_allocator();

	if(!alloc)
		return NULL;

	Error err = Error_none(), *e_rr = &err;
	Bool s_uccess = true;

	SRFile file = (SRFile) { 0 };
	CharString json = CharString_createNull();
	void *frame = NULL;

	CharString nameStr = Wasm_string(name);

	gotoIfError3(clean, Wasm_readSR(Wasm_input(ptr, length), &file, alloc, e_rr));
	gotoIfError3(clean, WasmJson_srFile(&file, nameStr, nameStr, &json, alloc, e_rr));
	frame = Wasm_frame(&json, NULL);

clean:
	SRFile_free(&file, alloc);
	CharString_free(&json, alloc);
	return s_uccess ? frame : Wasm_errorFrameFromError(&err, "oxc3_srRead() failed");
}

//Which entries of an oiSH form one pipeline, chosen the way the CLI chooses them: compute over graphics over
// ray tracing, every stage of the chosen kind bound, and a refusal rather than a guess when one stage kind
// occurs twice.
//`picks` overrides that with an explicit comma separated entry list, which is what -entry does.

static Bool Wasm_selectPipelineStages(
	const SHFile *file,
	CharString picks,
	SPStageRef *stages,
	U8 *stageCount,
	CharString *refusal,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;
	ListCharString split = (ListCharString) { 0 };

	*stageCount = 0;

	//An explicit pick is taken as given; SPFile_derivePipeline still refuses a set that can't be one pipeline.

	if (CharString_length(picks)) {

		CharStringSplit splitter = (CharStringSplit) { .s = &picks, .allocator = alloc, .result = &split };
		gotoIfError3(clean, CharString_splitSensitive(&splitter, ',', e_rr));

		for (U64 i = 0; i < split.length && *stageCount < 16; ++i) {

			U64 entryId = 0;

			if(!CharString_parseU64(split.ptr[i], &entryId) || entryId >= file->entries.length)
				retError(clean, Error_invalidParameter(1, 0, "Wasm_selectPipelineStages()::picks names no entry"));

			stages[(*stageCount)++] = (SPStageRef) { .fileId = 0, .entryId = (U16) entryId };
		}

		goto clean;
	}

	U64 kindCounts[3] = { 0 };

	for (U64 i = 0; i < file->entries.length; ++i) {

		U8 stage = file->entries.ptr[i].stage;

		if(stage == ESHPipelineStage_Compute)
			++kindCounts[0];

		else if(
			stage == ESHPipelineStage_Vertex || stage == ESHPipelineStage_Pixel || stage == ESHPipelineStage_Hull ||
			stage == ESHPipelineStage_Domain || stage == ESHPipelineStage_GeometryExt ||
			stage == ESHPipelineStage_MeshExt || stage == ESHPipelineStage_TaskExt
		)
			++kindCounts[1];

		else if(stage >= ESHPipelineStage_RtStartExt && stage <= ESHPipelineStage_RtEndExt)
			++kindCounts[2];
	}

	U8 chosenKind = kindCounts[0] ? 0 : kindCounts[1] ? 1 : kindCounts[2] ? 2 : 3;

	if (chosenKind == 3) {
		
		gotoIfError3(clean, CharString_createCopy(CharString_createRefCStrConst(
			"This file has no compute, graphics or ray tracing stage, so there is no pipeline to derive."
		), alloc, refusal, e_rr));

		goto clean;
	}

	//One slot per graphics stage kind, so a second entry of the same kind is a refusal rather than a silent pick.

	U64 slots[8] = { U64_MAX, U64_MAX, U64_MAX, U64_MAX, U64_MAX, U64_MAX, U64_MAX, U64_MAX };

	for (U64 i = 0; i < file->entries.length; ++i) {

		U8 stage = file->entries.ptr[i].stage;
		U64 *slot = NULL;

		switch (stage) {
			case ESHPipelineStage_Compute:      if(!chosenKind)      slot = &slots[0];  break;
			case ESHPipelineStage_Vertex:       if(chosenKind == 1)  slot = &slots[1];  break;
			case ESHPipelineStage_Pixel:        if(chosenKind == 1)  slot = &slots[2];  break;
			case ESHPipelineStage_Hull:         if(chosenKind == 1)  slot = &slots[3];  break;
			case ESHPipelineStage_Domain:       if(chosenKind == 1)  slot = &slots[4];  break;
			case ESHPipelineStage_GeometryExt:  if(chosenKind == 1)  slot = &slots[5];  break;
			case ESHPipelineStage_MeshExt:      if(chosenKind == 1)  slot = &slots[6];  break;
			case ESHPipelineStage_TaskExt:      if(chosenKind == 1)  slot = &slots[7];  break;
			default:                                                                    break;
		}

		Bool isRt = stage >= ESHPipelineStage_RtStartExt && stage <= ESHPipelineStage_RtEndExt;

		if (slot) {

			if (*slot != U64_MAX) {

				CharString_free(refusal, alloc);
				gotoIfError3(clean, CharString_format(
					alloc, refusal, e_rr,
					"This file has two %s entries, and which of them forms the pipeline is not something the "
					"shader states; pick one (the CLI's -entry).",
					SHEntry_stageNames[stage]
				));

				*stageCount = 0;
				goto clean;
			}

			*slot = i;
		}

		else if(!isRt || chosenKind != 2)
			continue;

		if(*stageCount < 16)
			stages[(*stageCount)++] = (SPStageRef) { .fileId = 0, .entryId = (U16) i };
	}

clean:
	ListCharString_free(&split, alloc);
	return s_uccess;
}

//The stage kinds a refusal is about, with the entries that clash, so the page can offer the same choice
// -entry offers.

static Bool Wasm_refusalCandidates(const SHFile *file, CharString *json, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	gotoIfError3(clean, Json_raw(json, "{", alloc, e_rr));

	Bool firstStage = true;

	for (U64 i = 0; i < file->entries.length; ++i) {

		U8 stage = file->entries.ptr[i].stage;
		U64 count = 0;
		Bool seen = false;

		for (U64 j = 0; j < file->entries.length; ++j) {

			if(file->entries.ptr[j].stage != stage)
				continue;

			if(j < i)
				seen = true;

			++count;
		}

		if(seen || count < 2)
			continue;

		gotoIfError3(clean, Json_key(json, SHEntry_stageNames[stage], &firstStage, alloc, e_rr));
		gotoIfError3(clean, Json_raw(json, "[", alloc, e_rr));

		Bool firstEntry = true;

		for (U64 j = 0; j < file->entries.length; ++j) {

			if(file->entries.ptr[j].stage != stage)
				continue;

			gotoIfError3(clean, Json_next(json, &firstEntry, alloc, e_rr));
			gotoIfError3(clean, Json_fmt(json, alloc, e_rr, "%"PRIu64, j));
		}

		gotoIfError3(clean, Json_raw(json, "]", alloc, e_rr));
	}

	gotoIfError3(clean, Json_raw(json, "}", alloc, e_rr));

clean:
	return s_uccess;
}

//SPFile_derivePipeline over one oiSH: everything reflection proves is filled in, everything it cannot is
// recorded as a specialization carrying the value that would be used and where it came from.

EMSCRIPTEN_KEEPALIVE void *oxc3_spDerive(const U8 *ptr, U32 length, const C8 *shaderName, const C8 *picks) {

	const Allocator *alloc = Wasm_allocator();

	if(!alloc)
		return NULL;

	Error err = Error_none(), *e_rr = &err;
	Bool s_uccess = true;

	SHFile shFile = (SHFile) { 0 };
	SPFile spFile = (SPFile) { 0 };
	ListSHFile files = (ListSHFile) { 0 };
	ListCharString shaderNames = (ListCharString) { 0 };
	CharString refusal = CharString_createNull();
	Buffer blob = Buffer_createNull();
	CharString json = CharString_createNull();
	void *frame = NULL;

	CharString shaderNameStr = Wasm_string(shaderName);

	SPStageRef stages[16];
	U8 stageCount = 0;
	U32 pipelineId = 0;

	gotoIfError3(clean, Wasm_readSH(Wasm_input(ptr, length), &shFile, alloc, e_rr));
	gotoIfError3(clean, Wasm_selectPipelineStages(
		&shFile, Wasm_string(picks), stages, &stageCount, &refusal, alloc, e_rr
	));

	if (CharString_length(refusal) || !stageCount) {

		if(!CharString_length(refusal))
			gotoIfError3(clean, CharString_createCopy(CharString_createRefCStrConst(
				"No entry of this file forms a pipeline."
			), alloc, &refusal, e_rr));

		gotoIfError3(clean, Json_raw(&json, "{\"refused\":", alloc, e_rr));
		gotoIfError3(clean, Json_str(&json, refusal, alloc, e_rr));
		gotoIfError3(clean, Json_raw(&json, ",\"candidates\":", alloc, e_rr));
		gotoIfError3(clean, Wasm_refusalCandidates(&shFile, &json, alloc, e_rr));
		gotoIfError3(clean, Json_raw(&json, "}", alloc, e_rr));

		frame = Wasm_frame(&json, NULL);
		goto clean;
	}

	gotoIfError3(clean, SPFile_create(ESPSettingsFlags_None, alloc, &spFile, e_rr));
	gotoIfError3(clean, ListSHFile_createRefConst(&shFile, 1, &files, e_rr));
	gotoIfError3(clean, ListCharString_createRefConst(&shaderNameStr, 1, &shaderNames, e_rr));

	gotoIfError3(clean, SPFile_derivePipeline(
		&spFile, &files, &shaderNames, CharString_createNull(), stages, stageCount, alloc, &pipelineId, e_rr
	));

	gotoIfError3(clean, Wasm_writeSP(&spFile, &blob, alloc, e_rr));
	gotoIfError3(clean, WasmJson_spFile(&spFile, shaderNameStr, shaderNameStr, &json, alloc, e_rr));
	frame = Wasm_frame(&json, &blob);

clean:
	SPFile_free(&spFile, alloc);
	SHFile_free(&shFile, alloc);
	ListSHFile_free(&files, alloc);
	ListCharString_free(&shaderNames, alloc);
	CharString_free(&refusal, alloc);
	CharString_free(&json, alloc);
	Buffer_free(&blob, alloc);
	return s_uccess ? frame : Wasm_errorFrameFromError(&err, "oxc3_spDerive() failed");
}

//SPFile_supply: the caller chose a value, so the field stops being assumed.
//The field arrives by the path the report prints ("blend.src[2]"), which is the same spelling -pso-set takes,
// so there is one vocabulary rather than two.

EMSCRIPTEN_KEEPALIVE void *oxc3_spSupply(
	const U8 *ptr, U32 length, U32 pipelineId, const C8 *fieldPath, U32 value, const C8 *name
) {

	const Allocator *alloc = Wasm_allocator();

	if(!alloc)
		return NULL;

	Error err = Error_none(), *e_rr = &err;
	Bool s_uccess = true;

	SPFile file = (SPFile) { 0 };
	Buffer blob = Buffer_createNull();
	CharString json = CharString_createNull();
	void *frame = NULL;

	CharString nameStr = Wasm_string(name);

	ESPField field = ESPField_Count;
	U8 index = 0;

	if(!ESPField_parsePath(Wasm_string(fieldPath), &field, &index))
		retError(clean, Error_invalidParameter(3, 0, "oxc3_spSupply()::fieldPath is not a field this report names"));

	gotoIfError3(clean, Wasm_readSP(Wasm_input(ptr, length), &file, alloc, e_rr));
	gotoIfError3(clean, SPFile_supply(&file, pipelineId, field, index, value, e_rr));
	gotoIfError3(clean, Wasm_writeSP(&file, &blob, alloc, e_rr));
	gotoIfError3(clean, WasmJson_spFile(&file, nameStr, nameStr, &json, alloc, e_rr));
	frame = Wasm_frame(&json, &blob);

clean:
	SPFile_free(&file, alloc);
	CharString_free(&json, alloc);
	Buffer_free(&blob, alloc);
	return s_uccess ? frame : Wasm_errorFrameFromError(&err, "oxc3_spSupply() failed");
}

//SPFile_print: the text `file data` prints for an oiSP, which is also what sits above a live ISA run, so an
// assumed value is never mistaken for something the shader declared.

EMSCRIPTEN_KEEPALIVE void *oxc3_spPrint(const U8 *ptr, U32 length, U32 pipelineId) {

	const Allocator *alloc = Wasm_allocator();

	if(!alloc)
		return NULL;

	Error err = Error_none(), *e_rr = &err;
	Bool s_uccess = true;

	SPFile file = (SPFile) { 0 };
	CharString text = CharString_createNull();
	CharString json = CharString_createNull();
	void *frame = NULL;

	gotoIfError3(clean, Wasm_readSP(Wasm_input(ptr, length), &file, alloc, e_rr));
	gotoIfError3(clean, SPFile_print(&file, pipelineId, alloc, &text, e_rr));

	gotoIfError3(clean, Json_raw(&json, "{\"text\":", alloc, e_rr));
	gotoIfError3(clean, Json_str(&json, text, alloc, e_rr));
	gotoIfError3(clean, Json_raw(&json, "}", alloc, e_rr));
	frame = Wasm_frame(&json, NULL);

clean:
	SPFile_free(&file, alloc);
	CharString_free(&text, alloc);
	CharString_free(&json, alloc);
	return s_uccess ? frame : Wasm_errorFrameFromError(&err, "oxc3_spPrint() failed");
}

EMSCRIPTEN_KEEPALIVE void *oxc3_spRead(const U8 *ptr, U32 length, const C8 *name) {

	const Allocator *alloc = Wasm_allocator();

	if(!alloc)
		return NULL;

	Error err = Error_none(), *e_rr = &err;
	Bool s_uccess = true;

	SPFile file = (SPFile) { 0 };
	CharString json = CharString_createNull();
	void *frame = NULL;

	CharString nameStr = Wasm_string(name);

	gotoIfError3(clean, Wasm_readSP(Wasm_input(ptr, length), &file, alloc, e_rr));
	gotoIfError3(clean, WasmJson_spFile(&file, nameStr, nameStr, &json, alloc, e_rr));
	frame = Wasm_frame(&json, NULL);

clean:
	SPFile_free(&file, alloc);
	CharString_free(&json, alloc);
	return s_uccess ? frame : Wasm_errorFrameFromError(&err, "oxc3_spRead() failed");
}

//Whether a SPIR-V module has an offline AMD ISA path at all, which is a property of its stages and needs no
// tool: ray tracing has none, so the page says so instead of offering a run that cannot answer.

EMSCRIPTEN_KEEPALIVE void *oxc3_isaHasOfflinePath(const U8 *ptr, U32 length) {

	const Allocator *alloc = Wasm_allocator();

	if(!alloc)
		return NULL;

	Error err = Error_none(), *e_rr = &err;
	Bool s_uccess = true;

	CharString json = CharString_createNull();
	void *frame = NULL;

	gotoIfError3(clean, Json_raw(&json, "{\"offline\":", alloc, e_rr));
	gotoIfError3(clean, Json_bool(&json, SpvISA_stageHasOfflinePath(Wasm_input(ptr, length), alloc), alloc, e_rr));
	gotoIfError3(clean, Json_raw(&json, "}", alloc, e_rr));
	frame = Wasm_frame(&json, NULL);

clean:
	CharString_free(&json, alloc);
	return s_uccess ? frame : Wasm_errorFrameFromError(&err, "oxc3_isaHasOfflinePath() failed");
}

//OxC3 isa devices.
//Both this and the disassemble below drive bundled tools as child processes, which no browser can do; they
// are exposed anyway so the reason the page reports is the one the library gives rather than a guess.

//The annotation vocabularies straight off the oiSH enums, so the page's syntax reference can never
//drift from what the compiler actually accepts (it did, twice, while these lists were hand-written).

//Project snapshot: the working tree (the page's files, mirrored into the module by syncProject) packed
//as a real .oiCA and back. This is the share vehicle once a project outgrows a URL: the archive travels
//as a file, the server stays static, and the same OxC3 readers open it everywhere.

typedef struct WasmCaPack {
	CAFile *ca;
	RefPtrType fileHandleType;
} WasmCaPack;

static Bool WasmCaPack_add(const FileInfo *info, void *ctx0, const Allocator *alloc, Error *e_rr) {

	WasmCaPack *ctx = (WasmCaPack*) ctx0;
	Bool s_uccess = true;

	Buffer data = Buffer_createNull();
	CharString sub = CharString_createNull();
	CharString parentPath = CharString_createNull();
	CharString leafCopy = CharString_createNull();

	//The enumerator hands back fully qualified paths ("/project/x/y" here); the archive stores paths
	// relative to the working directory, whose prefix already carries the trailing slash. The cut result
	// must be a fresh ref: CharString_cut refuses to write over an owning string such as info->path.

	CharString path = CharString_createNull();
	const CharString wd = Platform_instance->workDirectory;

	if(
		!CharString_startsWithStringSensitive(&info->path, &wd, 0) ||
		!CharString_cut(&info->path, CharString_length(wd), 0, &path)
	)
		path = CharString_createRefStrConst(info->path);

	gotoIfError3(clean, CharString_createCopy(path, alloc, &sub, e_rr));
	CharString_cutAfterLastSensitive(&sub, '/', &parentPath);

	CAHandle parent = CAHandle_Root;

	if (CharString_length(parentPath)) {

		parent = CAFile_resolve(ctx->ca, parentPath);

		if (parent == CAHandle_Invalid) {

			Log_errorLn(
				alloc, "WasmCaPack_add() parent \"%.*s\" of \"%.*s\" isn't in the archive (wd \"%.*s\")",
				(int) CharString_length(parentPath), parentPath.ptr,
				(int) CharString_length(path), path.ptr,
				(int) CharString_length(Platform_instance->workDirectory), Platform_instance->workDirectory.ptr
			);

			retError(clean, Error_invalidState(0, "WasmCaPack_add() parent lookup failed"));
		}
	}

	CharString leaf = CharString_createNull();
	CharString_cutBeforeLastSensitive(&sub, '/', &leaf);

	if(!leaf.ptr)
		leaf = sub;

	gotoIfError3(clean, CharString_createCopy(leaf, alloc, &leafCopy, e_rr));

	if (info->type == EFileType_File) {

		gotoIfError3(clean, File_read(&info->path, 100 * MS, 0, 0, &ctx->fileHandleType, &data, e_rr));

		CAHandle handle = CAFile_addFile(ctx->ca, parent, &leafCopy, info->timestamp, alloc, e_rr);

		if(handle == CAHandle_Invalid)
			retError(clean, Error_invalidState(0, "WasmCaPack_add() couldn't add file"));

		gotoIfError3(clean, CAFile_setData(ctx->ca, handle, alloc, &data, e_rr));
	}

	else if(CAFile_addFolder(ctx->ca, parent, &leafCopy, alloc, e_rr) == CAHandle_Invalid)
		retError(clean, Error_invalidState(0, "WasmCaPack_add() couldn't add folder"));

clean:
	Buffer_free(&data, alloc);
	CharString_free(&leafCopy, alloc);
	CharString_free(&sub, alloc);
	return s_uccess;
}

EMSCRIPTEN_KEEPALIVE void *oxc3_caPack() {

	const Allocator *alloc = Wasm_allocator();

	if(!alloc)
		return NULL;

	Error err = Error_none(), *e_rr = &err;
	Bool s_uccess = true;

	CAFile ca = (CAFile) { 0 };
	Bool caCreated = false;
	StreamRef *stream = NULL;
	const RefPtrType streamType = MemoryStream_makeType(alloc);
	CharString json = CharString_createNull();
	Buffer packed = Buffer_createNull();
	void *frame = NULL;

	CASettings settings = (CASettings) { .compressionType = EXXCompressionType_None };
	gotoIfError3(clean, CAFile_create(&settings, 16, 8, alloc, &ca, e_rr));
	caCreated = true;

	WasmCaPack ctx = (WasmCaPack) { .ca = &ca, .fileHandleType = FileHandle_makeType(alloc) };

	{
		const CharString here = CharString_createRefCStrConst(".");
		gotoIfError3(clean, File_foreach(&here, false, WasmCaPack_add, &ctx, true, alloc, e_rr));
	}

	gotoIfError3(clean, MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &streamType, &stream, e_rr));

	U64 offset = 0;
	gotoIfError3(clean, CAFile_write(&ca, NULL, stream, &offset, alloc, e_rr));
	gotoIfError3(clean, MemoryStream_move(&stream, &packed, e_rr));

	gotoIfError3(clean, Json_fmt(&json, alloc, e_rr, "{\"bytes\":%"PRIu64"}", Buffer_length(packed)));
	frame = Wasm_frame(&json, &packed);

clean:

	if(caCreated)
		CAFile_free(&ca, alloc);

	RefPtr_dec(&stream);
	CharString_free(&json, alloc);
	Buffer_free(&packed, alloc);
	return s_uccess ? frame : Wasm_errorFrameFromError(&err, "oxc3_caPack() failed");
}

typedef struct WasmCaUnpack {
	const CAFile *ca;
	CharString *json;
	RefPtrType fileHandleType;
	Bool first;
	U8 padding[7];
} WasmCaUnpack;

static Bool WasmCaUnpack_each(const FileInfo *info, void *ctx0, const Allocator *alloc, Error *e_rr) {

	WasmCaUnpack *ctx = (WasmCaUnpack*) ctx0;
	Bool s_uccess = true;

	if(info->type != EFileType_File)
		return true;

	Bool isValid = false;
	Buffer data = CAFile_getDataConst(ctx->ca, CAFile_resolve(ctx->ca, info->path), &isValid);

	if(!isValid)
		retError(clean, Error_invalidState(0, "WasmCaUnpack_each() entry data missing"));

	//Into the working tree, so the next compile sees exactly what the archive holds. The file API's own
	// jail refuses an entry whose path tries to climb out, which is the security story for hostile input.

	gotoIfError3(clean, File_write(&data, &info->path, 0, 0, 100 * MS, true, &ctx->fileHandleType, e_rr));

	if(!ctx->first)
		gotoIfError3(clean, Json_raw(ctx->json, ",", alloc, e_rr));

	ctx->first = false;
	gotoIfError3(clean, Json_str(ctx->json, info->path, alloc, e_rr));
	gotoIfError3(clean, Json_raw(ctx->json, ":", alloc, e_rr));
	{
		const CharString text = CharString_createRefSizedConst((const C8*)data.ptr, Buffer_length(data), false);
		gotoIfError3(clean, Json_str(ctx->json, text, alloc, e_rr));
	}

clean:
	return s_uccess;
}

EMSCRIPTEN_KEEPALIVE void *oxc3_caUnpack(const U8 *ptr, U32 length) {

	const Allocator *alloc = Wasm_allocator();

	if(!alloc || !ptr || !length)
		return NULL;

	Error err = Error_none(), *e_rr = &err;
	Bool s_uccess = true;

	CAFile ca = (CAFile) { 0 };
	Bool caCreated = false;
	StreamRef *stream = NULL;
	const RefPtrType streamType = MemoryStream_makeType(alloc);
	CharString json = CharString_createNull();
	void *frame = NULL;

	{
		Buffer src = Buffer_createRefConst(ptr, length);
		gotoIfError3(clean, MemoryStream_createFromBuffer(&src, 0, &streamType, &stream, e_rr));
	}

	U64 offset = 0;
	gotoIfError3(clean, CAFile_read(stream, NULL, offset, NULL, alloc, &ca, e_rr));
	caCreated = true;

	gotoIfError3(clean, Json_raw(&json, "{\"files\":{", alloc, e_rr));

	WasmCaUnpack ctx = (WasmCaUnpack) {
		.ca = &ca, .json = &json, .fileHandleType = FileHandle_makeType(alloc), .first = true
	};
	gotoIfError3(clean, CAFile_foreach(&ca, CAHandle_Root, WasmCaUnpack_each, &ctx, true, alloc, e_rr));

	gotoIfError3(clean, Json_raw(&json, "}}", alloc, e_rr));
	frame = Wasm_frame(&json, NULL);

clean:

	if(caCreated)
		CAFile_free(&ca, alloc);

	RefPtr_dec(&stream);
	CharString_free(&json, alloc);
	return s_uccess ? frame : Wasm_errorFrameFromError(&err, "oxc3_caUnpack() failed");
}

EMSCRIPTEN_KEEPALIVE void *oxc3_annotationEnums() {

	const Allocator *alloc = Wasm_allocator();

	if(!alloc)
		return NULL;

	Error err = Error_none(), *e_rr = &err;
	Bool s_uccess = true;

	CharString json = CharString_createNull();
	void *frame = NULL;

	gotoIfError3(clean, Json_raw(&json, "{\"extensions\":[", alloc, e_rr));

	for (U64 i = 0; i < ESHExtension_Count; ++i) {

		if(i)
			gotoIfError3(clean, Json_raw(&json, ",", alloc, e_rr));

		gotoIfError3(clean, Json_cstr(&json, ESHExtension_names[i], alloc, e_rr));
	}

	gotoIfError3(clean, Json_raw(&json, "],\"vendors\":[", alloc, e_rr));

	for (U64 i = 0; i < ESHVendor_Count; ++i) {

		if(i)
			gotoIfError3(clean, Json_raw(&json, ",", alloc, e_rr));

		gotoIfError3(clean, Json_cstr(&json, ESHVendor_names[i], alloc, e_rr));
	}

	gotoIfError3(clean, Json_raw(&json, "]}", alloc, e_rr));
	frame = Wasm_frame(&json, NULL);

clean:
	CharString_free(&json, alloc);
	return s_uccess ? frame : Wasm_errorFrameFromError(&err, "oxc3_annotationEnums() failed");
}

//The validator's verdict on arbitrary bytes (spirv-val, or DXC's validator for a container), with its own
//message: the page gates uploaded binaries through this before anything (reflection first of all) touches them.

EMSCRIPTEN_KEEPALIVE void *oxc3_validate(U32 binaryType, const U8 *ptr, U32 length) {

	const Allocator *alloc = Wasm_allocator();

	if(!alloc || !wasmHasCompiler)
		return Wasm_errorFrame("oxc3_validate() the module isn't initialized");

	Error err = Error_none(), *e_rr = &err;
	Bool s_uccess = true;

	Bool valid = false;
	CharString message = CharString_createNull();
	CharString json = CharString_createNull();
	void *frame = NULL;

	if(binaryType >= ESHBinaryType_Count)
		retError(clean, Error_invalidParameter(0, 0, "oxc3_validate()::binaryType is spirv or dxil"));

	gotoIfError3(clean, Compiler_validate(
		&wasmCompiler, (ESHBinaryType) binaryType, Wasm_input(ptr, length), alloc, &valid, &message, e_rr
	));

	gotoIfError3(clean, Json_raw(&json, valid ? "{\"valid\":true,\"message\":" : "{\"valid\":false,\"message\":", alloc, e_rr));
	gotoIfError3(clean, Json_str(&json, message, alloc, e_rr));
	gotoIfError3(clean, Json_raw(&json, "}", alloc, e_rr));
	frame = Wasm_frame(&json, NULL);

clean:
	CharString_free(&message, alloc);
	CharString_free(&json, alloc);
	return s_uccess ? frame : Wasm_errorFrameFromError(&err, "oxc3_validate() failed");
}

//The value vocabularies of every enum-typed oiSP field plus the texture formats, straight off the
//name tables beside the enums (sp_state.h / texture_format.h), so the page's field editors can offer
//dropdowns that can never drift from what the format accepts. Mask-valued fields list per-bit names;
//a reserved bit travels as null.

EMSCRIPTEN_KEEPALIVE void *oxc3_spFieldVocab() {

	const Allocator *alloc = Wasm_allocator();

	if(!alloc)
		return NULL;

	Error err = Error_none(), *e_rr = &err;
	Bool s_uccess = true;

	CharString json = CharString_createNull();
	void *frame = NULL;

	const struct { const C8 *key; const C8 *const *names; U64 count; } vocabs[] = {
		{ "ECullMode", ECullMode_names, ECullMode_Count },
		{ "ECompareOp", ECompareOp_names, ECompareOp_Count },
		{ "EStencilOp", EStencilOp_names, EStencilOp_Count },
		{ "ELogicOpExt", ELogicOpExt_names, ELogicOpExt_Count },
		{ "EBlend", EBlend_names, EBlend_Count },
		{ "EBlendOp", EBlendOp_names, EBlendOp_Count },
		{ "EMSAASamples", EMSAASamples_names, EMSAASamples_Count },
		{ "ETopologyMode", ETopologyMode_names, EToplogyMode_Count },
		{ "ERasterizerFlags", ERasterizerFlags_bitNames, 4 },
		{ "EDepthStencilFlags", EDepthStencilFlags_bitNames, 3 },
		{ "EWriteMask", EWriteMask_bitNames, 4 },
		{ "EPipelineRaytracingFlags", EPipelineRaytracingFlags_bitNames, 8 },
		{ "EDepthStencilFormat", EDepthStencilFormat_names, EDepthStencilFormat_Count },
		{ "ETextureFormatId", ETextureFormatId_name, ETextureFormatId_Count }
	};

	Bool first = true;
	gotoIfError3(clean, Json_raw(&json, "{", alloc, e_rr));

	for (U64 v = 0; v < sizeof(vocabs) / sizeof(vocabs[0]); ++v) {

		gotoIfError3(clean, Json_key(&json, vocabs[v].key, &first, alloc, e_rr));
		gotoIfError3(clean, Json_raw(&json, "[", alloc, e_rr));

		for (U64 i = 0; i < vocabs[v].count; ++i) {

			if(i)
				gotoIfError3(clean, Json_raw(&json, ",", alloc, e_rr));

			if(vocabs[v].names[i]) {
				gotoIfError3(clean, Json_cstr(&json, vocabs[v].names[i], alloc, e_rr));
			}

			else {
				gotoIfError3(clean, Json_raw(&json, "null", alloc, e_rr));
			}
		}

		gotoIfError3(clean, Json_raw(&json, "]", alloc, e_rr));
	}

	//The color-target subset of the formats: what rtv.format may legally hold. Undefined and the
	//compressed formats can't be rendered to, so they travel as null and a picker skips them while
	//the indices keep lining up with the enum's values.

	gotoIfError3(clean, Json_key(&json, "ETextureFormatIdColor", &first, alloc, e_rr));
	gotoIfError3(clean, Json_raw(&json, "[", alloc, e_rr));

	for (U64 i = 0; i < ETextureFormatId_Count; ++i) {

		if(i)
			gotoIfError3(clean, Json_raw(&json, ",", alloc, e_rr));

		if(!i || ETextureFormat_getIsCompressed(ETextureFormatId_unpack[i])) {
			gotoIfError3(clean, Json_raw(&json, "null", alloc, e_rr));
		}

		else {
			gotoIfError3(clean, Json_cstr(&json, ETextureFormatId_name[i], alloc, e_rr));
		}
	}

	gotoIfError3(clean, Json_raw(&json, "]", alloc, e_rr));

	gotoIfError3(clean, Json_raw(&json, "}", alloc, e_rr));
	frame = Wasm_frame(&json, NULL);

clean:
	CharString_free(&json, alloc);
	return s_uccess ? frame : Wasm_errorFrameFromError(&err, "oxc3_spFieldVocab() failed");
}

EMSCRIPTEN_KEEPALIVE void *oxc3_isaTargets() {

	const Allocator *alloc = Wasm_allocator();

	if(!alloc)
		return NULL;

	Error err = Error_none(), *e_rr = &err;
	Bool s_uccess = true;

	ListCharString targets = (ListCharString) { 0 };
	CharString json = CharString_createNull();
	void *frame = NULL;

	gotoIfError3(clean, SpvISA_listSupportedTargets(alloc, &targets, e_rr));

	gotoIfError3(clean, Json_raw(&json, "{\"targets\":[", alloc, e_rr));

	for (U64 i = 0; i < targets.length; ++i) {

		if(i)
			gotoIfError3(clean, Json_raw(&json, ",", alloc, e_rr));

		gotoIfError3(clean, Json_str(&json, targets.ptr[i], alloc, e_rr));
	}

	gotoIfError3(clean, Json_raw(&json, "]}", alloc, e_rr));
	frame = Wasm_frame(&json, NULL);

clean:
	ListCharString_freeUnderlying(&targets, alloc);
	CharString_free(&json, alloc);
	return s_uccess ? frame : Wasm_errorFrameFromError(&err, "oxc3_isaTargets() failed");
}

EMSCRIPTEN_KEEPALIVE void *oxc3_isaDisassemble(
	const U8 *ptr, U32 length, const C8 *gfxTarget, const C8 *entrypoint
) {

	const Allocator *alloc = Wasm_allocator();

	if(!alloc)
		return NULL;

	Error err = Error_none(), *e_rr = &err;
	Bool s_uccess = true;

	Buffer isa = Buffer_createNull();
	CharString json = CharString_createNull();
	void *frame = NULL;

	gotoIfError3(clean, SpvISA_disassemble(
		Wasm_input(ptr, length), Wasm_string(gfxTarget), Wasm_string(entrypoint), &isa, alloc, e_rr
	));

	gotoIfError3(clean, Json_raw(&json, "{\"text\":", alloc, e_rr));
	gotoIfError3(clean, Json_str(
		&json, CharString_createRefSizedConst((const C8*) isa.ptr, Buffer_length(isa), false), alloc, e_rr
	));
	gotoIfError3(clean, Json_raw(&json, "}", alloc, e_rr));
	frame = Wasm_frame(&json, NULL);

clean:
	Buffer_free(&isa, alloc);
	CharString_free(&json, alloc);
	return s_uccess ? frame : Wasm_errorFrameFromError(&err, "oxc3_isaDisassemble() failed");
}
