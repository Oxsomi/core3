# oiDL (Oxsomi Data List)

*The oiDL format is an [oiXX format](oiXX.md), as such it inherits the properties from that such as compression, encryption and endianness.*

**NOTE: oiDL is the successor of oiSL from ocore. This isn't the same format anymore. oiSL lacked a lot of important features (like storing UTF-8, plain data, compression and encryption) and is deprecated.**

oiDL is a collection of strings or data that can be used for any use case. One example as string list is file names, another might be node names in a scene file or dialog text. Another might be having multiple sprites and using an id to pick between them.

Commonly it's uncompressed and unencrypted since the file that contains this file will do encryption/compression instead. But still it does provide the possibility if needed.

Just like any oiXX file it's made with the following things in mind:

- Ease of read + write.
- ~~Better compression size than plain text or binary due to Brotli:11 compression~~. (TODO: Once implemented)
- ~~Support of GPU-friendly decompression with for example G-Brotli (DirectStorage)~~ (TODO: Once implemented)
- Possibility of encryption using AES256-GCM.
  - Though header and entry sizes are left unencrypted (but are verified).
- An easy spec.
- Good security for parsing + writing.
- Support for strings or data.

## File format spec

```c

typedef enum EDLFlags {

	EDLFlags_None 					= 0,

	EDLFlags_UseSHA256				= 1 << 0,		//Whether SHA256 (1) or CRC32C (0) is used as hash

    EDLFlags_IsString				= 1 << 1,		//If true; string must contain valid UTF8 characters

    //Chunk size of AES for multi threading. 0 = 128KiB, 1 = 1MiB, 2 = 8MiB, 3 = 64MiB

    EDLFlags_UseAESChunksA			= 1 << 2,
    EDLFlags_UseAESChunksB			= 1 << 3,

    //If it includes DLExtraInfo

    EDLFlags_HasExtendedData		= 1 << 4

} EDLFlags;

typedef struct DLHeader {

	U32 magicNumber;			//oiDL (0x4C44696F); optional if a sub file

	U8 version;					//major.minor (%10 = minor, /10 = major (+1 to get real major))
	U8 flags;					//EDLFlags
	U8 type;					//(EXXCompressionType << 4) | EXXEncryptionType. Each enum should be <Count (see oiXX.md).
	U8 sizeTypes;				//EXXDataSizeTypes: entryType | (compressedSizType << 2) | (dataType << 4) (Upper 2 bits are empty)

} DLHeader;

typedef struct DLExtraInfo {

	//Identifier to ensure the extension is detected.
	//0x0 - 0x1FFFFFFF are version headers, others are extensions.
	U32 extendedMagicNumber;

	U16 extendedHeader;			//If extensions want to add extra data to the header
	U16 perEntryExtendedData;	//What to store per entry besides a DataSizeType

} DLExtraInfo;

//Final file format; please manually parse the members.
//Verify if encoding is valid (if string list is used).
//Verify if everything's in bounds.
//Verify if SHA256 or CRC32C is valid (if compressed).
//Verify if uncompressedSize < compressedSize.
//Verify if AES256 tag is valid with supplied data (if applicable).
//Verify if DLFile includes any invalid data.

DLFile {		//Must be 16-byte aligned

    DLHeader header;

    EXXDataSizeType<entrySizeType> entryCount;

    if header.flags has extended data:
    	DLExtraInfo extraInfo;
	    U8 headerExt[extendedHeader];

	EXXDataSizeType<dataSizeType>[entryCount] entries
		with stride (sizeof(EXXDataSizeType<dataSizeType>) + header.perDataExtendedData);

    if compression:		//TODO: Think this through with chunking
	    EXXDataSizeType<compressedSizeType> compressedSize;
	    U32[header.useSHA256 ? 8 : 1] hash;				//CRC32C or SHA256

    if encryption:
    
		//Authenticates the header and identifies the HKDF key domain for this stream.
    	//Do note that when this is a subFile, the parent controls the rootIv.
    	//This is to provide security guarantees when multiple oiDLs are embedded in an encrypted container.
    	//In that case, the header of the container must be verified with a tag independently (before touching this oiDL)
		// and the rootIv shall be parent.rootIv ^ U64x2(0, 1 + N) to distinguish embedded streams.
    	// (do note that N may not exceed U32_MAX - 1, since our IV is truncated to 12-bytes)
		optional if not subFile U32[3] rootIv;
    
		I32x4 tag;
    
    U8[N] pad; /* padding to make next section 16-byte aligned */

    //Entry contents are concatenated in order; encryption chunks may cross entry boundaries.
    //See oiXX.md for compression, if enabled.
    if encryption:
        foreach chunkSize-byte chunk in the concatenated payload (last chunk may be shorter):
            U8[12] nonce             //Fresh CSPRNG nonce for every encryption, including overwrites.
            U32 reserved             //Must be zero.
            U8[16] tag               //Authenticates the ciphertext and chunk AAD.
            U8[chunk.size] ciphertext;
    else:
        U8[payload.size] data;       //Concatenated entry contents, without null terminators.
}
```

