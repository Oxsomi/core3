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

//formats/oiSP/sp_file.c

#include "formats/oiSP/sp_file.h"
#include "types/base/string_read.h"
#include "types/base/string_read_helper.h"
#include "types/math/type_cast.h"
#include "formats/oiSH/sh_file.h"
#include "formats/oiSH/sh_registers.h"
#include "formats/oiDL/dl_entry.h"
#include "formats/oiDL/dl_load.h"
#include "formats/oiSB/sb_variable.h"
#include "types/base/constants.h"
#include "types/container/texture_format.h"
#include "types/container/list_impl.h"
#include "types/container/string.h"
#include "types/container/log.h"
#include "types/container/buffer.h"
#include "types/base/error.h"
#include "types/base/allocator.h"

TListImpl(SPSpecialization);
TListImpl(SPStage);
TListImpl(SPPipelineBase);
TListImpl(SPVertexLayoutStored);
TListImpl(SPGraphicsState);
TListImpl(SPRaytracingState);
TListImpl(SPFile);

//One row per field: its path name, whether it's indexed, why reflection can't prove it and what's legal.
//Kept as one table so the report, a template and the sweep syntax can never disagree about a field.

typedef struct SPFieldInfo {
	const C8 *name;
	Bool indexed;
	const C8 *reason;
	const C8 *domain;
} SPFieldInfo;

static const SPFieldInfo SPField_info[ESPField_Count] = {

	{ "rtv.format", true,
		"the signature proves the primitive written, never the target's storage format", "any color format" },
	{ "rtv.count", false,
		"the signature proves how many targets are written, and more may be bound", "0..8" },
	{ "blend.enable", false, "blending is pipeline state a shader can't declare", "0 = off, 1 = on" },
	{ "blend.independent", false,
		"per target blend state is a pipeline choice", "0 = all targets share [0], 1 = per target" },
	{ "blend.targetMask", false, "which targets blend is a pipeline choice", "bit per render target" },
	{ "blend.logicOp", false, "a logic op replaces blending and is pipeline state", "ELogicOpExt" },
	{ "blend.writeMask", true, "which channels a target keeps is pipeline state", "EWriteMask bits" },
	{ "blend.src", true, "a blend factor is pipeline state", "EBlend" },
	{ "blend.dst", true, "a blend factor is pipeline state", "EBlend" },
	{ "blend.srcAlpha", true, "a blend factor is pipeline state", "EBlend" },
	{ "blend.dstAlpha", true, "a blend factor is pipeline state", "EBlend" },
	{ "blend.op", true, "a blend op is pipeline state", "EBlendOp" },
	{ "blend.opAlpha", true, "a blend op is pipeline state", "EBlendOp" },

	{ "depth.format", false,
		"the depth attachment is pipeline state a shader can't declare", "0 = none, else a depth format" },
	{ "depth.flags", false, "depth test, write and stencil enable are pipeline state", "EDepthStencilFlags bits" },
	{ "depth.compare", false, "the depth compare changes early vs late depth testing", "ECompareOp" },
	{ "stencil.compare", false, "stencil compare is pipeline state", "ECompareOp" },
	{ "stencil.fail", false, "a stencil op is pipeline state", "EStencilOp" },
	{ "stencil.pass", false, "a stencil op is pipeline state", "EStencilOp" },
	{ "stencil.depthFail", false, "a stencil op is pipeline state", "EStencilOp" },
	{ "stencil.writeMask", false, "a stencil mask is pipeline state", "0..255" },
	{ "stencil.readMask", false, "a stencil mask is pipeline state", "0..255" },

	{ "raster.cullMode", false, "culling is pipeline state", "ECullMode" },
	{ "raster.flags", false, "wireframe, clamp and similar are pipeline state", "ERasterizerFlags bits" },
	{ "raster.depthBiasConstant", false, "depth bias is pipeline state", "any I32" },
	{ "raster.depthBiasClamp", false, "depth bias is pipeline state", "any F32 (as bits)" },
	{ "raster.depthBiasSlope", false, "depth bias is pipeline state", "any F32 (as bits)" },

	{ "msaa", false,
		"sample count changes whether the pixel stage runs per sample", "0 = 1 sample, 1 = 2, 2 = 4, 3 = 8" },
	{ "msaa.minSampleShading", false,
		"sample rate shading changes the pixel stage's whole schedule", "0 = off, else 0..1 (as bits)" },
	{ "topology", false, "topology is pipeline state a shader can't declare", "ETopologyMode" },
	{ "patchControlPoints", false,
		"the control point count is pipeline state the hull stage doesn't declare", "1..32, 0 without tessellation" },

	{ "vertex.stride", true,
		"the signature proves the formats, never how they are packed into a buffer", "0..4095" },
	{ "vertex.rate", true, "per vertex or per instance is a pipeline choice", "0 = per vertex, 1 = per instance" },

	{ "rt.maxRecursionDepth", false,
		"recursion depth is a pipeline limit, not something a shader declares", "1 or higher" },
	{ "rt.flags", false, "skip triangles, skip AABBs and null shader rules are pipeline state",
		"EPipelineRaytracingFlags bits" },

	{ "layout.binding.type", true, "reflection only sees the registers this shader declares",
		"EGfxRegisterType plus mask bits" },
	{ "layout.binding.count", true, "an array can be sized wider than the shader indexes it", "1 or higher" },
	{ "layout.binding.spaceSpirv", true, "reflection only sees the registers this shader declares",
		"set index, U32_MAX = absent" },
	{ "layout.binding.registerSpirv", true, "reflection only sees the registers this shader declares",
		"binding, U32_MAX = absent" },
	{ "layout.binding.spaceDxil", true, "reflection only sees the registers this shader declares",
		"space, U32_MAX = absent" },
	{ "layout.binding.registerDxil", true, "reflection only sees the registers this shader declares",
		"register, U32_MAX = absent" },
	{ "layout.binding.visibility", true, "a layout can expose a register to more stages than declared it",
		"EGfxPipelineStage bits" },
	{ "layout.binding.data", true, "stride, size and format follow the register type",
		"stride, cbuffer size, texture format or 1 + sampler id" },

	{ "layout.pushConstant.size", false, "a shipping layout can reserve a bigger range than this shader uses",
		"bytes, 128 or less" },
	{ "layout.pushConstant.visibility", false, "a layout can expose the range to more stages than declared it",
		"EGfxPipelineStage bits" },

	//Sampler values never come from reflection, so these rows exist only once a stored layout supplied them

	{ "layout.sampler.filter", true, "reflection can never produce sampler values", "ESamplerFilterMode" },
	{ "layout.sampler.addressU", true, "reflection can never produce sampler values", "ESamplerAddressMode" },
	{ "layout.sampler.addressV", true, "reflection can never produce sampler values", "ESamplerAddressMode" },
	{ "layout.sampler.addressW", true, "reflection can never produce sampler values", "ESamplerAddressMode" },
	{ "layout.sampler.aniso", true, "reflection can never produce sampler values", "0-16" },
	{ "layout.sampler.border", true, "reflection can never produce sampler values", "ESamplerBorderColor" },
	{ "layout.sampler.compareOp", true, "reflection can never produce sampler values", "ECompareOp" },
	{ "layout.sampler.compareEnable", true, "reflection can never produce sampler values", "0 or 1" },
	{ "layout.sampler.mipBias", true, "reflection can never produce sampler values", "float, stored as F16" },
	{ "layout.sampler.minLod", true, "reflection can never produce sampler values", "float, stored as F16" },
	{ "layout.sampler.maxLod", true, "reflection can never produce sampler values", "float, stored as F16" }
};

const C8 *ESPField_name(ESPField field) {
	return field < ESPField_Count ? SPField_info[field].name : "<unknown>";
}

Bool ESPField_isIndexed(ESPField field) {
	return field < ESPField_Count && SPField_info[field].indexed;
}

U8 ESPField_indexCount(ESPField field) {

	if(field >= ESPField_Count || !SPField_info[field].indexed)
		return 1;

	//A layout's binding and sampler rows are bounded by the pipeline's own layout rather than a fixed array,
	// so the parse accepts the full index range and SPFile_supply bounds-checks against the actual row count.

	if(field >= ESPField_LayoutBindingType)
		return U8_MAX;

	return field == ESPField_VertexBufferStride || field == ESPField_VertexBufferRate ? 16 : 8;
}

Bool ESPField_parsePath(CharString path, ESPField *field, U8 *index) {

	if(!field || !index)
		return false;

	//Split "name[index]" on the bracket; a name without one addresses index 0.

	U64 nameLen = CharString_length(path);
	U64 idx = 0;
	Bool hasIndex = false;

	for (U64 i = 0; i < CharString_length(path); ++i)
		if (path.ptr[i] == '[') {

			nameLen = i;
			hasIndex = true;

			if(i + 2 > CharString_length(path) || path.ptr[CharString_length(path) - 1] != ']')
				return false;

			const CharString num = CharString_createRefSizedConst(path.ptr + i + 1, CharString_length(path) - i - 2, false);

			if(!CharString_length(num) || !CharString_parseU64(num, &idx))
				return false;

			break;
		}

	const CharString name = CharString_createRefSizedConst(path.ptr, nameLen, false);

	for (U64 i = 0; i < ESPField_Count; ++i) {

		const CharString candidate = CharString_createRefCStrConst(SPField_info[i].name);

		if(!CharString_equalsStringSensitive(&name, &candidate))
			continue;

		//An index on a field without one is as wrong as one past the end.

		if(hasIndex && (!SPField_info[i].indexed || idx >= ESPField_indexCount((ESPField) i)))
			return false;

		*field = (ESPField) i;
		*index = (U8) idx;
		return true;
	}

	return false;
}

