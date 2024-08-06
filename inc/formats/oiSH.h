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

#define OISH_SHADER_MODEL(maj, min) ((U16)((min) | ((maj) << 8)))

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

extern const C8 *ESHBinaryType_names[ESHBinaryType_Count];

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

	ESHExtension_MeshTaskTexDeriv			= 1 << 16,

	ESHExtension_Count						= 17

} ESHExtension;

extern const C8 *ESHExtension_defines[ESHExtension_Count];

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

typedef enum ESHStride {
	ESHStride_X8,			//This one is currently unused, but might be available in the future
	ESHStride_X16,
	ESHStride_X32,
	ESHStride_X64
} ESHStride;

#define ESHType_create(stride, prim, vec) (((stride) << 4) | ((prim) << 2) | (vec))

typedef enum ESHType {

	ESHType_F16		= ESHType_create(ESHStride_X16, ESHPrimitive_Float, ESHVector_N1),
	ESHType_F16x2	= ESHType_create(ESHStride_X16, ESHPrimitive_Float, ESHVector_N2),
	ESHType_F16x3	= ESHType_create(ESHStride_X16, ESHPrimitive_Float, ESHVector_N3),
	ESHType_F16x4	= ESHType_create(ESHStride_X16, ESHPrimitive_Float, ESHVector_N4),

	ESHType_I16		= ESHType_create(ESHStride_X16, ESHPrimitive_Int,   ESHVector_N1),
	ESHType_I16x2	= ESHType_create(ESHStride_X16, ESHPrimitive_Int,   ESHVector_N2),
	ESHType_I16x3	= ESHType_create(ESHStride_X16, ESHPrimitive_Int,   ESHVector_N3),
	ESHType_I16x4	= ESHType_create(ESHStride_X16, ESHPrimitive_Int,   ESHVector_N4),

	ESHType_U16		= ESHType_create(ESHStride_X16, ESHPrimitive_UInt,  ESHVector_N1),
	ESHType_U16x2	= ESHType_create(ESHStride_X16, ESHPrimitive_UInt,  ESHVector_N2),
	ESHType_U16x3	= ESHType_create(ESHStride_X16, ESHPrimitive_UInt,  ESHVector_N3),
	ESHType_U16x4	= ESHType_create(ESHStride_X16, ESHPrimitive_UInt,  ESHVector_N4),

	ESHType_F32		= ESHType_create(ESHStride_X32, ESHPrimitive_Float, ESHVector_N1),
	ESHType_F32x2	= ESHType_create(ESHStride_X32, ESHPrimitive_Float, ESHVector_N2),
	ESHType_F32x3	= ESHType_create(ESHStride_X32, ESHPrimitive_Float, ESHVector_N3),
	ESHType_F32x4	= ESHType_create(ESHStride_X32, ESHPrimitive_Float, ESHVector_N4),

	ESHType_I32		= ESHType_create(ESHStride_X32, ESHPrimitive_Int,   ESHVector_N1),
	ESHType_I32x2	= ESHType_create(ESHStride_X32, ESHPrimitive_Int,   ESHVector_N2),
	ESHType_I32x3	= ESHType_create(ESHStride_X32, ESHPrimitive_Int,   ESHVector_N3),
	ESHType_I32x4	= ESHType_create(ESHStride_X32, ESHPrimitive_Int,   ESHVector_N4),

	ESHType_U32		= ESHType_create(ESHStride_X32, ESHPrimitive_UInt,  ESHVector_N1),
	ESHType_U32x2	= ESHType_create(ESHStride_X32, ESHPrimitive_UInt,  ESHVector_N2),
	ESHType_U32x3	= ESHType_create(ESHStride_X32, ESHPrimitive_UInt,  ESHVector_N3),
	ESHType_U32x4	= ESHType_create(ESHStride_X32, ESHPrimitive_UInt,  ESHVector_N4),

	ESHType_F64		= ESHType_create(ESHStride_X64, ESHPrimitive_Float, ESHVector_N1),
	ESHType_F64x2	= ESHType_create(ESHStride_X64, ESHPrimitive_Float, ESHVector_N2),
	ESHType_F64x3	= ESHType_create(ESHStride_X64, ESHPrimitive_Float, ESHVector_N3),
	ESHType_F64x4	= ESHType_create(ESHStride_X64, ESHPrimitive_Float, ESHVector_N4),

	ESHType_I64		= ESHType_create(ESHStride_X64, ESHPrimitive_Int,   ESHVector_N1),
	ESHType_I64x2	= ESHType_create(ESHStride_X64, ESHPrimitive_Int,   ESHVector_N2),
	ESHType_I64x3	= ESHType_create(ESHStride_X64, ESHPrimitive_Int,   ESHVector_N3),
	ESHType_I64x4	= ESHType_create(ESHStride_X64, ESHPrimitive_Int,   ESHVector_N4),

	ESHType_U64		= ESHType_create(ESHStride_X64, ESHPrimitive_UInt,  ESHVector_N1),
	ESHType_U64x2	= ESHType_create(ESHStride_X64, ESHPrimitive_UInt,  ESHVector_N2),
	ESHType_U64x3	= ESHType_create(ESHStride_X64, ESHPrimitive_UInt,  ESHVector_N3),
	ESHType_U64x4	= ESHType_create(ESHStride_X64, ESHPrimitive_UInt,  ESHVector_N4)

} ESHType;

