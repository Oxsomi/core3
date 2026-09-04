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

//graphics/d3d12/generic/dx_amd_shader_analyzer.c

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

#include "graphics/d3d12/dx_amd_shader_analyzer.h"
#include "graphics/d3d12/direct3d12.h"
#include "graphics/generic/pipeline.h"
#include "formats/oiSH/sh_entries.h"
#include "types/container/list_impl.h"
#include "types/container/string.h"
#include "types/container/string_unicode.h"
#include "types/container/log.h"
#include "types/base/allocator.h"
#include "types/base/error.h"

//AMD's extension is C++ only (it uses `interface` and virtual inheritance) while this backend is C, so the two
// interfaces are transcribed to the vtable layout COM already uses for D3D12 here.
//The UUIDs pin exactly what is transcribed: COM gives an interface a new id when its layout changes, so a driver that
// hands back these ids hands back these vtables.
//x64 only, which is all the AMD user mode driver ships as, so there's no __thiscall vs __stdcall ambiguity.

typedef struct IAmdExtD3DFactory IAmdExtD3DFactory;
typedef struct IAmdExtD3DShaderAnalyzer1 IAmdExtD3DShaderAnalyzer1;
typedef struct IAmdExtD3DShaderAnalyzer1 IAmdExtD3DShaderAnalyzer2;   //Same object, longer vtable

typedef struct IAmdExtD3DFactoryVtbl {

	HRESULT (STDMETHODCALLTYPE *QueryInterface)(IAmdExtD3DFactory *self, REFIID riid, void **object);
	ULONG (STDMETHODCALLTYPE *AddRef)(IAmdExtD3DFactory *self);
	ULONG (STDMETHODCALLTYPE *Release)(IAmdExtD3DFactory *self);

	HRESULT (STDMETHODCALLTYPE *CreateInterface)(IAmdExtD3DFactory *self, IUnknown *outer, REFIID riid, void **object);

} IAmdExtD3DFactoryVtbl;

struct IAmdExtD3DFactory { const IAmdExtD3DFactoryVtbl *lpVtbl; };

