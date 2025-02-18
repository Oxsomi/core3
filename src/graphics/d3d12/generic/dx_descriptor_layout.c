/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "platforms/ext/listx_impl.h"
#include "graphics/generic/descriptor_layout.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/d3d12/dx_device.h"
#include "types/container/string.h"
#include "platforms/ext/stringx.h"
#include "formats/oiSH/entries.h"

TListImpl(D3D12_DESCRIPTOR_RANGE)
TListImpl(D3D12_DESCRIPTOR_RANGE1)

Bool DX_WRAP_FUNC(DescriptorLayout_free)(DescriptorLayout *layout) {
	DxDescriptorLayout *layoutExt = DescriptorLayout_ext(layout, Dx);
	ListU32_freex(&layoutExt->bindingOffsets);
	ListD3D12_DESCRIPTOR_RANGE1_freex(&layoutExt->rangesResources);
	ListD3D12_DESCRIPTOR_RANGE1_freex(&layoutExt->rangesSamplers);
	ListD3D12_DESCRIPTOR_RANGE_freex(&layoutExt->legacyResources);
	ListD3D12_DESCRIPTOR_RANGE_freex(&layoutExt->legacySamplers);
	return true;
}

D3D12_DESCRIPTOR_RANGE_TYPE dxGetDescriptorType(ESHRegisterType regType) {
	
	switch (regType & ESHRegisterType_TypeMask) {
		
		case ESHRegisterType_Sampler:
		case ESHRegisterType_SamplerComparisonState:
			return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;

		case ESHRegisterType_ConstantBuffer:
			return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;

		case ESHRegisterType_AccelerationStructure:
			return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

		case ESHRegisterType_SubpassInput:		//Doesn't exist in D3D12
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
			return !!(regType & ESHRegisterType_IsWrite) ? D3D12_DESCRIPTOR_RANGE_TYPE_UAV : D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	}
}

typedef struct SortingKey {
	const DescriptorBinding *binding;
	U32 mergedCount;						//Useful to allow merging later
	U32 padding;
} SortingKey;

ECompareResult SortingKey_compare(const SortingKey *aKey, const SortingKey *bKey) {

	const DescriptorBinding *a = aKey->binding;
	const DescriptorBinding *b = bKey->binding;

	if(a->space != b->space)
		return a->space < b->space ? ECompareResult_Lt : ECompareResult_Gt;

	D3D12_DESCRIPTOR_RANGE_TYPE registerTypeA = dxGetDescriptorType(a->registerType);
	D3D12_DESCRIPTOR_RANGE_TYPE registerTypeB = dxGetDescriptorType(b->registerType);

	if(registerTypeA != registerTypeB)
		return registerTypeA < registerTypeB ? ECompareResult_Lt : ECompareResult_Gt;

	if(a->id != b->id)
		return a->id < b->id ? ECompareResult_Lt : ECompareResult_Gt;

	return ECompareResult_Eq;
}

TList(SortingKey);
TListImpl(SortingKey);