const C8 *ESPField_reason(ESPField field) {
	return field < ESPField_Count ? SPField_info[field].reason : "";
}

const C8 *ESPField_domain(ESPField field) {
	return field < ESPField_Count ? SPField_info[field].domain : "";
}

U32 ESPField_maxValue(ESPField field) {

	switch (field) {

		case ESPField_LayoutSamplerCompareEnable:
		case ESPField_BlendEnable:
		case ESPField_BlendIndependent:
		case ESPField_VertexBufferRate:
			return 1;

		case ESPField_VertexBufferStride:
			return 4095;

		case ESPField_LayoutSamplerFilter:
		case ESPField_LayoutSamplerAddressU:
		case ESPField_LayoutSamplerAddressV:
		case ESPField_LayoutSamplerAddressW:
		case ESPField_LayoutSamplerAniso:
		case ESPField_LayoutSamplerBorder:
		case ESPField_LayoutSamplerCompareOp:
		case ESPField_RenderTargetFormat:
		case ESPField_RenderTargetCount:
		case ESPField_BlendTargetMask:
		case ESPField_BlendLogicOp:
		case ESPField_BlendWriteMask:
		case ESPField_BlendSrc:
		case ESPField_BlendDst:
		case ESPField_BlendSrcAlpha:
		case ESPField_BlendDstAlpha:
		case ESPField_BlendOp:
		case ESPField_BlendOpAlpha:
		case ESPField_DepthFormat:
		case ESPField_DepthStencilFlags:
		case ESPField_DepthCompare:
		case ESPField_StencilCompare:
		case ESPField_StencilFail:
		case ESPField_StencilPass:
		case ESPField_StencilDepthFail:
		case ESPField_StencilWriteMask:
		case ESPField_StencilReadMask:
		case ESPField_Msaa:
		case ESPField_TopologyMode:
		case ESPField_MaxRecursionDepth:
		case ESPField_RaytracingFlags:
			return U8_MAX;

		case ESPField_LayoutSamplerMipBias:
		case ESPField_LayoutSamplerMinLod:
		case ESPField_LayoutSamplerMaxLod:
		case ESPField_CullMode:
		case ESPField_RasterizerFlags:
			return U16_MAX;

		default:
			return U32_MAX;
	}
}

Bool SPFile_create(ESPSettingsFlags flags, const Allocator *alloc, SPFile *spFile, Error *e_rr) {

	Bool s_uccess = true;
	Bool allocated = false;

	if(!spFile)
		retError(clean, Error_nullPointer(2, "SPFile_create()::spFile is required"));

	if(spFile->pipelines.length || spFile->names.entryStrings.length)
		retError(clean, Error_invalidOperation(0, "SPFile_create()::spFile isn't empty, might indicate memleak"));

	const Bool avoidReserve = flags & ESPSettingsFlags_CreateNoReserve;
	flags &= ~ESPSettingsFlags_CreateNoReserve;

	if(flags & ESPSettingsFlags_Invalid)
		retError(clean, Error_invalidParameter(0, 0, "SPFile_create()::flags contained unsupported flag"));

	DLSettings settings = (DLSettings) {
		.dataType = EDLDataType_String,
		.flags = EDLSettingsFlags_HideMagicNumber
	};

	gotoIfError3(clean, DLFile_create(&settings, avoidReserve ? 0 : KIBI, alloc, &spFile->names, e_rr));
	allocated = true;

	if (!avoidReserve) {
		gotoIfError3(clean, DLFile_reserve(&spFile->names, 8, alloc, e_rr));
		gotoIfError3(clean, ListSPPipelineBase_reserve(&spFile->pipelines, 2, alloc, e_rr));
		gotoIfError3(clean, ListSPStage_reserve(&spFile->stages, 4, alloc, e_rr));
	}

	spFile->flags = flags;
	spFile->hash = 0;

clean:

	if(allocated && !s_uccess)
		SPFile_free(spFile, alloc);

	return s_uccess;
}

void SPFile_free(SPFile *spFile, const Allocator *alloc) {

	if(!spFile)
		return;

	DLFile_free(&spFile->names, alloc);
	ListSPPipelineBase_free(&spFile->pipelines, alloc);
	ListSPGraphicsState_free(&spFile->graphicsStates, alloc);
	ListSPRaytracingState_free(&spFile->raytracingStates, alloc);
	ListSPStage_free(&spFile->stages, alloc);
	ListSPSpecialization_free(&spFile->specializations, alloc);
	ListPLFile_freeUnderlying(&spFile->layouts, alloc);

	*spFile = (SPFile) { 0 };
}

void ListSPFile_freeUnderlying(ListSPFile *files, const Allocator *alloc) {

	if(!files)
		return;

	for(U64 i = 0; i < files->length; ++i)
		SPFile_free(&files->ptrNonConst[i], alloc);

	ListSPFile_free(files, alloc);
}

Bool SPFile_addString(SPFile *spFile, CharString *str, const Allocator *alloc, U32 *id, Error *e_rr) {

	Bool s_uccess = true;
	CharString copy = CharString_createNull();

	if(!spFile || !str || !id)
		retError(clean, Error_nullPointer(!spFile ? 0 : (!str ? 1 : 3), "SPFile_addString() requires spFile, str and id"));

	//No string

	if (!CharString_length(*str)) {
		*id = U32_MAX;
		goto clean;
	}

	//Deduplicate against what's already in the pool

	const U64 found = DLFile_findLoadedString(&spFile->names, 0, U64_MAX, str);

	if (found != U64_MAX) {
		*id = (U32) found;
		goto clean;
	}

	if(spFile->names.entryStrings.length >= U32_MAX - 1)
		retError(clean, Error_outOfBounds(
			0, spFile->names.entryStrings.length, U32_MAX - 1, "SPFile_addString() too many strings"
		));

	//The pool owns its strings, so store a copy (DLFile_addEntryString moves it in)

	gotoIfError3(clean, CharString_createCopy(*str, alloc, &copy, e_rr));
	gotoIfError3(clean, DLFile_addEntryString(&spFile->names, &copy, alloc, e_rr));
	copy = CharString_createNull();        //Ownership moved into the DLFile

	*id = (U32) (spFile->names.entryStrings.length - 1);

clean:
	CharString_free(&copy, alloc);
	return s_uccess;
}

//A vertex input's ESBType (SHEntry.inputs[]) to the matching vertex buffer ETextureFormatId, or Undefined if none.
//Vertex inputs are scalars/vectors; HLSL float/uint/int are 32-bit and half is 16-bit, so 8/64-bit have no format.
//16-bit has no 3-component format, so a 3-component 16-bit input falls back to the 4-component (RGBA) one.

static ETextureFormatId SPFile_vertexFormat(U8 esbType) {

	if(!esbType)
		return ETextureFormatId_Undefined;

	const ESBStride stride = ESBType_getStride(esbType);

	if(stride != ESBStride_X16 && stride != ESBStride_X32)
		return ETextureFormatId_Undefined;

	const U8 comp = (U8) ESBType_getVector(esbType);        //0..3 = 1..4 components

	static const ETextureFormatId f32[4] =
		{ ETextureFormatId_R32f, ETextureFormatId_RG32f, ETextureFormatId_RGB32f, ETextureFormatId_RGBA32f };
	static const ETextureFormatId u32[4] =
		{ ETextureFormatId_R32u, ETextureFormatId_RG32u, ETextureFormatId_RGB32u, ETextureFormatId_RGBA32u };
	static const ETextureFormatId i32[4] =
		{ ETextureFormatId_R32i, ETextureFormatId_RG32i, ETextureFormatId_RGB32i, ETextureFormatId_RGBA32i };
	static const ETextureFormatId f16[4] =
		{ ETextureFormatId_R16f, ETextureFormatId_RG16f, ETextureFormatId_RGBA16f, ETextureFormatId_RGBA16f };
	static const ETextureFormatId u16[4] =
		{ ETextureFormatId_R16u, ETextureFormatId_RG16u, ETextureFormatId_RGBA16u, ETextureFormatId_RGBA16u };
	static const ETextureFormatId i16[4] =
		{ ETextureFormatId_R16i, ETextureFormatId_RG16i, ETextureFormatId_RGBA16i, ETextureFormatId_RGBA16i };

	const Bool is16 = stride == ESBStride_X16;

	switch(ESBType_getPrimitive(esbType)) {
		case ESBPrimitive_Float: return (is16 ? f16 : f32)[comp];
		case ESBPrimitive_UInt:  return (is16 ? u16 : u32)[comp];
		case ESBPrimitive_Int:   return (is16 ? i16 : i32)[comp];
		default:                 return ETextureFormatId_Undefined;
	}
}

//A pixel output's ESBType only proves the primitive it writes (float/uint/int), never the target's storage format.
//RGBA8 is the common float target and the integer ones keep full precision, but all three are a choice the pipeline
// makes, so every render target format is reported as a field to specialize.

static ETextureFormatId SPFile_renderTargetFormat(U8 esbType) {

	switch(ESBType_getPrimitive(esbType)) {
		case ESBPrimitive_Float: return ETextureFormatId_RGBA8;
		case ESBPrimitive_UInt:  return ETextureFormatId_RGBA32u;
		case ESBPrimitive_Int:   return ETextureFormatId_RGBA32i;
		default:                 return ETextureFormatId_RGBA8;
	}
}

//The kind specific state hangs off the base record's stateIndex, so a walk over pipelines never needs to know which
// kind it's looking at until it actually wants that state.

const SPGraphicsState *SPFile_graphicsState(const SPFile *spFile, U32 pipelineId) {

	if(!spFile || pipelineId >= spFile->pipelines.length)
		return NULL;

	const SPPipelineBase base = spFile->pipelines.ptr[pipelineId];

	if(base.type != (U8) ESPPipelineType_Graphics || base.stateIndex >= spFile->graphicsStates.length)
		return NULL;

	return &spFile->graphicsStates.ptr[base.stateIndex];
}

