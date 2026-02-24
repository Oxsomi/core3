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

#pragma once
#include "types/container/stream.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef enum EMemoryStreamFlags {

	EMemoryStreamFlags_None			= 0,
	EMemoryStreamFlags_IsWritable	= 1 << 0,
	EMemoryStreamFlags_IsResizable	= 1 << 1,
	EMemoryStreamFlags_UnsafeMemory	= 1 << 2,		//Disable nulling of memory

	EMemoryStreamFlags_WriteResize	= EMemoryStreamFlags_IsWritable | EMemoryStreamFlags_IsResizable

} EMemoryStreamFlags;
		
typedef struct MemoryStream {
	Stream parent;
	Buffer data;
} MemoryStream;

typedef RefPtr MemoryStreamRef;

RefPtrType MemoryStream_makeType(const Allocator *alloc);

//Create a memory stream with size
Bool MemoryStream_create(
	U64 size,
	EMemoryStreamFlags flags,
	const RefPtrType *type,			//MemoryStream_makeType(alloc)
	MemoryStreamRef **stream,
	Error *e_rr
);

//Create from existing buffer (takes ownership if Buffer is an allocation)
Bool MemoryStream_createFromBuffer(
	Buffer *buffer,
	EMemoryStreamFlags flags,
	const RefPtrType *type,			//MemoryStream_makeType(alloc)
	MemoryStreamRef **stream,
	Error *e_rr
);

//Create from existing buffer region (needs Buffer to stay valid during the stream lifetime)
// Note: Resizing is explicitly disallowed for this, since it would need a copy for a buffer.
//length is optional, if 0 it's the remainder
Bool MemoryStream_createFromBufferRegion(
	Buffer buffer,
	U64 offset,
	U64 length,
	EMemoryStreamFlags flags,
	const RefPtrType *type,			//MemoryStream_makeType(alloc)
	MemoryStreamRef **stream,
	Error *e_rr
);

//Move buffer out of stream (invalidates stream and takes it from everyone else referencing it)
Bool MemoryStream_move(MemoryStreamRef **stream, Buffer *output, Error *e_rr);

#ifdef __cplusplus
	}
#endif
