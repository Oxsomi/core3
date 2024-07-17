# OxC3 tool

The OxC3 tool is intended to handle all operations required for Oxsomi core3. This includes:

- Calculating hashes.
- Generating random numbers or keys.
- Conversions between file formats.
- Packaging a project.
- Compiling shaders.
- **TODO**: Show GPU/graphics device info.
- Inspecting a file (printing the header and other important information).
- Encryption.
- **TODO**: Compression.

And might include more functionality in the future.

To get info about a certain category or operation you can type `-? or ?` or any unrecognized command after it. Example; `OxC3 ?` will show all categories. `OxC3 file ?` will show all file operations. `OxC3 file to ?` will show either all formats (if the operation supports formats) or all supported flags/arguments. If the operation supports formats then `OxC3 file to -format <format> ?` could be used.

## Calculating hashes

A hash can be calculated as following:

`OxC3 hash file -format SHA256 -input myDialog.txt`

Where the format can either be `CRC32C` or `SHA256`.

A hash from a string can be calculated like so:

`OxC3 hash string -format CRC32C -input "This is my input string"`

## Random

Random number generation is handy for multiple things. CSPRNG (cryptographically secure PRNG) is chosen by default. To generate multiple entries; use `-count <count>`. To output to a file use `-output <file>`.

`OxC3 rand key`

Generates a CSPRNG key that can be used for AES256 encryption. You can use `-length <lengthInBytes>` to customize byte count; defaulted to 32.

`OxC3 rand char`

Generates random chars; 32 by default. `-length <charCount>` can be used to customize length. The included characters by default are viable ASCII characters (<0x20, 0x7F>). In the future --utf8 will be an option, but not for now (**TODO**:). `-chars <chars>` can be used to pick between characters; ex. `-chars 0123456789` will create a random number. This also allows picking the same character multiple times (e.g. -chars 011 will have 2x more chance to pick 1 instead of 0). Some helpful flags: --alpha (A-Za-z), --numbers (0-9), --alphanumeric (0-9A-Za-z), --lowercase (a-z), --uppercase (A-Z), --symbols (everything excluding alphanumeric that's ASCII). If either of these flags are specified, it'll not use the valid ascii range but rather combine it (so -chars ABC --numbers would be ABC0123456789). E.g. --uppercase --numbers can be used to generate 0-9A-Z. If for example --alphanumeric --alpha is used, it will cancel out.

`OxC3 rand num`

Is just shorthand for `OxC3 rand char -chars <numberKeyset>`. If --hex is used, it'll use 0-9A-Z, if --nyto is used it'll use 0-9a-zA-Z_$, if --oct is used it'll use 0-7, if --bin is used it'll use 0-1. Decimal is the default (0-9). `-length <charCount>` can be used to set a limit by character count and `-bits <bitCount>` can be used to limit how many bits the number can have (for decimal output this can only be used with 64-bit numbers and below).

`OxC3 rand data -length 16 -output myFile.bin`

Allows to output random bytes to the binary. Alias to `OxC3 rand key` but to a binary file. -length can be used to tweak number of bytes. -count will added multiple generations appended in the same file. Without -output it will output a hexdump.

## File

`OxC3 file` is the category that is used to convert between file formats. The keywords `from` and `to` can be used to convert between native and non native files. For example:

`OxC3 file to -format oiDL -input myDialog.txt -output myDialog.oiDL --ascii`

Will convert the enter separated string in myDialog into a DL file (where each entry is a separate string). This can also combine multiple files into one DL file:

`OxC3 file to -format oiDL -input myFolder -output myFolder.oiDL`

This will package all files from myFolder into a nameless archive file. These files can be accessed by file id.

To unpackage this (losing the file names of course):

`OxC3 file from -format oiDL -input myFolder.oiDL -output myFolder`

### Common arguments

The following flags are commonly used in any format:

- --sha256: Includes 256-bit hashes instead of 32-bit ones into file if applicable.
  - If a file is compressed or encrypted, a hash is used to detect if it hasn't been corrupted or tempered with. A CRC32 (if this option is turned off) is insufficient if dealing with smart intermediates instead of uncompression/unencryption errors. 256-bit hash is sufficient at minimizing this risk (possible issue with quantum computers in the future).
- --uncompressed: Keep the data uncompressed (default is compressed).
  - By default, the data stored in the native format is compressed. If you're testing a custom file parser or want to inspect the data generated, it could be nice to test it like this.
- --fast-compress: Picks the compression algorithm that takes the least time to compress.
  - If a lot of files are compressed and need to be available quickly, then this option can be used. It's off by default, because this does impact how compact the file will be. Normally it optimizes for storage rather than speed. For example offline asset baking is when you don't want this turned on. If this is not present, it'll use brotli:11, otherwise brotli:1 will be used.
- --not-recursive: If folder is selected, blocks recursive file searching. Can be handy if only the direct directory should be included.
- **TODO**: --v: Verbose.
- **TODO**: --q: Quiet.

The following parameters are commonly used in any format:

- `-format <fileFormat>`: File format
  - Specifies the file format that has to be converted from / to. It doesn't detect this from file because this allows you to supply .bin files or other custom extensions.
- `-input <inputPath>`: Input file/folder (relative)
  - Specifies the input path. This is relative to the current working directory. You can provide an absolute path, but this will have to be located inside the current working directory. Otherwise you'll get an unauthorized error. Depending on the format, this can be either a file or a folder. Which will have to be one of the supported types. This format is detected based on the magic number or file extension (if magic number isn't applicable).
