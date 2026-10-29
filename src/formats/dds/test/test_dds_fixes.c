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

//formats/dds/test/test_dds_fixes.c

#include "test_dds_shared.h"
#include "formats/dds/dds_file.h"
#include "formats/dds/dds_headers.h"
#include "types/container/memory_stream.h"
#include "types/container/buffer.h"
#include "types/container/test/basic_alloc.h"
#include "types/container/test/failing_stream.h"
#include "types/base/mathi.h"

Bool buildSubResources(
	Test *t,
	U32 w, U32 h, U32 depth,
	U32 mips, U32 layers,
	ETextureFormatId fmtId,
	StreamRef **sharedStream,
	ListSubResourceData *subs,
	const RefPtrType *type
);

//The stream type has to outlive every stream made from it
static RefPtrType streamTypeStorage;

static const RefPtrType *streamType(Test *t) {
	streamTypeStorage = MemoryStream_makeType(t->alloc);
	return &streamTypeStorage;
}

//Allocator that refuses anything big and remembers the biggest request, so a regression that allocates from a header
// count fails cleanly instead of taking the machine down.

#define CAP_LIMIT (16 * 1024 * 1024)

static U64 capLargest;

static Bool capAlloc(void *allocator, U64 length, Buffer *output, Error *e_rr) {

	Bool s_uccess = true;

	(void) allocator;

	if(length > capLargest)
		capLargest = length;

	if(length > CAP_LIMIT)
		retError(clean, Error_outOfMemory(0, "capAlloc() over the cap"));

	gotoIfError3(clean, BasicAllocator_instance.alloc(BasicAllocator_instance.ptr, length, output, e_rr));

clean:
	return s_uccess;
}

static void capFree(void *allocator, Buffer buf) {
	(void) allocator;
	BasicAllocator_instance.free(BasicAllocator_instance.ptr, buf);
}

static const Allocator capAllocator = { .alloc = capAlloc, .free = capFree };

//Build a header; the data (if any) follows it

static DDSHeaderMax makeHeader(U32 w, U32 h, U32 mips, EDDSFlag flags) {

	DDSHeaderMax d = (DDSHeaderMax) { 0 };

	d.header.magicNumber = ddsMagic;
	d.header.size = 124;
	d.header.flags = EDDSFlag_Default | flags;
	d.header.width = w;
	d.header.height = h;
	d.header.mips = mips;
	d.header.format.size = 32;
	d.header.format.flags = EDDSPixelFormatFlag_RGB | EDDSPixelFormatFlag_AlphaPixels;
	d.header.format.rgbBitCount = 32;
	d.header.format.masks[0] = 0xFF;
	d.header.format.masks[1] = 0xFF00;
	d.header.format.masks[2] = 0xFF0000;
	d.header.format.masks[3] = 0xFF000000;
	d.header.caps.flag1 = EDDSCapsFlags1_Texture;
	return d;
}

static void makeDX10(DDSHeaderMax *d, DXFormat format, EDX10Dim dim, U32 arraySize, EDX10Misc misc) {
	d->header.format.flags = EDDSPixelFormatFlag_MagicNumber;
	d->header.format.magicNumber = EDDSFormatMagic_DX10;
	d->header.format.rgbBitCount = 0;
	d->header.format.masks[0] = d->header.format.masks[1] = d->header.format.masks[2] = d->header.format.masks[3] = 0;
	d->header10 = (DDSHeaderDXT10) { .format = format, .dim = dim, .miscFlag = misc, .arraySize = arraySize };
}

static Bool makeStream(Test *t, const void *bytes, U64 size, StreamRef **sr) {

	if (!MemoryStream_create(size, EMemoryStreamFlags_IsWritable, streamType(t), sr, &t->err))
		return false;

	OxStream *s = RefPtr_data(*sr, OxStream);

	return s->write(s, 0, size, Buffer_createRefConst(bytes, size), t->alloc, &t->err);
}

