#include "formats/oiCA.h"
#include "formats/oiDL.h"
#include "types/list.h"
#include "types/time.h"

/*	TODO: Implement compression and encryption

//File spec (docs/oiCA.md)

typedef enum ECAFlags {

	ECAFlags_None 					= 0,

	ECAFlags_HasHash				= 1 << 0,		//Should be true if compression is on
	ECAFlags_UseSHA256				= 1 << 1,		//Whether SHA256 (1) or CRC32C (0) is used as hash

	//See ECAFileObject

	ECAFlags_FilesHaveDate			= 1 << 2,
	ECAFlags_FilesHaveExtendedDate	= 1 << 3,

	//Indicates ECAFileSizeType. E.g. (ECAFileSizeType)((b0 << 1) | b1)

	ECAFlags_FileSizeType_Shift		= 4,
	ECAFlags_FileSizeType_Mask		= 3

	//ECAFlags_Next					= 1 << 6

} ECAFlags;

typedef struct CAHeader {

	U32 magicNumber;			//oiCA (0x4143696F)

	U8 version;					//major.minor (%10 = minor, /10 = major)
	U8 flags;					//ECAFlags
	U8 type;					//(ECACompressionType << 4) | ECAEncryptionType. Each enum should be <Count
	U8 headerExtensionSize;		//To skip extended data size

	U8 directoryExtensionSize;	//To skip directory extended data
	U8 fileExtensionSize;		//To skip file extended data		
	U16 directoryCount;			//How many base directories are available. Should be < 0xFFFF

	U32 fileCount;				//How many files are available. Should be < 0xFFFF0000

} CAHeader;

static const U32 CAHeader_magic = 0x4143696F;

//A directory points to the parent and to the children
//This allows us to easily access them without having to search all files
//Important is to verify if there are childs linking to the same parent or invalid child offsets

typedef struct CADirectory {

	U16 parentDirectory;		//0xFFFF for root directory, else id of parent directory (can't be self)
	U16 childDirectoryStart;	//Where the child dirs start. 0xFFFF indicates no child directories

	U16 childDirectoryCount;	//Up to 64Ki child directories  
	U16 childFileCount;			//Up to 64Ki child files

	U32 childFileStart;			//Where the child files start. 0xFFFFFFFF indicates no child files

} CADirectory;

typedef enum ECAFileSizeType {

	ECAFileSizeType_U8,
	ECAFileSizeType_U16,
	ECAFileSizeType_U32,
	ECAFileSizeType_U64

} ECAFileSizeType;

//Helper functions

inline Ns CAFile_loadDate(U16 time, U16 date) {
	return Time_date(
		1980 + (date >> 9), 1 + ((date >> 5) & 0xF), date & 0x1F, 
		time >> 11, (time >> 5) & 0x3F, (time & 0x1F) << 1, 0
	);
}

inline Bool CAFile_storeDate(Ns ns, U16 *time, U16 *date) {

	if(!time || !date)
		return false;

	U16 year; U8 month, day, hour, minute, second;
	Bool b = Time_getDate(ns, &year, &month, &day, &hour, &minute, &second, NULL);

	if(!b || year < 1980 || year > (1980 + 0x7F))
		return false;

	*time = (second >> 1) | ((U16)minute << 5) | ((U16)hour << 11);
	*date = day | ((U16)month << 5) | ((U16)(year - 1980) << 11);
	return true;
}

//

Error CAFile_create(CASettings settings, Allocator alloc, CAFile *caFile) {

	if(!caFile)
		return Error_nullPointer(0, 0);

	if(caFile->entries.ptr)
		return Error_invalidOperation(0);

	if(settings.compressionType >= ECACompressionType_Count)
		return Error_invalidParameter(0, 0, 0);

	if(settings.encryptionType >= ECAEncryptionType_Count)
		return Error_invalidParameter(0, 1, 0);

	if(settings.flags & ECASettingsFlags_Invalid)
		return Error_invalidParameter(0, 2, 0);

	if (settings.compressionType == ECAEncryptionType_AES256) {		//Check if we actually input a key

		U32 key[8] = { 0 };

		Bool b = false;
		Error err = Buffer_eq(
			Buffer_createRef(key, sizeof(key)), 
			Buffer_createRef(settings.encryptionKey, sizeof(key)), 
			&b
		);

		if(err.genericError)
			return err;

		if(b)
			return Error_invalidParameter(0, 3, 0);
	}

	caFile->entries = List_createEmpty(sizeof(CAEntry));

	Error err = List_reserve(&caFile->entries, 100, alloc);

	if(err.genericError)
		return err;

	caFile->settings = settings;
	return Error_none();
}

Error CAFile_free(CAFile *caFile, Allocator alloc) {

	if(!caFile || !caFile->entries.ptr)
		return Error_none();

	for (U64 i = 0; i < caFile->entries.length; ++i) {

		CAEntry entry = ((CAEntry*)caFile->entries.ptr)[i];

		Buffer_free(&entry.data, alloc);
		String_free(&entry.path, alloc);
	}

	Error err = List_free(&caFile->entries, alloc);
	*caFile = (CAFile) { 0 };
	return err;
}

Error CAFile_addEntry(CAFile *caFile, CAEntry entry, Allocator alloc);

//We currently only support writing brotli because it's the best 
//(space) compression compared to time to decompress/compress

static const U8 sizeByteType[4] = { 1, 2, 4, 8 };

Error CAFile_write(CAFile caFile, Allocator alloc, Buffer *result) {

	if(!result)
		return Error_nullPointer(2, 0);

	List directories = List_createEmpty(sizeof(String));
	List files = List_createEmpty(sizeof(String));

	Error err;

	if((err = List_reserve(&directories, 128, alloc)).genericError)
		return err;

	if((err = List_reserve(&files, 128, alloc)).genericError) {
		List_free(&directories, alloc);
		return err;
	}

	//Validate CAFile and calculate files and folders

	U64 outputSize = sizeof(CADirectory);		//Excluding header, hash and DLFile

	U64 baseFileHeader = sizeof(U16) + sizeof(U8) + (						//Parent and fileInfo
		!((caFile.settings.flags & ECASettingsFlags_IncludeFullDate) || 
		(caFile.settings.flags & ECASettingsFlags_IncludeDate)) ? 0 :
		(caFile.settings.flags & ECASettingsFlags_IncludeFullDate ? sizeof(Ns) : sizeof(U16) * 2)
	);

	U64 biggestFileSize = 0;

	for (U64 i = 0; i < caFile.entries.length; ++i) {

		CAEntry entry = ((CAEntry*)caFile.entries.ptr)[i];

		if(!entry.path.length)		//Skip empty file paths (just in case there's a root folder)
			continue;

		if (entry.isFolder) {

			if((err = List_pushBack(
				&directories, Buffer_createRef(&entry.path, sizeof(String)), alloc
			)).genericError) {
				List_free(&files, alloc);
				List_free(&directories, alloc);
				return err;
			}

			if(directories.length >= U16_MAX) {
				List_free(&files, alloc);
				List_free(&directories, alloc);
				return Error_outOfBounds(0, 0, 0xFFFF, U16_MAX - 1);
			}

			if(outputSize + sizeof(CADirectory) < outputSize) {
				List_free(&files, alloc);
				List_free(&directories, alloc);
				return Error_overflow(0, 0, outputSize + sizeof(CADirectory), outputSize);
			}

			outputSize += sizeof(CADirectory);
			continue;
		}

		if((err = List_pushBack(
			&files, Buffer_createRef(&entry.path, sizeof(String)), alloc
		)).genericError) {
			List_free(&files, alloc);
			List_free(&directories, alloc);
			return err;
		}

		if(files.length >= U32_MAX - U16_MAX) {
			List_free(&files, alloc);
			List_free(&directories, alloc);
			return Error_outOfBounds(0, 0, U32_MAX - U16_MAX, U32_MAX - U16_MAX - 1);
		}

		if(outputSize + baseFileHeader < outputSize) {
			List_free(&files, alloc);
			List_free(&directories, alloc);
			return Error_overflow(0, 0, outputSize + baseFileHeader, outputSize);
		}

		outputSize += baseFileHeader;
		
		if(outputSize + entry.data.length < outputSize) {
			List_free(&files, alloc);
			List_free(&directories, alloc);
			return Error_overflow(0, 0, outputSize + entry.data.length, outputSize);
		}

		outputSize += entry.data.length;
		biggestFileSize = U64_max(biggestFileSize, entry.data.length);
	}

	//Add the file size types based on the longest file size

	ECAFileSizeType sizeType = biggestFileSize <= U8_MAX ? ECAFileSizeType_U8 : (
		biggestFileSize <= U16_MAX ? ECAFileSizeType_U16 : (
			biggestFileSize <= U32_MAX ? ECAFileSizeType_U32 : 
			ECAFileSizeType_U64
		)
	);

	baseFileHeader += sizeByteType[sizeType];
	U64 sizeTypeBytes = files.length * sizeByteType[sizeType];

	if(outputSize + sizeTypeBytes < outputSize) {
		List_free(&files, alloc);
		List_free(&directories, alloc);
		return Error_overflow(0, 0, outputSize + sizeTypeBytes, outputSize);
	}

	outputSize += sizeTypeBytes;

	//Sort directories and files to ensure our file ids are correct
	//This is because we have full directory names, so it automatically resolves 
	// in a way where we know where our children are (in a linear way)
	//This sort is different than just sortString. It sorts on length first and then alphabetically.
	//This allows us to not have to reorder the files down the road since they're already sorted.

	if (
		(err = List_sortStringAndLength(directories)).genericError || 
		(err = List_sortStringAndLength(files)).genericError
	) {
		List_free(&files, alloc);
		List_free(&directories, alloc);
		return err;
	}

	//Allocate and generate DLFile

	DLFile dlFile;
	DLSettings dlSettings = (DLSettings) { .dataType = EDLDataType_Ascii };

	if((err = DLFile_create(dlSettings, alloc, &dlFile)).genericError) {
		List_free(&files, alloc);
		List_free(&directories, alloc);
		return err;
	}

	for(U64 i = 0; i < directories.length; ++i) {

		String dir = ((String*)directories.ptr)[i];
		String dirName = dir;

		String_cutBeforeLast(dir, '/', EStringCase_Sensitive, &dirName);

		if ((err = DLFile_addEntryAscii(&dlFile, dirName, alloc)).genericError) {
			DLFile_free(&dlFile, alloc);
			List_free(&files, alloc);
			List_free(&directories, alloc);
			return err;
		}
	}

	for(U64 i = 0; i < files.length; ++i) {

		String file = ((String*)files.ptr)[i];
		String fileName = file;

		String_cutBeforeLast(file, '/', EStringCase_Sensitive, &fileName);

		if ((err = DLFile_addEntryAscii(&dlFile, file, alloc)).genericError) {
			DLFile_free(&dlFile, alloc);
			List_free(&files, alloc);
			List_free(&directories, alloc);
			return err;
		}
	}

	//Complete DLFile as buffer

	Buffer dlFileBuffer;
	if((err = DLFile_write(dlFile, alloc, &dlFileBuffer)).genericError) {
		DLFile_free(&dlFile, alloc);
		List_free(&files, alloc);
		List_free(&directories, alloc);
		return err;
	}

	DLFile_free(&dlFile, alloc);

	//Build up final output buffer
	//DLFile
	//CADirectory[]
	//CAFileObject[]
	//U8[sum(file[i].data)]

	if (dlFileBuffer.length + outputSize < outputSize) {
		Buffer_free(&dlFileBuffer, alloc);
		List_free(&files, alloc);
		List_free(&directories, alloc);
		return Error_overflow(0, 0, dlFileBuffer.length + outputSize, outputSize);
	}

	Buffer outputBuffer;
	if ((err = Buffer_createUninitializedBytes(dlFileBuffer.length + outputSize, alloc, &outputBuffer)).genericError) {
		List_free(&dlFileBuffer, alloc);
		List_free(&files, alloc);
		List_free(&directories, alloc);
		return err;
	}

	//Append DLFile

	Buffer outputBufferIt = outputBuffer;

	if ((err = Buffer_appendBuffer(&outputBufferIt, dlFileBuffer)).genericError) {
		List_free(&outputBuffer, alloc);
		List_free(&dlFileBuffer, alloc);
		List_free(&files, alloc);
		List_free(&directories, alloc);
		return err;
	}

	List_free(&dlFileBuffer, alloc);

	//Append CADirectory[]

	CADirectory *dirPtr = (CADirectory*)outputBufferIt.ptr, *dirPtrRoot = dirPtr;
	++dirPtr;		//Exclude root

	for (U64 i = 0; i < directories.length; ++i) {

		String dir = ((String*)directories.ptr)[i];
		U64 it = String_findLast(dir, '/', EStringCase_Sensitive);

		U16 parent = U16_MAX;

		//Add to parent directory

		if (it != dir.length) {

			//We need to find the real parent.
			//Since we're sorted alphabetically, we are able to look back from our current directory
			//If we encounter something that doesn't start with our basepath/ then we know we are missing this dir
			//And this means we somehow messed with the input data

			String baseDir, realParentDir;
			if (
				!String_cut(dir, 0, it, &realParentDir) || 
				!String_cut(dir, 0, it + 1, &baseDir)
			) {
				List_free(&outputBuffer, alloc);
				List_free(&files, alloc);
				List_free(&directories, alloc);
				return err;
			}

			for(U64 j = i - 1; j != U64_MAX; --j)

				if (!String_startsWithString(((String*)directories.ptr)[i], baseDir, EStringCase_Sensitive)) {

					//We found the parent directory

					if (String_equalsString(
						((String*)directories.ptr)[i], realParentDir, EStringCase_Sensitive
					)) {
						parent = (U16) j;
						break;
					}

					//Or we didn't

					break;
				}

			if(parent == U16_MAX) {
				List_free(&outputBuffer, alloc);
				List_free(&files, alloc);
				List_free(&directories, alloc);
				return Error_invalidState(0);
			}

			//Update parent

			if (dirPtr[parent].childDirectoryStart == U16_MAX)
				dirPtr[parent].childDirectoryStart = i;

			++dirPtr[parent].childDirectoryCount;
		}

		//Add to root

		else {

			if (dirPtrRoot->childDirectoryStart == U16_MAX)
				dirPtrRoot->childDirectoryStart = i;

			++dirPtrRoot->childDirectoryCount;
		}

		//Add directory

		dirPtr[i] = (CADirectory) {
			.childDirectoryStart = U16_MAX,
			.childFileStart = U32_MAX,
			.parentDirectory = parent
		};
	}

	//Append CAFileObject[]

	U8 *fileObjectPtr = (U8*)(dirPtr + directories.length);
	U8 *fileDataPtrIt = fileObjectPtr + baseFileHeader * files.length;

	for (U64 i = 0; i < files.length; ++i) {

		String file = ((String*)files.ptr)[i];
		U64 it = String_findLast(file, '/', EStringCase_Sensitive);

		U16 parent = U16_MAX;

		//Add to parent directory

		if (it != file.length) {

			//We need to find the real parent.
			//Since we're sorted alphabetically, we are able to look back from our current directory
			//If we encounter something that doesn't start with our basepath then we know we are missing this dir
			//And this means we somehow messed with the input data

			String baseDir, realParentDir;
			if (
				!String_cut(file, 0, it, &realParentDir) || 
				!String_cut(file, 0, it + 1, &baseDir)
			) {
				List_free(&outputBuffer, alloc);
				List_free(&files, alloc);
				List_free(&directories, alloc);
				return err;
			}
			
			for(U64 j = directories.length - 1; j != U64_MAX; --j)

				if (String_equalsString(
					((String*)directories.ptr)[i], realParentDir, EStringCase_Sensitive
				)) {
					parent = (U16) j;
					break;
				}

			if(parent == U16_MAX) {
				List_free(&outputBuffer, alloc);
				List_free(&files, alloc);
				List_free(&directories, alloc);
				return Error_invalidState(0);
			}

			//Update parent

			if (dirPtr[parent].childFileStart == U32_MAX)
				dirPtr[parent].childFileStart = i;

			if(dirPtr[parent].childFileCount == U16_MAX) {
				List_free(&outputBuffer, alloc);
				List_free(&files, alloc);
				List_free(&directories, alloc);
				return Error_outOfBounds(0, 1, U16_MAX + 1, U16_MAX);
			}

			++dirPtr[parent].childFileCount;
		}

		else {

			//Update parent

			if (dirPtrRoot->childFileStart == U32_MAX)
				dirPtrRoot->childFileStart = i;

			if(dirPtrRoot->childFileCount == U16_MAX) {
				List_free(&outputBuffer, alloc);
				List_free(&files, alloc);
				List_free(&directories, alloc);
				return Error_outOfBounds(0, 1, U16_MAX + 1, U16_MAX);
			}

			++dirPtrRoot->childFileCount;
		}

		//Find corresponding file with name

		CAEntry *entry = NULL;

		for(U64 i = 0; i < caFile.entries.length; ++i)
			if (String_equalsString(
				((CAEntry*)caFile.entries.ptr + i)->path, file, EStringCase_Sensitive
			)) {
				entry = (CAEntry*)caFile.entries.ptr + i;
				break;
			}

		if (!entry) {
			List_free(&outputBuffer, alloc);
			List_free(&files, alloc);
			List_free(&directories, alloc);
			return Error_invalidState(2);
		}

		//Add file

		U8 *filePtr = fileObjectPtr + baseFileHeader * i;

		*(U16*)filePtr = parent;
		filePtr += sizeof(U16);

		if (
			(caFile.settings.flags & ECASettingsFlags_IncludeFullDate) ||
			(caFile.settings.flags & ECASettingsFlags_IncludeDate)
		) {

			if(caFile.settings.flags & ECASettingsFlags_IncludeFullDate) {
				*(Ns*)filePtr = entry->timestamp;
				filePtr += sizeof(Ns);
			}

			else if(!CAFile_storeDate(entry->timestamp, (U16*)filePtr + 1, (U16*)filePtr)) {
				List_free(&outputBuffer, alloc);
				List_free(&files, alloc);
				List_free(&directories, alloc);
				return Error_invalidState(1);
			}

			else filePtr += sizeof(U16) * 2;
		}

		U64 fileSize = entry->data.length;

		switch (sizeType) {
			case ECAFileSizeType_U8:	*(U8*)filePtr = (U8) fileSize;		break;
			case ECAFileSizeType_U16:	*(U16*)filePtr = (U16) fileSize;	break;
			case ECAFileSizeType_U32:	*(U32*)filePtr = (U32) fileSize;	break;
			default:					*(U64*)filePtr = fileSize;
		}

		//Append file data

		Buffer_copy(Buffer_createRef(fileDataPtrIt, entry->data.length), entry->data);
		fileDataPtrIt += fileSize;
	}

	//We're done with processing files and folders

	U16 dirCount = (U16) directories.length;
	U32 fileCount = (U32) files.length;

	List_free(&files, alloc);
	List_free(&directories, alloc);

	//Hash

	U32 hash[8];

	if(caFile.settings.encryptionType || caFile.settings.compressionType) {

		if (caFile.settings.flags & ECASettingsFlags_UseSHA256)
			Buffer_sha256(outputBuffer, hash);
		
		else hash[0] = Buffer_crc32c(outputBuffer);
	}

	//Compress

	Buffer compressedOutput;

	if (caFile.settings.compressionType != ECACompressionType_None) {

		BufferCompressionType comprType = 
			caFile.settings.compressionType == ECACompressionType_Brotli1 ? BufferCompressionType_Brotli1 :
			BufferCompressionType_Brotli11;

		if ((err = Buffer_compress(outputBuffer, comprType, alloc, &compressedOutput)).genericError) {
			List_free(&outputBuffer, alloc);
			return err;
		}

		List_free(&outputBuffer, alloc);
	}

	else compressedOutput = outputBuffer;

	//Encrypt

	if (caFile.settings.encryptionType != ECAEncryptionType_None) {

		if ((err = Buffer_encrypt(
			compressedOutput, BufferEncryptionType_AES256, caFile.settings.encryptionKey, alloc
		)).genericError) {
			List_free(&compressedOutput, alloc);
			return err;
		}
	}

	//Prepend header and hash

	U8 header[sizeof(CAHeader) + sizeof(hash)], *headerIt = header;

	*((CAHeader*)header) = (CAHeader) {

		.magicNumber = CAHeader_magic,

		.version = 10,		//1.0

		.flags = (U8) (

			(
				caFile.settings.encryptionType || caFile.settings.compressionType ? ECAFlags_HasHash | (
					caFile.settings.flags & ECASettingsFlags_UseSHA256 ? ECAFlags_UseSHA256 : 
					ECAFlags_None
				) : 
				ECAFlags_None
			) |

			(caFile.settings.flags & ECASettingsFlags_IncludeDate ? ECAFlags_FilesHaveDate : ECAFlags_None) |

			(
				caFile.settings.flags & ECASettingsFlags_IncludeFullDate ? 
				ECAFlags_FilesHaveDate | ECAFlags_FilesHaveExtendedDate : ECAFlags_None
			) |

			(sizeType << ECAFlags_FileSizeType_Shift)
		),

		.type = (dlFile.settings.compressionType << 4) | dlFile.settings.encryptionType,

		.directoryCount = (U16) dirCount,
		.fileCount = (U32) fileCount
	};

	headerIt += sizeof(CAHeader);

	if (caFile.settings.encryptionType || caFile.settings.compressionType)
		Buffer_copy(
			Buffer_createRef(headerIt, sizeof(hash)), 
			Buffer_createRef(hash, sizeof(hash))
		);

	Buffer realHeader = 
		Buffer_createRef(
			header, 
			sizeof(CAHeader) + (
				caFile.settings.encryptionType || caFile.settings.compressionType ? (
					caFile.settings.flags & ECASettingsFlags_UseSHA256 ? sizeof(hash) : sizeof(hash[0])
				) : 0
			)
		);

	err = Buffer_combine(realHeader, compressedOutput, alloc, result);
	List_free(&compressedOutput, alloc);
	return err;
}

Error CAFile_read(Buffer file, Allocator alloc, CAFile *caFile);

*/
