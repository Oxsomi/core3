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

//graphics/test/interface/test_graphics_format.c

//Pure format, packing and layout modules (5, 6, 17, 18, 19, 24).
//No device is needed, so these always run, even where no adapter exists.

#include "graphics/generic/device.h"
#include "graphics/generic/device_info.h"
#include "graphics/generic/tlas.h"
#include "graphics/generic/descriptor_layout.h"
#include "graphics/generic/descriptor_table.h"
#include "graphics/generic/bindless_descriptor.h"
#include "platforms/platform.h"
#include "types/test/test.h"
#include "types/container/texture_format.h"
#include "types/base/string_read_helper.h"
#include "test_graphics_shared.h"

// -- 5. Texture format helpers --------------------------------------------------

void Test_graphicsFormats(Test *t) {

	Test_setModule(t, "TextureFormat");

	Test_assert(t, "rgba8Size", ETextureFormat_getSize(ETextureFormat_RGBA8, 1, 1, 1) == 4);
	Test_assert(t, "rgba16fSize", ETextureFormat_getSize(ETextureFormat_RGBA16f, 1, 1, 1) == 8);
	Test_assert(t, "rgba32fSize", ETextureFormat_getSize(ETextureFormat_RGBA32f, 1, 1, 1) == 16);

	Test_assert(t, "rgba8Uncompressed", !ETextureFormat_getIsCompressed(ETextureFormat_RGBA8));
	Test_assert(t, "bc7Compressed", ETextureFormat_getIsCompressed(ETextureFormat_BC7));

	//4x4 BC7 block = 16 bytes (misaligned blocks handled the same)

	Test_assert(t, "bc7BlockSize4", ETextureFormat_getSize(ETextureFormat_BC7, 4, 4, 1) == 16);
	Test_assert(t, "bc7BlockSize2", ETextureFormat_getSize(ETextureFormat_BC7, 2, 2, 1) == 16);
	Test_assert(t, "bc7BlockSize1", ETextureFormat_getSize(ETextureFormat_BC7, 1, 1, 1) == 16);

	//Vertex attributes exclude compressed formats
	Test_assert(t, "rgba8VertexAttrib", GraphicsDeviceInfo_supportsFormatVertexAttribute(ETextureFormat_RGBA8));
	Test_assert(t, "bc7NoVertexAttrib", !GraphicsDeviceInfo_supportsFormatVertexAttribute(ETextureFormat_BC7));

	//ETextureFormatId <-> ETextureFormat mapping is consistent
	Test_assert(t, "unpackRGBA8", ETextureFormatId_unpack[ETextureFormatId_RGBA8] == ETextureFormat_RGBA8);
	Test_assert(t, "unpackUndefined", ETextureFormatId_unpack[ETextureFormatId_Undefined] == ETextureFormat_Undefined);

	//The support queries only read capabilities.dataTypes, so a synthesised info exercises both answers without
	// needing the adapter to happen to have or lack the type.

	GraphicsDeviceInfo none = (GraphicsDeviceInfo) { 0 };
	GraphicsDeviceInfo all = (GraphicsDeviceInfo) { 0 };

	all.capabilities.dataTypes =
		EGraphicsDataTypes_BCn | EGraphicsDataTypes_ASTC | EGraphicsDataTypes_RGB32f |
		EGraphicsDataTypes_D24S8 | EGraphicsDataTypes_S8;

	//Anything not gated by a data type is supported regardless of the device

	Test_assert(t, "rgba8Always", GraphicsDeviceInfo_supportsFormat(&none, ETextureFormat_RGBA8));

	Test_assert(t, "bc7NeedsBCn",
		!GraphicsDeviceInfo_supportsFormat(&none, ETextureFormat_BC7) &&
		GraphicsDeviceInfo_supportsFormat(&all, ETextureFormat_BC7)
	);

	Test_assert(t, "rgb32fNeedsType",
		!GraphicsDeviceInfo_supportsFormat(&none, ETextureFormat_RGB32f) &&
		GraphicsDeviceInfo_supportsFormat(&all, ETextureFormat_RGB32f)
	);

	//Render targets additionally exclude compressed formats, even where the device can sample them

	Test_assert(t, "bc7NoRenderTarget", !GraphicsDeviceInfo_supportsRenderTextureFormat(&all, ETextureFormat_BC7));
	Test_assert(t, "rgba8RenderTarget", GraphicsDeviceInfo_supportsRenderTextureFormat(&none, ETextureFormat_RGBA8));

	//Plain depth needs nothing, the stencil combinations each need their own data type

	Test_assert(t, "d32Always", GraphicsDeviceInfo_supportsDepthStencilFormat(&none, EDepthStencilFormat_D32));

	Test_assert(t, "d24s8NeedsType",
		!GraphicsDeviceInfo_supportsDepthStencilFormat(&none, EDepthStencilFormat_D24S8Ext) &&
		GraphicsDeviceInfo_supportsDepthStencilFormat(&all, EDepthStencilFormat_D24S8Ext)
	);

	Test_assert(t, "d32s8NeedsType", !GraphicsDeviceInfo_supportsDepthStencilFormat(&all, EDepthStencilFormat_D32S8X24Ext));

	Test_assert(t, "supportsNullDevice",
		!GraphicsDeviceInfo_supportsFormat(NULL, ETextureFormat_RGBA8) &&
		!GraphicsDeviceInfo_supportsDepthStencilFormat(NULL, EDepthStencilFormat_D32)
	);
}

