#include "platforms/file.h"
#include "platforms/ext/listx.h"
#include "platforms/ext/errorx.h"
#include "platforms/ext/stringx.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/formatx.h"
#include "platforms/log.h"
#include "types/time.h"

//TODO: Package folder
//TODO: Inspect

typedef enum EOperationHasParameter {

	//As shifts instead of masks

	EOperationHasParameter_FileFormatShift = 0,
	EOperationHasParameter_InputShift,
	EOperationHasParameter_OutputShift,
	EOperationHasParameter_AESShift,

	EOperationHasParameter_SplitByShift,

	EOperationHasParameter_Count,	
	EOperationHasParameter_Start = EOperationHasParameter_FileFormatShift,
	EOperationHasParameter_None			= 0,

	//As mask

	EOperationHasParameter_FileFormat	= 1 << EOperationHasParameter_FileFormatShift,
	EOperationHasParameter_Input		= 1 << EOperationHasParameter_InputShift,
	EOperationHasParameter_Output		= 1 << EOperationHasParameter_OutputShift,
	EOperationHasParameter_AES256			= 1 << EOperationHasParameter_AESShift,

	EOperationHasParameter_SplitBy		= 1 << EOperationHasParameter_SplitByShift

} EOperationHasParameter;

static const C8 *EOperationHasParameter_names[] = {
	"-f",
	"-i",
	"-o",
	"-aes",
	"-split"
};

static const C8 *EOperationHasParameter_descriptions[] = {
	"File format",
	"Input file/folder (relative)",
	"Output file/folder (relative)",
	"Encryption key (32-byte, hex or Nyto)",
	"Split by character (defaulted to newline)"
};

typedef enum EOperationFlags {

	EOperationFlags_None			= 0,

	//Default flags

	EOperationFlags_SHA256			= 1 << 0,
	EOperationFlags_Uncompressed	= 1 << 1,
	EOperationFlags_FastCompress	= 1 << 2,

	EOperationFlags_Default			= 
		EOperationFlags_SHA256 | EOperationFlags_Uncompressed | EOperationFlags_FastCompress,

	//String flags

	EOperationFlags_Ascii			= 1 << 3,
	EOperationFlags_UTF8			= 1 << 4,

	//File flags

	EOperationFlags_FullDate		= 1 << 5,
	EOperationFlags_Date			= 1 << 6,

	//Folder

	EOperationFlags_Recursive		= 1 << 7,

	EOperationFlags_Count			= 8

} EOperationFlags;

static const C8 *EOperationFlags_names[] = {
	"--sha256",
	"--uncompressed",
	"--fast-compress",
	"--ascii",
	"--utf8",
	"--full-date",
	"--date",
	"--not-recursive"
};

static const C8 *EOperationFlags_descriptions[] = {
	"Includes 256-bit hashes instead of 32-bit ones into file if applicable.",
	"Keep the data uncompressed (default is compressed).",
	"Picks the compression algorithm that takes the least time to compress.",
	"Indicates the input files should be threated as ascii. If a file; splits by enter, otherwise 1 entry/file.",
	"Indicates the input files should be threated as UTF8. If a file; splits by enter, otherwise 1 entry/file.",
	"Includes full file timestamp (Ns)",
	"Includes MS-DOS timestamp (YYYY-MM-dd HH-mm-ss (each two seconds))",
	"If folder is selected, blocks recursive file searching. Can be handy if only the direct directory should be included.",
};

typedef enum EOperation {
	EOperation_ConvertTo,
	EOperation_ConvertFrom,
	EOperation_Invalid
} EOperation;

typedef enum EOperationCategory {
	EOperationCategory_Convert,
	EOperationCategory_Invalid
} EOperationCategory;

static const C8 *EOperationCategory_names[] = {
	"convert"
};

static const C8 *EOperationCategory_description[] = {
	"Converting between OxC3 types and non native types."
};

typedef enum EFormat {
	EFormat_oiCA,
	EFormat_oiDL,
	EFormat_Invalid
} EFormat;

