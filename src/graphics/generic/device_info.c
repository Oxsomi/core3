/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "graphics/generic/device_info.h"
#include "graphics/generic/pipeline_structs.h"
#include "platforms/log.h"
#include "formats/texture.h"
#include "graphics/generic/instance.h"
#include "types/type_cast.h"

void GraphicsDeviceInfo_print(EGraphicsApi api, const GraphicsDeviceInfo *deviceInfo, Bool printCapabilities) {

	if(!deviceInfo || !deviceInfo->name[0])
		return;

	Log_debugLnx(
		"%s: %s (%s): %"PRIu64" bytes shared memory, %"PRIu64" bytes %s memory\n\t"
		"%s %"PRIu32"\n\tLUID %016"PRIx64"\n\tUUID %016"PRIx64"%016"PRIx64,
		api == EGraphicsApi_DirectX12 ? "D3D12" : (api == EGraphicsApi_Vulkan ? "Vulkan" : "Unknown"),
		deviceInfo->name,
		deviceInfo->driverInfo,
		deviceInfo->capabilities.sharedMemory,
		deviceInfo->capabilities.dedicatedMemory,
		deviceInfo->type != EGraphicsDeviceType_Dedicated ? "total" : "dedicated",
		(deviceInfo->type == EGraphicsDeviceType_CPU ? "CPU" : (
			deviceInfo->type == EGraphicsDeviceType_Dedicated ? "dGPU" : (
				deviceInfo->type == EGraphicsDeviceType_Integrated ? "iGPU" : "Simulated GPU"
			)
		)),
		deviceInfo->id,
		deviceInfo->capabilities.features & EGraphicsFeatures_LUID ? U64_swapEndianness(deviceInfo->luid) : 0,
		U64_swapEndianness(deviceInfo->uuid[0]),
		U64_swapEndianness(deviceInfo->uuid[1])
	);

	if (printCapabilities) {

		const GraphicsDeviceCapabilities cap = deviceInfo->capabilities;

		//Features

		const U32 feat = cap.features;

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

		if(feat & EGraphicsFeatures_Multiview)
			Log_debugLnx("\t\tMultiview");

		if(feat & EGraphicsFeatures_Raytracing)
			Log_debugLnx("\t\tRaytracing");

		if(feat & EGraphicsFeatures_RayPipeline)
			Log_debugLnx("\t\tRaytracing pipeline");

		if(feat & EGraphicsFeatures_RayQuery)
			Log_debugLnx("\t\tRay query");

		if(feat & EGraphicsFeatures_RayMicromapOpacity)
			Log_debugLnx("\t\tRaytracing opacity micromap");

		if(feat & EGraphicsFeatures_RayMicromapDisplacement)
			Log_debugLnx("\t\tRaytracing displacement micromap");

		if(feat & EGraphicsFeatures_RayMotionBlur)
			Log_debugLnx("\t\tRaytracing motion blur");

		if(feat & EGraphicsFeatures_RayReorder)
			Log_debugLnx("\t\tRay reorder");

		if(feat & EGraphicsFeatures_RayValidation)
			Log_debugLnx("\t\tRay validation");

		if(feat & EGraphicsFeatures_DebugMarkers)
			Log_debugLnx("\t\tDebug markers");

		if(feat & EGraphicsFeatures_Wireframe)
			Log_debugLnx("\t\tWireframe (rasterizer fill mode: line)");

		if(feat & EGraphicsFeatures_LogicOp)
			Log_debugLnx("\t\tLogic op (blend state)");

		if(feat & EGraphicsFeatures_DualSrcBlend)
			Log_debugLnx("\t\tDual src blend (blend state)");

		if(feat & EGraphicsFeatures_Workgraphs)
			Log_debugLnx("\t\tWork graphs");

		if(feat & EGraphicsFeatures_SwapchainCompute)
			Log_debugLnx("\t\tSwapchain compute");

		//Data types

		const U32 dat = cap.dataTypes;

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

		if(dat & EGraphicsDataTypes_RGB32f)
			Log_debugLnx("\t\tETextureFormat_RGBA32f for use in textures (not just vertex input)");

		if(dat & EGraphicsDataTypes_RGB32u)
			Log_debugLnx("\t\tETextureFormat_RGBA32u for use in textures (not just vertex input)");

		if(dat & EGraphicsDataTypes_RGB32i)
			Log_debugLnx("\t\tETextureFormat_RGBA32i for use in textures (not just vertex input)");

		//API specific features

		if(api == EGraphicsApi_DirectX12) {

			if(cap.featuresExt & EDxGraphicsFeatures_WriteBufferImmediate)
				Log_debugLnx("\t\tD3D12 WriteBufferImmediate");

			if(cap.featuresExt & EDxGraphicsFeatures_ReBAR)
				Log_debugLnx("\t\tD3D12 ReBAR");

			if(cap.featuresExt & EDxGraphicsFeatures_HardwareCopyQueue)
				Log_debugLnx("\t\tD3D12 Hardware copy queue");
		}

		else if(api == EGraphicsApi_Vulkan) {

			if(cap.featuresExt & EVkGraphicsFeatures_PerfQuery)
				Log_debugLnx("\t\tVulkan performance query");
		}
	}
}

Bool GraphicsDeviceInfo_supportsFormat(const GraphicsDeviceInfo *deviceInfo, ETextureFormat format) {

	if(!deviceInfo)
		return false;

	const ETextureCompressionAlgo algo = ETextureFormat_getCompressionAlgo(format);

	if(algo == ETextureCompressionAlgo_ASTC)
		return deviceInfo->capabilities.dataTypes & EGraphicsDataTypes_ASTC;

	if(algo == ETextureCompressionAlgo_BCn)
		return deviceInfo->capabilities.dataTypes & EGraphicsDataTypes_BCn;

	switch (format) {
		case ETextureFormat_RGB32f:		return deviceInfo->capabilities.dataTypes & EGraphicsDataTypes_RGB32f;
		case ETextureFormat_RGB32i:		return deviceInfo->capabilities.dataTypes & EGraphicsDataTypes_RGB32i;
		case ETextureFormat_RGB32u:		return deviceInfo->capabilities.dataTypes & EGraphicsDataTypes_RGB32u;
		default:						return true;
	}
}

Bool GraphicsDeviceInfo_supportsRenderTextureFormat(const GraphicsDeviceInfo *deviceInfo, ETextureFormat format) {
	return !ETextureFormat_getIsCompressed(format) && GraphicsDeviceInfo_supportsFormat(deviceInfo, format);
}

Bool GraphicsDeviceInfo_supportsFormatVertexAttribute(ETextureFormat format) {

	const ETextureCompressionAlgo algo = ETextureFormat_getCompressionAlgo(format);

	if(algo != ETextureCompressionAlgo_None)
		return false;

	return true;
}

Bool GraphicsDeviceInfo_supportsDepthStencilFormat(const GraphicsDeviceInfo *deviceInfo, EDepthStencilFormat format) {
	switch(format) {
		case EDepthStencilFormat_D24S8Ext:	return deviceInfo->capabilities.dataTypes & EGraphicsDataTypes_D24S8;
		case EDepthStencilFormat_S8Ext:		return deviceInfo->capabilities.dataTypes & EGraphicsDataTypes_S8;
		default:							return true;
	}
}