The types are Oxsomi types; `U<X>`: x-bit unsigned integer, `I<X>` x-bit signed integer.

compressedSizeType is only present if compression is enabled. In all other cases it should read the size from the entries by summing them.

Entry counts aren't compressed nor encrypted, as the sum is required to know the final size of the oiDL and not a lot can be gained by compressing it and not a lot of security is lost if you know the size of each entry. If it is important, you could embed an uncompressed/unencrypted oiDL within another oiDL (or oiCA) that does encrypt and/or compress it.

With extended data every entry takes a full stride (its size, then perEntryExtendedData bytes that readers skip).
For encrypted files, the header tag covers all entry strides, including the extension bytes. The root IV (standalone
files only) and header tag follow the other header fields. The data starts at the next 16-byte boundary after the
complete header, including these encryption fields when present, as shown above.

The magic number in the header can only be absent if embedded in another file. An example is the file name table in an oiCA file.

*Note: ~~oiDL supports the ability to choose between 10MiB, 50MiB and 100MiB blocks for speeding up AES by multi threading. Though this is currently not supported in OxC3 (TODO:)~~*
*Note2: When using encryption + compression, it has to be carefully assessed if the end-user can reveal anything sensitive that isn't meant to be revealed. A good example is secret header info that the client could intercept with HTTPS (BREACH or CRIME exploits). If the attacker doesn't control the input, then compression + encryption is ok.*

## Valid ASCII/UTF8 characters

If ASCII or UTF8 is used, certain characters are blacklisted to avoid problems after parsing them. The following ranges should be checked per character. If they fall outside of this range, they're invalid.

- [0x20, 0x7F> (Symbols, alphanumeric, space).
- 0x9 (Tab), 0xA (Newline \n), 0xD (Carriage return \r).
- Any other UTF-8 character if UTF8 flag is set (e.g. codepoint >=0xC280). As long as it's a valid UTF-8 codepoint. See https://www.charset.org/utf-8, https://en.wikipedia.org/wiki/UTF-8.

Strings loaded through `DLFile_loadEntry` undergo the same validation as strings loaded during `DLFile_read`.
A failed validation leaves the entry deferred and releases the temporary allocation.

## Automatic loading

`DLFile_read` applies per-entry and aggregate allocation limits; entries outside those limits remain in their stream.
`DLFile_canReadFullyLoaded` predicts whether every entry would load under that policy with `forceKeepInStreams == false`.
It checks entry sizes in serialized order, regardless of the current cache or loaded state, without validating contents.
Formats that require all entries in memory can use this query before writing and reject deferred entries after reading.

## Encryption key schedule and chunk authentication

The encryption key supplied by the application is 32 bytes of raw key material, not a password.
Each new standalone output generates a fresh 12-byte root IV with the operating system CSPRNG.
An embedded oiDL uses the distinct identity supplied by its parent.

Keys use RFC 5869 HKDF-SHA256 with the raw key as IKM, the 12 root-IV bytes as salt, and a 32-byte output.
The header uses the exact 16 ASCII bytes `OxC3 oiDL header` as HKDF info.
The chunk data uses the exact 16 ASCII bytes `OxC3 stream data` as HKDF info.
Neither info string includes a null terminator.
The header tag authenticates the header and entry sizes as AAD, with no plaintext, under its derived key and root IV.
Chunk zero therefore uses a different key from the header, even if its independently generated nonce were equal.

Every chunk encryption, including an overwrite or retry, generates a new random 96-bit nonce.
Its 32-byte metadata record contains nonce bytes, a zero reserved U32, and the 128-bit authentication tag.
The ciphertext follows at a 16-byte aligned offset.
The chunk AAD is exactly 48 bytes, concatenated in this order:

| Field | Encoding |
| --- | --- |
| Domain | The 16 ASCII bytes `OxC3 chunk data1`, without a terminator |
| Stream identity | The 12 root-IV bytes |
| Configured chunk size | U32, little endian |
| Chunk index | U64, little endian, starting at zero |
| Plaintext size of this chunk | U64, little endian |

The expected index, size and identity come from the authenticated file structure, not from untrusted chunk metadata.
This prevents moving valid records between positions or streams that share the raw key.
It does not detect replay of an older valid record at the same position, or rollback of a complete file.
Applications that need rollback protection must maintain an authenticated generation outside the replaceable file.
A partially written record fails authentication; stream writes do not provide crash-atomic replacement.

A derived data key permits at most 2^32 encryption attempts, including attempts whose output write fails.
EncryptionStream enforces this per open handle.
The application must enforce the aggregate limit across reopened handles or copies sharing the same raw key and root IV,
then reencrypt the complete output under a fresh root IV.
Partial writes and cursor flushes can encrypt a chunk more than once and each encryption consumes this budget.
New writes keep the format version unchanged during pre-release development; files written with the tag-only chunk layout
must be regenerated.

## Changelog

1.0: Basic format specification.
