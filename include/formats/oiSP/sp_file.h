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

//formats/oiSP/sp_file.h

#pragma once
#include "formats/oiSP/sp_state.h"
#include "formats/oiPL/pl_file.h"
#include "formats/oiDL/dl_file.h"
#include "formats/oiSH/sh_entries.h"
#include "types/container/list.h"
#include "types/container/list_basic_types.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct RefPtr RefPtr;
typedef RefPtr StreamRef;
typedef struct SHFile SHFile;
typedef struct ListSHFile ListSHFile;

//The kind of pipeline a record describes.

typedef enum ESPPipelineType {
	ESPPipelineType_Compute,
	ESPPipelineType_Graphics,
	ESPPipelineType_Raytracing,
	ESPPipelineType_Count
} ESPPipelineType;

//How a field got its value.
//Derived (proven by reflection) and Supplied (given by the caller) are trustworthy.
//Assumed means the value was picked for the caller, which has to be disclosed, since the pipeline state changes the
// compiled ISA and an invented value produces a real but unrelated disassembly.
//Serializing this is the point of keeping it: a stored pipeline still knows which of its fields nobody chose.

//The values mirror EPLSource so a layout row's tag and a field's tag mean the same thing everywhere.

typedef enum ESPFieldSource {
	ESPFieldSource_Derived  = EPLSource_Derived,
	ESPFieldSource_Supplied = EPLSource_Supplied,
	ESPFieldSource_Assumed  = EPLSource_Assumed,
	ESPFieldSource_Count    = EPLSource_Count
} ESPFieldSource;

//Every field of a pipeline that reflection can't prove gets an id, so one vocabulary serves the specialization
// report, a state template, sweep axes and a web form.
//This mirrors PipelineGraphicsInfo and PipelineRaytracingInfo field for field; a pipeline that supplies every field
// reported for it is complete, never partial.
//Fields addressing an array (render targets, vertex buffers) carry an index alongside the id.

typedef enum ESPField {

	//Render targets and blending

	ESPField_RenderTargetFormat,        //Indexed by render target; ETextureFormatId
	ESPField_RenderTargetCount,
	ESPField_BlendEnable,
	ESPField_BlendIndependent,          //allowIndependentBlend
	ESPField_BlendTargetMask,           //renderTargetMask
	ESPField_BlendLogicOp,              //ELogicOpExt
	ESPField_BlendWriteMask,            //Indexed by render target; EWriteMask
	ESPField_BlendSrc,                  //Indexed by render target; EBlend
	ESPField_BlendDst,                  //Indexed; EBlend
	ESPField_BlendSrcAlpha,             //Indexed; EBlend
	ESPField_BlendDstAlpha,             //Indexed; EBlend
	ESPField_BlendOp,                   //Indexed; EBlendOp
	ESPField_BlendOpAlpha,              //Indexed; EBlendOp

	//Depth and stencil

	ESPField_DepthFormat,               //EDepthStencilFormat
	ESPField_DepthStencilFlags,         //EDepthStencilFlags
	ESPField_DepthCompare,              //ECompareOp
	ESPField_StencilCompare,            //ECompareOp
	ESPField_StencilFail,               //EStencilOp
	ESPField_StencilPass,               //EStencilOp
	ESPField_StencilDepthFail,          //EStencilOp
	ESPField_StencilWriteMask,
	ESPField_StencilReadMask,

	//Rasterizer

	ESPField_CullMode,                  //ECullMode
	ESPField_RasterizerFlags,           //ERasterizerFlags
	ESPField_DepthBiasConstant,         //I32 stored in the U32
	ESPField_DepthBiasClamp,            //F32 bits stored in the U32
	ESPField_DepthBiasSlope,            //F32 bits stored in the U32

	//Multisampling, topology, tessellation

	ESPField_Msaa,                      //EMSAASamples
	ESPField_MsaaMinSampleShading,      //F32 bits stored in the U32
	ESPField_TopologyMode,              //ETopologyMode
	ESPField_PatchControlPoints,

	//Vertex input

	ESPField_VertexBufferStride,        //Indexed by vertex buffer
	ESPField_VertexBufferRate,          //Indexed by vertex buffer

	//Ray tracing

	ESPField_MaxRecursionDepth,
	ESPField_RaytracingFlags,           //EPipelineRaytracingFlags

	//Descriptor layout (any pipeline kind); indexed fields index the pipeline layout's own binding/sampler rows.
	//These only exist when the pipeline carries a layout (layoutIndex != U32_MAX): the device's default layout
	//isn't described by the file, so there is nothing there to address.

	ESPField_LayoutBindingType,         //EGfxRegisterType plus its mask bits
	ESPField_LayoutBindingCount,
	ESPField_LayoutBindingSpaceSpirv,   //U32_MAX means the register doesn't exist for that api
	ESPField_LayoutBindingRegisterSpirv,
	ESPField_LayoutBindingSpaceDxil,
	ESPField_LayoutBindingRegisterDxil,
	ESPField_LayoutBindingVisibility,   //Bit mask of EGfxPipelineStage
	ESPField_LayoutBindingData,         //The class typed slot: stride, cbuffer size, texture format or sampler id

	ESPField_LayoutPushConstantSize,
	ESPField_LayoutPushConstantVisibility,

	ESPField_LayoutSamplerFilter,       //ESamplerFilterMode
	ESPField_LayoutSamplerAddressU,     //ESamplerAddressMode
	ESPField_LayoutSamplerAddressV,
	ESPField_LayoutSamplerAddressW,
	ESPField_LayoutSamplerAniso,
	ESPField_LayoutSamplerBorder,       //ESamplerBorderColor
	ESPField_LayoutSamplerCompareOp,    //ECompareOp
	ESPField_LayoutSamplerCompareEnable,
	ESPField_LayoutSamplerMipBias,      //F16 bits in the low half
	ESPField_LayoutSamplerMinLod,       //F16 bits in the low half
	ESPField_LayoutSamplerMaxLod,       //F16 bits in the low half

	ESPField_Count

} ESPField;

