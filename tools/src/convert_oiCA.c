#include "cli.h"
#include "formats/oiCA.h"
#include "types/error.h"
#include "types/buffer.h"
#include "platforms/file.h"
#include "platforms/log.h"
#include "platforms/ext/stringx.h"
#include "platforms/ext/archivex.h"
#include "platforms/ext/formatx.h"
#include "platforms/ext/bufferx.h"

typedef struct CAFileRecursion {
	Archive *archive;
	String root;
} CAFileRecursion;

Error addFileToCAFile(FileInfo file, CAFileRecursion *caFile) {

	String subPath = String_createNull();
	
	if(!String_cut(file.path, caFile->root.length, 0, &subPath))
		return Error_invalidState(0);

	ArchiveEntry entry = (ArchiveEntry) {
		.path = subPath,
		.type = file.type,
		.timestamp = file.timestamp
	};

	Error err = Error_none();
	String copy = String_createNull();

	if (entry.type == EFileType_File)
		_gotoIfError(clean, File_read(file.path, 1 * SECOND, &entry.data));

	_gotoIfError(clean, String_createCopyx(entry.path, &copy));

	if (file.type == EFileType_File)
		_gotoIfError(clean, Archive_addFilex(caFile->archive, copy, entry.data, entry.timestamp))

	else _gotoIfError(clean, Archive_addDirectoryx(caFile->archive, copy));

	return Error_none();

clean:
	Buffer_freex(&entry.data);
	String_freex(&copy);
	return err;
}

Error _CLI_convertToCA(ParsedArgs args, String input, FileInfo inputInfo, String output, U32 encryptionKey[8]) {

	//TODO: Compression type

	CASettings settings = (CASettings) { .compressionType = EXXCompressionType_None };

	//Dates

	if(args.flags & EOperationFlags_FullDate)
		settings.flags |= ECASettingsFlags_IncludeDate | ECASettingsFlags_IncludeFullDate;

	else if(args.flags & EOperationFlags_Date)
		settings.flags |= ECASettingsFlags_IncludeDate;

	//Encryption type and hash type

	if(args.flags & EOperationFlags_SHA256)
		settings.flags |= ECASettingsFlags_UseSHA256;

	if(args.parameters & EOperationHasParameter_AES)
		settings.encryptionType = EXXEncryptionType_AES256GCM;

	//Compression type

	if(args.flags & EOperationFlags_Uncompressed)
		settings.compressionType = EXXCompressionType_None;

	//else if(args.flags & EOperationFlags_FastCompress)				TODO:
	//	settings.compressionType = ECACompressionType_Brotli1;

	//Copying encryption key

	if(settings.encryptionType)
		Buffer_copy(
			Buffer_createRef(settings.encryptionKey, sizeof(settings.encryptionKey)),
			Buffer_createRef(encryptionKey, sizeof(settings.encryptionKey))
		);

	//Create our entries

	CAFile file = (CAFile) { 0 };
	Error err = Error_none();

	//Archive

	Archive archive = (Archive) { 0 };
	String resolved = String_createNull();
	Buffer res = Buffer_createNull();
	Bool isVirtual = false;

	_gotoIfError(clean, Archive_createx(&archive));
	_gotoIfError(clean, File_resolvex(input, &isVirtual, 0, &resolved));

	if (isVirtual)
		_gotoIfError(clean, Error_invalidOperation(0));

	CAFileRecursion caFileRecursion = (CAFileRecursion) { 
		.archive = &archive, 
		.root = resolved
	};

	_gotoIfError(clean, File_foreach(
		caFileRecursion.root,
		(FileCallback)addFileToCAFile,
		&caFileRecursion,
		!(args.flags & EOperationFlags_NonRecursive)
	));

	//Convert to CAFile and write to file

	_gotoIfError(clean, CAFile_create(settings, archive, &file));
	archive = (Archive) { 0 };	//Archive has been moved to CAFile

	_gotoIfError(clean, CAFile_writex(file, &res));

	_gotoIfError(clean, File_write(res, output, 1 * SECOND));

clean:
	CAFile_freex(&file);
	Archive_freex(&archive);
	String_freex(&resolved);
	Buffer_freex(&res);
	return err;
}

Error _CLI_convertFromCA(ParsedArgs args, String input, FileInfo inputInfo, String output, U32 encryptionKey[8]) {

	//TODO: Batch multiple files

	if (inputInfo.type != EFileType_File) {
		Log_debug(String_createConstRefUnsafe("oiCA can only be converted from single file"), ELogOptions_NewLine);
		return Error_invalidOperation(0);
	}

	//Read file

	Buffer buf = Buffer_createNull();
	String outputPath = String_createNull();
	String loc = String_createNull();

	Error err = Error_none();
	CAFile file = (CAFile) { 0 };
	Bool didMakeFile = false;

	_gotoIfError(clean, File_read(input, 1 * SECOND, &buf));
	_gotoIfError(clean, CAFile_readx(buf, encryptionKey, &file));

	_gotoIfError(clean, File_add(output, EFileType_Folder, 1 * SECOND));
	didMakeFile = true;

	//Grab destination dest

	_gotoIfError(clean, String_createCopyx(output, &outputPath));

	if (String_endsWith(outputPath, '\0', EStringCase_Sensitive))
		outputPath.ptr[outputPath.length - 1] = '/';

	else _gotoIfError(clean, String_appendx(&outputPath, '/'));

	//Write archive to disk

	for (U64 i = 0; i < file.archive.entries.length; ++i) {

		ArchiveEntry ei = ((const ArchiveEntry*)file.archive.entries.ptr)[i];

		_gotoIfError(clean, String_createCopyx(outputPath, &loc));
		_gotoIfError(clean, String_appendStringx(&loc, ei.path));

		if (ei.type == EFileType_Folder) {
			_gotoIfError(clean, File_add(loc, EFileType_Folder, 1 * SECOND));
			continue;
		}

		_gotoIfError(clean, File_write(ei.data, loc, 1 * SECOND));
		String_freex(&loc);
	}

clean:

	if (didMakeFile && err.genericError)
		File_remove(output, 1 * SECOND);

	CAFile_freex(&file);
	Buffer_freex(&buf);
	String_freex(&outputPath);
	String_freex(&loc);
	return err;
}
