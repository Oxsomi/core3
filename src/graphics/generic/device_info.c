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

//graphics/generic/device_info.c

#include "types/container/list_impl.h"
#include "graphics/generic/device_info.h"
#include "graphics/generic/pipeline_structs.h"
#include "platforms/logx.h"
#include "types/container/texture_format.h"
#include "graphics/generic/instance.h"
#include "types/math/type_cast.h"
#include "formats/oiSH/sh_binaries.h"
#include "types/base/endianness.h"
#include "types/base/string_read_helper.h"

//Bit indexed: entry i names 1 << i. A bit with nothing behind it is still named, so the index of a name is
// always the bit it refers to and a table can never drift out of step with its enum.

const C8 *EGraphicsFeatures_names[EGraphicsFeatures_Count] = {
	"DirectRendering",
	"VariableRateShading",
	"MultiDrawIndirectCount",
	"MeshShader",
	"GeometryShader",
	"SubgroupArithmetic",
	"SubgroupShuffle",
	"Multiview",
	"Raytracing",
	"RayPipeline",
	"RayQuery",
	"RayMicromapOpacity",
	"RayTriPosition",
	"RayReorder",
	"RayValidation",
	"LUID",
	"Wireframe",
	"LogicOp",
	"DualSrcBlend",
	"Barycentrics",
	"SwapchainCompute",
	"ComputeDeriv",
	"MeshTaskTexDeriv",
	"WriteMSTexture",
	"Bindless",
	"SubgroupOperations",
	"CoopVec",
	"CoopMat",
	"CoopFP8",
	"CoopVecTraining",
	"SubgroupQuad",
	"Reserved"
};

const C8 *EGraphicsFeatures2_names[EGraphicsFeatures2_Count] = {
	"RayReorderActual",
	"DescriptorHeap",
	"RayClusterAS",
	"RayPartitionedTLAS",
	"RayIndirectASBuild",
	"RayMicromapOpacityActual",
	"Reserved6",
	"Timestamps",
	"Predication",
	"PipelineExecutableInfo",
	"RaySoftware",
	"DeviceFault",
	"WriteBufferImmediate",
	"ExperimentTier1",
	"ExperimentTier2"
};

const C8 *EGraphicsDataTypes_names[EGraphicsDataTypes_Count] = {
	"F64",
	"I64",
	"F16",
	"I16",
	"AtomicI64",
	"AtomicF32",
	"AtomicF64",
	"ASTC",
	"BCn",
	"MSAA2x",
	"MSAA8x",
	"RGB32f",
	"RGB32i",
	"RGB32u",
	"D24S8",
	"S8",
	"D32S8",
	"LinearFilter16Norm",
	"LinearFilter32f",
	"WriteRGB9E5"
};

const C8 *EVkGraphicsFeatures_names[EVkGraphicsFeatures_Count] = {
	"PerfQuery",
	"Maintenance4",
	"BufferDeviceAddress",
	"DriverProperties",
	"MemoryBudget",
	"PerformantPushDescriptor",
	"Reserved6",
	"NoCompactionQuery",
	"AddressBindingReport",
	"ExternalHostMemory",
	"MemoryPriority",
	"PageableMemory",
	"SubgroupSizeControl"
};

const C8 *EDxGraphicsFeatures_names[EDxGraphicsFeatures_Count] = {
	"Reserved0",
	"ReBAR",
	"HardwareCopyQueue",
	"WaveSize",
	"WaveSizeMinMax",
	"PAQ",
	"ReportReBARWrites",
	"TightAlignment",
	"AllowCombineHeaps",
	"RGBX32fMSAA",
	"IndependentDevices",
	"BatchedAsyncCommandList",
	"CacheCoherentUMA",
	"AllowTearing",
	"Reserved14",
	"Reserved15",
	"SM6_6",
	"SM6_7",
	"SM6_8",
	"SM6_9",
	"SM6_10"
};

//Shared only with generic/instance.c and generic/device.c when applying capability masks.

void GraphicsDeviceInfo_fixCapabilities(GraphicsDeviceInfo *info, EGraphicsApi api);