//Stable path name of a field, e.g. "rtv.format", used by the report, a template and the sweep syntax.

const C8 *ESPField_name(ESPField field);
Bool ESPField_isIndexed(ESPField field);

//How many indices an indexed field has (8 per render target, 16 per vertex buffer), 1 for a field without any.

U8 ESPField_indexCount(ESPField field);

//Parses the path a field is reported under ("blend.src[2]", "topology") back into the field and its index.
//Returns false for an unknown name or an index outside the field's range, so a typo can't land on another field.

Bool ESPField_parsePath(CharString path, ESPField *field, U8 *index);

//Why reflection can't prove a field, and which values are legal; both are static text keyed off the field.

const C8 *ESPField_reason(ESPField field);
const C8 *ESPField_domain(ESPField field);

//The widest value a field stores; SPFile_supply refuses anything above it rather than truncating.

U32 ESPField_maxValue(ESPField field);

//One field the caller has to specialize, carrying the value that would be used and where it came from.

typedef struct SPSpecialization {

	U8 field;                                   //ESPField
	U8 index;                                   //Sub index for indexed fields
	U8 source;                                  //ESPFieldSource
	U8 padding;

	U32 value;                                  //The value that would be used, in the field's own units

} SPSpecialization;

//One stage of a pipeline.
//A stored pipeline names the shader it came from rather than pointing into a list, so it can be resolved again after
// a load; sourceHash is the oiSH's own hash at the time, so a loader can tell when the shader moved on.

typedef struct SPStage {

	U32 shaderFile;                             //String id of the oiSH this stage came from, U32_MAX if unnamed

	U32 entrypoint;                             //String id of the entry name

	U32 sourceHash;                             //SHFile.sourceHash when the pipeline was made, 0 if unknown

	U8 stage;                                   //EGfxPipelineStage
	U8 padding[3];

} SPStage;

typedef enum ESPPipelineFlag {
	ESPPipelineFlag_None                  = 0,
	ESPPipelineFlag_GeneratedVertexStage  = 1 << 0,
	ESPPipelineFlag_GeneratedPixelStage   = 1 << 1,
	ESPPipelineFlag_AssumedHitGrouping    = 1 << 2,
	ESPPipelineFlag_Unsupported           = 0xFF << 3
} ESPPipelineFlag;

//What every pipeline has, whatever kind it is: a name, the stages it binds and the fields it had to specialize.
//Anything that walks stages, provenance or names takes this and doesn't care which kind of pipeline it got, which is
// what a template, a sweep or a form needs; the kind specific state hangs off stateIndex.

