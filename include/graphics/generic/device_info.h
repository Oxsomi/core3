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

//graphics/generic/device_info.h

#pragma once
#include "graphics/generic/graphics_types.h"

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
	EGraphicsVendorId_GOGL,
	EGraphicsVendorId_MESA,
	EGraphicsVendorId_Unknown
} EGraphicsVendorId;

typedef enum EGraphicsVendorPCIE {
	EGraphicsVendorPCIE_NV      = 0x10DE,
	EGraphicsVendorPCIE_AMD     = 0x1002,
	EGraphicsVendorPCIE_ARM     = 0x13B5,
	EGraphicsVendorPCIE_QCOM    = 0x5143,
	EGraphicsVendorPCIE_QCOM2   = 0x4D4F4351,
	EGraphicsVendorPCIE_INTC    = 0x8086,
	EGraphicsVendorPCIE_IMGT    = 0x1010,
	EGraphicsVendorPCIE_MSFT    = 0x1414,
	EGraphicsVendorPCIE_APPL    = 0x106B,
	EGraphicsVendorPCIE_SMSG    = 0x144D,
	EGraphicsVendorPCIE_HWEI    = 0x19E5,
	EGraphicsVendorPCIE_GOGL    = 0x1AE0,
	EGraphicsVendorPCIE_MESA    = 0x10005
} EGraphicsVendorPCIE;

//The vendor ids, in EGraphicsVendorId order, so a reported id can be turned into one by index.
//U32 rather than U16 because not every id is a PCI id: Khronos hands out its own above 0x10000 for vendors
// with no PCI presence, and Mesa's 0x10005 truncates to 5 in 16 bits, which clang catches as an error.
//QCOM2 is deliberately absent, being a second id for a vendor already listed rather than another vendor.

static const U32 EGraphicsVendor_PCIE[] = {
	EGraphicsVendorPCIE_NV,
	EGraphicsVendorPCIE_AMD,
	EGraphicsVendorPCIE_ARM,
	EGraphicsVendorPCIE_QCOM,
	EGraphicsVendorPCIE_INTC,
	EGraphicsVendorPCIE_IMGT,
	EGraphicsVendorPCIE_MSFT,
	EGraphicsVendorPCIE_APPL,
	EGraphicsVendorPCIE_SMSG,
	EGraphicsVendorPCIE_HWEI,
	EGraphicsVendorPCIE_GOGL,
	EGraphicsVendorPCIE_MESA
};

//If api type is Direct3D12

typedef enum EDxGraphicsFeatures {

	EDxGraphicsFeatures_None                    = 0,

	EDxGraphicsFeatures_WriteBufferImmediate    = 1 << 0,
	EDxGraphicsFeatures_ReBAR                   = 1 << 1,
	EDxGraphicsFeatures_HardwareCopyQueue       = 1 << 2,
	EDxGraphicsFeatures_WaveSize                = 1 << 3,
	EDxGraphicsFeatures_WaveSizeMinMax          = 1 << 4,
	EDxGraphicsFeatures_PAQ                     = 1 << 5,
	EDxGraphicsFeatures_ReportReBARWrites       = 1 << 6,        //A tool is attached and requires marking updates to ReBAR

	EDxGraphicsFeatures_TightAlignment          = 1 << 7,
	EDxGraphicsFeatures_AllowCombineHeaps       = 1 << 8,        //Some devices don't (Arc alchemist, <= Nv Pascal)

	EDxGraphicsFeatures_RGBX32fMSAA             = 1 << 9,        //3 and 4 element float MSAA

	EDxGraphicsFeatures_IndependentDevices      = 1 << 10,

	EDxGraphicsFeatures_BatchedAsyncCommandList = 1 << 11,        //ExecuteCommandLists batching across async queues

	EDxGraphicsFeatures_CacheCoherentUMA        = 1 << 12,        //UMA that snoops CPU caches, upload wants WRITE_BACK

	EDxGraphicsFeatures_ReallyReportReBARWrites = EDxGraphicsFeatures_ReportReBARWrites | EDxGraphicsFeatures_ReBAR,

	EDxGraphicsFeatures_SM6_6                   = 1 << 16,        //Last bits are for shader model
	EDxGraphicsFeatures_SM6_7                   = 1 << 17,
	EDxGraphicsFeatures_SM6_8                   = 1 << 18,
	EDxGraphicsFeatures_SM6_9                   = 1 << 19,
	EDxGraphicsFeatures_SM6_10                  = 1 << 20

} EDxGraphicsFeatures;

//If api type is Vulkan

