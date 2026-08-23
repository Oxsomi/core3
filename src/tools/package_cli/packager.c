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

//tools/package_cli/packager.c

#include "tools/package_cli/packager.h"
#include "platforms/file.h"
#include "platforms/platform.h"
#include "formats/oiCA/ca_edit.h"
#include "formats/oiSH/sh_file.h"
#include "types/container/memory_stream.h"
#include "formats/oiCA/ca_lookup.h"
#include "types/container/list_basic_types.h"
#include "types/container/file_base.h"
#include "types/container/buffer.h"
#include "types/container/encryption_stream.h"
#include "types/container/string_helper.h"
#include "types/container/log.h"
#include "platforms/logx.h"
#include "types/base/string_read_helper.h"
#include "types/base/string_mut_helper.h"
#include "types/base/time.h"

#ifdef CLI_SHADER_COMPILER
	#include "shader_compiler/compiler.h"
#endif

#ifdef CLI_SHADER_COMPILER

//Incremental compilation.
//
//A shader only has to be recompiled when something it was built FROM changed.
//An oiSH already records almost all of that itself: sourceHash is the CRC32C of the source, and every include it
// pulled in is listed with its own CRC32C (with '\r' stripped first, so a CRLF flip doesn't read as a change).
//
//Two things it does NOT record, which is what the sidecar next to each cached oiSH carries:
//
// 1. The settings the compile ran under.
//    Include dirs matter beyond their contents, since a different search path can resolve a DIFFERENT file
//    at the same relative include.
// 2. The toolchain itself, meaning DXC's code generation and OxC3's own preprocessing and reflection.
//    Nothing an input records can express either of those.
//
//The @ headers (types.hlsli, resources.hlsli, the extension headers) are NOT part of this.
//They are embedded into the shader compiler rather than read from disk, but the compiler enumerates them, so
// Packager_cacheIsClean compares the recorded CRC against the header the compiler carries right now.
//That is exact rather than a proxy, and per shader.
//Editing one still relinks the compiler in a normal build, so the stamp below invalidates everything before
// this check ever runs.
//What it buys is a cache that stays honest when the stamp cannot see the difference, such as one carried
// over from another tree.
//
//What is left is measured from the size and timestamp of the shader compiler module rather than its
// contents, which reach 187MB in Debug while a rebuild always moves both.
//The version is folded in on top.
//It is bumped by hand per release, so it cannot see a local edit, but it does separate two trees whose
// stamps happen to agree.

typedef struct ToolchainHashRecursion {
	U32 hash;
	U64 count;
} ToolchainHashRecursion;

static Bool Packager_hashBinary(const FileInfo *file, void *recursionGeneric, const Allocator *alloc, Error *e_rr) {

	(void) alloc;
	(void) e_rr;

	Bool s_uccess = true;
	ToolchainHashRecursion *recursion = (ToolchainHashRecursion*) recursionGeneric;

	if(file->type != EFileType_File)
		goto clean;

	//ONLY the module carrying the shader compiler, which is DXC too since that is statically linked in.
	//Deliberately NOT every OxC3 binary.
	//A graphics or audio test relinking says nothing about shader output, and hashing all of them meant any
	// relink anywhere invalidated every shader.
	//Debug symbols are excluded for the same reason.
	//A .pdb is rewritten constantly and changes nothing about what a shader compiles to.

	CharString name = CharString_createNull();

	if(!CharString_cutBeforeLastSensitive(&file->path, '/', &name))
		name = file->path;

	//Reduced to the module name before matching, so this is an EXACT comparison rather than a prefix one.
	//A prefix also matched OxC3_shader_compiler_test and OxC3_package_simple, which made an unrelated test
	// relinking invalidate every cached shader.
	//The lib prefix comes off too, or the hash would find nothing at all on Linux and macOS and quietly
	// disable the cache there.

	CharString stem = CharString_createNull();

	if(!CharString_cutAfterLastSensitive(&name, '.', &stem))
		stem = name;

	const CharString libPrefix = CharString_createRefCStrConst("lib");

	if(CharString_startsWithStringSensitive(&stem, &libPrefix, 0))
		stem = CharString_createRefSizedConst(stem.ptr + 3, CharString_length(stem) - 3, false);

	const CharString shaderCompiler = CharString_createRefCStrConst("OxC3_shader_compiler");
	const CharString packager = CharString_createRefCStrConst("OxC3_package");
	const CharString pdb = CharString_createRefCStrConst(".pdb");

	if(CharString_endsWithStringInsensitive(&name, &pdb, 0))
		goto clean;

	//The packager counts only when the compiler is linked INTO it; with a shader compiler module present that
	//module is the authority and the packager's own relinks are noise.

	if(
		!CharString_equalsStringInsensitive(&stem, &shaderCompiler) &&
		!CharString_equalsStringInsensitive(&stem, &packager)
	)
		goto clean;

	//Size and timestamp rather than contents: a rebuild moves both, and hashing the binaries themselves
	//would read hundreds of megabytes on every package.

	const U64 stamp[2] = { file->fileSize, (U64) file->timestamp };

	recursion->hash = Buffer_crc32cChained(Buffer_createRefConst(stamp, sizeof(stamp)), recursion->hash);
	recursion->hash = Buffer_crc32cChained(CharString_bufferConst(name), recursion->hash);
	++recursion->count;

clean:
	return s_uccess;
}

