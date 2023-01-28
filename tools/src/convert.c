#include "types/time.h"
#include "platforms/log.h"
#include "platforms/ext/errorx.h"
#include "platforms/ext/stringx.h"
#include "platforms/file.h"
#include "cli.h"

Bool _CLI_convert(ParsedArgs args, Bool isTo) {

	Ns start = Time_now();

	//Prepare for convert to/from

	Format f = Format_values[args.format];
	U64 offset = 0;

	Buffer inputArgBuf;
	Error err = List_get(args.args, offset++, &inputArgBuf);

	if (err.genericError) {
		Error_printx(err, ELogLevel_Error, ELogOptions_Default);
		return false;
	}

	String inputArg = *(String*)inputArgBuf.ptr;

	//Check if output is valid

	Buffer outputArgBuf;

	if ((err = List_get(args.args, offset++, &outputArgBuf)).genericError) {
		Error_printx(err, ELogLevel_Error, ELogOptions_Default);
		return false;
	}

	String outputArg = *(String*)outputArgBuf.ptr;

	//TODO: Support multiple files

	//Check if input is valid

	//Check if input file and file type are valid

	FileInfo info = (FileInfo) { 0 };

	if ((err = File_getInfo(inputArg, &info)).genericError) {
		Error_printx(err, ELogLevel_Error, ELogOptions_Default);
		return false;
	}

	if (!(f.flags & EFormatFlags_SupportFiles) && info.type == FileType_File) {

		Log_error(
			String_createConstRefUnsafe("Invalid file passed to convertTo. Only accepting folders."), 
			ELogOptions_NewLine
		);

		return false;
	}

	if (!(f.flags & EFormatFlags_SupportFolders) && info.type == FileType_Folder) {

		Log_error(
			String_createConstRefUnsafe("Invalid folder passed to convertTo. Only accepting files."), 
			ELogOptions_NewLine
		);

		return false;
	}

	//TODO: Parse encryption key

	U32 encryptionKey[8] = { 0 };

	//TODO: Support 

	if(!isTo) {
		Log_debug(String_createConstRefUnsafe("convertFrom not implemented yet"), ELogOptions_NewLine);
		return false;
	}

	//Now convert it

	switch (args.format) {

		case EFormat_oiDL: 
			_CLI_convertToDL(args, inputArg, info, outputArg, encryptionKey);
			break;

		//TODO: oiCA
		
		default:
			Log_debug(String_createConstRefUnsafe("Unimplemented format"), ELogOptions_NewLine);
			return false;
	}

	//Tell CLI users

	U64 timeInMs = (Time_now() - start + MS - 1) / MS;

	Log_debug(String_createConstRefUnsafe("Converted file to oiXX format in "), ELogOptions_None);

	String dec = String_createNull();

	if ((err = String_createDecx(timeInMs, false, &dec)).genericError)
		return true;

	Log_debug(dec, ELogOptions_None);
	String_freex(&dec);

	Log_debug(String_createConstRefUnsafe(" ms"), ELogOptions_NewLine);

	return true;
}

Bool CLI_convertTo(ParsedArgs args) {
	return _CLI_convert(args, true);
}

Bool CLI_convertFrom(ParsedArgs args) {
	return _CLI_convert(args, false);
}
