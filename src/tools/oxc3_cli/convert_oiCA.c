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

//tools/oxc3_cli/convert_oiCA.c

#include "tools/oxc3_cli/cli.h"
#include "formats/oiCA/ca_file.h"
#include "formats/oiCA/ca_edit.h"
#include "formats/oiCA/ca_lookup.h"
#include "formats/oiCA/ca_props.h"
#include "types/base/error.h"
#include "types/base/string_read.h"
#include "types/base/string_read_helper.h"
#include "types/base/constants.h"
#include "types/container/buffer.h"
#include "types/container/string.h"
#include "types/container/stream.h"
#include "types/container/memory_stream.h"
#include "types/container/encryption_stream.h"
#include "types/container/ref_ptr.h"
#include "platforms/file.h"
#include "platforms/platform.h"
#include "platforms/logx.h"

typedef struct CAFileRecursion {
	CAFile *archive;
	CharString root;
	const RefPtrType *fileHandleType;
} CAFileRecursion;

Bool addFileToCAFile(const FileInfo *file, void *caFileGeneric, const Allocator *alloc, Error *e_rr) {

	CAFileRecursion *caFile = (CAFileRecursion*) caFileGeneric;

	Bool s_uccess = true;
	CharString subPath = CharString_createNull();
	Buffer data = Buffer_createNull();

	if(!CharString_cut(&file->path, CharString_length(caFile->root), 0, &subPath))
		retError(clean, Error_invalidState(0, "addFileToCAFile() cut failed"));

	//Resolve the parent directory (added earlier in the walk since parents precede children)

	CharString parentPath = CharString_createNull();
	CharString_cutAfterLastSensitive(&subPath, '/', &parentPath);

	CAHandle parent = CAHandle_Root;

	if (CharString_length(parentPath)) {

		parent = CAFile_resolve(caFile->archive, parentPath);

		if(parent == CAHandle_Invalid)
			retError(clean, Error_invalidState(0, "addFileToCAFile() parent lookup failed"));
	}

	//Grab the leaf name and copy it, since CAFile_addFile/addFolder move the name

	CharString tmp = CharString_createNull();
	CharString_cutBeforeLastSensitive(&subPath, '/', &tmp);

	if(!tmp.ptr)
		tmp = subPath;

	subPath = CharString_createNull();
	gotoIfError3(clean, CharString_createCopy(tmp, alloc, &subPath, e_rr));

	if (file->type == EFileType_File) {

		gotoIfError3(clean, File_read(&file->path, 100 * MS, 0, 0, caFile->fileHandleType, &data, e_rr));

		CAHandle handle = CAFile_addFile(caFile->archive, parent, &subPath, file->timestamp, alloc, e_rr);

		if(handle == CAHandle_Invalid)
			retError(clean, Error_invalidState(0, "addFileToCAFile() couldn't add file"));

		gotoIfError3(clean, CAFile_setData(caFile->archive, handle, alloc, &data, e_rr));
	}

	else {

		CAHandle handle = CAFile_addFolder(caFile->archive, parent, &subPath, alloc, e_rr);

		if(handle == CAHandle_Invalid)
			retError(clean, Error_invalidState(0, "addFileToCAFile() couldn't add folder"));
	}

clean:
	CharString_free(&subPath, alloc);
	Buffer_free(&data, alloc);
	return s_uccess;
}