typedef struct ParsedArgs {
	EFormat format;
	EOperationFlags flags;
	EOperationHasParameter parameters;
	List args;					//[String]; use parameter flags to extract from low to high
} ParsedArgs;

typedef int (*OperationFunc)(ParsedArgs);

typedef struct Operation {

	EOperationCategory category;
	const C8 *name, *desc;
	OperationFunc func;

} Operation;

typedef enum EFormatFlags {

	EFormatFlags_None			= 0,
	EFormatFlags_SupportFiles	= 1 << 0,
	EFormatFlags_SupportFolders	= 1 << 1

} EFormatFlags;

typedef struct Format {

	const C8 *name;
	EOperationFlags operationFlags;
	EOperationHasParameter requiredParameters, optionalParameters;
	EFormatFlags flags;

} Format;

static Operation Operation_values[2];
static Format Format_values[2];

//End point functions

void showHelp(EOperationCategory category, EOperation op, EFormat f);

Error addFileToDLFile(FileInfo file, List *buffers) {

	if (file.type == FileType_Folder)
		return Error_none();

	Buffer buffer;
	Error err = File_read(file.path, &buffer);

	if(err.genericError)
		return err;

	if ((err = List_pushBackx(buffers, buffer)).genericError) {
		Buffer_freex(&buffer);
		return err;
	}

	return Error_none();
}

typedef struct CAFileRecursion {
	CAFile *file;
	String root;
} CAFileRecursion;

Error addFileToCAFile(FileInfo file, CAFileRecursion *caFile) {

	String subPath;
	
	if(!String_cut(file.path, caFile->root.length + 1, 0, &subPath))
		return Error_invalidState(0);

	CAEntry entry = (CAEntry) {
		.path = subPath,
		.isFolder = file.type == FileType_Folder,
		.timestamp = file.timestamp
	};

	Error err;

	if (!entry.isFolder && (err = File_read(file.path, &entry.data)).genericError)
		return err;

	if((err = CAFile_addEntryx(caFile->file, entry)).genericError) {
		Buffer_freex(&entry.data);
		return err;
	}

	return Error_none();
}

