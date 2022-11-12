#include "formats/zip.h"
#include "types/list.h"
#include "types/time.h"

Error Zip_create(ZipFile *zipFile, ZipSettings settings, Allocator alloc) {

	if(!zipFile)
		return Error_nullPointer(0, 0);

	if(zipFile->entries.length)
		return Error_invalidOperation(0);

	if(settings.segmentSize < 1024)
		return Error_invalidParameter(0, 0, 0);

	zipFile->entries = List_createEmpty(sizeof(ZipEntry));

	Error err = List_reserve(&zipFile->entries, 100, alloc);

	if(err.genericError)
		return err;

	zipFile->settings = settings;
	return Error_none();
}

Error Zip_free(ZipFile *zipFile, Allocator alloc) {

	if(!zipFile)
		return Error_none();

	//TODO: Free file temp buffers if needed

	Error err = List_free(&zipFile->entries, alloc);
	*zipFile = (ZipFile) { 0 };
	return err;
}

//Zip spec
//Sources:
//https://docs.fileformat.com/compression/zip/
//https://users.cs.jmu.edu/buchhofp/forensics/formats/pkzip-printable.html
//https://pkware.cachefly.net/webdocs/casestudies/APPNOTE.TXT

//TODO: ZIP64

#pragma pack(push, 1)

	typedef enum EZipProducer {

		EZipProducer_FAT32,
		EZipProducer_Amiga,
		EZipProducer_OpenVMS,
		EZipProducer_UNIX,
		EZipProducer_VMCMS,
		EZipProducer_AtariST,
		EZipProducer_OS2HPFS,
		EZipProducer_Mac,
		EZipProducer_ZSystem,
		EZipProducer_CPM,
		EZipProducer_NFTS,
		EZipProducer_MVS,
		EZipProducer_VSE,
		EZipProducer_ARM,
		EZipProducer_VFAT,
		EZipProducer_AMVS,
		EZipProducer_BeOS,
		EZipProducer_Tandem,
		EZipProducer_OS400,
		EZipProducer_OSX,

		EZipProducer_Count

	} EZipProducer;


	typedef enum EFlagLocalFile {

		EFlagLocalFile_None,

		EFlagLocalFile_IsEncrypted			= 1 << 0,

		//ECompressionMethod_Imploded? 8K : 4K sliding dictionary
		//ECompressionMethod_Imploded? 3 : 2 Shannon-Fano trees
		//ECompressionMethod_[A-Za-z]*Deflated ? { Normal, maximum, fast, super fast }[(b1 << 1) | b0]
		//ECompressionMethod_LZMA ? use EOS marker : useCompressedDataSize

		EFlagLocalFile_CompressionBit0		= 1 << 1,
		EFlagLocalFile_CompressionBit1		= 1 << 2,

		//compressedSize, uncompressedSize, CRC are 0, should use data descriptor instead

		EFlagLocalFile_DataDescriptor		= 1 << 3,

		//

		EFlagLocalFile_EnhancedDeflation	= 1 << 4,
		EFlagLocalFile_CompressedPatched	= 1 << 5,
		EFlagLocalFile_StrongEncryption		= 1 << 6,

		EFlagLocalFile_LanguageEncoding		= 1 << 11,		//UTF-8

		EFlagLocalFile_MaskHeaderValues		= 1 << 13

	} EFlagLocalFile;


	typedef enum ECompressionMethod {

		ECompressionMethod_None,

		//Legacy compression

		ECompressionMethod_Shrunk,

		ECompressionMethod_Reduced1,
		ECompressionMethod_Reduced2,
		ECompressionMethod_Reduced3,
		ECompressionMethod_Reduced4,

		ECompressionMethod_Imploded,

		//

		ECompressionMethod_Deflated = 8,
		ECompressionMethod_EnhancedDeflated,
		ECompressionMethod_PKWDCLImploded,

		ECompressionMethod_BZIP2 = 12,

		ECompressionMethod_LZMA = 14,

		ECompressionMethod_IBMTERSE = 18,
		ECompressionMethod_IBMLZ77,

		ECompressionMethod_ZStd = 93,
		ECompressionMethod_MP3,
		ECompressionMethod_XZ,
		ECompressionMethod_JPEG,
		ECompressionMethod_WavPack,
		ECompressionMethod_PPMdI1,
		ECompressionMethod_AExMarker

	} ECompressionMethod;


	typedef enum ELocalFileExtraFieldHeaderId {

		ELocalFileExtraFieldHeaderId_ZIP64 = 1,
		ELocalFileExtraFieldHeaderId_AVInfo = 7,

		ELocalFileExtraFieldHeaderId_OS2 = 9,
		ELocalFileExtraFieldHeaderId_NTFS,

		ELocalFileExtraFieldHeaderId_OpenVMS = 0xC,
		ELocalFileExtraFieldHeaderId_UNIX,

		ELocalFileExtraFieldHeaderId_PatchDescriptor = 0xF,

		ELocalFileExtraFieldHeaderId_PKCS7X509 = 0x14,
		ELocalFileExtraFieldHeaderId_X509CertForFile,
		ELocalFileExtraFieldHeaderId_X509CertForCD,
		ELocalFileExtraFieldHeaderId_StrongEncryptionHeader,
		ELocalFileExtraFieldHeaderId_RecordManagementControls,
		ELocalFileExtraFieldHeaderId_PKCS7EncCertList,

		ELocalFileExtraFieldHeaderId_PolicyDecryptionKey = 0x21,
		ELocalFileExtraFieldHeaderId_SmartcryptKeyProvider,
		ELocalFileExtraFieldHeaderId_SmartcryptKeyData,

		ELocalFileExtraFieldHeaderId_IBMZ390 = 0x65,

		//TODO: Third party mappings

		ELocalFileExtraFieldHeaderId_ApkAlignment = 0xD935

	} EArchiveExtraRecordSignature;


	typedef struct LocalFileExtraFieldHeader {

		U16 id;				//ELocalFileExtraFieldHeaderId
		U16 dataSize;

		//U8[dataSize]
		
	} LocalFileExtraFieldHeader;


	typedef struct LocalFileDataDescriptor {

		U32 crc;

		U32 compressedSize;

		U32 uncompressedSize;

	} LocalFileDataDescriptor;


	typedef struct LocalFileHeader {

		U32 magicNumber;

		U16 minimumVersion;		//Zip version is simply; %10 = minor, /10 = major
		U16 flags;				//EFlagLocalFile

		U16 compression;		//ECompressionMethod
		U16 lastModifyTime;

		U16 lastModifyDate;
		/* No padding here, due to packing */

		U32 crc;

		U32 compressedSize;		//U32_MAX if ZIP64. Length in extra field
	
		U32 uncompressedSize;	//U32_MAX if ZIP64. Length in extra field

		U16 fileNameLength;
		U16 extraFieldLength;

		//C8[fileNameLength]
		//LocalFileExtraFieldHeader[extraFieldLength]

		//LocalFileDataDescriptor if & EFlagLocalFile_DataDescriptor

	} LocalFileHeader;

	static const U32 LocalFileHeader_magicNumberValue = 0x04034B50;


	typedef enum ECentralDirectoryInternalFlags {

		ECentralDirectoryInternalFlags_None,
		ECentralDirectoryInternalFlags_TextFile = 1 << 0,
		ECentralDirectoryInternalFlags_ControlBeforeLogicalRecords = 1 << 2

	} ECentralDirectoryInternalFlags;


	typedef struct CentralFileHeader {

		U32 magicNumber;

		U16 version;
		U16 minimumVersion;

		U16 flags;
		U16 compression;

		U16 lastModifyTime;
		U16 lastModifyDate;

		U32 crc;

		U32 compressedSize;

		U32 uncompressedSize;

		U16 fileNameLength;
		U16 extraFieldLength;

		U16 fileCommentLength;
		U16 diskNumber;					//U16_MAX if ZIP64

		U16 attribInternal;
		/* No padding here, due to packing */

		U32 attribExternal;

		U32 relativeOffset;				//U32_MAX if ZIP64

		//C8[fileNameLength]			If CD encrypted: Actually a hexadecimal value that increments
		//U8[extraFieldLength]
		//C8[fileCommentLength]

	} CentralFileHeader;

	static const U32 CentralDirectoryHeader_magicNumberValue = 0x02014B50;


	typedef struct CentralDirectoryFooter {

		U32 magicNumber;

		U16 diskNumber;			//U16_MAX if ZIP64
		U16 directoryDisk;

		U16 recordsOnDisk;		//U16_MAX if ZIP64
		U16 totalRecords;		//U16_MAX if ZIP64

		U32 directorySize;		//U32_MAX if ZIP64

		U32 offset;				//U32_MAX if ZIP64

		U16 commentLength;
		/* No padding here, due to packing */

		//C8[commentLength]

	} CentralDirectoryFooter;

	static const U32 CentralDirectoryFooter_magicNumberValue = 0x06054B50;
	

	typedef struct ArchiveExtraRecord {

		U32 signature;				//EArchiveExtraRecordSignature

		U32 extraFieldLength;

		//U8[extraFieldLength]

	} ArchiveExtraRecord;


	typedef struct DigitalSignature {

		U32 magicNumber;

		U16 dataSize;

		//U8[dataSize]

	} ArchiveExtraRecord;

	static const U32 DigitalSignature_magicNumberValue = 0x05054B50;

	//TODO: ZIP64 isn't supported yet.
	//		Expansion files for apks are only allowed to be in ZIP format (not ZIP64)