const SPRaytracingState *SPFile_raytracingState(const SPFile *spFile, U32 pipelineId) {

	if(!spFile || pipelineId >= spFile->pipelines.length)
		return NULL;

	const SPPipelineBase base = spFile->pipelines.ptr[pipelineId];

	if(base.type != (U8) ESPPipelineType_Raytracing || base.stateIndex >= spFile->raytracingStates.length)
		return NULL;

	return &spFile->raytracingStates.ptr[base.stateIndex];
}

SPGraphicsState *SPFile_graphicsStateMut(SPFile *spFile, U32 pipelineId) {
	return (SPGraphicsState*) SPFile_graphicsState(spFile, pipelineId);
}

SPRaytracingState *SPFile_raytracingStateMut(SPFile *spFile, U32 pipelineId) {
	return (SPRaytracingState*) SPFile_raytracingState(spFile, pipelineId);
}

static Bool SPFile_assume(
	SPFile *spFile, SPPipelineBase *base, ESPField field, U8 index, U32 value, const Allocator *alloc, Error *e_rr
) {

	Bool s_uccess = true;

	const SPSpecialization spec = (SPSpecialization) {
		.field = (U8) field,
		.index = index,
		.source = ESPFieldSource_Assumed,
		.value = value
	};

	gotoIfError3(clean, ListSPSpecialization_pushBack(&spFile->specializations, spec, alloc, e_rr));
	++base->specializationCount;

clean:
	return s_uccess;
}

//Resolves a stage ref to its entry, or NULL when it points outside the files.

static const SHEntry *SPFile_entryOf(const ListSHFile *files, SPStageRef ref, U8 i, Error *e_rr) {

	if(ref.fileId >= files->length) {
		if(e_rr) *e_rr = Error_invalidParameter(1, i, "SPFile_derivePipeline()::stages[i].fileId out of bounds");
		return NULL;
	}

	const SHFile *file = &files->ptr[ref.fileId];

	if(ref.entryId >= file->entries.length) {
		if(e_rr) *e_rr = Error_invalidParameter(1, i, "SPFile_derivePipeline()::stages[i].entryId out of bounds");
		return NULL;
	}

	return &file->entries.ptr[ref.entryId];
}

static Bool SPStage_isRt(U8 stage) {
	return stage >= EGfxPipelineStage_RtStartExt && stage <= EGfxPipelineStage_RtEndExt;
}

static Bool SPStage_isGraphics(U8 stage) {
	return
		stage == EGfxPipelineStage_Vertex || stage == EGfxPipelineStage_Pixel ||
		stage == EGfxPipelineStage_Hull || stage == EGfxPipelineStage_Domain ||
		stage == EGfxPipelineStage_GeometryExt;
}

//Derives the layout from what the stages' binaries reflect, as its own oiPL appended to layouts.
//Registers named in excludedRegisters are the runtime's own (the bindless set and the per frame globals),
// so they belong to the device's layout rather than a custom one; with everything excluded and no push
// constants there is nothing custom to describe and layoutIndex stays U32_MAX, which means exactly that.
//Each stage derives from its entry's first binary: permutations of one entrypoint reflect identical inputs,
// so the first one speaks for all of them.

static Bool SPFile_deriveLayout(
	SPFile *spFile,
	SPPipelineBase *base,
	const ListSHFile *files,
	const SPStageRef *stages,
	U8 stageCount,
	const ListCharString *excludedRegisters,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool moved = false;

	PLFile layout = (PLFile) { 0 };
	gotoIfError3(clean, PLFile_create(EPLSettingsFlags_HideMagicNumber, alloc, &layout, e_rr));

	for (U8 i = 0; i < stageCount; ++i) {

		const SHEntry *entry = SPFile_entryOf(files, stages[i], i, e_rr);

		if(!entry) {
			s_uccess = false;
			goto clean;
		}

		if(!entry->binaryIds.length)
			continue;

		const SHFile *file = &files->ptr[stages[i].fileId];
		const SHBinaryInfo *bin = &file->binaries.ptr[entry->binaryIds.ptr[0]];
		const U32 stageBit = (U32)1 << entry->stage;

		for (U64 j = 0; j < bin->registers.length; ++j) {

			const SHRegisterRuntime *reg = &bin->registers.ptr[j];

			Bool excluded = false;

			if(excludedRegisters)
				for(U64 k = 0; k < excludedRegisters->length; ++k)
					if(CharString_equalsStringSensitive(&excludedRegisters->ptr[k], &reg->name)) {
						excluded = true;
						break;
					}

			if(excluded)
				continue;

			const U64 count64 = SHRegister_arrayCount(&reg->arrays);

			if(count64 > U32_MAX)
				retError(clean, Error_outOfBounds(
					0, count64, U32_MAX, "SPFile_deriveLayout() a register's array is larger than a layout stores"
				));

			const U32 count = (U32) count64;

			U32 data = 0;

			switch (reg->reg.registerType & EGfxRegisterType_TypeMask) {

				//The cbuffer's own size, or the element layout's size, which is the stride

				case EGfxRegisterType_ConstantBuffer:
				case EGfxRegisterType_PushConstants:
				case EGfxRegisterType_StructuredBuffer:
				case EGfxRegisterType_StructuredBufferAtomic:
				case EGfxRegisterType_StorageBuffer:
				case EGfxRegisterType_StorageBufferAtomic:
					data = reg->shaderBuffer.bufferSize;
					break;

				default:

					if(
						reg->reg.registerType & EGfxRegisterType_IsWrite &&
						(reg->reg.registerType & EGfxRegisterType_TypeMask) >= EGfxRegisterType_TextureStart
					)
						//GfxTextureFormat's own bytes
						data = (U32) reg->reg.texture.primitive | ((U32) reg->reg.texture.formatId << 8);

					break;
			}

			//A push constant is identified by its type alone: the SPIRV side never gives it a binding pair,
			// so requiring one would silently drop exactly the register this layout exists to describe

			if ((reg->reg.registerType & EGfxRegisterType_TypeMask) == EGfxRegisterType_PushConstants) {

				if (layout.hasPushConstant) {

					if(layout.pushConstant.strideOrLength != data)
						retError(clean, Error_invalidState(
							0, "SPFile_deriveLayout() stages disagree about the push constant's size"
						));

					//The root signature binds the constants at one register, so every stage has to name the same one

					for(U8 m = 0; m < EGfxBinaryType_Count; ++m) {

						const U64 mine = layout.pushConstant.bindings.arrU64[m];
						const U64 theirs = reg->reg.bindings.arrU64[m];

						if(mine != U64_MAX && theirs != U64_MAX && mine != theirs)
							retError(clean, Error_invalidState(
								0, "SPFile_deriveLayout() stages disagree about the push constant's register"
							));

						if(mine == U64_MAX)
							layout.pushConstant.bindings.arrU64[m] = theirs;
					}

					layout.pushConstant.visibility |= stageBit;
					continue;
				}

				U32 nameId = U32_MAX;
				CharString regName = reg->name;
				gotoIfError3(clean, PLFile_addString(&layout, &regName, alloc, &nameId, e_rr));

				layout.pushConstant = (PLDescriptorBinding) {
					.registerType = reg->reg.registerType,
					.count = count,
					.bindings = reg->reg.bindings,
					.visibility = stageBit,
					.strideOrLength = data,
					.name24_source8 = PLDescriptorBinding_pack(nameId, EPLSource_Derived)
				};

				layout.hasPushConstant = true;
				continue;
			}

			const U64 spv = reg->reg.bindings.arrU64[EGfxBinaryType_SPIRV];
			const U64 dxil = reg->reg.bindings.arrU64[EGfxBinaryType_DXIL];

			const Bool hasSpirv = spv != U64_MAX;
			const Bool hasDxil = dxil != U64_MAX;

			if(!hasSpirv && !hasDxil)
				continue;

			U32 nameId = U32_MAX;
			CharString regName = reg->name;
			gotoIfError3(clean, PLFile_addString(&layout, &regName, alloc, &nameId, e_rr));

			const U32 packed = PLDescriptorBinding_pack(nameId, EPLSource_Derived);

			//The register's name is its identity: another stage naming it again widens visibility and has
			// to agree about everything else.
			//A DIFFERENT register may share a number when its DXIL namespace differs (t0 next to b0), so
			// only a same-namespace clash at one space and binding is a real collision, checked per api.

			U64 match = U64_MAX;

			for(U64 k = 0; k < layout.bindings.length; ++k)
				if(PLDescriptorBinding_name(layout.bindings.ptr[k]) == nameId && nameId != U32_MAX) {
					match = k;
					break;
				}

			if (match != U64_MAX) {

				const PLDescriptorBinding other = layout.bindings.ptr[match];

				if(
					other.registerType != reg->reg.registerType ||
					(hasSpirv && other.bindings.arrU64[EGfxBinaryType_SPIRV] != spv) ||
					(hasDxil && other.bindings.arrU64[EGfxBinaryType_DXIL] != dxil) ||
					other.count != count || other.strideOrLength != data
				)
					retError(clean, Error_invalidState(
						0, "SPFile_deriveLayout() stages disagree about a register they both name"
					));

				layout.bindings.ptrNonConst[match].visibility |= stageBit;
				continue;
			}

			for (U64 k = 0; k < layout.bindings.length; ++k) {

				const PLDescriptorBinding o = layout.bindings.ptr[k];

				const Bool spirvClash = GfxBinding_overlaps(
					reg->reg.bindings.arr[EGfxBinaryType_SPIRV], reg->reg.registerType, count,
					o.bindings.arr[EGfxBinaryType_SPIRV], o.registerType, o.count, EGfxBinaryType_SPIRV
				);

				const Bool dxilClash = GfxBinding_overlaps(
					reg->reg.bindings.arr[EGfxBinaryType_DXIL], reg->reg.registerType, count,
					o.bindings.arr[EGfxBinaryType_DXIL], o.registerType, o.count, EGfxBinaryType_DXIL
				);

				if(spirvClash || dxilClash)
					retError(clean, Error_invalidState(
						0, "SPFile_deriveLayout() two registers overlap at one space, register range and namespace"
					));
			}

			const PLDescriptorBinding row = (PLDescriptorBinding) {
				.registerType = reg->reg.registerType,
				.count = count,
				.bindings = reg->reg.bindings,
				.visibility = stageBit,
				.strideOrLength = data,
				.name24_source8 = packed
			};

			gotoIfError3(clean, ListPLDescriptorBinding_pushBack(&layout.bindings, row, alloc, e_rr));

			if(layout.bindings.length > 255)
				retError(clean, Error_outOfBounds(
					0, layout.bindings.length, 255, "SPFile_deriveLayout() a layout is limited to 255 bindings"
				));
		}
	}

	if(!layout.bindings.length && !layout.hasPushConstant)
		goto clean;

	base->layoutIndex = (U32) spFile->layouts.length;
	gotoIfError3(clean, ListPLFile_pushBack(&spFile->layouts, layout, alloc, e_rr));
	moved = true;

clean:

	if(!moved)
		PLFile_free(&layout, alloc);

	return s_uccess;
}

