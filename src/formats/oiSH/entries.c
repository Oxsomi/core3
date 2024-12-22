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

#ifndef DISALLOW_SH_OXC3_PLATFORMS
	#include "platforms/ext/listx_impl.h"
#else
	#include "types/container/list_impl.h"
#endif

#include "types/container/log.h"
#include "formats/oiSH/sh_file.h"
#include "types/math/math.h"

TListImpl(SHEntry);
TListImpl(SHEntryRuntime);

#ifndef DISALLOW_SH_OXC3_PLATFORMS

	#include "platforms/platform.h"

	void SHEntry_printx(SHEntry entry) { SHEntry_print(entry, Platform_instance->alloc); }
	void SHEntryRuntime_printx(SHEntryRuntime entry) { SHEntryRuntime_print(entry, Platform_instance->alloc); }

	void SHEntry_freex(SHEntry *entry) {
		SHEntry_free(entry, Platform_instance->alloc);
	}

	void SHEntryRuntime_freex(SHEntryRuntime *entry) {
		SHEntryRuntime_free(entry, Platform_instance->alloc);
	}

	void ListSHEntryRuntime_freeUnderlyingx(ListSHEntryRuntime *entry) {
		ListSHEntryRuntime_freeUnderlying(entry, Platform_instance->alloc);
	}

#endif

const C8 *SHEntry_stageNames[ESHPipelineStage_Count] = {

	"vertex",
	"pixel",
	"compute",
	"geometry",
	"hull",
	"domain",

	"raygeneration",
	"callable",
	"miss",
	"closesthit",
	"anyhit",
	"intersection",

	"mesh",
	"task",

	"node"
};

const C8 *SHEntry_stageName(SHEntry entry) {
	return SHEntry_stageNames[entry.stage];
}

const C8 *ESHPipelineStage_getStagePrefix(ESHPipelineStage stage) {

	const C8 *targetPrefix = "lib";

	switch (stage) {
		default:													break;
		case ESHPipelineStage_Vertex:		targetPrefix = "vs";	break;
		case ESHPipelineStage_Pixel:		targetPrefix = "ps";	break;
		case ESHPipelineStage_Compute:		targetPrefix = "cs";	break;
		case ESHPipelineStage_GeometryExt:	targetPrefix = "gs";	break;
		case ESHPipelineStage_Hull:			targetPrefix = "hs";	break;
		case ESHPipelineStage_Domain:		targetPrefix = "ds";	break;
		case ESHPipelineStage_MeshExt:		targetPrefix = "ms";	break;
		case ESHPipelineStage_TaskExt:		targetPrefix = "as";	break;
	}

	return targetPrefix;
}