Bool CLI_convertToCA(const CLIConvert *convert, Error *e_rr) {

	if(!convert) return false;

	const Allocator *alloc = Platform_instance->alloc;

	Bool s_uccess = true;

	CAFile file = (CAFile) { 0 };
	CharString resolved = CharString_createNull();
	CharString fileName = CharString_createNull();
	Bool isVirtual = false;

	FileInfo fileInfo = (FileInfo) { 0 };
	Buffer fileData = Buffer_createNull();

	RefPtrType fileHandleType = FileHandle_makeType(alloc);
	RefPtrType streamType = FileStream_makeType(alloc);
	RefPtrType encStreamType = EncryptionStream_makeType(alloc);
	StreamRef *stream = NULL;

	(void)convert->inputInfo;

	//TODO: Compression type

	CASettings settings = (CASettings) { .compressionType = EXXCompressionType_None };

	//Dates

	if(convert->args->flags & EOperationFlags_FullDate)
		settings.flags |= ECASettingsFlags_IncludeDate | ECASettingsFlags_IncludeFullDate;

	else if(convert->args->flags & EOperationFlags_Date)
		settings.flags |= ECASettingsFlags_IncludeDate;

	//Encryption type and hash type

	if(convert->args->parameters & EOperationHasParameter_AnyAES)
		settings.encryptionType = EXXEncryptionType_AES256GCM;

	//Compression type

	if(convert->args->flags & EOperationFlags_Uncompressed)
		settings.compressionType = EXXCompressionType_None;

	//else if(args.flags & EOperationFlags_FastCompress)                TODO:
	//    settings.compressionType = ECACompressionType_Brotli1;

	//Copying encryption key

	if(settings.encryptionType)
		Buffer_memcpy(
			Buffer_createRef(settings.encryptionKey, sizeof(settings.encryptionKey)),
			Buffer_createRefConst(convert->encKey, sizeof(settings.encryptionKey))
		);

	//Archive

	gotoIfError3(clean, CAFile_create(&settings, 16, 8, alloc, &file, e_rr));
	gotoIfError3(clean, File_resolve(convert->input, &isVirtual, 0, &Platform_instance->defaultDir, alloc, &resolved, e_rr));

	if (isVirtual)
		retError(clean, Error_invalidOperation(0, "CLI_convertToCA() can't be used on virtual file"));

	CAFileRecursion caFileRecursion = (CAFileRecursion) {
		.archive = &file,
		.root = resolved,
		.fileHandleType = &fileHandleType
	};

	if(File_hasFile(&resolved, alloc)) {

		CharString subPath = CharString_createNull();

		if(!CharString_cutBeforeLastSensitive(&resolved, '/', &subPath))
			retError(clean, Error_invalidState(0, "CLI_convertToCA() cutBeforeLast failed"));

		gotoIfError3(clean, File_getInfo(&resolved, &fileInfo, alloc, e_rr));
		gotoIfError3(clean, File_read(&resolved, 100 * MS, 0, 0, &fileHandleType, &fileData, e_rr));

		gotoIfError3(clean, CharString_createCopy(subPath, alloc, &fileName, e_rr));

		CAHandle handle = CAFile_addFile(&file, CAHandle_Root, &fileName, fileInfo.timestamp, alloc, e_rr);

		if(handle == CAHandle_Invalid)
			retError(clean, Error_invalidState(0, "CLI_convertToCA() couldn't add file"));

		gotoIfError3(clean, CAFile_setData(&file, handle, alloc, &fileData, e_rr));
	}

	else {

		gotoIfError3(clean, CharString_append(&resolved, '/', alloc, e_rr));
		caFileRecursion.root = resolved;

		gotoIfError3(clean, File_foreach(
			&caFileRecursion.root,
			false,
			addFileToCAFile,
			&caFileRecursion,
			!(convert->args->flags & EOperationFlags_NonRecursive),
			alloc,
			e_rr
		));
	}

	//Convert to CAFile and write to file

	gotoIfError3(clean, File_openStream(
		convert->output, 50 * MS, EFileOpenType_Write, true, &fileHandleType, &streamType, &stream, e_rr
	));

	U64 startOffset = 0;
	gotoIfError3(clean, CAFile_write(&file, &encStreamType, stream, &startOffset, alloc, e_rr));

clean:
	if(settings.encryptionType)
		Buffer_clearAllSecure(Buffer_createRef(settings.encryptionKey, sizeof(settings.encryptionKey)));

	RefPtr_dec(&stream);
	FileInfo_free(&fileInfo, alloc);
	CAFile_free(&file, alloc);
	CharString_free(&resolved, alloc);
	CharString_free(&fileName, alloc);
	Buffer_free(&fileData, alloc);
	return s_uccess;
}

//Extraction (oiCA -> disk)

typedef struct CAFileExtractRecursion {
	CAFile *archive;
	CharString outputPath;
	const RefPtrType *fileHandleType;
} CAFileExtractRecursion;

//Fetch a file's data into an owned buffer, whether it's fully loaded or still stream-backed.

static Bool CLI_readCAFileData(const CAFile *caFile, CAHandle handle, const Allocator *alloc, Buffer *output, Error *e_rr) {

	Bool s_uccess = true;
	StreamRef *streamRef = NULL;
	U64 streamOff = U64_MAX;

	streamRef = CAFile_getDataStream(caFile, handle, &streamOff);

	if (streamRef && streamOff != U64_MAX) {

		U64 fileSize = CAFile_fileSize(caFile, handle);
		gotoIfError3(clean, Buffer_createUninitializedBytes(fileSize, alloc, output, e_rr));

		OxStream *stream = RefPtr_data(streamRef, OxStream);
		gotoIfError3(clean, stream->read(stream, streamOff, fileSize, *output, alloc, e_rr));
	}

	else {

		Bool isValid = false;
		Buffer tmp = CAFile_getDataConst(caFile, handle, &isValid);

		if (!isValid)
			retError(clean, Error_invalidOperation(0, "CLI_readCAFileData() CAFile_getDataConst returned invalid"));

		gotoIfError3(clean, Buffer_createCopy(tmp, alloc, output, e_rr));
	}

clean:
	RefPtr_dec(&streamRef);
	return s_uccess;
}