typedef struct IAmdExtD3DShaderAnalyzer1Vtbl {

	HRESULT (STDMETHODCALLTYPE *QueryInterface)(IAmdExtD3DShaderAnalyzer1 *self, REFIID riid, void **object);
	ULONG (STDMETHODCALLTYPE *AddRef)(IAmdExtD3DShaderAnalyzer1 *self);
	ULONG (STDMETHODCALLTYPE *Release)(IAmdExtD3DShaderAnalyzer1 *self);

	//IAmdExtD3DShaderAnalyzer

	HRESULT (STDMETHODCALLTYPE *GetAvailableVirtualGpuIds)(IAmdExtD3DShaderAnalyzer1 *self, void *gpuIdList);

	HRESULT (STDMETHODCALLTYPE *CreateGraphicsPipelineState)(
		IAmdExtD3DShaderAnalyzer1 *self,
		const D3D12_GRAPHICS_PIPELINE_STATE_DESC *desc,
		REFIID riid,
		void **pipelineState,
		AmdExtD3DGraphicsShaderStats *graphicsShaderStats,
		AmdExtD3DPipelineHandle *pipelineHandle
	);

	HRESULT (STDMETHODCALLTYPE *CreateComputePipelineState)(
		IAmdExtD3DShaderAnalyzer1 *self,
		const D3D12_COMPUTE_PIPELINE_STATE_DESC *desc,
		REFIID riid,
		void **pipelineState,
		AmdExtD3DComputeShaderStats *computeShaderStats,
		AmdExtD3DPipelineHandle *pipelineHandle
	);

	HRESULT (STDMETHODCALLTYPE *GetShaderIsaCode)(
		IAmdExtD3DShaderAnalyzer1 *self,
		AmdExtD3DPipelineHandle pipelineHandle,
		C8 *isaCode,
		U64 *isaCodeSize,
		AmdExtD3DPipelineDisassembly *disassembly
	);

	//IAmdExtD3DShaderAnalyzer1 adds reading the stats back from the handle, which is what lets creation and
	// inspection stay separate steps here.

	HRESULT (STDMETHODCALLTYPE *CreateGraphicsPipelineState1)(
		IAmdExtD3DShaderAnalyzer1 *self,
		const D3D12_GRAPHICS_PIPELINE_STATE_DESC *desc,
		REFIID riid,
		void **pipelineState,
		AmdExtD3DPipelineHandle *pipelineHandle
	);

	HRESULT (STDMETHODCALLTYPE *CreateComputePipelineState1)(
		IAmdExtD3DShaderAnalyzer1 *self,
		const D3D12_COMPUTE_PIPELINE_STATE_DESC *desc,
		REFIID riid,
		void **pipelineState,
		AmdExtD3DPipelineHandle *pipelineHandle
	);

	HRESULT (STDMETHODCALLTYPE *GetGraphicsShaderStats)(
		IAmdExtD3DShaderAnalyzer1 *self,
		AmdExtD3DPipelineHandle pipelineHandle,
		AmdExtD3DGraphicsShaderStats *graphicsShaderStats
	);

	HRESULT (STDMETHODCALLTYPE *GetComputeShaderStats)(
		IAmdExtD3DShaderAnalyzer1 *self,
		AmdExtD3DPipelineHandle pipelineHandle,
		AmdExtD3DComputeShaderStats *computeShaderStats
	);

	HRESULT (STDMETHODCALLTYPE *GetPipelineElfBinary)(
		IAmdExtD3DShaderAnalyzer1 *self,
		AmdExtD3DPipelineHandle pipelineHandle,
		void *pipelineElfHandle,
		U32 *size
	);

	//IAmdExtD3DShaderAnalyzer2 adds ray tracing, the half Vulkan can't answer on NVIDIA at all.
	//These entries always exist because the vtable is one contiguous layout; whether they may be CALLED is what
	// hasRaytracing records, since a version 1 object simply has no slots there.

	HRESULT (STDMETHODCALLTYPE *CreateStateObject)(
		IAmdExtD3DShaderAnalyzer2 *self,
		const D3D12_STATE_OBJECT_DESC *desc,
		REFIID riid,
		void **stateObject,
		AmdExtD3DPipelineHandle *pipelineHandle
	);

	HRESULT (STDMETHODCALLTYPE *GetRayTracingPipelineStats)(
		IAmdExtD3DShaderAnalyzer2 *self,
		AmdExtD3DPipelineHandle pipelineHandle,
		const wchar_t *rayGenerationExportName,
		AmdExtD3DShaderStatsRayTracing *stats
	);

	HRESULT (STDMETHODCALLTYPE *GetRayTracingPipelineStatsByIndex)(
		IAmdExtD3DShaderAnalyzer2 *self,
		AmdExtD3DPipelineHandle pipelineHandle,
		U32 pipelineIndex,
		AmdExtD3DShaderStatsRayTracing *stats
	);

	HRESULT (STDMETHODCALLTYPE *GetRayTracingPipelineShaderStats)(
		IAmdExtD3DShaderAnalyzer2 *self,
		AmdExtD3DPipelineHandle pipelineHandle,
		const wchar_t *shaderExportName,
		AmdExtD3DShaderStatsRayTracing *stats
	);

	HRESULT (STDMETHODCALLTYPE *GetRayTracingShaderIsaDisassembly)(
		IAmdExtD3DShaderAnalyzer2 *self,
		AmdExtD3DPipelineHandle pipelineHandle,
		const wchar_t *shaderExportName,
		C8 *isaCode,
		U64 *isaCodeSize
	);

	HRESULT (STDMETHODCALLTYPE *GetRayTracingPipelineIsaDisassembly)(
		IAmdExtD3DShaderAnalyzer2 *self,
		AmdExtD3DPipelineHandle pipelineHandle,
		const wchar_t *rayGenerationExportName,
		C8 *isaCode,
		U64 *isaCodeSize
	);

	HRESULT (STDMETHODCALLTYPE *IsRayTracingPipelineIndirect)(
		IAmdExtD3DShaderAnalyzer2 *self,
		AmdExtD3DPipelineHandle pipelineHandle,
		U32 pipelineIndex,
		Bool *isIndirect
	);

	HRESULT (STDMETHODCALLTYPE *GetRayTracingPipelineShaderCount)(
		IAmdExtD3DShaderAnalyzer2 *self,
		AmdExtD3DPipelineHandle pipelineHandle,
		U32 pipelineIndex,
		U32 *shaderCount
	);

	HRESULT (STDMETHODCALLTYPE *GetRayTracingPipelineShaderName)(
		IAmdExtD3DShaderAnalyzer2 *self,
		AmdExtD3DPipelineHandle pipelineHandle,
		U32 pipelineIndex,
		U32 shaderIndex,
		U64 *nameCount,
		wchar_t *nameBuffer
	);

	HRESULT (STDMETHODCALLTYPE *GetRayTracingPipelineIsaDisassemblyByIndex)(
		IAmdExtD3DShaderAnalyzer2 *self,
		AmdExtD3DPipelineHandle pipelineHandle,
		U32 pipelineIndex,
		C8 *isaCode,
		U64 *isaCodeSize
	);

	HRESULT (STDMETHODCALLTYPE *GetRayTracingPipelineElfCount)(
		IAmdExtD3DShaderAnalyzer2 *self,
		AmdExtD3DPipelineHandle pipelineHandle,
		U32 *pipelineCount
	);

	HRESULT (STDMETHODCALLTYPE *GetRayTracingPipelineElfBinary)(
		IAmdExtD3DShaderAnalyzer2 *self,
		AmdExtD3DPipelineHandle pipelineHandle,
		U32 pipelineIndex,
		void *pipelineElfHandle,
		U32 *size
	);

} IAmdExtD3DShaderAnalyzer1Vtbl;

