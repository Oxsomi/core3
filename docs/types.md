# OxC3 types (id: 0x1C30)

OxC3 types is the STL of OxC3: the scalar and vector types, strings, containers, streams, errors, threading,
cryptography and the math everything else is written in.

This page is a map, not a reference: the API is the headers under `include/types/`, with C++ wrappers (`*.hpp`)
beside the C headers they wrap.

In a C++ translation unit using the wrappers, include the `.hpp` headers and access the underlying C API
through `oxc::c`.
Do not also include OxC3 `.h` headers at global scope in that translation unit, including through another header.
The wrappers include those declarations inside `oxc::c`; header guards preserve the scope of their first inclusion.
System headers are included outside that namespace by the wrappers.
C translation units and C++ translation units that use only the raw C API may include `.h` headers normally.

The C headers require C11 and the C++ wrappers require C++20. CMake targets publish these requirements together
with the header compiler options, ABI definitions and SIMD instruction requirements. Link the appropriate target
instead of copying flags into the consumer; project warnings and optimization policy remain private to OxC3.

## Conventions

- **Types are PascalCase and sized by name.** `U8` to `U64` and `I8` to `I64`, `F32` and `F64`, `C8` for a char,
  `Bool`, and `Ns` / `DNs` for a timestamp and a signed duration in nanoseconds. `U128` and `BigInt` go past 64 bits.
  `F16`, `BF16`, `TF19`, `PXR24`, `FP24` and `F8` exist for conversion only, as their bits in an unsigned integer.
- **A function is `Type_verb`**, `Buffer_createCopy`, `CharString_parseDec`, and an `x` suffix marks the form that
  uses the platform's allocator instead of taking one (see [platforms.md](platforms.md)).
- **Limits and units are named constants** (`U32_MAX`, `KIBI`, `MIBI`, `SECOND`, `MS`) in `types/base/constants.h`.
- **`impl`** marks a function a platform or graphics backend defines, rather than the generic code.
- **Build defines** in `types/base/platform_types.h` say what is being targeted: `_PLATFORM_TYPE` (Windows, Linux,
  OS X, iOS, Android, web), `_ARCH` (x64, arm64), `_SIMD` (SSE, NEON, wasm, or none, the scalar fallback) and
  `_COMPILER` (MSVC, clang-cl, clang, gcc, emscripten). Code compares against `_COMPILER` rather than the compilers'
  own predefines, which overlap: clang-cl defines `_MSC_VER` and emscripten defines `__GNUC__`.
  `_FORCE_FLOAT_FALLBACK` simulates every float conversion in software.

## Errors

A function that can fail returns `Bool` and takes `Error *e_rr` as its last parameter, which may always be NULL:
an error carries a stack trace and is too large to return by value everywhere. It records a generic kind
(`outOfBounds`, `invalidParameter`, `notFound`, ...), a message naming the function and parameter, and up to two
values it failed on.

Inside such a function `Bool s_uccess = true` and a `clean:` label carry the flow: `retError(clean, Error_x(...))`
fails with a new error and `gotoIfError3(clean, call(..., e_rr))` propagates one, both jumping to the cleanup. The
unusual names keep the macros from colliding with a caller's. [code_style.md](code_style.md) shows them in use.

## types/base

What everything else needs and nothing else provides: the scalar types and their limits, `Error`, the `Allocator`
interface, characters and read only string operations, atomics, spin locks, threads, time (clocks, dates,
formatting), endianness, integer and float math that does not need vectors, sorting, fixed point positions (a
37.4 format for world scale coordinates), and type ids.

Timed spin-lock waits use a monotonic clock, so wall-clock adjustments do not change their timeout.
`Time_format` and `Time_parseFormat` use a `Z` suffix for UTC and no suffix for local timestamps.
Local parsing uses the system timezone and daylight saving rules; a repeated local time remains ambiguous.

## types/container

- **Buffer** and **CharString**: a byte range and a string, each either owned, a mutable reference or a const
  reference, which the C++ wrappers make explicit (`oxc::Buffer`, `oxc::String`, `oxc::StringView`). Strings are
  not null terminated unless they say so. UTF-8, UTF-16 and UTF-32 convert through `string_unicode.h`.
- **Lists**: `TList(T)` declares a typed dynamic array, `list_basic_types.h` has the common ones, and a generic list
  underneath holds any stride.
- **Hash sets**: `THashSet(T)` is a list that owns a hash index over its entries: find, get or add and swap erase by
  key, iterated as the list. `THashSetImplKey` / `THashSetImplKeyField` derive the hash and compare for a U32 or U64
  key; any other key passes a `HashIndexInfo`.
