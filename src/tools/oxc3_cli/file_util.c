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

//tools/oxc3_cli/file_util.c
//Small cross-platform file utilities (ls/tree/stat/count/copy/move/del/mkdir/touch, cmp/wipe/hexdump/gmac).
//All of these transparently work on physical paths as well as virtual "//" (embedded VFS) paths, because File_*
//dispatches both.

#include "tools/oxc3_cli/cli.h"
#include "platforms/platform.h"
#include "platforms/file.h"
#include "platforms/logx.h"
#include "types/container/buffer.h"
#include "types/container/buffer_encrypt.h"
#include "types/container/stream.h"
#include "types/container/memory_stream.h"
#include "types/container/encryption_stream.h"
#include "types/container/string.h"
#include "formats/oiCA/ca_file.h"
#include "formats/oiCA/ca_lookup.h"
#include "formats/oiCA/ca_props.h"
#include "formats/oiCA/ca_edit.h"
#include "formats/oiCA/ca_headers.h"
#include "formats/oiDL/dl_file.h"
#include "formats/oiDL/dl_headers.h"
#include "types/math/vec4i.h"
#include "types/base/algorithm.h"
#include "types/base/string_read.h"
#include "types/base/string_read_helper.h"
#include "types/base/c8.h"
#include "types/base/time.h"
#include "types/base/error.h"
#include "types/base/mathi.h"
#include "types/base/constants.h"
#include "types/base/string_base.h"

//-aes-file reads the key via File_read, so it obeys the same working-directory sandbox as every other path argument.
//--aes-stdin reads one line from stdin; there's no File_ helper for a stdin stream, so fgets is used there (only).

#include <stdio.h>
#include <string.h>

//Streaming chunk size.
//Multiple of 64 so it's block-aligned for AES-GMAC's chunked AAD and 16-aligned for hexdump lines.

#define CLI_FILE_CHUNK (1024 * 1024)
#define CLI_STREAM_CACHE (64 * 1024)        //Stream cursor cache; independent of the (bypass-cache) read/write chunk size

//Helper: fetch an input path argument

static Bool CLI_fileArg(const ParsedArgs *args, EOperationHasParameter shift, CharString *out) {
	return ParsedArgs_getArg(args, shift, out, NULL);
}

//Decode a 32-byte AES-256 key from hex text (optionally "0x"-prefixed, exactly 64 hex chars).
//Writes 8 U32 (32 bytes) into outKey.
//The caller is responsible for wiping the source text.

static Bool CLI_aesDecodeHex(CharString hex, U32 outKey[8], Error *e_rr) {

	Bool s_uccess = true;

	const CharString ox = CharString_createRefCStrConst("0x");

	if(!CharString_isHex(hex))
		retError(clean, Error_invalidState(0, "AES key must be a hex key (32 bytes / 64 hex chars)."));

	const U64 off = CharString_startsWithStringInsensitive(&hex, &ox, 0) ? 2 : 0;

	if(CharString_length(hex) - off != 64)
		retError(clean, Error_invalidState(1, "AES key must be exactly 32 bytes (64 hex chars)."));

	for(U64 i = off; i + 1 < CharString_length(hex); ++i) {
		const U8 v0 = C8_hex(hex.ptr[i]);
		const U8 v1 = C8_hex(hex.ptr[++i]);
		*((U8*)outKey + ((i - off) >> 1)) = (U8)((v0 << 4) | v1);
	}

clean:
	return s_uccess;
}

//Trim leading and trailing ASCII whitespace (space/tab/CR/LF) from [ptr, ptr+len), returning the trimmed slice.

static CharString CLI_trimWhitespace(const C8 *ptr, U64 len) {

	U64 start = 0;

	while(start < len && (ptr[start] == ' ' || ptr[start] == '\t' || ptr[start] == '\r' || ptr[start] == '\n'))
		++start;

	while(len > start && (ptr[len - 1] == ' ' || ptr[len - 1] == '\t' || ptr[len - 1] == '\r' || ptr[len - 1] == '\n'))
		--len;

	return CharString_createRefSizedConst(ptr + start, len - start, false);
}

//Read the AES key from a file: either a raw 32-byte binary key, or the same hex form -aes accepts.
//Detection is by content length: exactly 32 bytes = raw, otherwise the (whitespace-trimmed) text is decoded as hex.

static Bool CLI_aesFromFile(CharString path, U32 outKey[8], Error *e_rr) {

	Bool s_uccess = true;
	const Allocator *alloc = Platform_instance->alloc;
	const RefPtrType fileHandleType = FileHandle_makeType(alloc);
	Buffer file = Buffer_createNull();

	//File_read applies the CLI's working-directory sandbox, same as any other path argument.

	gotoIfError3(clean, File_read(&path, U64_MAX, 0, 0, &fileHandleType, &file, e_rr));

	const U64 read = Buffer_length(file);

	//A raw key is exactly 32 bytes; anything else (hex, possibly with a trailing newline) is decoded as text.

	if(read == 32)
		Buffer_memcpy(Buffer_createRef(outKey, 32), file);

	else {
		const CharString hex = CLI_trimWhitespace((const C8*) file.ptr, read);
		gotoIfError3(clean, CLI_aesDecodeHex(hex, outKey, e_rr));
	}

clean:
	Buffer_clearAllSecure(file);        //file held key material
	Buffer_free(&file, alloc);
	return s_uccess;
}

//Read the AES key (hex) from a single line of stdin.

static Bool CLI_aesFromStdin(U32 outKey[8], Error *e_rr) {

	Bool s_uccess = true;
	C8 line[256];

	if(!fgets(line, (int) sizeof(line), stdin))
		retError(clean, Error_invalidState(0, "-aes-stdin: couldn't read a key line from stdin."));

	const CharString hex = CLI_trimWhitespace(line, (U64) CharString_calcStrLen(line, U64_MAX));
	gotoIfError3(clean, CLI_aesDecodeHex(hex, outKey, e_rr));

clean:
	Buffer_clearAllSecure(Buffer_createRef(line, sizeof(line)));        //line held key material
	return s_uccess;
}

//Shared AES-256 key fetch for every CLI operation that accepts a key.
//Accepts at most one of -aes / -aes-file / -aes-stdin; fills outKey and sets *hasKey when a source is present.

