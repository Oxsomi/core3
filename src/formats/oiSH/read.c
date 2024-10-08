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

#include "formats/oiSH/sh_file.h"
#include "formats/oiSH/headers.h"
#include "types/allocator.h"
#include "types/error.h"
#include "types/buffer.h"
#include "formats/oiDL.h"

#include <stddef.h>

Bool SHFile_read(Buffer file, Bool isSubFile, Allocator alloc, SHFile *shFile, Error *e_rr) {

	Bool s_uccess = true;
	Bool allocate = false;

	DLFile strings = (DLFile) { 0 };
	DLFile shaderBuffers = (DLFile) { 0 };
	ListSBFile parsedShaderBuffers = (ListSBFile) { 0 };
	SHEntry entry = (SHEntry) { 0 };
	SHBinaryInfo binaryInfo = (SHBinaryInfo) { 0 };
	SHInclude include = (SHInclude) { 0 };
	ListListU32 arrays = (ListListU32) { 0 };		//All references
	SBFile copySB = (SBFile) { 0 };

	if(!shFile)
		retError(clean, Error_nullPointer(2, "SHFile_read()::shFile is required"))

	if(shFile->entries.ptr)
		retError(clean, Error_invalidOperation(0, "SHFile_read()::shFile isn't empty, might indicate memleak"))

	Buffer entireFile = file;

	file = Buffer_createRefFromBuffer(file, true);

	if (!isSubFile) {

		U32 magic;
		gotoIfError2(clean, Buffer_consume(&file, &magic, sizeof(magic)))

		if(magic != SHHeader_MAGIC)
			retError(clean, Error_invalidParameter(0, 0, "SHFile_read() requires magicNumber prefix"))
	}

	//Read from binary

	Buffer hash = file;
	SHHeader header;
	gotoIfError2(clean, Buffer_consume(&file, &header, sizeof(header)))

	//Validate header

	if(header.version != ESHVersion_V1_2)
		retError(clean, Error_invalidParameter(0, 1, "SHFile_read() header.version is invalid"))

	if(!header.stageCount)
		retError(clean, Error_invalidParameter(0, 1, "SHFile_read() header.stageCount is invalid"))

	//Validate hash

	Buffer_offset(&hash, offsetof(SHHeader, uniqueUniforms));

	if(Buffer_crc32c(hash) != header.hash)
		retError(clean, Error_unauthorized(0, "SHFile_read() mismatching CRC32C"))

	//Read DLFile (strings)

	gotoIfError3(clean, DLFile_read(file, NULL, true, alloc, &strings, e_rr))

	U64 minEntryBuffers =
		(U64)header.stageCount +
		header.includeFileCount +
		header.semanticCount +
		header.registerNameCount +
		header.uniqueUniforms +				//Names have to be unique
		(header.uniqueUniforms ? 1 : 0);	//Values can be shared

	if(
		strings.entryBuffers.length < minEntryBuffers ||
		strings.settings.flags & EDLSettingsFlags_UseSHA256 ||
		strings.settings.dataType == EDLDataType_Data ||
		strings.settings.encryptionType ||
		strings.settings.compressionType
	)
		retError(clean, Error_invalidParameter(0, 1, "SHFile_read() strings didn't match expectations"))

	gotoIfError2(clean, Buffer_offset(&file, strings.readLength))

	//Read DLFile (shader buffers)

	gotoIfError3(clean, DLFile_read(file, NULL, true, alloc, &shaderBuffers, e_rr))

	if(
		shaderBuffers.settings.flags & EDLSettingsFlags_UseSHA256 ||
		shaderBuffers.settings.dataType != EDLDataType_Data ||
		shaderBuffers.settings.encryptionType ||
		shaderBuffers.settings.compressionType
	)
		retError(clean, Error_invalidParameter(0, 1, "SHFile_read() shaderBuffers didn't match expectations"))

	gotoIfError2(clean, Buffer_offset(&file, shaderBuffers.readLength))

	gotoIfError2(clean, ListSBFile_resize(&parsedShaderBuffers, shaderBuffers.entryBuffers.length, alloc))

	for(U64 i = 0; i < shaderBuffers.entryBuffers.length; ++i)
		gotoIfError3(clean, SBFile_read(
			shaderBuffers.entryBuffers.ptr[i], true, alloc, &parsedShaderBuffers.ptrNonConst[i], e_rr
		))

	//Check if the buffer can at least contain the static sized data

	const BinaryInfoFixedSize *fixedBinaryInfo = (const BinaryInfoFixedSize*) file.ptr;
	const EntryInfoFixedSize *fixedEntryInfo = (const EntryInfoFixedSize*) (fixedBinaryInfo + header.binaryCount);
	const U32 *includeFileCrc32c = (const U32*) (fixedEntryInfo + header.stageCount);
	const U8 *arrayDims = (const U8*) (includeFileCrc32c + header.includeFileCount);
	const U32 *arrayCount = (const U32*) (arrayDims + header.arrayDimCount);

	gotoIfError2(clean, Buffer_offset(&file, (const U8*)arrayCount - file.ptr));
	gotoIfError2(clean, ListListU32_resize(&arrays, header.arrayDimCount, alloc))

	U64 totalArrayElements = 0;

	for (U8 i = 0; i < header.arrayDimCount; ++i) {

		U8 arrayDim = arrayDims[i];

		if(arrayDim > 32 || !arrayDim)
			retError(clean, Error_invalidState(0, "SHFile_read() array must be of size [1, 32]"))

		gotoIfError2(clean, ListU32_createRefConst(arrayCount, arrayDim, &arrays.ptrNonConst[i]))
		totalArrayElements += arrayDim;
		arrayCount += arrayDim;
	}

	gotoIfError2(clean, Buffer_offset(&file, totalArrayElements * sizeof(U32)));

	//Create SHFile container

	ESHSettingsFlags flags = isSubFile ? ESHSettingsFlags_HideMagicNumber : 0;

	if(strings.settings.dataType == EDLDataType_UTF8)
		flags |= ESHSettingsFlags_IsUTF8;

	gotoIfError3(clean, SHFile_create(flags, header.compilerVersion, header.sourceHash, alloc, shFile, e_rr))
	allocate = true;

	//Parse binaries

	U64 entrypointNameStart = strings.entryBuffers.length - header.semanticCount - header.stageCount;
	U64 includeNameStart = entrypointNameStart - header.includeFileCount;
	U64 registerNameStart = includeNameStart - header.registerNameCount;

	for(U64 j = 0; j < header.binaryCount; ++j) {

		BinaryInfoFixedSize binary = fixedBinaryInfo[j];

		if(!(binary.binaryFlags & ESHBinaryFlags_HasShaderAnnotation) && binary.entrypointType >= ESHPipelineStage_Count)
			retError(clean, Error_invalidState(
				1, "SHFile_read() contained binary that was marked as stage annotation but had no valid stageType"
			))

		binaryInfo = (SHBinaryInfo) {

			.identifier = (SHBinaryIdentifier) {
				.extensions = binary.extensions,
				.shaderVersion = ((binary.shaderModel & 0xF0) << 4) | (binary.shaderModel & 0xF),
				.stageType = binary.entrypointType
			},

			.vendorMask = binary.vendorMask,
			.hasShaderAnnotation = binary.binaryFlags & ESHBinaryFlags_HasShaderAnnotation
		};

		//Validate if entrypoint does actually reference binary

		if(binary.entrypoint != U16_MAX) {

			if(binary.binaryFlags & ESHBinaryFlags_HasShaderAnnotation)
				retError(clean, Error_invalidState(
					1, "SHFile_read() contained binary that was marked as shader annotation but had entrypoint != U16_MAX"
				))

			if(binary.entrypoint >= header.stageCount)
				retError(clean, Error_outOfBounds(
					0, binary.entrypoint, header.stageCount, "SHFile_read() one of the entrypoint ids it out of bounds"
				))

			CharString name = DLFile_stringAt(strings, entrypointNameStart + binary.entrypoint, NULL);

			gotoIfError2(clean, CharString_createCopy(name, alloc, &binaryInfo.identifier.entrypoint))
		}

		//Check if any entrypoint references this binary

		else if(!(binary.binaryFlags & ESHBinaryFlags_HasShaderAnnotation))
			retError(clean, Error_invalidState(
				1, "SHFile_read() contained binary that wasn't marked as shader annotation but had entrypoint = U16_MAX"
			))

		//Grab uniforms

		const U16 *uniformNames = (const U16*) file.ptr;
		const U16 *uniformValues = uniformNames + binary.uniformCount;

		//Grab registers

		const SHRegister *regs = (const SHRegister*) (uniformValues + binary.uniformCount);
		const SHRegister *regEnd = (const SHRegister*) (regs + binary.registerCount);

		gotoIfError2(clean, Buffer_offset(&file, (const U8*) regEnd - file.ptr))

		//Parse uniforms (names must be unique)

		ListCharString *uniformStrs = &binaryInfo.identifier.uniforms;
		gotoIfError2(clean, ListCharString_resize(uniformStrs, (U64) binary.uniformCount * 2, alloc))

		for(U64 i = 0; i < binary.uniformCount; ++i) {

			//Grab uniform and values, validate them too

			if(uniformNames[i] >= header.uniqueUniforms)
				retError(clean, Error_invalidState(1, "SHFile_read() uniformName out of bounds"))

			CharString uniformName = DLFile_stringAt(strings, (U64) uniformNames[i], NULL);

			//Since uniform values can be shared, we need to ensure we're not indexing out of bounds

			U64 uniformNameId = (U64) uniformValues[i] + header.uniqueUniforms;

			if(uniformNameId >= registerNameStart)
				retError(clean, Error_invalidState(1, "SHFile_read() uniformName out of bounds"))

			CharString uniformValue = DLFile_stringAt(strings, uniformNameId, NULL);

			//Check for duplicate uniform names

			for(U64 k = 0; k < i; ++k)
				if(CharString_equalsStringSensitive(uniformStrs->ptr[k << 1], uniformName))
					retError(clean, Error_alreadyDefined(1, "SHFile_read() uniformName already declared"))

			//Store

			gotoIfError2(clean, CharString_createCopy(uniformName, alloc, &uniformStrs->ptrNonConst[i << 1]));
			gotoIfError2(clean, CharString_createCopy(uniformValue, alloc, &uniformStrs->ptrNonConst[(i << 1) | 1]));
		}

		//Parse registers

		for (U64 i = 0; i < binary.registerCount; ++i) {

			SHRegister reg = regs[i];

			if(reg.nameId >= header.registerNameCount)
				retError(clean, Error_invalidState(1, "SHFile_read() nameId out of bounds"))

			CharString name = DLFile_stringAt(strings, reg.nameId + registerNameStart, NULL);
			name = CharString_createRefStrConst(name);

			if(reg.arrayId != U16_MAX && reg.arrayId >= header.arrayDimCount)
				retError(clean, Error_invalidState(1, "SHFile_read() arrayId out of bounds"))

			ListU32 arr = reg.arrayId != U16_MAX ? arrays.ptr[reg.arrayId] : (ListU32) { 0 };
			SBFile *sbFile = NULL;
			U8 regType = reg.registerType & ESHRegisterType_TypeMask;

			if (
				regType >= ESHRegisterType_BufferStart && regType < ESHRegisterType_BufferEnd &&
				reg.shaderBufferId != U16_MAX
			) {

				if(reg.shaderBufferId >= parsedShaderBuffers.length)
					retError(clean, Error_invalidState(1, "SHFile_read() shaderBufferId out of bounds"))

				sbFile = &copySB;
				gotoIfError3(clean, SBFile_createCopy(parsedShaderBuffers.ptr[reg.shaderBufferId], alloc, &copySB, e_rr))
			}

			gotoIfError3(clean, ListSHRegisterRuntime_addRegister(
				&binaryInfo.registers, &name, arr.length ? &arr : NULL, reg, sbFile, alloc, e_rr
			))
		}

		//Get binarySizes

		U64 binarySize[ESHBinaryType_Count] = { 0 };

		for (U8 i = 0; i < ESHBinaryType_Count; ++i) {

			if(!((binary.binaryFlags >> i) & 1))
				continue;

			gotoIfError2(clean, Buffer_consumeSizeType(&file, (header.sizeTypes >> (i << 1)) & 3, &binarySize[i]))
		}

		for (U8 i = 0; i < ESHBinaryType_Count; ++i) {

			if(!binarySize[i])
				continue;

			Buffer entryBuf = Buffer_createRefConst(file.ptr, binarySize[i]);
			gotoIfError2(clean, Buffer_offset(&file, binarySize[i]))

			gotoIfError2(clean, Buffer_createCopy(entryBuf, alloc, &binaryInfo.binaries[i]))
		}

		gotoIfError3(clean, SHFile_addBinaries(shFile, &binaryInfo, alloc, e_rr))
	}

	//Parse stages

	const U8 *nextMemPrev = file.ptr;
	const U8 *nextMem = file.ptr, *nextMemTemp = nextMem;
	U64 semanticCounter = 0;

	for (U64 i = 0; i < header.stageCount; ++i) {

		if(fixedEntryInfo[i].pipelineStage >= ESHPipelineStage_Count)
			retError(clean, Error_invalidParameter(0, 0, "SHFile_read() stage[i].pipelineStage out of bounds"))

		entry = (SHEntry) {
			.stage = fixedEntryInfo[i].pipelineStage
		};

		U8 binaryCount = fixedEntryInfo[i].binaryCount;

		if(!binaryCount)
			retError(clean, Error_invalidParameter(0, 0, "SHFile_read() stage[i].binaryCount must be >0"))

		U64 start = entrypointNameStart;
		CharString str = DLFile_stringAt(strings, start + i, NULL);		//Already safety checked

		if(DLFile_find(strings, start, start + i, str) != U64_MAX)
			retError(clean, Error_alreadyDefined(0, "SHFile_read() strings included duplicate entrypoint name"))

		gotoIfError2(clean, CharString_createCopy(str, alloc, &entry.name))

		switch (entry.stage) {

			default: {

				//U8 inputsAvail, outputsAvail

				if(nextMem + 2 > file.ptr + Buffer_length(file))
					retError(clean, Error_outOfBounds(
						0, nextMem - file.ptr, Buffer_length(file),
						"SHFile_read() couldn't parse graphics stage, out of bounds"
					))

				U8 inputs = nextMem[0];

				Bool hasSemantics = inputs & 0x80;
				inputs &= 0x7F;

				U8 outputs = nextMem[1];

				if(inputs > 0x10 || outputs > 0x10)
					retError(clean, Error_invalidParameter(
						0, 0, "SHFile_read() stage[i].inputs or outputs out of bounds"
					))

				nextMem += sizeof(U8) * 2;		//U8 inputsAvail, outputsAvail;

				//Unpack U8[inputs], U8[outputs]

				U8 inputBytes = inputs;
				U8 outputBytes = outputs;

				nextMemTemp = nextMem;
				nextMem += inputBytes + outputBytes;

				if(nextMem > file.ptr + Buffer_length(file))
					retError(clean, Error_outOfBounds(
						0, nextMem - file.ptr, Buffer_length(file), "SHFile_read() couldn't parse graphics stage, out of bounds"
					))

				for(U8 j = 0; j < inputBytes; ++j)
					entry.inputs[j] = nextMemTemp[j];

				for(U8 j = 0; j < outputBytes; ++j)
					entry.outputs[j] = nextMemTemp[inputBytes + j];

				nextMemTemp = nextMem;

				//Semantics

				if (hasSemantics) {

					//Get strings

					nextMem += 1 + inputs + outputs;
					U8 uniqueInputSemantics = nextMemTemp[0] & 0xF;
					U8 uniqueOutputSemantics = nextMemTemp[0] >> 4;

					entry.uniqueInputSemantics = uniqueInputSemantics;

					U64 startCounter = semanticCounter;
					semanticCounter += uniqueInputSemantics + uniqueOutputSemantics;

					if(semanticCounter > header.semanticCount)
						retError(clean, Error_outOfBounds(
							0, semanticCounter, header.semanticCount, "SHFile_read() semantic out of bounds"
						))

					startCounter += strings.entryBuffers.length - header.semanticCount;

					for (U64 j = startCounter; j < startCounter + uniqueInputSemantics + uniqueOutputSemantics; ++j) {

						CharString ref;

						if(strings.settings.dataType == EDLDataType_UTF8) {
							Buffer buf = strings.entryBuffers.ptr[j];
							ref = CharString_createRefSizedConst((const C8*)buf.ptr, Buffer_length(buf), false);
						}

						else ref = CharString_createRefStrConst(strings.entryStrings.ptr[j]);

						gotoIfError2(clean, ListCharString_pushBack(&entry.semanticNames, ref, alloc))
					}

					for (U8 j = 0; j < inputs; ++j) {
						U8 input = nextMemTemp[1 + j];
						entry.inputSemanticNames[j] = input;
					}

					for (U8 j = 0; j < outputs; ++j) {
						U8 output = nextMemTemp[1 + inputs + j];
						entry.outputSemanticNames[j] = output;
					}
				}

				if(entry.stage != ESHPipelineStage_MeshExt && entry.stage != ESHPipelineStage_TaskExt)
					break;
			}

			// fallthrough

			case ESHPipelineStage_WorkgraphExt:
			case ESHPipelineStage_Compute: {

				nextMemTemp = nextMem;
				nextMem += sizeof(U16) * 4;

				if(nextMem > file.ptr + Buffer_length(file))
					retError(clean, Error_outOfBounds(
						0, nextMem - file.ptr, Buffer_length(file), "SHFile_read() couldn't parse compute stage, out of bounds"
					))

				const U16 *groups = (const U16*)nextMemTemp;
				entry.groupX   = groups[0];
				entry.groupY   = groups[1];
				entry.groupZ   = groups[2];
				entry.waveSize = groups[3];

				if(entry.waveSize && (entry.stage == ESHPipelineStage_MeshExt || entry.stage == ESHPipelineStage_TaskExt))
					retError(clean, Error_invalidParameter(
						0, 0, "SHFile_read() entry.waveSize isn't supported by mesh or task shader"
					))

				for (U8 j = 0; j < 4; ++j) {

					U8 curr = (entry.waveSize >> (j << 2)) & 0xF;

					if(curr == 1 || curr == 2 || curr > 8)
						retError(clean, Error_invalidParameter(0, 0, "SHFile_read() entry.waveSize contained invalid data"))
				}

				break;
			}

			case ESHPipelineStage_RaygenExt:
			case ESHPipelineStage_CallableExt:
				break;

			case ESHPipelineStage_ClosestHitExt:
			case ESHPipelineStage_AnyHitExt:
			case ESHPipelineStage_IntersectionExt:

				if(nextMem + 1 > file.ptr + Buffer_length(file))
					retError(clean, Error_outOfBounds(
						0, nextMem - file.ptr, Buffer_length(file),
						"SHFile_read() couldn't parse raytracing stage, out of bounds"
					))

				entry.intersectionSize = *nextMem;
				++nextMem;

  				// fall through

			case ESHPipelineStage_MissExt:

				if(nextMem + 1 > file.ptr + Buffer_length(file))
					retError(clean, Error_outOfBounds(
						0, nextMem - file.ptr, Buffer_length(file),
						"SHFile_read() couldn't parse raytracing stage, out of bounds"
					))

				entry.payloadSize = *nextMem;
				++nextMem;
				break;
		}

		if(nextMem + sizeof(U16) * binaryCount > file.ptr + Buffer_length(file))
			retError(clean, Error_outOfBounds(
				0, nextMem - file.ptr, Buffer_length(file), "SHFile_read() couldn't parse binary ids, out of bounds"
			))

		ListU16 binaries = (ListU16) { 0 };
		gotoIfError2(clean, ListU16_createRefConst((const U16*) nextMem, binaryCount, &binaries))
		gotoIfError2(clean, ListU16_insertAll(&entry.binaryIds, binaries, 0, alloc))
		nextMem += ListU16_bytes(binaries);

		for(U64 j = 0; j < binaries.length; ++j) {

			U16 binaryId = binaries.ptr[j];

			if(binaryId >= header.binaryCount)
				retError(clean, Error_outOfBounds(
					0, binaryId, header.binaryCount, "SHFile_read() one of the binary ids it out of bounds"
				))

			U16 entryId = fixedBinaryInfo[binaryId].entrypoint;

			if(entryId != U16_MAX && entryId != i)
				retError(clean, Error_invalidParameter(
					0, 0, "SHFile_read() stage[i].binaries[j] referenced that had mismatching entryId"
				))
		}

		gotoIfError3(clean, SHFile_addEntrypoint(shFile, &entry, alloc, e_rr))
		entry = (SHEntry) { 0 };
	}

	gotoIfError2(clean, Buffer_offset(&file, nextMem - nextMemPrev))

	//Get includes

	for(U64 i = 0; i < header.includeFileCount; ++i) {
		CharString relativePath = DLFile_stringAt(strings, includeNameStart + i, NULL);
		gotoIfError2(clean, CharString_createCopy(relativePath, alloc, &include.relativePath))
		include.crc32c = includeFileCrc32c[i];
		gotoIfError3(clean, SHFile_addInclude(shFile, &include, alloc, e_rr))
	}

	//Verify if every binary has a stage

	for (U64 j = 0; j < shFile->binaries.length; ++j) {

		SHBinaryInfo info = shFile->binaries.ptr[j];

		if (info.hasShaderAnnotation) {

			Bool contains = false;

			for(U64 i = 0; i < shFile->entries.length; ++i) {

				ListU16 binaries = shFile->entries.ptr[i].binaryIds;

				for(U64 k = 0; k < binaries.length; ++k)
					if(binaries.ptr[k] == j) {
						contains = true;
						break;
					}

				if(contains)
					break;
			}

			if(!contains)
				retError(clean, Error_invalidState(
					1, "SHFile_read() contained binary that wasn't referenced by any entrypoints"
				))
		}

		else {

			ListU16 binaries = shFile->entries.ptr[fixedBinaryInfo[j].entrypoint].binaryIds;

			Bool contains = false;

			for(U64 i = 0; i < binaries.length; ++i)
				if(binaries.ptr[i] == j) {
					contains = true;
					break;
				}

			if(!contains)
				retError(clean, Error_invalidState(
					1, "SHFile_read() contained binary that wasn't referenced by the entrypoint"
				))
		}
	}

	//Store read length if it's a subfile

	if(!isSubFile && Buffer_length(file))
		retError(clean, Error_invalidState(1, "SHFile_read() contained extra data, not allowed if it's not a subfile"))

	if(file.ptr)
		shFile->readLength = file.ptr - entireFile.ptr;

	else shFile->readLength = Buffer_length(entireFile);

clean:

	if(!s_uccess && allocate)
		SHFile_free(shFile, alloc);

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