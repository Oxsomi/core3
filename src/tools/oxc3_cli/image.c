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

//tools/oxc3_cli/image.c

//OxC3 file convert, for images: HDR, BMP and uncompressed DDS into each other, formats picked by file extension.
//
//An image is held in one of two forms between reading and writing: linear RGBA32f (HDR, a float DDS) or sRGB encoded
// BGRA8 (BMP, an 8 bit DDS). Going to 8 bit from linear applies an exposure and -tonemap (types/math/tonemap.h);
// going to linear from 8 bit decodes sRGB.
//
//The exposure, in order: -exposure in stops, absolute. Then a .hdr's EXPOSURE header: Radiance's multiplier already
// applied to the samples, so a file that carries one is shown as stored, which is how a renderer that metered its own
// image hands that over. Then automatic: the log average luminance lands on mid grey, the key Reinhard et al. expose
// by, since a .hdr carries no unit and captures range from ones to hundreds of thousands. Metering on pixels alone
// cannot tell a white room from a lit grey one, so the automatic exposure is a sensible preview and no more.
//
//This MATERIALIZES the image, the whole of it in memory, which a command line conversion can afford and a streaming
// consumer should not copy.

#include "tools/oxc3_cli/cli.h"
#include "types/container/stream.h"
#include "types/container/memory_stream.h"
#include "types/container/buffer.h"
#include "types/container/texture_format.h"
#include "types/container/string.h"
#include "types/base/string_read.h"
#include "types/base/string_read_helper.h"
#include "types/base/error.h"
#include "types/base/constants.h"
#include "types/math/tonemap.h"
#include "formats/bmp/bmp_file.h"
#include "formats/hdr/hdr_file.h"
#include "formats/dds/dds_file.h"
#include "platforms/file.h"
#include "platforms/platform.h"
#include "platforms/logx.h"
#include "types/container/log.h"

typedef enum EImageFile {
	EImageFile_HDR,
	EImageFile_BMP,
	EImageFile_DDS,
	EImageFile_Count
} EImageFile;

static const C8 *const EImageFile_extensions[EImageFile_Count] = { ".hdr", ".bmp", ".dds" };

static const C8 *const ETonemap_names[ETonemap_Count] = { "none", "reinhard", "aces", "agx", "neutral" };

static EImageFile CLI_imageFileOf(CharString path) {

	for(U32 i = 0; i < EImageFile_Count; ++i) {

		const CharString extension = CharString_createRefCStrConst(EImageFile_extensions[i]);

		if(CharString_endsWithStringInsensitive(&path, &extension, 0))
			return (EImageFile) i;
	}

	return EImageFile_Count;
}

typedef struct CLIImage {
	U32 w, h;
	Bool isLinear;          //RGBA32f linear, else BGRA8 sRGB encoded
	U8 padding[3];
	F32 fileExposure;       //the multiplier the samples already carry, a .hdr's EXPOSURE header; 1 otherwise
	Buffer pixels;          //top row first
} CLIImage;

//Reads any of the three into a CLIImage. The file is read whole and the codecs stream out of it.

