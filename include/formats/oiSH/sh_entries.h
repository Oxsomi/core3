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

#pragma once
#include "formats/oiSH/sh_binaries.h"

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

typedef U8 SHPipelineStage;

const C8 *ESHPipelineStage_getStagePrefix(ESHPipelineStage stage);

//Deserialized SHEntry (in oiSH file)
//Though SHEntry on disk is more compact, with only the relevant data.

typedef struct SHEntry {

	CharString name;

	ListU16 binaryIds;							//Reference SHBinaryInfo

	ListCharString semanticNames;				//Unique semantics; outputs start at [uniqueInputSemantics], inputs at 0

	union {

		U64 equal[1];							//This compare can be done in addition to compatible to know exact equality

		struct {
			U16 waveSize;						//U4[4] req, min, max, preferSize: each U4 is in range [0, 9]
			U16 reserved0[3];					//Only use if property that might need unique comparison
		};
	};

	union {

		U64 compatible[10];						//This compare can be done to know if they're compatible

		struct {

			U16 groupX, groupY;					//Present for compute, workgraph, task and mesh shaders

			U16 groupZ;
			U8 intersectionSize, payloadSize;	//Raytracing payload sizes

			SHPipelineStage stage;
			U8 uniqueInputSemantics;
			U16 idOrPadding;					//Sometimes is used to store id in an array

			U32 reserved1;

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
		};
	};

} SHEntry;

extern const C8 *SHEntry_stageNames[ESHPipelineStage_Count];

typedef struct SHEntryRuntime {

	SHEntry entry;

	U16 vendorMask;
	Bool isShaderAnnotation;			//Switches [shader("string")] and [[oxc::stage("string")]]
	U8 isInitializedFlags;				//1 = init, 2 = init oxc::uniforms

	Bool isRt;
	Bool containsGfxOrComp;
	U16 uniformStride;					//How many bytes all uniforms combined take

	ListU32 extensions;					//Explicitly enabled extensions (ESHExtension[])

	ListU16 shaderVersions;				//U16: U8 major, minor;		If not defined will default.

	ListCharString defineNameValues;	//[defineName, defineValue][]
	ListU8 definesPerCompilation;		//How many defines are relevant for each compilation

	ListSHUniformRuntime uniforms;		//Uniforms used in the compilation unit; offset indexes into uniformData
	ListU8 uniformData;					//(U8[uniformStride])[uniforms.length]

} SHEntryRuntime;

typedef struct SHBinaryIdentifier SHBinaryIdentifier;
typedef struct SHBinaryInfo SHBinaryInfo;
typedef enum ESHBinaryType ESHBinaryType;
typedef enum ESHExtension ESHExtension;

U32 SHEntryRuntime_getCombinations(const SHEntryRuntime *runtime);				//How many binaries are stored (compile + link)
U32 SHEntryRuntime_getCombinationsCompiled(const SHEntryRuntime *runtime);		//How many binaries are compiled

Bool SHEntryRuntime_asBinaryInfo(
	const SHEntryRuntime *runtime,
	const SHBinaryIdentifier *identifier,
	ESHBinaryType binaryType,
	Buffer buf,
	ESHExtension dormantExtensions,
	SHBinaryInfo *binaryInfo,
	Error *e_rr
);

Bool SHEntryRuntime_asBinaryIdentifier(
	const SHEntryRuntime *runtime,
	U16 combinationId,
	SHBinaryIdentifier *binaryIdentifier,
	Error *e_rr
);

TList(SHEntry);
TList(SHEntryRuntime);

Bool SHEntry_equals(const SHEntry *entryA, const SHEntry *entryB);

void SHEntry_print(const SHEntry *entry, Bool isVerbose, const Allocator *alloc);
void SHEntryRuntime_print(const SHEntryRuntime *entry, const Allocator *alloc);

const C8 *SHEntry_stageName(const SHEntry *entry);

void SHEntry_free(SHEntry *entry, const Allocator *alloc);
void SHEntryRuntime_free(SHEntryRuntime *entry, const Allocator *alloc);
void ListSHEntry_freeUnderlying(ListSHEntry *entry, const Allocator *alloc);
void ListSHEntryRuntime_freeUnderlying(ListSHEntryRuntime *entry, const Allocator *alloc);

#ifdef __cplusplus
	}
#endif