Bool GraphicsDeviceInfo_disableCapability(GraphicsDeviceInfo *deviceInfo, EGraphicsApi api, CharString name) {

	if(!deviceInfo || !name.ptr || !CharString_length(name) || (U32) api >= EGraphicsApi_Count)
		return false;

	GraphicsDeviceCapabilities *cap = &deviceInfo->capabilities;

	for (U64 i = 0; i < EGraphicsFeatures_Count; ++i)
		if(CharString_equalsCStringSensitive(&name, EGraphicsFeatures_names[i])) {
			const U32 bit = (U32)1 << i;
			cap->flags.features = (EGraphicsFeatures) ((U32) cap->flags.features &~ bit);
			GraphicsDeviceInfo_fixCapabilities(deviceInfo, api);
			return true;
		}

	for (U64 i = 0; i < EGraphicsFeatures2_Count; ++i)
		if(CharString_equalsCStringSensitive(&name, EGraphicsFeatures2_names[i])) {
			const U32 bit = (U32)1 << i;
			cap->flags.features2 = (EGraphicsFeatures2) ((U32) cap->flags.features2 &~ bit);
			GraphicsDeviceInfo_fixCapabilities(deviceInfo, api);
			return true;
		}

	for (U64 i = 0; i < EGraphicsDataTypes_Count; ++i)
		if(CharString_equalsCStringSensitive(&name, EGraphicsDataTypes_names[i])) {
			cap->flags.dataTypes = (EGraphicsDataTypes) ((U32) cap->flags.dataTypes &~ ((U32)1 << i));
			GraphicsDeviceInfo_fixCapabilities(deviceInfo, api);
			return true;
		}

	//featuresExt is whatever the running api says it is, so only that api's names resolve against it

	const Bool isDx = api == EGraphicsApi_Direct3D12;
	const C8 **extNames = isDx ? EDxGraphicsFeatures_names : EVkGraphicsFeatures_names;
	const U64 extCount = isDx ? (U64) EDxGraphicsFeatures_Count : (U64) EVkGraphicsFeatures_Count;

	for (U64 i = 0; i < extCount; ++i)
		if(CharString_equalsCStringSensitive(&name, extNames[i])) {
			cap->flags.featuresExt &=~ ((U32)1 << i);
			GraphicsDeviceInfo_fixCapabilities(deviceInfo, api);
			return true;
		}

	return false;
}

