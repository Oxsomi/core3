# OxC3 (Oxsomi core 3 0.2)
![](https://github.com/Oxsomi/core3/workflows/C%2FC++%20CI/badge.svg)

OxC3 (0xC3 or Oxsomi core 3) is the successor to O(x)somi core v2 and v1. Specifically it combines the ostlc (standard template library), owc (window core) and ogc (graphics core) in the future. Focused more on being minimal abstraction compared to the predecessors by using C17 instead of C++20. Written so it can be wrapped with other languages (bindings) or even a VM in the future. Could also provide a C++20 layer for easier usage, such as operator overloads.

- OxC3_types
  - The basic types that you might need and useful utilities.
  - Archive for managing zip-file like entries.
  - 16-bit float casts and arbitrary floating point formats.
  - AllocationBuffer for managing block allocations.
  - Buffer manipulation such as compares, copies, bit manipulation, 
    - Encryption (aes256gcm), hashing (sha256, crc32c), cryptographically secure PRNG (CSPRNG).
    - Buffer layouts for manipulating buffers using struct metadata and a path.
  - List, CharString, CharStringList and CDFList.
  - Error type including stacktrace option.
  - Time utility.
  - Vectors (mathmetical) such as F32x2, F32x4, I32x2, I32x4.
- OxC3_formats: deps(OxC3_types)
  - A library for reading/writing files. Currently only for BMP and oiCA/oiDL (proprietary zip-style formats).
- OxC3_platforms: deps(OxC3_types, OxC3_formats)
  - For everything that's platform dependent (excluding some exceptions for OxC3_types).
  - Helpers for default allocator to simplify OxC3_types functions that require allocators.
  - File manipulation (in working or app dir only) such as read, write, move, rename, delete, create, info, foreach, checking.
  - Virtual file system; for accessing files included into the exe, apk, etc.
  - Input devices: multiple mice and keyboards (all accessible individually).
  - Lock and Thread for multi threading purposes.
  - Log for colored and proper cross platform logging.
  - Window for physical (OS-backed) and virtual (in memory) windows.
- Coming soon :tm: :OxC3_graphics: deps(OxC3_platforms)
  - Abstraction layer possible to port to newer graphics APIs such as D3D12, Vulkan, Metal and WebGPU. Vulkan would be the first important thing supported.
- OxC3: deps(OxC3_platforms)
  - Useful command line tool that exposes useful functions from OxC3. 
  - File manipulation:
    - Conversions between oiCA/oiDL and raw files (zip-like).
    - Encryption/decryption.
    - File inspection for oiCA/oiDL files.
- Hash tool for files and strings (supporting sha256, crc32c).
- Random key, char, data and number generator.
- Profile tool for testing speed of float casts, csprng, crc32c, sha256 and aes256 (encryption and decryption).

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
build Release On Off
```

This assumes you have VS2022 installed, if not, you can specify a different CMake generator by editing build.bat.

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
_gotoIfError(clean, File_loadVirtual("//myLibrary", NULL));		//Load myLibrary.
_gotoIfError(clean, File_loadVirtual("//.", NULL));			//Load everything.
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

## Graphics

The graphics API is built around modern APIs. So it won't be supporting OpenGL, DirectX11-, old Metal/Vulkan versions or WebGL. To keep Vulkan, DirectX12 and Metal usable, it will keep on bumping the minimum specs every so often in a release. 

### Minimum spec

OxC3 is not made for devices older than 3 years. Oxsomi core's spec for newer versions would require more recent devices to ensure developers don't keep on having to deal with the backwards compatibility for devices that aren't relevant anymore. For example; phones get replaced every 3 years. So it's safe to take 2 years back if you assume a development time/rollout of a year (at the time of OxC3 0.2). 

We're targeting the minimum specs of following systems in OxC3 0.2:

- Phones:
  - Binding tier low:
    - Samsung S20 (Samsung SM-G980F).
    - Apple iPhone 12 (A14).
  - Binding tier high:
    - Samsung S21 (Samsung SM-G9910).
    - Google Pixel 5a.
    - Xiaomi Redmi Note 8.
- Laptop:
  - Nvidia RTX 3060 Laptop GPU.
  - AMD RX 6600M.
  - Apple Macbook Pro 14 (M1 Pro).
- PCs:
  - Nvidia RTX 3060.
  - AMD 6600 XT.
  - Intel A750.

Binding tier is high for all laptops and PCs excluding Apple devices. All Apple devices are binding tier low until they release proper devices.

Just because these are the target minimum specs doesn't mean older hardware is unsupported. The hard requirements should be looked at instead to determine if the device is supported. ***These are just minimum feature targets, they aren't all tested.***

#### List of Vulkan requirements

Because of this, a device needs the following requirements to be OxC3 compatible:

- Vulkan 1.1 or higher.
- Tessellation shaders and geometry shaders are optional.
- subgroupSize of 16 - 128.
- subgroup operations of basic, vote, ballot. Available only in compute by default. arithmetic is optional.
- shaderSampledImageArrayDynamicIndexing, shaderStorageBufferArrayDynamicIndexing, shaderUniformBufferArrayDynamicIndexing turned on.
- samplerAnisotropy, drawIndirectFirstInstance, independentBlend, imageCubeArray, fullDrawIndexUint32, depthClamp, depthBiasClamp, tessellationShader turned on.
- Either BCn (textureCompressionBC) or ASTC (textureCompressionASTC_LDR) compression *must* be supported (can be both supported).
- shaderInt16 support.
- maxColorAttachments and maxFragmentOutputAttachments of 8 or higher.
- maxDescriptorSetInputAttachments, maxPerStageDescriptorInputAttachments of 7 or higher.
- MSAA support of 1 and 4 or higher (framebufferColorSampleCounts, framebufferDepthSampleCounts, framebufferNoAttachmentsSampleCounts, framebufferStencilSampleCounts, sampledImageColorSampleCounts, sampledImageDepthSampleCounts, sampledImageIntegerSampleCounts, sampledImageStencilSampleCounts). Support for MSAA 2 is non default.
- maxComputeSharedMemorySize of 16KiB or higher.
- maxComputeWorkGroupCount[N] of 64Ki or higher.
- maxComputeWorkGroupInvocations of 512 or higher.
- maxComputeWorkGroupSize of 1024 or higher.
- If present: maxDrawIndirectCount of 1Gi or higher.
- maxFragmentCombinedOutputResources of 16 or higher.
- maxFragmentInputComponents of 112 or higher.
- maxFramebufferWidth, maxFramebufferHeight, maxImageDimension1D,  maxImageDimension2D, maxImageDimensionCube, maxViewportDimensions[i] of 16Ki or higher.
- maxFramebufferLayers, maxImageDimension3D, maxImageArrayLayers of 256 or higher.
- maxPushConstantsSize of 128 or higher.
- maxSamplerAllocationCount of 4000 or higher.
- maxSamplerAnisotropy of 16 or higher.
- maxStorageBufferRange of .25GiB or higher.
- if tesselation is supported:
  - maxTessellationControlPerVertexInputComponents, maxTessellationControlPerVertexOutputComponents, maxTessellationEvaluationInputComponents, maxTessellationEvaluationOutputComponents of 124 or higher.
  - maxTessellationControlTotalOutputComponents of 4088 or higher.
  - maxTessellationControlPerPatchOutputComponents of 120 or higher.
  - maxTessellationGenerationLevel of 64 or higher.
  - maxTessellationPatchSize of 32 or higher.

- maxSamplerLodBias of 4 or higher.
- maxUniformBufferRange of 64KiB or higher.
- maxVertexInputAttributeOffset of 2047 or higher.
- maxVertexInputAttributes, maxVertexInputBindings of 16 or higher.
- maxVertexInputBindingStride of 2048 or higher.
- maxVertexOutputComponents of 124 or higher.
- maxComputeWorkGroupSize[0,1] of 1024 or higher. and maxComputeWorkGroupSize[2] of 64 or higher.
- if geometry shader is supported:
  - maxGeometryInputComponents of >=64.
  - maxGeometryOutputComponents of >=128.
  - maxGeometryOutputVertices of >=256.
  - maxGeometryShaderInvocations of >=32.
  - maxGeometryTotalOutputComponents of >=1024.
- maxMemoryAllocationCount of 4096 or higher.
- maxBoundDescriptorSets of 4 or higher.
- maxDescriptorSetStorageBuffersDynamic of  >=16.
- maxDescriptorSetUniformBuffersDynamic of >=32.

##### Resource binding tiers

###### EResourceBindingTier_Low

- maxPerStageDescriptorSamplers of 16 or higher.
- maxPerStageDescriptorUniformBuffers of 31 or higher.
- maxPerStageDescriptorStorageBuffers of 31 or higher.
- maxPerStageDescriptorSampledImages of 96 or higher.
- maxPerStageDescriptorStorageImages of 8 or higher.
- maxPerStageResources of 127 or higher.
- maxDescriptorSetSamplers of 80 or higher.
- maxDescriptorSetUniformBuffers of 155 or higher.
- maxDescriptorSetStorageBuffers of 155 or higher.
- maxDescriptorSetSampledImages of 480 or higher.
- maxDescriptorSetStorageImages of 40 or higher.

###### EResourceBindingTier_High

- Everything mentioned in resource tier low but with a limit of 200k or higher.
- maxPerStageResources of 1M or higher.

#### List of DirectX12 requirements

- DirectX12 Feature level 12_1. 
- WDDM 2.7 and above.
- Default of EResourceBindingTier_High.
- GPU:
  - Nvidia Maxwell 2nd gen and above.
  - AMD GCN 5 and above.
  - Intel Arc Alchemist and above.
  - Intel Gen 9 and above.

##### Default features in DirectX

Since Vulkan is more fragmented, the features are more split up. However in DirectX, the features supported by default are the following:

- EGraphicsBindingTier_High is always enabled. Enforces at least 200k resource binds available of each type.
- EGraphicsFeatures_SubgroupArithmetic, EGraphicsFeatures_SubgroupShuffle. Wave intrinsics are also supported by default.
- EGraphicsFeatures_GeometryShader, EGraphicsFeatures_TessellationShader and EGraphicsFeatures_MultiDrawIndirectCount are enabled by default.
- EGraphicsFeatures_Raytracing, EGraphicsFeatures_RayQuery, EGraphicsFeatures_MeshShaders, EGraphicsFeatures_VariableRateShading are a part of DirectX12 Ultimate (Turing, RDNA2, Arc and up).
- EDeviceDataTypes_BCn, EGraphicsDataTypes_I64, EGraphicsDataTypes_F64 are always set.

#### List of Metal requirements

- Metal 3 (Apple7 tier).
- Phone:
  - Apple iPhone 12 (A14, Apple7).
- Laptop:
  - Apple Macbook Pro 14 (M1 Pro, Apple7).

##### Default features in Metal

- EGraphicsFeatures_TiledRendering is always set.
- EGraphicsFeatures_TessellationShader is always set.
- EGraphicsDataTypes_ASTC is always set.
- EGraphicsDataTypes_BCn can be set as well.
- EGraphicsDataTypes_AtomicF32, EGraphicsDataTypes_AtomicI64, EGraphicsDataTypes_F16, EGraphicsDataTypes_I64 are always set.
- EGraphicsBindingTier_Low by default due to strict binding requirements. See "List of Vulkan requirements" -> "Resource binding tiers".

#### TODO: List of WebGPU requirements

Since WebGPU is still expiremental, no limitations will be made to OxC3 to support it yet.

## Contributions

To contribute to this repository, you agree to the [contribution guidelines](FOR_CONTRIBUTORS.md). Before merging a PR as an external party, you have to sign a contributor license agreement.

## License

This repository is available under two licenses:

- LGPL3 open source license.
- Commercial license.

Any company not wanting to adhere to the LGPL3 license can contact us as contact@osomi.net. 