struct IAmdExtD3DShaderAnalyzer1 { const IAmdExtD3DShaderAnalyzer1Vtbl *lpVtbl; };

//014937EC-9288-446F-A9AC-D75A8E3A984F and A2EA2E25-9709-47E3-B3A0-6DDA8A9372C4

static const GUID GUID_IAmdExtD3DFactory = {
	0x014937EC, 0x9288, 0x446F, { 0xA9, 0xAC, 0xD7, 0x5A, 0x8E, 0x3A, 0x98, 0x4F }
};

static const GUID GUID_IAmdExtD3DShaderAnalyzer1 = {
	0xA2EA2E25, 0x9709, 0x47E3, { 0xB3, 0xA0, 0x6D, 0xDA, 0x8A, 0x93, 0x72, 0xC4 }
};

//B63EEBE2-411B-494D-A4D1-08676225773C, version 2, which adds DXR

static const GUID GUID_IAmdExtD3DShaderAnalyzer2 = {
	0xB63EEBE2, 0x411B, 0x494D, { 0xA4, 0xD1, 0x08, 0x67, 0x62, 0x25, 0x77, 0x3C }
};

//The driver reads this while initializing the adapter, so it only takes effect for a device created afterwards.

static const C8 *AmdExtVirtualGpuIdEnvVar = "AmdVirtualGpuId";

typedef struct AmdExtD3DGpuIdEntry {
	U32 gpuId;
	U32 padding;
	const C8 *gpuIdName;                        //"gpuName:gfxIp"
} AmdExtD3DGpuIdEntry;

typedef struct AmdExtD3DGpuIdList {
	U32 numGpuIdEntries;
	U32 padding;
	AmdExtD3DGpuIdEntry *gpuIdEntries;
} AmdExtD3DGpuIdList;

TListImpl(DxAmdVirtualGpu);

typedef HRESULT (__cdecl *PFNAmdExtD3DCreateInterface)(IUnknown *outer, REFIID riid, void **object);

Bool DxAmdShaderAnalyzer_init(ID3D12Device *device, DxAmdShaderAnalyzer *analyzer) {

	if(!device || !analyzer)
		return false;

	*analyzer = (DxAmdShaderAnalyzer) { 0 };

	//The user mode driver is only present when an AMD driver is installed, so not finding it is the normal case on
	// every other machine and isn't reported as a failure.

	HMODULE umd = GetModuleHandleA("amdxc64.dll");

	if(!umd)
		umd = LoadLibraryA("amdxc64.dll");

	if(!umd)
		return false;

	//Through void*, because casting GetProcAddress's FARPROC straight to the entry's own signature is a cast
	// between incompatible function types, which clang rejects.

	const PFNAmdExtD3DCreateInterface createInterface =
		(PFNAmdExtD3DCreateInterface)(void*) GetProcAddress(umd, "AmdExtD3DCreateInterface");

	if(!createInterface)
		return false;

	IAmdExtD3DFactory *factory = NULL;

	if(createInterface((IUnknown*) device, &GUID_IAmdExtD3DFactory, (void**) &factory) != S_OK || !factory)
		return false;

	IAmdExtD3DShaderAnalyzer1 *shaderAnalyzer = NULL;

	//Version 2 is asked for first because it adds ray tracing; version 1 still covers graphics and compute.

	Bool hasRaytracing = true;

	HRESULT hr = factory->lpVtbl->CreateInterface(
		factory, (IUnknown*) device, &GUID_IAmdExtD3DShaderAnalyzer2, (void**) &shaderAnalyzer
	);

	if (hr != S_OK || !shaderAnalyzer) {

		hasRaytracing = false;

		hr = factory->lpVtbl->CreateInterface(
			factory, (IUnknown*) device, &GUID_IAmdExtD3DShaderAnalyzer1, (void**) &shaderAnalyzer
		);
	}

	if(hr != S_OK || !shaderAnalyzer) {
		factory->lpVtbl->Release(factory);
		return false;
	}

	analyzer->umd = umd;
	analyzer->factory = factory;
	analyzer->analyzer = shaderAnalyzer;
	analyzer->hasRaytracing = hasRaytracing;
	return true;
}

//Two calls, as the extension documents: the first reports how many entries there are, the second fills them.