typedef struct SPPipelineBase {

	U32 name;                                   //String id, U32_MAX if unnamed

	U8 type;                                    //ESPPipelineType
	U8 flags;                                   //ESPPipelineFlag
	U8 stageStart, stageCount;                  //Range into SPFile.stages

	U32 specializationStart;                    //Range into SPFile.specializations

	U32 specializationCount;

	U32 stateIndex;                             //Into graphicsStates or raytracingStates; unused by compute

	//Into layouts, U32_MAX when the pipeline takes the device's default one.
	//The layout is part of what the shader compiles into, not just a validation gate: descriptor set pointers and
	// push constants arrive in user SGPRs and eat the root signature's budget, so two layouts that both accept a
	// shader can still produce different ISA.

	U32 layoutIndex;

} SPPipelineBase;

//How vertices are assembled: the topology they form and where each input location is fetched from.
//Formats come from the vertex stage's signature; the packing into buffers is a pipeline choice, not a signature.

typedef struct SPInputAssembler {

	U8 topologyMode;                            //ETopologyMode
	U8 padding[3];

	U32 patchControlPoints;                     //1..32 with tessellation, else 0

	SPVertexLayoutRuntime vertexLayout;

} SPInputAssembler;

//Graphics only state, composed the way a graphics pipeline itself is.

typedef struct SPGraphicsState {

	U8 msaa;                                    //EMSAASamples
	U8 renderTargetCount;                       //0..8
	U8 depthFormat;                             //EDepthStencilFormat; 0 = no depth attachment
	U8 padding;

	F32 msaaMinSampleShading;                   //0 = off, else the 0..1 sample fraction

	U8 renderTargetFormats[8];                  //ETextureFormatId

	SPBlendStateRuntime blend;
	SPDepthState depth;
	SPRasterizerState rasterizer;
	SPInputAssembler inputAssembler;

} SPGraphicsState;

//A pipeline binds more state than a file has any reason to keep, so two of these structs exist twice: once as the
// runtime form a pipeline is created from, and once as the stored form a file actually holds.
//Nothing is lost between them; what the stored form leaves out is state no pipeline can reach.

//Blend reaches an attachment only when blending is on, and then only through attachments[0] (independent blending off)
// or through the targets renderTargetMask selects (independent blending on).

typedef struct SPBlendStateStored {

	Bool enable;
	Bool allowIndependentBlend;                 //0 = every target uses attachments[0]
	U8 renderTargetMask;                        //Bit per render target
	U8 logicOpExt;                              //ELogicOpExt; replaces blending when set

	U8 writeMask[8];                            //EWriteMask

} SPBlendStateStored;

Bool SPBlendStateRuntime_storesAttachment(SPBlendStateRuntime blend, U8 renderTarget);
U8 SPBlendStateRuntime_storedAttachmentCount(SPBlendStateRuntime blend);

//A vertex layout is sparse by index rather than by count: a buffer is real once it carries a stride or an instance
// rate, an input location once it has a format, and location 3 can be filled while 0 to 2 aren't.
//So which entries a state stored travels as a mask rather than as a count.

typedef struct SPVertexLayoutStored {

	U16 bufferMask;                             //Bit per vertex buffer that carries a stride or an instance rate
	U16 attributeMask;                          //Bit per input location that carries a format

} SPVertexLayoutStored;

U16 SPVertexLayoutRuntime_bufferMask(SPVertexLayoutRuntime layout);
U16 SPVertexLayoutRuntime_attributeMask(SPVertexLayoutRuntime layout);

typedef struct SPInputAssemblerStored {

	U8 topologyMode;                            //ETopologyMode
	U8 padding[3];

	U32 patchControlPoints;                     //1..32 with tessellation, else 0

	SPVertexLayoutStored vertexLayout;

} SPInputAssemblerStored;

//Which makes the graphics state a file holds its own struct, rather than the runtime one with holes in it.
//The entries the two masks and the blend state select follow in the file's own sections, see SPFile below.

typedef struct SPGraphicsStateStored {

	U8 msaa;                                    //EMSAASamples
	U8 renderTargetCount;                       //0..8
	U8 depthFormat;                             //EDepthStencilFormat; 0 = no depth attachment
	U8 padding;

	F32 msaaMinSampleShading;                   //0 = off, else the 0..1 sample fraction

	U8 renderTargetFormats[8];                  //ETextureFormatId

	SPBlendStateStored blend;
	SPDepthState depth;
	SPRasterizerState rasterizer;
	SPInputAssemblerStored inputAssembler;

} SPGraphicsStateStored;

//Converting between the two is the whole of the difference; the variable entries are handled by the reader and writer.