Bool SHFile_addEntrypoint(SHFile *shFile, SHEntry *entry, Allocator alloc, Error *e_rr) {

	Bool s_uccess = true;

	CharString copy = CharString_createNull();
	CharString oldName = copy;

	ListCharString oldSemantics = (ListCharString) { 0 };
	ListCharString copySemantics = (ListCharString) { 0 };

	ListU16 copyBinaryIds = (ListU16) { 0 };
	ListU16 oldBinaryIds = copyBinaryIds;

	if (!shFile || !shFile->entries.ptr || !entry)
		retError(clean, Error_nullPointer(!shFile ? 0 : 1, "SHFile_addEntrypoint()::shFile and entry are required"))

	if(shFile->entries.length + 1 >= U16_MAX)
		retError(clean, Error_outOfBounds(
			0, U16_MAX, U16_MAX, "SHFile_addEntrypoint() requires entries to not exceed U16_MAX"
		))

	if(!CharString_length(entry->name))
		retError(clean, Error_nullPointer(1, "SHFile_addEntrypoint()::entry->name is required"))

	if(entry->stage >= ESHPipelineStage_Count)
		retError(clean, Error_invalidEnum(
			1, entry->stage, ESHPipelineStage_Count, "SHFile_addEntrypoint()::entry->stage invalid enum"
		))

	for(U64 i = 0; i < entry->binaryIds.length; ++i)
		if(entry->binaryIds.ptr[i] >= shFile->binaries.length)
			retError(clean, Error_outOfBounds(
				0, i, entry->binaryIds.ptr[i], "SHFile_addEntrypoint() binaryIds[i] out of bounds"
			))

	if(entry->binaryIds.length >= U8_MAX)
		retError(clean, Error_outOfBounds(
			0, entry->binaryIds.length, U8_MAX, "SHFile_addEntrypoint() binaryIds.length out of bounds"
		))

	for(U8 i = 0; i < 4; ++i)
		if(((entry->waveSize >> (i << 2)) & 0xF) > 9)
			retError(clean, Error_outOfBounds(
				0, (entry->waveSize >> (i << 2)) & 0xF, 9, "SHFile_addEntrypoint() waveSize out of bounds"
			))

	if(entry->waveSize && entry->stage != ESHPipelineStage_Compute && entry->stage != ESHPipelineStage_WorkgraphExt)
		retError(clean, Error_invalidOperation(0, "SHFile_addEntrypoint() defined WaveSize, but wasn't a workgraph or compute"))

	Bool hasRt = entry->stage >= ESHPipelineStage_RtStartExt && entry->stage <= ESHPipelineStage_RtEndExt;
	Bool hasCompute = false;
	Bool hasGraphics = false;

	switch (entry->stage) {

		case ESHPipelineStage_MeshExt:
		case ESHPipelineStage_TaskExt:
			hasGraphics = true;
			// fallthrough

		case ESHPipelineStage_WorkgraphExt:
		case ESHPipelineStage_Compute:
			hasCompute = true;
			break;
	}

	hasGraphics |= !hasCompute && !hasRt;

	U16 groupXYZ = entry->groupX | entry->groupY | entry->groupZ;
	U64 totalGroup = (U64)entry->groupX * entry->groupY * entry->groupZ;

	if(!hasCompute && groupXYZ)
		retError(clean, Error_invalidOperation(2, "SHFile_addEntrypoint() can't have group size for non compute"))

	if(hasCompute && !totalGroup)
		retError(clean, Error_invalidOperation(2, "SHFile_addEntrypoint() needs group size for compute"))

	if(totalGroup > 512)
		retError(clean, Error_invalidOperation(2, "SHFile_addEntrypoint() group count out of bounds (512)"))

	if(U16_max(entry->groupX, entry->groupY) > 512)
		retError(clean, Error_invalidOperation(2, "SHFile_addEntrypoint() group count x or y out of bounds (512)"))

	if(entry->groupZ > 64)
		retError(clean, Error_invalidOperation(2, "SHFile_addEntrypoint() group count z out of bounds (64)"))

	if(
		entry->stage == ESHPipelineStage_ClosestHitExt ||
		entry->stage == ESHPipelineStage_AnyHitExt ||
		entry->stage == ESHPipelineStage_IntersectionExt ||
		entry->stage == ESHPipelineStage_MissExt
	) {

		if(!entry->payloadSize)
			retError(clean, Error_invalidOperation(
				2, "SHFile_addEntrypoint() payloadSize is required for hit/intersection/miss shaders"
			))

		if(entry->payloadSize > 128)
			retError(clean, Error_outOfBounds(
				0, entry->payloadSize, 128, "SHFile_addEntrypoint() payloadSize must be <=128"
			))
	}

	else if(entry->payloadSize)
		retError(clean, Error_invalidOperation(
			2, "SHFile_addEntrypoint() payloadSize is only required for hit/intersection/miss shaders"
		))

	if(!entry->intersectionSize) {

		if(
			entry->stage == ESHPipelineStage_IntersectionExt ||
			entry->stage == ESHPipelineStage_ClosestHitExt ||
			entry->stage == ESHPipelineStage_AnyHitExt
		)
			retError(clean, Error_invalidOperation(
				2, "SHFile_addEntrypoint() intersectionSize is required for intersection/hit shaders"
			))

		if(entry->intersectionSize > 32)
			retError(clean, Error_outOfBounds(
				0, entry->intersectionSize, 32, "SHFile_addEntrypoint() intersectionSize must be <=32"
			))
	}

	else if(
		entry->stage != ESHPipelineStage_IntersectionExt &&
		entry->stage != ESHPipelineStage_ClosestHitExt &&
		entry->stage != ESHPipelineStage_AnyHitExt
	)
		retError(clean, Error_invalidOperation(
			2, "SHFile_addEntrypoint() intersectionSize is only required for intersection/hit shaders"
		))

	if (
		!hasGraphics &&
		(entry->outputsU64[0] | entry->outputsU64[1] | entry->inputsU64[0] | entry->inputsU64[1])
	)
		retError(clean, Error_invalidOperation(
			3, "SHFile_addEntrypoint() outputsU64 and inputsU64 are only for graphics shaders"
		))

	//Check validity of inputs and outputs

	U8 inputs = 0, outputs = 0;

	if(entry->outputsU64[0] | entry->outputsU64[1] | entry->inputsU64[0] | entry->inputsU64[1])
		for (U8 i = 0; i < 16; ++i) {

			U8 vout = entry->outputs[i];
			U8 vin = entry->inputs[i];

			if(vout)
				outputs = i + 1;

			if(vin)
				inputs = i + 1;

			if(
				(vout && (
					ESBType_getPrimitive(vout) == ESBPrimitive_Invalid ||
					ESBType_getStride(vout) == ESBStride_X8 ||
					ESBType_getMatrix(vout)
				)) ||
				(vin  && (
					ESBType_getPrimitive(vin)  == ESBPrimitive_Invalid ||
					ESBType_getStride(vin)  == ESBStride_X8 ||
					ESBType_getMatrix(vin)
				))
			)
				retError(clean, Error_invalidOperation(
					3, "SHFile_addEntrypoint() outputs or inputs contains an invalid parameter"
				))
		}

	//Detect semantic duplicates and verify if the strings exist

	if(entry->uniqueInputSemantics >= 16)
		retError(clean, Error_invalidOperation(
			0, "SHFile_addEntrypoint() inputs semantics out of bounds"
		))

	if(entry->semanticNames.length < entry->uniqueInputSemantics)
		retError(clean, Error_invalidOperation(
			0, "SHFile_addEntrypoint() inputs semantic name out of bounds"
		))

	U64 uniqueOutputSemantics = entry->semanticNames.length - entry->uniqueInputSemantics;

	if(uniqueOutputSemantics >= 16)
		retError(clean, Error_invalidOperation(
			0, "SHFile_addEntrypoint() output semantics out of bounds"
		))

	for(U64 i = 0; i < entry->uniqueInputSemantics; ++i)
		for(U64 j = 0; j < i; ++j)
			if(CharString_equalsStringInsensitive(entry->semanticNames.ptr[i], entry->semanticNames.ptr[j]))
				retError(clean, Error_invalidOperation(
					0, "SHFile_addEntrypoint() found duplicate in input unique semantic names"
				))

	for(U64 i = entry->uniqueInputSemantics; i < entry->semanticNames.length; ++i)
		for(U64 j = entry->uniqueInputSemantics; j < i; ++j)
			if(CharString_equalsStringInsensitive(entry->semanticNames.ptr[i], entry->semanticNames.ptr[j]))
				retError(clean, Error_invalidOperation(
					0, "SHFile_addEntrypoint() found duplicate in output unique semantic names"
				))

	U32 presentMask[16] = { 0 };

	Bool anyInputSemanticName = entry->inputSemanticNamesU64[0] || entry->inputSemanticNamesU64[1];

	for (U8 i = 0; i < inputs; ++i) {

		U8 input = entry->inputSemanticNames[i];

		if(input && !entry->inputs[i])
			retError(clean, Error_invalidState(
				0, "SHFile_addEntrypoint() semantic defined, but input at slot was empty"
			))

		if((input >> 4) > entry->uniqueInputSemantics)
			retError(clean, Error_outOfBounds(
				0, input >> 4, entry->uniqueInputSemantics, "SHFile_addEntrypoint() semantic name out of bounds"
			))

		if(anyInputSemanticName && (presentMask[input >> 4] >> (input & 0xF)) & 1)
			retError(clean, Error_invalidState(
				0, "SHFile_addEntrypoint() semantic was already present"
			))

		presentMask[input >> 4] |= 1 << (input & 0xF);
	}

	for(U8 i = inputs; i < 16; ++i)
		if(entry->inputSemanticNames[i])
			retError(clean, Error_invalidState(
				0, "SHFile_addEntrypoint() input semantic defined but not present"
			))

	for(U8 i = 0; i < 16; ++i)		//Avoid overlap between input and output semantics
		presentMask[i] = 0;

	Bool anyOutputSemanticName = entry->outputSemanticNamesU64[0] || entry->outputSemanticNamesU64[1];

	for (U8 i = 0; i < outputs; ++i) {

		U8 output = entry->outputSemanticNames[i];

		if(output && !entry->outputs[i])
			retError(clean, Error_invalidState(
				0, "SHFile_addEntrypoint() semantic defined, but output at slot was empty"
			))

		if((output >> 4) > uniqueOutputSemantics)
			retError(clean, Error_outOfBounds(
				1, output >> 4, uniqueOutputSemantics, "SHFile_addEntrypoint() semantic name out of bounds"
			))

		if(anyOutputSemanticName && (presentMask[output >> 4] >> (16 + (output & 0xF))) & 1)
			retError(clean, Error_invalidState(
				1, "SHFile_addEntrypoint() semantic was already present"
			))

		presentMask[output >> 4] |= 1 << (16 + (output & 0xF));
	}

	for(U8 i = outputs; i < 16; ++i)
		if(entry->outputSemanticNames[i])
			retError(clean, Error_invalidState(
				0, "SHFile_addEntrypoint() output semantic defined but not present"
			))

	oldSemantics = entry->semanticNames;
	entry->semanticNames = (ListCharString) { 0 };

	gotoIfError3(clean, ListCharString_move(&oldSemantics, alloc, &entry->semanticNames, e_rr))

	oldName = entry->name;

	if (CharString_isRef(entry->name)) {
		gotoIfError2(clean, CharString_createCopy(entry->name, alloc, &copy))
		entry->name = copy;
	}

	oldBinaryIds = entry->binaryIds;

	if (ListU16_isRef(entry->binaryIds)) {
		gotoIfError2(clean, ListU16_createCopy(entry->binaryIds, alloc, &copyBinaryIds))
		entry->binaryIds = copyBinaryIds;
	}

	gotoIfError2(clean, ListSHEntry_pushBack(&shFile->entries, *entry, alloc))

	if(!Buffer_isAscii(CharString_bufferConst(entry->name)))
		shFile->flags |= ESHSettingsFlags_IsUTF8;

	*entry = (SHEntry) { 0 };

clean:

	if (!s_uccess && oldSemantics.ptr) {
		ListCharString_freeUnderlying(&copySemantics, alloc);
		entry->semanticNames = oldSemantics;
	}

	if(!s_uccess && copy.ptr) {
		CharString_free(&copy, alloc);
		entry->name = oldName;
	}

	if(!s_uccess && copyBinaryIds.ptr) {
		ListU16_free(&copyBinaryIds, alloc);
		entry->binaryIds = oldBinaryIds;
	}

	return s_uccess;
}

