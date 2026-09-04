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

//graphics/generic/descriptor_layout.h

#pragma once
#include "formats/oiSH/sh_file.h"
#include "types/container/list.h"
#include "types/container/ref_ptr.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct Error Error;
typedef struct SHFile SHFile;
typedef struct RefPtr RefPtr;
typedef enum EGfxRegisterType EGfxRegisterType;

typedef RefPtr GraphicsDeviceRef;

//The register space OxC3 keeps for itself on DXIL, where the per frame globals live (frame id, time,
//swapchain descriptors). Space 0 was the old home and is far too easy to pick by accident: it is
//what anyone writing their first constant buffer types, and custom layouts made that a real collision.
//Vulkan needs no equivalent, since the globals sit in their own descriptor SET there and set indices are
//too few to hide a reservation in.
//Keep this in sync with OXC3_RESERVED_SPACE in shader_compiler/shaders/types.hlsli; a shader compiled
//against a different value binds somewhere this runtime doesn't look.

//How many push descriptors one layout may hold. On D3D12 each is a root descriptor costing 2 of the root
//signature's 64 DWORDs, and Vulkan only guarantees a maxPushDescriptors at all where the extension exists.

#define OXC3_MAX_PUSH_DESCRIPTORS 8

typedef enum EDescriptorLayoutFlags {

	EDescriptorLayoutFlags_None                     = 0,
	EDescriptorLayoutFlags_AllowBindlessOnArrays    = 1 << 0,        //Required to use bindless, only enables bindless on desc[]
	EDescriptorLayoutFlags_AllowBindlessEverywhere  = 1 << 1,        //Potentially slower, will assume all registers are dynamic
	EDescriptorLayoutFlags_InternalWeakDeviceRef    = 1 << 2,
	EDescriptorLayoutFlags_HasPushDescriptors       = 1 << 3,        //Push descriptor set, no other descriptors allowed

	EDescriptorLayoutFlags_AllowBindlessAny         =
		EDescriptorLayoutFlags_AllowBindlessOnArrays | EDescriptorLayoutFlags_AllowBindlessEverywhere

} EDescriptorLayoutFlags;

typedef U16 DescriptorBindingFlags;

typedef struct DescriptorBinding {

	EGfxRegisterType registerType;

	U32 count;

	GfxBinding binding;

	U32 visibility;            //Bit mask of EGfxPipelineStage

	union {
		U32 structedBufferStride;
		U32 constantBufferSize;
		GfxTextureFormat textureFormat;

		//Sampler bindings only: 1 + an index into DescriptorLayoutInfo::immutableSamplers, 0 for none.
		//An immutable sampler is baked into the layout rather than bound, so it needs no heap slot and no
		// write: D3D12 puts it in the root signature as a static sampler and Vulkan in the set layout as
		// pImmutableSamplers.
		//An index rather than the ref itself, because a pointer does not fit in this union and widening it
		// would grow every binding.

		U32 immutableSamplerId;

		U32 data;
	};

} DescriptorBinding;

//immutableSamplerId shares the union above with every other class's data (a texture's format, a buffer's
// stride), so reading the field raw misreads any non sampler binding whose union is nonzero as an immutable
// sampler; THIS is the only way it may be read.

static inline U32 DescriptorBinding_immutableSamplerId(DescriptorBinding b) {

	const EGfxRegisterType type = (EGfxRegisterType)(b.registerType & EGfxRegisterType_TypeMask);

	if(type != EGfxRegisterType_Sampler && type != EGfxRegisterType_SamplerComparisonState)
		return 0;

	return b.immutableSamplerId;
}

Bool DescriptorBinding_overlaps(
	const DescriptorBinding *binding,
	EGfxRegisterType regType,
	const GfxBinding *b,
	U32 bcount,
	EGfxBinaryType type,
	Bool isPushConstant
);

TList(DescriptorBinding);

//Samplers a binding can name with immutableSamplerId.
//Held by ref rather than by value so several layouts naming the same sampler share one VkSampler instead of
// each minting its own; D3D12 never makes an object of one at all and only reads its info.

typedef RefPtr SamplerRef;

