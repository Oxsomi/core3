#include "formats/oiCA.h"
#include "formats/oiDL.h"
#include "types/file.h"
#include "types/list.h"
#include "types/time.h"
#include "types/allocator.h"
#include "types/error.h"
#include "types/buffer.h"

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
	ECAFlags_FileSizeType_Mask		= 3,

	//Chunk size of AES for multi threading. 0 = none, 1 = 10MiB, 2 = 100MiB, 3 = 500MiB

	ECAFlags_UseAESChunksA		= 1 << 6,
	ECAFlags_UseAESChunksB		= 1 << 7

} ECAFlags;

typedef struct CAHeader {

	U32 magicNumber;			//oiCA (0x4143696F)

	U8 version;					//major.minor (%10 = minor, /10 = major (+1 = real major))
	U8 flags;					//ECAFlags
	U8 type;					//(EXXCompressionType << 4) | EXXEncryptionType. Each enum should be <Count
	U8 headerExtensionSize;		//To skip extended data size. Highest bit is b0 of uncompressed size type.

	U8 directoryExtensionSize;	//To skip directory extended data. Highest bit is b1 of uncompressed size type.
	U8 fileExtensionSize;		//To skip file extended data		
	U16 directoryCount;			//How many base directories are available. Should be < 0xFFFF

	U32 fileCount;				//How many files are available. Should be < 0xFFFF0000

} CAHeader;

static const U32 CAHeader_MAGIC = 0x4143696F;

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

Error CAFile_create(CASettings settings, Archive archive, CAFile *caFile) {

	if(!caFile)
		return Error_nullPointer(0, 0);

	if(!archive.entries.ptr)
		return Error_nullPointer(1, 0);

	if(settings.compressionType >= EXXCompressionType_Count)
		return Error_invalidParameter(0, 0, 0);

	if(settings.compressionType > EXXCompressionType_None)
		return Error_unsupportedOperation(0);					//TODO: Support compression

	if(settings.encryptionType >= EXXEncryptionType_Count)
		return Error_invalidParameter(0, 1, 0);

	if(settings.flags & ECASettingsFlags_Invalid)
		return Error_invalidParameter(0, 2, 0);

	caFile->archive = archive;
	caFile->settings = settings;
	return Error_none();
}

Bool CAFile_free(CAFile *caFile, Allocator alloc) {

	if(!caFile || !caFile->archive.entries.ptr)
		return true;

	Bool b = Archive_free(&caFile->archive, alloc);
	*caFile = (CAFile) { 0 };
	return b;
}

ECompareResult sortParentCountAndFileNames(String *a, String *b) {

	U64 foldersA = String_countAll(*a, '/', EStringCase_Sensitive);
	U64 foldersB = String_countAll(*b, '/', EStringCase_Sensitive);

	//We wanna sort on folder count first
	//This ensures the root dirs are always at [0,N] and their children at [N, N+M], etc.

	if (foldersA < foldersB)
		return ECompareResult_Lt;

	if (foldersA > foldersB)
		return ECompareResult_Gt;

	//We want to sort on contents
	//Provided it's the same level of parenting.
	//This ensures things with the same parent also stay at the same location

	for (U64 i = 0; i < a->length && i < b->length; ++i) {

		C8 ai = a->ptr[i];
		C8 bi = b->ptr[i];

		if (ai < bi)
			return ECompareResult_Lt;

		if (ai > bi)
			return ECompareResult_Gt;
	}

	//If they start with the same thing, we want to sort on length

	if (a->length < b->length)
		return ECompareResult_Lt;

	if (a->length > b->length)
		return ECompareResult_Gt;

	return ECompareResult_Eq;
}

//We don't support any compression yet, but should be trivial to add once Buffer_compress/Buffer_decompress is supported.

