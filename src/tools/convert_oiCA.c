/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "cli.h"
#include "formats/oiCA/ca_file.h"
#include "types/error.h"
#include "types/buffer.h"
#include "platforms/file.h"
#include "platforms/log.h"
#include "platforms/ext/stringx.h"
#include "platforms/ext/archivex.h"
#include "platforms/ext/formatx.h"
#include "platforms/ext/bufferx.h"

typedef struct CAFileRecursion {
	Archive *archive;
	CharString root;
} CAFileRecursion;

Bool addFileToCAFile(FileInfo file, CAFileRecursion *caFile, Error *e_rr) {

	Bool s_uccess = true;
	CharString subPath = CharString_createNull();

	if(!CharString_cut(file.path, CharString_length(caFile->root), 0, &subPath))
		retError(clean, Error_invalidState(0, "addFileToCAFile() cut failed"))

	ArchiveEntry entry = (ArchiveEntry) {
		.path = subPath,
		.type = file.type,
		.timestamp = file.timestamp
	};

	if (entry.type == EFileType_File)
		gotoIfError3(clean, File_read(file.path, 1 * SECOND, &entry.data, e_rr))

	if (file.type == EFileType_File)
		gotoIfError3(clean, Archive_addFilex(caFile->archive, entry.path, &entry.data, entry.timestamp, e_rr))

	else gotoIfError3(clean, Archive_addDirectoryx(caFile->archive, entry.path, e_rr))

clean:
	Buffer_freex(&entry.data);
	return s_uccess;
}

Bool CLI_convertToCA(
	ParsedArgs args, CharString input, FileInfo inputInfo, CharString output, U32 encryptionKey[8], Error *e_rr
) {

	Bool s_uccess = true;

	CAFile file = (CAFile) { 0 };
	Archive archive = (Archive) { 0 };
	CharString resolved = CharString_createNull();
	Buffer res = Buffer_createNull();
	Bool isVirtual = false;

	FileInfo fileInfo = (FileInfo) { 0 };
	Buffer fileData = Buffer_createNull();

	(void)inputInfo;

	//TODO: Compression type

	CASettings settings = (CASettings) { .compressionType = EXXCompressionType_None };

	//Dates

	if(args.flags & EOperationFlags_FullDate)
		settings.flags |= ECASettingsFlags_IncludeDate | ECASettingsFlags_IncludeFullDate;

	else if(args.flags & EOperationFlags_Date)
		settings.flags |= ECASettingsFlags_IncludeDate;

	//Encryption type and hash type

	if(args.flags & EOperationFlags_SHA256)
		settings.flags |= ECASettingsFlags_UseSHA256;

	if(args.parameters & EOperationHasParameter_AES)
		settings.encryptionType = EXXEncryptionType_AES256GCM;

	//Ensure encryption key isn't provided if we're not encrypting

	if(encryptionKey && !settings.encryptionType)
		retError(clean, Error_invalidOperation(3, "CLI_convertToCA() encryptionKey provided but no encryption was used"))

	if(!encryptionKey && settings.encryptionType)
		retError(clean, Error_unauthorized(0, "CLI_convertToCA() requires encryptionKey but not provided"))

	//Compression type

	if(args.flags & EOperationFlags_Uncompressed)
		settings.compressionType = EXXCompressionType_None;

	//else if(args.flags & EOperationFlags_FastCompress)				TODO:
	//	settings.compressionType = ECACompressionType_Brotli1;

	//Copying encryption key

	if(settings.encryptionType)
		Buffer_copy(
			Buffer_createRef(settings.encryptionKey, sizeof(settings.encryptionKey)),
			Buffer_createRef(encryptionKey, sizeof(settings.encryptionKey))
		);

	//Archive

	gotoIfError3(clean, Archive_createx(&archive, e_rr))
	gotoIfError3(clean, File_resolvex(input, &isVirtual, 0, &resolved, e_rr))

	if (isVirtual)
		retError(clean, Error_invalidOperation(0, "CLI_convertToCA() can't be used on virtual file"))

	CAFileRecursion caFileRecursion = (CAFileRecursion) {
		.archive = &archive,
		.root = resolved
	};

	if(File_hasFile(resolved)) {

		CharString subPath = CharString_createNull();

		if(!CharString_cutBeforeLastSensitive(resolved, '/', &subPath))
			retError(clean, Error_invalidState(0, "CLI_convertToCA() cutBeforeLast failed"))

		gotoIfError3(clean, File_getInfox(resolved, &fileInfo, e_rr))
		gotoIfError3(clean, File_read(resolved, 1 * SECOND, &fileData, e_rr))
		gotoIfError3(clean, Archive_addFilex(&archive, subPath, &fileData, fileInfo.timestamp, e_rr))
	}

	else {

		gotoIfError2(clean, CharString_appendx(&resolved, '/'))
		caFileRecursion.root = resolved;

		gotoIfError3(clean, File_foreach(
			caFileRecursion.root,
			(FileCallback)addFileToCAFile,
			&caFileRecursion,
			!(args.flags & EOperationFlags_NonRecursive),
			e_rr
		))
	}

	//Convert to CAFile and write to file

	gotoIfError3(clean, CAFile_create(settings, &archive, &file, e_rr))
	gotoIfError3(clean, CAFile_writex(file, &res, e_rr))
	gotoIfError3(clean, File_write(res, output, 1 * SECOND, e_rr))

clean:
	FileInfo_freex(&fileInfo);
	CAFile_freex(&file);
	Archive_freex(&archive);
	CharString_freex(&resolved);
	Buffer_freex(&res);
	Buffer_freex(&fileData);
	return s_uccess;
}

