# OxC3 formats (id: 0x1C31)

OxC3 formats contains only the following:

- rgba8 bmp writing.
- [oiCA](oiCA.md) file format for zip-like files.
- [oiDL](oiDL.md) file format for lists of data such as strings or specific file types (e.g. an array of sprites that have a similar meaning such as particle effects).
- [oiXX](oiXX.md) basic definitions for allowing providing a base for custom OxC3 file formats (such as oiCA and oiDL).
- Texture formats for providing information about what type of data a CPU-sided texture buffer contains.

## BMP

BMP support is very basic, it restricts usage to the following:

- Total image size in bytes must not exceed **2GiB**.
- Width and height must be <= **I32_MAX**.
- Flipped images are allowed.
- **BGRA8** or **BGR8** is required.

Usage via:

- Error **BMP_write**(Buffer buf, BMPInfo info, Allocator allocator, Buffer *result)
- Error **BMP_read**(Buffer buf, BMPInfo *info, Allocator allocator, Buffer *result)
  - returns additional info about the BMP it just read into BMPInfo.

Where BMPInfo has the following properties:

- U32 **w**, **h**: dimensions (width and height)
- Bool **isFlipped**: if the image should be flipped on load and/or write.
- Bool **discardAlpha**: if the image saves alpha or not (BGR8 is not supported in texture formats, so it's converted at runtime).
- U8 **textureFormatId**: ETextureFormatId_BGRA8 is currently the only one supported.
- I32 **xPixPerM**, **yPixPerM**: preferred display size in pixels per meter. Setting this to 0 is allowed as it might be ignored by the displayer.

## oiXX

oiXX files have the following standards:

- EXXCompressionType: None, Brotli11, Brotli1. **TODO**: Currently no compression supported in OxC3 0.2.
- EXXEncryptionType: None, AES256GCM. If enabled the file is encrypted with AES256GCM. No other encryption is currently supported (AES128GCM is less secure and about the same speed on modern CPUs).
- EXXDataSizeType: how large a size attribute can be. For example a file can specify these two bits to reduce space spent on the size types. U8, U16, U32 and U64.

Which can be used with the following functions:

- Error **Buffer_consumeSizeType**(Buffer *buf, EXXDataSizeType type, U64 *result): reinterprets buf->ptr as the size type that was given and offsets the buffer by that amount.
- no bounds checks if it's obvious that the bounds will be valid.
  - U64 **Buffer_forceReadSizeType**(const U8 *ptr, EXXDataSizeType type)
  - U64 **Buffer_forceWriteSizeType**(U8 *ptr, EXXDataSizeType type, U64 result)
- EXXDataSizeType **EXXDataSizeType_getRequiredType**(U64 v): check what the smallest type is that the variable fits in.

## oiCA

[oiCA](oiCA.md) can be accessed through the following functions:

- Error **CAFile_create**(CASettings settings, Archive archive, CAFile *caFile): takes ownership of Archive and turns it into a CAFile which can be used to convert into a Buffer.
- Bool **CAFile_free**(CAFile *caFile, Allocator alloc): free the CAFile and Archive it took ownership of.
- Error **CAFile_write**(CAFile caFile, Allocator alloc, Buffer *result): serialize CAFile into a Buffer.
- Error **CAFile_read**(Buffer file, const U32 encryptionKey[8], Allocator alloc, CAFile *caFile): read CAFile from a Buffer back into an Archive and CASettings.

Where *CASettings* contains the following:

- EXXCompressionType **compressionType**
- EXXEncryptionType **encryptionType**
  - If not None, the U32 **encryptionKey**[8] must be present (non 0s). Otherwise, the encryptionKey can be left empty (all 0s).
- ECASettingsFlags **flags**: 
  - Date info (present if any of &3 (bottom 2 bits)):
    - IncludeDate (1: short date; U32)
    - IncludeFullDate (2: OxC3 date; U64)
  - UseSHA256 (4: if the hash should be CRC32C (off) or SHA256 (on))

## oiDL

[oiDL](oiDL.md) can be created/destroyed through the following functions:

- Error **DLFile_create**(DLSettings settings, Allocator alloc, DLFile *dlFile): reserves some data to ensure addEntry doesn't reallocate as often.
- Bool **DLFile_free**(DLFile *dlFile, Allocator alloc): frees all related data to the DLFile.
- Pre-generated buffer or string lists. These files require the settings.dataType to be identical to their corresponding *EDLDataType*. All of these relinquish ownership of the underlying data to the DLFile unless the buffer or string is a ref. The list that's being referenced is also moved, so manual copies have to be done if that's not desired.
  - Error **DLFile_createBufferList**(DLSettings settings, ListBuffer buffers, Allocator alloc, DLFile *dlFile): create DLFile of type EDLDataType_Data with various buffer entries.
  - Error **DLFile_createAsciiList**(DLSettings settings, ListCharString strings, Allocator alloc, DLFile *dlFile): create DLFile of type EDLDataType_Ascii with various CharString entries (need to be Ascii).
  - Error **DLFile_createUTF8List**(DLSettings settings, ListBuffer strings, Allocator alloc, DLFile *dlFile): create DLFile of type EDLDataType_UTF8 with correct UTF8 encoding.
  - Error **DLFile_createList**(DLSettings settings, ListBuffer *buffers, Allocator alloc, DLFile *dlFile): generate oiDL from buffers and interpret the data as specified in settings.dataType. createList moves the buffer from *buffers* to the new DLFile and doesn't allocate it as a new list (and doesn't copy). However, since Ascii uses a ListCharString rather than a ListBuffer, it is internally converted.

