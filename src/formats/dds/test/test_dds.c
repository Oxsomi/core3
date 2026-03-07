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

#include "test_dds_shared.h"
#include "types/container/memory_stream.h"
#include "types/base/mathi.h"
#include "formats/dds/dds_file.h"
#include "formats/dds/dds_headers.h"

//Helper: build a ListSubResourceData for a 2D-like layout (arraySize layers, mip chain, depth slices per mip).
//Fills each slice with a unique byte so content can be verified after round-trip.
//Caller must call ListSubResourceData_freeUnderlying on *subs,
//and RefPtr_dec on *sharedStream.
static Bool buildSubResources(
	Test *t,
	U32 w, U32 h, U32 depth,
	U32 mips, U32 layers,
	ETextureFormatId fmtId,
	StreamRef **sharedStream,
	ListSubResourceData *subs,
	const RefPtrType *type
) {
	const ETextureFormat fmt = ETextureFormatId_unpack[fmtId];

	//Compute total byte size across all subresources so we can back them all with one stream

	U64 total = 0;
	U32 mw = w, mh = h, ml = depth;

	for (U32 j = 0; j < mips; ++j) {
		total += ETextureFormat_getSize(fmt, mw, mh, 1) * ml * layers;
		mw = U32_max(1, mw >> 1);
		mh = U32_max(1, mh >> 1);
		ml = U32_max(1, ml >> 1);
	}

	StreamRef *sr = NULL;

	if (!MemoryStream_create(total, EMemoryStreamFlags_IsWritable, type, &sr, &t->err))
		return false;

	U64 off = 0;

	for (U32 i = 0; i < layers; ++i) {

		mw = w; mh = h; ml = depth;

		for (U32 j = 0; j < mips; ++j) {

			U64 sliceLen = ETextureFormat_getSize(fmt, mw, mh, 1);

			for (U32 k = 0; k < ml; ++k) {

				RefPtr_inc(sr);

				SubResourceData sub = (SubResourceData) {
					.layerId   = i,
					.mipId     = j,
					.z         = k,
					.stream    = sr,
					.streamOff = off,
					.streamLen = sliceLen
				};

				if (!ListSubResourceData_pushBack(subs, sub, t->alloc, &t->err)) {
					RefPtr_dec(&sr);
					RefPtr_dec(&sr);	//undo the inc above
					return false;
				}

				off += sliceLen;
			}

			mw = U32_max(1, mw >> 1);
			mh = U32_max(1, mh >> 1);
			ml = U32_max(1, ml >> 1);
		}
	}

	*sharedStream = sr;	//caller owns the base ref
	return true;
}

//Helper: write then read back a DDS, returns false on any failure.
//Caller owns *archiveSr (RefPtr_dec) and *result (freeUnderlying).
static Bool roundTrip(
	Test *t,
	ListSubResourceData *subs,
	const DDSInfo *info,
	StreamRef **archiveSr,
	DDSInfo *readInfo,
	ListSubResourceData *result,
	const RefPtrType *type
) {
	StreamRef *sr = NULL;

	if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, type, &sr, &t->err))
		return false;

	U64 off = 0;

	if (!DDS_write(sr, &off, subs, info, t->alloc, &t->err)) {
		RefPtr_dec(&sr);
		return false;
	}

	U64 readOff = 0;

	if (!DDS_read(sr, &readOff, readInfo, t->alloc, result, &t->err)) {
		RefPtr_dec(&sr);
		return false;
	}

	*archiveSr = sr;
	return true;
}

