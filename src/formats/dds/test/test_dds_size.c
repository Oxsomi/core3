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

//formats/dds/test/test_dds_size.c

#include "test_dds_shared.h"
#include "types/container/memory_stream.h"
#include "formats/dds/dds_file.h"

//Helper: run a null-stream write then a real write and compare sizes.
static Bool ddsCheckSizeConsistency(
	Test *t,
	ListSubResourceData *subs,
	const DDSInfo *info,
	const RefPtrType *type
) {
	U64 predictedSize = 0;

	if (!DDS_write(NULL, &predictedSize, subs, info, t->alloc, &t->err))
		return false;

	StreamRef *archiveSr = NULL;

	if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, type, &archiveSr, &t->err))
		return false;

	U64 realSize = 0;
	Bool ok = DDS_write(archiveSr, &realSize, subs, info, t->alloc, &t->err);
	RefPtr_dec(&archiveSr);

	if (!ok)
		return false;

	return predictedSize == realSize;
}

Bool buildSubResources(
	Test *t,
	U32 w, U32 h, U32 depth,
	U32 mips, U32 layers,
	ETextureFormatId fmtId,
	StreamRef **sharedStream,
	ListSubResourceData *subs,
	const RefPtrType *type
);

//Single 2D RGBA8, 1 mip, legacy pixel format path, no DXT10.
void Test_DDSWriteSizeConsistencyRGBA8(Test *t) {

	Test_setModule(t, "DDS write: null size == real size (RGBA8 2D)");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		StreamRef *dataSr = NULL;
		ListSubResourceData subs = { 0 };

		const ETextureFormatId fmtId = ETextureFormatId_RGBA8;
		DDSInfo info = { .w = 4, .h = 4, .l = 1, .mips = 1, .layers = 1, .type = ETextureType_2D, .textureFormatId = fmtId };

		if (!buildSubResources(t, 4, 4, 1, 1, 1, fmtId, &dataSr, &subs, &type)) {
			Test_assert(t, "build RGBA8 size subs", false);
			goto doneRGBA8Size;
		}

		Test_assert(t, "RGBA8 size consistent", ddsCheckSizeConsistency(t, &subs, &info, &type));

	doneRGBA8Size:
		RefPtr_dec(&dataSr);
		ListSubResourceData_freeUnderlying(&subs, t->alloc);
	}
}

//Full mip chain, size grows with each mip level.
void Test_DDSWriteSizeConsistencyMipChain(Test *t) {

	Test_setModule(t, "DDS write: null size == real size (mip chain)");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		StreamRef *dataSr = NULL;
		ListSubResourceData subs = { 0 };

		const ETextureFormatId fmtId = ETextureFormatId_RGBA8;
		DDSInfo info = { .w = 8, .h = 8, .l = 1, .mips = 4, .layers = 1, .type = ETextureType_2D, .textureFormatId = fmtId };

		if (!buildSubResources(t, 8, 8, 1, 4, 1, fmtId, &dataSr, &subs, &type)) {
			Test_assert(t, "build mip chain size subs", false);
			goto doneMipSize;
		}

		Test_assert(t, "mip chain size consistent", ddsCheckSizeConsistency(t, &subs, &info, &type));

	doneMipSize:
		RefPtr_dec(&dataSr);
		ListSubResourceData_freeUnderlying(&subs, t->alloc);
	}
}

//Cubemap, requires DXT10 header, 6 layers.
void Test_DDSWriteSizeConsistencyCubemap(Test *t) {

	Test_setModule(t, "DDS write: null size == real size (cubemap)");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		StreamRef *dataSr = NULL;
		ListSubResourceData subs = { 0 };

		const ETextureFormatId fmtId = ETextureFormatId_RGBA8;
		DDSInfo info = { .w = 4, .h = 4, .l = 1, .mips = 1, .layers = 6, .type = ETextureType_Cube, .textureFormatId = fmtId };

		if (!buildSubResources(t, 4, 4, 1, 1, 6, fmtId, &dataSr, &subs, &type)) {
			Test_assert(t, "build cubemap size subs", false);
			goto doneCubeSize;
		}

		Test_assert(t, "cubemap size consistent", ddsCheckSizeConsistency(t, &subs, &info, &type));

	doneCubeSize:
		RefPtr_dec(&dataSr);
		ListSubResourceData_freeUnderlying(&subs, t->alloc);
	}
}

//3D texture, depth slices contribute to total size.
void Test_DDSWriteSizeConsistency3D(Test *t) {

	Test_setModule(t, "DDS write: null size == real size (3D)");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		StreamRef *dataSr = NULL;
		ListSubResourceData subs = { 0 };

		const ETextureFormatId fmtId = ETextureFormatId_RGBA8;
		DDSInfo info = { .w = 4, .h = 4, .l = 4, .mips = 1, .layers = 1, .type = ETextureType_3D, .textureFormatId = fmtId };

		if (!buildSubResources(t, 4, 4, 4, 1, 1, fmtId, &dataSr, &subs, &type)) {
			Test_assert(t, "build 3D size subs", false);
			goto done3DSize;
		}

		Test_assert(t, "3D size consistent", ddsCheckSizeConsistency(t, &subs, &info, &type));

	done3DSize:
		RefPtr_dec(&dataSr);
		ListSubResourceData_freeUnderlying(&subs, t->alloc);
	}
}

//BC5 legacy FourCC path, no DXT10 header so size is smaller.
void Test_DDSWriteSizeConsistencyBC5(Test *t) {

	Test_setModule(t, "DDS write: null size == real size (BC5 legacy)");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	{
		StreamRef *dataSr = NULL;
		ListSubResourceData subs = { 0 };

		const ETextureFormatId fmtId = ETextureFormatId_BC5;
		DDSInfo info = { .w = 4, .h = 4, .l = 1, .mips = 1, .layers = 1, .type = ETextureType_2D, .textureFormatId = fmtId };

		if (!buildSubResources(t, 4, 4, 1, 1, 1, fmtId, &dataSr, &subs, &type)) {
			Test_assert(t, "build BC5 size subs", false);
			goto doneBC5Size;
		}

		Test_assert(t, "BC5 size consistent", ddsCheckSizeConsistency(t, &subs, &info, &type));

	doneBC5Size:
		RefPtr_dec(&dataSr);
		ListSubResourceData_freeUnderlying(&subs, t->alloc);
	}
}