void GraphicsDeviceInfo_print(EGraphicsApi api, const GraphicsDeviceInfo *deviceInfo, Bool printCapabilities) {

	if(!deviceInfo || !deviceInfo->name[0])
		return;

	Log_debugLnx(
		"%s: %s (%s): %"PRIu64" bytes shared memory, %"PRIu64" bytes %s memory\n\t"
		"Max buffer size: %"PRIu64" bytes, max allocation size: %"PRIu64" bytes\n\t"
		"%s %"PRIu64"\n\tLUID %016"PRIx64"\n\tUUID %016"PRIx64"%016"PRIx64"\n\t"
		"Vendor: %s",
		api == EGraphicsApi_Direct3D12 ? "D3D12" : (api == EGraphicsApi_Vulkan ? "Vulkan" : "Unknown"),
		deviceInfo->name,
		deviceInfo->driverInfo,
		deviceInfo->capabilities.sharedMemory,
		deviceInfo->capabilities.dedicatedMemory,
		deviceInfo->type != EGraphicsDeviceType_Dedicated ? "total" : "dedicated",
		deviceInfo->capabilities.maxBufferSize,
		deviceInfo->capabilities.maxAllocationSize,
		(deviceInfo->type == EGraphicsDeviceType_CPU ? "CPU" : (
			deviceInfo->type == EGraphicsDeviceType_Dedicated ? "dGPU" : (
				deviceInfo->type == EGraphicsDeviceType_Integrated ? "iGPU" : "Simulated GPU"
			)
		)),
		deviceInfo->id,
		deviceInfo->capabilities.flags.features & EGraphicsFeatures_LUID ? U64_swapEndianness(deviceInfo->luid) : 0,
		U64_swapEndianness(deviceInfo->uuid[0]),
		U64_swapEndianness(deviceInfo->uuid[1]),
		ESHVendor_names[deviceInfo->vendor]
	);

	if (printCapabilities) {

		const GraphicsDeviceCapabilities cap = deviceInfo->capabilities;

		//Features

		const U32 feat = cap.flags.features;

		if(feat)
			Log_debugLnx("\tFeatures:");

		if(feat & EGraphicsFeatures_DirectRendering)
			Log_debugLnx("\t\tDirect rendering");

		if(feat & EGraphicsFeatures_VariableRateShading)
			Log_debugLnx("\t\tVariable rate shading");

		if(feat & EGraphicsFeatures_MultiDrawIndirectCount)
			Log_debugLnx("\t\tMulti draw indirect count");

		if(feat & EGraphicsFeatures_MeshShader)
			Log_debugLnx("\t\tMesh shaders");

		if(feat & EGraphicsFeatures_GeometryShader)
			Log_debugLnx("\t\tGeometry shaders");

		if(feat & EGraphicsFeatures_SubgroupArithmetic)
			Log_debugLnx("\t\tSubgroup arithmetic");

		if(feat & EGraphicsFeatures_SubgroupShuffle)
			Log_debugLnx("\t\tSubgroup shuffle");

		if(feat & EGraphicsFeatures_SubgroupQuad)
			Log_debugLnx("\t\tSubgroup quad");

		if(feat & EGraphicsFeatures_Multiview)
			Log_debugLnx("\t\tMultiview");

		if(feat & EGraphicsFeatures_Raytracing)
			Log_debugLnx("\t\tRaytracing");

		if(feat & EGraphicsFeatures_RayPipeline)
			Log_debugLnx("\t\tRaytracing pipeline");

		if(feat & EGraphicsFeatures_RayQuery)
			Log_debugLnx("\t\tRay query");

		if(cap.flags.features2 & EGraphicsFeatures2_RaySoftware)
			Log_debugLnx("\t\tRay traversal in software (no dedicated traversal units)");

		if(feat & EGraphicsFeatures_RayMicromapOpacity)
			Log_debugLnx(
				cap.flags.features2 & EGraphicsFeatures2_RayMicromapOpacityActual ?
				"\t\tRaytracing opacity micromap (real micromaps worth building)" :
				"\t\tRaytracing opacity micromap (prefer special indices)"
			);

		if(feat & EGraphicsFeatures_RayReorder)
			Log_debugLnx(
				cap.flags.features2 & EGraphicsFeatures2_RayReorderActual ?
				"\t\tRay reorder (actually reorders)" : "\t\tRay reorder (API only, no-op on this device)"
			);

		if(feat & EGraphicsFeatures_RayValidation)
			Log_debugLnx("\t\tRay validation");

		if(feat & EGraphicsFeatures_RayTriPosition)
			Log_debugLnx("\t\tRay triangle vertex position fetch");

		if(feat & EGraphicsFeatures_Barycentrics)
			Log_debugLnx("\t\tFragment shader barycentrics");

		if(cap.flags.features2 & EGraphicsFeatures2_RayClusterAS)
			Log_debugLnx("\t\tRaytracing cluster acceleration structures (mega geometry)");

		if(cap.flags.features2 & EGraphicsFeatures2_RayPartitionedTLAS)
			Log_debugLnx("\t\tRaytracing partitioned TLAS (mega geometry)");

		if(cap.flags.features2 & EGraphicsFeatures2_RayIndirectASBuild)
			Log_debugLnx("\t\tRaytracing indirect acceleration structure builds");

		if(cap.flags.features2 & EGraphicsFeatures2_Timestamps)
			Log_debugLnx("\t\tGPU timestamps (%f ns/tick)", (F64) cap.timestampPeriod);

		if(cap.flags.features2 & EGraphicsFeatures2_Predication)
			Log_debugLnx("\t\tPredicated scopes");

		if(cap.flags.features2 & EGraphicsFeatures2_DeviceFault)
			Log_debugLnx("\t\tDevice fault reporting");

		if(cap.flags.features2 & EGraphicsFeatures2_WriteBufferImmediate)
			Log_debugLnx("\t\tWrite buffer immediate");

		if(cap.flags.features2 & EGraphicsFeatures2_ExperimentTier1)
			Log_debugLnx("\t\tExperiment tier 1 (claimed by the driver, meaning undefined by OxC3)");

		if(cap.flags.features2 & EGraphicsFeatures2_ExperimentTier2)
			Log_debugLnx("\t\tExperiment tier 2 (claimed by the driver, meaning undefined by OxC3)");

		if(feat & EGraphicsFeatures_Wireframe)
			Log_debugLnx("\t\tWireframe (rasterizer fill mode: line)");

		if(feat & EGraphicsFeatures_LogicOp)
			Log_debugLnx("\t\tLogic op (blend state)");

		if(feat & EGraphicsFeatures_DualSrcBlend)
			Log_debugLnx("\t\tDual src blend (blend state)");

		if(feat & EGraphicsFeatures_SwapchainCompute)
			Log_debugLnx("\t\tSwapchain compute");

		if(feat & EGraphicsFeatures_ComputeDeriv)
			Log_debugLnx("\t\tCompute shader texture derivatives");

		if(feat & EGraphicsFeatures_MeshTaskTexDeriv)
			Log_debugLnx("\t\tMesh/task shader texture derivatives");

		if(feat & EGraphicsFeatures_WriteMSTexture)
			Log_debugLnx("\t\tWrite MS texture");

		if(feat & EGraphicsFeatures_Bindless)
			Log_debugLnx("\t\tBindless");

		if(cap.flags.features2 & EGraphicsFeatures2_DescriptorHeap)
			Log_debugLnx("\t\tDescriptor heap (full bindless)");

		if(cap.flags.features2 & EGraphicsFeatures2_PipelineExecutableInfo)
			Log_debugLnx("\t\tPipeline executable info (live ISA disassembly + statistics)");

		if(feat & EGraphicsFeatures_SubgroupOperations)
			Log_debugLnx(
				"\t\tSubgroup operations (WaveSize %"PRIu8" to %"PRIu8" lanes)", cap.waveSizeMin, cap.waveSizeMax
			);

		if(feat & EGraphicsFeatures_CoopVec)
			Log_debugLnx("\t\tCooperative vectors (matvec)");

		if(feat & EGraphicsFeatures_CoopMat)
			Log_debugLnx("\t\tCooperative matrix (GEMM)");

		if(feat & EGraphicsFeatures_CoopFP8)
			Log_debugLnx("\t\tCooperative FP8 (e4m3/e5m2)");

		if(feat & EGraphicsFeatures_CoopVecTraining)
			Log_debugLnx("\t\tCooperative vector training");

		if(cap.experimentalFeatures)
			Log_debugLnx("\t\t(some of the features above are experimental preview)");

		//Data types

		const U32 dat = cap.flags.dataTypes;

		if(dat)
			Log_debugLnx("\tData types:");

		if(dat & EGraphicsDataTypes_F64)
			Log_debugLnx("\t\t64-bit floats");

		if(dat & EGraphicsDataTypes_I64)
			Log_debugLnx("\t\t64-bit integers");

		if(dat & EGraphicsDataTypes_F16)
			Log_debugLnx("\t\t16-bit floats");

		if(dat & EGraphicsDataTypes_I16)
			Log_debugLnx("\t\t16-bit ints");

		if(dat & EGraphicsDataTypes_AtomicI64)
			Log_debugLnx("\t\t64-bit integer atomics (buffer)");

		if(dat & EGraphicsDataTypes_AtomicF32)
			Log_debugLnx("\t\t32-bit float atomics (buffer)");

		if(dat & EGraphicsDataTypes_AtomicF64)
			Log_debugLnx("\t\t64-bit float atomics (buffer)");

		if(dat & EGraphicsDataTypes_ASTC)
			Log_debugLnx("\t\tASTC compression");

		if(dat & EGraphicsDataTypes_BCn)
			Log_debugLnx("\t\tBCn compression");

		if(dat & EGraphicsDataTypes_MSAA2x)
			Log_debugLnx("\t\tMSAA 2x");

		if(dat & EGraphicsDataTypes_MSAA8x)
			Log_debugLnx("\t\tMSAA 8x");

		if(dat & EGraphicsDataTypes_S8)
			Log_debugLnx("\t\tEDepthStencilFormat_S8");

		if(dat & EGraphicsDataTypes_D24S8)
			Log_debugLnx("\t\tEDepthStencilFormat_D24S8");

		if(dat & EGraphicsDataTypes_D32S8)
			Log_debugLnx("\t\tEDepthStencilFormat_D32S8");

		if(dat & EGraphicsDataTypes_RGB32f)
			Log_debugLnx("\t\tETextureFormat_RGBA32f for use in textures (not just vertex input)");

		if(dat & EGraphicsDataTypes_RGB32u)
			Log_debugLnx("\t\tETextureFormat_RGBA32u for use in textures (not just vertex input)");

		if(dat & EGraphicsDataTypes_RGB32i)
			Log_debugLnx("\t\tETextureFormat_RGBA32i for use in textures (not just vertex input)");

		if(dat & EGraphicsDataTypes_LinearFilter16Norm)
			Log_debugLnx("\t\tLinear filtering of 16 bit unorm/snorm textures");

		if(dat & EGraphicsDataTypes_LinearFilter32f)
			Log_debugLnx("\t\tLinear filtering of 32 bit float textures");

		if(dat & EGraphicsDataTypes_WriteRGB9E5)
			Log_debugLnx("\t\tWriting RGB9E5 (storage image and render target)");

		//API specific features

		if(api == EGraphicsApi_Direct3D12) {

			if(cap.flags.featuresExt)
				Log_debugLnx("\tD3D12 Extensions:");

			if(cap.flags.featuresExt & EDxGraphicsFeatures_ReBAR)
				Log_debugLnx("\t\tReBAR");

			if(
				(cap.flags.featuresExt & EDxGraphicsFeatures_ReallyReportReBARWrites) ==
				EDxGraphicsFeatures_ReallyReportReBARWrites
			)
				Log_debugLnx("\t\tReport ReBAR writes to tool");

			if(cap.flags.featuresExt & EDxGraphicsFeatures_HardwareCopyQueue)
				Log_debugLnx("\t\tHardware copy queue");

			if(cap.flags.featuresExt & EDxGraphicsFeatures_WaveSize)
				Log_debugLnx("\t\tWaveSize");

			if(cap.flags.featuresExt & EDxGraphicsFeatures_WaveSizeMinMax)
				Log_debugLnx("\t\tWaveSize (min/max)");

			if(cap.flags.featuresExt & EDxGraphicsFeatures_PAQ)
				Log_debugLnx("\t\tPAQ (Payload Access Qualifiers)");

			if(cap.flags.featuresExt & EDxGraphicsFeatures_ReportReBARWrites)
				Log_debugLnx("\t\tReport ReBAR writes (tool)");

			if(cap.flags.featuresExt & EDxGraphicsFeatures_TightAlignment)
				Log_debugLnx("\t\tTight resource alignment");

			if(cap.flags.featuresExt & EDxGraphicsFeatures_AllowCombineHeaps)
				Log_debugLnx("\t\tAllow combine heaps");

			if(cap.flags.featuresExt & EDxGraphicsFeatures_IndependentDevices)
				Log_debugLnx("\t\tIndependent devices");

			if(cap.flags.featuresExt & EDxGraphicsFeatures_BatchedAsyncCommandList)
				Log_debugLnx("\t\tBatched async command list");

			if(cap.flags.featuresExt & EDxGraphicsFeatures_CacheCoherentUMA)
				Log_debugLnx("\t\tCache coherent UMA");

			if(cap.flags.featuresExt & EDxGraphicsFeatures_AllowTearing)
				Log_debugLnx("\t\tAllow tearing");

			if(cap.flags.featuresExt & EDxGraphicsFeatures_RGBX32fMSAA)
				Log_debugLnx("\t\tRGB(A)32f supports MSAA");

			if(cap.flags.featuresExt & EDxGraphicsFeatures_SM6_6)
				Log_debugLnx("\t\tShader model 6.6");

			if(cap.flags.featuresExt & EDxGraphicsFeatures_SM6_7)
				Log_debugLnx("\t\tShader model 6.7");

			if(cap.flags.featuresExt & EDxGraphicsFeatures_SM6_8)
				Log_debugLnx("\t\tShader model 6.8");

			if(cap.flags.featuresExt & EDxGraphicsFeatures_SM6_9)
				Log_debugLnx("\t\tShader model 6.9");

			if(cap.flags.featuresExt & EDxGraphicsFeatures_SM6_10)
				Log_debugLnx("\t\tShader model 6.10");
		}

		else if(api == EGraphicsApi_Vulkan) {

			if(cap.flags.featuresExt)
				Log_debugLnx("\tVulkan extensions:");

			if(cap.flags.featuresExt & EVkGraphicsFeatures_PerfQuery)
				Log_debugLnx("\t\tPerformance query");

			if(cap.flags.featuresExt & EVkGraphicsFeatures_Maintenance4)
				Log_debugLnx("\t\tMaintenance4");

			if(cap.flags.featuresExt & EVkGraphicsFeatures_BufferDeviceAddress)
				Log_debugLnx("\t\tBuffer device address");

			if(cap.flags.featuresExt & EVkGraphicsFeatures_DriverProperties)
				Log_debugLnx("\t\tDriver properties");

			if(cap.flags.featuresExt & EVkGraphicsFeatures_MemoryBudget)
				Log_debugLnx("\t\tMemory budget");

			if(cap.flags.featuresExt & EVkGraphicsFeatures_PerformantPushDescriptor)
				Log_debugLnx("\t\tPush descriptors");

			if(cap.flags.featuresExt & EVkGraphicsFeatures_AddressBindingReport)
				Log_debugLnx("\t\tAddress binding report");

			if(cap.flags.featuresExt & EVkGraphicsFeatures_ExternalHostMemory)
				Log_debugLnx("\t\tExternal host memory");

			if(cap.flags.featuresExt & EVkGraphicsFeatures_MemoryPriority)
				Log_debugLnx("\t\tMemory priority");

			if(cap.flags.featuresExt & EVkGraphicsFeatures_PageableMemory)
				Log_debugLnx("\t\tPageable device local memory");

			if(cap.flags.featuresExt & EVkGraphicsFeatures_SubgroupSizeControl)
				Log_debugLnx("\t\tSubgroup size control");
		}
	}
}

