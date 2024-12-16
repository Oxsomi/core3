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

TListImpl(SHBinaryInfo);
TListImpl(SHBinaryIdentifier);

#ifndef DISALLOW_SH_OXC3_PLATFORMS

	#include "platforms/platform.h"

	void SHBinaryInfo_printx(SHBinaryInfo binary) { SHBinaryInfo_print(binary, Platform_instance->alloc); }

	void SHBinaryIdentifier_freex(SHBinaryIdentifier *identifier) {
		SHBinaryIdentifier_free(identifier, Platform_instance->alloc);
	}

	void SHBinaryInfo_freex(SHBinaryInfo *info) {
		SHBinaryInfo_free(info, Platform_instance->alloc);
	}

#endif

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
	"UNBOUNDARRAYSIZE",
	"SUBGROUPOPERATIONS"
};

const C8 *ESHExtension_names[ESHExtension_Count] = {
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
	"UnboundArraySize",
	"SubgroupOperations"
};

const C8 *ESHVendor_names[ESHVendor_Count] = {
		"NV",
		"AMD",
		"ARM",
		"QCOM",
		"INTC",
		"IMGT",
		"MSFT"
};

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
	info.dormantExtensions = binaries->dormantExtensions;

	gotoIfError2(clean, ListSHBinaryInfo_pushBack(&shFile->binaries, info, alloc))
	*binaries = info = (SHBinaryInfo) { 0 };

clean:
	SHBinaryInfo_free(&info, alloc);
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

void SHBinaryInfo_print(SHBinaryInfo binary, Allocator alloc) {

	if(binary.hasShaderAnnotation)
		Log_debugLn(alloc, "SH Binary (lib file)");

	else {
		CharString entrypoint = binary.identifier.entrypoint;
		const C8 *name = SHEntry_stageNames[binary.identifier.stageType];
		Log_debugLn(alloc, "SH Binary (%s): %.*s", name, (int) CharString_length(entrypoint), entrypoint.ptr);
	}

	U16 shaderVersion = binary.identifier.shaderVersion;
	Log_debugLn(alloc, "\t[[oxc::model(%"PRIu8".%"PRIu8")]]", (U8)(shaderVersion >> 8), (U8) shaderVersion);

	ESHExtension activeExt = (binary.identifier.extensions &~ binary.dormantExtensions) & ESHExtension_All;
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

	if(!activeExt)
		Log_debugLn(alloc, "\t[[oxc::active_extension()]]");

	else {

		Log_debug(alloc, ELogOptions_None, "\t//[[oxc::active_extension(");

		Bool prev = false;

		for(U64 j = 0; j < ESHExtension_Count; ++j)
			if((activeExt >> j) & 1) {
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
			Log_debugLn(alloc, "\t[[oxc::vendor(\"%s\")]]", ESHVendor_names[i]);

	Log_debugLn(alloc, "\tBinaries:");

	for(U8 i = 0; i < ESHBinaryType_Count; ++i)
		if(Buffer_length(binary.binaries[i]))
			Log_debugLn(alloc, "\t\t%s: %"PRIu64, ESHBinaryType_names[i], Buffer_length(binary.binaries[i]));

	ListSHRegisterRuntime_print(binary.registers, 1, alloc);
}

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