Error CAFile_write(CAFile caFile, Allocator alloc, Buffer *result) {

	Error err = Error_none();
	List directories = List_createEmpty(sizeof(String));
	List files = List_createEmpty(sizeof(String));
	DLFile dlFile = (DLFile) { 0 };
	Buffer dlFileBuffer = Buffer_createNull();
	Buffer outputBuffer = Buffer_createNull();
	Buffer compressedOutput = Buffer_createNull();

	if(!result)
		_gotoIfError(clean, Error_nullPointer(2, 0));

	_gotoIfError(clean, List_reserve(&directories, 128, alloc));
	_gotoIfError(clean, List_reserve(&files, 128, alloc));

	//Validate CAFile and calculate files and folders

	U64 outputSize = sizeof(CADirectory);		//Excluding header, hash and DLFile

	U32 hash[8] = { 0 };

	U64 realHeaderSize = sizeof(CAHeader) + (
		caFile.settings.compressionType ? (
			caFile.settings.flags & ECASettingsFlags_UseSHA256 ? sizeof(hash) : sizeof(hash[0])
		) : 0
	) + (caFile.settings.encryptionType ? sizeof(I32x4) + 12 : 0);

	U64 baseFileHeader = sizeof(U16) + (						//Parent and fileInfo
		(
			(caFile.settings.flags & ECASettingsFlags_IncludeFullDate) || 
			(caFile.settings.flags & ECASettingsFlags_IncludeDate)
		) ? (caFile.settings.flags & ECASettingsFlags_IncludeFullDate ? sizeof(Ns) : sizeof(U16) * 2) 
		: 0
	);

	U64 biggestFileSize = 0;

	for (U64 i = 0; i < caFile.archive.entries.length; ++i) {

		ArchiveEntry entry = ((ArchiveEntry*)caFile.archive.entries.ptr)[i];

		//Push back directory names and calculate output buffer

		if (entry.type == EFileType_Folder) {

			_gotoIfError(clean, List_pushBack(
				&directories, Buffer_createConstRef(&entry.path, sizeof(String)), alloc
			));

			if(directories.length >= U16_MAX)
				_gotoIfError(clean, Error_outOfBounds(0, 0, 0xFFFF, U16_MAX - 1));

			if(outputSize + sizeof(CADirectory) < outputSize)
				_gotoIfError(clean, Error_overflow(0, 0, outputSize + sizeof(CADirectory), outputSize));

			outputSize += sizeof(CADirectory);
			continue;
		}

		//Push back file names and calculate output buffer

		_gotoIfError(
			clean, 
			List_pushBack(&files, Buffer_createConstRef(&entry.path, sizeof(String)), alloc)
		);

		if(files.length >= U32_MAX - U16_MAX)
			_gotoIfError(clean, Error_outOfBounds(0, 0, U32_MAX - U16_MAX, U32_MAX - U16_MAX - 1));

		if(outputSize + baseFileHeader < outputSize)
			_gotoIfError(clean, Error_overflow(0, 0, outputSize + baseFileHeader, outputSize));

		outputSize += baseFileHeader;
		
		if(outputSize + Buffer_length(entry.data) < outputSize)
			_gotoIfError(clean, Error_overflow(0, 0, outputSize + Buffer_length(entry.data), outputSize));

		outputSize += Buffer_length(entry.data);
		biggestFileSize = U64_max(biggestFileSize, Buffer_length(entry.data));
	}

	//Add the file size types based on the longest file size

	EXXDataSizeType sizeType = biggestFileSize <= U8_MAX ? EXXDataSizeType_U8 : (
		biggestFileSize <= U16_MAX ? EXXDataSizeType_U16 : (
			biggestFileSize <= U32_MAX ? EXXDataSizeType_U32 : 
			EXXDataSizeType_U64
		)
	);

	baseFileHeader += SIZE_BYTE_TYPE[sizeType];
	U64 sizeTypeBytes = files.length * SIZE_BYTE_TYPE[sizeType];

	if(outputSize + sizeTypeBytes < outputSize)
		_gotoIfError(clean, Error_overflow(0, 0, outputSize + sizeTypeBytes, outputSize));

	outputSize += sizeTypeBytes;

	//Sort directories and files to ensure our file ids are correct
	//This is because we have full directory names, so it automatically resolves 
	// in a way where we know where our children are (in a linear way)
	//This sort is different than just sortString. It sorts on length first and then alphabetically.
	//This allows us to not have to reorder the files down the road since they're already sorted.

	if(
		!List_sortCustom(directories, (CompareFunction) sortParentCountAndFileNames) || 
		!List_sortCustom(files, (CompareFunction) sortParentCountAndFileNames)
	)
		_gotoIfError(clean, Error_invalidOperation(0));

	//Allocate and generate DLFile

	DLSettings dlSettings = (DLSettings) { .dataType = EDLDataType_Ascii };

	_gotoIfError(clean, DLFile_create(dlSettings, alloc, &dlFile));

	for(U64 i = 0; i < directories.length; ++i) {

		String dir = ((String*)directories.ptr)[i];
		String dirName = String_createRefSized(dir.ptr, dir.length);

		String_cutBeforeLast(dir, '/', EStringCase_Sensitive, &dirName);
		_gotoIfError(clean, DLFile_addEntryAscii(&dlFile, dirName, alloc));
	}

	for(U64 i = 0; i < files.length; ++i) {

		String file = ((String*)files.ptr)[i];
		String fileName = String_createRefSized(file.ptr, file.length);

		String_cutBeforeLast(file, '/', EStringCase_Sensitive, &fileName);
		_gotoIfError(clean, DLFile_addEntryAscii(&dlFile, fileName, alloc));
	}

	//Complete DLFile as buffer

	_gotoIfError(clean, DLFile_write(dlFile, alloc, &dlFileBuffer));

	DLFile_free(&dlFile, alloc);

	//Build up final output buffer
	//DLFile
	//CADirectory[]
	//CAFileObject[]
	//U8[sum(file[i].data)]

	if (outputSize + Buffer_length(dlFileBuffer) < outputSize)
		_gotoIfError(clean, Error_overflow(0, 0, outputSize + Buffer_length(dlFileBuffer), outputSize));

	outputSize += Buffer_length(dlFileBuffer);

	if (outputSize + realHeaderSize < outputSize)
		_gotoIfError(clean, Error_overflow(0, 0, outputSize + realHeaderSize, outputSize));

	outputSize += realHeaderSize;		//Reserve space for header (even though this won't be compressed)

	_gotoIfError(clean, Buffer_createUninitializedBytes(outputSize, alloc, &outputBuffer));

	//Append DLFile

	Buffer outputBufferIt = Buffer_createRef(outputBuffer.ptr + realHeaderSize, Buffer_length(outputBuffer) - realHeaderSize);

	_gotoIfError(clean, Buffer_appendBuffer(&outputBufferIt, dlFileBuffer));

	Buffer_free(&dlFileBuffer, alloc);

	//Append CADirectory[]

	CADirectory *dirPtr = (CADirectory*)outputBufferIt.ptr, *dirPtrRoot = dirPtr;
	++dirPtr;		//Exclude root

	*dirPtrRoot = (CADirectory) {
		.childDirectoryStart = U16_MAX,
		.childFileStart = U32_MAX,
		.parentDirectory = U16_MAX
	};

	for (U16 i = 0; i < (U16) directories.length; ++i) {

		String dir = ((String*)directories.ptr)[i];
		U64 it = String_findLast(dir, '/', EStringCase_Sensitive);

		U16 parent = U16_MAX;

		//Add to parent directory

		if (it != dir.length) {

			//We need to find the real parent.
			//Since we're sorted alphabetically, we are able to look back from our current directory
			//If we encounter something that doesn't start with our basepath/ then we know we are missing this dir
			//And this means we somehow messed with the input data

			String baseDir = String_createNull(), realParentDir = String_createNull();
			if (
				!String_cut(dir, 0, it, &realParentDir) || 
				!String_cut(dir, 0, it + 1, &baseDir)
			) 
				_gotoIfError(clean, Error_invalidOperation(0));

			for(U64 j = i - 1; j != U64_MAX; --j)

				if (String_equalsString(
					((String*)directories.ptr)[j], realParentDir, 
					EStringCase_Insensitive, true
				)) {
					parent = (U16) j;
					break;
				}

			if(parent == U16_MAX)
				_gotoIfError(clean, Error_invalidState(0));

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

	for (U32 i = 0; i < (U32) files.length; ++i) {

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
			)
				_gotoIfError(clean, Error_invalidOperation(0));
			
			for(U64 j = directories.length - 1; j != U64_MAX; --j)

				if (String_equalsString(
					((String*)directories.ptr)[j], realParentDir, 
					EStringCase_Sensitive, true
				)) {
					parent = (U16) j;
					break;
				}

			if(parent == U16_MAX)
				_gotoIfError(clean, Error_invalidState(1));

			//Update parent

			if (dirPtr[parent].childFileStart == U32_MAX)
				dirPtr[parent].childFileStart = i;

			if(dirPtr[parent].childFileCount == U16_MAX)
				_gotoIfError(clean, Error_outOfBounds(0, 1, U16_MAX + 1, U16_MAX));

			++dirPtr[parent].childFileCount;
		}

		else {

			//Update parent

			if (dirPtrRoot->childFileStart == U32_MAX)
				dirPtrRoot->childFileStart = i;

			if(dirPtrRoot->childFileCount == U16_MAX)
				_gotoIfError(clean, Error_outOfBounds(0, 1, U16_MAX + 1, U16_MAX));

			++dirPtrRoot->childFileCount;
		}

		//Find corresponding file with name

		ArchiveEntry *entry = NULL;

		for(U64 j = 0; j < caFile.archive.entries.length; ++j)
			if (String_equalsString(
				((ArchiveEntry*)caFile.archive.entries.ptr + j)->path, file, 
				EStringCase_Sensitive, true
			)) {
				entry = (ArchiveEntry*)caFile.archive.entries.ptr + j;
				break;
			}

		if (!entry)
			_gotoIfError(clean, Error_invalidState(2));

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

			else {

				if (!CAFile_storeDate(entry->timestamp, (U16*)filePtr + 1, (U16*)filePtr)) {
					_gotoIfError(clean, Error_invalidState(1));
				}

				filePtr += sizeof(U16) * 2;
			}
		}

		U64 fileSize = Buffer_length(entry->data);

		switch (sizeType) {
			case EXXDataSizeType_U8:	*filePtr = (U8) fileSize;			break;
			case EXXDataSizeType_U16:	*(U16*)filePtr = (U16) fileSize;	break;
			case EXXDataSizeType_U32:	*(U32*)filePtr = (U32) fileSize;	break;
			default:					*(U64*)filePtr = fileSize;
		}

		//Append file data

		Buffer_copy(Buffer_createRef(fileDataPtrIt, Buffer_length(entry->data)), entry->data);
		fileDataPtrIt += fileSize;
	}

	//We're done with processing files and folders

	U16 dirCount = (U16) directories.length;
	U32 fileCount = (U32) files.length;

	List_free(&files, alloc);
	List_free(&directories, alloc);

	//Generate header

	U8 header[sizeof(CAHeader) + sizeof(hash)], *headerIt = header;

	*((CAHeader*)header) = (CAHeader) {

		.magicNumber = CAHeader_MAGIC,

		.version = 0,		//1.0

		.flags = (U8) (

			(
				caFile.settings.compressionType ? ECAFlags_HasHash | (
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

		.type = (U8)((dlFile.settings.compressionType << 4) | dlFile.settings.encryptionType),

		.directoryCount = dirCount,
		.fileCount = fileCount
	};

	headerIt += sizeof(CAHeader);

	Buffer realHeader = Buffer_createConstRef(header, realHeaderSize);

	//Copy our header into the buffer with original result before hashing
	//Otherwise our hash will exclude the header data, which is no bueno.

	Buffer_copy(Buffer_createRefFromBuffer(outputBuffer, false), realHeader);

	//Hash

	if(caFile.settings.compressionType) {

		if (caFile.settings.flags & ECASettingsFlags_UseSHA256)
			Buffer_sha256(outputBuffer, hash);

		else hash[0] = Buffer_crc32c(outputBuffer);
	}

	//Store hash in header before encryption or finish

	if (caFile.settings.compressionType)
		Buffer_copy(
			Buffer_createRef(headerIt, sizeof(hash)), 
			Buffer_createConstRef(hash, sizeof(hash))
		);

	//Compress

	/*if (caFile.settings.compressionType != EXXCompressionType_None) {				TODO:

		Buffer toCompress = Buffer_createConstRef(
			outputBuffer.ptr + realHeaderSize, 
			Buffer_length(outputBuffer) - realHeaderSize
		);

		_gotoIfError(clean, Buffer_compress(toCompress, BufferCompressionType_Brotli11, alloc, &compressedOutput));

		Buffer_free(&outputBuffer, alloc);
	}

	else {*/
		//compressedOutput = outputBuffer;
		//outputBuffer = Buffer_createNull();
	//}

	//Encrypt

	/* TODO:
	if (caFile.settings.encryptionType != EXXEncryptionType_None) {

		//TODO:

		if ((err = Buffer_encrypt(
			compressedOutput, realHeader, 
			EBufferEncryptionType_AES256GCM, caFile.settings.encryptionKey, NULL,
			alloc, &outputBuffer
		)).genericError) {
			Buffer_free(&compressedOutput, alloc);
			return err;
		}
	}*/

	//Prepend header and hash

	*result = outputBuffer;
	outputBuffer = Buffer_createNull();

clean:
	Buffer_free(&compressedOutput, alloc);
	Buffer_free(&outputBuffer, alloc);
	Buffer_free(&dlFileBuffer, alloc);
	DLFile_free(&dlFile, alloc);
	List_free(&files, alloc);
	List_free(&directories, alloc);
	return err;
}

Error CAFile_read(Buffer file, Allocator alloc, CAFile *caFile);
