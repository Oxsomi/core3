/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
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

#include "types/buffer.h"
#include "platforms/file.h"
#include "platforms/log.h"
#include "platforms/ext/archivex.h"
#include "platforms/ext/errorx.h"
#include "platforms/ext/stringx.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/formatx.h"
#include "formats/oiCA.h"
#include "cli.h"

typedef struct CAFileRecursion {
	Archive *archive;
	CharString root;
} CAFileRecursion;

Error packageFile(FileInfo file, CAFileRecursion *caFile) {

	CharString subPath = CharString_createNull();
	
	if(!CharString_cut(file.path, CharString_length(caFile->root), 0, &subPath))
		return Error_invalidState(0);

	ArchiveEntry entry = (ArchiveEntry) {
		.path = subPath,
		.type = file.type
	};

	Error err = Error_none();
	CharString copy = CharString_createNull();

	if (entry.type == EFileType_File)
		_gotoIfError(clean, File_read(file.path, 1 * SECOND, &entry.data));

	_gotoIfError(clean, CharString_createCopyx(entry.path, &copy));

	if (file.type == EFileType_File) {

		//We have to detect file type and process it here to a custom type.
		//We don't have a custom file yet, so for now 
		//this will just be identical to addFileToCAFile.

		//

		_gotoIfError(clean, Archive_addFilex(caFile->archive, copy, entry.data, 0))
	}

	else _gotoIfError(clean, Archive_addDirectoryx(caFile->archive, copy));

	return Error_none();

clean:
	Buffer_freex(&entry.data);
	CharString_freex(&copy);
	return err;
}

Bool CLI_package(ParsedArgs args) {

	//Parse encryption key

	U32 encryptionKeyV[8] = { 0 };
	U32 *encryptionKey = NULL;			//Only if we have aes should encryption key be set.

	if (args.parameters & EOperationHasParameter_AES) {

		CharString key = CharString_createNull();

		if (
			(ParsedArgs_getArg(args, EOperationHasParameter_AESShift, &key)).genericError || 
			!CharString_isHex(key)
		) {
			Log_errorLnx("Invalid parameter sent to -aes. Expecting key in hex (32 bytes)");
			return false;
		}

		U64 off = CharString_startsWithString(key, CharString_createConstRefCStr("0x"), EStringCase_Insensitive) ? 2 : 0;

		if (CharString_length(key) - off != 64) {
			Log_errorLnx("Invalid parameter sent to -aes. Expecting key in hex (32 bytes)");
			return false;
		}

		for (U64 i = off; i + 1 < CharString_length(key); ++i) {

			U8 v0 = C8_hex(key.ptr[i]);
			U8 v1 = C8_hex(key.ptr[++i]);

			v0 = (v0 << 4) | v1;
			*((U8*)encryptionKeyV + ((i - off) >> 1)) = v0;
		}

		encryptionKey = encryptionKeyV;
	}

	CASettings settings = (CASettings) { .compressionType = EXXCompressionType_None };

	if(args.parameters & EOperationHasParameter_AES)
		settings.encryptionType = EXXEncryptionType_AES256GCM;

	//Copying encryption key

	if(settings.encryptionType)
		Buffer_copy(
			Buffer_createRef(settings.encryptionKey, sizeof(settings.encryptionKey)),
			Buffer_createRef(encryptionKey, sizeof(settings.encryptionKey))
		);

	//Get input

	U64 offset = 0;
	Buffer inputArgBuf;
	Error err = List_get(args.args, offset++, &inputArgBuf);

	if (err.genericError) {
		Error_printx(err, ELogLevel_Error, ELogOptions_Default);
		return false;
	}

	CharString input = *(const CharString*)inputArgBuf.ptr;

	//Check if output is valid

	Buffer outputArgBuf;

	if ((err = List_get(args.args, offset++, &outputArgBuf)).genericError) {
		Error_printx(err, ELogLevel_Error, ELogOptions_Default);
		return false;
	}

	CharString output = *(const CharString*)outputArgBuf.ptr;

	//Make archive

	Archive archive = (Archive) { 0 };
	CharString resolved = CharString_createNull();
	CAFile file = (CAFile) { 0 };
	Buffer res = Buffer_createNull();
	Bool isVirtual = false;

	_gotoIfError(clean, Archive_createx(&archive));
	_gotoIfError(clean, File_resolvex(input, &isVirtual, 0, &resolved));

	if (isVirtual)
		_gotoIfError(clean, Error_invalidOperation(0));

	_gotoIfError(clean, CharString_appendx(&resolved, '/'));

	CAFileRecursion caFileRecursion = (CAFileRecursion) { 
		.archive = &archive, 
		.root = resolved
	};

	_gotoIfError(clean, File_foreach(
		caFileRecursion.root,
		(FileCallback) packageFile,
		&caFileRecursion,
		true
	));

	//Convert to CAFile and write to file

	_gotoIfError(clean, CAFile_create(settings, archive, &file));
	archive = (Archive) { 0 };	//Archive has been moved to CAFile

	_gotoIfError(clean, CAFile_writex(file, &res));

	_gotoIfError(clean, File_add(output, EFileType_File, 1 * SECOND));		//Ensure subdirs are created
	_gotoIfError(clean, File_write(res, output, 1 * SECOND));

clean:

	if(!err.genericError)
		Log_debugLnx("-- Packaging %s success!", resolved.ptr);

	else Log_errorLnx("-- Packaging %s failed!!", resolved.ptr);

	Buffer_freex(&res);
	CAFile_freex(&file);
	Archive_freex(&archive);
	CharString_freex(&resolved);
	return !err.genericError;
}