Bool SPFile_derivePipeline(
	SPFile *spFile,
	const ListSHFile *files,
	const ListCharString *shaderNames,
	CharString name,
	const SPStageRef *stages,
	U8 stageCount,
	const ListCharString *excludedRegisters,
	const Allocator *alloc,
	U32 *pipelineId,
	Error *e_rr
) {

	Bool s_uccess = true;
	const U64 stageStart = spFile->stages.length;
	const U64 layoutStart = spFile->layouts.length;
	const U64 specializationStart = spFile->specializations.length;
	const U64 graphicsStart = spFile->graphicsStates.length;
	const U64 raytracingStart = spFile->raytracingStates.length;
	const U64 nameStart = spFile->names.entryStrings.length;
	Bool appended = false;

	if(!spFile || !files || !stages || !stageCount)
		retError(clean, Error_nullPointer(0, "SPFile_derivePipeline()::spFile, files and stages are required"));

	if(stageCount > 16)
		retError(clean, Error_outOfBounds(4, stageCount, 16, "SPFile_derivePipeline()::stageCount exceeds 16"));

	if(stageStart >> 8 || spFile->pipelines.length >= U32_MAX)
		retError(clean, Error_outOfBounds(0, stageStart, 256, "SPFile_derivePipeline() too many stages or pipelines"));

	SPPipelineBase base = (SPPipelineBase) {
		.name = U32_MAX,
		.stageStart = (U8) stageStart,
		.specializationStart = (U32) specializationStart,
		.stateIndex = U32_MAX,
		.layoutIndex = U32_MAX
	};

	SPGraphicsState gfx = (SPGraphicsState) { 0 };
	SPRaytracingState rt = (SPRaytracingState) { 0 };

	gotoIfError3(clean, SPFile_addString(spFile, &name, alloc, &base.name, e_rr));

	//Classify from exactly the stages listed, never from whatever else a file happens to hold.
	//A file may carry compute, graphics and ray tracing stages side by side; the caller picks which pipeline it means.

	const SHEntry *vs = NULL, *ps = NULL, *hs = NULL;
	U8 computeCount = 0, graphicsCount = 0, rtCount = 0;

	for (U8 i = 0; i < stageCount; ++i) {

		const SHEntry *entry = SPFile_entryOf(files, stages[i], i, e_rr);

		if(!entry) {
			s_uccess = false;
			goto clean;
		}

		//The same stage kind twice can't be one pipeline either.

		for (U8 j = 0; j < i; ++j) {

			const SHEntry *prev = SPFile_entryOf(files, stages[j], j, NULL);

			if(prev && prev->stage == entry->stage)
				retError(clean, Error_invalidParameter(
					4, i, "SPFile_derivePipeline()::stages has the same stage kind twice"
				));
		}

		if(entry->stage == EGfxPipelineStage_Compute)
			++computeCount;

		else if (SPStage_isGraphics(entry->stage)) {

			++graphicsCount;

			if(entry->stage == EGfxPipelineStage_Vertex)
				vs = entry;

			else if(entry->stage == EGfxPipelineStage_Pixel)
				ps = entry;

			else if(entry->stage == EGfxPipelineStage_Hull)
				hs = entry;
		}

		else if(SPStage_isRt(entry->stage))
			++rtCount;

		else retError(clean, Error_unsupportedOperation(
			0, "SPFile_derivePipeline()::stages has a stage kind no pipeline here is built from (mesh, task, node)"
		));

		//The stage records the shader by name, so the pipeline can be resolved again after a load.

		SPStage stage = (SPStage) {
			.shaderFile = U32_MAX,
			.entrypoint = U32_MAX,
			.sourceHash = files->ptr[stages[i].fileId].sourceHash,
			.stage = entry->stage
		};

		if(shaderNames && stages[i].fileId < shaderNames->length)
			gotoIfError3(clean, SPFile_addString(
				spFile, &shaderNames->ptrNonConst[stages[i].fileId], alloc, &stage.shaderFile, e_rr
			));

		CharString entryName = entry->name;
		gotoIfError3(clean, SPFile_addString(spFile, &entryName, alloc, &stage.entrypoint, e_rr));

		gotoIfError3(clean, ListSPStage_pushBack(&spFile->stages, stage, alloc, e_rr));
		++base.stageCount;
	}

	if((computeCount != 0) + (graphicsCount != 0) + (rtCount != 0) != 1)
		retError(clean, Error_invalidParameter(
			4, 0, "SPFile_derivePipeline()::stages mixes compute, graphics and ray tracing, which can't be one pipeline"
		));

	//Compute needs nothing beyond the shader itself, so its state is exact and it carries no extra record at all.

	if (computeCount) {
		base.type = (U8) ESPPipelineType_Compute;
		goto append;
	}

	//Ray tracing derives its stages from the lib, leaving only the pipeline's own limits to specialize.

	if (rtCount) {

		base.type = (U8) ESPPipelineType_Raytracing;
		base.stateIndex = (U32) raytracingStart;
		rt.maxRecursionDepth = 1;
		rt.raytracingFlags = (U8) EPipelineRaytracingFlags_Default;

		gotoIfError3(clean, SPFile_assume(spFile, &base, ESPField_MaxRecursionDepth, 0, 1, alloc, e_rr));
		gotoIfError3(clean, SPFile_assume(
			spFile, &base, ESPField_RaytracingFlags, 0, EPipelineRaytracingFlags_Default, alloc, e_rr
		));

		gotoIfError3(clean, ListSPRaytracingState_pushBack(&spFile->raytracingStates, rt, alloc, e_rr));
		goto append;
	}

	base.type = (U8) ESPPipelineType_Graphics;
	base.stateIndex = (U32) graphicsStart;

	//Vertex input: the signature proves the format at each location, but never how those locations are packed into
	// buffers, so the formats are derived while stride and input rate are specializations.

	if (vs) {

		U16 offset = 0;

		for (U8 i = 0; i < 16; ++i) {

			const ETextureFormatId format = SPFile_vertexFormat(vs->inputs[i]);

			if(!format)
				continue;

			gfx.inputAssembler.vertexLayout.attributes[i].format = (U8) format;
			gfx.inputAssembler.vertexLayout.attributes[i].offset11 = offset;
			gfx.inputAssembler.vertexLayout.attributes[i].bufferId4 = 0;

			offset += ESBType_getSize(vs->inputs[i], true);
		}

		if (offset) {
			gfx.inputAssembler.vertexLayout.bufferStrides12_isInstance1[0] = offset & 4095;
			gotoIfError3(clean, SPFile_assume(spFile, &base, ESPField_VertexBufferStride, 0, offset, alloc, e_rr));
			gotoIfError3(clean, SPFile_assume(spFile, &base, ESPField_VertexBufferRate, 0, 0, alloc, e_rr));
		}
	}

	//Render targets: the count comes from the pixel signature, but a format is never in it.
	//Without a pixel stage the pipeline still needs a target for a generated one to write.

	U8 count = 0;

	if (ps)
		for (U8 i = 0; i < 8 && ps->outputs[i]; ++i) {
			gfx.renderTargetFormats[i] = (U8) SPFile_renderTargetFormat(ps->outputs[i]);
			++count;
		}

	if (!count) {
		gfx.renderTargetFormats[0] = (U8) ETextureFormatId_RGBA8;
		count = 1;
	}

	gfx.renderTargetCount = count;
	gotoIfError3(clean, SPFile_assume(spFile, &base, ESPField_RenderTargetCount, 0, count, alloc, e_rr));

	//Per target state, every one of which changes the pixel stage's exports.

	for (U8 i = 0; i < count; ++i) {

		gfx.blend.writeMask[i] = 0xF;        //All channels

		gotoIfError3(clean, SPFile_assume(
			spFile, &base, ESPField_RenderTargetFormat, i, gfx.renderTargetFormats[i], alloc, e_rr
		));

		gotoIfError3(clean, SPFile_assume(spFile, &base, ESPField_BlendWriteMask, i, 0xF, alloc, e_rr));
		gotoIfError3(clean, SPFile_assume(spFile, &base, ESPField_BlendSrc, i, 0, alloc, e_rr));
		gotoIfError3(clean, SPFile_assume(spFile, &base, ESPField_BlendDst, i, 0, alloc, e_rr));
		gotoIfError3(clean, SPFile_assume(spFile, &base, ESPField_BlendSrcAlpha, i, 0, alloc, e_rr));
		gotoIfError3(clean, SPFile_assume(spFile, &base, ESPField_BlendDstAlpha, i, 0, alloc, e_rr));
		gotoIfError3(clean, SPFile_assume(spFile, &base, ESPField_BlendOp, i, 0, alloc, e_rr));
		gotoIfError3(clean, SPFile_assume(spFile, &base, ESPField_BlendOpAlpha, i, 0, alloc, e_rr));
	}

	//The remaining graphics state has no trace in any shader signature.
	//Each is listed in the order PipelineGraphicsInfo declares it, so the report reads as the pipeline does.

	static const ESPField unprovable[] = {
		ESPField_BlendEnable, ESPField_BlendIndependent, ESPField_BlendTargetMask, ESPField_BlendLogicOp,
		ESPField_DepthFormat, ESPField_DepthStencilFlags, ESPField_DepthCompare,
		ESPField_StencilCompare, ESPField_StencilFail, ESPField_StencilPass,
		ESPField_StencilDepthFail, ESPField_StencilWriteMask, ESPField_StencilReadMask,
		ESPField_CullMode, ESPField_RasterizerFlags, ESPField_DepthBiasConstant,
		ESPField_DepthBiasClamp, ESPField_DepthBiasSlope,
		ESPField_Msaa, ESPField_MsaaMinSampleShading, ESPField_TopologyMode
	};

	for(U64 i = 0; i < sizeof(unprovable) / sizeof(unprovable[0]); ++i)
		gotoIfError3(clean, SPFile_assume(spFile, &base, unprovable[i], 0, 0, alloc, e_rr));

	//Tessellation needs a control point count the hull stage never declares, and nothing otherwise.

	if (hs) {
		gfx.inputAssembler.patchControlPoints = 3;
		gotoIfError3(clean, SPFile_assume(spFile, &base, ESPField_PatchControlPoints, 0, 3, alloc, e_rr));
	}

	gotoIfError3(clean, ListSPGraphicsState_pushBack(&spFile->graphicsStates, gfx, alloc, e_rr));

append:

	gotoIfError3(clean, SPFile_deriveLayout(spFile, &base, files, stages, stageCount, excludedRegisters, alloc, e_rr));

	gotoIfError3(clean, ListSPPipelineBase_pushBack(&spFile->pipelines, base, alloc, e_rr));
	appended = true;

	if(pipelineId)
		*pipelineId = (U32) (spFile->pipelines.length - 1);

clean:

	//A half derived pipeline would leave orphaned records behind, so every pool is rolled back.

	if (!s_uccess && !appended) {
		ListSPStage_resize(&spFile->stages, stageStart, alloc, NULL);
		for(U64 i = layoutStart; i < spFile->layouts.length; ++i)
			PLFile_free(&spFile->layouts.ptrNonConst[i], alloc);

		ListPLFile_resize(&spFile->layouts, layoutStart, alloc, NULL);
		ListSPSpecialization_resize(&spFile->specializations, specializationStart, alloc, NULL);
		ListSPGraphicsState_resize(&spFile->graphicsStates, graphicsStart, alloc, NULL);
		ListSPRaytracingState_resize(&spFile->raytracingStates, raytracingStart, alloc, NULL);

		for(U64 i = nameStart; i < spFile->names.entryStrings.length; ++i)
			CharString_free(&spFile->names.entryStrings.ptrNonConst[i], alloc);

		ListCharString_resize(&spFile->names.entryStrings, nameStart, alloc, NULL);

		if(spFile->names.entryStreams.length > nameStart)
			ListDLEntryStream_resize(&spFile->names.entryStreams, nameStart, alloc, NULL);
	}

	return s_uccess;
}

