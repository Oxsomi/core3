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

//formats/oiPL/pl_file.c

#include "formats/oiPL/pl_file.h"
#include "formats/oiDL/dl_entry.h"
#include "formats/oiDL/dl_load.h"
#include "types/container/list_impl.h"
#include "types/container/string.h"
#include "types/base/error.h"
#include <inttypes.h>

TListImpl(PLSamplerInfo);
TListImpl(PLDescriptorBinding);
TListImpl(PLFile);

Bool PLFile_create(EPLSettingsFlags flags, const Allocator *alloc, PLFile *plFile, Error *e_rr) {

	Bool s_uccess = true;
	Bool allocated = false;

	if(!plFile)
		retError(clean, Error_nullPointer(2, "PLFile_create()::plFile is required"));

	if(plFile->bindings.length || plFile->names.entryStrings.length)
		retError(clean, Error_invalidOperation(0, "PLFile_create()::plFile isn't empty, might indicate memleak"));

	if(flags & EPLSettingsFlags_Invalid)
		retError(clean, Error_invalidParameter(0, 0, "PLFile_create()::flags contained unsupported flag"));

	DLSettings settings = (DLSettings) {
		.dataType = EDLDataType_String,
		.flags = EDLSettingsFlags_HideMagicNumber
	};

	gotoIfError3(clean, DLFile_create(&settings, 0, alloc, &plFile->names, e_rr));
	allocated = true;

	plFile->flags = flags;
	plFile->hash = 0;

clean:

	if(allocated && !s_uccess)
		PLFile_free(plFile, alloc);

	return s_uccess;
}

void PLFile_free(PLFile *plFile, const Allocator *alloc) {

	if(!plFile)
		return;

	DLFile_free(&plFile->names, alloc);
	ListPLDescriptorBinding_free(&plFile->bindings, alloc);
	ListPLSamplerInfo_free(&plFile->samplers, alloc);

	*plFile = (PLFile) { 0 };
}

void ListPLFile_freeUnderlying(ListPLFile *files, const Allocator *alloc) {

	if(!files)
		return;

	for(U64 i = 0; i < files->length; ++i)
		PLFile_free(&files->ptrNonConst[i], alloc);

	ListPLFile_free(files, alloc);
}

Bool PLFile_addString(PLFile *plFile, CharString *str, const Allocator *alloc, U32 *id, Error *e_rr) {

	Bool s_uccess = true;
	CharString copy = CharString_createNull();

	if(!plFile || !str || !id)
		retError(clean, Error_nullPointer(!plFile ? 0 : (!str ? 1 : 3), "PLFile_addString() requires plFile, str and id"));

	if (!CharString_length(*str)) {
		*id = U32_MAX;
		goto clean;
	}

	const U64 found = DLFile_findLoadedString(&plFile->names, 0, U64_MAX, str);

	if (found != U64_MAX) {
		*id = (U32) found;
		goto clean;
	}

	if(plFile->names.entryStrings.length >= U32_MAX - 1)
		retError(clean, Error_outOfBounds(
			0, plFile->names.entryStrings.length, U32_MAX - 1, "PLFile_addString() too many strings"
		));

	gotoIfError3(clean, CharString_createCopy(*str, alloc, &copy, e_rr));
	gotoIfError3(clean, DLFile_addEntryString(&plFile->names, &copy, alloc, e_rr));

	copy = CharString_createNull();
	*id = (U32) (plFile->names.entryStrings.length - 1);

clean:
	CharString_free(&copy, alloc);
	return s_uccess;
}

Bool PLFile_copy(const PLFile *src, const Allocator *alloc, PLFile *dst, Error *e_rr) {

	Bool s_uccess = true;
	Bool allocated = false;
	CharString copy = CharString_createNull();

	if(!src || !dst)
		retError(clean, Error_nullPointer(!src ? 0 : 2, "PLFile_copy()::src and dst are required"));

	if(dst->bindings.length || dst->names.entryStrings.length)
		retError(clean, Error_invalidOperation(0, "PLFile_copy()::dst isn't empty, might indicate memleak"));

	gotoIfError3(clean, PLFile_create(src->flags, alloc, dst, e_rr));
	allocated = true;

	gotoIfError3(clean, ListPLDescriptorBinding_createCopy(src->bindings, alloc, &dst->bindings, e_rr));
	gotoIfError3(clean, ListPLSamplerInfo_createCopy(src->samplers, alloc, &dst->samplers, e_rr));

	//Name ids stay valid because the pool is copied in order, without deduplicating differently

	for (U64 i = 0; i < src->names.entryStrings.length; ++i) {
		gotoIfError3(clean, CharString_createCopy(src->names.entryStrings.ptr[i], alloc, &copy, e_rr));
		gotoIfError3(clean, DLFile_addEntryString(&dst->names, &copy, alloc, e_rr));
	}

	dst->pushConstant = src->pushConstant;
	dst->hasPushConstant = src->hasPushConstant;
	dst->hash = src->hash;

clean:

	CharString_free(&copy, alloc);

	if(allocated && !s_uccess)
		PLFile_free(dst, alloc);

	return s_uccess;
}

