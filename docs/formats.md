# OxC3 formats (id: 0x1C31)

OxC3 formats reads and writes files: the third party formats an application meets (images, meshes, audio) and
OxC3's own binary formats, which the runtime, the shader compiler and the CLI are built on.

This page is a map, not a reference. The API of a format is its header under `include/formats/<format>/` (plus the
C++ wrappers `bmp.hpp`, `hdr.hpp`, `obj.hpp`, `ply.hpp` and `oiSH.hpp`), and the byte layout of OxC3's own formats is
its spec under [file/](file/). How complete each format is (read, write, encryption, known gaps) is kept in one place,
the [Formats table in STATUS.md](../STATUS.md#formats).

## How the formats behave

- **Streams in, streams out.** The codecs take a stream and produce one, rather than a whole file in memory. A
  reader that has to unpack (HDR's RLE scanlines, a mesh's faces) hands the result on in pieces, so a large file
  is never held whole twice. A convenience wrapper that does materialize a buffer says so.
- **Refused, not repaired.** A file outside what a reader supports fails with an error naming why, rather than
  loading as something close to it.
- **Limits are checked before allocating.** Headers claiming sizes no real file has are rejected up front.

## Third party formats

| Area | Formats | What they are for |
| --- | --- | --- |
| Images | BMP, DDS, HDR | BMP for 8-bit BGRA dumps; DDS for GPU ready textures in the modern DXGI formats, block compressed BC4 to BC7 included; HDR (Radiance RGBE) for environment captures in linear radiance |
| Meshes | OBJ, PLY | Both read into and write out of the flat mesh form in `types/mesh`: a position, a packed normal and a uv per vertex, and a triangle list. PLY is ASCII or binary in either byte order |
| Audio | WAV | Wave audio, its chunks found by walking the file through a stream rather than loading it whole |

A mesh file may carry more per vertex properties than the flat form holds. Those are parsed past and dropped, so
reading and writing a file back gives its geometry, not the file.

## OxC3's own formats

All of them are [oiXX](file/oiXX.md) formats and share its header conventions: a magic number, a version, sized
fields that take the smallest width their values fit, AES-256-GCM encryption where a format allows it, and a
compression field that is reserved but not yet implemented.

| Format | What it holds |
| --- | --- |
| [oiCA](file/oiCA.md) | An archive of files and folders, like a zip. Also what the virtual file system mounts, so an application's resources can ship inside its binary |
| [oiDL](file/oiDL.md) | A list of data entries, strings or buffers. The building block other formats use for their name tables |
| [oiSH](file/oiSH.md) | Compiled shaders: SPIR-V and DXIL binaries per entrypoint, with the reflection a runtime needs to bind and validate them |
| [oiSB](file/oiSB.md) | The layout of a shader buffer (structs, members, offsets), usually embedded in an oiSH |
| [oiSR](file/oiSR.md) | A shader's source level symbols (functions, types, resources, locations), for editor tooling |
| [oiPL](file/oiPL.md) | A pipeline layout without a device: bindings, the push constant range and baked samplers |
| [oiSP](file/oiSP.md) | Pipelines: the state around a shader, recording for each field whether it was proven, supplied or assumed |
| [oiBC](file/oiBC_chimera.md) | Chimera, a compiled intermediate format. Specification only, not implemented |

`gfx_util` holds the graphics vocabulary several of these store (register types, spaces, bindings), so no format
has to include another to name a value.

Texture formats (the channel layout of a texture's texels) are types rather than a file format, see
[types.md](types.md).