//Hashes to 0 with a count of 0 when the directory can't be walked, which the caller treats as "always dirty"
//rather than as a cache hit, since a toolchain it couldn't measure is one it can't vouch for.

static Bool Packager_toolchainHash(U32 *result, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;
	//Seeded with the OxC3 version, so a release whose shader output changed for a reason the binary stamp
	// below cannot see still invalidates.

	const U32 version = OXC3_VERSION;

	ToolchainHashRecursion recursion = (ToolchainHashRecursion) {
		.hash = Buffer_crc32c(Buffer_createRefConst(&version, sizeof(version)))
	};

	CharString appDir = Platform_instance->appDirectory;

	gotoIfError3(clean, File_foreach(&appDir, false, Packager_hashBinary, &recursion, false, alloc, e_rr));

	*result = recursion.count ? recursion.hash : 0;

clean:

	if(!s_uccess)
		*result = 0;

	return s_uccess;
}

#endif

//The cache reads a cached oiSH to decide staleness, so it only exists where the shader compiler does.
//OxC3_package_simple builds this same file without it and has nothing to cache.

#ifdef CLI_SHADER_COMPILER

//Everything a compile's OUTPUT depends on that the oiSH does not record itself.
//The oiSH carries its own sourceHash and the CRC32C of every include, so those answer "did the inputs change".
//The settings do not live in it: a different include dir can resolve a DIFFERENT file at the same relative path, and
// the compile flags change what comes out of identical inputs.

static U32 Packager_settingsHash(const PackageSettings *settings, U32 toolchain) {

	const U32 flags =
		(settings->isDebug            ? 1u << 0 : 0) |
		(settings->extraWarnings      ? 1u << 1 : 0) |
		(settings->ignoreEmptyFiles   ? 1u << 2 : 0) |
		(settings->merge              ? 1u << 3 : 0) |
		(settings->multipleModes      ? 1u << 4 : 0) |
		(settings->keepShaderSource   ? 1u << 5 : 0);

	const U32 words[3] = { toolchain, flags, settings->compileMode };

	U32 hash = Buffer_crc32c(Buffer_createRefConst(words, sizeof(words)));

	if(CharString_length(settings->includeDir))
		hash = Buffer_crc32cChained(CharString_bufferConst(settings->includeDir), hash);

	return hash;
}

//<cacheRoot>/<archive relative output> is the previously compiled binary, with its sidecar beside it.

static Bool Packager_cachePath(
	const CharString *cacheRoot,
	const CharString *outputRel,
	Bool isSidecar,
	const Allocator *alloc,
	CharString *result,
	Error *e_rr
) {

	Bool s_uccess = true;

	gotoIfError3(clean, CharString_createCopy(*cacheRoot, alloc, result, e_rr));
	gotoIfError3(clean, CharString_append(result, '/', alloc, e_rr));
	gotoIfError3(clean, CharString_appendString(result, outputRel, alloc, e_rr));

	if(isSidecar) {
		const CharString ext = CharString_createRefCStrConst(".txt");
		gotoIfError3(clean, CharString_appendString(result, &ext, alloc, e_rr));
	}

clean:

	if(!s_uccess)
		CharString_free(result, alloc);

	return s_uccess;
}

//The sidecar is plain text so it can be read when something goes wrong.
//Only the first two numbers are load bearing; the rest is there to make a stale entry explicable rather than
// mysterious.

static Bool Packager_writeSidecar(
	const CharString *path,
	U32 toolchain,
	U32 settingsHash,
	const CharString *includeDir,
	const RefPtrType *fileHandleType,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;
	CharString text = CharString_createNull();
	Buffer buf = Buffer_createNull();

	gotoIfError3(clean, CharString_format(
		alloc, &text, e_rr,
		"oxc3-shader-cache 1\ntoolchain %08X\nsettings %08X\nincludeDir %.*s\n",
		toolchain, settingsHash,
		(int) CharString_length(*includeDir), includeDir->ptr ? includeDir->ptr : ""
	));

	buf = CharString_bufferConst(text);
	gotoIfError3(clean, File_write(&buf, path, 0, 0, 100 * MS, true, fileHandleType, e_rr));

clean:
	CharString_free(&text, alloc);
	return s_uccess;
}

