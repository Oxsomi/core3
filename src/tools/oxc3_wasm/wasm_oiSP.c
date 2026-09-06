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
#include "formats/oiSH/sh_entries.h"
#include <inttypes.h>

static const C8 *spPipelineTypeNames[ESPPipelineType_Count] = { "compute", "graphics", "raytracing" };
static const C8 *spFieldSourceNames[ESPFieldSource_Count] = { "derived", "supplied", "assumed" };

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
				out, stage.stage < ESHPipelineStage_Count ? SHEntry_stageNames[stage.stage] : "unknown", alloc, e_rr
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

	gotoIfError3(clean, Json_raw(out, "}", alloc, e_rr));

clean:
	return s_uccess;
}