ESHStride ESHType_getStride(ESHType type);
ESHVector ESHType_getVector(ESHType type);
ESHPrimitive ESHType_getPrimitive(ESHType type);

const C8 *ESHType_name(ESHType type);

const C8 *ESHPipelineStage_getStagePrefix(ESHPipelineStage stage);

//Deserialized SHEntry (in oiSH file)
//Though SHEntry on disk is more compact, with only the relevant data.

typedef struct SHEntry {

	CharString name;

	ListU16 binaryIds;		//Reference SHBinaryInfo

	//Don't change order, compares use this (U32, U64[3])

	U8 stage;				//ESHPipelineStage
	U8 padding;
	U16 waveSize;			//U4[4] requiredSize, minSize, maxSize, preferSize: each U4 is in range [0, 9]. 0 = 0, 3 = 8, etc.

	U16 groupX, groupY;		//Present for compute, workgraph, task and mesh shaders

	U16 groupZ;
	U8 intersectionSize, payloadSize;		//Raytracing payload sizes

	//Verification for linking and PSO compatibility (graphics only)

	union {
		U8 inputs[16];		//ESHType
		U64 inputsU64[2];
	};

	union {
		U8 outputs[16];		//ESHType
		U64 outputsU64[2];
	};

} SHEntry;

//Runtime SHEntry with some extra information that is used to decide how to compile
//This is how the SHEntry is found in the shader. Afterwards, it is transformed into binaries.
//Then the SHEntry will point to the binaries instead to save space.

typedef struct SHEntryRuntime {

	SHEntry entry;

	U16 vendorMask;
	Bool isShaderAnnotation;			//Switches [shader("string")] and [stage("string")], the first indicates StateObject
	Bool isInitialized;

	U32 padding;

	ListU32 extensions;					//Explicitly enabled extensions (ESHExtension[])

	ListU16 shaderVersions;				//U16: U8 major, minor;		If not defined will default.

	ListCharString uniformNameValues;	//[uniformName, uniformValue][]
	ListU8 uniformsPerCompilation;		//How many uniforms are relevant for each compilation

} SHEntryRuntime;

typedef struct SHBinaryIdentifier SHBinaryIdentifier;
typedef struct SHBinaryInfo SHBinaryInfo;

U32 SHEntryRuntime_getCombinations(SHEntryRuntime runtime);

Bool SHEntryRuntime_asBinaryInfo(
	SHEntryRuntime runtime,
	U16 combinationId,
	ESHBinaryType binaryType,
	Buffer buf,
	SHBinaryInfo *binaryInfo,
	Error *e_rr
);

Bool SHEntryRuntime_asBinaryIdentifier(
	SHEntryRuntime runtime, U16 combinationId, SHBinaryIdentifier *binaryIdentifier, Error *e_rr
);

typedef struct SHBinaryIdentifier {

	CharString entrypoint;		//If it's not a lib, this defines the entrypoint to compile with
	ListCharString uniforms;	//[uniformName, uniformValue][]

	//Don't change order, is used for compare (U64)

	ESHExtension extensions;
	U16 shaderVersion;			//U8 maj, minor
	U16 stageType;				//ESHPipelineStage

} SHBinaryIdentifier;