- `-input2 <inputPath>`: Second input; useful when only two input arguments are needed.
- `-output <outputPath`>: Output file/folder (relative)
  - See -input.
- `-aes <key>`: Encryption key (32-byte hex)
  - A key should be generated using a good key generator. This key could for example be specified as: `0x00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000` in hex (64 characters). Can include spaces or tabs, as long as after that it's a valid hex number.

### oiDL format

Since a DL (Data List) file can include either binary files or strings, you'll have to specify what.

- --ascii: Indicates the input files should be threated as ascii. If a file; splits by enter, otherwise 1 entry/file.
- --utf8: Indicates the input files should be threated as UTF8. If a file; splits by enter, otherwise 1 entry/file.
- `-split <string>`: If the input is a string, allows you to split the file by a different string than an endline character / string.

If these are absent; it'll use binary format by default. When either ascii or utf8 is used, file(s) should be encoded using the proper format that is requested. If only one file is used, multiple strings will be created by splitting by the newline character(s) or the split string if overriden. Recombining an ascii oiDL will resolve to a .txt if split is available or the final destination ends with .txt.

*Example usage:*

`OxC3 file to -format oiDL -input myDialog0.txt -output myDialog.oiDL --ascii`

`OxC3 file from -format oiDL -output myDialog1.txt -input myDialog.oiDL --ascii`

### oiCA format

A CA (Compressed Archive) file can include either no, partial or full timestamps, you'll have to specify if you want that:

- --full-date: Includes full file timestamp (Ns).
  - Creates a full 64-bit timestamp in nanoseconds of the file (in the resolution that the underlying filesystem supports).
- --date: Includes MS-DOS timestamp (YYYY-MM-dd HH-mm-ss (each two seconds)).
  - Creates a 2x 16-bit (32-bit) timestamp with 2s accuracy. This is nice for optimization, but keep in mind that the resolution is limited. This date will run out in 2107 and can only support 1980 and up. Full 64-bit has up to nanosecond precision and can support 1970-2553.

These are left out by default, because often, file timestamps aren't very important when dealing with things like packaging for apps. The option is still left there for specific use cases.

*Example usage:*

`OxC3 file to -format oiCA -input myFolder -output myFolder.oiCA --full-date`

### Combine

`OxC3 file combine -format oiSH -input a.oiSH -input2 b.oiSH -output c.oiSH` can be used to combine two oiXX files into one if supported.

This is only supported if it can logically be merged:

