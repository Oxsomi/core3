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

#pragma once
#include "oiXX.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef enum ESHSettingsFlags {
	ESHSettingsFlags_None				= 0,
	ESHSettingsFlags_HideMagicNumber	= 1 << 0,		//Only valid if the oiSH can be 100% confidently detected otherwise
	ESHSettingsFlags_IsUTF8				= 1 << 1,		//If one of the entrypoint strings was UTF8
	ESHSettingsFlags_Invalid			= 0xFFFFFFFF << 2
} ESHSettingsFlags;

//Check docs/oiSH.md for the file spec

typedef enum ESHBinaryType {

	ESHBinaryType_SPIRV,
	ESHBinaryType_DXIL,

	//ESHBinaryType_MSL,
	//ESHBinaryType_WGSL,

	ESHBinaryType_Count

} ESHBinaryType;

typedef enum ESHExtension {

	ESHExtension_None						= 0,

	//These losely map to EGraphicsDataTypes in OxC3 graphics

	ESHExtension_F64						= 1 << 0,
	ESHExtension_I64						= 1 << 1,
	ESHExtension_16BitTypes					= 1 << 2,		//I16, F16

	ESHExtension_AtomicI64					= 1 << 3,
	ESHExtension_AtomicF32					= 1 << 4,
	ESHExtension_AtomicF64					= 1 << 5,

	//Some of them are present in EGraphicsFeatures in OxC3 graphics

	ESHExtension_SubgroupArithmetic			= 1 << 6,
	ESHExtension_SubgroupShuffle			= 1 << 7,

	ESHExtension_RayQuery					= 1 << 8,
	ESHExtension_RayMicromapOpacity			= 1 << 9,
	ESHExtension_RayMicromapDisplacement	= 1 << 10,
	ESHExtension_RayMotionBlur				= 1 << 11,
	ESHExtension_RayReorder					= 1 << 12,

	ESHExtension_Multiview					= 1 << 13,
	ESHExtension_ComputeDeriv				= 1 << 14,

	ESHExtension_PAQ						= 1 << 15,		//Payload access qualifiers

	ESHExtension_Count						= 16

} ESHExtension;

typedef enum ESHVendor {
	ESHVendor_NV,
	ESHVendor_AMD,
	ESHVendor_ARM,
	ESHVendor_QCOM,
	ESHVendor_INTC,
	ESHVendor_IMGT,
	ESHVendor_MSFT,
	ESHVendor_Count
} ESHVendor;

typedef enum ESHPipelineStage {

	ESHPipelineStage_Vertex,
	ESHPipelineStage_Pixel,
	ESHPipelineStage_Compute,
	ESHPipelineStage_GeometryExt,		//GeometryShader extension is required
	ESHPipelineStage_Hull,
	ESHPipelineStage_Domain,

	//MeshShader extension is required

	ESHPipelineStage_MeshExt,
	ESHPipelineStage_TaskExt,

	//WorkGraph extension is required

	ESHPipelineStage_WorkgraphExt,

	//RayPipeline extension is required

	ESHPipelineStage_RaygenExt,
	ESHPipelineStage_CallableExt,
	ESHPipelineStage_MissExt,
	ESHPipelineStage_ClosestHitExt,
	ESHPipelineStage_AnyHitExt,
	ESHPipelineStage_IntersectionExt,

	ESHPipelineStage_RtStartExt = ESHPipelineStage_RaygenExt,
	ESHPipelineStage_RtEndExt = ESHPipelineStage_IntersectionExt,

	ESHPipelineStage_Count

} ESHPipelineStage;

typedef enum ESHPrimitive {
	ESHPrimitive_Invalid,
	ESHPrimitive_Float,		//Float, unorm, snorm
	ESHPrimitive_Int,
	ESHPrimitive_UInt
} ESHPrimitive;

typedef enum ESHVector {
	ESHVector_N1,
	ESHVector_N2,
	ESHVector_N3,
	ESHVector_N4
} ESHVector;

#define ESHType_create(prim, vec) (((prim) << 2) | (vec))

typedef enum ESHType {

	ESHType_F32		= ESHType_create(ESHPrimitive_Float, ESHVector_N1),
	ESHType_F32x2	= ESHType_create(ESHPrimitive_Float, ESHVector_N2),
	ESHType_F32x3	= ESHType_create(ESHPrimitive_Float, ESHVector_N3),
	ESHType_F32x4	= ESHType_create(ESHPrimitive_Float, ESHVector_N4),

	ESHType_I32		= ESHType_create(ESHPrimitive_Int, ESHVector_N1),
	ESHType_I32x2	= ESHType_create(ESHPrimitive_Int, ESHVector_N2),
	ESHType_I32x3	= ESHType_create(ESHPrimitive_Int, ESHVector_N3),
	ESHType_I32x4	= ESHType_create(ESHPrimitive_Int, ESHVector_N4),

	ESHType_U32		= ESHType_create(ESHPrimitive_UInt, ESHVector_N1),
	ESHType_U32x2	= ESHType_create(ESHPrimitive_UInt, ESHVector_N2),
	ESHType_U32x3	= ESHType_create(ESHPrimitive_UInt, ESHVector_N3),
	ESHType_U32x4	= ESHType_create(ESHPrimitive_UInt, ESHVector_N4)

} ESHType;

