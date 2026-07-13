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

//graphics/d3d12/generic/dx_descriptor_layout.c

#include "types/container/list_impl.h"
#include "graphics/generic/descriptor_layout.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/d3d12/dx_device.h"
#include "formats/oiSH/sh_entries.h"
#include "types/container/string.h"
#include "types/container/list_basic_types.h"
#include "types/base/constants.h"

TListImpl(D3D12_DESCRIPTOR_RANGE1)
TListImpl(D3D12_ROOT_PARAMETER1)

void DX_WRAP_FUNC(DescriptorLayout_free)(DescriptorLayout *layout, const Allocator *alloc) {
	(void) alloc;
	DxDescriptorLayout *layoutExt = DescriptorLayout_ext(layout, Dx);
	ListU32_free(&layoutExt->bindingOffsets, alloc);
	ListD3D12_DESCRIPTOR_RANGE1_free(&layoutExt->rangesResources, alloc);
	ListD3D12_DESCRIPTOR_RANGE1_free(&layoutExt->rangesSamplers, alloc);
	ListU8_free(&layoutExt->rootParamOffsets, alloc);
	ListD3D12_ROOT_PARAMETER1_free(&layoutExt->rootParams, alloc);
}

static inline D3D12_DESCRIPTOR_RANGE_TYPE dxGetDescriptorType(ESHRegisterType regType) {

	switch (regType & ESHRegisterType_TypeMask) {

		case ESHRegisterType_Sampler:
		case ESHRegisterType_SamplerComparisonState:
			return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;

		case ESHRegisterType_ConstantBuffer:
			return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;

		case ESHRegisterType_AccelerationStructure:
			return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

		case ESHRegisterType_SubpassInput:        //Doesn't exist in D3D12
			return (D3D12_DESCRIPTOR_RANGE_TYPE) -1;

		default:
		case ESHRegisterType_ByteAddressBuffer:
		case ESHRegisterType_StructuredBuffer:
		case ESHRegisterType_StructuredBufferAtomic:
		case ESHRegisterType_StorageBuffer:
		case ESHRegisterType_StorageBufferAtomic:
		case ESHRegisterType_Texture1D:
		case ESHRegisterType_Texture2D:
		case ESHRegisterType_Texture3D:
		case ESHRegisterType_TextureCube:
		case ESHRegisterType_Texture2DMS:
			return regType & ESHRegisterType_IsWrite ? D3D12_DESCRIPTOR_RANGE_TYPE_UAV : D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	}
}

typedef struct SortingKey {
	const DescriptorBinding *binding;
	U32 mergedCount;                        //Useful to allow merging later
	U32 padding;
} SortingKey;

static inline ECompareResult SortingKey_compare(const SortingKey *aKey, const SortingKey *bKey) {

	const DescriptorBinding *a = aKey->binding;
	const DescriptorBinding *b = bKey->binding;

	if(a->binding.space != b->binding.space)
		return a->binding.space < b->binding.space ? ECompareResult_Lt : ECompareResult_Gt;

	D3D12_DESCRIPTOR_RANGE_TYPE registerTypeA = dxGetDescriptorType(a->registerType);
	D3D12_DESCRIPTOR_RANGE_TYPE registerTypeB = dxGetDescriptorType(b->registerType);

	if(registerTypeA != registerTypeB)
		return registerTypeA < registerTypeB ? ECompareResult_Lt : ECompareResult_Gt;

	if(a->binding.binding != b->binding.binding)
		return a->binding.binding < b->binding.binding ? ECompareResult_Lt : ECompareResult_Gt;

	return ECompareResult_Eq;
}

