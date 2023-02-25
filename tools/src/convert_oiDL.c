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
	String copy = String_createNull();

	if((err = String_createCopyx(file.path, &copy)).genericError)
		return err;

	if ((err = List_pushBackx(names, Buffer_createConstRef(&copy, sizeof(copy)))).genericError) {
		String_freex(&copy);
		return err;
	}

	return Error_none();
}

Error _CLI_convertToDL(ParsedArgs args, String input, FileInfo inputInfo, String output, U32 encryptionKey[8]) {

	//TODO: EXXCompressionType_Brotli11

	DLSettings settings = (DLSettings) { .compressionType = EXXCompressionType_None };

	//Encryption type and hash type

	if(args.flags & EOperationFlags_SHA256)
		settings.flags |= EDLSettingsFlags_UseSHA256;

	if(args.parameters & EOperationHasParameter_AES)
		settings.encryptionType = EXXEncryptionType_AES256GCM;

	//Data type

	if ((args.flags & EOperationFlags_UTF8) && (args.flags & EOperationFlags_Ascii)) {
		Log_errorLn("oiDL can only pick UTF8 or Ascii, not both.");
		return Error_invalidParameter(0, 0);
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
		return Error_invalidOperation(3);

	if(!encryptionKey && settings.encryptionType)
		return Error_unauthorized(0);

	//Copying encryption key

	if(settings.encryptionType)
		Buffer_copy(
			Buffer_createRef(settings.encryptionKey, sizeof(settings.encryptionKey)),
			Buffer_createRef(encryptionKey, sizeof(settings.encryptionKey))
		);

	//Check validity

	if(args.parameters & EOperationHasParameter_SplitBy && !(args.flags & EOperationFlags_Ascii)) {

		//TODO: Support split UTF8

		Log_errorLn("oiDL doesn't support splitting by a character if it's not a string list.");
		return Error_invalidParameter(0, 1);
	}

	List buffers = List_createEmpty(sizeof(Buffer));
	List paths = List_createEmpty(sizeof(String));
	List sortedPaths = List_createEmpty(sizeof(String));

	Error err = Error_none();
	StringList split = (StringList) { 0 };
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

			String str = String_createConstRefSized(buf.ptr, Buffer_length(buf), false);

			//Grab split string

			if (args.parameters & EOperationHasParameter_SplitBy) {

				String splitBy;
				_gotoIfError(clean, ParsedArgs_getArg(args, EOperationHasParameter_SplitByShift, &splitBy));
					
				_gotoIfError(clean, String_splitStringx(str, splitBy, EStringCase_Sensitive, &split));
			}

			else _gotoIfError(clean, String_splitLinex(str, &split));

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

		String basePath = String_createNull();

		//To do this, we will create a list that can hold all paths in sorted order

		_gotoIfError(clean, List_resizex(&sortedPaths, paths.length));

		Bool allLinear = true;

		for (U64 i = 0; i < paths.length; ++i) {

			String stri = ((const String*)paths.ptr)[i];
			String_cutBeforeLast(stri, '/', EStringCase_Sensitive, &basePath);

			String tmp = String_createNull();

			if(String_cutAfterLast(basePath, '.', EStringCase_Sensitive, &tmp))
				basePath = tmp;

			U64 dec = 0;
			if (!String_parseDec(basePath, &dec) || dec >> 32) {
				allLinear = false;
				break;
			}

			if (dec >= sortedPaths.length) {
				allLinear = false;
				break;
			}

			String sortedI = ((const String*)sortedPaths.ptr)[dec];

			if (String_length(sortedI)) {
				allLinear = false;
				break;
			}

			((String*)sortedPaths.ptr)[dec] = String_createConstRefSized(stri.ptr, String_length(stri), false);
		}

		//Keep the sorting as is, since it's not linear

		if(!allLinear) {

			for (U64 i = 0; i < paths.length; ++i) {

				String stri = ((const String*)paths.ptr)[i];
				_gotoIfError(clean, File_read(stri, 1 * SECOND, &fileBuf));
				_gotoIfError(clean, List_pushBackx(&buffers, Buffer_createConstRef(&fileBuf, sizeof(fileBuf))));

				fileBuf = Buffer_createNull();
			}

		}

		//Use sorted to insert into buffers

		else for (U64 i = 0; i < sortedPaths.length; ++i) {

			String stri = ((const String*)sortedPaths.ptr)[i];
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

			Buffer buf = *((Buffer*)buffers.ptr + i);
			String str = String_createConstRefSized(buf.ptr, Buffer_length(buf), false);
			
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
		String str = *((const String*)paths.ptr + i);
		String_freex(&str);
	}

	for(U64 i = 0; i < buffers.length; ++i) {
		Buffer buf = *((Buffer*)buffers.ptr + i);
		Buffer_freex(&buf);
	}

	DLFile_freex(&file);
	StringList_freex(&split);
	Buffer_freex(&res);
	Buffer_freex(&buf);
	Buffer_freex(&fileBuf);
	List_freex(&buffers);
	List_freex(&paths);
	List_freex(&sortedPaths);

	return err;
}

Error _CLI_convertFromDL(ParsedArgs args, String input, FileInfo inputInfo, String output, U32 encryptionKey[8]) {

	//TODO: Batch multiple files

	if (inputInfo.type != EFileType_File) {
		Log_errorLn("oiDL can only be converted from single file");
		return Error_invalidOperation(0);
	}

	//Read file

	Buffer buf = Buffer_createNull();
	String outputBase = String_createNull();
	String filePathi = String_createNull();
	String concatFile = String_createNull();

	Error err = Error_none();
	DLFile file = (DLFile) { 0 };
	Bool didMakeFile = false;

	_gotoIfError(clean, File_read(input, 1 * SECOND, &buf));
	_gotoIfError(clean, DLFile_readx(buf, encryptionKey, false, &file));

	//Write file

	EFileType type = EFileType_Folder;
	String txt = String_createConstRefUnsafe(".txt");

	if(
		(
			String_endsWithString(output, txt, EStringCase_Insensitive) ||
			(args.parameters & EOperationHasParameter_SplitBy)
		) &&
		file.settings.dataType == EDLDataType_Ascii
	)
		type = EFileType_File;

	_gotoIfError(clean, File_add(output, type, 1 * SECOND));
	didMakeFile = true;

	//Write it as a folder

	if(type == EFileType_Folder) {

		//Append / as base so it's easier to append per file later

		_gotoIfError(clean, String_createCopyx(output, &outputBase));

		if(!String_endsWith(outputBase, '/', EStringCase_Sensitive))
			_gotoIfError(clean, String_appendx(&outputBase, '/'));

		String bin = String_createConstRefUnsafe(".bin");

		for (U64 i = 0; i < file.entries.length; ++i) {

			DLEntry entry = ((DLEntry*)file.entries.ptr)[i];

			//File name "$base/$(i).+?(isBin ? ".bin" : ".txt")"

			_gotoIfError(clean, String_createDecx(i, 0, &filePathi));
			_gotoIfError(clean, String_insertStringx(&filePathi, outputBase, 0));

			if(file.settings.dataType == EDLDataType_Data)
				_gotoIfError(clean, String_appendStringx(&filePathi, bin))

			else _gotoIfError(clean, String_appendStringx(&filePathi, txt));

			Buffer fileDat = 
				file.settings.dataType == EDLDataType_Ascii ? String_bufferConst(entry.entryString) : 
				entry.entryBuffer;

			//

			_gotoIfError(clean, File_write(fileDat, filePathi, 1 * SECOND));

			String_freex(&filePathi);
		}
	}

	//Write it as a file;
	//e.g. concat all files after it

	else {

		_gotoIfError(clean, String_reservex(&concatFile, file.entries.length * 16));

		for (U64 i = 0; i < file.entries.length; ++i) {

			DLEntry entry = ((DLEntry*)file.entries.ptr)[i];
			_gotoIfError(clean, String_appendStringx(&concatFile, entry.entryString));

			if(i == file.entries.length - 1)
				break;

			if(args.parameters & EOperationHasParameter_SplitBy) {

				String split = String_createNull();
				_gotoIfError(clean, ParsedArgs_getArg(args, EOperationHasParameter_SplitByShift, &split));

				_gotoIfError(clean, String_appendStringx(&concatFile, split));

			}

			else _gotoIfError(clean, String_appendStringx(&concatFile, String_newLine()));

		}

		Buffer fileDat = Buffer_createConstRef(concatFile.ptr, String_length(concatFile));

		_gotoIfError(clean, File_write(fileDat, output, 1 * SECOND));

		String_freex(&concatFile);
	}

clean:

	if(err.genericError)
		Error_printx(err, ELogLevel_Error, ELogOptions_Default);

	if(didMakeFile && err.genericError)
		File_remove(output, 1 * SECOND);

	String_freex(&concatFile);
	String_freex(&filePathi);
	String_freex(&outputBase);
	DLFile_freex(&file);
	Buffer_freex(&buf);

	return err;
}
