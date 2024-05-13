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

#pragma once
#include "types/string.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef enum EXXCompressionType {
	EXXCompressionType_None,							//--uncompressed
	EXXCompressionType_Brotli11,						//(default)
	EXXCompressionType_Brotli1,							//--fast-compress
	EXXCompressionType_Count
} EXXCompressionType;

typedef enum EXXEncryptionType {
	EXXEncryptionType_None,								//(default)
	EXXEncryptionType_AES256GCM,						//--aes <32-byte key (in hex or nyto)>
	EXXEncryptionType_Count
} EXXEncryptionType;

typedef enum EXXDataSizeType {		//Can be represented as a 2-bit array for example
	EXXDataSizeType_U8,
	EXXDataSizeType_U16,
	EXXDataSizeType_U32,
	EXXDataSizeType_U64
} EXXDataSizeType;

static const U8 SIZE_BYTE_TYPE[4] = { 1, 2, 4, 8 };

Error Buffer_consumeSizeType(Buffer *buf, EXXDataSizeType type, U64 *result);

U64 Buffer_forceReadSizeType(const U8 *ptr, EXXDataSizeType type);
U64 Buffer_forceWriteSizeType(U8 *ptr, EXXDataSizeType type, U64 result);

EXXDataSizeType EXXDataSizeType_getRequiredType(U64 v);

#ifdef __cplusplus
	}
#endif
