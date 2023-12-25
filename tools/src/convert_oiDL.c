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

#include "types/error.h"
#include "types/list.h"
#include "types/buffer.h"
#include "formats/oiDL.h"
#include "platforms/log.h"
#include "platforms/file.h"
#include "platforms/ext/listx.h"
#include "platforms/ext/stringx.h"
#include "platforms/ext/formatx.h"
#include "platforms/ext/errorx.h"
#include "platforms/ext/bufferx.h"
#include "cli.h"

Error addFileToDLFile(FileInfo file, List *names) {

	if (file.type == EFileType_Folder)
		return Error_none();

	Error err;
	CharString copy = CharString_createNull();

	if((err = CharString_createCopyx(file.path, &copy)).genericError)
		return err;

	if ((err = List_pushBackx(names, Buffer_createConstRef(&copy, sizeof(copy)))).genericError) {
		CharString_freex(&copy);
		return err;
	}

	return Error_none();
}

Error _CLI_convertToDL(ParsedArgs args, CharString input, FileInfo inputInfo, CharString output, U32 encryptionKey[8]) {

	//TODO: EXXCompressionType_Brotli11

	DLSettings settings = (DLSettings) { .compressionType = EXXCompressionType_None };

	//Encryption type and hash type

	if(args.flags & EOperationFlags_SHA256)
		settings.flags |= EDLSettingsFlags_UseSHA256;

	if(args.parameters & EOperationHasParameter_AES)
		settings.encryptionType = EXXEncryptionType_AES256GCM;

	//Data type

	if ((args.flags & EOperationFlags_UTF8) && (args.flags & EOperationFlags_Ascii)) {
		Log_errorLnx("oiDL can only pick UTF8 or Ascii, not both.");
		return Error_invalidParameter(0, 0, "_CLI_convertToDL() oiDL can only pick UTF8 or Ascii, not both");
	}

	if(args.flags & EOperationFlags_UTF8)
		settings.dataType = EDLDataType_UTF8;

	else if(args.flags & EOperationFlags_Ascii)
		settings.dataType = EDLDataType_Ascii;

	//Compression type

	if(args.flags & EOperationFlags_Uncompressed)
		settings.compressionType = EXXCompressionType_None;

	//else if(args.flags & EOperationFlags_FastCompress)				TODO:
	//	settings.compressionType = EXXCompressionType_Brotli1;

	//Ensure encryption key isn't provided if we're not encrypting

	if(encryptionKey && !settings.encryptionType)
		return Error_invalidOperation(3, "_CLI_convertToDL() encryptionKey was provided but encryption wasn't used");

	if(!encryptionKey && settings.encryptionType)
		return Error_unauthorized(0, "_CLI_convertToDL() encryptionKey was needed but not provided");

	//Copying encryption key

	if(settings.encryptionType)
		Buffer_copy(
			Buffer_createRef(settings.encryptionKey, sizeof(settings.encryptionKey)),
			Buffer_createRef(encryptionKey, sizeof(settings.encryptionKey))
		);

	//Check validity

	if(args.parameters & EOperationHasParameter_SplitBy && !(args.flags & EOperationFlags_Ascii)) {

		//TODO: Support split UTF8

		Log_errorLnx("oiDL doesn't support splitting by a character if it's not a string list.");

		return Error_invalidParameter(
			0, 1, "_CLI_convertToDL() oiDL doesn't support splitting by a character if it's not a string list."
		);
	}

	List buffers = List_createEmpty(sizeof(Buffer));
	List paths = List_createEmpty(sizeof(CharString));
	List sortedPaths = List_createEmpty(sizeof(CharString));

	Error err = Error_none();
	CharStringList split = (CharStringList) { 0 };
	DLFile file = (DLFile) { 0 };
	Buffer fileBuf = Buffer_createNull();
	Buffer buf = Buffer_createNull();
	Buffer res = Buffer_createNull();

	//Validate if string and split string by endline
	//Merge that into a file

	if (inputInfo.type == EFileType_File) {

		_gotoIfError(clean, File_read(input, 1 * SECOND, &buf));

		//Create oiDL from text file. Splitting by enter or custom string

		if (args.flags & EOperationFlags_Ascii) {

			CharString str = CharString_createConstRefSized((const C8*)buf.ptr, Buffer_length(buf), false);

			//Grab split string

			if (args.parameters & EOperationHasParameter_SplitBy) {

				CharString splitBy;
				_gotoIfError(clean, ParsedArgs_getArg(args, EOperationHasParameter_SplitByShift, &splitBy));
					
				_gotoIfError(clean, CharString_splitStringSensitivex(str, splitBy, &split));
			}

			else _gotoIfError(clean, CharString_splitLinex(str, &split));

			//Create DLFile and write it

			_gotoIfError(clean, DLFile_createx(settings, &file));
					
			for(U64 i = 0; i < split.length; ++i) 
				_gotoIfError(clean, DLFile_addEntryAsciix(&file, split.ptr[i]));

			_gotoIfError(clean, DLFile_writex(file, &res));

			goto write;
		}

		//Add single file entry and create it as normal

		Buffer ref = Buffer_createConstRef(&buf, sizeof(buf));
		_gotoIfError(clean, List_pushBackx(&buffers, ref));

		buf = Buffer_createNull();		//Ensure we don't free twice.
	}

	else {

		//Merge folder's children

		_gotoIfError(clean, List_reservex(&buffers, 256));

		_gotoIfError(clean, File_foreach(
			input, (FileCallback) addFileToDLFile, &paths, 
			!(args.flags & EOperationFlags_NonRecursive)
		));

		//Check if they're all following a linear file order

		CharString basePath = CharString_createNull();

		//To do this, we will create a list that can hold all paths in sorted order

		_gotoIfError(clean, List_resizex(&sortedPaths, paths.length));

		Bool allLinear = true;

		for (U64 i = 0; i < paths.length; ++i) {

			CharString stri = ((const CharString*)paths.ptr)[i];
			CharString_cutBeforeLast(stri, '/', EStringCase_Sensitive, &basePath);

			CharString tmp = CharString_createNull();

			if(CharString_cutAfterLast(basePath, '.', EStringCase_Sensitive, &tmp))
				basePath = tmp;

			U64 dec = 0;
			if (!CharString_parseU64(basePath, &dec) || dec >> 32) {
				allLinear = false;
				break;
			}

			if (dec >= sortedPaths.length) {
				allLinear = false;
				break;
			}

			CharString sortedI = ((const CharString*)sortedPaths.ptr)[dec];

			if (CharString_length(sortedI)) {
				allLinear = false;
				break;
			}

			((CharString*)sortedPaths.ptr)[dec] = CharString_createConstRefSized(stri.ptr, CharString_length(stri), false);
		}

		//Keep the sorting as is, since it's not linear

		if(!allLinear) {

			for (U64 i = 0; i < paths.length; ++i) {

				CharString stri = ((const CharString*)paths.ptr)[i];
				_gotoIfError(clean, File_read(stri, 1 * SECOND, &fileBuf));
				_gotoIfError(clean, List_pushBackx(&buffers, Buffer_createConstRef(&fileBuf, sizeof(fileBuf))));

				fileBuf = Buffer_createNull();
			}

		}

		//Use sorted to insert into buffers

		else for (U64 i = 0; i < sortedPaths.length; ++i) {

			CharString stri = ((const CharString*)sortedPaths.ptr)[i];
			_gotoIfError(clean, File_read(stri, 1 * SECOND, &fileBuf));
			_gotoIfError(clean, List_pushBackx(&buffers, Buffer_createConstRef(&fileBuf, sizeof(fileBuf))));

			fileBuf = Buffer_createNull();
		}

		//

		List_freex(&sortedPaths);
		List_freex(&paths);
	}

	//Now we're left with only data entries
	//Create simple oiDL with all of them

	_gotoIfError(clean, DLFile_createx(settings, &file));

	//Add entries 

	for(U64 i = 0; i < buffers.length; ++i) {

		//Add entry

		if(settings.dataType == EDLDataType_Data)
			_gotoIfError(clean, DLFile_addEntryx(&file, *((Buffer*)buffers.ptr + i)))

		else if(settings.dataType == EDLDataType_Ascii) {

			Buffer bufi = *((Buffer*)buffers.ptr + i);
			CharString str = CharString_createConstRefSized((const C8*)bufi.ptr, Buffer_length(bufi), false);
			
			_gotoIfError(clean, DLFile_addEntryAsciix(&file, str));
		}

		else _gotoIfError(clean, DLFile_addEntryUTF8x(&file, *((Buffer*)buffers.ptr + i)));

		//Ensure we get don't free the same thing multiple times

		*((Buffer*)buffers.ptr + i) = Buffer_createNull();
	}

	//Convert to binary

	_gotoIfError(clean, DLFile_writex(file, &res));

write:

	_gotoIfError(clean, File_write(res, output, 1 * SECOND));

clean:

	if(err.genericError)
		Error_printx(err, ELogLevel_Error, ELogOptions_Default);

	for(U64 i = 0; i < paths.length; ++i) {
		CharString str = *((const CharString*)paths.ptr + i);
		CharString_freex(&str);
	}

	for(U64 i = 0; i < buffers.length; ++i) {
		Buffer bufi = *((Buffer*)buffers.ptr + i);
		Buffer_freex(&bufi);
	}

	DLFile_freex(&file);
	CharStringList_freex(&split);
	Buffer_freex(&res);
	Buffer_freex(&buf);
	Buffer_freex(&fileBuf);
	List_freex(&buffers);
	List_freex(&paths);
	List_freex(&sortedPaths);

	return err;
}

