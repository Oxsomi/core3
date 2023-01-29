#include "formats/oiDL.h"
#include "types/list.h"
#include "types/time.h"
#include "types/error.h"
#include "types/buffer.h"
#include "types/allocator.h"

//File spec (docs/oiDL.md)

typedef enum EDLFlags {

	EDLFlags_None 					= 0,

	EDLFlags_UseSHA256				= 1 << 0,		//Whether SHA256 (1) or CRC32C (0) is used as hash

	EDLFlags_IsString				= 1 << 1,		//If true; string must contain valid ASCII characters
	EDLFlags_UTF8					= 1 << 2,		//ASCII (if off), otherwise UTF-8
        
    //Chunk size of AES for multi threading. 0 = none, 1 = 10MiB, 2 = 50MiB, 3 = 100MiB
        
	EDLFlags_UseAESChunksA			= 1 << 3,
	EDLFlags_UseAESChunksB			= 1 << 4,

	EDLFlags_Invalid				= (0xFF << 5) & 0xFF

} EDLFlags;

typedef struct DLHeader {

	U32 magicNumber;			//oiDL (0x4C44696F)

	U8 version;					//major.minor (%10 = minor, /10 = major)
	U8 flags;					//EDLFlags
	U8 compressionType;			//EXXCompressionType. Should be <Count
	U8 encryptionType;			//EXXEncryptionType

	U8 headerExtendedData;		//If new versions or extensions want to add extra data to the header
	U8 perEntryExtendedData;	//What to store per entry besides a DataSizeType
	U8 sizeTypes;				//EXXDataSizeTypes: entrySizeType | (uncompressedSizeType << 2) | (dataSizeType << 4)
	U8 padding;					//For alignment reasons

} DLHeader;

static const U32 DLHeader_MAGIC = 0x4C44696F;
static const U8 DLHeader_V1_0  = 0;

//Helper functions to create it

Error DLFile_create(DLSettings settings, Allocator alloc, DLFile *dlFile) {

	if(!dlFile)
		return Error_nullPointer(0, 0);

	if(dlFile->entries.ptr)
		return Error_invalidOperation(0);

	if(settings.compressionType >= EXXCompressionType_Count)
		return Error_invalidParameter(0, 0, 0);

	if(settings.compressionType > EXXCompressionType_None)
		return Error_invalidOperation(0);							//TODO: Add support for compression

	if(settings.encryptionType >= EXXEncryptionType_Count)
		return Error_invalidParameter(0, 1, 0);

	if(settings.dataType >= EDLDataType_Count)
		return Error_invalidParameter(0, 2, 0);

	if(settings.flags & EDLSettingsFlags_Invalid)
		return Error_invalidParameter(0, 3, 0);

	dlFile->entries = List_createEmpty(sizeof(DLEntry));

	Error err = List_reserve(&dlFile->entries, 100, alloc);

	if(err.genericError)
		return err;

	dlFile->settings = settings;
	return Error_none();
}

Bool DLFile_free(DLFile *dlFile, Allocator alloc) {

	if(!dlFile || !dlFile->entries.ptr)
		return true;

	for (U64 i = 0; i < dlFile->entries.length; ++i) {

		DLEntry entry = ((DLEntry*)dlFile->entries.ptr)[i];

		if(dlFile->settings.dataType == EDLDataType_Ascii)
			String_free(&entry.entryString, alloc);

		else Buffer_free(&entry.entryBuffer, alloc);
	}

	List_free(&dlFile->entries, alloc);
	*dlFile = (DLFile) { 0 };
	return true;
}

//Writing

Error DLFile_addEntry(DLFile *dlFile, Buffer entryBuf, Allocator alloc) {

	if(!dlFile)
		return Error_nullPointer(0, 0);

	if(dlFile->settings.dataType != EDLDataType_Data)
		return Error_invalidOperation(0);

	DLEntry entry = { .entryBuffer = entryBuf };

	return List_pushBack(
		&dlFile->entries,
		Buffer_createConstRef(&entry, sizeof(entry)),
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
		Buffer_createConstRef(&entry, sizeof(entry)),
		alloc
	);
}