Bool CLI_getAesKey(const ParsedArgs *args, U32 outKey[8], Bool *hasKey, Error *e_rr) {

	Bool s_uccess = true;

	if(hasKey)
		*hasKey = false;

	if(!args)
		retError(clean, Error_nullPointer(0, "CLI_getAesKey()::args is required"));

	const Bool hasArg = (args->parameters & EOperationHasParameter_AES) != 0;
	const Bool hasFile = (args->parameters & EOperationHasParameter_AESFile) != 0;
	const Bool hasStdin = (args->flags & EOperationFlags_AESStdin) != 0;

	const U32 sources = (U32) hasArg + (U32) hasFile + (U32) hasStdin;

	if(!sources)
		goto clean;

	if(sources > 1)
		retError(clean, Error_invalidState(0, "Only one of -aes, -aes-file or --aes-stdin may be specified."));

	if(hasArg) {

		CharString keyStr = CharString_createNull();

		if(!CLI_fileArg(args, EOperationHasParameter_AESShift, &keyStr))
			retError(clean, Error_invalidState(1, "-aes requires a 32-byte hex key."));

		gotoIfError3(clean, CLI_aesDecodeHex(keyStr, outKey, e_rr));
	}

	else if(hasFile) {

		CharString path = CharString_createNull();

		if(!CLI_fileArg(args, EOperationHasParameter_AESFileShift, &path))
			retError(clean, Error_invalidState(2, "-aes-file requires a path."));

		gotoIfError3(clean, CLI_aesFromFile(path, outKey, e_rr));
	}

	else gotoIfError3(clean, CLI_aesFromStdin(outKey, e_rr));

	if(hasKey)
		*hasKey = true;

clean:
	if(!s_uccess)        //Never leave partially-decoded key material behind on failure
		Buffer_clearAllSecure(Buffer_createRef(outKey, 32));

	return s_uccess;
}

//ls / tree: print a single directory entry (used as a File_foreach callback)

static Bool CLI_filePrintEntry(const FileInfo *info, void *userData, const Allocator *alloc, Error *e_rr) {

	(void) userData; (void) alloc; (void) e_rr;

	TimeFormat tf = { 0 };
	Time_format(info->timestamp, tf, true);

	Log_debugLnx(
		"%s %12"PRIu64"  %s  %.*s",
		info->type == EFileType_Folder ? "<DIR>" : "     ",
		info->fileSize,
		tf,
		(int) CharString_length(info->path), info->path.ptr
	);

	return true;
}

//If the path is a virtual "//section/..." path, make sure the containing section is loaded before we walk/read it.
//(Sections are enumerated at startup but their archive contents only resolve once loaded.)
//The virtual root ("//" or "//.") lists all sections and needs no load; only a real "//section" does.
//Shared with inspect_header / inspect_data.

void CLI_ensureVirtualLoaded(const CharString *path) {

	if(!File_isVirtual(*path))
		return;

	const Allocator *alloc = Platform_instance->alloc;
	const U64 len = CharString_length(*path);

	//Section root is "//" followed by the first path component

	U64 slash = len;

	for(U64 i = 2; i < len; ++i)
		if(path->ptr[i] == '/') {
			slash = i;
			break;
		}

	//The virtual root ("//", "//.", "//./") has no specific section.
	//Rather than skip loading, load EVERY section so the root listing (file tree //) can enumerate them all.
	//Passing the root path to File_loadVirtual matches all sections since every section path starts with the (empty) root.

	const Bool isRoot = slash <= 2 || (slash == 3 && path->ptr[2] == '.');

	//The loaded section (and the memory stream backing it) outlives this call and stays in Platform_instance,
	// so the RefPtrType its RefPtr points at must outlive it too.
	//A local RefPtrType would dangle -> keep it program-lifetime.

	static RefPtrType memStreamType;
	static Bool memStreamTypeInit = false;

	if(!memStreamTypeInit) {
		memStreamType = MemoryStream_makeType(alloc);
		memStreamTypeInit = true;
	}

	const CharString section =
		isRoot ? CharString_createRefCStrConst("//") :
		CharString_createRefSizedConst(path->ptr, slash, false);

	Error err = Error_none();

	//File_loadVirtual is idempotent (already-loaded sections are skipped), so at root we just load unconditionally.

	if(isRoot || !File_isVirtualLoaded(&section, alloc, NULL))
		File_loadVirtual(&section, &memStreamType, NULL, NULL, alloc, &err);        //Best effort; the op reports real errors

	if(err.genericError)
		Error_print(alloc, &err, ELogLevel_Warn, ELogOptions_Default);
}

//--oiCA: run a file operation inside an oiCA archive (read it, act on its entries, rewrite it if mutated).

typedef enum EFileArchiveOp {
	EFileArchiveOp_List,
	EFileArchiveOp_Tree,
	EFileArchiveOp_Count,
	EFileArchiveOp_Stat,
	EFileArchiveOp_Delete,
	EFileArchiveOp_Copy
} EFileArchiveOp;