U32 SHEntryRuntime_getCombinations(SHEntryRuntime runtime) {
	return (U32)(
		U64_max(runtime.shaderVersions.length, 1) *
		U64_max(runtime.extensions.length, 1) *
		U64_max(runtime.uniformsPerCompilation.length, 1)
	);
}

Bool SHEntryRuntime_asBinaryIdentifier(
	SHEntryRuntime runtime,
	U16 combinationId,
	SHBinaryIdentifier *binaryIdentifier,
	Error *e_rr
) {

	Bool s_uccess = true;

	if(!binaryIdentifier)
		retError(clean, Error_nullPointer(2, "SHEntryRuntime_asBinaryIdentifier()::binaryIdentifier is required"))

	U16 shaderVersions = (U16) U64_max(runtime.shaderVersions.length, 1);
	U16 extensions = (U16) U64_max(runtime.extensions.length, 1);
	U16 uniforms = (U16) U64_max(runtime.uniformsPerCompilation.length, 1);

	U16 shaderVersion = combinationId % shaderVersions;
	combinationId /= shaderVersions;

	U16 extensionId = combinationId % extensions;
	combinationId /= extensions;

	U16 uniformId = combinationId % uniforms;

	if(combinationId >= uniforms)
		retError(clean, Error_outOfBounds(
			0, combinationId, uniforms,
			"SHEntryRuntime_asBinaryIdentifier()::combinationId out of bounds"
		))

	*binaryIdentifier = (SHBinaryIdentifier) {
		.entrypoint = runtime.isShaderAnnotation ? CharString_createNull() : CharString_createRefStrConst(runtime.entry.name),
		.extensions = runtime.extensions.length ? runtime.extensions.ptr[extensionId] : 0,
		.shaderVersion = runtime.shaderVersions.length ? runtime.shaderVersions.ptr[shaderVersion] : OISH_SHADER_MODEL(6, 5),
		.stageType = runtime.entry.stage
	};

	//Combine raytracing shaders, since they don't have different configs
	if(
		binaryIdentifier->stageType >= ESHPipelineStage_RtStartExt &&
		binaryIdentifier->stageType <= ESHPipelineStage_RtEndExt
	)
		binaryIdentifier->stageType = ESHPipelineStage_RtStartExt;

	if(runtime.uniformsPerCompilation.length) {

		U64 uniformOffset = 0;

		for(U64 i = 0; i < uniformId; ++i)
			uniformOffset += runtime.uniformsPerCompilation.ptr[i];

		if(runtime.uniformsPerCompilation.ptr[uniformId])
			gotoIfError2(clean, ListCharString_createRefConst(
				runtime.uniformNameValues.ptr + (uniformOffset << 1),
				(U64)runtime.uniformsPerCompilation.ptr[uniformId] << 1,
				&binaryIdentifier->uniforms
			))
	}

clean:
	return s_uccess;
}

