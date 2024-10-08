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
#include "types/allocator.h"
#include "types/error.h"
#include "types/buffer.h"
#include "types/math.h"

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