typedef enum EVkGraphicsFeatures {

	EVkGraphicsFeatures_PerfQuery                = 1 << 0,
	EVkGraphicsFeatures_Maintenance4             = 1 << 1,
	EVkGraphicsFeatures_BufferDeviceAddress      = 1 << 2,
	EVkGraphicsFeatures_DriverProperties         = 1 << 3,
	EVkGraphicsFeatures_MemoryBudget             = 1 << 4,

	//VK_KHR_push_descriptor with at least 32 push descriptors, so the globals constant buffer can be pushed
	// straight into the command buffer instead of being bound from a set allocated to hold it.
	//Named for the performance, not the capability: pushing descriptors always works, this only says the device
	// does it natively rather than through the emulation below.
	//Vulkan only, because it's the only api where this isn't a given; D3D12 has root descriptors and Metal has
	// argument buffers, so there's nothing for a caller to branch on outside of Vulkan.
	//When it's absent OxC3 uses one descriptor set per frame in flight instead,
	// which trades a push per frame for an allocation and a write at first submit.
	//Android emulators are the common case, since gfxstream drops the extension from the guest even when the
	// host driver exposes it.

	EVkGraphicsFeatures_PerformantPushDescriptor = 1 << 5,

	//VK_KHR_opacity_micromap (plus its VK_KHR_device_address_commands dependency) is what got enabled rather
	// than VK_EXT_opacity_micromap.
	//The EXT extension is promoted to KHR rather than deprecated, and current drivers commonly still expose
	// only EXT, so the backend prefers KHR and falls back; this bit records which one the device runs.

	EVkGraphicsFeatures_OpacityMicromapKHR       = 1 << 6

} EVkGraphicsFeatures;

//Generic graphics features

typedef enum EGraphicsFeatures {

	EGraphicsFeatures_None                      = 0,

	//When this is turned on, the device doesn't benefit from tiled rendering.
	//This is false for mobile devices only or some chips such as QCOM on windows.
	//On desktop and various dedicated GPUs this is always true.
	//If this is false, you have to use render passes.

	EGraphicsFeatures_DirectRendering           = 1 << 0,

	EGraphicsFeatures_VariableRateShading       = 1 << 1,

	EGraphicsFeatures_MultiDrawIndirectCount    = 1 << 2,

	EGraphicsFeatures_MeshShader                = 1 << 3,          //Mesh and task shaders
	EGraphicsFeatures_GeometryShader            = 1 << 4,

	EGraphicsFeatures_SubgroupArithmetic        = 1 << 5,          //Non prefix arithmetic operations
	EGraphicsFeatures_SubgroupShuffle           = 1 << 6,

	EGraphicsFeatures_Multiview                 = 1 << 7,

	//Raytracing extensions

	EGraphicsFeatures_Raytracing                = 1 << 8,          //Requires RayPipeline or RayQuery
	EGraphicsFeatures_RayPipeline               = 1 << 9,
	EGraphicsFeatures_RayQuery                  = 1 << 10,
	EGraphicsFeatures_RayMicromapOpacity        = 1 << 11,
	EGraphicsFeatures_RayTriPosition            = 1 << 12,         //SM6.10 tri vertex position fetch (RayQuery + ray-pipeline)
	EGraphicsFeatures_RayReorder                = 1 << 13,
	EGraphicsFeatures_RayValidation             = 1 << 14,         //Debugging for raytracing validation

	//LUID for sharing devices

	EGraphicsFeatures_LUID                      = 1 << 15,

	//Other features

	EGraphicsFeatures_Wireframe                 = 1 << 16,
	EGraphicsFeatures_LogicOp                   = 1 << 17,
	EGraphicsFeatures_DualSrcBlend              = 1 << 18,

	//SV_Barycentrics in fragment shaders (SM6.1 / VK_KHR_fragment_shader_barycentric)
	EGraphicsFeatures_Barycentrics              = 1 << 19,
	EGraphicsFeatures_SwapchainCompute          = 1 << 20,        //isComputeExt in createSwapchain is supported

	EGraphicsFeatures_ComputeDeriv              = 1 << 21,        //Compute derivatives (ddx/ddy)
	EGraphicsFeatures_MeshTaskTexDeriv          = 1 << 22,        //Compute derivatives in mesh/task shaders

	EGraphicsFeatures_WriteMSTexture            = 1 << 23,        //image2DMS or RWTexture2DMS
	EGraphicsFeatures_Bindless                  = 1 << 24,

	EGraphicsFeatures_SubgroupOperations        = 1 << 25,

	//(bit 26 is EGraphicsFeatures_RayTriPosition, grouped with the raytracing features above)

	//SM6.10 linalg, split to mirror the oiSH extensions (CoopVec/CoopMat/CoopFP8).
	//FP16 + INT8 are the base tier of CoopVec/CoopMat; CoopFP8 is the additive FP8 tier.
	//CoopVecTraining exposes the Tier-1.1 outer-product/reduce-sum ops.

	EGraphicsFeatures_CoopVec                   = 1 << 26,        //Cooperative vectors (per-thread matvec)
	EGraphicsFeatures_CoopMat                   = 1 << 27,        //Cooperative matrix (subgroup GEMM)
	EGraphicsFeatures_CoopFP8                   = 1 << 28,        //FP8 (e4m3/e5m2) cooperative type support
	EGraphicsFeatures_CoopVecTraining           = 1 << 29,        //CoopVec training (outer-product / reduce-sum accumulate)
	//Quad subgroup ops (QuadReadAcrossX/Y/Diagonal, QuadReadLaneAt).
	//Reported separately from SubgroupShuffle by Vulkan, and separate hardware from ComputeDeriv:
	// quad ops in a COMPUTE shader need both this and ComputeDeriv, in a pixel shader only this.
	EGraphicsFeatures_SubgroupQuad              = 1 << 30,

	//Spelled as a subtraction rather than as -2147483648, which is a unary minus on a constant too large for
	// int and so an unsigned one; msvc rejects that under C4146 while clang stays quiet.

	EGraphicsFeatures_Reserved                  = -2147483647 - 1  //Reserved for safety enum reasons

} EGraphicsFeatures;

