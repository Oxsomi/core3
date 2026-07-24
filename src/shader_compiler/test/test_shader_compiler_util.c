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

//shader_compiler/test/test_shader_compiler_util.c

#include "test_shader_compiler_shared.h"
#include <inttypes.h>
#include "shader_compiler/compiler.h"
#include "formats/oiSH/sh_file.h"
#include "platforms/file.h"
#include "types/container/string.h"
#include "types/container/buffer.h"
#include "types/container/list_basic_types.h"
#include "types/container/memory_stream.h"
#include "types/container/ref_ptr.h"
#include "types/base/time.h"

Bool compileInlineShaders(
	const Allocator *alloc,
	const C8 *const *srcs,
	U64 count,
	U8 mode,
	U64 threadCount,
	const C8 *namePrefix,   //Descriptive name so logs/errors read "<prefix>0.hlsl" instead of a generic one
	Bool enableLogging,
	ListBuffer *out,
	Error *e_rr
) {

	Bool s_uccess = true;

	ListCharString allFiles = (ListCharString) { 0 };
	ListCharString allShaderText = (ListCharString) { 0 };
	ListCharString allOutputs = (ListCharString) { 0 };
	ListU8 allCompileModes = (ListU8) { 0 };
	ListCharString includeDirs = (ListCharString) { 0 };
	CharString name = CharString_createNull();

	for (U64 i = 0; i < count; ++i) {

		//Distinct dummy names so each compiles into its own oiSH. The names never hit disk; the path
		//only feeds an -I <parent> arg, which resolves harmlessly within the working directory.

		CharString_free(&name, alloc);
		gotoIfError3(clean, CharString_format(alloc, &name, e_rr, "%s%"PRIu64".hlsl", namePrefix, i));
		gotoIfError3(clean, ListCharString_pushBack(&allFiles, name, alloc, e_rr));
		name = CharString_createNull();     //Moved into the list

		gotoIfError3(clean, ListCharString_pushBack(
			&allShaderText, CharString_createRefCStrConst(srcs[i]), alloc, e_rr
		));

		gotoIfError3(clean, CharString_format(alloc, &name, e_rr, "inline%"PRIu64".oiSH", i));
		gotoIfError3(clean, ListCharString_pushBack(&allOutputs, name, alloc, e_rr));
		name = CharString_createNull();

		gotoIfError3(clean, ListU8_pushBack(&allCompileModes, mode, alloc, e_rr));
	}

	gotoIfError3(clean, Compiler_compileShaders(
		&allFiles, &allShaderText, &allOutputs, &allCompileModes,
		threadCount,
		false,                              //isDebug
		(ECompilerWarning) 0,
		false,                              //ignoreEmptyFiles: we expect real output
		ECompileType_Compile,
		&includeDirs,
		enableLogging,
		alloc,
		out,
		e_rr
	));

clean:
	CharString_free(&name, alloc);
	ListCharString_freeUnderlying(&allFiles, alloc);
	ListCharString_free(&allShaderText, alloc);         //Elements are refs into srcs
	ListCharString_freeUnderlying(&allOutputs, alloc);
	ListU8_free(&allCompileModes, alloc);
	ListCharString_free(&includeDirs, alloc);
	return s_uccess;
}