int convertTo(ParsedArgs args) {

	//TODO: Support multiple files

	Ns start = Time_now();

	//Check if input is valid

	Format f = Format_values[args.format];
	U64 offset = 0;

	Buffer inputArgBuf;
	Error err = List_get(args.args, offset++, &inputArgBuf);

	if (err.genericError) {
		Error_printx(err, ELogLevel_Error, ELogOptions_Default);
		return -1;
	}

	String inputArg = String_createConstRef(inputArgBuf.ptr, inputArgBuf.length);

	//Check if output is valid

	Buffer outputArgBuf;

	if ((err = List_get(args.args, offset++, &outputArgBuf)).genericError) {
		Error_printx(err, ELogLevel_Error, ELogOptions_Default);
		return -11;
	}

	String outputArg = String_createConstRef(outputArgBuf.ptr, outputArgBuf.length);

	//Check if input file and file type are valid

	FileInfo info;

	if ((err = File_getInfo(inputArg, &info)).genericError) {
		Error_printx(err, ELogLevel_Error, ELogOptions_Default);
		return -2;
	}

	if (!(f.flags & EFormatFlags_SupportFiles) && info.type == FileType_File) {

		Log_error(
			String_createConstRefUnsafe("Invalid file passed to convertTo. Only accepting folders."), 
			ELogOptions_NewLine
		);

		return -3;
	}

	if (!(f.flags & EFormatFlags_SupportFolders) && info.type == FileType_Folder) {

		Log_error(
			String_createConstRefUnsafe("Invalid folder passed to convertTo. Only accepting files."), 
			ELogOptions_NewLine
		);

		return -4;
	}

	//TODO: Parse encryption key

	U32 encryptionKey[8] = { 0 };

	//Now convert it

	switch (args.format) {

		case EFormat_oiCA: {

			CASettings settings = (CASettings) { .compressionType = EDLCompressionType_Brotli11 };

			//Dates

			if(args.flags & EOperationFlags_FullDate)
				settings.flags |= ECASettingsFlags_IncludeDate | ECASettingsFlags_IncludeFullDate;

			else if(args.flags & EOperationFlags_Date)
				settings.flags |= ECASettingsFlags_IncludeDate;

			//Encryption type and hash type

			if(args.flags & EOperationFlags_SHA256)
				settings.flags |= ECASettingsFlags_UseSHA256;

			if(args.parameters & EOperationHasParameter_AES256)
				settings.encryptionType = ECAEncryptionType_AES256;

			//Compression type

			if(args.flags & EOperationFlags_Uncompressed)
				settings.compressionType = ECACompressionType_None;

			else if(args.flags & EOperationFlags_FastCompress)
				settings.compressionType = ECACompressionType_Brotli1;

			//Copying encryption key

			if(settings.encryptionType)
				Buffer_copy(
					Buffer_createRef(settings.encryptionKey, sizeof(encryptionKey)),
					Buffer_createRef(encryptionKey, sizeof(encryptionKey))
				);

			//Create our entries

			CAFile file;

			if((err = CAFile_createx(settings, &file)).genericError) {
				Error_printx(err, ELogLevel_Error, ELogOptions_Default);
				return -19;
			}

			CAFileRecursion caFileRecursion = (CAFileRecursion) { .file = &file, .root = info.path };

			if (
				(err = File_foreach(
					info.path, (FileCallback) addFileToCAFile, &caFileRecursion, 
					args.flags & EOperationFlags_Recursive
				)).genericError
			) {
				CAFile_freex(&file);
				Error_printx(err, ELogLevel_Error, ELogOptions_Default);
				return -18;
			}

			Buffer res;
			if ((err = CAFile_writex(file, &res)).genericError) {
				CAFile_freex(&file);
				Error_printx(err, ELogLevel_Error, ELogOptions_Default);
				return -20;
			}

			CAFile_freex(&file);

			if ((err = File_write(res, outputArg)).genericError) {
				Buffer_freex(&res);
				Error_printx(err, ELogLevel_Error, ELogOptions_Default);
				return -21;
			}

			Buffer_freex(&res);
			goto end;
		}

		case EFormat_oiDL: {

			DLSettings settings = (DLSettings) { .compressionType = EDLCompressionType_Brotli11 };

			//Encryption type and hash type

			if(args.flags & EOperationFlags_SHA256)
				settings.flags |= EDLSettingsFlags_UseSHA256;

			if(args.parameters & EOperationHasParameter_AES256)
				settings.encryptionType = EDLEncryptionType_AES256;

			//Data type

			if(args.flags & EOperationFlags_Ascii)
				settings.dataType = EDLDataType_Ascii;

			else if(args.flags & EOperationFlags_UTF8)
				settings.dataType = EDLDataType_UTF8;

			//Compression type

			if(args.flags & EOperationFlags_Uncompressed)
				settings.compressionType = EDLCompressionType_None;

			else if(args.flags & EOperationFlags_FastCompress)
				settings.compressionType = EDLCompressionType_Brotli1;

			//Copying encryption key

			if(settings.encryptionType)
				Buffer_copy(
					Buffer_createRef(settings.encryptionKey, sizeof(encryptionKey)),
					Buffer_createRef(encryptionKey, sizeof(encryptionKey))
				);

			//Check validity

			List buffers = List_createEmpty(sizeof(Buffer));

			if(args.flags & EOperationFlags_UTF8) {

				//TODO:

				Log_error(
					String_createConstRefUnsafe("oiDL doesn't support UTF-8 yet, because OxC3 doesn't either."), 
					ELogOptions_NewLine
				);

				return -5;
			}

			if(args.parameters & EOperationHasParameter_SplitBy && !(args.flags & EOperationFlags_Ascii)) {

				Log_error(
					String_createConstRefUnsafe(
						"oiDL doesn't support splitting by a character if it's not a string list."
					), 
					ELogOptions_NewLine
				);

				return -5;
			}

			//Validate if string and split string by endline
			//Merge that into a file

			if (info.type == FileType_File) {

				Buffer buf;
				if ((err = File_read(info.path, &buf)).genericError) {
					Error_printx(err, ELogLevel_Error, ELogOptions_Default);
					return -6;
				}

				//Create oiSL from text file. Splitting by enter or custom string

				if (args.flags & EOperationFlags_Ascii) {

					String str = String_createConstRef(buf.ptr, buf.length);
					StringList split;

					if (args.parameters & EOperationHasParameter_SplitBy) {

						U64 ourLoc = offset;

						for(U64 j = EOperationHasParameter_AESShift; j < EOperationHasParameter_SplitByShift; ++j)
							if((args.parameters >> j) & 1)
								++ourLoc;

						String splitBy;
						Buffer splitByBuf = Buffer_createRef(&splitBy, sizeof(splitBy));

						if ((err = List_get(args.args, ourLoc, &splitByBuf)).genericError) {
							Buffer_freex(&buf);
							Error_printx(err, ELogLevel_Error, ELogOptions_Default);
							return -15;
						}
					
						if ((
							err = String_splitStringx(str, splitBy, EStringCase_Sensitive, &split)
						).genericError) {
							Buffer_freex(&buf);
							Error_printx(err, ELogLevel_Error, ELogOptions_Default);
							return -14;
						}
					}

					else if ((err = String_splitLinex(str, &split)).genericError) {
						Buffer_freex(&buf);
						Error_printx(err, ELogLevel_Error, ELogOptions_Default);
						return -8;
					}

					DLFile file;
					
					if((err = DLFile_createx(settings, &file)).genericError) {
						Buffer_freex(&buf);
						StringList_freex(&split);
						Error_printx(err, ELogLevel_Error, ELogOptions_Default);
						return -9;
					}

					for(U64 i = 0; i < split.length; ++i) 
						if((err = DLFile_addEntryAsciix(&file, split.ptr[i])).genericError) {
							Buffer_freex(&buf);
							DLFile_freex(&file);
							StringList_freex(&split);
							Error_printx(err, ELogLevel_Error, ELogOptions_Default);
							return -10;
						}

					Buffer res;
					if ((err = DLFile_writex(file, &res)).genericError) {
						Buffer_freex(&buf);
						DLFile_freex(&file);
						StringList_freex(&split);
						Error_printx(err, ELogLevel_Error, ELogOptions_Default);
						return -10;
					}

					Buffer_freex(&buf);
					DLFile_freex(&file);
					StringList_freex(&split);

					if ((err = File_write(res, outputArg)).genericError) {
						Buffer_freex(&res);
						Error_printx(err, ELogLevel_Error, ELogOptions_Default);
						return -12;
					}

					Buffer_freex(&res);
					goto end;
				}

				//Add single file entry

				Buffer ref = Buffer_createRef(&buf, sizeof(buf));

				if ((err = List_pushBackx(&buffers, ref)).genericError) {
					Buffer_freex(&buf);
					Error_printx(err, ELogLevel_Error, ELogOptions_Default);
					return -17;
				}
			}

			else {

				//Merge folder's children

				if ((err = List_reservex(&buffers, 256)).genericError) {
					Error_printx(err, ELogLevel_Error, ELogOptions_Default);
					return -19;
				}

				if (
					(err = File_foreach(
						inputArg, (FileCallback) addFileToDLFile, &buffers, 
						args.flags & EOperationFlags_Recursive
					)).genericError
				) {

					for(U64 i = 0; i < buffers.length; ++i) {
						Buffer buf = *((Buffer*)buffers.ptr + i);
						Buffer_freex(&buf);
					}

					List_freex(&buffers);
					Error_printx(err, ELogLevel_Error, ELogOptions_Default);
					return -18;
				}
			}

			//Now we're left with only data entries
			//Create simple oiDL with all of them

			DLFile file;

			if((err = DLFile_createx(settings, &file)).genericError) {

				for(U64 i = 0; i < buffers.length; ++i) {
					Buffer buf = *((Buffer*)buffers.ptr + i);
					Buffer_freex(&buf);
				}

				List_freex(&buffers);

				Error_printx(err, ELogLevel_Error, ELogOptions_Default);
				return -13;
			}

			for(U64 i = 0; i < buffers.length; ++i) 
				if((err = DLFile_addEntryx(&file, *((Buffer*)buffers.ptr + i))).genericError) {

					DLFile_freex(&file);

					for(U64 i = 0; i < buffers.length; ++i) {
						Buffer buf = *((Buffer*)buffers.ptr + i);
						Buffer_freex(&buf);
					}

					List_freex(&buffers);

					Error_printx(err, ELogLevel_Error, ELogOptions_Default);
					return -14;
				}

			Buffer res;
			if ((err = DLFile_writex(file, &res)).genericError) {

				DLFile_freex(&file);

				for(U64 i = 0; i < buffers.length; ++i) {
					Buffer buf = *((Buffer*)buffers.ptr + i);
					Buffer_freex(&buf);
				}

				List_freex(&buffers);

				Error_printx(err, ELogLevel_Error, ELogOptions_Default);
				return -15;
			}

			for(U64 i = 0; i < buffers.length; ++i) {
				Buffer buf = *((Buffer*)buffers.ptr + i);
				Buffer_freex(&buf);
			}

			List_freex(&buffers);
			DLFile_freex(&file);

			if ((err = File_write(res, outputArg)).genericError) {
				Buffer_freex(&res);
				Error_printx(err, ELogLevel_Error, ELogOptions_Default);
				return -16;
			}

			Buffer_freex(&res);
			goto end;
		}
	}

end:

	U64 timeInMs = (Time_now() - start + ms - 1) / ms;

	Log_debug(String_createConstRefUnsafe("Converted file to oiDL from string list in "), ELogOptions_None);

	String dec;

	if ((err = String_createDecx(timeInMs, false, &dec)).genericError)
		return -13;

	Log_debug(dec, ELogOptions_None);
	String_freex(&dec);

	Log_debug(String_createConstRefUnsafe(" ms"), ELogOptions_NewLine);

	return 0;
}