typedef enum EGraphicsFeatures2 {

	EGraphicsFeatures2_None                     = 0,

	//SER (shader execution reordering) is exposed as two separate capabilities.
	//EGraphicsFeatures_RayReorder means the API (dx::HitObject / ReorderThread) is available.
	//It's always valid to call, but may be a no-op on some hardware.
	//This bit means the device actually PERFORMS the reordering, so it's worth restructuring shaders around it.
	//D3D12: OPTIONS22.ShaderExecutionReorderingActuallyReorders; Vulkan: rayTracingInvocationReorderReorderingHint.

	EGraphicsFeatures2_RayReorderActual         = 1 << 0,

	//Full bindless: shaders index the descriptor heap directly, without a fixed descriptor layout.
	//D3D12: SM6.6 dynamic resources (ResourceDescriptorHeap/SamplerDescriptorHeap) + resource binding tier 3.
	//Vulkan: VK_EXT_descriptor_heap (also requires EGraphicsFeatures_Bindless for parity with D3D12).

	EGraphicsFeatures2_DescriptorHeap           = 1 << 1,

	//Mega geometry (RTXMG); Vulkan splits it into two extensions, so OxC3 exposes two bits.
	//RayClusterAS: cluster acceleration structures (CLAS/cluster BLAS).
	//D3D12: NVAPI cluster operations caps; Vulkan: VK_NV_cluster_acceleration_structure.
	//RayPartitionedTLAS: partitioned top level acceleration structures (PTLAS).
	//D3D12: NVAPI partitioned TLAS caps; Vulkan: VK_NV_partitioned_acceleration_structure.

	EGraphicsFeatures2_RayClusterAS             = 1 << 2,
	EGraphicsFeatures2_RayPartitionedTLAS       = 1 << 3,

	//GPU-driven acceleration structure builds.
	//Vulkan: vkCmdBuildAccelerationStructuresIndirectKHR (classic AS builds).
	//D3D12: set when mega geometry is, since those builds are indirect by design
	// (BUILD_BLAS_FROM_CLAS cluster op / NvAPI_D3D12_BuildRaytracingPartitionedTlasIndirect).

	EGraphicsFeatures2_RayIndirectASBuild       = 1 << 4,

	//Whether opacity micromaps (EGraphicsFeatures_RayMicromapOpacity) are likely backed by dedicated hardware
	// rather than emulated, in the same shape as RayReorderActual above.
	//Neither API exposes this: D3D12 ships OMM wholesale with RAYTRACING_TIER_1_2, Vulkan's
	// VkPhysicalDeviceOpacityMicromapFeaturesEXT is a single bool, and the subdivision level properties are no
	// help either (an Ampere 3080 reports the spec maximum of 12/12, same as hardware that has the units).
	//So this bit is a HEURISTIC rather than a query, which is why it is derived once per device instead of
	// being reported by a backend.
	//On NVIDIA the SER reordering hardware and the OMM engines arrived in the same generation, so the SER
	// reordering hint doubles as "this generation or newer".
	//Deliberately not a device ID table: an unknown future GPU that reports reordering is treated as capable
	// instead of falling off the end of a lookup, and this works on every OS unlike NVAPI.
	//Other vendors are taken at their word, so NVIDIA is the only carve out and only because Ampere is known
	// to report OMM without the units.
	//Use it to decide whether a REAL micromap is worth building; special index only OMM costs nothing either
	// way, so on a device without this bit prefer special indices over a micromap object.

	EGraphicsFeatures2_RayMicromapOpacityActual = 1 << 5,

	//8-bit (R8u) OMM index buffers are legal on this device.
	//D3D12 ships this with opacity micromaps themselves. On Vulkan only VK_KHR_opacity_micromap permits
	// VK_INDEX_TYPE_UINT8 (VUID 11570; the EXT extension forbids it, VUID 10719), but the KHR path isn't
	// implemented yet, so no Vulkan device claims this bit today and R8u is rejected at BLAS create there.
	EGraphicsFeatures2_RayMicromapOpacityU8     = 1 << 6,

	//GPU timestamp queries: the device can write pipeline timestamps and reports a period to turn ticks into
	// nanoseconds. Vulkan gates this on timestampComputeAndGraphics with a non zero timestampValidBits on the
	// submit queue; D3D12 has it on the graphics and compute queues at root signature level.

	EGraphicsFeatures2_Timestamps               = 1 << 7,

	//Scopes can carry a PREDICATE: a U64 in a device buffer, read when the scope executes; zero skips the
	// scope's draws and dispatches while its barriers still run. D3D12 has this in core (SetPredication);
	// Vulkan gates it on VK_EXT_conditional_rendering, which reads the low 32 bits, so writers fill the
	// full 64 bits. Without the capability a predicated scope simply runs, which is the correct
	// degradation for the skip-empty-work uses the feature exists for.

	EGraphicsFeatures2_Predication              = 1 << 8,

	//Pipeline executable introspection: the driver can hand back per-pipeline ISA disassembly + VGPR/SGPR statistics.
	//Vulkan: VK_KHR_pipeline_executable_properties (pipelineExecutableInfo).
	//Used for live shader disassembly, not rendering; device+driver dependent so it isn't golden-pinnable.

	EGraphicsFeatures2_PipelineExecutableInfo   = 1 << 9

} EGraphicsFeatures2;