//Reads back the two load bearing numbers.
//A sidecar that is missing, truncated or from another version is simply "not a hit"; it is never an error, since the
// answer is always "compile it".

static Bool Packager_readSidecar(
	const CharString *path,
	U32 *toolchain,
	U32 *settingsHash,
	const RefPtrType *fileHandleType,
	const Allocator *alloc
) {

	Buffer data = Buffer_createNull();
	Bool ok = false;

	if(!File_hasFile(path, fileHandleType->alloc))        //A miss is normal; opening a missing file RETRIES
		return false;

	if(!File_read(path, 100 * MS, 0, 0, fileHandleType, &data, NULL))
		return false;

	CharString text = CharString_createRefSizedConst((const C8*) data.ptr, Buffer_length(data), false);

	CharString toolchainLine = CharString_createRefCStrConst("toolchain ");
	CharString settingsLine = CharString_createRefCStrConst("settings ");

	const U64 a = CharString_findFirstStringSensitive(&text, &toolchainLine, 0, 0);
	const U64 b = CharString_findFirstStringSensitive(&text, &settingsLine, 0, 0);

	//Both numbers are written as exactly 8 hex digits, so the fields are fixed width once located.

	if(
		a != U64_MAX && b != U64_MAX &&
		a + CharString_length(toolchainLine) + 8 <= CharString_length(text) &&
		b + CharString_length(settingsLine) + 8 <= CharString_length(text)
	) {

		const CharString aVal = CharString_createRefSizedConst(text.ptr + a + CharString_length(toolchainLine), 8, false);
		const CharString bVal = CharString_createRefSizedConst(text.ptr + b + CharString_length(settingsLine), 8, false);

		U64 av = 0, bv = 0;

		if(CharString_parseHex(aVal, &av) && CharString_parseHex(bVal, &bv)) {
			*toolchain = (U32) av;
			*settingsHash = (U32) bv;
			ok = true;
		}
	}

	Buffer_free(&data, alloc);
	return ok;
}

//Is the cached binary at cachePath still valid for this source?
//The oiSH answers for its own inputs: sourceHash against the source we just read, and every include's
//CRC32C against the file on disk. '\r' is stripped for includes because that is how the compiler hashed
//them, so a CRLF checkout does not read as a change.
//Includes starting with '@' are the headers embedded INTO the compiler, which no file on disk backs; the
//toolchain hash is what covers those.