Bool compileFileShader(
	const Allocator *alloc,
	const C8 *path,
	U8 mode,
	Bool enableLogging,
	ListBuffer *out,
	Error *e_rr
) {

	Bool s_uccess = true;

	const RefPtrType fileHandleType = FileHandle_makeType(alloc);
	Buffer fileData = Buffer_createNull();

	ListCharString allFiles = (ListCharString) { 0 };
	ListCharString allShaderText = (ListCharString) { 0 };
	ListCharString allOutputs = (ListCharString) { 0 };
	ListU8 allCompileModes = (ListU8) { 0 };
	ListCharString includeDirs = (ListCharString) { 0 };
	CharString outName = CharString_createNull();

	const CharString pathStr = CharString_createRefCStrConst(path);

	//Load the source and drive the real pipeline with the *actual* file name, so logs/errors point at the
	//shader instead of a placeholder. Feature/stage shaders are self-contained (@virtual includes only).

	gotoIfError3(clean, File_read(&pathStr, 1 * SECOND, 0, 0, &fileHandleType, &fileData, e_rr));

	gotoIfError3(clean, ListCharString_pushBack(&allFiles, CharString_createRefCStrConst(path), alloc, e_rr));

	gotoIfError3(clean, ListCharString_pushBack(
		&allShaderText,
		CharString_createRefSizedConst((const C8*) fileData.ptr, Buffer_length(fileData), false),
		alloc, e_rr
	));

	gotoIfError3(clean, CharString_format(alloc, &outName, e_rr, "%s.oiSH", path));
	gotoIfError3(clean, ListCharString_pushBack(&allOutputs, outName, alloc, e_rr));
	outName = CharString_createNull();

	gotoIfError3(clean, ListU8_pushBack(&allCompileModes, mode, alloc, e_rr));

	gotoIfError3(clean, Compiler_compileShaders(
		&allFiles, &allShaderText, &allOutputs, &allCompileModes,
		1, false, (ECompilerWarning) 0, false, ECompileType_Compile, &includeDirs, enableLogging, alloc, out, e_rr
	));

clean:
	CharString_free(&outName, alloc);
	ListCharString_free(&allFiles, alloc);              //elements ref `path`
	ListCharString_free(&allShaderText, alloc);         //element refs fileData
	ListCharString_freeUnderlying(&allOutputs, alloc);
	ListU8_free(&allCompileModes, alloc);
	ListCharString_free(&includeDirs, alloc);
	Buffer_free(&fileData, alloc);                      //after the (synchronous) compile consumed the ref
	return s_uccess;
}

Bool readOiSH(const Allocator *alloc, Buffer buf, SHFile *out, Error *e_rr) {

	Bool s_uccess = true;

	const RefPtrType msType = MemoryStream_makeType(alloc);
	MemoryStreamRef *ms = NULL;
	U64 off = 0;

	//Pass a ref: createFromBufferRegion keeps the buffer's ref bits, and MemoryStream frees owned data
	//on dec. A ref keeps the stream non-owning so the caller still owns `buf`.
	Buffer ref = Buffer_createRefFromBuffer(buf, true);

	gotoIfError3(clean, MemoryStream_createFromBufferRegion(
		ref, 0, Buffer_length(ref), EMemoryStreamFlags_None, &msType, &ms, e_rr
	));
	gotoIfError3(clean, SHFile_read((StreamRef*) ms, &off, false, alloc, out, e_rr));

clean:
	RefPtr_dec(&ms);
	return s_uccess;
}

Bool oiSHRoundtrips(const Allocator *alloc, Buffer produced, Error *e_rr) {

	SHFile file = (SHFile) { 0 };
	Buffer rewritten = Buffer_createNull();

	//Read the produced oiSH into an SHFile, serialize it back, and require byte-for-byte identity. This
	//confirms the compiler's serialization is a canonical fixed point (and that read+write agree), so a DXC
	//update or reflection change that quietly perturbs the oiSH is caught.
	Bool ok =
		readOiSH(alloc, produced, &file, e_rr) &&
		writeOiSH(alloc, &file, &rewritten, e_rr) &&
		Buffer_eq(produced, rewritten);

	Buffer_free(&rewritten, alloc);
	SHFile_free(&file, alloc);
	return ok;
}

Bool writeOiSH(const Allocator *alloc, const SHFile *file, Buffer *out, Error *e_rr) {

	Bool s_uccess = true;

	const RefPtrType msType = MemoryStream_makeType(alloc);
	MemoryStreamRef *ms = NULL;
	U64 off = 0;

	//Serialize through a resizable memory stream, then move its buffer out to the caller (who owns it).
	gotoIfError3(clean, MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &msType, &ms, e_rr));
	gotoIfError3(clean, SHFile_write((StreamRef*) ms, &off, file, alloc, e_rr));
	gotoIfError3(clean, MemoryStream_move(&ms, out, e_rr));

clean:
	RefPtr_dec(&ms);
	return s_uccess;
}
