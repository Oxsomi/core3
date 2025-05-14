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

#include "types/base/type_id.h"

TypeIdShort ETypeId_toShortId(ETypeId typeId) {
	
	EDataType type = ETypeId_getDataType(typeId);

	U8 offsetId = 0;

	switch (type) {

		case EDataType_Char:
		default:								return 0;

		case EDataType_Bool:	offsetId = 0;	break;
		case EDataType_Int:		offsetId = 1;	break;
		case EDataType_UInt:	offsetId = 5;	break;
		case EDataType_Float:	offsetId = 9;	break;
	}

	U8 stride = 12;

	U8 w = ETypeId_getWidth(typeId);
	U8 h = ETypeId_getHeight(typeId);

	if (w == 1 && h == 1)
		return 1 + offsetId;		//C8 starts at 0

	if (h == 1)						//Skip vectors and C8, offset with stride 3 and add width (starting 2)
		return 1 + stride + offsetId * 3 + (w - 2);

	//Skip vectors, matrices, stride 4x3, add width and height (starting at 2) offsets by 4 each time
	return 1 + stride + stride * 3 + offsetId * 4 * 3 + (w - 1) + (h - 2) * 4;
}

Bool EDataType_isSigned(EDataType type) { return type & EDataType_IsSigned; }

EDataType ETypeId_getDataType(ETypeId id) { return (EDataType)(id & 7); }
EDataTypeStride ETypeId_getDataTypeStride(ETypeId id) { return (EDataTypeStride)((id >> 3) & 3); }
Bool ETypeId_isObject(ETypeId id) { return ETypeId_getDataType(id) == EDataType_Object; }

U8 ETypeId_getDataTypeBytes(ETypeId id) { 

	EDataType type = ETypeId_getDataType(id);

	if (type == EDataType_Char || type == EDataType_Bool)
		return 1;

	return ETypeId_isObject(id) ? 0 : (1 << type);
}

U8 ETypeId_getHeight(ETypeId id) { return ETypeId_isObject(id) ? 0 : (((id >> 5) & 3) + 1); }
U8 ETypeId_getWidth(ETypeId id) { return ETypeId_isObject(id) ? 0 : (((id >> 7) & 3) + 1); }

U8 ETypeId_getElements(ETypeId id) {
	return ETypeId_isObject(id) ? 0 : ETypeId_getWidth(id) * ETypeId_getHeight(id);
}

U64 ETypeId_getBytes(ETypeId id) {

	U64 siz = ETypeId_isObject(id) ? 0 : (U64)ETypeId_getDataTypeBytes(id) * ETypeId_getElements(id);

	if (ETypeId_getDataType(id) == EDataType_Bool)	//Bits, not bytes
		return (siz + 7) >> 3;

	return siz;
}

U16 ETypeId_getLibraryId(ETypeId id) { return (U16)(id >> 19); }
U16 ETypeId_getLibraryTypeId(ETypeId id) { return (U16)((id >> 9) & ((1 << 10) - 1)); }