//Reads the header (plus the data bytes, if given) at an offset; returns the result and the sub resource list is freed
static Bool readBytes(
	Test *t, const void *bytes, U64 size, U64 off, const Allocator *alloc, DDSInfo *info, ListSubResourceData *res, U64 *outOff
) {

	StreamRef *sr = NULL;

	if (!makeStream(t, bytes, size, &sr))
		return false;

	U64 streamOff = off;
	Bool ok = DDS_read(sr, &streamOff, info, alloc, res, NULL);

	if(outOff)
		*outOff = streamOff;

	RefPtr_dec(&sr);
	return ok;
}

static Bool readHeaderOnly(Test *t, const DDSHeaderMax *d, U64 size, const Allocator *alloc, U64 *outOff) {

	U8 file[512] = { 0 };

	Buffer_memcpy(Buffer_createRef(file, sizeof(*d)), Buffer_createRefConst(d, sizeof(*d)));

	DDSInfo info = (DDSInfo) { 0 };
	ListSubResourceData res = (ListSubResourceData) { 0 };
	Bool ok = readBytes(t, file, size, 0, alloc, &info, &res, outOff);

	if(res.ptr)
		ListSubResourceData_freeUnderlying(&res, alloc);

	return ok;
}

void Test_DDSLegacyVariants(Test *t) {

	Test_setModule(t, "DDS read: zero mips and legacy BGRA8");

	for(U32 bgra = 0; bgra < 2; ++bgra)
		for(U32 mipCount = 0; mipCount < 2; ++mipCount) {

			DDSHeaderMax d = makeHeader(1, 1, mipCount, 0);

			if(bgra) {
				d.header.format.masks[0] = 0xFF0000;
				d.header.format.masks[2] = 0xFF;
			}

			U8 file[19 + sizeof(DDSHeader) + 4] = { 0 };

			Buffer_memcpy(Buffer_createRef(file + 19, sizeof(d.header)), Buffer_createRefConst(&d.header, sizeof(d.header)));

			DDSInfo info = (DDSInfo) { 0 };
			ListSubResourceData res = (ListSubResourceData) { 0 };
			U64 end = 0;
			Bool ok = readBytes(t, file, sizeof(file), 19, t->alloc, &info, &res, &end);

			Test_assert(t, "legacy file accepted", ok);

			if(ok) {

				Test_assert(
					t, "one mip and correct channel order",
					info.mips == 1 && info.textureFormatId == (bgra ? ETextureFormatId_BGRA8 : ETextureFormatId_RGBA8)
				);

				Test_assert(t, "embedded payload offset", res.length == 1 && res.ptr[0].streamOff == 19 + sizeof(DDSHeader));
				Test_assert(t, "file consumed", end == sizeof(file));
			}

			ListSubResourceData_freeUnderlying(&res, t->alloc);
		}
}

//Sizes that wrap or are way bigger than the file used to be accepted (with zero length subresources) or ask for
// gigabytes for the subresource list.

