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

//types/base/test/test_types_base_type_id.c

#include "test_types_base_shared.h"
#include "types/base/type_id.h"

void Test_typeId(Test *test) {

	Test_setModule(test, "TypeId");

	for (U64 i = 0; i < ETypeId_Max; ++i) {

		TypeId typeId = ETypeId_arr[i];
		TypeIdShort shortTypeId = ETypeId_toShortId(typeId);

		Test_assert(test, "ETypeId_toShortId", shortTypeId == i);
	}

	//A scalar's size is its width's: the data type only says how the bits are read.

	static const struct { TypeId typeId; U8 bytes; } scalars[] = {
		{ (TypeId) ETypeId_U8,  1 }, { (TypeId) ETypeId_U16, 2 }, { (TypeId) ETypeId_U32, 4 }, { (TypeId) ETypeId_U64, 8 },
		{ (TypeId) ETypeId_I8,  1 }, { (TypeId) ETypeId_I16, 2 }, { (TypeId) ETypeId_I32, 4 }, { (TypeId) ETypeId_I64, 8 },
		{ (TypeId) ETypeId_F16, 2 }, { (TypeId) ETypeId_F32, 4 }, { (TypeId) ETypeId_F64, 8 },
		{ (TypeId) ETypeId_B1,  1 }, { (TypeId) ETypeId_C8,  1 }
	};

	for (U64 i = 0; i < sizeof(scalars) / sizeof(scalars[0]); ++i) {
		Test_assert(test, "ETypeId_getDataTypeBytes", ETypeId_getDataTypeBytes(scalars[i].typeId) == scalars[i].bytes);
		Test_assert(test, "ETypeId_getBytes", ETypeId_getBytes(scalars[i].typeId) == scalars[i].bytes);
	}

	//A vector or matrix is its elements' bytes, a B1 one its bits rounded up to bytes.

	Test_assert(test, "ETypeId_getBytes F32x4", ETypeId_getBytes((TypeId) ETypeId_F32x4) == 16);
	Test_assert(test, "ETypeId_getBytes U16x3", ETypeId_getBytes((TypeId) ETypeId_U16x3) == 6);
	Test_assert(test, "ETypeId_getBytes F64x4x4", ETypeId_getBytes((TypeId) ETypeId_F64x4x4) == 128);
	Test_assert(test, "ETypeId_getBytes B1x4", ETypeId_getBytes((TypeId) ETypeId_B1x4) == 1);
}