static Bool CLI_fileArchive(const ParsedArgs *args, EFileArchiveOp op) {

	const Allocator *alloc = Platform_instance->alloc;
	const RefPtrType fileHandleType = FileHandle_makeType(alloc);
	const RefPtrType streamType = FileStream_makeType(alloc);
	const RefPtrType memType = MemoryStream_makeType(alloc);
	const RefPtrType encType = EncryptionStream_makeType(alloc);

	CharString caPath = CharString_createNull(), input = CharString_createNull(), output = CharString_createNull();

	if(!CLI_fileArg(args, EOperationHasParameter_oiCAShift, &caPath)) {
		Log_errorLnx("Missing -oiCA <archive>.");
		return false;
	}

	const Bool hasInput = CLI_fileArg(args, EOperationHasParameter_InputShift, &input);
	const Bool hasOutput = CLI_fileArg(args, EOperationHasParameter_OutputShift, &output);

	Buffer outBuf = Buffer_createNull(), dataCopy = Buffer_createNull();
	CharString nameCopy = CharString_createNull();
	StreamRef *readStream = NULL, *writeStream = NULL, *dataStream = NULL;
	StreamCursor dataCursor = (StreamCursor) { 0 };
	CAFile caFile = (CAFile) { 0 };
	U32 keyV[8] = { 0 };
	Bool hasKey = false;
	Bool s_uccess = true, mutated = false;
	Error err = Error_none(), *e_rr = &err;

	//An encrypted archive needs its -aes key both to decrypt on read and to re-encrypt if we rewrite it (copy/del).
	//CAFile_read stores the key into caFile.settings, so CAFile_write re-encrypts automatically.

	gotoIfError3(clean, CLI_getAesKey(args, keyV, &hasKey, e_rr));

	//Read from the archive file via a stream so we never pull the whole (possibly huge) oiCA into memory.

	gotoIfError3(clean, File_openStream(
		&caPath, 1 * SECOND, EFileOpenType_Read, false, &fileHandleType, &streamType, &readStream, e_rr
	));
	gotoIfError3(clean, CAFile_read(readStream, &encType, 0, hasKey ? keyV : NULL, alloc, &caFile, e_rr));

	switch(op) {

		case EFileArchiveOp_List:
		case EFileArchiveOp_Tree:
			gotoIfError3(clean, CAFile_foreach(
				&caFile, CAHandle_Root, CLI_filePrintEntry, NULL, op == EFileArchiveOp_Tree, alloc, e_rr
			));
			break;

		case EFileArchiveOp_Count: {
			const U64 files = CAFile_fileCount(&caFile, CAHandle_Root, true);
			const U64 folders = CAFile_dirCount(&caFile, CAHandle_Root, true);
			Log_debugLnx(
				"%.*s: %"PRIu64" files, %"PRIu64" folders",
				(int) CharString_length(caPath), caPath.ptr, files, folders
			);
			break;
		}

		case EFileArchiveOp_Stat: {

			if(!hasInput)
				retError(clean, Error_invalidState(0, "file stat -oiCA requires -input <entry>."));

			const CAHandle h = CAFile_resolve(&caFile, input);

			if(h == CAHandle_Invalid)
				retError(clean, Error_notFound(0, 0, "file stat -oiCA: entry not found in archive."));

			FileInfo info = (FileInfo) { 0 };
			gotoIfError3(clean, CAFile_getInfo(&caFile, h, &info, alloc, e_rr));

			TimeFormat tf = { 0 };
			Time_format(info.timestamp, tf, true);

			Log_debugLnx(
				"%.*s\n\tType:      %s\n\tSize:      %"PRIu64" bytes\n\tModified:  %s",
				(int) CharString_length(info.path), info.path.ptr,
				info.type == EFileType_Folder ? "folder" : "file", info.fileSize, tf
			);

			FileInfo_free(&info, alloc);
			break;
		}

		case EFileArchiveOp_Delete: {

			if(!hasInput)
				retError(clean, Error_invalidState(0, "file del -oiCA requires -input <entry>."));

			const CAHandle h = CAFile_resolve(&caFile, input);

			if(h == CAHandle_Invalid)
				retError(clean, Error_notFound(0, 0, "file del -oiCA: entry not found in archive."));

			gotoIfError3(clean, CAFile_remove(&caFile, h, alloc, e_rr));
			mutated = true;
			break;
		}

		case EFileArchiveOp_Copy: {

			if(!hasInput || !hasOutput)
				retError(clean, Error_invalidState(0, "file copy -oiCA requires -input <src entry> and -output <dst name>."));

			const CAHandle src = CAFile_resolveFile(&caFile, input);

			if(src == CAHandle_Invalid)
				retError(clean, Error_notFound(0, 0, "file copy -oiCA: source file not found in archive."));

			//The source may be a loaded buffer or still backed by a stream (large / not-yet-loaded entries); handle both.

			Bool valid = false;
			const Buffer data = CAFile_getDataConst(&caFile, src, &valid);

			if(valid) {
				gotoIfError3(clean, Buffer_createCopy(data, alloc, &dataCopy, e_rr));
			}

			else {

				U64 streamOff = 0;
				dataStream = CAFile_getDataStream(&caFile, src, &streamOff);        //Increments the stream ref

				if(!dataStream || streamOff == U64_MAX)
					retError(clean, Error_invalidState(0, "file copy -oiCA: couldn't read source data."));

				const U64 srcSize = CAFile_fileSize(&caFile, src);
				gotoIfError3(clean, Buffer_createUninitializedBytes(srcSize, alloc, &dataCopy, e_rr));
				gotoIfError3(clean, StreamCursor_create(dataStream, CLI_STREAM_CACHE, false, alloc, &dataCursor, e_rr));
				gotoIfError3(clean, StreamCursor_read(&dataCursor, dataCopy, streamOff, 0, srcSize, true, alloc, e_rr));
			}

			gotoIfError3(clean, CharString_createCopy(output, alloc, &nameCopy, e_rr));

			const CAHandle dst = CAFile_addFile(&caFile, CAHandle_Root, &nameCopy, 0, alloc, e_rr);        //Moves nameCopy

			if(dst == CAHandle_Invalid)
				retError(clean, Error_invalidState(0, "file copy -oiCA: couldn't add destination file."));

			gotoIfError3(clean, CAFile_setData(&caFile, dst, alloc, &dataCopy, e_rr));        //Moves dataCopy
			mutated = true;
			break;
		}
	}

	//For a write operation we always produce the destination archive - even if nothing actually changed - so scripts
	//depending on the output file don't break. 'del' can target a separate archive via -output (leaving the source
	//intact); 'copy' uses -output as the destination entry name, so it rewrites -oiCA in place.

	const Bool isWriteOp = op == EFileArchiveOp_Copy || op == EFileArchiveOp_Delete;
	const CharString *dest = (op == EFileArchiveOp_Delete && hasOutput) ? &output : &caPath;
	const Bool inPlace = dest == &caPath;

	if(mutated) {

		U64 writeOff = 0;

		if(!inPlace) {

			//Separate destination: stream the new archive straight to the file, no full-archive buffer.

			gotoIfError3(clean, File_openStream(
				dest, 1 * SECOND, EFileOpenType_Write, true, &fileHandleType, &streamType, &writeStream, e_rr
			));
			gotoIfError3(clean, CAFile_write(&caFile, &encType, writeStream, &writeOff, alloc, e_rr));
		}

		else {

			//In-place: we can't stream-write the very file we're stream-reading, so buffer through a memory stream
			//(the fallback), then release the source handles and overwrite the file.

			gotoIfError3(clean, MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &memType, &writeStream, e_rr));
			gotoIfError3(clean, CAFile_write(&caFile, &encType, writeStream, &writeOff, alloc, e_rr));
			gotoIfError3(clean, MemoryStream_move(&writeStream, &outBuf, e_rr));

			CAFile_free(&caFile, alloc);
			caFile = (CAFile) { 0 };
			StreamCursor_close(&dataCursor, alloc);
			RefPtr_dec(&dataStream);
			RefPtr_dec(&readStream);

			gotoIfError3(clean, File_write(&outBuf, &caPath, 0, 0, 1 * SECOND, true, &fileHandleType, e_rr));
		}

		Log_debugLnx("Updated archive %.*s", (int) CharString_length(*dest), dest->ptr);
	}

	else if(isWriteOp && !inPlace) {

		//Nothing changed, but the write op targets a separate archive: copy the source across so the destination exists.
		//(In practice copy/del always mutate, so this is a rare safety net.)

		Buffer whole = Buffer_createNull();
		gotoIfError3(clean, File_read(&caPath, 1 * SECOND, 0, 0, &fileHandleType, &whole, e_rr));

		if(!File_write(&whole, dest, 0, 0, 1 * SECOND, true, &fileHandleType, e_rr))
			s_uccess = false;

		Buffer_free(&whole, alloc);

		if(!s_uccess)
			goto clean;

		Log_debugLnx("Wrote unchanged archive to %.*s", (int) CharString_length(*dest), dest->ptr);
	}

clean:
	if(err.genericError)
		Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);

	StreamCursor_close(&dataCursor, alloc);
	CAFile_free(&caFile, alloc);
	RefPtr_dec(&readStream);
	RefPtr_dec(&writeStream);
	RefPtr_dec(&dataStream);
	Buffer_free(&outBuf, alloc);
	Buffer_free(&dataCopy, alloc);        //No-op if it was moved into the archive
	CharString_free(&nameCopy, alloc);    //No-op if it was moved into the archive
	return s_uccess;
}

static Bool CLI_fileListImpl(const ParsedArgs *args, Bool isRecursive) {

	if(!args)
		return false;

	const Allocator *alloc = Platform_instance->alloc;

	CharString input = CharString_createNull();
	if(!CLI_fileArg(args, EOperationHasParameter_InputShift, &input))
		input = CharString_createRefCStrConst(".");        //Default to the working directory

	CLI_ensureVirtualLoaded(&input);
	return File_foreach(&input, false, CLI_filePrintEntry, NULL, isRecursive, alloc, NULL);
}

