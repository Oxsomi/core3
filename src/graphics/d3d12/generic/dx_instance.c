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

//graphics/d3d12/generic/dx_instance.c

#include "types/container/list_impl.h"
#include "types/base/platform_types.h"
#include "graphics/generic/interface.h"
#include "graphics/d3d12/dx_interface.h"
#include "graphics/d3d12/dx_device.h"
#include "graphics/d3d12/dx_buffer.h"
#include "graphics/d3d12/dx_swapchain.h"
#include "graphics/generic/instance.h"
#include "graphics/generic/device_info.h"
#include "platforms/platform.h"
#include "platforms/logx.h"
#include "platforms/dynamic_library.h"
#include "types/container/string.h"
#include "types/container/string_unicode.h"
#include "types/container/file_base.h"
#include "types/container/buffer.h"
#include "types/base/error.h"
#include "types/base/platform_types.h"
#include "types/base/mathi.h"
#include "types/base/constants.h"

#include <dxgi1_6.h>
#include <d3d11.h>            //AMD AGS needs it...

#if defined(_HAS_NV_API) && _ARCH == ARCH_X86_64    //TODO: Enable for arm later
	#include <nvapi.h>
	#define USE_NVAPI
#endif

#ifdef _HAS_AMD_AGS
	#include <amd_ags.h>
#endif

TListNamed(IDXGIAdapter4*, ListIDXGIAdapter4)
TListNamedImpl(ListIDXGIAdapter4)

GraphicsObjectSizes DxGraphicsObjectSizes = {
	.blas = sizeof(DxBLAS),
	.tlas = sizeof(DxTLAS),
	.pipeline = sizeof(DxPipeline),
	.sampler = 16,        //Doesn't exist
	.buffer = sizeof(DxDeviceBuffer),
	.image = sizeof(DxUnifiedTexture),
	.swapchain = sizeof(DxSwapchain),
	.device = sizeof(DxGraphicsDevice),
	.instance = sizeof(DxGraphicsInstance),
	.descriptorLayout = sizeof(DxDescriptorLayout),
	.descriptorTable = sizeof(DxDescriptorTable),
	.descriptorHeap = sizeof(DxDescriptorHeap),
	.pipelineLayout = sizeof(ID3D12RootSignature) + 8
};

#ifndef GRAPHICS_API_DYNAMIC
	const GraphicsObjectSizes *GraphicsInterface_getObjectSizes(EGraphicsApi api) {
		(void) api;
		return &DxGraphicsObjectSizes;
	}
#else
	EXPORT_SYMBOL GraphicsInterfaceTable GraphicsInterface_getTable(Platform *instance, GraphicsInterface *interf) {

		Platform_instance = instance;
		GraphicsInterface_instance = interf;

		return (GraphicsInterfaceTable) {

			.api = EGraphicsApi_Direct3D12,
			.objectSizes = DxGraphicsObjectSizes,

			.blasInit = D3D12BLAS_init,
			.blasFlush = D3D12BLASRef_flush,
			.blasFree = D3D12BLAS_free,

			.tlasInit = D3D12TLAS_init,
			.tlasFlush = D3D12TLASRef_flush,
			.tlasFree = D3D12TLAS_free,

			.pipelineCreateGraphics = D3D12GraphicsDevice_createPipelineGraphics,
			.pipelineCreateCompute = D3D12GraphicsDevice_createPipelineCompute,
			.pipelineCreateRt = D3D12GraphicsDevice_createPipelineRaytracingInternal,
			.pipelineFree = D3D12Pipeline_free,

			.samplerCreate = D3D12GraphicsDeviceRef_createSampler,
			.samplerFree = D3D12Sampler_free,

			.bufferCreate = D3D12GraphicsDeviceRef_createBuffer,
			.bufferFlush = D3D12DeviceBufferRef_flush,
			.bufferFree = D3D12DeviceBuffer_free,

			.textureCreate = D3D12UnifiedTexture_create,
			.textureFlush = D3D12DeviceTextureRef_flush,
			.textureFree = D3D12UnifiedTexture_free,

			.swapchainCreate = D3D12GraphicsDeviceRef_createSwapchain,
			.swapchainFree = D3D12Swapchain_free,

			.descriptorLayoutCreate = D3D12GraphicsDeviceRef_createDescriptorLayout,
			.descriptorLayoutFree = D3D12DescriptorLayout_free,
			.pipelineLayoutCreate = D3D12GraphicsDeviceRef_createPipelineLayout,
			.pipelineLayoutFree = D3D12PipelineLayout_free,
			.descriptorHeapCreate = D3D12GraphicsDeviceRef_createDescriptorHeap,
			.descriptorHeapFree = D3D12DescriptorHeap_free,

			.descriptorTableCreate = D3D12DescriptorHeap_createDescriptorTable,
			.descriptorTableFree = D3D12DescriptorTable_free,
			.descriptorTableSet = D3D12DescriptorTable_setDescriptors,
			.descriptorTableUnset = D3D12DescriptorTable_unsetDescriptors,

			.memoryAllocate = D3D12DeviceMemoryAllocator_allocate,
			.memoryFree = D3D12DeviceMemoryAllocator_freeAllocation,

			.deviceInit = D3D12GraphicsDevice_init,
			.deviceWait = D3D12GraphicsDeviceRef_wait,
			.deviceFree = D3D12GraphicsDevice_free,
			.deviceSubmitCommands = D3D12GraphicsDevice_submitCommands,
			.deviceGetMemoryBudget = D3D12GraphicsDevice_getMemoryBudget,

			.commandListProcess = D3D12CommandList_process,

			.instanceCreate = D3D12GraphicsInstance_create,
			.instanceFree = D3D12GraphicsInstance_free,
			.instanceGetDevices = D3D12GraphicsInstance_getDeviceInfos
		};
	}
#endif

void DX_WRAP_FUNC(GraphicsInstance_free)(GraphicsInstance *data, const Allocator *alloc) {

	DxGraphicsInstance *instanceExt = GraphicsInstance_ext(data, Dx);

	if(!instanceExt)
		return;

	if(instanceExt->debug1NoSingleton)
		instanceExt->debug1NoSingleton->lpVtbl->Release(instanceExt->debug1NoSingleton);

	if(instanceExt->debug1Singleton)
		instanceExt->debug1Singleton->lpVtbl->Release(instanceExt->debug1Singleton);

	#ifdef USE_NVAPI
		if(instanceExt->flags & EDxGraphicsInstanceFlags_HasNVApi)
			NvAPI_Unload();
	#endif

	#ifdef _HAS_AMD_AGS
		if(instanceExt->flags & EDxGraphicsInstanceFlags_HasAMDAgs)
			agsDeInitialize(instanceExt->agsContext);
	#endif

	CharString_free(&instanceExt->nvDriverVersion, alloc);
	CharString_free(&instanceExt->amdDriverVersion, alloc);

	if(instanceExt->factory)
		instanceExt->factory->lpVtbl->Release(instanceExt->factory);

	if(instanceExt->config)
		instanceExt->config->lpVtbl->Release(instanceExt->config);

	if(instanceExt->deviceFactoryNoSingleton)
		instanceExt->deviceFactoryNoSingleton->lpVtbl->Release(instanceExt->deviceFactoryNoSingleton);

	if(instanceExt->deviceFactorySingleton)
		instanceExt->deviceFactorySingleton->lpVtbl->Release(instanceExt->deviceFactorySingleton);
}

