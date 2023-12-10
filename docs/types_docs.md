# OxC3 types

OxC3 types contains a lot of very basic types; it's the STL of OxC3. All these types are used throughout the entirety of OxC3.

## Basic defines

- _SIMD is what type of SIMD is used 
  - *types/types.h* defines these, while _SIMD is a compile define.
  - SIMD_NONE: Indicates a fallback mode only, useful for validation, debugging and adding new platforms with a lot less effort (if they use a different SIMD setting that isn't supported yet).
  - SIMD_SSE: Indicates the usage of SSE4.2/SSE4.1/SSE2/SSE/SSE3/SSSE3, AES, PCLMULQDQ, BMI1 and RDRAND and the possible usage of SHA instructions. This allows quicker and/or safer encryption/hashing & computation.
  - (**unsupported**): SIMD_NEON: Indicates the usage of ARM NEON intrinsics.
- _ARCH is what type of architecture is used 
  - *types/types.h* defines these, while _ARCH is a compile define.
  - ARCH_NONE: Unknown architecture; likely abstracted away from assembly level.
  - ARCH_X64: X86_64 architecture.
  - ARCH_ARM.
- _PLATFORM_TYPE is what kind of platform is running 
  - *types/platform_types.h* defines these, while _PLATFORM_TYPE is a compile define.
  - PLATFORM_WINDOWS.
  - PLATFORM_LINUX.
  - PLATFORM_ANDROID.
  - PLATFORM_WEB.
  - PLATFORM_IOS.
  - PLATFORM_OSX.
- _RELAX_FLOAT: Indicates that the floating point operations aren't necessarily accurate, but are faster.
- _FORCE_FLOAT_FALLBACK: All explicit float cast operations (e.g. F32_castF64) are software simulated.
- impl indicates that one of the other source files will define this; mostly a platform or API dependent version. For example impl is regularly used to indicate a function that is implemented by the Vulkan or Windows backends rather than one that is generic and used across other platforms & APIs.
- user_impl indicates that the user is meant to define the function themselves when using the framework. This is used by OxC3 platforms to take control of the main function but provide the Program_run & Program_exit function for the user.

## Basic data types (types/types.h)

- All types use PascalCase, even ints, floats, bool, etc.
- Basic arithmetic data types are suffixed by the bits that are taken by that type.

| Type name      | Description                                                  | C type                       |
| -------------- | ------------------------------------------------------------ | ---------------------------- |
| C8             | 8-bit char                                                   | char                         |
| U8             | 8-bit unsigned int                                           | uint8_t                      |
| I8             | 8-bit signed int                                             | int8_t                       |
| U16            | 16-bit unsigned int                                          | uint16_t                     |
| I16            | 16-bit signed int                                            | int16_t                      |
| F16            | **cast only**: 16-bit IEEE754 float                          | uint16_t (manual casting)    |
| U32            | 32-bit unsigned int                                          | uint32_t                     |
| I32            | 32-bit signed int                                            | int32_t                      |
| F32            | 32-bit IEEE754 float                                         | float                        |
| U64            | 64-bit unsigned int                                          | uint64_t                     |
| I64            | 64-bit signed int                                            | int64_t                      |
| F64            | 64-bit IEEE754 float                                         | double                       |
| U128           | 128-bit unsigned int (limited functionality)                 | complex (__uint128 or I32x4) |
| BigInt         | Big unsigned int (allocated per 8 bytes)                     | U64[]                        |
| BF16           | **cast only**: BFloat (8 bit exponent, 7 bit mantissa)       | U16                          |
| TF19           | **cast only**: TensorFloat (8 bit exponent, 10 bit mantissa) | U32 (19 used)                |
| PXR24          | **cast only**: PixarFloat (8 bit exponent, 15 bit mantissa)  | U32 (24 used)                |
| FP24           | **cast only**: AMD Float24 (7 bit exponent, 16 bit mantissa) | U32 (24 used)                |
| Ns             | 64-bit timestamp (unsigned); nanoseconds                     | U64                          |
| DNs            | 64-bit timestamp (signed); delta nanoseconds                 | I64                          |
| Bool           | Boolean                                                      | bool                         |
| ECompareResult | Compare result: Lt, Eq, Gt                                   | enum (I32)                   |

## Basic constants (types/types.h)

| Name    | Type | Value                                     |
| ------- | ---- | ----------------------------------------- |
| KIBI    | U64  | 1024^1                                    |
| MIBI    | U64  | 1024^2                                    |
| GIBI    | U64  | 1024^3                                    |
| TIBI    | U64  | 1024^4                                    |
| PEBI    | U64  | 1024^5                                    |
| KILO    | U64  | 10^3                                      |
| MEGA    | U64  | 10^6                                      |
| GIGA    | U64  | 10^9                                      |
| TERA    | U64  | 10^12                                     |
| PETA    | U64  | 10^15                                     |
| MU      | Ns   | 1e3                                       |
| MS      | Ns   | 1e6                                       |
| SECOND  | Ns   | 1e9                                       |
| MIN     | Ns   | 60e9                                      |
| HOUR    | Ns   | (60 * 60)e9                               |
| DAY     | Ns   | (60 * 60 * 24)e9                          |
| WEEK    | Ns   | (60 * 60 * 24 * 7)e9                      |
| U8_MIN  | U8   | 0                                         |
| U16_MIN | U16  | 0                                         |
| U32_MIN | U32  | 0                                         |
| U64_MIN | U64  | 0                                         |
| U8_MAX  | U8   | 255 (0xFF)                                |
| U16_MAX | U16  | 65535 (0xFFFF)                            |
| U32_MAX | U32  | 4294967295 (0xFFFFFFFF)                   |
| U64_MAX | U64  | 18446744073709551615 (0xFFFFFFFFFFFFFFFF) |
| I8_MIN  | I8   | -128 (0x80)                               |
| C8_MIN  | C8   | -128 (0x80)                               |
| I16_MIN | I16  | -32768 (0x8000)                           |
| I32_MIN | I32  | -2147483648 (0x80000000)                  |
| I64_MAX | I64  | -9223372036854775808 (0x8000000000000000) |
| I8_MAX  | I8   | 127 (0x7F)                                |
| C8_MAX  | C8   | 127 (0x7F)                                |
| I16_MAX | I16  | 32767 (0x7FFF)                            |
| I32_MAX | I32  | 2147483647 (0x7FFFFFFF)                   |
| I64_MAX | I64  | 9223372036854775807 (0x7FFFFFFFFFFFFFFF)  |
| F32_MIN | F32  | -3.402823466e+38F                         |
| F32_MAX | F32  | 3.402823466e+38F                          |
| F64_MIN | F64  | -1.7976931348623158e+308                  |
| F64_MAX | F64  | 1.7976931348623158e+308                   |

## TODO: C8 (char, types/types.h)

- EStringCase
- EStringTransform

C8 has some useful helper functions:

- C8_toLower
- C8_toUpper
- C8_transform
- C8_isBin
- C8_isOct
- C8_isDec
- C8_isHex
- C8_isNyto
- C8_isAlpha
- C8_isAlphaNumeric
- C8_isUpperCase
- C8_isLowerCase
- C8_isUpperCaseHex
- C8_isLowerCaseHex
- C8_isWhitespace
- C8_isValidAscii
- C8_isValidFileName
- C8_bin
- C8_oct
- C8_dec
- C8_hex
- C8_nyto
- C8_createBin
- C8_createOct
- C8_createDec
- C8_createHex
- C8_createNyto

## TODO: CharString

## TODO: Float casts (flp)

## TODO: Vectors

## TODO: Error

## TODO: Quaternion

## TODO: Buffer

- Buffer_length
- Buffer_isRef
- Buffer_isConstRef
- Buffer_createManagedPtr
- Buffer_createRefFromBuffer

## TODO: List

## TODO: Allocator

## TODO: AllocationBuffer

## TODO: Archive

## TODO: BufferLayout

## TODO: CDFList

## TODO: Basic file util

## Random (math/rand.h)

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

## TODO: Transform