Bool PLDescriptorBinding_validate(
	const PLDescriptorBinding *b, U64 nameCount, U64 samplerCount, Bool allowReservedSpace, Error *e_rr
) {

	Bool s_uccess = true;

	if(!b)
		retError(clean, Error_nullPointer(0, "PLDescriptorBinding_validate()::b is required"));

	const U32 classType = b->registerType & EGfxRegisterType_TypeMask;

	if(classType >= EGfxRegisterType_Count)
		retError(clean, Error_invalidState(0, "PLDescriptorBinding_validate() register type is unknown"));

	if(PLDescriptorBinding_source(*b) >= EPLSource_Count)
		retError(clean, Error_invalidState(0, "PLDescriptorBinding_validate() source is unknown"));

	const U32 name = PLDescriptorBinding_name(*b);

	if(nameCount != U64_MAX && name != PLDescriptorBinding_NAME_NONE && name >= nameCount)
		retError(clean, Error_invalidState(0, "PLDescriptorBinding_validate() name out of bounds"));

	Bool anyPair = false;

	for(U8 i = 0; i < EGfxBinaryType_Count; ++i) {

		const GfxBinding pair = b->bindings.arr[i];

		if((pair.space == U32_MAX) != (pair.binding == U32_MAX))
			retError(clean, Error_invalidState(
				0, "PLDescriptorBinding_validate() a binding pair has to be present or absent as a whole"
			));

		if(!allowReservedSpace && pair.space == OXC3_RESERVED_SPACE)
			retError(clean, Error_invalidState(
				0, "PLDescriptorBinding_validate() register space 0xC3 is reserved for the runtime's bindless set"
			));

		if(pair.space != U32_MAX)
			anyPair = true;
	}

	//The push constant row is the one register that binds through no pair (SPIRV has none for it)

	if(!anyPair && classType != EGfxRegisterType_PushConstants)
		retError(clean, Error_invalidState(
			0, "PLDescriptorBinding_validate() a binding has to exist for at least one binary type"
		));

	const Bool isSampler =
		classType == EGfxRegisterType_Sampler || classType == EGfxRegisterType_SamplerComparisonState;

	if(
		(b->registerType & EGfxRegisterType_IsWrite) && (
			isSampler || classType == EGfxRegisterType_AccelerationStructure ||
			classType == EGfxRegisterType_ConstantBuffer || classType == EGfxRegisterType_PushConstants
		)
	)
		retError(clean, Error_invalidState(
			0, "PLDescriptorBinding_validate() the register class can't carry the write flag"
		));

	if(
		(classType == EGfxRegisterType_ConstantBuffer || classType == EGfxRegisterType_PushConstants) &&
		(!b->strideOrLength || b->strideOrLength > 64 * KIBI)
	)
		retError(clean, Error_invalidState(
			0, "PLDescriptorBinding_validate() a constant buffer's size has to be 0 < size <= 64KiB"
		));

	if(
		(
			classType == EGfxRegisterType_StructuredBuffer || classType == EGfxRegisterType_StructuredBufferAtomic ||
			classType == EGfxRegisterType_StorageBuffer || classType == EGfxRegisterType_StorageBufferAtomic
		) &&
		!b->strideOrLength
	)
		retError(clean, Error_invalidState(
			0, "PLDescriptorBinding_validate() a structured buffer's stride can't be 0"
		));

	if(samplerCount != U64_MAX && isSampler && b->samplerId && b->samplerId - 1 >= samplerCount)
		retError(clean, Error_invalidState(
			0, "PLDescriptorBinding_validate() sampler row references a sampler the file hasn't got"
		));

clean:
	return s_uccess;
}