int convertFrom(ParsedArgs args) {
	Log_debug(String_createConstRefUnsafe("convertFrom not implemented yet"), ELogOptions_NewLine);
	return -1;
}

void showHelp(EOperationCategory category, EOperation op, EFormat f) {

	//Show all options

	if(op == EOperation_Invalid) {

		Log_debug(String_createConstRefUnsafe("All operations:"), ELogOptions_NewLine);

		for(U64 i = 0; i < EOperation_Invalid; ++i) {
			Log_debug(String_createConstRefUnsafe(Operation_values[i].name), ELogOptions_None);
			Log_debug(String_createConstRefUnsafe(" -f <format> ...{format dependent syntax}"), ELogOptions_NewLine);
		}

		Log_debug(String_createConstRefUnsafe("All formats:"), ELogOptions_NewLine);

		//TODO: Show formats

		return;
	}

	//Show all options for that operation

	if (f == EFormat_Invalid) {

		Log_debug(String_createConstRefUnsafe("Please use syntax: "), ELogOptions_NewLine);

		Log_debug(String_createConstRefUnsafe(Operation_values[op].name), ELogOptions_None);
		Log_debug(String_createConstRefUnsafe(" -f <format> ...{format dependent syntax}"), ELogOptions_NewLine);

		//TODO: Show formats

		return;
	}

	//TODO: Show all operations for that format
}

