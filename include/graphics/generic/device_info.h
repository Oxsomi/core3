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

#pragma once
#include "graphics/generic/types.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef enum ETextureFormat ETextureFormat;
typedef enum EDepthStencilFormat EDepthStencilFormat;

typedef enum EGraphicsDeviceType {
	EGraphicsDeviceType_Dedicated,
	EGraphicsDeviceType_Integrated,
	EGraphicsDeviceType_Simulated,
	EGraphicsDeviceType_CPU,
	EGraphicsDeviceType_Other
} EGraphicsDeviceType;

typedef enum EGraphicsVendorId {
	EGraphicsVendorId_NV,
	EGraphicsVendorId_AMD,
	EGraphicsVendorId_ARM,
	EGraphicsVendorId_QCOM,
	EGraphicsVendorId_INTC,
	EGraphicsVendorId_IMGT,
	EGraphicsVendorId_MSFT,
	EGraphicsVendorId_APPL,
	EGraphicsVendorId_SMSG,
	EGraphicsVendorId_HWEI,
	EGraphicsVendorId_Unknown
} EGraphicsVendorId;

typedef enum EGraphicsVendorPCIE {
	EGraphicsVendorPCIE_NV		= 0x10DE,
	EGraphicsVendorPCIE_AMD		= 0x1002,
	EGraphicsVendorPCIE_ARM		= 0x13B5,
	EGraphicsVendorPCIE_QCOM	= 0x5143,
	EGraphicsVendorPCIE_INTC	= 0x8086,
	EGraphicsVendorPCIE_IMGT	= 0x1010,
	EGraphicsVendorPCIE_MSFT	= 0x1414,
	EGraphicsVendorPCIE_APPL	= 0x106B,
	EGraphicsVendorPCIE_SMSG	= 0x144D,
	EGraphicsVendorPCIE_HWEI		= 0x19E5
} EGraphicsVendorPCIE;

static const U16 EGraphicsVendor_PCIE[] = {		//The PCIE ids of the vendors, so they can be easily detected
	EGraphicsVendorPCIE_NV,
	EGraphicsVendorPCIE_AMD,
	EGraphicsVendorPCIE_ARM,
	EGraphicsVendorPCIE_QCOM,
	EGraphicsVendorPCIE_INTC,
	EGraphicsVendorPCIE_IMGT,
	EGraphicsVendorPCIE_MSFT,
	EGraphicsVendorPCIE_APPL,
	EGraphicsVendorPCIE_SMSG,
	EGraphicsVendorPCIE_HWEI
};

//If api type is Direct3D12

typedef enum EDxGraphicsFeatures {

	EDxGraphicsFeatures_None					= 0,

	EDxGraphicsFeatures_WriteBufferImmediate	= 1 << 0,
	EDxGraphicsFeatures_ReBAR					= 1 << 1,
	EDxGraphicsFeatures_HardwareCopyQueue		= 1 << 2,
	EDxGraphicsFeatures_WaveSize				= 1 << 3,
	EDxGraphicsFeatures_WaveSizeMinMax			= 1 << 4,
	EDxGraphicsFeatures_PAQ						= 1 << 5,
	EDxGraphicsFeatures_ReportReBARWrites		= 1 << 6,		//A tool is attached and requires marking updates to ReBAR

	EDxGraphicsFeatures_TightAlignment			= 1 << 7,

	EDxGraphicsFeatures_ReallyReportReBARWrites = EDxGraphicsFeatures_ReportReBARWrites | EDxGraphicsFeatures_ReBAR,

	EDxGraphicsFeatures_SM6_6					= 1 << 16,		//Last bits are for shader model
	EDxGraphicsFeatures_SM6_7					= 1 << 17,
	EDxGraphicsFeatures_SM6_8					= 1 << 18,
	EDxGraphicsFeatures_SM6_9					= 1 << 19

} EDxGraphicsFeatures;

//If api type is Vulkan

typedef enum EVkGraphicsFeatures {
	EVkGraphicsFeatures_PerfQuery				= 1 << 0,
	EVkGraphicsFeatures_Maintenance4			= 1 << 1,
	EVkGraphicsFeatures_BufferDeviceAddress		= 1 << 2,
	EVkGraphicsFeatures_DriverProperties		= 1 << 3,
	EVkGraphicsFeatures_MemoryBudget			= 1 << 4
} EVkGraphicsFeatures;

//Generic graphics features

