#  OxC3 types (id: 0x1C30)

OxC3 types contains a lot of very basic types; it's the STL of OxC3. All these types are used throughout the entirety of OxC3.

## Basic defines

- **_SIMD** is what type of SIMD is used
  - *types/types.h* defines these, while _SIMD is a compile define.
  - **SIMD_NONE**: Indicates a fallback mode only, useful for validation, debugging and adding new platforms with a lot less effort (if they use a different SIMD setting that isn't supported yet).
  - **SIMD_SSE**: Indicates the usage of SSE4.2/SSE4.1/SSE2/SSE/SSE3/SSSE3, AES, PCLMULQDQ, BMI1 and RDRAND and the possible usage of SHA instructions. This allows quicker and/or safer encryption/hashing & computation.
  - (**unsupported**): SIMD_NEON: Indicates the usage of ARM NEON intrinsics.
- **_ARCH** is what type of architecture is used
  - *types/types.h* defines these, while _ARCH is a compile define.
  - **ARCH_NONE**: Unknown architecture; likely abstracted away from regular assembly level.
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

- Bool **C8_isValidAscii**(C8): Checks if the char is valid ascii (>= 0x20 && < 0x7F or \t,

  \n or \r).

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

## BigInt and U128 (types/big_int.h)

BigInt is an unsigned integer that can perform anywhere from 64 to 16320 bit operations. U128 is specifically optimized for 2 U64s working together as one (or anything else the hardware might support), while BigInt is several U64s. BigInt is less optimal than U128 due to having a variable width.

Both types have the following helper functions:

- **createFrom**(Hex/Dec/Oct/Bin/Nyto/String): Create a BigInt or U128 from a string.
  - BigInt: text, bitCount, (optional with x functions) allocator, BigInt*. Where bitCount is 0 to automatically determine bitCount, U16_MAX to indicate that the BigInt was already allocated with the right bitCount and anything else to force the bigInt to be allocated with the respective bitCount. text is then decoded with the respective bit encoding to use it in other BigInt operations. Since this allocates memory, it needs to free it afterwards.
  - U128: text, Error*. Where on error it returns 0. If the error pointer is given, it will also return the exact error.
- **hex**/**oct**/**bin**/**nyto**/**toString**: Convert to string with the current encoding.
  - (optional with x functions) allocator, CharString *result, Bool leadingZeros. If leadingZeros is true, it will print the whole BigInt/U128 including leading zeros (e.g. 0x0001 instead of 0x1). toString also specifies the encoding type (Hex, Bin, Oct, Nyto). Decimal is currently unsupported until the div/mod functions are available.
- Copying behavior on operations that return BigInt or U128: BigInt performs on the provided BigInt (a) and stores it in a. While U128 returns it in a temporary variable since that doesn't require an additional allocation.
- **xor**(a, b), **or**(a, b), **and**(a, b), and, **not**(a): Perform the bitwise operation on all elements.
- **lsh**(a, b), **rsh**(a, b): Shift the entire bitset to the left or right with the referenced bit count.
- **mul**(a, b), **add**(a, b), **sub**(a, b): Arithmetic operations.
- **cmp**(a, b): Returns an ECompareResult (Lt, Eq, Gt).
- **eq**(a, b), **neq**(a, b), **lt**(a, b), **leq**(a, b), **gt**(a, b), **geq**(a, b): Compare operations.
- **min**(a, b), **max**(a, b), **clamp**(a, b): Clamp operations. For BigInt, these all perform on pointers. It returns a BigInt* of the element that's relevant instead of modifying anything.
- U8 **bitScan** & Bool **isBase2**: Scan for the first available bit and either return it or use it to check if there's anything active below. This is effectively a quick log2 (which only returns the int portion).

U128 specific functions:

- **mul64**(a, b)/**add64**(a, b): Multiply or add two 64-bit uints into a 128-bit uint. This is quicker than normal mul/add, since it doesn't have to handle the upper 64 bits. And there might even be a OS or HW function that performs this quicker.
- **zero**/**one**: Function returning constants.
- **create**/**createU64x2**: Create from 16 U8s or 2 U64s.

BigInt specific functions:

- **byteCount**/**bitCount**: get size of the buffer that holds the BigInt.
- **buffer**/**bufferConst**: get the buffer that holds the BigInt.
- **trunc()**/**resize(newSize)**: Resize the BigInt to the specified size or to the first occurrence of non zero U64 (No-op if there are only 0s).
- create: BigInt might allocate memory which has to be freed later on.
  - **createNull**: BigInt with no allocation.
  - **createRef**/**createRefConst**: Turn a U64[] into a BigInt reference.
  - **createCopy**: Copy the BigInt into a new allocation.
  - **free**.

## CharString (types/string.h)

CharString itself contains three possible types of strings:

- **Managed strings**: These are allocated on the heap (free space) and freeing them will free the underlying memory. These are null terminated by default.
- **Ref strings**: These contain a reference to existing memory. As such, this is not necessarily null terminated. It can be a ref to an offset in a buffer or a different string. These strings don't allow any operations that resize the string itself. This type of string requires the lifetime to be shorter than the referenced string.
  - **Const ref strings**: The same as above, except all write operations are disallowed.

There are also two more strings that can be used when dynamic allocation is not available or would be very annoying:

- **ShortString**: String of up to 31 characters with 1 null terminator (32 C8s). When using this with CharString functions, a ref CharString should be created with the same (or shorter) lifetime.
- **LongString**: Same as above, except 63 characters (64 C8s).

### ListCharString

This file also defines **ListCharString** and **ListConstC8** (const C8*). The second requires null terminated strings and should rarely be used except to interface with other APIs that require it.

- ListCharString has **sort**(EStringCase) and **sortSensitive**()/**sortInsensitive**() to sort all of the strings.
- *Note: ListCharString doesn't do any string copies under the hood; this helps perf but is a very important consideration. When popping parameters / resizing, it needs explicit freeing or memory is leaked!*
  - Use **freeUnderlying** to free all underlying strings and **createCopyUnderlying** to copy all underlying strings from another ListCharString. *Other List functions currently don't support this functionality yet, but in the future, the 'Underlying'-suffixed functions will be used for it.*
  - **combine**`(Allocator, CharString*)`, **concat**`(C8, Allocator, CharString*)` and **concatString**`(CharString, Allocator, CharString*)` can be used to combine the ListCharString into a single CharString. For concat, the C8 or CharString is inserted in between each entry.

### TODO: Functions

A CharString itself has the following functions:

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

## math.h

Contains some constants and functions:

- Floats, ints, uints:
  - **min**(a, b), **max**(a, b), **clamp**(v, mi, ma).
- Floats only:
  - Constants: **E**, **PI**, **RAD_TO_DEG**, **DEG_TO_RAD**.
  - Returns Error when out of float:
    - **pow2**, **pow3**, **pow4**, **pow5**, **exp10**, **exp2**, **pow**, **expe**.
  - Returns the same float type:
    - **saturate**, **lerp**, **abs**, **sqrt**, **log10**, **loge**, **log2**, **asin**, **sin**, **cos**, **acos**, **tan**, **atan**, **atan2**, **round**, **ceil**, **floor**, **fract**, **sign**, **signInc**, **mod**.
  - To check if a float is valid: **isNaN**, **isInf**, **isValid** (!isNaN && !isInf).
- Ints/Uints only:
  - **pow2**, **pow3**, **pow4**, **pow5**, **exp10**, **exp2**.
- Ints only:
  - **abs**.

## pack.h

Is used to switch between packed and unpacked types:

- Quaternion:
  - QuatF32 **QuatS16_unpack**() & QuatS16 **QuatF32_pack**(). Convert to and from QuatS16 type; which is twice as efficient as F32 by packing it as a 16-bit snorm (-32768 -> 32767 = -1 -> 1).
- UInt:
  - Manipulating individual bits:
    - Bool **getBit**(T, U8 off): gets bit at off, returns false if out of bounds.
    - Bool **setBit**(T*, U8 off): sets bit[off], false if out of bounds or if NULL.
  - Converting between U21x3:
    - U64 **U64_pack21x3**(U32 x, y, z) packs the xyz bits into one U64. If one of them is out of bounds, it will return U64_MAX.
    - U32 **U64_unpack21x3**(U64, U8 off) where off is 0, 1 or 2. If offset is invalid, will return U32_MAX.
    - Bool **U64_setPacked21x3**(U64*, U8 off, U32 v) updates element v (0, 1 or 2) to v (lower 21 bits). If ptr, v or off is invalid, will return false.
  - Converting between U20x3u4:
    - Bool **U64_pack20x3u4**(U64*, U32 x, y, z, U8 w) packs the xyzw bits into one U64. If one of them is out of bounds or NULL, it will return false.
    - U32 **U64_unpack20x3u4**(U64, U8 off) where off is 0, 1, 2 or 3. If offset is invalid, will return U32_MAX.
    - Bool **U64_setPacked20x3u4**(U64*, U8 off, U32 v) where off is 0, 1, 2 or 3. If offset or v are out of bounds or ptr is NULL, will return false.

## type_cast.h

Helpers for casting while avoiding overflow/underflow or generating NaNs:

- **fromUInt**/**fromInt**/**fromFloat**/**fromDouble**.
  - Safely cast from U64, I64, F32 or F64 to the type. This function is only available if the type is different (so F64->F64 has no function).
- Swapping endianness (uint/int types that are 16 bit or higher only):
  - **swapEndianness**(): Swaps the bytes to flip from the current endianness to the desired one. Can be used to read big endian as little endian or vice versa.
- Casting between raw float bits and int bits:
  - U64 **U64_fromF32Bits**(F32): Casts raw F32 bits to U64.
  - U64 **U64_fromF64Bits(**F64): Casts raw F64 bits to U64.
  - Error **F32_fromBits**(U64, F32*): Casts raw U64 bits to F32, returns Error if it created NaN or Inf.
  - Error **F64_fromBits**(U64, F64*): Casts raw U64 bits to F64, returns Error if it created NaN or Inf.

## Vectors (types/vec.h)

There are currently 4 vector types supported:

- I32x2 (ivec2 / int2).
- F32x2 (vec2 / float2).
- I32x4 (ivec4 / int4).
- F32x4 (vec4 / float4).
- All with either 2 or 4 elements and aligned to 8 and 16 byte boundaries respectively. Reading from misaligned boundaries is unsupported (though it could be done explicitly via load1/load2/etc.).

The operations supported are as follows:

- All vectors:

  - **zero**(), **one**(), **two**(), **negTwo**(), **negOne**(): Constants.
  - **add**(b), **sub**(b), **mul**(b), **div**(b): +-*/ respectively.
  - **eq**(b), **neq**(b), **lt**(b), **gt**(b), **geq**(b), **leq**(b): Element-wise ==, !=, <, >, >=, <=.
  - **all**(), **any**(): Check if all or any are non zero.
  - **complement**(): 1 - self.
  - **negate**(): -self.
  - **pow2**(): self * self.
  - **reduce**(): Sum of all components.
  - **sign**(): Elementwise -1 if <0, otherwise 1.
  - **abs**(): Absolute value (<0 ? -self : self).
  - **mod**(d): self % d.
  - **min**(b), **max**(b), **clamp**(mi, ma): Ensure self is in bounds. Min picks the smallest between self and b, max picks the biggest and clamp does both.
  - **eqN**(b), **neqN**(b): All components are equal or not equal. Where N is 2 or 4 depending on vector type.
  - **xx2**(T x) or **xxxx4**(T x): Creates a vector with x as all components. Depending on if it's a 2D or 4D vector.
  - **createN**(...): Create a vector with N elements set to the values passed. All other elements are 0 (if present). For 4-element vectors that would be create1-4 while 2-element vectors only have 1-2. E.g. create3(x, y, z).
  - **loadN**(...): createN but operating on memory. This is not the same as force casting the address, as alignment requirements might not be satisfied (though this one can be misaligned).
  - *Swizzles*; foreach element: letter[i]. Example: **xyzw**() for 4 element vectors and **xy**() for 2 element vectors. **wzyx**() would reverse the vector's elements (x = w, y = z, z = y, w = x).
  - *Element access*: letter[i], **set**(i, t) to set element at i or **get**(i) to get the element at i. For example: **x**() to get x or get(0) to get 0 (x).
  - *Casting*: Same element type are always castable (e.g. I32x2 to I32x4 and vice versa) and same vector size are always castable (e.g. I32x2 to F32x2). However, F32x4 to I32x2 for example would require two casts (F32x4 -> I32x4 -> I32x2). Casting a smaller vector size to bigger (I32x2 -> I32x4) would null out all other elements. This can be done via **fromT**() e.g. **F32x4_fromI32x4(**a).
  - *Bit casting*: Different element type but same vector bit size are always bit castable. E.g. I32x2 to F32x2 (vice versa) and I32x4 to F32x4 (vice versa). Through **bitsT**(a) e.g. **F32x2_bitsI32x2**(a). This will reinterpret the raw bits from int to float or vice versa.
- 4-element vectors:
  - Swizzles with only 2 components are possible and will truncate to a vec2. Example: xy() will become a vec2 while xyzw() stays a vec4.
  - **trunc2**(), **trunc3**(): Truncate everything except the first 2 or 3 elements.
  - **create2_2**(Tx2 a, Tx2 b): Vector of a.x, a.y, b.x, b.y.
  - **create2_1_1**(Tx2 a, T b, T c): Vector of a.x, a.y, b, c.
  - **create1_2_1**(T a, Tx2 b, T c): Vector of a, b.x, b.y, c.
  - **create1_1_2**(T a, T b, Tx2 c): Vector of a, b, c.x, c.y.
  - **create2_1**(Tx2 a, T b): Vector of a.x, a.y, b.
  - **create1_2**(T a, Tx2 b): Vector of a, b.x, b.y.
- Int vectors:

  - **or**(b), **and**(b), **xor**(b): |&^ respectively.
  - **not**(): ~self.
  - **swapEndianness**(): Swap elements from endianness (e.g. big to little or little to big).
  - lsh: Left shift (<<) and rsh: Right shift (>>):
    - **(lr)sh32**(U8): Shifts N bits left or right.
- Float vectors:

  - **inverse**(): 1 / self.
  - **ceil**(), **floor**(), **round**(): Round float up, down or to closest value.
  - **pow**(e): self^e.
  - **fract**(): Fraction.
  - **acos**(), **cos**(), **asin**(v), **sin**(), **atan**(), **tan**(): Trig functions.
  -  **atan2**(x): Atan2 between self (y) and x.
  - **loge**(), **log10**(), **log2**(), **exp**(), **exp10**(), **exp2**(): Log and exponent functions.
  - **lerp**(b, perc): Lerp self to b (b when perc=1, a when perc=0).
  - **saturate**(): Clamp a between 0 and 1.
  - **sqrt**(), **rsqrt**(): Square root and 1 / squareRoot.
  - **dotN**(b), **satDotN**(b): Dot product between self and b with N elements. For 2D vectors N=2 is implied (so N is not specified) and for 4D can be 2, 3 or 4. satDot is the saturated dot product.
  - **sqLenN**(), **lenN**(), **normalizeN**(): Get the squared length, length or normalize the vector. For 2D vectors N=2 is implied (so N is not specified) and for 4D can be 2, 3 or 4.
  - **reflectN**(n): Reflect self around n. For 2D vectors N=2 is implied (so N is not specified) and for 4D can be 2, 3 or 4.
- I32x4:
  - lsh: Left shift (<<) and rsh: Right shift (>>):
    - **(lr)sh64**(U8): Reinterprets as a U64x2 and shifts N bits left or right.
    - **(lr)sh128**(U8): Reinterprets as a U128 and shifts N bits left or right.
    - **(lr)shByte**(U8): Reinterprets as U128 and shift 8 bits left or right N times.
  - **addI64x2**(b): Reinterprets as I64x2 and adds together into a I64x2.
  - **mulU32x2AsU64x2**(b): Reinterprets as U32x2 and results into a U64x2 after multiplication (keeps overflow).
  - **createFromU64x2**(U64, U64): Create I32x4 by reinterpreting two U64s.
  - **blend**(b, U8 xyzw): Select a or b depending on xyzw. If xyzw & 1, it will pick b.x for ret.x, if xyzw & 2, it will pick b.y for ret.y, etc. Otherwise it will pick self[i].
  - **combineRightShift**(b, U8 v): Combine self and b into one vector (e.g. b,a as an 8D vector), then shift with v elements to the right and truncate to a 4D vector.
    - v = 0: b, v = 4: a, v >= 7: 0.
    - v = 1: self.w, b.x, b.y, b.z.
    - v = 2: self.z, self.w, b.x, b.y.
    - etc.
  - **shuffleBytes**(I32x4 b): Reinterpret as I8x16 and shuffle bytes in a using the I8x16 selector in b. E.g. for 16: ret8[i] = b8[i] >> 7 ? 0 : self8[b8[i] & 0xF];
- F32x4:
  - **srgba8Unpack**(U32) and **rgb8Unpack**(U32): Unpack an 8-bit unorm rgb(a) into a vector. srgba8 preserves alpha and does gamma correction, while rgb8 discards alpha.
  - U32 **srgba8Pack**() and U32 **rgb8Pack**(): Pack into an 8-bit unorm rgb(a) from a vector. srgba8 preserves alpha and does gamma correction, while rgb8 discards alpha.
  - **cross3**(F32x4 b): Cross self with b.
  - **mul3x3**(F32x4 v3x3[3]): Multiply self (as F32x3) by a 3x3 matrix (orientation only).
  - **mul3x4**(F32x4 v3x4[4]): Multiply self (as F32x3) by a 3x4 matrix (orientation and translation).
- F32x2:
  - **mul2x2**(F32x2 v2x2[2]): Multiply self by a 2x2 matrix (orientation only).
  - **mul2x3**(F32x2 v2x3[3]): Multiply self by a 2x3 matrix (orientation and translation).

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

## Quaternion (types/quat.h)

A Quaternion is a vector (4) of floats (doubles also supported in the future) that represents an orientation which can easily be interpolated. It has the following functions:

- **create**(x, y, z, w) and **identity**() create a quaternion.
- **eq**(a, b), **neq**(a, b) compare the two quaternions (exactly).
- **x**(), **y**(), **z**(), **w**(), **get**(i) gets the element of the quaternion.
- **setX**(T), **setY**(T), **setZ**(T), **setW**(T), **set**(i, T) sets the element of the quaternion.
- **lerp**(b, T perc) lerp quaternion from current to b using perc (0-1).
- **angleAxis**(axis, angle), **fromEuler(**pitchYawRollDeg) create quaternion from angle(s).
- **toEuler**() get back euler angles from quaternion (pitchYawRollDeg).
- **mul**(b) multiply quaternion (self) by b.
- **targetDirection**(target) look at target from origin (self).
- **applyToNormal**(Tx4 normal) apply quaternion to normalized direction.
- **conj**(), **normalize**(), **inverse**() Quaternion operations to undo multiplication and create a normalized quaternion.

## Buffer (types/buffer.h)

A Buffer is a pointer and length, as well as two bits (is ref and is const). When a Buffer is marked as const, it may not be modified. When a buffer is a ref, it won't be freed. Otherwise, a buffer is managed memory and will be freed.

### BitRef

A BitRef is a U8* and (bit) offset + isConst. It provides the following functions to manipulate it:

- Only accessible if not const:
  - **set**(), **reset**(): Set or reset the Bool value (set = 1, reset = 0).
  - **setTo**(Bool): Sets Bool value.
- **get**(): Gets Bool value.

BitRef should only be used if there's no other way to manipulate the bits. BitRef is slow since it only works on one bit at a time. It has to be initialized manually through setting offset to the bit and the U8* to start + byteOffset.

### General functions

- **length**(): Gets length of buffer.
- **isRef**(): When a buffer is a ref, it may not be freed (Buffer_free already validates this).
- **isConstRef**(): Buffer data may not be modified.
- **createManagedPtr**(void*, U64): Create managed memory buffer (will be freed if Buffer_free is called).
- **createRefFromBuffer**(Buffer, Bool isConst): Make a managed memory buffer a ref or const ref instead. This is useful when a function may not modify it later on for example.
- Buffer **createNull**(): No contents buffer.
- Buffer **createRef**(void *v, U64 length): Create mutable ref to data with length.
- Buffer **createRefConst**(const void *v, U64 length): Create immutable ref to data with length.
- Allocating / freeing a buffer:
  - Bool **free**(Allocator alloc): Free buffer (No-op if on a ref).
  - Error **createCopy**(Buffer buf, Allocator alloc, Buffer *result): Create a copy of an existing buffer.
  - Error **createZeroBits**(U64 length, Allocator alloc, Buffer *result): Create N zero bits.
  - Error **createOneBits**(U64 length, Allocator alloc, Buffer *result): Create N one bits.
  - Error **createBits**(U64 length, Bool value, Allocator alloc, Buffer *result): Create N bits (initial state undefined).
  - Error **createEmptyBytes**(U64 length, Allocator alloc, Buffer *output): Create N bytes of 0.
  - Error **createUninitializedBytes**(U64 length, Allocator alloc, Buffer *result): Create N bytes (initial state undefined).
  - Error **createSubset**(Buffer buf, U64 offset, U64 length, Bool isConst, Buffer *output): Create a (const) ref to an existing buffer at offset for length bytes.
- Error **getBit**(U64 offset, Bool *output): Get the bit at offset (in bits) into output.
- Bool **copy/revCopy**(Buffer src): Copy src into self. revCopy copies backwards, which is useful if ranges overlap.
- Error **setBit/resetBit**(U64 offset): (Re)set bit at offset (in bits).
- Error **setBitTo**(U64 offset, Bool value): Set bit at offset (in bits) to value.
- Error **bitwiseOr**(Buffer b): Or (|=) self with b.
- Error **bitwiseAnd**(Buffer b): And (&=) self with b.
- Error **bitwiseXor**(Buffer b): Xor (^=) self with b.
- Error **bitwiseNot**(): ~self.
- Error **unsetBitRange/setBitRange**(U64 dstOff, U64 bits): (Un)set all bits in range.
- Error **unsetAllBits/setAllBits**(): (Un)set all bits.
- Error **setAllBitsTo**(Bool isOn): Set all bits to value.
- Bool **eq**(Buffer buf1): Compare to buffer (returns if the same).
- Bool **neq**(Buffer buf1): !eq(buf1).
- Error **offset**(U64 length): Offset current buffer with length.
- Error **append**(const void *v, U64 length): Append v[length] to the buffer and offset by length.
- Error **appendBuffer**(Buffer append): Same as append, but for Buffer.
- Error **consume**(void *v, U64 length): Consume current buffer into v[length] and offset by length.
- Error **combine**(Buffer b, Allocator alloc, Buffer *output): Combine a and b into one Buffer.
- Error **appendT**(T): Where T is a basic POD type such as I32x2, I32, Bool, etc. Same as append, but with the size of T and contents of t.
- Error **consumeT**(T*): ^ but consume.
- Unicode helpers:
  - Bool **isUnicode**(F32 threshold, Bool isUTF16): Check if the input is valid UTF16 or UTF8. Threshold of 0 means the entire file has to be valid, while 0.5 would mean 50%.
  - Bool **isUTF8**(F32 threshold): isUnicode with isUTF16 = false.
  - Bool **isUTF16**(F32 threshold): isUnicode with isUTF16 = true\.
  - Bool **isAscii**(): If the whole file is valid ASCII (C8_isValidAscii).
  - **UnicodeCodePoint** is a U32 in range [ 0, 0x10FFFF ] of unicode codepoints.
  - **UnicodeCodePointInfo** contains the UnicodeCodePoint as well as the char and byte count.
  - Error **readAsUTF8/readAsUTF16**(U64 i, UnicodeCodePointInfo *codepoint): Tries to read a UTF8 or UTF16 codepoint, returns an Error if invalid.
  - Error **writeAsUTF8/writeAsUTF16**(U64 i, UnicodeCodePoint codepoint, U8 *bytes): Write a UTF8 or UTF16 codepoint and return the byte count. Returns Error if invalid codepoint.

### Cryptography & Hashing

The following functions exist for crytography or hashing purposes:

- **crc32**(): Returns the U32 (CRC32 Castagnoli) hash of the Buffer. This is not for cryptography purposes, but only for quick validation (not really caring much about someone being able to force hash collisions).
- **sha256**(U32 output[8]): Returns a 256-bit (SHA256) hash of the Buffer. This hash type should be used for big data where it can't be easily bruteforced. As such, passwords are a very bad candidate and should use something like Argon2id instead.
- **csprng**(): Fill the buffer with bytes from a Cryptographically Secure Psuedo-Random Number Generator (CSPRNG). This is useful for things like key generation, as these need true random rather than something predictable.
- **Encryption**/**decryption**:
  - Types (EBufferEncryptionType):
    - **AES256GCM** is supported and should be the default encryption of all files. AES256 in Galois Counter Mode is the safest and fastest encryption algorithm in modern CPUs (with dedicated instructions). It can also easily be multi threaded by adding the hash of all blocks in sequence after encryption to produce the final "tag" (encryption checksum).
    - **AES128GCM** is also supported for legacy reasons **ONLY**, should only be used when 256 can't be used (e.g. old encoding scheme). This is because it's not quantum resistant (reduces it from 2^128 to 2^64 difficulty to be cracked in seconds with a 128 qubit quantum computer), while 256 has resistance.
    - **getAdditionalData**() can be used to check how much data the algorithm needs to store. In the case of AES that's 28 bytes (12 byte IV + 16 byte tag / checksum).
  - Error **encrypt**(Buffer additionalData, EBufferEncryptionType, EBufferEncryptionFlags, U32 *key, I32x4 *iv, I32x4 *tag)
    - Encrypts the current data ("plaintext") into the current buffer as cyphertext. If the current buffer is empty, it is still completely valid (for aes) to still call encrypt as a way to authenticate additionalData with the referenced key.
      - Authentication should store the key and iv until encryption is done to ensure it doesn't re-generate a mismatching one.
    - Flags: GenerateKey (0), GenerateIv (1). Will use CSPRNG to generate the key and/or IV if the flags are set. Otherwise it will use supplied keys/ivs and assumes the user has properly generated them.
      - Be careful about the following if iv and key are manually generated:
        - Don't reuse iv if supplied.
        - Don't use the key too often (e.g. use only <2^16 times for good measure).
        - Don't discard iv or key if any of them are generated.
        - Don't discard tag or cut off too many bytes.
    - Secret key is a `U32[4]` for AES128GCM and `U32[8]` for AES256GCM.
    - Secret key, iv and tag are required to be valid pointers. Key will be filled if GenerateKey is true, iv will be filled if GenerateIv is true. Tag will be filled as a checksum and as mentioned before, the tag can't be reduced too many bytes (otherwise it'll be easy to generate collisions).
    - There's a limit enforced for 4Gi - 3 AES blocks (16 bytes each or about 63 GiB). This is to ensure the IV doesn't run out of options, which would cause degradation of the encryption quality. As a fix, multiple keys can be generated for different regions of the file and decrypted separately. Very important: All different regions need their tag and iv to be authenticated one more time, otherwise a region could be replaced by an attacker.
  - Error **decrypt**(Buffer additionalData, EBufferEncryptionType, const U32 *key, I32x4 tag, I32x4 iv)
    - The iv, key and tag should be passed to match the ones from the encrypt function.
    - The key is the same size as mentioned in 'encrypt' and the best practices from it still apply too.
    - Unauthorized error is returned if the tag doesn't match the one generated from encrypt and in that case, no plaintext is generated. On error, it should not continue, as the cyphertext has been tampered with.

## TODO: GenericList and TList (types/list.h)

## Allocator (types/allocator.h)

An allocator is a struct that contains the following:

- `void *ptr`: Opaque object pointer that can represent any object that's related to the allocator.
- AllocFunc: `Error alloc(T *ptr, U64 length, Buffer *output);` where T can be the opaque object type if the function is properly cast.
  - Importantly: Validate if ptr is what you expected (if it's not ignored), ensure length can be allocated and that Buffer doesn't already contain data.
- FreeFunc: `Bool free(T *ptr, Buffer buf)` where T can be the opaque object type if the function is properly cast.
  - Importantly: Validate if ptr is as expected (if it's not ignored), ensure the length and position of buf is valid before freeing.

## AllocationBuffer (types/allocation_buffer.h)

An allocation buffer is a physical or virtual representation of allocation units and allocations in it. Generally used to allocate memory, be it GPU or CPU memory. As such, the units represented in the AllocationBuffer aren't necessarily bytes and the buffer is not necessarily mappable to CPU memory.

An AllocationBuffer is managed through the following:

- Error **create**(U64 size, Bool isVirtual, Allocator alloc, AllocationBuffer *allocationBuffer): Create an address range with the size. If isVirtual is true, the AllocationBuffer's internal buffer will be NULL but will have a size. This is to signal that the range is not directly accessible on the CPU (might represent GPU memory or something else).
- Error **createRefFromRegion**(Buffer origin, U64 offset, size, Allocator alloc, AllocationBuffer *allocationBuffer): Create a reference into pre-allocated memory.
- Bool **free**(Allocator alloc): Free the data of the buffer and allocation information.

After this AllocationBuffer is created, allocations can be managed through the following functions:

- Bool **freeBlock**(const U8 *ptr): Free the block located at ptr. Be aware: Passing NULL here is completely valid if it's a virtual address range that has an allocation there. This is because the range is then mapped to [ 0, size > since it doesn't represent any memory. For virtual ranges, this is just an offset as a pointer.
- Bool **freeAll**(): Free all blocks, but keep the memory itself alive.
- Error **allocateBlock**(U64 size, alignment, Allocator alloc, const U8 **result): Allocate a block with size and alignment into the current AllocationBuffer. result isn't touched if an error occurred, though it is explicitly set to NULL when there's no more memory in this allocation buffer (and outOfMemory error is returned). Please default to NULL if required, or if it's important to know what kind of allocation failure happened, default the pointer to for example 0x1 and detect if it was modified. If the default pointer was modified, then it indicates out of memory, otherwise a different error occurred. This can be useful to fallback on other allocation strategies (for example, allocating additional space in a separate AllocationBuffer). This function can be used for both virtual and physical memory, though for physical memory the data would be undefined (it's not zeroed), it is recommended to use allocateAndFillBlock for that use case instead.
- Error **allocateAndFillBlock**(Buffer data, U64 alignment, Allocator alloc, U8 **result): Allocate block with alignment and fill with the data provided (with that same size). This is only available for physical allocations and is the recommended function for physical memory to avoid not correctly initializing the provided data.

It has the members:

- **buffer**: Where the Buffer's pointer doesn't necessarily mean CPU visible memory. This indicates that the unit is also not defined (when NULL), it might represent bytes or something entirely different. If the pointer is not NULL, it represents CPU memory.
- **allocations**: List of AllocationBufferBlock, where each block has a U64 start, end and alignment.

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

## BufferLayout (types/buffer_layout.h)

A BufferLayout is the layout of the buffer. It doesn't necessarily have to be CPU memory, though when used with the setters, getters and resolve, it will be required. BufferLayout is the generic way of representing data types and how they're laid out, as such, at one point this layout could also represent JSON files.

Consider the following JSON file:

```json
{
    "myMember0": 1234, 		//U32
    "myMember1": 1234.0,	//F64
    "myMember2": '1',		//C8
    "myMember3": true,		//Bool
    "myMember4": {
        "a": true
    }
}
```

Would represent a tightly packed struct of U32, F64, C8, Bool and a struct with a Bool. It doesn't follow any alignment rules that normal C structs would.

Most behave similar to a C struct, except that dynamically sized arrays exist. These don't allocate any memory in the current struct, but instead have a separate allocation.

As expected, this would generate 6 members; one that's part of myMember4 and the other 5 are part of the root struct. A BufferLayout is a DAG, which means structs always have to have their members defined before they are defined and they can't nest themselves or anything defined after (no recursion).

**Note: Some JSON features aren't properly supported in the current BufferLayout. This is because the JSON format is very complex and not strongly typed, while this format is strongly typed. For example, a field could be either a Bool or a U32, which is not allowed here.**

### BufferLayoutMember

**BufferLayoutMember** contains the following info (header):

- **typeId** or **structId**. If offsetHiAndIsStruct >> 15 (**isStruct**()), this will include a struct id that represents this member (**getStructId**()). Otherwise, this member is represented with an ETypeId (**getTypeId**()), which can be matrices, vectors, floats, ints, etc. Currently, only POD type ids are supported here. It'd be invalid to use extended type ids or non pod types.
- **offset**: U64 stored in both offsetLo and offsetHiAndIsStruct, acquired through **getOffset**(). Represents the offset in the struct it's in. Since this offset is explicitly referenced, it is possible to use a union here.
- **arrayIndices**: Up to 255 dimensional arrays, though less dimensional should be used when possible due to better performance (less lookups). Mostly useful as 1D, 2D or 3D arrays. Can be 0 if it's not an array.
- **nameLength**: How long the name of the member is (up to 255).
- **stride**: Stride between array elements (up to 4GiB).

### BufferLayoutMemberInfo

That member also contains other data that is allocated with this header. When unpacked, this member expands to **BufferLayoutMemberInfo**:

- **name**: CharString for the struct member name.
- **arraySizes**: ListU32 where each entry is another dimension. If one of the U32s is 0 it will become a dynamically sized array, so memory is allocated for it dynamically.
- **typeId**: ETypeId_Undefined if it's a struct, otherwise the same as BufferLayoutMember.
- **structId**: U32_MAX if it's <u>not</u> a struct, otherwise the same as BufferLayoutMember.
- **offset, stride:** Same as BufferLayoutMember.

The reason this is not just the same as BufferLayoutMember, is because that is used for storage, while this is for ease of use.

It can be created through the following:

- **create**(ETypeId typeId, CharString name, U64 offset, U32 stride): Creates a member as a POD type (such as matrix, vector, float, int, Bool, etc.).
  - **createArray** can be used to turn it into an array. With ListU32 arraySizes passed after the name.
- **createStruct**(U32 structId, CharString name, U64 offset, U32 stride): Creates member as a struct type.
  - **createStructArray** can be used to turn it into an array. With ListU32 arraySizes passed after the name.

### BufferLayoutStruct

BufferLayoutStruct is the packed version of BufferLayoutStructInfo and it contains the actual data for the members that are stored in it. It has the following properties:

- **id**: Used to refer to this struct by other structs.
- **memberCount**: Only up to 65535 other members are allowed in a struct.
- **nameLength**: Name of the struct up to 255 characters.
- **data**: The data of the structName, BufferLayoutMember[memberCount] and then (U32 arrayIndices[member.arrayIndices], C8 name[member.nameLength])[memberCount].

The following helper functions can access the data:

- **getName**(), **getMember**(memberId), **getMemberInfo**(memberId): Get name, member and member info. The member info is a bit more complicated, so getMember should be used whenever possible.

#### BufferLayoutStructInfo

A BufferLayoutStruct can be unpacked into a BufferLayoutStructInfo. It then has a **name** and **members** (List of BufferLayoutMemberInfo).

### BufferLayoutPathInfo

BufferLayouts use paths to resolve a CharString to a Buffer. This buffer is either part of the main buffer or is an additional allocation for the dynamically allocated arrays. These paths are quite similar to file paths, except for the following:

- Most characters are allowed, unlike file paths.
- Backwards paths aren't supported. `..` is a member called `..` and `.` is a member called `.`.
- Backslash `\` isn't the same as slash `/`. Slash is a separator, backslash is an escape.
- `\/` is used to escape slashes and `\\` is used to escape backslashes.
- Paths are case sensitive.
- `a/0` could mean either a's member named 0 or the 0th index of a (e.g. no concept of a[0] vs a/0).
- Leading slashes are ignored, so `/a/0` and `a/0` are the same.
- Empty member names are invalid, so `//` is invalid.
- Array indices can be given by either:
  - Hex (0x[0-9A-Fa-f]+)
  - Octal (0o[0-7]+)
  - Binary (0b[0-1]+)
  - Decimal ([0-9]+)
  - Nytodecimal (0n[0-9A-Za-z_$]+)
  - E.g. `a/0` and `a/0x0` are the same.

This concept is resolved by BufferLayout_resolveLayout as the following parameters:

- **offset**, **length**: Information about the buffer.
- **typeId**, **structId**: Type information.
- **leftoverArray**: Information about how long the array it references is.

### BufferLayout

Is a List of BufferLayoutStruct (structs) and a U32 rootStructIndex. The root struct index points to the struct that the BufferLayout can instantiate. A BufferLayout is managed through the following:

- Error **create**(Allocator, BufferLayout*): Create an empty buffer layout.
- Bool **free**(Allocator, BufferLayout*): Free the buffer layout (structs).
- Error **createStruct**(BufferLayoutStructInfo, Allocator, U32 *id): Creates & packs a new BufferLayoutStruct and returns the id.
- Error **assignRootStruct**(U32 id): Make the specified struct the root of the buffer layout. This means that the buffer layout can be instantiated into real data.

When a BufferLayout is fully qualified (e.g. it has a root struct), it can be initiated through:

- Error **createInstance**(U64 count, Allocator, Buffer*): Allocates a buffer of the specified amount.

To resolve a path to a location, the resolve function or setters/getters can be used:

- Error **resolve**(Buffer instance, BufferLayout, CharString path, Buffer *output, Allocator): Resolve the BufferLayout instantiated as Buffer into the output.
- Error **resolveLayout**(BufferLayout layout, CharString path, BufferLayoutPathInfo *info, Allocator alloc): Resolve layout as BufferLayoutPathInfo, which already has additional info about where it is located. This should be cached when possible, to skip parsing the path.
- Error **setData**(Buffer instance, BufferLayout, CharString path, Buffer data, Allocator): Set the data to the data passed with a buffer copy.
- Error **getData**(Buffer instance, BufferLayout, CharString path, Buffer *currentData, Allocator): Get the data pointed to by path. This does not copy.
- Error **setT**(Buffer, BufferLayout, CharString path, T t, Allocator): Set the path to t, where T is one of the predefined types such as (U/I)(8/16/32/64), C8, Bool, F32, F64, (F/I)32x(2/4).
- Error **getT**(Buffer, BufferLayout, CharString path, T *t, Allocator): Copy resolved buffer location into t. See setT.

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

## Files

Basic file util (types/file.h) for resolving file paths and handling FileInfo freeing. Other more advanced file operations are handled by OxC3 platform's platforms/file.h. This only exists because formats such as oiCA and Archive use these functions.

- Error **File_resolve**(CharString loc, Bool *isVirtual, U64 maxFilePathLimit, CharString absoluteDir, Allocator alloc, CharString *result)
  - Prefix of `//` indicates a virtual file path. This is a special OxC3 layout (called OxC3 paths) that indicates special use. `//access` will index into files that were provided access to (through a file picker window for example) and `//function` represents a virtual file system that is managed by a custom manager (such as a temporarily loaded zip file in memory). `//network` is reserved to simulate Windows `\\` in the future.
  - Resolves a relative or virtual file path to a real path. Example: //myVirtualPath/test/../ resolves to //myVirtualPath. However, other absolute paths get resolved to a path relative to the absoluteDir. So for example `D:\MyFolder\test\..` with an absoluteDir of `D:` gets resolved to `MyFolder`. This is because OxC3 platform avoids access of files outside of the app/working directory (though permission could still be given to files outside of this folder through the virtual `//access` folder).
  - maxFilePathLimit is forced to 260 if not provided and on Windows this is enforced.
  - isVirtual pointer is required, even if unused and returns if the file path is a virtual file path.
  - A new CharString is allocated in result.
- Bool **File_isVirtual**(CharString): If the File is virtual (starts with //).
- Bool **FileInfo_free**(FileInfo*, Allocator): Free the FileInfo that was returned by a file function.

It also defines the following types:

- FileCallback: Returns Error and takes FileInfo, void *userData.
- FileInfo: type, access, timestamp, path and fileSize.
  - type is EFileType (Folder, File).
  - access is EFileAccess (None, Read, Write, ReadWrite).

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

## Thread

A thread can be accessed through the following functions:

- U64 **Thread_getId**(): Gets the handle of the current calling thread.
- Bool **Thread_sleep**(Ns ns): Sleeps for roughly x nanoseconds. Sometimes this unit isn't fully accurate, as it requires the minimal unit of 100ns, though it will round up to the next timepoint.
- Error **Thread_wait**(Thread *thread): Waits for the created thread from the current thread.
- Error **Thread_waitAndCleanup**(Allocator alloc, Thread **thread): Waits until the thread is done and then calls Thread_free on it.

A thread can be created/destroyed through the following functions:

- Error **Thread_create**(Allocator alloc, ThreadCallbackFunction callback, void *objectHandle, Thread **thread): Where ThreadCallbackFunction is a (void) function that takes a `void*` and the objectHandle is what is passed to that function.
- Bool **Thread_free**(Allocator alloc, Thread **thread): Free the thread and NULL the `Thread*`.

Make sure to avoid thread creation and try to use something similar to a job system. Not every thread might be created equally; some might be efficiency cores while others may be performance cores. Be sure that the thread scheduling isn't dependent on each thread to be equal in performance.

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