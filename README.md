# OxC3 (Oxsomi core 3)
![](https://github.com/Oxsomi/core3/workflows/C%2FC++%20CI/badge.svg)

OxC3 (0xC3 or Oxsomi core 3) is the successor to O(x)somi core v2 and v1. Specifically it combines the ostlc (standard template library), owc (window core) and ogc (graphics core) in the future. Focused more on being minimal abstraction compared to the predecessors by using C17 instead of C++20. Written so it can be wrapped with other languages (bindings) or even a VM in the future. Could also provide a C++20 layer for easier usage, such as operator overloads.

For the time being it is quite minimal, with only types, platform abstraction, and some networking. This will probably include a graphics library and might contain some networking code as well. These are split into separate dependencies, which can be removed if deemed appropriate.

- OxC3_types
  - The basic types that you might need and useful utilities.
- OxC3_formats: deps(OxC3_types)
  - A library for reading/writing files. Currently only for BMP.
- OxC3_platforms: deps(OxC3_types, OxC3_formats)
  - For everything that's platform dependent. From window management to threading. 
- Coming soon :tm: :OxC3_graphics: deps(OxC3_platforms)
  - Abstraction layer possible to port to newer graphics APIs such as D3D12, Vulkan, Metal and WebGPU. Vulkan would be the first important thing supported.

One of the useful things about C is that files are incredibly easy to compile and parse compared to C++; resulting in reduced build times and allowing easy parsing for reflection or even auto generated documentation for things like types, function signatures and errors a function might return.

## Requirements

- CMake >=3.13
- (Optional): Git or any tool that can work with GitHub.

## Running requirements

- Windows.
- A CPU.
  - Even though SSE4.2+ is recommended, this can be explicitly turned off. SSE can only be turned off if relax float is turned off; this is because normal floats (without SSE) aren't always IEEE754 compliant. SIMD option requires AES, PCLMULQDQ and RDRAND extensions too.
  - Recommended CPUs are AMD Zen, Intel Rocket lake (Gen 11) and up. This is because SHA256 is natively supported on them. These CPUs are faster and more secure. Minimum requirements for SSE build is Intel Broadwell+ (Gen 6+) and AMD Zen+ (1xxx+). The SSE-less build doesn't have any security guarantees for encryption, as these are software based instead of hardware based. Making them less secure. SSE-less build only exists for emulation purposes or for debugging, it's also notoriously slow since it doesn't use any intrinsics (SHA, AES, CRC, SIMD, etc.).

## Installing OxC3

```bash
git clone --recurse-submodules -j8 https://github.com/Oxsomi/core3
```

Or just download the repo from GitHub.

## Building OxC3

### Windows

```batch
mkdir builds && cd builds && cmake .. -G "Visual Studio 17 2022" && cmake --build . -j 8
```

If you have a higher or lower core count, you have to modify -j to fit your logical core count. This assumes you have VS2022 installed, if not, you can specify a different CMake generator.

### Other platforms

Unfortunately it has only been implemented on Windows as of yet, but will be supporting other platforms like Android in the future. 