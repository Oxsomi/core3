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

#include "formats/oiSB/sb_file.h"
#include "formats/oiDL/dl_file.h"
#include "formats/oiDL/dl_entry.h"
#include "formats/oiDL/dl_load.h"
#include "types/container/list_basic_types.h"
#include "types/base/string_read_helper.h"
#include "types/base/allocator.h"
#include "types/base/error.h"
#include "types/base/constants.h"
#include "types/base/mathi.h"

Bool SBFile_combine(const SBFile *a, const SBFile *b, const Allocator *alloc, SBFile *combined, Error *e_rr) {

	Bool s_uccess = true;
	Bool allocated = false;
	ListU16 remapVars = (ListU16) { 0 };
	ListU32 tmp = (ListU32) { 0 };

	if(!a || !b)
		retError(clean, Error_nullPointer(!a ? 0 : 1, "SBFile_combine()::a and b are required"));

	if (a->bufferSize != b->bufferSize || a->flags != b->flags)
		retError(clean, Error_invalidState(0, "SBFile_combine()::bufferSize or flags mismatch"));

	if (!combined)
		retError(clean, Error_nullPointer(0, "SBFile_combine()::combined is required"));

	if(combined->bufferSize)
		retError(clean, Error_invalidState(0, "SBFile_combine()::combined must be empty, otherwise indicated memleak"));

	if(a->vars.length != b->vars.length || a->structs.length != b->structs.length)
		retError(clean, Error_invalidState(0, "SBFile_combine() unrelated buffer layouts can't be merged"));

	gotoIfError3(clean, SBFile_create(a->flags | ESBSettingsFlags_CreateNoReserve, a->bufferSize, alloc, combined, e_rr));
	allocated = true;

	//Vars, structs and arrays can easily be copied from a

	gotoIfError3(clean, ListSBStruct_createCopy(a->structs, alloc, &combined->structs, e_rr));
	gotoIfError3(clean, ListSBVar_createCopy(a->vars, alloc, &combined->vars, e_rr));
	gotoIfError3(clean, ListListU32_reserve(&combined->arrays, U64_max(a->arrays.length, b->arrays.length), alloc, e_rr));

	gotoIfError3(clean, DLFile_createCopy(&a->names, alloc, &combined->names, e_rr));

	gotoIfError3(clean, ListListU32_resize(&combined->arrays, a->arrays.length, alloc, e_rr));
	for (U64 i = 0; i < a->arrays.length; ++i)
		gotoIfError3(clean, ListU32_createCopy(a->arrays.ptr[i], alloc, &combined->arrays.ptrNonConst[i], e_rr));

	//Detect structs not found

	for (U64 i = 0; i < b->structs.length; ++i) {

		U64 j = 0, k = a->structs.length;

		for (; j < k; ++j) {		//TODO: HashMap

			j = DLFile_findLoadedString(&a->names, j, a->structs.length, &b->names.entryStrings.ptr[i]);

			if (j >= k)
				break;

			if (combined->structs.ptr[j].stride == b->structs.ptr[i].stride)
				break;
		}

		if (j == k || j == U64_MAX)
			retError(clean, Error_invalidState(0, "SBFile_combine() unrelated buffer layouts can't be combined"));
	}

	//Merge variables (should contain same variables in every parent, just different use flags)

	gotoIfError3(clean, ListU16_resize(&remapVars, b->vars.length, alloc, e_rr));

	for (U64 i = 0; i < b->vars.length; ++i) {

		//Remap parent id and ensure it's basically the same in the parent

		CharString name = CharString_createNull();
		gotoIfError3(clean, DLFile_loadedStringAtConst(&b->names, b->structs.length + i, &name, e_rr));

		SBVar var = b->vars.ptr[i];

		U16 parent = U16_MAX;

		if(var.parentId != U16_MAX)
			parent = remapVars.ptr[var.parentId];

		//Find in parent

		U16 oldId = U16_MAX;

		{
			U64 j = a->structs.length, k = j + a->vars.length;

			for (; j < k; ++j) {		//TODO: HashMap

				j = DLFile_findLoadedString(&a->names, j, a->vars.length, &b->names.entryStrings.ptr[i]);

				if (j >= k)
					break;

				if (combined->vars.ptr[j].parentId == parent)
					break;
			}

			if (j == k || j == U64_MAX)
				retError(clean, Error_invalidState(0, "SBFile_combine() variable not found, mismatching buffer layout"));

			oldId = (U16)(j - a->structs.length);
		}

		//Ensure both have the same properties

		SBVar original = a->vars.ptr[oldId];

		if(var.offset != original.offset || var.type != original.type)
			retError(clean, Error_invalidState(0, "SBFile_combine() variable has mismatching type or offset"));

		if((var.structId != U16_MAX) != (original.structId != U16_MAX))
			retError(clean, Error_invalidState(0, "SBFile_combine() variable has mismatching variable type"));

		if((var.arrayDimOrArrayId != 0) != (original.arrayDimOrArrayId != 0))
			retError(clean, Error_invalidState(0, "SBFile_combine() has mismatching array arguments"));

		if (
			var.structId != U16_MAX && (
				b->structs.ptr[var.structId].stride != a->structs.ptr[original.structId].stride ||
				!CharString_equalsStringSensitive(
					&b->names.entryStrings.ptr[var.structId],
					&a->names.entryStrings.ptr[original.structId]
				)
			)
		)
			retError(clean, Error_invalidState(0, "SBFile_combine() variable has mismatching struct name or stride"));

		//Make sure that next lookups know where our b.vars[k] is at now

		remapVars.ptrNonConst[i] = oldId;

		//The only thing that can mismatch here is the flags.
		//Because DXIL might have the DXIL used flag set but SPIRV used flag unset.

		combined->vars.ptrNonConst[oldId].flags |= var.flags;

		//Arrays need special care, we need to do two things:
		//If 1D arrays, we allow "unflattening" if for example SPIRV is merged with DXIL.
		//If 1D vs ND array, we enforce same count.
		//Else, we require the same dimension and counts.

		//We need to first "unwrap" our array, this can either point to a real array or to an inline 15b array.
		//DXIL will extremely likely just be a 15-bit array dimension,
		// while SPIR-V could also be an ND array (but likely inline too).
		// DXIL can still exceed 15-bit which will allocate a full size array description.

		U32 array1DA = original.arrayDimOrArrayId;
		U32 array1DB = var.arrayDimOrArrayId;

		ListU32 arrayA = (ListU32) { 0 };
		ListU32 arrayB = (ListU32){ 0 };

		if(array1DA)
			gotoIfError3(clean, ListU32_createRefConst(&array1DA, 1, &arrayA, e_rr));

		if (array1DB)
			gotoIfError3(clean, ListU32_createRefConst(&array1DB, 1, &arrayB, e_rr));

		if (array1DA >> 15)
			arrayA = a->arrays.ptr[array1DA & (U32)I16_MAX];

		if (array1DB >> 15)
			arrayB = b->arrays.ptr[array1DB & (U32)I16_MAX];

		//One of them might be flat and the other one might be dynamic

		if (arrayA.length == 1 || arrayB.length == 1) {

			U64 dimsA = arrayA.length ? arrayA.ptr[0] : 0;
			U64 dimsB = arrayB.length ? arrayB.ptr[0] : 0;

			for(U64 j = 1; j < arrayA.length; ++j)
				dimsA *= arrayA.ptr[j];

			for(U64 j = 1; j < arrayB.length; ++j)
				dimsB *= arrayB.ptr[j];

			if (dimsA != dimsB)
				retError(clean, Error_invalidState(0, "SBFile_combine() variable has mismatching array flattened size"));

			//In this case, we have to point arrayId to B's array.
			//This is called unflattening ([9] -> [3][3] for example).
			//We can't free the original array, because others might point to it too.
			//And because existing arrayIds would have to be decreased.

			if (arrayB.length != 1) {

				gotoIfError3(clean, ListU32_createCopy(arrayB, alloc, &tmp, e_rr));

				if(combined->arrays.length + 1 >= I16_MAX)
					retError(clean, Error_invalidState(0, "SBFile_combine() combined arrays exceeded 32768"));

				combined->vars.ptrNonConst[oldId].arrayDimOrArrayId = ((U16) combined->arrays.length) | 0x8000;
				gotoIfError3(clean, ListListU32_pushBack(&combined->arrays, tmp, alloc, e_rr));
				tmp = (ListU32) { 0 };
			}
		}

		//Ensure they're the same size

		else {

			if(arrayA.length != arrayB.length)
				retError(clean, Error_invalidState(0, "SBFile_combine() variable has mismatching array dimensions"));

			for(U64 j = 0; j < arrayA.length; ++j)
				if(arrayA.ptr[j] != arrayB.ptr[j])
					retError(clean, Error_invalidState(0, "SBFile_combine() variable has mismatching array count"));
		}
	}

clean:
	if(allocated && !s_uccess)
		SBFile_free(combined, alloc);

	ListU16_free(&remapVars, alloc);
	ListU32_free(&tmp, alloc);
	return s_uccess;
}