- For oiSH, this means that it has to be compiled from the same source(s), so matching relative includes should have the same hash, the source hash needs to be the same and the compiler settings. This allows combining two lean files into a single one.
- **TODO**: For oiCA, this would mean combining two archives into one. This can only succeed if the two have similar settings and if the archive files don't overlap (archiveA/a.txt and archiveB/a.txt would conflict, unless the crc is the same).
- **TODO**: For oiDL, this simply means the second entries are appended to the other, provided the two oiDL settings are the same (UTF8, ascii, data).

### TODO: Split

`OxC3 file split -format oiSH -input combined.oiSH -output lean.spv.oiSH -compile-output spv` can be used to split the bulky oiSH into a single one.

- oiSH is supported with the `-compile-output` argument to determine which shader output(s) are included for final write.
- oiDL allows specifying `entry` and `offset` to remove everything except a section.
- oiCA allows specifying a folder to extract.

## TODO: Packaging a project

`OxC3 package -input myFolder -output myFolder.oiCA` is used to package a folder into Oxsomi formats. This means that it will standardize all files it detects and converts them to our standard file. For example a .fbx file could be automatically converted to a scene and/or model file, a texture could be converted to a standardized image file, etc. This is basically a baking process to ensure all shaders, textures, models and other resources are the correct format for target architectures. -aes argument is allowed to encrypt the modules.

These are generally attached to the exe, apk or other executable file to ensure these resources can be found and aren't as easily accidentally modified on disk, as well as making them more portable. See the README.

## Compiling shaders

`OxC3 compile shaders` is used to compile text shaders to application ready shaders. This could mean preprocessing text shaders to inline all includes for graphics APIs/platforms that take text only or compiling to an actual binary (DXIL or SPIRV).

When operating on a folder, it will attempt to find `.hlsl` files and then processes them in parallel into the output folder.

`-threads` can be used to limit thread count. Such as `-threads 0` = default , `-threads 50%` = 50% of all threads, `-threads 4` = 4 threads. Default behavior is: If total input length >=64KiB with at least 8 files or if at least 16 files are present, then all cores will be used for threading.