Bool DxAmdShaderAnalyzer_listVirtualGpus(
	const DxAmdShaderAnalyzer *analyzer, const Allocator *alloc, ListDxAmdVirtualGpu *result, Error *e_rr
) {

	Bool s_uccess = true;
	Buffer entries = Buffer_createNull();
	ListDxAmdVirtualGpu gpus = (ListDxAmdVirtualGpu) { 0 };

	if(!analyzer || !analyzer->analyzer || !result)
		retError(clean, Error_nullPointer(0, "DxAmdShaderAnalyzer_listVirtualGpus() requires all arguments"));

	IAmdExtD3DShaderAnalyzer1 *sa = (IAmdExtD3DShaderAnalyzer1*) analyzer->analyzer;

	AmdExtD3DGpuIdList list = (AmdExtD3DGpuIdList) { 0 };
	gotoIfError3(clean, dxCheck(sa->lpVtbl->GetAvailableVirtualGpuIds(sa, &list), e_rr));

	if(!list.numGpuIdEntries)
		goto clean;

	//The driver reports the count first and fills the entries second, so the loop stays on what was allocated
	// rather than on a count that could have shifted between the two calls.

	const U32 count = list.numGpuIdEntries;

	gotoIfError3(clean, Buffer_createEmptyBytes((U64) count * sizeof(AmdExtD3DGpuIdEntry), alloc, &entries, e_rr));

	list.gpuIdEntries = (AmdExtD3DGpuIdEntry*) entries.ptrNonConst;
	gotoIfError3(clean, dxCheck(sa->lpVtbl->GetAvailableVirtualGpuIds(sa, &list), e_rr));

	gotoIfError3(clean, ListDxAmdVirtualGpu_resize(&gpus, count, alloc, e_rr));

	for (U32 i = 0; i < count; ++i) {

		//The name belongs to the driver, so the list keeps its own copy

		DxAmdVirtualGpu *gpu = &gpus.ptrNonConst[i];
		gpu->gpuId = list.gpuIdEntries[i].gpuId;

		if(list.gpuIdEntries[i].gpuIdName)
			gotoIfError3(clean, CharString_createCopy(
				CharString_createRefCStrConst(list.gpuIdEntries[i].gpuIdName), alloc, &gpu->name, e_rr
			));
	}

	*result = gpus;
	gpus = (ListDxAmdVirtualGpu) { 0 };

clean:

	for(U64 i = 0; i < gpus.length; ++i)
		CharString_free(&gpus.ptrNonConst[i].name, alloc);

	ListDxAmdVirtualGpu_free(&gpus, alloc);
	Buffer_free(&entries, alloc);
	return s_uccess;
}

Bool DxAmdShaderAnalyzer_selectVirtualGpu(U32 gpuId) {
	//A plain decimal, written without pulling a formatter in for it

	C8 value[16] = { 0 };
	U8 digits = 0;

	do {
		value[digits++] = (C8)('0' + (gpuId % 10));
		gpuId /= 10;
	} while(gpuId && digits < sizeof(value) - 1);

	for (U8 i = 0; i < digits >> 1; ++i) {
		const C8 tmp = value[i];
		value[i] = value[digits - 1 - i];
		value[digits - 1 - i] = tmp;
	}

	return SetEnvironmentVariableA(AmdExtVirtualGpuIdEnvVar, value);
}

Bool DxAmdShaderAnalyzer_clearVirtualGpu() {
	return SetEnvironmentVariableA(AmdExtVirtualGpuIdEnvVar, NULL);
}

void DxAmdShaderAnalyzer_free(DxAmdShaderAnalyzer *analyzer) {

	if(!analyzer)
		return;

	if(analyzer->analyzer)
		((IAmdExtD3DShaderAnalyzer1*)analyzer->analyzer)->lpVtbl->Release(
			(IAmdExtD3DShaderAnalyzer1*)analyzer->analyzer
		);

	if(analyzer->factory)
		((IAmdExtD3DFactory*)analyzer->factory)->lpVtbl->Release((IAmdExtD3DFactory*)analyzer->factory);

	//The module handle is deliberately not freed: the driver stays loaded for the process either way, and releasing
	// it while another device still holds interfaces from it would take those down with it.

	*analyzer = (DxAmdShaderAnalyzer) { 0 };
}

Bool DxAmdShaderAnalyzer_createGraphicsPipeline(
	const DxAmdShaderAnalyzer *analyzer,
	const D3D12_GRAPHICS_PIPELINE_STATE_DESC *desc,
	ID3D12PipelineState **pipelineState,
	AmdExtD3DPipelineHandle *handle,
	Error *e_rr
) {

	Bool s_uccess = true;

	if(!analyzer || !analyzer->analyzer || !desc || !pipelineState || !handle)
		retError(clean, Error_nullPointer(0, "DxAmdShaderAnalyzer_createGraphicsPipeline() requires all arguments"));

	IAmdExtD3DShaderAnalyzer1 *sa = (IAmdExtD3DShaderAnalyzer1*) analyzer->analyzer;

	//The ...1 variant skips the stats out parameter, since they're read back from the handle when asked for.

	gotoIfError3(clean, dxCheck(sa->lpVtbl->CreateGraphicsPipelineState1(
		sa, desc, &IID_ID3D12PipelineState, (void**) pipelineState, handle
	), e_rr));

clean:
	return s_uccess;
}