void Test_DDSReadHugeSizes(Test *t) {

	Test_setModule(t, "DDS_read: huge sizes are rejected without allocating");

	U64 outOff = 0;
	const U64 fileSize = sizeof(DDSHeader);        //Only the header: no data at all

	//2^31 x 2^31 RGBA8: 2^65 bytes, which wrapped to a zero length subresource

	DDSHeaderMax d = makeHeader(1U << 31, 1U << 31, 1, 0);

	capLargest = 0;
	Test_assert(t, "2^31 x 2^31", !readHeaderOnly(t, &d, fileSize, &capAllocator, &outOff));
	Test_assert(t, "2^31 x 2^31 alloc", capLargest < 4096);
	Test_assert(t, "off unchanged on failure", outOff == 0);

	d = makeHeader(U32_MAX, U32_MAX, 1, 0);
	capLargest = 0;
	Test_assert(t, "2^32-1 square", !readHeaderOnly(t, &d, fileSize, &capAllocator, NULL));
	Test_assert(t, "2^32-1 square alloc", capLargest < 4096);

	//2^16 x 2^16 x 2^30 x 4 B = 2^64, which wrapped to 0 bytes and asked for 2^30 subresources

	d = makeHeader(1U << 16, 1U << 16, 1, EDDSFlag_Depth);
	d.header.depth = 1U << 30;
	d.header.caps.flag2 = EDDSCapsFlags2_Volume;
	d.header.caps.flag1 |= EDDSCapsFlags1_Complex;
	capLargest = 0;
	Test_assert(t, "volume wraps to 0", !readHeaderOnly(t, &d, fileSize, &capAllocator, NULL));
	Test_assert(t, "volume wraps alloc", capLargest < 4096);

	//Depth 2^32-1: 4 billion subresources from a 128 byte file

	d = makeHeader(1, 1, 1, EDDSFlag_Depth);
	d.header.depth = U32_MAX;
	d.header.caps.flag2 = EDDSCapsFlags2_Volume;
	d.header.caps.flag1 |= EDDSCapsFlags1_Complex;
	capLargest = 0;
	Test_assert(t, "volume 2^32-1 slices", !readHeaderOnly(t, &d, fileSize, &capAllocator, NULL));
	Test_assert(t, "volume 2^32-1 slices alloc", capLargest < 4096);

	//Enough bytes for the size but not for the subresource count cannot happen (each takes a byte), but a count
	// that fits the bytes is fine: 1x1x16 RGBA8 volume in exactly 64 bytes

	U8 file[sizeof(DDSHeader) + 64] = { 0 };

	d = makeHeader(1, 1, 1, EDDSFlag_Depth);
	d.header.depth = 16;
	d.header.caps.flag2 = EDDSCapsFlags2_Volume;
	d.header.caps.flag1 |= EDDSCapsFlags1_Complex;

	Buffer_memcpy(Buffer_createRef(file, sizeof(DDSHeader)), Buffer_createRefConst(&d, sizeof(DDSHeader)));

	DDSInfo info = (DDSInfo) { 0 };
	ListSubResourceData res = (ListSubResourceData) { 0 };

	Test_assert(t, "small volume fits", readBytes(t, file, sizeof(file), 0, t->alloc, &info, &res, &outOff));
	Test_assert(t, "small volume slices", res.length == 16 && info.l == 16 && outOff == sizeof(file));
	ListSubResourceData_freeUnderlying(&res, t->alloc);

	//DX10 array size: 2^16 x 2^16 RGBA8 is 2^34 bytes, times 2^30 layers wraps to 0

	d = makeHeader(1U << 16, 1U << 16, 1, 0);
	makeDX10(&d, ETextureFormatId_toDXFormat(ETextureFormatId_RGBA8), EDX10Dim_2D, 1U << 30, 0);
	capLargest = 0;
	Test_assert(t, "array wraps to 0", !readHeaderOnly(t, &d, sizeof(DDSHeaderMax), &capAllocator, &outOff));
	Test_assert(t, "array wraps alloc", capLargest < 4096);

	//Mips of a huge texture: the sum of the mips can't wrap either (every mip is checked)

	d = makeHeader(U32_MAX, U32_MAX, 32, EDDSFlag_Mips);
	capLargest = 0;
	Test_assert(t, "huge mip chain", !readHeaderOnly(t, &d, fileSize, &capAllocator, NULL));
	Test_assert(t, "huge mip chain alloc", capLargest < 4096);
}

//DXT10 cube arrays: arraySize counts cubes, so N cubes are 6N layers and none are dropped

