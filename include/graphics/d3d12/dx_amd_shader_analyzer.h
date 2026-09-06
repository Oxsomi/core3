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

//graphics/d3d12/dx_amd_shader_analyzer.h

/*  The AmdExtD3D* structure, enum and COM interface declarations in this file are transcribed from
*  AmdExtD3DShaderAnalyzerApi.h and AmdExtD3D.h in the Radeon GPU Analyzer package
*  (source/utils/dx12/backend/extension), which carry the following notice:
*
*  Copyright (c) 2016-2025 Advanced Micro Devices, Inc. All rights reserved.
*
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*
*  The above copyright notice and this permission notice shall be included in
*  all copies or substantial portions of the Software.
*
*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
*  THE SOFTWARE.
*/

#pragma once
#include "graphics/generic/pipeline_structs.h"
#include "types/container/list.h"
#include "types/container/string.h"
#include "d3d12.h"

//D3D12 has no equivalent of VK_KHR_pipeline_executable_properties, so the only way to read a compiled shader back is
// AMD's own driver extension, exported by their user mode driver (amdxc64.dll).
//A pipeline has to be CREATED through the extension to be inspectable afterwards, so the backend routes creation
// through it whenever EPipelineFlags_CaptureISA is set and the extension loaded; every other pipeline takes the
// normal path untouched.
//Absent on any machine without an AMD driver, which is why every entry point here reports availability rather than
// failing: the device simply doesn't advertise EGraphicsFeatures2_PipelineExecutableInfo then.

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct Error Error;
typedef struct Allocator Allocator;
typedef struct ListPipelineExecutable ListPipelineExecutable;

typedef void *AmdExtD3DPipelineHandle;

//The driver's own numbers, which map onto PipelineStatistic once read.
//Transcribed under the notice at the top of this file, because AMD ships the header C++ only and this backend is C.
//The UUIDs in the .c pin what is transcribed, since COM gives a changed layout a new interface id.

typedef struct AmdExtD3DShaderUsageStats {
	U32 numUsedVgprs;
	U32 numUsedSgprs;
	U32 ldsSizePerThreadGroup;                  //Bytes
	U64 ldsUsageSizeInBytes;
	U64 scratchMemUsageInBytes;
} AmdExtD3DShaderUsageStats;

typedef struct AmdExtD3DShaderStats {
	U32 shaderStageMask;                        //Multiple bits means the hardware merged those stages
	AmdExtD3DShaderUsageStats usageStats;
	U32 numPhysicalVgprs;
	U32 numPhysicalSgprs;
	U32 numAvailableVgprs;
	U32 numAvailableSgprs;
	U64 isaSizeInBytes;
} AmdExtD3DShaderStats;

typedef struct AmdExtD3DGraphicsShaderStats {
	AmdExtD3DShaderStats vertexShaderStats;
	AmdExtD3DShaderStats hullShaderStats;
	AmdExtD3DShaderStats domainShaderStats;
	AmdExtD3DShaderStats geometryShaderStats;
	AmdExtD3DShaderStats pixelShaderStats;
} AmdExtD3DGraphicsShaderStats;

//AmdExtD3DComputeShaderStats derives from AmdExtD3DShaderStats, so the base sits first in memory

typedef struct AmdExtD3DComputeShaderStats {
	AmdExtD3DShaderStats base;
	U32 numThreadsPerGroupX;
	U32 numThreadsPerGroupY;
	U32 numThreadsPerGroupZ;
} AmdExtD3DComputeShaderStats;

//AmdExtD3DShaderStatsRayTracing derives from AmdExtD3DShaderStats too; isInlined is a C++ bool, which OxC3's Bool
// is a typedef of, so the layout matches

typedef struct AmdExtD3DShaderStatsRayTracing {
	AmdExtD3DShaderStats base;
	U32 stackSizeInBytes;                       //Stack this shader export uses
	Bool isInlined;                             //An inlined shader has no disassembly of its own
} AmdExtD3DShaderStatsRayTracing;

//Per stage disassembly; each pointer is owned by the driver and is NULL when the pipeline has no such stage

typedef struct AmdExtD3DPipelineDisassembly {
	const C8 *computeDisassembly;
	const C8 *vertexDisassembly;
	const C8 *hullDisassembly;
	const C8 *domainDisassembly;
	const C8 *geometryDisassembly;
	const C8 *pixelDisassembly;
} AmdExtD3DPipelineDisassembly;