typedef enum EGraphicsFeatures {

	EGraphicsFeatures_None						= 0,

	//When this is turned on, the device doesn't benefit from tiled rendering.
	//This is false for mobile devices only or some chips such as QCOM on windows.
	//On desktop and various dedicated GPUs this is always true.
	//If this is false, you have to use render passes.

	EGraphicsFeatures_DirectRendering			= 1 << 0,

	EGraphicsFeatures_VariableRateShading		= 1 << 1,

	EGraphicsFeatures_MultiDrawIndirectCount	= 1 << 2,

	EGraphicsFeatures_MeshShader				= 1 << 3,		//Mesh and task shaders
	EGraphicsFeatures_GeometryShader			= 1 << 4,

	EGraphicsFeatures_SubgroupArithmetic		= 1 << 5,		//Non prefix arithmetic operations
	EGraphicsFeatures_SubgroupShuffle			= 1 << 6,

	EGraphicsFeatures_Multiview					= 1 << 7,

	//Raytracing extensions

	EGraphicsFeatures_Raytracing				= 1 << 8,		//Requires RayPipeline or RayQuery
	EGraphicsFeatures_RayPipeline				= 1 << 9,
	EGraphicsFeatures_RayQuery					= 1 << 10,
	EGraphicsFeatures_RayMicromapOpacity		= 1 << 11,
	EGraphicsFeatures_RayMicromapDisplacement	= 1 << 12,
	EGraphicsFeatures_RayMotionBlur				= 1 << 13,
	EGraphicsFeatures_RayReorder				= 1 << 14,
	EGraphicsFeatures_RayValidation				= 1 << 15,		//Debugging for raytracing validation

	//LUID for sharing devices

	EGraphicsFeatures_LUID						= 1 << 16,

	//Other features

	EGraphicsFeatures_Wireframe					= 1 << 17,
	EGraphicsFeatures_LogicOp					= 1 << 18,
	EGraphicsFeatures_DualSrcBlend				= 1 << 19,

	EGraphicsFeatures_Workgraphs				= 1 << 20,
	EGraphicsFeatures_SwapchainCompute			= 1 << 21,		//isComputeExt in createSwapchain is supported

	EGraphicsFeatures_ComputeDeriv				= 1 << 22,		//Compute derivatives (ddx/ddy)
	EGraphicsFeatures_MeshTaskTexDeriv			= 1 << 23,		//Compute derivatives in mesh/task shaders

	EGraphicsFeatures_WriteMSTexture			= 1 << 24,		//image2DMS or RWTexture2DMS
	EGraphicsFeatures_Bindless					= 1 << 25,

	EGraphicsFeatures_SubgroupOperations		= 1 << 26

} EGraphicsFeatures;

typedef enum EGraphicsFeatures2 {
	EGraphicsFeatures2_None
} EGraphicsFeatures2;

typedef enum EGraphicsDataTypes {

	EGraphicsDataTypes_None						= 0,

	//What operations are available on native data types

	EGraphicsDataTypes_F64						= 1 << 0,
	EGraphicsDataTypes_I64						= 1 << 1,
	EGraphicsDataTypes_F16						= 1 << 2,
	EGraphicsDataTypes_I16						= 1 << 3,

	EGraphicsDataTypes_AtomicI64				= 1 << 4,
	EGraphicsDataTypes_AtomicF32				= 1 << 5,
	EGraphicsDataTypes_AtomicF64				= 1 << 6,

	//What texture formats are available
	//These can be both supported.

	EGraphicsDataTypes_ASTC						= 1 << 7,			//If false, BCn has to be supported
	EGraphicsDataTypes_BCn						= 1 << 8,			//If false, ASTC has to be supported

	//If render targets can have MSAA8x or 2x.

	EGraphicsDataTypes_MSAA2x					= 1 << 9,
	EGraphicsDataTypes_MSAA8x					= 1 << 10,

	//Formats for use other than just vertex buffer usage

	EGraphicsDataTypes_RGB32f					= 1 << 11,
	EGraphicsDataTypes_RGB32i					= 1 << 12,
	EGraphicsDataTypes_RGB32u					= 1 << 13,

	//Depth stencil

	EGraphicsDataTypes_D24S8					= 1 << 14,
	EGraphicsDataTypes_S8						= 1 << 15,

	EGraphicsDataTypes_D32S8					= 1 << 16

} EGraphicsDataTypes;

//This struct represents the abilities a graphics device has.

typedef struct GraphicsDeviceCapabilities {

	EGraphicsFeatures features;
	EGraphicsFeatures2 features2;

	EGraphicsDataTypes dataTypes;
	U32 featuresExt;				//Extended device features, API dependent

	U64 dedicatedMemory;			//Memory accessible directly to the device

	U64 sharedMemory;				//Memory accessible through the CPU (can be equal to dedicatedMemory if iGPU or CPU)

	U64 maxBufferSize;
	U64 maxAllocationSize;

} GraphicsDeviceCapabilities;

//The device info struct represents a physical device.

typedef struct GraphicsDeviceInfo {

	C8 name[256];
	C8 driverInfo[256];		//Can be empty if unsupported

	EGraphicsDeviceType type;
	EGraphicsVendorId vendor;

	U64 id;

	GraphicsDeviceCapabilities capabilities;

	U64 luid;				//Check SupportsLUID

	U64 uuid[2];			//If UUIDs aren't supported, uuid[0] will be luid and uuid[1] will be 0

	void *ext;

} GraphicsDeviceInfo;

typedef enum EGraphicsApi EGraphicsApi;

void GraphicsDeviceInfo_print(EGraphicsApi api, const GraphicsDeviceInfo *deviceInfo, Bool printCapabilities);

//If a texture and render texture can be created with the format.
Bool GraphicsDeviceInfo_supportsFormat(const GraphicsDeviceInfo *deviceInfo, ETextureFormat format);

//If a render texture can be created with the format.
Bool GraphicsDeviceInfo_supportsRenderTextureFormat(const GraphicsDeviceInfo *deviceInfo, ETextureFormat format);

//If a texture format is allowed as a vertex attribute
Bool GraphicsDeviceInfo_supportsFormatVertexAttribute(ETextureFormat format);

Bool GraphicsDeviceInfo_supportsDepthStencilFormat(const GraphicsDeviceInfo *deviceInfo, EDepthStencilFormat format);

#ifdef __cplusplus
	}
#endif