static Bool Packager_cacheIsClean(
	const CharString *cachePath,
	const CharString *sourcePath,
	const CharString *sourceText,
	Buffer *cached,
	const RefPtrType *fileHandleType,
	const Allocator *alloc
) {

	Bool clean = false;
	Buffer data = Buffer_createNull();
	MemoryStreamRef *stream = NULL;
	SHFile shFile = (SHFile) { 0 };
	CharString resolved = CharString_createNull();
	CharString includeData = CharString_createNull();
	Buffer includeBuf = Buffer_createNull();

	const RefPtrType memStreamType = MemoryStream_makeType(alloc);

	if(!File_hasFile(cachePath, fileHandleType->alloc))
		return false;

	if(!File_read(cachePath, 100 * MS, 0, 0, fileHandleType, &data, NULL))
		return false;

	U64 streamOffset = 0;

	if(!MemoryStream_createFromBufferRegion(
		Buffer_createRefFromBuffer(data, true), 0, Buffer_length(data), EMemoryStreamFlags_None,
		&memStreamType, &stream, NULL
	) || !stream)
		goto clean;

	if(!SHFile_read((StreamRef*) stream, &streamOffset, false, alloc, &shFile, NULL))
		goto clean;

	if(shFile.sourceHash != Buffer_crc32c(CharString_bufferConst(*sourceText)))
		goto clean;

	//Each include is stored relative to the source's own directory, so it resolves against that.

	CharString sourceDir = CharString_createNull();

	if(!CharString_cutAfterLastSensitive(sourcePath, '/', &sourceDir))
		goto clean;

	clean = true;

	for (U64 i = 0; i < shFile.includes.length && clean; ++i) {

		const SHInclude inc = shFile.includes.ptr[i];

		//Embedded in the compiler rather than read from disk, so there is no file to compare against.
		//The compiler exposes them by name, which makes this exact instead of a proxy.
		//Per include rather than through the toolchain hash, so editing one header only recompiles the
		// shaders that actually include it.

		if (CharString_startsWithSensitive(inc.relativePath, '@', 0)) {

			const CompilerBuiltInInclude *builtIn = Compiler_findBuiltInInclude(inc.relativePath);

			if(!builtIn) {
				clean = false;                                              //Header the compiler no longer has
				break;
			}

			CharString_free(&includeData, alloc);

			if(!CharString_createCopy(
				CharString_createRefCStrConst(builtIn->source), alloc, &includeData, NULL
			)) {
				clean = false;
				break;
			}

			//Recorded with the \r characters stripped, so the comparison strips them too.
			//See IncludeHandler::LoadSource in compiler.cpp.

			CharString_eraseAllSensitive(&includeData, '\r', 0, 0);

			if(Buffer_crc32c(CharString_bufferConst(includeData)) != inc.crc32c)
				clean = false;

			continue;
		}

		CharString_free(&resolved, alloc);

		if(
			!CharString_createCopy(sourceDir, alloc, &resolved, NULL) ||
			!CharString_append(&resolved, '/', alloc, NULL) ||
			!CharString_appendString(&resolved, &inc.relativePath, alloc, NULL)
		) {
			clean = false;
			break;
		}

		Buffer_free(&includeBuf, alloc);

		if(
			!File_hasFile(&resolved, fileHandleType->alloc) ||
			!File_read(&resolved, 100 * MS, 0, 0, fileHandleType, &includeBuf, NULL)
		) {
			clean = false;                                                  //Include vanished or moved
			break;
		}

		CharString_free(&includeData, alloc);

		if(!CharString_createCopy(
			CharString_createRefSizedConst((const C8*) includeBuf.ptr, Buffer_length(includeBuf), false),
			alloc, &includeData, NULL
		)) {
			clean = false;
			break;
		}

		CharString_eraseAllSensitive(&includeData, '\r', 0, 0);

		if(Buffer_crc32c(CharString_bufferConst(includeData)) != inc.crc32c)
			clean = false;
	}

	//OWNERSHIP moves to the caller, it is not a ref: a ref would leave this allocation owned by nobody, and
	//the list it lands in frees refs as a no-op, so every cache hit leaked the whole cached oiSH.

	if (clean) {
		*cached = data;
		data = Buffer_createNull();
	}

clean:

	Buffer_free(&includeBuf, alloc);
	CharString_free(&includeData, alloc);
	CharString_free(&resolved, alloc);
	SHFile_free(&shFile, alloc);
	RefPtr_dec(&stream);

	if(!clean)
		Buffer_free(&data, alloc);

	return clean;
}

#endif

typedef struct CAFileRecursion {
	CAFile *archive;
	CharString root;
	const RefPtrType *fileHandleType;
	Bool keepShaderSource;        //See PackageSettings; stores .hlsl as-is rather than compiling it
} CAFileRecursion;

Bool packageFile(const FileInfo *file, void *pkgFileGeneric, const Allocator *alloc, Error *e_rr) {

	CAFileRecursion *pkgFile = (CAFileRecursion*) pkgFileGeneric;

	Bool s_uccess = true;
	CharString subPath = CharString_createNull();
	Buffer data = Buffer_createNull();

	//Virtual foreach also echoes the queried folder itself ("//section" for a root of "//section/"),
	// where physical foreach only reports children.
	//There's nothing to cut there; it IS the root.

	if(CharString_length(file->path) < CharString_length(pkgFile->root))
		goto clean;

	if(!CharString_cut(&file->path, CharString_length(pkgFile->root), 0, &subPath))
		retError(clean, Error_invalidState(0, "packageFile()::file.path cut failed"));

	CharString parentPath = CharString_createNull();
	CharString_cutAfterLastSensitive(&subPath, '/', &parentPath);

	CAHandle parent = CAHandle_Root;

	if (CharString_length(parentPath)) {

		parent = CAFile_resolve(pkgFile->archive, parentPath);

		if(parent == CAHandle_Invalid)
			retError(clean, Error_invalidState(0, "packageFile()::file.path parent lookup failed"));
	}

	CharString tmp = CharString_createNull();
	CharString_cutBeforeLastSensitive(&subPath, '/', &tmp);

	if(!tmp.ptr)
		tmp = subPath;

	subPath = CharString_createNull();
	gotoIfError3(clean, CharString_createCopy(tmp, alloc, &subPath, e_rr));

	if (file->type == EFileType_File) {

		gotoIfError3(clean, File_read(&file->path, 100 * MS, 0, 0, pkgFile->fileHandleType, &data, e_rr));

		//If shader compilation is used, skip hlsl files, they'll be turned into oiSH files later.

		#ifdef CLI_SHADER_COMPILER

			const CharString hlsl = CharString_createRefCStrConst(".hlsl");
			const CharString hlsli = CharString_createRefCStrConst(".hlsli");

			if (
				!pkgFile->keepShaderSource && (
					CharString_endsWithStringSensitive(&file->path, &hlsl, 0) ||
					CharString_endsWithStringSensitive(&file->path, &hlsli, 0)
				)
			)
				goto clean;
		#endif

		//We have to detect file type and process it here to a custom type.
		//We don't have a custom file yet (besides oiSH), so for now this will just be identical to addFileToCAFile.

		CAHandle handle = CAFile_addFile(pkgFile->archive, parent, &subPath, 0, alloc, e_rr);

		if(handle == CAHandle_Invalid)
			retError(clean, Error_invalidState(0, "packageFile() couldn't add file"));

		gotoIfError3(clean, CAFile_setData(pkgFile->archive, handle, alloc, &data, e_rr));
	}

	else {
		CAHandle handle = CAFile_addFolder(pkgFile->archive, parent, &subPath, alloc, e_rr);

		if(handle == CAHandle_Invalid)
			retError(clean, Error_invalidState(0, "packageFile() couldn't add folder"));
	}

clean:
	CharString_free(&subPath, alloc);
	Buffer_free(&data, alloc);
	return s_uccess;
}

