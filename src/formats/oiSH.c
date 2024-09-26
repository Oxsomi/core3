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
#include "formats/oiSB.h"
#include "formats/oiDL.h"
#include "types/math.h"
#include "types/time.h"

#include <stddef.h>

TListImpl(SHRegister);
TListImpl(SHRegisterRuntime);
TListImpl(SHEntry);
TListImpl(SHEntryRuntime);
TListImpl(SHBinaryInfo);
TListImpl(SHBinaryIdentifier);
TListImpl(SHInclude);
TListImpl(SHFile);

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
	"PAQ",
	"MESHTASKTEXDERIV",
	"WRITEMSTEXTURE",
	"BINDLESS",				//Unused, bindless is automatically turned on when it's detected
	"UNBOUNDARRAYSIZE"
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

	gotoIfError2(clean, ListSHEntry_reserve(&shFile->entries, 8, alloc))
	gotoIfError2(clean, ListSHBinaryInfo_reserve(&shFile->binaries, 4, alloc))
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
		ListCharString_freeUnderlying(&entry->semanticNames, alloc);
		ListU16_free(&entry->binaryIds, alloc);
	}

	for(U64 j = 0; j < shFile->binaries.length; ++j) {

		SHBinaryInfo *binary = &shFile->binaries.ptrNonConst[j];

		ListSHRegisterRuntime_freeUnderlying(&binary->registers, alloc);
		CharString_free(&binary->identifier.entrypoint, alloc);
		ListCharString_freeUnderlying(&binary->identifier.uniforms, alloc);

		for(U64 i = 0; i < ESHBinaryType_Count; ++i)
			Buffer_free(&binary->binaries[i], alloc);
	}

	ListSHEntry_freeUnderlying(&shFile->entries, alloc);
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
			2, 0,
			"SHFile_addBinary()::binaries->identifier.stageType or entrypoint is only available to [[oxc::stage()]] annotation"
		))

	if(!binaries->hasShaderAnnotation && !CharString_length(binaries->identifier.entrypoint))
		retError(clean, Error_invalidParameter(
			2, 0, "SHFile_addBinary()::binaries->identifier.entrypoint is required for [[oxc::stage()]] annotation"
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

	//Ensure bindless extension is correctly identified

	enum Counter {
		Counter_SamplerSPIRV,
		Counter_SamplerDXIL,
		Counter_CBV,
		Counter_UBO,
		Counter_UAV,
		Counter_SRV,
		Counter_RTASSPIRV,
		Counter_RTASDXIL,
		Counter_Image,
		Counter_Texture,
		Counter_SSBO,
		Counter_SubpassInput,
		Counter_Count
	};

	U64 counters[Counter_Count] = { 0 };
	U32 sets[4] = { 0 };
	U8 setCounters = 0;
	Bool unboundArraySize = false;

	for (U64 i = 0; i < binaries->registers.length; ++i) {

		SHRegisterRuntime reg = binaries->registers.ptr[i];

		if(reg.arrays.length == 1 && !reg.arrays.ptr[0])
			unboundArraySize = true;

		U64 regs = 1;

		for(U64 j = 0; j < reg.arrays.length; ++j)
			regs *= reg.arrays.ptr[j];

		//Keep track of current sets

		Bool hasSPIRV = reg.reg.bindings.arrU64[ESHBinaryType_SPIRV] != U64_MAX;
		Bool hasDXIL = reg.reg.bindings.arrU64[ESHBinaryType_DXIL] != U64_MAX;

		if (hasSPIRV) {

			U8 j = 0;

			for(; j < setCounters; ++j)
				if(sets[j] == reg.reg.bindings.arr[ESHBinaryType_SPIRV].space)
					break;

			//Insert new

			if (j == setCounters) {

				if(setCounters == 4)
					retError(clean, Error_invalidState(0, "SHFile_addBinary() registers contain more than 4 descriptor sets"))

				sets[setCounters++] = reg.reg.bindings.arr[ESHBinaryType_SPIRV].space;
			}
		}

		//Keep track of counts

		U8 regType = reg.reg.registerType;

		switch (regType & ESHRegisterType_TypeMask) {

			case ESHRegisterType_Sampler:
			case ESHRegisterType_SamplerComparisonState:
				if(hasSPIRV) counters[Counter_SamplerSPIRV] += regs;
				if(hasDXIL)  counters[Counter_SamplerDXIL]  += regs;
				break;

			case ESHRegisterType_SubpassInput:
				counters[Counter_SubpassInput] += regs;
				break;

			case ESHRegisterType_AccelerationStructure:

				if(hasSPIRV) counters[Counter_RTASSPIRV] += regs;

				if(hasDXIL) {
					counters[Counter_RTASDXIL] += regs;
					counters[Counter_SRV] += regs;
				}

				break;

			case ESHRegisterType_ConstantBuffer:
				if(hasSPIRV) counters[Counter_UBO] += regs;
				if(hasDXIL)  counters[Counter_CBV] += regs;
				break;
				
			case ESHRegisterType_ByteAddressBuffer:
			case ESHRegisterType_StructuredBuffer:
			case ESHRegisterType_StructuredBufferAtomic:
			case ESHRegisterType_StorageBuffer:
			case ESHRegisterType_StorageBufferAtomic:
				if(hasSPIRV) counters[Counter_SSBO] += regs;
				if(hasDXIL)  counters[regType & ESHRegisterType_IsWrite ? Counter_UAV : Counter_SRV] += regs;
				break;

				
			case ESHRegisterType_Texture1D:
			case ESHRegisterType_Texture2D:
			case ESHRegisterType_Texture3D:
			case ESHRegisterType_TextureCube:
			case ESHRegisterType_Texture2DMS:
				if(hasSPIRV) counters[regType & ESHRegisterType_IsWrite ? Counter_Image : Counter_Texture] += regs;
				if(hasDXIL)  counters[regType & ESHRegisterType_IsWrite ? Counter_UAV : Counter_SRV] += regs;
				break;
		}
	}

	U64 totalSPIRV = 
		counters[Counter_SamplerSPIRV] +
		counters[Counter_UBO] +
		counters[Counter_RTASSPIRV] +
		counters[Counter_Image] +
		counters[Counter_Texture] +
		counters[Counter_SSBO] +
		counters[Counter_SubpassInput];

	if(
		U64_max(counters[Counter_RTASSPIRV], counters[Counter_RTASDXIL]) > 16 ||
		counters[Counter_SubpassInput] > 8
	)
		retError(clean, Error_invalidState(0, "SHFile_addBinary() registers contain more than 8 SubpassInputs or 16 RTASes"))

	//Ensure we don't surpass the limits

	U64 countSampler = U64_max(counters[Counter_SamplerSPIRV], counters[Counter_SamplerDXIL]);
	U64 countCBV = U64_max(counters[Counter_CBV], counters[Counter_UBO]);

	Bool bindless = 
		countSampler > 16 ||
		countCBV > 12 ||
		counters[Counter_SSBO] > 8 ||
		counters[Counter_Texture] > 16 ||
		counters[Counter_Image] > 4 ||
		counters[Counter_SRV] > 128 ||
		counters[Counter_UAV] > 64 ||
		totalSPIRV > 44;

	if (bindless || unboundArraySize) {

		binaries->identifier.extensions |= ESHExtension_Bindless;

		if(unboundArraySize)
			binaries->identifier.extensions |= ESHExtension_UnboundArraySize;

		if (
			countSampler > 2048 ||
			countCBV > 12 ||
			counters[Counter_SSBO] > 500000 ||
			counters[Counter_Texture] > 250000 ||
			counters[Counter_Image] > 250000 ||
			counters[Counter_SRV] + counters[Counter_UAV] + counters[Counter_CBV] > 1000000 ||
			totalSPIRV > 1000000
		)
			retError(clean, Error_invalidState(0, "SHFile_addBinary() registers contain more resources than allowed by the oiSH spec"))
	}

	//Find binary

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

	//Copy registers
	
	if(binaries->registers.length) {

		if(ListSHRegisterRuntime_isRef(binaries->registers))
			gotoIfError3(clean, ListSHRegisterRuntime_createCopyUnderlying(
				binaries->registers, alloc, &info.registers, e_rr
			))

		else {
			info.registers = binaries->registers;
			binaries->registers = (ListSHRegisterRuntime) { 0 };
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

		if((presentMask[input >> 4] >> (input & 0xF)) & 1)
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

		if((presentMask[output >> 4] >> (16 + (output & 0xF))) & 1)
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

Bool SHFile_detectDuplicate(
	const ListSHRegisterRuntime *info,
	CharString name,
	SHBindings bindings,
	ESHRegisterType type,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool anyBinding = false;

	Bool anyDxilBinding = false;

	for(U8 i = 0; i < ESHBinaryType_Count; ++i)
		if(bindings.arr[i].space != U32_MAX || bindings.arr[i].binding != U32_MAX) {

			anyBinding = true;

			if(i == ESHBinaryType_DXIL)
				anyDxilBinding = true;
		}

	if(!anyBinding)
		retError(clean, Error_invalidState(0, "SHFile_detectDuplicate()::bindings contained no valid bindings"))

	if(!CharString_length(name))
		retError(clean, Error_invalidParameter(1, 0, "SHFile_detectDuplicate()::name is required"))

	//u, s, b, t registers in DXIL

	U8 registerBindingType = 0;

	switch (type & ESHRegisterType_TypeMask) {

		case ESHRegisterType_Sampler:
		case ESHRegisterType_SamplerComparisonState:
			registerBindingType = 2;
			break;

		case ESHRegisterType_ConstantBuffer:
			registerBindingType = 3;
			break;

		default:
			registerBindingType = type & ESHRegisterType_IsWrite ? 1 : 0;
			break;
	}

	for(U64 i = 0; i < info->length; ++i) {

		SHRegisterRuntime reg = info->ptr[i];

		if(CharString_equalsStringSensitive(reg.name, name))
			retError(clean, Error_invalidState(0, "SHFile_detectDuplicate()::name was already found in SHFile"))

		else if (anyDxilBinding) {

			SHBinding dstBinding = reg.reg.bindings.arr[ESHBinaryType_DXIL];
			SHBinding srcBinding = bindings.arr[ESHBinaryType_DXIL];

			U8 registerBindingTypei = 0;

			switch (reg.reg.registerType & ESHRegisterType_TypeMask) {

				case ESHRegisterType_Sampler:
				case ESHRegisterType_SamplerComparisonState:
					registerBindingTypei = 2;
					break;

				case ESHRegisterType_ConstantBuffer:
					registerBindingTypei = 3;
					break;

				default:
					registerBindingTypei = reg.reg.registerType & ESHRegisterType_IsWrite ? 1 : 0;
					break;
			}

			if(
				registerBindingType == registerBindingTypei &&
				dstBinding.binding == srcBinding.binding && dstBinding.space == srcBinding.space
			)
				retError(clean, Error_invalidState(
					0, "SHFile_detectDuplicate() DXIL space & binding combo was already found in SHFile"
				))
		}
	}

clean:
	return s_uccess;
}

Bool SHFile_validateRegister(
	const ListSHRegisterRuntime *info,
	CharString *name,
	ListU32 *arrays,
	SHBindings bindings,
	ESHRegisterType type,
	Error *e_rr
) {

	Bool s_uccess = true;

	if(!name || !CharString_length(*name))
		retError(clean, Error_nullPointer(0, "ListSHRegisterRuntime_addSampler()::name is required"))

	if(arrays && (!arrays->length || arrays->length > 32))
		retError(clean, Error_outOfBounds(1, arrays->length, 32, "ListSHRegisterRuntime_addSampler()::arrays.length should be [1, 32]"))

	gotoIfError3(clean, SHFile_detectDuplicate(info, *name, bindings, type, e_rr))

clean:
	return s_uccess;
}

Bool SHRegisterRuntime_hash(SHRegister registr, CharString name, ListU32 *arrays, SBFile *sbFile, U64 *res, Error *e_rr) {

	Bool s_uccess = true;

	if(CharString_length(name) > U32_MAX)
		retError(clean, Error_outOfBounds(
			0, CharString_length(name), U32_MAX, "SHRegisterRuntime_hash() name->length out of bounds"
		))

	if(arrays && arrays->length > U32_MAX)
		retError(clean, Error_outOfBounds(
			0, CharString_length(name), U32_MAX, "SHRegisterRuntime_hash() arrays->length out of bounds"
		))

	if(!res)
		retError(clean, Error_nullPointer(4, "SHRegisterRuntime_hash()::res is required"))

	//Compute hash to find register

	static_assert(sizeof(SHRegister) == sizeof(U64) * (ESHBinaryType_Count + 1), "Expected SHRegister as U64[N + 1]");

	U64 hash = sbFile ? sbFile->hash : Buffer_fnv1a64Offset;
	const U64 *regU64 = (const U64*) &registr;

	for(U64 i = 0; i < ESHBinaryType_Count + 1; ++i)
		hash = Buffer_fnv1a64Single(regU64[i], hash);

	hash = Buffer_fnv1a64Single(CharString_length(name) | ((arrays ? arrays->length : 0) << 32), hash);
	hash = Buffer_fnv1a64(CharString_bufferConst(name), hash);

	if (arrays) {

		const U64 *arraysU64 = (const U64*) &arrays->ptr;

		for(U64 i = 0; i < arrays->length >> 1; ++i)
			hash = Buffer_fnv1a64Single(arraysU64[i], hash);

		if(arrays->length & 1)
			hash = Buffer_fnv1a64Single(arrays->ptr[arrays->length - 1], hash);
	}

	*res = hash;

clean:
	return s_uccess;
}

Bool SHRegisterRuntime_createCopy(SHRegisterRuntime reg, Allocator alloc, SHRegisterRuntime *res, Error *e_rr) {

	Bool s_uccess = true;

	if(!res)
		retError(clean, Error_nullPointer(2, "SHRegisterRuntime_createCopy()::res is required"))

	if(res->name.ptr)
		retError(clean, Error_nullPointer(2, "SHRegisterRuntime_createCopy()::res already defined, could indicate memleak"))

	res->reg = reg.reg;
	gotoIfError2(clean, CharString_createCopy(reg.name, alloc, &res->name))
	gotoIfError2(clean, ListU32_createCopy(reg.arrays, alloc, &res->arrays))
	gotoIfError3(clean, SBFile_createCopy(reg.shaderBuffer, alloc, &res->shaderBuffer, e_rr))

clean:
	return s_uccess;
}

Bool SHBinaryInfo_addRegisterBase(
	ListSHRegisterRuntime *registers,
	CharString *name,
	ListU32 *arrays,
	SHBindings bindings,
	SHRegister registr,
	SBFile *sbFile,
	Allocator alloc,
	Error *e_rr
) {
	
	Bool s_uccess = true;
	SHRegisterRuntime reg = (SHRegisterRuntime) { 0 };

	if(!registers || !name)
		retError(clean, Error_nullPointer(
			!registers ? 0 : 1, "SHBinaryInfo_addRegisterBase()::registers and name are required"
		))

	U64 hash = 0;
	gotoIfError3(clean, SHRegisterRuntime_hash(registr, *name, arrays, sbFile, &hash, e_rr))

	//Find duplicate register (that is legal to add, though duplicates aren't)

	for(U64 i = 0; i < registers->length; ++i)
		if(registers->ptr[i].hash == hash)
			goto clean;

	//Ensure there's no registers with duplicate name or binding

	gotoIfError3(clean, SHFile_validateRegister(registers, name, arrays, bindings, registr.registerType, e_rr))

	reg.reg = registr;

	if(CharString_isRef(*name))
		gotoIfError2(clean, CharString_createCopy(*name, alloc, &reg.name))

	if(arrays && ListU32_isRef(*arrays))
		gotoIfError2(clean, ListU32_createCopy(*arrays, alloc, &reg.arrays))
		
	if(registers->length >= U16_MAX)
		retError(clean, Error_outOfBounds(
			0, registers->length, U16_MAX, "SHBinaryInfo_addRegisterBase() registers out of bounds"
		))

	SHRegisterRuntime tmp = reg;

	if(!CharString_isRef(*name))
		tmp.name = *name;

	if(arrays && !ListU32_isRef(*arrays))
		tmp.arrays = *arrays;

	if(sbFile)
		tmp.shaderBuffer = *sbFile;

	tmp.hash = hash;

	gotoIfError2(clean, ListSHRegisterRuntime_pushBack(registers, tmp, alloc))

	*name = CharString_createNull();

	if(arrays)
		*arrays = (ListU32) { 0 };

	if(sbFile)
		*sbFile = (SBFile) { 0 };

clean:
	if (!s_uccess)
		SHRegisterRuntime_free(&reg, alloc);

	return s_uccess;
}

SHBindings SHBindings_dummy() {

	SHBindings bindings = (SHBindings) { 0 };

	for(U8 i = 0; i < ESHBinaryType_Count; ++i)
		bindings.arr[i] = (SHBinding) { .binding = U32_MAX, .space = U32_MAX };

	return bindings;
}

Bool ListSHRegisterRuntime_createCopyUnderlying(
	ListSHRegisterRuntime orig,
	Allocator alloc,
	ListSHRegisterRuntime *dst,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool didAlloc = false;

	if(!dst)
		retError(clean, Error_nullPointer(1, "ListSHRegisterRuntime_createCopyUnderlying()::dst is required"))

	if(dst->ptr)
		retError(clean, Error_invalidParameter(
			1, 0, "ListSHRegisterRuntime_createCopyUnderlying()::dst is non zero, could indicate memleak"
		))

	gotoIfError2(clean, ListSHRegisterRuntime_createCopy(orig, alloc, dst))
	didAlloc = true;

	for(U64 i = 0; i < orig.length; ++i) {			//Ensure we don't accidentally free something we shouldn't
		dst->ptrNonConst[i].arrays = (ListU32) { 0 };
		dst->ptrNonConst[i].name = CharString_createNull();
		dst->ptrNonConst[i].shaderBuffer = (SBFile) { 0 };
	}

	for(U64 i = 0; i < orig.length; ++i) {			//Copy
		gotoIfError2(clean, ListU32_createCopy(orig.ptr[i].arrays, alloc, &dst->ptrNonConst[i].arrays))
		gotoIfError2(clean, CharString_createCopy(orig.ptr[i].name, alloc, &dst->ptrNonConst[i].name))
		gotoIfError3(clean, SBFile_createCopy(orig.ptr[i].shaderBuffer, alloc, &dst->ptrNonConst[i].shaderBuffer, e_rr))
	}

clean:

	if(didAlloc && !s_uccess)
		ListSHRegisterRuntime_freeUnderlying(dst, alloc);

	return s_uccess;
}

Bool ListSHRegisterRuntime_addSampler(
	ListSHRegisterRuntime *registers,
	U8 isUsedFlag,
	Bool isSamplerComparisonState,
	CharString *name,
	ListU32 *arrays,
	SHBindings bindings,
	Allocator alloc,
	Error *e_rr
) {
	return SHBinaryInfo_addRegisterBase(
		registers,
		name,
		arrays,
		bindings,
		(SHRegister) {
			.bindings = bindings,
			.registerType = (U8)(
				isSamplerComparisonState ? ESHRegisterType_SamplerComparisonState : ESHRegisterType_Sampler
			),
			.isUsedFlag = isUsedFlag
		},
		NULL,
		alloc,
		e_rr
	);
}

Bool ListSHRegisterRuntime_addBuffer(
	ListSHRegisterRuntime *registers,
	ESHBufferType registerType,
	Bool isWrite,
	U8 isUsedFlag,
	CharString *name,
	ListU32 *arrays,
	SBFile *sbFile,
	SHBindings bindings,
	Allocator alloc,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool isCBV = registerType == ESHBufferType_ConstantBuffer;

	if(registerType >= ESHBufferType_Count)
		retError(clean, Error_outOfBounds(
			1, registerType, ESHBufferType_Count, "ListSHRegisterRuntime_addBuffer()::registerType was invalid"
		))

	if(registerType == ESHBufferType_AccelerationStructure || registerType == ESHBufferType_ByteAddressBuffer) {
		if(sbFile)
			retError(clean, Error_invalidState(
				0, "ListSHRegisterRuntime_addBuffer()::sbFile should be NULL if the type is acceleration structure or BAB"
			))
	}

	else {

		if(!sbFile || !sbFile->bufferSize)
			retError(clean, Error_invalidState(
				0, "ListSHRegisterRuntime_addBuffer()::sbFile is required"
			))

		if(!(sbFile->flags & ESBSettingsFlags_IsTightlyPacked) != isCBV)
			retError(clean, Error_invalidState(
				0, "ListSHRegisterRuntime_addBuffer()::sbFile needs to be tightly packed for non CBV and loosely packed for CBV"
			))

		if(isCBV && sbFile->bufferSize >= 64 * KIBI)
			retError(clean, Error_invalidState(
				0, "ListSHRegisterRuntime_addBuffer()::sbFile is limited to 64KiB if it's a constant buffer"
			))
	}

	switch (registerType) {

		case ESHBufferType_StructuredBufferAtomic:
		case ESHBufferType_StorageBufferAtomic:

			if(!isWrite)
				retError(clean, Error_invalidState(
					0, "ListSHRegisterRuntime_addBuffer()::registerType needs write flag to always be enabled"
				))

			break;

		case ESHBufferType_ConstantBuffer:
		case ESHBufferType_AccelerationStructure:

			if(isWrite)
				retError(clean, Error_invalidState(
					0, "ListSHRegisterRuntime_addBuffer()::registerType was incompatible with write flag"
				))

			break;

		default:
			break;
	}

	gotoIfError3(clean, SHBinaryInfo_addRegisterBase(
		registers,
		name,
		arrays,
		bindings,
		(SHRegister) {
			.bindings = bindings,
			.registerType = (U8)(
				(ESHRegisterType_BufferStart + registerType) | 
				(isWrite ? ESHRegisterType_IsWrite : 0)
			),
			.isUsedFlag = isUsedFlag
		},
		sbFile,
		alloc,
		e_rr
	))

clean:
	return s_uccess;
}

Bool ListSHRegisterRuntime_addTextureBase(
	ListSHRegisterRuntime *registers,
	ESHTextureType registerType,
	Bool isLayeredTexture,
	Bool isCombinedSampler,
	Bool isWrite,
	U8 isUsedFlag,
	ESHTexturePrimitive textureFormatPrimitive,
	ETextureFormatId textureFormatId,
	CharString *name,
	ListU32 *arrays,
	SHBindings bindings,
	Allocator alloc,
	Error *e_rr
) {
	
	Bool s_uccess = true;

	if(registerType >= ESHTextureType_Count)
		retError(clean, Error_outOfBounds(
			1, registerType, ESHTextureType_Count, "ListSHRegisterRuntime_addRWTexture()::registerType was invalid"
		))

	if(textureFormatId >= ETextureFormatId_Count)
		retError(clean, Error_outOfBounds(
			5, textureFormatId, ETextureFormatId_Count, "ListSHRegisterRuntime_addRWTexture()::textureFormatId out of bounds"
		))

	if(
		(textureFormatPrimitive & ESHTexturePrimitive_TypeMask) > ESHTexturePrimitive_Count ||
		(textureFormatPrimitive & ESHTexturePrimitive_Unused)
	)
		retError(clean, Error_outOfBounds(
			5, textureFormatPrimitive, ESHTexturePrimitive_Count,
			"ListSHRegisterRuntime_addRWTexture()::textureFormatPrimitive out of bounds"
		))

	if(textureFormatPrimitive == ESHTexturePrimitive_Count && !textureFormatId && isWrite)
		retError(clean, Error_invalidState(
			0, "ListSHRegisterRuntime_addRWTexture() either texture format primitive or texture format id has to be set for RW"
		))

	ETextureFormat format = ETextureFormatId_unpack[textureFormatId];
	ESHTexturePrimitive primitive = ESHTexturePrimitive_Count;

	if(textureFormatId) {

		ETexturePrimitive texPrim = ETextureFormat_getPrimitive(format);
		U8 channels = ETextureFormat_getChannels(format);

		switch (texPrim) {

			case ETexturePrimitive_UInt:	primitive = ESHTexturePrimitive_UInt;		break;
			case ETexturePrimitive_SInt:	primitive = ESHTexturePrimitive_SInt;		break;
			case ETexturePrimitive_UNorm:	primitive = ESHTexturePrimitive_UNorm;		break;
			case ETexturePrimitive_SNorm:	primitive = ESHTexturePrimitive_SNorm;		break;

			case ETexturePrimitive_Float:
				primitive = ESHTexturePrimitive_Float;
				//TODO: 64 bit texture formats
				//primitive = ETextureFormat_getRedBits(format) == 64 ? ESHTexturePrimitive_Double : ESHTexturePrimitive_Float;
				break;

			default:
				retError(clean, Error_invalidState(
					0, "ListSHRegisterRuntime_addRWTexture() texture format is incompatible"
				))
		}

		switch (channels) {
			case 1:		primitive |= ESHTexturePrimitive_Component1;	break;
			case 2:		primitive |= ESHTexturePrimitive_Component2;	break;
			case 3:		primitive |= ESHTexturePrimitive_Component3;	break;
			case 4:		primitive |= ESHTexturePrimitive_Component4;	break;
			default:
				retError(clean, Error_invalidState(
					0, "ListSHRegisterRuntime_addRWTexture() texture format is incompatible"
				))
		}

		if(
			textureFormatPrimitive != primitive && 
			(textureFormatPrimitive & ESHTexturePrimitive_TypeMask) != ESHTexturePrimitive_Count
		)
			retError(clean, Error_invalidState(
				0, "ListSHRegisterRuntime_addRWTexture() texture primitive is incompatible"
			))
	}

	else primitive = textureFormatPrimitive;

	gotoIfError3(clean, SHBinaryInfo_addRegisterBase(
		registers,
		name,
		arrays,
		bindings,
		(SHRegister) {
			.bindings = bindings,
			.registerType = (U8)(
				(ESHRegisterType_TextureStart + registerType) |
				(isWrite ? ESHRegisterType_IsWrite : 0) |
				(isCombinedSampler ? ESHRegisterType_IsCombinedSampler : 0) |
				(isLayeredTexture ? ESHRegisterType_IsArray : 0)
			),
			.texture = (SHTextureFormat) {
				.formatId = textureFormatId,
				.primitive = textureFormatPrimitive
			},
			.isUsedFlag = isUsedFlag
		},
		NULL,
		alloc,
		e_rr
	))

clean:
	return s_uccess;
}

Bool ListSHRegisterRuntime_addTexture(
	ListSHRegisterRuntime *registers,
	ESHTextureType registerType,
	Bool isLayeredTexture,
	Bool isCombinedSampler,
	U8 isUsedFlag,
	ESHTexturePrimitive textureFormatPrimitive,
	CharString *name,
	ListU32 *arrays,
	SHBindings bindings,
	Allocator alloc,
	Error *e_rr
) {

	Bool s_uccess = true;

	gotoIfError3(clean, ListSHRegisterRuntime_addTextureBase(
		registers,
		registerType,
		isLayeredTexture,
		isCombinedSampler,
		false,
		isUsedFlag,
		textureFormatPrimitive,
		ETextureFormatId_Undefined,
		name,
		arrays,
		bindings,
		alloc,
		e_rr
	))

clean:
	return s_uccess;
}

Bool ListSHRegisterRuntime_addRWTexture(
	ListSHRegisterRuntime *registers,
	ESHTextureType registerType,
	Bool isLayeredTexture,
	U8 isUsedFlag,
	ESHTexturePrimitive textureFormatPrimitive,
	ETextureFormatId textureFormatId,
	CharString *name,
	ListU32 *arrays,
	SHBindings bindings,
	Allocator alloc,
	Error *e_rr
) {
	return ListSHRegisterRuntime_addTextureBase(
		registers,
		registerType,
		isLayeredTexture,
		false,
		true,
		isUsedFlag,
		textureFormatPrimitive,
		textureFormatId,
		name,
		arrays,
		bindings,
		alloc,
		e_rr
	);
}

Bool ListSHRegisterRuntime_addSubpassInput(
	ListSHRegisterRuntime *registers,
	U8 isUsedFlag,
	CharString *name,
	SHBindings bindings,
	U16 inputAttachmentId,
	Allocator alloc,
	Error *e_rr
) {
	Bool s_uccess = true;

	if(inputAttachmentId >= 7)
		retError(clean, Error_outOfBounds(
			4, inputAttachmentId, 7, "ListSHRegisterRuntime_addSubpassInput()::inputAttachmentId out of bounds"
		))

	for(U8 i = 0; i < ESHBinaryType_Count; ++i)
		if(i != ESHBinaryType_SPIRV && (bindings.arr[i].space != U32_MAX || bindings.arr[i].binding != U32_MAX))
			retError(clean, Error_invalidState(
				0, "ListSHRegisterRuntime_addSubpassInput() can only have bindings for SPIRV"
			))

	gotoIfError3(clean, SHBinaryInfo_addRegisterBase(
		registers,
		name,
		NULL,
		bindings,
		(SHRegister) {
			.bindings = bindings,
			.inputAttachmentId = inputAttachmentId,
			.registerType = (U8)ESHRegisterType_SubpassInput,
			.isUsedFlag = isUsedFlag
		},
		NULL,
		alloc,
		e_rr
	))

clean:
	return s_uccess;
}

Bool ListSHRegisterRuntime_addRegister(
	ListSHRegisterRuntime *registers,
	CharString *name,
	ListU32 *arrays,
	SHRegister reg,
	SBFile *sbFile,
	Allocator alloc,
	Error *e_rr
) {

	Bool s_uccess = true;

	U32 baseRegType = reg.registerType & ESHRegisterType_TypeMask;

	switch (baseRegType) {

		case ESHRegisterType_ConstantBuffer:
		case ESHRegisterType_ByteAddressBuffer:
		case ESHRegisterType_StructuredBuffer:
		case ESHRegisterType_StructuredBufferAtomic:
		case ESHRegisterType_StorageBuffer:
		case ESHRegisterType_StorageBufferAtomic:
		case ESHRegisterType_AccelerationStructure:
			
			if(reg.registerType & (ESHRegisterType_Masks &~ ESHRegisterType_IsWrite))
				retError(clean, Error_invalidParameter(
					2, 4,
					"ListSHRegisterRuntime_addRegister()::registerType buffer needs to be R/W only (not array or combined sampler)"
				))
				
			gotoIfError3(clean, ListSHRegisterRuntime_addBuffer(
				registers,
				(ESHBufferType)(baseRegType - ESHRegisterType_BufferStart),
				reg.registerType & ESHRegisterType_IsWrite,
				reg.isUsedFlag,
				name,
				arrays,
				sbFile,
				reg.bindings,
				alloc,
				e_rr
			))

			break;

		case ESHRegisterType_SamplerComparisonState:
		case ESHRegisterType_Sampler: {

			U32 regType = reg.registerType;
			Bool isComparisonState = regType == ESHRegisterType_SamplerComparisonState;
			
			if(regType != ESHRegisterType_Sampler && isComparisonState)
				retError(clean, Error_invalidParameter(2, 4, "ListSHRegisterRuntime_addRegister()::registerType is invalid"))

			if(reg.padding)
				retError(clean, Error_invalidParameter(
					2, 5, "ListSHRegisterRuntime_addRegister()::padding is invalid (non zero)"
				))

			if(sbFile)
				retError(clean, Error_invalidParameter(2, 7, "ListSHRegisterRuntime_addRegister()::sbFile on subpassInput not allowed"))
				
			gotoIfError3(clean, ListSHRegisterRuntime_addSampler(
				registers,
				reg.isUsedFlag,
				isComparisonState,
				name,
				arrays,
				reg.bindings,
				alloc,
				e_rr
			))

			break;
		}

		case ESHRegisterType_SubpassInput:
			
			if(reg.registerType != ESHRegisterType_SubpassInput)
				retError(clean, Error_invalidParameter(2, 0, "ListSHRegisterRuntime_addRegister()::registerType is invalid"))

			if(arrays)
				retError(clean, Error_invalidParameter(
					2, 2, "ListSHRegisterRuntime_addRegister()::arrays on subpassInput not allowed"
				))

			if(sbFile)
				retError(clean, Error_invalidParameter(
					2, 3, "ListSHRegisterRuntime_addRegister()::sbFile on subpassInput not allowed"
				))

			gotoIfError3(clean, ListSHRegisterRuntime_addSubpassInput(
				registers,
				reg.isUsedFlag,
				name,
				reg.bindings,
				reg.inputAttachmentId,
				alloc,
				e_rr
			))

			break;

		default:

			if(baseRegType < ESHRegisterType_TextureStart || baseRegType >= ESHRegisterType_TextureEnd)
				retError(clean, Error_invalidParameter(2, 0, "ListSHRegisterRuntime_addRegister()::registerType is invalid"))

			if(sbFile)
				retError(clean, Error_invalidParameter(2, 3, "ListSHRegisterRuntime_addRegister()::sbFile on subpassInput not allowed"))

			if (reg.registerType & ESHRegisterType_IsWrite) {

				if(reg.registerType & ESHRegisterType_IsCombinedSampler)
					retError(clean, Error_invalidParameter(
						2, 0, "ListSHRegisterRuntime_addRegister() RWTexture can't contain combined sampler"
					))

				gotoIfError3(clean, ListSHRegisterRuntime_addRWTexture(
					registers,
					(ESHTextureType)(baseRegType - ESHRegisterType_TextureStart),
					reg.registerType & ESHRegisterType_IsArray,
					reg.isUsedFlag,
					(ESHTexturePrimitive) reg.texture.primitive,
					(ETextureFormatId) reg.texture.formatId,
					name,
					arrays,
					reg.bindings,
					alloc,
					e_rr
				))
			}

			else {

				if(reg.texture.formatId)
					retError(clean, Error_invalidParameter(
						3, 5, "ListSHRegisterRuntime_addRegister()::texture.formatId isn't allowed only readonly texture"
					))

				gotoIfError3(clean, ListSHRegisterRuntime_addTexture(
					registers,
					(ESHTextureType)(baseRegType - ESHRegisterType_TextureStart),
					reg.registerType & ESHRegisterType_IsArray,
					reg.registerType & ESHRegisterType_IsCombinedSampler,
					reg.isUsedFlag,
					(ESHTexturePrimitive) reg.texture.primitive,
					name,
					arrays,
					reg.bindings,
					alloc,
					e_rr
				))
			}

			break;
	}

clean:
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

	U16 registerCount;
	U16 padding;

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

				if (entry.semanticNames.length)
					headerSize += 1 + inputs + outputs;

				if(entry.stage != ESHPipelineStage_MeshExt && entry.stage != ESHPipelineStage_TaskExt)
					break;

				else {
					// fallthrough
				}
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
		.version = SHHeader_V1_2,
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

				else {
					// fallthrough
				}
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

	if(header.version != SHHeader_V1_2)
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
						"SHFile_read() couldn't parse raytracing stage, out of bounds"
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

				else {
					// fallthrough
				}
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

			else {
				// fallthrough
			}
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

const C8 *ESHExtension_names[] = {
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
	"PAQ",
	"MeshTaskTexDeriv",
	"WriteMSTexture",
	"Bindless",
	"UnboundArraySize"
};

void SHEntryRuntime_print(SHEntryRuntime entry, Allocator alloc) {

	SHEntry_print(entry.entry, alloc);

	for(U64 i = 0; i < entry.shaderVersions.length; ++i) {
		U16 shaderVersion = entry.shaderVersions.ptr[i];
		Log_debugLn(alloc, "\t[[oxc::model(%"PRIu8".%"PRIu8")]]", (U8)(shaderVersion >> 8), (U8) shaderVersion);
	}

	for (U64 i = 0; i < entry.extensions.length; ++i) {

		ESHExtension exts = entry.extensions.ptr[i];

		if(!exts) {
			Log_debugLn(alloc, "\t[[oxc::extension()]]");
			continue;
		}

		Log_debug(alloc, ELogOptions_None, "\t[[oxc::extension(");

		Bool prev = false;

		for(U64 j = 0; j < ESHExtension_Count; ++j)
			if((exts >> j) & 1) {
				Log_debug(alloc, ELogOptions_None, "%s\"%s\"", prev ? ", " : "", ESHExtension_names[j]);
				prev = true;
			}

		Log_debug(alloc, ELogOptions_NewLine, ")]]");
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
			Log_debugLn(alloc, "\t[[oxc::vendor(\"%s\")]]", vendors[i]);

	const C8 *name = SHEntry_stageName(entry.entry);
	Log_debugLn(alloc, entry.isShaderAnnotation ? "\t[shader(\"%s\")]" : "\t[[oxc::stage(\"%s\")]]", name);
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
	Log_debugLn(alloc, "\t[[oxc::model(%"PRIu8".%"PRIu8")]]", (U8)(shaderVersion >> 8), (U8) shaderVersion);

	ESHExtension exts = binary.identifier.extensions;

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

	ListCharString uniforms = binary.identifier.uniforms;

	if(!(uniforms.length / 2))
		Log_debugLn(alloc, "\t[[oxc::uniforms()]");

	else {

		Log_debug(alloc, ELogOptions_None, "\t[[oxc::uniforms(");

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

		Log_debug(alloc, ELogOptions_NewLine, ")]]");
	}

	U16 mask = (1 << ESHVendor_Count) - 1;
	
	if((binary.vendorMask & mask) == mask)
		Log_debugLn(alloc, "\t[[oxc::vendor()]] //(any vendor)");

	else for(U64 i = 0; i < ESHVendor_Count; ++i)
		if((binary.vendorMask >> i) & 1)
			Log_debugLn(alloc, "\t[[oxc::vendor(\"%s\")]]", vendors[i]);

	Log_debugLn(alloc, "\tBinaries:");

	for(U8 i = 0; i < ESHBinaryType_Count; ++i)
		if(Buffer_length(binary.binaries[i]))
			Log_debugLn(alloc, "\t\t%s: %"PRIu64, ESHBinaryType_names[i], Buffer_length(binary.binaries[i]));

	ListSHRegisterRuntime_print(binary.registers, 1, alloc);
}

const C8 *ESHTexturePrimitive_name[ESHTexturePrimitive_CountAll] = {
	"uint",  "int",  "unorm float",  "snorm float",  "float",  "double",  "", "", "", "", "", "", "", "", "", "",
	"uint2", "int2", "unorm float2", "snorm float2", "float2", "double2", "", "", "", "", "", "", "", "", "", "",
	"uint3", "int3", "unorm float3", "snorm float3", "float3", "double3", "", "", "", "", "", "", "", "", "", "",
	"uint4", "int4", "unorm float4", "snorm float4", "float4", "double4", "", "", "", "", "", "", "", "", "", ""
};

void SHRegister_printBindings(ESHRegisterType type, SHBindings bindings, Allocator alloc, const C8 *prefix, const C8 *indent) {

	SHBinding spirvBinding = bindings.arr[ESHBinaryType_SPIRV];

	if(spirvBinding.space != U32_MAX || spirvBinding.binding != U32_MAX)
		Log_debugLn(
			alloc,
			"%s%s[[vk::binding(%"PRIu32", %"PRIu32")]]",
			indent,
			prefix,
			spirvBinding.binding,
			spirvBinding.space
		);

	SHBinding dxilBinding = bindings.arr[ESHBinaryType_DXIL];

	if(dxilBinding.space != U32_MAX || dxilBinding.binding != U32_MAX) {

		C8 letter = type == ESHRegisterType_ConstantBuffer ? 'b' : (
			type == ESHRegisterType_Sampler || type == ESHRegisterType_SamplerComparisonState ? 's' : (
				type & ESHRegisterType_IsWrite ? 'u' : 't'
			)
		);

		Log_debugLn(
			alloc,
			"%s: register(%c%"PRIu32", space%"PRIu32")",
			indent,
			letter,
			dxilBinding.binding,
			dxilBinding.space
		);
	}
}

void SHRegister_print(SHRegister reg, U64 indenting, Allocator alloc) {

	if(indenting >= SHORTSTRING_LEN) {
		Log_debugLn(alloc, "SHRegister_print() short string out of bounds");
		return;
	}

	ShortString indent;
	for(U8 i = 0; i < indenting; ++i) indent[i] = '\t';
	indent[indenting] = '\0';

	switch (reg.registerType & ESHRegisterType_TypeMask) {

		case ESHRegisterType_SubpassInput:
			Log_debugLn(alloc, "%sinput_attachment_index = %"PRIu8, indent, reg.inputAttachmentId);
			break;

		case ESHRegisterType_Sampler:					Log_debugLn(alloc, "%sSamplerState", indent);					 break;
		case ESHRegisterType_SamplerComparisonState:	Log_debugLn(alloc, "%sSamplerComparisonState", indent);			 break;
		case ESHRegisterType_ConstantBuffer:			Log_debugLn(alloc, "%sConstantBuffer", indent);					 break;
		case ESHRegisterType_AccelerationStructure:		Log_debugLn(alloc, "%sRaytracingAccelerationStructure", indent); break;

		case ESHRegisterType_ByteAddressBuffer:
			Log_debugLn(alloc, "%s%sByteAddressBuffer", indent, reg.registerType & ESHRegisterType_IsWrite ? "RW" : "");
			break;

		case ESHRegisterType_StructuredBuffer:
			Log_debugLn(alloc, "%s%sStructuredBuffer", indent, reg.registerType & ESHRegisterType_IsWrite ? "RW" : "");
			break;

		case ESHRegisterType_StorageBuffer:
			Log_debugLn(alloc, "%s%sStorageBuffer", indent, reg.registerType & ESHRegisterType_IsWrite ? "RW" : "");
			break;

		case ESHRegisterType_StorageBufferAtomic:
			Log_debugLn(alloc, "%s%sStorageBufferAtomic", indent, reg.registerType & ESHRegisterType_IsWrite ? "RW" : "");
			break;

		case ESHRegisterType_StructuredBufferAtomic:
			Log_debugLn(alloc, "%sAppend/ConsumeBuffer", indent);
			break;

		default: {

			const C8 *dim = "2D";

			switch(reg.registerType & ESHRegisterType_TypeMask) {

				case ESHRegisterType_Texture2D:						break;
				case ESHRegisterType_Texture1D:		dim = "1D";		break;
				case ESHRegisterType_Texture3D:		dim = "3D";		break;
				case ESHRegisterType_TextureCube:	dim = "Cube";	break;
				case ESHRegisterType_Texture2DMS:	dim = "2DMS";	break;
			}

			Log_debugLn(
				alloc, "%s%s%s%s%s",
				indent,
				reg.registerType & ESHRegisterType_IsWrite ? "RW" : "",
				reg.registerType & ESHRegisterType_IsCombinedSampler ? "sampler" : "Texture",
				dim,
				reg.registerType & ESHRegisterType_IsArray ? "Array" : ""
			);

			if(reg.texture.formatId)
				Log_debugLn(alloc, "%s%s", indent, ETextureFormatId_name[reg.texture.formatId]);

			if(reg.texture.primitive != ESHTexturePrimitive_Count)
				Log_debugLn(alloc, "%s%s", indent, ESHTexturePrimitive_name[reg.texture.primitive]);

			break;
		}
	}

	SHRegister_printBindings(reg.registerType, reg.bindings, alloc, "", indent);
}

void SHRegisterRuntime_print(SHRegisterRuntime reg, U64 indenting, Allocator alloc) {

	if(indenting >= SHORTSTRING_LEN) {
		Log_debugLn(alloc, "SHRegisterRuntime_print() short string out of bounds");
		return;
	}

	ShortString indent;
	for(U8 i = 0; i < indenting; ++i) indent[i] = '\t';
	indent[indenting] = '\0';

	Log_debug(
		alloc,
		ELogOptions_None,
		"%s%.*s",
		indent,
		(int)CharString_length(reg.name), reg.name.ptr
	);

	for(U64 i = 0; i < reg.arrays.length; ++i)
		Log_debug(
			alloc,
			ELogOptions_None,
			"[%"PRIu32"]",
			reg.arrays.ptr[i]
		);

	for(U8 i = 0; i < ESHBinaryType_Count; ++i) {

		if(reg.reg.bindings.arr[i].space == U32_MAX && reg.reg.bindings.arr[i].binding == U32_MAX)
			continue;

		Log_debug(
			alloc, ELogOptions_None,
			(reg.reg.isUsedFlag >> i) & 1 ? " (%s: Used)" : " (%s: Unused)",
			ESHBinaryType_names[i]
		);
	}

	Log_debugLn(alloc, "");

	SHRegister_print(reg.reg, indenting + 1, alloc);

	if(reg.shaderBuffer.vars.ptr)
		SBFile_print(reg.shaderBuffer, indenting + 1, U16_MAX, true, alloc);
}

void ListSHRegisterRuntime_print(ListSHRegisterRuntime reg, U64 indenting, Allocator alloc) {

	if(indenting >= SHORTSTRING_LEN) {
		Log_debugLn(alloc, "ListSHRegisterRuntime_print() short string out of bounds");
		return;
	}

	ShortString indent;
	for(U8 i = 0; i < indenting; ++i) indent[i] = '\t';
	indent[indenting] = '\0';

	Log_debugLn(alloc, "%sRegisters:", indent);

	for(U64 i = 0; i < reg.length; ++i)
		SHRegisterRuntime_print(reg.ptr[i], indenting + 1, alloc);
}

//Combining multiple oiSH files into one

Bool SHFile_combine(SHFile a, SHFile b, Allocator alloc, SHFile *combined, Error *e_rr) {

	//Flags can only merge UTF8 safely, compilerVersion, sourceHash and HideMagicNumber have to match.

	Bool s_uccess = true;
	Bool isUTF8 = (a.flags & ESHSettingsFlags_IsUTF8) || (b.flags & ESHSettingsFlags_IsUTF8);
	ListU16 remappedBinaries = (ListU16) { 0 };
	ListU16 tmpBins = (ListU16) { 0 };
	SHRegisterRuntime tmpReg = (SHRegisterRuntime) { 0 };
	ListSHRegisterRuntime registers = (ListSHRegisterRuntime) { 0 };

	if((a.flags & ESHSettingsFlags_HideMagicNumber) != (b.flags & ESHSettingsFlags_HideMagicNumber))
		retError(clean, Error_invalidState(0, "SHFile_combine()::a and b have different flags: HideMagicNumber"))

	if(a.compilerVersion != b.compilerVersion || a.sourceHash != b.sourceHash)
		retError(clean, Error_invalidState(0, "SHFile_combine()::a and b have mismatching sourceHash or compilerVersion"))

	gotoIfError3(clean, SHFile_create(
		a.flags | (isUTF8 ? ESHSettingsFlags_IsUTF8 : 0),
		a.compilerVersion,
		a.sourceHash,
		alloc,
		combined,
		e_rr
	))

	gotoIfError2(clean, ListSHInclude_reserve(&combined->includes, a.includes.length + b.includes.length, alloc))
	gotoIfError2(clean, ListSHBinaryInfo_reserve(&combined->binaries, a.binaries.length + b.binaries.length, alloc))
	gotoIfError2(clean, ListSHEntry_reserve(&combined->entries, a.entries.length + b.entries.length, alloc))

	//Includes can be safely combined

	for(U64 i = 0; i < a.includes.length + b.includes.length; ++i) {

		SHInclude src = i < a.includes.length ? a.includes.ptr[i] : b.includes.ptr[i - a.includes.length];

		SHInclude include = (SHInclude) {
			.relativePath = CharString_createRefStrConst(src.relativePath),
			.crc32c = src.crc32c
		};

		gotoIfError3(clean, SHFile_addInclude(combined, &include, alloc, e_rr))
	}

	//Try to merge binaries if possible

	gotoIfError2(clean, ListU16_resize(&remappedBinaries, b.binaries.length, alloc))

	for (U64 i = 0; i < a.binaries.length; ++i) {

		U64 j = 0;

		for (; j < b.binaries.length; ++j)
			if(SHBinaryIdentifier_equals(a.binaries.ptr[i].identifier, b.binaries.ptr[j].identifier))
				break;

		SHBinaryInfo ai = a.binaries.ptr[i];

		SHBinaryInfo c = (SHBinaryInfo) {
			.identifier = (SHBinaryIdentifier) {
				.uniforms = ListCharString_createRefFromList(ai.identifier.uniforms),
				.entrypoint = CharString_createRefStrConst(ai.identifier.entrypoint)
			},
			.registers = ListSHRegisterRuntime_createRefFromList(ai.registers),
			.vendorMask = ai.vendorMask,
			.hasShaderAnnotation = ai.hasShaderAnnotation
		};

		const void *extPtrSrc = &ai.identifier.extensions;
		void *extPtrDst = &c.identifier.extensions;

		*(U64*)extPtrDst = *(const U64*)extPtrSrc;

		//Couldn't find a match, can add the easy way
		//TODO: Old binaries to new binary id

		if (j == b.binaries.length)
			for(U8 k = 0; k < ESHBinaryType_Count; ++k)
				c.binaries[k] = Buffer_createRefFromBuffer(ai.binaries[k], true);

		//Otherwise validate and merge binaries

		else {
		
			SHBinaryInfo bi = b.binaries.ptr[j];

			if(ai.vendorMask != bi.vendorMask || ai.hasShaderAnnotation != bi.hasShaderAnnotation)
				retError(clean, Error_invalidState(
					(U32) i, "SHFile_combine()::a and b have binary with mismatching vendorMask or hasShaderAnnotation"
				))

			for(U8 k = 0; k < ESHBinaryType_Count; ++k)

				if(Buffer_length(ai.binaries[k]) && Buffer_length(bi.binaries[k])) {
					if(Buffer_neq(ai.binaries[k], bi.binaries[k]))
						retError(clean, Error_invalidState(
							(U32) i,
							"SHFile_combine()::a and b have binary of same ESHBinaryType that didn't have the same contents"
						))
				}

				else if(Buffer_length(ai.binaries[k]))
					c.binaries[k] = Buffer_createRefFromBuffer(ai.binaries[k], true);

				else if(Buffer_length(bi.binaries[k]))
					c.binaries[k] = Buffer_createRefFromBuffer(bi.binaries[k], true);

			//Ensure the binary can still easily be found

			remappedBinaries.ptrNonConst[j] = (U16) i;

			//Combine registers

			gotoIfError2(clean, ListSHRegisterRuntime_reserve(&registers, bi.registers.length + ai.registers.length, alloc))

			//Match registers that were already found

			for (U64 k = 0; k < c.registers.length; ++k) {
				
				SHRegisterRuntime rega = c.registers.ptr[k];

				U64 l = 0;
				for(; l < bi.registers.length; ++l)
					if(CharString_equalsStringSensitive(bi.registers.ptr[l].name, rega.name))
						break;

				//Not found

				if (l == bi.registers.length || rega.hash == bi.registers.ptr[l].hash) {
					gotoIfError3(clean, SHRegisterRuntime_createCopy(c.registers.ptr[k], alloc, &tmpReg, e_rr))
					gotoIfError2(clean, ListSHRegisterRuntime_pushBack(&registers, tmpReg, alloc))
					tmpReg = (SHRegisterRuntime) { 0 };
					continue;
				}

				//Merge shader buffer

				SHRegisterRuntime regb = bi.registers.ptr[l];

				if((!!rega.shaderBuffer.vars.ptr) != (!!regb.shaderBuffer.vars.ptr))
					retError(clean, Error_invalidState(0, "SHFile_combine() has mismatching shader buffers"))

				if(rega.shaderBuffer.vars.ptr)
					gotoIfError3(clean, SBFile_combine(
						rega.shaderBuffer, regb.shaderBuffer, alloc, &tmpReg.shaderBuffer, e_rr
					))

				//Copy name

				gotoIfError2(clean, CharString_createCopy(rega.name, alloc, &tmpReg.name))

				//Merge array

				ListU32 arrayA = rega.arrays;
				ListU32 arrayB = regb.arrays;

				//One of them might be flat and the other one might be dynamic

				if (arrayA.length == 1 || arrayB.length == 1) {

					U64 dimsA = arrayA.length ? arrayA.ptr[0] : 0;
					U64 dimsB = arrayB.length ? arrayB.ptr[0] : 0;

					for(U64 m = 1; m < arrayA.length; ++m)
						dimsA *= arrayA.ptr[m];

					for(U64 m = 1; m < arrayB.length; ++m)
						dimsB *= arrayB.ptr[m];

					if(dimsA != dimsB)
						retError(clean, Error_invalidState(
							0, "SHFile_combine() register has mismatching array flattened size"
						))

					//In this case, we have to point arrayId to B's array.
					//This is called unflattening ([9] -> [3][3] for example).

					if (arrayB.length != 1)
						gotoIfError2(clean, ListU32_createCopy(arrayB, alloc, &tmpReg.arrays))

					else gotoIfError2(clean, ListU32_createCopy(arrayA, alloc, &tmpReg.arrays))
				}

				//Ensure they're the same size

				else {

					if(arrayA.length != arrayB.length)
						retError(clean, Error_invalidState(0, "SHFile_combine() variable has mismatching array dimensions"))

					for(U64 m = 0; m < arrayA.length; ++m)
						if(arrayA.ptr[m] != arrayB.ptr[m])
							retError(clean, Error_invalidState(0, "SHFile_combine() variable has mismatching array count"))

					gotoIfError2(clean, ListU32_createCopy(arrayA, alloc, &tmpReg.arrays))
				}

				//Merge register

				SHRegister merged = rega.reg;
				merged.isUsedFlag |= regb.reg.isUsedFlag;

				//Register type is mostly the same with two exceptions:
				//SamplerComparisonState DXIL = Sampler SPIRV
				//CombinedSampler SPIRV = (no flag) DXIL
				//So we need to promote these two to eachother to unify them.

				Bool isCmpSamplerA = rega.reg.registerType == ESHRegisterType_SamplerComparisonState;
				Bool isSamplerA = rega.reg.registerType == ESHRegisterType_Sampler || isCmpSamplerA;

				Bool isCmpSamplerB = regb.reg.registerType == ESHRegisterType_SamplerComparisonState;
				Bool isSamplerB = regb.reg.registerType == ESHRegisterType_Sampler || isCmpSamplerB;

				if(
					(isSamplerA != isSamplerB) &&
					(rega.reg.registerType &~ ESHRegisterType_IsCombinedSampler) !=
					(regb.reg.registerType &~ ESHRegisterType_IsCombinedSampler)
				)
					retError(clean, Error_invalidState(0, "SHFile_combine() has mismatching register types"))

				if(isCmpSamplerB)
					merged.registerType = ESHRegisterType_SamplerComparisonState;

				else merged.registerType = rega.reg.registerType | (regb.reg.registerType & ESHRegisterType_IsCombinedSampler);

				//Merge bindings

				for (U8 m = 0; m < ESHBinaryType_Count; ++m) {

					U64 bindingA = rega.reg.bindings.arrU64[m];
					U64 bindingB = regb.reg.bindings.arrU64[m];

					if (bindingA != U64_MAX && bindingB != U64_MAX) {

						if(bindingA != bindingB)
							retError(clean, Error_invalidState(0, "SHFile_combine() has mismatching register bindings"))

						continue;
					}

					if(bindingA != U64_MAX)
						continue;

					merged.bindings.arrU64[m] = bindingB;
				}

				//Merge input attachment

				if(
					rega.reg.registerType == ESHRegisterType_SubpassInput &&
					rega.reg.inputAttachmentId != regb.reg.inputAttachmentId
				)
					retError(clean, Error_invalidState(0, "SHFile_combine() has mismatching input attachment id"))

				//Merge texture info

				U8 typeOnly = rega.reg.registerType & ESHRegisterType_TypeMask;

				if (typeOnly >= ESHRegisterType_TextureStart && typeOnly < ESHRegisterType_TextureEnd) {

					Bool hasTexturePrimitiveA = rega.reg.texture.primitive != ESHTexturePrimitive_Count;
					Bool hasTexturePrimitiveB = regb.reg.texture.primitive != ESHTexturePrimitive_Count;

					//Ensure both primitives are the same

					if (hasTexturePrimitiveA && hasTexturePrimitiveB) {

						if(rega.reg.texture.primitive != regb.reg.texture.primitive)
							retError(clean, Error_invalidState(0, "SHFile_combine() texture primitives are incompatible"))

						if(rega.reg.texture.formatId && rega.reg.texture.formatId != regb.reg.texture.formatId)
							retError(clean, Error_invalidState(0, "SHFile_combine() texture format ids are incompatible"))
					}

					//One of the two has texture format, make sure they're compatible

					else {

						//Both have format, so needs to be fully compatible

						if (!hasTexturePrimitiveA && !hasTexturePrimitiveB) {

							if(rega.reg.texture.primitive != regb.reg.texture.primitive)
								retError(clean, Error_invalidState(0, "SHFile_combine() texture primitives are incompatible"))

							if(rega.reg.texture.formatId != regb.reg.texture.formatId)
								retError(clean, Error_invalidState(0, "SHFile_combine() texture formatId are incompatible"))

						}

						else {

							tmpReg.reg.texture.primitive =
								hasTexturePrimitiveA ? rega.reg.texture.primitive : regb.reg.texture.primitive;

							tmpReg.reg.texture.formatId =
								!hasTexturePrimitiveA ? rega.reg.texture.formatId : regb.reg.texture.formatId;
						}
					}
				}

				//Finalize hash and push

				tmpReg.reg = merged;

				gotoIfError3(clean, SHRegisterRuntime_hash(
					tmpReg.reg,
					tmpReg.name,
					tmpReg.arrays.length ? &tmpReg.arrays : NULL,
					tmpReg.shaderBuffer.vars.ptr ? &tmpReg.shaderBuffer : NULL,
					&tmpReg.hash,
					e_rr
				))

				gotoIfError2(clean, ListSHRegisterRuntime_pushBack(&registers, tmpReg, alloc))
				tmpReg = (SHRegisterRuntime) { 0 };
			}
			
			//Registers that weren't matched are new in the second source

			for (U64 k = 0; k < bi.registers.length; ++k) {
				
				SHRegisterRuntime regb = bi.registers.ptr[k];

				U64 l = 0;
				for(; l < ai.registers.length; ++l)
					if(CharString_equalsStringSensitive(ai.registers.ptr[l].name, regb.name))
						break;

				//Not found

				if (l == ai.registers.length) {
					gotoIfError3(clean, SHRegisterRuntime_createCopy(regb, alloc, &tmpReg, e_rr))
					gotoIfError2(clean, ListSHRegisterRuntime_pushBack(&registers, tmpReg, alloc))
					tmpReg = (SHRegisterRuntime) { 0 };
					continue;
				}
			}
		}

		if(registers.length)
			c.registers = registers;

		gotoIfError3(clean, SHFile_addBinaries(combined, &c, alloc, e_rr))
		registers = (ListSHRegisterRuntime) { 0 };
	}

	//Insert binaries from b that weren't found in a

	for (U64 i = 0; i < b.binaries.length; ++i) {

		U64 j = 0;

		for (; j < a.binaries.length; ++j)
			if(SHBinaryIdentifier_equals(b.binaries.ptr[i].identifier, a.binaries.ptr[j].identifier))
				break;

		if(j != a.binaries.length)
			continue;
			
		SHBinaryInfo bi = b.binaries.ptr[i];

		SHBinaryInfo c = (SHBinaryInfo) {
			.identifier = (SHBinaryIdentifier) {
				.uniforms = ListCharString_createRefFromList(bi.identifier.uniforms),
				.entrypoint = CharString_createRefStrConst(bi.identifier.entrypoint)
			},
			.registers = ListSHRegisterRuntime_createRefFromList(bi.registers),
			.vendorMask = bi.vendorMask,
			.hasShaderAnnotation = bi.hasShaderAnnotation
		};

		const void *extPtrSrc = &bi.identifier.extensions;
		void *extPtrDst = &c.identifier.extensions;

		*(U64*)extPtrDst = *(const U64*)extPtrSrc;

		gotoIfError3(clean, SHFile_addBinaries(combined, &c, alloc, e_rr))
		remappedBinaries.ptrNonConst[i] = (U16) (combined->binaries.length - 1);
	}

	//Merge and remap entries (remappedBinaries for b, no change for a)

	for (U64 i = 0; i < a.entries.length; ++i) {

		SHEntry entry = a.entries.ptr[i], entryi = entry;

		entry.name = CharString_createRefStrConst(entry.name);
		entry.binaryIds = (ListU16) { 0 };
		
		U64 j = 0;

		for (; j < b.entries.length; ++j)
			if(CharString_equalsStringSensitive(entryi.name, b.entries.ptr[j].name))
				break;

		//No duplicate, we can accept a

		if (j == b.entries.length) {
			entry.binaryIds = ListU16_createRefFromList(a.entries.ptr[i].binaryIds);
			gotoIfError3(clean, SHFile_addEntrypoint(combined, &entry, alloc, e_rr))
			continue;
		}

		SHEntry entryj = a.entries.ptr[j];

		//Make sure the two have identical data

		const void *cmpA0 = &entryi.stage;
		const void *cmpB0 = &entryj.stage;
		const void *cmpA = &entryi.groupX;
		const void *cmpB = &entryj.groupX;

		if(*(const U16*)cmpA0 != *(const U16*)cmpB0)
			retError(clean, Error_invalidState(
				(U32) i, "SHFile_combine()::a and b have an combined entry with mismatching entry or semantics"
			))

		for(U64 k = 0; k < 9; ++k)
			if(((const U64*)cmpA)[k] != ((const U64*)cmpB)[k])
				retError(clean, Error_invalidState(
					(U32) i, "SHFile_combine()::a and b have an combined entry with mismatching values"
				))

		U8 waveSizeTypei = entryi.waveSize >> 4 ? 2 : (entryi.waveSize ? 1 : 0);
		U8 waveSizeTypej = entryj.waveSize >> 4 ? 2 : (entryj.waveSize ? 1 : 0);

		if(
			(entryi.waveSize && entryj.waveSize && entryi.waveSize != entryj.waveSize) ||
			(waveSizeTypei && waveSizeTypej && waveSizeTypei != waveSizeTypej)
		)
			retError(clean, Error_invalidState(
				(U32) i, "SHFile_combine()::a and b have mismatching waveSize"
			))

		entry.waveSize = U16_max(entryi.waveSize, entryj.waveSize);

		//Combine the binaryIds.
		//a stays unmodified, but b needs to be remapped first

		gotoIfError2(clean, ListU16_createCopy(a.entries.ptr[i].binaryIds, alloc, &tmpBins))
		ListU16 binaryIdsb = b.entries.ptr[j].binaryIds;

		for(U64 k = 0; k < binaryIdsb.length; ++k) {

			U16 binaryId = remappedBinaries.ptr[binaryIdsb.ptr[k]];

			if(ListU16_contains(tmpBins, binaryId, 0, NULL))
				continue;

			gotoIfError2(clean, ListU16_pushBack(&tmpBins, binaryId, alloc))
		}

		if(entry.semanticNames.length != entryi.semanticNames.length)
			retError(clean, Error_invalidState(
				(U32) i, "SHFile_combine()::a and b have mismatching semanticNames"
			))

		for(U64 k = 0; k < entry.semanticNames.length; ++k)
			if(!CharString_equalsStringInsensitive(entry.semanticNames.ptr[k], entryj.semanticNames.ptr[k]))
				retError(clean, Error_invalidState(
					(U32) i, "SHFile_combine()::a and b have mismatching semanticNames[k]"
				))

		entry.semanticNames = ListCharString_createRefFromList(entry.semanticNames);
		entry.binaryIds = tmpBins;
		gotoIfError3(clean, SHFile_addEntrypoint(combined, &entry, alloc, e_rr))
		tmpBins = (ListU16) { 0 };
	}

	//Add b binaries that don't exist, these need some remapping

	for (U64 i = 0; i < b.entries.length; ++i) {

		SHEntry entry = b.entries.ptr[i];
		entry.name = CharString_createRefStrConst(entry.name);
		entry.binaryIds = (ListU16) { 0 };

		U64 j = 0;

		for (; j < a.entries.length; ++j)
			if(CharString_equalsStringSensitive(b.entries.ptr[i].name, a.entries.ptr[j].name))
				break;

		if(j != a.entries.length)		//Skip already handled ones
			continue;

		ListU16 binaryIdsb = b.entries.ptr[i].binaryIds;
		gotoIfError2(clean, ListU16_resize(&tmpBins, binaryIdsb.length, alloc))

		for(U64 k = 0; k < binaryIdsb.length; ++k)
			tmpBins.ptrNonConst[k] = remappedBinaries.ptr[binaryIdsb.ptr[k]];

		entry.binaryIds = tmpBins;
		gotoIfError3(clean, SHFile_addEntrypoint(combined, &entry, alloc, e_rr))
		tmpBins = (ListU16) { 0 };
	}

clean:

	SHRegisterRuntime_free(&tmpReg, alloc);
	ListSHRegisterRuntime_free(&registers, alloc);
	ListU16_free(&remappedBinaries, alloc);
	ListU16_free(&tmpBins, alloc);

	if(!s_uccess && combined)
		SHFile_free(combined, alloc);

	return s_uccess;
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
	ListSHRegisterRuntime_freeUnderlying(&info->registers, alloc);
	
	for(U8 i = 0; i < ESHBinaryType_Count; ++i)
		Buffer_free(&info->binaries[i], alloc);
}

void SHEntry_free(SHEntry *entry, Allocator alloc) {

	if(!entry)
		return;

	CharString_free(&entry->name, alloc);
	ListCharString_freeUnderlying(&entry->semanticNames, alloc);
}

void SHInclude_free(SHInclude *include, Allocator alloc) {

	if(!include)
		return;
		
	CharString_free(&include->relativePath, alloc);
}

void SHRegisterRuntime_free(SHRegisterRuntime *reg, Allocator alloc) {

	if(!reg)
		return;

	CharString_free(&reg->name, alloc);
	ListU32_free(&reg->arrays, alloc);
	SBFile_free(&reg->shaderBuffer, alloc);
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

void ListSHInclude_freeUnderlying(ListSHInclude *includes, Allocator alloc) {

	if(!includes)
		return;
		
	for(U64 i = 0; i < includes->length; ++i)
		SHInclude_free(&includes->ptrNonConst[i], alloc);

	ListSHInclude_free(includes, alloc);
}

void ListSHRegisterRuntime_freeUnderlying(ListSHRegisterRuntime *reg, Allocator alloc) {

	if(!reg)
		return;
		
	for(U64 i = 0; i < reg->length; ++i)
		SHRegisterRuntime_free(&reg->ptrNonConst[i], alloc);

	ListSHRegisterRuntime_free(reg, alloc);
}