static Bool CLI_imageRead(
	CharString path, EImageFile type, const Allocator *alloc, CLIImage *image, Error *e_rr
) {

	Bool s_uccess = true;
	Buffer file = Buffer_createNull();
	StreamRef *in = NULL, *sink = NULL;
	ListSubResourceData subresources = (ListSubResourceData) { 0 };

	const RefPtrType fileHandleType = FileHandle_makeType(alloc);
	const RefPtrType memType = MemoryStream_makeType(alloc);

	gotoIfError3(clean, File_read(&path, 1 * SECOND, 0, 0, &fileHandleType, &file, e_rr));

	//A memory stream frees the buffer it was made from, so both streams here get a ref over memory held elsewhere.

	const Buffer fileRef = Buffer_createRefConst(file.ptr, Buffer_length(file));

	gotoIfError3(clean, MemoryStream_createFromBufferRegion(
		fileRef, 0, Buffer_length(fileRef), EMemoryStreamFlags_None, &memType, (MemoryStreamRef**) &in, e_rr
	));

	U64 off = 0;

	switch (type) {

		case EImageFile_HDR: {

			HDRInfo info = (HDRInfo) { 0 };

			gotoIfError3(clean, MemoryStream_create(
				0, EMemoryStreamFlags_WriteResize, &memType, (MemoryStreamRef**) &sink, e_rr
			));

			gotoIfError3(clean, HDR_read(in, &off, EHDRReadFlags_None, &info, sink, 0, alloc, e_rr));
			gotoIfError3(clean, MemoryStream_move((MemoryStreamRef**) &sink, &image->pixels, e_rr));

			image->w = info.w;
			image->h = info.h;
			image->isLinear = true;
			image->fileExposure = info.exposure;
			break;
		}

		//Rows are 4 byte aligned, 3 or 4 bytes a pixel, and bottom up when the file says so.

		case EImageFile_BMP: {

			BMPInfo info = (BMPInfo) { 0 };
			U64 dataOffset = 0;

			gotoIfError3(clean, BMP_read(in, &off, &dataOffset, &info, alloc, e_rr));

			const U64 pixelStride = info.discardAlpha ? 3 : 4;
			const U64 stride = ((U64) info.w * pixelStride + 3) &~ (U64) 3;

			gotoIfError3(clean, Buffer_createUninitializedBytes((U64) info.w * info.h * 4, alloc, &image->pixels, e_rr));

			for(U32 y = 0; y < info.h; ++y) {

				const U8 *src = file.ptr + dataOffset + stride * (info.isFlipped ? info.h - 1 - y : y);
				U8 *dst = image->pixels.ptrNonConst + (U64) y * info.w * 4;

				for(U32 x = 0; x < info.w; ++x) {
					dst[x * 4 + 0] = src[x * pixelStride + 0];
					dst[x * 4 + 1] = src[x * pixelStride + 1];
					dst[x * 4 + 2] = src[x * pixelStride + 2];
					dst[x * 4 + 3] = pixelStride == 4 ? src[x * pixelStride + 3] : 255;
				}
			}

			image->w = info.w;
			image->h = info.h;
			image->isLinear = false;
			break;
		}

		//Mip 0 of layer 0 only, in the formats a pixel copy can take: RGBA32f, RGBA8 or BGRA8.

		case EImageFile_DDS: {

			DDSInfo info = (DDSInfo) { 0 };
			gotoIfError3(clean, DDS_read(in, &off, &info, alloc, &subresources, e_rr));

			const ETextureFormatId format = (ETextureFormatId) info.textureFormatId;

			if(format != ETextureFormatId_RGBA32f && format != ETextureFormatId_RGBA8 && format != ETextureFormatId_BGRA8)
				retError(clean, Error_unsupportedOperation(
					0, "CLI_imageRead() DDS has to be RGBA32f, RGBA8 or BGRA8 (block compressed isn't decoded)"
				));

			const SubResourceData *first = NULL;

			for(U64 i = 0; i < subresources.length; ++i)
				if(!subresources.ptr[i].mipId && !subresources.ptr[i].layerId && !subresources.ptr[i].z)
					first = &subresources.ptr[i];

			const U64 texel = format == ETextureFormatId_RGBA32f ? 16 : 4;

			if(!first || first->streamLen < (U64) info.w * info.h * texel)
				retError(clean, Error_invalidState(0, "CLI_imageRead() DDS has no complete first subresource"));

			gotoIfError3(clean, Buffer_createUninitializedBytes((U64) info.w * info.h * texel, alloc, &image->pixels, e_rr));
			OxStream *stream = RefPtr_data(first->stream, OxStream);

			gotoIfError3(clean, stream->read(
				stream, first->streamOff, Buffer_length(image->pixels), image->pixels, alloc, e_rr
			));

			//RGBA8 swaps into the BGRA8 the 8 bit form holds.

			if(format == ETextureFormatId_RGBA8)
				for(U64 i = 0; i < Buffer_length(image->pixels); i += 4) {
					const U8 r = image->pixels.ptrNonConst[i];
					image->pixels.ptrNonConst[i] = image->pixels.ptrNonConst[i + 2];
					image->pixels.ptrNonConst[i + 2] = r;
				}

			image->w = info.w;
			image->h = info.h;
			image->isLinear = format == ETextureFormatId_RGBA32f;
			break;
		}

		default:
			retError(clean, Error_invalidParameter(0, 0, "CLI_imageRead() unsupported input file type"));
	}

clean:
	ListSubResourceData_freeUnderlying(&subresources, alloc);
	RefPtr_dec(&sink);
	RefPtr_dec(&in);
	Buffer_free(&file, alloc);
	return s_uccess;
}

//The exposure that puts a linear image's log average luminance on mid grey, in stops. Texels that are black or not
// finite carry no key and are skipped; an image with none is left at 0 stops.

