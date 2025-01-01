/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "types/base/allocator.h"
#include "types/base/error.h"

Bool SBFile_combine(SBFile a, SBFile b, Allocator alloc, SBFile *combined, Error *e_rr) {

	Bool s_uccess = true;
	Bool allocated = false;
	ListU16 remapVars = (ListU16) { 0 };
	ListU32 tmp = (ListU32) { 0 };

	if(a.bufferSize != b.bufferSize || (a.flags &~ ESBSettingsFlags_IsUTF8) != (b.flags &~ ESBSettingsFlags_IsUTF8)) {
		SBFile_print(a, 0, U16_MAX, true, alloc);
		SBFile_print(b, 0, U16_MAX, true, alloc);
		retError(clean, Error_invalidState(0, "SBFile_combine()::bufferSize or flags mismatch"))
	}

	if(!combined)
		retError(clean, Error_nullPointer(0, "SBFile_combine()::combined is required"))

	if(combined->vars.ptr)
		retError(clean, Error_invalidState(0, "SBFile_combine()::combined must be empty, otherwise indicated memleak"))

	if(a.vars.length != b.vars.length || a.structs.length != b.structs.length)
		retError(clean, Error_invalidState(0, "SBFile_combine() unrelated buffer layouts can't be merged"))

	gotoIfError3(clean, SBFile_create(a.flags | b.flags, a.bufferSize, alloc, combined, e_rr))

	//Vars, structs and arrays can easily be copied from a

	gotoIfError2(clean, ListSBStruct_resize(&combined->structs, a.structs.length, alloc))
	gotoIfError2(clean, ListSBVar_resize(&combined->vars, a.vars.length, alloc))
	gotoIfError2(clean, ListListU32_resize(&combined->arrays, a.arrays.length, alloc))

	gotoIfError2(clean, ListSBStruct_copy(a.structs, 0, combined->structs, 0, a.structs.length))
	gotoIfError2(clean, ListSBVar_copy(a.vars, 0, combined->vars, 0, a.vars.length))

	gotoIfError2(clean, ListCharString_resize(&combined->varNames, a.vars.length, alloc))
	gotoIfError2(clean, ListCharString_resize(&combined->structNames, a.structs.length, alloc))

	for(U64 i = 0; i < a.arrays.length; ++i)
		gotoIfError2(clean, ListU32_createCopy(a.arrays.ptr[i], alloc, &combined->arrays.ptrNonConst[i]))

	for(U64 i = 0; i < a.vars.length; ++i)
		gotoIfError2(clean, CharString_createCopy(a.varNames.ptr[i], alloc, &combined->varNames.ptrNonConst[i]))

	for(U64 i = 0; i < a.structs.length; ++i)
		gotoIfError2(clean, CharString_createCopy(a.structNames.ptr[i], alloc, &combined->structNames.ptrNonConst[i]))

	//Detect structs not found

	for (U64 i = 0; i < b.structs.length; ++i) {

		U64 j = 0;

		for(; j < combined->structs.length; ++j)
			if (
				combined->structs.ptr[j].stride == b.structs.ptr[i].stride &&
				CharString_equalsStringSensitive(combined->structNames.ptr[j], b.structNames.ptr[i])
			)
				break;

		if(j == combined->structs.length)
			retError(clean, Error_invalidState(0, "SBFile_combine() unrelated buffer layouts can't be combined"))
	}

	//Merge variables (should contain same variables in every parent, just different use flags)

	gotoIfError2(clean, ListU16_resize(&remapVars, b.vars.length, alloc))

	for (U64 i = 0; i < b.vars.length; ++i) {

		//Remap parent id and ensure it's basically the same in the parent

		CharString name = CharString_createRefStrConst(b.varNames.ptr[i]);
		SBVar var = b.vars.ptr[i];

		U16 parent = U16_MAX;

		if(var.parentId != U16_MAX)
			parent = remapVars.ptr[var.parentId];

		//Find in parent

		U16 newId = U16_MAX;

		for(U64 j = 0; j < a.vars.length; ++j)		//TODO: HashMap
			if (a.vars.ptr[j].parentId == parent && CharString_equalsStringSensitive(a.varNames.ptr[j], name)) {
				newId = (U16) j;
				break;
			}

		if(newId == U16_MAX)
			retError(clean, Error_invalidState(0, "SBFile_combine() variable not found, mismatching buffer layout"))

		//Ensure both have the same properties

		SBVar original = a.vars.ptr[newId];

		if(var.offset != original.offset || var.type != original.type)
			retError(clean, Error_invalidState(0, "SBFile_combine() variable has mismatching type or offset"))

		if((var.arrayIndex != U16_MAX) != (original.arrayIndex != U16_MAX))
			retError(clean, Error_invalidState(0, "SBFile_combine() variable has same variable, one with array, one without"))

		if((var.structId != U16_MAX) != (original.structId != U16_MAX))
			retError(clean, Error_invalidState(0, "SBFile_combine() variable has mismatching variable type"))

		if(
			var.structId != U16_MAX && (
				b.structs.ptr[var.structId].stride != a.structs.ptr[original.structId].stride ||
				!CharString_equalsStringSensitive(b.structNames.ptr[var.structId], a.structNames.ptr[original.structId])
			)
		)
			retError(clean, Error_invalidState(0, "SBFile_combine() variable has mismatching struct name or stride"))

		//Make sure that next lookups know where our b.vars[k] is at now

		remapVars.ptrNonConst[i] = newId;

		//The only thing that can mismatch here is the flags.
		//Because DXIL might have the DXIL used flag set but SPIRV used flag unset.

		combined->vars.ptrNonConst[i].flags |= var.flags;

		//Arrays need special care, we need to do two things:
		//If 1D arrays, we allow "unflattening" if for example SPIRV is merged with DXIL.
		//If 1D vs ND array, we enforce same count.
		//Else, we require the same dimension and counts.

		if (var.arrayIndex != U16_MAX) {

			ListU32 arrayA = a.arrays.ptr[original.arrayIndex];
			ListU32 arrayB = b.arrays.ptr[var.arrayIndex];

			//One of them might be flat and the other one might be dynamic

			if (arrayA.length == 1 || arrayB.length == 1) {

				U64 dimsA = arrayA.length ? arrayA.ptr[0] : 0;
				U64 dimsB = arrayB.length ? arrayB.ptr[0] : 0;

				for(U64 j = 1; j < arrayA.length; ++j)
					dimsA *= arrayA.ptr[j];

				for(U64 j = 1; j < arrayB.length; ++j)
					dimsB *= arrayB.ptr[j];

				if(dimsA != dimsB)
					retError(clean, Error_invalidState(0, "SBFile_combine() variable has mismatching array flattened size"))

				//In this case, we have to point arrayId to B's array.
				//This is called unflattening ([9] -> [3][3] for example).
				//We can't free the original array, because others might point to it too.
				//And because existing arrayIds would have to be decreased.

				if (arrayB.length != 1) {

					gotoIfError2(clean, ListU32_createCopy(arrayB, alloc, &tmp))

					if(combined->arrays.length + 1 >= U16_MAX)
						retError(clean, Error_invalidState(0, "SBFile_combine() combined arrays exceeded 65535"))

					combined->vars.ptrNonConst[i].arrayIndex = (U16) combined->arrays.length;
					gotoIfError2(clean, ListListU32_pushBack(&combined->arrays, tmp, alloc))
					tmp = (ListU32) { 0 };
				}
			}

			//Ensure they're the same size

			else {

				if(arrayA.length != arrayB.length)
					retError(clean, Error_invalidState(0, "SBFile_combine() variable has mismatching array dimensions"))

				for(U64 j = 0; j < arrayA.length; ++j)
					if(arrayA.ptr[j] != arrayB.ptr[j])
						retError(clean, Error_invalidState(0, "SBFile_combine() variable has mismatching array count"))
			}
		}
	}

clean:
	if(allocated && !s_uccess)
		SBFile_free(combined, alloc);

	ListU16_free(&remapVars, alloc);
	ListU32_free(&tmp, alloc);
	return s_uccess;
}
