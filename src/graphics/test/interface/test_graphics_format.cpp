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

//graphics/test/interface/test_graphics_format.cpp

//Pure format, packing and layout modules (5, 6, 17, 18, 19, 24).
//No device is needed, so these always run, even where no adapter exists.

//This module owns no handles, so it needs nothing OUT of graphics.hpp; what it needs is the pre-include
//discipline that header performs, which every other converted module gets for free by including it.
//The C headers below reach the standard library twice over: sh_registers.h ends up at the SSE intrinsics
//and so at <stdlib.h>, and platform.h reaches atomic.h and so at <atomic>. Pulled in from inside oxc::c
//those declare their contents in oxc::c::std and then fail to resolve every ::std:: they refer to, which on
//libc++ is a wall of errors and on the Microsoft STL happens to be survivable. Including graphics.hpp first
//puts all of them at global scope, so the copies inside the block hit their include guards and do nothing.

#include "graphics/graphics.hpp"

namespace oxc { namespace c {
	#include "types/base/string_read_helper.h"
	#include "types/container/texture_format.h"
	#include "types/test/test.h"
	#include "formats/oiSH/sh_registers.h"
	#include "platforms/platform.h"
	#include "graphics/generic/bindless_descriptor.h"
	#include "graphics/generic/descriptor_layout.h"
	#include "graphics/generic/descriptor_table.h"
	#include "graphics/generic/device.h"
	#include "graphics/generic/device_info.h"
	#include "graphics/generic/tlas.h"
	#include "test_graphics_shared.h"
} }

//Same namespace the C headers landed in, so the definitions here match the declarations in
//test_graphics_shared.h and the macros in those headers still expand to names that resolve.

// -- 5. Texture format helpers --------------------------------------------------

extern "C" void Test_graphicsFormats(oxc::c::Test *t) {

	using namespace oxc;

	c::Test_setModule(t, "TextureFormat");

	Test_assert(t, "rgba8Size", c::ETextureFormat_getSize(c::ETextureFormat_RGBA8, 1, 1, 1) == 4);
	Test_assert(t, "rgba16fSize", c::ETextureFormat_getSize(c::ETextureFormat_RGBA16f, 1, 1, 1) == 8);
	Test_assert(t, "rgba32fSize", c::ETextureFormat_getSize(c::ETextureFormat_RGBA32f, 1, 1, 1) == 16);

	Test_assert(t, "rgba8Uncompressed", !c::ETextureFormat_getIsCompressed(c::ETextureFormat_RGBA8));
	Test_assert(t, "bc7Compressed", c::ETextureFormat_getIsCompressed(c::ETextureFormat_BC7));

	//4x4 BC7 block = 16 bytes (misaligned blocks handled the same)

	Test_assert(t, "bc7BlockSize4", c::ETextureFormat_getSize(c::ETextureFormat_BC7, 4, 4, 1) == 16);
	Test_assert(t, "bc7BlockSize2", c::ETextureFormat_getSize(c::ETextureFormat_BC7, 2, 2, 1) == 16);
	Test_assert(t, "bc7BlockSize1", c::ETextureFormat_getSize(c::ETextureFormat_BC7, 1, 1, 1) == 16);

	//Vertex attributes exclude compressed formats
	Test_assert(t, "rgba8VertexAttrib", c::GraphicsDeviceInfo_supportsFormatVertexAttribute(c::ETextureFormat_RGBA8));
	Test_assert(t, "bc7NoVertexAttrib", !c::GraphicsDeviceInfo_supportsFormatVertexAttribute(c::ETextureFormat_BC7));

	//ETextureFormatId <-> ETextureFormat mapping is consistent
	Test_assert(t, "unpackRGBA8", c::ETextureFormatId_unpack[c::ETextureFormatId_RGBA8] == c::ETextureFormat_RGBA8);
	Test_assert(t, "unpackUndefined", c::ETextureFormatId_unpack[c::ETextureFormatId_Undefined] == c::ETextureFormat_Undefined);

	//The support queries only read capabilities.dataTypes, so a synthesised info exercises both answers without
	// needing the adapter to happen to have or lack the type.

	c::GraphicsDeviceInfo none {};
	c::GraphicsDeviceInfo all {};

	//Cast because C++ types an OR of enumerators as int, unlike C.

	all.capabilities.dataTypes = (c::EGraphicsDataTypes) (
		c::EGraphicsDataTypes_BCn | c::EGraphicsDataTypes_ASTC | c::EGraphicsDataTypes_RGB32f |
		c::EGraphicsDataTypes_D24S8 | c::EGraphicsDataTypes_S8
	);

	//Anything not gated by a data type is supported regardless of the device

	Test_assert(t, "rgba8Always", c::GraphicsDeviceInfo_supportsFormat(&none, c::ETextureFormat_RGBA8));

	Test_assert(t, "bc7NeedsBCn",
		!c::GraphicsDeviceInfo_supportsFormat(&none, c::ETextureFormat_BC7) &&
		c::GraphicsDeviceInfo_supportsFormat(&all, c::ETextureFormat_BC7)
	);

	Test_assert(t, "rgb32fNeedsType",
		!c::GraphicsDeviceInfo_supportsFormat(&none, c::ETextureFormat_RGB32f) &&
		c::GraphicsDeviceInfo_supportsFormat(&all, c::ETextureFormat_RGB32f)
	);

	//Render targets additionally exclude compressed formats, even where the device can sample them

	Test_assert(t, "bc7NoRenderTarget", !c::GraphicsDeviceInfo_supportsRenderTextureFormat(&all, c::ETextureFormat_BC7));
	Test_assert(t, "rgba8RenderTarget", c::GraphicsDeviceInfo_supportsRenderTextureFormat(&none, c::ETextureFormat_RGBA8));

	//Plain depth needs nothing, the stencil combinations each need their own data type

	Test_assert(t, "d32Always", c::GraphicsDeviceInfo_supportsDepthStencilFormat(&none, c::EDepthStencilFormat_D32));

	Test_assert(t, "d24s8NeedsType",
		!c::GraphicsDeviceInfo_supportsDepthStencilFormat(&none, c::EDepthStencilFormat_D24S8Ext) &&
		c::GraphicsDeviceInfo_supportsDepthStencilFormat(&all, c::EDepthStencilFormat_D24S8Ext)
	);

	Test_assert(
		t, "d32s8NeedsType", !c::GraphicsDeviceInfo_supportsDepthStencilFormat(&all, c::EDepthStencilFormat_D32S8X24Ext)
	);

	Test_assert(t, "supportsNullDevice",
		!c::GraphicsDeviceInfo_supportsFormat(NULL, c::ETextureFormat_RGBA8) &&
		!c::GraphicsDeviceInfo_supportsDepthStencilFormat(NULL, c::EDepthStencilFormat_D32)
	);
}

