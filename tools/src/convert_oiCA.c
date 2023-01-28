#include "cli.h"

/* TODO: CAFile

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
*/

/*

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

	if(args.parameters & EOperationHasParameter_AES)
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
}*/
