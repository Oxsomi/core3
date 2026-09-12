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
#include "formats/json/json_writer.h"
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
	JsonWriter writer = JsonWriter_create(&json, false, alloc);
	JsonWriter *w = &writer;

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

	gotoIfError3(clean, (
		JsonWriter_beginObject(w, e_rr) &&
		JsonWriter_key(w, "version", e_rr) &&
		JsonWriter_fmt(w, e_rr, "\"%u.%u.%u\"", OXC3_MAJOR, OXC3_MINOR, OXC3_PATCH) &&
		JsonWriter_keyObject(w, "capabilities", e_rr) &&
		JsonWriter_keyBool(w, "liveIsa", false, e_rr) &&
		JsonWriter_keyBool(w, "offlineIsa", offlineIsa, e_rr) &&
		JsonWriter_keyU64(w, "threads", Platform_getThreads(), e_rr) &&
		JsonWriter_endObject(w, e_rr) &&
		JsonWriter_endObject(w, e_rr)
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
	JsonWriter writer = JsonWriter_create(&json, false, alloc);
	JsonWriter *w = &writer;
	void *frame = NULL;

	gotoIfError3(clean, (
		JsonWriter_beginObject(w, e_rr) &&
		JsonWriter_keyArray(w, "includes", e_rr)
	));

	U64 count = Compiler_builtInIncludeCount();

	for (U64 i = 0; i < count; ++i) {

		const CompilerBuiltInInclude *include = Compiler_builtInIncludeAt(i);

		if(!include)
			continue;

		gotoIfError3(clean, (JsonWriter_beginObject(w, e_rr) && JsonWriter_keyCstr(w, "name", include->name, e_rr)));
		gotoIfError3(clean, JsonWriter_keyCstr(w, "src", include->source, e_rr));
		gotoIfError3(clean, JsonWriter_endObject(w, e_rr));
	}

	gotoIfError3(clean, (JsonWriter_endArray(w, e_rr) && JsonWriter_endObject(w, e_rr)));
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

//A document as the page consumes it: the name it knows the file by first, since a file stores none of its own,
// then the file's own members. The three formats share the shape.

static Bool Wasm_shDocument(
	const SHFile *file, CharString name, CharString sourceName, JsonWriter *w, const Allocator *alloc, Error *e_rr
) {
	return
		JsonWriter_beginObject(w, e_rr) &&
		JsonWriter_keyStr(w, "name", name, e_rr) &&
		JsonWriter_keyStr(w, "sourceName", sourceName, e_rr) &&
		SHFile_writeJsonMembers(file, NULL, NULL, w, alloc, e_rr) &&
		JsonWriter_endObject(w, e_rr);
}

static Bool Wasm_srDocument(
	const SRFile *file, CharString name, CharString sourceName, JsonWriter *w, const Allocator *alloc, Error *e_rr
) {
	return
		JsonWriter_beginObject(w, e_rr) &&
		JsonWriter_keyStr(w, "name", name, e_rr) &&
		JsonWriter_keyStr(w, "sourceName", sourceName, e_rr) &&
		SRFile_writeJsonMembers(file, w, alloc, e_rr) &&
		JsonWriter_endObject(w, e_rr);
}

static Bool Wasm_spDocument(const SPFile *file, CharString name, CharString sourceName, JsonWriter *w, Error *e_rr) {
	return
		JsonWriter_beginObject(w, e_rr) &&
		JsonWriter_keyStr(w, "name", name, e_rr) &&
		JsonWriter_keyStr(w, "sourceName", sourceName, e_rr) &&
		SPFile_writeJsonMembers(file, w, e_rr) &&
		JsonWriter_endObject(w, e_rr);
}

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
	JsonWriter writer = JsonWriter_create(&json, false, alloc);
	JsonWriter *w = &writer;
	void *frame = NULL;

	CharString nameStr = Wasm_string(name);
	CharString sourceStr = Wasm_string(source);

	if(!CharString_length(nameStr))
		retError(clean, Error_invalidParameter(0, 0, "oxc3_compileShaders()::name is required"));

	if(!(targetMask & ((1 << EGfxBinaryType_Count) - 1)))
		retError(clean, Error_invalidParameter(2, 0, "oxc3_compileShaders()::targetMask names no backend"));

	//The output name is shared by every backend on purpose: Compiler_compileShaders combines the results that
	// share one, which is the single bulky oiSH the page shows as one document.

	CharString output = CharString_createRefCStrConst("out.oiSH");

	for (U8 i = 0; i < EGfxBinaryType_Count; ++i) {

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
			for(U8 j = 0; j < EGfxBinaryType_Count; ++j)
				Buffer_free(&file.binaries.ptrNonConst[i].binaries[j], alloc);

		Buffer_free(&blob, alloc);
		gotoIfError3(clean, Wasm_writeSH(&file, &blob, alloc, e_rr));
	}

	gotoIfError3(clean, Wasm_shDocument(&file, nameStr, nameStr, w, alloc, e_rr));
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
	JsonWriter writer = JsonWriter_create(&json, false, alloc);
	JsonWriter *w = &writer;
	void *frame = NULL;

	CharString nameStr = Wasm_string(name);

	gotoIfError3(clean, Wasm_readSH(Wasm_input(ptr, length), &file, alloc, e_rr));
	gotoIfError3(clean, Wasm_shDocument(&file, nameStr, nameStr, w, alloc, e_rr));
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
	JsonWriter writer = JsonWriter_create(&json, false, alloc);
	JsonWriter *w = &writer;
	void *frame = NULL;

	CharString nameStr = Wasm_string(name);

	gotoIfError3(clean, Wasm_readSH(Wasm_input(aPtr, aLength), &a, alloc, e_rr));
	gotoIfError3(clean, Wasm_readSH(Wasm_input(bPtr, bLength), &b, alloc, e_rr));
	gotoIfError3(clean, SHFile_combine(&a, &b, alloc, &combined, e_rr));
	gotoIfError3(clean, Wasm_writeSH(&combined, &blob, alloc, e_rr));
	gotoIfError3(clean, Wasm_shDocument(&combined, nameStr, nameStr, w, alloc, e_rr));
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
	JsonWriter writer = JsonWriter_create(&json, false, alloc);
	JsonWriter *w = &writer;
	void *frame = NULL;

	gotoIfError3(clean, Wasm_readSH(Wasm_input(ptr, length), &file, alloc, e_rr));

	if(binaryId >= file.binaries.length)
		retError(clean, Error_outOfBounds(
			2, binaryId, file.binaries.length, "oxc3_shExtractBinary()::binaryId out of bounds"
		));

	if(binaryType >= EGfxBinaryType_Count)
		retError(clean, Error_invalidParameter(3, 0, "oxc3_shExtractBinary()::binaryType is spirv or dxil"));

	Buffer stored = file.binaries.ptr[binaryId].binaries[binaryType];

	if(!Buffer_length(stored))
		retError(clean, Error_notFound(0, 0, "oxc3_shExtractBinary() this binary holds no code for that backend"));

	gotoIfError3(clean, Buffer_createCopy(stored, alloc, &blob, e_rr));
	gotoIfError3(clean, (
		JsonWriter_beginObject(w, e_rr) &&
		JsonWriter_keyU64(w, "length", Buffer_length(blob), e_rr) &&
		JsonWriter_endObject(w, e_rr)
	));
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
	JsonWriter writer = JsonWriter_create(&json, false, alloc);
	JsonWriter *w = &writer;
	void *frame = NULL;

	Buffer input = Wasm_input(ptr, length);

	if(Buffer_length(input) < 4)
		retError(clean, Error_outOfBounds(1, 4, Buffer_length(input), "oxc3_fileHeader() needs at least a magic"));

	U32 magic = 0;
	Buffer_memcpy(Buffer_createRef(&magic, sizeof(magic)), Buffer_createRefConst(input.ptr, sizeof(magic)));

	if (magic == SRHeader_MAGIC) {
		gotoIfError3(clean, Wasm_readSR(input, &srFile, alloc, e_rr));
		gotoIfError3(clean, (
			JsonWriter_beginObject(w, e_rr) &&
			JsonWriter_keyCstr(w, "format", "oiSR", e_rr) &&
			JsonWriter_key(w, "document", e_rr)
		));
		gotoIfError3(clean, Wasm_srDocument(&srFile, CharString_createNull(), CharString_createNull(), w, alloc, e_rr));
		gotoIfError3(clean, JsonWriter_endObject(w, e_rr));
	}

	else if (magic == SPHeader_MAGIC) {
		gotoIfError3(clean, Wasm_readSP(input, &spFile, alloc, e_rr));
		gotoIfError3(clean, (
			JsonWriter_beginObject(w, e_rr) &&
			JsonWriter_keyCstr(w, "format", "oiSP", e_rr) &&
			JsonWriter_key(w, "document", e_rr)
		));
		gotoIfError3(clean, Wasm_spDocument(&spFile, CharString_createNull(), CharString_createNull(), w, e_rr));
		gotoIfError3(clean, JsonWriter_endObject(w, e_rr));
	}

	else if (magic == SHHeader_MAGIC) {
		gotoIfError3(clean, Wasm_readSH(input, &shFile, alloc, e_rr));
		gotoIfError3(clean, (
			JsonWriter_beginObject(w, e_rr) &&
			JsonWriter_keyCstr(w, "format", "oiSH", e_rr) &&
			JsonWriter_key(w, "document", e_rr)
		));
		gotoIfError3(clean, Wasm_shDocument(&shFile, CharString_createNull(), CharString_createNull(), w, alloc, e_rr));
		gotoIfError3(clean, JsonWriter_endObject(w, e_rr));
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
	JsonWriter writer = JsonWriter_create(&json, false, alloc);
	JsonWriter *w = &writer;
	void *frame = NULL;

	if(binaryType >= EGfxBinaryType_Count)
		retError(clean, Error_invalidParameter(0, 0, "oxc3_disassemble()::binaryType is spirv or dxil"));

	gotoIfError3(clean, Compiler_disassemble(
		&wasmCompiler, (EGfxBinaryType) binaryType, Wasm_input(ptr, length), alloc, &text, e_rr
	));

	gotoIfError3(clean, (JsonWriter_beginObject(w, e_rr) && JsonWriter_keyStr(w, "text", text, e_rr)));
	gotoIfError3(clean, JsonWriter_endObject(w, e_rr));
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
	JsonWriter writer = JsonWriter_create(&json, false, alloc);
	JsonWriter *w = &writer;
	void *frame = NULL;

	if(binaryType >= EGfxBinaryType_Count)
		retError(clean, Error_invalidParameter(0, 0, "oxc3_assemble()::binaryType is spirv or dxil"));

	gotoIfError3(clean, Compiler_assemble(
		&wasmCompiler, (EGfxBinaryType) binaryType, Wasm_string(text), alloc, &blob, e_rr
	));

	gotoIfError3(clean, (
		JsonWriter_beginObject(w, e_rr) &&
		JsonWriter_keyU64(w, "length", Buffer_length(blob), e_rr) &&
		JsonWriter_endObject(w, e_rr)
	));
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
	JsonWriter writer = JsonWriter_create(&json, false, alloc);
	JsonWriter *w = &writer;
	void *frame = NULL;

	if(binaryType >= EGfxBinaryType_Count)
		retError(clean, Error_invalidParameter(0, 0, "oxc3_uniqueEntrypoints()::binaryType is spirv or dxil"));

	gotoIfError3(clean, Compiler_getUniqueEntrypoints(
		&wasmCompiler, (EGfxBinaryType) binaryType, Wasm_input(ptr, length), showAll != 0, &entrypoints, alloc, e_rr
	));

	gotoIfError3(clean, (
		JsonWriter_beginObject(w, e_rr) &&
		JsonWriter_keyArray(w, "entrypoints", e_rr)
	));

	for (U64 i = 0; i < entrypoints.length; ++i) {

		gotoIfError3(clean, (JsonWriter_beginObject(w, e_rr) && JsonWriter_keyStr(w, "name", entrypoints.ptr[i].name, e_rr)));
		gotoIfError3(clean, JsonWriter_keyCstr(w, "stage", entrypoints.ptr[i].stage < EGfxPipelineStage_Count ?
			SHEntry_stageNames[entrypoints.ptr[i].stage] : "unknown", e_rr));
		gotoIfError3(clean, JsonWriter_endObject(w, e_rr));
	}

	gotoIfError3(clean, (JsonWriter_endArray(w, e_rr) && JsonWriter_endObject(w, e_rr)));
	frame = Wasm_frame(&json, NULL);

clean:

	for(U64 i = 0; i < entrypoints.length; ++i)
		CharString_free(&entrypoints.ptrNonConst[i].name, alloc);

	ListCompilerEntrypoint_free(&entrypoints, alloc);
	CharString_free(&json, alloc);
	return s_uccess ? frame : Wasm_errorFrameFromError(&err, "oxc3_uniqueEntrypoints() failed");
}

//An identifier's uniform values as `"uniforms": [{type, name, value}]`, the spelling every listing
// shares (the parse combinations, and the link steps of the argv query).

static Bool Wasm_jsonUniformValues(
	const SHBinaryIdentifier *identifier, JsonWriter *w, const Allocator *alloc, Error *e_rr
) {

	Bool s_uccess = true;
	CharString typeName = CharString_createNull();
	CharString value = CharString_createNull();

	gotoIfError3(clean, JsonWriter_keyArray(w, "uniforms", e_rr));

	for (U64 i = 0; i < identifier->uniforms.length; ++i) {

		SHUniformRuntime uniform = identifier->uniforms.ptr[i];
		TypeId typeId = ETypeId_arr[uniform.typeIdShort];

		CharString_free(&typeName, alloc);
		CharString_free(&value, alloc);

		if(!CharString_createFromETypeId(typeId, alloc, &typeName, NULL))
			typeName = CharString_createRefCStrConst("unknown");

		SHValue uniformValue = (SHValue) { 0 };
		Buffer_memcpy(
			Buffer_createRef(&uniformValue, sizeof(uniformValue)),
			Buffer_createRefConst(identifier->uniformData.ptr + uniform.dataOffset, ETypeId_getBytes(typeId))
		);

		if(!SHValue_stringify(&uniformValue, typeId, alloc, &value, NULL))
			value = CharString_createRefCStrConst("unknown");

		gotoIfError3(clean, (
			JsonWriter_beginObject(w, e_rr) &&
			JsonWriter_keyStr(w, "type", typeName, e_rr) &&
			JsonWriter_keyStr(w, "name", uniform.name, e_rr) &&
			JsonWriter_keyStr(w, "value", value, e_rr) &&
			JsonWriter_endObject(w, e_rr)
		));
	}

	gotoIfError3(clean, JsonWriter_endArray(w, e_rr));

clean:
	CharString_free(&typeName, alloc);
	CharString_free(&value, alloc);
	return s_uccess;
}

//One parse combination, spelled with SHFile_jsonBinary's field names and vocabularies, so the page can
// match a combination against a compiled document's binaries without a second dialect.

static Bool Wasm_jsonParseCombination(
	const SHEntryRuntime *runtime, U16 combinationId, JsonWriter *w, const Allocator *alloc, Error *e_rr
) {

	Bool s_uccess = true;

	SHBinaryIdentifier identifier = (SHBinaryIdentifier) { 0 };     //Refs into the runtime, nothing to free
	gotoIfError3(clean, SHEntryRuntime_asBinaryIdentifier(runtime, combinationId, &identifier, e_rr));

	//A graphics or compute [shader] entry always links, and the link stores the binary specialized to
	//its entrypoint and concrete stage (Compiler_compileLinkJob); only ray tracing entries stay libs.
	//The listing mirrors that, or none of these combinations would pair with what a compile stores.

	if (runtime->isShaderAnnotation && SHEntryRuntime_containsGfxOrComp(*runtime)) {
		identifier.entrypoint = CharString_createRefStrConst(runtime->entry.name);
		identifier.stageType = runtime->entry.stage;
	}

	Bool lib = !CharString_length(identifier.entrypoint);

	gotoIfError3(clean, JsonWriter_beginObject(w, e_rr));
	gotoIfError3(clean, JsonWriter_key(w, "entrypoint", e_rr));

	if(lib) {
		gotoIfError3(clean, JsonWriter_null(w, e_rr));
	}

	else gotoIfError3(clean, JsonWriter_str(w, identifier.entrypoint, e_rr));

	gotoIfError3(clean, JsonWriter_keyCstr(w, "stage", lib ? "lib" : SHEntry_stageNames[identifier.stageType], e_rr));
	gotoIfError3(clean, JsonWriter_keyBool(w, "lib", lib, e_rr));

	gotoIfError3(clean, JsonWriter_key(w, "model", e_rr));
	gotoIfError3(clean, JsonWriter_fmt(
		w, e_rr, "\"%"PRIu8".%"PRIu8"\"", (U8)(identifier.shaderVersion >> 8), (U8) identifier.shaderVersion
	));

	gotoIfError3(clean, JsonWriter_keyArray(w, "extensions", e_rr));

	for(U64 i = 0; i < ESHExtension_Count; ++i)
		if ((identifier.extensions >> i) & 1) {
			gotoIfError3(clean, JsonWriter_cstr(w, ESHExtension_names[i], e_rr));
		}

	gotoIfError3(clean, JsonWriter_endArray(w, e_rr));

	gotoIfError3(clean, JsonWriter_keyArray(w, "defines", e_rr));

	for (U64 i = 0; i < identifier.defines.length / 2; ++i)
		gotoIfError3(clean, (
			JsonWriter_beginObject(w, e_rr) &&
			JsonWriter_keyStr(w, "name", identifier.defines.ptr[i << 1], e_rr) &&
			JsonWriter_keyStr(w, "value", identifier.defines.ptr[(i << 1) | 1], e_rr) &&
			JsonWriter_endObject(w, e_rr)
		));

	gotoIfError3(clean, JsonWriter_endArray(w, e_rr));

	gotoIfError3(clean, Wasm_jsonUniformValues(&identifier, w, alloc, e_rr));
	gotoIfError3(clean, JsonWriter_endObject(w, e_rr));

clean:
	return s_uccess;
}

//Compiler_parse: the annotated entrypoints of a source and every permutation each one expands into,
// without compiling any of them. This is what fills the page's "IntelliSense follows" picker while the
// file is being edited, so it has to stay a parse: one reflection pass, no DXC codegen.
//A source that doesn't parse answers entries: null (plus the parser's own messages), so the page keeps
// the previous listing instead of flashing empty on a half-typed line.

EMSCRIPTEN_KEEPALIVE void *oxc3_parseEntrypoints(const C8 *name, const C8 *source) {

	const Allocator *alloc = Wasm_allocator();

	if(!alloc || !wasmHasCompiler)
		return Wasm_errorFrame("oxc3_parseEntrypoints() the module isn't initialized");

	Error err = Error_none(), *e_rr = &err;
	Bool s_uccess = true;

	CompileResult result = (CompileResult) { 0 };
	CharString json = CharString_createNull();
	JsonWriter writer = JsonWriter_create(&json, false, alloc);
	JsonWriter *w = &writer;
	void *frame = NULL;

	CharString nameStr = Wasm_string(name);

	if(!CharString_length(nameStr))
		retError(clean, Error_invalidParameter(0, 0, "oxc3_parseEntrypoints()::name is required"));

	CompilerSettings settings = (CompilerSettings) {
		.string = Wasm_string(source),
		.path = nameStr,
		.format = ECompilerFormat_HLSL,
		.outputType = EGfxBinaryType_SPIRV      //Ignored by parse; it reflects stage and backend agnostic
	};

	gotoIfError3(clean, Compiler_parse(&wasmCompiler, &settings, alloc, &result, e_rr));

	gotoIfError3(clean, JsonWriter_beginObject(w, e_rr));

	if (!result.isSuccess || result.type != ECompileResultType_SHEntryRuntime) {

		gotoIfError3(clean, JsonWriter_keyNull(w, "entries", e_rr));
		gotoIfError3(clean, JsonWriter_keyArray(w, "errors", e_rr));

		for (U64 i = 0; i < result.compileErrors.length; ++i)
			gotoIfError3(clean, JsonWriter_str(w, result.compileErrors.ptr[i].error, e_rr));

		gotoIfError3(clean, JsonWriter_endArray(w, e_rr));
	}

	else {

		gotoIfError3(clean, JsonWriter_keyArray(w, "entries", e_rr));

		for (U64 i = 0; i < result.shEntriesRuntime.length; ++i) {

			const SHEntryRuntime *runtime = &result.shEntriesRuntime.ptr[i];

			gotoIfError3(clean, JsonWriter_beginObject(w, e_rr));
			gotoIfError3(clean, JsonWriter_keyStr(w, "name", runtime->entry.name, e_rr));
			gotoIfError3(clean, JsonWriter_keyCstr(w, "stage", runtime->entry.stage < EGfxPipelineStage_Count ?
				SHEntry_stageNames[runtime->entry.stage] : "unknown", e_rr));

			//lib says what the STORED binaries are, matching entries[].lib in a compiled document: a
			//graphics or compute [shader] entry links into specialized binaries, so it is not one.

			gotoIfError3(clean, JsonWriter_keyBool(
				w, "lib", runtime->isShaderAnnotation && !SHEntryRuntime_containsGfxOrComp(*runtime), e_rr
			));

			//A combination id is a U16 by contract, so the space is capped rather than wrapped; a file
			// anywhere near the cap is degenerate, not a workflow.

			U32 combinations = SHEntryRuntime_getCombinations(runtime);

			if (combinations > U16_MAX)
				combinations = U16_MAX;

			gotoIfError3(clean, JsonWriter_keyArray(w, "combinations", e_rr));

			for (U32 j = 0; j < combinations; ++j)
				gotoIfError3(clean, Wasm_jsonParseCombination(runtime, (U16) j, w, alloc, e_rr));

			gotoIfError3(clean, JsonWriter_endArray(w, e_rr));
			gotoIfError3(clean, JsonWriter_endObject(w, e_rr));
		}

		gotoIfError3(clean, JsonWriter_endArray(w, e_rr));
	}

	gotoIfError3(clean, JsonWriter_endObject(w, e_rr));
	frame = Wasm_frame(&json, NULL);

clean:
	CompileResult_free(&result, alloc);
	CharString_free(&json, alloc);
	return s_uccess ? frame : Wasm_errorFrameFromError(&err, "oxc3_parseEntrypoints() failed");
}

//The link steps that finish one compile, serialized straight from Compiler_getLinkSteps, which
// enumerates and describes them with the compile driver's own code; nothing here re-derives any
// compiler behavior, so the printed steps can't drift from the links that run.

static Bool Wasm_jsonStringList(const C8 *key, const ListCharString *list, JsonWriter *w, Error *e_rr) {

	Bool s_uccess = true;

	gotoIfError3(clean, JsonWriter_keyArray(w, key, e_rr));

	for (U64 i = 0; i < list->length; ++i)
		gotoIfError3(clean, JsonWriter_str(w, list->ptr[i], e_rr));

	gotoIfError3(clean, JsonWriter_endArray(w, e_rr));

clean:
	return s_uccess;
}

static Bool Wasm_jsonCompileLinks(
	const ListSHEntryRuntime *entries,
	const SHBinaryIdentifier *cardIdentifier,
	U16 storedEntryId,
	EGfxBinaryType bt,
	Bool keepRegisters,
	JsonWriter *w,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;
	ListCompilerLinkStep steps = (ListCompilerLinkStep) { 0 };

	gotoIfError3(clean, Compiler_getLinkSteps(
		entries, cardIdentifier, storedEntryId, bt, keepRegisters, &steps, alloc, e_rr
	));

	gotoIfError3(clean, JsonWriter_keyArray(w, "links", e_rr));

	for (U64 i = 0; i < steps.length; ++i) {

		const CompilerLinkStep *step = &steps.ptr[i];
		Bool lib = !CharString_length(step->identifier.entrypoint);

		gotoIfError3(clean, JsonWriter_beginObject(w, e_rr));
		gotoIfError3(clean, JsonWriter_key(w, "entrypoint", e_rr));

		if (lib) {
			gotoIfError3(clean, JsonWriter_null(w, e_rr));
		}

		else gotoIfError3(clean, JsonWriter_str(w, step->identifier.entrypoint, e_rr));

		gotoIfError3(clean, JsonWriter_keyCstr(
			w, "stage", lib ? "lib" : SHEntry_stageNames[step->identifier.stageType], e_rr
		));

		gotoIfError3(clean, JsonWriter_keyU64(w, "combination", step->combinationId, e_rr));
		gotoIfError3(clean, JsonWriter_keyStr(w, "profile", step->profile, e_rr));
		gotoIfError3(clean, Wasm_jsonUniformValues(&step->identifier, w, alloc, e_rr));

		if (bt == EGfxBinaryType_DXIL) {

			gotoIfError3(clean, JsonWriter_key(w, "uniformsHlsl", e_rr));

			if (CharString_length(step->uniformsHlsl)) {
				gotoIfError3(clean, JsonWriter_str(w, step->uniformsHlsl, e_rr));
			}

			else gotoIfError3(clean, JsonWriter_null(w, e_rr));

			gotoIfError3(clean, Wasm_jsonStringList("uniformsArgs", &step->uniformsArgs, w, e_rr));
			gotoIfError3(clean, Wasm_jsonStringList("libs", &step->libs, w, e_rr));
			gotoIfError3(clean, Wasm_jsonStringList("linkArgs", &step->linkArgs, w, e_rr));
		}

		else gotoIfError3(clean, Wasm_jsonStringList("spirvOpt", &step->spirvOpt, w, e_rr));

		gotoIfError3(clean, JsonWriter_endObject(w, e_rr));
	}

	gotoIfError3(clean, JsonWriter_endArray(w, e_rr));

clean:
	ListCompilerLinkStep_freeUnderlying(&steps, alloc);
	return s_uccess;
}

//The exact dxc invocations a compile of this source would run: parse the entrypoints, dedupe their
// combinations the way the compile driver does (Compiler_getUniqueCompiles), gate backends the way it
// gates them, and per compile hand back the argv Compiler_compile passes dxc, the amended source when
// uniforms inject one, and the link steps that finish each permutation (Wasm_jsonCompileLinks).
//Everything comes from the driver's own helpers, so a printed line can't drift from what pressing
// compile runs.
//A source that doesn't parse answers compiles: null (plus the parser's own messages), matching
// oxc3_parseEntrypoints, so the page keeps the previous listing while a line is half typed.

EMSCRIPTEN_KEEPALIVE void *oxc3_getCompileArgs(const C8 *name, const C8 *source, U32 targetMask, U32 flags) {

	const Allocator *alloc = Wasm_allocator();

	if(!alloc || !wasmHasCompiler)
		return Wasm_errorFrame("oxc3_getCompileArgs() the module isn't initialized");

	Error err = Error_none(), *e_rr = &err;
	Bool s_uccess = true;

	CompileResult result = (CompileResult) { 0 };
	ListU32 compiles = (ListU32) { 0 };
	ListCharString args = (ListCharString) { 0 };
	CharString amended = CharString_createNull();
	CharString json = CharString_createNull();
	JsonWriter writer = JsonWriter_create(&json, false, alloc);
	JsonWriter *w = &writer;
	void *frame = NULL;

	CharString nameStr = Wasm_string(name);

	if(!CharString_length(nameStr))
		retError(clean, Error_invalidParameter(0, 0, "oxc3_getCompileArgs()::name is required"));

	if(!(targetMask & ((1 << EGfxBinaryType_Count) - 1)))
		retError(clean, Error_invalidParameter(2, 0, "oxc3_getCompileArgs()::targetMask names no backend"));

	CompilerSettings parseSettings = (CompilerSettings) {
		.string = Wasm_string(source),
		.path = nameStr,
		.format = ECompilerFormat_HLSL,
		.outputType = EGfxBinaryType_SPIRV      //Ignored by parse; it reflects stage and backend agnostic
	};

	gotoIfError3(clean, Compiler_parse(&wasmCompiler, &parseSettings, alloc, &result, e_rr));

	gotoIfError3(clean, JsonWriter_beginObject(w, e_rr));

	if (!result.isSuccess || result.type != ECompileResultType_SHEntryRuntime) {

		gotoIfError3(clean, JsonWriter_keyNull(w, "compiles", e_rr));
		gotoIfError3(clean, JsonWriter_keyArray(w, "errors", e_rr));

		for (U64 i = 0; i < result.compileErrors.length; ++i)
			gotoIfError3(clean, JsonWriter_str(w, result.compileErrors.ptr[i].error, e_rr));

		gotoIfError3(clean, JsonWriter_endArray(w, e_rr));
	}

	else {

		gotoIfError3(clean, Compiler_getUniqueCompiles(&result.shEntriesRuntime, &compiles, alloc, e_rr));

		//The union of what the file's entrypoints target after [[oxc::binary(...)]]: a backend nothing
		// in the file targets spawns no compiles, so it prints no lines either (the same gate
		// Compiler_compileShaderFile applies).

		U8 fileBinaryTypes = 0;

		for(U64 i = 0; i < result.shEntriesRuntime.length; ++i)
			fileBinaryTypes |= SHEntryRuntime_getBinaryTypes(&result.shEntriesRuntime.ptr[i]);

		gotoIfError3(clean, JsonWriter_keyArray(w, "compiles", e_rr));

		for (U8 bt = 0; bt < EGfxBinaryType_Count; ++bt) {

			if(!((targetMask >> bt) & 1) || !((fileBinaryTypes >> bt) & 1))
				continue;

			for (U64 i = 0; i < compiles.length; ++i) {

				U16 runtimeEntryId = (U16) (compiles.ptr[i] >> 16);
				U16 combinationId  = (U16) compiles.ptr[i];

				Bool isRt = combinationId >> 15;
				Bool isGfxOrComp = runtimeEntryId >> 15;

				runtimeEntryId &= (U16) I16_MAX;
				combinationId  &= (U16) I16_MAX;

				const SHEntryRuntime *runtime = &result.shEntriesRuntime.ptr[runtimeEntryId];

				//Per combination the driver skips a backend the stage or extensions can't express

				if (!((SHEntryRuntime_getSupportedBinaryTypes(runtime) >> bt) & 1))
					continue;

				CompilerSettings settings = (CompilerSettings) { 0 };
				SHBinaryIdentifier identifier = (SHBinaryIdentifier) { 0 };

				gotoIfError3(clean, Compiler_describeCompile(
					runtime, combinationId, (EGfxBinaryType) bt,
					(flags & EWasmCompileFlag_Debug) != 0,
					(flags & EWasmCompileFlag_NoOpt) != 0,
					(flags & EWasmCompileFlag_KeepRegisters) != 0,
					isRt, isGfxOrComp,
					nameStr, parseSettings.string, NULL,
					&settings, &identifier, e_rr
				));

				gotoIfError3(clean, Compiler_buildCompileArgs(&settings, &identifier, &args, &amended, alloc, e_rr));

				Bool lib = !CharString_length(identifier.entrypoint);
				Bool requiresLink = identifier.uniforms.length || (settings.isLib && settings.containsGfxOrComp);

				gotoIfError3(clean, JsonWriter_beginObject(w, e_rr));
				gotoIfError3(clean, JsonWriter_keyU64(w, "entryId", runtimeEntryId, e_rr));
				gotoIfError3(clean, JsonWriter_keyU64(w, "combination", combinationId, e_rr));
				gotoIfError3(clean, JsonWriter_keyCstr(w, "binaryType", bt == EGfxBinaryType_SPIRV ? "spirv" : "dxil", e_rr));
				gotoIfError3(clean, JsonWriter_key(w, "entrypoint", e_rr));

				if(lib) {
					gotoIfError3(clean, JsonWriter_null(w, e_rr));
				}

				else gotoIfError3(clean, JsonWriter_str(w, identifier.entrypoint, e_rr));

				gotoIfError3(clean, JsonWriter_keyCstr(
					w, "stage", lib ? "lib" : SHEntry_stageNames[identifier.stageType], e_rr
				));

				gotoIfError3(clean, JsonWriter_keyBool(w, "lib", lib, e_rr));
				gotoIfError3(clean, JsonWriter_keyBool(w, "requiresLink", requiresLink, e_rr));

				gotoIfError3(clean, JsonWriter_keyArray(w, "args", e_rr));

				for(U64 j = 0; j < args.length; ++j)
					gotoIfError3(clean, JsonWriter_str(w, args.ptr[j], e_rr));

				gotoIfError3(clean, JsonWriter_endArray(w, e_rr));
				gotoIfError3(clean, JsonWriter_key(w, "amendedSource", e_rr));

				if(amended.ptr) {
					gotoIfError3(clean, JsonWriter_str(w, amended, e_rr));
				}

				else gotoIfError3(clean, JsonWriter_null(w, e_rr));

				gotoIfError3(clean, Wasm_jsonCompileLinks(
					&result.shEntriesRuntime, &identifier, runtimeEntryId, (EGfxBinaryType) bt,
					(flags & EWasmCompileFlag_KeepRegisters) != 0, w, alloc, e_rr
				));

				gotoIfError3(clean, JsonWriter_endObject(w, e_rr));

				ListCharString_freeUnderlying(&args, alloc);
				CharString_free(&amended, alloc);
			}
		}

		gotoIfError3(clean, JsonWriter_endArray(w, e_rr));
	}

	gotoIfError3(clean, JsonWriter_endObject(w, e_rr));
	frame = Wasm_frame(&json, NULL);

clean:
	ListCharString_freeUnderlying(&args, alloc);
	CharString_free(&amended, alloc);
	ListU32_free(&compiles, alloc);
	CompileResult_free(&result, alloc);
	CharString_free(&json, alloc);
	return s_uccess ? frame : Wasm_errorFrameFromError(&err, "oxc3_getCompileArgs() failed");
}

//OxC3 shader reflect-symbols: the frontend symbol AST of the source, as an oiSR.
//This runs on source rather than on a compile, so it follows the editor instead of the compile button.

//disabledExt masks extensions out of the parse (0 = the everything-enabled default) and defines carries
//the followed binary's uniforms as newline separated NAME=VALUE lines; together they are the page's
//"IntelliSense follows this binary" picker. backend (EGfxBinaryType) picks which leg's view of the
//source is reflected: SPIRV adds -spirv, so __spirv__ and the vk:: namespace mean what they mean there.

EMSCRIPTEN_KEEPALIVE void *oxc3_reflectSymbols(
	const C8 *name, const C8 *source, U32 allowErrors, U32 disabledExt, const C8 *defines, U32 backend
) {

	const Allocator *alloc = Wasm_allocator();

	if(!alloc || !wasmHasCompiler)
		return Wasm_errorFrame("oxc3_reflectSymbols() the module isn't initialized");

	Error err = Error_none(), *e_rr = &err;
	Bool s_uccess = true;

	SRFile reflection = (SRFile) { 0 };
	Buffer blob = Buffer_createNull();
	CharString json = CharString_createNull();
	JsonWriter writer = JsonWriter_create(&json, false, alloc);
	JsonWriter *w = &writer;
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

	if(backend >= EGfxBinaryType_Count)
		retError(clean, Error_invalidParameter(5, 0, "oxc3_reflectSymbols()::backend is spirv or dxil"));

	CompilerSettings settings = (CompilerSettings) {
		.string = Wasm_string(source),
		.path = nameStr,
		.format = ECompilerFormat_HLSL,
		.outputType = (EGfxBinaryType) backend,
		.reflectAllowErrors = allowErrors != 0,
		.reflectDisabledExt = (ESHExtension) disabledExt,
		.reflectDefines = definePairs
	};

	gotoIfError3(clean, Compiler_reflect(&wasmCompiler, &settings, alloc, &reflection, e_rr));
	gotoIfError3(clean, Wasm_writeSR(&reflection, &blob, alloc, e_rr));
	gotoIfError3(clean, Wasm_srDocument(&reflection, nameStr, nameStr, w, alloc, e_rr));
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
	JsonWriter writer = JsonWriter_create(&json, false, alloc);
	JsonWriter *w = &writer;
	void *frame = NULL;

	CharString nameStr = Wasm_string(name);

	gotoIfError3(clean, Wasm_readSR(Wasm_input(ptr, length), &file, alloc, e_rr));
	gotoIfError3(clean, Wasm_srDocument(&file, nameStr, nameStr, w, alloc, e_rr));
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

		if(stage == EGfxPipelineStage_Compute)
			++kindCounts[0];

		else if(
			stage == EGfxPipelineStage_Vertex || stage == EGfxPipelineStage_Pixel || stage == EGfxPipelineStage_Hull ||
			stage == EGfxPipelineStage_Domain || stage == EGfxPipelineStage_GeometryExt ||
			stage == EGfxPipelineStage_MeshExt || stage == EGfxPipelineStage_TaskExt
		)
			++kindCounts[1];

		else if(stage >= EGfxPipelineStage_RtStartExt && stage <= EGfxPipelineStage_RtEndExt)
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
			case EGfxPipelineStage_Compute:      if(!chosenKind)      slot = &slots[0];  break;
			case EGfxPipelineStage_Vertex:       if(chosenKind == 1)  slot = &slots[1];  break;
			case EGfxPipelineStage_Pixel:        if(chosenKind == 1)  slot = &slots[2];  break;
			case EGfxPipelineStage_Hull:         if(chosenKind == 1)  slot = &slots[3];  break;
			case EGfxPipelineStage_Domain:       if(chosenKind == 1)  slot = &slots[4];  break;
			case EGfxPipelineStage_GeometryExt:  if(chosenKind == 1)  slot = &slots[5];  break;
			case EGfxPipelineStage_MeshExt:      if(chosenKind == 1)  slot = &slots[6];  break;
			case EGfxPipelineStage_TaskExt:      if(chosenKind == 1)  slot = &slots[7];  break;
			default:                                                                     break;
		}

		Bool isRt = stage >= EGfxPipelineStage_RtStartExt && stage <= EGfxPipelineStage_RtEndExt;

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

static Bool Wasm_refusalCandidates(const SHFile *file, JsonWriter *w, Error *e_rr) {

	Bool s_uccess = true;

	gotoIfError3(clean, JsonWriter_beginObject(w, e_rr));

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

			gotoIfError3(clean, JsonWriter_keyArray(w, SHEntry_stageNames[stage], e_rr));

		for (U64 j = 0; j < file->entries.length; ++j) {

			if(file->entries.ptr[j].stage != stage)
				continue;

			gotoIfError3(clean, JsonWriter_u64(w, j, e_rr));
		}

		gotoIfError3(clean, JsonWriter_endArray(w, e_rr));
	}

	gotoIfError3(clean, JsonWriter_endObject(w, e_rr));

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
	JsonWriter writer = JsonWriter_create(&json, false, alloc);
	JsonWriter *w = &writer;
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

		gotoIfError3(clean, (JsonWriter_beginObject(w, e_rr) && JsonWriter_keyStr(w, "refused", refusal, e_rr)));
		gotoIfError3(clean, JsonWriter_key(w, "candidates", e_rr));
		gotoIfError3(clean, Wasm_refusalCandidates(&shFile, w, e_rr));
		gotoIfError3(clean, JsonWriter_endObject(w, e_rr));

		frame = Wasm_frame(&json, NULL);
		goto clean;
	}

	gotoIfError3(clean, SPFile_create(ESPSettingsFlags_None, alloc, &spFile, e_rr));
	gotoIfError3(clean, ListSHFile_createRefConst(&shFile, 1, &files, e_rr));
	gotoIfError3(clean, ListCharString_createRefConst(&shaderNameStr, 1, &shaderNames, e_rr));

	gotoIfError3(clean, SPFile_derivePipeline(
		&spFile, &files, &shaderNames, CharString_createNull(), stages, stageCount, NULL, alloc, &pipelineId, e_rr
	));

	gotoIfError3(clean, Wasm_writeSP(&spFile, &blob, alloc, e_rr));
	gotoIfError3(clean, Wasm_spDocument(&spFile, shaderNameStr, shaderNameStr, w, e_rr));
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
	JsonWriter writer = JsonWriter_create(&json, false, alloc);
	JsonWriter *w = &writer;
	void *frame = NULL;

	CharString nameStr = Wasm_string(name);

	ESPField field = ESPField_Count;
	U8 index = 0;

	if(!ESPField_parsePath(Wasm_string(fieldPath), &field, &index))
		retError(clean, Error_invalidParameter(3, 0, "oxc3_spSupply()::fieldPath is not a field this report names"));

	gotoIfError3(clean, Wasm_readSP(Wasm_input(ptr, length), &file, alloc, e_rr));
	gotoIfError3(clean, SPFile_supply(&file, pipelineId, field, index, value, e_rr));
	gotoIfError3(clean, Wasm_writeSP(&file, &blob, alloc, e_rr));
	gotoIfError3(clean, Wasm_spDocument(&file, nameStr, nameStr, w, e_rr));
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
	JsonWriter writer = JsonWriter_create(&json, false, alloc);
	JsonWriter *w = &writer;
	void *frame = NULL;

	gotoIfError3(clean, Wasm_readSP(Wasm_input(ptr, length), &file, alloc, e_rr));
	gotoIfError3(clean, SPFile_print(&file, pipelineId, alloc, &text, e_rr));

	gotoIfError3(clean, (JsonWriter_beginObject(w, e_rr) && JsonWriter_keyStr(w, "text", text, e_rr)));
	gotoIfError3(clean, JsonWriter_endObject(w, e_rr));
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
	JsonWriter writer = JsonWriter_create(&json, false, alloc);
	JsonWriter *w = &writer;
	void *frame = NULL;

	CharString nameStr = Wasm_string(name);

	gotoIfError3(clean, Wasm_readSP(Wasm_input(ptr, length), &file, alloc, e_rr));
	gotoIfError3(clean, Wasm_spDocument(&file, nameStr, nameStr, w, e_rr));
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
	JsonWriter writer = JsonWriter_create(&json, false, alloc);
	JsonWriter *w = &writer;
	void *frame = NULL;

	gotoIfError3(clean, (
		JsonWriter_beginObject(w, e_rr) &&
		JsonWriter_keyBool(w, "offline", SpvISA_stageHasOfflinePath(Wasm_input(ptr, length), alloc), e_rr)
	));
	gotoIfError3(clean, JsonWriter_endObject(w, e_rr));
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
	JsonWriter writer = JsonWriter_create(&json, false, alloc);
	JsonWriter *w = &writer;
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

	gotoIfError3(clean, (
		JsonWriter_beginObject(w, e_rr) &&
		JsonWriter_keyU64(w, "bytes", Buffer_length(packed), e_rr) &&
		JsonWriter_endObject(w, e_rr)
	));
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
	JsonWriter *w;
	RefPtrType fileHandleType;
} WasmCaUnpack;

