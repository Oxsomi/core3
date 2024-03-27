/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "graphics/directx12/dx_device.h"
#include "graphics/generic/instance.h"
#include "graphics/generic/device_info.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/stringx.h"
#include "platforms/log.h"
#include "types/error.h"
#include "types/buffer.h"

#include <dxgi1_6.h>
#include <nvapi.h>

#include <d3d11.h>			//AMD AGS needs it...
#include <amd_ags.h>

TListNamed(IDXGIAdapter4*, ListIDXGIAdapter4)
TListNamedImpl(ListIDXGIAdapter4)

//Graphics instance doesn't really exist for DirectX12.
//It's only used for defining SDK version and that we're using DirectX12.
//The closest thing we can map is a DXGI factory that's used to query adapters

typedef enum EDxGraphicsInstanceFlags {
	EDxGraphicsInstanceFlags_HasNVApi		= 1 << 0,
	EDxGraphicsInstanceFlags_HasAMDAgs		= 1 << 1
} EDxGraphicsInstanceFlags;

typedef struct DxGraphicsInstance {

	IDXGIFactory6 *factory;
	AGSContext *agsContext;

	CharString nvDriverVersion;
	CharString amdDriverVersion;

	AGSGPUInfo gpuInfo;

	U32 flags, padding;

} DxGraphicsInstance;

Bool GraphicsInstance_free(GraphicsInstance *data, Allocator alloc) {

	(void)alloc;

	DxGraphicsInstance *instanceExt = GraphicsInstance_ext(data, Dx);

	if(!instanceExt)
		return true;

	if(instanceExt->flags & EDxGraphicsInstanceFlags_HasNVApi)
		NvAPI_Unload();

	if(instanceExt->flags & EDxGraphicsInstanceFlags_HasAMDAgs)
		agsDeInitialize(instanceExt->agsContext);

	CharString_freex(&instanceExt->nvDriverVersion);
	CharString_freex(&instanceExt->amdDriverVersion);

	if(instanceExt->factory)
		instanceExt->factory->lpVtbl->Release(instanceExt->factory);

	return true;
}

const U64 GraphicsInstanceExt_size = sizeof(DxGraphicsInstance);

Error GraphicsInstance_createExt(GraphicsApplicationInfo info, GraphicsInstanceRef **instanceRef) {

	(void)info;
	GraphicsInstance *instance = GraphicsInstanceRef_ptr(*instanceRef);
	DxGraphicsInstance *instanceExt = GraphicsInstance_ext(instance, Dx);

	Error err = dxCheck(CreateDXGIFactory(&IID_IDXGIFactory6, (void**) &instanceExt->factory));

	if(err.genericError)
		return err;

	//Check for NVApi

	const NvAPI_Status status = NvAPI_Initialize();

	if(status == NVAPI_OK) {

		instanceExt->flags |= EDxGraphicsInstanceFlags_HasNVApi;

		U32 driverVersion = 0;
		NvAPI_ShortString shortString = { 0 };

		if(NvAPI_SYS_GetDriverAndBranchVersion((NvU32*)&driverVersion, shortString) != NVAPI_OK) {
			NvAPI_Unload();
			instanceExt->flags &=~ EDxGraphicsInstanceFlags_HasNVApi;
		}

		else gotoIfError(clean, CharString_formatx(
			&instanceExt->nvDriverVersion, "%"PRIu32".%"PRIu32, driverVersion / 100, driverVersion % 100
		))
	}

	//Check for AMDAgs

	const AGSConfiguration config = (AGSConfiguration) { 0 };

	if(agsInitialize(AGS_CURRENT_VERSION, &config, &instanceExt->agsContext, &instanceExt->gpuInfo) == AGS_SUCCESS) {

		instanceExt->flags |= EDxGraphicsInstanceFlags_HasAMDAgs;

		gotoIfError(clean, CharString_createCopyx(
			CharString_createRefCStrConst(instanceExt->gpuInfo.radeonSoftwareVersion), &instanceExt->amdDriverVersion
		))

		if(CharString_length(instanceExt->amdDriverVersion) >= 256) {
			Log_warnLnx("GraphicsInstance_createExt() AMD AGS initialize failed, version string is too long");
			agsDeInitialize(instanceExt->agsContext);
			instanceExt->flags &=~ EDxGraphicsInstanceFlags_HasNVApi;
		}
	}

	instance->api = EGraphicsApi_DirectX12;
	instance->apiVersion = D3D12_SDK_VERSION;

clean:
	return err;
}