static F64 CLI_imageAutoStops(const CLIImage *image) {

	const U64 texels = (U64) image->w * image->h;

	F64 logSum = 0;
	U64 counted = 0;

	for(U64 i = 0; i < texels; ++i) {

		F32 v[4];
		Buffer_memcpy(Buffer_createRef(v, 16), Buffer_createRefConst(image->pixels.ptr + i * 16, 16));

		const F64 lum = 0.2126 * v[0] + 0.7152 * v[1] + 0.0722 * v[2];

		if(lum > 0 && lum < 1e30) {
			logSum += F64_log2(lum);
			++counted;
		}
	}

	return counted ? F64_log2(0.18) - logSum / (F64) counted : 0;
}

//Brings an image to the form the output needs: linear by decoding sRGB, or 8 bit through exposure and the tonemap.

static Bool CLI_imageToForm(
	CLIImage *image, Bool linear, F32 exposure, ETonemap tonemap, const Allocator *alloc, Error *e_rr
) {

	Bool s_uccess = true;
	Buffer converted = Buffer_createNull();

	if(image->isLinear == linear)
		goto clean;

	const U64 texels = (U64) image->w * image->h;

	gotoIfError3(clean, Buffer_createUninitializedBytes(texels * (linear ? 16 : 4), alloc, &converted, e_rr));

	for(U64 i = 0; i < texels; ++i) {

		if(linear) {

			const U8 *src = image->pixels.ptr + i * 4;

			const F32x4 v = F32x4_srgbDecode(F32x4_create4(
				src[2] / 255.f, src[1] / 255.f, src[0] / 255.f, src[3] / 255.f
			));

			Buffer_memcpy(Buffer_createRef(converted.ptrNonConst + i * 16, 16), Buffer_createRefConst(&v, 16));
		}

		else {

			F32 src[4];
			Buffer_memcpy(Buffer_createRef(src, 16), Buffer_createRefConst(image->pixels.ptr + i * 16, 16));

			const F32x4 v = F32x4_tonemap(
				F32x4_create4(src[0] * exposure, src[1] * exposure, src[2] * exposure, F32_clamp(src[3], 0, 1)), tonemap
			);

			U8 *dst = converted.ptrNonConst + i * 4;
			dst[0] = (U8) (F32x4_z(v) * 255 + 0.5f);
			dst[1] = (U8) (F32x4_y(v) * 255 + 0.5f);
			dst[2] = (U8) (F32x4_x(v) * 255 + 0.5f);
			dst[3] = (U8) (F32x4_w(v) * 255 + 0.5f);
		}
	}

	Buffer_free(&image->pixels, alloc);
	image->pixels = converted;
	image->isLinear = linear;
	converted = Buffer_createNull();

clean:
	Buffer_free(&converted, alloc);
	return s_uccess;
}

static Bool CLI_imageWrite(CharString path, EImageFile type, const CLIImage *image, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;
	StreamRef *in = NULL, *out = NULL;
	ListSubResourceData subresources = (ListSubResourceData) { 0 };

	const RefPtrType fileHandleType = FileHandle_makeType(alloc);
	const RefPtrType streamType = FileStream_makeType(alloc);
	const RefPtrType memType = MemoryStream_makeType(alloc);

	const Buffer pixelsRef = Buffer_createRefConst(image->pixels.ptr, Buffer_length(image->pixels));

	gotoIfError3(clean, MemoryStream_createFromBufferRegion(
		pixelsRef, 0, Buffer_length(pixelsRef), EMemoryStreamFlags_None, &memType, (MemoryStreamRef**) &in, e_rr
	));

	gotoIfError3(clean, File_openStream(
		&path, 1 * SECOND, EFileOpenType_Write, true, &fileHandleType, &streamType, &out, e_rr
	));

	U64 off = 0;

	switch (type) {

		//The exposure the samples carry goes with them, so it survives a trip through a DDS and back.

		case EImageFile_HDR:
			gotoIfError3(clean, HDR_writeExposed(
				out, &off, EHDRWriteFlags_None, image->w, image->h, image->fileExposure, alloc, in, 0, e_rr
			));
			break;

		case EImageFile_BMP: {

			const BMPInfo info = (BMPInfo) { .w = image->w, .h = image->h, .textureFormatId = ETextureFormatId_BGRA8 };
			gotoIfError3(clean, BMP_write(out, &off, &info, alloc, in, 0, false, e_rr));
			break;
		}

		case EImageFile_DDS: {

			const DDSInfo info = (DDSInfo) {
				.w = image->w, .h = image->h, .l = 1, .mips = 1, .layers = 1,
				.textureFormatId = image->isLinear ? ETextureFormatId_RGBA32f : ETextureFormatId_BGRA8,
				.type = ETextureType_2D
			};

			const SubResourceData first = (SubResourceData) {
				.stream = in, .streamOff = 0, .streamLen = Buffer_length(image->pixels)
			};

			gotoIfError3(clean, ListSubResourceData_pushBack(&subresources, first, alloc, e_rr));
			gotoIfError3(clean, DDS_write(out, &off, &subresources, &info, alloc, e_rr));
			break;
		}

		default:
			retError(clean, Error_invalidParameter(0, 0, "CLI_imageWrite() unsupported output file type"));
	}

clean:
	ListSubResourceData_free(&subresources, alloc);
	RefPtr_dec(&out);
	RefPtr_dec(&in);
	return s_uccess;
}

