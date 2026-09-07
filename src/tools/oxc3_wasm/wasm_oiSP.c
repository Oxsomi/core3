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
	const PLFile *layout, PLDescriptorBinding b, JsonWriter *w, Error *e_rr
) {

	Bool s_uccess = true;

	const EGfxRegisterType base = (EGfxRegisterType)(b.registerType & EGfxRegisterType_TypeMask);
	const Bool isWrite = (b.registerType & EGfxRegisterType_IsWrite) != 0;
	const Bool isSampler = base == EGfxRegisterType_Sampler || base == EGfxRegisterType_SamplerComparisonState;
	const Bool isTexture = base >= EGfxRegisterType_Texture1D && base <= EGfxRegisterType_Texture2DMS;
	const U32 nameId = PLDescriptorBinding_name(b);
	const EPLSource source = PLDescriptorBinding_source(b);

	gotoIfError3(clean, JsonWriter_beginObject(w, e_rr));

	gotoIfError3(clean, JsonWriter_key(w, "name", e_rr));

	if(nameId != PLDescriptorBinding_NAME_NONE && nameId < layout->names.entryStrings.length) {
		gotoIfError3(clean, JsonWriter_str(w, layout->names.entryStrings.ptr[nameId], e_rr));
	}

	else {
		gotoIfError3(clean, JsonWriter_null(w, e_rr));
	}

	gotoIfError3(clean, JsonWriter_keyCstr(
		w, "source", (U32) source < ESPFieldSource_Count ? spFieldSourceNames[source] : "unknown", e_rr
	));

	gotoIfError3(clean, JsonWriter_keyCstr(w, "class", WasmJson_registerClass(base), e_rr));

	gotoIfError3(clean, JsonWriter_keyCstr(w, "type", WasmJson_registerBaseName(base, isWrite), e_rr));

	gotoIfError3(clean, JsonWriter_keyBool(w, "isWrite", isWrite, e_rr));

	gotoIfError3(clean, JsonWriter_keyBool(w, "isArray", (b.registerType & EGfxRegisterType_IsArray) != 0, e_rr));

	gotoIfError3(clean, JsonWriter_keyU64(w, "count", b.count, e_rr));

	//The stages that see the row, by name, since the page never reasons about the mask itself

	gotoIfError3(clean, JsonWriter_keyArray(w, "visibility", e_rr));

	{

		for (U32 st = 0; st < EGfxPipelineStage_Count && st < 32; ++st)
			if (b.visibility & ((U32)1 << st)) {
				gotoIfError3(clean, JsonWriter_cstr(w, SHEntry_stageNames[st], e_rr));
			}
	}

	gotoIfError3(clean, JsonWriter_endArray(w, e_rr));

	gotoIfError3(clean, JsonWriter_keyObject(w, "bindings", e_rr));

	for (U32 t = 0; t < EGfxBinaryType_Count; ++t)
		gotoIfError3(clean, (
			JsonWriter_keyObject(w, spBinaryKeys[t], e_rr) &&
			JsonWriter_keyU64(w, "space", b.bindings.arr[t].space, e_rr) &&
			JsonWriter_keyU64(w, "binding", b.bindings.arr[t].binding, e_rr) &&
			JsonWriter_endObject(w, e_rr)
		));

	gotoIfError3(clean, JsonWriter_endObject(w, e_rr));

	if (isSampler) {
		gotoIfError3(clean, JsonWriter_keyU64(w, "samplerId", b.samplerId, e_rr));
	}

	else if (isTexture && isWrite) {
		gotoIfError3(clean, (
			JsonWriter_keyObject(w, "texture", e_rr) &&
			JsonWriter_keyU64(w, "primitive", (U32) b.texture.primitive, e_rr) &&
			JsonWriter_keyU64(w, "formatId", (U32) b.texture.formatId, e_rr) &&
			JsonWriter_endObject(w, e_rr)
		));
	}

	else {
		gotoIfError3(clean, JsonWriter_keyU64(w, "strideOrLength", b.strideOrLength, e_rr));
	}

	gotoIfError3(clean, JsonWriter_endObject(w, e_rr));

clean:
	return s_uccess;
}

//One embedded oiPL: its rows, the samplers the rows index into, and the push constant range if it has one