D3D12_SHADER_VISIBILITY DxDescriptorLayout_convertVisibility(U32 a) {
	switch (a) {
		case 1 << ESHPipelineStage_Vertex:      return D3D12_SHADER_VISIBILITY_VERTEX;
		case 1 << ESHPipelineStage_Pixel:       return D3D12_SHADER_VISIBILITY_PIXEL;
		case 1 << ESHPipelineStage_Hull:        return D3D12_SHADER_VISIBILITY_HULL;
		case 1 << ESHPipelineStage_Domain:      return D3D12_SHADER_VISIBILITY_DOMAIN;
		case 1 << ESHPipelineStage_GeometryExt: return D3D12_SHADER_VISIBILITY_GEOMETRY;
		case 1 << ESHPipelineStage_MeshExt:     return D3D12_SHADER_VISIBILITY_MESH;
		case 1 << ESHPipelineStage_TaskExt:     return D3D12_SHADER_VISIBILITY_AMPLIFICATION;
		default:                                return D3D12_SHADER_VISIBILITY_ALL;
	}
}

TList(SortingKey);
TListImpl(SortingKey);

Bool DX_WRAP_FUNC(GraphicsDeviceRef_createDescriptorLayout)(
	GraphicsDeviceRef *dev, DescriptorLayout *layout, const CharString *name, Error *e_rr
) {

	Bool s_uccess = true;
	const Allocator *alloc = GraphicsDeviceRef_getAlloc(dev);

	(void) name;

	ListSortingKey sortedList = (ListSortingKey) { 0 };

	DxDescriptorLayout *layoutExt = DescriptorLayout_ext(layout, Dx);

	const DescriptorLayoutInfo info = layout->info;

	gotoIfError3(clean, ListSortingKey_reserve(&sortedList, info.bindings.length, alloc, e_rr));
	gotoIfError3(clean, ListU8_resize(&layoutExt->rootParamOffsets, info.bindings.length, alloc, e_rr));
	gotoIfError3(clean, ListD3D12_ROOT_PARAMETER1_reserve(&layoutExt->rootParams, 4, alloc, e_rr));      //Samp, Res, NV, pad

	//Sort by set and merge shaders that allow it and check we only have 4 sets bound

	U32 visibility1 = 0;
	U32 visibility2 = 0;

	U8 resourceParam = U8_MAX, samplerParam = U8_MAX;

	Bool isPushDescriptors = info.flags & EDescriptorLayoutFlags_HasPushDescriptors;

	for(U64 i = 0; i < info.bindings.length; ++i) {

		const DescriptorBinding *binding = &info.bindings.ptr[i];

		if(isPushDescriptors) {

			layoutExt->rootParamOffsets.ptrNonConst[i] = (U8) layoutExt->rootParams.length;

			D3D12_ROOT_PARAMETER_TYPE type = D3D12_ROOT_PARAMETER_TYPE_SRV;

			if(binding->registerType == ESHRegisterType_ConstantBuffer)
				type = D3D12_ROOT_PARAMETER_TYPE_CBV;

			else if(binding->registerType & ESHRegisterType_IsWrite)
				type = D3D12_ROOT_PARAMETER_TYPE_UAV;

			D3D12_DESCRIPTOR_RANGE_FLAGS bindFlags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
			D3D12_ROOT_DESCRIPTOR_FLAGS rootFlags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE;

			if (type == D3D12_ROOT_PARAMETER_TYPE_CBV) {
				bindFlags = 0;
				rootFlags = 0;
			}

			if(
				!(info.flags & EDescriptorLayoutFlags_AllowBindlessEverywhere) &&
				!((info.flags & EDescriptorLayoutFlags_AllowBindlessOnArrays) && binding->count > 1)
			) {
				bindFlags = 0;
				rootFlags = 0;
			}

			gotoIfError3(clean, ListD3D12_ROOT_PARAMETER1_pushBack(&layoutExt->rootParams, (D3D12_ROOT_PARAMETER1) {
				.ParameterType = type,
				.Descriptor = (D3D12_ROOT_DESCRIPTOR1) {
					.ShaderRegister = binding->binding.binding,
					.RegisterSpace = binding->binding.space,
					.Flags = rootFlags
				},
				.ShaderVisibility = DxDescriptorLayout_convertVisibility(binding->visibility)
			}, alloc, e_rr));

			continue;
		}

		//Remember visibility for root params

		if (
			binding->registerType == ESHRegisterType_Sampler ||
			binding->registerType  == ESHRegisterType_SamplerComparisonState
		) {

			visibility2 |= binding->visibility;

			if(samplerParam == U8_MAX) {
				samplerParam = (U8) layoutExt->rootParams.length;
				gotoIfError3(clean, ListD3D12_ROOT_PARAMETER1_pushBack(
					&layoutExt->rootParams,
					(D3D12_ROOT_PARAMETER1) { 0 },
					alloc,
					e_rr
				));
			}

			layoutExt->rootParamOffsets.ptrNonConst[i] = samplerParam;
		}

		else {

			visibility1 |= binding->visibility;

			if(resourceParam == U8_MAX) {
				resourceParam = (U8) layoutExt->rootParams.length;
				gotoIfError3(clean, ListD3D12_ROOT_PARAMETER1_pushBack(
					&layoutExt->rootParams,
					(D3D12_ROOT_PARAMETER1) { 0 },
					alloc,
					e_rr
				));
			}

			layoutExt->rootParamOffsets.ptrNonConst[i] = resourceParam;
		}

		gotoIfError3(clean, ListSortingKey_pushBack(&sortedList, (SortingKey) { binding }, alloc, e_rr));
	}

	if(!ListSortingKey_sortCustom(sortedList, (CompareFunction) SortingKey_compare))
		retError(clean, Error_invalidState(
			0, "GraphicsDeviceRef_createDescriptorLayout can't sort list"
		));

	//Collapse nearby bindings if possible

	for (U64 i = sortedList.length - 1; i != U64_MAX && i; --i) {

		//Combining is only allowed if same space, id and type.
		//And if bindless on arrays is on; we don't want to combine non bindless + bindless in one.
		//This would not allow us to turn off descriptor flags we don't want turned on.

		DescriptorBinding a = *sortedList.ptr[i - 1].binding;
		DescriptorBinding b = *sortedList.ptr[i].binding;

		U32 aCount = sortedList.ptr[i - 1].mergedCount ? sortedList.ptr[i - 1].mergedCount : a.count;
		U32 bCount = sortedList.ptr[i].mergedCount ? sortedList.ptr[i].mergedCount : b.count;

		if(
			dxGetDescriptorType(a.registerType) != dxGetDescriptorType(b.registerType) ||
			a.binding.space != b.binding.space ||
			a.binding.binding + aCount != b.binding.binding ||
			((info.flags & EDescriptorLayoutFlags_AllowBindlessOnArrays) && (a.count > 1) != (b.count > 1))
		)
			continue;

		//We've found a match, let's shorten the array and remember how many we merge

		gotoIfError3(clean, ListSortingKey_popLocation(&sortedList, i, NULL, e_rr));
		sortedList.ptrNonConst[i - 1].mergedCount = aCount + bCount;
	}

	//Create our ranges

	Bool isNv = GraphicsDeviceRef_ptr(dev)->info.vendor == EGraphicsVendorId_NV;

	gotoIfError3(clean, ListD3D12_DESCRIPTOR_RANGE1_resize(
		&layoutExt->rangesResources, sortedList.length + isNv, alloc, e_rr
	));

	gotoIfError3(clean, ListD3D12_DESCRIPTOR_RANGE1_resize(&layoutExt->rangesSamplers, sortedList.length, alloc, e_rr));
	gotoIfError3(clean, ListU32_resize(&layoutExt->bindingOffsets, layout->info.bindings.length, alloc, e_rr));

	U32 offset1 = 0, offset2 = 0;
	U64 resourceRange = 0, samplerRange = 0;

	for (U64 i = 0; i < sortedList.length; ++i) {

		SortingKey key = sortedList.ptr[i];

		U32 count = key.mergedCount ? key.mergedCount : key.binding->count;

		D3D12_DESCRIPTOR_RANGE_TYPE type = dxGetDescriptorType(key.binding->registerType);

		D3D12_DESCRIPTOR_RANGE_FLAGS bindFlags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;

		if(type == D3D12_DESCRIPTOR_RANGE_TYPE_CBV)
			bindFlags = 0;

		if(
			!(info.flags & EDescriptorLayoutFlags_AllowBindlessEverywhere) &&
			!((info.flags & EDescriptorLayoutFlags_AllowBindlessOnArrays) && key.binding->count > 1)
		)
			bindFlags = 0;

		D3D12_DESCRIPTOR_RANGE1 range = (D3D12_DESCRIPTOR_RANGE1) {
			.RangeType = type,
			.NumDescriptors = count,
			.BaseShaderRegister = key.binding->binding.binding,
			.RegisterSpace = key.binding->binding.space,
			.Flags = bindFlags,
			.OffsetInDescriptorsFromTableStart = offset1
		};

		if (type == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER) {
			range.OffsetInDescriptorsFromTableStart = offset2;
			layoutExt->rangesSamplers.ptrNonConst[samplerRange++] = range;
			offset2 += count;
		}

		else {
			layoutExt->rangesResources.ptrNonConst[resourceRange++] = range;
			offset1 += count;
		}

		//Map registers to descriptor table offset

		for (U64 j = 0; j < info.bindings.length; ++j) {

			DescriptorBinding bindj = info.bindings.ptr[j];
			D3D12_DESCRIPTOR_RANGE_TYPE typej = dxGetDescriptorType(bindj.registerType);

			if(
				typej != type ||
				bindj.binding.space != key.binding->binding.space ||
				!(
					bindj.binding.binding >= key.binding->binding.binding &&
					bindj.binding.binding < key.binding->binding.binding + count
				)
			)
				continue;

			layoutExt->bindingOffsets.ptrNonConst[j] =
				range.OffsetInDescriptorsFromTableStart + bindj.binding.binding - key.binding->binding.binding;
		}
	}

	//Dummy UAV for NVAPI extensions

	if (isNv && !isPushDescriptors)
		gotoIfError3(clean, ListD3D12_ROOT_PARAMETER1_pushBack(&layoutExt->rootParams, (D3D12_ROOT_PARAMETER1) {
			.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV,
			.Descriptor = (D3D12_ROOT_DESCRIPTOR1) {
				.ShaderRegister = 99999,
				.RegisterSpace = 99999
			},
		}, alloc, e_rr));

	gotoIfError3(clean, ListD3D12_DESCRIPTOR_RANGE1_resize(&layoutExt->rangesResources, resourceRange, alloc, e_rr));
	gotoIfError3(clean, ListD3D12_DESCRIPTOR_RANGE1_resize(&layoutExt->rangesSamplers, samplerRange, alloc, e_rr));

	//Root params fo sampler and resource ranges

	if(layoutExt->rangesResources.length)
		layoutExt->rootParams.ptrNonConst[resourceParam] = (D3D12_ROOT_PARAMETER1) {
				.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
				.DescriptorTable = (D3D12_ROOT_DESCRIPTOR_TABLE1) {
					.NumDescriptorRanges = (U32) layoutExt->rangesResources.length,
					.pDescriptorRanges = layoutExt->rangesResources.ptr
				},
				.ShaderVisibility = DxDescriptorLayout_convertVisibility(visibility1)
			};

	if(layoutExt->rangesSamplers.length)
		layoutExt->rootParams.ptrNonConst[samplerParam] = (D3D12_ROOT_PARAMETER1) {
				.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
				.DescriptorTable = (D3D12_ROOT_DESCRIPTOR_TABLE1) {
					.NumDescriptorRanges = (U32) layoutExt->rangesSamplers.length,
					.pDescriptorRanges = layoutExt->rangesSamplers.ptr
				},
				.ShaderVisibility = DxDescriptorLayout_convertVisibility(visibility2)
			};

clean:
	ListSortingKey_free(&sortedList, alloc);
	return s_uccess;
}
