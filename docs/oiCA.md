# oiCA (Oxsomi Compressed Archive)

*The oiCA format is an [oiXX format](oiXX.md), as such it inherits the properties from that such as compression, encryption and endianness.*

oiCA is a simplified version of a zip file.

It's made with the following things in mind:

- Ease of read + write.
- Better compression size due to Brotli:11 compression. Or fast compression using Brotli:1 (trade-off is compression size).
- Possibility of encryption using AES256-GCM.
  - Only the header is left unencrypted but is verified. The entire file table and contents are encrypted.
- A more modern version of zip with an easier spec / less bloat.
- Improved security.
- 64-bit file support.

## File format spec

```c
typedef enum ECAFlags {

	ECAFlags_None 						= 0,

   	//Whether SHA256 (1) or CRC32C (0) is used as hash
    //(Only if compression is on)

	ECAFlags_UseSHA256					= 1 << 0,

	//See ECAFileObject

	ECAFlags_FilesHaveDate				= 1 << 1,
	ECAFlags_FilesHaveExtendedDate		= 1 << 2,

    //Indicates EXXDataSizeType. E.g. (EXXDataSizeType)((b0 << 1) | b1)
    //This indicates the type the biggest file size uses

	ECAFlags_FileSizeType_Shift			= 3,
	ECAFlags_FileSizeType_Mask			= 3,

    //Chunk size of AES for multi threading. 0 = none, 1 = 10MiB, 2 = 100MiB, 3 = 500MiB

    ECAFlags_UseAESChunksA				= 1 << 5,
    ECAFlags_UseAESChunksB				= 1 << 6,

    ECAFlags_HasExtendedData			= 1 << 7,		//CAExtraData

    //Indicates EXXDataSizeType. E.g. (EXXDataSizeType)((b0 << 1) | b1)
    //This indicates the type the type for compression type if available.

	ECAFlags_CompressedSizeType_Shift	= 8,
	ECAFlags_CompressedSizeType_Mask	= 3,

    //Determines how many bytes the counter for files takes up.
    //If DirectoriesCountLong is set, it will allow up to 254 dirs, otherwise 64Ki-1.
    //If FilesCountLong is set, it will allow up to 64Ki, otherwise 4Gi.

    ECAFlags_DirectoriesCountLong		= 1 << 10,
    ECAFlags_FilesCountLong				= 1 << 11

} ECAFlags;

typedef struct CAHeader {		//4-byte aligned

    U32 magicNumber;			//oiCA (0x4143696F)

    U8 version;					//major.minor (%10 = minor, /10 = major (+1 to get real major)
    U8 type;					//(EXXCompressionType << 4) | EXXEncryptionType. Each enum should be <Count (see oiXX.md).
    U16 flags;					//ECAFlags

} CAHeader;

typedef struct CAExtraData {

	//Identifier to ensure the extension is detected.
	//0x0 - 0x1FFFFFFF are version headers, others are extensions.

	U32 extendedMagicNumber;

	U16 headerExtensionSize;		//To skip extended data size.

	U8 directoryExtensionSize;		//To skip directory extended data.
	U8 fileExtensionSize;			//To skip file extended data

} CAExtraData;

//A directory points to the parent.
//Important is to verify if there are no wrong indices.
//Small is used if there are <=254 folders, Large is used when there are more.

typedef U16 CADirectoryLarge;	//0xFFFF for root directory, else id of parent directory (can't >=self)
typedef U8 CADirectorySmall;	//0xFF for root directory, else id of parent directory (can't >=self)

//CADirectory is referred to as the type that fits folders (<=254 = Small else Large).

//Pseudo code; please manually parse the members. Struct is NOT aligned.

CAFileObject<FileSizeType, hasDateAndTime, isExtendedTime> {

    CADirectory parent;

    header.hasDateAndTime:
    	if !header.isExtendedTime:					//MS-DOS time.
		    U16 date;								//Day (5b), Month (4b), Year (Since 1980-2107 (7b))
		   	U16 time;								//Sec/2 (5b), Min (6b), Hour (5b)
		else: Ns timestamp;							//U64; (Unix timestamp * 1e9 + ns). 1970-2553

    FileSizeType size;								//File size
};

//Final file format; please manually parse the members.
//Verify if directories / files are linked to correct parent; has to be a folder available at that time.
//Verify if SHA256 and/or CRC32C is valid (if applicable).
//Verify if date and/or time is valid (if applicable).
//Verify if uncompressedSize > compressedSize.
//Verify if AES256 tag is valid with supplied data (if applicable).
//Verify if CAFile includes any invalid data.

CAFile {

    CAHeader header;

    [U16, U32][header.longFileCount] fileCount;				//<MAX (<64Ki or <4Gi)
    [U8, U16][header.longDirectoryCount] directoryCount;	//<MAX (<255 or <64Ki)

    if header.flags has extended data:
    	CAExtraInfo extraInfo;
	    U8 headerExt[extendedHeader];

    if compression:
    	EXXDataSizeType<uncompressedSizeType /* see header + directory extended size */> uncompressedSize;
	    U32[header.useSHA256 ? 8 : 1] hash;				//CRC32C or SHA256

    if encryption:
    	if blockCompression:
    		I32x4[blocks]; 		//(sum(entries) + blockSize - 1) / blockSize
		U8[12] iv;
		I32x4 tag;

    compress & encrypt the following if necessary:		//See oiXX.md

    	//This includes the names of everything in order.
    	//The names should only have characters in range [0x20, 0x7F>.
    	//Currently OxC3 doesn't support UTF8, but any valid unicode character
    	//	(above this range) could be supported by other implementations.
    	//Excluded characters are (<>:"|?*).
    	//You can't suffix with . (meaning ., .. are also out of question).
    	//Files such as CON, AUX, NUL, PRN, COM0-COM9, LPT0-LPT9 are also banned.
    	//	This is also the case if they're followed directly by an extension.
    	//Total file path can't exceed 128 characters.
    	//DLFile format MAY NOT use compression or encryption, since that's done by CAFile.
    	//DLFile should have the string flag set. If not, the file is invalid.
    	//	Could have the UTF8 flag set.
    	//DLFile also needs to include dirCount + fileCount entries.
    	//DLFile also doesn't include a leading magicNumber, context already implies it.
    	//File paths are insensitive; it can't have duplicates.

    	DLFile names;			//Names in order: [0, dirCount>, [dirCount, dirCount+fileCount>

    	//Directory and files

	    CADirectory[header.directoryCount] directories
            with stride (sizeof(CADirectory) + header.directoryExtensionSize);

	    CAFileObject[header.fileCount] files
            with stride (fileHeaderSize + header.fileExtensionSize);

		foreach file in files:
			U8[file.size] data;
}
```

The types are Oxsomi types; `U<X>`: x-bit unsigned integer, `I<X>` x-bit signed integer. Ki is Kibi like KiB (1024).

*Note: oiCA supports the ability to choose between 10MiB, 100MiB and 500MiB blocks for speeding up AES by multi threading.*

*Note2: When using encryption + compression, it has to be carefully assessed if the end-user can reveal anything sensitive that isn't meant to be revealed. A good example is secret header info that the client could intercept with HTTPS (BREACH or CRIME exploits). If the attacker doesn't control the input, then compression + encryption is ok.*

## Changelog

1.0: Basic format specification.