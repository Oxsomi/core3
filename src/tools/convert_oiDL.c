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

Bool addFileToDLFile(FileInfo file, ListCharString *names, Error *e_rr) {

	if (file.type == EFileType_Folder)
		return true;

	Bool s_uccess = true;
	CharString copy = CharString_createNull();

	gotoIfError2(clean, CharString_createCopyx(file.path, &copy))
	gotoIfError2(clean, ListCharString_pushBackx(names, copy))

clean:
	return s_uccess;
}

Bool CLI_convertToDL(
	ParsedArgs args, CharString input, FileInfo inputInfo, CharString output, U32 encryptionKey[8], Error *e_rr
) {
	//TODO: EXXCompressionType_Brotli11

	Bool s_uccess = true;
	DLSettings settings = (DLSettings) { .compressionType = EXXCompressionType_None };

	ListBuffer buffers = { 0 };
	ListCharString paths = { 0 };
	ListCharString sortedPaths = { 0 };

	ListCharString split = (ListCharString) { 0 };
	DLFile file = (DLFile) { 0 };
	Buffer fileBuf = Buffer_createNull();
	Buffer buf = Buffer_createNull();
	Buffer res = Buffer_createNull();

	//Encryption type and hash type

	if(args.flags & EOperationFlags_SHA256)
		settings.flags |= EDLSettingsFlags_UseSHA256;

	if(args.parameters & EOperationHasParameter_AES)
		settings.encryptionType = EXXEncryptionType_AES256GCM;

	//Data type

	if ((args.flags & EOperationFlags_UTF8) && (args.flags & EOperationFlags_Ascii)) {
		Log_errorLnx("oiDL can only pick UTF8 or Ascii, not both.");
		retError(clean, Error_invalidParameter(
			0, 0, "CLI_convertToDL() oiDL can only pick UTF8 or Ascii, not both"
		))
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
		retError(clean, Error_invalidOperation(
			3, "CLI_convertToDL() encryptionKey was provided but encryption wasn't used"
		))

	if(!encryptionKey && settings.encryptionType)
		retError(clean, Error_unauthorized(0, "CLI_convertToDL() encryptionKey was needed but not provided"))

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

		retError(clean, Error_invalidParameter(
			0, 1,
			"CLI_convertToDL() oiDL doesn't support splitting by a character if it's not a string list."
		))
	}

	//Validate if string and split string by endline
	//Merge that into a file

	if (inputInfo.type == EFileType_File) {

		gotoIfError3(clean, File_read(input, 1 * SECOND, &buf, e_rr))

		//Create oiDL from text file. Splitting by enter or custom string

		if (args.flags & EOperationFlags_Ascii) {

			CharString str = CharString_createRefSizedConst((const C8*)buf.ptr, Buffer_length(buf), false);

			//Grab split string

			if (args.parameters & EOperationHasParameter_SplitBy) {

				CharString splitBy;
				gotoIfError2(clean, ParsedArgs_getArg(args, EOperationHasParameter_SplitByShift, &splitBy))
				gotoIfError2(clean, CharString_splitStringSensitivex(str, splitBy, &split))
			}

			else gotoIfError2(clean, CharString_splitLinex(str, &split))

			//Create DLFile and write it
			//TODO: When split returns ListCharString, replace with DLFile_createAsciiList

			gotoIfError3(clean, DLFile_createx(settings, &file, e_rr))

			for(U64 i = 0; i < split.length; ++i)
				gotoIfError3(clean, DLFile_addEntryAsciix(&file, split.ptr[i], e_rr))

			gotoIfError3(clean, DLFile_writex(file, &res, e_rr))
			goto write;
		}

		//Add single file entry and create it as normal

		Buffer ref = Buffer_createRefConst(&buf, sizeof(buf));
		gotoIfError2(clean, ListBuffer_pushBackx(&buffers, ref))

		buf = Buffer_createNull();		//Ensure we don't free twice.
	}

	else {

		//Merge folder's children

		gotoIfError2(clean, ListBuffer_reservex(&buffers, 256))

		gotoIfError3(clean, File_foreach(
			input, (FileCallback) addFileToDLFile, &paths,
			!(args.flags & EOperationFlags_NonRecursive),
			e_rr
		))

		//Check if they're all following a linear file order

		CharString basePath = CharString_createNull();

		//To do this, we will create a list that can hold all paths in sorted order

		gotoIfError2(clean, ListCharString_resizex(&sortedPaths, paths.length))

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

			sortedPaths.ptrNonConst[dec] = CharString_createRefStrConst(stri);
		}

		//Keep the sorting as is, since it's not linear

		if(!allLinear) {

			for (U64 i = 0; i < paths.length; ++i) {

				CharString stri = paths.ptr[i];
				gotoIfError3(clean, File_read(stri, 1 * SECOND, &fileBuf, e_rr))
				gotoIfError2(clean, ListBuffer_pushBackx(&buffers, fileBuf))

				fileBuf = Buffer_createNull();
			}

		}

		//Use sorted to insert into buffers

		else for (U64 i = 0; i < sortedPaths.length; ++i) {

			CharString stri = sortedPaths.ptr[i];
			gotoIfError3(clean, File_read(stri, 1 * SECOND, &fileBuf, e_rr))
			gotoIfError2(clean, ListBuffer_pushBackx(&buffers, fileBuf))

			fileBuf = Buffer_createNull();
		}

		//

		ListCharString_freex(&sortedPaths);
		ListCharString_freex(&paths);
	}

	//Now we're left with only data entries
	//Create simple oiDL with all of them

	gotoIfError3(clean, DLFile_createListx(settings, &buffers, &file, e_rr))

	//Convert to binary

	gotoIfError3(clean, DLFile_writex(file, &res, e_rr))