static Bool WasmJson_plFile(const PLFile *layout, JsonWriter *w, Error *e_rr) {

	Bool s_uccess = true;

	gotoIfError3(clean, JsonWriter_beginObject(w, e_rr));

	gotoIfError3(clean, JsonWriter_keyArray(w, "bindings", e_rr));

	for (U64 i = 0; i < layout->bindings.length; ++i) {

		gotoIfError3(clean, WasmJson_plBinding(layout, layout->bindings.ptr[i], w, e_rr));
	}

	gotoIfError3(clean, JsonWriter_endArray(w, e_rr));

	gotoIfError3(clean, JsonWriter_keyArray(w, "samplers", e_rr));

	for (U64 i = 0; i < layout->samplers.length; ++i) {

		const PLSamplerInfo sm = layout->samplers.ptr[i];

		gotoIfError3(clean, (
			JsonWriter_beginObject(w, e_rr) &&
			JsonWriter_keyU64(w, "filter", (U32) sm.filter, e_rr) &&
			JsonWriter_keyU64(w, "addressU", (U32) sm.addressU, e_rr) &&
			JsonWriter_keyU64(w, "addressV", (U32) sm.addressV, e_rr) &&
			JsonWriter_keyU64(w, "addressW", (U32) sm.addressW, e_rr) &&
			JsonWriter_keyU64(w, "aniso", (U32) sm.aniso, e_rr) &&
			JsonWriter_keyU64(w, "borderColor", (U32) sm.borderColor, e_rr) &&
			JsonWriter_keyU64(w, "comparisonFunction", (U32) sm.comparisonFunction, e_rr) &&
			JsonWriter_keyBool(w, "enableComparison", sm.enableComparison, e_rr) &&
			JsonWriter_keyF64(w, "mipBias", (F64) F32_castF16(sm.mipBias), e_rr) &&
			JsonWriter_keyF64(w, "minLod", (F64) F32_castF16(sm.minLod), e_rr) &&
			JsonWriter_keyF64(w, "maxLod", (F64) F32_castF16(sm.maxLod), e_rr) &&
			JsonWriter_endObject(w, e_rr)
		));
	}

	gotoIfError3(clean, JsonWriter_endArray(w, e_rr));

	gotoIfError3(clean, JsonWriter_key(w, "pushConstant", e_rr));

	if(layout->hasPushConstant) {
		gotoIfError3(clean, WasmJson_plBinding(layout, layout->pushConstant, w, e_rr));
	}

	else {
		gotoIfError3(clean, JsonWriter_null(w, e_rr));
	}

	gotoIfError3(clean, JsonWriter_endObject(w, e_rr));

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
	JsonWriter *w,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;

	(void) alloc;        //The three serializers share one signature; this one allocates nothing of its own

	if(!file || !w)
		retError(clean, Error_nullPointer(!file ? 0 : 3, "WasmJson_spFile()::file and w are required"));

	gotoIfError3(clean, JsonWriter_beginObject(w, e_rr));

	gotoIfError3(clean, JsonWriter_keyStr(w, "name", name, e_rr));

	gotoIfError3(clean, JsonWriter_keyStr(w, "sourceName", sourceName, e_rr));

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

	gotoIfError3(clean, (
		JsonWriter_keyObject(w, "header", e_rr) &&
		JsonWriter_keyCstr(w, "version", "1.1", e_rr) &&
		JsonWriter_keyObject(w, "counts", e_rr) &&
		JsonWriter_keyU64(w, "pipelines", (U64) file->pipelines.length, e_rr) &&
		JsonWriter_keyU64(w, "stages", (U64) file->stages.length, e_rr) &&
		JsonWriter_keyU64(w, "specializations", (U64) file->specializations.length, e_rr) &&
		JsonWriter_keyU64(w, "graphicsStates", (U64) file->graphicsStates.length, e_rr) &&
		JsonWriter_keyU64(w, "raytracingStates", (U64) file->raytracingStates.length, e_rr) &&
		JsonWriter_keyU64(w, "blendAttachments", blendAttachments, e_rr) &&
		JsonWriter_keyU64(w, "vertexBuffers", vertexBuffers, e_rr) &&
		JsonWriter_keyU64(w, "vertexAttributes", vertexAttributes, e_rr) &&
		JsonWriter_endObject(w, e_rr) &&
		JsonWriter_endObject(w, e_rr)
	));

	gotoIfError3(clean, JsonWriter_keyArray(w, "pipelines", e_rr));

	for (U64 i = 0; i < file->pipelines.length; ++i) {

		SPPipelineBase pipeline = file->pipelines.ptr[i];

		gotoIfError3(clean, JsonWriter_beginObject(w, e_rr));

		gotoIfError3(clean, JsonWriter_keyStr(w, "name", WasmJson_spString(file, pipeline.name), e_rr));

		gotoIfError3(clean, JsonWriter_key(w, "type", e_rr));
		gotoIfError3(clean, JsonWriter_cstr(
			w, pipeline.type < ESPPipelineType_Count ? spPipelineTypeNames[pipeline.type] : "unknown", e_rr
		));

		//Into layouts below, or -1 for the device's default layout

		gotoIfError3(clean, JsonWriter_keyI64(
			w, "layoutIndex", pipeline.layoutIndex == U32_MAX ? (I64) -1 : (I64) pipeline.layoutIndex, e_rr
		));

		gotoIfError3(clean, JsonWriter_keyArray(w, "flags", e_rr));

		if (pipeline.flags & ESPPipelineFlag_GeneratedVertexStage) {
			gotoIfError3(clean, JsonWriter_cstr(w, "GeneratedVertexStage", e_rr));
		}

		if (pipeline.flags & ESPPipelineFlag_GeneratedPixelStage) {
			gotoIfError3(clean, JsonWriter_cstr(w, "GeneratedPixelStage", e_rr));
		}

		if (pipeline.flags & ESPPipelineFlag_AssumedHitGrouping) {
			gotoIfError3(clean, JsonWriter_cstr(w, "AssumedHitGrouping", e_rr));
		}

		gotoIfError3(clean, JsonWriter_endArray(w, e_rr));

		gotoIfError3(clean, JsonWriter_keyArray(w, "stages", e_rr));

		for (U8 j = 0; j < pipeline.stageCount; ++j) {

			U64 stageId = (U64) pipeline.stageStart + j;

			if(stageId >= file->stages.length)
				break;

			SPStage stage = file->stages.ptr[stageId];

			//A stage with no shader name behind it is a stand in the pipeline generated rather than one the
			// shader declared, which the page marks so its disassembly is never read as the shader's own.

			Bool generated = stage.shaderFile == U32_MAX;

			gotoIfError3(clean, (JsonWriter_beginObject(w, e_rr) && JsonWriter_key(w, "stage", e_rr)));
			gotoIfError3(clean, JsonWriter_cstr(
				w, stage.stage < EGfxPipelineStage_Count ? SHEntry_stageNames[stage.stage] : "unknown", e_rr
			));
			gotoIfError3(clean, JsonWriter_keyStr(w, "shaderFile", WasmJson_spString(file, stage.shaderFile), e_rr));
			gotoIfError3(clean, JsonWriter_keyStr(w, "entrypoint", WasmJson_spString(file, stage.entrypoint), e_rr));
			gotoIfError3(clean, (
				JsonWriter_keyU64(w, "sourceHash", stage.sourceHash, e_rr) &&
				JsonWriter_keyBool(w, "generated", generated, e_rr)
			));
			gotoIfError3(clean, JsonWriter_endObject(w, e_rr));
		}

		gotoIfError3(clean, JsonWriter_endArray(w, e_rr));

		//Every field reflection could not prove, with the value that would be used, where it came from, why it
		// could not be proven and which values are legal. The last two are static text keyed off the field, so
		// this is the only place the page has to read them from.

		gotoIfError3(clean, JsonWriter_keyArray(w, "fields", e_rr));

		for (U32 j = 0; j < pipeline.specializationCount; ++j) {

			U64 specializationId = (U64) pipeline.specializationStart + j;

			if(specializationId >= file->specializations.length)
				break;

			SPSpecialization specialization = file->specializations.ptr[specializationId];
			ESPField field = (ESPField) specialization.field;

			gotoIfError3(clean, (
				JsonWriter_beginObject(w, e_rr) &&
				JsonWriter_keyCstr(w, "field", ESPField_name(field), e_rr)
			));
			gotoIfError3(clean, (
				JsonWriter_keyU64(w, "index", specialization.index, e_rr) &&
				JsonWriter_keyU64(w, "value", specialization.value, e_rr) &&
				JsonWriter_key(w, "source", e_rr)
			));

			gotoIfError3(clean, JsonWriter_cstr(
				w, specialization.source < ESPFieldSource_Count ? spFieldSourceNames[specialization.source] : "unknown", e_rr
			));

			gotoIfError3(clean, JsonWriter_keyCstr(w, "reason", ESPField_reason(field), e_rr));
			gotoIfError3(clean, JsonWriter_keyCstr(w, "domain", ESPField_domain(field), e_rr));
			gotoIfError3(clean, JsonWriter_keyBool(w, "indexed", ESPField_isIndexed(field), e_rr));
			gotoIfError3(clean, JsonWriter_endObject(w, e_rr));
		}

		gotoIfError3(clean, JsonWriter_endArray(w, e_rr));

		//The prose a generated stage or an inferred hit grouping needs is what SPFile_print writes, which the
		// page shows as the `file data` card, so nothing is restated here.

			gotoIfError3(clean, (JsonWriter_keyArray(w, "notes", e_rr) && JsonWriter_endArray(w, e_rr)));

		gotoIfError3(clean, JsonWriter_endObject(w, e_rr));
	}

	gotoIfError3(clean, JsonWriter_endArray(w, e_rr));

	//Every oiPL the file embeds, indexed by a pipeline's layoutIndex

	gotoIfError3(clean, JsonWriter_keyArray(w, "layouts", e_rr));

	for (U64 i = 0; i < file->layouts.length; ++i) {

		gotoIfError3(clean, WasmJson_plFile(&file->layouts.ptr[i], w, e_rr));
	}

	gotoIfError3(clean, JsonWriter_endArray(w, e_rr));

	gotoIfError3(clean, JsonWriter_endObject(w, e_rr));

clean:
	return s_uccess;
}