//Constants

void initOperations() {

	Format_values[EFormat_oiCA] = (Format) {
		.name = "oiCA",
		.operationFlags = EOperationFlags_Default | EOperationFlags_Date | EOperationFlags_FullDate,
		.optionalParameters = EOperationHasParameter_AES256,
		.requiredParameters = EOperationHasParameter_Input | EOperationHasParameter_Output,
		.flags = EFormatFlags_SupportFolders
	},

	Format_values[EFormat_oiDL] = (Format) { 
		.name = "oiDL",
		.operationFlags = EOperationFlags_Default | EOperationFlags_Ascii | EOperationFlags_UTF8,
		.optionalParameters = EOperationHasParameter_AES256 | EOperationHasParameter_SplitBy,
		.requiredParameters = EOperationHasParameter_Input | EOperationHasParameter_Output,
		.flags = EFormatFlags_SupportFiles | EFormatFlags_SupportFolders
	},

	Operation_values[EOperation_ConvertTo] = (Operation) { 
		.category = EOperationCategory_Convert, 
		.name = "to", 
		.desc = "Converting from a non native file format to a native file format.", 
		.func = &convertTo
	};

	Operation_values[EOperation_ConvertFrom] = (Operation) { 
		.category = EOperationCategory_Convert, 
		.name = "from", 
		.desc = "Converting to a non native file format from a native file format.", 
		.func = &convertFrom
	};
}