Error DLFile_addEntryUTF8(DLFile *dlFile, Buffer entryBuf, Allocator alloc) {

	if(!dlFile)
		return Error_nullPointer(0, 0);

	if(dlFile->settings.dataType != EDLDataType_UTF8)
		return Error_invalidOperation(0);

	if(!Buffer_isUTF8(entryBuf, 1))
		return Error_invalidParameter(1, 0, 0);

	DLEntry entry = { .entryBuffer = entryBuf };

	return List_pushBack(
		&dlFile->entries,
		Buffer_createConstRef(&entry, sizeof(entry)),
		alloc
	);
}

//We currently don't support compression yet. But once Buffer_compress/uncompress is available, it should be easy.

inline EXXDataSizeType getRequiredType(U64 v) {
	return v <= U8_MAX ? EXXDataSizeType_U8 : (
		v <= U16_MAX ? EXXDataSizeType_U16 : (
			v <= U32_MAX ? EXXDataSizeType_U32 : 
			EXXDataSizeType_U64
		)
	);
}

Error DLFile_write(DLFile dlFile, Allocator alloc, Buffer *result) {

	if(!result)
		return Error_nullPointer(2, 0);

	if(result->ptr)
		return Error_invalidOperation(0);

	//Get header size (excluding compressedSize + entryCount)

	U64 hashSize = dlFile.settings.compressionType ? (
		dlFile.settings.flags & EDLSettingsFlags_UseSHA256 ? 32 : 4
	) : 0;

	U64 headerSize = sizeof(DLHeader) + hashSize;

	//Get data size

	U64 outputSize = 0, maxSize = 0;

	for (U64 i = 0; i < dlFile.entries.length; ++i) {

		DLEntry entry = ((DLEntry*)dlFile.entries.ptr)[i];

		U64 len = dlFile.settings.dataType != EDLDataType_Ascii ? Buffer_length(entry.entryBuffer) : entry.entryString.length;

		if(outputSize + len < outputSize)
			return Error_overflow(0, 0, outputSize + len, outputSize);

		outputSize += len;
		maxSize = U64_max(maxSize, len);
	}

	U8 dataSizeType = SIZE_BYTE_TYPE[getRequiredType(maxSize)];
	U64 entrySizes = dataSizeType * dlFile.entries.length;

	if(entrySizes / dataSizeType != dlFile.entries.length)
		return Error_overflow(0, 0, entrySizes, entrySizes / dataSizeType);

	if(outputSize + entrySizes < outputSize)
		return Error_overflow(0, 0, outputSize + entrySizes, outputSize);

	outputSize += entrySizes;

	U8 entrySizeType = SIZE_BYTE_TYPE[getRequiredType(dlFile.entries.length)];
	headerSize += entrySizeType;

	U8 uncompressedSizeType = SIZE_BYTE_TYPE[getRequiredType(outputSize)];

	if(dlFile.settings.compressionType)
		headerSize += uncompressedSizeType;

	if(outputSize + headerSize < outputSize)
		return Error_overflow(0, 0, outputSize + headerSize, outputSize);

	//Create our final uncompressed buffer

	Buffer uncompressedData = Buffer_createNull();
	Error err = Buffer_createUninitializedBytes(outputSize + headerSize, alloc, &uncompressedData);

	if(err.genericError)
		return err;

	U8 *sizes = uncompressedData.ptr + headerSize;
	U8 *dat = sizes + dataSizeType * dlFile.entries.length;

	for (U64 i = 0; i < dlFile.entries.length; ++i) {

		DLEntry entry = ((DLEntry*)dlFile.entries.ptr)[i];
		Buffer buf = dlFile.settings.dataType != EDLDataType_Ascii ? entry.entryBuffer : String_bufferConst(entry.entryString);
		U64 len = Buffer_length(buf);

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

	//Prepend header and hash

	U32 hash[8] = { 0 };

	U8 header[sizeof(DLHeader) + sizeof(hash) + sizeof(U64) * 2 + 12 + sizeof(I32x4)];
	U8 *headerIt = header;

	*((DLHeader*)headerIt) = (DLHeader) {

		.magicNumber = DLHeader_MAGIC,

		.version = 0,		//1.0

		.flags = (U8) (

			(
				dlFile.settings.compressionType ? (
					dlFile.settings.flags & EDLSettingsFlags_UseSHA256 ? EDLFlags_UseSHA256 : 
					EDLFlags_None
				) : 
				EDLFlags_None
			) |

			(dlFile.settings.dataType == EDLDataType_Ascii ? EDLFlags_IsString : (
				dlFile.settings.dataType == EDLDataType_UTF8 ? EDLFlags_IsString | EDLFlags_UTF8 : EDLFlags_None
			))
		),

		.compressionType = dlFile.settings.compressionType,
		.encryptionType = dlFile.settings.encryptionType,

		.sizeTypes = 
			(U8)getRequiredType(dlFile.entries.length) | 
			((U8)getRequiredType(outputSize) << 2) | 
			((U8)getRequiredType(maxSize) << 4)
	};

	headerIt += sizeof(DLHeader);

	switch (entrySizeType) {
		case 1:		*(U8*)headerIt	= (U8) dlFile.entries.length;		headerIt += sizeof(U8);		break;
		case 2:		*(U16*)headerIt	= (U16) dlFile.entries.length;		headerIt += sizeof(U16);	break;
		case 4:		*(U32*)headerIt	= (U32) dlFile.entries.length;		headerIt += sizeof(U32);	break;
		default:	*(U64*)headerIt	= dlFile.entries.length;			headerIt += sizeof(U64);
	}

	if (dlFile.settings.compressionType)
		switch (uncompressedSizeType) {
			case 1:		*(U8*)headerIt	= (U8) outputSize;		headerIt += sizeof(U8);		break;
			case 2:		*(U16*)headerIt	= (U16) outputSize;		headerIt += sizeof(U16);	break;
			case 4:		*(U32*)headerIt	= (U32) outputSize;		headerIt += sizeof(U32);	break;
			default:	*(U64*)headerIt	= outputSize;			headerIt += sizeof(U64);
		}

	//Copy empty hash

	if(dlFile.settings.compressionType) 
		Buffer_copy(
			Buffer_createRef(headerIt, hashSize),
			Buffer_createConstRef(hash, hashSize)
		);

	//Copy header to intermediate

	Buffer_copy(
		Buffer_createRef(uncompressedData.ptr, (headerIt + hashSize) - header),
		Buffer_createConstRef(header, headerSize)
	);

	//Hash

	if(dlFile.settings.compressionType) {

		//Generate hash

		if (dlFile.settings.flags & EDLSettingsFlags_UseSHA256)
			Buffer_sha256(uncompressedData, hash);
		
		else hash[0] = Buffer_crc32c(uncompressedData);

		//Copy real hash to our header

		Buffer_copy(
			Buffer_createRef(headerIt, hashSize),
			Buffer_createConstRef(hash, hashSize)
		);

		headerIt += hashSize;
	}

	//Compress

	//U64 uncompressedSize = Buffer_length(uncompressedData) - headerSize;

	Buffer compressedOutput;

	/*if (dlFile.settings.compressionType != EXXCompressionType_None) {

		//TODO: Impossible to reach for now, but implement this once it exists

		if ((err = Buffer_compress(
				uncompressedData, BufferCompressionType_Brotli11, alloc, &compressedOutput
			)).genericError
		) {
			Buffer_free(&uncompressedData, alloc);
			return err;
		}

		Buffer_free(&uncompressedData, alloc);

	}

	else {*/
		compressedOutput = Buffer_createRef(
			uncompressedData.ptr + headerSize, 
			Buffer_length(uncompressedData) - headerSize
		);
	//}

	//Encrypt

	if (dlFile.settings.encryptionType != EXXEncryptionType_None) {

		//TODO: Support chunks
		//		Select no chunks if <40MiB
		//		Select 10MiB if at least 4 threads can be kept busy.
		//		Select 50MiB if ^ and utilization is about the same (e.g. 24 threads doing 10MiB would need 1.2GiB).
		//		Select 100MiB if ^ (24 threads would need 2.4GiB).
	
		U32 key[8] = { 0 };

		Bool b = false;
		Error cmpErr = Buffer_eq(
			Buffer_createConstRef(key, sizeof(key)), 
			Buffer_createConstRef(dlFile.settings.encryptionKey, sizeof(key)), 
			&b
		);

		if ((err = Buffer_encrypt(

			compressedOutput, 
			Buffer_createConstRef(header, headerSize), 

			EBufferEncryptionType_AES256GCM, 

			EBufferEncryptionFlags_GenerateIv | (
				b || cmpErr.genericError ? EBufferEncryptionFlags_GenerateKey :
				EBufferEncryptionFlags_None
			),

			b || cmpErr.genericError ? NULL : dlFile.settings.encryptionKey,

			(I32x4*)headerIt,
			(I32x4*)((U8*)headerIt + 12)

		)).genericError) {
			Buffer_free(&compressedOutput, alloc);
			return err;
		}

		headerIt += 12 + sizeof(I32x4);
		headerSize += 12 + sizeof(I32x4);
	}

	Buffer headerBuf = Buffer_createConstRef(header, headerSize);

	err = Buffer_combine(headerBuf, compressedOutput, alloc, result);
	Buffer_free(&compressedOutput, alloc);
	return err;
}

void dec(const U8 **ptr, U64 *len, U64 l) {
	*ptr += l;
	*len -= l;
}

Error DLFile_read(Buffer file, const U32 encryptionKey[8], Allocator alloc, DLFile *dlFile) {

	if(!dlFile)
		return Error_nullPointer(2, 0);

	if(dlFile->entries.ptr)
		return Error_invalidOperation(0);

	if(Buffer_isConstRef(file))
		return Error_constData(0, 0);

	if(Buffer_length(file) < sizeof(DLFile))
		return Error_outOfBounds(0, 0, sizeof(DLFile), Buffer_length(file));

	//Read from binary

	U8 *ptr = file.ptr;
	U64 len = Buffer_length(file);

	DLHeader header = *(const DLHeader*)ptr;
	dec(&ptr, &len, sizeof(DLHeader));

	//Validate header

	if(header.magicNumber != DLHeader_MAGIC)
		return Error_invalidParameter(0, 0, 0);

	if(header.version != DLHeader_V1_0)
		return Error_invalidParameter(0, 1, 0);

	if(header.flags & EDLFlags_Invalid)
		return Error_invalidParameter(0, 2, 0);

	if(header.flags & (EDLFlags_UseAESChunksA | EDLFlags_UseAESChunksB))		//TODO: AES chunks
		return Error_unsupportedOperation(0);

	if(header.compressionType)							//TODO: Compression
		return Error_unsupportedOperation(1);

	if(header.flags & EDLFlags_UseSHA256)				//TODO: SHA256
		return Error_unsupportedOperation(2);

	if(header.encryptionType >= EXXEncryptionType_Count)
		return Error_invalidParameter(0, 4, 0);

	if(header.sizeTypes >> 6)
		return Error_invalidParameter(0, 7, 0);

	if(header.padding)
		return Error_invalidParameter(0, 8, 0);

	EXXDataSizeType entrySizeType			= (EXXDataSizeType)(header.sizeTypes & 3);
	//EXXDataSizeType uncompressedSizeType	= (EXXDataSizeType)((header.sizeTypes >> 2) & 3);
	EXXDataSizeType dataSizeType			= (EXXDataSizeType)(header.sizeTypes >> 4);

	//Consume extended header

	if(len < header.headerExtendedData)
		return Error_outOfBounds(0, 0, (ptr + header.perEntryExtendedData) - file.ptr, Buffer_length(file));

	dec(&ptr, &len, header.headerExtendedData);

	//Entry size

	U8 entrySizeTypeLen = SIZE_BYTE_TYPE[entrySizeType];

	if(len < entrySizeTypeLen)
		return Error_outOfBounds(0, 0, (ptr + entrySizeTypeLen) - file.ptr, Buffer_length(file));

	U64 entryCount = 0;

	switch (entrySizeTypeLen) {
		case 1:		entryCount = *ptr;					break;
		case 2:		entryCount = *(const U16*)ptr;		break;
		case 4:		entryCount = *(const U32*)ptr;		break;
		case 8:		entryCount = *(const U64*)ptr;		break;
	}

	dec(&ptr, &len, entrySizeTypeLen);

	//TODO: Uncompressed size

	//Unencrypt

	if (header.encryptionType) {

		//Get tag and iv

		if(len < sizeof(I32x4) + 12)
			return Error_outOfBounds(0, 0, (ptr + 12 + sizeof(I32x4)) - file.ptr, Buffer_length(file));

		const U8 *encStart = ptr;

		I32x4 iv = I32x4_zero();
		Buffer_copy(Buffer_createRef(&iv, 12), Buffer_createConstRef(ptr, 12));
		dec(&ptr, &len, 12);

		I32x4 tag = *(const I32x4*)ptr;
		dec(&ptr, &len, sizeof(I32x4));

		if(!encryptionKey)
			return Error_nullPointer(3, 0);

		//Decrypt

		Error err = Buffer_decrypt(
			Buffer_createRef(ptr, len), 
			Buffer_createConstRef(file.ptr, encStart - file.ptr),
			EBufferEncryptionType_AES256GCM,
			encryptionKey,
			tag,
			iv
		);

		if(err.genericError)
			return err;
	}

	//Decrypted, check if size makes sense with entry points.

	U64 entryStride = (U64)header.perEntryExtendedData + SIZE_BYTE_TYPE[dataSizeType];

	if(entryCount * entryStride < entryCount)
		return Error_overflow(0, 0, entryStride * entryCount, U64_MAX);

	if(len < entryCount * entryStride)
		return Error_outOfBounds(0, 0, (ptr + entryCount * entryStride) - file.ptr, Buffer_length(file));

	const U8 *entryStart = ptr;
	dec(&ptr, &len, entryCount * entryStride);

	//Validate entry size with buffers

	const U8 *emulatedPtr = ptr;
	U64 emulatedLen = len;

	for (U64 i = 0; i < entryCount; ++i) {

		const U8 *entryi = entryStart + i * entryStride;

		U64 entryLen = 0;

		switch (SIZE_BYTE_TYPE[dataSizeType]) {
			case 1:		entryLen = *entryi;						break;
			case 2:		entryLen = *(const U16*)entryi;			break;
			case 4:		entryLen = *(const U32*)entryi;			break;
			case 8:		entryLen = *(const U64*)entryi;			break;
		}

		if(entryLen > emulatedLen)
			return Error_outOfBounds(0, 0, emulatedPtr - file.ptr, Buffer_length(file));

		dec(&emulatedPtr, &emulatedLen, entryLen);
	}

	//Ensure we don't have any leftover data

	if(emulatedLen)
		return Error_invalidState(0);

	//Allocate DLFile

	DLSettings settings = (DLSettings) { 

		.compressionType = (EXXCompressionType) header.compressionType,
		.encryptionKey = (EXXEncryptionType) header.encryptionType,

		.dataType = header.flags & EDLFlags_UTF8 ? EDLDataType_UTF8 : (
			header.flags & EDLFlags_IsString ? EDLDataType_Ascii : EDLDataType_Data
		),

		.flags = header.flags & EDLFlags_UseSHA256 ? EDLSettingsFlags_UseSHA256 : EDLSettingsFlags_None
	};

	if(encryptionKey)
		Buffer_copy(
			Buffer_createRef(settings.encryptionKey, sizeof(settings.encryptionKey)),
			Buffer_createConstRef(encryptionKey, sizeof(settings.encryptionKey))
		);

	//Create DLFile

	Error err = DLFile_create(settings, alloc, dlFile);
	_gotoIfError(clean, err);

	//Per entry

	for (U64 i = 0; i < entryCount; ++i) {

		const U8 *entryi = entryStart + i * entryStride;

		U64 entryLen = 0;

		switch (SIZE_BYTE_TYPE[dataSizeType]) {
			case 1:		entryLen = *entryi;						break;
			case 2:		entryLen = *(const U16*)entryi;			break;
			case 4:		entryLen = *(const U32*)entryi;			break;
			case 8:		entryLen = *(const U64*)entryi;			break;
		}

		Buffer buf = Buffer_createConstRef(ptr, entryLen);

		switch(settings.dataType) {

			case EDLDataType_Data:	err = DLFile_addEntry(dlFile, buf, alloc);		break;
			case EDLDataType_UTF8:	err = DLFile_addEntryUTF8(dlFile, buf, alloc);	break;

			case EDLDataType_Ascii:	
				err = DLFile_addEntryAscii(dlFile, String_createConstRefSized((const C8*)ptr, entryLen), alloc);
				break;
		}

		_gotoIfError(clean, err);

		dec(&ptr, &len, entryLen);
	}

	return Error_none();

clean:
	DLFile_free(dlFile, alloc);
	return err;
}