// -- 6. Bindless descriptor packing (pure, no device) ----------------------------

extern "C" void Test_bindlessDescriptorPacking(oxc::c::Test *t) {

	using namespace oxc;

	c::Test_setModule(t, "BindlessDescriptor/pack");

	//A handle is a 4 bit bindless type (0 is reserved) followed by a 17 bit array index, so 21 bits in total.
	//Three of them fit in a U64, which is what pack3/unpack3 are for.

	const c::BindlessDescriptor a = ((c::BindlessDescriptor)1 << 17) | 1;
	const c::BindlessDescriptor b = ((c::BindlessDescriptor)2 << 17) | 1234;
	const c::BindlessDescriptor c = ((c::BindlessDescriptor)15 << 17) | ((1 << 17) - 1);

	Test_assert(t, "getBindlessType", c::BindlessDescriptor_getBindlessType(b) == 2);
	Test_assert(t, "getId", c::BindlessDescriptor_getId(b) == 1234);
	Test_assert(t, "maxHandleFits", c == (c::BindlessDescriptor)((1 << 21) - 1));

	const c::U64 packed = c::BindlessDescriptor_pack3(a, b, c);
	Test_assert(t, "pack3", packed != c::U64_MAX);        //U64_MAX marks a component that didn't fit in 21 bits

	const c::I32x4 unpacked = c::BindlessDescriptor_unpack3(packed);

	Test_assert(t, "unpack3X", (c::BindlessDescriptor) c::I32x4_x(unpacked) == a);
	Test_assert(t, "unpack3Y", (c::BindlessDescriptor) c::I32x4_y(unpacked) == b);
	Test_assert(t, "unpack3Z", (c::BindlessDescriptor) c::I32x4_z(unpacked) == c);

	//None is a handle like any other to pack3, it just doesn't address a descriptor.

	const c::U64 none = c::BindlessDescriptor_pack3(
		c::BindlessDescriptor_None, c::BindlessDescriptor_None, c::BindlessDescriptor_None
	);
	Test_assert(t, "pack3None", none == 0);

	const c::I32x4 unpackedNone = c::BindlessDescriptor_unpack3(none);

	Test_assert(t, "unpack3None", !c::I32x4_x(unpackedNone) && !c::I32x4_y(unpackedNone) && !c::I32x4_z(unpackedNone));

	//Anything wider than 21 bits can't be packed, whichever component it is.

	const c::BindlessDescriptor tooWide = (c::BindlessDescriptor)1 << 21;

	Test_assert(t, "pack3OverflowX", c::BindlessDescriptor_pack3(tooWide, 0, 0) == c::U64_MAX);
	Test_assert(t, "pack3OverflowY", c::BindlessDescriptor_pack3(0, tooWide, 0) == c::U64_MAX);
	Test_assert(t, "pack3OverflowZ", c::BindlessDescriptor_pack3(0, 0, tooWide) == c::U64_MAX);
}