//Records a value the caller chose, so the field stops being assumed.

Bool SPFile_supply(SPFile *spFile, U32 pipelineId, ESPField field, U8 index, U32 value, Error *e_rr) {

	Bool s_uccess = true;

	if(!spFile)
		retError(clean, Error_nullPointer(0, "SPFile_supply()::spFile is required"));

	if(pipelineId >= spFile->pipelines.length)
		retError(clean, Error_outOfBounds(1, pipelineId, spFile->pipelines.length, "SPFile_supply()::pipelineId invalid"));

	if(field >= ESPField_Count)
		retError(clean, Error_invalidParameter(2, 0, "SPFile_supply()::field is unknown"));

	if(value > ESPField_maxValue(field))
		retError(clean, Error_outOfBounds(
			4, value, ESPField_maxValue(field), "SPFile_supply()::value is wider than the field stores"
		));

	const Bool indexed = SPField_info[field].indexed;
	const U8 limit = ESPField_indexCount(field);

	if(indexed && index >= limit)
		retError(clean, Error_outOfBounds(3, index, limit, "SPFile_supply()::index out of bounds"));

	if(!indexed && index)
		retError(clean, Error_invalidParameter(3, 0, "SPFile_supply()::index given for a field that has none"));

	SPPipelineBase *base = &spFile->pipelines.ptrNonConst[pipelineId];
	SPGraphicsState *gfx = SPFile_graphicsStateMut(spFile, pipelineId);
	SPRaytracingState *rt = SPFile_raytracingStateMut(spFile, pipelineId);

	//Layout fields apply to any pipeline kind and edit the pipeline's oiPL rather than the state, so they
	// resolve through layoutIndex and return before the kind gate below.
	//Editing marks the row supplied; new rows can't be created here, since structure comes through replacing
	// the whole layout (-pso-input) rather than through per field supplies.

	if (field >= ESPField_LayoutBindingType) {

		if(base->layoutIndex == U32_MAX)
			retError(clean, Error_invalidOperation(
				2, "SPFile_supply() the pipeline uses the device's default layout, which the file doesn't describe; "
				"supply one through a stored oiSP first"
			));

		PLFile *layout = &spFile->layouts.ptrNonConst[base->layoutIndex];

		if (field >= ESPField_LayoutSamplerFilter) {

			if(index >= layout->samplers.length)
				retError(clean, Error_outOfBounds(
					3, index, layout->samplers.length, "SPFile_supply()::index has no sampler"
				));

			PLSamplerInfo *smp = &layout->samplers.ptrNonConst[index];

			switch (field) {
				case ESPField_LayoutSamplerFilter:        smp->filter = (U8) value;                    break;
				case ESPField_LayoutSamplerAddressU:      smp->addressU = (U8) value;                  break;
				case ESPField_LayoutSamplerAddressV:      smp->addressV = (U8) value;                  break;
				case ESPField_LayoutSamplerAddressW:      smp->addressW = (U8) value;                  break;
				case ESPField_LayoutSamplerAniso:         smp->aniso = (U8) value;                     break;
				case ESPField_LayoutSamplerBorder:        smp->borderColor = (U8) value;               break;
				case ESPField_LayoutSamplerCompareOp:     smp->comparisonFunction = (U8) value;        break;
				case ESPField_LayoutSamplerCompareEnable: smp->enableComparison = value != 0;          break;
				case ESPField_LayoutSamplerMipBias:       smp->mipBias = (F16) value;                  break;
				case ESPField_LayoutSamplerMinLod:        smp->minLod = (F16) value;                   break;
				default:                                  smp->maxLod = (F16) value;                   break;
			}
		}

		else if (field >= ESPField_LayoutPushConstantSize) {

			if(!layout->hasPushConstant)
				retError(clean, Error_invalidOperation(2, "SPFile_supply() the layout has no push constant"));

			if(field == ESPField_LayoutPushConstantSize)
				layout->pushConstant.strideOrLength = value;

			else layout->pushConstant.visibility = value;

			layout->pushConstant.name24_source8 =
				PLDescriptorBinding_pack(PLDescriptorBinding_name(layout->pushConstant), EPLSource_Supplied);
		}

		else {

			if(index >= layout->bindings.length)
				retError(clean, Error_outOfBounds(
					3, index, layout->bindings.length, "SPFile_supply()::index has no binding"
				));

			PLDescriptorBinding *row = &layout->bindings.ptrNonConst[index];

			switch (field) {
				case ESPField_LayoutBindingType:            row->registerType = value;                               break;
				case ESPField_LayoutBindingCount:           row->count = value;                                      break;
				case ESPField_LayoutBindingSpaceSpirv:      row->bindings.arr[EGfxBinaryType_SPIRV].space = value;   break;
				case ESPField_LayoutBindingRegisterSpirv:   row->bindings.arr[EGfxBinaryType_SPIRV].binding = value; break;
				case ESPField_LayoutBindingSpaceDxil:       row->bindings.arr[EGfxBinaryType_DXIL].space = value;    break;
				case ESPField_LayoutBindingRegisterDxil:    row->bindings.arr[EGfxBinaryType_DXIL].binding = value;  break;
				case ESPField_LayoutBindingVisibility:      row->visibility = value;                                 break;
				default:                                    row->strideOrLength = value;                             break;
			}

			row->name24_source8 = PLDescriptorBinding_pack(PLDescriptorBinding_name(*row), EPLSource_Supplied);
		}

		goto clean;
	}

	//A field belongs to one kind of pipeline, so supplying a graphics field on a compute pipeline is a mistake worth
	// reporting rather than a write into state that doesn't exist.

	const Bool isRtField = field == ESPField_MaxRecursionDepth || field == ESPField_RaytracingFlags;

	if(isRtField && !rt)
		retError(clean, Error_invalidOperation(0, "SPFile_supply()::field is ray tracing only but the pipeline isn't"));

	if(!isRtField && !gfx)
		retError(clean, Error_invalidOperation(1, "SPFile_supply()::field is graphics only but the pipeline isn't"));

	//Writing the value into the state itself keeps the lowering free of provenance handling.

	switch (field) {

		case ESPField_RenderTargetFormat:

			gfx->renderTargetFormats[index] = (U8) value;

			if(index >= gfx->renderTargetCount)
				gfx->renderTargetCount = index + 1;

			break;

		case ESPField_RenderTargetCount:    gfx->renderTargetCount = (U8) value;                            break;
		case ESPField_BlendEnable:          gfx->blend.enable = value != 0;                                 break;
		case ESPField_BlendIndependent:     gfx->blend.allowIndependentBlend = value != 0;                  break;
		case ESPField_BlendTargetMask:      gfx->blend.renderTargetMask = (U8) value;                       break;
		case ESPField_BlendLogicOp:         gfx->blend.logicOpExt = (U8) value;                             break;
		case ESPField_BlendWriteMask:       gfx->blend.writeMask[index] = (U8) value;                       break;
		case ESPField_BlendSrc:             gfx->blend.attachments[index].srcBlend = (U8) value;            break;
		case ESPField_BlendDst:             gfx->blend.attachments[index].dstBlend = (U8) value;            break;
		case ESPField_BlendSrcAlpha:        gfx->blend.attachments[index].srcBlendAlpha = (U8) value;       break;
		case ESPField_BlendDstAlpha:        gfx->blend.attachments[index].dstBlendAlpha = (U8) value;       break;
		case ESPField_BlendOp:              gfx->blend.attachments[index].blendOp = (U8) value;             break;
		case ESPField_BlendOpAlpha:         gfx->blend.attachments[index].blendOpAlpha = (U8) value;        break;

		case ESPField_DepthFormat:          gfx->depthFormat = (U8) value;                                  break;
		case ESPField_DepthStencilFlags:    gfx->depth.flags = (U8) value;                                  break;
		case ESPField_DepthCompare:         gfx->depth.depthCompare = (U8) value;                           break;
		case ESPField_StencilCompare:       gfx->depth.stencilCompare = (U8) value;                         break;
		case ESPField_StencilFail:          gfx->depth.stencilFail = (U8) value;                            break;
		case ESPField_StencilPass:          gfx->depth.stencilPass = (U8) value;                            break;
		case ESPField_StencilDepthFail:     gfx->depth.stencilDepthFail = (U8) value;                       break;
		case ESPField_StencilWriteMask:     gfx->depth.stencilWriteMask = (U8) value;                       break;
		case ESPField_StencilReadMask:      gfx->depth.stencilReadMask = (U8) value;                        break;

		case ESPField_CullMode:             gfx->rasterizer.cullMode = (U16) value;                         break;
		case ESPField_RasterizerFlags:      gfx->rasterizer.flags = (U16) value;                            break;
		case ESPField_DepthBiasConstant:    gfx->rasterizer.depthBiasConstantFactor = (I32) value;          break;
		case ESPField_DepthBiasClamp:       gfx->rasterizer.depthBiasClamp = F32_fromU32Bits(value);        break;
		case ESPField_DepthBiasSlope:       gfx->rasterizer.depthBiasSlopeFactor = F32_fromU32Bits(value);  break;

		case ESPField_Msaa:                 gfx->msaa = (U8) value;                                         break;
		case ESPField_MsaaMinSampleShading: gfx->msaaMinSampleShading = F32_fromU32Bits(value);             break;
		case ESPField_TopologyMode:         gfx->inputAssembler.topologyMode = (U8) value;                  break;
		case ESPField_PatchControlPoints:   gfx->inputAssembler.patchControlPoints = value;                 break;

		//Stride and instance rate share one U16 per buffer, so each writes only its own bits.

		case ESPField_VertexBufferStride: {
			U16 *strides = gfx->inputAssembler.vertexLayout.bufferStrides12_isInstance1;
			strides[index] = (strides[index] & ~(U16)4095) | (U16)(value & 4095);
			break;
		}

		case ESPField_VertexBufferRate: {
			U16 *strides = gfx->inputAssembler.vertexLayout.bufferStrides12_isInstance1;
			strides[index] = (strides[index] & (U16)4095) | (value ? (U16)(1 << 12) : 0);
			break;
		}

		case ESPField_MaxRecursionDepth:      rt->maxRecursionDepth = (U8) value;        break;
		case ESPField_RaytracingFlags:        rt->raytracingFlags = (U8) value;          break;

		default:
			retError(clean, Error_invalidParameter(2, 1, "SPFile_supply()::field isn't handled"));
	}

	//The field stops being assumed once the caller owns its value.

	for (U32 i = 0; i < base->specializationCount; ++i) {

		SPSpecialization *spec = &spFile->specializations.ptrNonConst[base->specializationStart + i];

		if(spec->field != (U8) field || spec->index != index)
			continue;

		spec->source = ESPFieldSource_Supplied;
		spec->value = value;
		break;
	}

clean:
	return s_uccess;
}