void Test_DDSCubeArray(Test *t) {

	Test_setModule(t, "DDS: cube arrays");

	const ETextureFormatId fmtId = ETextureFormatId_RGBA8;
	const RefPtrType *type = streamType(t);

	for (U32 cubes = 1; cubes <= 3; ++cubes) {

		const U32 layers = cubes * 6;

		StreamRef *src = NULL, *dst = NULL;
		ListSubResourceData subs = (ListSubResourceData) { 0 }, result = (ListSubResourceData) { 0 };
		DDSInfo info = {
			.w = 4, .h = 4, .l = 1, .mips = 2, .layers = layers, .type = ETextureType_Cube, .textureFormatId = fmtId
		};

		DDSInfo readInfo = (DDSInfo) { 0 };

		if (!Test_assert(t, "build", buildSubResources(t, 4, 4, 1, 2, layers, fmtId, &src, &subs, type))) {

			ListSubResourceData_freeUnderlying(&subs, t->alloc);
			RefPtr_dec(&src);
			continue;
		}

		Test_assert(t, "create out", MemoryStream_create(0, EMemoryStreamFlags_WriteResize, type, &dst, &t->err));

		U64 off = 0;

		if (!Test_assert(t, "write", DDS_write(dst, &off, &subs, &info, t->alloc, &t->err))) {

			ListSubResourceData_freeUnderlying(&subs, t->alloc);
			RefPtr_dec(&dst);
			RefPtr_dec(&src);
			continue;
		}

		//The raw header says N cubes (the spec's meaning), not 6N

		const U8 *bytes = RefPtr_data(dst, MemoryStream)->data.ptr;
		U32 arraySize = 0;

		for (U32 i = 0; i < 4; ++i)
			arraySize |= (U32) bytes[sizeof(DDSHeader) + 12 + i] << (i << 3);

		Test_assert(t, "header arraySize is the cube count", arraySize == cubes);

		U64 readOff = 0;

		Test_assert(t, "read", DDS_read(dst, &readOff, &readInfo, t->alloc, &result, &t->err));
		Test_assert(t, "type", readInfo.type == ETextureType_Cube);
		Test_assert(t, "layers", readInfo.layers == layers);
		Test_assert(t, "sub count", result.length == (U64) layers * 2);
		Test_assert(t, "read stops at the end", readOff == off);

		Bool contentOk = result.length == (U64) layers * 2;

		for (U64 i = 0; contentOk && i < result.length; ++i) {
			const SubResourceData *s = &result.ptr[i];
			contentOk = s->layerId == i / 2 && s->mipId == (i & 1) && s->streamLen == (s->mipId ? 16 : 64);
		}

		Test_assert(t, "layer and mip ids", contentOk);

		ListSubResourceData_freeUnderlying(&result, t->alloc);

		//arraySize 6 means six cubes, so one cube's data under it is a truncated file, never read as one cube

		if (cubes == 1) {

			U8 *raw = RefPtr_data(dst, MemoryStream)->data.ptrNonConst;

			raw[sizeof(DDSHeader) + 12] = 6;

			readInfo = (DDSInfo) { 0 };
			readOff = 0;

			Test_assert(t, "face count as arraySize refused", !DDS_read(dst, &readOff, &readInfo, t->alloc, &result, NULL));
			Test_assert(t, "face count as arraySize off unchanged", readOff == 0 && !result.length);
			ListSubResourceData_freeUnderlying(&result, t->alloc);
		}

		ListSubResourceData_freeUnderlying(&subs, t->alloc);
		RefPtr_dec(&dst);
		RefPtr_dec(&src);
	}

	//Six cubes are written as arraySize 6 and read back as all 36 faces

	{

		StreamRef *src = NULL, *dst = NULL;
		ListSubResourceData subs = (ListSubResourceData) { 0 }, result = (ListSubResourceData) { 0 };
		DDSInfo info = { .w = 4, .h = 4, .l = 1, .mips = 1, .layers = 36, .type = ETextureType_Cube, .textureFormatId = fmtId };
		DDSInfo readInfo = (DDSInfo) { 0 };
		U64 off = 0, readOff = 0;

		if (
			Test_assert(t, "six cubes build", buildSubResources(t, 4, 4, 1, 1, 36, fmtId, &src, &subs, type)) &&
			Test_assert(t, "six cubes out", MemoryStream_create(0, EMemoryStreamFlags_WriteResize, type, &dst, &t->err)) &&
			Test_assert(t, "six cubes write", DDS_write(dst, &off, &subs, &info, t->alloc, &t->err))
		) {
			Test_assert(t, "six cubes read", DDS_read(dst, &readOff, &readInfo, t->alloc, &result, &t->err));
			Test_assert(t, "six cubes are six cubes", readInfo.layers == 36 && result.length == 36 && readOff == off);
		}

		ListSubResourceData_freeUnderlying(&result, t->alloc);
		ListSubResourceData_freeUnderlying(&subs, t->alloc);
		RefPtr_dec(&dst);
		RefPtr_dec(&src);
	}

	//A face count that isn't a whole number of cubes is refused by the writer

	{
		StreamRef *src = NULL, *dst = NULL;
		ListSubResourceData subs = (ListSubResourceData) { 0 };
		DDSInfo info = { .w = 4, .h = 4, .l = 1, .mips = 1, .layers = 7, .type = ETextureType_Cube, .textureFormatId = fmtId };

		if (Test_assert(t, "build 7", buildSubResources(t, 4, 4, 1, 1, 7, fmtId, &src, &subs, type))) {

			Test_assert(t, "create out", MemoryStream_create(0, EMemoryStreamFlags_WriteResize, type, &dst, &t->err));

			U64 off = 0;

			Test_assert(t, "7 faces refused", !DDS_write(dst, &off, &subs, &info, t->alloc, NULL));
		}

		ListSubResourceData_freeUnderlying(&subs, t->alloc);
		RefPtr_dec(&dst);
		RefPtr_dec(&src);
	}

	//A cube array that is too big for 6N layers

	DDSHeaderMax d = makeHeader(1, 1, 1, 0);

	d.header.caps.flag1 |= EDDSCapsFlags1_Complex;
	d.header.caps.flag2 = EDDSCapsFlags2_Cubemap;
	makeDX10(&d, ETextureFormatId_toDXFormat(ETextureFormatId_RGBA8), EDX10Dim_2D, U32_MAX / 6 + 1, EDX10Misc_IsCube);
	Test_assert(t, "cube count overflow", !readHeaderOnly(t, &d, sizeof(DDSHeaderMax), &capAllocator, NULL));
}