//Capabilities neither API reports, worked out from the ones it did.
//Called on every GraphicsDeviceInfo the instance hands out, so a device search can filter on these bits like
// on any other, which is the whole reason they are bits rather than a query function.

void GraphicsDeviceInfo_deriveCapabilities(GraphicsDeviceInfo *deviceInfo) {

	if(!deviceInfo)
		return;

	//See the header for why a ray pipeline without a ray query is NVIDIA's software traversal set.

	if(
		deviceInfo->vendor == EGraphicsVendorId_NV &&
		(deviceInfo->capabilities.flags.features & EGraphicsFeatures_RayPipeline) &&
		!(deviceInfo->capabilities.flags.features & EGraphicsFeatures_RayQuery)
	)
		deviceInfo->capabilities.flags.features2 |= EGraphicsFeatures2_RaySoftware;

	if(!(deviceInfo->capabilities.flags.features & EGraphicsFeatures_RayMicromapOpacity))
		return;

	//NVIDIA is the only vendor we have evidence of reporting OMM on hardware without the units (Ampere), so it
	// is the only one that gets a carve out.
	//RayReorderActual means the driver hints it ACTUALLY reorders rather than accepting the SER API as a
	// no-op, which on NVIDIA only holds from the generation that also introduced the OMM hardware.
	//See the header for why this is a heuristic and why it isn't a device ID table.
	//Every other vendor is taken at its word: a reported capability is assumed real until profiling proves it
	// is emulated, rather than being denied on suspicion.

	const Bool actual =
		deviceInfo->vendor != EGraphicsVendorId_NV ||
		!!(deviceInfo->capabilities.flags.features2 & EGraphicsFeatures2_RayReorderActual);

	if(actual)
		deviceInfo->capabilities.flags.features2 |= EGraphicsFeatures2_RayMicromapOpacityActual;
}