Error _CLI_convertFromDL(ParsedArgs args, CharString input, FileInfo inputInfo, CharString output, U32 encryptionKey[8]) {

	//TODO: Batch multiple files

	if (inputInfo.type != EFileType_File) {
		Log_errorLnx("oiDL can only be converted from single file");
		return Error_invalidOperation(0, "_CLI_convertFromDL() oiDL can only be converted from single file");
	}

	//Read file

	Buffer buf = Buffer_createNull();
	CharString outputBase = CharString_createNull();
	CharString filePathi = CharString_createNull();
	CharString concatFile = CharString_createNull();

	Error err = Error_none();
	DLFile file = (DLFile) { 0 };
	Bool didMakeFile = false;

	_gotoIfError(clean, File_read(input, 1 * SECOND, &buf));
	_gotoIfError(clean, DLFile_readx(buf, encryptionKey, false, &file));

	//Write file

	EFileType type = EFileType_Folder;
	CharString txt = CharString_createConstRefCStr(".txt");

	if(
		(CharString_endsWithStringInsensitive(output, txt) || (args.parameters & EOperationHasParameter_SplitBy)) &&
		file.settings.dataType == EDLDataType_Ascii
	)
		type = EFileType_File;

	_gotoIfError(clean, File_add(output, type, 1 * SECOND));
	didMakeFile = true;

	//Write it as a folder

	if(type == EFileType_Folder) {

		//Append / as base so it's easier to append per file later

		_gotoIfError(clean, CharString_createCopyx(output, &outputBase));

		if(!CharString_endsWithSensitive(outputBase, '/'))
			_gotoIfError(clean, CharString_appendx(&outputBase, '/'));

		CharString bin = CharString_createConstRefCStr(".bin");

		for (U64 i = 0; i < file.entries.length; ++i) {

			DLEntry entry = ((DLEntry*)file.entries.ptr)[i];

			//File name "$base/$(i).+?(isBin ? ".bin" : ".txt")"

			_gotoIfError(clean, CharString_createDecx(i, 0, &filePathi));
			_gotoIfError(clean, CharString_insertStringx(&filePathi, outputBase, 0));

			if(file.settings.dataType == EDLDataType_Data)
				_gotoIfError(clean, CharString_appendStringx(&filePathi, bin))

			else _gotoIfError(clean, CharString_appendStringx(&filePathi, txt));

			Buffer fileDat = 
				file.settings.dataType == EDLDataType_Ascii ? CharString_bufferConst(entry.entryString) : 
				entry.entryBuffer;

			//

			_gotoIfError(clean, File_write(fileDat, filePathi, 1 * SECOND));

			CharString_freex(&filePathi);
		}
	}

	//Write it as a file;
	//e.g. concat all files after it

	else {

		_gotoIfError(clean, CharString_reservex(&concatFile, file.entries.length * 16));

		for (U64 i = 0; i < file.entries.length; ++i) {

			DLEntry entry = ((DLEntry*)file.entries.ptr)[i];
			_gotoIfError(clean, CharString_appendStringx(&concatFile, entry.entryString));

			if(i == file.entries.length - 1)
				break;

			if(args.parameters & EOperationHasParameter_SplitBy) {

				CharString split = CharString_createNull();
				_gotoIfError(clean, ParsedArgs_getArg(args, EOperationHasParameter_SplitByShift, &split));

				_gotoIfError(clean, CharString_appendStringx(&concatFile, split));

			}

			else _gotoIfError(clean, CharString_appendStringx(&concatFile, CharString_newLine()));

		}

		Buffer fileDat = Buffer_createConstRef(concatFile.ptr, CharString_length(concatFile));

		_gotoIfError(clean, File_write(fileDat, output, 1 * SECOND));

		CharString_freex(&concatFile);
	}

clean:

	if(err.genericError)
		Error_printx(err, ELogLevel_Error, ELogOptions_Default);

	if(didMakeFile && err.genericError)
		File_remove(output, 1 * SECOND);

	CharString_freex(&concatFile);
	CharString_freex(&filePathi);
	CharString_freex(&outputBase);
	DLFile_freex(&file);
	Buffer_freex(&buf);

	return err;
}
