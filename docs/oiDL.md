# oiDL (Oxsomi Data List)

*The oiDL format is an [oiXX format](oiXX.md), as such it inherits the properties from that such as compression, encryption and endianness.*

**NOTE: oiDL is the successor of oiSL from ocore. This isn't the same format anymore. oiSL lacked a lot of important features (like storing UTF-8, plain data, compression and encryption) and is deprecated.**

oiDL is a collection of strings or data that can be used for any use case. One example as string list is file names, another might be node names in a scene file or dialog text. Another might be having multiple sprites and using an id to pick between them.

Commonly it's uncompressed and unencrypted since the file that contains this file will do encryption/compression instead. But still it does provide the possibility if needed.

Just like any oiXX file it's made with the following things in mind: 

- Ease of read + write.
- Better compression size than plain text or binary due to Brotli:11 compression. (TODO: Once implemented)
- Possibility of encryption using AES256-GCM.
- An easy spec.
- Good security for parsing + writing.
- Support for strings or data.
  - UTF-8 support if needed (default is ASCII).
    - Avoid using UTF-8 if not needed. UTF-8 will be slower to parse.

## File format spec

```c

typedef enum EDLFlags {
    
	EDLFlags_None 					= 0,

	EDLFlags_UseSHA256				= 1 << 0,		//Whether SHA256 (1) or CRC32C (0) is used as hash
    
    EDLFlags_IsString				= 1 << 1,		//If true; string must contain valid ASCII characters (or UTF8)
    EDLFlags_UTF8					= 1 << 2,		//ASCII (if off), otherwise UTF-8
        
    //Chunk size of AES for multi threading. 0 = none, 1 = 10MiB, 2 = 50MiB, 3 = 100MiB
        
    EDLFlags_UseAESChunksA			= 1 << 3,
    EDLFlags_UseAESChunksB			= 1 << 4
    
} EDLFlags;

typedef struct DLHeader {
    
	U32 magicNumber;			//oiDL (0x4C44696F)

	U8 version;					//major.minor (%10 = minor, /10 = major (+1 to get real major))
	U8 flags;					//EDLFlags
	U8 compressionType;			//EXXCompressionType. Should be <Count (see oiXX.md).
	U8 encryptionType;			//EXXEncryptionType

	U8 headerExtendedData;		//If new versions or extensions want to add extra data to the header
	U8 perEntryExtendedData;	//What to store per entry besides a DataSizeType
	U8 sizeTypes;				//EXXDataSizeTypes: entryType | (uncompressedSizType << 2) | (dataType << 4)
	U8 padding;					//For alignment reasons (set to 0)
    
} DLHeader;

//Final file format; please manually parse the members.
//Verify if encoding is valid (if string list is used).
//Verify if everything's in bounds.
//Verify if SHA256 or CRC32C is valid (if compressed).
//Verify if uncompressedSize > compressedSize.
//Verify if AES256 tag is valid with supplied data (if applicable).
//Verify if DLFile includes any invalid data.

DLFile {
    
    DLHeader header;
    U8 headerExt[header.headerExtendedData];
    
    EXXDataSizeType<entrySizeType> entryCount;
    
    if compression:
	    EXXDataSizeType<uncompressedSizeType> uncompressedSize;
	    U32[header.useSHA256 ? 8 : 1] hash;				//CRC32C or SHA256
    
    //Encryption header; see oiXX.md.
    //Such as (blocks ? I32x4[blocks]), U8[12] iv, I32x4 tag.
    
    compress & encrypt the following if necessary:		//See oiXX.md
    
	    EXXDataSizeType<dataSizeType>[entryCount] entries
            with stride (sizeof(EXXDataSizeType<dataSizeType>) + header.perDataExtendedData);
    
		foreach dat in data:
			U8[dat.size] data;		//Non null terminated. We know the size
}
```

The types are Oxsomi types; `U<X>`: x-bit unsigned integer, `I<X>` x-bit signed integer.

uncompressedSizeType is only present if compression is enabled. In all other cases it should be assumed that the remainder of the file is the rest of the data.

*Note: oiDL supports the ability to choose between 10MiB, 50MiB and 100MiB blocks for speeding up AES by multi threading.*

## Invalid ASCII/UTF8 characters

If ASCII or UTF8 is used, certain characters are blacklisted to avoid problems after parsing them. The following ranges should be checked per character. If they fall outside of this range, they're invalid.

- [0x20, 0x7F> (Symbols, alphanumeric, space).
- 0x9 (Tab), 0xA (Newline \n), 0xD (Carriage return \r).
- Any other UTF-8 character if UTF8 flag is set (e.g. codepoint >=0xC280). As long as it's a valid UTF-8 codepoint. See https://www.charset.org/utf-8, https://en.wikipedia.org/wiki/UTF-8.

## Changelog

1.0: Basic format specification.