#pragma pack(pop)

inline Ns Zip_loadDate(U16 time, U16 date) {
	return Time_date(
		1980 + (date >> 9), 1 + ((date >> 5) & 0xF), date & 0x1F, 
		time >> 11, (time >> 5) & 0x3F, (time & 0x1F) << 1, 0
	);
}

inline Bool Zip_storeDate(Ns ns, U16 *time, U16 *date) {

	if(!time || !date)
		return false;

	U16 year; U8 month, day, hour, minute, second;
	Bool b = Time_getDate(ns, year, month, day, hour, minute, second, NULL);

	if(!b || year < 1980 || year > (1980 + 0x7F))
		return false;

	*time = (second >> 1) | ((U16)minute << 5) | ((U16)hour << 11);
	*date = day | ((U16)month << 5) | ((U16)(year - 1980) << 11);
	return true;
}

inline U16 Zip_makeVersion(U8 minor, U8 major, EZipProducer producer) {

	if(producer >= EZipProducer_Count || minor >= 10 || major >= 25)
		return U16_MAX;

	return (minor + major * 10) | ((U16)producer << 8);
}

inline Bool Zip_getVersion(U16 v, U8 *minor, U8 *major, EZipProducer* producer) {

	if((v >> 8) >= EZipProducer_Count)
		return false;

	if(producer) *producer = (EZipProducer)(v >> 8);
	if(minor) *minor = (U8)v % 10;
	if(major) *major = (U8)v / 10;

	return true;
}

