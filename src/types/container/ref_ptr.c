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

//types/container/ref_ptr.c

#include "types/container/list_impl.h"
#include "types/container/ref_ptr.h"

TListNamedImpl(ListRefPtr);
TListNamedImpl(ListWeakRefPtr);

Bool RefPtr_create(const RefPtrType *type, RefPtr **result, Error *e_rr) {

	Bool s_uccess = true;

	//Either free or freeInvoke has to be there, since an object that frees neither way would leak.
	//A wrapper from another language sets freeInvoke and leaves free NULL (see ObjectFreeInvoke).

	if(!type || !RefPtrType_length(type) || (!type->free && !type->freeInvoke) || !type->alloc || !result)
		retError(clean, Error_nullPointer(
			!result ? 1 : 0,
			"RefPtr_create()::type, ->lengthAndAlignment, ->alloc, one of ->free / ->freeInvoke, and result are required"
		));

	if(*result)
		retError(clean, Error_invalidParameter(3, 0, "RefPtr_create()::result isn't empty, might indicate memleak"));

	//Only pay for the aligned allocator when the type asks for more than the allocator hands out anyway.
	//It over-allocates and puts a byte in front of what it returns, so the free below has to match.

	const U64 alignment = RefPtrType_alignment(type);
	const U64 length = sizeof(RefPtr) + RefPtrType_length(type);

	Buffer buf = Buffer_createNull();

	if(alignment > BUFFER_DEFAULT_ALIGNMENT) {
		gotoIfError3(clean, Buffer_createEmptyBytesAligned(length, alignment, sizeof(RefPtr), type->alloc, &buf, e_rr));
	}

	else {
		gotoIfError3(clean, Buffer_createEmptyBytes(length, type->alloc, &buf, e_rr));
	}

	*(*result = (RefPtr*)buf.ptr) = (RefPtr) { .refCount = (AtomicI64) { 1 }, .refPtrType = type };

clean:
	return s_uccess;
}

Bool RefPtr_inc(RefPtr *ptr) {

	if(!ptr || !ptr->refPtrType)
		return false;

	AtomicI64_inc(&ptr->refCount);
	return true;
}

void RefPtr_dec(RefPtr **pptr) {

	if(!pptr || !*pptr)
		return;

	RefPtr *ptr = *pptr;

	if(!AtomicI64_dec(&ptr->refCount)) {

		//freeInvoke first: a wrapper registers that one precisely because its own callback can't carry the
		// Allocator type (see ObjectFreeInvoke).

		if(ptr->refPtrType->freeInvoke)
			ptr->refPtrType->freeInvoke(RefPtr_data(ptr, void), ptr->refPtrType->alloc);

		else if(ptr->refPtrType->free)
			ptr->refPtrType->free(RefPtr_data(ptr, void), ptr->refPtrType->alloc);

		const RefPtrType *type = ptr->refPtrType;
		Buffer orig = Buffer_createManagedPtr(ptr, sizeof(*ptr) + RefPtrType_length(type));

		//The Buffer that carried the aligned bit went away when only the pointer was kept, so it has to be put
		// back before freeing, or Buffer_free hands the allocator the payload instead of the real base.

		if(RefPtrType_alignment(type) > BUFFER_DEFAULT_ALIGNMENT)
			Buffer_markAligned(&orig);

		Buffer_free(&orig, type->alloc);
	}

	*pptr = NULL;
}