// -- 6. Bindless descriptor packing (pure, no device) ----------------------------

void Test_bindlessDescriptorPacking(Test *t) {

	Test_setModule(t, "BindlessDescriptor/pack");

	//A handle is a 4 bit bindless type (0 is reserved) followed by a 17 bit array index, so 21 bits in total.
	//Three of them fit in a U64, which is what pack3/unpack3 are for.

	const BindlessDescriptor a = ((BindlessDescriptor)1 << 17) | 1;
	const BindlessDescriptor b = ((BindlessDescriptor)2 << 17) | 1234;
	const BindlessDescriptor c = ((BindlessDescriptor)15 << 17) | ((1 << 17) - 1);

	Test_assert(t, "getBindlessType", BindlessDescriptor_getBindlessType(b) == 2);
	Test_assert(t, "getId", BindlessDescriptor_getId(b) == 1234);
	Test_assert(t, "maxHandleFits", c == (BindlessDescriptor)((1 << 21) - 1));

	const U64 packed = BindlessDescriptor_pack3(a, b, c);
	Test_assert(t, "pack3", packed != U64_MAX);        //U64_MAX marks a component that didn't fit in 21 bits

	const I32x4 unpacked = BindlessDescriptor_unpack3(packed);

	Test_assert(t, "unpack3X", (BindlessDescriptor) I32x4_x(unpacked) == a);
	Test_assert(t, "unpack3Y", (BindlessDescriptor) I32x4_y(unpacked) == b);
	Test_assert(t, "unpack3Z", (BindlessDescriptor) I32x4_z(unpacked) == c);

	//None is a handle like any other to pack3, it just doesn't address a descriptor.

	const U64 none = BindlessDescriptor_pack3(BindlessDescriptor_None, BindlessDescriptor_None, BindlessDescriptor_None);
	Test_assert(t, "pack3None", none == 0);

	const I32x4 unpackedNone = BindlessDescriptor_unpack3(none);

	Test_assert(t, "unpack3None", !I32x4_x(unpackedNone) && !I32x4_y(unpackedNone) && !I32x4_z(unpackedNone));

	//Anything wider than 21 bits can't be packed, whichever component it is.

	const BindlessDescriptor tooWide = (BindlessDescriptor)1 << 21;

	Test_assert(t, "pack3OverflowX", BindlessDescriptor_pack3(tooWide, 0, 0) == U64_MAX);
	Test_assert(t, "pack3OverflowY", BindlessDescriptor_pack3(0, tooWide, 0) == U64_MAX);
	Test_assert(t, "pack3OverflowZ", BindlessDescriptor_pack3(0, 0, tooWide) == U64_MAX);
}

