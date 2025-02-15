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
#include "graphics/generic/pipeline_layout.h"
#include "graphics/generic/descriptor_layout.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/d3d12/dx_device.h"
#include "types/container/string.h"
#include "platforms/ext/stringx.h"
#include "platforms/log.h"
#include "formats/oiSH/entries.h"

Bool DX_WRAP_FUNC(PipelineLayout_free)(PipelineLayout *layout) {

	DxPipelineLayout *layoutExt = PipelineLayout_ext(layout, Dx);

	if(layoutExt->rootSig)
		layoutExt->rootSig->lpVtbl->Release(layoutExt->rootSig);

	return true;
}

D3D12_SHADER_VISIBILITY DxPipelineLayout_convertVisibility(U32 a) {
	switch (a) {
		case 1 << ESHPipelineStage_Vertex:		return D3D12_SHADER_VISIBILITY_VERTEX;
		case 1 << ESHPipelineStage_Pixel:		return D3D12_SHADER_VISIBILITY_PIXEL;
		case 1 << ESHPipelineStage_Hull:		return D3D12_SHADER_VISIBILITY_HULL;
		case 1 << ESHPipelineStage_Domain:		return D3D12_SHADER_VISIBILITY_DOMAIN;
		case 1 << ESHPipelineStage_GeometryExt:	return D3D12_SHADER_VISIBILITY_GEOMETRY;
		case 1 << ESHPipelineStage_MeshExt:		return D3D12_SHADER_VISIBILITY_MESH;
		case 1 << ESHPipelineStage_TaskExt:		return D3D12_SHADER_VISIBILITY_AMPLIFICATION;
		default:								return D3D12_SHADER_VISIBILITY_ALL;
	}
}

Error DX_WRAP_FUNC(GraphicsDeviceRef_createPipelineLayout)(
	GraphicsDeviceRef *dev,
	PipelineLayout *layout,
	CharString name
) {

	ListU16 nameUtf16 = (ListU16) { 0 };
	Error err = Error_none();
	DxPipelineLayout *layoutExt = PipelineLayout_ext(layout, Dx);
	GraphicsDevice *device = GraphicsDeviceRef_ptr(dev);
	DxGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Dx);

	DescriptorLayout *desc = DescriptorLayoutRef_ptr(layout->info.bindings);
	DxDescriptorLayout *descExt = DescriptorLayout_ext(desc, Dx);

	ID3DBlob *rootSigBlob = NULL, *errBlob = NULL;

	//Grab visibility to better set shader visibility

	U32 visibility1 = 0;
	U32 visibility2 = 0;

	for (U64 i = 0; i < desc->info.bindings.length; ++i) {

		DescriptorBinding bind = desc->info.bindings.ptr[i];
		bind.registerType &= ESHRegisterType_TypeMask;

		if (bind.registerType == ESHRegisterType_Sampler || bind.registerType == ESHRegisterType_SamplerComparisonState)
			visibility2 |= bind.visibility;

		else visibility1 |= bind.visibility;
	}

	D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	U32 rootParamCounter = 0;
	D3D12_ROOT_PARAMETER rootParam[2];
	D3D12_ROOT_PARAMETER1 rootParam1[2];
	D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSig;

	if (!(device->info.capabilities.featuresExt & EDxGraphicsFeatures_RootSig1_1)) {

		if(descExt->legacyResources.length)
			rootParam[rootParamCounter++] = (D3D12_ROOT_PARAMETER) {
					.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
					.DescriptorTable = (D3D12_ROOT_DESCRIPTOR_TABLE) {
						.NumDescriptorRanges = (U32) descExt->legacyResources.length,
						.pDescriptorRanges = descExt->legacyResources.ptr
					},
					.ShaderVisibility = DxPipelineLayout_convertVisibility(visibility1)
				};

		if(descExt->legacySamplers.length)
			rootParam[rootParamCounter++] = (D3D12_ROOT_PARAMETER) {
					.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
					.DescriptorTable = (D3D12_ROOT_DESCRIPTOR_TABLE) {
						.NumDescriptorRanges = (U32) descExt->legacySamplers.length,
						.pDescriptorRanges = descExt->legacySamplers.ptr
					},
			.ShaderVisibility = DxPipelineLayout_convertVisibility(visibility2)
				};

		rootSig = (D3D12_VERSIONED_ROOT_SIGNATURE_DESC) {
			.Version = D3D_ROOT_SIGNATURE_VERSION_1,
			.Desc_1_0 = (D3D12_ROOT_SIGNATURE_DESC) {
				.Flags = flags,
				.NumParameters = rootParamCounter,
				.pParameters = rootParam
			}
		};
	}

	else {

		if(descExt->legacyResources.length)
			rootParam1[rootParamCounter++] = (D3D12_ROOT_PARAMETER1) {
					.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
					.DescriptorTable = (D3D12_ROOT_DESCRIPTOR_TABLE1) {
						.NumDescriptorRanges = (U32) descExt->rangesResources.length,
						.pDescriptorRanges = descExt->rangesResources.ptr
					},
					.ShaderVisibility = DxPipelineLayout_convertVisibility(visibility1)
				};

		if(descExt->legacySamplers.length)
			rootParam1[rootParamCounter++] = (D3D12_ROOT_PARAMETER1) {
					.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
					.DescriptorTable = (D3D12_ROOT_DESCRIPTOR_TABLE1) {
						.NumDescriptorRanges = (U32) descExt->rangesSamplers.length,
						.pDescriptorRanges = descExt->rangesSamplers.ptr
					},
					.ShaderVisibility = DxPipelineLayout_convertVisibility(visibility2)
				};

		rootSig = (D3D12_VERSIONED_ROOT_SIGNATURE_DESC) {
			.Version = D3D_ROOT_SIGNATURE_VERSION_1_1,
			.Desc_1_1 = (D3D12_ROOT_SIGNATURE_DESC1) {
				.Flags = flags,
				.NumParameters = rootParamCounter,
				.pParameters = rootParam1
			}
		};
	}

	err = dxCheck(deviceExt->deviceConfig->lpVtbl->SerializeVersionedRootSignature(
		deviceExt->deviceConfig, &rootSig, &rootSigBlob, &errBlob
	));

	if(err.genericError) {

		if(errBlob && !!(device->flags & EGraphicsDeviceFlags_IsDebug))
			Log_errorLnx(
				"D3D12: Create root signature failed: %s", (const C8*) errBlob->lpVtbl->GetBufferPointer(errBlob)
			);

		goto clean;
	}

	gotoIfError(clean, dxCheck(deviceExt->device->lpVtbl->CreateRootSignature(
		deviceExt->device,
		0, rootSigBlob->lpVtbl->GetBufferPointer(rootSigBlob), rootSigBlob->lpVtbl->GetBufferSize(rootSigBlob),
		&IID_ID3D12RootSignature, (void**) &layoutExt->rootSig
	)))

	if (CharString_length(name) && !!(device->flags & EGraphicsDeviceFlags_IsDebug)) {
		gotoIfError(clean, CharString_toUTF16x(name, &nameUtf16))
		gotoIfError(clean, dxCheck(layoutExt->rootSig->lpVtbl->SetName(layoutExt->rootSig, (const wchar_t*) nameUtf16.ptr)))
	}

clean:
	ListU16_freex(&nameUtf16);
	if(rootSigBlob) rootSigBlob->lpVtbl->Release(rootSigBlob);
	if(errBlob) errBlob->lpVtbl->Release(errBlob);
	return err;
}
