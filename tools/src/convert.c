#include "types/time.h"
#include "types/file.h"
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

	if (!(f.flags & EFormatFlags_SupportFiles) && info.type == EFileType_File) {

		Log_error(
			String_createConstRefUnsafe("Invalid file passed to convertTo. Only accepting folders."), 
			ELogOptions_NewLine
		);

		return false;
	}

	if (!(f.flags & EFormatFlags_SupportFolders) && info.type == EFileType_Folder) {

		Log_error(
			String_createConstRefUnsafe("Invalid folder passed to convertTo. Only accepting files."), 
			ELogOptions_NewLine
		);

		return false;
	}

	//Parse encryption key

	U32 encryptionKey[8] = { 0 };

	if (args.parameters & EOperationHasParameter_AES) {

		String key = String_createNull();

		if (
			(ParsedArgs_getArg(args, EOperationHasParameter_AESShift, &key)).genericError || 
			!String_isHex(key)
		) {

			Log_error(
				String_createConstRefUnsafe("Invalid parameter sent to -aes. Expecting key in hex (32 bytes)"), 
				ELogOptions_NewLine
			);

			return false;
		}

		U64 off = String_startsWithString(key, String_createConstRefUnsafe("0x"), EStringCase_Insensitive) ? 2 : 0;

		if (key.length - off != 64) {

			Log_error(
				String_createConstRefUnsafe("Invalid parameter sent to -aes. Expecting 32-byte key in hex"), 
				ELogOptions_NewLine
			);

			return false;
		}

		for (U64 i = off; i + 1 < key.length; ++i) {

			U8 v0 = C8_hex(key.ptr[i]);
			U8 v1 = C8_hex(key.ptr[++i]);

			v0 = (v0 << 4) | v1;
			*((U8*)encryptionKey + ((i - off) >> 1)) = v0;
		}
	}

	//Now convert it

	Bool success = false;

	switch (args.format) {

		case EFormat_oiDL: 

			if(isTo)
				success = _CLI_convertToDL(args, inputArg, info, outputArg, encryptionKey);

			else success = _CLI_convertFromDL(args, inputArg, info, outputArg, encryptionKey);

			break;

		case EFormat_oiCA: 

			if(isTo)
				success = _CLI_convertToCA(args, inputArg, info, outputArg, encryptionKey);

			//else _CLI_convertFromCA(args, inputArg, info, outputArg, encryptionKey);	TODO:

			break;
		
		default:
			Log_debug(String_createConstRefUnsafe("Unimplemented format"), ELogOptions_NewLine);
			return false;
	}

	if (!success) {
		Log_error(String_createConstRefUnsafe("File conversion failed!"), ELogOptions_NewLine);
		return false;
	}

	//Tell CLI users

	U64 timeInMs = (Time_now() - start + MS - 1) / MS;

	Log_debug(String_createConstRefUnsafe("Converted file oiXX format in "), ELogOptions_None);

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