Bool SHEntryRuntime_asBinaryInfo(
	SHEntryRuntime runtime,
	U16 combinationId,
	ESHBinaryType binaryType,
	Buffer buf,
	ESHExtension dormantExtensions,
	SHBinaryInfo *binaryInfo,
	Error *e_rr
) {

	Bool s_uccess = true;

	if(binaryType >= ESHBinaryType_Count)
		retError(clean, Error_invalidEnum(
			2, binaryType, ESHBinaryType_Count, "SHEntryRuntime_asBinaryInfo()::binaryType out of bounds"
		))

	if(!binaryInfo)
		retError(clean, Error_nullPointer(0, "SHEntryRuntime_asBinaryInfo()::binaryInfo is required"))

	gotoIfError3(clean, SHEntryRuntime_asBinaryIdentifier(runtime, combinationId, &binaryInfo->identifier, e_rr))

	binaryInfo->dormantExtensions = dormantExtensions;
	binaryInfo->vendorMask = runtime.vendorMask;
	binaryInfo->hasShaderAnnotation = runtime.isShaderAnnotation;
	binaryInfo->binaries[binaryType] = Buffer_createRefFromBuffer(buf, true);

clean:
	return s_uccess;
}

void SHEntry_print(SHEntry shEntry, Allocator alloc) {

	const C8 *name = SHEntry_stageName(shEntry);

	Log_debugLn(alloc, "Entry (%s): %.*s", name, (int) CharString_length(shEntry.name), shEntry.name.ptr);

	switch(shEntry.stage) {

		default: {

			Bool hasSemantics =
				shEntry.inputSemanticNamesU64[0] || shEntry.inputSemanticNamesU64[1] |
				shEntry.outputSemanticNamesU64[0] || shEntry.outputSemanticNamesU64[1];

			Bool hasInputs = shEntry.inputsU64[0] || shEntry.inputsU64[1];
			Bool hasOutputs = shEntry.outputsU64[0] | shEntry.outputsU64[1];
			U8 value = (hasInputs ? 1 : 0) | (hasOutputs ? 2 : 0);

			for(U64 j = 0; j < 2; ++j)
				if ((value >> j) & 1) {

					Log_debugLn(alloc, j ? "\tOutputs:" : "\tInputs:");

					for (U8 i = 0; i < 16; ++i) {

						ESBType type = (ESBType)(j ? shEntry.outputs[i] : shEntry.inputs[i]);

						if(!type)
							continue;

						U8 semanticValue = j ? shEntry.outputSemanticNames[i] : shEntry.inputSemanticNames[i];
						U64 semanticNameId = semanticValue >> 4;
						U64 semanticOff = j ? shEntry.uniqueInputSemantics : 0;

						CharString semantic =
							semanticNameId ? shEntry.semanticNames.ptr[semanticNameId - 1 + semanticOff] : (
								j && shEntry.stage == ESHPipelineStage_Pixel ? CharString_createRefCStrConst("SV_TARGET") :
								CharString_createRefCStrConst("TEXCOORD")
								);

						Log_debugLn(
							alloc,
							"\t\t%"PRIu8" %s : %.*s%"PRIu8,
							i,
							ESBType_name(type),
							(int) CharString_length(semantic),
							semantic.ptr,
							hasSemantics ? (U8) (semanticValue & 0xF) : i
						);
					}
				}

			if(shEntry.stage != ESHPipelineStage_MeshExt && shEntry.stage != ESHPipelineStage_TaskExt)
				break;
		}

		// fallthrough

		case ESHPipelineStage_Compute:
		case ESHPipelineStage_WorkgraphExt:

			Log_debugLn(
				alloc,
				"\tThread count: %"PRIu16", %"PRIu16", %"PRIu16,
				shEntry.groupX, shEntry.groupY, shEntry.groupZ
			);

			for (U8 j = 0; j < 4; ++j) {

				const C8 *formats[] = {
					"\tWaveSize.required: %"PRIu8,
					"\tWaveSize.min: %"PRIu8,
					"\tWaveSize.max: %"PRIu8,
					"\tWaveSize.recommended: %"PRIu8,
				};

				U8 curr = (shEntry.waveSize >> (j << 2)) & 0xF;

				if(curr)
					Log_debugLn(alloc, formats[j], curr);
			}

			break;

		case ESHPipelineStage_RaygenExt:
		case ESHPipelineStage_CallableExt:
			break;

		case ESHPipelineStage_ClosestHitExt:
		case ESHPipelineStage_AnyHitExt:
		case ESHPipelineStage_IntersectionExt:
			Log_debugLn(alloc, "\tIntersection size: %"PRIu8, shEntry.intersectionSize);
			// fall through

		case ESHPipelineStage_MissExt:
			Log_debugLn(alloc, "\tPayload size: %"PRIu8, shEntry.payloadSize);
			break;
	}
}