//Single 4x4 RGBA8 2D texture, 1 mip, 1 layer. Uses legacy pixel-format path.
void Test_DDSRoundTripRGBA8(Test *t) {

	Test_setModule(t, "DDS round-trip: RGBA8 2D single mip");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		StreamRef *dataSr    = NULL;
		StreamRef *archiveSr = NULL;
		ListSubResourceData subs   = { 0 };
		ListSubResourceData result = { 0 };
		DDSInfo readInfo = { 0 };

		const ETextureFormatId fmtId = ETextureFormatId_RGBA8;

		DDSInfo info = { .w = 4, .h = 4, .l = 1, .mips = 1, .layers = 1,
		                 .type = ETextureType_2D, .textureFormatId = fmtId };

		if (!buildSubResources(t, 4, 4, 1, 1, 1, fmtId, &dataSr, &subs, &type)) {
			Test_assert(t, "build RGBA8", false);
			goto doneRGBA8;
		}

		Test_assert(t, "RGBA8 round-trip", roundTrip(t, &subs, &info, &archiveSr, &readInfo, &result, &type));
		Test_assert(t, "RGBA8 w",          readInfo.w               == 4);
		Test_assert(t, "RGBA8 h",          readInfo.h               == 4);
		Test_assert(t, "RGBA8 l",          readInfo.l               == 1);
		Test_assert(t, "RGBA8 mips",       readInfo.mips            == 1);
		Test_assert(t, "RGBA8 layers",     readInfo.layers          == 1);
		Test_assert(t, "RGBA8 fmt",        readInfo.textureFormatId == fmtId);
		Test_assert(t, "RGBA8 type",       readInfo.type            == ETextureType_2D);
		Test_assert(t, "RGBA8 sub count",  result.length            == 1);

		if (result.length == 1) {
			const ETextureFormat fmt = ETextureFormatId_unpack[fmtId];
			Test_assert(t, "RGBA8 sub len", result.ptr[0].streamLen == ETextureFormat_getSize(fmt, 4, 4, 1));
			Test_assert(t, "RGBA8 mipId",   result.ptr[0].mipId     == 0);
			Test_assert(t, "RGBA8 layerId", result.ptr[0].layerId   == 0);
			Test_assert(t, "RGBA8 z",       result.ptr[0].z         == 0);
		}

	doneRGBA8:
		RefPtr_dec(&dataSr);
		RefPtr_dec(&archiveSr);
		ListSubResourceData_freeUnderlying(&subs,   t->alloc);
		ListSubResourceData_freeUnderlying(&result, t->alloc);
	}
}

//8x8 RGBA8 2D, full mip chain (4 mips: 8x8, 4x4, 2x2, 1x1).
//Verifies subresource count and each mip's streamLen.
void Test_DDSRoundTripMipChain(Test *t) {

	Test_setModule(t, "DDS round-trip: RGBA8 full mip chain");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		StreamRef *dataSr    = NULL;
		StreamRef *archiveSr = NULL;
		ListSubResourceData subs   = { 0 };
		ListSubResourceData result = { 0 };
		DDSInfo readInfo = { 0 };

		const ETextureFormatId fmtId = ETextureFormatId_RGBA8;
		const ETextureFormat   fmt   = ETextureFormatId_unpack[fmtId];
		const U32 W = 8, H = 8, numMips = 4;

		DDSInfo info = { .w = W, .h = H, .l = 1, .mips = numMips, .layers = 1,
		                 .type = ETextureType_2D, .textureFormatId = fmtId };

		if (!buildSubResources(t, W, H, 1, numMips, 1, fmtId, &dataSr, &subs, &type)) {
			Test_assert(t, "build mip chain", false);
			goto doneMips;
		}

		Test_assert(t, "mips round-trip",  roundTrip(t, &subs, &info, &archiveSr, &readInfo, &result, &type));
		Test_assert(t, "mips count",       readInfo.mips  == numMips);
		Test_assert(t, "mips sub count",   result.length  == numMips);

		U32 mw = W, mh = H;

		for (U32 m = 0; m < numMips && m < (U32)result.length; ++m) {
			Test_assert(t, "mip mipId",   result.ptr[m].mipId    == m);
			Test_assert(t, "mip layerId", result.ptr[m].layerId  == 0);
			Test_assert(t, "mip streamLen", result.ptr[m].streamLen == ETextureFormat_getSize(fmt, mw, mh, 1));
			mw = U32_max(1, mw >> 1);
			mh = U32_max(1, mh >> 1);
		}

	doneMips:
		RefPtr_dec(&dataSr);
		RefPtr_dec(&archiveSr);
		ListSubResourceData_freeUnderlying(&subs,   t->alloc);
		ListSubResourceData_freeUnderlying(&result, t->alloc);
	}
}