SPGraphicsStateStored SPGraphicsState_store(SPGraphicsState state);
SPGraphicsState SPGraphicsStateStored_expand(SPGraphicsStateStored stored);

//These are written to disk verbatim, so their sizes are part of the format rather than an implementation detail.
//The graphics layer aliases them, which means a field added there would otherwise change the layout silently.

static_assert(sizeof(SPRasterizerState) == 16, "SPRasterizerState size is part of oiSP");
static_assert(sizeof(SPDepthState) == 8, "SPDepthState size is part of oiSP");
static_assert(sizeof(SPBlendAttachment) == 6, "SPBlendAttachment size is part of oiSP");
static_assert(sizeof(SPBlendStateRuntime) == 60, "SPBlendStateRuntime size is part of oiSP");
static_assert(sizeof(SPVertexAttribute) == 4, "SPVertexAttribute size is part of oiSP");
static_assert(sizeof(SPVertexLayoutRuntime) == 96, "SPVertexLayoutRuntime size is part of oiSP");
static_assert(sizeof(SPInputAssembler) == 104, "SPInputAssembler size is part of oiSP");
static_assert(sizeof(SPGraphicsState) == 204, "SPGraphicsState size is part of oiSP");
static_assert(sizeof(SPBlendStateStored) == 12, "SPBlendStateStored size is part of oiSP");
static_assert(sizeof(SPVertexLayoutStored) == 4, "SPVertexLayoutStored size is part of oiSP");
static_assert(sizeof(SPInputAssemblerStored) == 12, "SPInputAssemblerStored size is part of oiSP");
static_assert(sizeof(SPGraphicsStateStored) == 64, "SPGraphicsStateStored size is part of oiSP");

//Ray tracing only state.
//Compute has none, which is why a compute pipeline is just its base record.

typedef struct SPRaytracingState {

	U8 maxRecursionDepth;
	U8 raytracingFlags;                         //EPipelineRaytracingFlags
	U16 padding;

} SPRaytracingState;

TList(SPSpecialization);
TList(SPStage);
TList(SPPipelineBase);
TList(SPGraphicsState);
TList(SPRaytracingState);
TList(SPVertexLayoutStored);

typedef enum ESPSettingsFlags {
	ESPSettingsFlags_None             = 0,
	ESPSettingsFlags_HideMagicNumber  = 1 << 0,      //Set when embedded as a subfile; not hashed
	ESPSettingsFlags_CreateNoReserve  = 1 << 1,      //Only for SPFile_create: skip the initial reserve
	ESPSettingsFlags_Invalid          = 0xFFFFFFFF << 2
} ESPSettingsFlags;

//A set of pipelines with the pools they index into.
//Several pipelines share one file so a whole material or pass can be stored and loaded as a unit.

typedef struct SPFile {

	DLFile names;                //String pool: pipeline names, shader file names, entrypoint names

	ListSPPipelineBase pipelines;
	ListSPStage stages;
	ListSPSpecialization specializations;

	//Kind specific state, indexed by SPPipelineBase.stateIndex, so a compute pipeline costs its base record alone.

	ListSPGraphicsState graphicsStates;
	ListSPRaytracingState raytracingStates;

	//Every pipeline layout as its own oiPL, indexed by SPPipelineBase.layoutIndex, so a layout shared by
	// several pipelines is stored once and one entry can be lifted out or dropped in whole.

	ListPLFile layouts;

	ESPSettingsFlags flags;
	U32 padding;
	U64 hash;                    //Refreshed by SPFile_finalize

} SPFile;

TList(SPFile);

Bool SPFile_create(ESPSettingsFlags flags, const Allocator *alloc, SPFile *spFile, Error *e_rr);
void SPFile_free(SPFile *spFile, const Allocator *alloc);
void ListSPFile_freeUnderlying(ListSPFile *files, const Allocator *alloc);

//Add a string to the pool (deduplicated), returning its id.
//U32_MAX means "no string"; passing an empty/null CharString returns U32_MAX without adding.

Bool SPFile_addString(SPFile *spFile, CharString *str, const Allocator *alloc, U32 *id, Error *e_rr);

//A stage of a pipeline being derived: which file of `files` it lives in and which entry of that file.
//This is the same (file, entry) pair a pipeline is created from at runtime, so a pipeline can be described whose
// stages come from several oiSH files.

typedef struct SPStageRef {
	U16 fileId;
	U16 entryId;
} SPStageRef;