void SHEntryRuntime_print(SHEntryRuntime entry, Allocator alloc) {

	SHEntry_print(entry.entry, alloc);

	for(U64 i = 0; i < entry.shaderVersions.length; ++i) {
		U16 shaderVersion = entry.shaderVersions.ptr[i];
		Log_debugLn(alloc, "\t[[oxc::model(%"PRIu8".%"PRIu8")]]", (U8)(shaderVersion >> 8), (U8) shaderVersion);
	}

	for (U64 i = 0; i < entry.extensions.length; ++i) {

		ESHExtension exts = entry.extensions.ptr[i];

		if(!exts)
			Log_debugLn(alloc, "\t[[oxc::extension()]]");
		
		else {

			Log_debug(alloc, ELogOptions_None, "\t[[oxc::extension(");

			Bool prev = false;

			for(U64 j = 0; j < ESHExtension_Count; ++j)
				if((exts >> j) & 1) {
					Log_debug(alloc, ELogOptions_None, "%s\"%s\"", prev ? ", " : "", ESHExtension_names[j]);
					prev = true;
				}

			Log_debug(alloc, ELogOptions_NewLine, ")]]");
		}
	}

	for (U64 i = 0, k = 0; i < entry.uniformsPerCompilation.length; ++i) {

		U8 uniforms = entry.uniformsPerCompilation.ptr[i];

		if(!uniforms) {
			Log_debugLn(alloc, "\t[[oxc::uniforms()]]");
			continue;
		}

		Log_debug(alloc, ELogOptions_None, "\t[[oxc::uniforms(");

		Bool prev = false;

		for(U64 j = 0; j < uniforms; ++j) {

			CharString name = entry.uniformNameValues.ptr[(j + k) << 1];
			CharString value = entry.uniformNameValues.ptr[((j + k) << 1) | 1];

			Log_debug(
				alloc, ELogOptions_None,
				CharString_length(value) ? "%s\"%.*s\" = \"%.*s\"" : "%s\"%.*s\"",
				prev ? ", " : "",
				(int)CharString_length(name), name.ptr,
				(int)CharString_length(value), value.ptr
			);

			prev = true;
		}

		Log_debug(alloc, ELogOptions_NewLine, ")]]");

		k += uniforms;
	}

	if(entry.vendorMask == U16_MAX)
		Log_debugLn(alloc, "\t[[oxc::vendor()]] //(any vendor)");

	else for(U64 i = 0; i < ESHVendor_Count; ++i)
		if((entry.vendorMask >> i) & 1)
			Log_debugLn(alloc, "\t[[oxc::vendor(\"%s\")]]", ESHVendor_names[i]);

	const C8 *name = SHEntry_stageName(entry.entry);
	Log_debugLn(alloc, entry.isShaderAnnotation ? "\t[shader(\"%s\")]" : "\t[[oxc::stage(\"%s\")]]", name);
}