//The DXT10 header is looked for at the stream offset, not at the start of the stream

void Test_DDSReadAtOffset(Test *t) {

	Test_setModule(t, "DDS_read: DXT10 detection at a stream offset");

	//A legacy 1x1 RGBA8 file is 132 bytes: at offset 30 the stream is 162 bytes, over a DXT10 header's 148,
	// but only 132 are left of the file

	U8 file[30 + sizeof(DDSHeader) + 4] = { 0 };
	DDSHeaderMax d = makeHeader(1, 1, 1, 0);

	Buffer_memcpy(Buffer_createRef(file + 30, sizeof(DDSHeader)), Buffer_createRefConst(&d, sizeof(DDSHeader)));

	file[30 + sizeof(DDSHeader)] = 0xAB;

	DDSInfo info = (DDSInfo) { 0 };
	ListSubResourceData res = (ListSubResourceData) { 0 };
	U64 outOff = 0;

	Test_assert(t, "legacy at offset 30", readBytes(t, file, sizeof(file), 30, t->alloc, &info, &res, &outOff));
	Test_assert(t, "offset after", outOff == sizeof(file));
	Test_assert(t, "subresource offset", res.length == 1 && res.ptr[0].streamOff == 30 + sizeof(DDSHeader));
	ListSubResourceData_freeUnderlying(&res, t->alloc);

	//Less than a header left is an error and the offset stays

	DDSInfo info2 = (DDSInfo) { 0 };

	Test_assert(t, "short header", !readBytes(t, file, sizeof(file) - 8, 30, t->alloc, &info2, &res, &outOff));
	Test_assert(t, "short header offset unchanged", outOff == 30);
	Test_assert(t, "offset past the end", !readBytes(t, file, sizeof(file), sizeof(file) + 1, t->alloc, &info2, &res, &outOff));

	//A DXT10 file at an offset

	U8 file10[24 + sizeof(DDSHeaderMax) + 1] = { 0 };

	d = makeHeader(1, 1, 1, 0);
	makeDX10(&d, ETextureFormatId_toDXFormat(ETextureFormatId_R8), EDX10Dim_2D, 1, 0);

	Buffer_memcpy(Buffer_createRef(file10 + 24, sizeof(DDSHeaderMax)), Buffer_createRefConst(&d, sizeof(DDSHeaderMax)));

	Test_assert(t, "dxt10 at offset 24", readBytes(t, file10, sizeof(file10), 24, t->alloc, &info, &res, &outOff));
	Test_assert(t, "dxt10 format", info.textureFormatId == ETextureFormatId_R8);
	Test_assert(t, "dxt10 offset after", outOff == sizeof(file10));
	ListSubResourceData_freeUnderlying(&res, t->alloc);

	//A failing read leaves the offset where it was (a truncated body fails after the header was parsed)

	Test_assert(t, "truncated body", !readBytes(t, file10, sizeof(file10) - 1, 24, t->alloc, &info2, &res, &outOff));
	Test_assert(t, "truncated body offset unchanged", outOff == 24);
}