void GraphicsDeviceInfo_fixCapabilities(GraphicsDeviceInfo *info, EGraphicsApi api) {

	if(!info)
		return;

	GraphicsDeviceCapFlags *flags = &info->capabilities.flags;

	//Only remove dependent capabilities here. Deriving from masked bits could restore an explicitly disabled
	// capability or mistake a disabled hardware feature for a hardware limitation.

	if(api == EGraphicsApi_Vulkan) {

		if(!(flags->featuresExt & EVkGraphicsFeatures_BufferDeviceAddress)) {
			flags->features &= ~EGraphicsFeatures_Raytracing;
			flags->features2 &= ~EGraphicsFeatures2_DescriptorHeap;
		}

		if(!(flags->featuresExt & EVkGraphicsFeatures_MemoryPriority))
			flags->featuresExt &= ~EVkGraphicsFeatures_PageableMemory;

		if(!(flags->features & EGraphicsFeatures_SubgroupOperations))
			flags->featuresExt &= ~EVkGraphicsFeatures_SubgroupSizeControl;

		if(!(flags->featuresExt & EVkGraphicsFeatures_SubgroupSizeControl) && info->capabilities.waveSizeDefault)
			info->capabilities.waveSizeMin = info->capabilities.waveSizeMax = info->capabilities.waveSizeDefault;
	}

	else if(api == EGraphicsApi_Direct3D12) {

		//A shader model includes every earlier model's instructions, so removing one lowers the ceiling.

		if(!(flags->featuresExt & EDxGraphicsFeatures_SM6_6)) {
			flags->featuresExt &= ~(EDxGraphicsFeatures_SM6_7 | EDxGraphicsFeatures_WaveSize | EDxGraphicsFeatures_PAQ);
			flags->features &= ~(EGraphicsFeatures_ComputeDeriv | EGraphicsFeatures_MeshTaskTexDeriv);
			flags->features2 &= ~EGraphicsFeatures2_DescriptorHeap;
			flags->dataTypes &= ~EGraphicsDataTypes_AtomicI64;
		}

		if(!(flags->featuresExt & EDxGraphicsFeatures_SM6_7)) {
			flags->featuresExt &= ~EDxGraphicsFeatures_SM6_8;
			flags->features &= ~EGraphicsFeatures_WriteMSTexture;
		}

		if(!(flags->featuresExt & EDxGraphicsFeatures_SM6_8))
			flags->featuresExt &= ~(EDxGraphicsFeatures_SM6_9 | EDxGraphicsFeatures_WaveSizeMinMax);

		if(!(flags->featuresExt & EDxGraphicsFeatures_SM6_9)) {
			flags->featuresExt &= ~EDxGraphicsFeatures_SM6_10;
			flags->features &= ~EGraphicsFeatures_RayReorder;
		}

		if(!(flags->featuresExt & EDxGraphicsFeatures_SM6_10)) {
			flags->features &= ~(
				EGraphicsFeatures_CoopVec | EGraphicsFeatures_CoopMat | EGraphicsFeatures_CoopFP8 |
				EGraphicsFeatures_CoopVecTraining | EGraphicsFeatures_RayTriPosition
			);
		}

		if(!(flags->features & EGraphicsFeatures_SubgroupOperations))
			flags->featuresExt &= ~EDxGraphicsFeatures_WaveSize;

		if(!(flags->featuresExt & EDxGraphicsFeatures_WaveSize))
			flags->featuresExt &= ~EDxGraphicsFeatures_WaveSizeMinMax;
	}

	if(!(flags->features & (EGraphicsFeatures_RayQuery | EGraphicsFeatures_RayPipeline)))
		flags->features &= ~EGraphicsFeatures_Raytracing;

	if(!(flags->features & EGraphicsFeatures_Raytracing)) {
		flags->features &= ~EGraphicsFeatures_RaytracingFeats;
		flags->features2 &= ~EGraphicsFeatures2_RaytracingFeats;
	}

	if(!(flags->features & EGraphicsFeatures_RayPipeline))
		flags->features &= ~EGraphicsFeatures_RayReorder;

	if(!(flags->features & EGraphicsFeatures_RayReorder))
		flags->features2 &= ~EGraphicsFeatures2_RayReorderActual;

	if(!(flags->features & EGraphicsFeatures_RayMicromapOpacity))
		flags->features2 &= ~EGraphicsFeatures2_RayMicromapOpacityActual;

	if(!(flags->features & EGraphicsFeatures_MeshShader))
		flags->features &= ~EGraphicsFeatures_MeshTaskTexDeriv;

	if(!(flags->features & EGraphicsFeatures_SubgroupOperations)) {
		flags->features &= ~(
			EGraphicsFeatures_SubgroupArithmetic | EGraphicsFeatures_SubgroupShuffle | EGraphicsFeatures_SubgroupQuad
		);

		info->capabilities.waveSizeMin = info->capabilities.waveSizeMax = 0;
	}

	if(!(flags->features & EGraphicsFeatures_CoopVec))
		flags->features &= ~EGraphicsFeatures_CoopVecTraining;

	if(!(flags->features & (EGraphicsFeatures_CoopVec | EGraphicsFeatures_CoopMat)))
		flags->features &= ~EGraphicsFeatures_CoopFP8;

	if(!(flags->features & EGraphicsFeatures_Bindless))
		flags->features2 &= ~EGraphicsFeatures2_DescriptorHeap;

	if(!(flags->dataTypes & EGraphicsDataTypes_I64))
		flags->dataTypes &= ~EGraphicsDataTypes_AtomicI64;

	if(!(flags->dataTypes & EGraphicsDataTypes_F64))
		flags->dataTypes &= ~EGraphicsDataTypes_AtomicF64;

	if(!(flags->features2 & EGraphicsFeatures2_Timestamps))
		info->capabilities.timestampPeriod = 0;

	//Experimental capabilities are subsets of the enabled features, including after a dependency is removed.

	info->capabilities.experimentalFeatures &= flags->features;
	info->capabilities.experimentalFeatures2 &= flags->features2;
}