const C8 *ESHType_name(ESHType type);

//Serialized SHEntry (in oiSH file)

typedef struct SHEntry {

	CharString name;

	U16 stage;			//ESHPipelineStage
	U16 waveSize;		//U4[3] recommendedSize, minSize, maxSize: each U4 is in range [0, 9]. 0 = 0, 1 = 1, 3 = 8, etc.

	U16 groupX, groupY;

	U16 groupZ;
	U8 intersectionSize, payloadSize;		//Raytracing payload sizes

	//Verification for linking and PSO compatibility (graphics only)

	union {
		U8 inputs[8];		//ESHType[2] per U8
		U64 inputsU64;
	};

	union {
		U8 outputs[8];		//ESHType[2] per U8
		U64 outputsU64;
	};

	U16 vendorMask;
	U16 padding0;

	U32 padding1;

} SHEntry;

//Runtime SHEntry with some extra information that is used to decide how to compile

typedef struct SHEntryRuntime {

	SHEntry entry;

	Bool isShaderAnnotation;			//Switches [shader("string")] and [stage("string")], the first indicates StateObject
	U8 padding[7];

	ListU32 extensions;					//Explicitly enabled extensions (ESHExtension[])

	ListU16 shaderVersions;				//U16: U8 major, minor;		If not defined will default.

	ListCharString uniforms;
	ListCharString uniformValues;
	ListU8 uniformsPerCompilation;		//How many uniforms are relevant for each compilation

} SHEntryRuntime;

typedef struct SHBinaryInfo {

	CharString entrypoint;		//If it's not a lib, this defines the entrypoint to compile with

	ESHExtension extensions;
	U16 shaderVersion;
	U8 uniforms;
	U8 stageType;				//ESHPipelineStage

	Bool hasShaderAnnotation;	//If [shader("")] is used rather than [stage("")]
	U8 padding[7];

	CharString *uniformNames, *uniformValues;

} SHBinaryInfo;

TList(SHEntry);
TList(SHEntryRuntime);

void SHEntry_print(SHEntry entry, Allocator alloc);
void SHEntryRuntime_print(SHEntryRuntime entry, Allocator alloc);

void SHEntryRuntime_free(SHEntryRuntime *entry, Allocator alloc);
void ListSHEntryRuntime_freeUnderlying(ListSHEntryRuntime *entry, Allocator alloc);

const C8 *SHEntry_stageName(SHEntry entry);

typedef enum ESHPipelineType {
	ESHPipelineType_Graphics,
	ESHPipelineType_Compute,
	ESHPipelineType_Raytracing,
	ESHPipelineType_Workgraph,
	ESHPipelineType_Count
} ESHPipelineType;

typedef struct SHFile {

	Buffer binaries[ESHBinaryType_Count];

	ListSHEntry entries;

	U64 readLength;				//How many bytes were read for this file

	ESHSettingsFlags flags;
	ESHExtension extensions;

	U32 compilerVersion;		//OxC3 compiler version

} SHFile;

Error SHFile_create(ESHSettingsFlags flags, ESHExtension extension, U32 shaderVersion, Allocator alloc, SHFile *shFile);
Bool SHFile_free(SHFile *shFile, Allocator alloc);

Error SHFile_addBinary(SHFile *shFile, ESHBinaryType type, Buffer *entry, Allocator alloc);		//Moves entry
Error SHFile_addEntrypoint(SHFile *shFile, SHEntry *entry, Allocator alloc);					//Moves entry->name

Error SHFile_write(SHFile shFile, Allocator alloc, Buffer *result);
Error SHFile_read(Buffer file, Bool isSubFile, Allocator alloc, SHFile *shFile);

//File headers

//File spec (docs/oiSH.md)

typedef enum ESHFlags {

	ESHFlags_None 					= 0,

	//What type of binaries it includes
	//Must be one at least

	ESHFlags_HasSPIRV				= 1 << 0,
	ESHFlags_HasDXIL				= 1 << 1,

	//Reserved
	//ESHFlags_HasMSL				= 1 << 2,
	//ESHFlags_HasWGSL				= 1 << 3

	ESHFlags_Invalid				= 0xFF << 2,

	ESHFlags_HasBinary				= ESHFlags_HasSPIRV | ESHFlags_HasDXIL,
	//ESHFlags_HasText				= ESHFlags_HasMSL | ESHFlags_HasWGSL
	ESHFlags_HasSource				= ESHFlags_HasBinary // | ESHFlags_HasText

} ESHFlags;

typedef struct SHHeader {

	U8 version;					//major.minor (%10 = minor, /10 = major (+1 to get real major)) at least 1
	U8 flags;					//ESHFlags
	U8 sizeTypes;				//EXXDataSizeTypes: spirvType | (dxilType << 2) | (mslType << 4) | (wgslType << 6)
	U8 pipelineType;

	ESHExtension extensions;

	U32 compilerVersion;

} SHHeader;

#define SHHeader_MAGIC 0x4853696F

#ifdef __cplusplus
	}
#endif
