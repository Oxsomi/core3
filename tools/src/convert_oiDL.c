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

#include "platforms/ext/listx_impl.h"
#include "types/error.h"
#include "types/list.h"
#include "types/buffer.h"
#include "formats/oiDL.h"
#include "platforms/log.h"
#include "platforms/file.h"
#include "platforms/ext/stringx.h"
#include "platforms/ext/formatx.h"
#include "platforms/ext/errorx.h"
#include "platforms/ext/bufferx.h"
#include "cli.h"

Error addFileToDLFile(FileInfo file, ListCharString *names) {

	if (file.type == EFileType_Folder)
		return Error_none();

	Error err;
	CharString copy = CharString_createNull();

	if((err = CharString_createCopyx(file.path, &copy)).genericError)
		return err;

	if ((err = ListCharString_pushBackx(names, copy)).genericError) {
		CharString_freex(&copy);
		return err;
	}

	return Error_none();
}

Error CLI_convertToDL(ParsedArgs args, CharString input, FileInfo inputInfo, CharString output, U32 encryptionKey[8]) {

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
		return Error_invalidParameter(
			0, 0, "CLI_convertToDL() oiDL can only pick UTF8 or Ascii, not both"
		);
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
		return Error_invalidOperation(
			3, "CLI_convertToDL() encryptionKey was provided but encryption wasn't used"
		);

	if(!encryptionKey && settings.encryptionType)
		return Error_unauthorized(0, "CLI_convertToDL() encryptionKey was needed but not provided");

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
			0, 1,
			"CLI_convertToDL() oiDL doesn't support splitting by a character if it's not a string list."
		);
	}

	ListBuffer buffers = { 0 };
	ListCharString paths = { 0 };
	ListCharString sortedPaths = { 0 };

	Error err = Error_none();
	ListCharString split = (ListCharString) { 0 };
	DLFile file = (DLFile) { 0 };
	Buffer fileBuf = Buffer_createNull();
	Buffer buf = Buffer_createNull();
	Buffer res = Buffer_createNull();

	//Validate if string and split string by endline
	//Merge that into a file

	if (inputInfo.type == EFileType_File) {

		gotoIfError(clean, File_read(input, 1 * SECOND, &buf))

		//Create oiDL from text file. Splitting by enter or custom string

		if (args.flags & EOperationFlags_Ascii) {

			CharString str = CharString_createRefSizedConst((const C8*)buf.ptr, Buffer_length(buf), false);

			//Grab split string

			if (args.parameters & EOperationHasParameter_SplitBy) {

				CharString splitBy;
				gotoIfError(clean, ParsedArgs_getArg(args, EOperationHasParameter_SplitByShift, &splitBy))
				gotoIfError(clean, CharString_splitStringSensitivex(str, splitBy, &split))
			}

			else gotoIfError(clean, CharString_splitLinex(str, &split))

			//Create DLFile and write it
			//TODO: When split returns ListCharString, replace with DLFile_createAsciiList

			gotoIfError(clean, DLFile_createx(settings, &file))

			for(U64 i = 0; i < split.length; ++i)
				gotoIfError(clean, DLFile_addEntryAsciix(&file, split.ptr[i]))

			gotoIfError(clean, DLFile_writex(file, &res))
			goto write;
		}

		//Add single file entry and create it as normal

		Buffer ref = Buffer_createRefConst(&buf, sizeof(buf));
		gotoIfError(clean, ListBuffer_pushBackx(&buffers, ref))

		buf = Buffer_createNull();		//Ensure we don't free twice.
	}

	else {

		//Merge folder's children

		gotoIfError(clean, ListBuffer_reservex(&buffers, 256))

		gotoIfError(clean, File_foreach(
			input, (FileCallback) addFileToDLFile, &paths,
			!(args.flags & EOperationFlags_NonRecursive)
		))

		//Check if they're all following a linear file order

		CharString basePath = CharString_createNull();

		//To do this, we will create a list that can hold all paths in sorted order

		gotoIfError(clean, ListCharString_resizex(&sortedPaths, paths.length))

		Bool allLinear = true;

		for (U64 i = 0; i < paths.length; ++i) {

			CharString stri = paths.ptr[i];
			CharString_cutBeforeLastSensitive(stri, '/', &basePath);

			CharString tmp = CharString_createNull();

			if(CharString_cutAfterLastSensitive(basePath, '.', &tmp))
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

			CharString sortedI = sortedPaths.ptr[dec];

			if (CharString_length(sortedI)) {
				allLinear = false;
				break;
			}

			sortedPaths.ptrNonConst[dec] = CharString_createRefSizedConst(
				stri.ptr, CharString_length(stri), false
			);
		}

		//Keep the sorting as is, since it's not linear

		if(!allLinear) {

			for (U64 i = 0; i < paths.length; ++i) {

				CharString stri = paths.ptr[i];
				gotoIfError(clean, File_read(stri, 1 * SECOND, &fileBuf))
				gotoIfError(clean, ListBuffer_pushBackx(&buffers, fileBuf))

				fileBuf = Buffer_createNull();
			}

		}

		//Use sorted to insert into buffers

		else for (U64 i = 0; i < sortedPaths.length; ++i) {

			CharString stri = sortedPaths.ptr[i];
			gotoIfError(clean, File_read(stri, 1 * SECOND, &fileBuf))
			gotoIfError(clean, ListBuffer_pushBackx(&buffers, fileBuf))

			fileBuf = Buffer_createNull();
		}

		//

		ListCharString_freex(&sortedPaths);
		ListCharString_freex(&paths);
	}

	//Now we're left with only data entries
	//Create simple oiDL with all of them

	gotoIfError(clean, DLFile_createListx(settings, &buffers, &file))

	//Convert to binary

	gotoIfError(clean, DLFile_writex(file, &res))