Helper functions:

- Bool **DLFile_isAllocated**(DLFile file)
- U64 **DLFile_entryCount**(DLFile file)

New entries can be generated through the following functions (these relinquish ownership to the DLFile if it's not a ref):

- Error **DLFile_addEntry**(DLFile *dlFile, Buffer entry, Allocator alloc): add data entry. Requires DLFile to be created as EDLDataType_Data.
- Error **DLFile_addEntryAscii**(DLFile *dlFile, CharString entry, Allocator alloc): add ascii entry. Requires DLFile to be created as EDLDataType_Ascii.
- Error **DLFile_addEntryUTF8**(DLFile *dlFile, Buffer entry, Allocator alloc): add UTF8 entry. Requires DLFile to be created as EDLDataType_UTF8.

Then the file can be (de)serialized through the following functions:

- Error **DLFile_write**(DLFile dlFile, Allocator alloc, Buffer *result): serialize DLFile into a Buffer.
- Error **DLFile_read**(Buffer file, const U32 encryptionKey[8], Bool isSubfile, Allocator alloc, DLFile *dlFile): deserialize a buffer back into a DLFile.
  - isSubFile sets HideMagicNumber flag and allows leftover data after the oiDL, this is used for an oiCA to store an oiDL in it without having to specify the magicNumber.
  - On successful write, DLFile::readLength contains the read length, so if it's a subfile it can jump ahead to the next data.

Where *DLSettings* contains the following:

- EXXCompressionType **compressionType**
- EXXEncryptionType **encryptionType**
  - If not none, the U32 **encryptionKey**[8] must be present (non 0s). Otherwise, the encryptionKey can be left empty.
- EDLDataType **dataType**: the type of data the oiDL represents. When EDLDataType_Ascii is used only ListCharString *entryStrings* is valid and must contain a valid ascii string (CharString_isAscii), otherwise ListBuffer *entryBuffers* must be used. When EDLDataType_UTF8 is used, the buffer must be a properly encoded UTF8 string.
- EDLSettingsFlags **flags**: UseSHA256 (1, if SHA256 is used rather than CRC32C for a hash check), HideMagicNumber (2, if the magic number is hidden during serialization, to save a few bytes; only do this if the magic number can be safely inferred).

## Texture formats

ETextureFormat is an enum and consist of the following properties:

- **alphaBits**, **blueBits**, **greenBits**, **redBits** (6 bits); allowing representing up to 128 bits for the channel bit depth with a stepSize of 2 (bitCount is divided by 2 before storing in the enum).
  - If texture primitive is Compressed, then the bits are only 0 or 1. In that case, they occupy only 3 bits but their alignment is the same (e.g. alphaBits at 0, blueBits at 6, greenBits at 12, redBits at 18).
- **texture primitive** (3 bits); allowing Undefined (0), UNorm (1), SNorm (2), UInt (3), SInt (4), Float (5), Compressed (6) and UnormBGR (7).
- **total bit length** (5 bits); total size of all channels combined in bits. Stored in range [ 4, 128 ] with stepSize 4.

If the texture primitive is Compressed, the layout is a bit different:

- The abgr bits are still located at 0,6,12,18 but they only signify if the color is present rather than a bitDepth, because the bitDepth is irrelevant. This means that effectively they only contain 0-1, and so the lower 3 bits (&7) are used for that information.
- **compType** is stored at bit 9 with 3 bits and represents UNorm, SNorm, Float or sRGB. This is the primitive format of the compressed data.
- Alignments are the required block size of the compression algorithm. For example BCn forces 4x4 while ASTC has various block sizes for different purposes.
  - The alignment is encoded as follows: [ 4, 5, 6, 8, 10, 12 ]
  - **alignmentY** is stored at bit 15 with 3 bits.
  - **alignmentX** is stored at bit 21 with 3 bits.
- **blockSizeBits** is encoded at bit 27 with 3 bits and ranges from 64 - 512 bits, with stepSize 64.
- **algo** is encoded at bit 30 with 2 bits (only 1 used, 1 reserved), representing ASTC (0) or BCn (1).

Because this is so much data packed in an enum, a more storage efficient version can be used that only uses up to 8 bits. It's called ETextureFormatId and it contains the index only and can be mapped to the full format using ETextureFormatId_unpack[id].

To make usage simpler, the following helper functions have been added:

- ETexturePrimitive **ETextureFormat_getPrimitive**(ETextureFormat f)
- Bool **ETextureFormat_getIsCompressed**(ETextureFormat f)
- U64 **ETextureFormat_getBits**(ETextureFormat f)
- U64 **ETextureFormat_getAlphaBits**(ETextureFormat f)
- U64 **ETextureFormat_getBlueBits**(ETextureFormat f)
- U64 **ETextureFormat_getGreenBits**(ETextureFormat f)
- U64 **ETextureFormat_getRedBits**(ETextureFormat f)
- Bool **ETextureFormat_hasRed**(ETextureFormat f)
- Bool **ETextureFormat_hasGreen**(ETextureFormat f)
- Bool **ETextureFormat_hasBlue**(ETextureFormat f)
- Bool **ETextureFormat_hasAlpha**(ETextureFormat f)
- U8 **ETextureFormat_getChannels**(ETextureFormat f)
- ETextureCompressionType **ETextureFormat_getCompressionType**(ETextureFormat f): UNorm, SNorm, Float or sRGB.
- ETextureCompressionAlgo **ETextureFormat_getCompressionAlgo**(ETextureFormat f): ASTC or BCn.
- Bool **ETextureFormat_getAlignment**(ETextureFormat f, U8 *x, U8 *y): unpack x and y alignment (both are required), will return false when one of them isn't supplied or if the format doesn't use compression.
- U64 **ETextureFormat_getSize**(ETextureFormat f, U32 w, U32 h): calculate the size in bytes of the texture, respecting alignment. Returns U64_MAX if alignment is incorrect.

The following formats are present:

- Raw formats (usable as write without compression):
  - BGRA8, BGR10A2 for Swapchain usage.
  - UNorm formats: R(G(BA))(8/16)
  - SNorm formats: R(G(BA))(8/16)s
  - SInt formats: R(G(BA))(8/16/32)i
  - UInt formats: R(G(BA))(8/16/32)u
  - Float formats: R(G(BA))(16/32)f
- Compressed formats:
  - BCn:
    - UNorm: BC(4/5/7)
    - Float: BC6H
    - SNorm: BC(4/5)s
    - sRGB: BC7_sRGB
  - ASTC:
    - UNorm: [5,6,8,10]x5_sRGB
    - sRGB: [5,6,8,10]x5_sRGB