// -- 17. TLASTransformSRT (pure, no device) --------------------------------------

//The SRT layout is VkSRTDataNV, where scale, pivot and shearing are interleaved rather than kept as three vectors
// (sx, a, b / pvx, sy, c / pvy, sz, pvz).
//Setting all five with distinct values and reading all five back is what catches a crossed over field;
// round tripping one component at a time would pass even if two of them shared a slot.
//The struct is handed to the driver as is, so the raw fields are pinned here too.
//Compared exactly rather than approximately, since every value here is stored and loaded without any arithmetic.

void Test_tlasTransformSRT(Test *t) {

	Test_setModule(t, "TLASTransformSRT");

	Test_assert(t, "size", sizeof(TLASTransformSRT) == 16 * sizeof(F32));

	TLASTransformSRT srt = (TLASTransformSRT) { 0 };

	TLASTransformSRT_setScale(&srt, F32x4_create3(1, 2, 3));
	TLASTransformSRT_setPivot(&srt, F32x4_create3(4, 5, 6));
	TLASTransformSRT_setTranslate(&srt, F32x4_create3(7, 8, 9));
	TLASTransformSRT_setShearing(&srt, F32x4_create3(10, 11, 12));
	TLASTransformSRT_setQuat(&srt, QuatF32_create(0.25f, 0.5f, 0.125f, 0.75f));

	Test_assert(t, "getScale", F32x4_eqExact4(TLASTransformSRT_getScale(&srt), F32x4_create3(1, 2, 3)));
	Test_assert(t, "getPivot", F32x4_eqExact4(TLASTransformSRT_getPivot(&srt), F32x4_create3(4, 5, 6)));
	Test_assert(t, "getTranslate", F32x4_eqExact4(TLASTransformSRT_getTranslate(&srt), F32x4_create3(7, 8, 9)));
	Test_assert(t, "getShearing", F32x4_eqExact4(TLASTransformSRT_getShearing(&srt), F32x4_create3(10, 11, 12)));

	Test_assert(t, "getQuat",
		F32x4_eqExact4(TLASTransformSRT_getQuat(&srt), QuatF32_create(0.25f, 0.5f, 0.125f, 0.75f))
	);

	//Pinned against VkSRTDataNV, since a wrong field here reaches the driver silently

	Test_assert(t, "layoutScale", srt.sx == 1 && srt.sy == 2 && srt.sz == 3);
	Test_assert(t, "layoutPivot", srt.pvx == 4 && srt.pvy == 5 && srt.pvz == 6);
	Test_assert(t, "layoutTranslate", srt.tx == 7 && srt.ty == 8 && srt.tz == 9);
	Test_assert(t, "layoutShearing", srt.a == 10 && srt.b == 11 && srt.c == 12);

	//createSimple leaves the two it doesn't take at zero, create fills all five

	const TLASTransformSRT simple = TLASTransformSRT_createSimple(
		F32x4_create3(2, 2, 2), F32x4_create3(1, 0, 0), QuatF32_identity()
	);

	Test_assert(t, "createSimpleScale", F32x4_eqExact4(TLASTransformSRT_getScale(&simple), F32x4_create3(2, 2, 2)));
	Test_assert(t, "createSimpleTranslate", F32x4_eqExact4(TLASTransformSRT_getTranslate(&simple), F32x4_create3(1, 0, 0)));
	Test_assert(t, "createSimpleNoPivot", F32x4_eqExact4(TLASTransformSRT_getPivot(&simple), F32x4_zero()));
	Test_assert(t, "createSimpleNoShearing", F32x4_eqExact4(TLASTransformSRT_getShearing(&simple), F32x4_zero()));

	const TLASTransformSRT full = TLASTransformSRT_create(
		F32x4_create3(1, 2, 3), F32x4_create3(4, 5, 6), F32x4_create3(7, 8, 9),
		QuatF32_identity(), F32x4_create3(10, 11, 12)
	);

	Test_assert(t, "createPivot", F32x4_eqExact4(TLASTransformSRT_getPivot(&full), F32x4_create3(4, 5, 6)));
	Test_assert(t, "createShearing", F32x4_eqExact4(TLASTransformSRT_getShearing(&full), F32x4_create3(10, 11, 12)));

	//NULL is a query on nothing rather than a crash, and the setters report that they did nothing

	Test_assert(t, "setNull",
		!TLASTransformSRT_setScale(NULL, F32x4_zero()) && !TLASTransformSRT_setPivot(NULL, F32x4_zero()) &&
		!TLASTransformSRT_setTranslate(NULL, F32x4_zero()) && !TLASTransformSRT_setShearing(NULL, F32x4_zero()) &&
		!TLASTransformSRT_setQuat(NULL, QuatF32_identity())
	);

	Test_assert(t, "getNullZero",
		F32x4_eqExact4(TLASTransformSRT_getScale(NULL), F32x4_zero()) &&
		F32x4_eqExact4(TLASTransformSRT_getPivot(NULL), F32x4_zero()) &&
		F32x4_eqExact4(TLASTransformSRT_getTranslate(NULL), F32x4_zero()) &&
		F32x4_eqExact4(TLASTransformSRT_getShearing(NULL), F32x4_zero())
	);

	//A zeroed quat would be a degenerate rotation, so NULL gives identity instead

	Test_assert(t, "getNullQuatIdentity", F32x4_eqExact4(TLASTransformSRT_getQuat(NULL), QuatF32_identity()));
}