Bool GraphicsDeviceInfo_supportsFormat(const GraphicsDeviceInfo *deviceInfo, ETextureFormat format) {

	if(!deviceInfo)
		return false;

	const ETextureCompressionAlgo algo = ETextureFormat_getCompressionAlgo(format);

	if(algo == ETextureCompressionAlgo_ASTC)
		return deviceInfo->capabilities.flags.dataTypes & EGraphicsDataTypes_ASTC;

	if(algo == ETextureCompressionAlgo_BCn)
		return deviceInfo->capabilities.flags.dataTypes & EGraphicsDataTypes_BCn;

	switch (format) {
		case ETextureFormat_RGB32f:        return deviceInfo->capabilities.flags.dataTypes & EGraphicsDataTypes_RGB32f;
		case ETextureFormat_RGB32i:        return deviceInfo->capabilities.flags.dataTypes & EGraphicsDataTypes_RGB32i;
		case ETextureFormat_RGB32u:        return deviceInfo->capabilities.flags.dataTypes & EGraphicsDataTypes_RGB32u;
		default:                           return true;
	}
}

Bool GraphicsDeviceInfo_supportsFormatLinearFilter(const GraphicsDeviceInfo *deviceInfo, ETextureFormat format) {

	if(!deviceInfo)
		return false;

	//Integer formats can't be filtered at all, by the spec rather than by device.

	const ETexturePrimitive prim = ETextureFormat_getPrimitive(format);

	if(prim == ETexturePrimitive_UInt || prim == ETexturePrimitive_SInt)
		return false;

	switch (format) {

		//The two families that split by vendor (see EGraphicsDataTypes_LinearFilter*).

		case ETextureFormat_R16:        case ETextureFormat_R16s:
		case ETextureFormat_RG16:       case ETextureFormat_RG16s:
		case ETextureFormat_RGBA16:     case ETextureFormat_RGBA16s:
			return deviceInfo->capabilities.flags.dataTypes & EGraphicsDataTypes_LinearFilter16Norm;

		case ETextureFormat_R32f:       case ETextureFormat_RG32f:      case ETextureFormat_RGBA32f:
			return deviceInfo->capabilities.flags.dataTypes & EGraphicsDataTypes_LinearFilter32f;

		//Shared exponent is its own hardware path and measures at 100%, so it does NOT inherit the 32 bit
		// float family's gap even though its primitive says Float.

		case ETextureFormat_RGB9E5:
			return true;

		//Everything else that OxC3 requires at all is filterable wherever it is sampleable.

		default:
			return GraphicsDeviceInfo_supportsFormat(deviceInfo, format);
	}
}

