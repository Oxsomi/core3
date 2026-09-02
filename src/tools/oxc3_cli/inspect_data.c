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

//tools/oxc3_cli/inspect_data.c

#include "types/container/list_basic_types.h"
#include "types/base/c8.h"
#include "types/base/error.h"
#include "types/base/mathi.h"
#include "types/base/string_read.h"
#include "types/base/string_read_helper.h"
#include "types/math/vec4i.h"
#include "types/container/buffer.h"
#include "types/container/string.h"
#include "types/base/string_mut.h"
#include "types/container/string_unicode.h"
#include "types/container/memory_stream.h"
#include "types/container/encryption_stream.h"
#include "types/container/stream.h"
#include "types/container/ref_ptr.h"
#include "formats/oiCA/ca_file.h"
#include "formats/oiCA/ca_headers.h"
#include "formats/oiCA/ca_lookup.h"
#include "formats/oiCA/ca_props.h"
#include "formats/oiDL/dl_file.h"
#include "formats/oiDL/dl_headers.h"
#include "formats/oiSH/sh_file.h"
#include "formats/oiSH/sh_headers.h"
#include "formats/oiSB/sb_file.h"
#include "formats/oiSR/sr_file.h"
#include "formats/oiSP/sp_file.h"
#include "platforms/file.h"
#include "platforms/platform.h"
#include "platforms/logx.h"
#include "tools/oxc3_cli/cli.h"
#include "types/base/constants.h"

#ifdef CLI_SHADER_COMPILER
	#include "shader_compiler/compiler.h"
#endif

//Printing an entry

Bool collectArchiveEntries(const FileInfo *info, void *argGeneric, const Allocator *alloc, Error *e_rr) {

	ListCharString *arg = (ListCharString*) argGeneric;

	Bool s_uccess = true;
	CharString tmp = CharString_createNull();

	gotoIfError3(clean, CharString_createCopy(info->path, alloc, &tmp, e_rr));
	gotoIfError3(clean, ListCharString_pushBack(arg, tmp, alloc, e_rr));

	tmp = CharString_createNull();        //Belongs to list now

clean:
	CharString_free(&tmp, alloc);
	return s_uccess;
}

//Printing an entry

typedef struct OutputFolderToDisk {
	CharString base, output;
	const CAFile *sourceArchive;
} OutputFolderToDisk;

Bool writeToDisk(const FileInfo *info, void *outputGeneric, const Allocator *alloc, Error *e_rr) {

	OutputFolderToDisk *output = (OutputFolderToDisk*) outputGeneric;

	Bool s_uccess = true;
	CharString subDir = CharString_createNull();
	CharString tmp = CharString_createNull();

	const U64 start = CharString_length(output->base) == 1 && output->base.ptr[0] == '.' ? 0 : CharString_length(output->base);

	if (!CharString_cut(&info->path, start, 0, &subDir))
		retError(clean, Error_invalidOperation(0, "writeToDisk()::info.path cut failed"));

	gotoIfError3(clean, CharString_createCopy(output->output, alloc, &tmp, e_rr));
	gotoIfError3(clean, CharString_appendString(&tmp, &subDir, alloc, e_rr));

	if (info->type == EFileType_File) {
		gotoIfError3(clean, CLI_extractArchiveEntry(
			output->sourceArchive, CAFile_resolve(output->sourceArchive, info->path), &tmp, alloc, e_rr
		));
	}

	else gotoIfError3(clean, File_add(&tmp, EFileType_Folder, false, alloc, e_rr));

clean:
	CharString_free(&tmp, alloc);
	return s_uccess;
}

//Showing the entire file or a part to disk or to log

//The display cap: a dump is meant to be read, so only this much is ever pulled out of the stream.

#define CLI_SHOW_MAX (32 * 32)

//How the shown bytes are rendered.
//Detect decides from the bytes that get shown, which is all that is read; validating an entire entry to
// pick a rendering for a capped window would read the whole thing back for nothing.

typedef enum ECLIShowFormat {
	ECLIShowFormat_Binary,
	ECLIShowFormat_UTF8,
	ECLIShowFormat_Detect
} ECLIShowFormat;