// -- 18. Descriptor packing (pure, no device) ------------------------------------

//A buffer descriptor keeps a 48 bit start and end region, and hides the 32 bit counter offset in the top 16 bits of
// each of those two U64s.
//A counter offset with both halves set is what proves the split and the masking agree; one that fits in 16 bits
// would pass even if the high half were dropped.

void Test_descriptorPacking(Test *t) {

	Test_setModule(t, "Descriptor/pack");

	const U64 start = 0x1000, end = 0x5000;
	const U32 counterOffset = 0xABCD1234;

	const Descriptor buf = Descriptor_buffer(NULL, start, end, NULL, counterOffset);

	Test_assert(t, "start", Descriptor_startBuffer(&buf) == start);
	Test_assert(t, "end", Descriptor_endBuffer(&buf) == end);
	Test_assert(t, "length", Descriptor_bufferLength(&buf) == end - start);
	Test_assert(t, "counterOffset", Descriptor_counterOffset(&buf) == counterOffset);

	//48 bits is the widest region that survives sharing its U64 with the counter offset

	const U64 max48 = ((U64)1 << 48) - 1;
	const Descriptor wide = Descriptor_buffer(NULL, 0, max48, NULL, counterOffset);

	Test_assert(t, "max48End", Descriptor_endBuffer(&wide) == max48);
	Test_assert(t, "max48Length", Descriptor_bufferLength(&wide) == max48);
	Test_assert(t, "max48Counter", Descriptor_counterOffset(&wide) == counterOffset);

	//Anything wider is refused with a null descriptor rather than silently truncated into the counter offset

	const Descriptor tooWideStart = Descriptor_buffer(NULL, (U64)1 << 48, end, NULL, 0);
	const Descriptor tooWideEnd = Descriptor_buffer(NULL, start, (U64)1 << 48, NULL, 0);

	Test_assert(t, "tooWideStart", !Descriptor_startBuffer(&tooWideStart) && !Descriptor_endBuffer(&tooWideStart));
	Test_assert(t, "tooWideEnd", !Descriptor_startBuffer(&tooWideEnd) && !Descriptor_endBuffer(&tooWideEnd));

	//Texture descriptors share the same union, so their fields have to land where the range isn't

	const Descriptor tex = Descriptor_texture(NULL, 1, 2, 3, 4, 5, 6);

	Test_assert(t, "textureMip", tex.texture.mipId == 1 && tex.texture.mipCount == 2);
	Test_assert(t, "texturePlane", tex.texture.planeId == 3 && tex.texture.imageId == 4);
	Test_assert(t, "textureArray", tex.texture.arrayId == 5 && tex.texture.arrayCount == 6);

	//tlas and sampler carry only the resource, so nothing may read back as a range

	const Descriptor tlas = Descriptor_tlas(NULL);
	const Descriptor sampler = Descriptor_sampler(NULL);

	Test_assert(t, "tlasEmpty", !Descriptor_startBuffer(&tlas) && !Descriptor_endBuffer(&tlas) && !tlas.resource);
	Test_assert(t, "samplerEmpty", !Descriptor_bufferLength(&sampler) && !sampler.resource);

	Test_assert(t, "nullDescriptor",
		!Descriptor_startBuffer(NULL) && !Descriptor_endBuffer(NULL) &&
		!Descriptor_bufferLength(NULL) && !Descriptor_counterOffset(NULL)
	);
}