Bool Packager_package(const PackageSettings *settings, const Allocator *alloc, Error *e_rr) {

	CAFile archive = (CAFile) { 0 };
	CharString resolved = CharString_createNull();
	CharString leafCopy = CharString_createNull();
	Bool isVirtual = false;
	Bool s_uccess = true;
	StreamRef *stream = NULL;

	ListCharString allFiles = (ListCharString) { 0 };
	ListCharString allShaderText = (ListCharString) { 0 };
	ListCharString allOutputs = (ListCharString) { 0 };
	ListU8 allCompileOutputs = (ListU8) { 0 };
	ListBuffer allBuffers = (ListBuffer) { 0 };

	//Incremental shader compilation; see the cache block below.

	ListCharString dirtyFiles = (ListCharString) { 0 };
	ListCharString dirtyText = (ListCharString) { 0 };
	ListCharString dirtyOutputs = (ListCharString) { 0 };
	ListU8 dirtyCompileOutputs = (ListU8) { 0 };
	ListU64 dirtyIndices = (ListU64) { 0 };
	ListBuffer dirtyBuffers = (ListBuffer) { 0 };
	ListU8 reused = (ListU8) { 0 };

	//The serialized package, and whatever the output already held, so an unchanged one is not rewritten.

	Buffer packaged = Buffer_createNull();
	Buffer previous = Buffer_createNull();

	CharString cacheRoot = CharString_createNull();
	CharString cachePath = CharString_createNull();
	CharString sidecarPath = CharString_createNull();
	ListCharString includeDirs = (ListCharString) { 0 };

	Ns start = Time_now();

	CASettings caSettings = (CASettings) { .compressionType = EXXCompressionType_None };

	RefPtrType fileHandleType = FileHandle_makeType(alloc);
	RefPtrType encStreamType = EncryptionStream_makeType(alloc);
	RefPtrType memStreamType = MemoryStream_makeType(alloc);

	if(!settings)
		retError(clean, Error_nullPointer(0, "Packager_package()::settings is required"));

	if(settings->encryptionKey)
		caSettings.encryptionType = EXXEncryptionType_AES256GCM;

	//Copying encryption key

	if(caSettings.encryptionType)
		Buffer_memcpy(
			Buffer_createRef(caSettings.encryptionKey, sizeof(caSettings.encryptionKey)),
			Buffer_createRefConst(settings->encryptionKey, sizeof(caSettings.encryptionKey))
		);

	//Grab all files that need compilation

	//keepShaderSource stores the .hlsl instead, so there's nothing to compile and the enumeration below
	// would only find shaders it must not touch.

	#ifdef CLI_SHADER_COMPILER
		if(!settings->keepShaderSource)
			gotoIfError3(clean, Compiler_getTargetsFromFile(
				settings->input,
				ECompileType_Compile,
				settings->compileMode,
				settings->multipleModes,
				settings->merge,
				settings->enableLogging,
				alloc,
				NULL,
				NULL,        //Don't write to output, write to Buffer[] instead
				&allFiles,
				&allShaderText,
				&allOutputs,
				&allCompileOutputs
			));
	#endif

	//Make archive

	gotoIfError3(clean, CAFile_create(&caSettings, 16, 8, alloc, &archive, e_rr));
	gotoIfError3(clean, File_resolve(&settings->input, &isVirtual, 0, &Platform_instance->defaultDir, alloc, &resolved, e_rr));

	gotoIfError3(clean, CharString_append(&resolved, '/', alloc, e_rr));

	//Foreach reports full virtual paths ("//section/...") while resolve strips the marker; re-add it so
	// the root cut in packageFile lines up when a virtual folder is being packaged.

	if(isVirtual) {
		const CharString virtualPrefix = CharString_createRefCStrConst("//");
		gotoIfError3(clean, CharString_insertString(&resolved, &virtualPrefix, 0, alloc, e_rr));
	}

	CAFileRecursion caFileRecursion = (CAFileRecursion) {
		.archive = &archive,
		.root = resolved,
		.fileHandleType = &fileHandleType,
		.keepShaderSource = settings->keepShaderSource
	};

	gotoIfError3(clean, File_foreach(
		&caFileRecursion.root,
		false,
		packageFile,
		&caFileRecursion,
		true,
		alloc,
		e_rr
	));

	//Convert shaders

	#ifdef CLI_SHADER_COMPILER

		if(allFiles.length) {

			//Split the ';'-delimited include dir (e.g. "a;b;c") into separate -I search paths

			if(CharString_length(settings->includeDir)) {
				CharStringSplit includeSplit = (CharStringSplit) {
					.s = &settings->includeDir, .allocator = alloc, .result = &includeDirs
				};
				gotoIfError3(clean, CharString_splitSensitive(&includeSplit, ';', e_rr));
			}

			//Incremental compilation.
			//
			//A shader only has to be recompiled when something it was built FROM changed, and the oiSH records almost all of
			// that itself (see Packager_cacheIsClean).
			//What it does not record is the toolchain and the settings, which the sidecar beside each cached binary carries.
			//
			//The unit is the OUTPUT path, not the source: with merge on, several consecutive entries share one output and only
			// the LAST carries data, the earlier ones being deliberately empty (see the archive loop below).
			//So a run is reused or recompiled whole, never half.

			const CharString cacheSuffix = CharString_createRefCStrConst(".cache");

			U32 toolchainHash = 0;
			(void) Packager_toolchainHash(&toolchainHash, alloc, NULL);

			const U32 settingsHash = Packager_settingsHash(settings, toolchainHash);

			//A toolchain we could not measure is one we cannot vouch for, so it disables the cache rather than
			//silently serving whatever was there.

			const Bool cacheUsable = toolchainHash != 0;

			gotoIfError3(clean, CharString_createCopy(settings->output, alloc, &cacheRoot, e_rr));
			gotoIfError3(clean, CharString_appendString(&cacheRoot, &cacheSuffix, alloc, e_rr));

			gotoIfError3(clean, ListBuffer_resize(&allBuffers, allOutputs.length, alloc, e_rr));
			gotoIfError3(clean, ListU8_resize(&reused, allOutputs.length, alloc, e_rr));

			U64 hits = 0, misses = 0;
			U64 missNoSidecar = 0, missToolchain = 0, missSettings = 0, missContent = 0;

			for (U64 i = 0; i < allOutputs.length; ++i) {

				//Only the last index of a run of equal output names carries the binary.

				const Bool isLastOfRun =
					i + 1 == allOutputs.length ||
					!CharString_equalsStringSensitive(&allOutputs.ptr[i], &allOutputs.ptr[i + 1]);

				if(!isLastOfRun || !cacheUsable)
					continue;

				CharString_free(&cachePath, alloc);
				CharString_free(&sidecarPath, alloc);

				if(
					!Packager_cachePath(&cacheRoot, &allOutputs.ptr[i], false, alloc, &cachePath, NULL) ||
					!Packager_cachePath(&cacheRoot, &allOutputs.ptr[i], true, alloc, &sidecarPath, NULL)
				)
					continue;

				U32 storedToolchain = 0, storedSettings = 0;

				if(!Packager_readSidecar(&sidecarPath, &storedToolchain, &storedSettings, &fileHandleType, alloc)) {
					++missNoSidecar;                        //Nothing cached yet, or the cache was wiped
					continue;
				}

				if(storedToolchain != toolchainHash) {
					++missToolchain;                        //A shader compiler or DXC binary moved
					continue;
				}

				if(storedSettings != settingsHash) {
					++missSettings;                         //Include dirs or compile flags differ
					continue;
				}

				Buffer cached = Buffer_createNull();

				if(!Packager_cacheIsClean(
					&cachePath, &allFiles.ptr[i], &allShaderText.ptr[i], &cached, &fileHandleType, alloc
				)) {
					++missContent;                          //The source or one of its includes changed
					continue;
				}

				allBuffers.ptrNonConst[i] = cached;        //Owned by the list now, freed with it
				reused.ptrNonConst[i] = 1;
				++hits;
			}

			//Everything not reused is compiled, in one call, so threading is unchanged for the work that is
			//actually left.

			for (U64 i = 0; i < allOutputs.length; ++i) {

				if(reused.ptr[i])
					continue;

				//An entry whose RUN was reused rides along with it and must not be compiled either.

				U64 runEnd = i;

				while(
					runEnd + 1 < allOutputs.length &&
					CharString_equalsStringSensitive(&allOutputs.ptr[runEnd], &allOutputs.ptr[runEnd + 1])
				)
					++runEnd;

				if(reused.ptr[runEnd]) {
					reused.ptrNonConst[i] = 1;             //Empty buffer, exactly as a merged compile produces
					continue;
				}

				gotoIfError3(clean, ListCharString_pushBack(&dirtyFiles, allFiles.ptr[i], alloc, e_rr));
				gotoIfError3(clean, ListCharString_pushBack(&dirtyText, allShaderText.ptr[i], alloc, e_rr));
				gotoIfError3(clean, ListCharString_pushBack(&dirtyOutputs, allOutputs.ptr[i], alloc, e_rr));
				gotoIfError3(clean, ListU8_pushBack(&dirtyCompileOutputs, allCompileOutputs.ptr[i], alloc, e_rr));
				gotoIfError3(clean, ListU64_pushBack(&dirtyIndices, i, alloc, e_rr));
				++misses;
			}

			if(settings->enableLogging && cacheUsable)
				Log_debugLnx(
					"-- Shader cache: %"PRIu64" reused, %"PRIu64" to compile "
					"(toolchain %08X, settings %08X; missed: %"PRIu64" uncached, %"PRIu64" toolchain, "
					"%"PRIu64" settings, %"PRIu64" content)",
					hits, misses, toolchainHash, settingsHash,
					missNoSidecar, missToolchain, missSettings, missContent
				);

			if (dirtyOutputs.length) {

				gotoIfError3(clean, Compiler_compileShaders(
					&dirtyFiles, &dirtyText, &dirtyOutputs, &dirtyCompileOutputs,
					settings->threadCount,
					settings->isDebug,
					false,                        //keepRegisters; engine shaders keep the default (strip-friendly) mode
					settings->extraWarnings,
					settings->ignoreEmptyFiles,
					ECompileType_Compile,
					&includeDirs,
					true,
					alloc,
					&dirtyBuffers,
					e_rr
				));

				//Splice the freshly compiled binaries back onto the indices they came from, and record each one
				//in the cache for next time.

				for (U64 i = 0; i < dirtyIndices.length && i < dirtyBuffers.length; ++i) {

					const U64 dst = dirtyIndices.ptr[i];

					allBuffers.ptrNonConst[dst] = dirtyBuffers.ptrNonConst[i];
					dirtyBuffers.ptrNonConst[i] = Buffer_createNull();        //Ownership moved

					if(!cacheUsable || !Buffer_length(allBuffers.ptr[dst]))
						continue;

					CharString_free(&cachePath, alloc);
					CharString_free(&sidecarPath, alloc);

					if(
						!Packager_cachePath(&cacheRoot, &allOutputs.ptr[dst], false, alloc, &cachePath, NULL) ||
						!Packager_cachePath(&cacheRoot, &allOutputs.ptr[dst], true, alloc, &sidecarPath, NULL)
					)
						continue;

					//A cache that fails to write is not an error: the build is still correct, just not faster.

					if(File_write(&allBuffers.ptr[dst], &cachePath, 0, 0, 100 * MS, true, &fileHandleType, NULL))
						(void) Packager_writeSidecar(
							&sidecarPath, toolchainHash, settingsHash, &settings->includeDir,
							&fileHandleType, alloc, NULL
						);
				}
			}
		}

		for(U64 i = 0; i < allOutputs.length; ++i) {

			if(!Buffer_length(allBuffers.ptrNonConst[i])) {

				if(                                                            //Merged binaries contain empty buffers
					settings->merge &&
					i + 1 != allOutputs.length &&
					CharString_equalsStringSensitive(&allOutputs.ptr[i], &allOutputs.ptr[i + 1])
				)
					continue;

				retError(clean, Error_invalidState(0, "Packager_package() one of the shaders didn't compile, aborting packaging"));
			}

			//allOutputs[i] is an archive-relative path (same rooting as packageFile's subPath),
			// so split it into parent folder + leaf name, resolve the parent (already added during the file walk),
			// add the leaf and attach the compiled buffer.
			//CAFile_addFile moves the name and CAFile_setData moves the buffer out.

			CharString outPath = allOutputs.ptr[i];

			CharString parentPath = CharString_createNull();
			CharString_cutAfterLastSensitive(&outPath, '/', &parentPath);

			CAHandle parent = CAHandle_Root;

			if (CharString_length(parentPath)) {

				parent = CAFile_resolve(&archive, parentPath);

				if(parent == CAHandle_Invalid)
					retError(clean, Error_invalidState(0, "Packager_package() shader output parent folder lookup failed"));
			}

			CharString leaf = CharString_createNull();
			CharString_cutBeforeLastSensitive(&outPath, '/', &leaf);

			if(!leaf.ptr)
				leaf = outPath;

			gotoIfError3(clean, CharString_createCopy(leaf, alloc, &leafCopy, e_rr));

			CAHandle handle = CAFile_addFile(&archive, parent, &leafCopy, 0, alloc, e_rr);

			if(handle == CAHandle_Invalid)
				retError(clean, Error_invalidState(0, "Packager_package() couldn't add shader output"));

			gotoIfError3(clean, CAFile_setData(&archive, handle, alloc, &allBuffers.ptrNonConst[i], e_rr));
		}

	#endif

	//Convert to CAFile and write to file.
	//
	//Serialized into memory first, so an unchanged package can leave the file alone.
	//Writing it unconditionally makes the output newer than everything else on every build, and a package
	//is an input to the binary that embeds it (an .rc entry on Windows, a linker section elsewhere), so a
	//build that changed nothing still relinked every executable carrying one, and re-ran every test that
	//hangs off those executables.
	//
	//This is only sound because packaging is byte deterministic; see Compiler_sortBinaries.

	gotoIfError3(clean, MemoryStream_create(
		0, EMemoryStreamFlags_WriteResize, &memStreamType, (MemoryStreamRef**) &stream, e_rr
	));

	U64 startOffset = 0;
	gotoIfError3(clean, CAFile_write(&archive, &encStreamType, stream, &startOffset, alloc, e_rr));
	gotoIfError3(clean, MemoryStream_move((MemoryStreamRef**) &stream, &packaged, e_rr));

	//An output that is already byte identical is left alone, timestamp included.
	//A failed read just means it gets rewritten, which is the old behaviour.

	Bool unchanged = false;

	if (File_hasFile(&settings->output, alloc))
		unchanged =
			File_read(&settings->output, 100 * MS, 0, 0, &fileHandleType, &previous, NULL) &&
			Buffer_eq(previous, packaged);

	if(!unchanged)
		gotoIfError3(clean, File_write(&packaged, &settings->output, 0, 0, 100 * MS, true, &fileHandleType, e_rr));

clean:
	if(caSettings.encryptionType)
		Buffer_clearAllSecure(Buffer_createRef(caSettings.encryptionKey, sizeof(caSettings.encryptionKey)));

	F64 dt = (F64)(Time_now() - start) / SECOND;

	if(settings && settings->enableLogging) {

		if(s_uccess)
			Log_debugLn(alloc, "-- Packaging %s success in %fs!", resolved.ptr, dt);

		else Log_errorLn(alloc, "-- Packaging %s failed in %fs!", resolved.ptr, dt);

		if(e_rr)
			Error_print(alloc, e_rr, ELogLevel_Error, ELogOptions_NewLine);
	}

	ListBuffer_freeUnderlying(&allBuffers, alloc);

	//The dirty lists hold refs into the all* lists, so the containers go but the strings do not.

	ListCharString_free(&dirtyFiles, alloc);
	ListCharString_free(&dirtyText, alloc);
	ListCharString_free(&dirtyOutputs, alloc);
	ListU8_free(&dirtyCompileOutputs, alloc);
	ListU64_free(&dirtyIndices, alloc);
	ListBuffer_freeUnderlying(&dirtyBuffers, alloc);
	ListU8_free(&reused, alloc);

	Buffer_free(&packaged, alloc);
	Buffer_free(&previous, alloc);

	CharString_free(&sidecarPath, alloc);
	CharString_free(&cachePath, alloc);
	CharString_free(&cacheRoot, alloc);
	ListCharString_freeUnderlying(&allFiles, alloc);
	ListCharString_freeUnderlying(&allShaderText, alloc);
	ListCharString_freeUnderlying(&allOutputs, alloc);
	ListCharString_free(&includeDirs, alloc);        //Elements are refs into settings->includeDir; free the list only
	ListU8_free(&allCompileOutputs, alloc);

	RefPtr_dec(&stream);
	CAFile_free(&archive, alloc);
	CharString_free(&resolved, alloc);
	CharString_free(&leafCopy, alloc);        //No-op on success (CAFile_addFile moved it); frees on error path

	return s_uccess;
}
