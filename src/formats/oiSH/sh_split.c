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

//formats/oiSH/sh_split.c

#include "formats/oiSH/sh_file.h"
#include "types/container/buffer.h"
#include "types/container/list_basic_types.h"
#include "types/base/allocator.h"
#include "types/base/error.h"
#include "types/base/constants.h"
#include "types/base/string_read_helper.h"

Bool SHFile_split(const SHFile *a, EGfxBinaryType type, const Allocator *alloc, SHFile *split, Error *e_rr) {

	Bool s_uccess = true;
	ListU16 remappedBinaries = (ListU16) { 0 };
	ListU16 tmpBins = (ListU16) { 0 };
	SHRegisterRuntime tmpReg = (SHRegisterRuntime) { 0 };
	ListSHRegisterRuntime registers = (ListSHRegisterRuntime) { 0 };
	SHBinaryInfo c = (SHBinaryInfo) { 0 };

	if(!a || !split)
		retError(clean, Error_nullPointer(!a ? 0 : 3, "SHFile_split()::a and split are required"));

	if(type >= EGfxBinaryType_Count)
		retError(clean, Error_invalidParameter(1, 0, "SHFile_split()::type is out of bounds"));

	//A reflection only oiSH carries no compiled code, so there is nothing to select binaries on.
	//Which backend a binary was for is written as which of its buffers carry code, and stripping those to
	// reflection takes that with it, so every binary and entrypoint has to be kept.
	//The registers still know which backend bound them, so the split is still worth doing: it is a register
	// level one, giving the reflection each backend would have had on its own.

	const Bool reflectionOnly = (a->flags & ESHSettingsFlags_ReflectionOnly) != 0;

	gotoIfError3(clean, SHFile_create(a->flags, a->compilerVersion, a->sourceHash, alloc, split, e_rr));

	gotoIfError3(clean, ListSHInclude_reserve(&split->includes, a->includes.length, alloc, e_rr));
	gotoIfError3(clean, ListSHBinaryInfo_reserve(&split->binaries, a->binaries.length, alloc, e_rr));
	gotoIfError3(clean, ListSHEntry_reserve(&split->entries, a->entries.length, alloc, e_rr));

	//Includes describe the source, which both halves were compiled from

	for (U64 i = 0; i < a->includes.length; ++i) {

		SHInclude include = (SHInclude) {
			.relativePath = CharString_createRefStrConst(a->includes.ptr[i].relativePath),
			.crc32c = a->includes.ptr[i].crc32c
		};

		gotoIfError3(clean, SHFile_addInclude(split, &include, alloc, e_rr));
	}

	//Keep the binaries that carry code for this type and remember where each one moved to.
	//U16_MAX marks one that was dropped, so the entrypoints below can tell which of theirs are gone.

	gotoIfError3(clean, ListU16_resize(&remappedBinaries, a->binaries.length, alloc, e_rr));

	for (U64 i = 0; i < a->binaries.length; ++i) {

		SHBinaryInfo ai = a->binaries.ptr[i];
		remappedBinaries.ptrNonConst[i] = U16_MAX;

		if(!reflectionOnly && !Buffer_length(ai.binaries[type]))
			continue;

		c = (SHBinaryInfo) {
			.identifier = (SHBinaryIdentifier) {
				.defines = ListCharString_createRefFromList(ai.identifier.defines),
				.entrypoint = CharString_createRefStrConst(ai.identifier.entrypoint),
				.uniformData = ListU8_createRefFromList(ai.identifier.uniformData),
				.uniforms = ListSHUniformRuntime_createRefFromList(ai.identifier.uniforms)
			},
			.dormantExtensions = ai.dormantExtensions,
			.vendorMask = ai.vendorMask,
			.hasShaderAnnotation = ai.hasShaderAnnotation
		};

		//extensions and dormantExt are a U32-aligned pair, so moving them as one in-place U64 can be a misaligned access.
		//The same eight bytes are copied instead.

		Buffer_memcpy(
			Buffer_createRef(&c.identifier.extensions, sizeof(U64)),
			Buffer_createRefConst(&ai.identifier.extensions, sizeof(U64))
		);

		if(Buffer_length(ai.binaries[type]))
			c.binaries[type] = Buffer_createRefFromBuffer(ai.binaries[type], true);

		//A register the other binary type had on its own isn't present in this one.
		//That's how a DXIL only standalone sampler, a SPIRV only subpass input or the $Globals / push constant
		// pair that SHFile_combine leaves unmatched (their names differ) each end up back in the half they came from.
		//What survives is copied as it was found: the reflection both halves gained from the merge is deliberately
		// kept, so two splits of one file keep agreeing with eachother (see docs/oiSH.md).

		for (U64 j = 0; j < ai.registers.length; ++j) {

			if(!SHRegister_isPresentIn(&ai.registers.ptr[j].reg, type))
				continue;

			gotoIfError3(clean, SHRegisterRuntime_createCopy(&ai.registers.ptr[j], alloc, &tmpReg, e_rr));

			//SHRegisterRuntime_createCopy leaves the hash for the caller to fill in.
			//Everything it feeds on was copied as it was, so this lands back on the value the register already had.

			gotoIfError3(clean, SHRegisterRuntime_hash(
				&tmpReg.reg,
				&tmpReg.name,
				tmpReg.arrays.length ? &tmpReg.arrays : NULL,
				tmpReg.shaderBuffer.vars.ptr ? &tmpReg.shaderBuffer : NULL,
				&tmpReg.hash,
				e_rr
			));

			gotoIfError3(clean, ListSHRegisterRuntime_pushBack(&registers, tmpReg, alloc, e_rr));
			tmpReg = (SHRegisterRuntime) { 0 };
		}

		//Ownership of the registers moves into c, which the clean label frees if adding the binary fails.
		//Dropping registers only ever lowers SHFile_addBinary's resource counters, so its limits can't newly trip.
		//Bindless and friends stay set on the identifier even if this half no longer needs them, which is what
		// SHBinaryIdentifier_equals already expects of a merged file.

		c.registers = registers;
		registers = (ListSHRegisterRuntime) { 0 };

		gotoIfError3(clean, SHFile_addBinary(split, &c, alloc, e_rr));
		remappedBinaries.ptrNonConst[i] = (U16) (split->binaries.length - 1);
	}

	if(!split->binaries.length)
		retError(clean, Error_invalidState(0, "SHFile_split()::a has no binary of the requested type"));

	//Remap the entrypoints, dropping the ones that only ever compiled to the other type

	for (U64 i = 0; i < a->entries.length; ++i) {

		SHEntry entry = a->entries.ptr[i];
		const ListU16 binaryIds = entry.binaryIds;

		entry.name = CharString_createRefStrConst(entry.name);
		entry.semanticNames = ListCharString_createRefFromList(entry.semanticNames);
		entry.binaryIds = (ListU16) { 0 };

		for (U64 j = 0; j < binaryIds.length; ++j) {

			const U16 binaryId = remappedBinaries.ptr[binaryIds.ptr[j]];

			if(binaryId == U16_MAX)
				continue;

			gotoIfError3(clean, ListU16_pushBack(&tmpBins, binaryId, alloc, e_rr));
		}

		//Nothing this entrypoint pointed at survived, so it isn't part of this half.
		//An [[oxc::binary()]] annotation naming only the other type lands here.

		if(!tmpBins.length)
			continue;

		entry.binaryIds = tmpBins;
		gotoIfError3(clean, SHFile_addEntrypoint(split, &entry, alloc, e_rr));
		tmpBins = (ListU16) { 0 };
	}

clean:

	SHBinaryInfo_free(&c, alloc);
	SHRegisterRuntime_free(&tmpReg, alloc);
	ListSHRegisterRuntime_freeUnderlying(&registers, alloc);
	ListU16_free(&remappedBinaries, alloc);
	ListU16_free(&tmpBins, alloc);

	if(!s_uccess && split)
		SHFile_free(split, alloc);

	return s_uccess;
}
