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

#include "types/base/error.h"
#include "types/container/buffer.h"
#include "formats/oiXX/oiXX.h"

Error Buffer_consumeSizeType(Buffer *buf, EXXDataSizeType type, U64 *result) {

	if(!buf || !result)
		return Error_nullPointer(!buf ? 0 : 2, "Buffer_consumeSizeType()::buf and result are required");

	*result = 0;		//This is ok, as little endian a U8 would be stored in the first bytes not the last

	switch (type) {
		case EXXDataSizeType_U8:		return Buffer_consume(buf, result, 1);
		case EXXDataSizeType_U16:		return Buffer_consume(buf, result, 2);
		case EXXDataSizeType_U32:		return Buffer_consume(buf, result, 4);
		case EXXDataSizeType_U64:		return Buffer_consume(buf, result, 8);
		default:
			return Error_invalidEnum(
				1, (U64)type, (U64)EXXDataSizeType_U64,
				"Buffer_consumeSizeType()::type out of bounds"
			);
	}
}

U64 Buffer_forceReadSizeType(const U8 *ptr, EXXDataSizeType type) {

	if(!ptr)
		return 0;

	switch (type) {
		case EXXDataSizeType_U8:		return *ptr;
		case EXXDataSizeType_U16:		return Buffer_readU16(Buffer_createRefConst(ptr, sizeof(U16)), 0, NULL);
		case EXXDataSizeType_U32:		return Buffer_readU32(Buffer_createRefConst(ptr, sizeof(U32)), 0, NULL);
		case EXXDataSizeType_U64:		return Buffer_readU64(Buffer_createRefConst(ptr, sizeof(U64)), 0, NULL);
		default:						return 0;
	}
}

U64 Buffer_forceWriteSizeType(U8 *ptr, EXXDataSizeType type, U64 result) {

	if(!ptr)
		return 0;

	switch (type) {
		case EXXDataSizeType_U8:	*ptr = (U8) result;				return sizeof(U8);
		case EXXDataSizeType_U16:	Buffer_writeU16(Buffer_createRef(ptr, sizeof(U16)), 0, (U16) result);	return sizeof(U16);
		case EXXDataSizeType_U32:	Buffer_writeU32(Buffer_createRef(ptr, sizeof(U32)), 0, (U32) result);	return sizeof(U32);
		case EXXDataSizeType_U64:	Buffer_writeU64(Buffer_createRef(ptr, sizeof(U64)), 0, result);			return sizeof(U64);
		default:					return 0;
	}
}

EXXDataSizeType EXXDataSizeType_getRequiredType(U64 v) {
	return v <= U8_MAX ? EXXDataSizeType_U8 : (v <= U16_MAX ? EXXDataSizeType_U16 : (
		v <= U32_MAX ? EXXDataSizeType_U32 : EXXDataSizeType_U64
	));
}
