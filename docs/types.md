# OxC3 types (id: 0x1C30)

OxC3 types contains a lot of very basic types; it's the STL of OxC3. All these types are used throughout the entirety of OxC3.

## Basic defines

- **_SIMD** is what type of SIMD is used
  - *types/types.h* defines these, while _SIMD is a compile define.
  - **SIMD_NONE**: Indicates a fallback mode only, useful for validation, debugging and adding new platforms with a lot less effort (if they use a different SIMD setting that isn't supported yet).
  - **SIMD_SSE**: Indicates the usage of SSE4.2/SSE4.1/SSE2/SSE/SSE3/SSSE3, AES, PCLMULQDQ, BMI1 and RDRAND and the possible usage of SHA instructions. This allows quicker and/or safer encryption/hashing & computation.
  - (**unsupported**): SIMD_NEON: Indicates the usage of ARM NEON intrinsics.
- **_ARCH** is what type of architecture is used
  - *types/types.h* defines these, while _ARCH is a compile define.
  - **ARCH_NONE**: Unknown architecture; likely abstracted away from assembly level.
  - **ARCH_X64**: X86_64 architecture.
  - (**unsupported**): ARCH_ARM.
- **_PLATFORM_TYPE** is what kind of platform is running
  - *types/platform_types.h* defines these, while _PLATFORM_TYPE is a compile define.
  - **PLATFORM_WINDOWS**.
  - **PLATFORM_LINUX**.
  - **PLATFORM_ANDROID**.
  - **PLATFORM_WEB**.
  - **PLATFORM_IOS**.
  - **PLATFORM_OSX**.
- **_RELAX_FLOAT**: Indicates that the floating point operations aren't necessarily accurate, but are faster.
- **_FORCE_FLOAT_FALLBACK**: All explicit float cast operations (e.g. F32_castF64) are software simulated.
- **impl** indicates that one of the other source files will define this; mostly a platform or API dependent version. For example impl is regularly used to indicate a function that is implemented by the Vulkan or Windows backends rather than one that is generic and used across other platforms & APIs.
- **user_impl** indicates that the user is meant to define the function themselves when using the framework. This is used by OxC3 platforms to take control of the main function but provide the Program_run & Program_exit function for the user.

## Basic data types (types/types.h)

- All types use PascalCase, even ints, floats, bool, etc.
- Basic arithmetic data types are suffixed by the bits that are taken by that type.

| Type name          | Description                                                  | C type                       |
| ------------------ | ------------------------------------------------------------ | ---------------------------- |
| **C8**             | 8-bit char                                                   | char                         |
| **U8**             | 8-bit unsigned int                                           | uint8_t                      |
| **I8**             | 8-bit signed int                                             | int8_t                       |
| **U16**            | 16-bit unsigned int                                          | uint16_t                     |
| **I16**            | 16-bit signed int                                            | int16_t                      |
| **F16**            | **cast only**: 16-bit IEEE754 float                          | uint16_t (manual casting)    |
| **U32**            | 32-bit unsigned int                                          | uint32_t                     |
| **I32**            | 32-bit signed int                                            | int32_t                      |
| **F32**            | 32-bit IEEE754 float                                         | float                        |
| **U64**            | 64-bit unsigned int                                          | uint64_t                     |
| **I64**            | 64-bit signed int                                            | int64_t                      |
| **F64**            | 64-bit IEEE754 float                                         | double                       |
| **U128**           | 128-bit unsigned int (limited functionality)                 | complex (__uint128 or I32x4) |
| **BigInt**         | Big unsigned int (allocated per 8 bytes)                     | U64[]                        |
| **BF16**           | **cast only**: BFloat (8 bit exponent, 7 bit mantissa)       | U16                          |
| **TF19**           | **cast only**: TensorFloat (8 bit exponent, 10 bit mantissa) | U32 (19 used)                |
| **PXR24**          | **cast only**: PixarFloat (8 bit exponent, 15 bit mantissa)  | U32 (24 used)                |
| **FP24**           | **cast only**: AMD Float24 (7 bit exponent, 16 bit mantissa) | U32 (24 used)                |
| **Ns**             | 64-bit timestamp (unsigned); nanoseconds                     | U64                          |
| **DNs**            | 64-bit timestamp (signed); delta nanoseconds                 | I64                          |
| **Bool**           | Boolean                                                      | bool                         |
| **ECompareResult** | Compare result: Lt, Eq, Gt                                   | enum (I32)                   |
| **F8**             | **cast only**: MiniFloat (4 bit exponent, 3 bit mantissa)    | U8                           |

## Basic constants (types/types.h)

| Name        | Type | Value                                     |
| ----------- | ---- | ----------------------------------------- |
| **KIBI**    | U64  | 1024^1                                    |
| **MIBI**    | U64  | 1024^2                                    |
| **GIBI**    | U64  | 1024^3                                    |
| **TIBI**    | U64  | 1024^4                                    |
| **PEBI**    | U64  | 1024^5                                    |
| **KILO**    | U64  | 10^3                                      |
| **MEGA**    | U64  | 10^6                                      |
| **GIGA**    | U64  | 10^9                                      |
| **TERA**    | U64  | 10^12                                     |
| **PETA**    | U64  | 10^15                                     |
| **MU**      | Ns   | 1e3                                       |
| **MS**      | Ns   | 1e6                                       |
| **SECOND**  | Ns   | 1e9                                       |
| **MIN**     | Ns   | 60e9                                      |
| **HOUR**    | Ns   | (60 * 60)e9                               |
| **DAY**     | Ns   | (60 * 60 * 24)e9                          |
| **WEEK**    | Ns   | (60 * 60 * 24 * 7)e9                      |
| **U8_MIN**  | U8   | 0                                         |
| **U16_MIN** | U16  | 0                                         |
| **U32_MIN** | U32  | 0                                         |
| **U64_MIN** | U64  | 0                                         |
| **U8_MAX**  | U8   | 255 (0xFF)                                |
| **U16_MAX** | U16  | 65535 (0xFFFF)                            |
| **U32_MAX** | U32  | 4294967295 (0xFFFFFFFF)                   |
| **U64_MAX** | U64  | 18446744073709551615 (0xFFFFFFFFFFFFFFFF) |
| **I8_MIN**  | I8   | -128 (0x80)                               |
| **C8_MIN**  | C8   | -128 (0x80)                               |
| **I16_MIN** | I16  | -32768 (0x8000)                           |
| **I32_MIN** | I32  | -2147483648 (0x80000000)                  |
| **I64_MAX** | I64  | -9223372036854775808 (0x8000000000000000) |
| **I8_MAX**  | I8   | 127 (0x7F)                                |
| **C8_MAX**  | C8   | 127 (0x7F)                                |
| **I16_MAX** | I16  | 32767 (0x7FFF)                            |
| **I32_MAX** | I32  | 2147483647 (0x7FFFFFFF)                   |
| **I64_MAX** | I64  | 9223372036854775807 (0x7FFFFFFFFFFFFFFF)  |
| **F32_MIN** | F32  | -3.402823466e+38F                         |
| **F32_MAX** | F32  | 3.402823466e+38F                          |
| **F64_MIN** | F64  | -1.7976931348623158e+308                  |
| **F64_MAX** | F64  | 1.7976931348623158e+308                   |

## Nytodecimal

Nytodecimal is a base64-like encoding that is easy to decode/encode and quite compact. There are other more efficient encodings but they are very expensive to decode. In Nyto it stores [0-9A-Za-z_$] as 0-63. Nyto can encode an octal pair as 1 character, 6 bits or 1.5 hex chars.

## C8 (char, types/types.h)

- **EStringCase**: Sensitive, Insensitive: If a string or char operation should care about casing or not.
- **EStringTransform**: None, Lower, Upper: What transform to apply to a string of char.

C8 has some useful helper functions:

- C8 **C8_toLower**(C8): Transforms char to lowercase.
- C8 **C8_toUpper**(C8): Transforms char to uppercase.
- C8 **C8_transform**(C8, EStringTransform): Transforms the character (upper, lower or no modification).
- Bool **C8_isBin**(C8): Checks if the char is binary (0-1).
- Bool **C8_isOct**(C8): Checks if the char is octal (0-7).
- Bool **C8_isDec**(C8): Checks if the char is decimal (0-9).
- Bool **C8_isHex**(C8): Checks if the char is hex (0-9A-Fa-f).
- Bool **C8_isNyto**(C8): Checks if the char is nytodecimal (0-9A-Za-z_$).
- Bool **C8_isAlpha**(C8): Checks if the char is alpha (A-Za-z).
- Bool **C8_isAlphaNumeric**(C8): Checks if the char is alphanumeric (0-9A-Za-z).
- Bool **C8_isUpperCase**(C8): Checks if the char is uppercase (A-Z).
- Bool **C8_isLowerCase**(C8): Checks if the char is lowercase (a-z).
- Bool **C8_isUpperCaseHex**(C8): Checks if the char is uppercase hex (A-F).
- Bool **C8_isLowerCaseHex**(C8): Checks if the char is lowercase hex (a-f).
- Bool **C8_isWhitespace**(C8): Checks if the char is whitespace (space, tab, CR, LF).
- Bool **C8_isNewLine**(C8): Checks if the char is a newline (CR, LF).
- Bool **C8_isValidAscii**(C8): Checks if the char is valid ascii (>= 0x20 && < 0x7F).
- Bool **C8_isValidFileName**(C8): Checks if the char is valid for use in a file name (valid ascii and not: <>:"|?*/\\).
- U8 **C8_bin**(C8): Converts the char to binary (0-1) or returns U8_MAX if invalid.
- U8 **C8_oct**(C8): Converts the char to octal (0-7) or returns U8_MAX if invalid.
- U8 **C8_dec**(C8): Converts the char to decimal (0-9) or returns U8_MAX if invalid.
- U8 **C8_hex**(C8): Converts the char to hex (0-15) or returns U8_MAX if invalid.
- U8 **C8_nyto**(C8): Converts the char to nytodecimal (0-63) or returns U8_MAX if invalid.
- C8 **C8_createBin**(U8): Converts the binary (0-1) to a char or returns C8_MAX if invalid.
- C8 **C8_createOct**(U8): Converts the octal (0-7) to a char or returns C8_MAX if invalid.
- C8 **C8_createDec**(U8): Converts the decimal (0-9) to a char or returns C8_MAX if invalid.
- C8 **C8_createHex**(U8): Converts the hex (0-15) to a char or returns C8_MAX if invalid.
- C8 **C8_createNyto**(U8): Converts the nytodecimal (0-63) to a char or returns C8_MAX if invalid.

## TODO: BigInt and U128 (types/big_int.h)

## TODO: CharString (types/string.h)

## Float casts (types/flp.h)

EFloatType defines the way the bits are laid out of a float. Currently the following types are supported:

- **EFloatType_F8** (See basic data types)
- **EFloatType_F16** (See basic data types)
- **EFloatType_F32** (See basic data types)
- **EFloatType_F64** (See basic data types)
- **EFloatType_BF16** (See basic data types)
- **EFloatType_TF19** (See basic data types)
- **EFloatType_PXR24** (See basic data types)
- **EFloatType_FP24** (See basic data types)

On these enums the following functions can be used to extract the information about the float data types:

- U8 **EFloatType_bytes**(EFloatType type): The minimal data type size that can represent it (U8, U16, U32, U64).
- U8 **EFloatType_exponentBits**(EFloatType type): How many bits are used for the exponent.
- U8 **EFloatType_mantissaBits**(EFloatType type): How many bits are used for the mantissa.
- U64 **EFloatType_signShift**(EFloatType type): How many bits to shift to get to the sign bit.
- U64 **EFloatType_exponentShift**(EFloatType type): How many bits to shift to get to the exponent.
- U64 **EFloatType_mantissaShift**(EFloatType type): How many bits to shift to get to the mantissa.
- U64 **EFloatType_signMask**(EFloatType type): Sign bit active to be able to mask it.
- U64 **EFloatType_exponentMask**(EFloatType type): Exponent mask if it's already shifted.
- U64 **EFloatType_mantissaMask**(EFloatType type): Mantissa mask if it's already shifted.

The following functions can be used to parse the float stored as a U64 (raw bits). Only the bytes are used that are specified by the EFloatType.

- Bool **EFloatType_sign**(EFloatType type, U64 v): If the sign bit is active.
- U64 **EFloatType_abs**(EFloatType type, U64 v): Unset sign bit (return positive value always).
- U64 **EFloatType_negate**(EFloatType type, U64 v): Set sign bit (return negative value always).
- U64 **EFloatType_exponent**(EFloatType type, U64 v): Get the exponent bits.
- U64 **EFloatType_mantissa**(EFloatType type, U64 v): Get the mantissa bits.
- U64 **EFloatType_isFinite**(EFloatType type, U64 v): Check if the flp is finite (not NaN or Inf, exponent isn't all 1s, ignoring sign).
- U64 **EFloatType_isDeN**(EFloatType type, U64 v): Check if the flp is denormalized (exponent is 0, ignoring sign).
- U64 **EFloatType_isNaN**(EFloatType type, U64 v): Check if the flp is Not a Number (NaN, exponent is all 1s but one of the mantissa bits is set, ignoring sign).
- U64 **EFloatType_isInf**(EFloatType type, U64 v): Check if the flp is Infinite (Inf, exponent is all 1s and the mantissa is all 0s, ignoring sign).
- U64 **EFloatType_isZero**(EFloatType type, U64 v): If mantissa and exponent are 0 (ignoring sign).
- U64 **EFloatType_convert**(EFloatType type, U64 v, EFloatType conversionType): Convert two floating point types to eachother. If this is hardware supported it will accelerate it (e.g. F32->F64, F64->F32) as long as *_FORCE_FLOAT_FALLBACK* is not turned on.

The convert can be accessed through a more straightforward way by using the specialized functions:

- `ResultType <StartType>_cast<ResultType>(StartType v)` e.g. `F32 F16_castF32(F16 v)` and `F16 F32_castF16(F32 v)`.

Note: For more info check the detailed [IEEE754 Floating point format doc](IEEE754 Floating point format.md).

## type_id.h

Type id is a standard that is used to specify what object type something is. It could be stored either with a serialized object or something like a RefPtr.

It consists of the following properties:

- U13 **libraryId**; representing the library it came from.
  - The top bit represents if it's seen as a standard library. As such, it's reserved for OxC3 libraries. OxC3 libraries generally start with 0x1C30 and go up from there. It is possible that one day this will wrap around back to 0x1000.
- U10 **typeId**; representing the unique id of the object.
- U2 **width**; 1-4: represents the width of a matrix or vector.
- U2 **height**; 1-4: represents the height of a matrix.
- U2 **dataTypeStride**; [ 8, 16, 32, 64 ] bit stride
- U3 **dataType**; UInt (0), Int (1), Unused (2), Float (3), Bool (4), Object (5), Char (6), Unused (7).

If all bits are on, it represents undefined.

### Base types and ids

- C8 (0), Bool (1)
- Signed ints: [ I8, I16, I32, I64 ]: [ 2, 3, 4, 5 ]
- Unsigned ints: [ U8, U16, U32, U64 ]: [ 6, 7, 8, 9 ]
- Floats: [ F16, F32, F64 ]: [ 10, 11, 12 ]
- Signed int vectors: `I<8/16/32/64>x<2/3/4>`: [ 13 -> 25 >
- Unsigned int vectors: `U<8/16/32/64>x<2/3/4>`: [ 25 -> 37 >
- Float vectors: `F<16/32/64>x<2/3/4>`: [ 37 -> 46 >
- Signed int matrices: `I<8/16/32/64>x<2/3/4>x<2/3/4>`: [ 46, 82 >
- Unsigned int matrices: `U<8/16/32/64>x<2/3/4>x<2/3/4>`: [ 82, 118 >
- Float matrices: `F<16/32/64>x<2/3/4>x<2/3/4>`: [ 118, 145 >

Other types can be made but are external or are part of a different standard library.

## TODO: math.h

## TODO: pack.h

## TODO: type_cast.h

## TODO: Vectors (types/vec.h)

## Error (types/error.h)

Error is a struct generally passed between OxC3 functions that can error for any reason (such as; incorrect inputs or memory allocation issues). Information such as the stacktrace and error string when the error occurred can be obtained from it. It contains the following info:

- EGenericError **genericError**: non zero if the function threw an error, specifies type of error.
- const C8* **errorStr**: hardcoded human readable string, for ease of searching through the code and tracking down the issue.
- void* **stackTrace**[N]: a stacktrace of max ERROR_STACKTRACE length; only 1 level for release mode but up to _STACKTRACE_SIZE (32) for debug.
- U64 **paramValue0**: if genericError allows it, specifies the first value the error was caught on (for example out of bounds id).
- U64 **paramValue1**: if genericError allows it, specifies the second value the error was caught on (for example max array length if out of bounds is thrown).
- U32 **paramId**: if applicable, can hint which input parameter caused the issue.
- U32 **errorSubId**: can be handy as an extra identifier besides the errorStr to identify if two identical errorStrs are used.

The errors can be returned by using `Error_<genericError>` such as `Error_outOfBounds` with the dedicated constructor.

### GenericError

The following generic errors are defined:

```c
none
outOfMemory
outOfBounds
nullPointer
unauthorized
notFound
divideByZero
overflow
underflow
NaN
invalidEnum
invalidParameter
invalidOperation
invalidCast
invalidState
rateLimit
loopLimit
alreadyDefined
unsupportedOperation
timedOut
constData
platformError
unimplemented
stderr
```

## TODO: Quaternion (types/quat.h)

## TODO: Buffer (types/buffer.h)

- Buffer_length
- Buffer_isRef
- Buffer_isConstRef
- Buffer_createManagedPtr
- Buffer_createRefFromBuffer
-

## TODO: GenericList and TList (types/list.h)

## Allocator (types/allocator.h)

An allocator is a struct that contains the following:

- `void *ptr`: Opaque object pointer that can represent any object that's related to the allocator.
- AllocFunc: `Error alloc(T *ptr, U64 length, Buffer *output);` where T can be the opaque object type if the function is properly cast.
  - Importantly: Validate if ptr is what you expected (if it's not ignored), ensure length can be allocated and that Buffer doesn't already contain data.
- FreeFunc: `Bool free(T *ptr, Buffer buf)` where T can be the opaque object type if the function is properly cast.
  - Importantly: Validate if ptr is as expected (if it's not ignored), ensure the length and position of buf is valid before freeing.

## TODO: AllocationBuffer (types/allocation_buffer.h)

## Archive (types/archive.h)

An archive is essentially a flat list of all entries in something like a zip archive (or oiCA file). It can be manipulated through the functions to add files, update data, move files, etc.

An archive is just a list of ArchiveEntries, which consist of the following:

- CharString **path**; fully qualified path that leads to the archive entry (OxC3 fully qualified path). A resolved path has to consist of max 128 characters.
- EFileType **type**; the type of file the entry represents: Folder, File.
- Buffer **data**; if type is file, the data is assumed to be owned by it. It is of course possible to supply external data here as well as long as it's a ref (so it won't be freed).
- Ns **timestamp**; if type is file, it's the OxC3 timestamp since last modification. It is possible that a filesystem doesn't provide this info, in which case it will contain 0 (Unix epoch).

An archive can be created and destroyed through the following functions:

- Error **Archive_create**(Allocator alloc, Archive *result): create an empty archive and reserve some space for future entries.
- Bool **Archive_free**(Archive *archive, Allocator alloc): free the archive and all data contained in it.

It can be modified through the following:

- Add a file or directory; these operations will attempt to create parent folders until it can add the file itself. If this is not possible, it will fail.
  - Error **Archive_addDirectory**(Archive *archive, CharString path, Allocator alloc)
  - Error **Archive_addFile**(Archive *archive, CharString path, Buffer data, Ns time, Allocator alloc)
- Remove a file or directory; these operations will also delete any children if applicable.
  - Error **Archive_removeFile**(Archive *archive, CharString path, Allocator alloc): removes an entry if it's a file, if it's a folder it will error.
  - Error **Archive_removeFolder**(Archive *archive, CharString path, Allocator alloc): removes an entry if it's a folder, if it's a file it will error.
  - Error **Archive_remove**(Archive *archive, CharString path, Allocator alloc): removes an entry regardless of if it's a file or folder.
- Error **Archive_rename**(Archive *archive, CharString loc, CharString newFileName, Allocator alloc): rename a file entry while keeping the same parent folder. This operation is faster than a move because it won't require adding a parent or re-parenting.
- Error **Archive_move**(Archive *archive, CharString loc, CharString directoryName, Allocator alloc): move a file from its parent to another folder.
- Error **Archive_updateFileData**(Archive *archive, CharString path, Buffer data, Allocator alloc): overwrites the previous data at the file entry with new data.

It can be queried through the following:

- Bool **Archive_hasFile**(Archive archive, CharString path, Allocator alloc): check if an entry is present and is a file rather than a folder.
- Bool **Archive_hasFolder**(Archive archive, CharString path, Allocator alloc): check if an entry is present and is a folder rather than a file.
- Bool **Archive_has**(Archive archive, CharString path, Allocator alloc): check if an entry is present regardless of whether it is a folder or a file.
- Get data of a file. The const version should be used if no write access is needed. This is only important if an external const source (const ref) is referenced.
  - Error **Archive_getFileData**(Archive archive, CharString path, Buffer *data, Allocator alloc)
  - Error **Archive_getFileDataConst**(Archive archive, CharString path, Buffer *data, Allocator alloc)
- U64 **Archive_getIndex**(Archive archive, CharString path, Allocator alloc): get index of the entry in the archive. Returns U64_MAX if not found.
- Error **Archive_getInfo**(Archive archive, CharString loc, FileInfo *info, Allocator alloc): grabs the properties of the entry as a FileInfo struct. This FileInfo struct owns the resolved path, so it has to be freed with FileInfo_free.
- Query file, folder or file entry count:
  - Error **Archive_queryFileCount**(Archive archive, CharString loc, Bool isRecursive, U64 *res,  Allocator alloc)
  - Error **Archive_queryFolderCount**(Archive archive, CharString loc, Bool isRecursive, U64 *res,  Allocator alloc)
  - Error **Archive_queryFileEntryCount**(Archive archive, CharString loc, Bool isRecursive, U64 *res,  Allocator alloc)
- Error **Archive_foreach**(Archive archive, CharString loc, FileCallback callback, void *userData, Bool isRecursive, EFileType type, Allocator alloc): loop over the children of the entry and the entry itself. FileCallback will be called when something is encountered, it can return Error if it should stop searching and takes FileInfo and a void *userData.

## TODO: BufferLayout (types/buffer_layout.h)

## CDFList (types/cdf_list.h)

A CDFList is a list of entries that have a probability and combined they can form a list from which a random element can be selected. Some examples can be a list of random loot drops, spawn rates or chances of picking a pixel (for example raytracing skybox importance sampling).

A CDFList consists of the following:

- ListCDFValue **cdf**: a List of F32s that indicate the probability of the current element and the sum of the elements that came before. This allows a binary search to be performed more easily.
- GenericList **elements**: The elements the CDF represents and the data it should contain.
- ECDFListFlags **flags**: None, IsFinalized: If IsFinalized is true, the cdf is valid, otherwise CDFList_finalize has to be called first.
- F32 **total**: Represents the sum of all probabilities (only useful if IsFinalized is true).
- U64 **totalElements**: How many elements are inserted into the cdf.

A CDF can be created through the following functions:

- Error **CDFList_create**(U64 maxElements, Bool isReserved, U64 elementSize, Allocator allocator, CDFList *result): Creates N entries in a new CDFList or reserves N entries instead if isReserved is true. elementSize is the size of the data that backs the CDFList.
- Error **CDFList_createSubset**(GenericList preallocated, U64 elementOffset, U64 elementCount, Allocator allocator, CDFList *result): Creates N entries in a new CDFList but takes the backing memory as a ref from the preallocated list. This is useful for a 2D CDF for example, because it doesn't need to allocate any more.

When the CDF is created, it can be updated through the following functions:

- On an existing element:
  - Bool **CDFList_setProbability**(CDFList *list, U64 i, F32 value, F32 *oldValue): Sets the probability of the element at i to the value. If oldValue is not NULL, it will be filled with the old value that was replaced.
  - Bool **CDFList_setElement**(CDFList *list, U64 i, Buffer element): Set the value of the element at i.
  - Bool **CDFList_set**(CDFList *list, U64 i, F32 value, Buffer element): Set the value and chance of element at i.
- On a new element (if the CDFList wasn't created through CDFList_createSubset):
  - Error **CDFList_pushBack**(CDFList *list, F32 value, Buffer element, Allocator allocator): Append the value and element to the cdf list.
  - Error **CDFList_pushFront**(CDFList *list, F32 value, Buffer element, Allocator allocator): Prepend the value and element to the cdf list.
  - Error **CDFList_pushIndex**(CDFList *list, U64 i, F32 value, Buffer element, Allocator allocator): Insert the value and element into the cdf list.
  - Error **CDFList_popFront**(CDFList *list, Buffer elementValue): Remove the first element and if elementValue isn't null store the popped element in elementValue.
  - Error **CDFList_popBack**(CDFList *list, Buffer elementValue): Remove the last element and if elementValue isn't null store the popped element in elementValue.
  - Error **CDFList_popIndex**(CDFList *list, U64 i, Buffer elementValue): Remove an element and if elementValue isn't null store the popped element in elementValue.
- These all return true if successful or Error_none() if it returns an Error.

If anything about the CDF is changed, it will be dirty and can't be sampled. So finalize it through the following function if it's really done (this process can be very expensive, depending on elementCount):

- Error **CDFList_finalize**(CDFList *list).

After the CDFList is finalized, it can be used in one of the following functions:

- Error **CDFList_getElementAtOffset**(CDFList *list, F32 offset, CDFListElement *elementValue): Get the element at the offset, where offset is [0,total>. It finds it through a binary search. CDFListElement is [ Buffer value, U64 id, F32 chance ].
- Error **CDFList_getRandomElementFast**(CDFList *list, CDFListElement *elementValue, U32 *seed): Sample a random element from the list using a seed. (This is not cryptographically secure).
- Error **CDFList_getRandomElementSecure**(CDFList *list, CDFListElement *elementValue): Sample a cryptographically secure random element from the list. This isn't fast, so it should only be used when necessary.

## TODO: Basic file util (types/file.h)

## Random (types/rand.h)

Random only provides some basic PRNG for picking a number between 0-1. This is only useful for some basic operations and shouldn't be used for critical applications such as encryption. For CSPRNG (Cryptographically secure PRNG) please use Buffer_csprng instead.

- U32 **Random_seed**(U32 val0, U32 val1): Make a seed from a pair of 32-bit unsigned ints. A good example could be raytracing and needing random per pixel, then you could pass y * w + x as val0 and the frameId as val1.
- The following sample from the seed a couple times to produce a random number [0 - 1>:
  - F32 **Random_sample**(U32 *seed)
  - F32x2 **Random_sample2**(U32 *seed)
  - F32x4 **Random_sample4**(U32 *seed)

## Time (types/time.h)

"OxC3 time" is defined as "Ns" (or U64) as in nanoseconds since unix epoch in UTC.

Has the following functionality:

- Ns **Time_now**(): Returns the time in OxC3 time.
- DNs **Time_elapsed**(Ns timepoint): Returns elapsed time since last timepoint (OxC3 time) in DNs (delta nanoseconds).
- Ns **Time_date**(U16 y, U8 M, U8 D, U8 h, U8 m, U8 s, U32 ns, Bool isLocalTime): Returns OxC3 time from a human-readable timestamp. Returns U64_MAX on error: year should be >1970, month should be [1,12], day should be [0,31] and a valid day in the month, hour should be [0,23], minute and second should be [0,59] and nanoseconds should be < 1e9. If isLocalTime is on, the timestamp will be converted from local timezone to OxC3 time.
- Bool **Time_getDate**(Ns timestamp, U16 *Y, U8 *M, U8 *D, U8 *h, U8 *m, U8 *s, U32 *ns, Bool isLocalTIme): Same thing as Time_date but in reverse (parses human readable date to an OxC3 timestamp). Returns false if the time couldn't be converted.
- DNs **Time_dns**(Ns a, Ns b): Gets the absolute difference between two timepoints (unsigned) and clamps to I64_MAX if the times are too far apart.
- F64 **Time_dt**(Ns a, Ns b): Gets the time in seconds as double between two timepoints.
- U64 **Time_clocks**(): The current number of hardware cycles. This can be used to estimate hardware cycles between two points. Hardware cycles should only be used if there's no other alternative, since OxC3 time is a much more accurate estimation. Conversion from hardware cycles to time can depend on clock speed and other variables such as throttling or memory speeds. It may however sometimes be a useful indicator.
- I64 **Time_clocksElapsed**(U64 prevClocks): Basically Time_dns except it uses hardware cycles instead (see Time_clocks()).
- TimeFormat is a ShortString (C8[32]) that can be used in the following functions:
  - void **Time_format**(Ns time, TimeFormat time, Bool isLocalTime): Formats an OxC3 timestamp into a human readable format and stringifies it (Time_getDate -> C8[32]) as ISO 8601 (0000-00-00T00:00:00.000000000Z).
  - Bool **Time_parseFormat**(Ns *time, TimeFormat time, Bool isLocalTime): Parses an ISO 8601 date as an OxC3 timestamp (C8[32] -> Time_date). isLocalTime allows local timezone conversion.
  - Parsing and formatting to and from local formats are unsupported. ISO 8601 (with a Z as the timezone rather than an offset) is currently the only allowed format.

## TODO: Transform (types/transform.h)

## TODO: Thread

## Atomic / AtomicI64

AtomicI64 is a 64-bit int that can be accessed simultaneously from different threads. 

- Returns previous result before applying the operation:
  - I64 **AtomicI64_xor**(AtomicI64 *ptr, I64 value): Bitwise XOR (^).
  - I64 **AtomicI64_and**(AtomicI64 *ptr, I64 value): Bitwise AND (&).
  - I64 **AtomicI64_or**(AtomicI64 *ptr, I64 value): Bitwise OR (|).
  - I64 **AtomicI64_load**(AtomicI64 *ptr): Read atomic value (Be careful: Atomic might be changed right after loading this if other atomics are used on it).
  - I64 **AtomicI64_store**(AtomicI64 *ptr, I64 value): Store value in atomic.
  - I64 **AtomicI64_cmpStore**(AtomicI64 *ptr, I64 compare, I64 value): Store value in atomic if it's equal to compare. Always returns previous value.
  - I64 **AtomicI64_add**(AtomicI64 *ptr, I64 value): Add value to atomic.
  - I64 **AtomicI64_sub**(AtomicI64 *ptr, I64 value): Subtract value from atomic.
- Return current result after applying the operation:
  - I64 **AtomicI64_dec**(AtomicI64 *ptr): Decreasing the atomic by 1.
  - I64 **AtomicI64_inc**(AtomicI64 *ptr): Increasing the atomic by 1.

## Lock

Lock is a system of safely handling standard data types such as Lists/Maps from multiple threads. Before reading the lock has to be acquired, and after the lock has to be released. A Lock is essentially just an atomic that uses the thread id to acquire it.

```c
//Global scope (pseudocode)
Lock testLock = Lock_create();
ListU8 myList = (ListU8) { 0 };

//Threading function
Error myThread(U8 i) {
    
    //Acquire lock; can return:
    //Invalid (Lock uninitialized), 
    //TimedOut (Time limit exceeded), 
    //Acquired (This thread now owns the lock)
    //AlreadyLocked (Thread already owns lock, safe to continue)
    ELockAcquire acq = Lock_lock(&testLock, 1 * SECOND);
    
    //Catch Invalid and TimedOut
    if(acq < ELockAcquire_Success)
        return Error_timedOut(0, "myThread() lock couldn't be acquired in 1s");
    
    //Now we can safely append to the list
    Error err = Error_none();
    _gotoIfError(clean, ListU8_pushBackx(&myList, i));
    
    //Always use _gotoIfError / clean label syntax to avoid leaking the lock.
clean:
    
    //Only acquired locks should be unlocked. 
    //It's possible the calling function already acquired it.
    if(acq == ELockAcquire_Acquired)
   		Lock_unlock(&testLock);
        
    return err;
}
```

Make sure to use locks sparingly since they are atomic operations.

## Log

The Log functions should be used to properly handle cross platform. This will forward to the debug log / console on Windows but will forward to system messages (logcat) on Android. The respective error levels should be used for their purposes (not just for their color), since the underlying platform is allowed to determine to treat them more severe (e.g. extra logging).

The following enums exist to allow logging:

- ELogLevel: Debug, Performance, Warn, Error. Each log level generally has a different output color and severity.
- ELogOptions: Timestamp, NewLine, Thread. If the underlying system doesn't already log timestamp/thread, these options will add it. NewLine will always append a new line after the message.

The following utility function exists:

- void **Log_captureStackTrace**(Allocator alloc, void **stackTrace, U64 stackSize, U8 skip): Capture a `void*[stackSize]`  where each `void*` represents an address in the stack. 'skip' indicates how many previous stacks to skip; this might be because it's called in a helper function where it wants to point higher up in the stack. 

The following functions print an output to the console (or whatever is relevant on the platform).

- void **Log_printCapturedStackTraceCustom**(Allocator, const void **stackTrace, U64 stackSize, ELogLevel lvl, ELogOptions options). Prints a previously captured `void*[stackSize]` stackTrack with the specified options and severity.
- void **Log_printCapturedStackTrace**(Allocator alloc, const StackTrace stackTrace, ELogLevel lvl, ELogOptions options): Similar to Log_printCapturedStackTraceCustom except the stackTrace always has a max length of _STACKTRACE_SIZE (32).
- void **Log_log**(Allocator alloc, ELogLevel lvl, ELogOptions options, CharString arg): Log the string.

For ease of use, these printf-like functions have been added. They use the same printf format under the hood. **This means, you should never pass user content into these functions! To avoid undefined behavior / exploits. For user content, use Log_log or use "%.*s" with (int) CharString_length(myStr); myStr.ptr**.

- void **Log_debug/Log_performance/Log_warn/Log_error**(Allocator alloc, ELogOptions options, ...): Log in printf like fashion. Example: Log_debug(alloc, ELogOptions_Default, "Hello world %"PRIu64"\n", (U64)1 << 48).
- void **Log_debugLn/Log_performanceLn/Log_warnLn/Log_errorLn**(Allocator alloc, ...): Log in printf fashion; always with ELogOptions_NewLine. Log_debugLn(alloc, "Hello world %"PRIu64, (U64)1 << 48).

## RefPtr

A RefPtr is essentially just an object that has type information, a ref counter and the object it represents directly allocated after it.

A RefPtr contains the following properties:

- refCount: AtomicI64. When it reaches 0 it will destroy the data allocated for the RefPtr as well as calling the free function. To avoid this, increase the RefPtr using RefPtr_inc before "copying" it (e.g. adding a reference in a different library or to keep it alive). When done using it, make sure to use RefPtr_dec.
- typeId: ETypeId. Can be any type defined by any library. The library has to follow the standard as specified in the type_id.h documentation.
- length: U32. Defines how long the object is in memory. If the object is bigger than this, it should dynamically allocate through the free and alloc functions. 
- alloc: Allocator. Defines how the memory is freed/allocated.
- free: ObjectFreeFunc; called before freeing the memory.

The following helper functions exist for a RefPtr:

- Bool **RefPtr_inc**(RefPtr *ptr): Increases the RefPtr, so it can be kept alive by another object or system.
- Bool **RefPtr_dec**(RefPtr **ptr): Decreases the RefPtr and sets the `RefPtr*` passed to NULL to avoid duplicate decrements. This might free the object if it's the last reference.
- **RefPtr_data**(dat, T): Get the data from the RefPtr. It assumes you already checked the typeId for the right code. It simply just changes the pointer to directly after the RefPtr.

The RefPtr can be created through the following functions:

- Error **RefPtr_create**(U32 objectLength, Allocator alloc, ObjectFreeFunc free, ETypeId type, RefPtr **result): Create the RefPtr with a struct of objectLength allocated after it. The free function will be called after it's done. 'result' needs to be NULL to ensure that no object is leaked. ETypeId and objectLength should match what the library wants the object to represent.

A WeakRefPtr is just a normal RefPtr, except it indicates to the user that it shouldn't be increased/decreased when using it. Care should be taken to get rid of the WeakRefPtr itself to ensure it doesn't dangle (without using dec or inc on it; just by NULLing the pointer itself).