Error GraphicsInstance_getDeviceInfos(const GraphicsInstance *inst, ListGraphicsDeviceInfo *result) {

	if(!inst || !result)
		return Error_nullPointer(!inst ? 0 : 2, "GraphicsInstance_getDeviceInfos()::inst and result are required");

	if(result->ptr)
		return Error_invalidParameter(
			1, 0, "GraphicsInstance_getDeviceInfos()::result isn't empty, may indicate memleak"
		);

	const DxGraphicsInstance *instanceExt = GraphicsInstance_ext(inst, Dx);
	ListIDXGIAdapter4 adapters = (ListIDXGIAdapter4) { 0 };
	ListGraphicsDeviceInfo tempInfos = (ListGraphicsDeviceInfo) { 0 };
	ID3D12Device5 *device = NULL;		//Temporary device, we need to know
	CharString tmp = CharString_createNull();

	//Get all possible adapters

	Error err = Error_none();

	{
		HRESULT hr;
		U32 adapterId = 0;

		do {

			gotoIfError(clean, ListIDXGIAdapter4_resizex(&adapters, adapters.length + 1))
			hr = instanceExt->factory->lpVtbl->EnumAdapters1(
				instanceExt->factory, adapterId, (void*)&adapters.ptrNonConst[adapterId]
			);

			if(FAILED(hr))
				gotoIfError(clean, ListIDXGIAdapter4_resizex(&adapters, adapters.length - 1))

			++adapterId;

		} while(SUCCEEDED(hr));

		if(hr != DXGI_ERROR_NOT_FOUND)
			gotoIfError(clean, dxCheck(hr))
	}

	gotoIfError(clean, ListGraphicsDeviceInfo_reservex(&tempInfos, adapters.length))

	//Get compatible adapters

	for(U64 i = 0; i < adapters.length; ++i) {

		DXGI_ADAPTER_DESC3 desc = (DXGI_ADAPTER_DESC3) { 0 };
		gotoIfError(clean, dxCheck(adapters.ptr[i]->lpVtbl->GetDesc3(adapters.ptr[i], &desc)))

		//Fences are required for D3D12

		if(!(desc.Flags & DXGI_ADAPTER_FLAG3_SUPPORT_MONITORED_FENCES)) {
			Log_debugLnx("DXGI: Unsupported device %"PRIu32", doesn't support D3D12 fences", i);
			continue;
		}

		U64 sharedMem = desc.SharedSystemMemory;
		U64 dedicatedMem = desc.DedicatedVideoMemory;

		if(!dedicatedMem)
			dedicatedMem = sharedMem;

		if(sharedMem < 512 * MIBI || dedicatedMem < 512 * MIBI) {
			Log_debugLnx("DXGI: Unsupported device %"PRIu32", not enough system or video memory", i);
			continue;
		}

		EGraphicsVendorId vendorId = EGraphicsVendorId_Unknown;

		switch(desc.VendorId) {
			case EGraphicsVendorPCIE_NV:	vendorId = EGraphicsVendorId_NV;	break;
			case EGraphicsVendorPCIE_AMD:	vendorId = EGraphicsVendorId_AMD;	break;
			case EGraphicsVendorPCIE_ARM:	vendorId = EGraphicsVendorId_ARM;	break;
			case EGraphicsVendorPCIE_QCOM:	vendorId = EGraphicsVendorId_QCOM;	break;
			case EGraphicsVendorPCIE_INTC:	vendorId = EGraphicsVendorId_INTC;	break;
			case EGraphicsVendorPCIE_IMGT:	vendorId = EGraphicsVendorId_IMGT;	break;
			case EGraphicsVendorPCIE_MSFT:	vendorId = EGraphicsVendorId_MSFT;	break;
		}

		//Grab properties

		EGraphicsDeviceType type = EGraphicsDeviceType_CPU;

		const U64 luid = *(const U64*) &desc.AdapterLuid;

		//Create temporary device.
		//Unfortunately, there's no other way to query features it seems.

		HRESULT lastError = 0;

		if(FAILED(lastError = D3D12CreateDevice(
			(IUnknown*)adapters.ptr[i], D3D_FEATURE_LEVEL_12_1, &IID_ID3D12Device5, (void**) &device)
		)) {
			Log_debugLnx("D3D12: Unsupported device %"PRIu32", doesn't support feature level 12.1", i);
			goto next;
		}

		//Get capabilities

		GraphicsDeviceCapabilities caps = (GraphicsDeviceCapabilities) { 0 };
		caps.sharedMemory = sharedMem;
		caps.dedicatedMemory = dedicatedMem;

		caps.features |= 
			EGraphicsFeatures_LUID | EGraphicsFeatures_MultiDrawIndirectCount | EGraphicsFeatures_DebugMarkers |
			EGraphicsFeatures_GeometryShader | EGraphicsFeatures_SubgroupArithmetic | EGraphicsFeatures_SubgroupShuffle |
			EGraphicsFeatures_Wireframe | EGraphicsFeatures_LogicOp | EGraphicsFeatures_DualSrcBlend | EGraphicsFeatures_Multiview;

		caps.dataTypes |=
			EGraphicsDataTypes_I64 | EGraphicsDataTypes_BCn | EGraphicsDataTypes_MSAA2x | EGraphicsDataTypes_MSAA8x |
			EGraphicsDataTypes_AtomicI64 | EGraphicsDataTypes_BCn;

		if(vendorId != EGraphicsVendorId_AMD)
			caps.dataTypes |= EGraphicsDataTypes_D24S8;
		
		if (
			vendorId == EGraphicsVendorId_NV || vendorId == EGraphicsVendorId_AMD || 
			vendorId == EGraphicsVendorId_INTC || vendorId == EGraphicsVendorId_MSFT
		)
			caps.features |= EGraphicsFeatures_DirectRendering;

		D3D12_FEATURE_DATA_D3D12_OPTIONS opt0 = (D3D12_FEATURE_DATA_D3D12_OPTIONS) { 0 };
		D3D12_FEATURE_DATA_D3D12_OPTIONS1 opt1 = (D3D12_FEATURE_DATA_D3D12_OPTIONS1) { 0 };
		D3D12_FEATURE_DATA_D3D12_OPTIONS3 opt3 = (D3D12_FEATURE_DATA_D3D12_OPTIONS3) { 0 };
		D3D12_FEATURE_DATA_D3D12_OPTIONS4 opt4 = (D3D12_FEATURE_DATA_D3D12_OPTIONS4) { 0 };
		D3D12_FEATURE_DATA_D3D12_OPTIONS5 opt5 = (D3D12_FEATURE_DATA_D3D12_OPTIONS5) { 0 };
		D3D12_FEATURE_DATA_D3D12_OPTIONS6 opt6 = (D3D12_FEATURE_DATA_D3D12_OPTIONS6) { 0 };
		D3D12_FEATURE_DATA_D3D12_OPTIONS7 opt7 = (D3D12_FEATURE_DATA_D3D12_OPTIONS7) { 0 };
		D3D12_FEATURE_DATA_D3D12_OPTIONS8 opt8 = (D3D12_FEATURE_DATA_D3D12_OPTIONS8) { 0 };
		D3D12_FEATURE_DATA_D3D12_OPTIONS9 opt9 = (D3D12_FEATURE_DATA_D3D12_OPTIONS9) { 0 };
		D3D12_FEATURE_DATA_D3D12_OPTIONS12 opt12 = (D3D12_FEATURE_DATA_D3D12_OPTIONS12) { 0 };
		D3D12_FEATURE_DATA_D3D12_OPTIONS16 opt16 = (D3D12_FEATURE_DATA_D3D12_OPTIONS16) { 0 };

		D3D12_FEATURE_DATA_SHADER_MODEL shaderOpt = (D3D12_FEATURE_DATA_SHADER_MODEL) { 0 };
		D3D12_FEATURE_DATA_ARCHITECTURE1 arch = (D3D12_FEATURE_DATA_ARCHITECTURE1) { 0 };
		D3D12_FEATURE_DATA_HARDWARE_COPY hwCopy = (D3D12_FEATURE_DATA_HARDWARE_COPY) { 0 };

		if(
			FAILED(device->lpVtbl->CheckFeatureSupport(device, D3D12_FEATURE_D3D12_OPTIONS, &opt0, sizeof(opt0))) ||
			opt0.TiledResourcesTier < D3D12_TILED_RESOURCES_TIER_3 ||
			!opt0.TypedUAVLoadAdditionalFormats ||
			!opt0.OutputMergerLogicOp ||
			!opt0.ROVsSupported ||
			opt0.ConservativeRasterizationTier < D3D12_CONSERVATIVE_RASTERIZATION_TIER_2
		) {
			Log_debugLnx("D3D12: Unsupported device %"PRIu32", doesn't support required D3D12_OPTIONS", i);
			goto next;
		}

		if(opt0.DoublePrecisionFloatShaderOps)
			caps.dataTypes |= EGraphicsDataTypes_F64;
		
		if(
			FAILED(device->lpVtbl->CheckFeatureSupport(device, D3D12_FEATURE_D3D12_OPTIONS1, &opt1, sizeof(opt1))) ||
			!opt1.WaveOps || !opt1.Int64ShaderOps
		) {
			Log_debugLnx("D3D12: Unsupported device %"PRIu32", doesn't support required D3D12_OPTIONS1", i);
			goto next;
		}

		if(opt1.WaveLaneCountMin < 4 || opt1.WaveLaneCountMax > 128) {
			Log_debugLnx("D3D12: Unsupported device %"PRIu32", subgroup size is not in range 16-128!", i);
			goto next;
		}

		if(
			SUCCEEDED(device->lpVtbl->CheckFeatureSupport(device, D3D12_FEATURE_D3D12_OPTIONS3, &opt3, sizeof(opt3))) &&
			opt3.WriteBufferImmediateSupportFlags
		)
			caps.featuresExt |= EDxGraphicsFeatures_WriteBufferImmediate;

		if(
			SUCCEEDED(device->lpVtbl->CheckFeatureSupport(device, D3D12_FEATURE_D3D12_OPTIONS4, &opt4, sizeof(opt4))) &&
			opt4.Native16BitShaderOpsSupported
		)
			caps.dataTypes |= EGraphicsDataTypes_F16 | EGraphicsDataTypes_I16;

		if(
			SUCCEEDED(device->lpVtbl->CheckFeatureSupport(device, D3D12_FEATURE_D3D12_OPTIONS5, &opt5, sizeof(opt5))) &&
			opt5.RaytracingTier >= D3D12_RAYTRACING_TIER_1_0
		) {
			caps.features |= EGraphicsFeatures_RayPipeline | EGraphicsFeatures_Raytracing;

			if(opt5.RaytracingTier >= D3D12_RAYTRACING_TIER_1_1)
				caps.features |= EGraphicsFeatures_RayQuery;
		}

		if(
			SUCCEEDED(device->lpVtbl->CheckFeatureSupport(device, D3D12_FEATURE_D3D12_OPTIONS6, &opt6, sizeof(opt6))) &&
			opt6.VariableShadingRateTier >= D3D12_VARIABLE_SHADING_RATE_TIER_1
		)
			caps.features |= EGraphicsFeatures_VariableRateShading;

		if(
			SUCCEEDED(device->lpVtbl->CheckFeatureSupport(device, D3D12_FEATURE_D3D12_OPTIONS7, &opt7, sizeof(opt7))) &&
			opt7.MeshShaderTier >= D3D12_MESH_SHADER_TIER_1
		)
			caps.features |= EGraphicsFeatures_MeshShader;

		if(
			FAILED(device->lpVtbl->CheckFeatureSupport(device, D3D12_FEATURE_D3D12_OPTIONS8, &opt8, sizeof(opt8))) ||
			!opt8.UnalignedBlockTexturesSupported
		) {
			Log_debugLnx("D3D12: Unsupported device %"PRIu32", doesn't support required D3D12_OPTIONS8", i);
			goto next;
		}

		if(
			FAILED(device->lpVtbl->CheckFeatureSupport(device, D3D12_FEATURE_D3D12_OPTIONS9, &opt9, sizeof(opt9))) ||
			!opt9.AtomicInt64OnTypedResourceSupported || 
			!opt9.AtomicInt64OnGroupSharedSupported
		) {
			Log_debugLnx("D3D12: Unsupported device %"PRIu32", doesn't support required D3D12_OPTIONS9", i);
			goto next;
		}
		
		if(
			FAILED(device->lpVtbl->CheckFeatureSupport(device, D3D12_FEATURE_D3D12_OPTIONS12, &opt12, sizeof(opt12))) ||
			!opt12.EnhancedBarriersSupported
		) {
			Log_debugLnx("D3D12: Unsupported device %"PRIu32", doesn't support required D3D12_OPTIONS12", i);
			goto next;
		}
		
		if(
			SUCCEEDED(device->lpVtbl->CheckFeatureSupport(device, D3D12_FEATURE_D3D12_OPTIONS16, &opt16, sizeof(opt16))) &&
			opt16.GPUUploadHeapSupported
		)
			caps.featuresExt |= EDxGraphicsFeatures_ReBAR;
		
		if(
			SUCCEEDED(device->lpVtbl->CheckFeatureSupport(device, D3D12_FEATURE_HARDWARE_COPY, &hwCopy, sizeof(hwCopy))) &&
			hwCopy.Supported
		)
			caps.featuresExt |= EDxGraphicsFeatures_HardwareCopyQueue;

		shaderOpt.HighestShaderModel = D3D_SHADER_MODEL_6_5;		//Nice way of querying DirectX...
		if(FAILED(device->lpVtbl->CheckFeatureSupport(device, D3D12_FEATURE_SHADER_MODEL, &shaderOpt, sizeof(shaderOpt)))) {
			Log_debugLnx("D3D12: Unsupported device %"PRIu32", doesn't support required shader model (6.5)", i);
			goto next;
		}
		
		if(FAILED(device->lpVtbl->CheckFeatureSupport(device, D3D12_FEATURE_ARCHITECTURE1, &arch, sizeof(arch)))) {
			Log_debugLnx("D3D12: Unsupported device %"PRIu32", doesn't support required D3D12_FEATURE_ARCHITECTURE1", i);
			goto next;
		}

		if(!(desc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) 
			type = !arch.UMA ? EGraphicsDeviceType_Dedicated : EGraphicsDeviceType_Integrated;

		ID3D12Device *device0 = NULL;
		if(FAILED(device->lpVtbl->QueryInterface(device, &IID_ID3D12Device, (void**)&device0))) {
			Log_debugLnx("D3D12: Unsupported device %"PRIu32", can't query version 0 device", i);
			goto next;
		}

		//Validate formats

		const U8 requiredUavTypedLoad[] = {

			ETextureFormatId_R8s,
			ETextureFormatId_RG8i, ETextureFormatId_RG8u, ETextureFormatId_RG8s, ETextureFormatId_RG8,

			ETextureFormatId_R16, ETextureFormatId_R16s,

			ETextureFormatId_RG16f, ETextureFormatId_RG16u,
			ETextureFormatId_RG16i, ETextureFormatId_RG16, ETextureFormatId_RG16s,

			ETextureFormatId_RGBA16, ETextureFormatId_RGBA16s,

			ETextureFormatId_RG32f, ETextureFormatId_RG32i, ETextureFormatId_RG32u,

			ETextureFormatId_BGR10A2
		};

		for(U64 j = 0; j < sizeof(requiredUavTypedLoad); ++j) {

			D3D12_FEATURE_DATA_FORMAT_SUPPORT format = (D3D12_FEATURE_DATA_FORMAT_SUPPORT) {
				.Format = ETextureFormatId_toDXFormat(requiredUavTypedLoad[j])
			};

			D3D12_FORMAT_SUPPORT1 mask1 = D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW;
			D3D12_FORMAT_SUPPORT2 mask2 = D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD | D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE;

			if(
				FAILED(device->lpVtbl->CheckFeatureSupport(device, D3D12_FEATURE_FORMAT_SUPPORT, &format, sizeof(format))) || 
				(format.Support2 & mask2) != mask2 || 
				(format.Support1 & mask1) != mask1
			) {
				Log_debugLnx("D3D12: Unsupported device %"PRIu32", doesn't support required format (typed uav load)", i);
				goto next;
			}
		}

		//Optional formats

		D3D12_FEATURE_DATA_FORMAT_SUPPORT format = (D3D12_FEATURE_DATA_FORMAT_SUPPORT) {
			.Format = DXGI_FORMAT_R32G32B32_FLOAT
		};

		U32 mask1 = D3D12_FORMAT_SUPPORT1_RENDER_TARGET | D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE | D3D12_FORMAT_SUPPORT1_BLENDABLE;

		if(
			SUCCEEDED(device->lpVtbl->CheckFeatureSupport(device, D3D12_FEATURE_FORMAT_SUPPORT, &format, sizeof(format))) && 
			(format.Support1 & mask1) == mask1
		)
			caps.dataTypes |= EGraphicsDataTypes_RGB32f;

		if(
			(Bool)(format.Format = DXGI_FORMAT_R32G32B32_SINT) &&
			SUCCEEDED(device->lpVtbl->CheckFeatureSupport(device, D3D12_FEATURE_FORMAT_SUPPORT, &format, sizeof(format))) && 
			format.Support1 & D3D12_FORMAT_SUPPORT1_RENDER_TARGET
		)
			caps.dataTypes |= EGraphicsDataTypes_RGB32i;

		if(
			(Bool)(format.Format = DXGI_FORMAT_R32G32B32_UINT) &&
			SUCCEEDED(device->lpVtbl->CheckFeatureSupport(device, D3D12_FEATURE_FORMAT_SUPPORT, &format, sizeof(format))) && 
			format.Support1 & D3D12_FORMAT_SUPPORT1_RENDER_TARGET
		)
			caps.dataTypes |= EGraphicsDataTypes_RGB32u;

		//Require 8x MSAA for RGBA32f and RGB32f

		D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msaa = (D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS) {
			.Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
			.SampleCount = 8
		};

		if(
			FAILED(device->lpVtbl->CheckFeatureSupport(device, D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msaa, sizeof(msaa))) || 
			!msaa.NumQualityLevels
		) {
			Log_debugLnx("D3D12: Unsupported device %"PRIu32", doesn't support required MSAA flag for RGBA32f", i);
			goto next;
		}

		if(
			(caps.dataTypes & EGraphicsDataTypes_RGB32f) &&
			(Bool)(msaa.Format = DXGI_FORMAT_R32G32B32_FLOAT) && (
				FAILED(device->lpVtbl->CheckFeatureSupport(device, D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msaa, sizeof(msaa))) || 
				!msaa.NumQualityLevels
			)
		)
			caps.dataTypes &= ~EGraphicsDataTypes_RGB32f;

		//Fully converted type

		gotoIfError(clean, ListGraphicsDeviceInfo_resize(&tempInfos, tempInfos.length + 1, (Allocator){ 0 }))

		GraphicsDeviceInfo *info = tempInfos.ptrNonConst + tempInfos.length - 1;

		*info = (GraphicsDeviceInfo) {
			.type = type,
			.vendor = vendorId,
			.id = i,
			.capabilities = caps,
			.luid = luid,
			.uuid = { luid, 0 }
		};

		gotoIfError(clean, CharString_createFromUTF16x(desc.Description, 128, &tmp));

		Buffer_copy(
			Buffer_createRef(info->name, sizeof(info->name)),
			Buffer_createRefConst(tmp.ptr, CharString_length(tmp))
		);

		//Query driver version as string

		if(vendorId == EGraphicsVendorId_AMD)
			Buffer_copy(
				Buffer_createRef(info->driverInfo, sizeof(info->driverInfo)),
				CharString_bufferConst(instanceExt->amdDriverVersion)
			);

		else if(vendorId == EGraphicsVendorId_NV) {

			Buffer_copy(
				Buffer_createRef(info->driverInfo, sizeof(info->driverInfo)),
				CharString_bufferConst(instanceExt->nvDriverVersion)
			);

			//Raytracing validation

			NvAPI_Status status = NvAPI_D3D12_EnableRaytracingValidation(device, NVAPI_D3D12_RAYTRACING_VALIDATION_FLAG_NONE);

			if(status != NVAPI_ACCESS_DENIED && status != NVAPI_OK)
				Log_debugLnx("D3D12: NVAPI: Couldn't enable raytracing validation on device %"PRIu32"", i);

			if(status == NVAPI_OK)
				info->capabilities.features |= EGraphicsFeatures_RayValidation;

			//SER (Shader execution reordering)

			NVAPI_D3D12_RAYTRACING_THREAD_REORDERING_CAPS ser = (NVAPI_D3D12_RAYTRACING_THREAD_REORDERING_CAPS) { 0 };

			status = NvAPI_D3D12_GetRaytracingCaps(
				device0, NVAPI_D3D12_RAYTRACING_CAPS_TYPE_THREAD_REORDERING, &ser, sizeof(ser)
			);

			if(status != NVAPI_OK)
				Log_debugLnx("D3D12: NVAPI: Couldn't query for SER on device %"PRIu32"", i);

			else if(ser == NVAPI_D3D12_RAYTRACING_THREAD_REORDERING_CAP_STANDARD)
				info->capabilities.features |= EGraphicsFeatures_RayReorder;

			//Opacity micromaps

			NVAPI_D3D12_RAYTRACING_OPACITY_MICROMAP_CAPS omm = (NVAPI_D3D12_RAYTRACING_OPACITY_MICROMAP_CAPS) { 0 };

			status = NvAPI_D3D12_GetRaytracingCaps(
				device0, NVAPI_D3D12_RAYTRACING_CAPS_TYPE_OPACITY_MICROMAP, &omm, sizeof(omm)
			);

			if(status != NVAPI_OK)
				Log_debugLnx("D3D12: NVAPI: Couldn't query for OMM on device %"PRIu32"", i);

			else if(omm == NVAPI_D3D12_RAYTRACING_OPACITY_MICROMAP_CAP_STANDARD)
				info->capabilities.features |= EGraphicsFeatures_RayMicromapOpacity;

			//Displacement micromaps

			NVAPI_D3D12_RAYTRACING_DISPLACEMENT_MICROMAP_CAPS dmm = (NVAPI_D3D12_RAYTRACING_DISPLACEMENT_MICROMAP_CAPS) { 0 };

			status = NvAPI_D3D12_GetRaytracingCaps(
				device0, NVAPI_D3D12_RAYTRACING_CAPS_TYPE_DISPLACEMENT_MICROMAP, &dmm, sizeof(dmm)
			);

			if(status != NVAPI_OK)
				Log_debugLnx("D3D12: NVAPI: Couldn't query for DMM on device %"PRIu32"", i);

			else if(omm == NVAPI_D3D12_RAYTRACING_DISPLACEMENT_MICROMAP_CAP_STANDARD)
				info->capabilities.features |= EGraphicsFeatures_RayMicromapDisplacement;
		}

		else {
			//TODO: Intel, QCOM, etc.
		}

		//Release temporary device

	next:

		if(device)
			device->lpVtbl->Release(device);

		device = NULL;
	}

	if(!tempInfos.length)
		gotoIfError(clean, Error_unsupportedOperation(0, "GraphicsInstance_getDeviceInfos() no supported OxC3 device found"))

	*result = tempInfos;

clean:

	if(err.genericError)
		ListGraphicsDeviceInfo_freex(&tempInfos);

	if(device)
		device->lpVtbl->Release(device);		//Release device. We might re-create, but we can't pass it around

	for(U64 i = 0; i < adapters.length; ++i)
		adapters.ptr[i]->lpVtbl->Release(adapters.ptr[i]);

	CharString_freex(&tmp);
	ListIDXGIAdapter4_freex(&adapters);
	return err;
}
