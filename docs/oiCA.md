# oiCA (Oxsomi Compressed Archive)

oiCA is a simplified version of a zip file and is also partially inspired by the NARC file format. 

It's made with the following things in mind: 

- Ease of read + write.
- Good compression / small file size.
- Possibility of encryption.
- A more modern version with an easier spec / less bloat.
- Improved security.
- 64-bit file support.

## File format spec

```c
typedef enum ECAFlags {

    ECAFlags_None = 0,
    ECAFlags_Encryption = 1 << 0,		//If this is turned on; files REQUIRE a hash (CRC32 or SHA256)
    
    ECAFlags_Invalid = 0x7F << 1		//Reserved, but for now threat this as invalid file!
    
} ECAFlags;

typedef struct CAHeader {
    
    U32 magicNumber;		//oiCA (0x4143696F)
    
    U8 flags;				//ECAFlags
    U8 version;				//major.minor (%10 = minor, /10 = major)
    U16 directoryCount;		//How many base directories are available. Should be < 0xFFFF
    
    U32 fileCount;			//How many files are available. Should be < 0xFFFF0000
    
} CAHeader;

//A directory points to the parent and to the children
//This allows us to easily access them without having to search all files

typedef struct CADirectory {
    
    U16 parentDirectory;		//0xFFFF for root directory, else id of parent directory (can't be self)
    U16 childDirectoryStart;	//Where the child dirs start. 0xFFFF indicates no child directories
    
    U16 childDirectoryCount;	//Up to 64Ki child directories  
    U16 childFileCount;			//Up to 64Ki child files
    
    U32 childFileStart;			//Where the child files start. 0xFFFFFFFF indicates no child files
    
} CADirectory;

//A file is a blob of data with some extra info

typedef enum ECAFileSizeType {
    
    ECAFileSizeType_U8,
    ECAFileSizeType_U16,
    ECAFileSizeType_U32,
    ECAFileSizeType_U64
    
} ECAFileSizeType;

typedef enum ECAFileTypeBits {
  
    ECAFileFlags_None		= 0,
    
    //First 2 bits are EFileSizeType
    //This allows us to shave off a few bytes per file
    //Since a lot of files are only <256, <64KiB or <2GiB
    
    ECAFileFlags_FileSizeType_Mask	= 3,
    ECAFileFlags_FileSizeType_Shift = 0,
    
    //Whether or not a feature is present:
    
    ECAFileFlags_HasDateAndTime	= 1 << 2,		//If it has a date (YYYY-MM-dd) and time (HH-mm-ss)
    
    ECAFileFlags_HasHash		= 1 << 3,		//If it has a hash (CRC32 or SHA256). True if encrypted
    ECAFileFlags_UseSHA256		= 1 << 4,		//What type of hash to use (CRC32 (0), SHA256 (1))
    
    ECAFileFlags_IsUncompressed	= 1 << 5,		//Useful if compressing is worse or equal
    ECAFileFlags_IsExtendedTime = 1 << 6		//Whether or not to use Ns (Oxsomi Time) instead of MS-DOS
        
	ECAFileFlags_Reserved		= 1 << 7
    
} ECAFileTypeBits;

//Pseudo code; please manually parse the members. Struct is NOT aligned.

CAFileObject<FileSizeType, hasDateAndTime, hasHash, useSHA256, isUncompressed, isExtendedTime> {
    
    U16 parent;											//0xFFFF for root directory
    
    U8 fileInfo;										//ECAFileTypeBits
    
    hasDateAndTime:
    	if !isExtendedTime:							//MS-DOS time.
		    U16 date;								//Day (5b), Month (4b), Year (Since 1980-2107 (7b))
		   	U16 time;								//Sec/2 (5b), Min (6b), Hour (5b)
		else: Ns timestamp;							//U64; (Unix timestamp * 1e9 + ns). 1970-2553
    
    hasHash:
    	U32[useSHA256 ? 8 : 1] hash;					//CRC32 or SHA256
    
    FileSizeType uncompressedSize;						//Uncompressed size 
    !isUncompressed: FileSizeType compressedSize;		//Compressed size
    
};

//Final file format; please manually parse the members.
//Verify if directories / files are correctly linked to parent and children.
//Verify if SHA256 and/or CRC32 are valid (if applicable).
//Verify if date and/or time is valid (if applicable).
//Verify if uncompressedSize > compressedSize.
//Verify if CAFile includes any invalid data.

CAFile {
    
    CAHeader header;
    
    //This includes the names of everything in order.
    //The names should only be [0-9A-Za-z_.\ ]+ as well as non ASCII characters.
    //This means that special characters are banned for interoperability reasons.
    //You can't suffix with . (meaning ., .. are also out of question).
    
    SLFile names;			//Names in order: [0, dirCount>, [dirCount, dirCount+fileCount>
    
    //Directories and files
    
    CADirectory[header.directoryCount] directories;
    
    CAFileObject[header.fileCount] files;
    
    foreach file in files:
    	U8[!file.isUncompressed ? file.compressedSize : file.uncompressedSize] data;
}
```

The types are Oxsomi types; `U<X>`: x-bit unsigned integer, `I<X>` x-bit signed integer. Ki is Kibi like KiB (1024).