//4x4 RGBA8 cubemap, 1 mip, 6 layers. Requires DXT10 header.
//Verifies type, layers, and that all 6 face subresources are present.
void Test_DDSRoundTripCubemap(Test *t) {

	Test_setModule(t, "DDS round-trip: RGBA8 cubemap");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		StreamRef *dataSr    = NULL;
		StreamRef *archiveSr = NULL;
		ListSubResourceData subs   = { 0 };
		ListSubResourceData result = { 0 };
		DDSInfo readInfo = { 0 };

		const ETextureFormatId fmtId = ETextureFormatId_RGBA8;
		const ETextureFormat   fmt   = ETextureFormatId_unpack[fmtId];

		DDSInfo info = { .w = 4, .h = 4, .l = 1, .mips = 1, .layers = 6,
		                 .type = ETextureType_Cube, .textureFormatId = fmtId };

		if (!buildSubResources(t, 4, 4, 1, 1, 6, fmtId, &dataSr, &subs, &type)) {
			Test_assert(t, "build cubemap", false);
			goto doneCube;
		}

		Test_assert(t, "cube round-trip",  roundTrip(t, &subs, &info, &archiveSr, &readInfo, &result, &type));
		Test_assert(t, "cube type",        readInfo.type   == ETextureType_Cube);
		Test_assert(t, "cube layers",      readInfo.layers == 6);
		Test_assert(t, "cube w",           readInfo.w      == 4);
		Test_assert(t, "cube sub count",   result.length   == 6);

		const U64 faceLen = ETextureFormat_getSize(fmt, 4, 4, 1);

		for (U32 f = 0; f < 6 && f < (U32)result.length; ++f) {
			Test_assert(t, "cube face layerId",   result.ptr[f].layerId  == f);
			Test_assert(t, "cube face mipId",     result.ptr[f].mipId    == 0);
			Test_assert(t, "cube face streamLen", result.ptr[f].streamLen == faceLen);
		}

	doneCube:
		RefPtr_dec(&dataSr);
		RefPtr_dec(&archiveSr);
		ListSubResourceData_freeUnderlying(&subs,   t->alloc);
		ListSubResourceData_freeUnderlying(&result, t->alloc);
	}
}

//4x4 BC5 single mip: uses legacy FourCC path (no DXT10).
//Verifies the legacy pixel-format detection reconstructs the right formatId.
void Test_DDSRoundTripBC5Legacy(Test *t) {

	Test_setModule(t, "DDS round-trip: BC5 legacy FourCC");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		StreamRef *dataSr    = NULL;
		StreamRef *archiveSr = NULL;
		ListSubResourceData subs   = { 0 };
		ListSubResourceData result = { 0 };
		DDSInfo readInfo = { 0 };

		const ETextureFormatId fmtId = ETextureFormatId_BC5;

		DDSInfo info = { .w = 4, .h = 4, .l = 1, .mips = 1, .layers = 1,
		                 .type = ETextureType_2D, .textureFormatId = fmtId };

		if (!buildSubResources(t, 4, 4, 1, 1, 1, fmtId, &dataSr, &subs, &type)) {
			Test_assert(t, "build BC5", false);
			goto doneBC5;
		}

		Test_assert(t, "BC5 round-trip",  roundTrip(t, &subs, &info, &archiveSr, &readInfo, &result, &type));
		Test_assert(t, "BC5 fmt",         readInfo.textureFormatId == fmtId);
		Test_assert(t, "BC5 type",        readInfo.type            == ETextureType_2D);
		Test_assert(t, "BC5 sub count",   result.length            == 1);

	doneBC5:
		RefPtr_dec(&dataSr);
		RefPtr_dec(&archiveSr);
		ListSubResourceData_freeUnderlying(&subs,   t->alloc);
		ListSubResourceData_freeUnderlying(&result, t->alloc);
	}
}

//4x4x4 RGBA8 3D texture, single mip (4 depth slices).
//Verifies type == ETextureType_3D, l == 4, and subresource count == 4.
void Test_DDSRoundTrip3D(Test *t) {

	Test_setModule(t, "DDS round-trip: RGBA8 3D");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		StreamRef *dataSr    = NULL;
		StreamRef *archiveSr = NULL;
		ListSubResourceData subs   = { 0 };
		ListSubResourceData result = { 0 };
		DDSInfo readInfo = { 0 };

		const ETextureFormatId fmtId = ETextureFormatId_RGBA8;
		const ETextureFormat   fmt   = ETextureFormatId_unpack[fmtId];
		const U32 W = 4, H = 4, L = 4;

		DDSInfo info = { .w = W, .h = H, .l = L, .mips = 1, .layers = 1,
		                 .type = ETextureType_3D, .textureFormatId = fmtId };

		if (!buildSubResources(t, W, H, L, 1, 1, fmtId, &dataSr, &subs, &type)) {
			Test_assert(t, "build 3D", false);
			goto done3D;
		}

		Test_assert(t, "3D round-trip",  roundTrip(t, &subs, &info, &archiveSr, &readInfo, &result, &type));
		Test_assert(t, "3D type",        readInfo.type      == ETextureType_3D);
		Test_assert(t, "3D l",           readInfo.l         == L);
		Test_assert(t, "3D w",           readInfo.w         == W);
		Test_assert(t, "3D h",           readInfo.h         == H);
		Test_assert(t, "3D sub count",   result.length      == L);

		const U64 sliceLen = ETextureFormat_getSize(fmt, W, H, 1);

		for (U32 z = 0; z < L && z < (U32)result.length; ++z) {
			Test_assert(t, "3D slice z",         result.ptr[z].z         == z);
			Test_assert(t, "3D slice mipId",     result.ptr[z].mipId     == 0);
			Test_assert(t, "3D slice streamLen", result.ptr[z].streamLen == sliceLen);
		}

	done3D:
		RefPtr_dec(&dataSr);
		RefPtr_dec(&archiveSr);
		ListSubResourceData_freeUnderlying(&subs,   t->alloc);
		ListSubResourceData_freeUnderlying(&result, t->alloc);
	}
}

