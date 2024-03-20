/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "formats/oiCA.h"
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

Error addFileToCAFile(FileInfo file, CAFileRecursion *caFile) {

	CharString subPath = CharString_createNull();

	if(!CharString_cut(file.path, CharString_length(caFile->root), 0, &subPath))
		return Error_invalidState(0, "addFileToCAFile() cut failed");

	ArchiveEntry entry = (ArchiveEntry) {
		.path = subPath,
		.type = file.type,
		.timestamp = file.timestamp
	};

	Error err = Error_none();
	CharString copy = CharString_createNull();

	if (entry.type == EFileType_File)
		gotoIfError(clean, File_read(file.path, 1 * SECOND, &entry.data))

	gotoIfError(clean, CharString_createCopyx(entry.path, &copy))

	if (file.type == EFileType_File)
		gotoIfError(clean, Archive_addFilex(caFile->archive, copy, entry.data, entry.timestamp))

	else gotoIfError(clean, Archive_addDirectoryx(caFile->archive, copy))

	return Error_none();

clean:
	Buffer_freex(&entry.data);
	CharString_freex(&copy);
	return err;
}

Error CLI_convertToCA(ParsedArgs args, CharString input, FileInfo inputInfo, CharString output, U32 encryptionKey[8]) {

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
		return Error_invalidOperation(3, "CLI_convertToCA() encryptionKey provided but no encryption was used");

	if(!encryptionKey && settings.encryptionType)
		return Error_unauthorized(0, "CLI_convertToCA() requires encryptionKey but not provided");

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

	//Create our entries

	CAFile file = (CAFile) { 0 };
	Error err = Error_none();

	//Archive

	Archive archive = (Archive) { 0 };
	CharString resolved = CharString_createNull();
	CharString tmp = CharString_createNull();
	Buffer res = Buffer_createNull();
	Bool isVirtual = false;

	FileInfo fileInfo = (FileInfo) { 0 };
	Buffer fileData = Buffer_createNull();

	gotoIfError(clean, Archive_createx(&archive))
	gotoIfError(clean, File_resolvex(input, &isVirtual, 0, &resolved))

	if (isVirtual)
		gotoIfError(clean, Error_invalidOperation(0, "CLI_convertToCA() can't be used on virtual file"))

	CAFileRecursion caFileRecursion = (CAFileRecursion) {
		.archive = &archive,
		.root = resolved
	};

	if(File_hasFile(resolved)) {

		CharString subPath = CharString_createNull();

		if(!CharString_cutBeforeLastSensitive(resolved, '/', &subPath))
			gotoIfError(clean, Error_invalidState(0, "CLI_convertToCA() cutBeforeLast failed"))

		gotoIfError(clean, File_getInfo(resolved, &fileInfo))
		gotoIfError(clean, File_read(resolved, 1 * SECOND, &fileData))
		gotoIfError(clean, CharString_createCopyx(subPath, &tmp))
		gotoIfError(clean, Archive_addFilex(&archive, tmp, fileData, fileInfo.timestamp))

		//Belongs to archive now

		fileData = Buffer_createNull();
		tmp = CharString_createNull();
	}

	else {

		gotoIfError(clean, CharString_appendx(&resolved, '/'))
		caFileRecursion.root = resolved;

		gotoIfError(clean, File_foreach(
			caFileRecursion.root,
			(FileCallback)addFileToCAFile,
			&caFileRecursion,
			!(args.flags & EOperationFlags_NonRecursive)
		))
	}

	//Convert to CAFile and write to file

	gotoIfError(clean, CAFile_create(settings, archive, &file))
	archive = (Archive) { 0 };	//Archive has been moved to CAFile

	gotoIfError(clean, CAFile_writex(file, &res))

	gotoIfError(clean, File_write(res, output, 1 * SECOND))

clean:
	FileInfo_freex(&fileInfo);
	CAFile_freex(&file);
	Archive_freex(&archive);
	CharString_freex(&resolved);
	CharString_freex(&tmp);
	Buffer_freex(&res);
	Buffer_freex(&fileData);
	return err;
}

Error CLI_convertFromCA(ParsedArgs args, CharString input, FileInfo inputInfo, CharString output, U32 encryptionKey[8]) {

	(void)args;

	//TODO: Batch multiple files

	if (inputInfo.type != EFileType_File) {
		Log_errorLnx("oiCA can only be converted from single file");
		return Error_invalidOperation(0, "CLI_convertFromCA()::inputInfo.type needs to be file");
	}

	//Read file

	Buffer buf = Buffer_createNull();
	CharString outputPath = CharString_createNull();
	CharString loc = CharString_createNull();

	Error err = Error_none();
	CAFile file = (CAFile) { 0 };
	Bool didMakeFile = false;

	gotoIfError(clean, File_read(input, 1 * SECOND, &buf))
	gotoIfError(clean, CAFile_readx(buf, encryptionKey, &file))

	Bool outputAsSingle = file.archive.entries.length == 1;
	EFileType outputType = outputAsSingle ? file.archive.entries.ptr->type : EFileType_Folder;

	gotoIfError(clean, File_add(output, outputType, 1 * SECOND))
	didMakeFile = true;

	if (outputAsSingle) {

		if(outputType == EFileType_File)
			gotoIfError(clean, File_write(file.archive.entries.ptr->data, output, 1 * SECOND))
	}

	else {

		//Grab destination dest

		gotoIfError(clean, CharString_createCopyx(output, &outputPath))

		if(!CharString_endsWithSensitive(outputPath, '/'))
			gotoIfError(clean, CharString_appendx(&outputPath, '/'))

		//Write archive to disk

		for (U64 i = 0; i < file.archive.entries.length; ++i) {

			ArchiveEntry ei = file.archive.entries.ptr[i];

			gotoIfError(clean, CharString_createCopyx(outputPath, &loc))
			gotoIfError(clean, CharString_appendStringx(&loc, ei.path))

			if (ei.type == EFileType_Folder) {
				gotoIfError(clean, File_add(loc, EFileType_Folder, 1 * SECOND))
				CharString_freex(&loc);
				continue;
			}

			gotoIfError(clean, File_write(ei.data, loc, 1 * SECOND))
			CharString_freex(&loc);
		}
	}

clean:

	if (didMakeFile && err.genericError)
		File_remove(output, 1 * SECOND);

	CAFile_freex(&file);
	Buffer_freex(&buf);
	CharString_freex(&outputPath);
	CharString_freex(&loc);
	return err;
}