Bool CLI_showFileStream(
	const ParsedArgs *args,
	StreamRef *stream,
	U64 base,
	U64 size,
	U64 start,
	U64 length,
	ECLIShowFormat format,
	Bool showEntireFile
) {

	if(!args || !stream) return false;

	//Validate offset

	if (start + (!!size) > size) {
		Log_debugLnx("Section out of bounds.");
		return false;
	}

	Error err = Error_none(), *e_rr = &err;        //Surface write failures (e.g. a path outside the working dir)
	CharString tmp = CharString_createNull();
	CharString tmp1 = CharString_createNull();
	Buffer window = Buffer_createNull();
	StreamCursor cur = (StreamCursor) { 0 };
	Bool s_uccess = false;

	const Allocator *alloc = Platform_instance->alloc;

	//Output it to a folder on disk was requested

	if (args->parameters & EOperationHasParameter_Output) {

		if(!length)
			length = size - start;

		if (start + length > size) {
			Log_debugLnx("Section out of bounds.");
			goto clean;
		}

		CharString out = CharString_createNull();

		if (!ParsedArgs_getArg(args, EOperationHasParameter_OutputShift, &out, NULL)) {
			Log_errorLnx("Invalid argument -output <string>.");
			goto clean;
		}

		//Straight from the source stream to the file, so the entry's size never bounds what can be written.

		gotoIfError3(clean, CLI_writeStreamRegion(stream, base + start, length, &out, alloc, e_rr));
	}

	//More info about a single entry

	else {

		if (!size) {
			Log_debugLnx("Section is empty.");
			goto clean;
		}

		Log_debugLnx("Section has %"PRIu64" bytes.", size);

		//Get length

		if(!length)
			length = showEntireFile ? size - start : U64_min(CLI_SHOW_MAX, size - start);

		else length = U64_min(CLI_SHOW_MAX * 2, length);

		if (start + length > size) {
			Log_debugLnx("Section out of bounds.");
			goto clean;
		}

		//Only the bytes that get displayed are pulled in.

		gotoIfError3(clean, Buffer_createUninitializedBytes(length, alloc, &window, e_rr));
		gotoIfError3(clean, StreamCursor_create(stream, CLI_STREAM_CACHE, false, alloc, &cur, e_rr));
		gotoIfError3(clean, StreamCursor_read(&cur, window, base + start, 0, length, true, alloc, e_rr));

		const U8 *w = (const U8*) window.ptr;

		const Bool isUTF8 =
			format == ECLIShowFormat_Detect ?
			CharString_isValidUTF8(CharString_createRefSizedConst((const C8*) w, length, false)) :
			format == ECLIShowFormat_UTF8;

		//Show what offset is being displayed

		Log_debugLnx("Showing offset #%"PRIx64" with size %"PRIu64, start, length);
		Log_debugLnx(isUTF8 ? "File contents: (utf8)" : "File contents: (binary)");

		//UTF8 can be directly output to log

		if (isUTF8) {
			tmp = CharString_createRefSizedConst((const C8*) w, length, false);
			Log_debugLnx("%.*s", CharString_length(tmp), tmp.ptr);
			tmp = CharString_createNull();
		}

		//Binary needs to be formatted first

		else {

			const CharString newLine = CharString_newLine();

			for (U64 i = 0; i < length; ++i) {

				gotoIfError3(clean, CharString_createHex(&(CharStringCreateNumber) {
					.v = w[i], .leadingZeros = 2, .allocator = alloc, .result = &tmp1
				}, e_rr));
				gotoIfError3(clean, CharString_popFrontCount(&tmp1, 2, e_rr));
				gotoIfError3(clean, CharString_appendString(&tmp, &tmp1, alloc, e_rr));
				gotoIfError3(clean, CharString_append(&tmp, ' ', alloc, e_rr));

				if (!((i + 1) & 31))
					gotoIfError3(clean, CharString_appendString(&tmp, &newLine, alloc, e_rr));

				CharString_free(&tmp1, alloc);
			}

			Log_debugLnx("%.*s", CharString_length(tmp), tmp.ptr);
			CharString_free(&tmp, alloc);
		}
	}

	s_uccess = true;

clean:
	if(err.genericError)
		Error_print(alloc, &err, ELogLevel_Error, ELogOptions_NewLine);

	StreamCursor_close(&cur, alloc);
	Buffer_free(&window, alloc);
	CharString_free(&tmp1, alloc);
	CharString_free(&tmp, alloc);
	return s_uccess;
}

//A buffer already in memory is shown through the same path, as a stream over its own bytes.

Bool CLI_showFile(const ParsedArgs *args, Buffer b, U64 start, U64 length, ECLIShowFormat format, Bool showEntireFile) {

	if(!args) return false;

	Error err = Error_none(), *e_rr = &err;
	Bool s_uccess = false;
	MemoryStreamRef *ms = NULL;

	const Allocator *alloc = Platform_instance->alloc;
	const RefPtrType memType = MemoryStream_makeType(alloc);
	const U64 size = Buffer_length(b);

	//An explicit ref, because a memory stream takes ownership of a buffer that owns its allocation and b
	// belongs to the caller.

	const Buffer ref = Buffer_createRefConst(b.ptr, size);

	gotoIfError3(clean, MemoryStream_createFromBufferRegion(
		ref, 0, size, EMemoryStreamFlags_None, &memType, &ms, e_rr
	));

	s_uccess = CLI_showFileStream(args, ms, 0, size, start, length, format, showEntireFile);

clean:
	if(err.genericError)
		Error_print(alloc, &err, ELogLevel_Error, ELogOptions_NewLine);

	RefPtr_dec(&ms);
	return s_uccess;
}

//Storing file or folder on disk