Bool GraphicsDeviceInfo_supportsRenderTextureFormat(const GraphicsDeviceInfo *deviceInfo, ETextureFormat format) {

	//Sampling RGB9E5 is required, writing it is not; a render texture is the writing half.

	if(format == ETextureFormat_RGB9E5)
		return deviceInfo && (deviceInfo->capabilities.flags.dataTypes & EGraphicsDataTypes_WriteRGB9E5);

	return !ETextureFormat_getIsCompressed(format) && GraphicsDeviceInfo_supportsFormat(deviceInfo, format);
}

Bool GraphicsDeviceInfo_supportsFormatVertexAttribute(ETextureFormat format) {

	const ETextureCompressionAlgo algo = ETextureFormat_getCompressionAlgo(format);

	if(algo != ETextureCompressionAlgo_None)
		return false;

	return true;
}

Bool GraphicsDeviceInfo_supportsDepthStencilFormat(const GraphicsDeviceInfo *deviceInfo, EDepthStencilFormat format) {

	if(!deviceInfo)
		return false;

	switch(format) {
		case EDepthStencilFormat_D24S8Ext:      return deviceInfo->capabilities.flags.dataTypes & EGraphicsDataTypes_D24S8;
		case EDepthStencilFormat_D32S8X24Ext:   return deviceInfo->capabilities.flags.dataTypes & EGraphicsDataTypes_D32S8;
		case EDepthStencilFormat_S8X24Ext:      return deviceInfo->capabilities.flags.dataTypes & EGraphicsDataTypes_S8;
		default:                                return true;
	}
}