static Bool PLFile_pushIssue(
	ListCharString *issues, const Allocator *alloc, const C8 *label, U64 index, const C8 *what, Error *e_rr
) {

	Bool s_uccess = true;
	CharString issue = CharString_createNull();

	gotoIfError3(clean, CharString_format(alloc, &issue, e_rr, "%s[%"PRIu64"]: %s", label, index, what));
	gotoIfError3(clean, ListCharString_pushBack(issues, issue, alloc, e_rr));
	issue = CharString_createNull();

clean:
	CharString_free(&issue, alloc);
	return s_uccess;
}

Bool PLFile_validate(const PLFile *plFile, const Allocator *alloc, ListCharString *issues, Error *e_rr) {

	Bool s_uccess = true;

	if(!plFile || !issues)
		retError(clean, Error_nullPointer(!plFile ? 0 : 2, "PLFile_validate()::plFile and issues are required"));

	for (U64 i = 0; i < plFile->bindings.length; ++i) {

		const PLDescriptorBinding b = plFile->bindings.ptr[i];

		Error rowError = Error_none();

		if(!PLDescriptorBinding_validate(
			&b, plFile->names.entryStrings.length, plFile->samplers.length, false, &rowError
		))
			gotoIfError3(clean, PLFile_pushIssue(
				issues, alloc, "layout.binding", i, rowError.errorStr ? rowError.errorStr : "invalid", e_rr
			));

		if(!b.count)
			gotoIfError3(clean, PLFile_pushIssue(
				issues, alloc, "layout.binding", i,
				"unbounded array; supply layout.binding.count before creating the layout on a device", e_rr
			));

		//Two rows can't overlap where a device would bind them

		for(U64 j = 0; j < i; ++j) {

			const PLDescriptorBinding o = plFile->bindings.ptr[j];

			const Bool spirvClash = GfxBinding_overlaps(
				b.bindings.arr[EGfxBinaryType_SPIRV], b.registerType, b.count,
				o.bindings.arr[EGfxBinaryType_SPIRV], o.registerType, o.count, EGfxBinaryType_SPIRV
			);

			const Bool dxilClash = GfxBinding_overlaps(
				b.bindings.arr[EGfxBinaryType_DXIL], b.registerType, b.count,
				o.bindings.arr[EGfxBinaryType_DXIL], o.registerType, o.count, EGfxBinaryType_DXIL
			);

			if (spirvClash || dxilClash) {
				
				gotoIfError3(clean, PLFile_pushIssue(
					issues, alloc, "layout.binding", i, "overlaps an earlier row's register range", e_rr
				));

				break;
			}
		}
	}

	if (plFile->hasPushConstant) {

		Error rowError = Error_none();

		if ((plFile->pushConstant.registerType & EGfxRegisterType_TypeMask) != EGfxRegisterType_PushConstants) {
			gotoIfError3(clean, PLFile_pushIssue(
				issues, alloc, "layout.pushConstant", 0, "isn't a push constant register", e_rr
			));
		}

		else if (!PLDescriptorBinding_validate(
			&plFile->pushConstant, plFile->names.entryStrings.length, plFile->samplers.length, false, &rowError
		))
			gotoIfError3(clean, PLFile_pushIssue(
				issues, alloc, "layout.pushConstant", 0, rowError.errorStr ? rowError.errorStr : "invalid", e_rr
			));
	}

clean:
	return s_uccess;
}

Bool PLFile_finalize(PLFile *plFile, const Allocator *alloc, Error *e_rr) {

	(void) alloc;

	Bool s_uccess = true;

	if(!plFile)
		retError(clean, Error_nullPointer(0, "PLFile_finalize()::plFile is required"));

	U64 hash = Buffer_fnv1a64Single(plFile->hasPushConstant, Buffer_fnv1a64Offset);
	hash = Buffer_fnv1a64(ListPLDescriptorBinding_bufferConst(plFile->bindings), hash);
	hash = Buffer_fnv1a64(ListPLSamplerInfo_bufferConst(plFile->samplers), hash);

	if(plFile->hasPushConstant)
		hash = Buffer_fnv1a64(
			Buffer_createRefConst(&plFile->pushConstant, sizeof(plFile->pushConstant)), hash
		);

	for(U64 i = 0; i < plFile->names.entryStrings.length; ++i)
		hash = Buffer_fnv1a64(CharString_bufferConst(plFile->names.entryStrings.ptr[i]), hash);

	plFile->hash = hash;

clean:
	return s_uccess;
}
