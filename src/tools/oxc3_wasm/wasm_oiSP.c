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

//tools/oxc3_wasm/wasm_oiSP.c

#include "tools/oxc3_wasm/wasm_bridge.h"
#include "formats/oiSP/sp_file.h"
#include "formats/oiPL/pl_file.h"
#include "formats/gfx_util/gfx_util.h"
#include "formats/oiSH/sh_entries.h"
#include <inttypes.h>

static const C8 *spPipelineTypeNames[ESPPipelineType_Count] = { "compute", "graphics", "raytracing" };
static const C8 *spFieldSourceNames[ESPFieldSource_Count] = { "derived", "supplied", "assumed" };

//The keys the rest of the document uses for a binary type, so a layout row's pair reads like a binary's sizes.
static const C8 *spBinaryKeys[EGfxBinaryType_Count] = { "spirv", "dxil" };

//One oiPL row: a binding, or the push constant range that sits beside them. The active union member follows
//the register class, which is why the class is spelled out before it.

static Bool WasmJson_plBinding(
	const PLFile *layout, PLDescriptorBinding b, CharString *out, const Allocator *alloc, Error *e_rr
) {

	Bool s_uccess = true;
	Bool first = true;

	const EGfxRegisterType base = (EGfxRegisterType)(b.registerType & EGfxRegisterType_TypeMask);
	const Bool isWrite = (b.registerType & EGfxRegisterType_IsWrite) != 0;
	const Bool isSampler = base == EGfxRegisterType_Sampler || base == EGfxRegisterType_SamplerComparisonState;
	const Bool isTexture = base >= EGfxRegisterType_Texture1D && base <= EGfxRegisterType_Texture2DMS;
	const U32 nameId = PLDescriptorBinding_name(b);
	const EPLSource source = PLDescriptorBinding_source(b);

	gotoIfError3(clean, Json_raw(out, "{", alloc, e_rr));

	gotoIfError3(clean, Json_key(out, "name", &first, alloc, e_rr));

	if(nameId != PLDescriptorBinding_NAME_NONE && nameId < layout->names.entryStrings.length) {
		gotoIfError3(clean, Json_str(out, layout->names.entryStrings.ptr[nameId], alloc, e_rr));
	}

	else {
		gotoIfError3(clean, Json_raw(out, "null", alloc, e_rr));
	}

	gotoIfError3(clean, Json_key(out, "source", &first, alloc, e_rr));
	gotoIfError3(clean, Json_cstr(
		out, (U32) source < ESPFieldSource_Count ? spFieldSourceNames[source] : "unknown", alloc, e_rr
	));

	gotoIfError3(clean, Json_key(out, "class", &first, alloc, e_rr));
	gotoIfError3(clean, Json_cstr(out, WasmJson_registerClass(base), alloc, e_rr));

	gotoIfError3(clean, Json_key(out, "type", &first, alloc, e_rr));
	gotoIfError3(clean, Json_cstr(out, WasmJson_registerBaseName(base, isWrite), alloc, e_rr));

	gotoIfError3(clean, Json_key(out, "isWrite", &first, alloc, e_rr));
	gotoIfError3(clean, Json_bool(out, isWrite, alloc, e_rr));

	gotoIfError3(clean, Json_key(out, "isArray", &first, alloc, e_rr));
	gotoIfError3(clean, Json_bool(out, (b.registerType & EGfxRegisterType_IsArray) != 0, alloc, e_rr));

	gotoIfError3(clean, Json_key(out, "count", &first, alloc, e_rr));
	gotoIfError3(clean, Json_fmt(out, alloc, e_rr, "%"PRIu32, b.count));

	//The stages that see the row, by name, since the page never reasons about the mask itself

	gotoIfError3(clean, Json_key(out, "visibility", &first, alloc, e_rr));
	gotoIfError3(clean, Json_raw(out, "[", alloc, e_rr));

	{
		Bool firstStage = true;

		for (U32 st = 0; st < EGfxPipelineStage_Count && st < 32; ++st)
			if (b.visibility & ((U32)1 << st)) {
				gotoIfError3(clean, Json_next(out, &firstStage, alloc, e_rr));
				gotoIfError3(clean, Json_cstr(out, SHEntry_stageNames[st], alloc, e_rr));
			}
	}

	gotoIfError3(clean, Json_raw(out, "]", alloc, e_rr));

	gotoIfError3(clean, Json_key(out, "bindings", &first, alloc, e_rr));
	gotoIfError3(clean, Json_raw(out, "{", alloc, e_rr));

	for (U32 t = 0; t < EGfxBinaryType_Count; ++t)
		gotoIfError3(clean, Json_fmt(
			out, alloc, e_rr, "%s\"%s\":{\"space\":%"PRIu32",\"binding\":%"PRIu32"}",
			t ? "," : "", spBinaryKeys[t], b.bindings.arr[t].space, b.bindings.arr[t].binding
		));

	gotoIfError3(clean, Json_raw(out, "}", alloc, e_rr));

	if (isSampler) {
		gotoIfError3(clean, Json_key(out, "samplerId", &first, alloc, e_rr));
		gotoIfError3(clean, Json_fmt(out, alloc, e_rr, "%"PRIu32, b.samplerId));
	}

	else if (isTexture && isWrite) {
		gotoIfError3(clean, Json_key(out, "texture", &first, alloc, e_rr));
		gotoIfError3(clean, Json_fmt(
			out, alloc, e_rr, "{\"primitive\":%u,\"formatId\":%u}", (U32) b.texture.primitive, (U32) b.texture.formatId
		));
	}

	else {
		gotoIfError3(clean, Json_key(out, "strideOrLength", &first, alloc, e_rr));
		gotoIfError3(clean, Json_fmt(out, alloc, e_rr, "%"PRIu32, b.strideOrLength));
	}

	gotoIfError3(clean, Json_raw(out, "}", alloc, e_rr));

clean:
	return s_uccess;
}

