#include "formats/oiDL.h"
#include "types/list.h"
#include "types/time.h"

/*	TODO: Implement compression and encryption

//File spec (docs/oiDL.md)

typedef enum EDLFlags {

	EDLFlags_None 					= 0,

	EDLFlags_HasHash				= 1 << 0,		//Should be true if compression or encryption is on
	EDLFlags_UseSHA256				= 1 << 1,		//Whether SHA256 (1) or CRC32C (0) is used as hash

	EDLFlags_IsString				= 1 << 2,		//If true; string must contain valid ASCII characters
	EDLFlags_UTF8					= 1 << 3		//ASCII (if off), otherwise UTF-8

} EDLFlags;

typedef struct DLHeader {

	U32 magicNumber;			//oiDL (0x4C44696F)

	U8 version;					//major.minor (%10 = minor, /10 = major)
	U8 flags;					//EDLFlags
	U8 compressionType;			//EDLCompressionType. Should be <Count
	U8 encryptionType;			//EDLEncryptionType

	U8 headerExtendedData;		//If new versions or extensions want to add extra data to the header
	U8 perEntryExtendedData;	//What to store per entry besides a DataSizeType
	U8 sizeTypes;				//EDLDataSizeTypes: entrySizeType | (uncompressedSizeType << 2) | (dataSizeType << 4)
	U8 padding;					//For alignment reasons

} DLHeader;

static const U32 DLHeader_magic = 0x4C44696F;

typedef enum EDLDataSizeType {

	EDLDataSizeType_U8,
	EDLDataSizeType_U16,
	EDLDataSizeType_U32,
	EDLDataSizeType_U64

} EDLDataSizeType;

//Helper functions to create it

Error DLFile_create(DLSettings settings, Allocator alloc, DLFile *dlFile) {

	if(!dlFile)
		return Error_nullPointer(0, 0);

	if(dlFile->entries.ptr)
		return Error_invalidOperation(0);

	if(settings.compressionType >= EDLCompressionType_Count)
		return Error_invalidParameter(0, 0, 0);

	if(settings.encryptionType >= EDLEncryptionType_Count)
		return Error_invalidParameter(0, 1, 0);

	if(settings.dataType >= EDLDataType_Count)
		return Error_invalidParameter(0, 2, 0);

	if(settings.flags & EDLSettingsFlags_Invalid)
		return Error_invalidParameter(0, 3, 0);

	if (settings.compressionType == EDLEncryptionType_AES256) {		//Check if we actually input a key

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
			return Error_invalidParameter(0, 4, 0);
	}

	dlFile->entries = List_createEmpty(sizeof(DLEntry));

	Error err = List_reserve(&dlFile->entries, 100, alloc);

	if(err.genericError)
		return err;

	dlFile->settings = settings;
	return Error_none();
}

Error DLFile_free(DLFile *dlFile, Allocator alloc) {

	if(!dlFile || !dlFile->entries.ptr)
		return Error_none();

	for (U64 i = 0; i < dlFile->entries.length; ++i) {

		DLEntry entry = ((DLEntry*)dlFile->entries.ptr)[i];

		if(dlFile->settings.dataType != EDLDataType_Data)
			String_free(&entry.entryString, alloc);

		else Buffer_free(&entry.entryBuffer, alloc);
	}

	Error err = List_free(&dlFile->entries, alloc);
	*dlFile = (DLFile) { 0 };
	return err;
}

//Writing a zip

Error DLFile_addEntry(DLFile *dlFile, Buffer entryBuf, Allocator alloc) {

	if(!dlFile)
		return Error_nullPointer(0, 0);

	if(dlFile->settings.dataType != EDLDataType_Data)
		return Error_invalidOperation(0);

	DLEntry entry = { .entryBuffer = entryBuf };

	return List_pushBack(
		&dlFile->entries,
		Buffer_createRef(&entry, sizeof(entry)),
		alloc
	);
}

Error DLFile_addEntryAscii(DLFile *dlFile, String entryStr, Allocator alloc) {

	if(!dlFile)
		return Error_nullPointer(0, 0);

	if(dlFile->settings.dataType != EDLDataType_Ascii)
		return Error_invalidOperation(0);

	if(!String_isValidAscii(entryStr))
		return Error_invalidParameter(1, 0, 0);

	DLEntry entry = { .entryString = entryStr };

	return List_pushBack(
		&dlFile->entries,
		Buffer_createRef(&entry, sizeof(entry)),
		alloc
	);
}

//We currently only support writing brotli because it's the best 
//(space) compression compared to time to decompress/compress

static const U8 sizeByteType[4] = { 1, 2, 4, 8 };

inline EDLDataSizeType getRequiredType(U64 v) {
	return v <= U8_MAX ? EDLDataSizeType_U8 : (
		v <= U16_MAX ? EDLDataSizeType_U16 : (
			v <= U32_MAX ? EDLDataSizeType_U32 : 
			EDLDataSizeType_U64
		)
	);
}

Error DLFile_write(DLFile dlFile, Allocator alloc, Buffer *result) {

	if(!result)
		return Error_nullPointer(2, 0);

	//Get header size (excluding compressedSize + entryCount)

	U64 headerSize = sizeof(DLHeader) + (
		dlFile.settings.compressionType || dlFile.settings.encryptionType ? (
			dlFile.settings.flags & EDLSettingsFlags_UseSHA256 ? 32 : 4
		) : 0
	);

	//Get data size

	U64 outputSize = 0, maxSize = 0;

	for (U64 i = 0; i < dlFile.entries.length; ++i) {

		DLEntry entry = ((DLEntry*)dlFile.entries.ptr)[i];

		U64 len = dlFile.settings.dataType == EDLDataType_Data ? entry.entryBuffer.length : entry.entryString.length;

		if(outputSize + len < outputSize)
			return Error_overflow(0, 0, outputSize + len, outputSize);

		outputSize += len;
		maxSize = U64_max(maxSize, len);
	}

	U8 dataSizeType = sizeByteType[getRequiredType(maxSize)];
	U64 entrySizes = dataSizeType * dlFile.entries.length;

	if(entrySizes / dataSizeType != dlFile.entries.length)
		return Error_overflow(0, 0, entrySizes, entrySizes / dataSizeType);

	if(outputSize + entrySizes < outputSize)
		return Error_overflow(0, 0, outputSize + entrySizes, outputSize);

	outputSize += entrySizes;

	U8 entrySizeType = sizeByteType[getRequiredType(dlFile.entries.length)];
	headerSize += entrySizeType;

	U8 uncompressedSizeType = sizeByteType[getRequiredType(outputSize)];

	if(dlFile.settings.compressionType)
		headerSize += uncompressedSizeType;

	//Create our final uncompressed buffer

	Buffer uncompressedData;
	Error err = Buffer_createUninitializedBytes(outputSize, alloc, &uncompressedData);

	if(err.genericError)
		return err;

	U8 *sizes = uncompressedData.ptr;
	U8 *dat = uncompressedData.ptr + dataSizeType * dlFile.entries.length;

	for (U64 i = 0; i < dlFile.entries.length; ++i) {

		DLEntry entry = ((DLEntry*)dlFile.entries.ptr)[i];
		Buffer buf = dlFile.settings.dataType == EDLDataType_Data ? entry.entryBuffer : String_buffer(entry.entryString);
		U64 len = buf.length;

		U8 *sizePtr = sizes + dataSizeType * i;

		switch (dataSizeType) {
			case 1:		*(U8*)sizePtr	= (U8) len;		break;
			case 2:		*(U16*)sizePtr	= (U16) len;	break;
			case 4:		*(U32*)sizePtr	= (U32) len;	break;
			default:	*(U64*)sizePtr	= len;
		}

		Buffer_copy(Buffer_createRef(dat, len), buf);
		dat += len;
	}

	//Hash

	U32 hash[8];

	if(dlFile.settings.encryptionType || dlFile.settings.compressionType) {

		if (dlFile.settings.flags & EDLSettingsFlags_UseSHA256)
			Buffer_sha256(uncompressedData, hash);
		
		else hash[0] = Buffer_crc32c(uncompressedData);
	}

	//Compress

	U64 uncompressedSize = uncompressedData.length;

	Buffer compressedOutput;

	if (dlFile.settings.compressionType != EDLCompressionType_None) {

		BufferCompressionType comprType = 
			dlFile.settings.compressionType == EDLCompressionType_Brotli1 ? BufferCompressionType_Brotli1 :
			BufferCompressionType_Brotli11;

		if ((err = Buffer_compress(uncompressedData, comprType, alloc, &compressedOutput)).genericError) {
			Buffer_free(&uncompressedData, alloc);
			return err;
		}

		Buffer_free(&uncompressedData, alloc);
	}

	else compressedOutput = uncompressedData;

	//Encrypt

	if (dlFile.settings.encryptionType != EDLEncryptionType_None) {

		if ((err = Buffer_encrypt(
			compressedOutput, BufferEncryptionType_AES256, dlFile.settings.encryptionKey
		)).genericError) {
			Buffer_free(&compressedOutput, alloc);
			return err;
		}
	}

	//Prepend header and hash

	U8 header[sizeof(DLHeader) + sizeof(hash) + sizeof(U64) * 2];
	U8 *headerIt = header;

	*((DLHeader*)headerIt) = (DLHeader) {

		.magicNumber = DLHeader_magic,

		.version = 10,		//1.0

		.flags = (U8) (

			(
				dlFile.settings.encryptionType || dlFile.settings.compressionType ? EDLFlags_HasHash | (
					dlFile.settings.flags & EDLSettingsFlags_UseSHA256 ? EDLFlags_UseSHA256 : 
					EDLFlags_None
				) : 
				EDLFlags_None
			) |

			(dlFile.settings.dataType == EDLDataType_Ascii ? EDLFlags_IsString : EDLFlags_None)
		),

		.compressionType = dlFile.settings.compressionType,
		.encryptionType = dlFile.settings.encryptionType,

		.sizeTypes = entrySizeType | (uncompressedSizeType << 2) | (dataSizeType << 4)
	};

	headerIt += sizeof(DLHeader);

	switch (entrySizeType) {
		case 1:		*(U8*)headerIt  = (U8) dlFile.entries.length;		headerIt += sizeof(U8);		break;
		case 2:		*(U16*)headerIt	= (U16) dlFile.entries.length;		headerIt += sizeof(U16);	break;
		case 4:		*(U32*)headerIt	= (U32) dlFile.entries.length;		headerIt += sizeof(U32);	break;
		default:	*(U64*)headerIt	= dlFile.entries.length;			headerIt += sizeof(U64);
	}

	if(dlFile.settings.compressionType)
		switch (uncompressedSizeType) {
			case 1:		*(U8*)headerIt  = (U8) uncompressedSize;		headerIt += sizeof(U8);		break;
			case 2:		*(U16*)headerIt	= (U16) uncompressedSize;		headerIt += sizeof(U16);	break;
			case 4:		*(U32*)headerIt	= (U32) uncompressedSize;		headerIt += sizeof(U32);	break;
			default:	*(U64*)headerIt	= uncompressedSize;				headerIt += sizeof(U64);
		}

	if (dlFile.settings.encryptionType || dlFile.settings.compressionType) {

		if (!(dlFile.settings.flags & EDLSettingsFlags_UseSHA256)) {
			*(U32*)headerIt = hash[0];
			headerIt += sizeof(hash[0]);
		}
		
		else {

			Buffer_copy(
				Buffer_createRef(headerIt, sizeof(hash)), 
				Buffer_createRef(hash, sizeof(hash))
			);

			headerIt += sizeof(hash);
		}
	}

	Buffer headerBuf = Buffer_createRef(header, headerSize);

	err = Buffer_combine(headerBuf, compressedOutput, alloc, result);
	Buffer_free(&compressedOutput, alloc);
	return err;
}

Error DLFile_read(Buffer file, Allocator alloc, DLFile *dlFile);

*/