// -- 18. Descriptor packing (pure, no device) ------------------------------------

//A buffer descriptor keeps a 48 bit start and end region, and hides the 32 bit counter offset in the top 16 bits of
// each of those two U64s.
//A counter offset with both halves set is what proves the split and the masking agree; one that fits in 16 bits
// would pass even if the high half were dropped.

extern "C" void Test_descriptorPacking(oxc::c::Test *t) {

	using namespace oxc;

	c::Test_setModule(t, "Descriptor/pack");

	const c::U64 start = 0x1000, end = 0x5000;
	const c::U32 counterOffset = 0xABCD1234;

	const c::Descriptor buf = c::Descriptor_buffer(NULL, start, end, NULL, counterOffset);

	Test_assert(t, "start", c::Descriptor_startBuffer(&buf) == start);
	Test_assert(t, "end", c::Descriptor_endBuffer(&buf) == end);
	Test_assert(t, "length", c::Descriptor_bufferLength(&buf) == end - start);
	Test_assert(t, "counterOffset", c::Descriptor_counterOffset(&buf) == counterOffset);

	//48 bits is the widest region that survives sharing its U64 with the counter offset

	const c::U64 max48 = ((c::U64)1 << 48) - 1;
	const c::Descriptor wide = c::Descriptor_buffer(NULL, 0, max48, NULL, counterOffset);

	Test_assert(t, "max48End", c::Descriptor_endBuffer(&wide) == max48);
	Test_assert(t, "max48Length", c::Descriptor_bufferLength(&wide) == max48);
	Test_assert(t, "max48Counter", c::Descriptor_counterOffset(&wide) == counterOffset);

	//Anything wider is refused with a null descriptor rather than silently truncated into the counter offset

	const c::Descriptor tooWideStart = c::Descriptor_buffer(NULL, (c::U64)1 << 48, end, NULL, 0);
	const c::Descriptor tooWideEnd = c::Descriptor_buffer(NULL, start, (c::U64)1 << 48, NULL, 0);

	Test_assert(t, "tooWideStart", !c::Descriptor_startBuffer(&tooWideStart) && !c::Descriptor_endBuffer(&tooWideStart));
	Test_assert(t, "tooWideEnd", !c::Descriptor_startBuffer(&tooWideEnd) && !c::Descriptor_endBuffer(&tooWideEnd));

	//Texture descriptors share the same union, so their fields have to land where the range isn't

	const c::Descriptor tex = c::Descriptor_texture(NULL, 1, 2, 3, 4, 5, 6);

	Test_assert(t, "textureMip", tex.texture.mipId == 1 && tex.texture.mipCount == 2);
	Test_assert(t, "texturePlane", tex.texture.planeId == 3 && tex.texture.imageId == 4);
	Test_assert(t, "textureArray", tex.texture.arrayId == 5 && tex.texture.arrayCount == 6);

	//tlas and sampler carry only the resource, so nothing may read back as a range

	const c::Descriptor tlas = c::Descriptor_tlas(NULL);
	const c::Descriptor sampler = c::Descriptor_sampler(NULL);

	Test_assert(t, "tlasEmpty", !c::Descriptor_startBuffer(&tlas) && !c::Descriptor_endBuffer(&tlas) && !tlas.resource);
	Test_assert(t, "samplerEmpty", !c::Descriptor_bufferLength(&sampler) && !sampler.resource);

	Test_assert(t, "nullDescriptor",
		!c::Descriptor_startBuffer(NULL) && !c::Descriptor_endBuffer(NULL) &&
		!c::Descriptor_bufferLength(NULL) && !c::Descriptor_counterOffset(NULL)
	);
}

