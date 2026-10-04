# oiCA (Oxsomi Compressed Archive)

*The oiCA format is an [oiXX format](oiXX.md), as such it inherits the properties from that such as compression, encryption and endianness.*

oiCA is a simplified version of a zip file.

It's made with the following things in mind:

- Ease of read + write.
- Encryption and compression as specified by oiDL.
  - Only the header is left unencrypted but is verified. The entire file table and contents are encrypted.
- A more modern version of zip with an easier spec / less bloat.
- Improved security.
- 64-bit file support by default with no messy footer.

## File format spec

```c
typedef enum ECAFlags {

	ECAFlags_None 						= 0,

	//See ECAFileObject

	ECAFlags_FilesHaveDate				= 1 << 0,
	ECAFlags_FilesHaveExtendedDate		= 1 << 1,

    ECAFlags_HasExtendedData			= 1 << 2,		//CAExtraData
        
    //Determines how many bytes the counter for files takes up.
    //If DirectoriesCountLong is set, it will allow up to 254 dirs, otherwise 64Ki-1.
    //If FilesCountLong is set, it will allow up to 64Ki, otherwise 4Gi.

    ECAFlags_DirectoriesCountLong		= 1 << 3,
    ECAFlags_FilesCountLong				= 1 << 4

} ECAFlags;

typedef struct CAHeader {		//4-byte aligned

    U32 magicNumber;			//oiCA (0x4143696F)

    U8 version;					//major.minor (%10 = minor, /10 = major (+1 to get real major, e.g. 10 = 2.0)
    U8 encryptionType;			//EXXEncryptionType (see oiXX.md)
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

//CADirectoryId is referred to as the type that fits folders (<=254 = Small else Large).
typedef [U8,U16][!!(header.flags & ECAFlags_DirectoriesCountLong)] CADirectory;

//CAFileId is referred to as the type that fits files (<=65535 = Small else Large).
//fileId may never be a folder id.
typedef [U16,U32][!!(header.flags & ECAFlags_FilesCountLong)] CAFileId;

//Pseudo code; please manually parse the members. Struct is NOT aligned.

CAFileObject<hasDateAndTime, isExtendedTime> {

    CADirectoryId parent;

    header.hasDateAndTime:
    	if !header.isExtendedTime:					//MS-DOS time.
		    U16 date;								//Day (5b), Month (4b), Year (Since 1980-2107 (7b))
		   	U16 time;								//Sec/2 (5b), Min (6b), Hour (5b)
		else: Ns timestamp;							//U64; (Unix timestamp * 1e9 + ns). 1970-2553
};

//Final file format; please manually parse the members.
//Verify if directories / files are linked to correct parent; has to be a folder available at that time.
//Verify if date and/or time is valid (if applicable).
//Verify if AES256 tag is valid with header data and header data of oiDLs.
//Verify if CAFile includes any invalid data.
//Files are written in order from root to child, so the parent must always appear before the child.
//Directories are always defined before files.
//Recursion is limited to 128.

CAFile {			//Must be 16-byte aligned

    CAHeader header;
    
    CAFileId fileCount;				//<MAX (<64Ki or <4Gi)
	CADirectoryId directoryCount;	//<MAX (<255 or <64Ki)
    
    if header.flags has extended data:
    	CAExtraData extraInfo;
	    U8 headerExt[extendedHeader];

    //parentIds per directory, must reference <selfId to avoid recursion.
    CADirectoryId[directoryCount] directories
        with stride (sizeof(CADirectory) + header.directoryExtensionSize);

    CAFileObject[fileCount] files
        with stride (fileHeaderSize + header.fileExtensionSize);
    
    if encryption:
    
    	//Our root IV is used to generate the tag that validates the header of this CAFile,
		// it also gets passed to the DLFiles embedded to ensure they aren't swapped around.
		//Basically the IV for them will be rootIv ^ U64x2(0, 1) and rootIv ^ U64x2(0, 2) respectively.
		U8[12] rootIv;
    
	    //This verifies the header of the oiCA and the headers of the oiDL (written after writing the DLFiles)
    	//To prevent runtime overhead, it doesn't actually check DLFile content.
    	//This is fine, as chunkId is contained in each iv of each DLFile's chunk and 
	    //	the same key is verified using the oiDL header. Invalid data would error right when it's decrypted in the stream.
		I32x4 tag;
    
    U8[N] pad;	//Padding to align to 16-byte

    //This includes the names of everything in order.
    //The names should only have characters in range [0x20, 0x7E] or a valid UTF8 sequence.
    //Excluded characters are (<>:"|?*).
    //You can't suffix with . (meaning ., .. are also out of question).
    //Files such as CON, AUX, NUL, PRN, COM0-COM9, LPT0-LPT9 are also banned.
    //	This is also the case if they're followed directly by an extension.
    //Total file path can't exceed 192 characters.
    //DLFile MAY use compression or encryption, since that's done by DLFile.
    //	In case of encryption, both names and content have to be encrypted and match header.encryptionType.
    //DLFile should have the string flag set. If not, the file is invalid.
    //	Could contain UTF8.
    //DLFile also needs to include dirCount + fileCount entries.
    //	directory names are defined at [0, dirCount - 1] and file names at [dirCount, dirCount + fileCount - 1]
    //DLFile also doesn't include a leading magicNumber, context already implies it.
    //File paths are insensitive; it can't have duplicates.
    //Implementations SHOULD keep these names in memory since they may not exceed 96 bytes each.
    // Names are relative, not absolute.
    DLFile names;
    
    U8[N] pad;	//Padding to align to 16-byte
    
    //Must be a data list (not string list).
    //DLFile also needs to include fileCount entries. (e.g. content[fileId] = content)
    //DLFile also doesn't include a leading magicNumber, context already implies it.
    DLFile content;
}
```

The types are Oxsomi types; `U<X>`: x-bit unsigned integer, `I<X>` x-bit signed integer. Ki is Kibi like KiB (1024).

All oiDL notes apply, see [oiDL format](oiDL.md).

## Changelog

1.0: Basic format specification.