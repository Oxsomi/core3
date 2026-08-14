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

//tools/oxc3_cli/convert_oiDL.c

#include "types/base/error.h"
#include "types/base/constants.h"
#include "types/base/string_read.h"
#include "types/base/string_read_helper.h"
#include "types/container/list_basic_types.h"
#include "types/container/string.h"
#include "types/container/string_helper.h"
#include "types/container/buffer.h"
#include "types/container/file_base.h"
#include "types/container/memory_stream.h"
#include "types/container/encryption_stream.h"
#include "formats/oiDL/dl_file.h"
#include "formats/oiDL/dl_entry.h"
#include "formats/oiDL/dl_list.h"
#include "platforms/platform.h"
#include "platforms/logx.h"
#include "platforms/file.h"
#include "tools/oxc3_cli/cli.h"

Bool addFileToDLFile(const FileInfo *file, void *namesGeneric, const Allocator *alloc, Error *e_rr) {

	ListCharString *names = (ListCharString*) namesGeneric;

	if (file->type == EFileType_Folder)
		return true;

	Bool s_uccess = true;
	CharString copy = CharString_createNull();

	gotoIfError3(clean, CharString_createCopy(file->path, alloc, &copy, e_rr));
	gotoIfError3(clean, ListCharString_pushBack(names, copy, alloc, e_rr));

clean:
	return s_uccess;
}

Bool CLI_convertToDL(const CLIConvert *convert, Error *e_rr) {

	if(!convert) return false;

	//TODO: EXXCompressionType_Brotli11

	const Allocator *alloc = Platform_instance->alloc;

	Bool s_uccess = true;
	DLSettings settings = (DLSettings) { .compressionType = EXXCompressionType_None };

	RefPtrType fileHandleType = FileHandle_makeType(alloc);
	RefPtrType fileStreamType = FileStream_makeType(alloc);
	RefPtrType encStreamType = EncryptionStream_makeType(alloc);
	StreamRef *outStream = NULL;

	ListBuffer buffers = { 0 };
	ListCharString paths = { 0 };
	ListCharString sortedPaths = { 0 };

	ListCharString split = (ListCharString) { 0 };
	DLFile file = (DLFile) { 0 };
	Buffer fileBuf = Buffer_createNull();
	Buffer buf = Buffer_createNull();

	//Encryption type and hash type

	if(convert->args->parameters & EOperationHasParameter_AnyAES)
		settings.encryptionType = EXXEncryptionType_AES256GCM;

	//Data type

	if ((convert->args->flags & EOperationFlags_UTF8) && (convert->args->flags & EOperationFlags_Ascii))
		retError(clean, Error_invalidParameter(
			0, 0, "CLI_convertToDL() oiDL can only pick UTF8 or Ascii, not both"
		));

	if(convert->args->flags & EOperationFlags_UTF8)
		settings.dataType = EDLDataType_String;

	else if(convert->args->flags & EOperationFlags_Ascii)
		settings.dataType = EDLDataType_String;

	//Compression type

	if(convert->args->flags & EOperationFlags_Uncompressed)
		settings.compressionType = EXXCompressionType_None;

	//else if(args->flags & EOperationFlags_FastCompress)                TODO:
	//    settings.compressionType = EXXCompressionType_Brotli1;

	//Copying encryption key

	if(settings.encryptionType)
		Buffer_memcpy(
			Buffer_createRef(settings.encryptionKey, sizeof(settings.encryptionKey)),
			Buffer_createRefConst(convert->encKey, sizeof(settings.encryptionKey))
		);

	//Check validity

	if(convert->args->parameters & EOperationHasParameter_SplitBy && !(convert->args->flags & EOperationFlags_Ascii)) {

		//TODO: Support split UTF8, in DLFile first, otherwise split can't work

		retError(clean, Error_invalidParameter(
			0, 1,
			"CLI_convertToDL() oiDL doesn't support splitting by a character if it's not a string list."
		));
	}

	//Validate if string and split string by endline
	//Merge that into a file

	if (convert->inputInfo->type == EFileType_File) {

		gotoIfError3(clean, File_read(convert->input, 100 * MS, 0, 0, &fileHandleType, &buf, e_rr));

		//Create oiDL from text file.
		//Splitting by enter or custom string

		if (convert->args->flags & EOperationFlags_Ascii) {

			CharString str = CharString_createRefSizedConst((const C8*)buf.ptr, Buffer_length(buf), false);

			//Grab split string

			if (convert->args->parameters & EOperationHasParameter_SplitBy) {
				CharString splitBy;
				gotoIfError3(clean, ParsedArgs_getArg(convert->args, EOperationHasParameter_SplitByShift, &splitBy, e_rr));
				gotoIfError3(clean, CharString_splitStringSensitive(
					&(CharStringSplit) { .s = &str, .allocator = alloc, .result = &split }, &splitBy, e_rr
				));
			}

			else gotoIfError3(clean, CharString_splitLine(str, alloc, &split, e_rr));

			//Create DLFile and write it
			//TODO: When split returns ListCharString, replace with DLFile_createAsciiList

			gotoIfError3(clean, DLFile_create(&settings, 0, alloc, &file, e_rr));

			for(U64 i = 0; i < split.length; ++i) {
				CharString entry = split.ptr[i];
				gotoIfError3(clean, DLFile_addEntryString(&file, &entry, alloc, e_rr));
			}

			goto write;
		}

		//Add single file entry and create it as normal

		Buffer ref = Buffer_createRefConst(&buf, sizeof(buf));
		gotoIfError3(clean, ListBuffer_pushBack(&buffers, ref, alloc, e_rr));

		buf = Buffer_createNull();        //Ensure we don't free twice.
	}

	else {

		//Merge folder's children

		gotoIfError3(clean, ListBuffer_reserve(&buffers, 256, alloc, e_rr));

		gotoIfError3(clean, File_foreach(
			convert->input, false, addFileToDLFile, &paths,
			!(convert->args->flags & EOperationFlags_NonRecursive),
			alloc,
			e_rr
		));

		//Check if they're all following a linear file order

		CharString basePath = CharString_createNull();

		//To do this, we will create a list that can hold all paths in sorted order

		gotoIfError3(clean, ListCharString_resize(&sortedPaths, paths.length, alloc, e_rr));

		Bool allLinear = true;

		for (U64 i = 0; i < paths.length; ++i) {

			CharString stri = paths.ptr[i];
			CharString_cutBeforeLastSensitive(&stri, '/', &basePath);

			CharString tmp = CharString_createNull();

			if(CharString_cutAfterLastSensitive(&basePath, '.', &tmp))
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
				gotoIfError3(clean, File_read(&stri, 100 * MS, 0, 0, &fileHandleType, &fileBuf, e_rr));
				gotoIfError3(clean, ListBuffer_pushBack(&buffers, fileBuf, alloc, e_rr));

				fileBuf = Buffer_createNull();
			}
		}

		//Use sorted to insert into buffers

		else for (U64 i = 0; i < sortedPaths.length; ++i) {

			CharString stri = sortedPaths.ptr[i];
			gotoIfError3(clean, File_read(&stri, 100 * MS, 0, 0, &fileHandleType, &fileBuf, e_rr));
			gotoIfError3(clean, ListBuffer_pushBack(&buffers, fileBuf, alloc, e_rr));

			fileBuf = Buffer_createNull();
		}

		ListCharString_free(&sortedPaths, alloc);
		ListCharString_free(&paths, alloc);
	}

	//Now we're left with only data entries
	//Create simple oiDL with all of them

	gotoIfError3(clean, DLFile_createBufferList(&settings, &buffers, alloc, &file, e_rr));

	//Convert to binary