Bool DxAmdShaderAnalyzer_createComputePipeline(
	const DxAmdShaderAnalyzer *analyzer,
	const D3D12_COMPUTE_PIPELINE_STATE_DESC *desc,
	ID3D12PipelineState **pipelineState,
	AmdExtD3DPipelineHandle *handle,
	Error *e_rr
) {

	Bool s_uccess = true;

	if(!analyzer || !analyzer->analyzer || !desc || !pipelineState || !handle)
		retError(clean, Error_nullPointer(0, "DxAmdShaderAnalyzer_createComputePipeline() requires all arguments"));

	IAmdExtD3DShaderAnalyzer1 *sa = (IAmdExtD3DShaderAnalyzer1*) analyzer->analyzer;

	gotoIfError3(clean, dxCheck(sa->lpVtbl->CreateComputePipelineState1(
		sa, desc, &IID_ID3D12PipelineState, (void**) pipelineState, handle
	), e_rr));

clean:
	return s_uccess;
}

Bool DxAmdShaderAnalyzer_createStateObject(
	const DxAmdShaderAnalyzer *analyzer,
	const D3D12_STATE_OBJECT_DESC *desc,
	ID3D12StateObject **stateObject,
	AmdExtD3DPipelineHandle *handle,
	Error *e_rr
) {

	Bool s_uccess = true;

	if(!analyzer || !analyzer->analyzer || !desc || !stateObject || !handle)
		retError(clean, Error_nullPointer(0, "DxAmdShaderAnalyzer_createStateObject() requires all arguments"));

	if(!analyzer->hasRaytracing)
		retError(clean, Error_unsupportedOperation(
			0, "DxAmdShaderAnalyzer_createStateObject() needs version 2 of the extension, which this driver lacks"
		));

	IAmdExtD3DShaderAnalyzer2 *sa = (IAmdExtD3DShaderAnalyzer2*) analyzer->analyzer;

	gotoIfError3(clean, dxCheck(sa->lpVtbl->CreateStateObject(
		sa, desc, &IID_ID3D12StateObject, (void**) stateObject, handle
	), e_rr));

clean:
	return s_uccess;
}

//One statistic, named the way the Vulkan backend names the driver's own

static Bool DxAmdShaderAnalyzer_addStat(
	ListPipelineStatistic *stats, const C8 *name, const C8 *description, U64 value, const Allocator *alloc, Error *e_rr
) {

	Bool s_uccess = true;
	PipelineStatistic stat = (PipelineStatistic) { .value = value, .format = EPipelineStatisticFormat_U64 };

	gotoIfError3(clean, CharString_createCopy(CharString_createRefCStrConst(name), alloc, &stat.name, e_rr));
	gotoIfError3(clean, CharString_createCopy(CharString_createRefCStrConst(description), alloc, &stat.description, e_rr));
	gotoIfError3(clean, ListPipelineStatistic_pushBack(stats, stat, alloc, e_rr));
	stat = (PipelineStatistic) { 0 };

clean:
	CharString_free(&stat.name, alloc);
	CharString_free(&stat.description, alloc);
	return s_uccess;
}

//One executable per stage the pipeline holds, carrying that stage's disassembly and the driver's numbers for it