//Legacy masks: every channel has to match, not just the first few

void Test_DDSLegacyMasks(Test *t) {

	Test_setModule(t, "DDS_read: legacy pixel format masks");

	U8 file[sizeof(DDSHeader) + 4] = { 0 };

	typedef struct MaskCase {
		const C8 *name;
		U32 masks[4];
		ETextureFormatId expected;
	} MaskCase;

	const MaskCase cases[] = {
		{ "rgba8", { 0xFF, 0xFF00, 0xFF0000, 0xFF000000 }, ETextureFormatId_RGBA8 },
		{ "rg16", { 0xFFFF, 0xFFFF0000, 0, 0 }, ETextureFormatId_RG16 },
		{ "rg16 with a blue mask", { 0xFFFF, 0xFFFF0000, 0xFF, 0 }, ETextureFormatId_Undefined },
		{ "rg16 with an alpha mask", { 0xFFFF, 0xFFFF0000, 0, 0xFF000000 }, ETextureFormatId_Undefined },
		{ "rgb10a2", { 0x3FF, 0xFFC00, 0x3FF00000, 0xC0000000 }, ETextureFormatId_BGR10A2 },
		{ "rgb10 with no alpha mask", { 0x3FF, 0xFFC00, 0x3FF00000, 0 }, ETextureFormatId_Undefined },
		{ "rgb10 with a wrong alpha mask", { 0x3FF, 0xFFC00, 0x3FF00000, 0x80000000 }, ETextureFormatId_Undefined },
		{ "rgba8 with a wrong alpha mask", { 0xFF, 0xFF00, 0xFF0000, 0 }, ETextureFormatId_Undefined },
	};

	for (U64 i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {

		DDSHeaderMax d = makeHeader(1, 1, 1, 0);

		for (U32 j = 0; j < 4; ++j)
			d.header.format.masks[j] = cases[i].masks[j];

		Buffer_memcpy(Buffer_createRef(file, sizeof(DDSHeader)), Buffer_createRefConst(&d, sizeof(DDSHeader)));

		DDSInfo info = (DDSInfo) { 0 };
		ListSubResourceData res = (ListSubResourceData) { 0 };
		Bool ok = readBytes(t, file, sizeof(file), 0, t->alloc, &info, &res, NULL);

		if(cases[i].expected)
			Test_assert(t, cases[i].name, ok && info.textureFormatId == cases[i].expected);

		else Test_assert(t, cases[i].name, !ok);

		if(res.ptr)
			ListSubResourceData_freeUnderlying(&res, t->alloc);
	}
}

//A depth on a texture that isn't a volume would multiply every mip by a length it doesn't have

void Test_DDSDepthOnNonVolume(Test *t) {

	Test_setModule(t, "DDS_read: depth flag on a non volume texture");

	U8 file[sizeof(DDSHeader) + 5 * 4] = { 0 };

	DDSHeaderMax d = makeHeader(1, 1, 1, EDDSFlag_Depth);

	d.header.depth = 5;

	Buffer_memcpy(Buffer_createRef(file, sizeof(DDSHeader)), Buffer_createRefConst(&d, sizeof(DDSHeader)));

	DDSInfo info = (DDSInfo) { 0 };
	ListSubResourceData res = (ListSubResourceData) { 0 };

	Test_assert(t, "2d with depth 5", !readBytes(t, file, sizeof(file), 0, t->alloc, &info, &res, NULL));

	//Depth 1 with the flag is fine

	d.header.depth = 1;

	Buffer_memcpy(Buffer_createRef(file, sizeof(DDSHeader)), Buffer_createRefConst(&d, sizeof(DDSHeader)));

	Test_assert(t, "2d with depth 1", readBytes(t, file, sizeof(file), 0, t->alloc, &info, &res, NULL));
	ListSubResourceData_freeUnderlying(&res, t->alloc);

	//A cubemap with a depth

	d.header.depth = 2;
	d.header.caps.flag1 |= EDDSCapsFlags1_Complex;
	d.header.caps.flag2 = EDDSCapsFlags2_Cubemap;

	Buffer_memcpy(Buffer_createRef(file, sizeof(DDSHeader)), Buffer_createRefConst(&d, sizeof(DDSHeader)));

	Test_assert(t, "cube with depth 2", !readBytes(t, file, sizeof(file), 0, t->alloc, &info, &res, NULL));
}

//The sort comparator must be a strict weak order, which means equal elements compare equal

ECompareResult SubResourceData_sort(const void *aRaw, const void *bRaw, void *context);

void Test_DDSSortComparator(Test *t) {

	Test_setModule(t, "DDS_write: subresource comparator");

	SubResourceData a = (SubResourceData) { .layerId = 1, .mipId = 2, .z = 3 };
	SubResourceData b = a;

	Test_assert(t, "equal is Eq", SubResourceData_sort(&a, &b, NULL) == ECompareResult_Eq);
	Test_assert(t, "equal is Eq reversed", SubResourceData_sort(&b, &a, NULL) == ECompareResult_Eq);

	b.z = 4;
	Test_assert(t, "z lt", SubResourceData_sort(&a, &b, NULL) == ECompareResult_Lt);
	Test_assert(t, "z gt", SubResourceData_sort(&b, &a, NULL) == ECompareResult_Gt);

	b = a;
	b.mipId = 1;
	Test_assert(t, "mip gt", SubResourceData_sort(&a, &b, NULL) == ECompareResult_Gt);
	Test_assert(t, "mip lt", SubResourceData_sort(&b, &a, NULL) == ECompareResult_Lt);

	b = a;
	b.layerId = 0;
	Test_assert(t, "layer gt", SubResourceData_sort(&a, &b, NULL) == ECompareResult_Gt);
	Test_assert(t, "layer lt", SubResourceData_sort(&b, &a, NULL) == ECompareResult_Lt);
}

//Failing each write in turn: a lost write must not be reported as a success

void Test_DDSWriteFailedWrites(Test *t) {

	Test_setModule(t, "DDS_write: failed writes are reported");

	const RefPtrType *type = streamType(t);
	const ETextureFormatId fmtId = ETextureFormatId_RGBA8;

	//Small images write everything in one flush at the end, bigger ones through several cursor writes

	const U32 sizes[] = { 1, 64 };

	for (U64 sz = 0; sz < sizeof(sizes) / sizeof(sizes[0]); ++sz) {

		StreamRef *src = NULL;
		ListSubResourceData subs = (ListSubResourceData) { 0 };
		DDSInfo info = {
			.w = sizes[sz], .h = sizes[sz], .l = 1, .mips = 1, .layers = 1, .type = ETextureType_2D, .textureFormatId = fmtId
		};

		if (!Test_assert(t, "build", buildSubResources(t, sizes[sz], sizes[sz], 1, 1, 1, fmtId, &src, &subs, type))) {
			ListSubResourceData_freeUnderlying(&subs, t->alloc);
			RefPtr_dec(&src);
			continue;
		}

		StreamRef *dst = NULL;

		Test_assert(t, "create", MemoryStream_create(0, EMemoryStreamFlags_WriteResize, type, &dst, &t->err));
		FailingStream_arm(dst, 0);

		U64 off = 0;

		Test_assert(t, "write ok", DDS_write(dst, &off, &subs, &info, t->alloc, &t->err));

		const U64 writes = FailingStream_writes();

		Test_assert(t, "wrote something", writes > 0);
		RefPtr_dec(&dst);

		for (U64 i = 1; i <= writes; ++i) {

			Test_assert(t, "create", MemoryStream_create(0, EMemoryStreamFlags_WriteResize, type, &dst, &t->err));
			FailingStream_arm(dst, i);

			Error err = (Error) { 0 };

			off = 0;
			Test_assert(t, "failed write is reported", !DDS_write(dst, &off, &subs, &info, t->alloc, &err));
			Test_assert(t, "the write was reached", FailingStream_failed() && err.genericError);
			Test_assert(t, "a failed write leaves the offset", !off);
			RefPtr_dec(&dst);
		}

		ListSubResourceData_freeUnderlying(&subs, t->alloc);
		RefPtr_dec(&src);
	}
}