static Bool extractCAFileEntry(const FileInfo *file, void *extractGeneric, const Allocator *alloc, Error *e_rr) {

	CAFileExtractRecursion *extract = (CAFileExtractRecursion*) extractGeneric;

	Bool s_uccess = true;
	CharString loc = CharString_createNull();
	Buffer data = Buffer_createNull();

	gotoIfError3(clean, CharString_createCopy(extract->outputPath, alloc, &loc, e_rr));
	gotoIfError3(clean, CharString_appendString(&loc, &file->path, alloc, e_rr));

	if (file->type == EFileType_Folder) {
		gotoIfError3(clean, File_add(&loc, EFileType_Folder, false, alloc, e_rr));
	}

	else {

		CAHandle handle = CAFile_resolveFile(extract->archive, file->path);

		if (handle == CAHandle_Invalid)
			retError(clean, Error_invalidState(0, "extractCAFileEntry() couldn't resolve file"));

		gotoIfError3(clean, CLI_readCAFileData(extract->archive, handle, alloc, &data, e_rr));
		gotoIfError3(clean, File_write(&data, &loc, 0, 0, 1 * SECOND, true, extract->fileHandleType, e_rr));
	}

clean:
	Buffer_free(&data, alloc);
	CharString_free(&loc, alloc);
	return s_uccess;
}

Bool CLI_convertFromCA(const CLIConvert *convert, Error *e_rr) {

	if(!convert) return false;

	const Allocator *alloc = Platform_instance->alloc;

	Bool s_uccess = true;

	Buffer buf = Buffer_createNull();
	Buffer data = Buffer_createNull();
	CharString outputPath = CharString_createNull();

	CAFile file = (CAFile) { 0 };

	RefPtrType fileHandleType = FileHandle_makeType(alloc);
	RefPtrType memStreamType = MemoryStream_makeType(alloc);
	RefPtrType encStreamType = EncryptionStream_makeType(alloc);
	MemoryStreamRef *memStream = NULL;

	//TODO: Batch multiple files

	if (convert->inputInfo->type != EFileType_File) {
		Log_errorLnx("oiCA can only be converted from single file");
		retError(clean, Error_invalidOperation(0, "CLI_convertFromCA()::inputInfo.type needs to be file"));
	}

	//Only hand CAFile_read an encryption key when -aes was actually given. encKey is a fixed array (always a
	//valid pointer), so passing it unconditionally makes reading a plaintext archive fail with
	//"encryptionKey is provided but no encryption is used".

	const Bool hasAES = convert->args && (convert->args->parameters & EOperationHasParameter_AnyAES);

	//Read file

	gotoIfError3(clean, File_read(convert->input, 100 * MS, 0, 0, &fileHandleType, &buf, e_rr));
	gotoIfError3(clean, MemoryStream_createFromBuffer(&buf, EMemoryStreamFlags_None, &memStreamType, &memStream, e_rr));
	gotoIfError3(clean, CAFile_read(memStream, &encStreamType, 0, hasAES ? convert->encKey : NULL, alloc, &file, e_rr));

	U64 entryCount = CAFile_fileObjectCount(&file, CAHandle_Root, true);
	Bool outputAsSingle = entryCount == 1;

	if (outputAsSingle) {

		CAHandle only = CAFile_fileObjectAt(&file, CAHandle_Root, 0);

		if(CAHandle_isFile(only)) {
			gotoIfError3(clean, CLI_readCAFileData(&file, only, alloc, &data, e_rr));
			gotoIfError3(clean, File_write(&data, convert->output, 0, 0, 1 * SECOND, true, &fileHandleType, e_rr));
		}
	}

	else {

		//Grab destination dest

		gotoIfError3(clean, CharString_createCopy(*convert->output, alloc, &outputPath, e_rr));

		if(!CharString_endsWithSensitive(outputPath, '/', 0))
			gotoIfError3(clean, CharString_append(&outputPath, '/', alloc, e_rr));

		//Write archive to disk

		CAFileExtractRecursion extract = (CAFileExtractRecursion) {
			.archive = &file,
			.outputPath = outputPath,
			.fileHandleType = &fileHandleType
		};

		gotoIfError3(clean, CAFile_foreach(
			&file,
			CAHandle_Root,
			extractCAFileEntry,
			&extract,
			true,
			alloc,
			e_rr
		));
	}

clean:
	CAFile_free(&file, alloc);
	RefPtr_dec(&memStream);
	Buffer_free(&buf, alloc);
	Buffer_free(&data, alloc);
	CharString_free(&outputPath, alloc);
	return s_uccess;
}
