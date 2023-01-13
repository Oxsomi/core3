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
- Continue with a U8 version, which uses % 10 = minor (starting from 1.0), / 10 = major. If this version would run out, an extended header should include the next type that stores higher majors. 
- Continue with a U8 flags, which contains any important flags for reading the file back. If space runs out, extended header should be used.
- Then should be the compression and/or encryption type. This can be either a 4-bit pair (compressionType << 4) | encryptionType or a byte pair (compressionType, encryptionType). Which one is picked depends on if the struct would run out of alignment if the smaller is picked.
- Then would be the extension sizes. The first one being the extended header size; 0 being default and means not available. Generally this is used to allow adding new attributes as an extension, while allowing backwards compatibility. For example oiCA might use this to support custom file attributes that are optional for others to read. 
- The rest of the header is dependent on the file type. Though it is required to ensure it pads correctly. This means that the header itself conforms to the following alignment:
  - 16-byte elements such as U16, I16 should be aligned to 2-byte boundaries.
  - 32-byte elements such as U32, I32, F32 should be aligned to 4-byte boundaries.
  - 64-byte elements such as U64, I64, F64, I32x2, F32x2 should be aligned to 8-byte boundaries.
  - 128-byte elements such as F32x4, I32x4 should be aligned to 16-byte boundaries.
- Any platform dependent info such as size_t or void* *SHOULD NOT* be serialized alongside the data. This should rather be a defined type, rather than a platform dependent one. For example a small enum that determines what size it could be (1,2,4,8) in the header
- The highest size element determines the alignment. The struct has to explicitly be packed to ensure correct cross platform behavior. So if a 128-bit vector is present, the size needs to be padded to be a multiple of 8 bytes.
- Before the compressed / encrypted data, it should contain a hash if the flag enabled it. This flag should always be set if compression is enabled, to validate the result. The hash can either be CRC32C or SHA256 (set by the flag). SHA256 should only be used if security is important but likely not if encryption is used, since that already has a tag that is safe enough to rely on. This hash has to be calculated over the *ENTIRE* uncompressed contents. This hash is over the uncompressed data + header, not the encrypted data, since the encryption algorithm needs to know the hash before it encrypts. This hash should be calculated with the hash in header initialized as 0. Encryption doesn't require a SHA or CRC checksum unlike compression; where the compression could be done on corrupt file for example; the reason being that the encryption itself uses a tag which is generated using the header.

## Order of encryption / compression

If both are available, compression should be done BEFORE encryption, as doing it the other way around will make it useless.

## Encryption algorithms

Currently only AES256-GCM is supported. This means that the following would be how the data looks:

- 12-byte IV (initialization vector).
- U8[compressedSize (totalSize if no compression)]
- 16-byte tag to validate integrity.

What's special about AES256-GCM's tag is that the header data is fed as additional data, to ensure that it hasn't been messed with after encryption. The only way to modify this is if you know the key. This is basically everything preceding the compressed + encrypted data.  

## Compression algorithms

Currently the only compression algorithm supported will be Brotli:11. In the future two more Brotli versions will be supported; G-Brotli (GPU friendly Brotli) and Brotli:1 (A version of Brotli that's faster to compress but less optimal).

Reasons to use Brotli:

- LZMA isn't used due to higher compression and decompression times for minor compression gains. Instead we opted to use Brotli:11 (https://cran.r-project.org/web/packages/brotli/vignettes/brotli-2015-09-22.pdf).
- Brotli is slow to compress but fast to decompress and very efficient. 
- Deflate isn't used because it's slower and bulkier than Brotli.

## Endianness

All fields are little endian. The file format can be supported on big endian machines, but little endian HAS to be used when reading and writing from the format. Little endian was chosen because almost every system uses it, so occurring a performance penalty for every system to keep it big endian seems like a bad design choice.

## oiXX header

```c
typedef enum EXXCompressionType {	//Has to be represented as 4-bit at the least
	EXXCompressionType_None,
	EXXCompressionType_Brotli11,
	EXXCompressionType_Count
} EXXCompressionType;

typedef enum EXXEncryptionType {	//Has to be represented as 4-bit at the least
    EXXEncryptionType_None,
    EXXEncryptionType_AES256GCM,
    EXXEncryptionType_Count
} EXXEncryptionType;

typedef enum EXXDataSizeType {		//Can be represented as a 2-bit array for example
    
    EXXDataSizeType_U8,
    EXXDataSizeType_U16,
    EXXDataSizeType_U32,
    EXXDataSizeType_U64
    
} EXXDataSizeType;
```

## Changelog

Version 1.0: Added initial spec.