Bool CLI_fileList(const ParsedArgs *args) {
	if(args && (args->parameters & EOperationHasParameter_oiCA)) return CLI_fileArchive(args, EFileArchiveOp_List);
	return CLI_fileListImpl(args, false);
}

Bool CLI_fileTree(const ParsedArgs *args) {
	if(args && (args->parameters & EOperationHasParameter_oiCA)) return CLI_fileArchive(args, EFileArchiveOp_Tree);
	return CLI_fileListImpl(args, true);
}

//stat

Bool CLI_fileStat(const ParsedArgs *args) {

	if(!args)
		return false;

	if(args->parameters & EOperationHasParameter_oiCA)
		return CLI_fileArchive(args, EFileArchiveOp_Stat);

	const Allocator *alloc = Platform_instance->alloc;

	CharString input = CharString_createNull();
	if(!CLI_fileArg(args, EOperationHasParameter_InputShift, &input)) {
		Log_errorLnx("Invalid argument -input <path>.");
		return false;
	}

	CLI_ensureVirtualLoaded(&input);

	FileInfo info = (FileInfo) { 0 };
	Bool s_uccess = true;
	Error err = Error_none(), *e_rr = &err;

	gotoIfError3(clean, File_getInfo(&input, &info, alloc, e_rr));

	TimeFormat tf = { 0 };
	Time_format(info.timestamp, tf, true);

	const C8 *access = info.access == EFileAccess_ReadWrite ? "read/write" : (
		info.access == EFileAccess_Write ? "write" : (info.access == EFileAccess_Read ? "read" : "none")
	);

	Log_debugLnx(
		"%.*s\n\tType:      %s\n\tSize:      %"PRIu64" bytes\n\tAccess:    %s\n\tModified:  %s",
		(int) CharString_length(info.path), info.path.ptr,
		info.type == EFileType_Folder ? "folder" : "file",
		info.fileSize,
		access,
		tf
	);

clean:
	if(err.genericError)
		Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);

	FileInfo_free(&info, alloc);
	return s_uccess;
}

//count

Bool CLI_fileCount(const ParsedArgs *args) {

	if(!args) return false;

	if(args->parameters & EOperationHasParameter_oiCA)
		return CLI_fileArchive(args, EFileArchiveOp_Count);

	const Allocator *alloc = Platform_instance->alloc;

	CharString input = CharString_createNull();
	if(!CLI_fileArg(args, EOperationHasParameter_InputShift, &input))
		input = CharString_createRefCStrConst(".");        //Default to the working directory

	CLI_ensureVirtualLoaded(&input);

	const Bool recursive = !(args->flags & EOperationFlags_NonRecursive);

	U64 files = 0, folders = 0;
	Bool s_uccess = true;
	Error err = Error_none(), *e_rr = &err;

	gotoIfError3(clean, File_queryFileCount(&input, recursive, &files, alloc, e_rr));
	gotoIfError3(clean, File_queryFolderCount(&input, recursive, &folders, alloc, e_rr));

	Log_debugLnx(
		"%.*s: %"PRIu64" files, %"PRIu64" folders (%s)",
		(int) CharString_length(input), input.ptr, files, folders, recursive ? "recursive" : "non-recursive"
	);

clean:
	if(err.genericError)
		Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);

	return s_uccess;
}

//copy

Bool CLI_fileCopy(const ParsedArgs *args) {

	if(!args) return false;

	if(args->parameters & EOperationHasParameter_oiCA)
		return CLI_fileArchive(args, EFileArchiveOp_Copy);

	const Allocator *alloc = Platform_instance->alloc;
	RefPtrType fileHandleType = FileHandle_makeType(alloc);
	RefPtrType streamType = FileStream_makeType(alloc);

	CharString input = CharString_createNull(), output = CharString_createNull();
	if(
		!CLI_fileArg(args, EOperationHasParameter_InputShift, &input) ||
		!CLI_fileArg(args, EOperationHasParameter_OutputShift, &output)
	) {
		Log_errorLnx("file copy requires -input <src> and -output <dst>.");
		return false;
	}

	CLI_ensureVirtualLoaded(&input);

	if(File_hasFolder(&input, alloc)) {
		Log_errorLnx("file copy currently supports files only (not folders).");
		return false;
	}

	//File_open(Stream) isn't supported on virtual files,
	// so read the (in-memory, embedded) file into a buffer and write it out.
	//Virtual files can't be huge (they're embedded), so buffering here is fine.

	if(File_isVirtual(input)) {

		Buffer vbuf = Buffer_createNull();
		Error verr = Error_none();

		const Bool ok =
			File_read(&input, 1 * SECOND, 0, 0, &fileHandleType, &vbuf, &verr) &&
			File_write(&vbuf, &output, 0, 0, 1 * SECOND, true, &fileHandleType, &verr);

		if(ok)
			Log_debugLnx(
				"Copied %"PRIu64" bytes: %.*s -> %.*s", Buffer_length(vbuf),
				(int) CharString_length(input), input.ptr, (int) CharString_length(output), output.ptr
			);

		else Error_print(alloc, &verr, ELogLevel_Error, ELogOptions_Default);

		Buffer_free(&vbuf, alloc);
		return ok;
	}

	StreamRef *inStream = NULL, *outStream = NULL;
	StreamCursor inCur = (StreamCursor) { 0 }, outCur = (StreamCursor) { 0 };
	Bool s_uccess = true;
	Error err = Error_none(), *e_rr = &err;

	gotoIfError3(clean, File_openStream(
		&input, 1 * SECOND, EFileOpenType_Read, false, &fileHandleType, &streamType, &inStream, e_rr
	));
	gotoIfError3(clean, File_openStream(
		&output, 1 * SECOND, EFileOpenType_Write, true, &fileHandleType, &streamType, &outStream, e_rr
	));

	gotoIfError3(clean, StreamCursor_create(inStream, CLI_STREAM_CACHE, false, alloc, &inCur, e_rr));
	gotoIfError3(clean, StreamCursor_create(outStream, CLI_STREAM_CACHE, true, alloc, &outCur, e_rr));

	gotoIfError3(clean, StreamCursor_copyStream(&outCur, &inCur, 0, 0, 0, alloc, e_rr));        //length 0 = all remaining
	gotoIfError3(clean, StreamCursor_flush(&outCur, alloc, e_rr));

	Log_debugLnx(
		"Copied %.*s -> %.*s",
		(int) CharString_length(input), input.ptr, (int) CharString_length(output), output.ptr
	);

clean:
	if(err.genericError)
		Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);

	StreamCursor_close(&inCur, alloc);
	StreamCursor_close(&outCur, alloc);
	RefPtr_dec(&inStream);
	RefPtr_dec(&outStream);
	return s_uccess;
}

//move (into a directory)

Bool CLI_fileMove(const ParsedArgs *args) {

	if(!args) return false;

	const Allocator *alloc = Platform_instance->alloc;

	CharString input = CharString_createNull(), output = CharString_createNull();
	if(
		!CLI_fileArg(args, EOperationHasParameter_InputShift, &input) ||
		!CLI_fileArg(args, EOperationHasParameter_OutputShift, &output)
	) {
		Log_errorLnx("file move requires -input <src> and -output <destination directory>.");
		return false;
	}

	Bool s_uccess = true;
	Error err = Error_none(), *e_rr = &err;

	gotoIfError3(clean, File_move(&input, &output, 1 * SECOND, alloc, e_rr));

	Log_debugLnx(
		"Moved %.*s -> %.*s",
		(int) CharString_length(input), input.ptr, (int) CharString_length(output), output.ptr
	);

clean:
	if(err.genericError)
		Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);

	return s_uccess;
}

