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

//graphics/d3d12/generic/dx_pipeline_layout.c

#include "graphics/generic/pipeline_layout.h"
#include "graphics/generic/descriptor_layout.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/d3d12/dx_device.h"
#include "platforms/logx.h"
#include "types/container/string_unicode.h"
#include "types/container/list_basic_types.h"

void DX_WRAP_FUNC(PipelineLayout_free)(PipelineLayout *layout, const Allocator *alloc) {

	(void) alloc;

	DxPipelineLayout *layoutExt = PipelineLayout_ext(layout, Dx);

	if(layoutExt->rootSig)
		layoutExt->rootSig->lpVtbl->Release(layoutExt->rootSig);
}

D3D12_SHADER_VISIBILITY DxDescriptorLayout_convertVisibility(U32 a);

Bool DX_WRAP_FUNC(GraphicsDeviceRef_createPipelineLayout)(
	GraphicsDeviceRef *dev,
	PipelineLayout *layout,
	const CharString *name,
	Error *e_rr
) {

	Bool s_uccess = true;
	const Allocator *alloc = GraphicsDeviceRef_getAlloc(dev);

	ListU16 nameUtf16 = (ListU16) { 0 };
	ListD3D12_ROOT_PARAMETER1 rootParams = (ListD3D12_ROOT_PARAMETER1) { 0 };
	DxPipelineLayout *layoutExt = PipelineLayout_ext(layout, Dx);
	GraphicsDevice *device = GraphicsDeviceRef_ptr(dev);
	DxGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Dx);

	ID3DBlob *rootSigBlob = NULL, *errBlob = NULL;

	D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSig = (D3D12_VERSIONED_ROOT_SIGNATURE_DESC) {
		.Version = D3D_ROOT_SIGNATURE_VERSION_1_1,
		.Desc_1_1 = (D3D12_ROOT_SIGNATURE_DESC1) { .Flags = flags }
	};

	D3D12_ROOT_PARAMETER1 pushConstants = (D3D12_ROOT_PARAMETER1) { 0 };

	DescriptorBinding constants = layout->info.pushConstants;

	if(constants.count)
		pushConstants = (D3D12_ROOT_PARAMETER1) {
			.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
			.Constants = (D3D12_ROOT_CONSTANTS) {
				.Num32BitValues = constants.constantBufferSize >> 2,
				.RegisterSpace = constants.binding.space,
				.ShaderRegister = constants.binding.binding
			},
			.ShaderVisibility = DxDescriptorLayout_convertVisibility(constants.visibility)
		};

	//Bindings only

	if (layout->info.bindings && !layout->info.pushConstants.count && !layout->info.pushDescriptors) {
		DescriptorLayout *desc = DescriptorLayoutRef_ptr(layout->info.bindings);
		DxDescriptorLayout *descExt = DescriptorLayout_ext(desc, Dx);
		rootSig.Desc_1_1.NumParameters = (U32) descExt->rootParams.length;
		rootSig.Desc_1_1.pParameters = descExt->rootParams.ptr;
	}

	//Push descriptors only

	else if (layout->info.pushDescriptors && !layout->info.pushConstants.count && !layout->info.bindings) {
		DescriptorLayout *desc = DescriptorLayoutRef_ptr(layout->info.pushDescriptors);
		DxDescriptorLayout *descExt = DescriptorLayout_ext(desc, Dx);
		rootSig.Desc_1_1.NumParameters = (U32) descExt->rootParams.length;
		rootSig.Desc_1_1.pParameters = descExt->rootParams.ptr;
	}

	//Push constants only

	else if (constants.count && !layout->info.pushDescriptors && !layout->info.bindings) {
		rootSig.Desc_1_1.NumParameters = 1;
		rootSig.Desc_1_1.pParameters = &pushConstants;
	}

	//Merge

	else {

		U64 count = 0;

		if (layout->info.bindings) {
			DxDescriptorLayout *descExt = DescriptorLayout_ext(DescriptorLayoutRef_ptr(layout->info.bindings), Dx);
			count += descExt->rootParams.length;
		}

		if (layout->info.pushDescriptors) {
			DxDescriptorLayout *descExt = DescriptorLayout_ext(DescriptorLayoutRef_ptr(layout->info.pushDescriptors), Dx);
			layoutExt->rootParamPushDescriptors = (U32) count;
			count += descExt->rootParams.length;
		}

		if(constants.count) {
			layoutExt->rootParamPushConstants = (U32) count;
			++count;
		}

		gotoIfError3(clean, ListD3D12_ROOT_PARAMETER1_resize(&rootParams, count, alloc, e_rr));
		count = 0;

		rootSig.Desc_1_1.NumParameters = (U32) rootParams.length;
		rootSig.Desc_1_1.pParameters = rootParams.length ? rootParams.ptr : NULL;

		if (layout->info.bindings) {

			DxDescriptorLayout *descExt = DescriptorLayout_ext(DescriptorLayoutRef_ptr(layout->info.bindings), Dx);

			for(U64 i = 0; i < descExt->rootParams.length; ++i)
				rootParams.ptrNonConst[count++] = descExt->rootParams.ptr[i];
		}

		if (layout->info.pushDescriptors) {

			DxDescriptorLayout *descExt = DescriptorLayout_ext(DescriptorLayoutRef_ptr(layout->info.pushDescriptors), Dx);

			for(U64 i = 0; i < descExt->rootParams.length; ++i)
				rootParams.ptrNonConst[count++] = descExt->rootParams.ptr[i];
		}

		if(constants.count)
			rootParams.ptrNonConst[count++] = pushConstants;
	}

	if(!dxCheck(deviceExt->deviceConfig->lpVtbl->SerializeVersionedRootSignature(
		deviceExt->deviceConfig, &rootSig, &rootSigBlob, &errBlob
	), e_rr)) {

		if(errBlob && (device->flags & EGraphicsDeviceFlags_IsDebug))
			Log_errorLnx(
				"D3D12: Create root signature failed: %s", (const C8*) errBlob->lpVtbl->GetBufferPointer(errBlob)
			);

		s_uccess = false;
		goto clean;
	}

	gotoIfError3(clean, dxCheck(deviceExt->device->lpVtbl->CreateRootSignature(
		deviceExt->device,
		0, rootSigBlob->lpVtbl->GetBufferPointer(rootSigBlob), rootSigBlob->lpVtbl->GetBufferSize(rootSigBlob),
		&IID_ID3D12RootSignature, (void**) &layoutExt->rootSig
	), e_rr));

	if (name && CharString_length(*name) && (device->flags & EGraphicsDeviceFlags_IsDebug)) {
		gotoIfError3(clean, CharString_toUTF16(*name, alloc, &nameUtf16, e_rr));
		gotoIfError3(clean, dxCheck(
			layoutExt->rootSig->lpVtbl->SetName(layoutExt->rootSig, (const wchar_t*) nameUtf16.ptr),
			e_rr
		));
	}

clean:
	ListD3D12_ROOT_PARAMETER1_free(&rootParams, alloc);
	ListU16_free(&nameUtf16, alloc);
	if(rootSigBlob) rootSigBlob->lpVtbl->Release(rootSigBlob);
	if(errBlob) errBlob->lpVtbl->Release(errBlob);
	return s_uccess;
}
