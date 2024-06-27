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

#include "types/list_impl.h"
#include "formats/oiSH.h"
#include "formats/oiDL.h"
#include "types/math.h"
#include "types/time.h"

#include <stddef.h>

TListImpl(SHEntry);
TListImpl(SHEntryRuntime);
TListImpl(SHBinaryInfo);
TListImpl(SHBinaryIdentifier);
TListImpl(SHInclude);

static const U8 SHHeader_V1_2 = 2;

const C8 *ESHBinaryType_names[ESHBinaryType_Count] = {
	"SPV",
	"DXIL"
};

const C8 *ESHExtension_defines[ESHExtension_Count] = {
	"F64",
	"I64",
	"16BITTYPES",
	"ATOMICI64",
	"ATOMICF32",
	"ATOMICF64",
	"SUBGROUPARITHMETIC",
	"SUBGROUPSHUFFLE",
	"RAYQUERY",
	"RAYMICROMAPOPACITY",
	"RAYMICROMAPDISPLACEMENT",
	"RAYMOTIONBLUR",
	"RAYREORDER",
	"MULTIVIEW",
	"COMPUTEDERIV",
	"PAQ"
};

const C8 *SHEntry_stageNames[] = {

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

const C8 *ESHType_names[] = {
	"", "", "", "",
	"F32",
	"F32x2",
	"F32x3",
	"F32x4",
	"I32",
	"I32x2",
	"I32x3",
	"I32x4",
	"U32",
	"U32x2",
	"U32x3",
	"U32x4"
};

const C8 *ESHType_name(ESHType type) {
	return ESHType_names[type & 0xF];
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

//Helper functions to create it

Bool SHFile_create(
	ESHSettingsFlags flags,
	U32 compilerVersion,
	U32 sourceHash,
	Allocator alloc,
	SHFile *shFile,
	Error *e_rr
) {

	Bool s_uccess = true;

	if(!shFile)
		retError(clean, Error_nullPointer(0, "SHFile_create()::shFile is required"))

	if(shFile->entries.ptr)
		retError(clean, Error_invalidOperation(0, "SHFile_create()::shFile isn't empty, might indicate memleak"))

	if(flags & ESHSettingsFlags_Invalid)
		retError(clean, Error_invalidParameter(0, 3, "SHFile_create()::flags contained unsupported flag"))

	gotoIfError2(clean, ListSHEntry_reserve(&shFile->entries, 16, alloc))
	gotoIfError2(clean, ListSHBinaryInfo_reserve(&shFile->binaries, 16, alloc))
	gotoIfError2(clean, ListSHInclude_reserve(&shFile->includes, 16, alloc))

	shFile->flags = flags;
	shFile->compilerVersion = compilerVersion;
	shFile->sourceHash = sourceHash;

clean:
	return s_uccess;
}

void SHFile_free(SHFile *shFile, Allocator alloc) {

	if(!shFile || !shFile->entries.ptr)
		return;

	for(U64 i = 0; i < shFile->entries.length; ++i) {
		SHEntry *entry = &shFile->entries.ptrNonConst[i];
		CharString_free(&entry->name, alloc);
		ListU16_free(&entry->binaryIds, alloc);
	}

	for(U64 j = 0; j < shFile->binaries.length; ++j) {

		SHBinaryInfo *binary = &shFile->binaries.ptrNonConst[j];

		CharString_free(&binary->identifier.entrypoint, alloc);
		ListCharString_freeUnderlying(&binary->identifier.uniforms, alloc);

		for(U64 i = 0; i < ESHBinaryType_Count; ++i)
			Buffer_free(&binary->binaries[i], alloc);
	}

	ListSHEntry_free(&shFile->entries, alloc);
	ListSHBinaryInfo_free(&shFile->binaries, alloc);
	ListSHInclude_freeUnderlying(&shFile->includes, alloc);

	*shFile = (SHFile) { 0 };
}

//Writing

Bool SHFile_addBinaries(SHFile *shFile, SHBinaryInfo *binaries, Allocator alloc, Error *e_rr) {

	Bool s_uccess = true;
	SHBinaryInfo info = (SHBinaryInfo) { 0 };

	//Validate everything

	if(!shFile || !binaries)
		retError(clean, Error_nullPointer(!shFile ? 0 : 2, "SHFile_addBinary()::shFile and *binaries are required"))

	Bool containsBinary = false;

	for(U8 i = 0; i < ESHBinaryType_Count; ++i)
		if(Buffer_length(binaries->binaries[i])) {
			containsBinary = true;
			break;
		}

	if(!containsBinary)
		retError(clean, Error_nullPointer(
			!shFile ? 0 : 2, "SHFile_addBinary() at least one of binaries->binaries[i] is required"
		))

	if(!binaries->vendorMask)
		retError(clean, Error_nullPointer(!shFile ? 0 : 2, "SHFile_addBinary()::binaries->vendorMask is required"))

	if(binaries->vendorMask == U16_MAX)
		binaries->vendorMask &= (1 << ESHVendor_Count) - 1;

	if(binaries->vendorMask >> ESHVendor_Count)
		retError(clean, Error_invalidParameter(2, 0, "SHFile_addBinary()::binaries->vendorMask out of bounds"))

	if(binaries->identifier.extensions >> ESHExtension_Count)
		retError(clean, Error_invalidParameter(2, 0, "SHFile_addBinary()::binaries->identifier.extensions out of bounds"))

	if(binaries->identifier.stageType >= ESHPipelineStage_Count)
		retError(clean, Error_invalidParameter(2, 0, "SHFile_addBinary()::binaries->identifier.stageType out of bounds"))

	if(binaries->identifier.uniforms.length & 1)
		retError(clean, Error_invalidParameter(
			2, 0, "SHFile_addBinary()::binaries->identifier.uniforms needs [uniformName,uniformValue][]"
		))

	if((binaries->identifier.uniforms.length >> 1) >= U8_MAX)
		retError(clean, Error_invalidParameter(
			2, 0, "SHFile_addBinary()::binaries->identifier.uniforms needs to be <=[uniformName,uniformValue][255]"
		))

	if(binaries->hasShaderAnnotation && CharString_length(binaries->identifier.entrypoint))
		retError(clean, Error_invalidParameter(
			2, 0, "SHFile_addBinary()::binaries->identifier.stageType or entrypoint is only available to [stage()] annotation"
		))

	if(!binaries->hasShaderAnnotation && !CharString_length(binaries->identifier.entrypoint))
		retError(clean, Error_invalidParameter(
			2, 0, "SHFile_addBinary()::binaries->identifier.entrypoint is required for [stage()] annotation"
		))

	if(
		binaries->identifier.shaderVersion < OISH_SHADER_MODEL(6, 5) ||
		binaries->identifier.shaderVersion > OISH_SHADER_MODEL(6, 8)
	)
		retError(clean, Error_invalidParameter(
			2, 0, "SHFile_addBinary()::binaries->identifier.shaderVersion is unsupported must be 6.5 -> 6.8"
		))

	if(Buffer_length(binaries->binaries[ESHBinaryType_SPIRV]) & 3)
		retError(clean, Error_invalidParameter(2, 0, "SHFile_addBinary()::binaries->binaries[SPIRV] needs to be a U32[]"))

	for(U64 i = 0; i < shFile->binaries.length; ++i)
		if(SHBinaryIdentifier_equals(shFile->binaries.ptr[i].identifier, binaries->identifier))
			retError(clean, Error_alreadyDefined(0, "SHFile_addBinary() binary identifier was already defined"))

	if(shFile->binaries.length + 1 >= U16_MAX)
		retError(clean, Error_outOfBounds(0, U16_MAX, U16_MAX, "SHFile_addBinary() requires binaries to not exceed U16_MAX"))

	//Validate unique uniforms

	Bool isUTF8 = false;

	if(
		CharString_length(binaries->identifier.entrypoint) &&
		!Buffer_isAscii(CharString_bufferConst(binaries->identifier.entrypoint))
	)
		isUTF8 = true;

	for (U64 i = 0; i < binaries->identifier.uniforms.length; ++i) {

		CharString str = binaries->identifier.uniforms.ptr[i];

		if(!CharString_length(str))
			continue;

		if(!Buffer_isAscii(CharString_bufferConst(str)))
			isUTF8 = true;

		if(i && !(i & 1))
			for(U64 j = 0; j < (i >> 1); ++j)
				if(CharString_equalsStringSensitive(str, binaries->identifier.uniforms.ptr[j << 1]))
					retError(clean, Error_alreadyDefined(0, "SHFile_addBinary() uniform already defined"))
	}

	//Start copying

	void *dstU64 = &info.identifier.extensions;
	const void *srcU64 = &binaries->identifier.extensions;

	*(U64*)dstU64 = *(const U64*) srcU64;

	//Copy buffers

	for(U8 i = 0; i < ESHBinaryType_Count; ++i) {

		Buffer *entry = &binaries->binaries[i];
		Buffer *result = &info.binaries[i];

		if(Buffer_isRef(*entry))
			gotoIfError2(clean, Buffer_createCopy(*entry, alloc, result))

		else *result = *entry;

		*entry = Buffer_createNull();
	}

	//Copy entrypoint

	if(CharString_isRef(binaries->identifier.entrypoint))
		gotoIfError2(clean, CharString_createCopy(binaries->identifier.entrypoint, alloc, &info.identifier.entrypoint))

	else info.identifier.entrypoint = binaries->identifier.entrypoint;

	binaries->identifier.entrypoint = CharString_createNull();

	//Copy uniforms

	if(binaries->identifier.uniforms.length) {

		if(ListCharString_isRef(binaries->identifier.uniforms))
			gotoIfError2(clean, ListCharString_createCopyUnderlying(
				binaries->identifier.uniforms, alloc, &info.identifier.uniforms
			))

		else {

			info.identifier.uniforms = binaries->identifier.uniforms;
			binaries->identifier.uniforms = (ListCharString) { 0 };

			for (U64 i = 0; i < info.identifier.uniforms.length; ++i) {

				CharString *uniform = &info.identifier.uniforms.ptrNonConst[i], uniformOld = *uniform;

				if(!CharString_isRef(uniformOld))		//It's already moved
					continue;

				*uniform = CharString_createNull();
				gotoIfError2(clean, CharString_createCopy(uniformOld, alloc, uniform))		//Allocate real data
			}
		}
	}

	//Finalize

	if(isUTF8)
		shFile->flags |= ESHSettingsFlags_IsUTF8;

	info.vendorMask = binaries->vendorMask;
	info.hasShaderAnnotation = binaries->hasShaderAnnotation;

	gotoIfError2(clean, ListSHBinaryInfo_pushBack(&shFile->binaries, info, alloc))
	*binaries = info = (SHBinaryInfo) { 0 };

clean:
	SHBinaryInfo_free(&info, alloc);
	return s_uccess;
}

typedef enum ESHPipelineType {
	ESHPipelineType_Compute,
	ESHPipelineType_Graphics,
	ESHPipelineType_Raytracing
} ESHPipelineType;

Bool SHFile_addEntrypoint(SHFile *shFile, SHEntry *entry, Allocator alloc, Error *e_rr) {

	Bool s_uccess = true;

	CharString copy = CharString_createNull();
	CharString oldName = copy;

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

	for(U8 i = 0; i < 3; ++i)
		if(((entry->waveSize >> (i << 2)) & 0xF) > 9)
			retError(clean, Error_outOfBounds(
				0, (entry->waveSize >> (i << 2)) & 0xF, 9, "SHFile_addEntrypoint() waveSize out of bounds"
			))
			
	if(entry->waveSize >> 12)
		retError(clean, Error_outOfBounds(
			0, entry->waveSize, 1 << 12, "SHFile_addEntrypoint() waveSize[3] should be empty"
		))

	Bool isRt = entry->stage >= ESHPipelineStage_RtStartExt && entry->stage <= ESHPipelineStage_RtEndExt;

	ESHPipelineType pipelineType =
		entry->stage == ESHPipelineStage_Compute ? ESHPipelineType_Compute : (isRt ? ESHPipelineType_Raytracing : (
			entry->stage == ESHPipelineStage_WorkgraphExt ? ESHPipelineType_Compute : ESHPipelineType_Graphics
		));

	U16 groupXYZ = entry->groupX | entry->groupY | entry->groupZ;
	U64 totalGroup = (U64)entry->groupX * entry->groupY * entry->groupZ;

	if(pipelineType != ESHPipelineType_Compute && groupXYZ)
		retError(clean, Error_invalidOperation(2, "SHFile_addEntrypoint() can't have group size for non compute"))

	if(pipelineType == ESHPipelineType_Compute && !totalGroup)
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
		entry->stage == ESHPipelineStage_IntersectionExt
	) {
		//TODO: When it works
		//if(!entry->payloadSize)
		//	retError(clean, Error_invalidOperation(
		//		2, "SHFile_addEntrypoint() payloadSize is required for hit/intersection shaders"
		//	))
	}

	else if(entry->payloadSize)
		retError(clean, Error_invalidOperation(
			2, "SHFile_addEntrypoint() payloadSize is only required for hit/intersection shaders"
		))

	if(!entry->intersectionSize) {

		if(entry->stage == ESHPipelineStage_IntersectionExt)
			retError(clean, Error_invalidOperation(
				2, "SHFile_addEntrypoint() intersectionSize is required for intersection shader"
			))
	}

	else if(entry->stage != ESHPipelineStage_IntersectionExt)
		retError(clean, Error_invalidOperation(
			2, "SHFile_addEntrypoint() intersectionSize is only required for intersection shader"
		))

	if (pipelineType != ESHPipelineType_Graphics && (entry->outputsU64 | entry->inputsU64))
		retError(clean, Error_invalidOperation(
			3, "SHFile_addEntrypoint() outputsU64 and inputsU64 are only for graphics shaders"
		))

	//Check validity of inputs and outputs

	if(entry->outputsU64 | entry->inputsU64)
		for (U64 i = 0; i < 16; ++i) {

			U8 vout = (entry->outputs[i >> 1] >> ((i & 1) << 2)) & 0xF;
			U8 vin = (entry->inputs[i >> 1] >> ((i & 1) << 2)) & 0xF;

			if((vout && vout < ESHType_F32) || (vin && vin < ESHType_F32))
				retError(clean, Error_invalidOperation(
					3, "SHFile_addEntrypoint() outputs or inputs contains an invalid parameter"
				))
		}

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

Bool SHFile_addInclude(SHFile *shFile, SHInclude *include, Allocator alloc, Error *e_rr) {

	Bool s_uccess = true;
	CharString tmp = (CharString) { 0 };

	//Validate everything

	if(!shFile || !include)
		retError(clean, Error_nullPointer(!shFile ? 0 : 1, "SHFile_addInclude()::shFile and include are required"))
	
	if(!CharString_length(include->relativePath) || !include->crc32c)
		retError(clean, Error_nullPointer(1, "SHFile_addInclude()::include->relativePath and crc32c are required"))

	//Avoid duplicates.
	//Though if the CRC32C mismatches, then we have a big problem.

	for(U64 i = 0; i < shFile->includes.length; ++i)
		if(CharString_equalsStringSensitive(include->relativePath, shFile->includes.ptr[i].relativePath)) {
			
			if(include->crc32c != shFile->includes.ptr[i].crc32c)
				retError(clean, Error_alreadyDefined(
					0, "SHFile_addInclude()::include was already defined, but with different CRC32C"
				))

			SHInclude_free(include, alloc);		//include is assumed to be moved, if it's not then it'd leak
			goto clean;
		}

	if((shFile->includes.length + 1) >> 16)
		retError(clean, Error_overflow(
			0, shFile->includes.length, 1 << 16, "SHFile_addInclude()::shFile->includes is limited to 16-bit"
		))

	//Ensure we insert it sorted, otherwise it's not reproducible

	U64 i = 0;

	for(; i < shFile->includes.length; ++i)
		if(CharString_compareSensitive(shFile->includes.ptr[i].relativePath, include->relativePath) == ECompareResult_Gt)
			break;

	//Create copy and/or move

	if(CharString_isRef(include->relativePath))
		gotoIfError2(clean, CharString_createCopy(include->relativePath, alloc, &tmp))

	SHInclude tmpInclude = (SHInclude) {
		.relativePath = CharString_isRef(include->relativePath) ? tmp : include->relativePath,
		.crc32c = include->crc32c
	};

	gotoIfError2(clean, ListSHInclude_insert(&shFile->includes, i, tmpInclude, alloc))
	*include = (SHInclude) { 0 };
	tmp = CharString_createNull();

clean:
	CharString_free(&tmp, alloc);
	return s_uccess;
}

typedef enum ESHBinaryFlags {

	ESHBinaryFlags_None 					= 0,

	//What type of binaries it includes
	//Must be one at least

	ESHBinaryFlags_HasSPIRV					= 1 << 0,
	ESHBinaryFlags_HasDXIL					= 1 << 1,

	//Reserved
	//ESHBinaryFlags_HasMSL					= 1 << 2,
	//ESHBinaryFlags_HasWGSL				= 1 << 3

	ESHBinaryFlags_HasShaderAnnotation		= 1 << 4,

	ESHBinaryFlags_HasBinary				= ESHBinaryFlags_HasSPIRV | ESHBinaryFlags_HasDXIL,
	//ESHBinaryFlags_HasText				= ESHBinaryFlags_HasMSL | ESHBinaryFlags_HasWGSL
	ESHBinaryFlags_HasSource				= ESHBinaryFlags_HasBinary // | ESHBinaryFlags_HasText

} ESHBinaryFlags;

typedef struct BinaryInfoFixedSize {

	U8 shaderModel;				//U4 major, minor
	U8 entrypointType;			//ESHPipelineStage: See entrypointType section in oiSH.md
	U16 entrypoint;				//U16_MAX if library, otherwise index into stageNames

	U16 vendorMask;				//Bitset of ESHVendor
	U8 uniformCount;
	U8 binaryFlags;				//ESHBinaryFlags

	ESHExtension extensions;

} BinaryInfoFixedSize;

typedef struct EntryInfoFixedSize {
	U8 pipelineStage;			//ESHPipelineStage
	U8 binaryCount;				//How many binaries this entrypoint references
} EntryInfoFixedSize;

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

	DLFile dlFile = (DLFile) { 0 };
	Buffer dlFileBuf = Buffer_createNull();
	gotoIfError2(clean, DLFile_create(settings, alloc, &dlFile))

	//Calculate easy sizes and add uniform names

	U64 binaryCount[ESHBinaryType_Count] = { 0 };
	U8 requiredTypes[ESHBinaryType_Count] = { 0 };

	for(U64 i = 0; i < shFile.binaries.length; ++i) {

		SHBinaryInfo binary = shFile.binaries.ptr[i];

		//Register uniform names

		for (U64 j = 0; j < binary.identifier.uniforms.length; j += 2) {

			//Only add if it's new

			CharString str = binary.identifier.uniforms.ptr[j];

			if(DLFile_find(dlFile, 0, U64_MAX, str) != U64_MAX)
				continue;

			if(isUTF8)
				gotoIfError2(clean, DLFile_addEntryUTF8(&dlFile, CharString_bufferConst(str), alloc))

			else gotoIfError2(clean, DLFile_addEntryAscii(&dlFile, CharString_createRefStrConst(str), alloc))

			if(dlFile.entryBuffers.length - shFile.entries.length >= (U16)(U16_MAX - 1))
				retError(clean, Error_invalidState(0, "DLFile didn't have space for uniform names"))
		}

		//Add size

		headerSize += binary.identifier.uniforms.length * sizeof(U16);

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
	U64 uniValStart = dlFile.entryBuffers.length;

	if(uniValStart >> 16)
		retError(clean, Error_invalidState(0, "SHFile_write() exceeded max uniform count"))

	for(U64 i = 0; i < shFile.binaries.length; ++i) {

		SHBinaryInfo binary = shFile.binaries.ptr[i];

		//Register uniform values

		for (U64 j = 1; j < binary.identifier.uniforms.length; j += 2) {

			//Only add if it's new

			CharString str = binary.identifier.uniforms.ptr[j];

			if(DLFile_find(dlFile, uniValStart, dlFile.entryBuffers.length, str) != U64_MAX)
				continue;

			if(dlFile.entryBuffers.length - uniValStart >= (U16)(U16_MAX - 1))
				retError(clean, Error_invalidState(0, "DLFile didn't have space for uniform values"))

			if(isUTF8)
				gotoIfError2(clean, DLFile_addEntryUTF8(&dlFile, CharString_bufferConst(str), alloc))

			else gotoIfError2(clean, DLFile_addEntryAscii(&dlFile, CharString_createRefStrConst(str), alloc))
		}
	}

	//Add includes

	for (U64 i = 0; i < shFile.includes.length; ++i) {

		CharString includeName = shFile.includes.ptr[i].relativePath;

		if(isUTF8)
			gotoIfError2(clean, DLFile_addEntryUTF8(&dlFile, CharString_bufferConst(includeName), alloc))

		else gotoIfError2(clean, DLFile_addEntryAscii(&dlFile, CharString_createRefStrConst(includeName), alloc))
	}

	//Add entries

	for (U64 i = 0; i < shFile.entries.length; ++i) {

		SHEntry entry = shFile.entries.ptr[i];
		CharString entryName = entry.name;

		if(isUTF8)
			gotoIfError2(clean, DLFile_addEntryUTF8(&dlFile, CharString_bufferConst(entryName), alloc))

		else gotoIfError2(clean, DLFile_addEntryAscii(&dlFile, CharString_createRefStrConst(entryName), alloc))

		switch(entry.stage) {

			default: {

				headerSize += sizeof(U8) * 2;		//U8 inputsAvail, outputsAvail;

				U8 inputs = 0, outputs = 0;

				//We align to 2 elements since we store U4s. So we store 4 bits too much, but who cares.

				for(; inputs < 8; ++inputs)
					if(!entry.inputs[inputs])
						break;

				for(; outputs < 8; ++outputs)
					if(!entry.outputs[outputs])
						break;

				headerSize += inputs + outputs;
				break;
			}

			case ESHPipelineStage_Compute:
			case ESHPipelineStage_WorkgraphExt:
				headerSize += sizeof(U16) * 4;			//group x, y, z, waveSize
				break;

			case ESHPipelineStage_RaygenExt:
			case ESHPipelineStage_CallableExt:
				break;

			case ESHPipelineStage_IntersectionExt:
				headerSize += sizeof(U8);				//intersectionSize
				//fallthrough

			case ESHPipelineStage_MissExt:
			case ESHPipelineStage_ClosestHitExt:
			case ESHPipelineStage_AnyHitExt:
				headerSize += sizeof(U8);				//payloadSize
				break;
		}

		headerSize += entry.binaryIds.length * sizeof(U16);
	}

	gotoIfError2(clean, DLFile_write(dlFile, alloc, &dlFileBuf))

	U64 len = 
		headerSize +
		Buffer_length(dlFileBuf) +
		shFile.entries.length * sizeof(EntryInfoFixedSize) +
		shFile.binaries.length * sizeof(BinaryInfoFixedSize) +
		shFile.includes.length * sizeof(U32);

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
		.version = SHHeader_V1_2,
		.sizeTypes = sizeTypes,
		.binaryCount = (U16) shFile.binaries.length,
		.stageCount = (U16) shFile.entries.length,
		.uniqueUniforms = (U16) uniValStart,
		.includeFileCount = (U16) shFile.includes.length
	};

	headerIt += sizeof(SHHeader);

	Buffer_copy(Buffer_createRef(headerIt, Buffer_length(dlFileBuf)), dlFileBuf);
	headerIt += Buffer_length(dlFileBuf);

	//Find offsets

	BinaryInfoFixedSize *binaryStaticStart = (BinaryInfoFixedSize*) headerIt;
	EntryInfoFixedSize *stagesStaticStart = (EntryInfoFixedSize*) (binaryStaticStart + shFile.binaries.length);
	U32 *crc32c = (U32*) (stagesStaticStart + shFile.entries.length);
	U8 *pipelineStagesStart = (U8*)(crc32c + shFile.includes.length);
	headerIt = pipelineStagesStart;	
	
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

		U64 entryStart = dlFile.entryBuffers.length - entries;
		U64 includeStart = entryStart - shFile.includes.length;
		U64 entrypoint = entryStart + U16_MAX;		//Indicate no entry

		if(!binary.hasShaderAnnotation) {

			entrypoint = DLFile_find(
				dlFile, entryStart, dlFile.entryBuffers.length, binary.identifier.entrypoint
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

			.extensions = binary.identifier.extensions
		};

		//Dynamic part

		U16 *uniformNames = (U16*) headerIt;
		headerIt += sizeof(U16) * uniformCount;

		U16 *uniValues = (U16*) headerIt;
		headerIt += sizeof(U16) * uniformCount;

		for(U64 j = 0; j < uniformCount; ++j) {
			ListCharString uniforms = binary.identifier.uniforms;
			uniformNames[j] = (U16) (DLFile_find(dlFile, uniNamesStart, uniValStart, uniforms.ptr[j << 1]) - uniNamesStart);
			uniValues[j] = (U16) (DLFile_find(dlFile, uniValStart, includeStart, uniforms.ptr[(j << 1) | 1]) - uniValStart);
		}

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

				//We align to 2 elements since we store U4s. So we store 4 bits too much, but who cares.

				for(; inputs < 8; ++inputs) {

					U8 input = entry.inputs[inputs];

					if(!input)
						break;

					*headerIt = input;
					++headerIt;
				}

				for(; outputs < 8; ++outputs) {

					U8 output = entry.outputs[inputs];

					if(!output)
						break;

					*headerIt = output;
					++headerIt;
				}

				//Find real inputs/outputs

				inputs <<= 1;
				outputs <<= 1;

				if(inputs < 0x10)
					inputs  |= entry.inputs [inputs  >> 1] >= 0x10;

				if(outputs < 0x10)
					outputs |= entry.outputs[outputs >> 1] >= 0x10;

				//Output packed

				begin[0] = inputs;
				begin[1] = outputs;
				break;
			}

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

			case ESHPipelineStage_IntersectionExt:
				*headerIt = entry.intersectionSize;
				headerIt += sizeof(U8);				//intersectionSize
				//fallthrough

			case ESHPipelineStage_MissExt:
			case ESHPipelineStage_ClosestHitExt:
			case ESHPipelineStage_AnyHitExt:
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
	Buffer_free(&dlFileBuf, alloc);
	DLFile_free(&dlFile, alloc);
	return s_uccess;
}

Bool SHFile_read(Buffer file, Bool isSubFile, Allocator alloc, SHFile *shFile, Error *e_rr) {

	Bool s_uccess = true;

	DLFile dlFile = (DLFile) { 0 };
	SHEntry entry = (SHEntry) { 0 };
	SHBinaryInfo binaryInfo = (SHBinaryInfo) { 0 };
	SHInclude include = (SHInclude) { 0 };

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

	if(header.version != SHHeader_V1_2)
		retError(clean, Error_invalidParameter(0, 1, "SHFile_read() header.version is invalid"))

	if(!header.stageCount)
		retError(clean, Error_invalidParameter(0, 1, "SHFile_read() header.stageCount is invalid"))

	//Validate hash
	
	Buffer_offset(&hash, offsetof(SHHeader, uniqueUniforms));

	if(Buffer_crc32c(hash) != header.hash)
		retError(clean, Error_unauthorized(0, "SHFile_read() mismatching CRC32C"))

	//Read DLFile

	gotoIfError2(clean, DLFile_read(file, NULL, true, alloc, &dlFile))
	gotoIfError2(clean, Buffer_offset(&file, dlFile.readLength))

	U64 minEntryBuffers =
		(U64)header.stageCount + 
		header.includeFileCount + 
		header.uniqueUniforms +				//Names have to be unique
		(header.uniqueUniforms ? 1 : 0);	//Values can be shared

	if(
		dlFile.entryBuffers.length < minEntryBuffers ||
		dlFile.settings.flags & EDLSettingsFlags_UseSHA256 ||
		dlFile.settings.dataType == EDLDataType_Data ||
		dlFile.settings.encryptionType ||
		dlFile.settings.compressionType
	)
		retError(clean, Error_invalidParameter(0, 1, "SHFile_read() dlFile didn't match expectations"))

	//Check if the buffer can at least contain the static sized data

	const BinaryInfoFixedSize *fixedBinaryInfo = (const BinaryInfoFixedSize*) file.ptr;
	const EntryInfoFixedSize *fixedEntryInfo = (const EntryInfoFixedSize*) (fixedBinaryInfo + header.binaryCount);
	const U32 *includeFileCrc32c = (const U32*) (fixedEntryInfo + header.stageCount);

	gotoIfError2(clean, Buffer_offset(&file, (const U8*)(includeFileCrc32c + header.includeFileCount) - file.ptr));

	//Create SHFile container

	ESHSettingsFlags flags = ESHSettingsFlags_HideMagicNumber;

	if(dlFile.settings.dataType == EDLDataType_UTF8)
		flags |= ESHSettingsFlags_IsUTF8;

	gotoIfError3(clean, SHFile_create(flags, header.compilerVersion, header.sourceHash, alloc, shFile, e_rr))

	//Parse binaries

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

			CharString name = DLFile_stringAt(
				dlFile, dlFile.entryBuffers.length - header.stageCount + binary.entrypoint, NULL
			);

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
		const U16 *uniformsEnd = uniformValues + binary.uniformCount;

		gotoIfError2(clean, Buffer_offset(&file, (const U8*) uniformsEnd - file.ptr))

		//Parse uniforms (names must be unique)

		ListCharString *uniformStrs = &binaryInfo.identifier.uniforms;
		gotoIfError2(clean, ListCharString_resize(uniformStrs, (U64) binary.uniformCount * 2, alloc))

		for(U64 i = 0; i < binary.uniformCount; ++i) {

			//Grab uniform and values, validate them too

			if(uniformNames[i] >= header.uniqueUniforms)
				retError(clean, Error_invalidState(1, "SHFile_read() uniformName out of bounds"))

			CharString uniformName = DLFile_stringAt(dlFile, (U64) uniformNames[i], NULL);

			//Since uniform values can be shared, we need to ensure we're not indexing out of bounds

			U64 uniformNameId = (U64) uniformValues[i] + header.uniqueUniforms;
			U64 endIndex = dlFile.entryBuffers.length - header.stageCount - header.includeFileCount;

			if(uniformNameId >= endIndex)
				retError(clean, Error_invalidState(1, "SHFile_read() uniformName out of bounds"))
				
			CharString uniformValue = DLFile_stringAt(dlFile, uniformNameId, NULL);

			//Check for duplicate uniform names

			for(U64 k = 0; k < i; ++k)
				if(CharString_equalsStringSensitive(uniformStrs->ptr[k << 1], uniformName))
					retError(clean, Error_alreadyDefined(1, "SHFile_read() uniformName already declared"))

			//Store

			gotoIfError2(clean, CharString_createCopy(uniformName, alloc, &uniformStrs->ptrNonConst[i << 1]));
			gotoIfError2(clean, CharString_createCopy(uniformValue, alloc, &uniformStrs->ptrNonConst[(i << 1) | 1]));
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

	for (U64 i = 0; i < header.stageCount; ++i) {

		if(fixedEntryInfo[i].pipelineStage >= ESHPipelineStage_Count)
			retError(clean, Error_invalidParameter(0, 0, "SHFile_read() stage[i].pipelineStage out of bounds"))

		entry = (SHEntry) {
			.stage = fixedEntryInfo[i].pipelineStage
		};

		U8 binaryCount = fixedEntryInfo[i].binaryCount;

		if(!binaryCount)
			retError(clean, Error_invalidParameter(0, 0, "SHFile_read() stage[i].binaryCount must be >0"))

		U64 start = dlFile.entryBuffers.length - header.stageCount;

		CharString str = DLFile_stringAt(dlFile, start + i, NULL);		//Already safety checked

		if(DLFile_find(dlFile, start, start + i, str) != U64_MAX)
			retError(clean, Error_alreadyDefined(0, "SHFile_read() dlFile included duplicate entrypoint name"))

		gotoIfError2(clean, CharString_createCopy(str, alloc, &entry.name))

		switch (entry.stage) {

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
			case ESHPipelineStage_ClosestHitExt:
			case ESHPipelineStage_AnyHitExt:

				if(nextMem + 1 > file.ptr + Buffer_length(file))
					retError(clean, Error_outOfBounds(
						0, nextMem - file.ptr, Buffer_length(file),
						"SHFile_read() couldn't parse raytracing stage, out of bounds"
					))

				entry.payloadSize = *nextMem;
				++nextMem;
				break;

			default: {

				//U8 availableValues: U4 inputsAvail, outputsAvail

				if(nextMem + 2 > file.ptr + Buffer_length(file))
					retError(clean, Error_outOfBounds(
						0, nextMem - file.ptr, Buffer_length(file),
						"SHFile_read() couldn't parse raytracing stage, out of bounds"
					))

				U8 inputs = nextMem[0];
				U8 outputs = nextMem[1];

				if(inputs > 0x10 || outputs > 0x10)
					retError(clean, Error_invalidParameter(
						0, 0, "SHFile_read() stage[i].inputs or outputs out of bounds"
					))

				nextMem += sizeof(U8) * 2;		//U8 inputsAvail, outputsAvail;

				//Unpack U8[inputs], U8[outputs]

				U8 inputBytes = ((inputs + 1) >> 1);
				U8 outputBytes = ((outputs + 1) >> 1);

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

				//Clear upper bits if misaligned

				if(inputs & 1)
					entry.inputs[inputs >> 1] &= 0xF;

				if(outputs & 1)
					entry.outputs[outputs >> 1] &= 0xF;

				break;
			}
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

		CharString relativePath = DLFile_stringAt(
			dlFile,
			dlFile.entryBuffers.length - header.stageCount - header.includeFileCount + i,
			NULL
		);

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

	if(!s_uccess)
		SHFile_free(shFile, alloc);

	SHInclude_free(&include, alloc);
	SHBinaryInfo_free(&binaryInfo, alloc);
	SHEntry_free(&entry, alloc);
	DLFile_free(&dlFile, alloc);
	return s_uccess;
}

static const C8 *vendors[] = {
	"NV",
	"AMD",
	"ARM",
	"QCOM",
	"INTC",
	"IMGT",
	"MSFT"
};

U32 SHEntryRuntime_getCombinations(SHEntryRuntime runtime) {
	return (U32)(
		U64_max(runtime.shaderVersions.length, 1) *
		U64_max(runtime.extensions.length, 1) *
		U64_max(runtime.uniformsPerCompilation.length, 1)
	);
}

Bool SHEntryRuntime_asBinaryIdentifier(
	SHEntryRuntime runtime, U16 combinationId, SHBinaryIdentifier *binaryIdentifier, Error *e_rr
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

	binaryInfo->vendorMask = runtime.vendorMask;
	binaryInfo->hasShaderAnnotation = runtime.isShaderAnnotation;
	binaryInfo->binaries[binaryType] = Buffer_createRefFromBuffer(buf, true);

clean:
	return s_uccess;
}

Bool SHBinaryIdentifier_equals(SHBinaryIdentifier a, SHBinaryIdentifier b) {

	const void *extensionsA = &a.extensions;
	const void *extensionsB = &b.extensions;

	if(
		*(const U64*)extensionsA != *(const U64*)extensionsB ||
		a.uniforms.length != b.uniforms.length ||
		!CharString_equalsStringSensitive(a.entrypoint, b.entrypoint)
	)
		return false;

	for(U64 i = 0; i < a.uniforms.length; ++i)
		if(!CharString_equalsStringSensitive(a.uniforms.ptr[i], b.uniforms.ptr[i]))
			return false;

	return true;
}

void SHEntry_print(SHEntry shEntry, Allocator alloc) {

	const C8 *name = SHEntry_stageName(shEntry);

	Log_debugLn(alloc, "Entry (%s): %.*s", name, (int) CharString_length(shEntry.name), shEntry.name.ptr);

	switch(shEntry.stage) {

		default:

			if (shEntry.inputsU64) {

				Log_debugLn(alloc, "\tInputs:");

				for (U8 i = 0; i < 16; ++i) {

					ESHType type = (ESHType)(shEntry.inputs[i >> 1] >> ((i & 1) << 2)) & 0xF;

					if(!type)
						continue;

					Log_debugLn(alloc, "\t\t%"PRIu8" %s", i, ESHType_name(type));
				}
			}

			if (shEntry.outputsU64) {

				Log_debugLn(alloc, "\tOutputs:");

				for (U8 i = 0; i < 16; ++i) {

					ESHType type = (ESHType)(shEntry.outputs[i >> 1] >> ((i & 1) << 2)) & 0xF;

					if(!type)
						continue;

					Log_debugLn(alloc, "\t\t%"PRIu8" %s", i, ESHType_name(type));
				}
			}

			break;

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

		case ESHPipelineStage_IntersectionExt:
			Log_debugLn(alloc, "\tIntersection size: %"PRIu8, shEntry.intersectionSize);
  			// fall through

		case ESHPipelineStage_MissExt:
		case ESHPipelineStage_ClosestHitExt:
		case ESHPipelineStage_AnyHitExt:
			Log_debugLn(alloc, "\tPayload size: %"PRIu8, shEntry.payloadSize);
			break;
	}
}

static const C8 *extensions[] = {
	"F64",
	"I64",
	"16BitTypes",
	"AtomicI64",
	"AtomicF32",
	"AtomicF64",
	"SubgroupArithmetic",
	"SubgroupShuffle",
	"RayQuery",
	"RayMicromapOpacity",
	"RayMicromapDisplacement",
	"RayMotionBlur",
	"RayReorder",
	"Multiview",
	"ComputeDeriv",
	"PAQ"
};

void SHEntryRuntime_print(SHEntryRuntime entry, Allocator alloc) {

	SHEntry_print(entry.entry, alloc);

	for(U64 i = 0; i < entry.shaderVersions.length; ++i) {
		U16 shaderVersion = entry.shaderVersions.ptr[i];
		Log_debugLn(alloc, "\t[model(%"PRIu8".%"PRIu8")]", (U8)(shaderVersion >> 8), (U8) shaderVersion);
	}

	for (U64 i = 0; i < entry.extensions.length; ++i) {

		ESHExtension exts = entry.extensions.ptr[i];

		if(!exts) {
			Log_debugLn(alloc, "\t[extension()]");
			continue;
		}

		Log_debug(alloc, ELogOptions_None, "\t[extension(");

		Bool prev = false;

		for(U64 j = 0; j < ESHExtension_Count; ++j)
			if((exts >> j) & 1) {
				Log_debug(alloc, ELogOptions_None, "%s\"%s\"", prev ? ", " : "", extensions[j]);
				prev = true;
			}

		Log_debug(alloc, ELogOptions_NewLine, ")]");
	}

	for (U64 i = 0, k = 0; i < entry.uniformsPerCompilation.length; ++i) {

		U8 uniforms = entry.uniformsPerCompilation.ptr[i];

		if(!uniforms) {
			Log_debugLn(alloc, "\t[uniforms()]");
			continue;
		}

		Log_debug(alloc, ELogOptions_None, "\t[uniforms(");

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

		Log_debug(alloc, ELogOptions_NewLine, ")]");

		k += uniforms;
	}
	
	if(entry.vendorMask == U16_MAX)
		Log_debugLn(alloc, "\t[vendor()] //(any vendor)");

	else for(U64 i = 0; i < ESHVendor_Count; ++i)
		if((entry.vendorMask >> i) & 1)
			Log_debugLn(alloc, "\t[vendor(\"%s\")]", vendors[i]);

	const C8 *name = SHEntry_stageName(entry.entry);
	Log_debugLn(alloc, entry.isShaderAnnotation ? "\t[shader(\"%s\")]" : "\t[stage(\"%s\")]", name);
}

void SHBinaryInfo_print(SHBinaryInfo binary, Allocator alloc) {

	if(binary.hasShaderAnnotation)
		Log_debugLn(alloc, "SH Binary (lib file)");

	else {

		CharString entrypoint = binary.identifier.entrypoint;
		const C8 *name = SHEntry_stageNames[binary.identifier.stageType];

		Log_debugLn(
			alloc, "SH Binary (%s): %.*s", name, (int) CharString_length(entrypoint), entrypoint.ptr
		);
	}

	U16 shaderVersion = binary.identifier.shaderVersion;
	Log_debugLn(alloc, "\t[model(%"PRIu8".%"PRIu8")]", (U8)(shaderVersion >> 8), (U8) shaderVersion);

	ESHExtension exts = binary.identifier.extensions;

	if(!exts)
		Log_debugLn(alloc, "\t[extension()]");

	else {

		Log_debug(alloc, ELogOptions_None, "\t[extension(");

		Bool prev = false;

		for(U64 j = 0; j < ESHExtension_Count; ++j)
			if((exts >> j) & 1) {
				Log_debug(alloc, ELogOptions_None, "%s\"%s\"", prev ? ", " : "", extensions[j]);
				prev = true;
			}

		Log_debug(alloc, ELogOptions_NewLine, ")]");
	}

	ListCharString uniforms = binary.identifier.uniforms;

	if(!(uniforms.length / 2))
		Log_debugLn(alloc, "\t[uniforms()]");

	else {

		Log_debug(alloc, ELogOptions_None, "\t[uniforms(");

		Bool prev = false;

		for(U64 j = 0; j < uniforms.length / 2; ++j) {

			CharString name = uniforms.ptr[j << 1];
			CharString value = uniforms.ptr[(j << 1) | 1];

			Log_debug(
				alloc, ELogOptions_None, 
				CharString_length(value) ? "%s\"%.*s\" = \"%.*s\"" : "%s\"%.*s\"",
				prev ? ", " : "",
				(int)CharString_length(name), name.ptr,
				(int)CharString_length(value), value.ptr
			);

			prev = true;
		}

		Log_debug(alloc, ELogOptions_NewLine, ")]");
	}

	U16 mask = (1 << ESHVendor_Count) - 1;
	
	if((binary.vendorMask & mask) == mask)
		Log_debugLn(alloc, "\t[vendor()] //(any vendor)");

	else for(U64 i = 0; i < ESHVendor_Count; ++i)
		if((binary.vendorMask >> i) & 1)
			Log_debugLn(alloc, "\t[vendor(\"%s\")]", vendors[i]);

	Log_debugLn(alloc, "\tBinaries:");

	for(U8 i = 0; i < ESHBinaryType_Count; ++i)
		if(Buffer_length(binary.binaries[i]))
			Log_debugLn(alloc, "\t\t%s: %"PRIu64, ESHBinaryType_names[i], Buffer_length(binary.binaries[i]));
}

//Free functions

void SHBinaryIdentifier_free(SHBinaryIdentifier *identifier, Allocator alloc) {

	if(!identifier)
		return;

	CharString_free(&identifier->entrypoint, alloc);
	ListCharString_freeUnderlying(&identifier->uniforms, alloc);
}

void SHBinaryInfo_free(SHBinaryInfo *info, Allocator alloc) {

	if(!info)
		return;

	SHBinaryIdentifier_free(&info->identifier, alloc);
	
	for(U8 i = 0; i < ESHBinaryType_Count; ++i)
		Buffer_free(&info->binaries[i], alloc);
}

void SHEntry_free(SHEntry *entry, Allocator alloc) {

	if(!entry)
		return;

	CharString_free(&entry->name, alloc);
}

void SHInclude_free(SHInclude *include, Allocator alloc) {

	if(!include)
		return;
		
	CharString_free(&include->relativePath, alloc);
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

void ListSHEntryRuntime_freeUnderlying(ListSHEntryRuntime *entry, Allocator alloc) {

	if(!entry)
		return;

	for(U64 i = 0; i < entry->length; ++i)
		SHEntryRuntime_free(&entry->ptrNonConst[i], alloc);

	ListSHEntryRuntime_free(entry, alloc);
}

void ListSHInclude_freeUnderlying(ListSHInclude *includes, Allocator alloc) {

	if(!includes)
		return;
		
	for(U64 i = 0; i < includes->length; ++i)
		SHInclude_free(&includes->ptrNonConst[i], alloc);

	ListSHInclude_free(includes, alloc);
}
