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

#include "formats/oiSH/sh_file.h"
#include "formats/oiSH/sh_headers.h"
#include "formats/oiDL/dl_file.h"
#include "types/container/buffer.h"
#include "types/container/ref_ptr.h"
#include "types/container/types.h"
#include "types/container/stream.h"
#include "types/container/list_basic_types.h"
#include "types/base/mathi.h"
#include "types/base/allocator.h"
#include "types/base/error.h"
#include "types/base/c8.h"
#include "types/base/constants.h"
#include "types/base/string_read_helper.h"

#include <stddef.h>

Bool SHFile_read(StreamRef *streamRef, U64 *offset, Bool isSubFile, const Allocator *alloc, SHFile *shFile, Error *e_rr) {

	Bool s_uccess = true;
	Bool allocate = false;

	DLFile strings = (DLFile) { 0 };
	DLFile shaderBuffers = (DLFile) { 0 };
	ListSBFile parsedShaderBuffers = (ListSBFile) { 0 };
	SHEntry entry = (SHEntry) { 0 };
	SHBinaryInfo binaryInfo = (SHBinaryInfo) { 0 };
	SHInclude include = (SHInclude) { 0 };
	ListListU32 arrays = (ListListU32) { 0 };
	SBFile copySB = (SBFile) { 0 };
	StreamCursor cursor = (StreamCursor) { 0 };
	Buffer staticBuf = Buffer_createNull();
	Buffer arraysBuf = Buffer_createNull();
	Buffer tmp = Buffer_createNull();

	if (!shFile)
		retError(clean, Error_nullPointer(2, "SHFile_read()::shFile is required"));

	if (shFile->binaries.ptr)
		retError(clean, Error_invalidOperation(0, "SHFile_read()::shFile isn't empty, might indicate memleak"));

	if (!streamRef || !offset)
		retError(clean, Error_nullPointer(!streamRef ? 0 : 1, "SHFile_read()::streamRef and offset are required"));

	if (streamRef->refPtrType->typeId != (ETypeId)EContainerTypeId_Stream)
		retError(clean, Error_invalidOperation(0, "SHFile_read()::streamRef invalid type"));

	if (*offset & 15)
		retError(clean, Error_unsupportedOperation(0, "SHFile_read() at misaligned offset is unsupported (16-byte)"));

	gotoIfError3(clean, StreamCursor_create(streamRef, 0, false, alloc, &cursor, e_rr));

	if (!isSubFile) {		//Magic

		U32 magic = 0;
		gotoIfError3(clean, StreamCursor_consume(&cursor, offset, &magic, sizeof(magic), alloc, e_rr));

		if (magic != SHHeader_MAGIC)
			retError(clean, Error_invalidParameter(0, 0, "SHFile_read() requires magicNumber prefix"));
	}

	//Read header, hashStart points at SHHeader::uniqueDefines, skipping magic (if present) and pre-hash fields

	U64 hashStart = *offset + offsetof(SHHeader, uniqueDefines);

	SHHeader header = (SHHeader) { 0 };
	gotoIfError3(clean, StreamCursor_consume(&cursor, offset, &header, sizeof(header), alloc, e_rr));

	//Validate header

	if (header.version != ESHVersion_V1_2)
		retError(clean, Error_invalidParameter(0, 1, "SHFile_read() header.version is invalid"));

	if (!header.stageCount)
		retError(clean, Error_invalidParameter(0, 1, "SHFile_read() header.stageCount is invalid"));

	*offset = (*offset + 15) & ~15;

	//Read DLFile (strings)

	gotoIfError3(clean, DLFile_read(streamRef, offset, NULL, I32x4_zero(), true, alloc, NULL, &strings, e_rr));

	U64 minEntryBuffers =
		(U64)header.stageCount +
		header.includeFileCount +
		header.semanticCount +
		header.registerNameCount +
		header.uniformNameCount +
		header.uniqueDefines +
		(header.uniqueDefines ? 1 : 0);

	if (
		strings.entryBuffers.length < minEntryBuffers ||
		strings.settings.dataType != EDLDataType_String ||
		strings.settings.encryptionType ||
		strings.settings.compressionType
	)
		retError(clean, Error_invalidParameter(0, 1, "SHFile_read() strings didn't match expectations"));

	*offset = (*offset + 15) & ~15;

	//Read DLFile (shader buffers)

	gotoIfError3(clean, DLFile_read(streamRef, offset, NULL, I32x4_zero(), true, alloc, NULL, &shaderBuffers, e_rr));

	if (
		shaderBuffers.settings.dataType != EDLDataType_Data ||
		shaderBuffers.settings.encryptionType ||
		shaderBuffers.settings.compressionType
	)
		retError(clean, Error_invalidParameter(0, 1, "SHFile_read() shaderBuffers didn't match expectations"));

	gotoIfError3(clean, ListSBFile_resize(&parsedShaderBuffers, shaderBuffers.entryBuffers.length, alloc, e_rr));

	for (U64 i = 0; i < shaderBuffers.entryBuffers.length; ++i)
		gotoIfError3(clean, SBFile_read(
			streamRef, offset, true, alloc, &parsedShaderBuffers.ptrNonConst[i], e_rr
		));

	//Read static sized data

	U64 staticSize =
		header.binaryCount * sizeof(BinaryInfoFixedSize) +
		header.stageCount * sizeof(EntryInfoFixedSize) +
		header.includeFileCount * sizeof(U32) +
		header.arrayDimCount * sizeof(U8);

	gotoIfError3(clean, Buffer_createUninitializedBytes(staticSize, alloc, &staticBuf, e_rr));
	gotoIfError3(clean, StreamCursor_consume(&cursor, offset, staticBuf.ptrNonConst, staticSize, alloc, e_rr));

	const BinaryInfoFixedSize *fixedBinaryInfo = (const BinaryInfoFixedSize*) staticBuf.ptr;
	const EntryInfoFixedSize *fixedEntryInfo    = (const EntryInfoFixedSize*) (fixedBinaryInfo + header.binaryCount);
	const U32 *includeFileCrc32c                = (const U32*) (fixedEntryInfo + header.stageCount);
	const U8 *arrayDims                         = (const U8*) (includeFileCrc32c + header.includeFileCount);

	//Parse array dims

	gotoIfError3(clean, ListListU32_resize(&arrays, header.arrayDimCount, alloc, e_rr));

	U64 totalArrayElements = 0;

	for (U8 i = 0; i < header.arrayDimCount; ++i) {

		U8 arrayDim = arrayDims[i];

		if (arrayDim > 32 || !arrayDim)
			retError(clean, Error_invalidState(0, "SHFile_read() array must be of size [1, 32]"));

		totalArrayElements += arrayDim;
	}

	gotoIfError3(clean, Buffer_createUninitializedBytes(totalArrayElements * sizeof(U32), alloc, &arraysBuf, e_rr));
	gotoIfError3(clean, StreamCursor_consume(
		&cursor, offset, arraysBuf.ptrNonConst, totalArrayElements * sizeof(U32), alloc, e_rr
	));

	{
		const U32 *arrayCount = (const U32*) arraysBuf.ptr;

		for (U8 i = 0; i < header.arrayDimCount; ++i) {
			gotoIfError3(clean, ListU32_createRefConst(arrayCount, arrayDims[i], &arrays.ptrNonConst[i], e_rr));
			arrayCount += arrayDims[i];
		}
	}

	//Create SHFile container

	ESHSettingsFlags flags = isSubFile ? ESHSettingsFlags_HideMagicNumber : ESHSettingsFlags_None;
	gotoIfError3(clean, SHFile_create(flags, header.compilerVersion, header.sourceHash, alloc, shFile, e_rr));
	allocate = true;

	//Precompute string section offsets

	U64 entrypointNameStart = strings.entryStrings.length - header.semanticCount - header.stageCount;
	U64 includeNameStart    = entrypointNameStart - header.includeFileCount;
	U64 registerNameStart   = includeNameStart - header.registerNameCount;
	U64 uniformNameStart    = registerNameStart - header.uniformNameCount;

	//Parse binaries
	for (U64 j = 0; j < header.binaryCount; ++j) {

		BinaryInfoFixedSize binary = fixedBinaryInfo[j];

		//Validate binary

		if (binary.entrypointType > ESHPipelineStage_Count)
			retError(clean, Error_invalidState(
				1, "SHFile_read() binary had invalid stageType"
			));

		if (!(binary.binaryFlags & ESHBinaryFlags_HasShaderAnnotation) && binary.entrypointType >= ESHPipelineStage_Count)
			retError(clean, Error_invalidState(
				1, "SHFile_read() binary not marked as shader annotation but had no valid stageType"
			));

		if (binary.shaderModel < OISH_SHADER_MODEL_MIN8 || binary.shaderModel > OISH_SHADER_MODEL_MAX8)
			retError(clean, Error_invalidState(1, "SHFile_read() binary had invalid shaderModel"));

		if (binary.extensions >> ESHExtension_Count)
			retError(clean, Error_invalidState(1, "SHFile_read() binary had invalid extensions"));

		binaryInfo = (SHBinaryInfo){

			.identifier = (SHBinaryIdentifier) {
				.extensions = binary.extensions,
				.shaderVersion = ((binary.shaderModel & 0xF0) << 4) | (binary.shaderModel & 0xF),
				.stageType = binary.entrypointType
			},

			.dormantExtensions = binary.dormantExt,
			.vendorMask = binary.vendorMask,
			.hasShaderAnnotation = binary.binaryFlags & ESHBinaryFlags_HasShaderAnnotation
		};

		if (binary.entrypoint != U16_MAX) {

			if (binary.binaryFlags & ESHBinaryFlags_HasShaderAnnotation)
				retError(clean, Error_invalidState(
					1, "SHFile_read() binary marked as shader annotation but had entrypoint != U16_MAX"
				));

			if (binary.entrypoint >= header.stageCount)
				retError(clean, Error_outOfBounds(
					0, binary.entrypoint, header.stageCount, "SHFile_read() entrypoint id out of bounds"
				));

			CharString name = strings.entryStrings.ptr[entrypointNameStart + binary.entrypoint];
			gotoIfError3(clean, CharString_createCopy(name, alloc, &binaryInfo.identifier.entrypoint, e_rr));
		}

		else if (!(binary.binaryFlags & ESHBinaryFlags_HasShaderAnnotation))
			retError(clean, Error_invalidState(
				1, "SHFile_read() binary not marked as shader annotation but had entrypoint = U16_MAX"
			));

		//Compute the total size of all fixed binary data excluding the variable-length binary blobs,
		//then read it all in one shot.

		U64 binaryDataSize =
			(U64)binary.defineCount * 2 * sizeof(U16) +
			(U64)binary.uniformCount * sizeof(SHUniform) +
			(U64)binary.registerCount * sizeof(SHRegister);

		gotoIfError3(clean, Buffer_createUninitializedBytes(binaryDataSize, alloc, &tmp, e_rr));
		gotoIfError3(clean, StreamCursor_consumeBuffer(&cursor, offset, tmp, alloc, e_rr));

		const U16 *defineNames = (const U16*)tmp.ptr;
		const U16 *defineValues = defineNames + binary.defineCount;
		const SHUniform *uniforms = (const SHUniform *)(defineValues + binary.defineCount);
		const SHRegister *regs = (const SHRegister *)(uniforms + binary.uniformCount);

		//Compute uniform data size

		U64 maxOff = 0;

		for (U64 i = 0; i < binary.uniformCount; ++i) {

			if (uniforms[i].typeIdShort >= ETypeId_Max)
				retError(clean, Error_invalidState(
					1, "SHFile_read() typeIdShort wasn't a valid index into ETypeId_arr"
				));

			maxOff = U64_max(maxOff, uniforms[i].dataOffset + ETypeId_getBytes(ETypeId_arr[uniforms[i].typeIdShort]));
		}

		//Parse defines

		ListCharString *defineStrs = &binaryInfo.identifier.defines;
		gotoIfError3(clean, ListCharString_resize(defineStrs, (U64)binary.defineCount * 2, alloc, e_rr));

		for (U64 i = 0; i < binary.defineCount; ++i) {

			if (defineNames[i] >= header.uniqueDefines)
				retError(clean, Error_invalidState(1, "SHFile_read() defineName out of bounds"));

			CharString defineName = strings.entryStrings.ptr[defineNames[i]];

			U64 defineValueId = (U64)defineValues[i] + header.uniqueDefines;

			if (defineValueId >= registerNameStart)
				retError(clean, Error_invalidState(1, "SHFile_read() defineValue out of bounds"));

			CharString defineValue = strings.entryStrings.ptr[defineValueId];

			for (U64 k = 0; k < i; ++k)
				if (CharString_equalsStringSensitive(&defineStrs->ptr[k << 1], &defineName))
					retError(clean, Error_alreadyDefined(1, "SHFile_read() defineName already declared"));

			gotoIfError3(clean, CharString_createCopy(defineName, alloc, &defineStrs->ptrNonConst[i << 1], e_rr));
			gotoIfError3(clean, CharString_createCopy(defineValue, alloc, &defineStrs->ptrNonConst[(i << 1) | 1], e_rr));

			//TODO: In theory, if we're the last defineValue/defineName of that id we can move it
		}

		//Parse uniforms

		gotoIfError3(clean, ListU8_resize(&binaryInfo.identifier.uniformData, maxOff, alloc, e_rr));

		gotoIfError3(clean, StreamCursor_consumeBuffer(
			&cursor, offset, ListU8_buffer(binaryInfo.identifier.uniformData), alloc, e_rr
		));

		gotoIfError3(clean, ListSHUniformRuntime_resize(&binaryInfo.identifier.uniforms, binary.uniformCount, alloc, e_rr));

		for (U64 i = 0; i < binary.uniformCount; ++i) {

			SHUniform uniform = uniforms[i];

			if (uniform.nameId >= header.uniformNameCount)
				retError(clean, Error_invalidState(1, "SHFile_read() uniform nameId out of bounds"));

			CharString uniformName = strings.entryStrings.ptr[uniformNameStart + uniform.nameId];

			if (!CharString_length(uniformName) || (!C8_isAlpha(uniformName.ptr[0]) && uniformName.ptr[0] != '_'))
				retError(clean, Error_invalidState(
					1, "SHFile_read() uniform name empty or started with non [A-Za-z_]"
				));

			for (U64 k = 1; k < CharString_length(uniformName); ++k)
				if (!C8_isAlphaNumeric(uniformName.ptr[k]) && uniformName.ptr[k] != '_')
					retError(clean, Error_invalidState(
						1, "SHFile_read() uniform name contained non [A-Za-z0-9_]"
					));

			for (U64 k = 0; k < i; ++k)
				if (CharString_equalsStringSensitive(&binaryInfo.identifier.uniforms.ptr[k].name, &uniformName))
					retError(clean, Error_alreadyDefined(1, "SHFile_read() uniformName already declared"));

			binaryInfo.identifier.uniforms.ptrNonConst[i] = (SHUniformRuntime) {
				.dataOffset = uniform.dataOffset,
				.typeIdShort = uniform.typeIdShort
			};

			gotoIfError3(clean, CharString_createCopy(
				uniformName, alloc, &binaryInfo.identifier.uniforms.ptrNonConst[i].name, e_rr
			));

			//TODO: In theory, if we're the last uniformName of that id we can move it
		}

		//Parse registers

		for (U64 i = 0; i < binary.registerCount; ++i) {

			const SHRegister *reg = &regs[i];

			if (reg->nameId >= header.registerNameCount)
				retError(clean, Error_invalidState(1, "SHFile_read() register nameId out of bounds"));

			CharString name = CharString_createRefStrConst(strings.entryStrings.ptr[reg->nameId + registerNameStart]);

			ListU32 arr = (ListU32) { 0 };

			if (reg->registerType & ESHRegisterType_IsArray) {

				if (reg->arrayDimOrId & 0x8000) {

					U16 arrayId = reg->arrayDimOrId & 0x7FFF;

					if (arrayId >= header.arrayDimCount)
						retError(clean, Error_invalidState(1, "SHFile_read() arrayId out of bounds"));

					arr = ListU32_createRefFromList(arrays.ptr[arrayId]);
				}

				//Small 1D array: arrayDimOrId is the dimension directly
				else gotoIfError3(clean, ListU32_createRefConst((const U32*)&reg->arrayDimOrId, 1, &arr, e_rr));
			}

			SBFile *sbFile = NULL;
			U8 regType = reg->registerType & ESHRegisterType_TypeMask;

			if (
				regType >= ESHRegisterType_BufferStart && regType <= ESHRegisterType_BufferEnd &&
				reg->shaderBufferId != U16_MAX
			) {

				if (reg->shaderBufferId >= parsedShaderBuffers.length)
					retError(clean, Error_invalidState(1, "SHFile_read() shaderBufferId out of bounds"));

				sbFile = &copySB;
				gotoIfError3(clean, SBFile_createCopy(&parsedShaderBuffers.ptr[reg->shaderBufferId], alloc, sbFile, e_rr));

				//TODO: In theory, if we're the last shaderBuffer of that id we can move it
			}

			gotoIfError3(clean, ListSHRegisterRuntime_addRegister(
				&binaryInfo.registers, &name, arr.length ? &arr : NULL, *reg, sbFile, alloc, e_rr
			));

			SBFile_free(sbFile, alloc);
		}

		//Read binary sizes and data

		for (U8 i = 0; i < ESHBinaryType_Count; ++i) {

			if (!((binary.binaryFlags >> i) & 1))
				continue;

			U64 binarySize = 0;
			gotoIfError3(clean, StreamCursor_consumeSizeType(
				&cursor, offset, (header.sizeTypes >> (i << 1)) & 3, &binarySize, alloc, e_rr
			));

			if (!binarySize)
				continue;

			gotoIfError3(clean, Buffer_createUninitializedBytes(binarySize, alloc, &binaryInfo.binaries[i], e_rr));
		}

		for (U8 i = 0; i < ESHBinaryType_Count; ++i) {

			if (!((binary.binaryFlags >> i) & 1))
				continue;

			gotoIfError3(clean, StreamCursor_consumeBuffer(&cursor, offset, binaryInfo.binaries[i], alloc, e_rr));
		}

		gotoIfError3(clean, SHFile_addBinary(shFile, &binaryInfo, alloc, e_rr));
		Buffer_free(&tmp, alloc);
	}

	//Parse stages

	U64 semanticCounter = 0;

	for (U64 i = 0; i < header.stageCount; ++i) {

		if (fixedEntryInfo[i].pipelineStage >= ESHPipelineStage_Count)
			retError(clean, Error_invalidParameter(0, 0, "SHFile_read() stage[i].pipelineStage out of bounds"));

		entry = (SHEntry) { .stage = fixedEntryInfo[i].pipelineStage };

		U8 binaryCount = fixedEntryInfo[i].binaryCount;

		if (!binaryCount)
			retError(clean, Error_invalidParameter(0, 0, "SHFile_read() stage[i].binaryCount must be >0"));

		CharString str = strings.entryStrings.ptr[entrypointNameStart + i];

		for (U64 k = 0; k < i; ++k)
			if (CharString_equalsStringSensitive(&strings.entryStrings.ptr[entrypointNameStart + k], &str))
				retError(clean, Error_alreadyDefined(0, "SHFile_read() duplicate entrypoint name"));

		gotoIfError3(clean, CharString_createCopy(str, alloc, &entry.name, e_rr));

		switch (entry.stage) {

			default: {

				U8 inputsAvail = 0, outputsAvail = 0;
				gotoIfError3(clean, StreamCursor_consumeU8(&cursor, offset, &inputsAvail, alloc, e_rr));
				gotoIfError3(clean, StreamCursor_consumeU8(&cursor, offset, &outputsAvail, alloc, e_rr));

				Bool hasSemantics = inputsAvail & 0x80;
				U8 inputs  = inputsAvail & 0x7F;
				U8 outputs = outputsAvail;

				if (inputs > 16 || outputs > 16)
					retError(clean, Error_invalidParameter(
						0, 0, "SHFile_read() stage[i].inputs or outputs out of bounds"
					));

				gotoIfError3(clean, StreamCursor_consume(&cursor, offset, entry.inputs, inputs, alloc, e_rr));
				gotoIfError3(clean, StreamCursor_consume(&cursor, offset, entry.outputs, outputs, alloc, e_rr));

				if (hasSemantics) {

					U8 semanticCounts = 0;
					gotoIfError3(clean, StreamCursor_consumeU8(&cursor, offset, &semanticCounts, alloc, e_rr));

					U8 uniqueInputSemantics  = semanticCounts & 0xF;
					U8 uniqueOutputSemantics = semanticCounts >> 4;

					entry.uniqueInputSemantics = uniqueInputSemantics;

					U64 startCounter = semanticCounter;
					semanticCounter += uniqueInputSemantics + uniqueOutputSemantics;

					if (semanticCounter > header.semanticCount)
						retError(clean, Error_outOfBounds(
							0, semanticCounter, header.semanticCount, "SHFile_read() semantic out of bounds"
						));

					U64 semanticBase = strings.entryBuffers.length - header.semanticCount + startCounter;

					for (U64 k = 0; k < uniqueInputSemantics + uniqueOutputSemantics; ++k) {
						CharString ref = CharString_createRefStrConst(strings.entryStrings.ptr[semanticBase + k]);
						gotoIfError3(clean, ListCharString_pushBack(&entry.semanticNames, ref, alloc, e_rr));
					}

					gotoIfError3(clean, StreamCursor_consume(&cursor, offset, entry.inputSemanticNames, inputs, alloc, e_rr));
					gotoIfError3(clean, StreamCursor_consume(&cursor, offset, entry.outputSemanticNames, outputs, alloc, e_rr));
				}

				if (entry.stage != ESHPipelineStage_MeshExt && entry.stage != ESHPipelineStage_TaskExt)
					break;
			}

			// fallthrough

			case ESHPipelineStage_Compute:
			case ESHPipelineStage_WorkgraphExt: {

				SHGroups groups = (SHGroups) { 0 };
				gotoIfError3(clean, StreamCursor_consume(&cursor, offset, &groups, sizeof(groups), alloc, e_rr));

				entry.groupX   = groups.x;
				entry.groupY   = groups.y;
				entry.groupZ   = groups.z;
				entry.waveSize = groups.waveSize;

				if (entry.waveSize && entry.stage != ESHPipelineStage_Compute && entry.stage != ESHPipelineStage_WorkgraphExt)
					retError(clean, Error_invalidParameter(
						0, 0, "SHFile_read() waveSize not supported by mesh or task shader"
					));

				for (U8 k = 0; k < 4; ++k) {
					U8 curr = (entry.waveSize >> (k << 2)) & 0xF;
					if (curr == 1 || curr == 2 || curr > 8)
						retError(clean, Error_invalidParameter(0, 0, "SHFile_read() entry.waveSize contained invalid data"));
				}

				break;
			}

			case ESHPipelineStage_RaygenExt:
				break;

			case ESHPipelineStage_ClosestHitExt:
			case ESHPipelineStage_AnyHitExt:
			case ESHPipelineStage_IntersectionExt:
				gotoIfError3(clean, StreamCursor_consumeU8(&cursor, offset, &entry.intersectionSize, alloc, e_rr));
				// fallthrough

			case ESHPipelineStage_CallableExt:
			case ESHPipelineStage_MissExt:
				gotoIfError3(clean, StreamCursor_consumeU8(&cursor, offset, &entry.payloadSize, alloc, e_rr));
				break;
		}

		//Read binary ids

		gotoIfError3(clean, ListU16_resize(&entry.binaryIds, binaryCount, alloc, e_rr));
		gotoIfError3(clean, StreamCursor_consumeBuffer(&cursor, offset, ListU16_buffer(entry.binaryIds), alloc, e_rr));

		for (U64 k = 0; k < entry.binaryIds.length; ++k) {

			U16 binaryId = entry.binaryIds.ptr[k];

			if (binaryId >= header.binaryCount)
				retError(clean, Error_outOfBounds(0, binaryId, header.binaryCount, "SHFile_read() binary id out of bounds"));

			U16 entryId = fixedBinaryInfo[binaryId].entrypoint;

			if (entryId != U16_MAX && entryId != i)
				retError(clean, Error_invalidParameter(0, 0, "SHFile_read() stage[i].binaries[j] had mismatching entryId"));
		}

		gotoIfError3(clean, SHFile_addEntrypoint(shFile, &entry, alloc, e_rr));
		SHEntry_free(&entry, alloc);
	}

	//Verify every binary is referenced by a stage

	for (U64 j = 0; j < shFile->binaries.length; ++j) {

		SHBinaryInfo info = shFile->binaries.ptr[j];
		Bool contains = false;

		if (info.hasShaderAnnotation) {

			for (U64 i = 0; i < shFile->entries.length && !contains; ++i) {
				ListU16 binaries = shFile->entries.ptr[i].binaryIds;
				for (U64 k = 0; k < binaries.length && !contains; ++k)
					if (binaries.ptr[k] == j)
						contains = true;
			}
		}

		else {
			U16 entryIdx = fixedBinaryInfo[j].entrypoint;
			ListU16 binaries = shFile->entries.ptr[entryIdx].binaryIds;
			for (U64 i = 0; i < binaries.length && !contains; ++i)
				if (binaries.ptr[i] == j)
					contains = true;
		}

		if (!contains)
			retError(clean, Error_invalidState(1, "SHFile_read() binary not referenced by any entrypoint"));
	}

	//Validate hash, read from hashStart to current offset, compare with header.hash

	{
		U64 hashLen = *offset - hashStart;
		gotoIfError3(clean, Buffer_createUninitializedBytes(hashLen, alloc, &tmp, e_rr));
		gotoIfError3(clean, StreamCursor_consumeBuffer(&cursor, &hashStart, tmp, alloc, e_rr));

		U32 hash = Buffer_crc32c(tmp);
		Buffer_free(&tmp, alloc);

		if (hash != header.hash)
			retError(clean, Error_unauthorized(0, "SHFile_read() mismatching CRC32C"));
	}

clean:
	if (!s_uccess && allocate)
		SHFile_free(shFile, alloc);

	StreamCursor_close(&cursor, alloc);
	Buffer_free(&tmp, alloc);
	Buffer_free(&staticBuf, alloc);
	Buffer_free(&arraysBuf, alloc);
	SBFile_free(&copySB, alloc);
	ListListU32_free(&arrays, alloc);
	SHInclude_free(&include, alloc);
	SHBinaryInfo_free(&binaryInfo, alloc);
	SHEntry_free(&entry, alloc);
	DLFile_free(&strings, alloc);
	ListSBFile_freeUnderlying(&parsedShaderBuffers, alloc);
	DLFile_free(&shaderBuffers, alloc);
	return s_uccess;
}
