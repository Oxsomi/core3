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

//formats/oiSH/sh_write.c

#include "formats/oiSH/sh_file.h"
#include "formats/oiSH/sh_headers.h"
#include "formats/oiDL/dl_file.h"
#include "formats/oiDL/dl_entry.h"
#include "formats/oiDL/dl_load.h"
#include "types/container/buffer.h"
#include "types/container/ref_ptr.h"
#include "types/container/container_types.h"
#include "types/container/memory_stream.h"
#include "types/container/list_basic_types.h"
#include "types/base/mathi.h"
#include "types/base/allocator.h"
#include "types/base/error.h"
#include "types/base/constants.h"

#include <stddef.h>

Bool SHFile_write(StreamRef *streamRef, U64 *offset, const SHFile *shFile, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	DLFile strings = (DLFile){ 0 };

	DLFile shaderBuffers = (DLFile) { 0 };
	ListListU32 arrays = (ListListU32) { 0 };            //Only contains references
	ListSBFile shaderBufferList = (ListSBFile) { 0 };    //Only contains references

	StreamCursor cursor = (StreamCursor) { 0 };

	const RefPtrType memStreamType = MemoryStream_makeType(alloc);
	MemoryStreamRef *memoryStream = NULL;

	Buffer tmp = Buffer_createNull();

	if (!streamRef || !offset)
		retError(clean, Error_nullPointer(!streamRef ? 0 : 1, "SHFile_write()::streamRef and offset are required"));

	OxStream *stream = RefPtr_data(streamRef, OxStream);

	if (streamRef->refPtrType->typeId != (TypeId)EContainerTypeId_Stream || !stream->write)
		retError(clean, Error_invalidOperation(0, "SHFile_write()::streamRef invalid type"));

	if (*offset & 15)
		retError(clean, Error_unsupportedOperation(0, "SHFile_write() at misaligned offset is unsupported (16-byte)"));

	if (!shFile || !shFile->entries.ptr)
		retError(clean, Error_nullPointer(2, "SHFile_write()::shFile is required"));

	U64 fileStart = *offset;

	//Get header size

	U64 hdrSize = sizeof(SHHeader);

	if(!(shFile->flags & ESHSettingsFlags_HideMagicNumber))        //Magic number (can be hidden by parent; such as oiSC)
		hdrSize += sizeof(U32);

	hdrSize = (hdrSize + 15) & ~15;

	//Get data size

	DLSettings settings = (DLSettings) {
		.dataType = EDLDataType_String,
		.flags = EDLSettingsFlags_HideMagicNumber
	};

	gotoIfError3(clean, DLFile_create(&settings, 0, alloc, &strings, e_rr));

	settings = (DLSettings) {
		.dataType = EDLDataType_Data,
		.flags = EDLSettingsFlags_HideMagicNumber
	};

	gotoIfError3(clean, DLFile_create(&settings, 0, alloc, &shaderBuffers, e_rr));

	//Calculate easy sizes and add define names

	U64 dataSize = 0;

	U64 binaryCount[ESHBinaryType_Count] = { 0 };
	U8 requiredTypes[ESHBinaryType_Count] = { 0 };

	for(U64 i = 0; i < shFile->binaries.length; ++i) {

		SHBinaryInfo binary = shFile->binaries.ptr[i];

		//Register define names

		for (U64 j = 0; j < binary.identifier.defines.length; j += 2) {

			//Only add if it's new

			CharString str = CharString_createRefStrConst(binary.identifier.defines.ptr[j]);

			if(DLFile_findLoadedString(&strings, 0, U64_MAX, &str) != U64_MAX)
				continue;

			gotoIfError3(clean, DLFile_addEntryString(&strings, &str, alloc, e_rr));

			if(strings.entryBuffers.length - shFile->entries.length >= (U16)(U16_MAX - 1))
				retError(clean, Error_invalidState(0, "DLFile didn't have space for define names"));
		}

		//Add size

		dataSize +=
			binary.identifier.defines.length * sizeof(U16) +
			binary.registers.length * sizeof(SHRegister);

		for(U8 j = 0; j < ESHBinaryType_Count; ++j) {

			U64 leni = Buffer_length(binary.binaries[j]);
			dataSize += leni;

			if(leni) {
				++binaryCount[j];
				EXXDataSizeType requiredType = EXXDataSizeType_getRequiredType(leni);
				requiredTypes[j] = U8_max(requiredTypes[j], (U8) requiredType);
			}
		}
	}

	//Add define values

	U64 defineNamesStart = 0;
	U64 defineValStart = strings.entryBuffers.length;

	if (defineValStart >> 16)
		retError(clean, Error_invalidState(0, "SHFile_write() exceeded max define count"));

	for(U64 i = 0; i < shFile->binaries.length; ++i) {

		SHBinaryInfo binary = shFile->binaries.ptr[i];

		//Register define values

		for (U64 j = 1; j < binary.identifier.defines.length; j += 2) {

			//Only add if it's new

			CharString str = CharString_createRefStrConst(binary.identifier.defines.ptr[j]);

			if(DLFile_findLoadedString(&strings, defineValStart, strings.entryBuffers.length, &str) != U64_MAX)
				continue;

			if (strings.entryBuffers.length - defineValStart >= (U16)(U16_MAX - 1))
				retError(clean, Error_invalidState(0, "SHFile_write() DLFile didn't have space for define values"));

			gotoIfError3(clean, DLFile_addEntryString(&strings, &str, alloc, e_rr));
		}
	}
	
	//Add uniforms

	U64 uniNamesStart = strings.entryBuffers.length;

	for (U64 i = 0; i < shFile->binaries.length; ++i) {

		SHBinaryInfo bin = shFile->binaries.ptr[i];
		ListSHUniformRuntime uniforms = bin.identifier.uniforms;

		dataSize += uniforms.length * sizeof(SHUniform);
		dataSize += bin.identifier.uniformData.length;

		for (U64 j = 0; j < uniforms.length; ++j) {

			CharString str = CharString_createRefStrConst(uniforms.ptr[j].name);

			if (DLFile_findLoadedString(&strings, 0, U64_MAX, &str) != U64_MAX)
				continue;

			gotoIfError3(clean, DLFile_addEntryString(&strings, &str, alloc, e_rr));
		}
	}

	U64 regNameStart = strings.entryBuffers.length;

	if ((regNameStart - uniNamesStart) >= U8_MAX)
		retError(clean, Error_invalidState(0, "SHFile_write() exceeded max uniform count"));

	//Add register names, arrays and shader buffers

	for (U64 i = 0; i < shFile->binaries.length; ++i) {

		SHBinaryInfo binary = shFile->binaries.ptr[i];

		for (U64 j = 0; j < binary.registers.length; ++j) {

			SHRegisterRuntime reg = binary.registers.ptr[j];

			//Add name if it's new

			CharString str = CharString_createRefStrConst(reg.name);

			if(DLFile_findLoadedString(&strings, regNameStart, strings.entryBuffers.length, &str) == U64_MAX) {

				if (strings.entryBuffers.length - regNameStart >= (U16)(U16_MAX - 1))
					retError(clean, Error_invalidState(0, "SHFile_write() DLFile didn't have space for register names"));

				gotoIfError3(clean, DLFile_addEntryString(&strings, &str, alloc, e_rr));
			}

			//Add array if it's new

			Bool isSmallArray = reg.arrays.length == 1 && reg.arrays.ptr[0] <= 32767;

			if(reg.arrays.length && !isSmallArray) {

				U16 arrayId = 0;

				for(; arrayId < arrays.length; ++arrayId)
					if(ListU32_eq(arrays.ptr[arrayId], reg.arrays))
						break;

				if (arrayId >= (U16)(U16_MAX - 1))
					retError(clean, Error_invalidState(0, "SHFile_write() No more space for arrays"));

				if(arrayId == arrays.length)
					gotoIfError3(clean, ListListU32_pushBack(&arrays, ListU32_createRefFromList(reg.arrays), alloc, e_rr));
			}

			//Add shader buffer if it's new

			if (reg.shaderBuffer.vars.ptr) {

				U64 shaderBufferId = 0;

				for (; shaderBufferId < shaderBufferList.length; ++shaderBufferId)
					if(shaderBufferList.ptr[shaderBufferId].hash == reg.shaderBuffer.hash)
						break;

				if (shaderBufferId >= (U16)(U16_MAX - 1))
					retError(clean, Error_invalidState(0, "SHFile_write() No more space for shader buffers"));

				if(shaderBufferId == shaderBufferList.length) {
					SBFile buffer = reg.shaderBuffer;
					buffer.flags |= ESBSettingsFlags_HideMagicNumber;
					gotoIfError3(clean, ListSBFile_pushBack(&shaderBufferList, buffer, alloc, e_rr));
				}
			}
		}
	}

	//Arrays are U8 N + U32 count[N]

	for(U64 i = 0; i < arrays.length; ++i)
		dataSize += arrays.ptr[i].length * sizeof(U32);

	//Add includes

	U64 includeStart = strings.entryBuffers.length;

	for (U64 i = 0; i < shFile->includes.length; ++i) {
		CharString includeName = CharString_createRefStrConst(shFile->includes.ptr[i].relativePath);
		gotoIfError3(clean, DLFile_addEntryString(&strings, &includeName, alloc, e_rr));
	}

	//Add entries

	for (U64 i = 0; i < shFile->entries.length; ++i) {

		SHEntry entry = shFile->entries.ptr[i];
		CharString entryName = CharString_createRefStrConst(entry.name);

		gotoIfError3(clean, DLFile_addEntryString(&strings, &entryName, alloc, e_rr));

		switch(entry.stage) {

			default: {

				dataSize += sizeof(U8) * 2;        //U8 inputsAvail, outputsAvail;

				U8 inputs = 0, outputs = 0;

				//We align to 2 elements since we store U4s. So we store 4 bits too much, but who cares.

				for(; inputs < 16; ++inputs)
					if(!entry.inputs[inputs])
						break;

				for(; outputs < 16; ++outputs)
					if(!entry.outputs[outputs])
						break;

				dataSize += inputs + outputs;

				//Entries need extra allocations for semantics if supported

				if (
					entry.inputSemanticNamesU64[0] | entry.inputSemanticNamesU64[1] |
					entry.outputSemanticNamesU64[0] | entry.outputSemanticNamesU64[1]
				)
					dataSize += 1 + inputs + outputs;

				if(entry.stage != ESHPipelineStage_MeshExt && entry.stage != ESHPipelineStage_TaskExt)
					break;
			}

			// fallthrough

			case ESHPipelineStage_Compute:
			case ESHPipelineStage_WorkgraphExt:
				dataSize += sizeof(U16) * 4;            //group x, y, z, waveSize
				break;

			case ESHPipelineStage_RaygenExt:
				break;

			case ESHPipelineStage_ClosestHitExt:
			case ESHPipelineStage_AnyHitExt:
			case ESHPipelineStage_IntersectionExt:
				dataSize += sizeof(U8);                //intersectionSize
				// fallthrough

			case ESHPipelineStage_CallableExt:
			case ESHPipelineStage_MissExt:
				dataSize += sizeof(U8);                //payloadSize
				break;
		}

		dataSize += entry.binaryIds.length * sizeof(U16);
	}

	U64 uniqueSemantics = 0;

	for (U64 i = 0; i < shFile->entries.length; ++i) {

		SHEntry entry = shFile->entries.ptr[i];

		for (U64 j = 0; j < entry.semanticNames.length; ++j) {
			CharString semantic = CharString_createRefStrConst(entry.semanticNames.ptr[j]);
			gotoIfError3(clean, DLFile_addEntryString(&strings, &semantic, alloc, e_rr));
		}

		uniqueSemantics += entry.semanticNames.length;

		if (uniqueSemantics >= U16_MAX)
			retError(clean, Error_outOfBounds(0, uniqueSemantics, U16_MAX, "SHFile_write() exceeded max semantic name count"));

		if (entry.uniqueInputSemantics >= 16)
			retError(clean, Error_outOfBounds(
				0, entry.uniqueInputSemantics, 16, "SHFile_write() exceeded unique input semantic count"
			));
	}

	//We reserve once into a single memory stream to save allocations.
	//We can't fully solve allocations, since DLFile needs to reference the exact size of each DLFile.
	//Since each SBFile needs to be 16-byte aligned, we pad to 16 byte except the last entry.

	U64 shaderBufferSizes = 0;        //Works because we're 16-byte aligned for this DLFile

	for (U64 i = 0; i < shaderBufferList.length; ++i) {

		gotoIfError3(clean, SBFile_write(&shaderBufferList.ptr[i], alloc, NULL, &shaderBufferSizes, e_rr));

		if (i + 1 != shaderBufferList.length)
			shaderBufferSizes = (shaderBufferSizes + 15) & ~15;
	}

	if (shaderBufferSizes)
		gotoIfError3(clean, MemoryStream_create(
			shaderBufferSizes, EMemoryStreamFlags_IsWritable, &memStreamType, &memoryStream, e_rr
		));

	shaderBufferSizes = 0;

	//Create shader buffers and move to DLFile

	for (U64 i = 0; i < shaderBufferList.length; ++i) {

		U64 currOffset = shaderBufferSizes;
		gotoIfError3(clean, SBFile_write(&shaderBufferList.ptr[i], alloc, memoryStream, &shaderBufferSizes, e_rr));

		if (i + 1 != shaderBufferList.length)
			shaderBufferSizes = (shaderBufferSizes + 15) & ~15;

		RefPtr_inc(memoryStream);
		MemoryStreamRef *ref = memoryStream;

		gotoIfError3(clean, DLFile_addEntryStream(
			&shaderBuffers, &ref, currOffset, shaderBufferSizes - currOffset, alloc, e_rr
		));
	}

	//Compute length as cheaply as possible

	U64 len = hdrSize;
	gotoIfError3(clean, DLFile_write(&strings, alloc, NULL, NULL, I32x4_zero(), &len, e_rr));
	len = (len + 15) & ~15;
	gotoIfError3(clean, DLFile_write(&shaderBuffers, alloc, NULL, NULL, I32x4_zero(), &len, e_rr));

	len +=
		shFile->entries.length * sizeof(EntryInfoFixedSize) +
		shFile->binaries.length * sizeof(BinaryInfoFixedSize) +
		shFile->includes.length * sizeof(U32) +
		arrays.length * sizeof(U8) +
		dataSize;

	U8 sizeTypes = 0;

	for (U8 j = 0; j < ESHBinaryType_Count; ++j) {
		len += SIZE_BYTE_TYPE[requiredTypes[j]] * binaryCount[j];
		sizeTypes |= requiredTypes[j] << (j << 1);
	}

	if (stream->reserve)
		gotoIfError3(clean, stream->reserve(stream, *offset + len, alloc, e_rr));

	//Create header & DLFiles

	typedef struct SHMagicHeader {
		U32 magic;
		SHHeader header;
	} SHMagicHeader;

	SHMagicHeader header = (SHMagicHeader) {
		.magic = SHHeader_MAGIC,
		.header = (SHHeader) {
			.compilerVersion = shFile->compilerVersion,
			.hash = 0,        //Computed afterwards
			.sourceHash = shFile->sourceHash,
			.uniqueDefines = (U16) defineValStart,
			.version = ESHVersion_V1_2,
			.extensionCount = ESHExtension_Count,
			.sizeTypes = sizeTypes,
			.binaryCount = (U16) shFile->binaries.length,
			.stageCount = (U16) shFile->entries.length,
			.includeFileCount = (U16) shFile->includes.length,
			.semanticCount = (U16) uniqueSemantics,
			.arrayDimCount = (U16) arrays.length,
			.registerNameCount = (U16) (includeStart - regNameStart),
			.uniformNameCount = (U16) (regNameStart - uniNamesStart),

			//Persist the settings flags. HideMagicNumber is a serialization detail derived from the magic on read,
			//so it isn't stored; ReflectionOnly (and any future content flags) round-trips here.

			.flags = (U16) (shFile->flags & ~(ESHSettingsFlags) ESHSettingsFlags_HideMagicNumber)
		}
	};

	U64 hdrOff = (shFile->flags & ESHSettingsFlags_HideMagicNumber) ? sizeof(U32) : 0;

	gotoIfError3(clean, stream->write(        //Rawdog this, we can't pass StreamCursor to DLFile yet
		stream,
		*offset,
		sizeof(header) - hdrOff,
		Buffer_createRefConst((const U8*)&header + hdrOff, sizeof(header) - hdrOff),
		alloc,
		e_rr
	));

	*offset = (*offset + sizeof(header) - hdrOff + 15) & ~15;

	gotoIfError3(clean, DLFile_write(&strings, alloc, streamRef, NULL, I32x4_zero(), offset, e_rr));
	*offset = (*offset + 15) & ~15;
	gotoIfError3(clean, DLFile_write(&shaderBuffers, alloc, streamRef, NULL, I32x4_zero(), offset, e_rr));

	//Easy contents of SHFile (relatively static sized)

	gotoIfError3(clean, StreamCursor_create(streamRef, 0, true, alloc, &cursor, e_rr));

	U64 entries = shFile->entries.length;

	for (U64 i = 0; i < shFile->binaries.length; ++i) {

		SHBinaryInfo binary = shFile->binaries.ptr[i];

		ESHBinaryFlags binaryFlags = ESHBinaryFlags_None;

		if (binary.hasShaderAnnotation)
			binaryFlags |= ESHBinaryFlags_HasShaderAnnotation;

		for (U8 j = 0; j < ESHBinaryType_Count; ++j)
			if (Buffer_length(binary.binaries[j]))
				binaryFlags |= 1 << j;

		U64 entryStart = strings.entryBuffers.length - uniqueSemantics - entries;
		U64 entrypoint = entryStart + U16_MAX;        //Indicate no entry

		if (!binary.hasShaderAnnotation) {

			entrypoint = DLFile_findLoadedString(
				&strings, entryStart, strings.entryBuffers.length, &binary.identifier.entrypoint
			);

			if (entrypoint == U64_MAX)
				retError(clean, Error_invalidState(0, "SHFile_write() couldn't link binary to entrypoint"));
		}

		BinaryInfoFixedSize binInfo = (BinaryInfoFixedSize) {

			.shaderModel = ((binary.identifier.shaderVersion >> 4) & 0xF0) | (binary.identifier.shaderVersion & 0xF),
			.entrypointType = (U8)binary.identifier.stageType,
			.entrypoint = (U16)(entrypoint - entryStart),

			.vendorMask = binary.vendorMask,
			.defineCount = (U8)(binary.identifier.defines.length >> 1),
			.binaryFlags = binaryFlags,

			.extensions = binary.identifier.extensions,
			.dormantExt = binary.dormantExtensions,

			.registerCount = (U16)binary.registers.length,
			.uniformCount = (U8)binary.identifier.uniforms.length
		};

		gotoIfError3(clean, StreamCursor_append(&cursor, offset, &binInfo, sizeof(binInfo), alloc, e_rr));
	}

	for (U64 i = 0; i < entries; ++i) {

		SHEntry entry = shFile->entries.ptr[i];

		EntryInfoFixedSize entryInfo = (EntryInfoFixedSize) {
			.pipelineStage = entry.stage,
			.binaryCount = (U8)entry.binaryIds.length
		};

		gotoIfError3(clean, StreamCursor_append(&cursor, offset, &entryInfo, sizeof(entryInfo), alloc, e_rr));
	}

	for (U64 i = 0; i < shFile->includes.length; ++i)
		gotoIfError3(clean, StreamCursor_appendU32(&cursor, offset, shFile->includes.ptr[i].crc32c, alloc, e_rr));

	for (U64 i = 0; i < arrays.length; ++i)
		gotoIfError3(clean, StreamCursor_appendU8(&cursor, offset, (U8)arrays.ptr[i].length, alloc, e_rr));

	for (U64 i = 0; i < arrays.length; ++i)
		gotoIfError3(clean, StreamCursor_appendBuffer(&cursor, offset, ListU32_bufferConst(arrays.ptr[i]), alloc, e_rr));

	//Fill binaries

	for (U64 i = 0; i < shFile->binaries.length; ++i) {

		SHBinaryInfo binary = shFile->binaries.ptr[i];

		U8 defineCount = (U8)(binary.identifier.defines.length >> 1);
		ListCharString defines = binary.identifier.defines;

		for (U64 j = 0; j < defineCount; ++j) {

			U64 defineName = DLFile_findLoadedString(&strings, defineNamesStart, defineValStart, &defines.ptr[j << 1]);

			if (defineName == U64_MAX)
				retError(clean, Error_invalidState(0, "SHFile_write() couldn't find define name"));

			gotoIfError3(clean, StreamCursor_appendU16(&cursor, offset, (U16)(defineName - defineNamesStart), alloc, e_rr));
		}

		for (U64 j = 0; j < defineCount; ++j) {

			U64 defineValue = DLFile_findLoadedString(&strings, defineValStart, uniNamesStart, &defines.ptr[(j << 1) | 1]);

			if (defineValue == U64_MAX)
				retError(clean, Error_invalidState(0, "SHFile_write() couldn't find define value"));

			gotoIfError3(clean, StreamCursor_appendU16(&cursor, offset, (U16)(defineValue - defineValStart), alloc, e_rr));
		}

		for (U64 j = 0; j < binary.identifier.uniforms.length; ++j) {

			SHUniformRuntime uniform = binary.identifier.uniforms.ptr[j];
			U64 uniformNameId = DLFile_findLoadedString(&strings, uniNamesStart, regNameStart, &uniform.name);

			if (uniformNameId == U64_MAX)
				retError(clean, Error_invalidState(0, "SHFile_write() couldn't find uniformName"));

			SHUniform uniformStatic = (SHUniform) {
				.dataOffset = uniform.dataOffset,
				.typeIdShort = uniform.typeIdShort,
				.nameId = (U8)(uniformNameId - uniNamesStart)
			};

			gotoIfError3(clean, StreamCursor_append(&cursor, offset, &uniformStatic, sizeof(uniformStatic), alloc, e_rr));
		}

		for (U64 j = 0; j < binary.registers.length; ++j) {

			SHRegisterRuntime reg = binary.registers.ptr[j];

			U64 regName = DLFile_findLoadedString(&strings, regNameStart, includeStart, &reg.name);

			if (regName == U64_MAX)
				retError(clean, Error_invalidState(0, "SHFile_write() couldn't find regName"));

			reg.reg.nameId = (U16) (regName - regNameStart);

			if(!reg.arrays.length)
				reg.reg.arrayDimOrId = 0;

			else {

				if (reg.arrays.length == 1 && reg.arrays.ptr[0] <= 32767)
					reg.reg.arrayDimOrId = (U16)reg.arrays.ptr[0];

				else for (U64 arrayId = 0; arrayId < arrays.length; ++arrayId)
					if (ListU32_eq(arrays.ptr[arrayId], reg.arrays)) {
						reg.reg.arrayDimOrId = (U16)arrayId | 0x8000;
						break;
					}
			}

			U8 realReg = reg.reg.registerType & ESHRegisterType_TypeMask;

			if(realReg >= ESHRegisterType_BufferStart && realReg <= ESHRegisterType_BufferEnd)
				reg.reg.shaderBufferId = U16_MAX;

			if(reg.shaderBuffer.vars.ptr) {

				U64 k = 0;

				for (; k < shaderBufferList.length; ++k)
					if(shaderBufferList.ptr[k].hash == reg.shaderBuffer.hash)
						break;

				reg.reg.shaderBufferId = (U16) k;
			}

			gotoIfError3(clean, StreamCursor_append(&cursor, offset, &reg.reg, sizeof(reg.reg), alloc, e_rr));
		}

		gotoIfError3(clean, StreamCursor_appendBuffer(
			&cursor, offset, ListU8_bufferConst(binary.identifier.uniformData), alloc, e_rr
		));

		for (U8 j = 0; j < ESHBinaryType_Count; ++j) {

			U64 length = Buffer_length(binary.binaries[j]);

			if(length)
				gotoIfError3(clean, StreamCursor_appendSizeType(&cursor, offset, length, requiredTypes[j], alloc, e_rr));
		}

		for (U8 j = 0; j < ESHBinaryType_Count; ++j)
			gotoIfError3(clean, StreamCursor_appendBuffer(&cursor, offset, binary.binaries[j], alloc, e_rr));
	}

	//Fill entries

	for (U64 i = 0; i < entries; ++i) {

		SHEntry entry = shFile->entries.ptr[i];

		switch(entry.stage) {

			default: {

				U8 inputs = 0, outputs = 0;

				for (; inputs < 16; ++inputs)
					if (!entry.inputs[inputs])
						break;

				for (; outputs < 16; ++outputs)
					if (!entry.outputs[outputs])
						break;

				Bool hasSemantics =
					entry.inputSemanticNamesU64[0] | entry.inputSemanticNamesU64[1] |
					entry.outputSemanticNamesU64[0] | entry.outputSemanticNamesU64[1];

				gotoIfError3(clean, StreamCursor_appendU8(&cursor, offset, inputs | (hasSemantics ? 0x80 : 0), alloc, e_rr));
				gotoIfError3(clean, StreamCursor_appendU8(&cursor, offset, outputs, alloc, e_rr));

				gotoIfError3(clean, StreamCursor_append(&cursor, offset, entry.inputs, inputs * sizeof(U8), alloc, e_rr));
				gotoIfError3(clean, StreamCursor_append(&cursor, offset, entry.outputs, outputs * sizeof(U8), alloc, e_rr));

				if(hasSemantics) {

					U8 semanticCounts =
						((U8) entry.uniqueInputSemantics) |
						((U8) (entry.semanticNames.length - entry.uniqueInputSemantics) << 4);

					gotoIfError3(clean, StreamCursor_appendU8(&cursor, offset, semanticCounts, alloc, e_rr));

					gotoIfError3(clean, StreamCursor_append(&cursor, offset, entry.inputSemanticNames, inputs * sizeof(U8), alloc, e_rr));
					gotoIfError3(clean, StreamCursor_append(&cursor, offset, entry.outputSemanticNames, outputs * sizeof(U8), alloc, e_rr));
				}

				if(entry.stage != ESHPipelineStage_MeshExt && entry.stage != ESHPipelineStage_TaskExt)
					break;
			}

			// fallthrough

			case ESHPipelineStage_Compute:
			case ESHPipelineStage_WorkgraphExt: {
				SHGroups groups = { .x = entry.groupX, .y = entry.groupY, .z = entry.groupZ, .waveSize = entry.waveSize };
				gotoIfError3(clean, StreamCursor_append(&cursor, offset, &groups, sizeof(groups), alloc, e_rr));
				break;
			}

			case ESHPipelineStage_RaygenExt:
				break;

			case ESHPipelineStage_ClosestHitExt:
			case ESHPipelineStage_AnyHitExt:
			case ESHPipelineStage_IntersectionExt:
				gotoIfError3(clean, StreamCursor_appendU8(&cursor, offset, entry.intersectionSize, alloc, e_rr));
				//fallthrough

			case ESHPipelineStage_CallableExt:
			case ESHPipelineStage_MissExt:
				gotoIfError3(clean, StreamCursor_appendU8(&cursor, offset, entry.payloadSize, alloc, e_rr));
				break;
		}

		gotoIfError3(clean, StreamCursor_appendBuffer(&cursor, offset, ListU16_bufferConst(entry.binaryIds), alloc, e_rr));
	}

	gotoIfError3(clean, StreamCursor_setReadOnly(&cursor, alloc, e_rr));

	//Finalize by adding the hash

	U64 ignoredStart = offsetof(SHHeader, uniqueDefines);
	U64 headerStart = fileStart;

	if (!(shFile->flags & ESHSettingsFlags_HideMagicNumber)) {
		ignoredStart += sizeof(U32);
		headerStart += sizeof(U32);
	}

	U64 hashStart = fileStart + ignoredStart;
	U64 hashLen = *offset - hashStart;

	gotoIfError3(clean, Buffer_createUninitializedBytes(hashLen, alloc, &tmp, e_rr));
	gotoIfError3(clean, StreamCursor_read(&cursor, tmp, hashStart, 0, hashLen, false, alloc, e_rr));

	U32 hash = Buffer_crc32c(tmp);
	Buffer_free(&tmp, alloc);

	gotoIfError3(clean, StreamCursor_setWritable(&cursor, e_rr));

	U64 hashLoc = headerStart + offsetof(SHHeader, hash);
	gotoIfError3(clean, StreamCursor_appendU32(&cursor, &hashLoc, hash, alloc, e_rr));

clean:
	StreamCursor_close(&cursor, alloc);
	RefPtr_dec(&memoryStream);
	ListSBFile_free(&shaderBufferList, alloc);
	ListListU32_free(&arrays, alloc);            //Doesn't need freeUnderlying, it's all references
	DLFile_free(&shaderBuffers, alloc);
	DLFile_free(&strings, alloc);
	Buffer_free(&tmp, alloc);
	return s_uccess;
}
