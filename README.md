# OxC3 (Oxsomi core 3 0.2)
| Platforms | Vulkan/MoltenVK support                                      | Native API support                                           |
| --------- | ------------------------------------------------------------ | ------------------------------------------------------------ |
| Windows   | **Vulkan**: ![example workflow](https://github.com/Oxsomi/core3/actions/workflows/windows.yml/badge.svg) | **D3D12**: ![example workflow](https://github.com/Oxsomi/core3/actions/workflows/windows_d3d12.yml/badge.svg) |
| Mac OS X  | **MolenVK**: **Unimplemented**                               | **Metal**: **Unimplemented**                                 |
| Linux     | **Vulkan**: **Unimplemented**                                | N/A                                                          |
| Android   | **Vulkan**: **Unimplemented**                                | N/A                                                          |
| iOS       | **MoltenVK**: **Unimplemented**                              | **Metal**: **Unimplemented**                                 |

OxC3 (0xC3 or Oxsomi core 3) is the successor to O(x)somi core v2 and v1. Specifically it combines the ostlc (standard template library), owc (window core) and ogc (graphics core). Focused more on being minimal abstraction compared to the predecessors by using C17 instead of C++20. Written so it can be wrapped with other languages (bindings) or even a VM in the future. Could also provide a C++20 layer for easier usage, such as operator overloads.

- OxC3_types
  - The basic types that you might need and useful utilities.
  - Archive for managing zip-file like entries.
  - 16-bit float casts and arbitrary floating point formats.
  - 128-bit and bigger unsigned ints (U128 and BigInt).
  - AllocationBuffer for managing block allocations.
  - Buffer manipulation such as compares, copies, bit manipulation,
    - Encryption (aes256gcm), hashing (sha256, crc32c), cryptographically secure random (CSPRNG).
    - Buffer layouts for manipulating buffers using struct metadata and a path.
  - GenericList, CharString, TList (Makes Lists such as ListCharString, ListU32, etc.) and CDFList.
  - Error type including stacktrace option.
  - Time utility.
  - Vectors (mathematical) such as F32x2, F32x4, I32x2, I32x4.
  - Lock, Atomics and Thread for multi threading purposes.
  - Log for colored and proper cross platform logging.
  - For more info check the [documentation](docs/types.md).
- OxC3_formats: deps(OxC3_types)
  - A library for reading/writing files. Currently only for BMP, DDS and oiCA/oiDL (proprietary zip-style formats).
  - For more info check the [documentation](docs/formats.md).
- OxC3_platforms: deps(OxC3_types, OxC3_formats)
  - For everything that's platform dependent (excluding some exceptions for OxC3_types).
  - Helpers for default allocator to simplify OxC3_types functions that require allocators.
  - File manipulation (in working or app dir only) such as read, write, move, rename, delete, create, info, foreach, checking.
  - Virtual file system; for accessing files included into the exe, apk, etc. Which are built through CMake.
  - Input devices: multiple mice and keyboards.
  - Window for physical (OS-backed) and virtual (in memory) windows.
  - Allocator that detects memory leaks, free without alloc (or double free) and allocation stacktraces.
  - For more info check the [documentation](docs/platforms.md).
- OxC3_graphics: deps(OxC3_platforms)
  - Abstraction layer possible to port to newer graphics APIs such as D3D12, Vulkan, Metal and WebGPU. Currently, only Vulkan and D3D12 are supported.
  - For more info check the [documentation](docs/graphics_api.md).
- OxC3: deps(OxC3_platforms)
  - Command line tool that exposes useful functions from OxC3.
  - File manipulation:
    - Conversions between oiCA/oiDL and raw files (zip-like).
    - Encryption/decryption.
    - File inspection for oiCA/oiDL/oiSH files.
  - Hash tool for files and strings (supporting sha256, crc32c).
  - Random key, char, data and number generator.
  - Profile tool for testing speed of float casts, csprng, crc32c, sha256 and aes256 (encryption and decryption).
  - Shader preprocessing and reflection (TBD).
  - Iterating graphics devices (Vulkan or DirectX12).
  - For more info check the [documentation](docs/OxC3_tool.md).

One of the useful things about C is that files are incredibly easy to compile and parse compared to C++; resulting in reduced build times and allowing easy parsing for reflection or even auto generated documentation for things like types, function signatures and errors a function might return.

## Requirements

- CMake >=3.13.
- Vulkan SDK (latest preferred, but at least 1.3.226).
- (Required on Windows for Git Bash, otherwise optional): Git or any tool that can work with GitHub.

## Running requirements

- Windows.
- A 64-bit CPU.
  - Currently only x64 (AMD64) is supported. Though ARM could be supported too, by turning off SIMD (**not recommended for production builds!!**).
  - Even though SSE4.2+ is recommended, this can be explicitly turned off. SSE can only be turned off if relax float is turned off; this is because normal floats (without SSE) aren't always IEEE754 compliant. SIMD option requires SSE4.2/SSE4.1/SSE2/SSE/SSE3/SSSE3, AES, PCLMULQDQ, BMI1 and RDRAND extensions.
  - Recommended CPUs are AMD Zen, Intel Rocket lake (Gen 11) and up. This is because SHA256 is natively supported on them. These CPUs are faster and more secure. Minimum requirements for SSE build is Intel Broadwell+ (Gen 6+) and AMD Zen+ (1xxx+). **The SSE-less build doesn't have any security guarantees for encryption, as these are software based instead of hardware based. Making them less secure, since no time and effort was put into preventing cache timing attacks.** SSE-less build only exists for emulation purposes or for debugging, it's also notoriously slow since it doesn't use any intrinsics (SHA, AES, CRC, SIMD, etc.). The SSE-less build is also meant for easily porting to a new system without having to support the entire SIMD there first, before finally supporting SIMD after the base has been ported.

## Installing OxC3

```bash
git clone --recurse-submodules -j8 https://github.com/Oxsomi/core3
```

Or just download the repo from GitHub.

## Building OxC3

### Windows

```batch
build Release On
```

This assumes you have VS2022 installed, if not, you can specify a different CMake generator by editing build.bat.

### Mac OS X

```c
build Release Off
```

Currently the Mac build doesn't support SSE or NEON. So SIMD mode has to be forced to None. It also doesn't support anything above OxC3 platforms yet.

### Other platforms

Other platforms like Linux, Android and iOS are coming in the future.

## Graphics

The graphics API is built around modern APIs. So it won't be supporting OpenGL, DirectX11-, old Metal/Vulkan versions or WebGL. To keep Vulkan, DirectX12 and Metal usable, it will keep on bumping the minimum specs every so often in a release.

For the graphics minimum spec check the [minimum spec](graphics_spec.md). When unsure if a device is capable, please run `OxC3 graphics devices` to see if your device is supported.

To use the graphics API, make sure that all entrypoints that use it link to it properly by including it: `#include "graphics/generic/application.h"`. This must be done in the real executable and not the DLL or lib/so file, or else not all graphics features might be activated (required for proper D3D12 support: Agility SDK).

## Contributions

To contribute to this repository, you agree to the [contribution guidelines](FOR_CONTRIBUTORS.md). Before merging a PR as an external party, you have to sign a contributor license agreement.

## License

This repository is available under two licenses:

- LGPL3 open source license.
- Commercial license.

Any company not wanting to adhere to the LGPL3 license can contact us as contact@osomi.net.

## Deployables

For a full OxC3 build (including all projects), a build typically contains the following on Windows (x64):

```
D3D12:
	D3D12/D3D12Core.dll
	D3D12/d3d12SDKLayers.dll
	amd_ags_x64.dll
	d3d10warp.dll
	dxcompiler.dll
	dxil.dll
	OxC3.exe
	...

Vulkan:
	dxcompiler.dll
	dxil.dll
	OxC3.exe
	...
```

To ship OxC3 or anything that uses OxC3_shader_compiler it requires only dxcompiler.dll and dxil.dll. For graphics: d3d10warp.dll is optional and should only be used for testing. The other D3D12/*.dll and amd_ags_x64.dll are required when OxC3 graphics is used with DirectX12 (or if OxC3 is used).

OxC3 is optional and doesn't have to be distributed with the application, though it provides nice functionality such as shader compilation, viewing graphics device capabilities and a few others.

