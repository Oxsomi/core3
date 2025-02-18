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

#pragma once
#include "types/base/types.h"
#include "types/base/allocator.h"
#include "types/container/list.h"
#include "types/base/type_id.h"
#include "types/base/atomic.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef Bool (*ObjectFreeFunc)(void *ptr, Allocator allocator);

typedef enum ETypeId ETypeId;

typedef struct RefPtr {

	AtomicI64 refCount;

	ETypeId typeId;
	U32 length;

	Allocator alloc;

	ObjectFreeFunc free;

} RefPtr;

TListNamed(RefPtr*, ListRefPtr);

Error RefPtr_create(U32 objectLength, Allocator alloc, ObjectFreeFunc free, ETypeId type, RefPtr **result);

Bool RefPtr_inc(RefPtr *ptr);
Bool RefPtr_dec(RefPtr **ptr);	//Clears pointer if it's gone

#define RefPtr_data(dat, T) (!(dat) ? NULL : (T*)((dat) + 1))

//Signifies that the RefPtr will not need inc/dec, because the owner will manually ensure
//that the ref is removed before it's important.
typedef RefPtr WeakRefPtr;

TListNamed(WeakRefPtr*, ListWeakRefPtr);

#ifdef __cplusplus
	}
#endif