//One embedded oiPL: its rows, the samplers the rows index into, and the push constant range if it has one

static Bool WasmJson_plFile(const PLFile *layout, CharString *out, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;
	Bool first = true;

	gotoIfError3(clean, Json_raw(out, "{", alloc, e_rr));

	gotoIfError3(clean, Json_key(out, "bindings", &first, alloc, e_rr));
	gotoIfError3(clean, Json_raw(out, "[", alloc, e_rr));

	for (U64 i = 0; i < layout->bindings.length; ++i) {

		if(i)
			gotoIfError3(clean, Json_raw(out, ",", alloc, e_rr));

		gotoIfError3(clean, WasmJson_plBinding(layout, layout->bindings.ptr[i], out, alloc, e_rr));
	}

	gotoIfError3(clean, Json_raw(out, "]", alloc, e_rr));

	gotoIfError3(clean, Json_key(out, "samplers", &first, alloc, e_rr));
	gotoIfError3(clean, Json_raw(out, "[", alloc, e_rr));

	for (U64 i = 0; i < layout->samplers.length; ++i) {

		const PLSamplerInfo sm = layout->samplers.ptr[i];

		gotoIfError3(clean, Json_fmt(
			out, alloc, e_rr,
			"%s{\"filter\":%u,\"addressU\":%u,\"addressV\":%u,\"addressW\":%u,\"aniso\":%u,\"borderColor\":%u,"
			"\"comparisonFunction\":%u,\"enableComparison\":%s,\"mipBias\":%g,\"minLod\":%g,\"maxLod\":%g}",
			i ? "," : "",
			(U32) sm.filter, (U32) sm.addressU, (U32) sm.addressV, (U32) sm.addressW, (U32) sm.aniso,
			(U32) sm.borderColor, (U32) sm.comparisonFunction, sm.enableComparison ? "true" : "false",
			(F64) F32_castF16(sm.mipBias), (F64) F32_castF16(sm.minLod), (F64) F32_castF16(sm.maxLod)
		));
	}

	gotoIfError3(clean, Json_raw(out, "]", alloc, e_rr));

	gotoIfError3(clean, Json_key(out, "pushConstant", &first, alloc, e_rr));

	if(layout->hasPushConstant) {
		gotoIfError3(clean, WasmJson_plBinding(layout, layout->pushConstant, out, alloc, e_rr));
	}

	else {
		gotoIfError3(clean, Json_raw(out, "null", alloc, e_rr));
	}

	gotoIfError3(clean, Json_raw(out, "}", alloc, e_rr));

clean:
	return s_uccess;
}

static U64 WasmJson_bitCount(U16 mask) {

	U64 count = 0;

	for(U8 i = 0; i < 16; ++i)
		count += (mask >> i) & 1;

	return count;
}

//A string id of U32_MAX is how the pools spell "unnamed", which reaches the page as an empty string rather than
// as an index it would have to know the sentinel of.

static CharString WasmJson_spString(const SPFile *file, U32 id) {
	return id == U32_MAX || id >= file->names.entryStrings.length ?
		CharString_createNull() : file->names.entryStrings.ptr[id];
}