typedef enum EGraphicsDataTypes {

	EGraphicsDataTypes_None                     = 0,

	//What operations are available on native data types

	EGraphicsDataTypes_F64                      = 1 << 0,
	EGraphicsDataTypes_I64                      = 1 << 1,
	EGraphicsDataTypes_F16                      = 1 << 2,
	EGraphicsDataTypes_I16                      = 1 << 3,

	EGraphicsDataTypes_AtomicI64                = 1 << 4,
	EGraphicsDataTypes_AtomicF32                = 1 << 5,
	EGraphicsDataTypes_AtomicF64                = 1 << 6,

	//What texture formats are available
	//These can be both supported.

	EGraphicsDataTypes_ASTC                     = 1 << 7,            //If false, BCn has to be supported
	EGraphicsDataTypes_BCn                      = 1 << 8,            //If false, ASTC has to be supported

	//If render targets can have MSAA8x or 2x.

	EGraphicsDataTypes_MSAA2x                   = 1 << 9,
	EGraphicsDataTypes_MSAA8x                   = 1 << 10,

	//Formats for use other than just vertex buffer usage

	EGraphicsDataTypes_RGB32f                   = 1 << 11,
	EGraphicsDataTypes_RGB32i                   = 1 << 12,
	EGraphicsDataTypes_RGB32u                   = 1 << 13,

	//Depth stencil

	EGraphicsDataTypes_D24S8                    = 1 << 14,
	EGraphicsDataTypes_S8                       = 1 << 15,

	EGraphicsDataTypes_D32S8                    = 1 << 16,

	//Linear filtering is a SEPARATE capability from being able to sample a format at all, and Vulkan only
	// mandates it for a subset. Every format below can be sampled everywhere OxC3 runs; these bits say
	// whether a linear sampler over one does anything, since a device without it either fails validation
	// or silently point samples.
	//
	//Two bits and not one: the hardware that lacks each is almost disjoint. Measured over the devices
	// reporting Vulkan 1.1+ within a year, 42 lack the 16 bit norm filter and 40 lack the 32 bit float one,
	// and only 9 lack both, because it splits by vendor. ARM (Mali) filters 32 bit float but not 16 bit
	// norm; Qualcomm (Adreno) is the mirror image. Folding them together would deny each vendor the half
	// it actually supports.

	EGraphicsDataTypes_LinearFilter16Norm       = 1 << 17,        //R16, RG16, RGBA16 and their snorm twins
	EGraphicsDataTypes_LinearFilter32f          = 1 << 18,        //R32f, RG32f, RGBA32f

	//RGB9E5 splits cleanly in two: SAMPLING it (and filtering it) is available on effectively everything,
	// measured at 100% of Vulkan 1.1+ devices reporting within a year, so reading needs no bit and OxC3
	// requires it. WRITING is the optional half, around a quarter of devices for storage and a third for
	// render targets, so anything that produces the format rather than consuming it has to check this and
	// carry a fallback.

	EGraphicsDataTypes_WriteRGB9E5              = 1 << 19

} EGraphicsDataTypes;