typedef struct DescriptorLayoutInfo {

	EDescriptorLayoutFlags flags;
	U32 padding;

	ListDescriptorBinding bindings;
	ListCharString bindingNames;

	//Owned: DescriptorLayoutInfo_free releases a ref on each, and createDescriptorLayout moves the list into
	// the layout along with everything else here.

	ListRefPtr immutableSamplers;

} DescriptorLayoutInfo;

//Takes a ref and hands back the id to put in a sampler binding's immutableSamplerId (never 0).

Bool DescriptorLayoutInfo_addImmutableSampler(
	DescriptorLayoutInfo *info,
	SamplerRef *sampler,
	U32 *id,
	const Allocator *alloc,
	Error *e_rr
);

typedef enum EDetectDescriptorLayoutFlags {

	EDetectDescriptorLayoutFlags_None                    = 0,

	//These should only be used if the contents are so frequently changing that allocating a descriptor set makes no sense.
	//For example a copy image or mip map shader, otherwise use None.
	//These only apply on non arrays, since those could be used for bindless for example.
	//Samplers need static samplers, doesn't work with push descriptors.

	EDetectDescriptorLayoutFlags_AssumePushConstants     = 1 << 0,       //First buffer (<128 bytes) receives a push constant
	EDetectDescriptorLayoutFlags_AssumePushDescriptors   = 1 << 1        //Assume non push constant buffer as push descriptors

} EDetectDescriptorLayoutFlags;

//Whether a reflected register name is one the RUNTIME owns (the bindless set or the per frame globals)
//rather than the caller's. Names, not spaces: the spaces differ per backend.

Bool GraphicsDeviceRef_isRuntimeRegister(GraphicsDeviceRef *dev, const CharString *name);

//Every runtime owned register name as refs into the device's layouts; the list is the caller's, the refs the device's.

Bool GraphicsDeviceRef_runtimeRegisterNames(
	GraphicsDeviceRef *dev, ListCharString *out, const Allocator *alloc, Error *e_rr
);

Bool GraphicsDeviceRef_detectLayoutFromEntries(
	GraphicsDeviceRef *dev,
	const SHFile *tmpBinary,
	const ListU32 *entrypoints,                //U32 (U16 entryId, binaryId)
	EDescriptorLayoutFlags flags,
	EDetectDescriptorLayoutFlags detectFlags,
	const ListCharString *pushDescriptors,     //Empty if no push descriptors or if AssumePushConstants
	const CharString *pushConstantName,        //Empty if no push constants or AssumePushConstants
	DescriptorBinding *pushConstantOut,
	DescriptorLayoutInfo *info,
	DescriptorLayoutInfo *pushDescriptorInfo,
	Error *e_rr
);

Bool GraphicsDeviceRef_detectLayoutFromEntry(
	GraphicsDeviceRef *dev,
	const SHFile *binary,
	U32 entrypoint,                            //U32 (U16 entryId, binaryId)
	EDescriptorLayoutFlags flags,
	EDetectDescriptorLayoutFlags detectFlags,
	const ListCharString *pushDescriptors,     //Empty if no push descriptors or if AssumePushDescriptors
	const CharString *pushConstantName,        //Empty if no push constants or AssumePushConstants
	DescriptorBinding *pushConstantOut,
	DescriptorLayoutInfo *info,
	DescriptorLayoutInfo *pushDescriptorInfo,
	Error *e_rr
);

void DescriptorLayoutInfo_free(DescriptorLayoutInfo *info, const Allocator *alloc);

typedef RefPtr GraphicsDeviceRef;
typedef RefPtr DescriptorLayoutRef;

typedef struct DescriptorLayout {

	GraphicsDeviceRef *device;
	DescriptorLayoutInfo info;

	ListU16 bindlessTypeToBinding;
	ListU8 bindingToBindlessType;        //U8_MAX indicates "none"

	Bool anySampler, anyResource;
	U8 padding[14];

} DescriptorLayout;

#define DescriptorLayout_ext(ptr, T) (!ptr ? NULL : (T##DescriptorLayout*)(ptr + 1))        //impl
#define DescriptorLayoutRef_ptr(ptr) RefPtr_data(ptr, DescriptorLayout)

Bool GraphicsDeviceRef_createDescriptorLayout(
	GraphicsDeviceRef *dev,
	DescriptorLayoutInfo *info,        //Moves info
	const CharString *name,
	DescriptorLayoutRef **layout,
	Error *e_rr
);

#ifdef __cplusplus
	}
#endif