Bool DX_WRAP_FUNC(GraphicsInstance_create)(
	const GraphicsApplicationInfo *info, GraphicsInstanceRef **instanceRef, Error *e_rr
) {

	Bool s_uccess = true;
	const Allocator *alloc = instanceRef && *instanceRef ? GraphicsInstanceRef_ptr(*instanceRef)->alloc : NULL;

	(void)info;
	GraphicsInstance *instance = GraphicsInstanceRef_ptr(*instanceRef);
	DxGraphicsInstance *instanceExt = GraphicsInstance_ext(instance, Dx);
	CharString locationD3D12 = CharString_createNull();
	Bool isVirtual = false;

	//Braced so the else unambiguously belongs to the FAILED check, which is what it always meant.
	//A debug factory that came up fine skips the non-debug creation below.

	if (instance->flags & EGraphicsInstanceFlags_IsDebug) {

		if (FAILED(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, &IID_IDXGIFactory6, (void**)&instanceExt->factory))) {
			Log_warnLnx(
				"Tried to enable debugging on DxGraphicsInstance but couldn't, "
				"please setup the developer environment for this functionality"
			);

			instance->flags &= ~EGraphicsInstanceFlags_IsDebug;
		}

		else goto setup;
	}

	gotoIfError3(clean, dxCheck(CreateDXGIFactory2(0, &IID_IDXGIFactory6, (void**) &instanceExt->factory), e_rr));

setup:

	const CharString d3d12Dir = CharString_createRefCStrConst("./D3D12/");

	gotoIfError3(clean, File_resolve(
		&d3d12Dir,
		&isVirtual,
		0,
		&Platform_instance->appDirectory,
		alloc,
		&locationD3D12,
		e_rr
	));

	gotoIfError3(clean, dxCheck(D3D12GetInterface(
		&CLSID_D3D12SDKConfiguration, &IID_ID3D12SDKConfiguration1, (void**) &instanceExt->config
	), e_rr));

	//CreateDeviceFactory must request the SDK version matching the D3D12Core.dll deployed next to us.
	//A preview Agility SDK ships a preview core (exports D3D12_PREVIEW_SDK_VERSION); a stable SDK ships the stable core.
	//Prefer the preview core (unlocks SM6.9/6.10 - cooperative vectors/matrix - but needs Windows Developer Mode),
	// and fall back to the stable version when the preview core isn't there so we still run on non-dev-mode machines.

	U32 sdkVersions[2];
	U32 sdkVersionCount = 0;

	#if defined(D3D12_PREVIEW_SDK_VERSION) && D3D12_PREVIEW_SDK_VERSION >= 720
		sdkVersions[sdkVersionCount++] = D3D12_PREVIEW_SDK_VERSION;
	#endif

	sdkVersions[sdkVersionCount++] = D3D12_SDK_VERSION;

	U32 usedSdkVersion = 0;
	HRESULT factoryHr = E_FAIL;

	for(U32 i = 0; i < sdkVersionCount; ++i) {

		factoryHr = instanceExt->config->lpVtbl->CreateDeviceFactory(
			instanceExt->config, sdkVersions[i], locationD3D12.ptr,
			&IID_ID3D12DeviceFactory, (void**) &instanceExt->deviceFactoryNoSingleton
		);

		if(SUCCEEDED(factoryHr)) {
			usedSdkVersion = sdkVersions[i];
			break;
		}

		if(i + 1 < sdkVersionCount)
			Log_debugLnx(
				"D3D12: preview SDK core unavailable (needs Windows Developer Mode + preview Agility SDK); "
				"falling back to the stable SDK version - SM6.10 / cooperative features are off"
			);
	}

	gotoIfError3(clean, dxCheck(factoryHr, e_rr));

	gotoIfError3(clean, dxCheck(instanceExt->deviceFactoryNoSingleton->lpVtbl->SetFlags(
		instanceExt->deviceFactoryNoSingleton, D3D12_DEVICE_FACTORY_FLAG_DISALLOW_STORING_NEW_DEVICE_AS_SINGLETON
	), e_rr));

	//Second factory reuses the version that just worked - no need to re-probe.

	gotoIfError3(clean, dxCheck(instanceExt->config->lpVtbl->CreateDeviceFactory(
		instanceExt->config,
		usedSdkVersion, locationD3D12.ptr,
		&IID_ID3D12DeviceFactory, (void**) &instanceExt->deviceFactorySingleton
	), e_rr));

	gotoIfError3(clean, dxCheck(instanceExt->deviceFactorySingleton->lpVtbl->SetFlags(
		instanceExt->deviceFactorySingleton, D3D12_DEVICE_FACTORY_FLAG_ALLOW_RETURNING_EXISTING_DEVICE
	), e_rr));

	//Preview shader models (SM6.9/6.10, required for the cooperative-vector/matrix ops) are experimental on D3D12.
	//Only meaningful when we actually got the preview core; enable per-instance on both factories (before any device).
	//Failure is non-fatal - SM6.10 simply isn't reported and the coop features aren't advertised
	// (they'd also be flagged in caps.experimentalFeatures).
	//It's an opt-in preview path, never a hard requirement.

	#if defined(D3D12_PREVIEW_SDK_VERSION) && D3D12_PREVIEW_SDK_VERSION >= 720

		if(usedSdkVersion == D3D12_PREVIEW_SDK_VERSION) {

			if(FAILED(instanceExt->deviceFactoryNoSingleton->lpVtbl->EnableExperimentalFeatures(
				instanceExt->deviceFactoryNoSingleton, 1, &D3D12ExperimentalShaderModels, NULL, NULL
			)))
				Log_debugLnx(
					"D3D12: experimental shader models unavailable "
					"(needs Windows Developer Mode); SM6.10 / cooperative features are off"
				);

			(void) instanceExt->deviceFactorySingleton->lpVtbl->EnableExperimentalFeatures(
				instanceExt->deviceFactorySingleton, 1, &D3D12ExperimentalShaderModels, NULL, NULL
			);
		}

	#endif

	if(instance->flags & EGraphicsInstanceFlags_IsDebug) {

		gotoIfError3(clean, dxCheck(instanceExt->deviceFactoryNoSingleton->lpVtbl->GetConfigurationInterface(
			instanceExt->deviceFactoryNoSingleton, &CLSID_D3D12Debug,
			&IID_ID3D12Debug1, (void**) &instanceExt->debug1NoSingleton
		), e_rr));

		instanceExt->debug1NoSingleton->lpVtbl->EnableDebugLayer(instanceExt->debug1NoSingleton);

		if(!(instance->flags & EGraphicsInstanceFlags_DisableGPUBV))
			instanceExt->debug1NoSingleton->lpVtbl->SetEnableGPUBasedValidation(instanceExt->debug1NoSingleton, true);

		gotoIfError3(clean, dxCheck(instanceExt->deviceFactorySingleton->lpVtbl->GetConfigurationInterface(
			instanceExt->deviceFactorySingleton, &CLSID_D3D12Debug,
			&IID_ID3D12Debug1, (void**) &instanceExt->debug1Singleton
		), e_rr));

		instanceExt->debug1Singleton->lpVtbl->EnableDebugLayer(instanceExt->debug1Singleton);

		if(!(instance->flags & EGraphicsInstanceFlags_DisableGPUBV))
			instanceExt->debug1Singleton->lpVtbl->SetEnableGPUBasedValidation(instanceExt->debug1Singleton, true);
	}

	//Check for NVApi

	#ifdef USE_NVAPI

		const NvAPI_Status status = NvAPI_Initialize();

		if(status == NVAPI_OK) {

			instanceExt->flags |= EDxGraphicsInstanceFlags_HasNVApi;

			U32 driverVersion = 0;
			NvAPI_ShortString shortString = { 0 };

			if(NvAPI_SYS_GetDriverAndBranchVersion((NvU32*)&driverVersion, shortString) != NVAPI_OK) {
				NvAPI_Unload();
				instanceExt->flags &=~ EDxGraphicsInstanceFlags_HasNVApi;
			}

			else {
				gotoIfError3(clean, CharString_format(
					alloc,
					&instanceExt->nvDriverVersion,
					e_rr,
					"%"PRIu32".%"PRIu32,
					driverVersion / 100,
					driverVersion % 100
				));
			}
		}

	#endif

	#ifdef _HAS_AMD_AGS

		//Check for AMDAgs

		const AGSConfiguration config = (AGSConfiguration) { 0 };

		AGSGPUInfo gpuInfo;
		if(agsInitialize(AGS_CURRENT_VERSION, &config, &instanceExt->agsContext, &gpuInfo) == AGS_SUCCESS) {

			instanceExt->flags |= EDxGraphicsInstanceFlags_HasAMDAgs;

			gotoIfError3(clean, CharString_createCopy(
				CharString_createRefCStrConst(gpuInfo.radeonSoftwareVersion),
				alloc,
				&instanceExt->amdDriverVersion,
				e_rr
			));

			if(CharString_length(instanceExt->amdDriverVersion) >= 256) {
				Log_warnLnx("D3D12GraphicsInstance_create() AMD AGS initialize failed, version string is too long");
				agsDeInitialize(instanceExt->agsContext);
				instanceExt->flags &=~ EDxGraphicsInstanceFlags_HasAMDAgs;
			}
		}

	#endif

	instance->api = EGraphicsApi_Direct3D12;
	instance->apiVersion = usedSdkVersion;