Bool CLI_imageConvert(const ParsedArgs *args) {

	if(!args)
		return false;

	const Allocator *alloc = Platform_instance->alloc;

	CharString input = CharString_createNull(), output = CharString_createNull();

	if(
		!ParsedArgs_getArg(args, EOperationHasParameter_InputShift, &input, NULL) ||
		!ParsedArgs_getArg(args, EOperationHasParameter_OutputShift, &output, NULL)
	) {
		Log_errorLnx("CLI_imageConvert() file convert needs -input and -output");
		return false;
	}

	const EImageFile from = CLI_imageFileOf(input), to = CLI_imageFileOf(output);

	if(from == EImageFile_Count || to == EImageFile_Count) {
		Log_errorLnx("CLI_imageConvert() -input and -output have to end in .hdr, .bmp or .dds");
		return false;
	}

	//PBR Neutral by default: it leaves base colors below its compression start where they were authored.

	ETonemap tonemap = ETonemap_PBRNeutral;

	if (args->parameters & EOperationHasParameter_Tonemap) {

		CharString name = CharString_createNull();
		ParsedArgs_getArg(args, EOperationHasParameter_TonemapShift, &name, NULL);

		tonemap = ETonemap_Count;

		for(U32 i = 0; i < ETonemap_Count; ++i)
			if(CharString_equalsCStringInsensitive(&name, ETonemap_names[i]))
				tonemap = (ETonemap) i;

		if(tonemap == ETonemap_Count) {
			Log_errorLnx("CLI_imageConvert() -tonemap has to be none, reinhard, aces, agx or neutral");
			return false;
		}
	}

	F64 stops = 0;
	const Bool autoExposure = !(args->parameters & EOperationHasParameter_Exposure);

	if (!autoExposure) {

		CharString value = CharString_createNull();
		ParsedArgs_getArg(args, EOperationHasParameter_ExposureShift, &value, NULL);

		if(!CharString_parseDouble(value, &stops)) {
			Log_errorLnx("CLI_imageConvert() -exposure has to be a number of stops, e.g. -1.5");
			return false;
		}
	}

	CLIImage image = (CLIImage) { .fileExposure = 1 };
	Error err = Error_none();
	Bool s_uccess = true;

	gotoIfError3(clean, CLI_imageRead(input, from, alloc, &image, &err));

	//HDR is linear and BMP 8 bit; a DDS keeps the form it was read in, float as float and 8 bit as 8 bit.

	const Bool linear = to == EImageFile_HDR || (to == EImageFile_DDS && image.isLinear);

	if(autoExposure && image.isLinear && !linear) {

		if(image.fileExposure != 1)
			Log_debugLnx(
				"CLI_imageConvert() exposure from the file (EXPOSURE=%g already applied), shown as stored",
				(F64) image.fileExposure
			);

		else {
			stops = CLI_imageAutoStops(&image);
			Log_debugLnx("CLI_imageConvert() exposure %.3f stops, automatic (pass -exposure to set it)", stops);
		}
	}

	gotoIfError3(clean, CLI_imageToForm(&image, linear, (F32) F64_exp2(stops), tonemap, alloc, &err));

	gotoIfError3(clean, CLI_imageWrite(output, to, &image, alloc, &err));

	Log_debugLnx(
		"CLI_imageConvert() wrote %.*s, %ux%u (%s)", (int) CharString_length(output), output.ptr,
		(unsigned) image.w, (unsigned) image.h, image.isLinear ? "linear" : ETonemap_names[tonemap]
	);

clean:

	if(!s_uccess)
		Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);

	Buffer_free(&image.pixels, alloc);
	return s_uccess;
}
