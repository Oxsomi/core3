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

//tools/oxc3_cli/shader_tool.c

#include "tools/oxc3_cli/cli.h"
#include "shader_compiler/compiler.h"
#include "platforms/platform.h"
#include "platforms/file.h"
#include "platforms/logx.h"
#include "formats/oiSH/sh_file.h"
#include "formats/oiSR/sr_file.h"
#include "types/container/buffer.h"
#include "types/container/string.h"
#include "types/container/memory_stream.h"
#include "types/base/error.h"
#include "types/base/string_read_helper.h"
#include "types/base/constants.h"

#ifdef CLI_SHADER_COMPILER

	void CLI_ensureVirtualLoaded(const CharString *path);        //file_util.c

	//Map a file extension to a binary type: ".dxil" -> DXIL, otherwise SPIRV (".spv"/".spv.txt").

	static ESHBinaryType CLI_shaderBinaryType(CharString path) {
		const CharString dxil = CharString_createRefCStrConst(".dxil");
		return CharString_endsWithStringInsensitive(&path, &dxil, 0) ? ESHBinaryType_DXIL : ESHBinaryType_SPIRV;
	}

	//Read a raw file (the -input, unless a specific path is given) into a Buffer.

	static Bool CLI_shaderReadFile(CharString path, Buffer *out, Error *e_rr) {
		Bool s_uccess = true;
		const RefPtrType fileHandleType = FileHandle_makeType(Platform_instance->alloc);
		CLI_ensureVirtualLoaded(&path);
		gotoIfError3(clean, File_read(&path, 1 * SECOND, 0, 0, &fileHandleType, out, e_rr));
	clean:
		return s_uccess;
	}

	//Read + parse an oiSH from a path.

	static Bool CLI_shaderReadOiSH(CharString path, SHFile *out, Error *e_rr) {

		Bool s_uccess = true;
		const Allocator *alloc = Platform_instance->alloc;
		Buffer buf = Buffer_createNull();
		const RefPtrType memoryStreamType = MemoryStream_makeType(alloc);
		StreamRef *stream = NULL;

		gotoIfError3(clean, CLI_shaderReadFile(path, &buf, e_rr));

		gotoIfError3(clean, MemoryStream_createFromBufferRegion(
			Buffer_createRefFromBuffer(buf, true), 0, Buffer_length(buf),
			EMemoryStreamFlags_None, &memoryStreamType, &stream, e_rr
		));

		U64 off = 0;
		gotoIfError3(clean, SHFile_read(stream, &off, false, alloc, out, e_rr));

	clean:
		RefPtr_dec(&stream);
		Buffer_free(&buf, alloc);
		return s_uccess;
	}

	//shader entrypoints: list the entrypoints (name + stage) declared in an oiSH.

	Bool CLI_shaderEntrypoints(const ParsedArgs *args) {

		if(!args) return false;

		Bool s_uccess = true;
		Error err = Error_none(), *e_rr = &err;
		const Allocator *alloc = Platform_instance->alloc;
		const Bool verbose = args->flags & EOperationFlags_Verbose;
		SHFile file = (SHFile) { 0 };
		CharString input = (CharString) { 0 };

		gotoIfError3(clean, ParsedArgs_getArg(args, EOperationHasParameter_InputShift, &input, e_rr));
		gotoIfError3(clean, CLI_shaderReadOiSH(input, &file, e_rr));

		Log_debugLnx("%"PRIu64" entrypoint(s):", file.entries.length);

		for(U64 i = 0; i < file.entries.length; ++i) {

			const SHEntry *entry = &file.entries.ptr[i];

			if(verbose)
				SHEntry_print(entry, true, alloc);

			else Log_debugLnx(
				"\t%s: %.*s",
				SHEntry_stageNames[entry->stage],
				(int) CharString_length(entry->name), entry->name.ptr
			);
		}

	clean:
		SHFile_free(&file, alloc);
		Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);
		return s_uccess;
	}

	//shader includes: list the include files (relative path + CRC32C) the oiSH was compiled from.

	Bool CLI_shaderIncludes(const ParsedArgs *args) {

		if(!args) return false;

		Bool s_uccess = true;
		Error err = Error_none(), *e_rr = &err;
		const Allocator *alloc = Platform_instance->alloc;
		SHFile file = (SHFile) { 0 };
		CharString input = (CharString) { 0 };

		gotoIfError3(clean, ParsedArgs_getArg(args, EOperationHasParameter_InputShift, &input, e_rr));
		gotoIfError3(clean, CLI_shaderReadOiSH(input, &file, e_rr));

		Log_debugLnx("%"PRIu64" include(s):", file.includes.length);

		for(U64 i = 0; i < file.includes.length; ++i) {

			const SHInclude *inc = &file.includes.ptr[i];

			Log_debugLnx(
				"\t%.*s (CRC32C: 0x%08X)",
				(int) CharString_length(inc->relativePath), inc->relativePath.ptr, inc->crc32c
			);
		}

	clean:
		SHFile_free(&file, alloc);
		Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);
		return s_uccess;
	}

	//shader feature_set: show the shader model, binary types and extensions used across an oiSH's binaries.

	Bool CLI_shaderFeatureSet(const ParsedArgs *args) {

		if(!args) return false;

		Bool s_uccess = true;
		Error err = Error_none(), *e_rr = &err;
		const Allocator *alloc = Platform_instance->alloc;
		SHFile file = (SHFile) { 0 };
		CharString input = (CharString) { 0 };

		gotoIfError3(clean, ParsedArgs_getArg(args, EOperationHasParameter_InputShift, &input, e_rr));
		gotoIfError3(clean, CLI_shaderReadOiSH(input, &file, e_rr));

		Log_debugLnx("%"PRIu64" binary/binaries:", file.binaries.length);

		for(U64 i = 0; i < file.binaries.length; ++i) {

			const SHBinaryInfo *bin = &file.binaries.ptr[i];
			const SHBinaryIdentifier *id = &bin->identifier;

			Log_debugLnx(
				"\t%.*s (shader model %u.%u):",
				(int) CharString_length(id->entrypoint), id->entrypoint.ptr,
				(U32)(id->shaderVersion >> 8), (U32)(id->shaderVersion & 0xFF)
			);

			for(U8 j = 0; j < ESHBinaryType_Count; ++j)
				if(Buffer_length(bin->binaries[j]))
					Log_debugLnx("\t\t%s: %"PRIu64" bytes", ESHBinaryType_names[j], Buffer_length(bin->binaries[j]));

			for(U8 j = 0; j < ESHExtension_Count; ++j)
				if(id->extensions & (1 << j))
					Log_debugLnx("\t\tExtension: %s", ESHExtension_names[j]);
		}

	clean:
		SHFile_free(&file, alloc);
		Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);
		return s_uccess;
	}

	//shader disassemble: disassemble a standalone .spv/.dxil binary to text (stdout, or -output <file>).

	Bool CLI_shaderDisassemble(const ParsedArgs *args) {

		if(!args) return false;

		Bool s_uccess = true;
		Error err = Error_none(), *e_rr = &err;
		const Allocator *alloc = Platform_instance->alloc;
		const RefPtrType fileHandleType = FileHandle_makeType(alloc);

		Buffer buf = Buffer_createNull();
		CharString text = CharString_createNull();
		Compiler comp = (Compiler) { 0 };
		Bool hasCompiler = false;
		CharString input = (CharString) { 0 };

		gotoIfError3(clean, ParsedArgs_getArg(args, EOperationHasParameter_InputShift, &input, e_rr));
		gotoIfError3(clean, CLI_shaderReadFile(input, &buf, e_rr));

		gotoIfError3(clean, Compiler_create(alloc, &comp, e_rr));
		hasCompiler = true;

		gotoIfError3(clean, Compiler_disassemble(&comp, CLI_shaderBinaryType(input), buf, alloc, &text, e_rr));

		if(args->parameters & EOperationHasParameter_Output) {

			CharString output = (CharString) { 0 };
			gotoIfError3(clean, ParsedArgs_getArg(args, EOperationHasParameter_OutputShift, &output, e_rr));

			const Buffer textBuf = Buffer_createRefConst(text.ptr, CharString_length(text));
			gotoIfError3(clean, File_write(&textBuf, &output, 0, 0, 1 * SECOND, true, &fileHandleType, e_rr));
		}

		else Log_debugLnx("%.*s", (int) CharString_length(text), text.ptr);

	clean:
		if(hasCompiler)
			Compiler_free(&comp, alloc);

		CharString_free(&text, alloc);
		Buffer_free(&buf, alloc);
		Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);
		return s_uccess;
	}

	//shader assemble: assemble SPIR-V text into a .spv binary (DXIL assembly isn't supported yet).

	Bool CLI_shaderAssemble(const ParsedArgs *args) {

		if(!args) return false;

		Bool s_uccess = true;
		Error err = Error_none(), *e_rr = &err;
		const Allocator *alloc = Platform_instance->alloc;
		const RefPtrType fileHandleType = FileHandle_makeType(alloc);

		Buffer buf = Buffer_createNull();
		Buffer binary = Buffer_createNull();
		Compiler comp = (Compiler) { 0 };
		Bool hasCompiler = false;
		CharString input = (CharString) { 0 }, output = (CharString) { 0 };

		gotoIfError3(clean, ParsedArgs_getArg(args, EOperationHasParameter_InputShift, &input, e_rr));
		gotoIfError3(clean, ParsedArgs_getArg(args, EOperationHasParameter_OutputShift, &output, e_rr));

		gotoIfError3(clean, CLI_shaderReadFile(input, &buf, e_rr));

		const CharString text = CharString_createRefSizedConst((const C8*) buf.ptr, Buffer_length(buf), false);

		gotoIfError3(clean, Compiler_create(alloc, &comp, e_rr));        //DXIL assembly needs DXC (IDxcUtils/Assembler)
		hasCompiler = true;

		gotoIfError3(clean, Compiler_assemble(&comp, CLI_shaderBinaryType(output), text, alloc, &binary, e_rr));
		gotoIfError3(clean, File_write(&binary, &output, 0, 0, 1 * SECOND, true, &fileHandleType, e_rr));

		Log_debugLnx("Assembled %"PRIu64" bytes -> %.*s", Buffer_length(binary), (int) CharString_length(output), output.ptr);

	clean:
		if(hasCompiler)
			Compiler_free(&comp, alloc);

		Buffer_free(&binary, alloc);
		Buffer_free(&buf, alloc);
		Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);
		return s_uccess;
	}

	//Read one oiSH at `path`, free its compiled binaries and rewrite it in place so it's reflection-only
	//(SHFile_isComplete becomes false). Missing paths are skipped (returns true).

	static Bool CLI_shaderStripToReflection(CharString path, Bool *stripped, Error *e_rr) {

		Bool s_uccess = true;
		const Allocator *alloc = Platform_instance->alloc;
		const RefPtrType memoryStreamType = MemoryStream_makeType(alloc);
		const RefPtrType fileHandleType = FileHandle_makeType(alloc);

		SHFile file = (SHFile) { 0 };
		StreamRef *writeStream = NULL;
		Buffer outBuf = Buffer_createNull();

		if(!File_hasFile(&path, alloc))
			goto clean;

		gotoIfError3(clean, CLI_shaderReadOiSH(path, &file, e_rr));

		//Mark reflection-only + drop the compiled binaries, keeping registers/entries reflection intact

		file.flags |= ESHSettingsFlags_ReflectionOnly;

		for(U64 i = 0; i < file.binaries.length; ++i) {

			SHBinaryInfo *bin = &file.binaries.ptrNonConst[i];

			for(U8 j = 0; j < ESHBinaryType_Count; ++j)
				Buffer_free(&bin->binaries[j], alloc);
		}

		U64 writeOff = 0;
		gotoIfError3(clean, MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &memoryStreamType, &writeStream, e_rr));
		gotoIfError3(clean, SHFile_write(writeStream, &writeOff, &file, alloc, e_rr));
		gotoIfError3(clean, MemoryStream_move(&writeStream, &outBuf, e_rr));
		gotoIfError3(clean, File_write(&outBuf, &path, 0, 0, 1 * SECOND, true, &fileHandleType, e_rr));

		Log_debugLnx("-- Reflect: stripped binaries -> reflection-only %.*s", (int) CharString_length(path), path.ptr);

		if(stripped)
			*stripped = true;

	clean:
		RefPtr_dec(&writeStream);
		Buffer_free(&outBuf, alloc);
		SHFile_free(&file, alloc);
		return s_uccess;
	}

	//shader reflect: compile normally, then strip the compiled binaries so only reflection remains.
	//The compiler writes one oiSH per binary type (<output>.spv.oiSH / <output>.dxil.oiSH), so strip each.

	Bool CLI_shaderReflect(const ParsedArgs *args) {

		if(!args) return false;

		//Compile first (reuses the shader compile path)

		if(!CLI_compileShader(args))
			return false;

		Bool s_uccess = true;
		Error err = Error_none(), *e_rr = &err;
		const Allocator *alloc = Platform_instance->alloc;

		CharString output = (CharString) { 0 };
		CharString spvPath = CharString_createNull(), dxilPath = CharString_createNull();
		Bool stripped = false;

		gotoIfError3(clean, ParsedArgs_getArg(args, EOperationHasParameter_OutputShift, &output, e_rr));

		//Strip each candidate output (the explicit path, plus the per-type split names the compiler emits)

		gotoIfError3(clean, CLI_shaderStripToReflection(output, &stripped, e_rr));

		gotoIfError3(clean, CharString_format(alloc, &spvPath, e_rr, "%.*s.spv.oiSH", (int) CharString_length(output), output.ptr));
		gotoIfError3(clean, CLI_shaderStripToReflection(spvPath, &stripped, e_rr));

		gotoIfError3(clean, CharString_format(alloc, &dxilPath, e_rr, "%.*s.dxil.oiSH", (int) CharString_length(output), output.ptr));
		gotoIfError3(clean, CLI_shaderStripToReflection(dxilPath, &stripped, e_rr));

		if(!stripped) {
			Log_errorLnx("CLI_shaderReflect() no oiSH was produced at %.*s", (int) CharString_length(output), output.ptr);
			s_uccess = false;
		}

	clean:
		CharString_free(&spvPath, alloc);
		CharString_free(&dxilPath, alloc);
		Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);
		return s_uccess;
	}

	//shader reflect-symbols: run the frontend HLSL reflection (source-level symbol AST) via Compiler_reflect and
	//emit an oiSR. This is distinct from `shader reflect` (which strips a compiled oiSH to backend-binding reflection):
	//here we get entrypoints, user types, resources, enums and scopes as they appear in the source, for editor tooling.

	Bool CLI_shaderReflectSymbols(const ParsedArgs *args) {

		if(!args) return false;

		Bool s_uccess = true;
		Error err = Error_none(), *e_rr = &err;
		const Allocator *alloc = Platform_instance->alloc;
		const RefPtrType memoryStreamType = MemoryStream_makeType(alloc);
		const RefPtrType fileHandleType = FileHandle_makeType(alloc);

		Buffer buf = Buffer_createNull();
		Buffer outBuf = Buffer_createNull();
		Compiler comp = (Compiler) { 0 };
		Bool hasCompiler = false;
		SRFile reflection = (SRFile) { 0 };
		StreamRef *writeStream = NULL;
		CharString input = (CharString) { 0 };

		gotoIfError3(clean, ParsedArgs_getArg(args, EOperationHasParameter_InputShift, &input, e_rr));
		gotoIfError3(clean, CLI_shaderReadFile(input, &buf, e_rr));

		gotoIfError3(clean, Compiler_create(alloc, &comp, e_rr));
		hasCompiler = true;

		CompilerSettings settings = (CompilerSettings) {
			.string = CharString_createRefSizedConst((const C8*) buf.ptr, Buffer_length(buf), false),
			.path = input,
			.format = ECompilerFormat_HLSL,
			.outputType = ESHBinaryType_SPIRV
		};

		gotoIfError3(clean, Compiler_reflect(&comp, &settings, alloc, &reflection, e_rr));

		if(args->parameters & EOperationHasParameter_Output) {

			CharString output = (CharString) { 0 };
			gotoIfError3(clean, ParsedArgs_getArg(args, EOperationHasParameter_OutputShift, &output, e_rr));

			U64 writeOff = 0;
			gotoIfError3(clean, MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &memoryStreamType, &writeStream, e_rr));
			gotoIfError3(clean, SRFile_write(&reflection, alloc, writeStream, &writeOff, e_rr));
			gotoIfError3(clean, MemoryStream_move(&writeStream, &outBuf, e_rr));
			gotoIfError3(clean, File_write(&outBuf, &output, 0, 0, 1 * SECOND, true, &fileHandleType, e_rr));

			Log_debugLnx(
				"Reflected %"PRIu64" nodes -> %.*s",
				(U64) reflection.nodes.length, (int) CharString_length(output), output.ptr
			);
		}

		else SRFile_print(&reflection, 0, alloc);

	clean:
		RefPtr_dec(&writeStream);
		SRFile_free(&reflection, alloc);

		if(hasCompiler)
			Compiler_free(&comp, alloc);

		Buffer_free(&outBuf, alloc);
		Buffer_free(&buf, alloc);
		Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);
		return s_uccess;
	}

#else

	Bool CLI_shaderReflect(const ParsedArgs *args) { if(!args) return false; (void) args; return false; }
	Bool CLI_shaderReflectSymbols(const ParsedArgs *args) { if(!args) return false; (void) args; return false; }
	Bool CLI_shaderEntrypoints(const ParsedArgs *args) { if(!args) return false; (void) args; return false; }
	Bool CLI_shaderIncludes(const ParsedArgs *args) { if(!args) return false; (void) args; return false; }
	Bool CLI_shaderFeatureSet(const ParsedArgs *args) { if(!args) return false; (void) args; return false; }
	Bool CLI_shaderDisassemble(const ParsedArgs *args) { if(!args) return false; (void) args; return false; }
	Bool CLI_shaderAssemble(const ParsedArgs *args) { if(!args) return false; (void) args; return false; }

#endif