clean:
	CharString_free(&locationD3D12, alloc);
	return s_uccess;
}

#if D3D12_PREVIEW_SDK_VERSION >= 720

	//Query whether a thread-scope cooperative-vector matrix*vector (matvec) is supported for the given matrix element
	//type, with an FP16 vector input, bias and result (the shape guaranteed by the linalg Minimum Support Set).
	//Optionally reports whether the driver has to emulate it (e.g. FP8 up-converted to FP16 where there's no FP8 unit).
	//Returns false if the per-operation linalg query itself isn't supported.

	Bool DxGraphicsInstance_supportsCoopVecMatVec(
		ID3D12Device10 *device, D3D12_LINEAR_ALGEBRA_DATATYPE matrixType, Bool *emulated
	) {

		D3D12_FEATURE_DATA_LINEAR_ALGEBRA_MATRIX_OPERATION_SUPPORT op =
			(D3D12_FEATURE_DATA_LINEAR_ALGEBRA_MATRIX_OPERATION_SUPPORT) { 0 };

		op.OperationType = D3D12_LINEAR_ALGEBRA_OPERATION_TYPE_THREAD_VECTOR_MATRIX_MULTIPLY;
		op.ThreadVectorMatrixMultiply.VectorInputType = D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT16;
		op.ThreadVectorMatrixMultiply.MatrixInputType = matrixType;
		op.ThreadVectorMatrixMultiply.BiasInputType = D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT16;
		op.ThreadVectorMatrixMultiply.VectorResultType = D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT16;

		if(FAILED(device->lpVtbl->CheckFeatureSupport(
			device, D3D12_FEATURE_LINEAR_ALGEBRA_LINEAR_ALGEBRA_MATRIX_OPERATION_SUPPORT, &op, sizeof(op)
		)))
			return false;

		if(emulated)
			*emulated = !!(op.ThreadVectorMatrixMultiply.SupportFlags & (
				D3D12_LINEAR_ALGEBRA_MULTIPLICATION_SUPPORT_FLAG_EMULATED_INPUTS |
				D3D12_LINEAR_ALGEBRA_MULTIPLICATION_SUPPORT_FLAG_EMULATED_OUTPUTS
			));

		return !!(op.ThreadVectorMatrixMultiply.SupportFlags & D3D12_LINEAR_ALGEBRA_MULTIPLICATION_SUPPORT_FLAG_SUPPORTED);
	}

#endif

//Reporting every unmet requirement instead of bailing on the first one is what tells an emulator or a weak GPU apart;
// one says "this will never run OxC3", the other says "this could run OxC3 if the graphics spec were lowered".
//deviceUnsupported logs the requirement and counts it, the adapter loop rejects on that count once every check has run.
//requireCap names a single flag out of a D3D12_OPTIONS block,
// so a device is told which capability it lacks rather than which struct it failed.
//These read the adapter loop's locals (i, unsupportedCount) so they only work inside that loop.

#define deviceUnsupported(...) do { Log_debugLnx(__VA_ARGS__); ++unsupportedCount; } while(false)

#define requireCap(query, cap) if(!(query).cap) deviceUnsupported("D3D12: Device %"PRIu64" lacks capability " #cap, i)