//This struct represents the abilities a graphics device has.

typedef struct GraphicsDeviceCapabilities {

	EGraphicsFeatures features;
	EGraphicsFeatures2 features2;

	//Subset of `features` that is experimental/preview on this device+build
	// (not final; can change or be removed across SDK/driver updates).
	//On D3D12 the SM6.10-gated cooperative features land here
	// (enabled via the preview SDK + D3D12ExperimentalShaderModels + Developer Mode).
	//On Vulkan they're real extensions, so this stays empty.
	EGraphicsFeatures experimentalFeatures;
	EGraphicsFeatures2 experimentalFeatures2;

	EGraphicsDataTypes dataTypes;
	U32 featuresExt;                //Extended device features, API dependent
	
	F32 timestampPeriod;            //Nanoseconds per GPU timestamp tick under EGraphicsFeatures2_Timestamps, else 0
	U32 padding;

	U64 dedicatedMemory;            //Memory accessible directly to the device
	U64 sharedMemory;               //Memory accessible through the CPU (can be equal to dedicatedMemory if iGPU or CPU)

	U64 maxBufferSize;
	U64 maxAllocationSize;

} GraphicsDeviceCapabilities;

//The device info struct represents a physical device.

typedef struct GraphicsDeviceInfo {

	C8 name[256];
	C8 driverInfo[256];             //Can be empty if unsupported

	EGraphicsDeviceType type;
	EGraphicsVendorId vendor;

	U64 id;

	GraphicsDeviceCapabilities capabilities;

	U64 luid;                       //Check SupportsLUID

	U64 uuid[2];                    //If UUIDs aren't supported, uuid[0] will be luid and uuid[1] will be 0

	void *ext;

} GraphicsDeviceInfo;

//Defined here rather than in instance.h (which includes this header):
// a C11 forward `typedef enum EGraphicsApi EGraphicsApi;` before the definition is ill-formed C++,
// and the C++ graphics layer (graphics/graphics.hpp) includes these headers inside namespace oxc::c.

#define GRAPHICS_API_VULKAN 0
#define GRAPHICS_API_D3D12 1

typedef enum EGraphicsApi {
	EGraphicsApi_Vulkan            = GRAPHICS_API_VULKAN,
	EGraphicsApi_Direct3D12        = GRAPHICS_API_D3D12,
	//EGraphicsApi_Metal, EGraphicsApi_WebGPU,
	EGraphicsApi_Count
} EGraphicsApi;

void GraphicsDeviceInfo_print(EGraphicsApi api, const GraphicsDeviceInfo *deviceInfo, Bool printCapabilities);

//If a texture and render texture can be created with the format.
Bool GraphicsDeviceInfo_supportsFormat(const GraphicsDeviceInfo *deviceInfo, ETextureFormat format);

//If a render texture can be created with the format.
Bool GraphicsDeviceInfo_supportsRenderTextureFormat(const GraphicsDeviceInfo *deviceInfo, ETextureFormat format);

//If a texture format is allowed as a vertex attribute
Bool GraphicsDeviceInfo_supportsFormatVertexAttribute(ETextureFormat format);

//Whether a LINEAR sampler over this format actually filters, which is not implied by supportsFormat:
// the formats this can answer false for are all sampleable everywhere OxC3 runs.
//Only meaningful for a sampled texture; point sampling and every non sampling use are unaffected.

Bool GraphicsDeviceInfo_supportsFormatLinearFilter(const GraphicsDeviceInfo *deviceInfo, ETextureFormat format);

Bool GraphicsDeviceInfo_supportsDepthStencilFormat(const GraphicsDeviceInfo *deviceInfo, EDepthStencilFormat format);

#ifdef __cplusplus
	}
#endif