// -- 19. TextureRange (pure, no device) ------------------------------------------

extern "C" void Test_textureRange(oxc::c::Test *t) {

	using namespace oxc;

	c::Test_setModule(t, "TextureRange");

	const c::TextureRange r = { .startRange = { 1, 2, 3 }, .endRange = { 11, 22, 33 }, .levelId = 4 };

	Test_assert(t, "width", c::TextureRange_width(r) == 10);
	Test_assert(t, "height", c::TextureRange_height(r) == 20);
	Test_assert(t, "length", c::TextureRange_length(r) == 30);

	//An empty range is legal and measures zero, it isn't an error value

	const c::TextureRange empty {};

	Test_assert(t, "emptyWidth", !c::TextureRange_width(empty));
	Test_assert(t, "emptyHeight", !c::TextureRange_height(empty));
	Test_assert(t, "emptyLength", !c::TextureRange_length(empty));
}

// -- 24. Default bindless layout (pure, no device) -------------------------------

//What OxC3's own shaders are compiled against, so the numbers here are a contract with resources.hlsli rather
// than an implementation detail that may drift.
//The DXIL offsets form a chain where each register range starts exactly where the previous one ends, which pins
// the exact numbers without this test needing the private count enums.

extern "C" void Test_graphicsDefaultBindlessLayout(oxc::c::Test *t) {

	using namespace oxc;

	c::Test_setModule(t, "GraphicsDevice/defaultLayout");

	const c::Allocator *alloc = c::Platform_instance->alloc;

	c::GraphicsDeviceInfo info {};
	c::GraphicsDeviceInfo infoRt {};
	infoRt.capabilities.features = c::EGraphicsFeatures_Raytracing;

	c::DescriptorLayoutInfo result {};

	Test_assert(t, "nullInfo", !c::GraphicsDevice_defaultBindlessLayout(
		NULL, c::EGfxBinaryType_DXIL, c::EGraphicsDeviceFlags_None, &result, alloc, NULL
	));
	Test_assert(t, "nullResult", !c::GraphicsDevice_defaultBindlessLayout(
		&info, c::EGfxBinaryType_DXIL, c::EGraphicsDeviceFlags_None, NULL, alloc, NULL
	));

	//A binary type without binding numbers is refused rather than silently handed DXIL's registers

	Test_assert(t, "unknownBinary", !c::GraphicsDevice_defaultBindlessLayout(
		&info, c::EGfxBinaryType_Count, c::EGraphicsDeviceFlags_None, &result, alloc, NULL
	));

	Test_assert(t, "unknownLeftEmpty", !result.bindings.ptr && !result.bindingNames.ptr);

	//SPIRV without raytracing: samplers alone in set 0, everything else packed into set 1 in declaration order

	if(Test_assert(t, "spirv", c::GraphicsDevice_defaultBindlessLayout(
		&info, c::EGfxBinaryType_SPIRV, c::EGraphicsDeviceFlags_EnableDynamicSamplers, &result, alloc, &t->err
	))) {

		Test_assert(t, "spirvCount", result.bindings.length == 12 && result.bindingNames.length == 12);
		Test_assert(t, "spirvFlags", result.flags == c::EDescriptorLayoutFlags_AllowBindlessOnArrays);

		//A non empty result is a leak about to happen, so it's refused before anything is written

		Test_assert(t, "nonEmptyRefused", !c::GraphicsDevice_defaultBindlessLayout(
			&info, c::EGfxBinaryType_SPIRV, c::EGraphicsDeviceFlags_EnableDynamicSamplers, &result, alloc, NULL
		));

		const c::DescriptorBinding *b = result.bindings.ptr;

		Test_assert(t, "spirvSampler",
			b[0].registerType == c::EGfxRegisterType_Sampler && !b[0].binding.space && !b[0].binding.binding
		);

		Test_assert(t, "spirvSamplerName", c::CharString_equalsCStringSensitive(&result.bindingNames.ptr[0], "_samplers"));

		c::Bool packed = true;
		c::Bool hasTlas = false;

		for(c::U64 i = 1; i < result.bindings.length; ++i) {
			packed &= b[i].binding.space == 1 && b[i].binding.binding == i - 1 && b[i].visibility == c::U32_MAX && b[i].count;
			hasTlas |= b[i].registerType == c::EGfxRegisterType_AccelerationStructure;
		}

		Test_assert(t, "spirvPacked", packed);
		Test_assert(t, "spirvNoTlas", !hasTlas);

		c::DescriptorLayoutInfo_free(&result, alloc);
	}

	//Without EnableDynamicSamplers the _samplers[] binding is gone entirely, which is the whole point of the
	// flag: no sampler heap, no sampler root table, and on Vulkan one fewer set out of the four there are.
	//Everything else keeps its own binding number, since only set 0 held the sampler.

	if(Test_assert(t, "spirvNoDynamicSamplers", c::GraphicsDevice_defaultBindlessLayout(
		&info, c::EGfxBinaryType_SPIRV, c::EGraphicsDeviceFlags_None, &result, alloc, &t->err
	))) {

		Test_assert(t, "noSamplerCount", result.bindings.length == 11 && result.bindingNames.length == 11);

		c::Bool anySampler = false;

		for(c::U64 i = 0; i < result.bindings.length; ++i) {

			const c::EGfxRegisterType type =
				(c::EGfxRegisterType)(result.bindings.ptr[i].registerType & c::EGfxRegisterType_TypeMask);

			anySampler |=
				type == c::EGfxRegisterType_Sampler || type == c::EGfxRegisterType_SamplerComparisonState;
		}

		Test_assert(t, "noSamplerBinding", !anySampler);

		//The first entry is now what used to sit behind the sampler, still at set 1 binding 0

		Test_assert(t, "noSamplerFirstIsTexture",
			result.bindings.ptr[0].registerType == c::EGfxRegisterType_Texture2D &&
			result.bindings.ptr[0].binding.space == 1 && !result.bindings.ptr[0].binding.binding
		);

		Test_assert(t, "noSamplerFirstName",
			c::CharString_equalsCStringSensitive(&result.bindingNames.ptr[0], "_textures2D")
		);

		c::DescriptorLayoutInfo_free(&result, alloc);
	}

	//Raytracing adds exactly one binding at the end rather than reshuffling anything before it

	if(Test_assert(t, "spirvRt", c::GraphicsDevice_defaultBindlessLayout(
		&infoRt, c::EGfxBinaryType_SPIRV, c::EGraphicsDeviceFlags_EnableDynamicSamplers, &result, alloc, &t->err
	))) {

		Test_assert(t, "spirvRtCount", result.bindings.length == 13);

		const c::DescriptorBinding last = result.bindings.ptr[12];

		Test_assert(t, "spirvRtTlas",
			last.registerType == c::EGfxRegisterType_AccelerationStructure &&
			last.binding.space == 1 && last.binding.binding == 11
		);

		Test_assert(t, "spirvRtTlasName", c::CharString_equalsCStringSensitive(&result.bindingNames.ptr[12], "_tlasExt"));

		c::DescriptorLayoutInfo_free(&result, alloc);
	}

	//DXIL: everything lives in OxC3's reserved space (not space 0, which callers get to use) and the ranges
	// chain, SRVs through t and UAVs through u

	//The shape below is the full 13 binding layout with _samplers at [0], which is the dynamic sampler
	// variant; without the flag the sampler binding is gone and every index shifts down one.

	if(Test_assert(t, "dxil", c::GraphicsDevice_defaultBindlessLayout(
		&infoRt, c::EGfxBinaryType_DXIL, c::EGraphicsDeviceFlags_EnableDynamicSamplers, &result, alloc, &t->err
	))) {

		const c::DescriptorBinding *b = result.bindings.ptr;

		c::Bool reservedSpace = true;

		for(c::U64 i = 0; i < result.bindings.length; ++i)
			reservedSpace &= b[i].binding.space == OXC3_RESERVED_SPACE;

		Test_assert(t, "dxilReservedSpace", reservedSpace && result.bindings.length == 13);
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

		c::Bool uavChain = !b[5].binding.binding;

		for(c::U64 i = 6; i <= 11; ++i)
			uavChain &= b[i].binding.binding == b[i - 1].binding.binding + b[i - 1].count;

		Test_assert(t, "dxilUavChain", uavChain);

		c::DescriptorLayoutInfo_free(&result, alloc);
	}
}