Bool CLI_storeFileOrFolder(const ParsedArgs *args, const CAFile *a, CAHandle handle, Bool *madeFile, U64 start, U64 len) {

	if(!args) return false;

	CharString tmp = CharString_createNull();
	FileInfo info = (FileInfo) { 0 };
	Bool s_uccess = false;
	Error *e_rr = NULL;

	const Allocator *alloc = Platform_instance->alloc;
	RefPtrType fileHandleType = FileHandle_makeType(alloc);
	(void) fileHandleType;

	//Save folder

	if (CAHandle_isFolder(handle)) {

		if(start || (len && len != CAFile_fileObjectCount(a, CAHandle_Root, true)))
			Log_warnLnx("Folder output to disk ignores offset and/or count.");

		CharString out = CharString_createNull();

		if (!ParsedArgs_getArg(args, EOperationHasParameter_OutputShift, &out, NULL)) {
			Log_errorLnx("Invalid argument -output <string>.");
			goto clean;
		}

		gotoIfError3(clean, File_add(&out, EFileType_Folder, true, alloc, e_rr));
		*madeFile = true;

		gotoIfError3(clean, CharString_createCopy(out, alloc, &tmp, e_rr));
		gotoIfError3(clean, CharString_append(&tmp, '/', alloc, e_rr));

		gotoIfError3(clean, CAFile_getInfo(a, handle, &info, alloc, e_rr));

		OutputFolderToDisk output = (OutputFolderToDisk) {
			.base = info.path,
			.output = tmp,
			.sourceArchive = a
		};

		gotoIfError3(clean, CAFile_foreach(
			a,
			handle,
			writeToDisk,
			&output,
			true,
			alloc,
			e_rr
		));

		CharString_free(&tmp, alloc);
		madeFile = false;            //We successfully wrote, so keep it from deleting the folder
		s_uccess = true;

		goto clean;
	}

	//Save file

	else {

		StreamRef *entry = NULL;
		U64 base = 0, size = 0;
		const RefPtrType memType = MemoryStream_makeType(alloc);        //Outlives entry

		gotoIfError3(clean, CLI_openArchiveEntry(a, handle, &memType, &entry, &base, &size, e_rr));

		CLI_showFileStream(args, entry, base, size, start, len, ECLIShowFormat_Binary, false);
		RefPtr_dec(&entry);
		s_uccess = true;
		goto clean;
	}

clean:
	FileInfo_free(&info, alloc);
	CharString_free(&tmp, alloc);
	return s_uccess;
}

//Handle inspection of individual data.
//Also handles info about the file in general.

