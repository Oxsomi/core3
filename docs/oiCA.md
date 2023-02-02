# oiCA (Oxsomi Compressed Archive)

*The oiCA format is an [oiXX format](oiXX.md), as such it inherits the properties from that such as compression, encryption and endianness.*

oiCA is a simplified version of a zip file and is also partially inspired by the NARC file format. 

It's made with the following things in mind: 

- Ease of read + write.
- Better compression size due to Brotli:11 compression. Or fast compression using Brotli:1 (trade-off is compression size).
- Possibility of encryption using AES256-GCM.
- A more modern version of zip with an easier spec / less bloat.
- Improved security.
- 64-bit file support.

## File format spec

```c
typedef enum ECAFlags {
    
	ECAFlags_None 					= 0,

	ECAFlags_HasHash				= 1 << 0,		//Should be true if compression or encryption is on
	ECAFlags_UseSHA256				= 1 << 1,		//Whether SHA256 (1) or CRC32C (0) is used as hash

	//See ECAFileObject

	ECAFlags_FilesHaveDate			= 1 << 2,
	ECAFlags_FilesHaveExtendedDate	= 1 << 3,
    
    //Indicates ECAFileSizeType. E.g. (ECAFileSizeType)((b0 << 1) | b1)
    //This indicates the type the biggest file size uses
    
	ECAFlags_FileSizeType_Shift		= 4,
	ECAFlags_FileSizeType_Mask		= 3

    //Chunk size of AES for multi threading. 0 = none, 1 = 10MiB, 2 = 100MiB, 3 = 500MiB
        
    ECAFlags_UseAESChunksA		= 1 << 6,
    ECAFlags_UseAESChunksB		= 1 << 7
        
} ECAFlags;

typedef struct CAHeader {
    
    U32 magicNumber;			//oiCA (0x4143696F)
    
    U8 version;					//major.minor (%10 = minor, /10 = major (+1 to get real major)
    U8 flags;					//ECAFlags
    U8 type;					//(EXXCompressionType << 4) | EXXEncryptionType. Each enum should be <Count (see oiXX.md).
    U8 headerExtensionSize;		//To skip extended data size. Highest bit is b0 of uncompressed size type.
    
    U8 directoryExtensionSize;	//To skip directory extended data. Highest bit is b1 of uncompressed size type.
    U8 fileExtensionSize;		//To skip file extended data		
    U16 directoryCount;			//How many base directories are available. Should be < 0xFFFF
    
    U32 fileCount;				//How many files are available. Should be < 0xFFFF0000
    
} CAHeader;

//A directory points to the parent and to the children
//This allows us to easily access them without having to search all files
//Important is to verify if there are childs linking to the same parent or invalid child offsets

typedef struct CADirectory {
    
    U16 parentDirectory;		//0xFFFF for root directory, else id of parent directory (can't be self, unless root directory then it should be 0xFFFF)
    U16 childDirectoryStart;	//Where the child dirs start. 0xFFFF indicates no child directories
    
    U16 childDirectoryCount;	//Up to 64Ki child directories  
    U16 childFileCount;			//Up to 64Ki child files
    
    U32 childFileStart;			//Where the child files start. 0xFFFFFFFF indicates no child files
    
} CADirectory;

//Pseudo code; please manually parse the members. Struct is NOT aligned.

CAFileObject<FileSizeType, hasDateAndTime, isExtendedTime> {
    
    U16 parent;										//0xFFFF for root directory
    
    header.hasDateAndTime:
    	if !header.isExtendedTime:					//MS-DOS time.
		    U16 date;								//Day (5b), Month (4b), Year (Since 1980-2107 (7b))
		   	U16 time;								//Sec/2 (5b), Min (6b), Hour (5b)
		else: Ns timestamp;							//U64; (Unix timestamp * 1e9 + ns). 1970-2553
    
    FileSizeType size;								//File size 
};

//Final file format; please manually parse the members.
//Verify if directories / files are correctly linked to parent and children.
//Verify if SHA256 and/or CRC32C is valid (if applicable).
//Verify if date and/or time is valid (if applicable).
//Verify if uncompressedSize > compressedSize.
//Verify if AES256 tag is valid with supplied data (if applicable).
//Verify if CAFile includes any invalid data.

CAFile {
    
    CAHeader header;
    U8 headerExt[header.headerExtensionSize];
    
    if compression:
    	EXXDataSizeType<uncompressedSizeType /* see header + directory extended size */> uncompressedSize;
    
    if header.useHash:
	    U32[header.useSHA256 ? 8 : 1] hash;				//CRC32C or SHA256
    
    /* Encryption header; see oiXX.md. Such as (blocks ? I32x4[blocks]), U8[12] iv. */
    
    compress & encrypt the following if necessary:		//See oiXX.md
    
    	//This includes the names of everything in order.
    	//The names should only be [0-9A-Za-z_.-\ ]+ as well as non ASCII characters.
    	//This means that special characters are banned for interoperability reasons.
    	//You can't suffix with . (meaning ., .. are also out of question).
    	//Files such as CON, AUX, NUL, PRN, COM0-COM9, LPT0-LPT9 are also banned.
    	//Total file path can't exceed 128 characters.
    	//DLFile format MAY NOT use compression or encryption, since that's done by CAFile.
    	//DLFile should have the string flag set. If not, the file is invalid.
    
    	DLFile names;			//Names in order: [0, dirCount>, [dirCount, dirCount+fileCount>
    
    	//Directory and files
    
    	//Always needed; 0xFFFF directory id aka root ("."). This isn't present in DLFile names.
    	//Points to direct children of root directory.
    	//Parent HAS to be 0xFFFF.
    	//
    	CADirectory root;
    
	    CADirectory[header.directoryCount] directories
            with stride (sizeof(CADirectory) + header.directoryExtensionSize);
    	
	    CAFileObject[header.fileCount] files
            with stride (fileHeaderSize + header.fileExtensionSize);
    
		foreach file in files:
			U8[file.size] data;
    
    /* Encryption footer; see oiXX.md. Such as I32x4 tag */
}
```

The types are Oxsomi types; `U<X>`: x-bit unsigned integer, `I<X>` x-bit signed integer. Ki is Kibi like KiB (1024).

*Note: oiCA supports the ability to choose between 10MiB, 100MiB and 500MiB blocks for speeding up AES by multi threading.*

*NOTE: * 

## Changelog

1.0: Basic format specification.