//del

Bool CLI_fileDelete(const ParsedArgs *args) {

	if(!args) return false;

	if(args->parameters & EOperationHasParameter_oiCA)
		return CLI_fileArchive(args, EFileArchiveOp_Delete);

	const Allocator *alloc = Platform_instance->alloc;

	CharString input = CharString_createNull();
	if(!CLI_fileArg(args, EOperationHasParameter_InputShift, &input)) {
		Log_errorLnx("Invalid argument -input <path>.");
		return false;
	}

	Bool s_uccess = true;
	Error err = Error_none(), *e_rr = &err;

	gotoIfError3(clean, File_remove(&input, 1 * SECOND, alloc, e_rr));

	Log_debugLnx("Deleted %.*s", (int) CharString_length(input), input.ptr);

clean:
	if(err.genericError)
		Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);

	return s_uccess;
}

//mkdir / touch

static Bool CLI_fileAddImpl(const ParsedArgs *args, EFileType type) {

	if(!args) return false;

	const Allocator *alloc = Platform_instance->alloc;

	CharString input = CharString_createNull();
	if(!CLI_fileArg(args, EOperationHasParameter_InputShift, &input)) {
		Log_errorLnx("Invalid argument -input <path>.");
		return false;
	}

	Bool s_uccess = true;
	Error err = Error_none(), *e_rr = &err;

	gotoIfError3(clean, File_add(&input, type, false, alloc, e_rr));

	Log_debugLnx(
		"Created %s %.*s", type == EFileType_Folder ? "folder" : "file", (int) CharString_length(input), input.ptr
	);

clean:
	if(err.genericError)
		Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);

	return s_uccess;
}

Bool CLI_fileMkdir(const ParsedArgs *args) { return CLI_fileAddImpl(args, EFileType_Folder); }
Bool CLI_fileTouch(const ParsedArgs *args) { return CLI_fileAddImpl(args, EFileType_File); }

//cmp (byte compare of two files)

Bool CLI_fileCmp(const ParsedArgs *args) {

	if(!args) return false;

	const Allocator *alloc = Platform_instance->alloc;
	RefPtrType fileHandleType = FileHandle_makeType(alloc);
	RefPtrType streamType = FileStream_makeType(alloc);

	CharString a = CharString_createNull(), b = CharString_createNull();
	if(
		!CLI_fileArg(args, EOperationHasParameter_InputShift, &a) ||
		!CLI_fileArg(args, EOperationHasParameter_Input2Shift, &b)
	) {
		Log_errorLnx("file cmp requires -input <a> and -input2 <b>.");
		return false;
	}

	FileInfo infoA = (FileInfo) { 0 }, infoB = (FileInfo) { 0 };
	StreamRef *sa = NULL, *sb = NULL;
	StreamCursor ca = (StreamCursor) { 0 }, cb = (StreamCursor) { 0 };
	Buffer chunkA = Buffer_createNull(), chunkB = Buffer_createNull();
	Bool s_uccess = true;
	Error err = Error_none(), *e_rr = &err;

	gotoIfError3(clean, File_getInfo(&a, &infoA, alloc, e_rr));
	gotoIfError3(clean, File_getInfo(&b, &infoB, alloc, e_rr));

	const U64 lenA = infoA.fileSize, lenB = infoB.fileSize;
	const U64 minLen = U64_min(lenA, lenB);

	gotoIfError3(clean, File_openStream(&a, 1 * SECOND, EFileOpenType_Read, false, &fileHandleType, &streamType, &sa, e_rr));
	gotoIfError3(clean, File_openStream(&b, 1 * SECOND, EFileOpenType_Read, false, &fileHandleType, &streamType, &sb, e_rr));
	gotoIfError3(clean, StreamCursor_create(sa, CLI_STREAM_CACHE, false, alloc, &ca, e_rr));
	gotoIfError3(clean, StreamCursor_create(sb, CLI_STREAM_CACHE, false, alloc, &cb, e_rr));

	gotoIfError3(clean, Buffer_createUninitializedBytes(CLI_FILE_CHUNK, alloc, &chunkA, e_rr));
	gotoIfError3(clean, Buffer_createUninitializedBytes(CLI_FILE_CHUNK, alloc, &chunkB, e_rr));

	U64 firstDiff = U64_MAX;

	for(U64 off = 0; off < minLen && firstDiff == U64_MAX; off += CLI_FILE_CHUNK) {

		const U64 n = U64_min((U64) CLI_FILE_CHUNK, minLen - off);

		gotoIfError3(clean, StreamCursor_read(&ca, chunkA, off, 0, n, true, alloc, e_rr));
		gotoIfError3(clean, StreamCursor_read(&cb, chunkB, off, 0, n, true, alloc, e_rr));

		for(U64 i = 0; i < n; ++i)
			if(chunkA.ptr[i] != chunkB.ptr[i]) {
				firstDiff = off + i;
				break;
			}
	}

	if(firstDiff == U64_MAX && lenA == lenB)
		Log_debugLnx("Files are identical (%"PRIu64" bytes).", lenA);

	else if(firstDiff == U64_MAX)
		Log_debugLnx(
			"Files share the first %"PRIu64" bytes but differ in length (%"PRIu64" vs %"PRIu64").", minLen, lenA, lenB
		);

	else Log_debugLnx(
		"Files differ at byte 0x%"PRIx64" (%"PRIu64"): 0x%02x vs 0x%02x.",
		firstDiff, firstDiff, chunkA.ptr[firstDiff % CLI_FILE_CHUNK], chunkB.ptr[firstDiff % CLI_FILE_CHUNK]
	);

clean:
	if(err.genericError)
		Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);

	Buffer_free(&chunkA, alloc);
	Buffer_free(&chunkB, alloc);
	StreamCursor_close(&ca, alloc);
	StreamCursor_close(&cb, alloc);
	RefPtr_dec(&sa);
	RefPtr_dec(&sb);
	FileInfo_free(&infoA, alloc);
	FileInfo_free(&infoB, alloc);
	return s_uccess;
}

//file diff: structural comparison of two archives (oiCA or oiDL), reporting added / removed / modified entries.

//Read the first U32 (magic number) of a file, to detect the archive format.

static Bool CLI_peekMagic(
	const CharString *path,
	const RefPtrType *fileHandleType,
	const RefPtrType *streamType,
	const Allocator *alloc,
	U32 *magic,
	Error *e_rr
) {
	Bool s_uccess = true;
	StreamRef *s = NULL;
	StreamCursor c = (StreamCursor) { 0 };

	*magic = 0;

	gotoIfError3(clean, File_openStream(path, 1 * SECOND, EFileOpenType_Read, false, fileHandleType, streamType, &s, e_rr));
	gotoIfError3(clean, StreamCursor_create(s, CLI_STREAM_CACHE, false, alloc, &c, e_rr));
	gotoIfError3(clean, StreamCursor_read(&c, Buffer_createRef(magic, sizeof(U32)), 0, 0, sizeof(U32), true, alloc, e_rr));

clean:
	StreamCursor_close(&c, alloc);
	RefPtr_dec(&s);
	return s_uccess;
}

