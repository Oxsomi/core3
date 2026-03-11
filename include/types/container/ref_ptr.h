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
#include "types/container/list.h"
#include "types/base/types.h"
#include "types/base/allocator.h"
#include "types/base/type_id.h"
#include "types/base/atomic.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef void (*ObjectFreeFunc)(void *ptr, const Allocator *allocator);

typedef enum ETypeId ETypeId;

typedef struct RefPtrType {
	ETypeId typeId;
	U32 length;
	const Allocator *alloc;
	ObjectFreeFunc free;
} RefPtrType;

typedef struct RefPtr {
	AtomicI64 refCount;
	const RefPtrType *refPtrType;
} RefPtr;

TListNamed(RefPtr*, ListRefPtr);

//Needs type to stay allocated through the lifetime of the RefPtr.
//Also needs a unique one per length & alloc. So there can be multiple RefPtrType* that reference to the same typeId.
Bool RefPtr_create(const RefPtrType *type, RefPtr **result, Error *e_rr);

Bool RefPtr_inc(RefPtr *ptr);
void RefPtr_dec(RefPtr **ptr);	//Clears pointer if it's gone

#define RefPtr_data(dat, T) (!(dat) ? NULL : (T*)((dat) + 1))

//Signifies that the RefPtr will not need inc/dec, because the owner will manually ensure
//that the ref is removed before it's important.
typedef RefPtr WeakRefPtr;

TListNamed(WeakRefPtr*, ListWeakRefPtr);

#ifdef __cplusplus
	}
#endif