- **Streams**: a readable or writable source with a block size and a relative read cost, so a consumer can decide
  whether splitting a read pays. Memory streams, file streams, encrypting streams, and read only windows onto
  another stream all share the interface; the codecs in formats read and write through it.
  Resizable owned memory streams grow capacity geometrically for writes and reservations. Their readable length
  remains the logical stream size; reserving capacity does not extend it, and moving the buffer out returns exactly
  those logical bytes. Streams over caller-owned reference buffers retain their fixed capacity.
- **RefPtr**: a reference counted handle with a type, which the graphics and file objects are.
- **JobQueue**: a pool running jobs over a fixed set of threads, each with a stable id for per thread resources.
  Jobs go into a lane (Critical, Normal, Background), and the workers can be placed by core class: reserved
  performance workers never take Background work, and efficiency workers never take Critical work and run at
  Background priority. `Thread_setPriority` and `Thread_setAffinity` are the per thread calls underneath, and
  `Platform_instance->cpuInfo.performance` and `.efficiency` say where a hybrid CPU's cores are and how many.
- **AllocationBuffer**: a ring buffer that falls back to a block allocator when it cannot allocate in order.
- **Cryptography**: AES-256-GCM, SHA-256, CRC32C and a CSPRNG, on the CPU's instructions where it has them (a
  scalar fallback where not), and routed to the host's crypto on the web, where wasm has no AES or SHA.
- **Big integers** (`BigInt`, `U128`), **logging** with stack traces, and **texture formats**: the channel layout,
  bit depth and block compression of a texel, packed into one enum with a compact id beside it, which is what
  graphics, DDS and the shader compiler agree on.

## types/math

Vectors (`F32x2`, `F32x4`, `I32x2`, `I32x4`, and wider integer ones on SSE) with one implementation per SIMD target
and a scalar one for targets without; matrices and quaternions; conversions between float formats
(`flp.h`), including the cast only ones above; checked and unchecked integer casts; a small non cryptographic PRNG;
and `pack.h`, the CPU twins of the packing functions shaders use (RGB9E5, RGB10A2, oct normals, F21 triples), kept
bit identical to `@pack.hlsli` so data packed on either side reads back the same on the other. `tonemap.h` holds the
display transforms (Reinhard, ACES, AgX, PBR Neutral) and the sRGB transfer function, with twins in `@tonemap.hlsli`.

`F32x4_saturate` clamps lanes to [0, 1], mapping NaN and negative zero to positive zero.
The RGB8 and sRGB8 packers use it before conversion. `F32x4_dot4` sums adjacent pairs on every backend,
so its rounding follows `(x + y) + (z + w)`. Integer vector modulo follows the divisor's sign;
exact multiples produce zero, including `I32_MIN` modulo -1. A zero divisor is undefined.

The CPU `F32x4x4_inverse` uses F64 intermediates and reports failure for singular matrices or unrepresentable results.
HLSL `inverseSlow` uses unchecked F32 cofactor arithmetic; its inputs, intermediate values, and inverse must be representable.
These functions share matrix conventions, with different numerical precision and failure handling.

Quaternion application uses this angle convention: for unit `q`, `applyToNormal(q, p)`
computes `conjugate(q) * p * q`. A positive 90-degree quaternion rotation around +Z maps +X to -Y, while
`F32x4x4_rotateZ(pi/2)` maps +X to +Y. `F32x4x4_rotateQuat(q)` preserves the quaternion's rotation exactly;
conjugating a unit quaternion reverses its rotation. `angleAxis` takes radians, quaternion Euler helpers use
degrees, and matrix Euler helpers use radians. The matrix conventions also apply to the F64 variants.

`QuatS16` stores three 20-bit components and reconstructs the largest quaternion component, retaining precision
near half turns. Bits 60..61 identify the omitted component, bit 62 stores its sign, and bit 63 is reserved as zero.

## types/mesh

The flat mesh every mesh reader fills and every writer takes: positions, one packed normal and a uv per vertex, and
a triangle list, plus the derived data a file leaves out (normals from the faces). A consumer building vertex
buffers and an acceleration structure never needs to know which format the bytes came from.

## types/test

The test framework every suite uses: modules, named asserts, a summary with counts, and one entry point that runs
as its own executable on desktop and inside the bundled test app on Android and the web.

## Nytodecimal

A base 64 encoding of `[0-9A-Za-z_$]` as 0 to 63, cheap to encode and decode, used where OxC3 wants a compact
printable id. It is also the character set virtual file libraries and sections are restricted to.