//Read an oiCA entry's bytes (loaded buffer or stream-backed) into out as an owned copy.

static Bool CLI_caReadEntry(const CAFile *ca, CAHandle h, const Allocator *alloc, Buffer *out, Error *e_rr) {

	Bool s_uccess = true;
	StreamRef *ds = NULL;
	StreamCursor dc = (StreamCursor) { 0 };

	Bool valid = false;
	const Buffer data = CAFile_getDataConst(ca, h, &valid);

	if(valid) {
		gotoIfError3(clean, Buffer_createCopy(data, alloc, out, e_rr));
		goto clean;
	}

	U64 streamOff = 0;
	ds = CAFile_getDataStream(ca, h, &streamOff);

	if(!ds || streamOff == U64_MAX)
		retError(clean, Error_invalidState(0, "file diff: couldn't read archive entry data."));

	const U64 size = CAFile_fileSize(ca, h);
	gotoIfError3(clean, Buffer_createUninitializedBytes(size, alloc, out, e_rr));
	gotoIfError3(clean, StreamCursor_create(ds, CLI_STREAM_CACHE, false, alloc, &dc, e_rr));
	gotoIfError3(clean, StreamCursor_read(&dc, *out, streamOff, 0, size, true, alloc, e_rr));

clean:
	StreamCursor_close(&dc, alloc);
	RefPtr_dec(&ds);
	return s_uccess;
}

typedef struct CLIDiffState {
	const CAFile *a, *b;
	U64 added, removed, modified, typeChanged, unchanged;
} CLIDiffState;

//A -> B: classify each entry of A as removed / type-changed / modified / unchanged.

static Bool CLI_diffEntryAB(const FileInfo *info, void *userData, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;
	CLIDiffState *st = (CLIDiffState*) userData;
	FileInfo infoB = (FileInfo) { 0 };
	Buffer da = Buffer_createNull(), db = Buffer_createNull();

	const CAHandle hb = CAFile_resolve(st->b, info->path);

	if(hb == CAHandle_Invalid) {
		Log_debugLnx("  - %.*s", (int) CharString_length(info->path), info->path.ptr);
		++st->removed;
		goto clean;
	}

	gotoIfError3(clean, CAFile_getInfo(st->b, hb, &infoB, alloc, e_rr));

	if(infoB.type != info->type) {
		Log_debugLnx("  ! %.*s (type changed)", (int) CharString_length(info->path), info->path.ptr);
		++st->typeChanged;
		goto clean;
	}

	if(info->type == EFileType_Folder)        //Folders carry no content to compare
		goto clean;

	if(info->fileSize != infoB.fileSize) {
		Log_debugLnx(
			"  ~ %.*s (%"PRIu64" -> %"PRIu64" bytes)",
			(int) CharString_length(info->path), info->path.ptr, info->fileSize, infoB.fileSize
		);
		++st->modified;
		goto clean;
	}

	//Equal size: compare content byte-for-byte.

	const CAHandle ha = CAFile_resolveFile(st->a, info->path);
	gotoIfError3(clean, CLI_caReadEntry(st->a, ha, alloc, &da, e_rr));
	gotoIfError3(clean, CLI_caReadEntry(st->b, hb, alloc, &db, e_rr));

	if(!Buffer_eq(da, db)) {
		Log_debugLnx(
			"  ~ %.*s (%"PRIu64" bytes, content differs)",
			(int) CharString_length(info->path), info->path.ptr, info->fileSize
		);
		++st->modified;
	}

	else ++st->unchanged;

clean:
	FileInfo_free(&infoB, alloc);
	Buffer_free(&da, alloc);
	Buffer_free(&db, alloc);
	return s_uccess;
}

//B -> A: report entries present only in B as added.

static Bool CLI_diffEntryBA(const FileInfo *info, void *userData, const Allocator *alloc, Error *e_rr) {

	(void) alloc; (void) e_rr;
	CLIDiffState *st = (CLIDiffState*) userData;

	if(CAFile_resolve(st->a, info->path) == CAHandle_Invalid) {
		Log_debugLnx("  + %.*s", (int) CharString_length(info->path), info->path.ptr);
		++st->added;
	}

	return true;
}

