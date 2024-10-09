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
#include "formats/oiDL/dl_file.h"
#include "types/allocator.h"
#include "types/error.h"
#include "types/buffer.h"
#include "types/math.h"

#include <stddef.h>

Bool SHFile_write(SHFile shFile, Allocator alloc, Buffer *result, Error *e_rr) {

	Bool s_uccess = true;

	if(!shFile.entries.ptr)
		retError(clean, Error_nullPointer(0, "SHFile_write()::shFile is required"))

	if(!result)
		retError(clean, Error_nullPointer(2, "SHFile_write()::result is required"))

	if(result->ptr)
		retError(clean, Error_invalidOperation(0, "SHFile_write()::result wasn't empty, might indicate memleak"))

	//Get header size

	U64 headerSize = sizeof(SHHeader);

	if(!(shFile.flags & ESHSettingsFlags_HideMagicNumber))		//Magic number (can be hidden by parent; such as oiSC)
		headerSize += sizeof(U32);

	//Get data size

	Bool isUTF8 = shFile.flags & ESHSettingsFlags_IsUTF8;

	DLSettings settings = (DLSettings) {
		.dataType = isUTF8 ? EDLDataType_UTF8 : EDLDataType_Ascii,
		.flags = EDLSettingsFlags_HideMagicNumber
	};

	DLFile strings = (DLFile) { 0 };
	Buffer stringsDlFile = Buffer_createNull();

	gotoIfError3(clean, DLFile_create(settings, alloc, &strings, e_rr))

	DLFile shaderBuffers = (DLFile) { 0 };
	Buffer shaderBuffersDlFile = Buffer_createNull();
	ListListU32 arrays = (ListListU32) { 0 };			//Only contains references
	ListSBFile shaderBufferList = (ListSBFile) { 0 };	//Only contains references

	settings = (DLSettings) {
		.dataType = EDLDataType_Data,
		.flags = EDLSettingsFlags_HideMagicNumber
	};

	gotoIfError3(clean, DLFile_create(settings, alloc, &shaderBuffers, e_rr))

	//Calculate easy sizes and add uniform names

	U64 binaryCount[ESHBinaryType_Count] = { 0 };
	U8 requiredTypes[ESHBinaryType_Count] = { 0 };

	for(U64 i = 0; i < shFile.binaries.length; ++i) {

		SHBinaryInfo binary = shFile.binaries.ptr[i];

		//Register uniform names

		for (U64 j = 0; j < binary.identifier.uniforms.length; j += 2) {

			//Only add if it's new

			CharString str = binary.identifier.uniforms.ptr[j];

			if(DLFile_find(strings, 0, U64_MAX, str) != U64_MAX)
				continue;

			if(isUTF8)
				gotoIfError3(clean, DLFile_addEntryUTF8(&strings, CharString_bufferConst(str), alloc, e_rr))

			else gotoIfError3(clean, DLFile_addEntryAscii(&strings, CharString_createRefStrConst(str), alloc, e_rr))

			if(strings.entryBuffers.length - shFile.entries.length >= (U16)(U16_MAX - 1))
				retError(clean, Error_invalidState(0, "DLFile didn't have space for uniform names"))
		}

		//Add size

		headerSize +=
			binary.identifier.uniforms.length * sizeof(U16) +
			binary.registers.length * sizeof(SHRegister);

		for(U8 j = 0; j < ESHBinaryType_Count; ++j) {

			U64 leni = Buffer_length(binary.binaries[j]);
			headerSize += leni;

			if(leni) {
				++binaryCount[j];
				EXXDataSizeType requiredType = EXXDataSizeType_getRequiredType(leni);
				requiredTypes[j] = U8_max(requiredTypes[j], (U8) requiredType);
			}
		}
	}

	//Add uniform values

	U64 uniNamesStart = 0;
	U64 uniValStart = strings.entryBuffers.length;

	if(uniValStart >> 16)
		retError(clean, Error_invalidState(0, "SHFile_write() exceeded max uniform count"))

	for(U64 i = 0; i < shFile.binaries.length; ++i) {

		SHBinaryInfo binary = shFile.binaries.ptr[i];

		//Register uniform values

		for (U64 j = 1; j < binary.identifier.uniforms.length; j += 2) {

			//Only add if it's new

			CharString str = binary.identifier.uniforms.ptr[j];

			if(DLFile_find(strings, uniValStart, strings.entryBuffers.length, str) != U64_MAX)
				continue;

			if(strings.entryBuffers.length - uniValStart >= (U16)(U16_MAX - 1))
				retError(clean, Error_invalidState(0, "SHFile_write() DLFile didn't have space for uniform values"))

			if(isUTF8)
				gotoIfError3(clean, DLFile_addEntryUTF8(&strings, CharString_bufferConst(str), alloc, e_rr))

			else gotoIfError3(clean, DLFile_addEntryAscii(&strings, CharString_createRefStrConst(str), alloc, e_rr))
		}
	}

	U64 regNameStart = strings.entryBuffers.length;

	//Add register names, arrays and shader buffers

	for (U64 i = 0; i < shFile.binaries.length; ++i) {

		SHBinaryInfo binary = shFile.binaries.ptr[i];

		for (U64 j = 0; j < binary.registers.length; ++j) {

			SHRegisterRuntime reg = binary.registers.ptr[j];

			//Add name if it's new

			CharString str = reg.name;

			if(DLFile_find(strings, regNameStart, strings.entryBuffers.length, str) == U64_MAX) {

				if(strings.entryBuffers.length - regNameStart >= (U16)(U16_MAX - 1))
					retError(clean, Error_invalidState(0, "SHFile_write() DLFile didn't have space for register names"))

				if(isUTF8)
					gotoIfError3(clean, DLFile_addEntryUTF8(&strings, CharString_bufferConst(str), alloc, e_rr))

				else gotoIfError3(clean, DLFile_addEntryAscii(&strings, CharString_createRefStrConst(str), alloc, e_rr))
			}

			//Add array if it's new

			if(reg.arrays.length) {

				U16 arrayId = 0;

				for(; arrayId < arrays.length; ++arrayId)
					if(ListU32_eq(arrays.ptr[arrayId], reg.arrays))
						break;

				if(arrayId >= (U16)(U16_MAX - 1))
					retError(clean, Error_invalidState(0, "SHFile_write() No more space for arrays"))

				if(arrayId == arrays.length)
					gotoIfError2(clean, ListListU32_pushBack(&arrays, ListU32_createRefFromList(reg.arrays), alloc))
			}

			//Add shader buffer if it's new

			if (reg.shaderBuffer.vars.ptr) {

				U64 shaderBufferId = 0;

				for (; shaderBufferId < shaderBufferList.length; ++shaderBufferId)
					if(shaderBufferList.ptr[shaderBufferId].hash == reg.shaderBuffer.hash)
						break;

				if(shaderBufferId >= (U16)(U16_MAX - 1))
					retError(clean, Error_invalidState(0, "SHFile_write() No more space for shader buffers"))

				if(shaderBufferId == shaderBufferList.length) {
					SBFile buffer = reg.shaderBuffer;
					buffer.flags |= ESBSettingsFlags_HideMagicNumber;
					gotoIfError2(clean, ListSBFile_pushBack(&shaderBufferList, buffer, alloc))
				}
			}
		}
	}

	//Arrays are U8 N + U32 count[N]

	for(U64 i = 0; i < arrays.length; ++i)
		headerSize += arrays.ptr[i].length * sizeof(U32);

	//Add includes

	U64 includeStart = strings.entryBuffers.length;

	for (U64 i = 0; i < shFile.includes.length; ++i) {

		CharString includeName = shFile.includes.ptr[i].relativePath;

		if(isUTF8)
			gotoIfError3(clean, DLFile_addEntryUTF8(&strings, CharString_bufferConst(includeName), alloc, e_rr))

		else gotoIfError3(clean, DLFile_addEntryAscii(&strings, CharString_createRefStrConst(includeName), alloc, e_rr))
	}

	//Add entries

	for (U64 i = 0; i < shFile.entries.length; ++i) {

		SHEntry entry = shFile.entries.ptr[i];
		CharString entryName = entry.name;

		if(isUTF8)
			gotoIfError3(clean, DLFile_addEntryUTF8(&strings, CharString_bufferConst(entryName), alloc, e_rr))

		else gotoIfError3(clean, DLFile_addEntryAscii(&strings, CharString_createRefStrConst(entryName), alloc, e_rr))

		switch(entry.stage) {

			default: {

				headerSize += sizeof(U8) * 2;		//U8 inputsAvail, outputsAvail;

				U8 inputs = 0, outputs = 0;

				//We align to 2 elements since we store U4s. So we store 4 bits too much, but who cares.

				for(; inputs < 16; ++inputs)
					if(!entry.inputs[inputs])
						break;

				for(; outputs < 16; ++outputs)
					if(!entry.outputs[outputs])
						break;

				headerSize += inputs + outputs;

				//Entries need extra allocations for semantics if supported

				if (
					entry.inputSemanticNamesU64[0] | entry.inputSemanticNamesU64[1] |
					entry.outputSemanticNamesU64[0] | entry.outputSemanticNamesU64[1]
				)
					headerSize += 1 + inputs + outputs;

				if(entry.stage != ESHPipelineStage_MeshExt && entry.stage != ESHPipelineStage_TaskExt)
					break;
			}

			// fallthrough

			case ESHPipelineStage_Compute:
			case ESHPipelineStage_WorkgraphExt:
				headerSize += sizeof(U16) * 4;			//group x, y, z, waveSize
				break;

			case ESHPipelineStage_RaygenExt:
			case ESHPipelineStage_CallableExt:
				break;

			case ESHPipelineStage_ClosestHitExt:
			case ESHPipelineStage_AnyHitExt:
			case ESHPipelineStage_IntersectionExt:
				headerSize += sizeof(U8);				//intersectionSize
				// fallthrough

			case ESHPipelineStage_MissExt:
				headerSize += sizeof(U8);				//payloadSize
				break;
		}

		headerSize += entry.binaryIds.length * sizeof(U16);
	}

	U64 uniqueSemantics = 0;

	for (U64 i = 0; i < shFile.entries.length; ++i) {

		SHEntry entry = shFile.entries.ptr[i];

		for (U64 j = 0; j < entry.semanticNames.length; ++j) {

			CharString semantic = entry.semanticNames.ptr[j];

			if(isUTF8)
				gotoIfError3(clean, DLFile_addEntryUTF8(&strings, CharString_bufferConst(semantic), alloc, e_rr))

			else gotoIfError3(clean, DLFile_addEntryAscii(&strings, CharString_createRefStrConst(semantic), alloc, e_rr))
		}

		uniqueSemantics += entry.semanticNames.length;

		if(uniqueSemantics >= U16_MAX)
			retError(clean, Error_outOfBounds(0, uniqueSemantics, U16_MAX, "SHFile_write() exceeded max semantic name count"))

		if(entry.uniqueInputSemantics >= 16)
			retError(clean, Error_outOfBounds(
				0, entry.uniqueInputSemantics, 16, "SHFile_write() exceeded unique input semantic count"
			))
	}

	//Create shader buffers and move to DLFile

	for (U64 i = 0; i < shaderBufferList.length; ++i) {
		gotoIfError3(clean, SBFile_write(shaderBufferList.ptr[i], alloc, &shaderBuffersDlFile, e_rr))
		gotoIfError3(clean, DLFile_addEntry(&shaderBuffers, shaderBuffersDlFile, alloc, e_rr))
		shaderBuffersDlFile = Buffer_createNull();
	}

	//Create DLFiles and calculate length

	gotoIfError3(clean, DLFile_write(strings, alloc, &stringsDlFile, e_rr))
	gotoIfError3(clean, DLFile_write(shaderBuffers, alloc, &shaderBuffersDlFile, e_rr))

	U64 len =
		headerSize +
		Buffer_length(stringsDlFile) +
		Buffer_length(shaderBuffersDlFile) +
		shFile.entries.length * sizeof(EntryInfoFixedSize) +
		shFile.binaries.length * sizeof(BinaryInfoFixedSize) +
		shFile.includes.length * sizeof(U32) +
		arrays.length * sizeof(U8);

	//Create sizes and calculate binary size buffer size

	U8 sizeTypes = 0;

	for(U8 j = 0; j < ESHBinaryType_Count; ++j) {
		len += SIZE_BYTE_TYPE[requiredTypes[j]] * binaryCount[j];
		sizeTypes |= requiredTypes[j] << (j << 1);
	}

	//Create buffer

	gotoIfError2(clean, Buffer_createUninitializedBytes(len, alloc, result))

	//Prepend header

	U8 *headerIt = (U8*)result->ptr;

	if (!(shFile.flags & ESHSettingsFlags_HideMagicNumber)) {
		*(U32*)headerIt = SHHeader_MAGIC;
		headerIt += sizeof(U32);
	}

	SHHeader *shHeader = (SHHeader*) headerIt;

	*shHeader = (SHHeader) {
		.version = ESHVersion_V1_2,
		.sizeTypes = sizeTypes,
		.binaryCount = (U16) shFile.binaries.length,
		.stageCount = (U16) shFile.entries.length,
		.uniqueUniforms = (U16) uniValStart,
		.includeFileCount = (U16) shFile.includes.length,
		.semanticCount = (U16) uniqueSemantics,
		.arrayDimCount = (U16) arrays.length,
		.registerNameCount = (U16) (includeStart - regNameStart)
	};

	headerIt += sizeof(SHHeader);

	Buffer_copy(Buffer_createRef(headerIt, Buffer_length(stringsDlFile)), stringsDlFile);
	headerIt += Buffer_length(stringsDlFile);

	Buffer_copy(Buffer_createRef(headerIt, Buffer_length(shaderBuffersDlFile)), shaderBuffersDlFile);
	headerIt += Buffer_length(shaderBuffersDlFile);

	//Find offsets

	BinaryInfoFixedSize *binaryStaticStart = (BinaryInfoFixedSize*) headerIt;
	EntryInfoFixedSize *stagesStaticStart = (EntryInfoFixedSize*) (binaryStaticStart + shFile.binaries.length);
	U32 *crc32c = (U32*) (stagesStaticStart + shFile.entries.length);
	U8 *arrayDims = (U8*) (crc32c + shFile.includes.length);
	U32 *arrayCount = (U32*) (arrayDims + arrays.length);

	for (U64 i = 0; i < arrays.length; ++i) {
		arrayDims[i] = (U8) arrays.ptr[i].length;
		Buffer_copy(Buffer_createRef(arrayCount, ListU32_bytes(arrays.ptr[i])), ListU32_bufferConst(arrays.ptr[i]));
		arrayCount += arrayDims[i];
	}

	headerIt = (U8*) arrayCount;

	//Fill binaries

	U64 entries = shFile.entries.length;

	for (U64 i = 0; i < shFile.binaries.length; ++i) {

		SHBinaryInfo binary = shFile.binaries.ptr[i];

		//Static part

		ESHBinaryFlags binaryFlags = ESHBinaryFlags_None;

		if(binary.hasShaderAnnotation)
			binaryFlags |= ESHBinaryFlags_HasShaderAnnotation;

		for(U8 j = 0; j < ESHBinaryType_Count; ++j)
			if(Buffer_length(binary.binaries[j]))
				binaryFlags |= 1 << j;

		U64 entryStart = strings.entryBuffers.length - uniqueSemantics - entries;
		U64 entrypoint = entryStart + U16_MAX;		//Indicate no entry

		if(!binary.hasShaderAnnotation) {

			entrypoint = DLFile_find(
				strings, entryStart, strings.entryBuffers.length, binary.identifier.entrypoint
			);

			if(entrypoint == U64_MAX)
				retError(clean, Error_invalidState(0, "SHFile_write() couldn't link binary to entrypoint"))
		}

		U8 uniformCount = (U8)(binary.identifier.uniforms.length >> 1);

		binaryStaticStart[i] = (BinaryInfoFixedSize) {

			.shaderModel = ((binary.identifier.shaderVersion >> 4) & 0xF0) | (binary.identifier.shaderVersion & 0xF),
			.entrypointType = (U8) binary.identifier.stageType,
			.entrypoint = (U16)(entrypoint - entryStart),

			.vendorMask = binary.vendorMask,
			.uniformCount = uniformCount,
			.binaryFlags = binaryFlags,

			.extensions = binary.identifier.extensions,
			.registerCount = (U16) binary.registers.length
		};

		//Dynamic part

		U16 *uniformNames = (U16*) headerIt;
		headerIt += sizeof(U16) * uniformCount;

		U16 *uniValues = (U16*) headerIt;
		headerIt += sizeof(U16) * uniformCount;

		for(U64 j = 0; j < uniformCount; ++j) {
			ListCharString uniforms = binary.identifier.uniforms;
			uniformNames[j] = (U16) (DLFile_find(strings, uniNamesStart, uniValStart, uniforms.ptr[j << 1]) - uniNamesStart);
			uniValues[j] = (U16) (DLFile_find(strings, uniValStart, includeStart, uniforms.ptr[(j << 1) | 1]) - uniValStart);
		}

		SHRegister *regs = (SHRegister*) headerIt;

		for (U64 j = 0; j < binary.registers.length; ++j) {

			SHRegisterRuntime reg = binary.registers.ptr[j];

			reg.reg.nameId = (U16) (DLFile_find(strings, regNameStart, includeStart, reg.name) - regNameStart);

			if(!reg.arrays.length)
				reg.reg.arrayId = U16_MAX;

			else for(U64 arrayId = 0; arrayId < arrays.length; ++arrayId)
				if(ListU32_eq(arrays.ptr[arrayId], reg.arrays)) {
					reg.reg.arrayId = (U16) arrayId;
					break;
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

			regs[j] = reg.reg;
		}

		headerIt += sizeof(SHRegister) * binary.registers.length;

		for (U8 j = 0; j < ESHBinaryType_Count; ++j) {

			U64 length = Buffer_length(binary.binaries[j]);

			if(length)
				headerIt += Buffer_forceWriteSizeType(headerIt, requiredTypes[j], length);
		}

		for (U8 j = 0; j < ESHBinaryType_Count; ++j) {

			U64 length = Buffer_length(binary.binaries[j]);

			if (length) {
				Buffer_copy(Buffer_createRef(headerIt, length), binary.binaries[j]);
				headerIt += length;
			}
		}
	}

	//Fill entries

	for (U64 i = 0; i < shFile.entries.length; ++i) {

		SHEntry entry = shFile.entries.ptr[i];

		//Static part

		stagesStaticStart[i] = (EntryInfoFixedSize) {
			.pipelineStage = entry.stage,
			.binaryCount = (U8) entry.binaryIds.length
		};

		//Dynamic part

		switch(entry.stage) {

			default: {

				U8 inputs = 0, outputs = 0;

				U8 *begin = headerIt;
				headerIt += sizeof(U8) * 2;		//U8 inputsAvail, outputsAvail;

				for(; inputs < 16; ++inputs) {

					U8 input = entry.inputs[inputs];

					if(!input)
						break;

					*headerIt = input;
					++headerIt;
				}

				for(; outputs < 16; ++outputs) {

					U8 output = entry.outputs[outputs];

					if(!output)
						break;

					*headerIt = output;
					++headerIt;
				}

				Bool hasSemantics =
					entry.inputSemanticNamesU64[0]  | entry.inputSemanticNamesU64[1] |
					entry.outputSemanticNamesU64[0] | entry.outputSemanticNamesU64[1];

				begin[0] = inputs | (hasSemantics ? 0x80 : 0);
				begin[1] = outputs;

				if(hasSemantics) {

					headerIt[0] =
						((U8) entry.uniqueInputSemantics) |
						((U8) (entry.semanticNames.length - entry.uniqueInputSemantics) << 4);

					for (U8 j = 0; j < inputs; ++j)
						headerIt[1 + j] = entry.inputSemanticNames[j];

					for (U8 j = 0; j < outputs; ++j)
						headerIt[1 + inputs + j] = entry.outputSemanticNames[j];

					headerIt += 1 + inputs + outputs;
				}

				if(entry.stage != ESHPipelineStage_MeshExt && entry.stage != ESHPipelineStage_TaskExt)
					break;
			}

			// fallthrough

			case ESHPipelineStage_Compute:
			case ESHPipelineStage_WorkgraphExt: {

				U16 *groups = (U16*) headerIt;

				groups[0] = entry.groupX;
				groups[1] = entry.groupY;
				groups[2] = entry.groupZ;
				groups[3] = entry.waveSize;

				headerIt += sizeof(U16) * 4;
				break;
			}

			case ESHPipelineStage_RaygenExt:
			case ESHPipelineStage_CallableExt:
				break;

			case ESHPipelineStage_ClosestHitExt:
			case ESHPipelineStage_AnyHitExt:
			case ESHPipelineStage_IntersectionExt:
				*headerIt = entry.intersectionSize;
				headerIt += sizeof(U8);				//intersectionSize
				//fallthrough

			case ESHPipelineStage_MissExt:
				*headerIt = entry.payloadSize;
				headerIt += sizeof(U8);				//payloadSize
				break;
		}

		for (U64 j = 0; j < entry.binaryIds.length; ++j) {
			*(U16*)headerIt = entry.binaryIds.ptr[j];
			headerIt += sizeof(U16);
		}
	}

	//Fill includes

	for (U64 i = 0; i < shFile.includes.length; ++i)
		crc32c[i] = shFile.includes.ptr[i].crc32c;

	//Finalize by adding the hash

	U64 hashLen = Buffer_length(*result) - offsetof(SHHeader, uniqueUniforms);
	const U8 *shHashStart = (const U8*) &shHeader->uniqueUniforms;

	if (!(shFile.flags & ESHSettingsFlags_HideMagicNumber))
		hashLen -= sizeof(U32);

	shHeader->hash = Buffer_crc32c(Buffer_createRefConst((const U8*) shHashStart, hashLen));
	shHeader->compilerVersion = shFile.compilerVersion;
	shHeader->sourceHash = shFile.sourceHash;

clean:
	ListSBFile_free(&shaderBufferList, alloc);
	ListListU32_free(&arrays, alloc);			//Doesn't need freeUnderlying, it's all references
	Buffer_free(&shaderBuffersDlFile, alloc);
	DLFile_free(&shaderBuffers, alloc);
	Buffer_free(&stringsDlFile, alloc);
	DLFile_free(&strings, alloc);
	return s_uccess;
}