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
#include "types/container/string.h"

#ifndef DISALLOW_SH_OXC3_PLATFORMS
	#include "platforms/ext/listx.h"
#endif

#ifdef __cplusplus
	extern "C" {
#endif

typedef enum ESHPipelineStage {

	ESHPipelineStage_Vertex,
	ESHPipelineStage_Pixel,
	ESHPipelineStage_Compute,
	ESHPipelineStage_GeometryExt,		//GeometryShader extension is required
	ESHPipelineStage_Hull,
	ESHPipelineStage_Domain,

	//RayPipeline extension is required

	ESHPipelineStage_RaygenExt,
	ESHPipelineStage_CallableExt,
	ESHPipelineStage_MissExt,
	ESHPipelineStage_ClosestHitExt,
	ESHPipelineStage_AnyHitExt,
	ESHPipelineStage_IntersectionExt,

	ESHPipelineStage_RtStartExt = ESHPipelineStage_RaygenExt,
	ESHPipelineStage_RtEndExt = ESHPipelineStage_IntersectionExt,

	//MeshShader extension is required

	ESHPipelineStage_MeshExt,
	ESHPipelineStage_TaskExt,

	//WorkGraph extension is required

	ESHPipelineStage_WorkgraphExt,

	ESHPipelineStage_Count

} ESHPipelineStage;

const C8 *ESHPipelineStage_getStagePrefix(ESHPipelineStage stage);

//Deserialized SHEntry (in oiSH file)
//Though SHEntry on disk is more compact, with only the relevant data.

typedef struct SHEntry {

	CharString name;

	ListU16 binaryIds;					//Reference SHBinaryInfo

	//Don't change order

	U8 stage;							//ESHPipelineStage
	U8 uniqueInputSemantics;
	U16 waveSize;						//U4[4] req, min, max, preferSize: each U4 is in range [0, 9]. 0 = 0, 3 = 8, etc.

	U16 groupX, groupY;					//Present for compute, workgraph, task and mesh shaders

	U16 groupZ;
	U8 intersectionSize, payloadSize;	//Raytracing payload sizes

	U32 padding;

	//Verification for linking and PSO compatibility (graphics only)

	union {
		U64 inputsU64[2];
		U8 inputs[16];					//ESBType, but ESBMatrix_N1
	};

	union {
		U64 outputsU64[2];
		U8 outputs[16];					//ESBType, but ESBMatrix_N1
	};

	union {
		U64 inputSemanticNamesU64[2];
		U8 inputSemanticNames[16];		//(U4 semanticId, semanticName)[]
	};

	union {
		U64 outputSemanticNamesU64[2];
		U8 outputSemanticNames[16];		//(U4 semanticId, semanticName)[]
	};

	ListCharString semanticNames;		//Unique semantics; outputs start at [uniqueInputSemantics], inputs at 0

} SHEntry;

extern const C8 *SHEntry_stageNames[ESHPipelineStage_Count];

//Runtime SHEntry with some extra information that is used to decide how to compile
//This is how the SHEntry is found in the shader. Afterwards, it is transformed into binaries.
//Then the SHEntry will point to the binaries instead to save space.

typedef struct SHEntryRuntime {

	SHEntry entry;

	U16 vendorMask;
	Bool isShaderAnnotation;			//Switches [shader("string")] and [[oxc::stage("string")]], shader = StateObject
	Bool isInitialized;

	ListU32 extensions;					//Explicitly enabled extensions (ESHExtension[])

	ListU16 shaderVersions;				//U16: U8 major, minor;		If not defined will default.

	ListCharString uniformNameValues;	//[uniformName, uniformValue][]
	ListU8 uniformsPerCompilation;		//How many uniforms are relevant for each compilation

} SHEntryRuntime;

typedef struct SHBinaryIdentifier SHBinaryIdentifier;
typedef struct SHBinaryInfo SHBinaryInfo;
typedef enum ESHBinaryType ESHBinaryType;

U32 SHEntryRuntime_getCombinations(SHEntryRuntime runtime);

Bool SHEntryRuntime_asBinaryInfo(
	SHEntryRuntime runtime,
	U16 combinationId,
	ESHBinaryType binaryType,
	Buffer buf,
	ESHExtension dormantExtensions,
	SHBinaryInfo *binaryInfo,
	Error *e_rr
);

Bool SHEntryRuntime_asBinaryIdentifier(
	SHEntryRuntime runtime,
	U16 combinationId,
	SHBinaryIdentifier *binaryIdentifier,
	Error *e_rr
);

TList(SHEntry);
TList(SHEntryRuntime);

void SHEntry_print(SHEntry entry, Allocator alloc);
void SHEntryRuntime_print(SHEntryRuntime entry, Allocator alloc);
void SHEntry_free(SHEntry *entry, Allocator alloc);
const C8 *SHEntry_stageName(SHEntry entry);
void SHEntryRuntime_free(SHEntryRuntime *entry, Allocator alloc);
void ListSHEntry_freeUnderlying(ListSHEntry *entry, Allocator alloc);
void ListSHEntryRuntime_freeUnderlying(ListSHEntryRuntime *entry, Allocator alloc);

#ifndef DISALLOW_SH_OXC3_PLATFORMS
	void SHEntry_printx(SHEntry entry);
	void SHEntryRuntime_printx(SHEntryRuntime entry);
	void SHEntry_freex(SHEntry *entry);
	void SHEntryRuntime_freex(SHEntryRuntime *entry);
	void ListSHEntryRuntime_freeUnderlyingx(ListSHEntryRuntime *entry);
#endif

#ifdef __cplusplus
	}
#endif