static Bool DxAmdShaderAnalyzer_addExecutable(
	ListPipelineExecutable *executables,
	const C8 *name,
	const C8 *disassembly,
	const AmdExtD3DShaderStats *stats,
	EGfxPipelineStage stage,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;
	PipelineExecutable exec = (PipelineExecutable) { .stages = (U32)(1 << stage) };

	//A stage the pipeline doesn't have has neither disassembly nor any register usage to report

	if(!disassembly && (!stats || !stats->usageStats.numUsedVgprs))
		return true;

	gotoIfError3(clean, CharString_createCopy(CharString_createRefCStrConst(name), alloc, &exec.name, e_rr));

	gotoIfError3(clean, CharString_createCopy(
		CharString_createRefCStrConst("AMD driver shader analyzer (IAmdExtD3DShaderAnalyzer)"), alloc, &exec.description, e_rr
	));

	if(disassembly)
		gotoIfError3(clean, CharString_createCopy(CharString_createRefCStrConst(disassembly), alloc, &exec.disassembly, e_rr));

	if (stats) {

		const AmdExtD3DShaderUsageStats *usage = &stats->usageStats;

		gotoIfError3(clean, DxAmdShaderAnalyzer_addStat(
			&exec.statistics, "VGPRs", "Vector general purpose registers used", usage->numUsedVgprs, alloc, e_rr
		));

		gotoIfError3(clean, DxAmdShaderAnalyzer_addStat(
			&exec.statistics, "SGPRs", "Scalar general purpose registers used", usage->numUsedSgprs, alloc, e_rr
		));

		gotoIfError3(clean, DxAmdShaderAnalyzer_addStat(
			&exec.statistics, "Available VGPRs", "Vector registers made available to the shader",
			stats->numAvailableVgprs, alloc, e_rr
		));

		gotoIfError3(clean, DxAmdShaderAnalyzer_addStat(
			&exec.statistics, "Available SGPRs", "Scalar registers made available to the shader",
			stats->numAvailableSgprs, alloc, e_rr
		));

		gotoIfError3(clean, DxAmdShaderAnalyzer_addStat(
			&exec.statistics, "LDS usage", "Local data share used by the shader in bytes",
			usage->ldsUsageSizeInBytes, alloc, e_rr
		));

		gotoIfError3(clean, DxAmdShaderAnalyzer_addStat(
			&exec.statistics, "Scratch usage", "Scratch memory used by the shader in bytes",
			usage->scratchMemUsageInBytes, alloc, e_rr
		));

		gotoIfError3(clean, DxAmdShaderAnalyzer_addStat(
			&exec.statistics, "ISA size", "Size of this shader's ISA in bytes", stats->isaSizeInBytes, alloc, e_rr
		));
	}

	gotoIfError3(clean, ListPipelineExecutable_pushBack(executables, exec, alloc, e_rr));
	exec = (PipelineExecutable) { 0 };

clean:
	PipelineExecutable_free(&exec, alloc);
	return s_uccess;
}

static Bool DxAmdShaderAnalyzer_addRaytracingExecutable(
	ListPipelineExecutable *executables,
	CharString name,
	const C8 *disassembly,
	const AmdExtD3DShaderStatsRayTracing *stats,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;

	//Every ray tracing stage lands in one executable list, so the stage mask says raygen through callable rather
	// than trying to attribute an export to one of them.

	PipelineExecutable exec = (PipelineExecutable) {
		.stages =
			(1 << EGfxPipelineStage_RaygenExt) | (1 << EGfxPipelineStage_MissExt) |
			(1 << EGfxPipelineStage_ClosestHitExt) | (1 << EGfxPipelineStage_AnyHitExt) |
			(1 << EGfxPipelineStage_IntersectionExt) | (1 << EGfxPipelineStage_CallableExt)
	};

	gotoIfError3(clean, CharString_createCopy(name, alloc, &exec.name, e_rr));

	gotoIfError3(clean, CharString_createCopy(
		CharString_createRefCStrConst("AMD driver shader analyzer (IAmdExtD3DShaderAnalyzer2, DXR)"),
		alloc, &exec.description, e_rr
	));

	if(disassembly)
		gotoIfError3(clean, CharString_createCopy(CharString_createRefCStrConst(disassembly), alloc, &exec.disassembly, e_rr));

	if (stats) {

		const AmdExtD3DShaderUsageStats *usage = &stats->base.usageStats;

		gotoIfError3(clean, DxAmdShaderAnalyzer_addStat(
			&exec.statistics, "VGPRs", "Vector general purpose registers used", usage->numUsedVgprs, alloc, e_rr
		));

		gotoIfError3(clean, DxAmdShaderAnalyzer_addStat(
			&exec.statistics, "SGPRs", "Scalar general purpose registers used", usage->numUsedSgprs, alloc, e_rr
		));

		gotoIfError3(clean, DxAmdShaderAnalyzer_addStat(
			&exec.statistics, "Scratch usage", "Scratch memory used by the shader in bytes",
			usage->scratchMemUsageInBytes, alloc, e_rr
		));

		gotoIfError3(clean, DxAmdShaderAnalyzer_addStat(
			&exec.statistics, "Stack size", "Stack this shader export uses in bytes", stats->stackSizeInBytes, alloc, e_rr
		));

		gotoIfError3(clean, DxAmdShaderAnalyzer_addStat(
			&exec.statistics, "ISA size", "Size of this shader's ISA in bytes", stats->base.isaSizeInBytes, alloc, e_rr
		));

		gotoIfError3(clean, DxAmdShaderAnalyzer_addStat(
			&exec.statistics, "Inlined", "Whether the driver folded this shader into an uber shader",
			stats->isInlined, alloc, e_rr
		));
	}

	gotoIfError3(clean, ListPipelineExecutable_pushBack(executables, exec, alloc, e_rr));
	exec = (PipelineExecutable) { 0 };

clean:
	PipelineExecutable_free(&exec, alloc);
	return s_uccess;
}

//Ray tracing is enumerated rather than named: the driver reports how many pipelines the state object compiled to and
// which shaders each holds, so nothing has to be matched against HLSL export names from this side.
//A shader the driver inlined into an uber shader has no disassembly of its own, which it says through isInlined; the
// pipeline wide disassembly is added once in that case so the ISA isn't simply missing.