//Claim more mips than dimensions allow (5 mips for a 4x4 texture, max is 3).
//DDS_write must reject this.
void Test_DDSWriteInvalidMipCount(Test *t) {

	Test_setModule(t, "DDS write: mip count exceeds dimensions");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		StreamRef *dataSr    = NULL;
		StreamRef *archiveSr = NULL;
		ListSubResourceData subs = { 0 };

		const ETextureFormatId fmtId   = ETextureFormatId_RGBA8;
		const U32 claimedMips = 5;	//4x4 only supports 3 mips

		//Build subresources matching the claimed (invalid) mip count so write gets past the count check
		//and hits the mip validation

		if (!buildSubResources(t, 4, 4, 1, claimedMips, 1, fmtId, &dataSr, &subs, &type)) {
			Test_assert(t, "build bad mip subs", false);
			goto doneBadMip;
		}

		if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &archiveSr, &t->err)) {
			Test_assert(t, "create stream bad mip", false);
			goto doneBadMip;
		}

		U64 off = 0;
		DDSInfo info = { .w = 4, .h = 4, .l = 1, .mips = claimedMips, .layers = 1,
		                 .type = ETextureType_2D, .textureFormatId = fmtId };

		Test_assert(t, "write bad mip count fails", !DDS_write(archiveSr, &off, &subs, &info, t->alloc, NULL));

	doneBadMip:
		RefPtr_dec(&dataSr);
		RefPtr_dec(&archiveSr);
		ListSubResourceData_freeUnderlying(&subs, t->alloc);
	}
}

//Feed a zeroed stream (wrong magic). DDS_read must fail cleanly.
void Test_DDSReadInvalidMagic(Test *t) {

	Test_setModule(t, "DDS read: invalid magic number");

	{
		const RefPtrType type = MemoryStream_makeType(t->alloc);
		StreamRef *sr  = NULL;
		ListSubResourceData result = { 0 };
		DDSInfo info = { 0 };

		if (!MemoryStream_create(sizeof(DDSHeader) * 2, EMemoryStreamFlags_IsWritable, &type, &sr, &t->err)) {
			Test_assert(t, "create zeroed stream", false);
			goto doneInvalidMagic;
		}

		U64 off = 0;
		Test_assert(t, "read invalid magic fails", !DDS_read(sr, &off, &info, t->alloc, &result, NULL));
		Test_assert(t, "result empty on fail",     result.length == 0);

	doneInvalidMagic:
		RefPtr_dec(&sr);
		ListSubResourceData_freeUnderlying(&result, t->alloc);
	}
}

//buf->length doesn't match info's implied subresource count. Must fail.
void Test_DDSWriteSubresourceMismatch(Test *t) {

	Test_setModule(t, "DDS write: subresource count mismatch");

	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		StreamRef *dataSr    = NULL;
		StreamRef *archiveSr = NULL;
		ListSubResourceData subs = { 0 };

		const ETextureFormatId fmtId = ETextureFormatId_RGBA8;

		//Build 1 subresource but tell info there are 2 mips (which requires 2 subresources)

		if (!buildSubResources(t, 4, 4, 1, 1, 1, fmtId, &dataSr, &subs, &type)) {
			Test_assert(t, "build mismatch subs", false);
			goto doneMismatch;
		}

		if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &archiveSr, &t->err)) {
			Test_assert(t, "create stream mismatch", false);
			goto doneMismatch;
		}

		U64 off = 0;

		//info claims 2 mips but subs only has 1 entry

		DDSInfo info = { .w = 4, .h = 4, .l = 1, .mips = 2, .layers = 1,
		                 .type = ETextureType_2D, .textureFormatId = fmtId };

		Test_assert(t, "write mismatch fails", !DDS_write(archiveSr, &off, &subs, &info, t->alloc, NULL));

	doneMismatch:
		RefPtr_dec(&dataSr);
		RefPtr_dec(&archiveSr);
		ListSubResourceData_freeUnderlying(&subs, t->alloc);
	}
}
