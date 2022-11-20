# oiDL (Oxsomi Data List)

**NOTE: oiDL is the successor of oiSL from ocore. This isn't the same format anymore. oiSL lacks a lot of important features (like storing UTF-8, plain data, compression and encryption) and is deprecated.**

oiDL is a collection of strings or data that can be used for any use case. One example as string list is file names, another might be node names in a scene file or dialog text. Another might be having multiple sprites and using an id to pick between them.

Commonly it's uncompressed and unencrypted since the file that contains it will do that. But still it does provide the possibility if needed.

Just like oiCA it's made with the following things in mind: 

- Ease of read + write.
- Better compression size than plain text due to Brotli:11 compression.
  - LZMA isn't used due to higher compression and decompression times for minor compression gains. Instead we opted to use Brotli:11 (https://cran.r-project.org/web/packages/brotli/vignettes/brotli-2015-09-22.pdf).
  - Brotli is slow to compress but fast to decompress and very efficient. Brotli:1 can be used if compression speed is important.
  - Deflate isn't used because it's slower and bulkier than Brotli.
- Possibility of encryption using AES256.
- An easy spec.
- Good security for parsing + writing.
- Support for strings or data.
  - UTF-8 support if needed (default is ASCII).
    - Currently not supported in OxC3 because UTF-8 strings aren't supported yet.
    - Avoid using UTF-8 if not needed. UTF-8 will be slower to parse.

## File format spec

```c

typedef enum EDLCompressionType {
	EDLCompressionType_None,
	EDLCompressionType_Brotli11,
	EDLCompressionType_Brotli1,
	EDLCompressionType_Count
} EDLCompressionType;

typedef enum EDLEncryptionType {
    EDLEncryptionType_None,
    EDLEncryptionType_AES256,
    EDLEncryptionType_Count
} EDLEncryptionType;

typedef enum EDLDataSizeType {
    
    EDLDataSizeType_U8,
    EDLDataSizeType_U16,
    EDLDataSizeType_U32,
    EDLDataSizeType_U64
    
} EDLDataSizeType;

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
	U8 sizeTypes;				//EDLDataSizeTypes: entryType | (uncompressedSizType << 2) | (dataType << 4)
	U8 padding;					//For alignment reasons
    
} DLHeader;

//Final file format; please manually parse the members.
//Verify if encoding is valid (if string list is used).
//Verify if everything's in bounds.
//Verify if SHA256 and/or CRC32C is valid (if applicable).
//Verify if uncompressedSize > compressedSize.
//Verify if DLFile includes any invalid data.

DLFile {
    
    DLHeader header;
    U8 headerExt[header.headerExtendedData];
    
    EDLDataSizeType<entrySizeType> entryCount;
    
    if compression:
    	EDLDataSizeType<uncompressedSizeType> uncompressedSize;
    
    if(header.useHash)
	    U32[header.useSHA256 ? 8 : 1] hash;				//CRC32C or SHA256
    
    compress & encrypt the following if necessary:
    
	    EDLDataSizeType<dataSizeType>[entryCount] entries
            with stride (sizeof(EDLDataSizeType<dataSizeType>) + header.perDataExtendedData);
    
		foreach dat in data:
			U8[dat.size] data;		//Non null terminated. We know the size
}
```

The types are Oxsomi types; `U<X>`: x-bit unsigned integer, `I<X>` x-bit signed integer.

## Invalid ASCII characters

If ASCII or UTF8 is used, certain characters are blacklisted to avoid problems after parsing them. The following ranges should be checked per character. If they fall outside of this range, they're invalid.

- [0x20, 0x7F> (Symbols, alphanumeric, space).
- 0x9 (Tab), 0xA (Newline \n), 0xD (Carriage return \r).
- Any UTF-8 character if UTF8 is set (e.g. codepoint >=0xC280). As long as it's a valid UTF-8 codepoint. See https://www.charset.org/utf-8.

## Changelog

1.0: Basic format specification.