static Bool DxAmdShaderAnalyzer_addRaytracingPipeline(
	IAmdExtD3DShaderAnalyzer2 *sa,
	AmdExtD3DPipelineHandle handle,
	U32 pipelineIndex,
	ListPipelineExecutable *executables,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;
	Buffer isa = Buffer_createNull();
	Buffer nameBuf = Buffer_createNull();
	CharString name = CharString_createNull();
	Bool anyInlined = false;

	U32 shaderCount = 0;
	gotoIfError3(clean, dxCheck(sa->lpVtbl->GetRayTracingPipelineShaderCount(sa, handle, pipelineIndex, &shaderCount), e_rr));

	for (U32 i = 0; i < shaderCount; ++i) {

		//The export name comes back as UTF-16 and is asked for by size first, like every other query here

		U64 nameCount = 0;
		gotoIfError3(clean, dxCheck(sa->lpVtbl->GetRayTracingPipelineShaderName(
			sa, handle, pipelineIndex, i, &nameCount, NULL
		), e_rr));

		if(!nameCount)
			continue;

		Buffer_free(&nameBuf, alloc);
		gotoIfError3(clean, Buffer_createUninitializedBytes((nameCount + 1) * sizeof(wchar_t), alloc, &nameBuf, e_rr));

		wchar_t *nameW = (wchar_t*) nameBuf.ptrNonConst;
		nameW[nameCount] = 0;

		gotoIfError3(clean, dxCheck(sa->lpVtbl->GetRayTracingPipelineShaderName(
			sa, handle, pipelineIndex, i, &nameCount, nameW
		), e_rr));

		CharString_free(&name, alloc);
		gotoIfError3(clean, CharString_createFromUTF16((const U16*) nameW, U64_MAX, alloc, &name, e_rr));

		AmdExtD3DShaderStatsRayTracing stats = (AmdExtD3DShaderStatsRayTracing) { 0 };
		sa->lpVtbl->GetRayTracingPipelineShaderStats(sa, handle, nameW, &stats);

		//Per shader disassembly fails for an inlined shader, which isn't an error: it has none to give

		Buffer_free(&isa, alloc);
		U64 isaSize = 0;

		const Bool hasIsa =
			!stats.isInlined &&
			sa->lpVtbl->GetRayTracingShaderIsaDisassembly(sa, handle, nameW, NULL, &isaSize) == S_OK &&
			isaSize;

		if (hasIsa) {

			gotoIfError3(clean, Buffer_createUninitializedBytes(isaSize + 1, alloc, &isa, e_rr));
			isa.ptrNonConst[isaSize] = 0;

			gotoIfError3(clean, dxCheck(sa->lpVtbl->GetRayTracingShaderIsaDisassembly(
				sa, handle, nameW, (C8*) isa.ptrNonConst, &isaSize
			), e_rr));
		}

		else anyInlined = true;

		gotoIfError3(clean, DxAmdShaderAnalyzer_addRaytracingExecutable(
			executables, name, hasIsa ? (const C8*) isa.ptr : NULL, &stats, alloc, e_rr
		));
	}

	//One entry for the uber shader the inlined ones ended up in, so their ISA is reachable even unattributed

	if (anyInlined) {

		Buffer_free(&isa, alloc);
		U64 isaSize = 0;

		if (
			sa->lpVtbl->GetRayTracingPipelineIsaDisassemblyByIndex(sa, handle, pipelineIndex, NULL, &isaSize) == S_OK &&
			isaSize
		) {

			gotoIfError3(clean, Buffer_createUninitializedBytes(isaSize + 1, alloc, &isa, e_rr));
			isa.ptrNonConst[isaSize] = 0;

			gotoIfError3(clean, dxCheck(sa->lpVtbl->GetRayTracingPipelineIsaDisassemblyByIndex(
				sa, handle, pipelineIndex, (C8*) isa.ptrNonConst, &isaSize
			), e_rr));

			CharString_free(&name, alloc);
			gotoIfError3(clean, CharString_format(alloc, &name, e_rr, "Pipeline %"PRIu32" (inlined shaders)", pipelineIndex));

			AmdExtD3DShaderStatsRayTracing stats = (AmdExtD3DShaderStatsRayTracing) { 0 };
			sa->lpVtbl->GetRayTracingPipelineStatsByIndex(sa, handle, pipelineIndex, &stats);

			gotoIfError3(clean, DxAmdShaderAnalyzer_addRaytracingExecutable(
				executables, name, (const C8*) isa.ptr, &stats, alloc, e_rr
			));
		}
	}

clean:
	CharString_free(&name, alloc);
	Buffer_free(&nameBuf, alloc);
	Buffer_free(&isa, alloc);
	return s_uccess;
}