Bool CLI_fileDiff(const ParsedArgs *args) {

	if(!args) return false;

	const Allocator *alloc = Platform_instance->alloc;
	const RefPtrType fileHandleType = FileHandle_makeType(alloc);
	const RefPtrType streamType = FileStream_makeType(alloc);

	CharString a = CharString_createNull(), b = CharString_createNull();

	if(
		!CLI_fileArg(args, EOperationHasParameter_InputShift, &a) ||
		!CLI_fileArg(args, EOperationHasParameter_Input2Shift, &b)
	) {
		Log_errorLnx("file diff requires -input <a> and -input2 <b> (two oiCA or two oiDL archives).");
		return false;
	}

	StreamRef *sa = NULL, *sb = NULL;
	CAFile caA = (CAFile) { 0 }, caB = (CAFile) { 0 };
	DLFile dlA = (DLFile) { 0 }, dlB = (DLFile) { 0 };
	Bool s_uccess = true;
	Error err = Error_none(), *e_rr = &err;

	//Detect format from the magic number.
	//Both inputs must be the same archive type.

	U32 magicA = 0, magicB = 0;
	gotoIfError3(clean, CLI_peekMagic(&a, &fileHandleType, &streamType, alloc, &magicA, e_rr));
	gotoIfError3(clean, CLI_peekMagic(&b, &fileHandleType, &streamType, alloc, &magicB, e_rr));

	if(magicA != magicB)
		retError(clean, Error_invalidParameter(
			0, 0, "file diff: inputs are different formats (both must be oiCA or both oiDL)."
		));

	gotoIfError3(clean, File_openStream(&a, 1 * SECOND, EFileOpenType_Read, false, &fileHandleType, &streamType, &sa, e_rr));
	gotoIfError3(clean, File_openStream(&b, 1 * SECOND, EFileOpenType_Read, false, &fileHandleType, &streamType, &sb, e_rr));

	if(magicA == CAHeader_MAGIC) {

		gotoIfError3(clean, CAFile_read(sa, NULL, 0, NULL, alloc, &caA, e_rr));
		gotoIfError3(clean, CAFile_read(sb, NULL, 0, NULL, alloc, &caB, e_rr));

		CLIDiffState st = (CLIDiffState) { .a = &caA, .b = &caB };

		Log_debugLnx(
			"Diff %.*s -> %.*s (oiCA):",
			(int) CharString_length(a), a.ptr, (int) CharString_length(b), b.ptr
		);

		gotoIfError3(clean, CAFile_foreach(&caA, CAHandle_Root, CLI_diffEntryAB, &st, true, alloc, e_rr));
		gotoIfError3(clean, CAFile_foreach(&caB, CAHandle_Root, CLI_diffEntryBA, &st, true, alloc, e_rr));

		Log_debugLnx(
			"%"PRIu64" added, %"PRIu64" removed, %"PRIu64" modified, %"PRIu64" type-changed, %"PRIu64" unchanged.",
			st.added, st.removed, st.modified, st.typeChanged, st.unchanged
		);
	}

	else if(magicA == DLHeader_MAGIC) {

		U64 offA = 0, offB = 0;
		gotoIfError3(clean, DLFile_read(sa, &offA, NULL, I32x4_zero(), false, false, alloc, NULL, &dlA, e_rr));
		gotoIfError3(clean, DLFile_read(sb, &offB, NULL, I32x4_zero(), false, false, alloc, NULL, &dlB, e_rr));

		const U64 na = dlA.entryStreams.length, nb = dlB.entryStreams.length;
		const U64 common = U64_min(na, nb);
		U64 modified = 0, unchanged = 0;

		Log_debugLnx(
			"Diff %.*s -> %.*s (oiDL, %"PRIu64" vs %"PRIu64" entries):",
			(int) CharString_length(a), a.ptr, (int) CharString_length(b), b.ptr, na, nb
		);

		for(U64 i = 0; i < common; ++i) {

			const U64 sizeA = DLFile_entrySize(&dlA, i), sizeB = DLFile_entrySize(&dlB, i);

			if(sizeA != sizeB) {
				Log_debugLnx("  ~ entry[%"PRIu64"] (%"PRIu64" -> %"PRIu64" bytes)", i, sizeA, sizeB);
				++modified;
				continue;
			}

			//Equal size: compare content when both entries are fully loaded (small entries are).

			if(!DLFile_isFullyLoaded(&dlA, i) || !DLFile_isFullyLoaded(&dlB, i)) {
				++unchanged;        //Can't cheaply compare stream-backed entries of equal size
				continue;
			}

			Bool eq =
				dlA.settings.dataType == EDLDataType_String ?
				CharString_equalsStringSensitive(&dlA.entryStrings.ptr[i], &dlB.entryStrings.ptr[i]) :
				Buffer_eq(dlA.entryBuffers.ptr[i], dlB.entryBuffers.ptr[i]);

			if(!eq) {
				Log_debugLnx("  ~ entry[%"PRIu64"] (%"PRIu64" bytes, content differs)", i, sizeA);
				++modified;
			}

			else ++unchanged;
		}

		if(nb > na)
			Log_debugLnx("  + entries [%"PRIu64"..%"PRIu64"] only in second (added)", na, nb - 1);

		if(na > nb)
			Log_debugLnx("  - entries [%"PRIu64"..%"PRIu64"] only in first (removed)", nb, na - 1);

		Log_debugLnx(
			"%"PRIu64" added, %"PRIu64" removed, %"PRIu64" modified, %"PRIu64" unchanged.",
			nb > na ? nb - na : 0, na > nb ? na - nb : 0, modified, unchanged
		);
	}

	else retError(clean, Error_invalidParameter(0, 0, "file diff: inputs aren't oiCA or oiDL archives."));

clean:
	if(err.genericError)
		Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);

	CAFile_free(&caA, alloc);
	CAFile_free(&caB, alloc);
	DLFile_free(&dlA, alloc);
	DLFile_free(&dlB, alloc);
	RefPtr_dec(&sa);
	RefPtr_dec(&sb);
	return s_uccess;
}

//wipe (overwrite a file's contents with zeros)

Bool CLI_fileWipe(const ParsedArgs *args) {

	if(!args) return false;

	const Allocator *alloc = Platform_instance->alloc;
	const RefPtrType fileHandleType = FileHandle_makeType(alloc);

	CharString input = CharString_createNull();
	if(!CLI_fileArg(args, EOperationHasParameter_InputShift, &input)) {
		Log_errorLnx("Invalid argument -input <path>.");
		return false;
	}

	RefPtrType streamType = FileStream_makeType(alloc);

	FileInfo info = (FileInfo) { 0 };
	Buffer zero = Buffer_createNull();
	StreamRef *stream = NULL;
	StreamCursor cur = (StreamCursor) { 0 };
	Bool s_uccess = true;
	Error err = Error_none(), *e_rr = &err;

	gotoIfError3(clean, File_getInfo(&input, &info, alloc, e_rr));

	if(info.type != EFileType_File)
		retError(clean, Error_invalidState(0, "file wipe only works on files."));

	const U64 size = info.fileSize;

	if(size) {

		//Overwrite the existing bytes in place (ReadWrite, no truncation) a chunk at a time

		gotoIfError3(clean, Buffer_createUninitializedBytes(U64_min((U64) CLI_FILE_CHUNK, size), alloc, &zero, e_rr));
		gotoIfError3(clean, Buffer_unsetAllBits(zero, e_rr));

		gotoIfError3(clean, File_openStream(
			&input, 1 * SECOND, EFileOpenType_ReadWrite, false, &fileHandleType, &streamType, &stream, e_rr
		));
		gotoIfError3(clean, StreamCursor_create(stream, CLI_STREAM_CACHE, true, alloc, &cur, e_rr));

		for(U64 off = 0; off < size; off += CLI_FILE_CHUNK) {
			const U64 n = U64_min((U64) CLI_FILE_CHUNK, size - off);
			gotoIfError3(clean, StreamCursor_write(&cur, zero, 0, off, n, true, alloc, e_rr));
		}

		gotoIfError3(clean, StreamCursor_flush(&cur, alloc, e_rr));
	}

	Log_debugLnx("Wiped %"PRIu64" bytes of %.*s", size, (int) CharString_length(input), input.ptr);

clean:
	if(err.genericError)
		Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);

	StreamCursor_close(&cur, alloc);
	RefPtr_dec(&stream);
	Buffer_free(&zero, alloc);
	FileInfo_free(&info, alloc);
	return s_uccess;
}

//hexdump