write:

	gotoIfError(clean, File_write(res, output, 1 * SECOND))

clean:

	if(err.genericError)
		Error_printx(err, ELogLevel_Error, ELogOptions_Default);

	for(U64 i = 0; i < buffers.length; ++i) {
		Buffer bufi = buffers.ptr[i];
		Buffer_freex(&bufi);
	}

	DLFile_freex(&file);
	ListCharString_freex(&split);
	Buffer_freex(&res);
	Buffer_freex(&buf);
	Buffer_freex(&fileBuf);
	ListBuffer_freex(&buffers);
	ListCharString_freeUnderlyingx(&paths);
	ListCharString_freex(&sortedPaths);

	return err;
}

Error CLI_convertFromDL(ParsedArgs args, CharString input, FileInfo inputInfo, CharString output, U32 encryptionKey[8]) {

	//TODO: Batch multiple files

	if (inputInfo.type != EFileType_File) {
		Log_errorLnx("oiDL can only be converted from single file");
		return Error_invalidOperation(0, "CLI_convertFromDL() oiDL can only be converted from single file");
	}

	//Read file

	Buffer buf = Buffer_createNull();
	CharString outputBase = CharString_createNull();
	CharString filePathi = CharString_createNull();
	CharString concatFile = CharString_createNull();

	Error err = Error_none();
	DLFile file = (DLFile) { 0 };
	Bool didMakeFile = false;

	gotoIfError(clean, File_read(input, 1 * SECOND, &buf))
	gotoIfError(clean, DLFile_readx(buf, encryptionKey, false, &file))

	//Write file

	EFileType type = EFileType_Folder;
	CharString txt = CharString_createRefCStrConst(".txt");

	if(
		(CharString_endsWithStringInsensitive(output, txt) || (args.parameters & EOperationHasParameter_SplitBy)) &&
		file.settings.dataType == EDLDataType_Ascii
	)
		type = EFileType_File;

	gotoIfError(clean, File_add(output, type, 1 * SECOND))
	didMakeFile = true;

	//Write it as a folder

	if(type == EFileType_Folder) {

		//Append / as base so it's easier to append per file later

		gotoIfError(clean, CharString_createCopyx(output, &outputBase))

		if(!CharString_endsWithSensitive(outputBase, '/'))
			gotoIfError(clean, CharString_appendx(&outputBase, '/'))

		CharString bin = CharString_createRefCStrConst(".bin");

		for (U64 i = 0; i < DLFile_entryCount(file); ++i) {

			//File name "$base/$(i).+?(isBin ? ".bin" : ".txt")"

			gotoIfError(clean, CharString_createDecx(i, 0, &filePathi))
			gotoIfError(clean, CharString_insertStringx(&filePathi, outputBase, 0))

			if(file.settings.dataType == EDLDataType_Data)
				gotoIfError(clean, CharString_appendStringx(&filePathi, bin))

			else gotoIfError(clean, CharString_appendStringx(&filePathi, txt))

			Buffer fileDat =
				file.settings.dataType == EDLDataType_Ascii ? CharString_bufferConst(file.entryStrings.ptr[i]) :
				file.entryBuffers.ptr[i];

			//

			gotoIfError(clean, File_write(fileDat, filePathi, 1 * SECOND))

			CharString_freex(&filePathi);
		}
	}

	//Write it as a file;
	//e.g. concat all files after it

	else {

		gotoIfError(clean, CharString_reservex(&concatFile, DLFile_entryCount(file) * 16))

		for (U64 i = 0; i < DLFile_entryCount(file); ++i) {

			gotoIfError(clean, CharString_appendStringx(&concatFile, file.entryStrings.ptr[i]))

			if(i == DLFile_entryCount(file) - 1)
				break;

			if(args.parameters & EOperationHasParameter_SplitBy) {

				CharString split = CharString_createNull();
				gotoIfError(clean, ParsedArgs_getArg(args, EOperationHasParameter_SplitByShift, &split))

				gotoIfError(clean, CharString_appendStringx(&concatFile, split))
			}

			else gotoIfError(clean, CharString_appendStringx(&concatFile, CharString_newLine()))

		}

		Buffer fileDat = Buffer_createRefConst(concatFile.ptr, CharString_length(concatFile));

		gotoIfError(clean, File_write(fileDat, output, 1 * SECOND))

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