//Writing a zip

Error Zip_addEntry(ZipFile *zipFile, ZipEntry entry);		//TODO: public/private key or password encryption
Error Zip_removeEntry(ZipFile *zipFile, ZipEntry entry);

//We currently only support writing LZMA because it's the best (space) compression

Error Zip_write(ZipFile zipEntries, Allocator alloc, Buffer *result) {

	if(!zipEntries.settings.segmentSize)
		return Error_invalidParameter(0, 1, 0);

	if(zipEntries.entries.stride != sizeof(ZipEntry))
		return Error_invalidParameter(0, 0, 0);

	//This is the version we output. We take version 6.3 because LZMA compression is very compact.
	//We also need 5.1 (AES encryption) and possibly 6.2 (Central directory encryption).
	//Possibly 4.5 (ZIP64 format extensions)

	static const U8 majorVersion = 6, minorVersion = 3;

	//Calculate file size

	U64 totalFileSize = sizeof(CentralDirectoryFooter);

	for (U64 i = 0; i < zipEntries.entries.length; ++i) {

		ZipEntry entry = *((ZipEntry*)zipEntries.entries.ptr + i);

		U64 fileSize = 
			sizeof(LocalFileHeader) + // TODO: (entry.isEncrypted ? sizeof(EncryptionFileHeader) : 0) + 
			sizeof(CentralFileHeader);

		if(fileSize + entry.data.siz < fileSize)
			return Error_overflow(2, 0, fileSize + entry.data.siz, fileSize);

		fileSize += entry.data.siz;

		if(totalFileSize + fileSize < fileSize)
			return Error_overflow(2, 1, totalFileSize + fileSize, totalFileSize);

		totalFileSize += fileSize;
	}

	//Allocate; this can easily go out of mem since all files have to be available at once

	//Fill buffer

	foreach file:
		local file header
		encryption header (if applicable)
		file contents
		//data descriptor (if applicable)

	archive decryption header
	archive extra data record

	foreach file:
		central directory header

	end of central directory record
	end of central directory locator
	end of central directory record

	CentralDirectoryFooter lfh = (struct CentralDirectoryFooter) {

		.magicNumber = CentralDirectoryFooter_magicNumberValue,

		.diskCount = ...,
		.directoryDisk = ...,

		.recordsOnDisk = ...,
		.totalRecords = ...,

		.directorySize = ...,
		.offset = ...
	};

	//TODO: Adhere to 

	return Error_none();
}