U64 SPFile_assumedCount(const SPFile *spFile, U32 pipelineId) {

	if(!spFile || pipelineId >= spFile->pipelines.length)
		return 0;

	const SPPipelineBase base = spFile->pipelines.ptr[pipelineId];
	U64 count = 0;

	for (U32 i = 0; i < base.specializationCount; ++i)
		count += spFile->specializations.ptr[base.specializationStart + i].source == ESPFieldSource_Assumed;

	return count;
}

Bool SPFile_isExact(const SPFile *spFile, U32 pipelineId) {
	return !SPFile_assumedCount(spFile, pipelineId);
}

static Bool SPFile_issue(
	ListCharString *issues, const Allocator *alloc, Error *e_rr, const C8 *format, U32 a, U32 b
) {

	Bool s_uccess = true;
	CharString issue = CharString_createNull();

	gotoIfError3(clean, CharString_format(alloc, &issue, e_rr, format, a, b));
	gotoIfError3(clean, ListCharString_pushBack(issues, issue, alloc, e_rr));
	issue = CharString_createNull();

clean:
	CharString_free(&issue, alloc);
	return s_uccess;
}

Bool SPFile_validate(
	const SPFile *spFile,
	U32 pipelineId,
	const ListSHFile *files,
	const SPStageRef *stages,
	const Allocator *alloc,
	ListCharString *issues,
	Error *e_rr
) {

	Bool s_uccess = true;

	if(!spFile || !files || !stages || !issues)
		retError(clean, Error_nullPointer(0, "SPFile_validate()::spFile, files, stages and issues are required"));

	if(pipelineId >= spFile->pipelines.length)
		retError(clean, Error_outOfBounds(1, pipelineId, spFile->pipelines.length, "SPFile_validate()::pipelineId invalid"));

	const SPPipelineBase base = spFile->pipelines.ptr[pipelineId];
	const SPRaytracingState *rt = SPFile_raytracingState(spFile, pipelineId);
	const SPGraphicsState *gfx = SPFile_graphicsState(spFile, pipelineId);

	//The layout validates through its own format, so a bad row names itself here rather than at the driver

	if(base.layoutIndex != U32_MAX && base.layoutIndex < spFile->layouts.length)
		gotoIfError3(clean, PLFile_validate(&spFile->layouts.ptr[base.layoutIndex], alloc, issues, e_rr));

	//Checks every pipeline shares, whatever kind it is.

	const SHEntry *vs = NULL, *ps = NULL, *hs = NULL, *ds = NULL;

	for (U8 i = 0; i < base.stageCount; ++i) {

		const SHEntry *entry = SPFile_entryOf(files, stages[i], i, e_rr);

		if(!entry) {
			s_uccess = false;
			goto clean;
		}

		switch (entry->stage) {
			case EGfxPipelineStage_Vertex:  vs = entry;  break;
			case EGfxPipelineStage_Pixel:   ps = entry;  break;
			case EGfxPipelineStage_Hull:    hs = entry;  break;
			case EGfxPipelineStage_Domain:  ds = entry;  break;
			default:                                     break;
		}
	}

	if(!base.stageCount)
		gotoIfError3(clean, SPFile_issue(issues, alloc, e_rr, "the pipeline binds no stage at all", 0, 0));

	//A pipeline whose kind and state disagree can't be lowered by any backend.

	if(base.type == (U8) ESPPipelineType_Graphics && !gfx)
		gotoIfError3(clean, SPFile_issue(issues, alloc, e_rr, "a graphics pipeline is missing its graphics state", 0, 0));

	if(base.type == (U8) ESPPipelineType_Raytracing && !rt)
		gotoIfError3(clean, SPFile_issue(issues, alloc, e_rr, "a ray tracing pipeline is missing its state", 0, 0));

	if (rt && !rt->maxRecursionDepth)
		gotoIfError3(clean, SPFile_issue(
			issues, alloc, e_rr, "rt.maxRecursionDepth is 0, which can't trace a single ray", 0, 0
		));

	if (!gfx)
		goto clean;

	//A pixel shader writing a target the pipeline doesn't declare can't be compiled.

	if (ps) {

		U8 written = 0;

		for(U8 i = 0; i < 8 && ps->outputs[i]; ++i)
			++written;

		if(written > gfx->renderTargetCount)
			gotoIfError3(clean, SPFile_issue(
				issues, alloc, e_rr,
				"the pixel stage writes %"PRIu32" render target(s) but the pipeline declares %"PRIu32,
				written, gfx->renderTargetCount
			));
	}

	//Every declared target needs a format, since Undefined can't be an attachment.

	for (U8 i = 0; i < gfx->renderTargetCount && i < 8; ++i)
		if(!gfx->renderTargetFormats[i])
			gotoIfError3(clean, SPFile_issue(
				issues, alloc, e_rr, "rtv.format[%"PRIu32"] is undefined while rtv.count declares it", i, 0
			));

	//Depth state without a depth attachment is a mismatch the backend rejects.

	if((gfx->depth.flags || gfx->depth.depthCompare) && !gfx->depthFormat)
		gotoIfError3(clean, SPFile_issue(
			issues, alloc, e_rr, "depth.flags/depth.compare are set while depth.format is none", 0, 0
		));

	//A vertex input the pipeline has no format for would fetch nothing.

	if (vs)
		for (U8 i = 0; i < 16; ++i)
			if(vs->inputs[i] && !gfx->inputAssembler.vertexLayout.attributes[i].format)
				gotoIfError3(clean, SPFile_issue(
					issues, alloc, e_rr,
					"the vertex stage reads location %"PRIu32" but the pipeline has no format for it", i, 0
				));

	//Checks that need no device and no graphics enum, so they belong here rather than in a backend.

	if(gfx->renderTargetCount > 8)
		gotoIfError3(clean, SPFile_issue(
			issues, alloc, e_rr, "rtv.count is %"PRIu32", the maximum is 8", gfx->renderTargetCount, 0
		));

	//Blending a target the pipeline doesn't declare would reference an attachment that isn't there.

	for (U8 i = gfx->renderTargetCount; i < 8; ++i)
		if (gfx->blend.renderTargetMask & (1 << i)) {
			gotoIfError3(clean, SPFile_issue(
				issues, alloc, e_rr, "blend.targetMask enables target %"PRIu32" but only %"PRIu32" are declared",
				i, gfx->renderTargetCount
			));
			break;
		}

	//Sample rate shading is a fraction of the samples, so anything outside 0..1 is meaningless.

	if(gfx->msaaMinSampleShading < 0 || gfx->msaaMinSampleShading > 1)
		gotoIfError3(clean, SPFile_issue(issues, alloc, e_rr, "msaa.minSampleShading is outside 0..1", 0, 0));

	//A pipeline that blends without naming any target to blend does nothing with its blend state.

	if(gfx->blend.enable && !gfx->blend.renderTargetMask)
		gotoIfError3(clean, SPFile_issue(
			issues, alloc, e_rr, "blend.enable is set while blend.targetMask enables no target", 0, 0
		));

	//Tessellation is a pair with a control point count; half a pair or no count can't form a pipeline.

	if((hs != NULL) != (ds != NULL))
		gotoIfError3(clean, SPFile_issue(
			issues, alloc, e_rr, "a hull stage needs its domain stage and the other way around", 0, 0
		));

	if(hs && (!gfx->inputAssembler.patchControlPoints || gfx->inputAssembler.patchControlPoints > 32))
		gotoIfError3(clean, SPFile_issue(
			issues, alloc, e_rr, "patchControlPoints is %"PRIu32", it has to be 1..32 with tessellation",
			gfx->inputAssembler.patchControlPoints, 0
		));

	if(!hs && gfx->inputAssembler.patchControlPoints)
		gotoIfError3(clean, SPFile_issue(
			issues, alloc, e_rr, "patchControlPoints is set without tessellation stages", 0, 0
		));

clean:
	return s_uccess;
}

