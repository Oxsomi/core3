/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2026 Oxsomi / Nielsbishere (Niels Brunekreef)
*
*  This program is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, either version 3 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program. If not, see https://github.com/Oxsomi/core3/blob/main/LICENSE.
*  Be aware that GPL3 requires closed source products to be GPL3 too if released to the public.
*  To prevent this a separate license will have to be requested at contact@osomi.net for a premium;
*  This is called dual licensing.
*/

//formats/hdr/hdr_file.h

#pragma once
#include "types/base/types.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct Allocator Allocator;
typedef struct Error Error;
typedef struct Buffer Buffer;
typedef struct RefPtr RefPtr;
typedef RefPtr StreamRef;

//Refuses a header claiming a dimension no real capture has. It bounds each dimension rather than their product,
// so it rejects NONSENSE and not merely huge: a header at the limit still describes an image no allocation can satisfy,
// and the allocation failing is what actually stops that one.
//
//Shared by both directions, but it is not what governs interop. That is the 8 to 32767 width range adaptive RLE is legal
// in, which both sides fall out of into flat scanlines rather than exceed.

#define HDR_MAX_DIM 65536

typedef struct HDRInfo {

	U32 w, h;

	//ETextureFormatId of what came back: RGBA32f decoded, RGBA8u under EHDRReadFlags_KeepRGBE.
	//
	//Decoded is RGBA32f and not something narrower because RGBE's shared exponent reaches far past a half,
	// which clips at 65504, and a capture with a properly exposed sun exceeds that by orders of magnitude.
	//Scaling to fit is the CONSUMER's choice, not the reader's, so nothing here narrows it for them.

	U8 textureFormatId;
	U8 padding[3];

	//The EXPOSURE header's cumulative value, 1 when absent. Radiance defines it as a value already APPLIED to the samples,
	// so a reader recovering absolute radiance divides by it; left to the caller since most consumers don't care.

	F32 exposure;

} HDRInfo;

typedef enum EHDRReadFlags {

	EHDRReadFlags_None        = 0,

	//Hand back the RGBE plane as it sits in the file, 4 bytes per texel, rather than decoding it.
	//
	//Decoding QUADRUPLES the result: a 4k lat-long is 33 MiB of RGBE against 134 MiB of RGBA32f,
	// and a 16k one is 537 MiB against 2.1 GiB, which the consumer then pays again for whatever it converts into.
	//A consumer that is going to upload this to a GPU anyway can take the plane, decode in a compute shader,
	// and never materialize the float form at all.
	//
	//Decoding it later costs one multiply per channel, so little is given up by taking the plane:
	//the value is (mantissa + 0.5) * 2^(exponent - 136), and exponent byte 0 is an exact zero, not a denormal.
	//That scale is a power of two, so it is built from the exponent field rather than asked of exp2.
	//Neither side of this codec keeps a table for it.

	EHDRReadFlags_KeepRGBE    = 1 << 0

} EHDRReadFlags;

//Radiance RGBE (.hdr), the format every HDRI library ships.
//
//Unlike BMP this cannot hand back an offset into the stream: the scanlines are RLE compressed,
// so the pixels have to be unpacked rather than pointed at. They are written to outputStream a SCANLINE at a time,
// row 0 first, so nothing here holds more than one row and a consumer taking the image in bands never holds the whole of
// it. Rows are w * 4 F32s, or w * 4 U8s under EHDRReadFlags_KeepRGBE.
//
//The output size is known before the first row, so a resizable sink is reserved once and ends up exactly the right length.
//A caller that wants it as a Buffer creates a resizable MemoryStream and moves the buffer out of it (MemoryStream_move),
// which is what oxc::hdr::read does.
//
//Only the 32-bit_rle_rgbe format is accepted. XYZE is refused rather than silently read as RGB,
// since the two are indistinguishable once decoded and a wrong colour space is worse than a failed load.

//Parses only the header, leaving *off at the first byte of the first scanline.
//
//This is what a consumer that streams the pixel data somewhere else needs. It learns the dimensions and where the data
// begins without decoding any of it, which is the shape a DirectStorage style path wants: the file goes to the GPU and
// the unpacking happens there.
//
//Scanlines are variable length, so knowing where the DATA starts is not the same as knowing where any particular
// scanline starts. Decoding them in parallel needs an offset table, built by one serial pass that reads the run counts
// without writing pixels, which is far cheaper than a decode and is the piece such a path would add.

Bool HDR_readHeader(
	StreamRef *stream,
	U64 *off,
	EHDRReadFlags flags,       //Only decides which format id is reported
	HDRInfo *info,
	const Allocator *alloc,
	Error *e_rr
);

Bool HDR_read(
	StreamRef *stream,
	U64 *off,
	EHDRReadFlags flags,
	HDRInfo *info,
	StreamRef *outputStream,   //Receives h rows of w * 4 F32s, or of U8s under KeepRGBE
	U64 outputOffset,
	const Allocator *alloc,
	Error *e_rr
);

typedef enum EHDRWriteFlags {

	EHDRWriteFlags_None            = 0,

	//The source is already the RGBE plane, 4 bytes per texel, rather than RGBA32f.
	//
	//The mirror of EHDRReadFlags_KeepRGBE, and the reason it exists: a producer that can encode RGBE itself,
	// a compute shader for instance, hands the bytes over untouched. That is a quarter of the readback,
	// and neither side re-encodes what the other already encoded.

	EHDRWriteFlags_SourceIsRGBE    = 1 << 0

} EHDRWriteFlags;

//Encodes as Radiance RGBE, row 0 at the TOP. The source is RGBA32f,
// or the RGBE plane itself under EHDRWriteFlags_SourceIsRGBE, in which case nothing is encoded and the bytes pass straight
// through.
//
//Streams both ways like BMP_write: one scanline in, one encoded scanline out. Scratch is a few rows rather than the image,
// roughly 96 KiB at 4k wide and a third of that when the source is already RGBE,
// against the 134 MiB the decoded image would occupy.
//
//The output is 32-bit_rle_rgbe with the -Y +X orientation, which is what HDR_read accepts,
// so anything written here round trips. Scanlines are adaptively RLE'd where the width allows and flat otherwise,
// since the adaptive marker is only distinguishable from pixel data inside that range.
//
//Alpha is DROPPED: Radiance carries three channels over a shared exponent and has nowhere to put it.

Bool HDR_write(
	StreamRef *stream,
	U64 *off,
	EHDRWriteFlags flags,
	U32 w,
	U32 h,
	const Allocator *alloc,
	StreamRef *inputStream,    //w * h * 4 F32s from inputOffset, or U8s under SourceIsRGBE
	U64 inputOffset,
	Error *e_rr
);

#ifdef __cplusplus
	}
#endif