Bool CLI_fileHexdump(const ParsedArgs *args) {

	if(!args) return false;

	const Allocator *alloc = Platform_instance->alloc;
	const RefPtrType fileHandleType = FileHandle_makeType(alloc);

	CharString input = CharString_createNull();
	if(!CLI_fileArg(args, EOperationHasParameter_InputShift, &input)) {
		Log_errorLnx("Invalid argument -input <path>.");
		return false;
	}

	RefPtrType streamType = FileStream_makeType(alloc);

	Buffer buf = Buffer_createNull();
	StreamRef *stream = NULL;
	StreamCursor cur = (StreamCursor) { 0 };
	Bool s_uccess = true;
	Error err = Error_none(), *e_rr = &err;

	//Optional -start / -length.
	//Default dumps up to 2KiB; everything is clamped to the file size.

	U64 start = 0, length = 0;
	const Bool hasLength = (args->parameters & EOperationHasParameter_Length) != 0;
	CharString tmp = CharString_createNull();

	if((args->parameters & EOperationHasParameter_StartOffset) && CLI_fileArg(args, EOperationHasParameter_StartOffsetShift, &tmp))
		if(!CharString_parseDec(tmp, &start))
			start = 0;

	if(hasLength && CLI_fileArg(args, EOperationHasParameter_LengthShift, &tmp))
		if(!CharString_parseDec(tmp, &length))
			length = 0;

	FileInfo info = (FileInfo) { 0 };
	gotoIfError3(clean, File_getInfo(&input, &info, alloc, e_rr));
	const U64 fileSize = info.fileSize;
	FileInfo_free(&info, alloc);

	if(start > fileSize)
		start = fileSize;

	const U64 avail = fileSize - start;

	if(!hasLength)
		length = avail < 2 * KIBI ? avail : 2 * KIBI;
	else if(length > avail)
		length = avail;

	if(!length) {
		Log_debugLnx("(no data at offset %"PRIu64")", start);
		goto clean;
	}

	gotoIfError3(clean, File_openStream(
		&input, 1 * SECOND, EFileOpenType_Read, false, &fileHandleType, &streamType, &stream, e_rr
	));
	gotoIfError3(clean, StreamCursor_create(stream, CLI_STREAM_CACHE, false, alloc, &cur, e_rr));
	gotoIfError3(clean, Buffer_createUninitializedBytes(CLI_FILE_CHUNK, alloc, &buf, e_rr));

	for(U64 chunkOff = 0; chunkOff < length; chunkOff += CLI_FILE_CHUNK) {

		const U64 n = U64_min((U64) CLI_FILE_CHUNK, length - chunkOff);
		gotoIfError3(clean, StreamCursor_read(&cur, buf, start + chunkOff, 0, n, true, alloc, e_rr));

		const U8 *ptr = buf.ptr;

		for(U64 i = 0; i < n; i += 16) {

			C8 line[80];
			U64 c = 0;

			//Offset

			for(int shift = 28; shift >= 0; shift -= 4)
				line[c++] = C8_createHex((U8)((start + chunkOff + i) >> shift) & 0xF);

			line[c++] = ':';
			line[c++] = ' ';

			//Hex bytes

			for(U64 j = 0; j < 16; ++j) {
				if(i + j < n) {
					line[c++] = C8_createHex(ptr[i + j] >> 4);
					line[c++] = C8_createHex(ptr[i + j] & 0xF);
				}
				else { line[c++] = ' '; line[c++] = ' '; }
				line[c++] = ' ';
			}

			line[c++] = ' ';

			//Ascii

			for(U64 j = 0; j < 16 && i + j < n; ++j) {
				const U8 v = ptr[i + j];
				line[c++] = (v >= 0x20 && v < 0x7F) ? (C8) v : '.';
			}

			line[c] = '\0';
			Log_debugLnx("%s", line);
		}
	}

	Log_debugLnx("(%"PRIu64" bytes from offset %"PRIu64")", length, start);

clean:
	if(err.genericError)
		Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);

	StreamCursor_close(&cur, alloc);
	RefPtr_dec(&stream);
	Buffer_free(&buf, alloc);
	return s_uccess;
}

//gmac (AES-GMAC: authenticate a file with AES-GCM using the file as additional data and empty plaintext)

Bool CLI_fileGmac(const ParsedArgs *args) {

	if(!args) return false;

	const Allocator *alloc = Platform_instance->alloc;
	const RefPtrType fileHandleType = FileHandle_makeType(alloc);

	CharString input = CharString_createNull();
	if(!CLI_fileArg(args, EOperationHasParameter_InputShift, &input)) {
		Log_errorLnx("file gmac requires -input <file>.");
		return false;
	}

	RefPtrType streamType = FileStream_makeType(alloc);

	Buffer buf = Buffer_createNull();
	StreamRef *stream = NULL;
	StreamCursor cur = (StreamCursor) { 0 };
	FileInfo info = (FileInfo) { 0 };
	AESEncryptionContext ctx = (AESEncryptionContext) { 0 };
	U32 keyV[8] = { 0 };
	Bool hasKey = false;
	Bool s_uccess = true;
	Error err = Error_none(), *e_rr = &err;

	//Fetch the 32-byte (256-bit) key from -aes / -aes-file / -aes-stdin (a key is required for gmac).

	gotoIfError3(clean, CLI_getAesKey(args, keyV, &hasKey, e_rr));

	if(!hasKey)
		retError(clean, Error_invalidState(0, "file gmac requires a key via -aes, -aes-file or --aes-stdin."));

	//Stream the whole file as GCM additional-data with a fixed zero IV, so the tag is a deterministic MAC (GMAC).

	gotoIfError3(clean, File_getInfo(&input, &info, alloc, e_rr));
	const U64 size = info.fileSize;

	AESEncryptionKey aesKey = (AESEncryptionKey) { 0 };
	for(U8 i = 0; i < 8; ++i)
		aesKey.u32x8[i] = keyV[i];

	U8 blockSizeMax = 0, use256Or512 = 0;

	gotoIfError3(clean, Buffer_aesExpertCreate(
		I32x4_zero(), EBufferEncryptionType_AES256GCM, aesKey,
		0,          //streamSizeHint: prefer large streams
		0,          //oneTimeHint
		0xFF,       //use256Or512Override: auto-detect what the hardware supports
		&blockSizeMax, &use256Or512, &ctx, e_rr
	));

	//Every AAD chunk except the last must be a whole multiple of the GHASH block size; round the chunk down to it.

	const U64 effectiveChunk = blockSizeMax ? (CLI_FILE_CHUNK / blockSizeMax) * blockSizeMax : CLI_FILE_CHUNK;

	if(size) {

		gotoIfError3(clean, Buffer_createUninitializedBytes(U64_min(effectiveChunk, size), alloc, &buf, e_rr));

		gotoIfError3(clean, File_openStream(
			&input, 1 * SECOND, EFileOpenType_Read, false, &fileHandleType, &streamType, &stream, e_rr
		));
		gotoIfError3(clean, StreamCursor_create(stream, CLI_STREAM_CACHE, false, alloc, &cur, e_rr));

		for(U64 pos = 0; pos < size; pos += effectiveChunk) {
			const U64 n = U64_min(effectiveChunk, size - pos);
			gotoIfError3(clean, StreamCursor_read(&cur, buf, pos, 0, n, true, alloc, e_rr));
			Buffer_aesExpertUpdateAAD(&ctx, Buffer_createRefConst(buf.ptr, n), blockSizeMax, use256Or512);
		}
	}

	(void) Buffer_aesExpertFinalize(&ctx, size, 0, I32x4_zero());        //dataLen 0; expectTag unused when generating

	const I32x4 tag = ctx.tag;

	Log_debugLnx(
		"GMAC of %.*s (%"PRIu64" bytes, AES-256-GCM, zero IV)\n"
		"\tTag: 0x%08x%08x%08x%08x",
		(int) CharString_length(input), input.ptr, size,
		(U32) I32x4_x(tag), (U32) I32x4_y(tag), (U32) I32x4_z(tag), (U32) I32x4_w(tag)
	);

clean:
	if(err.genericError)
		Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);

	Buffer_clearAllSecure(Buffer_createRef(keyV, sizeof(keyV)));
	StreamCursor_close(&cur, alloc);
	RefPtr_dec(&stream);
	Buffer_free(&buf, alloc);
	FileInfo_free(&info, alloc);
	return s_uccess;
}
