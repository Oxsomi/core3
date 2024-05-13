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
#include "types/types.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct AllocationBuffer AllocationBuffer;

Error Buffer_createCopyx(Buffer buf, Buffer *output);
Error Buffer_createZeroBitsx(U64 length, Buffer *output);
Error Buffer_createOneBitsx(U64 length, Buffer *output);
Error Buffer_createBitsx(U64 length, Bool value, Buffer *result);
Bool Buffer_freex(Buffer *buf);
Error Buffer_createEmptyBytesx(U64 length, Buffer *output);
Error Buffer_createUninitializedBytesx(U64 length, Buffer *output);

Error AllocationBuffer_createx(U64 size, Bool isVirtual, AllocationBuffer *allocationBuffer);
Bool AllocationBuffer_freex(AllocationBuffer *allocationBuffer);

Error AllocationBuffer_createRefFromRegionx(Buffer origin, U64 offset, U64 size, AllocationBuffer *allocationBuffer);
Error AllocationBuffer_allocateBlockx(AllocationBuffer *allocationBuffer, U64 size, U64 alignment, const U8 **result);
Error AllocationBuffer_allocateAndFillBlockx(AllocationBuffer *allocationBuffer, Buffer data, U64 alignment, U8 **result);

#ifdef __cplusplus
	}
#endif
