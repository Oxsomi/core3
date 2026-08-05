# OxC3 (Oxsomi core 3.2.103)

| Platforms | x64 -> Vulkan | x64 -> Native API | x64 dynamic (Vk + Native) | ARM -> Vulkan | ARM -> Native API | ARM dynamic (Vk + Native) |
| --------- | ------------- | ----------------- | ------------------------- | ------------- | ----------------- | ------------------------- |
| Windows   | ![vulkan](https://github.com/Oxsomi/core3/actions/workflows/windows.yml/badge.svg) | **D3D12**: ![d3d12](https://github.com/Oxsomi/core3/actions/workflows/windows_d3d12.yml/badge.svg) | ![dynamic](https://github.com/Oxsomi/core3/actions/workflows/windows_dynamic.yml/badge.svg) | **![vulkan](https://github.com/Oxsomi/core3/actions/workflows/windows_arm.yml/badge.svg)** | **D3D12**: ![d3d12](https://github.com/Oxsomi/core3/actions/workflows/windows_d3d12_arm.yml/badge.svg) | **![dynamic](https://github.com/Oxsomi/core3/actions/workflows/windows_arm_dynamic.yml/badge.svg)** |
| Mac OS X  | ![vulkan](https://github.com/Oxsomi/core3/actions/workflows/osx.yml/badge.svg) | **Metal**: **TBD** | ![dynamic](https://github.com/Oxsomi/core3/actions/workflows/osx_dynamic.yml/badge.svg) | ![vulkan](https://github.com/Oxsomi/core3/actions/workflows/osx_arm.yml/badge.svg) | **Metal**: **TBD** | ![dynamic](https://github.com/Oxsomi/core3/actions/workflows/osx_arm_dynamic.yml/badge.svg) |
| Linux     | ![vulkan](https://github.com/Oxsomi/core3/actions/workflows/linux.yml/badge.svg) | N/A | ![dynamic](https://github.com/Oxsomi/core3/actions/workflows/linux_dynamic.yml/badge.svg) | **![vulkan](https://github.com/Oxsomi/core3/actions/workflows/linux_arm.yml/badge.svg)** | N/A | **![vulkan](https://github.com/Oxsomi/core3/actions/workflows/linux_arm_dynamic.yml/badge.svg)** |
| SteamOS   | ![vulkan](https://github.com/Oxsomi/core3/actions/workflows/steamos.yml/badge.svg) | N/A | See linux, but not recommended | N/A | N/A | N/A |
| Android   | *host:* ![windows host](https://github.com/Oxsomi/core3/actions/workflows/android_on_windows.yml/badge.svg) ![linux host](https://github.com/Oxsomi/core3/actions/workflows/android_on_linux.yml/badge.svg) ![macos host](https://github.com/Oxsomi/core3/actions/workflows/android_on_osx.yml/badge.svg) | N/A | N/A, no dynamic linking | (same jobs; both ABIs are built together) | N/A | N/A, no dynamic linking |
| iOS       | **TBD** | **Metal**: **TBD** | N/A, no dynamic linking | **TBD** | **Metal**: **TBD** | N/A, no dynamic linking |
| Xbox UWP  | N/A | **D3D12**: TBD | N/A | N/A | N/A | N/A |

**OxC3** (0xC3, Oxsomi core 3) is a cross-platform C11 framework for applications, tools and games. It is the successor to O(x)somi core v2/v1, merging ostlc (standard template library), owc (window core) and ogc (graphics core) into one coherent, layered codebase. It is written in C so it stays fast to build, easy to parse for reflection/codegen, and straightforward to wrap from other languages (bindings or a future VM); a C++20 convenience layer is possible on top.

For per-module maturity, see [STATUS.md](STATUS.md). For how the modules fit together (and the error-handling idiom used everywhere), see [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Modules

- **OxC3_types**, the foundation, split in three:
  - *base*: fixed-width types, `Error` + stacktraces, `Allocator` interface, `Buffer` views with const/ref safety, `CharString` basics, atomics, `SpinLock`, `Thread`, `Time` (ISO-8601, cycle counters), fixed point, endianness, `ETypeId` reflection ids. Allocation-free.
  - *math*: SIMD vectors (`F32x4`, `I32x4`, `F32x2`, `I32x2`) with SSE/NEON/scalar backends, quaternions, arbitrary IEEE-754 float format casts (F16/BF16/TF19/PXR24/FP24/…), checked numeric casts, bit packing, PRNG helpers.
  - *container*: `TList(T)` typed lists over one `GenericList` core, owning strings + full UTF-8/16/32 interop, `Archive`, `BigInt`/`U128`, `AllocationBuffer` (GPU-style suballocator), `RefPtr`, streams, `JobQueue`, `Log`, `Buffer` hashing (SHA256, CRC32C, MD5), AES256/128-GCM encryption (AES-NI/VAES/AVX512 and ARM crypto paths) and CSPRNG.
  - Docs: [docs/types.md](docs/types.md).
- **OxC3_formats_***, file format read/write, all input-validated:
  - Standard: BMP (BGRA8), DDS (modern DXGI subset), WAV.
  - Oxsomi ([oiXX](docs/oiXX.md) family, sharing encryption/endianness conventions): [oiCA](docs/oiCA.md) archives (zip-like, AES-GCM capable, streamable), [oiDL](docs/oiDL.md) data lists, [oiSH](docs/oiSH.md) shader containers (DXIL+SPIR-V + reflection), [oiSB](docs/oiSB.md) shader buffer layouts, [oiBC](docs/oiBC_chimera.md) bytecode (*spec draft; not implemented yet*).
  - Docs: [docs/formats.md](docs/formats.md).
- **OxC3_platforms**, everything OS-dependent: default/tracked allocators (leak reports with per-allocation stacktraces in debug), sandboxed file IO (app/working dir only) + `FileStream`, virtual file system for assets embedded in the exe/apk (oiCA-backed), windows (physical + virtual), monitors, keyboard/mouse multi-device input, dynamic library loading. Backends: Windows, Linux (Wayland), OS X (partial), Android.
  - Docs: [docs/platforms.md](docs/platforms.md).
- **OxC3_graphics**, modern-only GPU abstraction over **Vulkan** and **Direct3D12** (Metal/WebGPU reserved): refcounted objects, *virtual command lists* with automatic resource transitions, descriptor heap/layout/table model, bindless, compute/graphics/raytracing pipelines, BLAS/TLAS, mesh shaders, VRS, swapchains. Vulkan and D3D12 can be loaded side-by-side via dynamic linking and selected at runtime.
  - Docs: [docs/graphics_api.md](docs/graphics_api.md), minimum spec: [docs/graphics_spec.md](docs/graphics_spec.md).
- **OxC3_shader_compiler**, statically linked DXC wrapper: HLSL → DXIL and/or SPIR-V on all target platforms, multithreaded batch compilation, `[[oxc::...]]` annotations (stage/model/vendor/extensions/uniforms/binary masks), reflection + include tracking for incremental builds and hot reload, output to oiSH.
- **OxC3(CLI)**, the `OxC3` command line tool: oiCA/oiDL ↔ raw conversion, encrypt/decrypt, file inspection, hashing (sha256/crc32c/md5), random generation, profiling (float casts, CSPRNG, hashes, AES), multithreaded shader compilation, graphics device enumeration, and the asset packager used by the CMake integration.
  - Docs: [docs/OxC3_tool.md](docs/OxC3_tool.md).

## Requirements

- **Python 3.8.10+** and **Conan 2.7.1+** (avoids huge build times for DXC/LLVM/SPIRV deps).
- **CMake 3.13+**.
- A C11/C++ compiler (MSVC, clang, gcc). C++ is only used to interface with C++ deps such as DXC (and a C++ layer for samples or complex work is exposed).
- **Windows on ARM64**: ARMASM64 (install the ARM64 build tools via the VS installer) when using MSVC.
- **OS X**: `brew install llvm` for llvm-objcopy. If using the Vulkan SDK with bindless, export `MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS=1` and set `VULKAN_SDK` (e.g. in `~/.bash_profile`).
- **Linux**: Wayland is the window backend, `sudo apt install libwayland-dev libxkbcommon-dev -y` (plus wayland-scanner). For audio deps: `sudo apt install libasound2-dev libpipewire-0.3-dev -y`. For the windowed functional tests: `sudo apt install xdotool -y`. *X11-only sessions are currently unsupported for windowing.*
- **Android**: NDK installed with `ANDROID_NDK` set (plus `ANDROID_SDK` + a JDK when building an apk); Android 10 (API 29)+ on device (Vulkan 1.1+). Cross compiles from Windows, Linux and macOS. Ninja or make can drive the build (Ninja required for Debug builds due to the Vulkan validation layers). Since the packaging tool is built for the host too, the host's own prerequisites (above) apply as well. See [Android SDK setup](#android-sdk-setup).

## Getting started

```bash
git clone --recurse-submodules -j8 https://github.com/Oxsomi/core3
cd core3
python build.py -mode Release -tests True
```

`build.py` syntax:

- `python build.py -mode [Release/Debug/MinSizeRel/RelWithDebInfo/(empty; Windows multi config)] -simd [True/False] -tests [True/False] -dynamic_linking [True/False]`
  - `simd`: use SIMD (vectors, AES, SHA, CRC). Keep on (default); off exists for porting/fallback validation.
  - `tests`: build + run the unit tests.
  - `dynamic_linking`: desktop-only; allows multiple graphics APIs in one process.
- Extra flags via `-o flag=Bool`:
  - `forceVulkan`: prefer Vulkan over the native API (e.g. over D3D12 on Windows). Off by default.
  - `enableOxC3CLI`: build the OxC3 CLI. On by default.
  - `forceFloatFallback`: software half↔float casts. Off by default.
  - `enableShaderCompiler`: include the shader compiler (longer build). On by default.
  - `dynamicLinkingShaderCompiler`: build the shader compiler as a shared library. **On by default** on
    Windows/Linux/OS X; coerced off on Android and when `enableShaderCompiler` is off. Independent of
    `dynamicLinkingGraphics`, which exists for a different reason (runtime Vulkan/D3D12 selection).
    DXC is statically linked, so every executable touching the shader compiler otherwise carries ~28 MB
    of it; shared, that bulk exists once and links once, so builds are faster too, and it can be
    shipped or omitted separately.
    It also puts the compiler behind a module boundary, so a different backend could be swapped in
    without relinking consumers as long as it keeps the ABI.
    Callers must call `Compiler_setPlatform(Platform_instance)` once after `Platform_create` (an inline
    no-op in static builds), since the module has its own `Platform_instance`.
  - `debugShaderCompiler`: build/consume DXC and SPIRV-Reflect in the current mode instead of Release.
    Off by default, so a Debug build doesn't pay for a Debug DXC; those two dominate a from-scratch
    build and are rarely what you're stepping into. Also available as `-debug_shader_compiler True`.
  - `cliGraphics`: allow CLI operations that need OxC3_graphics; turn off for headless or to avoid shipping graphics dlls.

### Android

Cross compiled from Windows, Linux or macOS; the host half goes through the same code as `build.py`
(see `build_common.py`), so the same conan profiles apply.

```bash
python3 build_android.py -mode Debug
```

An android build can't run its own packager: the shader compiler is off there, and the resulting
binaries wouldn't be runnable on the build machine anyway. So it `tool_requires` a *host* OxC3 that has
`OxC3_package` in it, which `add_virtual_files()` then finds via `find_program`. `build_android.py`
builds and exports that host package from your working tree before cross compiling. The option set it's
built with lives in `HOST_TOOL_OPTIONS` in `conanfile.py` and is deliberately fixed, so one host package
serves every android configuration.

- `-api 31` (default): target API level; `-arch` defaults to arm64 **and** x64; `-simd True` by default;
  `-generator` defaults to "MinGW Makefiles" on Windows and "Unix Makefiles" elsewhere (Ninja is a good
  choice on every host, and is required for Debug builds because of the Vulkan validation layers).
- `--host_package_only` builds just the host `OxC3_package` and stops; `--skip_host_package` assumes it's
  already in the conan cache. CI uses the pair so the three android configurations share one host build.
- `--apk -package net.osomi.test -version 0.1.0 -lib myLibName -name "My test app"` builds an APK (same arch/mode/api/simd/generator settings must match prebuilt binaries when combined with `--skip_build`).
- `-packages <dir>` (repeatable) adds another folder of oiCA archives to the apk, for when the app that's
  being packaged has virtual files of its own next to OxC3's.
- `--sign` signs the APK: provide `-keystore` (and optionally `-keystore_password`), or have `JAVA_HOME` set to create a temporary keystore.
- `--run` installs and runs on a connected device in developer mode (requires `-package` and `-lib` if no apk step).
- `-category game` (default) sets the app category; `--install` exports the android package so a dependent
  project can `requires()` it; `--skip_build` reuses prebuilt binaries.

Only `ANDROID_NDK` is needed to build the libraries; `ANDROID_SDK` is additionally required for `--apk`
and `--run` (aapt/d8/zipalign/adb).

#### Running the unit tests on device

Android has no exec, so the per-suite executables ctest runs elsewhere don't exist there. `-tests True`
builds `OxC3_atest` instead: one `.so` with every suite, loaded by a NativeActivity
(see [src/test/android](src/test/android)). One command builds, packages, installs, launches and reports:

```bash
python build_android.py -mode Release -arch arm64 -generator Ninja -tests True --apk --sign --run \
  -keystore_password <pw> -package net.osomi.oxc3test -version 0.1.0 -lib OxC3_atest -name "OxC3 tests"
```

`--run` streams the device log and exits non-zero if any suite fails; it waits for the `OXC3_TEST_END`
line the runner emits, since `am start` gives back no exit code (`-test_timeout`, default 600s, bounds
the wait). Test apks are marked `android:exported` in every mode, because a non-exported activity can't
be launched by `am start` at all.

Add `--interactive` to also run the functional suites (window/input/audio); they need a human watching
the device, so they're skipped otherwise. That sets `debug.oxc3.interactive`, which you can also flip by
hand with `adb shell setprop`. Interactive runs aren't timed out.

Every suite is bundled except **shader_compiler**, since DXC isn't built for android.

#### Android SDK setup

**Android Studio is not needed.** Four standalone downloads, ~400 MB total:

1. **JDK 11+** ([Temurin](https://adoptium.net)). `sdkmanager` is itself a Java program, so this comes first.
   Set `JAVA_HOME`; `--sign` calls `keytool` through it.
2. **[Command line tools only](https://developer.android.com/studio#command-line-tools-only)**. Unzip so
   you end up with `<sdk>/cmdline-tools/latest/bin/sdkmanager`. The zip's own top folder is named
   `cmdline-tools`, so the inner one has to be moved into `latest/`, sdkmanager refuses to run with
   *"Could not determine SDK root"* if it isn't in a `latest`/version subdirectory.
3. **SDK packages.** `<sdk>` is the parent directory, and becomes `ANDROID_SDK`:
   ```bash
   sdkmanager --licenses
   sdkmanager "platforms;android-31" "build-tools;30.0.3"
   ```
   Add **[platform-tools](https://developer.android.com/tools/releases/platform-tools)** (adb) if you want
   `--run`; it unzips straight into `<sdk>/platform-tools`.
4. **[NDK](https://developer.android.com/ndk/downloads)**, a plain zip, unpack anywhere and point
   `ANDROID_NDK` at it. r27 is what CI uses.

Two things that will bite you otherwise:

- **build-tools version matters.** The apk step uses `aapt` (v1, not aapt2), which newer build-tools no
  longer ship, and `buildToolsDir()` always picks the newest installed. Keep an `aapt`-bearing version
  (30.0.3) as the newest one you have. Its `d8` warns that API 31 isn't supported and dexes anyway;
  that's cosmetic, it only affects desugaring, not `minSdkVersion`.
- **The legacy `tools` package is not required**, despite build-tools' `d8` wrapper looking for
  `find_java` inside it. `build_android.py` runs `d8.jar` directly to avoid that (the wrapper otherwise
  exits 0 having produced no `classes.dex`, and the build fails much later in aapt).

Expected layout:

```
<sdk>/cmdline-tools/latest/bin/sdkmanager
<sdk>/build-tools/30.0.3/{aapt,d8,zipalign}
<sdk>/platforms/android-31/android.jar
<sdk>/platform-tools/adb                     # only for --run/--install
```

### Using the virtual file system from your app

Graphics apps must embed the OxC3_graphics oiCA file(s) (built-in shaders, fonts, future LUTs):

```cmake
# Optional: configure_icon(OxC3 "${CMAKE_CURRENT_SOURCE_DIR}/res/logo.ico")
add_virtual_dependencies_external(TARGET Target DEPENDENCIES OxC3)
apply_dependencies(Target)
```

## Platform & instruction set support

| Platform | Status |
| -------- | ------ |
| Windows | **Full** |
| Linux | **Full** (Wayland sessions) |
| SteamOS | **Full** (a few gamescope bugs left) |
| OS X | **Partial**, no window support or input yet |
| Android | **Okay**, close to full; missing render passes and bindful |
| Xbox UWP / iOS | Planned |
| Web | Not for a while |
| GDK Xbox / Playstation / Switch | Not planned |

| ISA | Status |
| --- | ------ |
| x64 | **Decent**, full on Windows; SSE transcendentals (sin/exp/…) not yet efficient elsewhere |
| arm64 | **Okay**, same transcendental caveat |
| scalar fallback | **Full**, used to bring up new platforms before their SIMD backend exists |
| risc-v / wasm | None / not yet |

64-bit CPUs only. The SIMD build requires SSE4.2/AES/PCLMULQDQ/BMI1+2/F16C/AVX/FMA (Intel Haswell 2013 / AMD Zen and up); Intel Gen 11+ / AMD Zen are recommended for hardware SHA256. The SIMD-less build exists for porting, emulation and debugging, it is markedly slower (no AES/SHA/CRC/SIMD intrinsics).

## Deployables

A full build typically ships:

```
D3D12:
	D3D12/*.dll, D3D12/*.pdb
	(debug only) d3d10warp.dll
	(optional) OxC3.exe
	yourExecutable.exe

Vulkan:
	(optional) OxC3 / OxC3.exe
	yourExecutable(.exe/.apk/.ipa/…)

Dynamic linking additionally:
	OxC3_graphics_d3d12.dll (Windows) and/or OxC3_graphics_vk(.dll/.so)
```

The shader compiler needs no extra binaries (DXC is statically linked). `d3d10warp.dll` is testing-only. The `OxC3` CLI is optional at runtime but handy (shader compilation, `OxC3 graphics devices` capability checks, and more). Dynamic linking enables per-API dlls: runtime API switching, side-by-side Vulkan+D3D12 (some extensions are only reachable on one), and drop-in dll updates; static linking gives easier distribution and lower call overhead.

## Dependencies

- **OxC3_types**: none (OS only).
- **OxC3_formats / OxC3_platforms**: OxC3_types only. *(Note: platforms depends on formats for its oiCA-backed virtual file system.)*
- **OxC3_graphics**: Vulkan and/or D3D12 (+ NVAPI, AMD_AGS, AgilitySDK; optional WARP).
- **OxC3_shader_compiler**: DXC (LLVM/clang, DirectX-Headers, SPIRV-Headers/Tools), SPIRV-Reflect; SPIRV-Cross planned for MSL.
- **OxC3(CLI)**: platforms + formats (+ optional shader_compiler, graphics).

## Contributing

See [FOR_CONTRIBUTORS.md](FOR_CONTRIBUTORS.md) (code style: [docs/code_style.md](docs/code_style.md)). External PRs require signing a CLA before merge.

## License

This repository is dual-licensed:

- **GPL3** open source license (see [LICENSE](LICENSE)). Note that GPL3 is a strong copyleft license: distributing a product that links OxC3 (statically or dynamically) requires that product to be GPL3 as well, with source available.
- **Commercial license** — for closed-source use, contact us at contact@osomi.net.