void SHEntry_free(SHEntry *entry, Allocator alloc) {

	if(!entry)
		return;

	CharString_free(&entry->name, alloc);
	ListCharString_freeUnderlying(&entry->semanticNames, alloc);
}

void SHEntryRuntime_free(SHEntryRuntime *entry, Allocator alloc) {

	if(!entry)
		return;

	SHEntry_free(&entry->entry, alloc);
	ListU16_free(&entry->shaderVersions, alloc);
	ListU32_free(&entry->extensions, alloc);
	ListU8_free(&entry->uniformsPerCompilation, alloc);
	ListCharString_freeUnderlying(&entry->uniformNameValues, alloc);
}

void ListSHEntry_freeUnderlying(ListSHEntry *entry, Allocator alloc) {

	if(!entry)
		return;

	for(U64 i = 0; i < entry->length; ++i)
		SHEntry_free(&entry->ptrNonConst[i], alloc);

	ListSHEntry_free(entry, alloc);
}

void ListSHEntryRuntime_freeUnderlying(ListSHEntryRuntime *entry, Allocator alloc) {

	if(!entry)
		return;

	for(U64 i = 0; i < entry->length; ++i)
		SHEntryRuntime_free(&entry->ptrNonConst[i], alloc);

	ListSHEntryRuntime_free(entry, alloc);
}