typedef struct SHBinaryInfo {

	SHBinaryIdentifier identifier;

	U16 vendorMask;
	Bool hasShaderAnnotation;	//If [shader("")] is used rather than [stage("")]
	U8 padding[5];

	Buffer binaries[ESHBinaryType_Count];

} SHBinaryInfo;

typedef struct SHInclude {

	CharString relativePath;	//Path relative to oiSH source's directory (e.g. ../Includes/myInclude.hlsl)

	U32 crc32c;					//Content CRC32C. However, if it contains \\r it's removed first!
	U32 padding;

} SHInclude;

TList(SHEntry);
TList(SHEntryRuntime);
TList(SHBinaryIdentifier);
TList(SHBinaryInfo);
TList(SHInclude);

Bool SHBinaryIdentifier_equals(SHBinaryIdentifier a, SHBinaryIdentifier b);

void SHEntry_print(SHEntry entry, Allocator alloc);
void SHBinaryInfo_print(SHBinaryInfo binary, Allocator alloc);
void SHEntryRuntime_print(SHEntryRuntime entry, Allocator alloc);

void SHBinaryIdentifier_free(SHBinaryIdentifier *identifier, Allocator alloc);
void SHBinaryInfo_free(SHBinaryInfo *info, Allocator alloc);
void SHEntry_free(SHEntry *entry, Allocator alloc);
void SHInclude_free(SHInclude *include, Allocator alloc);

void SHEntryRuntime_free(SHEntryRuntime *entry, Allocator alloc);
void ListSHEntryRuntime_freeUnderlying(ListSHEntryRuntime *entry, Allocator alloc);
void ListSHInclude_freeUnderlying(ListSHInclude *includes, Allocator alloc);

const C8 *SHEntry_stageName(SHEntry entry);

typedef struct SHFile {

	ListSHBinaryInfo binaries;
	ListSHEntry entries;
	ListSHInclude includes;

	U64 readLength;				//How many bytes were read for this file

	ESHSettingsFlags flags;

	U32 compilerVersion;		//OxC3 compiler version

	U32 sourceHash;				//CRC32C of sources

} SHFile;

Bool SHFile_create(
	ESHSettingsFlags flags,
	U32 compilerVersion,
	U32 sourceHash,
	Allocator alloc,
	SHFile *shFile,
	Error *e_rr
);

void SHFile_free(SHFile *shFile, Allocator alloc);

Bool SHFile_addBinaries(
	SHFile *shFile,
	SHBinaryInfo *binaries,				//Moves binaries
	Allocator alloc,
	Error *e_rr
);

Bool SHFile_addEntrypoint(SHFile *shFile, SHEntry *entry, Allocator alloc, Error *e_rr);	//Moves entry->name and binaryIds
Bool SHFile_addInclude(SHFile *shFile, SHInclude *include, Allocator alloc, Error *e_rr);	//Moves include->name

Bool SHFile_write(SHFile shFile, Allocator alloc, Buffer *result, Error *e_rr);
Bool SHFile_read(Buffer file, Bool isSubFile, Allocator alloc, SHFile *shFile, Error *e_rr);

Bool SHFile_combine(SHFile a, SHFile b, Allocator alloc, SHFile *combined, Error *e_rr);

//File headers

//File spec (docs/oiSH.md)

typedef struct SHHeader {

	U32 compilerVersion;

	U32 hash;					//CRC32C of contents starting at SHHeader::version

	U32 sourceHash;				//CRC32C of source(s), for determining if it's dirty

	U16 uniqueUniforms;
	U8 version;					//major.minor (%10 = minor, /10 = major (+1 to get real major)) at least 1
	U8 sizeTypes;				//Every 2 bits size type of spirv, dxil

	U16 binaryCount;			//How many unique binaries are stored
	U16 stageCount;				//How many stages reference into the binaries

	U16 includeFileCount;		//Number of (recursive) include files
	U16 padding;

} SHHeader;

#define SHHeader_MAGIC 0x4853696F

#ifdef __cplusplus
	}
#endif