Bool SPFile_print(const SPFile *spFile, U32 pipelineId, const Allocator *alloc, CharString *result, Error *e_rr) {

	Bool s_uccess = true;
	CharString line = CharString_createNull();

	if(!spFile || !result)
		retError(clean, Error_nullPointer(0, "SPFile_print()::spFile and result are required"));

	if(pipelineId >= spFile->pipelines.length)
		retError(clean, Error_outOfBounds(1, pipelineId, spFile->pipelines.length, "SPFile_print()::pipelineId invalid"));

	static const C8 *typeNames[ESPPipelineType_Count] = { "compute", "graphics", "raytracing" };
	static const C8 *sourceNames[ESPFieldSource_Count] = { "derived", "supplied", "assumed" };

	const SPPipelineBase base = spFile->pipelines.ptr[pipelineId];

	gotoIfError3(clean, CharString_format(
		alloc, &line, e_rr, "; Pipeline state (%s), %"PRIu32" stage(s), %"PRIu64" assumed field(s)\n",
		base.type < ESPPipelineType_Count ? typeNames[base.type] : "<unknown>",
		(U32) base.stageCount, SPFile_assumedCount(spFile, pipelineId)
	));

	gotoIfError3(clean, CharString_appendString(result, &line, alloc, e_rr));
	CharString_free(&line, alloc);

	if (base.flags & ESPPipelineFlag_GeneratedVertexStage) {

		const CharString note = CharString_createRefCStrConst(
			";   NOTE: the vertex stage was generated, since this shader declares none and a graphics pipeline needs "
			"one. Its outputs mirror the next stage's inputs and its values are read from the app data buffer so they "
			"can't be folded into the stages you asked about. Its own disassembly means nothing here.\n"
		);

		gotoIfError3(clean, CharString_appendString(result, &note, alloc, e_rr));
	}

	if (base.flags & ESPPipelineFlag_GeneratedPixelStage) {

		const CharString note = CharString_createRefCStrConst(
			";   NOTE: the pixel stage was generated, since this shader declares none. It consumes every input it "
			"receives so the preceding stage's output writes aren't eliminated as dead. Its own disassembly means "
			"nothing here.\n"
		);

		gotoIfError3(clean, CharString_appendString(result, &note, alloc, e_rr));
	}

	if (base.flags & ESPPipelineFlag_AssumedHitGrouping) {

		const CharString note = CharString_createRefCStrConst(
			";   NOTE: this lib has more than one hit shader, so their pairing into hit groups was inferred by order. "
			"Which any-hit or intersection shader belongs with which closest-hit shader is a pipeline decision the "
			"shader doesn't state.\n"
		);

		gotoIfError3(clean, CharString_appendString(result, &note, alloc, e_rr));
	}

	for (U32 i = 0; i < base.specializationCount; ++i) {

		const SPSpecialization spec = spFile->specializations.ptr[base.specializationStart + i];
		const C8 *name = ESPField_name((ESPField) spec.field);
		const C8 *source = spec.source < ESPFieldSource_Count ? sourceNames[spec.source] : "<unknown>";

		if (ESPField_isIndexed((ESPField) spec.field)) {
			gotoIfError3(clean, CharString_format(
				alloc, &line, e_rr, ";   %s[%"PRIu32"] = %"PRIu32" (%s; %s)\n",
				name, (U32) spec.index, spec.value, source, ESPField_domain((ESPField) spec.field)
			));
		}

		else gotoIfError3(clean, CharString_format(
			alloc, &line, e_rr, ";   %s = %"PRIu32" (%s; %s)\n",
			name, spec.value, source, ESPField_domain((ESPField) spec.field)
		));

		gotoIfError3(clean, CharString_appendString(result, &line, alloc, e_rr));
		CharString_free(&line, alloc);
	}

	//The layout the pipeline was built against, one settable path per row so overriding needs no other syntax.
	//The three F16 fields print as the float value -pso-set parses, not as their bit pattern.

	if (base.layoutIndex != U32_MAX) {

		const PLFile *layout = &spFile->layouts.ptr[base.layoutIndex];

		static const ESPField bindingFields[8] = {
			ESPField_LayoutBindingType, ESPField_LayoutBindingCount,
			ESPField_LayoutBindingSpaceSpirv, ESPField_LayoutBindingRegisterSpirv,
			ESPField_LayoutBindingSpaceDxil, ESPField_LayoutBindingRegisterDxil,
			ESPField_LayoutBindingVisibility, ESPField_LayoutBindingData
		};

		for (U32 i = 0; i < (U32) layout->bindings.length; ++i) {

			const PLDescriptorBinding row = layout->bindings.ptr[i];
			const U32 rowNameId = PLDescriptorBinding_name(row);
			const EPLSource rowSource = PLDescriptorBinding_source(row);
			const C8 *source = rowSource < EPLSource_Count ? sourceNames[rowSource] : "<unknown>";

			if (rowNameId != PLDescriptorBinding_NAME_NONE && rowNameId < layout->names.entryStrings.length) {

				const CharString rowName = layout->names.entryStrings.ptr[rowNameId];

				gotoIfError3(clean, CharString_format(
					alloc, &line, e_rr, ";   layout.binding[%"PRIu32"] '%.*s'\n",
					i, (int) CharString_length(rowName), rowName.ptr
				));

				gotoIfError3(clean, CharString_appendString(result, &line, alloc, e_rr));
				CharString_free(&line, alloc);
			}

			const GfxBinding spvPair = row.bindings.arr[EGfxBinaryType_SPIRV];
			const GfxBinding dxilPair = row.bindings.arr[EGfxBinaryType_DXIL];

			const U32 values[8] = {
				row.registerType, row.count, spvPair.space, spvPair.binding, dxilPair.space, dxilPair.binding,
				row.visibility, row.strideOrLength
			};

			for (U8 j = 0; j < 8; ++j) {

				gotoIfError3(clean, CharString_format(
					alloc, &line, e_rr, ";   %s[%"PRIu32"] = %"PRIu32" (%s; %s)\n",
					ESPField_name(bindingFields[j]), i, values[j], source, ESPField_domain(bindingFields[j])
				));

				gotoIfError3(clean, CharString_appendString(result, &line, alloc, e_rr));
				CharString_free(&line, alloc);
			}
		}

		static const ESPField samplerFields[11] = {
			ESPField_LayoutSamplerFilter, ESPField_LayoutSamplerAddressU, ESPField_LayoutSamplerAddressV,
			ESPField_LayoutSamplerAddressW, ESPField_LayoutSamplerAniso, ESPField_LayoutSamplerBorder,
			ESPField_LayoutSamplerCompareOp, ESPField_LayoutSamplerCompareEnable, ESPField_LayoutSamplerMipBias,
			ESPField_LayoutSamplerMinLod, ESPField_LayoutSamplerMaxLod
		};

		for (U32 i = 0; i < (U32) layout->samplers.length; ++i) {

			const PLSamplerInfo smp = layout->samplers.ptr[i];

			const U32 values[8] = {
				smp.filter, smp.addressU, smp.addressV, smp.addressW, smp.aniso, smp.borderColor,
				smp.comparisonFunction, smp.enableComparison
			};

			for (U8 j = 0; j < 8; ++j) {

				gotoIfError3(clean, CharString_format(
					alloc, &line, e_rr, ";   %s[%"PRIu32"] = %"PRIu32" (supplied; %s)\n",
					ESPField_name(samplerFields[j]), i, values[j], ESPField_domain(samplerFields[j])
				));

				gotoIfError3(clean, CharString_appendString(result, &line, alloc, e_rr));
				CharString_free(&line, alloc);
			}

			const F16 lods[3] = { smp.mipBias, smp.minLod, smp.maxLod };

			for (U8 j = 0; j < 3; ++j) {

				const F32 lod = F32_fromU32Bits((U32) EFloatType_convert(EFloatType_F16, lods[j], EFloatType_F32));

				gotoIfError3(clean, CharString_format(
					alloc, &line, e_rr, ";   %s[%"PRIu32"] = %f (supplied; %s)\n",
					ESPField_name(samplerFields[8 + j]), i, lod, ESPField_domain(samplerFields[8 + j])
				));

				gotoIfError3(clean, CharString_appendString(result, &line, alloc, e_rr));
				CharString_free(&line, alloc);
			}
		}

		if (layout->hasPushConstant) {

			const PLDescriptorBinding pc = layout->pushConstant;
			const EPLSource pcSource = PLDescriptorBinding_source(pc);
			const C8 *source = pcSource < EPLSource_Count ? sourceNames[pcSource] : "<unknown>";

			gotoIfError3(clean, CharString_format(
				alloc, &line, e_rr, ";   %s = %"PRIu32" (%s; %s)\n",
				ESPField_name(ESPField_LayoutPushConstantSize), pc.strideOrLength, source,
				ESPField_domain(ESPField_LayoutPushConstantSize)
			));

			gotoIfError3(clean, CharString_appendString(result, &line, alloc, e_rr));
			CharString_free(&line, alloc);

			gotoIfError3(clean, CharString_format(
				alloc, &line, e_rr, ";   %s = %"PRIu32" (%s; %s)\n",
				ESPField_name(ESPField_LayoutPushConstantVisibility), pc.visibility, source,
				ESPField_domain(ESPField_LayoutPushConstantVisibility)
			));

			gotoIfError3(clean, CharString_appendString(result, &line, alloc, e_rr));
			CharString_free(&line, alloc);
		}
	}

	else {

		CharString note = CharString_createRefCStrConst(";   NOTE: the pipeline uses the device's default layout\n");
		gotoIfError3(clean, CharString_appendString(result, &note, alloc, e_rr));
	}

clean:
	CharString_free(&line, alloc);
	return s_uccess;
}

