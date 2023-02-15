# oiXX standard

The oiXX standard is a standard of how Oxsomi file formats are structured.

## Guidelines

The file formats should always:

- Support the possibility of compression + encryption.
- Be as small as possible, provided this doesn't harm usability.
- Be simple to understand and read/write in code.
- Be secure. An example of how not to do this is file formats that search through the data for magic numbers. As sometimes data is included in between there that might be mistaken for the magic number that means the next section. Not only is this slow, it's also insecure. A better way is to just use an offset that points directly to it and can easily be verified if it's correct.
- Write a clear spec on the layout of the file structure.
- Start with a U32 magicNumber. This is the 4-character indicator of the file format. This should always start with oi if it adheres to this standard and must not conflict with any other formats. The last 2 characters don't have to be ASCII, so this gives a theoretical limit of 64Ki file formats, though more logically 36^2 (~1300). 
- Continue with a version id (could be any type, as long as it's deemed sufficient in the long run (U8 would be okay for mostly static data types)), which uses % N= minor, / N= major (starting from 1.0). Where N is 10 (U8), 100 (U16) or 1000 (U32+). If this version would run out, an extended header should include the next type that stores higher majors. 
- Continue with a flags, which contains any important flags for reading the file back. If space runs out, extended header should be used. A U8 is only deemed sufficient if the format rarely changes. It is illegal to check for unused flags to determine if the file is valid; doing so could break backwards compatibility for new versions.
- Then should be the compression and/or encryption type. This is generally a 4-bit pair (compressionType << 4) | encryptionType.
- Then would be the extension header. Generally this is used to allow adding new attributes as an extension, while allowing backwards compatibility. For example oiCA might use this to support custom file attributes that are optional for others to read. The layout is (U32 magicNumber, U16 extendedDataHeader, `<per file type extension>`). If magicNumber < 0x1FFFFFFF it indicates a version extension header. This means the version is deemed optional and can still be read by older versions. If the version breaks existing functionality, it should update the version id. In this case, the extended versions should be made core parts of the new version.
- The rest of the header is dependent on the file type. Though it is required to ensure it pads correctly. This means that the header itself conforms to the following alignment:
  - 16-byte elements such as U16, I16 should be aligned to 2-byte boundaries.
  - 32-byte elements such as U32, I32, F32 should be aligned to 4-byte boundaries.
  - 64-byte elements such as U64, I64, F64, I32x2, F32x2 should be aligned to 8-byte boundaries.
  - 128-byte elements such as F32x4, I32x4 should be aligned to 16-byte boundaries.
- The highest size element determines the alignment. The struct has to explicitly be packed to ensure correct cross platform behavior. So if a 128-bit vector is present, the size needs to be padded to be a multiple of 8 bytes.
- Any platform dependent info such as size_t or void* *SHOULD NOT* be serialized alongside the data. This should rather be a defined type, instead of a platform dependent one. For example a small enum that determines what size it could be (1,2,4,8) in the header. This is called a EXXDataSizeType and is referenced how long this is for certain fields in the header. For example, an oiCA entry size could be 1 byte and the entry count could be 2 bytes. 
- Alongside the compressed data (if enabled), it should contain a hash. The hash can either be CRC32C or SHA256 (set by the flag). SHA256 should only be used if security is important but likely not if encryption is used, since that already has a tag that is safe enough to rely on (especially since the header is validated too). This hash has to be calculated over the *ENTIRE* uncompressed contents. This hash is over the uncompressed data + header, not the encrypted data, since the encryption algorithm needs to know the hash before it encrypts. This hash should be calculated with the hash in header initialized as 0. Encryption doesn't require a SHA or CRC checksum unlike compression; where the compression could be done on corrupt file for example; the reason being that the encryption itself uses a tag which is generated using the header.
- Target 64-bit systems and beyond. Assume that 64-bit sizes for buffers are available. The implementation can choose not to support it, but that's up to the app to implement as they want.

## Order of encryption / compression

If both are used, compression should be done BEFORE encryption, as doing it the other way around will make it useless.

## Encryption algorithms

Currently only AES256-GCM is supported. This means that the following would be how the data looks:

- If chunk compression: 16-byte tag per chunk.
- 12-byte IV (initialization vector).
- 16-byte tag to validate integrity.
- U8[compressedSize (totalSize if no compression)]

What's special about AES256-GCM's tag is that the header data is fed as additional data, to ensure that it hasn't been messed with after encryption. The only way to modify this is if you know the key. This is basically everything preceding the compressed + encrypted data.  

To support chunk encryption (for multi threading), a format is allowed to specify what type of chunk size it supports. These can then be provided via a flag or a supplied size in the header. The recommended chunk size is between 10 MiB and 1 GiB (~50MiB should be fine). When this is done, a new key should be used for the total (since the same iv might accidentally be generated multiple times). The iv should be initialized to a random number and each chunk increases it by 1, thus allowing easy multi threading. Each chunk has a tag, these tags need to be prepended in front of the IV and needs to be included into the end of the additional data when creating the final tag. This ensures that our data hasn't been switched around or messed with.

## Compression algorithms

Currently the only compression algorithm supported will be Brotli:11 and Brotli:1. In the future another Brotli version will be supported; G-Brotli (GPU friendly Brotli). Brotli:11 is space optimal and fast to uncompress but slow to compress and Brotli:1 is fast to compress and uncompress, but not space optimal.

Reasons to use Brotli:

- LZMA isn't used due to high decompression times for minor compression gains. Instead we opted to use Brotli:11 and Brotli:1 (https://cran.r-project.org/web/packages/brotli/vignettes/brotli-2015-09-22.pdf).
- Deflate isn't used because it's bulkier (in compression size) than Brotli.
- Brotli allows a quality setting to swap between ideal compression speed and compression size. Brotli:11 is optimal for decompression times and minimizing size, while Brotli:1 is optimal for (de)compression times but sacrifices size.

When compressing, it should contain the compressed size after the header and the size type for the compressed size (e.g. if size < 2^16 it can use 2 bytes).

## Endianness

All fields are little endian. The file format can be supported on big endian machines, but little endian HAS to be used when reading and writing from the format. Little endian was chosen because almost every system uses it, so occurring a performance penalty for every system to keep it big endian seems like a bad design choice.

## oiXX header

```c
typedef enum EXXCompressionType {	//Has to be represented as 4-bit at the least
	EXXCompressionType_None,
	EXXCompressionType_Brotli11,
	EXXCompressionType_Brotli1,
	EXXCompressionType_Count
} EXXCompressionType;

typedef enum EXXEncryptionType {	//Has to be represented as 4-bit at the least
    EXXEncryptionType_None,
    EXXEncryptionType_AES256GCM,
    EXXEncryptionType_Count
} EXXEncryptionType;

typedef enum EXXDataSizeType {		//Can be represented as a 2-bit array or larger
    EXXDataSizeType_U8,
    EXXDataSizeType_U16,
    EXXDataSizeType_U32,
    EXXDataSizeType_U64
} EXXDataSizeType;
```

## Changelog

Version 1.0: Added initial spec.