Bool CLI_inspectData(const ParsedArgs *args) {

	if(!args) return false;

	Error err = Error_none(), *e_rr = &err;
	Bool s_uccess = false;

	CharString path = CharString_createNull();
	CharString tmp = CharString_createNull();
	CharString tmp1 = CharString_createNull();
	Buffer isaText = Buffer_createNull();        //Only used by the oiSH '-asic' ISA view (CLI_RGA)
	Buffer virtualBuf = Buffer_createNull();     //Virtual files can't be opened as a stream, so they get buffered

	const Allocator *alloc = Platform_instance->alloc;
	RefPtrType fileHandleType = FileHandle_makeType(alloc);
	RefPtrType memoryStreamType = MemoryStream_makeType(alloc);
	RefPtrType fileStreamType = FileStream_makeType(alloc);
	RefPtrType encStreamType = EncryptionStream_makeType(alloc);
	StreamRef *stream = NULL;
	StreamCursor cursor = (StreamCursor) { 0 };        //Peeks the magic; the format readers make their own cursor

	//Get file

	if (!ParsedArgs_getArg(args, EOperationHasParameter_InputShift, &path, &err)) {
		Log_errorLnx("Invalid argument -input <string>.");
		goto clean;
	}

	CLI_ensureVirtualLoaded(&path);        //If it's a "//section/..." path, load the section so File_* can resolve it

	//Get the size without reading the whole (possibly huge) file; the format readers stream what they need from disk.

	FileInfo fileInfo = (FileInfo) { 0 };

	if (!File_getInfo(&path, &fileInfo, alloc, &err)) {
		Log_errorLnx("Invalid file path.");
		goto clean;
	}

	const U64 fileSize = fileInfo.fileSize;
	FileInfo_free(&fileInfo, alloc);

	if (fileSize < 4) {
		Log_errorLnx("File has to start with magic number.");
		goto clean;
	}

	//File_openStream isn't supported on virtual files, so those are read into a buffer and wrapped in a memory stream;
	// physical files stream straight from disk via the StreamCursor-based format readers.

	if (File_isVirtual(path)) {

		if (!File_read(&path, 100 * MS, 0, 0, &fileHandleType, &virtualBuf, e_rr)) {
			Log_errorLnx("Invalid file path.");
			goto clean;
		}

		if (!MemoryStream_createFromBuffer(&virtualBuf, EMemoryStreamFlags_None, &memoryStreamType, &stream, e_rr))
			goto clean;
	}

	else if (!File_openStream(&path, 100 * MS, EFileOpenType_Read, false, &fileHandleType, &fileStreamType, &stream, e_rr)) {
		Log_errorLnx("Couldn't open file.");
		goto clean;
	}

	//Parse entry if available

	CharString entry = CharString_createNull();

	if (args->parameters & EOperationHasParameter_Entry)
		if (!ParsedArgs_getArg(args, EOperationHasParameter_EntryShift, &entry, &err)) {
			Log_errorLnx("Invalid argument -entry <string or uint>.");
			goto clean;
		}

	//Parse start if available

	CharString starts = CharString_createNull();
	U64 start = 0;

	if (args->parameters & EOperationHasParameter_StartOffset)
		if (
			!ParsedArgs_getArg(args, EOperationHasParameter_StartOffsetShift, &starts, &err) ||
			!CharString_parseU64(starts, &start) ||
			(start >> 32)
		) {
			Log_errorLnx("Invalid argument -start <uint>.");
			goto clean;
		}

	//Parse end if available

	CharString lengths = CharString_createNull();
	U64 length = 0;

	if (args->parameters & EOperationHasParameter_Length)
		if (
			!ParsedArgs_getArg(args, EOperationHasParameter_LengthShift, &lengths, &err) ||
			!CharString_parseU64(lengths, &length) ||
			(length >> 32)
		) {
			Log_errorLnx("Invalid argument -length <uint>.");
			goto clean;
		}

	//Parse encryption key

	U32 encryptionKeyV[8] = { 0 };
	U32 *encryptionKey = NULL;            //Only if we have aes should encryption key be set.
	Bool hasKey = false;

	//Parse encryption key (-aes / -aes-file / -aes-stdin)

	gotoIfError3(clean, CLI_getAesKey(args, encryptionKeyV, &hasKey, e_rr));

	if(hasKey)
		encryptionKey = encryptionKeyV;

	//Peek the 4-byte magic to dispatch; each format reader below makes its own cursor from offset 0 (reads are by offset).

	U32 magic = 0;

	if (!StreamCursor_create(stream, 0, false, alloc, &cursor, e_rr))
		goto clean;

	{
		U64 peekOff = 0;
		gotoIfError3(clean, StreamCursor_consumeU32(&cursor, &peekOff, &magic, alloc, e_rr));
	}

	StreamCursor_close(&cursor, alloc);

	if((args->flags & EOperationFlags_Bin) && magic != SHHeader_MAGIC) {
		Log_errorLnx("--bin flag can only be used with an oiSH file");
		goto clean;
	}

	//--includes lists include files for an oiSH; for an oiSR it expands the builtin-include symbols
	// (@types.hlsli etc.) that are otherwise collapsed into a summary.

	if((args->flags & EOperationFlags_Includes) && magic != SHHeader_MAGIC && magic != SRHeader_MAGIC) {
		Log_errorLnx("--includes flag can only be used with an oiSH or oiSR file");
		goto clean;
	}

	ESHBinaryType binaryType = ESHBinaryType_Count;

	if (args->parameters & EOperationHasParameter_ShaderOutputMode) {

		if(magic != SHHeader_MAGIC) {
			Log_errorLnx("-compile-output argument can only be used with an oiSH file");
			goto clean;
		}

		CharString shaderOutputMode = CharString_createNull();
		if (!ParsedArgs_getArg(args, EOperationHasParameter_ShaderOutputModeShift, &shaderOutputMode, &err)) {
			Log_errorLnx("Missing argument -compile-output");
			goto clean;
		}

		if(CharString_equalsCStringInsensitive(&shaderOutputMode, "DXIL"))
			binaryType = ESHBinaryType_DXIL;

		else if(CharString_equalsCStringInsensitive(&shaderOutputMode, "SPV"))
			binaryType = ESHBinaryType_SPIRV;

		else {
			Log_errorLnx("Invalid argument. Expected: -compile-output <spv/dxil>.");
			goto clean;
		}
	}

	switch (magic) {

		//oiCA header

		case CAHeader_MAGIC: {

			CAFile file = (CAFile) { 0 };
			ListCharString strings = { 0 };
			U64 baseCount = 0;

			Bool madeFile = false;
			CharString out = CharString_createNull();

			gotoIfError3(cleanCa, CAFile_read(stream, &encStreamType, 0, encryptionKey, alloc, &file, e_rr));

			if(encryptionKey)
				Buffer_clearAllSecure(Buffer_createRef(encryptionKeyV, sizeof(encryptionKeyV)));

			//Specific entry was requested

			if (args->parameters & EOperationHasParameter_Entry) {

				//Get handle of entry (by path, otherwise fall back to a numeric index into root's objects)

				CAHandle handle = CAFile_resolve(&file, entry);

				if (handle == CAHandle_Invalid) {

					U64 index = 0;

					if (!CharString_parseU64(entry, &index)) {
						Log_errorLnx("Invalid argument -entry <uint> or <valid path> expected.");
						goto cleanCa;
					}

					handle = CAFile_fileObjectAt(&file, CAHandle_Root, index);

					if (handle == CAHandle_Invalid) {
						Log_errorLnx(
							"Index out of bounds, max is %"PRIu64".", CAFile_fileObjectCount(&file, CAHandle_Root, false)
						);
						goto cleanCa;
					}
				}

				//Output it to a folder on disk was requested

				if (args->parameters & EOperationHasParameter_Output) {
					CLI_storeFileOrFolder(args, &file, handle, &madeFile, start, length);
					goto cleanCa;
				}

				//Want to output to log

				else {

					//Simply print the archive folder

					if (CAHandle_isFolder(handle)) {

						CharString entryPath = CharString_createNull();
						gotoIfError3(cleanCa, CAFile_getFullName(&file, handle, alloc, &entryPath, e_rr));

						baseCount = CharString_countAllSensitive(&entryPath, '/', 0) + 1;
						CharString_free(&entryPath, alloc);

						gotoIfError3(cleanCa, CAFile_foreach(
							&file,
							handle,
							collectArchiveEntries,
							&strings,
							true,
							alloc,
							e_rr
						));
					}

					//Print the subsection of the file

					else {

						StreamRef *entryStream = NULL;
						U64 base = 0, size = 0;
						const RefPtrType memType = MemoryStream_makeType(alloc);        //Outlives entryStream

						gotoIfError3(cleanCa, CLI_openArchiveEntry(
							&file, handle, &memType, &entryStream, &base, &size, e_rr
						));

						CharString entryPath = CharString_createNull();
						gotoIfError3(cleanCa, CAFile_getFullName(&file, handle, alloc, &entryPath, e_rr));
						Log_debugLnx("%.*s", CharString_length(entryPath), entryPath.ptr);
						CharString_free(&entryPath, alloc);

						CLI_showFileStream(args, entryStream, base, size, start, length, ECLIShowFormat_Detect, false);
						RefPtr_dec(&entryStream);
						goto cleanCa;
					}

				}
			}

			//General info was requested

			else {

				if (args->parameters & EOperationHasParameter_Output) {
					CLI_storeFileOrFolder(args, &file, CAHandle_Root, &madeFile, start, length);
					goto cleanCa;
				}

				gotoIfError3(cleanCa, CAFile_foreach(
					&file,
					CAHandle_Root,
					collectArchiveEntries,
					&strings,
					true,
					alloc,
					e_rr
				));
			}

			//Sort to ensure the subdirectories are correct

			if(!ListCharString_sortInsensitive(strings))
				retError(cleanCa, Error_invalidOperation(0, "CLI_inspectData() sort strings (oiCA) failed"));

			//Process all and print

			if(!length && start < strings.length)
				length = U64_min(64, strings.length - start);

			U64 end = start + length;

			Log_debugLnx(
				"Showing offset #%"PRIx64" with count %"PRIu64" in selected folder (File contains %"PRIu64" entries)",
				start, length, CAFile_fileObjectCount(&file, CAHandle_Root, true)
			);

			for(U64 i = start; i < end && i < strings.length; ++i) {

				CharString pathi = strings.ptr[i];
				U64 parentCount = CharString_countAllSensitive(&pathi, '/', 0);

				CAHandle v = CAFile_resolve(&file, pathi);

				//000: self
				//001:   child (indented by 2)

				if (v == CAHandle_Invalid)
					retError(cleanCa, Error_notFound(0, 0, "CLI_inspectData() couldn't find archive entry (oiCA)"));

				gotoIfError3(cleanCa, CharString_createDec(&(CharStringCreateNumber) {
					.v = CAHandle_getId(v), .leadingZeros = 3, .allocator = alloc, .result = &tmp
				}, e_rr));
				gotoIfError3(cleanCa, CharString_create(' ', 2 * (parentCount - baseCount), alloc, &tmp1, e_rr));
				gotoIfError3(cleanCa, CharString_append(&tmp, ':', alloc, e_rr));
				gotoIfError3(cleanCa, CharString_append(&tmp, ' ', alloc, e_rr));
				gotoIfError3(cleanCa, CharString_appendString(&tmp, &tmp1, alloc, e_rr));
				CharString_free(&tmp1, alloc);

				CharString sub = CharString_createNull();
				if(!CharString_cutBeforeLastSensitive(&pathi, '/', &sub))
					sub = CharString_createRefSizedConst(pathi.ptr, CharString_length(pathi), false);

				gotoIfError3(cleanCa, CharString_appendString(&tmp, &sub, alloc, e_rr));

				//Log and free temp

				Log_debugLnx("%.*s", CharString_length(tmp), tmp.ptr);
				CharString_free(&tmp, alloc);
			}

			if(!strings.length)
				Log_debugLnx("Folder is empty.");

		cleanCa:

			if(madeFile)
				File_remove(&out, 1 * SECOND, alloc, NULL);

			CAFile_free(&file, alloc);
			ListCharString_freeUnderlying(&strings, alloc);
			CharString_free(&tmp, alloc);

			RefPtr_dec(&stream);

			if(err.genericError)
				goto clean;

			break;
		}

		//oiDL header

		case DLHeader_MAGIC: {

			DLFile file = (DLFile) { 0 };

			U64 dlOff = 0;
			gotoIfError3(cleanDl, DLFile_read(
				stream, &dlOff, encryptionKey, I32x4_zero(), false, false, alloc, &encStreamType, &file, e_rr
			));

			if(encryptionKey)
				Buffer_clearAllSecure(Buffer_createRef(encryptionKeyV, sizeof(encryptionKeyV)));

			U64 end = 0;

			if (!(args->parameters & EOperationHasParameter_Entry)) {

				if(!length && start < DLFile_entryCount(&file))
					length = U64_min(64, DLFile_entryCount(&file) - start);

				end = start + length;
			}

			if (args->parameters & EOperationHasParameter_Entry) {

				//Grab entry

				U64 entryI = 0;

				if (!CharString_parseU64(entry, &entryI)) {
					Log_errorLnx("Invalid argument -entry <uint> expected.");
					goto cleanDl;
				}

				if (entryI >= DLFile_entryCount(&file)) {
					Log_errorLnx("Index out of bounds, max is %"PRIu64, DLFile_entryCount(&file));
					goto cleanDl;
				}

				Bool isAscii = file.settings.dataType == EDLDataType_String;
				Buffer b =
					isAscii ? CharString_bufferConst(file.entryStrings.ptr[entryI]) :
					file.entryBuffers.ptr[entryI];

				if(!CLI_showFile(args, b, start, length, isAscii ? ECLIShowFormat_UTF8 : ECLIShowFormat_Binary, false))
					goto cleanDl;
			}

			else {

				Log_debugLnx("oiDL Entries:");

				for (U64 i = start; i < end && i < DLFile_entryCount(&file); ++i) {

					U64 entrySize =
						file.settings.dataType == EDLDataType_String ? CharString_length(file.entryStrings.ptr[i]) :
						Buffer_length(file.entryBuffers.ptr[i]);

					const CharString colLen = CharString_createRefCStrConst(": length = ");

					gotoIfError3(cleanDl, CharString_createDec(&(CharStringCreateNumber) {
						.v = i, .leadingZeros = 3, .allocator = alloc, .result = &tmp
					}, e_rr));
					gotoIfError3(cleanDl, CharString_appendString(&tmp, &colLen, alloc, e_rr));

					gotoIfError3(cleanDl, CharString_createDec(&(CharStringCreateNumber) {
						.v = entrySize, .leadingZeros = 0, .allocator = alloc, .result = &tmp1
					}, e_rr));
					gotoIfError3(cleanDl, CharString_appendString(&tmp, &tmp1, alloc, e_rr));

					Log_debugLnx("%s", tmp.ptr);
					CharString_free(&tmp, alloc);
					CharString_free(&tmp1, alloc);
				}
			}

		cleanDl:

			CharString_free(&tmp, alloc);
			CharString_free(&tmp1, alloc);
			DLFile_free(&file, alloc);

			RefPtr_dec(&stream);

			if(err.genericError)
				goto clean;

			break;
		}

		//oiSH header

		case SHHeader_MAGIC: {

			if(encryptionKey)
				retError(clean, Error_invalidState(0, "CLI_inspectData() oiSH doesn't have aes support!"));

			SHFile file = (SHFile) { 0 };

			#ifdef CLI_SHADER_COMPILER
				Compiler comp = (Compiler) { 0 };
			#endif

			U64 shOff = 0;
			gotoIfError3(cleanSh, SHFile_read(stream, &shOff, false, alloc, &file, e_rr));

			Bool binaryMode = args->flags & EOperationFlags_Bin;
			Bool includesMode = args->flags & EOperationFlags_Includes;
			Bool isVerbose = args->flags & EOperationFlags_Verbose;

			if (binaryMode && includesMode) {
				Log_errorLnx("oiSH file data can't use --bin and --includes at the same time");
				goto cleanSh;
			}

			#ifdef CLI_RGA

				//-asic views a shader binary as AMD ISA. '?' just lists devices; a concrete ASIC implies viewing
				//SPIR-V, so default to --bin + SPIR-V (no -compile-output needed) and let -entry pick the binary.

				CharString isaAsic = CharString_createNull();
				const Bool hasAsic =
					ParsedArgs_getArg(args, EOperationHasParameter_ISAAsicShift, &isaAsic, NULL) &&
					CharString_length(isaAsic);

				if(hasAsic) {

					Bool asicHandled = false;
					gotoIfError3(cleanSh, CLI_isaResolveAsic(isaAsic, &asicHandled, alloc, e_rr));

					if(asicHandled)        //'?' listed the devices; nothing more to do
						goto cleanSh;

					binaryMode = true;

					if(binaryType == ESHBinaryType_Count)
						binaryType = ESHBinaryType_SPIRV;
				}

			#endif

			if((args->parameters & EOperationHasParameter_Output) && (
				binaryType == ESHBinaryType_Count ||
				!binaryMode ||
				!(args->parameters & EOperationHasParameter_Entry)
			)) {
				Log_errorLnx("oiSH file data can't use --output if -entry, --bin and -compile-output aren't defined");
				goto cleanSh;
			}

			U64 count = includesMode ? file.includes.length : (binaryMode ? file.binaries.length : file.entries.length);
			U64 end = 0;

			//A concrete -asic on a single-binary oiSH implies viewing that one binary, so treat it as -entry 0

			Bool doEntry = (args->parameters & EOperationHasParameter_Entry) != 0;

			#ifdef CLI_RGA
				const Bool asicAutoEntry = hasAsic && !doEntry && binaryMode && count == 1;
				if(asicAutoEntry)
					doEntry = true;
			#endif

			if (!doEntry) {

				if(!length && start < count)
					length = U64_min(64, count - start);

				end = start + length;
			}

			if (doEntry) {

				//Grab entry (an index into the binaries or entries)

				U64 entryI = 0;

				if (includesMode) {
					Log_errorLnx("oiSH file data includes mode doesn't support -entry, since all data is already displayed");
					goto cleanSh;
				}

				Bool needParse = true;

				#ifdef CLI_RGA
					if(asicAutoEntry)        //Synthesized entry 0, nothing to parse
						needParse = false;
				#endif

				if (needParse && !CharString_parseU64(entry, &entryI)) {
					Log_errorLnx("Invalid argument -entry <uint> expected.");
					goto cleanSh;
				}

				if (entryI >= count) {
					Log_errorLnx("Index out of bounds, max is %"PRIu64, count);
					goto cleanSh;
				}

				if(!binaryMode)
					SHEntry_print(&file.entries.ptr[entryI], true, alloc);

				else {

					//Compile mode was selected

					if (binaryType != ESHBinaryType_Count) {

						Buffer binary = file.binaries.ptr[entryI].binaries[binaryType];

						if (!Buffer_length(binary)) {
							Log_errorLnx("%s binary is missing at index %"PRIu64, ESHBinaryType_names[binaryType], entryI);
							goto cleanSh;
						}

						#ifdef CLI_RGA

							//-asic (already validated at the top of this case): SPIR-V has an offline ISA path (rga);
							//DXIL doesn't (that's the live-AMD-device route), so warn and fall through to DXIL disasm.

							if(hasAsic) {

								if(binaryType != ESHBinaryType_SPIRV)
									Log_warnLnx(
										"-asic has no offline ISA path for %s (that needs the live AMD device via 'OxC3 isa'); "
										"showing %s disassembly instead",
										ESHBinaryType_names[binaryType], ESHBinaryType_names[binaryType]
									);

								else {

									gotoIfError3(cleanSh, CLI_isaDisassembleSpirv(
										binary, isaAsic, file.binaries.ptr[entryI].identifier.entrypoint, &isaText, alloc, e_rr
									));

									if(!CLI_showFile(args, isaText, start, length, ECLIShowFormat_UTF8, true))
										goto cleanSh;

									goto cleanSh;
								}
							}

						#endif

						//Show as disassembly (DXIL or SPIRV disassembly) unless not available

						#ifdef CLI_SHADER_COMPILER

							if(!(args->parameters & EOperationHasParameter_Output)) {

								gotoIfError3(cleanSh, Compiler_create(alloc, &comp, e_rr));

								if (!Compiler_disassemble(&comp, binaryType, binary, alloc, &tmp, e_rr)) {
									Log_errorLnx(
										"%s disassembly failed at index %"PRIu64, ESHBinaryType_names[binaryType], entryI
									);
									goto cleanSh;
								}

								if(!CLI_showFile(args, CharString_bufferConst(tmp), start, length, ECLIShowFormat_UTF8, true))
									goto cleanSh;

								goto cleanSh;
							}

						#endif

						if(!CLI_showFile(args, binary, start, length, ECLIShowFormat_Binary, false))
							goto cleanSh;
					}

					else SHBinaryInfo_print(&file.binaries.ptr[entryI], true, alloc);
				}
			}

			else {

				if (includesMode) {

					Log_debugLnx("oiSH includes:");

					for (U64 i = start; i < end && i < count; ++i) {

						SHInclude inc = file.includes.ptr[i];

						Log_debugLnx(
							"Include %"PRIu64" (%.*s) with CRC32C %"PRIX32,
							i,
							(int) CharString_length(inc.relativePath),
							inc.relativePath.ptr,
							inc.crc32c
						);
					}
				}

				else if (binaryMode) {

					Log_debugLnx("oiSH binaries:");

					for (U64 i = start; i < end && i < count; ++i)
						SHBinaryInfo_print(&file.binaries.ptr[i], isVerbose, alloc);
				}

				else {

					if (!(args->parameters & (EOperationHasParameter_Entry | EOperationHasParameter_StartOffset)))
						SHFile_print(&file, isVerbose, alloc);

					else {

						Log_debugLnx("oiSH entries:");

						for (U64 i = start; i < end && i < count; ++i) {

							SHEntry shEntry = file.entries.ptr[i];
							const C8 *name = SHEntry_stageName(&shEntry);

							Log_debugLnx(
								"Entry %"PRIu64" (%s): %.*s", i, name, (int) CharString_length(shEntry.name), shEntry.name.ptr
							);
						}
					}
				}
			}

		cleanSh:

			SHFile_free(&file, alloc);

			#ifdef CLI_SHADER_COMPILER
				Compiler_free(&comp, alloc);
			#endif

			RefPtr_dec(&stream);

			if(err.genericError)
				goto clean;

			break;
		}

		//oiSB file

		case SBHeader_MAGIC: {

			if(encryptionKey)
				retError(clean, Error_invalidState(0, "CLI_inspectData() oiSH doesn't have aes support!"));

			SBFile file = (SBFile) { 0 };

			U64 sbOff = 0;
			gotoIfError3(cleanSb, SBFile_read(stream, &sbOff, false, alloc, &file, e_rr));

			SBFile_print(&file, 0, U16_MAX, true, alloc);        //TODO: parent

		cleanSb:

			SBFile_free(&file, alloc);

			RefPtr_dec(&stream);

			if(err.genericError)
				goto clean;

			break;
		}

		//oiSR file (frontend symbol AST reflection)

		case SPHeader_MAGIC: {

			if(encryptionKey)
				retError(clean, Error_invalidState(0, "CLI_inspectData() oiSP doesn't have aes support!"));

			SPFile file = (SPFile) { 0 };
			CharString printed = CharString_createNull();

			U64 spOff = 0;
			gotoIfError3(cleanSp, SPFile_read(stream, &spOff, false, alloc, &file, e_rr));

			Log_debugLnx("oiSP with %"PRIu64" pipeline(s):", file.pipelines.length);

			//Each pipeline prints its whole state with every field's provenance, so a stored pipeline shows which of
			// its values nobody actually chose.

			for (U64 i = 0; i < file.pipelines.length; ++i) {

				const SPPipelineBase pipeline = file.pipelines.ptr[i];

				if(pipeline.name != U32_MAX)
					Log_debugLnx(
						"Pipeline %"PRIu64": %.*s", i,
						(int) CharString_length(file.names.entryStrings.ptr[pipeline.name]),
						file.names.entryStrings.ptr[pipeline.name].ptr
					);

				else Log_debugLnx("Pipeline %"PRIu64":", i);

				//Stages name the shader they came from, which is what makes a stored pipeline resolvable again.

				for (U8 j = 0; j < pipeline.stageCount; ++j) {

					const SPStage stage = file.stages.ptr[pipeline.stageStart + j];

					const CharString shaderFile =
						stage.shaderFile != U32_MAX ?
						file.names.entryStrings.ptr[stage.shaderFile] : CharString_createRefCStrConst("<unnamed>");

					const CharString entryName =
						stage.entrypoint != U32_MAX ?
						file.names.entryStrings.ptr[stage.entrypoint] : CharString_createRefCStrConst("<unnamed>");

					Log_debugLnx(
						"\t%s: %.*s in %.*s (source hash 0x%08"PRIX32")",
						SHEntry_stageNames[stage.stage],
						(int) CharString_length(entryName), entryName.ptr,
						(int) CharString_length(shaderFile), shaderFile.ptr,
						stage.sourceHash
					);
				}

				CharString_free(&printed, alloc);
				gotoIfError3(cleanSp, SPFile_print(&file, (U32) i, alloc, &printed, e_rr));
				Log_debugLnx("%.*s", (int) CharString_length(printed), printed.ptr);
			}

		cleanSp:

			CharString_free(&printed, alloc);
			SPFile_free(&file, alloc);

			RefPtr_dec(&stream);

			if(err.genericError)
				goto clean;

			break;
		}

		case SRHeader_MAGIC: {

			if(encryptionKey)
				retError(clean, Error_invalidState(0, "CLI_inspectData() oiSR doesn't have aes support!"));

			SRFile file = (SRFile) { 0 };

			U64 srOff = 0;
			gotoIfError3(cleanSr, SRFile_read(stream, &srOff, false, alloc, &file, e_rr));

			SRFile_print(
				&file, 0,
				(args->flags & EOperationFlags_Verbose) != 0,
				!(args->flags & EOperationFlags_Includes),        //--includes expands the builtin-include symbols
				alloc
			);

		cleanSr:

			SRFile_free(&file, alloc);

			RefPtr_dec(&stream);

			if(err.genericError)
				goto clean;

			break;
		}

		//Invalid

		default:
			Log_errorLnx("File had unrecognized magic number.");
			goto clean;
	}

	s_uccess = true;

clean:

	if(err.genericError)
		Error_print(alloc, &err, ELogLevel_Error, ELogOptions_NewLine);

	StreamCursor_close(&cursor, alloc);
	RefPtr_dec(&stream);
	CharString_free(&tmp, alloc);
	Buffer_free(&isaText, alloc);
	Buffer_free(&virtualBuf, alloc);
	return s_uccess;
}