//A blend state only reaches an attachment it can actually use, so that's all the file keeps.
//With independent blending off every target reads attachments[0], and with it on only the masked targets are read.

Bool SPBlendStateRuntime_storesAttachment(SPBlendStateRuntime blend, U8 renderTarget) {

	if(!blend.enable || renderTarget >= 8)
		return false;

	if(!blend.allowIndependentBlend)
		return !renderTarget;

	return (blend.renderTargetMask >> renderTarget) & 1;
}

U8 SPBlendStateRuntime_storedAttachmentCount(SPBlendStateRuntime blend) {

	U8 count = 0;

	for(U8 i = 0; i < 8; ++i)
		if(SPBlendStateRuntime_storesAttachment(blend, i))
			++count;

	return count;
}

//An input location is only real once it has a format, and a buffer only once it carries a stride or an instance rate.
//Anything else reads back as zero, so leaving it out of the file loses nothing.

U16 SPVertexLayoutRuntime_bufferMask(SPVertexLayoutRuntime layout) {

	U16 mask = 0;

	for(U8 i = 0; i < 16; ++i)
		if(layout.bufferStrides12_isInstance1[i])
			mask |= (U16)1 << i;

	return mask;
}

U16 SPVertexLayoutRuntime_attributeMask(SPVertexLayoutRuntime layout) {

	U16 mask = 0;

	for(U8 i = 0; i < 16; ++i)
		if(layout.attributes[i].format)
			mask |= (U16)1 << i;

	return mask;
}

//The stored form drops the blend attachments and the vertex entries, keeping the masks that say which ones follow.

SPGraphicsStateStored SPGraphicsState_store(SPGraphicsState state) {

	SPGraphicsStateStored stored = (SPGraphicsStateStored) {

		.msaa = state.msaa,
		.renderTargetCount = state.renderTargetCount,
		.depthFormat = state.depthFormat,

		.msaaMinSampleShading = state.msaaMinSampleShading,

		.blend = (SPBlendStateStored) {
			.enable = state.blend.enable,
			.allowIndependentBlend = state.blend.allowIndependentBlend,
			.renderTargetMask = state.blend.renderTargetMask,
			.logicOpExt = state.blend.logicOpExt
		},

		.depth = state.depth,
		.rasterizer = state.rasterizer,

		.inputAssembler = (SPInputAssemblerStored) {
			.topologyMode = state.inputAssembler.topologyMode,
			.patchControlPoints = state.inputAssembler.patchControlPoints,
			.vertexLayout = (SPVertexLayoutStored) {
				.bufferMask = SPVertexLayoutRuntime_bufferMask(state.inputAssembler.vertexLayout),
				.attributeMask = SPVertexLayoutRuntime_attributeMask(state.inputAssembler.vertexLayout)
			}
		}
	};

	for(U8 i = 0; i < 8; ++i)
		stored.renderTargetFormats[i] = state.renderTargetFormats[i];

	for(U8 i = 0; i < 8; ++i)
		stored.blend.writeMask[i] = state.blend.writeMask[i];

	return stored;
}

//Expanding leaves the attachments and vertex entries zeroed; the reader fills in the ones the masks selected.

SPGraphicsState SPGraphicsStateStored_expand(SPGraphicsStateStored stored) {

	SPGraphicsState state = (SPGraphicsState) {

		.msaa = stored.msaa,
		.renderTargetCount = stored.renderTargetCount,
		.depthFormat = stored.depthFormat,

		.msaaMinSampleShading = stored.msaaMinSampleShading,

		.blend = (SPBlendStateRuntime) {
			.enable = stored.blend.enable,
			.allowIndependentBlend = stored.blend.allowIndependentBlend,
			.renderTargetMask = stored.blend.renderTargetMask,
			.logicOpExt = stored.blend.logicOpExt
		},

		.depth = stored.depth,
		.rasterizer = stored.rasterizer,

		.inputAssembler = (SPInputAssembler) {
			.topologyMode = stored.inputAssembler.topologyMode,
			.patchControlPoints = stored.inputAssembler.patchControlPoints
		}
	};

	for(U8 i = 0; i < 8; ++i)
		state.renderTargetFormats[i] = stored.renderTargetFormats[i];

	for(U8 i = 0; i < 8; ++i)
		state.blend.writeMask[i] = stored.blend.writeMask[i];

	return state;
}

Bool SPFile_finalize(SPFile *spFile, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;
	(void) alloc;        //No allocation needed; kept for signature consistency and future use

	if(!spFile)
		retError(clean, Error_nullPointer(0, "SPFile_finalize()::spFile is required"));

	//An entry the file won't store has to be cleared, or the hash would describe bytes that don't
	// survive a write.

	for (U64 i = 0; i < spFile->graphicsStates.length; ++i) {

		SPGraphicsState *gfx = &spFile->graphicsStates.ptrNonConst[i];

		for(U8 j = 0; j < 8; ++j)
			if(!SPBlendStateRuntime_storesAttachment(gfx->blend, j))
				gfx->blend.attachments[j] = (SPBlendAttachment) { 0 };

		for(U8 j = 0; j < 16; ++j)
			if(!gfx->inputAssembler.vertexLayout.attributes[j].format)
				gfx->inputAssembler.vertexLayout.attributes[j] = (SPVertexAttribute) { 0 };
	}

	//Hash content deterministically (flags without HideMagicNumber, then the POD arrays and strings)

	const U32 hashedFlags = spFile->flags & ~(U32)ESPSettingsFlags_HideMagicNumber;

	U64 hash = Buffer_fnv1a64Single(hashedFlags, Buffer_fnv1a64Offset);
	hash = Buffer_fnv1a64(ListSPPipelineBase_bufferConst(spFile->pipelines), hash);
	hash = Buffer_fnv1a64(ListSPStage_bufferConst(spFile->stages), hash);
	hash = Buffer_fnv1a64(ListSPSpecialization_bufferConst(spFile->specializations), hash);
	hash = Buffer_fnv1a64(ListSPGraphicsState_bufferConst(spFile->graphicsStates), hash);
	hash = Buffer_fnv1a64(ListSPRaytracingState_bufferConst(spFile->raytracingStates), hash);
	//Each layout hashes itself, so a standalone oiPL and an embedded one can never disagree

	for (U64 i = 0; i < spFile->layouts.length; ++i) {
		gotoIfError3(clean, PLFile_finalize(&spFile->layouts.ptrNonConst[i], alloc, e_rr));
		hash = Buffer_fnv1a64Single(spFile->layouts.ptr[i].hash, hash);
	}

	for(U64 i = 0; i < spFile->names.entryStrings.length; ++i)
		hash = Buffer_fnv1a64(CharString_bufferConst(spFile->names.entryStrings.ptr[i]), hash);

	spFile->hash = hash;

clean:
	return s_uccess;
}
