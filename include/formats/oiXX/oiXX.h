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

//formats/oiXX/oiXX.h

#pragma once
#include "types/base/types.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct StreamCursor StreamCursor;
typedef struct Buffer Buffer;
typedef struct Error Error;
typedef struct Allocator Allocator;

typedef enum EXXCompressionType {
	EXXCompressionType_None,                            //--uncompressed (default)
	//EXXCompressionType_Brotli11,                      //(default): Unsupported
	//EXXCompressionType_Brotli1,                       //--fast-compress
	EXXCompressionType_Count
} EXXCompressionType;

typedef U8 XXCompressionType;        //EXXCompressionType

typedef enum EXXEncryptionType {
	EXXEncryptionType_None,                             //(default)
	EXXEncryptionType_AES256GCM,                        //--aes <32-byte key (in hex or nyto)>
	EXXEncryptionType_Count
} EXXEncryptionType;

typedef U8 XXEncryptionType;         //EXXEncryptionType

typedef enum EXXDataSizeType {       //Can be represented as a 2-bit array for example
	EXXDataSizeType_U8,
	EXXDataSizeType_U16,
	EXXDataSizeType_U32,
	EXXDataSizeType_U64,
	EXXDataSizeType_Count
} EXXDataSizeType;

typedef U8 XXDataSizeType;           //EXXDataSizeType

//Byte count of each EXXDataSizeType, indexed by it
static const U8 SIZE_BYTE_TYPE[4] = { 1, 2, 4, 8 };

//Reads one little endian size of the given type and advances buf past it.
//On failure buf is unchanged and *result is 0, also for an invalid type.
Bool Buffer_consumeSizeType(Buffer *buf, EXXDataSizeType type, U64 *result, Error *e_rr);

//*it is the byte offset in the stream and advances by the type's byte count, only on success.
//On failure *it is unchanged and *result is unspecified.
Bool StreamCursor_consumeSizeType(
	StreamCursor *cursor,
	U64 *it,
	EXXDataSizeType type,
	U64 *result,
	const Allocator *alloc,
	Error *e_rr
);

//Writes v at byte offset *it and advances *it, only on success. v is truncated to the type's width without a check;
//EXXDataSizeType_getRequiredType picks a type that holds it.
Bool StreamCursor_appendSizeType(
	StreamCursor *cursor,
	U64 *it,
	U64 v,
	EXXDataSizeType type,
	const Allocator *alloc,
	Error *e_rr
);

//Unchecked: reads/writes the type's byte count at ptr (any alignment), the caller guarantees the space.
//Both return 0 on NULL ptr or an invalid type, which a read can't tell from a stored 0.
//Write returns the bytes written and truncates the value to the type's width.
U64 Buffer_forceReadSizeType(const U8 *ptr, EXXDataSizeType type);
U64 Buffer_forceWriteSizeType(U8 *ptr, EXXDataSizeType type, U64 result);

//Smallest type that holds v
EXXDataSizeType EXXDataSizeType_getRequiredType(U64 v);

#ifdef __cplusplus
	}
#endif