static Bool WasmCaUnpack_each(const FileInfo *info, void *ctx0, const Allocator *alloc, Error *e_rr) {

	WasmCaUnpack *ctx = (WasmCaUnpack*) ctx0;
	JsonWriter *w = ctx->w;
	Bool s_uccess = true;

	(void) alloc;        //The writer allocates; the parameter is the callback signature's

	if(info->type != EFileType_File)
		return true;

	Bool isValid = false;
	Buffer data = CAFile_getDataConst(ctx->ca, CAFile_resolve(ctx->ca, info->path), &isValid);

	if(!isValid)
		retError(clean, Error_invalidState(0, "WasmCaUnpack_each() entry data missing"));

	//Into the working tree, so the next compile sees exactly what the archive holds. The file API's own
	// jail refuses an entry whose path tries to climb out, which is the security story for hostile input.

	gotoIfError3(clean, File_write(&data, &info->path, 0, 0, 100 * MS, true, &ctx->fileHandleType, e_rr));

	gotoIfError3(clean, JsonWriter_keyString(w, info->path, e_rr));
	{
		const CharString text = CharString_createRefSizedConst((const C8*)data.ptr, Buffer_length(data), false);
		gotoIfError3(clean, JsonWriter_str(w, text, e_rr));
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
	JsonWriter writer = JsonWriter_create(&json, false, alloc);
	JsonWriter *w = &writer;
	void *frame = NULL;

	{
		Buffer src = Buffer_createRefConst(ptr, length);
		gotoIfError3(clean, MemoryStream_createFromBuffer(&src, 0, &streamType, &stream, e_rr));
	}

	U64 offset = 0;
	gotoIfError3(clean, CAFile_read(stream, NULL, offset, NULL, alloc, &ca, e_rr));
	caCreated = true;

	gotoIfError3(clean, (
		JsonWriter_beginObject(w, e_rr) &&
		JsonWriter_keyObject(w, "files", e_rr)
	));

	WasmCaUnpack ctx = (WasmCaUnpack) {
		.ca = &ca, .w = w, .fileHandleType = FileHandle_makeType(alloc)
	};
	gotoIfError3(clean, CAFile_foreach(&ca, CAHandle_Root, WasmCaUnpack_each, &ctx, true, alloc, e_rr));

	gotoIfError3(clean, (JsonWriter_endObject(w, e_rr) && JsonWriter_endObject(w, e_rr)));
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
	JsonWriter writer = JsonWriter_create(&json, false, alloc);
	JsonWriter *w = &writer;
	void *frame = NULL;

	gotoIfError3(clean, (
		JsonWriter_beginObject(w, e_rr) &&
		JsonWriter_keyArray(w, "extensions", e_rr)
	));

	for (U64 i = 0; i < ESHExtension_Count; ++i) {

		gotoIfError3(clean, JsonWriter_cstr(w, ESHExtension_names[i], e_rr));
	}

	gotoIfError3(clean, (JsonWriter_endArray(w, e_rr) && JsonWriter_keyArray(w, "vendors", e_rr)));

	for (U64 i = 0; i < ESHVendor_Count; ++i) {

		gotoIfError3(clean, JsonWriter_cstr(w, ESHVendor_names[i], e_rr));
	}

	//The extensions only one backend can compile, by the compiler's own masks

	gotoIfError3(clean, (
		JsonWriter_endArray(w, e_rr) &&
		JsonWriter_keyArray(w, "extensionsNoDxil", e_rr)
	));

	{
		for (U64 i = 0; i < ESHExtension_Count; ++i)
			if (ESHExtension_NoDxilCompile & ((U64)1 << i)) {
				gotoIfError3(clean, JsonWriter_cstr(w, ESHExtension_names[i], e_rr));
			}
	}

	gotoIfError3(clean, (
		JsonWriter_endArray(w, e_rr) &&
		JsonWriter_keyArray(w, "extensionsNoSpirv", e_rr)
	));

	{
		for (U64 i = 0; i < ESHExtension_Count; ++i)
			if (ESHExtension_NoSpirvCompile & ((U64)1 << i)) {
				gotoIfError3(clean, JsonWriter_cstr(w, ESHExtension_names[i], e_rr));
			}
	}

	//Every pipeline stage: its name, whether it is a library stage, and the DXC target prefix it compiles as

	gotoIfError3(clean, (JsonWriter_endArray(w, e_rr) && JsonWriter_keyArray(w, "stages", e_rr)));

	for (U64 i = 0; i < EGfxPipelineStage_Count; ++i) {

		const Bool lib = i >= EGfxPipelineStage_RtStartExt && i <= EGfxPipelineStage_RtEndExt;

		gotoIfError3(clean, (
			JsonWriter_beginObject(w, e_rr) &&
			JsonWriter_key(w, "name", e_rr) &&
			JsonWriter_fmt(w, e_rr, "\"%s\"", SHEntry_stageNames[i]) &&
			JsonWriter_keyBool(w, "lib", lib, e_rr) &&
			JsonWriter_key(w, "profile", e_rr) &&
			JsonWriter_fmt(w, e_rr, "\"%s\"", EGfxPipelineStage_getStagePrefix((EGfxPipelineStage) i)) &&
			JsonWriter_endObject(w, e_rr)
		));
	}

	//The shader models a binary may declare, and the floor an extension raises it to where it has one

	gotoIfError3(clean, (
		JsonWriter_endArray(w, e_rr) &&
		JsonWriter_keyObject(w, "shaderModels", e_rr) &&
		JsonWriter_key(w, "min", e_rr) &&
		JsonWriter_fmt(w, e_rr, "\"%u.%u\"", (U32) (OISH_SHADER_MODEL_MIN >> 8), (U32) (OISH_SHADER_MODEL_MIN & 0xFF)) &&
		JsonWriter_key(w, "max", e_rr) &&
		JsonWriter_fmt(w, e_rr, "\"%u.%u\"", (U32) (OISH_SHADER_MODEL_MAX >> 8), (U32) (OISH_SHADER_MODEL_MAX & 0xFF)) &&
		JsonWriter_endObject(w, e_rr) &&
		JsonWriter_keyObject(w, "extensionMinModel", e_rr)
	));

	{
		for (U64 i = 0; i < ESHExtension_Count; ++i) {

			const U16 minModel = ESHExtension_minShaderModel((ESHExtension)((U64)1 << i));

			if(minModel <= OISH_SHADER_MODEL_MIN)
				continue;

			gotoIfError3(clean, (
				JsonWriter_key(w, ESHExtension_names[i], e_rr) &&
				JsonWriter_fmt(w, e_rr, "\"%u.%u\"", (U32) (minModel >> 8), (U32) (minModel & 0xFF))
			));
		}
	}

	gotoIfError3(clean, (
		JsonWriter_endObject(w, e_rr) &&
		JsonWriter_keyObject(w, "version", e_rr) &&
		JsonWriter_keyU64(w, "major", (U32) OXC3_MAJOR, e_rr) &&
		JsonWriter_keyU64(w, "minor", (U32) OXC3_MINOR, e_rr) &&
		JsonWriter_keyU64(w, "patch", (U32) OXC3_PATCH, e_rr) &&
		JsonWriter_endObject(w, e_rr) &&
		JsonWriter_endObject(w, e_rr)
	));
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
	JsonWriter writer = JsonWriter_create(&json, false, alloc);
	JsonWriter *w = &writer;
	void *frame = NULL;

	if(binaryType >= EGfxBinaryType_Count)
		retError(clean, Error_invalidParameter(0, 0, "oxc3_validate()::binaryType is spirv or dxil"));

	gotoIfError3(clean, Compiler_validate(
		&wasmCompiler, (EGfxBinaryType) binaryType, Wasm_input(ptr, length), alloc, &valid, &message, e_rr
	));

	gotoIfError3(clean, (
		JsonWriter_beginObject(w, e_rr) &&
		JsonWriter_keyBool(w, "valid", valid, e_rr) &&
		JsonWriter_keyStr(w, "message", message, e_rr)
	));
	gotoIfError3(clean, JsonWriter_endObject(w, e_rr));
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
	JsonWriter writer = JsonWriter_create(&json, false, alloc);
	JsonWriter *w = &writer;
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

	gotoIfError3(clean, JsonWriter_beginObject(w, e_rr));

	for (U64 v = 0; v < sizeof(vocabs) / sizeof(vocabs[0]); ++v) {

		gotoIfError3(clean, JsonWriter_keyArray(w, vocabs[v].key, e_rr));

		for (U64 i = 0; i < vocabs[v].count; ++i) {

			if(vocabs[v].names[i]) {
				gotoIfError3(clean, JsonWriter_cstr(w, vocabs[v].names[i], e_rr));
			}

			else {
				gotoIfError3(clean, JsonWriter_null(w, e_rr));
			}
		}

		gotoIfError3(clean, JsonWriter_endArray(w, e_rr));
	}

	//The color-target subset of the formats: what rtv.format may legally hold. Undefined and the
	//compressed formats can't be rendered to, so they travel as null and a picker skips them while
	//the indices keep lining up with the enum's values.

	gotoIfError3(clean, JsonWriter_keyArray(w, "ETextureFormatIdColor", e_rr));

	for (U64 i = 0; i < ETextureFormatId_Count; ++i) {

		if(!i || ETextureFormat_getIsCompressed(ETextureFormatId_unpack[i])) {
			gotoIfError3(clean, JsonWriter_null(w, e_rr));
		}

		else {
			gotoIfError3(clean, JsonWriter_cstr(w, ETextureFormatId_name[i], e_rr));
		}
	}

	gotoIfError3(clean, JsonWriter_endArray(w, e_rr));

	gotoIfError3(clean, JsonWriter_endObject(w, e_rr));
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
	JsonWriter writer = JsonWriter_create(&json, false, alloc);
	JsonWriter *w = &writer;
	void *frame = NULL;

	gotoIfError3(clean, SpvISA_listSupportedTargets(alloc, &targets, e_rr));

	gotoIfError3(clean, (
		JsonWriter_beginObject(w, e_rr) &&
		JsonWriter_keyArray(w, "targets", e_rr)
	));

	for (U64 i = 0; i < targets.length; ++i) {

		gotoIfError3(clean, JsonWriter_str(w, targets.ptr[i], e_rr));
	}

	gotoIfError3(clean, (JsonWriter_endArray(w, e_rr) && JsonWriter_endObject(w, e_rr)));
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
	JsonWriter writer = JsonWriter_create(&json, false, alloc);
	JsonWriter *w = &writer;
	void *frame = NULL;

	gotoIfError3(clean, SpvISA_disassemble(
		Wasm_input(ptr, length), Wasm_string(gfxTarget), Wasm_string(entrypoint), &isa, alloc, e_rr
	));

	gotoIfError3(clean, (JsonWriter_beginObject(w, e_rr) && JsonWriter_key(w, "text", e_rr)));
	gotoIfError3(clean, JsonWriter_str(
		w, CharString_createRefSizedConst((const C8*) isa.ptr, Buffer_length(isa), false), e_rr
	));
	gotoIfError3(clean, JsonWriter_endObject(w, e_rr));
	frame = Wasm_frame(&json, NULL);

clean:
	Buffer_free(&isa, alloc);
	CharString_free(&json, alloc);
	return s_uccess ? frame : Wasm_errorFrameFromError(&err, "oxc3_isaDisassemble() failed");
}