Bool WasmJson_spFile(
	const SPFile *file,
	CharString name,
	CharString sourceName,
	CharString *out,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;

	if(!file || !out)
		retError(clean, Error_nullPointer(!file ? 0 : 3, "WasmJson_spFile()::file and out are required"));

	gotoIfError3(clean, Json_raw(out, "{", alloc, e_rr));

	Bool first = true;

	gotoIfError3(clean, Json_key(out, "name", &first, alloc, e_rr));
	gotoIfError3(clean, Json_str(out, name, alloc, e_rr));

	gotoIfError3(clean, Json_key(out, "sourceName", &first, alloc, e_rr));
	gotoIfError3(clean, Json_str(out, sourceName, alloc, e_rr));

	//What a write would store rather than what the runtime state holds: a blend attachment only travels when
	// blending can reach it, and a vertex entry only when it carries something, so the counts are recomputed
	// here exactly as SPFile_write selects them.

	U64 blendAttachments = 0, vertexBuffers = 0, vertexAttributes = 0;

	for (U64 i = 0; i < file->graphicsStates.length; ++i) {

		SPGraphicsState state = file->graphicsStates.ptr[i];

		blendAttachments += SPBlendStateRuntime_storedAttachmentCount(state.blend);
		vertexBuffers += WasmJson_bitCount(SPVertexLayoutRuntime_bufferMask(state.inputAssembler.vertexLayout));
		vertexAttributes += WasmJson_bitCount(SPVertexLayoutRuntime_attributeMask(state.inputAssembler.vertexLayout));
	}

	gotoIfError3(clean, Json_key(out, "header", &first, alloc, e_rr));
	gotoIfError3(clean, Json_fmt(
		out, alloc, e_rr,
		"{\"version\":\"1.1\",\"counts\":{\"pipelines\":%"PRIu64",\"stages\":%"PRIu64",\"specializations\":%"PRIu64
		",\"graphicsStates\":%"PRIu64",\"raytracingStates\":%"PRIu64",\"blendAttachments\":%"PRIu64
		",\"vertexBuffers\":%"PRIu64",\"vertexAttributes\":%"PRIu64"}}",
		(U64) file->pipelines.length, (U64) file->stages.length, (U64) file->specializations.length,
		(U64) file->graphicsStates.length, (U64) file->raytracingStates.length,
		blendAttachments, vertexBuffers, vertexAttributes
	));

	gotoIfError3(clean, Json_key(out, "pipelines", &first, alloc, e_rr));
	gotoIfError3(clean, Json_raw(out, "[", alloc, e_rr));

	for (U64 i = 0; i < file->pipelines.length; ++i) {

		SPPipelineBase pipeline = file->pipelines.ptr[i];

		if(i)
			gotoIfError3(clean, Json_raw(out, ",", alloc, e_rr));

		gotoIfError3(clean, Json_raw(out, "{", alloc, e_rr));

		Bool firstField = true;

		gotoIfError3(clean, Json_key(out, "name", &firstField, alloc, e_rr));
		gotoIfError3(clean, Json_str(out, WasmJson_spString(file, pipeline.name), alloc, e_rr));

		gotoIfError3(clean, Json_key(out, "type", &firstField, alloc, e_rr));
		gotoIfError3(clean, Json_cstr(
			out, pipeline.type < ESPPipelineType_Count ? spPipelineTypeNames[pipeline.type] : "unknown", alloc, e_rr
		));

		//Into layouts below, or -1 for the device's default layout

		gotoIfError3(clean, Json_key(out, "layoutIndex", &firstField, alloc, e_rr));
		gotoIfError3(clean, Json_fmt(
			out, alloc, e_rr, "%"PRIi64, pipeline.layoutIndex == U32_MAX ? (I64) -1 : (I64) pipeline.layoutIndex
		));

		gotoIfError3(clean, Json_key(out, "flags", &firstField, alloc, e_rr));
		gotoIfError3(clean, Json_raw(out, "[", alloc, e_rr));

		Bool firstFlag = true;

		if (pipeline.flags & ESPPipelineFlag_GeneratedVertexStage) {
			gotoIfError3(clean, Json_next(out, &firstFlag, alloc, e_rr));
			gotoIfError3(clean, Json_cstr(out, "GeneratedVertexStage", alloc, e_rr));
		}

		if (pipeline.flags & ESPPipelineFlag_GeneratedPixelStage) {
			gotoIfError3(clean, Json_next(out, &firstFlag, alloc, e_rr));
			gotoIfError3(clean, Json_cstr(out, "GeneratedPixelStage", alloc, e_rr));
		}

		if (pipeline.flags & ESPPipelineFlag_AssumedHitGrouping) {
			gotoIfError3(clean, Json_next(out, &firstFlag, alloc, e_rr));
			gotoIfError3(clean, Json_cstr(out, "AssumedHitGrouping", alloc, e_rr));
		}

		gotoIfError3(clean, Json_raw(out, "]", alloc, e_rr));

		gotoIfError3(clean, Json_key(out, "stages", &firstField, alloc, e_rr));
		gotoIfError3(clean, Json_raw(out, "[", alloc, e_rr));

		for (U8 j = 0; j < pipeline.stageCount; ++j) {

			U64 stageId = (U64) pipeline.stageStart + j;

			if(stageId >= file->stages.length)
				break;

			SPStage stage = file->stages.ptr[stageId];

			//A stage with no shader name behind it is a stand in the pipeline generated rather than one the
			// shader declared, which the page marks so its disassembly is never read as the shader's own.

			Bool generated = stage.shaderFile == U32_MAX;

			gotoIfError3(clean, Json_raw(out, j ? ",{\"stage\":" : "{\"stage\":", alloc, e_rr));
			gotoIfError3(clean, Json_cstr(
				out, stage.stage < EGfxPipelineStage_Count ? SHEntry_stageNames[stage.stage] : "unknown", alloc, e_rr
			));
			gotoIfError3(clean, Json_raw(out, ",\"shaderFile\":", alloc, e_rr));
			gotoIfError3(clean, Json_str(out, WasmJson_spString(file, stage.shaderFile), alloc, e_rr));
			gotoIfError3(clean, Json_raw(out, ",\"entrypoint\":", alloc, e_rr));
			gotoIfError3(clean, Json_str(out, WasmJson_spString(file, stage.entrypoint), alloc, e_rr));
			gotoIfError3(clean, Json_fmt(out, alloc, e_rr, ",\"sourceHash\":%"PRIu32",\"generated\":", stage.sourceHash));
			gotoIfError3(clean, Json_bool(out, generated, alloc, e_rr));
			gotoIfError3(clean, Json_raw(out, "}", alloc, e_rr));
		}

		gotoIfError3(clean, Json_raw(out, "]", alloc, e_rr));

		//Every field reflection could not prove, with the value that would be used, where it came from, why it
		// could not be proven and which values are legal. The last two are static text keyed off the field, so
		// this is the only place the page has to read them from.

		gotoIfError3(clean, Json_key(out, "fields", &firstField, alloc, e_rr));
		gotoIfError3(clean, Json_raw(out, "[", alloc, e_rr));

		for (U32 j = 0; j < pipeline.specializationCount; ++j) {

			U64 specializationId = (U64) pipeline.specializationStart + j;

			if(specializationId >= file->specializations.length)
				break;

			SPSpecialization specialization = file->specializations.ptr[specializationId];
			ESPField field = (ESPField) specialization.field;

			gotoIfError3(clean, Json_raw(out, j ? ",{\"field\":" : "{\"field\":", alloc, e_rr));
			gotoIfError3(clean, Json_cstr(out, ESPField_name(field), alloc, e_rr));
			gotoIfError3(clean, Json_fmt(
				out, alloc, e_rr, ",\"index\":%"PRIu8",\"value\":%"PRIu32",\"source\":",
				specialization.index, specialization.value
			));
			
			gotoIfError3(clean, Json_cstr(
				out,
				specialization.source < ESPFieldSource_Count ? spFieldSourceNames[specialization.source] : "unknown",
				alloc, e_rr
			));

			gotoIfError3(clean, Json_raw(out, ",\"reason\":", alloc, e_rr));
			gotoIfError3(clean, Json_cstr(out, ESPField_reason(field), alloc, e_rr));
			gotoIfError3(clean, Json_raw(out, ",\"domain\":", alloc, e_rr));
			gotoIfError3(clean, Json_cstr(out, ESPField_domain(field), alloc, e_rr));
			gotoIfError3(clean, Json_raw(out, ",\"indexed\":", alloc, e_rr));
			gotoIfError3(clean, Json_bool(out, ESPField_isIndexed(field), alloc, e_rr));
			gotoIfError3(clean, Json_raw(out, "}", alloc, e_rr));
		}

		gotoIfError3(clean, Json_raw(out, "]", alloc, e_rr));

		//The prose a generated stage or an inferred hit grouping needs is what SPFile_print writes, which the
		// page shows as the `file data` card, so nothing is restated here.

		gotoIfError3(clean, Json_key(out, "notes", &firstField, alloc, e_rr));
		gotoIfError3(clean, Json_raw(out, "[]", alloc, e_rr));

		gotoIfError3(clean, Json_raw(out, "}", alloc, e_rr));
	}

	gotoIfError3(clean, Json_raw(out, "]", alloc, e_rr));

	//Every oiPL the file embeds, indexed by a pipeline's layoutIndex

	gotoIfError3(clean, Json_key(out, "layouts", &first, alloc, e_rr));
	gotoIfError3(clean, Json_raw(out, "[", alloc, e_rr));

	for (U64 i = 0; i < file->layouts.length; ++i) {

		if(i)
			gotoIfError3(clean, Json_raw(out, ",", alloc, e_rr));

		gotoIfError3(clean, WasmJson_plFile(&file->layouts.ptr[i], out, alloc, e_rr));
	}

	gotoIfError3(clean, Json_raw(out, "]", alloc, e_rr));

	gotoIfError3(clean, Json_raw(out, "}", alloc, e_rr));

clean:
	return s_uccess;
}