//Derives a pipeline from the given stages, filling everything reflection can prove and recording the rest as
// specializations holding the value that would be used, then appends it to the file.
//The descriptor layout is derived the same way (see SPPipelineBase.layoutIndex); excludedRegisters names the
// registers the runtime owns, and NULL means none are, which only a caller without a device should pass.
//A mix of compute, graphics and ray tracing stages in one call is an error, since that can't be one pipeline, and so
// is the same stage kind twice.
//shaderNames is optional and parallel to `files`: the name each file is stored under, so the pipeline can be resolved
// again after a load.

Bool SPFile_derivePipeline(
	SPFile *spFile,
	const ListSHFile *files,
	const ListCharString *shaderNames,        //Optional, parallel to files
	CharString name,                          //Optional pipeline name
	const SPStageRef *stages,
	U8 stageCount,
	const ListCharString *excludedRegisters,
	const Allocator *alloc,
	U32 *pipelineId,
	Error *e_rr
);

//The kind specific state of a pipeline, or NULL when it isn't that kind.
//Compute has no extra state, so a NULL from both simply means a compute pipeline.

const SPGraphicsState *SPFile_graphicsState(const SPFile *spFile, U32 pipelineId);
const SPRaytracingState *SPFile_raytracingState(const SPFile *spFile, U32 pipelineId);

SPGraphicsState *SPFile_graphicsStateMut(SPFile *spFile, U32 pipelineId);
SPRaytracingState *SPFile_raytracingStateMut(SPFile *spFile, U32 pipelineId);

//Marks a field as supplied by the caller, dropping it from the pipeline's assumed set.

Bool SPFile_supply(SPFile *spFile, U32 pipelineId, ESPField field, U8 index, U32 value, Error *e_rr);

//A pipeline is exact only when nothing was assumed; anything else describes a state nobody chose.

Bool SPFile_isExact(const SPFile *spFile, U32 pipelineId);
U64 SPFile_assumedCount(const SPFile *spFile, U32 pipelineId);

//Structural validation, which needs no device: the pipeline has to agree with the shader signatures it's built from.
//Device validation (format support, sample counts, whether the driver really compiles it) belongs to the backend.

Bool SPFile_validate(
	const SPFile *spFile,
	U32 pipelineId,
	const ListSHFile *files,
	const SPStageRef *stages,                 //Parallel to the pipeline's stages, resolving them back to entries
	const Allocator *alloc,
	ListCharString *issues,
	Error *e_rr
);

//Appends the pipeline with each field's provenance, so what produced a disassembly is never implicit.

Bool SPFile_print(
	const SPFile *spFile, U32 pipelineId, const Allocator *alloc, CharString *result, Error *e_rr
);

//Recompute the content hash; call once the pipelines are finalized.

Bool SPFile_finalize(SPFile *spFile, const Allocator *alloc, Error *e_rr);

Bool SPFile_write(
	const SPFile *spFile,
	const Allocator *alloc,
	StreamRef *streamRef,        //Pass NULL to calculate length only (*offset)
	U64 *offset,
	Error *e_rr
);

Bool SPFile_read(StreamRef *streamRef, U64 *offset, Bool isSubFile, const Allocator *alloc, SPFile *spFile, Error *e_rr);

//File headers (file spec: docs/oiSP.md)

typedef enum ESPVersion {
	ESPVersion_Undefined,
	ESPVersion_V1_1            //Current (on-disk version byte 1, displayed as major.minor 1.1)
} ESPVersion;

typedef enum ESPFlag {
	ESPFlag_None        = 0,
	ESPFlag_Unsupported = 0xFF
} ESPFlag;

typedef struct SPHeader {

	U8 version;                     //ESPVersion
	U8 flags;                       //ESPFlag
	U16 padding;

	U32 pipelineCount;

	U32 stageCount;                 //Shared pool, pipelines index ranges into it

	U32 specializationCount;        //Shared pool, pipelines index ranges into it

	U32 graphicsStateCount;         //Kind specific pools, referenced by SPPipelineBase.stateIndex

	U32 raytracingStateCount;

	U32 blendAttachmentCount;       //Only the attachments a blend state actually uses are stored

	U32 vertexBufferCount;              //Only the buffers that carry a stride or instance rate are stored

	U32 vertexAttributeCount;           //Only the input locations that carry a format are stored

	U32 layoutCount;                    //oiPL subfiles, referenced by SPPipelineBase.layoutIndex

} SPHeader;

#define SPHeader_MAGIC 0x5053696F

#ifdef __cplusplus
	}
#endif