// -- 19. TextureRange (pure, no device) ------------------------------------------

void Test_textureRange(Test *t) {

	Test_setModule(t, "TextureRange");

	const TextureRange r = (TextureRange) { .startRange = { 1, 2, 3 }, .endRange = { 11, 22, 33 }, .levelId = 4 };

	Test_assert(t, "width", TextureRange_width(r) == 10);
	Test_assert(t, "height", TextureRange_height(r) == 20);
	Test_assert(t, "length", TextureRange_length(r) == 30);

	//An empty range is legal and measures zero, it isn't an error value

	const TextureRange empty = (TextureRange) { 0 };

	Test_assert(t, "emptyWidth", !TextureRange_width(empty));
	Test_assert(t, "emptyHeight", !TextureRange_height(empty));
	Test_assert(t, "emptyLength", !TextureRange_length(empty));
}

// -- 24. Default bindless layout (pure, no device) -------------------------------

//What OxC3's own shaders are compiled against, so the numbers here are a contract with resources.hlsli rather
// than an implementation detail that may drift.
//The DXIL offsets form a chain where each register range starts exactly where the previous one ends, which pins
// the exact numbers without this test needing the private count enums.

void Test_graphicsDefaultBindlessLayout(Test *t) {

	Test_setModule(t, "GraphicsDevice/defaultLayout");

	const Allocator *alloc = Platform_instance->alloc;

	GraphicsDeviceInfo info = (GraphicsDeviceInfo) { 0 };
	GraphicsDeviceInfo infoRt = (GraphicsDeviceInfo) { 0 };
	infoRt.capabilities.features = EGraphicsFeatures_Raytracing;

	DescriptorLayoutInfo result = (DescriptorLayoutInfo) { 0 };

	Test_assert(t, "nullInfo", !GraphicsDevice_defaultBindlessLayout(NULL, ESHBinaryType_DXIL, &result, alloc, NULL));
	Test_assert(t, "nullResult", !GraphicsDevice_defaultBindlessLayout(&info, ESHBinaryType_DXIL, NULL, alloc, NULL));

	//A binary type without binding numbers is refused rather than silently handed DXIL's registers

	Test_assert(t, "unknownBinary", !GraphicsDevice_defaultBindlessLayout(
		&info, ESHBinaryType_Count, &result, alloc, NULL
	));

	Test_assert(t, "unknownLeftEmpty", !result.bindings.ptr && !result.bindingNames.ptr);

	//SPIRV without raytracing: samplers alone in set 0, everything else packed into set 1 in declaration order

	if(Test_assert(t, "spirv", GraphicsDevice_defaultBindlessLayout(
		&info, ESHBinaryType_SPIRV, &result, alloc, &t->err
	))) {

		Test_assert(t, "spirvCount", result.bindings.length == 12 && result.bindingNames.length == 12);
		Test_assert(t, "spirvFlags", result.flags == EDescriptorLayoutFlags_AllowBindlessOnArrays);

		//A non empty result is a leak about to happen, so it's refused before anything is written

		Test_assert(t, "nonEmptyRefused", !GraphicsDevice_defaultBindlessLayout(
			&info, ESHBinaryType_SPIRV, &result, alloc, NULL
		));

		const DescriptorBinding *b = result.bindings.ptr;

		Test_assert(t, "spirvSampler",
			b[0].registerType == ESHRegisterType_Sampler && !b[0].binding.space && !b[0].binding.binding
		);

		Test_assert(t, "spirvSamplerName", CharString_equalsCStringSensitive(&result.bindingNames.ptr[0], "_samplers"));

		Bool packed = true;
		Bool hasTlas = false;

		for(U64 i = 1; i < result.bindings.length; ++i) {
			packed &= b[i].binding.space == 1 && b[i].binding.binding == i - 1 && b[i].visibility == U32_MAX && b[i].count;
			hasTlas |= b[i].registerType == ESHRegisterType_AccelerationStructure;
		}

		Test_assert(t, "spirvPacked", packed);
		Test_assert(t, "spirvNoTlas", !hasTlas);

		DescriptorLayoutInfo_free(&result, alloc);
	}

	//Raytracing adds exactly one binding at the end rather than reshuffling anything before it

	if(Test_assert(t, "spirvRt", GraphicsDevice_defaultBindlessLayout(
		&infoRt, ESHBinaryType_SPIRV, &result, alloc, &t->err
	))) {

		Test_assert(t, "spirvRtCount", result.bindings.length == 13);

		const DescriptorBinding last = result.bindings.ptr[12];

		Test_assert(t, "spirvRtTlas",
			last.registerType == ESHRegisterType_AccelerationStructure &&
			last.binding.space == 1 && last.binding.binding == 11
		);

		Test_assert(t, "spirvRtTlasName", CharString_equalsCStringSensitive(&result.bindingNames.ptr[12], "_tlasExt"));

		DescriptorLayoutInfo_free(&result, alloc);
	}

	//DXIL: everything lives in space 0 and the ranges chain, SRVs through t and UAVs through u

	if(Test_assert(t, "dxil", GraphicsDevice_defaultBindlessLayout(
		&infoRt, ESHBinaryType_DXIL, &result, alloc, &t->err
	))) {

		const DescriptorBinding *b = result.bindings.ptr;

		Bool space0 = true;

		for(U64 i = 0; i < result.bindings.length; ++i)
			space0 &= !b[i].binding.space;

		Test_assert(t, "dxilSpace0", space0 && result.bindings.length == 13);
		Test_assert(t, "dxilSampler", !b[0].binding.binding);

		//[1] textures2D, [2] cubes, [3] 3D, [4] buffer, [12] tlas share the t namespace

		Test_assert(t, "dxilSrvChain",
			!b[1].binding.binding &&
			b[2].binding.binding == b[1].binding.binding + b[1].count &&
			b[3].binding.binding == b[2].binding.binding + b[2].count &&
			b[4].binding.binding == b[3].binding.binding + b[3].count &&
			b[12].binding.binding == b[4].binding.binding + b[4].count
		);

		//[5] rwBuffer then the six rw textures share the u namespace

		Bool uavChain = !b[5].binding.binding;

		for(U64 i = 6; i <= 11; ++i)
			uavChain &= b[i].binding.binding == b[i - 1].binding.binding + b[i - 1].count;

		Test_assert(t, "dxilUavChain", uavChain);

		DescriptorLayoutInfo_free(&result, alloc);
	}
}