`-compile-output` is the outputs that are enabled. If this mode is multiple and --split is enabled then it will rename to .spv.hlsl and .dxil.hlsl for example (if preprocessing) or .txt for includes. The following modes are supported: `spv` and `dxil`. To use spv and dxil, you can use `dxil,spv` or `all` (will include others in the future). By default (if the argument isn't present) it compiles as `all`, so the shader is usable by all backends. Without --split, it will include the output modes as specified into a single oiSH file.

`-compile-type` is the compile type. Can be one of the following: `preprocess`, `includes`, `reflect` and `compile`. Each compile mode has their own info section. By default (if the argument isn't present) the mode is `compile`.

`-include-dir` can be used to add only a single include directory to search for includes (aside from relative includes).

`@myFile.hlsl` specifies builtin shaders, such as `@types.hlsl` and `@resources.hlsl` which are bindings to be compatible with OxC3. This can also access NV specific HLSL extensions when DXIL is used as a target and the `extension` annotation is used.

### Preprocess

The `-compile-type preprocess` will turn the .hlsl into an HLSL ready for parsing (without includes and defines) and is used internally automatically when other compile modes are used; the option to do it can still prove useful if there's another compiler or parser or if it's important that only a single shader file is shipped rather than multiple includes.

`OxC3 compile shaders -format HLSL -compile-output spv -compile-type preprocess -input a.hlsl -output a.preprocessed.hlsl`

### Includes

The `-compile-type includes` will turn the .hlsl into an include list and can be used to determine the heaviest include dependencies. Each include will have their own counter and either a file or a folder can be used to determine how many times an include is referenced by other includes or source files.

`OxC3 compile shaders -format HLSL -compile-output spv -compile-type includes -input a.hlsl -output a.preprocessed.hlsl`

Will show something like this:

```
Includes:
123 reference(s): <hash> <fileSize> <optional: timestamp> //types.hlsl
<hash> <fileSize> <optional: timestamp> /shaders/mySimpleInclude.hlsl

Sources:
<hash> <fileSize> /shaders/mySimpleFile.comp.hlsl
```

This tool can be useful to determine if the includes should be re-examined because they might trigger to many recompiles on change for example. Timestamp is in `Time_format` (0000-00-00T00:00:00.000000000Z), hash is CRC32c and fileSize is in bytes.

Reference count is optional if reference count is 1, since it's a common case for single include files (since #pragma once is almost always used).

When toggled on a folder, it will make a .txt file per file that it processes, so individual files can be inspected. It also makes a root.txt file which contains a merged version that  is easier to read if it's about getting all of them in one place. Example:

```
Includes:
022 reference(s): fe9ec6b9 10219 types.hlsl
022 reference(s): cc0a2ce9 08839 resources.hlsl
012 reference(s): bb15afc7 01860 2024-05-04T14:32:55.000000000Z D:/programming/repos/rt_core/res/shaders/resource_bindings.hlsl
008 reference(s): c36476d2 87746 nvHLSLExtns.h
008 reference(s): a78d6265 01329 2024-05-04T14:33:08.000000000Z D:/programming/repos/rt_core/res/shaders/ray_basics.hlsl
008 reference(s): 32fdc427 10554 nvShaderExtnEnums.h
008 reference(s): 10398840 29623 nvHLSLExtnsInternal.h
004 reference(s): 916fbecd 03174 2024-05-04T14:33:14.000000000Z D:/programming/repos/rt_core/res/shaders/primitive.hlsl
002 reference(s): 70055f36 01630 2024-05-04T21:10:48.000000000Z D:/programming/repos/rt_core/res/shaders/camera.hlsl
002 reference(s): 4470bdfa 07490 2024-05-07T21:36:53.000000000Z D:/programming/repos/rt_core/res/shaders/atmosphere.hlsl

Sources:
4470bdfa 07490 D:/programming/repos/rt_core/res/shaders/atmosphere.hlsl
70055f36 01630 D:/programming/repos/rt_core/res/shaders/camera.hlsl
31e8a279 02363 D:/programming/repos/rt_core/res/shaders/depth_test.hlsl
3acc8620 01888 D:/programming/repos/rt_core/res/shaders/graphics_test.hlsl
bcc2bb61 01373 D:/programming/repos/rt_core/res/shaders/indirect_compute.hlsl
0b762523 02790 D:/programming/repos/rt_core/res/shaders/indirect_prepare.hlsl
916fbecd 03174 D:/programming/repos/rt_core/res/shaders/primitive.hlsl
db3878c9 03860 D:/programming/repos/rt_core/res/shaders/raytracing_pipeline_test.hlsl
aeb6a491 02528 D:/programming/repos/rt_core/res/shaders/raytracing_test.hlsl
a78d6265 01329 D:/programming/repos/rt_core/res/shaders/ray_basics.hlsl
bb15afc7 01860 D:/programming/repos/rt_core/res/shaders/resource_bindings.hlsl
```

#### Limits

Includes use OxC3 file paths, as such, escaping out of the working directory is prohibited. Any resolved file path that doesn't start with the working directory is illegal. And any tool that may read these includes (such as a file watcher) should have a similar protection mechanism.
This gives a logical limit to how far back an include can go. Such a limit is also very useful to prevent any tooling from reading into files that weren't intentional or don't exist within the scope of the program (such as embedding a fictional include file which could read any file the program has access to). It will also enforce relative files, rather than allowing absolute files in the includes for some reason.

### TODO: Reflect

### Symbols

The `-compile-type symbols` will turn the .hlsl into a list of symbols and can be used to determine where certain functions/variables/structs are located. This can be very useful for refactoring to see if there's any function/variable that should be elsewhere. It could also be useful for better search options as well as debugging the parser.

`OxC3 compile shaders -format HLSL -compile-output dxil -compile-type symbols -input a.hlsl -output a.symbols.hlsl`

Will show something like this:

```
Struct Camera at L#26:1
	Variable v at L#28:10
	Variable p at L#28:13
	Variable vp at L#28:16
	Variable vInv at L#29:10
	Variable pInv at L#29:16
	Variable vpInv at L#29:22
	Function getRay at L#31:2
		Parameter id at L#31:17
		Parameter dims at L#31:27
```

Note: The difference between reflect and symbols is that reflect maintains the data as a binary which can be easily queried, shipped and used in production code. Reflect is a more advanced version of symbols, which exists more for advanced usages.

### Compile

Compile mode (default) will turn the text into shaders ready for consumption by a graphics API. This could be DXIL, SPIRV or even text representations (MSL, WGSL or even GLSL in the future). These are then stored in an oiSH file, which contains information about the uniforms, inputs/outputs, basic reflection info and entrypoint binary/name as well as other metadata. These oiSH files can be either bulky (works for every backend) or lean (works only for the target(s)).

#### Built-in defines

The following defines are set by OxC3 during compilation:

- `__OXC3` to indicate that OxC3 is compiling or parsing the shader.
- `__OXC3_MAJOR`, `__OXC3_MINOR` and `__OXC3_PATCH` to indicate OxC3 version. For 0.2.0 these would be 0, 2 and 0 respectively.
- `__OXC3_VERSION` same layout as `OXC3_MAKE_VERSION` aka (major << 22) | (minor << 12) | patch.
- `__OXC3_EXT_<X>` foreach extension that's enabled by the current compilation. For example: `__OXC3_EXT_F16`, `__OXC3_EXT_F64`, `__OXC3_EXT_RAYQUERY`, etc.

#### Semantics

Semantics for input(s) and output(s) for shaders get parsed and have the following restrictions:

- SV_ is reserved for HLSL semantic values.
- Other semantic values must follow TEXCOORD[n], seeing as SPIRV has no concept of semantics. These can be bound through Vulkan as binding n or in D3D12 as TEXCOORD with id n. Allowing semantics would cause shaders that can't be compiled for SPIRV or any languages enabled by SPIRV (such as MSL or WGSL). As such, semantics for input/output layouts are completely incompatible with the OxC3 shader compiler.

#### Entrypoint annotations

Each entrypoint can have annotations on top of the ones used by DXC. The ones introduced in OxC3's pre-processor are the following:

- [stage("vertex")] similar to DXC's [shader("vertex")] but instead of implying a library compilation (for use in StateObjects), it signals a standalone compilation for this entrypoint. Essentially allowing pixel and vertex shaders to be compiled and packaged as a single oiSH file.
  - Defining stages is only allowed for stages that have to be separately compiled. For raytracing shaders and workgraphs, the shader annotation should still be used.
  - Type must be one of vertex, pixel, compute, geometry, hull, domain, mesh, task.
  - Do keep in mind that each stage needs a full compile. It might be beneficial to bundle them as a single lib using the "shader" type, when this feature becomes available with workgraphs.
  - If not defined, the compiler will ignore the functions if they don't have either a `stage` or `shader` annotation.
- [vendor("NV", "AMD", "QCOM")] which vendors are allowed to run this entrypoint. There could be a reason to restrict this, for example when NV specific instructions are used (specifically together with DXIL). Must be one of: NV, AMD, ARM, QCOM, INTC, IMGT, MSFT. If not defined, will assume all vendors are applicable. Multiple annotations for vendor is illegal to clarify that it won't induce a new compile for each vendor.
- [extension("16BitTypes")] which extensions to enable. For example 16BitTypes will enable 16-bit types for that entrypoint. 
  - Do keep in mind that extensions might introduce another recompile for entrypoints that don't have the same extensions. For example with raytracing shaders. In their case, it will introduce two compiles if one entrypoint doesn't support 16-bit ints and another does.
  - `__OXC3_EXT_<X>` can be used to see which extension is enabled. For example `__OXC3_EXT_ATOMICI64`.
  - Extension must be one of F64, I64, 16BitTypes, AtomicI64, AtomicF32, AtomicF64, SubgroupArithmetic, SubgroupShuffle, RayQuery, RayMicromapOpacity, RayMicromapDisplacement, RayMotionBlur, RayReorder, Multiview, ComputeDeriv, PAQ.
    - **Note**: RayReorder is currently only available for raygeneration shaders.
    - **Note**: Multiple extension annotations will indicate there will be a separate compile with each. For example: `[extension()]` and `[extension("16BitTypes")]` in front of the same function will indicate the function will be compiled with 16-bit types on and off. 16-bit off would for example run on <=Pascal (GTX 10xx). This will allow the same entrypoint to be ran with different functionality. This could aid for example in unpacking vertex/texture data with native support (rather than manual f16tof32).
      - Another good example could be RayReorder, which could give substantial boosts in path tracing workloads. Lovelace would need `[extension("RayReorder")]` while the rest such as non NV and Pascal, Turing, Ampere would need `[extension()]`. This will force a recompile.
      - If multiple extensions are available, it will pick from top to bottom if they're all available. So `[extension("16BitTypes")]` and `[extension("RayReorder")]` would for example only pick the second one if the first one isn't available.
- [uniforms("X" = "F32x3(1, 0, 0)", "Y" = "F32x3(0, 1, 0)")] Introduces one subtype of the stage that has the defines `$X` and `$Y` set to the relevant values of `(F32x3(1, 0, 0))` and `(F32x3(0, 1, 0))`.
  - This attribute can be used multiple times to compile the same entrypoint with different defines. It will try to bundle compiles wherever possible (try to keep defines similar).
  - Only defining the uniform but no assign will just see it as a define that can be checked with #ifdef. 
  - Uniform name must not indicate symbols or spaces.
  - The application is in charge of picking the uniform combo for the entire lib or each entrypoint. So it is possible to switch between uniforms based on what the app wants, unlike models and extensions which are handled by the runtime.
  
- [model(6.8)]
  - Which shader model to use. If this annotation is present multiple times, it indicates multiple compiles with different shader models.
    - Minimum must be 6.5+ since that's the minimum OxC3 supports.
    - Workgraphs require SM6.8.
    - AtomicI64 requires SM6.6.
    - ComputeDeriv requires SM6.6.
    - WaveSize requires SM6.6.
    - WaveSize with 2 or 3 arguments (min, max, recommended) requires SM6.8.
    - PAQ (Payload Access Qualifiers) requires SM6.6.
    - Stage type always has to be compatible with specified models.
    - If extensions are defined, one of the pair of extensions/models has to be compatible with the minimum requirement. If this is the case, that one is used as fallback.
      - Example: [model(6.2)] and [model(6.0)] is possible with [extensions("16BitTypes")] only if there's another [extensions()] available. Otherwise it knows that model 6.0 can't be compatible with I16 and F16. In this case, there would only be 3 binaries: 6.0 16-bit off, 6.2 16-bit off, 6.2 16-bit on. Without model specified, it would determine minimum featureset for those extensions and push them. So specifying the two extensions separately (without models) will just make two binaries (6.0 16-bit off, 6.2 16-bit on).
    - If multiple models are available, the runtime will choose from highest shader model to lowest shader model available.
  - If not defined, will use minimum for the detected feature set.
  - `__SHADER_TARGET_MAJOR` and `__SHADER_TARGET_MINOR` can be used to distinguish which one is being compiled. 

**NOTE**: Since multiple models, uniforms and extensions can cause multiple compiles, be very careful that you don't use too many of these decorations (2 different sets of uniforms + 2 different extensions + 2 models = 2^3 or 8 compiles). Even though the compiler will do everything in parallel, it will still be slower to do multiple configurations that will never be used. So keep duplicate annotations in check wherever possible. The compiler will also try to combine compiles if it notices that they can be shared (for example with lib compiles where one compile might expose multiple entrypoints).

#### Special flags

- `--debug` is used to toggle debug info in the binary.
- `--error-empty-files` is used to error when no entrypoints are found to compile. This is useful to enforce that the destination directory shouldn't contain includes alongside source files (rather than using -include-dir with a separate includes directory).
- `--split` is used to split up every oiSH file into its own file. This is very useful when building for 1 dedicated target. By default this is turned off, to make sure every shader can be ran with every backend.

## Show GPU/graphics device info


It can also be used to print GPU info regarding devices that support OxC3 and what their features are. It uses the currently active graphics API (be it DirectX12 or Vulkan).

`OxC3 graphics devices` will show all OxC3 compatible devices with minimal info (such as VRAM, LUID/UUID).

`graphics devices -entry 0 -count 0` can be used to show detailed info about all OxC3 devices. Not mentioning `-count` with `-entry` is the same as `-count 1` (0 means 'remainder').

## File inspect

Two operations constitute as file inspection: `file header` and `file data`.

Header is useful to know what the header says. It also allows to inspect the header of certain subsections of the data. This only works on Oxsomi formats.

Data allows you to actually inspect the data section of certain parts of the file.

`file header -input test.oiCA` would print the information about the file header.

`file data -input test.oiCA` would tell about the file table for example. File data also needs to provide `-aes` if the source is encrypted. If entry is absent, it will provide a general view of the file.

With `-entry <offset or path>` a specific entry can be viewed. If an entry is specified, the `-output` can be used to extract that one entry into a single file (or folder). If this is not specified, it will show the file as either a hexdump or plain text (if it's ascii) or the folder's subdirectories.

`file data` also allows the `-length` specifier for how many entries are shown. Normally in the log it limits to 64 lines (so for data that'd mean 64 * 64 / 2 (hex) = 2KiB per view). The `-start` argument can be used to set an offset of what it should show. An example: we have an oiCA with 128 entries but want to show the last 32; `file data -input our.oiCA -start 96 -length 32`. For a file entry, it would specify the byte offset and length (length is defaulted to 2KiB). If `-output` is used, it will binary dump the entire remainder of the file if `-length` is not specified (the remainder is the entire file size if `-start` is not specified).

For oiSH files, it is possible to supply `--bin` can be used  to fetch the binary instead of the entrypoints. Since an oiSH file can have more than one binary embedded in it. Not supplying an entry offset will behave as usual; showing all binaries. Supplying an entry offset and --binary will show that specific binary. To see the actual compiled binary, `-compile-output` can be used to obtain the specific information (for example DXIL or SPV binary). Example: `file data -input test.oiSH --bin -compile-output DXIL -entry 0` will show the DXIL binary at compiled entry 0 if available. This can still be used with `-start`, `-length` and `-output` to easily read & export binaries from an oiSH file.

## Encrypt

`OxC3 file encr -format <encryptionType> -input <file> -aes <key in hex> (optional: -output output)`

`OxC3 file decr -format <encryptionType> -input <file> -output <file> -k <key in hex> `

Generates an encrypted oiCA file. Works on files and folders. oiCA doesn't support AES128GCM, so AES256GCM is used by default.

## TODO: Compress

`OxC3 file pack -input <file> (optional: --fast-compress, --k <key in hex>)`

`OxC3 file unpack -input <file> (optional: --aes <key in hex>)`

Generates a compressed oiCA file. Can be encrypted. Works on files and folders.

*Note: Currently unimplemented.*

## Profile

Profiles the speed of important operations that might be happening a lot or operations that might take long.

- `OxC3 profile cast`: profiles how long casts take between F64, F32, F16 and a smaller or bigger float type. This doesn't include any additional floating point formats (only half, float and double). It does tests with normal numbers, denormalized numbers, NaNs and Infs.
- `OxC3 profile rng`: profiles how expensive Buffer_CSPRNG is (cryptographically secure random).
- `OxC3 profile crc32c`: profiles how much time a Buffer CRC32C takes up if the buffer isn't small.
- `OxC3 profile sha256`: profiles how fast a Buffer SHA256 is if the buffer isn't small.
- `OxC3 profile aes256/aes128`: how fast AES encryption is. AES256 should be preferred though for legacy reasons the other might be used (It's about the same speed). The encryption mode is always GCM.

## Helper functions

The following helper functions provide more information about the tool and commands that are available:

```bat
OxC3 info license
OxC3 info about
OxC3 help
OxC3 help categories
OxC3 help operations -input category
OxC3 help operation -input category:operation
OxC3 help format -input category:operation:format
```