write:
	gotoIfError3(clean, File_write(res, output, 1 * SECOND, e_rr))

clean:

	DLFile_freex(&file);
	ListCharString_freex(&split);
	Buffer_freex(&res);
	Buffer_freex(&buf);
	Buffer_freex(&fileBuf);
	ListBuffer_freeUnderlyingx(&buffers);
	ListCharString_freeUnderlyingx(&paths);
	ListCharString_freex(&sortedPaths);

	return s_uccess;
}

Bool CLI_convertFromDL(
	ParsedArgs args, CharString input, FileInfo inputInfo, CharString output, U32 encryptionKey[8], Error *e_rr
) {

	Bool s_uccess = true;

	Buffer buf = Buffer_createNull();
	CharString outputBase = CharString_createNull();
	CharString filePathi = CharString_createNull();
	CharString concatFile = CharString_createNull();

	DLFile file = (DLFile) { 0 };
	Bool didMakeFile = false;

	//TODO: Batch multiple files

	if (inputInfo.type != EFileType_File) {
		Log_errorLnx("oiDL can only be converted from single file");
		retError(clean, Error_invalidOperation(0, "CLI_convertFromDL() oiDL can only be converted from single file"))
	}

	//Read file

	gotoIfError3(clean, File_read(input, 1 * SECOND, &buf, e_rr))
	gotoIfError3(clean, DLFile_readx(buf, encryptionKey, false, &file, e_rr))

	//Write file

	EFileType type = EFileType_Folder;
	CharString txt = CharString_createRefCStrConst(".txt");

	if(
		(CharString_endsWithStringInsensitive(output, txt, 0) || (args.parameters & EOperationHasParameter_SplitBy)) &&
		file.settings.dataType == EDLDataType_Ascii
	)
		type = EFileType_File;

	gotoIfError3(clean, File_add(output, type, 1 * SECOND, true, e_rr))
	didMakeFile = true;

	//Write it as a folder

	if(type == EFileType_Folder) {

		//Append / as base so it's easier to append per file later

		gotoIfError2(clean, CharString_createCopyx(output, &outputBase))

		if(!CharString_endsWithSensitive(outputBase, '/', 0))
			gotoIfError2(clean, CharString_appendx(&outputBase, '/'))

		CharString bin = CharString_createRefCStrConst(".bin");

		for (U64 i = 0; i < DLFile_entryCount(file); ++i) {

			//File name "$base/$(i).+?(isBin ? ".bin" : ".txt")"

			gotoIfError2(clean, CharString_createDecx(i, 0, &filePathi))
				gotoIfError2(clean, CharString_insertStringx(&filePathi, outputBase, 0))

			if(file.settings.dataType == EDLDataType_Data)
				gotoIfError2(clean, CharString_appendStringx(&filePathi, bin))

			else gotoIfError2(clean, CharString_appendStringx(&filePathi, txt))

			Buffer fileDat =
				file.settings.dataType == EDLDataType_Ascii ? CharString_bufferConst(file.entryStrings.ptr[i]) :
				file.entryBuffers.ptr[i];

			gotoIfError3(clean, File_write(fileDat, filePathi, 1 * SECOND, e_rr))

			CharString_freex(&filePathi);
		}
	}

	//Write it as a file;
	//e.g. concat all files after it

	else {

		gotoIfError2(clean, CharString_reservex(&concatFile, DLFile_entryCount(file) * 16))

		for (U64 i = 0; i < DLFile_entryCount(file); ++i) {

			gotoIfError2(clean, CharString_appendStringx(&concatFile, file.entryStrings.ptr[i]))

			if(i == DLFile_entryCount(file) - 1)
				break;

			if(args.parameters & EOperationHasParameter_SplitBy) {

				CharString split = CharString_createNull();
				gotoIfError2(clean, ParsedArgs_getArg(args, EOperationHasParameter_SplitByShift, &split))

				gotoIfError2(clean, CharString_appendStringx(&concatFile, split))
			}

			else gotoIfError2(clean, CharString_appendStringx(&concatFile, CharString_newLine()))

		}

		Buffer fileDat = Buffer_createRefConst(concatFile.ptr, CharString_length(concatFile));

		gotoIfError3(clean, File_write(fileDat, output, 1 * SECOND, e_rr))

		CharString_freex(&concatFile);
	}

clean:

	if(didMakeFile && !s_uccess)
		File_remove(output, 1 * SECOND, NULL);

	CharString_freex(&concatFile);
	CharString_freex(&filePathi);
	CharString_freex(&outputBase);
	DLFile_freex(&file);
	Buffer_freex(&buf);

	return s_uccess;
}
