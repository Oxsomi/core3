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

	EDataTypeStride strid = ETypeId_getDataTypeStride(typeId);
	U8 localOff = 0;
	U8 localStride = 1;

	switch (type) {

		default:
			break;

		case EDataType_Float:

			switch (strid) {
				
				default:
				case EDataTypeStride_8:		return (TypeIdShort) -1;

				case EDataTypeStride_16:					break;
				case EDataTypeStride_32:	++localOff;		break;
				case EDataTypeStride_64:	localOff += 2;	break;
			}

			localStride = 3;
			break;

		case EDataType_UInt:
		case EDataType_Int:

			switch (strid) {
				default:				return (TypeIdShort)-1;
				case EDataTypeStride_8:					break;
				case EDataTypeStride_16:	++localOff;		break;
				case EDataTypeStride_32:	localOff += 2;	break;
				case EDataTypeStride_64:	localOff += 3;	break;
			}

			localStride = 4;
			break;
	}

	U8 stride = 12;

	U8 w = ETypeId_getWidth(typeId);
	U8 h = ETypeId_getHeight(typeId);

	if (w == 1 && h == 1)
		return 1 + offsetId + localOff;		//C8 starts at 0

	U8 base = 1 + stride;

	if (h == 1)						//Skip vectors and C8, offset with stride 3 and add width (starting 2)
		return base + offsetId * 3 + (w - 2) * localStride + localOff;

	base += stride * 3 + offsetId * 4 * 3;

	//Skip vectors, matrices, stride 4x3, add width and height (starting at 2) offsets by 4 each time
	return base + ((w - 1) + (h - 2) * 4) * localStride + localOff;
}
