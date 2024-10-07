# OxC3 (Oxsomi core 3 0.2)
| Platforms | x64 -> Vulkan                                                | x64 -> Native API                                            | ARM -> Vulkan | ARM -> Native API      |
| --------- | ------------------------------------------------------------ | ------------------------------------------------------------ | ------------- | ---------------------- |
| Windows   | ![example workflow](https://github.com/Oxsomi/core3/actions/workflows/windows.yml/badge.svg) | **D3D12**: ![example workflow](https://github.com/Oxsomi/core3/actions/workflows/windows_d3d12.yml/badge.svg) | **Failing**   | **D3D12**: **Failing** |
| Mac OS X  | ![example workflow](https://github.com/Oxsomi/core3/actions/workflows/osx.yml/badge.svg) | **Metal**: **TBD**                                           | **Failing**   | **Metal**: **TBD**     |
| Linux     | ![example workflow](https://github.com/Oxsomi/core3/actions/workflows/linux.yml/badge.svg) | N/A                                                          | **Failing**   | N/A                    |
| Android   | **TBD**                                                      | N/A                                                          | **TBD**       | N/A                    |
| iOS       | **TBD**                                                      | **Metal**: **TBD**                                           | **TBD**       | **Metal**: **TBD**     |

OxC3 (0xC3 or Oxsomi core 3) is the successor to O(x)somi core v2 and v1. Specifically it combines the ostlc (standard template library), owc (window core) and ogc (graphics core). Focused more on being minimal abstraction compared to the predecessors by using C17 instead of C++20. Written so it can be wrapped with other languages (bindings) or even a VM in the future. Could also provide a C++20 layer for easier usage, such as operator overloads.

- OxC3_types
  - The basic types that you might need and useful utilities.
  - Archive for managing zip-file like entries.
  - 16-bit float casts and arbitrary floating point formats.
  - 128-bit and bigger unsigned ints (U128 and BigInt).
  - AllocationBuffer for managing block allocations.
  - Buffer manipulation such as compares, copies, bit manipulation,
    - Encryption (aes256gcm), hashing (sha256, crc32c, md5), cryptographically secure random (CSPRNG).
    - Buffer layouts for manipulating buffers using struct metadata and a path.
  - GenericList, CharString, TList (Makes Lists such as ListCharString, ListU32, etc.) and CDFList.
  - Error type including stacktrace option.
  - Time utility.
  - Vectors (mathematical) such as F32x2, F32x4, I32x2, I32x4.
  - SpinLock, Atomics and Thread for multi threading purposes.
  - Log for colored and proper cross platform logging.
  - For more info check the [documentation](docs/types.md).
- OxC3_formats
  - A library for reading/writing files. Currently only for BMP, DDS and oiCA/oiDL (proprietary zip-style formats) and oiSH (wrapping compiled shaders into one for use in different graphics APIs).
  - For more info check the [documentation](docs/formats.md).
- OxC3_platforms
  - For everything that's platform dependent (excluding some exceptions for OxC3_types).
  - Helpers for default allocator to simplify OxC3_types functions that require allocators.
  - File manipulation (in working or app dir only) such as read, write, move, rename, delete, create, info, foreach, checking.
  - Virtual file system; for accessing files included into the exe, apk, etc. Which are built through CMake.
  - Input devices: multiple mice and keyboards.
  - Window for physical (OS-backed) and virtual (in memory) windows.
  - Allocator that detects memory leaks, free without alloc (or double free) and allocation stacktraces.
  - For more info check the [documentation](docs/platforms.md).
- OxC3_graphics
  - Abstraction layer possible to port to newer graphics APIs such as D3D12, Vulkan, Metal and WebGPU. Currently, only Vulkan and D3D12 are supported.
  - Ability to create both D3D12 and Vulkan context side-by-side to allow switching API at runtime and/or better support for existing applications which might determine that at runtime.
  - For more info check the [documentation](docs/graphics_api.md).
- OxC3_shader_compiler
  - Abstraction layer around DXC to make it possible to statically link, execute on other platforms and sign DXIL even on non Windows PCs. This also allows being able to find symbols in shaders, preprocess files (transform to without includes + defines) and output include info. OxC3SC currently supports DXIL and SPIRV with multi threading support. Shaders have custom annotation syntax to be able to parse entrypoints and being able to compile them in parallel.
- OxC3(CLI)
  - Command line tool that exposes useful functions from OxC3.
  - File manipulation:
    - Conversions between oiCA/oiDL and raw files (zip-like).
    - Encryption/decryption.
    - File inspection for oiCA/oiDL/oiSH files.
  - Hash tool for files and strings (supporting sha256, crc32c, md5).
  - Random key, char, data and number generator.
  - Profile tool for testing speed of float casts, csprng, crc32c, sha256, md5 and aes256 (encryption and decryption).
  - Shader preprocessing, viewing includes, viewing symbols, multi threaded compilation to DXIL/SPIRV and reflection (TBD).
  - Iterating graphics devices (Vulkan or Direct3D12).
  - For more info check the [documentation](docs/OxC3_tool.md).

One of the useful things about C is that files are incredibly easy to compile and parse compared to C++; resulting in reduced build times and allowing easy parsing for reflection or even auto generated documentation for things like types, function signatures and errors a function might return.

## Requirements

- CMake >=3.13.
- (Optional on Windows): Vulkan SDK (latest preferred, but at least 1.3.226).
- If using Vulkan SDK on OSX, make sure to set envar MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS to 1. This can be done in the ~/.bash_profile file by doing export MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS=1, also set VULKAN_SDK to the right directory there.
- (Optional): Git or any tool that can work with GitHub.
- C++ and C compiler such as MSVC, clang or g++/gcc. C++ is only used to interface with some deps not using C such as DXC.
- Conan to avoid huge build times due to DXC/Clang/LLVM/SPIRV.

## Running requirements

- Windows (full support).
- Linux / OS X (**partial** support: no virtual files, nor window support).
- A 64-bit CPU.
  - Currently only x64 (AMD64) is supported. Though ARM could be supported too, by turning off shader compilation and SIMD (**not recommended for production builds!!**). The shader compiler currently is the only thing that doesn't support ARM if SIMD is turned off.
  - Even though SSE4.2+ is recommended, this can be explicitly turned off. SSE can only be turned off if relax float is turned off; this is because normal floats (without SSE) aren't always IEEE754 compliant. SIMD option requires SSE4.2/SSE4.1/SSE2/SSE/SSE3/SSSE3, AES, PCLMULQDQ, BMI1 and RDRAND extensions.
  - Recommended CPUs are AMD Zen, Intel Rocket lake (Gen 11) and up. This is because SHA256 is natively supported on them. These CPUs are faster and more secure. Minimum requirements for SSE build is Intel Broadwell+ (Gen 6+) and AMD Zen+ (1xxx+). **The SSE-less build doesn't have any security guarantees for encryption, as these are software based instead of hardware based. Making them less secure, since no time and effort was put into preventing cache timing attacks.** SSE-less build only exists for emulation purposes or for debugging, it's also notoriously slow since it doesn't use any intrinsics (SHA, AES, CRC, SIMD, etc.). The SSE-less build is also meant for easily porting to a new system without having to support the entire SIMD there first, before finally supporting SIMD after the base has been ported.

## Installing OxC3

```bash
git clone --recurse-submodules -j8 https://github.com/Oxsomi/core3
```

## Building OxC3

The build command has the following syntax:

- `buildCmd [Release/Debug] [EnableSIMD: True/False] [EnableTests: True/False]`
  - EnableSIMD: If SIMD extensions should be used to accelerate vector operations or things like encryption/hashing/etc. Recommended to always keep this on, unless not possible. On for Windows, off on other platforms (not supported yet).
  - EnableTests: Enable the unit tests that run afterwards.
- Extra flags can be controlled via `-o flag=Bool` such as:
  - forceVulkan: If there's a native API available on the target machine, it will attempt to use that by default. If instead it should try to use Vulkan, this flag should be set. An example is on Windows you have D3D12 and/or Vulkan; D3D12 is the default, but Vulkan can be turned on like this. Off by default.
  - enableOxC3CLI: Enable the OxC3CLI project along with the OxC3 executable. On by default.
  - forceFloatFallback: Forces half -> float casts to use software rather than hardware. Off by default.
  - enableShaderCompiler: If the shader compiler should be included. This will take longer to build, but is useful for tools or applications that need realtime shader compilation. On by default.
  - cliGraphics: If the OxC3 CLI tool allows operations that require graphics (OxC3 graphics). This can be turned off to exclude shipping dlls required for OxC3 graphics or to allow running on headless systems.

### Windows

```batch
build Release True False
```

The Windows implementation supports SSE.

### Mac OS X

```c
bash build.sh Release False False
```

Currently the Mac implementation doesn't support SSE or NEON. So SIMD mode has to be forced to None. It also doesn't support anything above OxC3 platforms yet (no virtual filesystem + window management).

### Linux

```c
bash build.sh Release False
```

Currently the Linux build doesn't support SSE or NEON. So SIMD mode has to be forced to None. It also doesn't support anything above OxC3 platforms yet (no virtual filesystem + window management).

### Other platforms

Other platforms like Android and iOS are coming in the future.

## Graphics

The graphics API is built around modern APIs. So it won't be supporting OpenGL, DirectX11-, old Metal/Vulkan versions or WebGL. To keep Vulkan, Direct3D12 and Metal usable, it will keep on bumping the minimum specs every so often in a release.

For the graphics minimum spec check the [minimum spec](graphics_spec.md). When unsure if a device is capable, please run `OxC3 graphics devices` to see if your device is supported.

## Contributions

To contribute to this repository, you agree to the [contribution guidelines](FOR_CONTRIBUTORS.md). Before merging a PR as an external party, you have to sign a contributor license agreement.

## License

This repository is available under two licenses:

- LGPL3 open source license.
- Commercial license.

Any company not wanting to adhere to the LGPL3 license can contact us as contact@osomi.net.

## Deployables

For a full OxC3 build (including all projects), a build typically contains the following:

```
D3D12:
	D3D12/*.dll
	D3D12/*.pdb
	(debug only) d3d10warp.dll
	(optional) OxC3.exe
	yourExecutable.exe

Vulkan:
	(optional) OxC3.exe or OxC3
	yourExecutable(,.exe,.apk,.ipa,etc.)
	
Dynamic linking:
	Windows only:
		D3D12/*.dll
		D3D12/*.pdb
		OxC3_graphics_d3d12.dll
		(debug only) d3d10warp.dll
	OxC3_graphics_vk (Almost everywhere else, .so, .dll, etc)
	(optional) OxC3.exe or OxC3
	yourExecutable
```

To ship anything that uses OxC3_shader_compiler it doesn't require any additional binaries (DXC is linked statically). For graphics: d3d10warp.dll is optional and should only be used for testing. D3D12/*.dll is required when OxC3 graphics is used with Direct3D12 (cliGraphics=True and forceVulkan=False and on Windows).

OxC3 is optional and doesn't have to be distributed with the application, though it provides nice functionality such as shader compilation, viewing graphics device capabilities and a few others.

The "renderer" directory is present if dynamic linking is used. In this case, there may be 1 or more graphics APIs that are compatible and this can be useful for switching at runtime or using multiple backends at once. For example, some extensions might only be supported via Vulkan or DirectX on desktop and using both might be the only way to use them. Or one of the backends is more stable for your application but the other provides more features. It also allows more easily updating by simply updating a single dll (if the interface didn't change) or adding a new graphics API. Static linking provides benefits such as easier distribution and less overhead for API calls.

## Dependencies

- OxC3(CLI).
  - (optional): OxC3_shader_compiler.
  - (optional): OxC3_graphics.
  - OxC3_platforms, OxC3_formats.
    - By extension OxC3_types.
- OxC3_shader_compiler.
  - *DXC* (and by extension LLVM, clang, DirectX-Headers, SPIRV-Headers, SPIRV-Tools): Compiling HLSL to DXIL and SPIRV.
  - *SPIRV-Tools*: Stripping, optimizing and disassembling spirv binaries.
  - *SPIRV-Reflect*: Reflecting SPIRV.
  - *DirectX-Headers*: Reflecting DXIL.
  - (**TODO**): *SPIRV-Cross*: Cross compiling SPIRV to MSL (and WGSL?).
- OxC3_graphics.
  - *Vulkan*.
  - and/or *D3D12*.
    - *NVAPI*.
    - *AMD_AGS*.
    - *AgilitySDK*.
    - (Optional): *Warp*.
- OxC3_platforms and OxC3_formats.
  - OxC3_types.
  - No other dependencies, except OS.
- OxC3_types.
  - No other dependencies, except OS.