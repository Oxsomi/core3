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

//types/container/ref_ptr.h

#pragma once
#include "types/container/list.h"
#include "types/base/types.h"
#include "types/base/allocator.h"
#include "types/base/type_id.h"
#include "types/base/atomic.h"
#include <stdalign.h>

#ifdef __cplusplus
	extern "C" {
#endif

typedef void (*ObjectFreeFunc)(void *ptr, const Allocator *allocator);

typedef U32 TypeId;

typedef struct RefPtrType {
	TypeId typeId;
	U32 lengthAndAlignment;        //Pack with RefPtrType_pack, read with RefPtrType_length / _alignment
	const Allocator *alloc;
	ObjectFreeFunc free;
} RefPtrType;

//Alignment goes in the upper U8 as its log2, so it costs nothing next to the length rather than another
//field. That leaves 24 bits of length, which is 16MiB; nothing reference counted comes close.
//
//The alignment is the one RefPtr_data has to satisfy, not the allocation's. RefPtr sits in front of the
//object, so those are different addresses and it's the object that has the type with the requirement.
//Pass alignof(T) and anything at or below what the allocator already gives costs nothing.

static inline U32 RefPtrType_pack(U64 length, U64 alignment) {

	U32 log2Alignment = 0;

	while(((U64)1 << log2Alignment) < alignment)
		++log2Alignment;

	return (U32) length | ((U32) log2Alignment << 24);
}

static inline U32 RefPtrType_length(const RefPtrType *type) { return type->lengthAndAlignment & 0xFFFFFF; }

static inline U64 RefPtrType_alignment(const RefPtrType *type) {
	return (U64)1 << (type->lengthAndAlignment >> 24);
}

typedef struct RefPtr {
	AtomicI64 refCount;
	const RefPtrType *refPtrType;
} RefPtr;

TListNamed(RefPtr*, ListRefPtr);

//Needs type to stay allocated through the lifetime of the RefPtr.
//Also needs a unique one per length & alloc. So there can be multiple RefPtrType* that reference to the same typeId.
Bool RefPtr_create(const RefPtrType *type, RefPtr **result, Error *e_rr);

Bool RefPtr_inc(RefPtr *ptr);
void RefPtr_dec(RefPtr **ptr);    //Clears pointer if it's gone

#define RefPtr_data(dat, T) (!(dat) ? NULL : (T*)((dat) + 1))

//Signifies that the RefPtr will not need inc/dec, because the owner will manually ensure
//that the ref is removed before it's important.
typedef RefPtr WeakRefPtr;

TListNamed(WeakRefPtr*, ListWeakRefPtr);

#ifdef __cplusplus
	}
#endif