write:

	gotoIfError3(clean, File_openStream(
		convert->output, 1 * SECOND, EFileOpenType_Write, true, &fileHandleType, &fileStreamType, &outStream, e_rr
	));

	{
		U64 startOffset = 0;
		gotoIfError3(clean, DLFile_write(
			&file, alloc, outStream, settings.encryptionType ? &encStreamType : NULL, I32x4_zero(), &startOffset, e_rr
		));
	}

clean:

	if(settings.encryptionType)
		Buffer_clearAllSecure(Buffer_createRef(settings.encryptionKey, sizeof(settings.encryptionKey)));

	RefPtr_dec(&outStream);
	DLFile_free(&file, alloc);
	ListCharString_free(&split, alloc);
	Buffer_free(&buf, alloc);
	Buffer_free(&fileBuf, alloc);
	ListBuffer_freeUnderlying(&buffers, alloc);
	ListCharString_freeUnderlying(&paths, alloc);
	ListCharString_free(&sortedPaths, alloc);

	return s_uccess;
}

Bool CLI_convertFromDL(const CLIConvert *convert, Error *e_rr) {

	if(!convert) return false;

	const Allocator *alloc = Platform_instance->alloc;

	Bool s_uccess = true;

	RefPtrType fileHandleType = FileHandle_makeType(alloc);
	RefPtrType memoryStreamType = MemoryStream_makeType(alloc);
	RefPtrType encStreamType = EncryptionStream_makeType(alloc);
	StreamRef *inStream = NULL;

	Buffer buf = Buffer_createNull();
	CharString outputBase = CharString_createNull();
	CharString filePathi = CharString_createNull();
	CharString concatFile = CharString_createNull();

	DLFile file = (DLFile) { 0 };

	//TODO: Batch multiple files

	if (convert->inputInfo->type != EFileType_File) {
		Log_errorLnx("oiDL can only be converted from single file");
		retError(clean, Error_invalidOperation(0, "CLI_convertFromDL() oiDL can only be converted from single file"));
	}

	//Read file

	gotoIfError3(clean, File_read(convert->input, 100 * MS, 0, 0, &fileHandleType, &buf, e_rr));

	gotoIfError3(clean, MemoryStream_createFromBufferRegion(
		buf, 0, 0, EMemoryStreamFlags_None, &memoryStreamType, &inStream, e_rr
	));

	{
		U64 readOffset = 0;
		Bool hasEncryption = (convert->args->parameters & EOperationHasParameter_AnyAES) != 0;
		gotoIfError3(clean, DLFile_read(
			inStream, &readOffset, hasEncryption ? convert->encKey : NULL, I32x4_zero(), false, false, alloc,
			hasEncryption ? &encStreamType : NULL, &file, e_rr
		));
	}

	//Write file

	EFileType type = EFileType_Folder;
	CharString txt = CharString_createRefCStrConst(".txt");

	if(
		(
			CharString_endsWithStringInsensitive(convert->output, &txt, 0) ||
			(convert->args->parameters & EOperationHasParameter_SplitBy)
		) &&
		file.settings.dataType == EDLDataType_String
	)
		type = EFileType_File;

	//Write it as a folder

	if(type == EFileType_Folder) {

		//Append / as base so it's easier to append per file later

		gotoIfError3(clean, CharString_createCopy(*convert->output, alloc, &outputBase, e_rr));

		if (!CharString_endsWithSensitive(outputBase, '/', 0))
			gotoIfError3(clean, CharString_append(&outputBase, '/', alloc, e_rr));

		CharString bin = CharString_createRefCStrConst(".bin");

		for (U64 i = 0; i < DLFile_entryCount(&file); ++i) {

			//File name "$base/$(i).+?(isBin ? ".bin" : ".txt")"

			gotoIfError3(clean, CharString_createDec(
				&(CharStringCreateNumber) { .v = i, .leadingZeros = 0, .allocator = alloc, .result = &filePathi }, e_rr
			));
			gotoIfError3(clean, CharString_insertString(&filePathi, &outputBase, 0, alloc, e_rr));

			if (file.settings.dataType == EDLDataType_Data) {
				gotoIfError3(clean, CharString_appendString(&filePathi, &bin, alloc, e_rr));
			}

			else gotoIfError3(clean, CharString_appendString(&filePathi, &txt, alloc, e_rr));

			Buffer fileDat =
				file.settings.dataType == EDLDataType_String ? CharString_bufferConst(file.entryStrings.ptr[i]) :
				file.entryBuffers.ptr[i];

			gotoIfError3(clean, File_write(&fileDat, &filePathi, 0, 0, 1 * SECOND, true, &fileHandleType, e_rr));

			CharString_free(&filePathi, alloc);
		}
	}

	//Write it as a file;
	//e.g. concat all files after it

	else {

		const CharString newLine = CharString_newLine();

		gotoIfError3(clean, CharString_reserve(&concatFile, DLFile_entryCount(&file) * 16, alloc, e_rr));

		for (U64 i = 0; i < DLFile_entryCount(&file); ++i) {

			gotoIfError3(clean, CharString_appendString(&concatFile, &file.entryStrings.ptr[i], alloc, e_rr));

			if(i == DLFile_entryCount(&file) - 1)
				break;

			if (convert->args->parameters & EOperationHasParameter_SplitBy) {

				CharString split = CharString_createNull();
				gotoIfError3(clean, ParsedArgs_getArg(convert->args, EOperationHasParameter_SplitByShift, &split, e_rr));

				gotoIfError3(clean, CharString_appendString(&concatFile, &split, alloc, e_rr));
			}

			else gotoIfError3(clean, CharString_appendString(&concatFile, &newLine, alloc, e_rr));
		}

		Buffer fileDat = Buffer_createRefConst(concatFile.ptr, CharString_length(concatFile));
		gotoIfError3(clean, File_write(&fileDat, convert->output, 0, 0, 1 * SECOND, true, &fileHandleType, e_rr));
		CharString_free(&concatFile, alloc);
	}

clean:

	RefPtr_dec(&inStream);
	CharString_free(&concatFile, alloc);
	CharString_free(&filePathi, alloc);
	CharString_free(&outputBase, alloc);
	DLFile_free(&file, alloc);
	Buffer_free(&buf, alloc);

	return s_uccess;
}
