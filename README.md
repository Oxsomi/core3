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

- CMake >=3.13.
- (Optional): Git or any tool that can work with GitHub.

## Running requirements

- Windows.
- A CPU.
  - Even though SSE4.2+ is recommended, this can be explicitly turned off. SSE can only be turned off if relax float is turned off; this is because normal floats (without SSE) aren't always IEEE754 compliant. SIMD option requires AES, PCLMULQDQ and RDRAND extensions too.
  - Recommended CPUs are AMD Zen, Intel Rocket lake (Gen 11) and up. This is because SHA256 is natively supported on them. These CPUs are faster and more secure. Minimum requirements for SSE build is Intel Broadwell+ (Gen 6+) and AMD Zen+ (1xxx+). **The SSE-less build doesn't have any security guarantees for encryption, as these are software based instead of hardware based. Making them less secure, since no time and effort was put into preventing cache timing attacks.** SSE-less build only exists for emulation purposes or for debugging, it's also notoriously slow since it doesn't use any intrinsics (SHA, AES, CRC, SIMD, etc.). The SSE-less build is also meant for easily porting to a new system without having to support the entire SIMD there first, before finally supporting SIMD after the base has been ported.

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

## Virtual file system

OxC3 supports a virtual file system that allows baking/preprocessing of external file formats to our own, as well as embedding these files into the exe/apk directly. The executable embeds sections, which can be loaded individually and support dependencies. For example;

```
myLibrary
	shaders
	fonts
	textures
myOtherLibrary
	shaders
	fonts
	textures
```

The example above shows the sections that are supported for our example executable. To access these resources from our application we have to load either the root or the specific sections:

```c
_gotoIfError(clean, File_loadVirtual("//myLibrary/fonts", NULL));	//Load section.
_gotoIfError(clean, File_loadVirtual("//myLibrary", NULL));			//Load myLibrary.
_gotoIfError(clean, File_loadVirtual("//.", NULL));					//Load everything.
```

These files are decompressed and unencrypted (if they were) and kept in memory, so they can be quickly accessed. They can then be unloaded if they're deemed unimportant. 

The example layout above is only useful if the dependencies have very little resources. The moment you have lots of resources (or little RAM) then you probably want to split them up based on how they're accessed. For example; if assets are only used in a certain level then consider splitting up the file structure per level to ensure little RAM is wasted on it. Another example could be splitting up assets based on level environment, so if only all forest environment levels use a certain tree, then it's wasteful to load that for every level. The levels would then reference dependencies to sections and these sections would then be loaded. 

When a virtual file system is loaded, it can be accessed the same way as a normal file is loaded. The exception being that write access isn't allowed anymore. The only limitation is that files need both a library and a section folder `//myLibrary/fonts/*` rather than `//myLibrary/*` and the section has to be loaded. To ensure it is loaded, a File_loadVirtual can be called or File_isVirtualLoaded can be called. 

The same limitations as with a normal file system apply here; the file names have to be windows compatible and section/library names are even stricter (Nytodecimal only; 0-9A-Za-z$_). These are case insensitive and will likely be transformed to a different casing depending on platform. 

The only reserved library names besides the windows ones (NUL, COM, etc.) are: access, function. So `//access/...` is reserved for future access to directories and files outside of the working directory and app directory (access has to be allowed through selecting for example from the Windows file explorer). `//function/...` is reserved for future functionality to allow custom functionality to emulate for example old file formats; the fully resolved path would be passed to a user function to allow custom behavior.

### Usage in CMake

to add the files to your project, you can use the following:

```cmake

add_virtual_files(TARGET myProject NAME mySection ROOT ${CMAKE_CURRENT_SOURCE_DIR}/res/mySectionFolder)
configure_icon(myProject "${CMAKE_CURRENT_SOURCE_DIR}/res/logo.ico")
configure_virtual_files(myProject)
```

Virtual files are then linked into the project.

To add a dependency, use the following:

```cmake
add_virtual_dependencies(TARGET myProject DEPENDENCIES myDep)
```

This should be done before the configure_virtual_files and ensures the files for the dependency are present in this project. A dependency itself can't include an icon or use configure_virtual_files; as this is reserved for executables only.

*Note: Dependencies can't be overlapping. So if B and C both include A then including B and C in D won't work.*

## Contributions

To contribute to this repository, you agree to the [contribution guidelines](FOR CONTRIBUTORS.md). Before merging a PR as an external party, you have to sign a contributor license agreement.

## License

This repository is available under two licenses:

- LGPL3 open source license.
- Commercial license.

Any company not adhering to the LGPL3 license can contact us as contact@osomi.net. 