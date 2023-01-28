#include "types/error.h"
#include "types/list.h"
#include "types/buffer.h"
#include "formats/oiDL.h"
#include "platforms/file.h"
#include "platforms/log.h"
#include "platforms/ext/listx.h"
#include "platforms/ext/stringx.h"
#include "platforms/ext/formatx.h"
#include "cli.h"

Error addFileToDLFile(FileInfo file, List *buffers) {

	if (file.type == FileType_Folder)
		return Error_none();

	Buffer buffer;
	Error err = File_read(file.path, 2 * SECOND, &buffer);

	if(err.genericError)
		return err;

	if ((err = List_pushBackx(buffers, buffer)).genericError) {
		Buffer_freex(&buffer);
		return err;
	}

	return Error_none();
}

Bool _CLI_convertToDL(ParsedArgs args, String input, FileInfo inputInfo, String output, U32 encryptionKey[8]) {

	//TODO: EXXCompressionType_Brotli11

	DLSettings settings = (DLSettings) { .compressionType = EXXCompressionType_None };

	//Encryption type and hash type

	if(args.flags & EOperationFlags_SHA256)
		settings.flags |= EDLSettingsFlags_UseSHA256;

	if(args.parameters & EOperationHasParameter_AES)
		settings.encryptionType = EXXEncryptionType_AES256GCM;

	//Data type

	if ((args.flags & EOperationFlags_UTF8) && (args.flags & EOperationFlags_Ascii)) {
		Log_debug(String_createConstRefUnsafe("oiDL can only pick UTF8 or Ascii, not both."), ELogOptions_NewLine);
		return false;
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

	//Copying encryption key

	if(settings.encryptionType)
		Buffer_copy(
			Buffer_createRef(settings.encryptionKey, sizeof(settings.encryptionKey)),
			Buffer_createRef(encryptionKey, sizeof(settings.encryptionKey))
		);

	//Check validity

	List buffers = List_createEmpty(sizeof(Buffer));

	if(args.parameters & EOperationHasParameter_SplitBy && !(args.flags & EOperationFlags_Ascii)) {

		//TODO: Support split UTF8

		Log_error(
			String_createConstRefUnsafe(
				"oiDL doesn't support splitting by a character if it's not a string list."
			), 
			ELogOptions_NewLine
		);

		return false;
	}

	Error err = Error_none();
	StringList split = (StringList) { 0 };
	DLFile file = (DLFile) { 0 };
	Buffer buf = Buffer_createNull();
	Buffer res = Buffer_createNull();
	Bool success = false;

	//Validate if string and split string by endline
	//Merge that into a file

	if (inputInfo.type == FileType_File) {

		_gotoIfError(clean, File_read(input, 1 * SECOND, &buf));

		//Create oiDL from text file. Splitting by enter or custom string

		if (args.flags & EOperationFlags_Ascii) {

			String str = String_createConstRef(buf.ptr, Buffer_length(buf));

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
			input, (FileCallback) addFileToDLFile, &buffers, 
			!(args.flags & EOperationFlags_NonRecursive)
		));
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
			String str = String_createConstRefSized(buf.ptr, Buffer_length(buf));
			
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
	success = true;

clean:

	if(err.genericError)
		Error_printx(err, ELogLevel_Error, ELogOptions_Default);

	for(U64 i = 0; i < buffers.length; ++i) {
		Buffer buf = *((Buffer*)buffers.ptr + i);
		Buffer_freex(&buf);
	}

	DLFile_freex(&file);
	StringList_freex(&split);
	Buffer_freex(&res);
	Buffer_freex(&buf);
	List_freex(&buffers);

	return success;
}