Bool CLI_convertFromCA(
	ParsedArgs args, CharString input, FileInfo inputInfo, CharString output, U32 encryptionKey[8], Error *e_rr
) {

	Bool s_uccess = true;

	Buffer buf = Buffer_createNull();
	CharString outputPath = CharString_createNull();
	CharString loc = CharString_createNull();

	CAFile file = (CAFile) { 0 };
	Bool didMakeFile = false;

	(void)args;

	//TODO: Batch multiple files

	if (inputInfo.type != EFileType_File) {
		Log_errorLnx("oiCA can only be converted from single file");
		retError(clean, Error_invalidOperation(0, "CLI_convertFromCA()::inputInfo.type needs to be file"))
	}

	//Read file

	gotoIfError3(clean, File_read(input, 1 * SECOND, &buf, e_rr))
	gotoIfError3(clean, CAFile_readx(buf, encryptionKey, &file, e_rr))

	Bool outputAsSingle = file.archive.entries.length == 1;
	EFileType outputType = outputAsSingle ? file.archive.entries.ptr->type : EFileType_Folder;

	gotoIfError3(clean, File_add(output, outputType, 1 * SECOND, true, e_rr))
	didMakeFile = true;

	if (outputAsSingle) {

		if(outputType == EFileType_File)
			gotoIfError3(clean, File_write(file.archive.entries.ptr->data, output, 1 * SECOND, e_rr))
	}

	else {

		//Grab destination dest

		gotoIfError2(clean, CharString_createCopyx(output, &outputPath))

		if(!CharString_endsWithSensitive(outputPath, '/', 0))
			gotoIfError2(clean, CharString_appendx(&outputPath, '/'))

		//Write archive to disk

		for (U64 i = 0; i < file.archive.entries.length; ++i) {

			ArchiveEntry ei = file.archive.entries.ptr[i];

			gotoIfError2(clean, CharString_createCopyx(outputPath, &loc))
			gotoIfError2(clean, CharString_appendStringx(&loc, ei.path))

			if (ei.type == EFileType_Folder) {
				gotoIfError3(clean, File_add(loc, EFileType_Folder, 1 * SECOND, false, e_rr))
				CharString_freex(&loc);
				continue;
			}

			gotoIfError3(clean, File_write(ei.data, loc, 1 * SECOND, e_rr))
			CharString_freex(&loc);
		}
	}

clean:

	if (didMakeFile && !s_uccess)
		File_remove(output, 1 * SECOND, NULL);

	CAFile_freex(&file);
	Buffer_freex(&buf);
	CharString_freex(&outputPath);
	CharString_freex(&loc);
	return s_uccess;
}