Error DX_WRAP_FUNC(GraphicsDeviceRef_createDescriptorLayout)(
	GraphicsDeviceRef *dev,
	DescriptorLayout *layout,
	CharString name
) {

	(void) name;

	Error err = Error_none();
	ListSortingKey sortedList = (ListSortingKey) { 0 };

	DxDescriptorLayout *layoutExt = DescriptorLayout_ext(layout, Dx);

	const DescriptorLayoutInfo info = layout->info;

	gotoIfError(clean, ListSortingKey_reservex(&sortedList, info.bindings.length))

	//Sort by set and merge shaders that allow it and check we only have 4 sets bound

	for(U64 i = 0; i < info.bindings.length; ++i)
		gotoIfError(clean, ListSortingKey_pushBack(&sortedList, (SortingKey) { &info.bindings.ptr[i] }, (Allocator) { 0 }))

	if(!ListSortingKey_sortCustom(sortedList, (CompareFunction) SortingKey_compare))
		gotoIfError(clean, Error_invalidState(
			0, "GraphicsDeviceRef_createDescriptorLayout can't sort list"
		))

	//Collapse nearby bindings if possible

	for (U64 i = sortedList.length - 1; i != U64_MAX && !!i; --i) {

		//Combining is only allowed if same space, id and type.
		//And if bindless on arrays is on; we don't want to combine non bindless + bindless in one.
		//This would not allow us to turn off descriptor flags we don't want turned on.

		DescriptorBinding a = *sortedList.ptr[i - 1].binding;
		DescriptorBinding b = *sortedList.ptr[i].binding;

		if(
			dxGetDescriptorType(a.registerType) != dxGetDescriptorType(b.registerType) ||
			a.space != b.space ||
			a.id + a.count != b.id ||
			(!!(info.flags & EDescriptorLayoutFlags_AllowBindlessOnArrays) && (a.count > 1) != (b.count > 1))
		)
			continue;

		//We've found a match, let's shorten the array and remember how many we merge

		gotoIfError(clean, ListSortingKey_popLocation(&sortedList, i, NULL))
		sortedList.ptrNonConst[i - 1].mergedCount = a.count + b.count;
	}

	//Create our ranges

	gotoIfError(clean, ListD3D12_DESCRIPTOR_RANGE1_resizex(&layoutExt->rangesResources, sortedList.length + 1))
	gotoIfError(clean, ListD3D12_DESCRIPTOR_RANGE1_resizex(&layoutExt->rangesSamplers, sortedList.length))
	gotoIfError(clean, ListU32_resizex(&layoutExt->bindingOffsets, layout->info.bindings.length))

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
			!(!!(info.flags & EDescriptorLayoutFlags_AllowBindlessOnArrays) && key.binding->count > 1)
		)
			bindFlags = 0;

		D3D12_DESCRIPTOR_RANGE1 range = (D3D12_DESCRIPTOR_RANGE1) {
			.RangeType = type,
			.NumDescriptors = count,
			.BaseShaderRegister = key.binding->id,
			.RegisterSpace = key.binding->space,
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
				bindj.space != key.binding->space ||
				!(bindj.id >= key.binding->id && bindj.id < key.binding->id + count)
			)
				continue;
			
			layoutExt->bindingOffsets.ptrNonConst[j] = range.OffsetInDescriptorsFromTableStart + bindj.id - key.binding->id;
		}
	}

	//Dummy UAV for NVAPI extensions

	if (GraphicsDeviceRef_ptr(dev)->info.vendor == EGraphicsVendorId_NV)
		layoutExt->rangesResources.ptrNonConst[resourceRange++] = (D3D12_DESCRIPTOR_RANGE1) {
			.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
			.NumDescriptors = 1,
			.BaseShaderRegister = 99999,
			.RegisterSpace = 99999,
			.OffsetInDescriptorsFromTableStart = offset1
		};
	
	gotoIfError(clean, ListD3D12_DESCRIPTOR_RANGE1_resizex(&layoutExt->rangesResources, resourceRange))
	gotoIfError(clean, ListD3D12_DESCRIPTOR_RANGE1_resizex(&layoutExt->rangesSamplers, samplerRange))
	
	//Legacy root sig support
	//Note: Even if unsupported, D3D12_DESCRIPTOR_RANGE1[] will still hang around.
	//		This is to allow ID3D12RootSignature to be able to detect DENY flags.

	GraphicsDevice *device = GraphicsDeviceRef_ptr(dev);

	if (!(device->info.capabilities.featuresExt & EDxGraphicsFeatures_RootSig1_1)) {

		gotoIfError(clean, ListD3D12_DESCRIPTOR_RANGE_resizex(&layoutExt->legacyResources, layoutExt->rangesResources.length))
		gotoIfError(clean, ListD3D12_DESCRIPTOR_RANGE_resizex(&layoutExt->legacySamplers, layoutExt->rangesSamplers.length))
		
		for(U64 i = 0; i < layoutExt->rangesResources.length; ++i) {

			D3D12_DESCRIPTOR_RANGE1 range = layoutExt->rangesResources.ptr[i];

			layoutExt->legacyResources.ptrNonConst[i] = (D3D12_DESCRIPTOR_RANGE) {
				.RangeType = range.RangeType,
				.NumDescriptors = range.NumDescriptors,
				.BaseShaderRegister = range.BaseShaderRegister,
				.RegisterSpace = range.RegisterSpace,
				.OffsetInDescriptorsFromTableStart = range.OffsetInDescriptorsFromTableStart
			};
		}
		
		for(U64 i = 0; i < layoutExt->rangesSamplers.length; ++i) {

			D3D12_DESCRIPTOR_RANGE1 range = layoutExt->rangesSamplers.ptr[i];

			layoutExt->legacySamplers.ptrNonConst[i] = (D3D12_DESCRIPTOR_RANGE) {
				.RangeType = range.RangeType,
				.NumDescriptors = range.NumDescriptors,
				.BaseShaderRegister = range.BaseShaderRegister,
				.RegisterSpace = range.RegisterSpace,
				.OffsetInDescriptorsFromTableStart = range.OffsetInDescriptorsFromTableStart
			};
		}
	}

clean:
	ListSortingKey_freex(&sortedList);
	return err;
}