//One per device, loaded once and shared by every pipeline that asks for capture.

typedef struct DxAmdShaderAnalyzer {
	void *umd;                                  //amdxc64.dll, kept loaded for as long as the interfaces live
	void *factory;                              //IAmdExtD3DFactory
	void *analyzer;                             //IAmdExtD3DShaderAnalyzer1, or 2 when hasRaytracing

	//Ray tracing needs the DXR half of the extension (IAmdExtD3DShaderAnalyzer2); a driver that only offers
	// version 1 still does graphics and compute.

	Bool hasRaytracing;
} DxAmdShaderAnalyzer;

//The AMD driver can compile for ASICs other than the one installed, which is what makes a single AMD machine able
// to produce ISA for a whole generation rather than only its own GPU.
//Selecting one is an environment variable the driver reads while initializing the adapter, so it has to be set
// BEFORE the device is created, and the listing needs a device, meaning a run that targets a virtual GPU creates
// one device to enumerate and a second to compile with.

typedef struct DxAmdVirtualGpu {
	CharString name;                            //As the driver reports it, "gpuName:gfxIp"
	U32 gpuId;
	U32 padding;
} DxAmdVirtualGpu;

TList(DxAmdVirtualGpu);

Bool DxAmdShaderAnalyzer_listVirtualGpus(
	const DxAmdShaderAnalyzer *analyzer, const Allocator *alloc, ListDxAmdVirtualGpu *result, Error *e_rr
);

//Picks the ASIC the next device created in this process compiles for; clearing it returns to the real GPU.

Bool DxAmdShaderAnalyzer_selectVirtualGpu(U32 gpuId);
Bool DxAmdShaderAnalyzer_clearVirtualGpu();

//Whether this process already carries a selection, which it does when it inherited one.
//Two things follow from it and neither is the caller's to remember: the D3D12 debug layer faults inside the AMD
// driver as soon as a virtual GPU is selected, and an emulated ASIC reports that ASIC's memory rather than the
// machine's, which the device requirements would reject it over.

Bool DxAmdShaderAnalyzer_virtualGpuSelected();

//Loads the driver extension for this device; false (with the struct zeroed) on any machine that has no AMD driver,
// which is a normal outcome rather than an error.

Bool DxAmdShaderAnalyzer_init(ID3D12Device *device, DxAmdShaderAnalyzer *analyzer);
void DxAmdShaderAnalyzer_free(DxAmdShaderAnalyzer *analyzer);

//Pipeline creation that keeps the compiled shader inspectable; both mirror the ID3D12Device call they replace and
// additionally hand back the handle the ISA is later read with.

Bool DxAmdShaderAnalyzer_createGraphicsPipeline(
	const DxAmdShaderAnalyzer *analyzer,
	const D3D12_GRAPHICS_PIPELINE_STATE_DESC *desc,
	ID3D12PipelineState **pipelineState,
	AmdExtD3DPipelineHandle *handle,
	Error *e_rr
);

Bool DxAmdShaderAnalyzer_createComputePipeline(
	const DxAmdShaderAnalyzer *analyzer,
	const D3D12_COMPUTE_PIPELINE_STATE_DESC *desc,
	ID3D12PipelineState **pipelineState,
	AmdExtD3DPipelineHandle *handle,
	Error *e_rr
);

//Ray tracing goes through a state object rather than a pipeline state, and only version 2 offers it.

Bool DxAmdShaderAnalyzer_createStateObject(
	const DxAmdShaderAnalyzer *analyzer,
	const D3D12_STATE_OBJECT_DESC *desc,
	ID3D12StateObject **stateObject,
	AmdExtD3DPipelineHandle *handle,
	Error *e_rr
);

//Reads the driver's ISA and statistics back for a pipeline created above, in the same shape the Vulkan backend
// returns: one PipelineExecutable per stage a graphics or compute pipeline holds, or per shader export for a ray
// tracing one.

Bool DxAmdShaderAnalyzer_getExecutables(
	const DxAmdShaderAnalyzer *analyzer,
	AmdExtD3DPipelineHandle handle,
	EPipelineType type,
	const Allocator *alloc,
	ListPipelineExecutable *result,
	Error *e_rr
);

#ifdef __cplusplus
	}
#endif