int Program_run() {

	initOperations();

	Bool success = true;

	if (Platform_instance.args.length >= 2) {

		//Grab category

		String arg0 = Platform_instance.args.ptr[0];

		EOperationCategory category = EOperationCategory_Invalid;

		for(U64 i = 0; i < EOperationCategory_Invalid; ++i)

			if (String_equalsString(
				arg0, 
				String_createConstRefUnsafe(EOperationCategory_names[i]),
				EStringCase_Insensitive
			)) {
				category = i;
				break;
			}

		if (category == EOperationCategory_Invalid) {
			showHelp(EOperationCategory_Invalid, EOperation_Invalid, EFormat_Invalid);
			return 12;
		}

		//Grab operation

		String arg1 = Platform_instance.args.ptr[1];

		EOperation operation = EOperation_Invalid;

		for(U64 i = 0; i < EOperation_Invalid; ++i)

			if (String_equalsString(
				arg1, 
				String_createConstRefUnsafe(Operation_values[i].name),
				EStringCase_Insensitive
			)) {
				operation = i;
				break;
			}

		//Parse command line options

		if(operation != Operation_values) {

			ParsedArgs args = (ParsedArgs) { 0 };
			args.args = List_createEmpty(sizeof(String));

			Error err = List_reservex(&args.args, 100);

			if (err.genericError) {
				List_freex(&args.args);
				Error_printx(err, ELogLevel_Error, ELogOptions_Default);
				return 3;
			}

			//Grab all flags

			for(U64 i = 0; i < EOperationFlags_Count; ++i)
				for(U64 j = 1; j < Platform_instance.args.length; ++j)

					if (String_equalsString(
						Platform_instance.args.ptr[j],
						String_createConstRefUnsafe(EOperationFlags_names[i]),
						EStringCase_Insensitive
					)) {
						args.flags |= (EOperationFlags)(1 << i);
						break;
					}

			//Grab all params

			args.format = EFormat_Invalid;

			for(U64 i = 0; i < EOperationHasParameter_Count; ++i)
				for(U64 j = 1; j < Platform_instance.args.length; ++j)
					if (String_equalsString(
						Platform_instance.args.ptr[j],
						String_createConstRefUnsafe(EOperationHasParameter_names[i]),
						EStringCase_Insensitive
					)) {

						EOperationHasParameter param = (EOperationHasParameter)(1 << i);

						//Check neighbor and save as list entry

						if (
							j + 1 >= Platform_instance.args.length ||
							String_getAt(Platform_instance.args.ptr[j + 1], 0) == '-'
						) {
							Log_debug(String_createConstRefUnsafe("Parameter is missing argument: "), ELogOptions_None);
							Log_debug(Platform_instance.args.ptr[j], ELogOptions_NewLine);
							List_freex(&args.args);
							return 9;
						}

						else {

							//Parse file format

							if (param == EOperationHasParameter_FileFormat) {

								for (U64 k = 0; k < EFormat_Invalid; ++k)
									if (String_equalsString(
										Platform_instance.args.ptr[j + 1],
										String_createConstRefUnsafe(Format_values[i].name),
										EStringCase_Insensitive
									)) {
										args.format = (EFormat) k;
										break;
									}

								break;
							}

							//Mark as present

							args.parameters |= param;

							//Store param for parsing later

							if ((err = List_insertx(
								&args.args, args.args.length, String_buffer(Platform_instance.args.ptr[j + 1])
							)).genericError) {
								Error_printx(err, ELogLevel_Error, ELogOptions_Default);
								List_freex(&args.args);
								return 4;
							}
						}

						++j;			//Skip next argument
						break;
					}

			//Invalid usage

			if(args.format == EFormat_Invalid) {
				showHelp(category, operation, EFormat_Invalid);
				List_freex(&args.args);
				return 4;
			}

			//Find invalid flags or parameters that couldn't be matched

			for (U64 j = 1; j < Platform_instance.args.length; ++j) {

				if (String_getAt(Platform_instance.args.ptr[j], 0) == '-') {

					if (String_getAt(Platform_instance.args.ptr[j], 1) == '-') {

						U64 i = 0;

						for (; i < EOperationHasParameter_Count; ++i)
							if (String_equalsString(
								Platform_instance.args.ptr[j],
								String_createConstRefUnsafe(EOperationHasParameter_names[i]),
								EStringCase_Insensitive
							))
								break;

						if(i == EOperationHasParameter_Count) {
							Log_debug(String_createConstRefUnsafe("Invalid parameter is present: "), ELogOptions_None);
							Log_debug(Platform_instance.args.ptr[j], ELogOptions_NewLine);
							showHelp(category, operation, args.format);
							List_freex(&args.args);
							return 10;
						}

						++j;
						continue;
					}
					
					U64 i = 0;

					for (; i < EOperationFlags_Count; ++i)
						if (String_equalsString(
							Platform_instance.args.ptr[j],
							String_createConstRefUnsafe(EOperationFlags_names[i]),
							EStringCase_Insensitive
						))
							break;

					if(i == EOperationFlags_Count) {
						Log_debug(String_createConstRefUnsafe("Invalid flag is present: "), ELogOptions_None);
						Log_debug(Platform_instance.args.ptr[j], ELogOptions_NewLine);
						showHelp(category, operation, args.format);
						List_freex(&args.args);
						return 11;
					}
				}

				else {
					Log_debug(String_createConstRefUnsafe("Invalid argument is present: "), ELogOptions_None);
					Log_debug(Platform_instance.args.ptr[j], ELogOptions_NewLine);
					showHelp(category, operation, args.format);
					List_freex(&args.args);
					return 8;
				}
			}

			//Check that parameters passed are valid

			Format f = Format_values[args.format];

			//It must have all required params

			if ((args.parameters & f.requiredParameters) != f.requiredParameters) {
				Log_debug(String_createConstRefUnsafe("Required parameter is missing."), ELogOptions_NewLine);
				showHelp(category, operation, args.format);
				List_freex(&args.args);
				return 5;
			}

			//It has some parameter that we don't support

			if (args.parameters & ~(f.requiredParameters | f.optionalParameters)) {
				Log_debug(String_createConstRefUnsafe("Unsupported parameter is present."), ELogOptions_NewLine);
				showHelp(category, operation, args.format);
				List_freex(&args.args);
				return 6;
			}

			//It has some flag we don't support

			if (args.parameters & ~f.operationFlags) {
				Log_debug(String_createConstRefUnsafe("Unsupported flag is present."), ELogOptions_NewLine);
				showHelp(category, operation, args.format);
				List_freex(&args.args);
				return 7;
			}

			//Now we can enter the function

			int res = Operation_values[operation].func(args);
			List_freex(&args.args);
			return res;
		}
	}

	//Show help

	showHelp(EOperationCategory_Invalid, EOperation_Invalid, EFormat_Invalid);
	return 1;
}

void Program_exit() { }