Bool DxAmdShaderAnalyzer_getExecutables(
	const DxAmdShaderAnalyzer *analyzer,
	AmdExtD3DPipelineHandle handle,
	EPipelineType type,
	const Allocator *alloc,
	ListPipelineExecutable *result,
	Error *e_rr
) {

	Bool s_uccess = true;
	Buffer isa = Buffer_createNull();
	ListPipelineExecutable executables = (ListPipelineExecutable) { 0 };

	if(!analyzer || !analyzer->analyzer || !handle || !result)
		retError(clean, Error_nullPointer(0, "DxAmdShaderAnalyzer_getExecutables() requires all arguments"));

	IAmdExtD3DShaderAnalyzer1 *sa = (IAmdExtD3DShaderAnalyzer1*) analyzer->analyzer;

	//Ray tracing is a different shape entirely: many shader exports rather than a fixed set of stages.

	if (type == EPipelineType_RaytracingExt) {

		if(!analyzer->hasRaytracing)
			retError(clean, Error_unsupportedOperation(
				0, "DxAmdShaderAnalyzer_getExecutables() ray tracing needs version 2 of the extension"
			));

		IAmdExtD3DShaderAnalyzer2 *sa2 = (IAmdExtD3DShaderAnalyzer2*) analyzer->analyzer;

		//A state object can compile to more than one pipeline, and each is enumerated on its own.

		U32 pipelineCount = 0;
		gotoIfError3(clean, dxCheck(sa2->lpVtbl->GetRayTracingPipelineElfCount(sa2, handle, &pipelineCount), e_rr));

		for(U32 i = 0; i < pipelineCount; ++i)
			gotoIfError3(clean, DxAmdShaderAnalyzer_addRaytracingPipeline(
				sa2, handle, i, &executables, alloc, e_rr
			));

		*result = executables;
		executables = (ListPipelineExecutable) { 0 };
		goto clean;
	}

	//The ISA buffer is queried for its size first, then filled; the per stage pointers point into it.

	U64 isaSize = 0;
	gotoIfError3(clean, dxCheck(sa->lpVtbl->GetShaderIsaCode(sa, handle, NULL, &isaSize, NULL), e_rr));

	AmdExtD3DPipelineDisassembly disassembly = (AmdExtD3DPipelineDisassembly) { 0 };

	if (isaSize) {

		gotoIfError3(clean, Buffer_createUninitializedBytes(isaSize, alloc, &isa, e_rr));

		gotoIfError3(clean, dxCheck(sa->lpVtbl->GetShaderIsaCode(
			sa, handle, (C8*) isa.ptrNonConst, &isaSize, &disassembly
		), e_rr));
	}

	//The driver reports the stages the pipeline actually holds, so a graphics pipeline without tessellation simply
	// has no hull or domain disassembly to add.

	if (type == EPipelineType_Compute) {

		AmdExtD3DComputeShaderStats stats = (AmdExtD3DComputeShaderStats) { 0 };
		gotoIfError3(clean, dxCheck(sa->lpVtbl->GetComputeShaderStats(sa, handle, &stats), e_rr));

		gotoIfError3(clean, DxAmdShaderAnalyzer_addExecutable(
			&executables, "Compute shader", disassembly.computeDisassembly, &stats.base,
			EGfxPipelineStage_Compute, alloc, e_rr
		));
	}

	else {

		const C8 *names[5] = { "Vertex shader", "Hull shader", "Domain shader", "Geometry shader", "Pixel shader" };

		const EGfxPipelineStage stages[5] = {
			EGfxPipelineStage_Vertex, EGfxPipelineStage_Hull, EGfxPipelineStage_Domain,
			EGfxPipelineStage_GeometryExt, EGfxPipelineStage_Pixel
		};

		const C8 *disassemblies[5] = {
			disassembly.vertexDisassembly, disassembly.hullDisassembly, disassembly.domainDisassembly,
			disassembly.geometryDisassembly, disassembly.pixelDisassembly
		};

		AmdExtD3DGraphicsShaderStats stats = (AmdExtD3DGraphicsShaderStats) { 0 };
		gotoIfError3(clean, dxCheck(sa->lpVtbl->GetGraphicsShaderStats(sa, handle, &stats), e_rr));

		const AmdExtD3DShaderStats *perStage[5] = {
			&stats.vertexShaderStats, &stats.hullShaderStats, &stats.domainShaderStats,
			&stats.geometryShaderStats, &stats.pixelShaderStats
		};

		for(U8 i = 0; i < 5; ++i)
			gotoIfError3(clean, DxAmdShaderAnalyzer_addExecutable(
				&executables, names[i], disassemblies[i], perStage[i], stages[i], alloc, e_rr
			));
	}

	*result = executables;
	executables = (ListPipelineExecutable) { 0 };

clean:
	ListPipelineExecutable_freeUnderlying(&executables, alloc);
	Buffer_free(&isa, alloc);
	return s_uccess;
}
