/* MIT License
*   
*  Copyright (c) 2022 Oxsomi, Nielsbishere (Niels Brunekreef)
*  
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*  
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.
*  
*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE. 
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
	String root;
} CAFileRecursion;

Error packageFile(FileInfo file, CAFileRecursion *caFile) {

	String subPath = String_createNull();
	
	if(!String_cut(file.path, String_length(caFile->root), 0, &subPath))
		return Error_invalidState(0);

	ArchiveEntry entry = (ArchiveEntry) {
		.path = subPath,
		.type = file.type
	};

	Error err = Error_none();
	String copy = String_createNull();

	if (entry.type == EFileType_File)
		_gotoIfError(clean, File_read(file.path, 1 * SECOND, &entry.data));

	_gotoIfError(clean, String_createCopyx(entry.path, &copy));

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
	String_freex(&copy);
	return err;
}

Bool CLI_package(ParsedArgs args) {

	//Parse encryption key

	U32 encryptionKeyV[8] = { 0 };
	U32 *encryptionKey = NULL;			//Only if we have aes should encryption key be set.

	if (args.parameters & EOperationHasParameter_AES) {

		String key = String_createNull();

		if (
			(ParsedArgs_getArg(args, EOperationHasParameter_AESShift, &key)).genericError || 
			!String_isHex(key)
		) {
			Log_errorLn("Invalid parameter sent to -aes. Expecting key in hex (32 bytes)");
			return false;
		}

		U64 off = String_startsWithString(key, String_createConstRefUnsafe("0x"), EStringCase_Insensitive) ? 2 : 0;

		if (String_length(key) - off != 64) {
			Log_errorLn("Invalid parameter sent to -aes. Expecting key in hex (32 bytes)");
			return false;
		}

		for (U64 i = off; i + 1 < String_length(key); ++i) {

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

	String input = *(const String*)inputArgBuf.ptr;

	//Check if output is valid

	Buffer outputArgBuf;

	if ((err = List_get(args.args, offset++, &outputArgBuf)).genericError) {
		Error_printx(err, ELogLevel_Error, ELogOptions_Default);
		return false;
	}

	String output = *(const String*)outputArgBuf.ptr;

	//Make archive

	Archive archive = (Archive) { 0 };
	String resolved = String_createNull();
	CAFile file = (CAFile) { 0 };
	Buffer res = Buffer_createNull();
	Bool isVirtual = false;

	_gotoIfError(clean, Archive_createx(&archive));
	_gotoIfError(clean, File_resolvex(input, &isVirtual, 0, &resolved));

	if (isVirtual)
		_gotoIfError(clean, Error_invalidOperation(0));

	_gotoIfError(clean, String_appendx(&resolved, '/'));

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
		Log_debugLn("-- Packaging %s success!", resolved.ptr);

	else Log_errorLn("-- Packaging %s failed!!", resolved.ptr);

	Buffer_freex(&res);
	CAFile_freex(&file);
	Archive_freex(&archive);
	String_freex(&resolved);
	return !err.genericError;
}