Bool DX_WRAP_FUNC(GraphicsInstance_getDeviceInfos)(const GraphicsInstance *inst, ListGraphicsDeviceInfo *result, Error *e_rr) {

	Bool s_uccess = true;
	const Allocator *alloc = inst ? inst->alloc : NULL;

	const DxGraphicsInstance *instanceExt = GraphicsInstance_ext(inst, Dx);
	ListIDXGIAdapter4 adapters = (ListIDXGIAdapter4) { 0 };
	ListGraphicsDeviceInfo tempInfos = (ListGraphicsDeviceInfo) { 0 };
	ID3D12Device10 *device = NULL;        //Temporary device, we need to know
	CharString tmp = CharString_createNull();

	if(!inst || !result)
		retError(clean, Error_nullPointer(
			!inst ? 0 : 2,
			"D3D12GraphicsInstance_getDeviceInfos()::inst and result are required"
		));

	if(result->ptr)
		retError(clean, Error_invalidParameter(
			1, 0, "D3D12GraphicsInstance_getDeviceInfos()::result isn't empty, may indicate memleak"
		));

	//Get all possible adapters

	{
		HRESULT hr;
		U32 adapterId = 0;

		do {

			gotoIfError3(clean, ListIDXGIAdapter4_resize(&adapters, adapters.length + 1, alloc, e_rr));
			hr = instanceExt->factory->lpVtbl->EnumAdapters1(
				instanceExt->factory, adapterId, (IDXGIAdapter1**)&adapters.ptrNonConst[adapterId]
			);

			if(FAILED(hr))
				gotoIfError3(clean, ListIDXGIAdapter4_resize(&adapters, adapters.length - 1, alloc, e_rr));

			++adapterId;

		} while(SUCCEEDED(hr));

		if(hr != DXGI_ERROR_NOT_FOUND)
			gotoIfError3(clean, dxCheck(hr, e_rr));
	}

	//How many adapters DXGI handed us, captured before the duplicate pre-filter below can drop any of them.
	//"No adapter at all" and "every adapter was rejected" are reported as different errors at the end of this function.

	const U64 adapterCount = adapters.length;

	gotoIfError3(clean, ListGraphicsDeviceInfo_reserve(&tempInfos, adapters.length, alloc, e_rr));

	//Pre-filter, this removes duplicate devices that get generated because of virtual displays (RDP).
	//The reason why we want to filter this:
	//    Even though this is a valid D3D12 device, the LUID is not real.
	//    This would mean no other device can interop if it were to be selected (CUDA, Vulkan, etc.).
	//    It shows as multiple devices, which might give the indication of having multiple physical devices
	//        (e.g. 2 of the same GPU).

	for(U64 i = 0; i < adapters.length; ++i) {

		DXGI_ADAPTER_DESC3 desci = (DXGI_ADAPTER_DESC3) { 0 };
		gotoIfError3(clean, dxCheck(adapters.ptr[i]->lpVtbl->GetDesc3(adapters.ptr[i], &desci), e_rr));

		for(U64 j = 0; j < i; ++j) {

			DXGI_ADAPTER_DESC3 descj = (DXGI_ADAPTER_DESC3) { 0 };
			gotoIfError3(clean, dxCheck(adapters.ptr[j]->lpVtbl->GetDesc3(adapters.ptr[j], &descj), e_rr));

			//Duplicate, this is generally a virtual display

			if (desci.DeviceId == descj.DeviceId) {

				Bool isVirtualj =
					descj.GraphicsPreemptionGranularity == DXGI_GRAPHICS_PREEMPTION_DMA_BUFFER_BOUNDARY &&
					descj.ComputePreemptionGranularity == DXGI_COMPUTE_PREEMPTION_DMA_BUFFER_BOUNDARY;

				Bool isVirtuali =
					desci.GraphicsPreemptionGranularity == DXGI_GRAPHICS_PREEMPTION_DMA_BUFFER_BOUNDARY &&
					desci.ComputePreemptionGranularity == DXGI_COMPUTE_PREEMPTION_DMA_BUFFER_BOUNDARY;

				if (isVirtuali) {
					adapters.ptr[i]->lpVtbl->Release(adapters.ptr[i]);
					ListIDXGIAdapter4_erase(&adapters, i, NULL);
					--i;
					break;
				}

				if (isVirtualj) {
					adapters.ptr[j]->lpVtbl->Release(adapters.ptr[j]);
					ListIDXGIAdapter4_erase(&adapters, j, NULL);
					--i;
					break;
				}
			}
		}
	}

	//Get compatible adapters

	for(U64 i = 0; i < adapters.length; ++i) {

		DXGI_ADAPTER_DESC3 desc = (DXGI_ADAPTER_DESC3) { 0 };
		gotoIfError3(clean, dxCheck(adapters.ptr[i]->lpVtbl->GetDesc3(adapters.ptr[i], &desc), e_rr));

		//Every unmet requirement is logged and counted here, the adapter is only rejected after the last check has run.

		U64 unsupportedCount = 0;

		//Fences are required for D3D12

		if(!(desc.Flags & DXGI_ADAPTER_FLAG3_SUPPORT_MONITORED_FENCES))
			deviceUnsupported("DXGI: Unsupported device %"PRIu64", doesn't support D3D12 fences", i);

		EGraphicsVendorId vendorId = EGraphicsVendorId_Unknown;

		switch(desc.VendorId) {
			case EGraphicsVendorPCIE_NV:     vendorId = EGraphicsVendorId_NV;        break;
			case EGraphicsVendorPCIE_AMD:    vendorId = EGraphicsVendorId_AMD;       break;
			case EGraphicsVendorPCIE_ARM:    vendorId = EGraphicsVendorId_ARM;       break;
			case EGraphicsVendorPCIE_QCOM2:
			case EGraphicsVendorPCIE_QCOM:   vendorId = EGraphicsVendorId_QCOM;      break;
			case EGraphicsVendorPCIE_INTC:   vendorId = EGraphicsVendorId_INTC;      break;
			case EGraphicsVendorPCIE_IMGT:   vendorId = EGraphicsVendorId_IMGT;      break;
			case EGraphicsVendorPCIE_MSFT:   vendorId = EGraphicsVendorId_MSFT;      break;
			default: Log_debugLnx("Unrecognized vendor: %"PRIX32, desc.VendorId);    break;
		}

		//Grab properties

		EGraphicsDeviceType type = EGraphicsDeviceType_CPU;

		const U64 luid = *(const U64*) &desc.AdapterLuid;

		//Create temporary device.
		//Unfortunately, there's no other way to query features it seems.

		HRESULT lastError = instanceExt->deviceFactoryNoSingleton->lpVtbl->CreateDevice(
			instanceExt->deviceFactoryNoSingleton,
			(IUnknown*)adapters.ptr[i], D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device10, (void**) &device
		);

		Bool independentDevices = false;

		if(lastError == DXGI_ERROR_UNSUPPORTED)
			lastError = instanceExt->deviceFactorySingleton->lpVtbl->CreateDevice(
				instanceExt->deviceFactorySingleton,
				(IUnknown*)adapters.ptr[i], D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device10, (void**) &device
			);

		else independentDevices = true;

		//This is the one requirement that can't wait for the gate below.
		//Every capability query after it goes through the device we just failed to create,
		// so there is nothing further left to report on this adapter.

		if(FAILED(lastError)) {
			deviceUnsupported("D3D12: Unsupported device %"PRIu64", doesn't support feature level 11.0", i);
			goto next;
		}

		//Get capabilities

		GraphicsDeviceCapabilities caps = (GraphicsDeviceCapabilities) { 0 };

		caps.features |=
			EGraphicsFeatures_LUID | EGraphicsFeatures_MultiDrawIndirectCount |
			EGraphicsFeatures_GeometryShader | EGraphicsFeatures_SubgroupArithmetic | EGraphicsFeatures_SubgroupShuffle |
			EGraphicsFeatures_Wireframe | EGraphicsFeatures_LogicOp | EGraphicsFeatures_DualSrcBlend |
			EGraphicsFeatures_Multiview | EGraphicsFeatures_SubgroupOperations;

		caps.dataTypes |=
			EGraphicsDataTypes_I64 | EGraphicsDataTypes_BCn | EGraphicsDataTypes_MSAA2x | EGraphicsDataTypes_MSAA8x |
			EGraphicsDataTypes_D32S8;

		if(vendorId != EGraphicsVendorId_AMD)            //AMD creates D24X8 and S8X24 internally, don't want to report D24S8.
			caps.dataTypes |= EGraphicsDataTypes_D24S8;

		caps.features |= EGraphicsFeatures_DirectRendering;

		if(independentDevices)
			caps.featuresExt |= EDxGraphicsFeatures_IndependentDevices;

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
		D3D12_FEATURE_DATA_D3D12_OPTIONS14 opt14 = (D3D12_FEATURE_DATA_D3D12_OPTIONS14) { 0 };
		D3D12_FEATURE_DATA_D3D12_OPTIONS16 opt16 = (D3D12_FEATURE_DATA_D3D12_OPTIONS16) { 0 };
		D3D12_FEATURE_DATA_D3D12_OPTIONS17 opt17 = (D3D12_FEATURE_DATA_D3D12_OPTIONS17) { 0 };
		D3D12_FEATURE_DATA_D3D12_OPTIONS21 opt21 = (D3D12_FEATURE_DATA_D3D12_OPTIONS21) { 0 };

		D3D12_FEATURE_DATA_SHADER_MODEL shaderOpt = (D3D12_FEATURE_DATA_SHADER_MODEL) { 0 };
		D3D12_FEATURE_DATA_ARCHITECTURE1 arch = (D3D12_FEATURE_DATA_ARCHITECTURE1) { 0 };
		D3D12_FEATURE_DATA_HARDWARE_COPY hwCopy = (D3D12_FEATURE_DATA_HARDWARE_COPY) { 0 };
		D3D12_FEATURE_DATA_ROOT_SIGNATURE rootSig = (D3D12_FEATURE_DATA_ROOT_SIGNATURE) { 0 };

		#if D3D12_SDK_VERSION >= 618
			D3D12_FEATURE_DATA_TIGHT_ALIGNMENT tightAlignment = (D3D12_FEATURE_DATA_TIGHT_ALIGNMENT) { 0 };
		#endif

		//A query that failed and a capability that is absent are different problems, so they're reported apart.
		//The flags are only inspected when the query succeeded,
		// because a struct left zeroed by a failed query would claim every capability in it is missing.

		if(FAILED(device->lpVtbl->CheckFeatureSupport(device, D3D12_FEATURE_D3D12_OPTIONS, &opt0, sizeof(opt0))))
			deviceUnsupported("D3D12: Unsupported device %"PRIu64", couldn't query D3D12_OPTIONS", i);

		else {
			requireCap(opt0, TypedUAVLoadAdditionalFormats);
			requireCap(opt0, OutputMergerLogicOp);
		}

		if(opt0.ResourceBindingTier >= D3D12_RESOURCE_BINDING_TIER_3)
			caps.features |= EGraphicsFeatures_Bindless;

		if(opt0.ResourceHeapTier >= D3D12_RESOURCE_HEAP_TIER_2)
			caps.featuresExt |= EDxGraphicsFeatures_AllowCombineHeaps;

		if(opt0.DoublePrecisionFloatShaderOps)
			caps.dataTypes |= EGraphicsDataTypes_F64;

		//The subgroup size lives in the same struct, so it's checked here too rather than off a zeroed query.

		if(FAILED(device->lpVtbl->CheckFeatureSupport(device, D3D12_FEATURE_D3D12_OPTIONS1, &opt1, sizeof(opt1))))
			deviceUnsupported("D3D12: Unsupported device %"PRIu64", couldn't query D3D12_OPTIONS1", i);

		else {

			requireCap(opt1, WaveOps);
			requireCap(opt1, Int64ShaderOps);

			if(opt1.WaveLaneCountMin < 4 || opt1.WaveLaneCountMax > 128)
				deviceUnsupported(
					"D3D12: Unsupported device %"PRIu64", subgroup size %"PRIu32"-%"PRIu32" is not in range 4-128!",
					i, opt1.WaveLaneCountMin, opt1.WaveLaneCountMax
				);
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

			//DXR 1.2 natively supports SER (shader execution reordering, dx::HitObject) and opacity micromaps,
			// vendor-neutrally.
			if(opt5.RaytracingTier >= D3D12_RAYTRACING_TIER_1_2) {

				caps.features |= EGraphicsFeatures_RayReorder | EGraphicsFeatures_RayMicromapOpacity;

				//RayReorder above only means the SER API is available (it can be a no-op).
				//OPTIONS22 reports whether the device actually reorders,
				// which is the bit apps care about when deciding to restructure a shader for SER.
				D3D12_FEATURE_DATA_D3D12_OPTIONS22 opt22 = (D3D12_FEATURE_DATA_D3D12_OPTIONS22) { 0 };

				if(
					SUCCEEDED(device->lpVtbl->CheckFeatureSupport(
						device, D3D12_FEATURE_D3D12_OPTIONS22, &opt22, sizeof(opt22)
					)) &&
					opt22.ShaderExecutionReorderingActuallyReorders
				)
					caps.features2 |= EGraphicsFeatures2_RayReorderActual;
			}
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

		if(FAILED(device->lpVtbl->CheckFeatureSupport(device, D3D12_FEATURE_D3D12_OPTIONS8, &opt8, sizeof(opt8))))
			deviceUnsupported("D3D12: Unsupported device %"PRIu64", couldn't query D3D12_OPTIONS8", i);

		else requireCap(opt8, UnalignedBlockTexturesSupported);

		if (
			SUCCEEDED(device->lpVtbl->CheckFeatureSupport(device, D3D12_FEATURE_D3D12_OPTIONS9, &opt9, sizeof(opt9))) &&
			opt9.AtomicInt64OnTypedResourceSupported && opt9.AtomicInt64OnGroupSharedSupported
		)
			caps.dataTypes |= EGraphicsDataTypes_AtomicI64;

		if(opt9.DerivativesInMeshAndAmplificationShadersSupported)
			caps.features |= EGraphicsFeatures_MeshTaskTexDeriv;

		if(FAILED(device->lpVtbl->CheckFeatureSupport(device, D3D12_FEATURE_D3D12_OPTIONS12, &opt12, sizeof(opt12))))
			deviceUnsupported("D3D12: Unsupported device %"PRIu64", couldn't query D3D12_OPTIONS12", i);

		else requireCap(opt12, EnhancedBarriersSupported);

		if(
			SUCCEEDED(device->lpVtbl->CheckFeatureSupport(device, D3D12_FEATURE_D3D12_OPTIONS14, &opt14, sizeof(opt14))) &&
			opt14.WriteableMSAATexturesSupported
		)
			caps.features |= EGraphicsFeatures_WriteMSTexture;

		if(
			SUCCEEDED(device->lpVtbl->CheckFeatureSupport(device, D3D12_FEATURE_D3D12_OPTIONS16, &opt16, sizeof(opt16))) &&
			opt16.GPUUploadHeapSupported
		)
			caps.featuresExt |= EDxGraphicsFeatures_ReBAR;

		if(
			SUCCEEDED(device->lpVtbl->CheckFeatureSupport(device, D3D12_FEATURE_D3D12_OPTIONS17, &opt17, sizeof(opt17))) &&
			opt17.ManualWriteTrackingResourceSupported
		)
			caps.featuresExt |= EDxGraphicsFeatures_ReportReBARWrites;

		if(
			SUCCEEDED(device->lpVtbl->CheckFeatureSupport(device, D3D12_FEATURE_D3D12_OPTIONS21, &opt21, sizeof(opt21))) &&
			opt21.WorkGraphsTier >= D3D12_WORK_GRAPHS_TIER_1_0
		)
			caps.features |= EGraphicsFeatures_Workgraphs;

		if(
			SUCCEEDED(device->lpVtbl->CheckFeatureSupport(device, D3D12_FEATURE_HARDWARE_COPY, &hwCopy, sizeof(hwCopy))) &&
			hwCopy.Supported
		)
			caps.featuresExt |= EDxGraphicsFeatures_HardwareCopyQueue;

		rootSig.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;    //Nice way of querying..

		if(FAILED(device->lpVtbl->CheckFeatureSupport(device, D3D12_FEATURE_ROOT_SIGNATURE, &rootSig, sizeof(rootSig))))
			deviceUnsupported("D3D12: Unsupported device %"PRIu64", doesn't support required root signature 1.1", i);

		shaderOpt.HighestShaderModel = D3D_SHADER_MODEL_6_5;        //Nice way of querying DirectX...

		if(FAILED(device->lpVtbl->CheckFeatureSupport(device, D3D12_FEATURE_SHADER_MODEL, &shaderOpt, sizeof(shaderOpt))))
			deviceUnsupported("D3D12: Unsupported device %"PRIu64", doesn't support required shader model (6.5)", i);

		shaderOpt.HighestShaderModel = D3D_SHADER_MODEL_6_6;
		if(SUCCEEDED(device->lpVtbl->CheckFeatureSupport(device, D3D12_FEATURE_SHADER_MODEL, &shaderOpt, sizeof(shaderOpt)))) {

			caps.featuresExt |= EDxGraphicsFeatures_WaveSize | EDxGraphicsFeatures_PAQ | EDxGraphicsFeatures_SM6_6;
			caps.features |= EGraphicsFeatures_ComputeDeriv;

			//Full bindless: SM6.6 dynamic resources (ResourceDescriptorHeap/SamplerDescriptorHeap).
			//Requires binding tier 3 (EGraphicsFeatures_Bindless, set from OPTIONS above) on top of the shader model.

			if(caps.features & EGraphicsFeatures_Bindless)
				caps.features2 |= EGraphicsFeatures2_DescriptorHeap;
		}

		shaderOpt.HighestShaderModel = D3D_SHADER_MODEL_6_7;
		if(SUCCEEDED(device->lpVtbl->CheckFeatureSupport(device, D3D12_FEATURE_SHADER_MODEL, &shaderOpt, sizeof(shaderOpt))))
			caps.featuresExt |= EDxGraphicsFeatures_SM6_7;

		shaderOpt.HighestShaderModel = D3D_SHADER_MODEL_6_8;
		if(SUCCEEDED(device->lpVtbl->CheckFeatureSupport(device, D3D12_FEATURE_SHADER_MODEL, &shaderOpt, sizeof(shaderOpt))))
			caps.featuresExt |= EDxGraphicsFeatures_WaveSizeMinMax | EDxGraphicsFeatures_SM6_8;

		shaderOpt.HighestShaderModel = D3D_SHADER_MODEL_6_9;
		if(SUCCEEDED(device->lpVtbl->CheckFeatureSupport(device, D3D12_FEATURE_SHADER_MODEL, &shaderOpt, sizeof(shaderOpt))))
			caps.featuresExt |= EDxGraphicsFeatures_SM6_9;

		#if D3D12_SDK_VERSION >= 618

			if(SUCCEEDED(device->lpVtbl->CheckFeatureSupport(
				device, D3D12_FEATURE_D3D12_TIGHT_ALIGNMENT, &tightAlignment, sizeof(tightAlignment)
			)) && tightAlignment.SupportTier >= D3D12_TIGHT_ALIGNMENT_TIER_1)
				caps.featuresExt |= EDxGraphicsFeatures_TightAlignment;

		#endif

		#if D3D12_PREVIEW_SDK_VERSION >= 720
			shaderOpt.HighestShaderModel = D3D_SHADER_MODEL_6_10;
			if(SUCCEEDED(device->lpVtbl->CheckFeatureSupport(device, D3D12_FEATURE_SHADER_MODEL, &shaderOpt, sizeof(shaderOpt)))) {

				caps.featuresExt |= EDxGraphicsFeatures_SM6_10;

				//Ray tri vertex position fetch is an SM6.9 raytracing feature; still proxied off SM6.10 (no query wired).

				caps.features |= EGraphicsFeatures_RayTriPosition;
				caps.experimentalFeatures |= EGraphicsFeatures_RayTriPosition;

				//Cooperative vectors/matrix (DXIL "Linear Algebra") need the real linalg tier query IN ADDITION to the
				// shader model - a device can report SM6.10 yet expose no linalg tier, so SM6.10 alone would over-report.
				//Gate the coop features on the actual tier,
				// then confirm each family with the per-operation support query (which also reports native vs emulated).
				//Only reachable because experimental shader models were enabled above,
				// so they stay flagged in experimentalFeatures until the linalg API leaves preview.

				D3D12_FEATURE_DATA_LINEAR_ALGEBRA_SUPPORT linalg = (D3D12_FEATURE_DATA_LINEAR_ALGEBRA_SUPPORT) { 0 };

				if(
					SUCCEEDED(device->lpVtbl->CheckFeatureSupport(
						device, D3D12_FEATURE_LINEAR_ALGEBRA_SUPPORT, &linalg, sizeof(linalg)
					)) &&
					linalg.LinearAlgebraTier >= D3D12_LINEAR_ALGEBRA_TIER_1_0
				) {

					EGraphicsFeatures coopFeatures = EGraphicsFeatures_None;

					//Thread matvec = cooperative vectors (OxC3 CoopVec).
					//FP16 is in the guaranteed minimum support set.

					if(DxGraphicsInstance_supportsCoopVecMatVec(device, D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT16, NULL))
						coopFeatures |= EGraphicsFeatures_CoopVec;

					//FP8 (E4M3/E5M2) weights are also in the minimum set,
					// but may be emulated (up-cast to FP16) on GPUs without native FP8 - the query reports which,
					// so we can log it (both interpretations count as CoopFP8).

					Bool fp8Emulated = false;

					if(
						DxGraphicsInstance_supportsCoopVecMatVec(
							device, D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT8_E4M3FN, &fp8Emulated
						) ||
						DxGraphicsInstance_supportsCoopVecMatVec(
							device, D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT8_E5M2, &fp8Emulated
						)
					) {
						coopFeatures |= EGraphicsFeatures_CoopFP8;

						if(fp8Emulated)
							Log_debugLnx(
								"D3D12: cooperative-vector FP8 is emulated (no native FP8 hardware; up-converted to FP16)"
							);
					}

					//Thread outer-product accumulate = cooperative-vector training (the TIER_1_1 op set).

					D3D12_FEATURE_DATA_LINEAR_ALGEBRA_MATRIX_OPERATION_SUPPORT outer =
						(D3D12_FEATURE_DATA_LINEAR_ALGEBRA_MATRIX_OPERATION_SUPPORT) { 0 };

					outer.OperationType = D3D12_LINEAR_ALGEBRA_OPERATION_TYPE_THREAD_OUTER_PRODUCT;
					outer.ThreadOuterProductSupport.InputComponentType = D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT16;
					outer.ThreadOuterProductSupport.ResultComponentType = D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT16;

					if(
						SUCCEEDED(device->lpVtbl->CheckFeatureSupport(
							device, D3D12_FEATURE_LINEAR_ALGEBRA_LINEAR_ALGEBRA_MATRIX_OPERATION_SUPPORT,
							&outer, sizeof(outer)
						)) &&
						outer.ThreadOuterProductSupport.Supported
					)
						coopFeatures |= EGraphicsFeatures_CoopVecTraining;

					//Wave-scope matrix*matrix (GEMM) = cooperative matrix (OxC3 CoopMat).

					D3D12_FEATURE_DATA_D3D12_OPTIONS1 waveOpt = (D3D12_FEATURE_DATA_D3D12_OPTIONS1) { 0 };
					(void) device->lpVtbl->CheckFeatureSupport(
						device, D3D12_FEATURE_D3D12_OPTIONS1, &waveOpt, sizeof(waveOpt)
					);

					D3D12_FEATURE_DATA_LINEAR_ALGEBRA_MATRIX_OPERATION_SUPPORT wave =
						(D3D12_FEATURE_DATA_LINEAR_ALGEBRA_MATRIX_OPERATION_SUPPORT) { 0 };

					wave.OperationType = D3D12_LINEAR_ALGEBRA_OPERATION_TYPE_WAVE_MATRIX_MULTIPLY;
					wave.WaveMatrixMultiply.Inputs.WaveSize = waveOpt.WaveLaneCountMin ? waveOpt.WaveLaneCountMin : 32;
					wave.WaveMatrixMultiply.Inputs.MatrixAComponentType = D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT16;
					wave.WaveMatrixMultiply.Inputs.MatrixBComponentType = D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT16;
					wave.WaveMatrixMultiply.Inputs.AccumulatorComponentType = D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT16;

					if(
						SUCCEEDED(device->lpVtbl->CheckFeatureSupport(
							device, D3D12_FEATURE_LINEAR_ALGEBRA_LINEAR_ALGEBRA_MATRIX_OPERATION_SUPPORT,
							&wave, sizeof(wave)
						)) &&
						(wave.WaveMatrixMultiply.SupportFlags & D3D12_LINEAR_ALGEBRA_MULTIPLICATION_SUPPORT_FLAG_SUPPORTED)
					)
						coopFeatures |= EGraphicsFeatures_CoopMat;

					caps.features |= coopFeatures;
					caps.experimentalFeatures |= coopFeatures;
				}
			}

			//Batched asynchronous command list submission (D3D12_FEATURE_ASYNC_COMMANDS, Agility 1.720-preview).
			D3D12_FEATURE_DATA_ASYNC_COMMANDS asyncCmds = (D3D12_FEATURE_DATA_ASYNC_COMMANDS) { 0 };

			if(
				SUCCEEDED(device->lpVtbl->CheckFeatureSupport(
					device, D3D12_FEATURE_ASYNC_COMMANDS, &asyncCmds, sizeof(asyncCmds)
				)) && asyncCmds.Supported
			)
				caps.featuresExt |= EDxGraphicsFeatures_BatchedAsyncCommandList;
		#endif

		const Bool hasArch =
			SUCCEEDED(device->lpVtbl->CheckFeatureSupport(device, D3D12_FEATURE_ARCHITECTURE1, &arch, sizeof(arch)));

		if(!hasArch)
			deviceUnsupported("D3D12: Unsupported device %"PRIu64", couldn't query D3D12_FEATURE_ARCHITECTURE1", i);

		if(!(desc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE))
			type = !arch.UMA ? EGraphicsDeviceType_Dedicated : EGraphicsDeviceType_Integrated;

		U64 sharedMem = desc.SharedSystemMemory;
		U64 dedicatedMem = desc.DedicatedVideoMemory;

		if (type != EGraphicsDeviceType_Dedicated)
			dedicatedMem = sharedMem;

		//System memory and video memory are two separate requirements, so the device is told which of the two it fails.
		//The video memory one is withheld when ARCHITECTURE1 failed, because a zeroed arch reads as non-UMA,
		// which would report a UMA device as having no video memory on top of the query failure that already rejected it.

		if (sharedMem < 512 * MIBI)
			deviceUnsupported(
				"DXGI: Unsupported device %"PRIu64", not enough system memory (%"PRIu64" MiB, needs 512 MiB)",
				i, sharedMem / MIBI
			);

		if (hasArch && dedicatedMem < 512 * MIBI)
			deviceUnsupported(
				"DXGI: Unsupported device %"PRIu64", not enough video memory (%"PRIu64" MiB, needs 512 MiB)",
				i, dedicatedMem / MIBI
			);

		caps.sharedMemory = sharedMem;
		caps.dedicatedMemory = dedicatedMem;

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

			//Naming the format turns "some format is missing" into a list of exactly which ones are,
			// which is the difference between a device that is one format short and one that supports none of them.

			const C8 *formatName = ETextureFormatId_name[requiredUavTypedLoad[j]];

			if(FAILED(device->lpVtbl->CheckFeatureSupport(device, D3D12_FEATURE_FORMAT_SUPPORT, &format, sizeof(format))))
				deviceUnsupported(
					"D3D12: Unsupported device %"PRIu64", couldn't query format support for %s", i, formatName
				);

			else {

				if((format.Support1 & mask1) != mask1)
					deviceUnsupported(
						"D3D12: Unsupported device %"PRIu64", format %s doesn't support a typed unordered access view",
						i, formatName
					);

				if((format.Support2 & mask2) != mask2)
					deviceUnsupported(
						"D3D12: Unsupported device %"PRIu64", format %s doesn't support typed uav load and store",
						i, formatName
					);
			}
		}

		//Every requirement has run by now, so the device can be rejected with the full list already logged above.
		//This is the only rejection of a device that was created, and it goes through next: to release it.

		if (unsupportedCount) {
			Log_debugLnx("D3D12: Unsupported device %"PRIu64", %"PRIu64" requirement(s) not met", i, unsupportedCount);
			goto next;
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

		format.Format = DXGI_FORMAT_R32G32B32_SINT;

		if(
			SUCCEEDED(device->lpVtbl->CheckFeatureSupport(device, D3D12_FEATURE_FORMAT_SUPPORT, &format, sizeof(format))) &&
			format.Support1 & D3D12_FORMAT_SUPPORT1_RENDER_TARGET
		)
			caps.dataTypes |= EGraphicsDataTypes_RGB32i;

		format.Format = DXGI_FORMAT_R32G32B32_UINT;

		if(
			SUCCEEDED(device->lpVtbl->CheckFeatureSupport(device, D3D12_FEATURE_FORMAT_SUPPORT, &format, sizeof(format))) &&
			format.Support1 & D3D12_FORMAT_SUPPORT1_RENDER_TARGET
		)
			caps.dataTypes |= EGraphicsDataTypes_RGB32u;

		//8x MSAA is optional for RGBA32f and RGB32f

		Bool supportRGBX32MSAA = true;

		D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msaa = (D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS) {
			.Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
			.SampleCount = 8
		};

		if(
			FAILED(device->lpVtbl->CheckFeatureSupport(device, D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msaa, sizeof(msaa))) ||
			!msaa.NumQualityLevels
		)
			supportRGBX32MSAA = false;

		msaa.Format = DXGI_FORMAT_R32G32B32_FLOAT;

		if(
			(caps.dataTypes & EGraphicsDataTypes_RGB32f) && (
				FAILED(device->lpVtbl->CheckFeatureSupport(device, D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msaa, sizeof(msaa))) ||
				!msaa.NumQualityLevels
			)
		)
			supportRGBX32MSAA = false;

		if(supportRGBX32MSAA)
			caps.featuresExt |= EDxGraphicsFeatures_RGBX32fMSAA;

		//Max allocation size and max buffer size isn't queryable in D3D12 yet.
		//We hardcode the reported sizes from Vulkan and the hardlimit as defined by HLSL.
		//ByteAddressBuffer.Load takes an int, so only 2GiB accessible,
		// even though it's against the spec, intel does support it (by converting int to uint).

		caps.maxAllocationSize = vendorId == EGraphicsVendorId_AMD ? 2 * GIBI : 42 * GIGA / 10;
		caps.maxBufferSize = vendorId != EGraphicsVendorId_INTC ? 2 * GIBI : 42 * GIGA / 10;

		if(vendorId == EGraphicsVendorId_QCOM)
			caps.maxAllocationSize = caps.maxBufferSize = 1 * GIBI;

		//Fully converted type

		gotoIfError3(clean, ListGraphicsDeviceInfo_resize(&tempInfos, tempInfos.length + 1, alloc, e_rr));

		GraphicsDeviceInfo *info = tempInfos.ptrNonConst + tempInfos.length - 1;

		*info = (GraphicsDeviceInfo) {
			.type = type,
			.vendor = vendorId,
			.id = i,
			.capabilities = caps,
			.luid = luid,
			.uuid = { luid, 0 }
		};

		gotoIfError3(clean, CharString_createFromUTF16(desc.Description, 128, alloc, &tmp, e_rr));

		Buffer_memcpy(
			Buffer_createRef(info->name, sizeof(info->name)),
			Buffer_createRefConst(tmp.ptr, CharString_length(tmp))
		);

		//Query driver version as string

		if(vendorId == EGraphicsVendorId_AMD)
			Buffer_memcpy(
				Buffer_createRef(info->driverInfo, sizeof(info->driverInfo)),
				CharString_bufferConst(instanceExt->amdDriverVersion)
			);

		else if(vendorId == EGraphicsVendorId_NV) {

			Buffer_memcpy(
				Buffer_createRef(info->driverInfo, sizeof(info->driverInfo)),
				CharString_bufferConst(instanceExt->nvDriverVersion)
			);

			#ifdef USE_NVAPI

				//Raytracing validation

				NvAPI_Status status = NvAPI_D3D12_EnableRaytracingValidation(
					(ID3D12Device5*)device, NVAPI_D3D12_RAYTRACING_VALIDATION_FLAG_NONE
				);

				if(status != NVAPI_ACCESS_DENIED && status != NVAPI_OK)
					Log_debugLnx("D3D12: NVAPI: Couldn't enable raytracing validation on device %"PRIu64"", i);

				if(status == NVAPI_OK)
					info->capabilities.features |= EGraphicsFeatures_RayValidation;

				//SER and opacity micromaps are detected vendor-neutrally via D3D12_RAYTRACING_TIER_1_2 (see above).

				//Mega geometry (RTXMG) has no vendor-neutral D3D12 query, so it goes through NVAPI raytracing caps.
				//Two separate bits to match the Vulkan split (cluster acceleration structures vs partitioned TLAS).

				if(info->capabilities.features & EGraphicsFeatures_Raytracing) {

					NVAPI_D3D12_RAYTRACING_CLUSTER_OPERATIONS_CAPS clusterCaps =
						NVAPI_D3D12_RAYTRACING_CLUSTER_OPERATIONS_CAP_NONE;

					if(
						NvAPI_D3D12_GetRaytracingCaps(
							(ID3D12Device*)device,
							NVAPI_D3D12_RAYTRACING_CAPS_TYPE_CLUSTER_OPERATIONS,
							&clusterCaps, sizeof(clusterCaps)
						) == NVAPI_OK &&
						(clusterCaps & NVAPI_D3D12_RAYTRACING_CLUSTER_OPERATIONS_CAP_STANDARD)
					)
						info->capabilities.features2 |= EGraphicsFeatures2_RayClusterAS;

					NVAPI_D3D12_RAYTRACING_PARTITIONED_TLAS_CAPS ptlasCaps =
						NVAPI_D3D12_RAYTRACING_PARTITIONED_TLAS_CAP_NONE;

					if(
						NvAPI_D3D12_GetRaytracingCaps(
							(ID3D12Device*)device,
							NVAPI_D3D12_RAYTRACING_CAPS_TYPE_PARTITIONED_TLAS,
							&ptlasCaps, sizeof(ptlasCaps)
						) == NVAPI_OK &&
						(ptlasCaps & NVAPI_D3D12_RAYTRACING_PARTITIONED_TLAS_CAP_STANDARD)
					)
						info->capabilities.features2 |= EGraphicsFeatures2_RayPartitionedTLAS;

					//Indirect AS builds are inherent to the mega geometry APIs, so either implies the bit

					if(
						info->capabilities.features2 &
						(EGraphicsFeatures2_RayClusterAS | EGraphicsFeatures2_RayPartitionedTLAS)
					)
						info->capabilities.features2 |= EGraphicsFeatures2_RayIndirectASBuild;
				}

			#endif
		}

		else {

			LARGE_INTEGER version;

			if(SUCCEEDED(adapters.ptr[i]->lpVtbl->CheckInterfaceSupport(adapters.ptr[i], &IID_IDXGIDevice, &version))) {

				U16 maj = (U16)(version.QuadPart >> 16);

				U64 k = 0;
				U64 digits = maj < 10 ? 1 : (maj < 100 ? 2 : (maj < 1000 ? 3 : (maj < 10000 ? 4 : 5)));

				for (U64 j = 0; j < digits; ++j) {
					info->driverInfo[digits - 1 - j] = '0' + maj % 10;
					maj /= 10;
				}

				U16 min = (U16)version.QuadPart;
				info->driverInfo[digits] = '.';
				k += digits + 1;
				digits = min < 10 ? 1 : (min < 100 ? 2 : (min < 1000 ? 3 : (min < 10000 ? 4 : 5)));

				if(vendorId == EGraphicsVendorId_INTC)        //INTC is xxx.yyyy most likely. For safety only minor digits > 4
					digits = U64_max(4, digits);

				for (U16 j = 0; j < digits; ++j) {
					info->driverInfo[k + digits - 1 - j] = '0' + min % 10;
					min /= 10;
				}
			}

			else {
				Log_debugLnx("D3D12: Couldn't get driver version for device %"PRIu64"", i);
				goto next;
			}
		}

		//Release temporary device

	next:

		if(device)
			device->lpVtbl->Release(device);

		device = NULL;
	}

	//A machine with no adapter at all and a machine whose adapters OxC3 turned down are very different results.
	//The first isn't ours to fail on and a test should skip it, the second is a real result and says our spec is too high,
	// so they get distinct error codes for the caller to branch on.

	if(!tempInfos.length) {

		if(!adapterCount)
			retError(clean, Error_notFound(0, 0, "D3D12GraphicsInstance_getDeviceInfos() no graphics adapter present"));

		retError(clean, Error_unsupportedOperation(
			0, "D3D12GraphicsInstance_getDeviceInfos() no supported OxC3 device found"
		));
	}

	*result = tempInfos;

clean:

	if(!s_uccess)
		ListGraphicsDeviceInfo_free(&tempInfos, alloc);

	if(device)
		device->lpVtbl->Release(device);        //Release device. We might re-create, but we can't pass it around

	for(U64 i = 0; i < adapters.length; ++i)
		if(adapters.ptr[i])
			adapters.ptr[i]->lpVtbl->Release(adapters.ptr[i]);

	CharString_free(&tmp, alloc);
	ListIDXGIAdapter4_free(&adapters, alloc);
	return s_uccess;
}

#undef deviceUnsupported
#undef requireCap
